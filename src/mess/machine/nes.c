#include "emu.h"
#include "crsshair.h"
#include "hash.h"
#include "cpu/m6502/m6502.h"
#include "video/ppu2c0x.h"
#include "includes/nes.h"
#include "machine/nes_mmc.h"
#include "devices/cartslot.h"
#include "devices/flopdrv.h"

/***************************************************************************
    CONSTANTS
***************************************************************************/

/* Set to dump info about the inputs to the errorlog */
#define LOG_JOY		0

/* Set to generate prg & chr files when the cart is loaded */
#define SPLIT_PRG	0
#define SPLIT_CHR	0

/***************************************************************************
    FUNCTION PROTOTYPES
***************************************************************************/

static void init_nes_core(running_machine *machine);
static void nes_machine_stop(running_machine *machine);
static READ8_HANDLER(nes_fds_r);
static WRITE8_HANDLER(nes_fds_w);
static void fds_irq(running_device *device, int scanline, int vblank, int blanked);

/***************************************************************************
    FUNCTIONS
***************************************************************************/

static void init_nes_core( running_machine *machine )
{
	nes_state *state = (nes_state *)machine->driver_data;
	const address_space *space = cputag_get_address_space(machine, "maincpu", ADDRESS_SPACE_PROGRAM);
	static const char *const bank_names[] = { "bank1", "bank2", "bank3", "bank4" };
	int prg_banks = (state->prg_chunks == 1) ? (2 * 2) : (state->prg_chunks * 2);
	int i;

	/* We set these here in case they weren't set in the cart loader */
	state->rom = memory_region(machine, "maincpu");
	state->vrom = memory_region(machine, "gfx1");
	state->vram = memory_region(machine, "gfx2");
	state->ciram = memory_region(machine, "gfx3");

	/* Brutal hack put in as a consequence of the new memory system; we really need to fix the NES code */
	memory_install_readwrite_bank(space, 0x0000, 0x07ff, 0, 0x1800, "bank10");

	memory_install_readwrite8_handler(cpu_get_address_space(devtag_get_device(machine, "ppu"), ADDRESS_SPACE_PROGRAM), 0, 0x1fff, 0, 0, nes_chr_r, nes_chr_w);
	memory_install_readwrite8_handler(cpu_get_address_space(devtag_get_device(machine, "ppu"), ADDRESS_SPACE_PROGRAM), 0x2000, 0x3eff, 0, 0, nes_nt_r, nes_nt_w);

	memory_set_bankptr(machine, "bank10", state->rom);

	/* If there is Disk Expansion and no cart has been loaded, setup memory accordingly */
	if (state->disk_expansion && state->pcb_id == NO_BOARD)
	{
		state->slow_banking = 0;
		/* If we are loading a disk we have already filled state->fds_data and we don't want to overwrite it,
		 if we are loading a cart image identified as mapper 20 (probably wrong mapper...) we need to alloc
		 memory for use in nes_fds_r/nes_fds_w. Same goes for allocation of fds_ram (used for bank2)  */
		if (state->fds_data == NULL)
		{
			UINT32 size = memory_region_length(machine, "maincpu") - 0x10000;
			state->fds_data = auto_alloc_array_clear(machine, UINT8, size);
			memcpy(state->fds_data, state->rom, size);	// copy in fds_data the cart PRG
		}
		if (state->fds_ram == NULL)
			state->fds_ram = auto_alloc_array(machine, UINT8, 0x8000);
		
		memory_install_read8_handler(space, 0x4030, 0x403f, 0, 0, nes_fds_r);
		memory_install_read_bank(space, 0x6000, 0xdfff, 0, 0, "bank2");
		memory_install_read_bank(space, 0xe000, 0xffff, 0, 0, "bank1");
		
		memory_install_write8_handler(space, 0x4020, 0x402f, 0, 0, nes_fds_w);
		memory_install_write_bank(space, 0x6000, 0xdfff, 0, 0, "bank2");
		
		memory_set_bankptr(machine, "bank1", &state->rom[0xe000]);
		memory_set_bankptr(machine, "bank2", state->fds_ram);
		return;
	}

	/* Set up the mapper callbacks */
	if (state->format == 1)
		mapper_handlers_setup(machine);
	else if (state->format == 3 || state->format == 2)
		pcb_handlers_setup(machine);

	/* Set up the memory handlers for the mapper */
	state->slow_banking = 0;
	memory_install_read_bank(space, 0x8000, 0x9fff, 0, 0, "bank1");
	memory_install_read_bank(space, 0xa000, 0xbfff, 0, 0, "bank2");
	memory_install_read_bank(space, 0xc000, 0xdfff, 0, 0, "bank3");
	memory_install_read_bank(space, 0xe000, 0xffff, 0, 0, "bank4");
	memory_install_readwrite_bank(space, 0x6000, 0x7fff, 0, 0, "bank5");
	
	/* configure banks 1-4 */
	for (i = 0; i < 4; i++)
	{
		memory_configure_bank(machine, bank_names[i], 0, prg_banks, memory_region(machine, "maincpu") + 0x10000, 0x2000);
		// some mappers (e.g. MMC5) can map PRG RAM in  0x8000-0xffff as well
		if (state->prg_ram)
			memory_configure_bank(machine, bank_names[i], prg_banks, state->wram_size / 0x2000, state->wram, 0x2000);
		// however, at start we point to PRG ROM
		memory_set_bank(machine, bank_names[i], i);
		state->prg_bank[i] = i;
	}
	
	/* bank 5 configuration is more delicate, since it can have PRG RAM, PRG ROM or SRAM mapped to it */
	/* we first map PRG ROM banks, then the battery bank (if a battery is present), and finally PRG RAM (state->wram) */
	memory_configure_bank(machine, "bank5", 0, prg_banks, memory_region(machine, "maincpu") + 0x10000, 0x2000);
	state->battery_bank5_start = prg_banks;
	state->prgram_bank5_start = prg_banks;
	state->empty_bank5_start = prg_banks;
	
	/* add battery ram, but only if there's no trainer since they share overlapping memory. */
	if (state->battery && !state->trainer)
	{
		UINT32 bank_size = (state->battery_size > 0x2000) ? 0x2000 : state->battery_size;
		int bank_num = (state->battery_size > 0x2000) ? state->battery_size / 0x2000 : 1;
		memory_configure_bank(machine, "bank5", prg_banks, bank_num, state->battery_ram, bank_size);
		state->prgram_bank5_start += bank_num;
		state->empty_bank5_start += bank_num;
	}
	/* add prg ram. */
	if (state->prg_ram)
	{
		memory_configure_bank(machine, "bank5", state->prgram_bank5_start, state->wram_size / 0x2000, state->wram, 0x2000);
		state->empty_bank5_start += state->wram_size / 0x2000;
	}
	
	memory_configure_bank(machine, "bank5", state->empty_bank5_start, 1, state->rom + 0x6000, 0x2000);
	
	/* if we have any additional PRG RAM, point bank5 to its first bank */
	if (state->battery || state->prg_ram)
		state->prg_bank[4] = state->battery_bank5_start;
	else
		state->prg_bank[4] = state->empty_bank5_start; // or shall we point to "maincpu" region at 0x6000? point is that we should never access this region if no sram or wram is present!
	
	memory_set_bank(machine, "bank5", state->prg_bank[4]);
	
	if (state->four_screen_vram)
	{
		state->extended_ntram = auto_alloc_array(machine, UINT8, 0x2000);
		state_save_register_global_pointer(machine, state->extended_ntram, 0x2000);
	}
	
	// there are still some quirk about writes to bank5... I hope to fix them soon. (mappers 34,45,52,246 have both mid_w and WRAM-->check)
	if (state->mmc_write_mid)
		memory_install_write8_handler(space, 0x6000, 0x7fff, 0, 0, state->mmc_write_mid);
	if (state->mmc_write)
		memory_install_write8_handler(space, 0x8000, 0xffff, 0, 0, state->mmc_write);
	
	// In fact, we also allow single pcbs to overwrite the bank read handlers defined above,
	// because some pcbs (mainly pirate ones) require protection values to be read instead of
	// the expected ROM banks: these handlers, though, must take care of the ROM access as well
	if (state->mmc_read_mid)
		memory_install_read8_handler(space, 0x6000, 0x7fff, 0, 0, state->mmc_read_mid);
	if (state->mmc_read)
		memory_install_read8_handler(space, 0x8000, 0xffff, 0, 0, state->mmc_read);
	
	// install additional handlers
	if (state->pcb_id == BTL_SMB2B)
	{
		memory_install_write8_handler(space, 0x4020, 0x403f, 0, 0, smb2jb_extra_w);
		memory_install_write8_handler(space, 0x40a0, 0x40bf, 0, 0, smb2jb_extra_w);
	}
	if (state->mapper == 50)
	{
		memory_install_write8_handler(space, 0x4020, 0x403f, 0, 0, nes_mapper50_add_w);
		memory_install_write8_handler(space, 0x40a0, 0x40bf, 0, 0, nes_mapper50_add_w);
	}
}

// to be probably removed (it does nothing since a long time)
int nes_ppu_vidaccess( running_device *device, int address, int data )
{
	return data;
}

MACHINE_RESET( nes )
{
	nes_state *state = (nes_state *)machine->driver_data;

	/* Some carts have extra RAM and require it on at startup, e.g. Metroid */
	state->mid_ram_enable = 1;
	
	/* Reset the mapper variables. Will also mark the char-gen ram as dirty */
	if (state->disk_expansion && state->pcb_id == NO_BOARD)
		ppu2c0x_set_hblank_callback(state->ppu, fds_irq);

	if (state->format == 1)
		nes_mapper_reset(machine);

	if (state->format == 2 || state->format == 3)
		nes_pcb_reset(machine);

	/* Reset the serial input ports */
	state->in_0.shift = 0;
	state->in_1.shift = 0;

	devtag_get_device(machine, "maincpu")->reset();
}

static TIMER_CALLBACK( nes_irq_callback )
{
	nes_state *state = (nes_state *)machine->driver_data;
	cpu_set_input_line(state->maincpu, M6502_IRQ_LINE, HOLD_LINE);
	timer_adjust_oneshot(state->irq_timer, attotime_never, 0);
}

static STATE_POSTLOAD( nes_banks_restore )
{
	nes_state *state = (nes_state *)machine->driver_data;

	memory_set_bank(machine, "bank1", state->prg_bank[0]);
	memory_set_bank(machine, "bank2", state->prg_bank[1]);
	memory_set_bank(machine, "bank3", state->prg_bank[2]);
	memory_set_bank(machine, "bank4", state->prg_bank[3]);
	memory_set_bank(machine, "bank5", state->prg_bank[4]);
}

static void nes_state_register( running_machine *machine )
{
	nes_state *state = (nes_state *)machine->driver_data;

	state_save_register_global_array(machine, state->prg_bank);

	state_save_register_global(machine, state->MMC5_floodtile);
	state_save_register_global(machine, state->MMC5_floodattr);
	state_save_register_global_array(machine, state->mmc5_vram);
	state_save_register_global(machine, state->mmc5_vram_control);

	state_save_register_global_array(machine, state->nes_vram_sprite);
	state_save_register_global(machine, state->last_frame_flip);
	state_save_register_global(machine, state->mid_ram_enable);

	// shared mapper variables
	state_save_register_global(machine, state->IRQ_enable);
	state_save_register_global(machine, state->IRQ_enable_latch);
	state_save_register_global(machine, state->IRQ_count);
	state_save_register_global(machine, state->IRQ_count_latch);
	state_save_register_global(machine, state->IRQ_toggle);
	state_save_register_global(machine, state->IRQ_reset);
	state_save_register_global(machine, state->IRQ_status);
	state_save_register_global(machine, state->IRQ_mode);
	state_save_register_global(machine, state->mult1);
	state_save_register_global(machine, state->mult2);
	state_save_register_global(machine, state->mmc_chr_source);
	state_save_register_global(machine, state->mmc_cmd1);
	state_save_register_global(machine, state->mmc_cmd2);
	state_save_register_global(machine, state->mmc_count);
	state_save_register_global(machine, state->mmc_prg_base);
	state_save_register_global(machine, state->mmc_prg_mask);
	state_save_register_global(machine, state->mmc_chr_base);
	state_save_register_global(machine, state->mmc_chr_mask);
	state_save_register_global_array(machine, state->mmc_prg_bank);
	state_save_register_global_array(machine, state->mmc_vrom_bank);
	state_save_register_global_array(machine, state->mmc_extra_bank);

	state_save_register_global(machine, state->fds_motor_on);
	state_save_register_global(machine, state->fds_door_closed);
	state_save_register_global(machine, state->fds_current_side);
	state_save_register_global(machine, state->fds_head_position);
	state_save_register_global(machine, state->fds_status0);
	state_save_register_global(machine, state->fds_read_mode);
	state_save_register_global(machine, state->fds_write_reg);
	state_save_register_global(machine, state->fds_last_side);
	state_save_register_global(machine, state->fds_count);

	state_save_register_global_pointer(machine, state->wram, state->wram_size);
	if (state->battery)
		state_save_register_global_pointer(machine, state->battery_ram, state->battery_size);

	state_save_register_postload(machine, nes_banks_restore, NULL);
}

MACHINE_START( nes )
{
	nes_state *state = (nes_state *)machine->driver_data;

	init_nes_core(machine);
	add_exit_callback(machine, nes_machine_stop);

	state->maincpu = devtag_get_device(machine, "maincpu");
	state->ppu = devtag_get_device(machine, "ppu");
	state->sound = devtag_get_device(machine, "nessound");
	state->cart = devtag_get_device(machine, "cart");

	state->irq_timer = timer_alloc(machine, nes_irq_callback, NULL);
	nes_state_register(machine);
}

static void nes_machine_stop( running_machine *machine )
{
	nes_state *state = (nes_state *)machine->driver_data;

	/* Write out the battery file if necessary */
	// FIXME: are there cases with both internal mapper RAM and external RAM (both battery backed)?
	if (state->battery)
		image_battery_save(state->cart, state->battery_ram, state->battery_size);
	if (state->mapper_ram_size)
		image_battery_save(state->cart, state->mapper_ram, state->mapper_ram_size);
}



READ8_HANDLER( nes_IN0_r )
{
	nes_state *state = (nes_state *)space->machine->driver_data;
	int cfg = input_port_read(space->machine, "CTRLSEL");
	int ret;

	if ((cfg & 0x000f) >= 0x04)	// for now we treat the FC keyboard separately from other inputs!
	{
		// here we should have the tape input
		ret = 0;
	}
	else
	{
		/* Some games expect bit 6 to be set because the last entry on the data bus shows up */
		/* in the unused upper 3 bits, so typically a read from $4016 leaves 0x40 there. */
		ret = 0x40;
		
		ret |= ((state->in_0.i0 >> state->in_0.shift) & 0x01);
		
		/* zapper */
		if ((cfg & 0x000f) == 0x0002)
		{
			int x = state->in_0.i1;	/* read Zapper x-position */
			int y = state->in_0.i2;	/* read Zapper y-position */
			UINT32 pix, color_base;
			
			/* get the pixel at the gun position */
			pix = ppu2c0x_get_pixel(state->ppu, x, y);
			
			/* get the color base from the ppu */
			color_base = ppu2c0x_get_colorbase(state->ppu);
			
			/* look at the screen and see if the cursor is over a bright pixel */
			if ((pix == color_base + 0x20) || (pix == color_base + 0x30) ||
				(pix == color_base + 0x33) || (pix == color_base + 0x34))
			{
				ret &= ~0x08; /* sprite hit */
			}
			else
				ret |= 0x08;  /* no sprite hit */
			
			/* If button 1 is pressed, indicate the light gun trigger is pressed */
			ret |= ((state->in_0.i0 & 0x01) << 4);
		}
		
		if (LOG_JOY)
			logerror("joy 0 read, val: %02x, pc: %04x, bits read: %d, chan0: %08x\n", ret, cpu_get_pc(space->cpu), state->in_0.shift, state->in_0.i0);
		
		state->in_0.shift++;
	}

	return ret;
}

// row of the keyboard matrix are read 4-bits at time, and gets returned as bit1->bit4
static UINT8 nes_read_fc_keyboard_line( running_machine *machine, UINT8 scan, UINT8 mode )
{
	static const char *const fc_keyport_names[] = { "FCKEY0", "FCKEY1", "FCKEY2", "FCKEY3", "FCKEY4", "FCKEY5", "FCKEY6", "FCKEY7", "FCKEY8" };
	return ((input_port_read(machine, fc_keyport_names[scan]) >> (mode * 4)) & 0x0f) << 1;
}

static UINT8 nes_read_subor_keyboard_line( running_machine *machine, UINT8 scan, UINT8 mode )
{
	static const char *const sub_keyport_names[] = { "SUBKEY0", "SUBKEY1", "SUBKEY2", "SUBKEY3", "SUBKEY4", 
		"SUBKEY5", "SUBKEY6", "SUBKEY7", "SUBKEY8", "SUBKEY9", "SUBKEY10", "SUBKEY11", "SUBKEY12" };
	return ((input_port_read(machine, sub_keyport_names[scan]) >> (mode * 4)) & 0x0f) << 1;
}

READ8_HANDLER( nes_IN1_r )
{
	nes_state *state = (nes_state *)space->machine->driver_data;
	int cfg = input_port_read(space->machine, "CTRLSEL");
	int ret;

	if ((cfg & 0x000f) == 0x04)	// for now we treat the FC keyboard separately from other inputs!
	{
		if (state->fck_scan < 9)
			ret = ~nes_read_fc_keyboard_line(space->machine, state->fck_scan, state->fck_mode) & 0x1e;
		else
			ret = 0x1e;
	}
	else if ((cfg & 0x000f) == 0x08)	// for now we treat the Subor keyboard separately from other inputs!
	{
		if (state->fck_scan < 12)
			ret = ~nes_read_subor_keyboard_line(space->machine, state->fck_scan, state->fck_mode) & 0x1e;
		else
			ret = 0x1e;
	}
	else
	{
		/* Some games expect bit 6 to be set because the last entry on the data bus shows up */
		/* in the unused upper 3 bits, so typically a read from $4017 leaves 0x40 there. */
		ret = 0x40;
		
		/* Handle data line 0's serial output */
		ret |= ((state->in_1.i0 >> state->in_1.shift) & 0x01);
		
		/* zapper */
		if ((cfg & 0x00f0) == 0x0030)
		{
			int x = state->in_1.i1;	/* read Zapper x-position */
			int y = state->in_1.i2;	/* read Zapper y-position */
			UINT32 pix, color_base;
			
			/* get the pixel at the gun position */
			pix = ppu2c0x_get_pixel(state->ppu, x, y);
			
			/* get the color base from the ppu */
			color_base = ppu2c0x_get_colorbase(state->ppu);
			
			/* look at the screen and see if the cursor is over a bright pixel */
			if ((pix == color_base + 0x20) || (pix == color_base + 0x30) ||
				(pix == color_base + 0x33) || (pix == color_base + 0x34))
			{
				ret &= ~0x08; /* sprite hit */
			}
			else
				ret |= 0x08;  /* no sprite hit */
			
			/* If button 1 is pressed, indicate the light gun trigger is pressed */
			ret |= ((state->in_1.i0 & 0x01) << 4);
		}
		
		/* arkanoid dial */
		else if ((cfg & 0x00f0) == 0x0040)
		{
			/* Handle data line 2's serial output */
			ret |= ((state->in_1.i2 >> state->in_1.shift) & 0x01) << 3;
			
			/* Handle data line 3's serial output - bits are reversed */
			/* NPW 27-Nov-2007 - there is no third subscript! commenting out */
			/* ret |= ((state->in_1[3] >> state->in_1.shift) & 0x01) << 4; */
			/* ret |= ((state->in_1[3] << state->in_1.shift) & 0x80) >> 3; */
		}
		
		if (LOG_JOY)
			logerror("joy 1 read, val: %02x, pc: %04x, bits read: %d, chan0: %08x\n", ret, cpu_get_pc(space->cpu), state->in_1.shift, state->in_1.i0);
		
		state->in_1.shift++;
	}

	return ret;
}



static void nes_read_input_device( running_machine *machine, int cfg, nes_input *vals, int pad_port, int supports_zapper, int paddle_port )
{
	static const char *const padnames[] = { "PAD1", "PAD2", "PAD3", "PAD4" };

	vals->i0 = 0;
	vals->i1 = 0;
	vals->i2 = 0;

	switch(cfg & 0x0f)
	{
		case 0x01:	/* gamepad */
			if (pad_port >= 0)
				vals->i0 = input_port_read(machine, padnames[pad_port]);
			break;

		case 0x02:	/* zapper 1 */
			if (supports_zapper)
			{
				vals->i0 = input_port_read(machine, "ZAPPER1_T");
				vals->i1 = input_port_read(machine, "ZAPPER1_X");
				vals->i2 = input_port_read(machine, "ZAPPER1_Y");
			}
			break;

		case 0x03:	/* zapper 2 */
			if (supports_zapper)
			{
				vals->i0 = input_port_read(machine, "ZAPPER2_T");
				vals->i1 = input_port_read(machine, "ZAPPER2_X");
				vals->i2 = input_port_read(machine, "ZAPPER2_Y");
			}
			break;

		case 0x04:	/* arkanoid paddle */
			if (paddle_port >= 0)
				vals->i0 = (UINT8) ((UINT8) input_port_read(machine, "PADDLE") + (UINT8)0x52) ^ 0xff;
			break;
	}
}


static TIMER_CALLBACK( lightgun_tick )
{
	if ((input_port_read(machine, "CTRLSEL") & 0x000f) == 0x0002)
	{
		/* enable lightpen crosshair */
		crosshair_set_screen(machine, 0, CROSSHAIR_SCREEN_ALL);
	}
	else
	{
		/* disable lightpen crosshair */
		crosshair_set_screen(machine, 0, CROSSHAIR_SCREEN_NONE);
	}

	if ((input_port_read(machine, "CTRLSEL") & 0x00f0) == 0x0030)
	{
		/* enable lightpen crosshair */
		crosshair_set_screen(machine, 1, CROSSHAIR_SCREEN_ALL);
	}
	else
	{
		/* disable lightpen crosshair */
		crosshair_set_screen(machine, 1, CROSSHAIR_SCREEN_NONE);
	}
}

WRITE8_HANDLER( nes_IN0_w )
{
	nes_state *state = (nes_state *)space->machine->driver_data;
	int cfg = input_port_read(space->machine, "CTRLSEL");

	/* Check if lightgun has been chosen as input: if so, enable crosshair */
	timer_set(space->machine, attotime_zero, NULL, 0, lightgun_tick);
	
	if ((cfg & 0x000f) >= 0x04)	// for now we treat the FC keyboard separately from other inputs!
	{
		// here we should also have the tape output

		if (BIT(data, 2))	// keyboard active
		{
			int lines = ((cfg & 0x000f) == 0x04) ? 9 : 12;
			UINT8 out = BIT(data, 1);	// scan
			
			if (state->fck_mode && !out && ++state->fck_scan > lines)
				state->fck_scan = 0;
			
			state->fck_mode = out;	// access lower or upper 4 bits
			
			if (BIT(data, 0))	// reset
				state->fck_scan = 0;
		}
	}
	else
	{
		if (data & 0x01)
			return;
		
		if (LOG_JOY)
			logerror("joy 0 bits read: %d\n", state->in_0.shift);
		
		/* Toggling bit 0 high then low resets both controllers */
		state->in_0.shift = 0;
		state->in_1.shift = 0;
		
		/* Read the input devices */
		nes_read_input_device(space->machine, cfg >>  0, &state->in_0, 0,  TRUE, -1);
		nes_read_input_device(space->machine, cfg >>  4, &state->in_1, 1,  TRUE,  1);
		nes_read_input_device(space->machine, cfg >>  8, &state->in_2, 2, FALSE, -1);
		nes_read_input_device(space->machine, cfg >> 12, &state->in_3, 3, FALSE, -1);
		
		if (cfg & 0x0f00)
			state->in_0.i0 |= (state->in_2.i0 << 8) | (0x08 << 16);
		
		if (cfg & 0xf000)
			state->in_1.i0 |= (state->in_3.i0 << 8) | (0x04 << 16);
	}
}


WRITE8_HANDLER( nes_IN1_w )
{
}

struct nes_cart_lines
{
	const char *tag;
	int line;
};

static const struct nes_cart_lines nes_cart_lines_table[] =
{
	{ "PRG A0",    0 },
	{ "PRG A1",    1 },
	{ "PRG A2",    2 },
	{ "PRG A3",    3 },
	{ "PRG A4",    4 },
	{ "PRG A5",    5 },
	{ "PRG A6",    6 },
	{ "PRG A7",    7 },
	{ "CHR A10",  10 },
	{ "CHR A11",  11 },
	{ "CHR A12",  12 },
	{ "CHR A13",  13 },
	{ "CHR A14",  14 },
	{ "CHR A15",  15 },
	{ "CHR A16",  16 },
	{ "CHR A17",  17 },
	{ "NC",      127 },
	{ 0 }
};

static int nes_cart_get_line( const char *feature )
{
	const struct nes_cart_lines *nes_line = &nes_cart_lines_table[0];

	if (feature == NULL)
		return 128;

	while (nes_line->tag)
	{
		if (strcmp(nes_line->tag, feature) == 0)
			break;
		
		nes_line++;
	}

	return nes_line->line;
}

DEVICE_IMAGE_LOAD( nes_cart )
{
	nes_state *state = (nes_state *)image->machine->driver_data;
	state->pcb_id = NO_BOARD;	// initialization

	if (image_software_entry(image) == NULL)
	{
		const char *mapinfo;
		int mapint1 = 0, mapint2 = 0, mapint3 = 0, mapint4 = 0, goodcrcinfo = 0;
		char magic[4], extend[5];
		int local_options = 0;
		char m;

		/* Check first 4 bytes of the image to decide if it is UNIF or iNES */
		/* Unfortunately, many .unf files have been released as .nes, so we cannot rely on extensions only */
		memset(magic, '\0', sizeof(magic));
		image_fread(image, magic, 4);

		if ((magic[0] == 'N') && (magic[1] == 'E') && (magic[2] == 'S'))	/* If header starts with 'NES' it is iNES */
		{
			UINT32 prg_size;
			state->format = 1;	// we use this to select between mapper_reset / unif_reset
			state->ines20 = 0;
			state->battery_size = NES_BATTERY_SIZE; // with iNES we can only support 8K WRAM battery (iNES 2.0 might fix this)
			state->prg_ram = 1;	// always map state->wram in bank5 (eventually, this should be enabled only for some mappers) 

			// check if the image is recognized by nes.hsi
			mapinfo = image_extrainfo(image);

			// image_extrainfo() resets the file position back to start.
			// Let's skip past the magic header once again.
			image_fseek(image, 4, SEEK_SET);
			
			image_fread(image, &state->prg_chunks, 1);
			image_fread(image, &state->chr_chunks, 1);
			/* Read the first ROM option byte (offset 6) */
			image_fread(image, &m, 1);
			
			/* Interpret the iNES header flags */
			state->mapper = (m & 0xf0) >> 4;
			local_options = m & 0x0f;
			
			/* Read the second ROM option byte (offset 7) */
			image_fread(image, &m, 1);
			
			switch (m & 0xc)
			{
				case 0x4:
				case 0xc:	
					// probably the header got corrupted: don't trust upper bits for mapper
					break;

				case 0x8:	// it's iNES 2.0 format
					state->ines20 = 1;
				case 0x0:
				default:
					state->mapper = state->mapper | (m & 0xf0);
					break;
			}

			if (mapinfo /* && !state->ines20 */)
			{
				if (4 == sscanf(mapinfo,"%d %d %d %d", &mapint1, &mapint2, &mapint3, &mapint4))
				{
					/* image is present in nes.hsi: overwrite the header settings with these */
					state->mapper = mapint1;
					local_options = mapint2 & 0x0f;
					state->crc_hack = mapint2 & 0xf0;	// this is used to differentiate among variants of the same Mapper (see nes_mmc.c)
					state->prg_chunks = mapint3;
					state->chr_chunks = mapint4;
					logerror("NES.HSI info: %d %d %d %d\n", mapint1, mapint2, mapint3, mapint4);
					goodcrcinfo = 1;
					state->ines20 = 0;
				}
				else
				{
					logerror("NES: [%s], Invalid mapinfo found\n", mapinfo);
				}
			}
			else
			{
				logerror("NES: No extrainfo found\n");
			}

			state->hard_mirroring = (local_options & 0x01) ? PPU_MIRROR_VERT : PPU_MIRROR_HORZ;
			state->battery = local_options & 0x02;
			state->trainer = local_options & 0x04;
			state->four_screen_vram = local_options & 0x08;

			if (state->battery)
				logerror("-- Battery found\n");

			if (state->trainer)
				logerror("-- Trainer found\n");

			if (state->four_screen_vram)
				logerror("-- 4-screen VRAM\n");

			if (state->ines20)
			{
				logerror("Extended iNES format:\n");
				image_fread(image, &extend, 5);
				state->mapper |= (extend[0] & 0x0f) << 8;
				logerror("-- mapper: %d\n", state->mapper);
				logerror("-- submapper: %d\n", (extend[0] & 0xf0) >> 4);
				state->prg_chunks |= ((extend[1] & 0x0f) << 8);
				state->chr_chunks |= ((extend[1] & 0xf0) << 4);
				logerror("-- PRG chunks: %d\n", state->prg_chunks);
				logerror("-- CHR chunks: %d\n", state->chr_chunks);
				logerror("-- PRG NVWRAM: %d\n", extend[2] & 0x0f);
				logerror("-- PRG WRAM: %d\n", (extend[2] & 0xf0) >> 4);
				logerror("-- CHR NVWRAM: %d\n", extend[3] & 0x0f);
				logerror("-- CHR WRAM: %d\n", (extend[3] & 0xf0) >> 4);
				logerror("-- TV System: %d\n", extend[4] & 3);
			}

			/* Free the regions that were allocated by the ROM loader */
			memory_region_free(image->machine, "maincpu");
			memory_region_free(image->machine, "gfx1");

			/* Allocate them again with the proper size */
			prg_size = (state->prg_chunks == 1) ? 2 * 0x4000 : state->prg_chunks * 0x4000;
			memory_region_alloc(image->machine, "maincpu", 0x10000 + prg_size, 0);
			if (state->chr_chunks)
				memory_region_alloc(image->machine, "gfx1", state->chr_chunks * 0x2000, 0);

			state->rom = memory_region(image->machine, "maincpu");
			state->vrom = memory_region(image->machine, "gfx1");
			
			state->vram_chunks = memory_region_length(image->machine, "gfx2") / 0x2000;
			state->vram = memory_region(image->machine, "gfx2");
			// FIXME: this should only be allocated if there is actual wram in the cart (i.e. if state->prg_ram = 1)!
			// or if there is a trainer, I think
			state->wram_size = 0x10000;
			state->wram = auto_alloc_array(image->machine, UINT8, state->wram_size);
			
			/* Position past the header */
			image_fseek(image, 16, SEEK_SET);

			/* Load the 0x200 byte trainer at 0x7000 if it exists */
			if (state->trainer)
				image_fread(image, &state->wram[0x1000], 0x200);

			/* Read in the program chunks */
			image_fread(image, &state->rom[0x10000], 0x4000 * state->prg_chunks);
			if (state->prg_chunks == 1)
				memcpy(&state->rom[0x14000], &state->rom[0x10000], 0x4000);

#if SPLIT_PRG
			{
				FILE *prgout;
				char outname[255];

				sprintf(outname, "%s.prg", image_filename(image));
				prgout = fopen(outname, "wb");
				if (prgout)
				{
					fwrite(&state->rom[0x10000], 1, 0x4000 * state->prg_chunks, prgout);
					printf("Created PRG chunk\n");
				}

				fclose(prgout);
			}
#endif

			logerror("**\n");
			logerror("Mapper: %d\n", state->mapper);
			logerror("PRG chunks: %02x, size: %06x\n", state->prg_chunks, 0x4000 * state->prg_chunks);
			// printf("Mapper: %d\n", state->mapper);
			// printf("PRG chunks: %02x, size: %06x\n", state->prg_chunks, 0x4000 * state->prg_chunks);

			/* Read in any chr chunks */
			if (state->chr_chunks > 0)
			{
				image_fread(image, state->vrom, state->chr_chunks * 0x2000);
				if (state->mapper == 2)
					logerror("Warning: VROM has been found in VRAM-based mapper. Either the mapper is set wrong or the ROM image is incorrect.\n");
			}

#if SPLIT_CHR
			if (state->chr_chunks > 0)
			{
				FILE *chrout;
				char outname[255];

				sprintf(outname, "%s.chr", image_filename(image));
				chrout= fopen(outname, "wb");
				if (chrout)
				{
					fwrite(state->vrom, 1, 0x2000 * state->chr_chunks, chrout);
					printf("Created CHR chunk\n");
				}
				fclose(chrout);
			}
#endif

			logerror("CHR chunks: %02x, size: %06x\n", state->chr_chunks, 0x2000 * state->chr_chunks);
			logerror("**\n");
			// printf("CHR chunks: %02x, size: %06x\n", state->chr_chunks, 0x2000 * state->chr_chunks);
			// printf("**\n");
		}
		else if ((magic[0] == 'U') && (magic[1] == 'N') && (magic[2] == 'I') && (magic[3] == 'F')) /* If header starts with 'UNIF' it is UNIF */
		{
			UINT32 unif_ver = 0;
			char magic2[4];
			UINT8 buffer[4];
			UINT32 chunk_length = 0, read_length = 0x20;
			UINT32 prg_start = 0, chr_start = 0, prg_size;
			char unif_mapr[32];	// here we should store MAPR chunks
			UINT32 size = image_length(image);
			int mapr_chunk_found = 0;
			// allocate space to temporarily store PRG & CHR banks
			UINT8 *temp_prg = auto_alloc_array(image->machine, UINT8, 256 * 0x4000);
			UINT8 *temp_chr = auto_alloc_array(image->machine, UINT8, 256 * 0x2000);

			/* init prg/chr chunks to 0: the exact number of chunks will be determined while reading the file */
			state->prg_chunks = 0;
			state->chr_chunks = 0;

			state->format = 2;	// we use this to select between mapper_reset / unif_reset

			image_fread(image, &buffer, 4);
			unif_ver = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);
			logerror("UNIF file found, version %d\n", unif_ver);

			if (size <= 0x20)
			{
				logerror("%s only contains the UNIF header and no data.\n", image_filename(image));
				return INIT_FAIL;
			}

			do
			{
				image_fseek(image, read_length, SEEK_SET);

				memset(magic2, '\0', sizeof(magic2));
				image_fread(image, &magic2, 4);

				/* We first run through the whole image to find a [MAPR] chunk. This is needed 
				 because, unfortunately, the MAPR chunk is not always the first chunk (see 
				 Super 24-in-1). When such a chunk is found, we set mapr_chunk_found=1 and 
				 we go back to load other chunks! */
				if (!mapr_chunk_found)
				{
					if ((magic2[0] == 'M') && (magic2[1] == 'A') && (magic2[2] == 'P') && (magic2[3] == 'R'))
					{
						mapr_chunk_found = 1;
						logerror("[MAPR] chunk found: ");
						image_fread(image, &buffer, 4);
						chunk_length = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);

						if (chunk_length <= 0x20)
							image_fread(image, &unif_mapr, chunk_length);

						// find out prg/chr size, battery, wram, etc.
						unif_mapr_setup(image->machine, unif_mapr);

						/* now that we found the MAPR chunk, we can go back to load other chunks */
						image_fseek(image, 0x20, SEEK_SET);
						read_length = 0x20;
					}
					else
					{
						logerror("Skip this chunk. We need a [MAPR] chunk before anything else.\n");
						image_fread(image, &buffer, 4);
						chunk_length = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);

						read_length += (chunk_length + 8);
					}
				}
				else
				{
					/* What kind of chunk do we have here? */
					if ((magic2[0] == 'M') && (magic2[1] == 'A') && (magic2[2] == 'P') && (magic2[3] == 'R'))
					{
						/* The [MAPR] chunk has already been read, so we skip it */
						/* TO DO: it would be nice to check if more than one MAPR chunk is present */
						logerror("[MAPR] chunk found (in the 2nd run). Already loaded.\n");
						image_fread(image, &buffer, 4);
						chunk_length = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);

						read_length += (chunk_length + 8);
					}
					else if ((magic2[0] == 'R') && (magic2[1] == 'E') && (magic2[2] == 'A') && (magic2[3] == 'D'))
					{
						logerror("[READ] chunk found. No support yet.\n");
						image_fread(image, &buffer, 4);
						chunk_length = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);

						read_length += (chunk_length + 8);
					}
					else if ((magic2[0] == 'N') && (magic2[1] == 'A') && (magic2[2] == 'M') && (magic2[3] == 'E'))
					{
						logerror("[NAME] chunk found. No support yet.\n");
						image_fread(image, &buffer, 4);
						chunk_length = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);

						read_length += (chunk_length + 8);
					}
					else if ((magic2[0] == 'W') && (magic2[1] == 'R') && (magic2[2] == 'T') && (magic2[3] == 'R'))
					{
						logerror("[WRTR] chunk found. No support yet.\n");
						image_fread(image, &buffer, 4);
						chunk_length = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);

						read_length += (chunk_length + 8);
					}
					else if ((magic2[0] == 'T') && (magic2[1] == 'V') && (magic2[2] == 'C') && (magic2[3] == 'I'))
					{
						logerror("[TVCI] chunk found. No support yet.\n");
						image_fread(image, &buffer, 4);
						chunk_length = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);

						read_length += (chunk_length + 8);
					}
					else if ((magic2[0] == 'D') && (magic2[1] == 'I') && (magic2[2] == 'N') && (magic2[3] == 'F'))
					{
						logerror("[DINF] chunk found. No support yet.\n");
						image_fread(image, &buffer, 4);
						chunk_length = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);

						read_length += (chunk_length + 8);
					}
					else if ((magic2[0] == 'C') && (magic2[1] == 'T') && (magic2[2] == 'R') && (magic2[3] == 'L'))
					{
						logerror("[CTRL] chunk found. No support yet.\n");
						image_fread(image, &buffer, 4);
						chunk_length = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);

						read_length += (chunk_length + 8);
					}
					else if ((magic2[0] == 'B') && (magic2[1] == 'A') && (magic2[2] == 'T') && (magic2[3] == 'R'))
					{
						logerror("[BATR] chunk found. No support yet.\n");
						image_fread(image, &buffer, 4);
						chunk_length = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);

						read_length += (chunk_length + 8);
					}
					else if ((magic2[0] == 'V') && (magic2[1] == 'R') && (magic2[2] == 'O') && (magic2[3] == 'R'))
					{
						logerror("[VROR] chunk found. No support yet.\n");
						image_fread(image, &buffer, 4);
						chunk_length = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);

						read_length += (chunk_length + 8);
					}
					else if ((magic2[0] == 'M') && (magic2[1] == 'I') && (magic2[2] == 'R') && (magic2[3] == 'R'))
					{
						logerror("[MIRR] chunk found. No support yet.\n");
						image_fread(image, &buffer, 4);
						chunk_length = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);

						read_length += (chunk_length + 8);
					}
					else if ((magic2[0] == 'P') && (magic2[1] == 'C') && (magic2[2] == 'K'))
					{
						logerror("[PCK%c] chunk found. No support yet.\n", magic2[3]);
						image_fread(image, &buffer, 4);
						chunk_length = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);

						read_length += (chunk_length + 8);
					}
					else if ((magic2[0] == 'C') && (magic2[1] == 'C') && (magic2[2] == 'K'))
					{
						logerror("[CCK%c] chunk found. No support yet.\n", magic2[3]);
						image_fread(image, &buffer, 4);
						chunk_length = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);

						read_length += (chunk_length + 8);
					}
					else if ((magic2[0] == 'P') && (magic2[1] == 'R') && (magic2[2] == 'G'))
					{
						logerror("[PRG%c] chunk found. ", magic2[3]);
						image_fread(image, &buffer, 4);
						chunk_length = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);

						// FIXME: we currently don't support PRG chunks smaller than 16K!
						state->prg_chunks += (chunk_length / 0x4000);

						if (chunk_length / 0x4000)
							logerror("It consists of %d 16K-blocks.\n", chunk_length / 0x4000);
						else
							logerror("This chunk is smaller than 16K: the emulation might have issues. Please report this file to the MESS forums.\n");

						/* Read in the program chunks */
						image_fread(image, &temp_prg[prg_start], chunk_length);

						prg_start += chunk_length;
						read_length += (chunk_length + 8);
					}
					else if ((magic2[0] == 'C') && (magic2[1] == 'H') && (magic2[2] == 'R'))
					{
						logerror("[CHR%c] chunk found. ", magic2[3]);
						image_fread(image, &buffer, 4);
						chunk_length = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);

						state->chr_chunks += (chunk_length / 0x2000);

						logerror("It consists of %d 8K-blocks.\n", chunk_length / 0x2000);

						/* Read in the vrom chunks */
						image_fread(image, &temp_chr[chr_start], chunk_length);

						chr_start += chunk_length;
						read_length += (chunk_length + 8);
					}
					else
					{
						printf("Unsupported UNIF chunk. Please report the problem at MESS Board.\n");
						read_length = size;
					}
				}
			} while (size > read_length);

			if (!mapr_chunk_found)
			{
				auto_free(image->machine, temp_prg);
				auto_free(image->machine, temp_chr);
				fatalerror("UNIF should have a [MAPR] chunk to work. Check if your image has been corrupted");
			}

			/* Free the regions that were allocated by the ROM loader */
			memory_region_free(image->machine, "maincpu");
			memory_region_free(image->machine, "gfx1");
			memory_region_free(image->machine, "gfx2");
			
			/* Allocate them again, and copy PRG/CHR from temp buffers */
			/* Take care of PRG */
			prg_size = (state->prg_chunks == 1) ? 2 * 0x4000 : state->prg_chunks * 0x4000;
			memory_region_alloc(image->machine, "maincpu", 0x10000 + prg_size, 0);
			state->rom = memory_region(image->machine, "maincpu");
			memcpy(&state->rom[0x10000], &temp_prg[0x00000], state->prg_chunks * 0x4000);
			/* If only a single 16K PRG chunk is present, mirror it! */
			if (state->prg_chunks == 1)
				memcpy(&state->rom[0x14000], &state->rom[0x10000], 0x4000);

			/* Take care of CHR ROM */
			if (state->chr_chunks)
			{
				memory_region_alloc(image->machine, "gfx1", state->chr_chunks * 0x2000, 0);
				state->vrom = memory_region(image->machine, "gfx1");
				memcpy(&state->vrom[0x00000], &temp_chr[0x00000], state->chr_chunks * 0x2000);
			}

			/* Take care of CHR RAM */
			if (state->vram_chunks)
			{
				memory_region_alloc(image->machine, "gfx2", state->vram_chunks * 0x2000, 0);
				state->vram = memory_region(image->machine, "gfx2");
			}

			// FIXME: this should only be allocated if there is actual wram in the cart (i.e. if state->prg_ram = 1)!
			state->wram_size = 0x10000;
			state->wram = auto_alloc_array(image->machine, UINT8, state->wram_size);
				
#if SPLIT_PRG
			{
				FILE *prgout;
				char outname[255];
				
				sprintf(outname, "%s.prg", image_filename(image));
				prgout = fopen(outname, "wb");
				if (prgout)
				{
					fwrite(&state->rom[0x10000], 1, 0x4000 * state->prg_chunks, prgout);
					printf("Created PRG chunk\n");
				}
				
				fclose(prgout);
			}
#endif
			
#if SPLIT_CHR
			if (state->chr_chunks > 0)
			{
				FILE *chrout;
				char outname[255];
				
				sprintf(outname, "%s.chr", image_filename(image));
				chrout= fopen(outname, "wb");
				if (chrout)
				{
					fwrite(state->vrom, 1, 0x2000 * state->chr_chunks, chrout);
					printf("Created CHR chunk\n");
				}
				fclose(chrout);
			}
#endif
			// free the temporary copy of PRG/CHR
			auto_free(image->machine, temp_prg);
			auto_free(image->machine, temp_chr);			
			logerror("UNIF support is only very preliminary.\n");
		}
		else
		{
			logerror("%s is NOT a file in either iNES or UNIF format.\n", image_filename(image));
			return INIT_FAIL;
		}
	}
	else
	{
		UINT32 prg_size = image_get_software_region_length(image, "prg");
		UINT32 chr_size = image_get_software_region_length(image, "chr");
		UINT32 vram_size = image_get_software_region_length(image, "vram");
		vram_size += image_get_software_region_length(image, "vram2");
		
		/* Free the regions that were allocated by the ROM loader */
		memory_region_free(image->machine, "maincpu");
		memory_region_free(image->machine, "gfx1");
		memory_region_free(image->machine, "gfx2");

		/* Allocate them again with the proper size */
		memory_region_alloc(image->machine, "maincpu", 0x10000 + prg_size, 0);

		// validate the xml fields
		if (!prg_size)
			fatalerror("No PRG entry for this software! Please check if the xml list got corrupted");
		if (prg_size < 0x8000)
			fatalerror("PRG entry is too small! Please check if the xml list got corrupted");

		if (chr_size)
			memory_region_alloc(image->machine, "gfx1", chr_size, 0);

		if (vram_size)
			memory_region_alloc(image->machine, "gfx2", vram_size, 0);

		state->rom = memory_region(image->machine, "maincpu");
		state->vrom = memory_region(image->machine, "gfx1");
		state->vram = memory_region(image->machine, "gfx2");

		memcpy(state->rom + 0x10000, image_get_software_region(image, "prg"), prg_size);

		if (chr_size)
			memcpy(state->vrom, image_get_software_region(image, "chr"), chr_size);

		state->prg_chunks = prg_size / 0x4000;
		state->chr_chunks = chr_size / 0x2000;
		state->vram_chunks = vram_size / 0x2000;

		state->format = 3;
		state->mapper = 0;		// this allows us to set up memory handlers without duplicating code (for the moment)
		state->pcb_id = nes_get_pcb_id(image->machine, image_get_feature(image, "pcb"));

		if (state->pcb_id == STD_TVROM || state->pcb_id == STD_DRROM || state->pcb_id == IREM_LROG017)
			state->four_screen_vram = 1;
		else
			state->four_screen_vram = 0;

		state->battery = (image_get_software_region(image, "bwram") != NULL) ? 1 : 0;
		state->battery_size = image_get_software_region_length(image, "bwram");

		if (state->pcb_id == BANDAI_LZ93EX)
		{
			// allocate the 24C01 or 24C02 EEPROM
			state->battery = 1;
			state->battery_size += 0x2000;
		}

		if (state->pcb_id == BANDAI_DATACH)
		{
			// allocate the 24C01 and 24C02 EEPROM
			state->battery = 1;
			state->battery_size += 0x4000;
		}
		
		state->prg_ram = (image_get_software_region(image, "wram") != NULL) ? 1 : 0;
		state->wram_size = image_get_software_region_length(image, "wram");
		state->mapper_ram_size = image_get_software_region_length(image, "mapper_ram");
		if (state->prg_ram)
			state->wram = auto_alloc_array(image->machine, UINT8, state->wram_size);
		if (state->mapper_ram_size)
			state->mapper_ram = auto_alloc_array(image->machine, UINT8, state->mapper_ram_size);

		/* Check for mirroring */
		if (image_get_feature(image, "mirroring") != NULL)
		{
			const char *mirroring = image_get_feature(image, "mirroring");
			if (!strcmp(mirroring, "horizontal"))
				state->hard_mirroring = PPU_MIRROR_HORZ;
			if (!strcmp(mirroring, "vertical"))
				state->hard_mirroring = PPU_MIRROR_VERT;
			if (!strcmp(mirroring, "high"))
				state->hard_mirroring = PPU_MIRROR_HIGH;
			if (!strcmp(mirroring, "low"))
				state->hard_mirroring = PPU_MIRROR_LOW;
		}

		state->chr_open_bus = 0;
		state->ce_mask = 0;
		state->ce_state = 0;
		state->vrc_ls_prg_a = 0;
		state->vrc_ls_prg_b = 0;
		state->vrc_ls_chr = 0;

		/* Check for pins in specific boards which require them */
		if (state->pcb_id == STD_CNROM)
		{
			if (image_get_feature(image, "chr-pin26") != NULL)
			{
				state->ce_mask |= 0x01;
				state->ce_state |= !strcmp(image_get_feature(image, "chr-pin26"), "CE") ? 0x01 : 0;
			}
			if (image_get_feature(image, "chr-pin27") != NULL)
			{
				state->ce_mask |= 0x02;
				state->ce_state |= !strcmp(image_get_feature(image, "chr-pin27"), "CE") ? 0x02 : 0;
			}
		}

		if (state->pcb_id == TAITO_X1_005 && image_get_feature(image, "x1-pin17") != NULL && image_get_feature(image, "x1-pin31") != NULL)
		{
			if (!strcmp(image_get_feature(image, "x1-pin17"), "CIRAM A10") && !strcmp(image_get_feature(image, "x1-pin31"), "NC"))
				state->pcb_id = TAITO_X1_005_A;
		}

		if (state->pcb_id == KONAMI_VRC2)
		{
			state->vrc_ls_prg_a = nes_cart_get_line(image_get_feature(image, "vrc2-pin3"));
			state->vrc_ls_prg_b = nes_cart_get_line(image_get_feature(image, "vrc2-pin4"));
			state->vrc_ls_chr = (nes_cart_get_line(image_get_feature(image, "vrc2-pin21")) != 10) ? 1 : 0;
//			printf("VRC-2, pin3: A%d, pin4: A%d, pin21: %s\n", state->vrc_ls_prg_a, state->vrc_ls_prg_b, state->vrc_ls_chr ? "NC" : "A10");
		}

		if (state->pcb_id == KONAMI_VRC4)
		{
			state->vrc_ls_prg_a = nes_cart_get_line(image_get_feature(image, "vrc4-pin3"));
			state->vrc_ls_prg_b = nes_cart_get_line(image_get_feature(image, "vrc4-pin4"));
//			printf("VRC-4, pin3: A%d, pin4: A%d\n", state->vrc_ls_prg_a, state->vrc_ls_prg_b);
		}
		
		if (state->pcb_id == KONAMI_VRC6)
		{
			state->vrc_ls_prg_a = nes_cart_get_line(image_get_feature(image, "vrc6-pin9"));
			state->vrc_ls_prg_b = nes_cart_get_line(image_get_feature(image, "vrc6-pin10"));
//			printf("VRC-6, pin9: A%d, pin10: A%d\n", state->vrc_ls_prg_a, state->vrc_ls_prg_b);
		}

		/* Check for other misc board variants */
		if (state->pcb_id == STD_SOROM)
		{
			if (image_get_feature(image, "mmc1_type") != NULL && !strcmp(image_get_feature(image, "mmc1_type"), "MMC1A"))
				state->pcb_id = STD_SOROM_A;	// in MMC1-A PRG RAM is always enabled
		}
		
		if (state->pcb_id == STD_SXROM)
		{
			if (image_get_feature(image, "mmc1_type") != NULL && !strcmp(image_get_feature(image, "mmc1_type"), "MMC1A"))
				state->pcb_id = STD_SXROM_A;	// in MMC1-A PRG RAM is always enabled
		}
		
		if (state->pcb_id == STD_NXROM || state->pcb_id == SUNSOFT_DCS)
		{
			if (image_get_software_region(image, "minicart") != NULL)	// check for dual minicart
			{
				state->pcb_id = SUNSOFT_DCS;
				// we shall load somewhere the minicart, but we still do not support this
			}
		}
		
#if 0
		if (state->pcb_id == UNSUPPORTED_BOARD)
			printf("This board (%s) is currently not supported by MESS\n", image_get_feature(image, "pcb"));
		printf("PCB Feature: %s\n", image_get_feature(image, "pcb"));
		printf("PRG chunks: %d\n", state->prg_chunks);
		printf("CHR chunks: %d\n", state->chr_chunks);
		printf("VRAM: Present %s, size: %d\n", state->vram_chunks ? "Yes" : "No", vram_size);
		printf("NVWRAM: Present %s, size: %d\n", state->battery ? "Yes" : "No", state->battery_size);
		printf("WRAM:   Present %s, size: %d\n", state->prg_ram ? "Yes" : "No", state->wram_size);
#endif
	}

	// Attempt to load a battery file for this ROM
	// A few boards have internal RAM with a battery (MMC6, Taito X1-005 & X1-017, etc.)
	if (state->battery || state->mapper_ram_size)
	{
		UINT8 *temp_nvram = auto_alloc_array(image->machine, UINT8, state->battery_size + state->mapper_ram_size);
		image_battery_load(image, temp_nvram, state->battery_size + state->mapper_ram_size, 0x00);
		if (state->battery)
		{
			state->battery_ram = auto_alloc_array(image->machine, UINT8, state->battery_size);
			memcpy(state->battery_ram, temp_nvram, state->battery_size);
		}
		if (state->mapper_ram_size)
			memcpy(state->mapper_ram, temp_nvram + state->battery_size, state->mapper_ram_size);
		
		auto_free(image->machine, temp_nvram);
	}

	return INIT_PASS;
}



void nes_partialhash( char *dest, const unsigned char *data, unsigned long length, unsigned int functions )
{
	if (length <= 16)
		return;
	hash_compute(dest, &data[16], length - 16, functions);
}

/**************************
 
 Disk System emulation
 
**************************/

static void fds_irq( running_device *device, int scanline, int vblank, int blanked )
{
	nes_state *state = (nes_state *)device->machine->driver_data;
	
	if (state->IRQ_enable_latch)
		cpu_set_input_line(state->maincpu, M6502_IRQ_LINE, HOLD_LINE);
	
	if (state->IRQ_enable)
	{
		if (state->IRQ_count <= 114)
		{
			cpu_set_input_line(state->maincpu, M6502_IRQ_LINE, HOLD_LINE);
			state->IRQ_enable = 0;
			state->fds_status0 |= 0x01;
		}
		else
			state->IRQ_count -= 114;
	}
}

static READ8_HANDLER( nes_fds_r )
{
	nes_state *state = (nes_state *)space->machine->driver_data;
	UINT8 ret = 0x00;
	
	switch (offset)
	{
		case 0x00: /* $4030 - disk status 0 */
			ret = state->fds_status0;
			/* clear the disk IRQ detect flag */
			state->fds_status0 &= ~0x01;
			break;
		case 0x01: /* $4031 - data latch */
			/* don't read data if disk is unloaded */
			if (state->fds_data == NULL)
				ret = 0;
			else if (state->fds_current_side)
				ret = state->fds_data[(state->fds_current_side - 1) * 65500 + state->fds_head_position++];
			else
				ret = 0;
			break;
		case 0x02: /* $4032 - disk status 1 */
			/* return "no disk" status if disk is unloaded */
			if (state->fds_data == NULL)
				ret = 1;
			else if (state->fds_last_side != state->fds_current_side)
			{
				/* If we've switched disks, report "no disk" for a few reads */
				ret = 1;
				state->fds_count++;
				if (state->fds_count == 50)
				{
					state->fds_last_side = state->fds_current_side;
					state->fds_count = 0;
				}
			}
			else
				ret = (state->fds_current_side == 0); /* 0 if a disk is inserted */
			break;
		case 0x03: /* $4033 */
			ret = 0x80;
			break;
		default:
			ret = 0x00;
			break;
	}

	return ret;
}

static WRITE8_HANDLER( nes_fds_w )
{
	nes_state *state = (nes_state *)space->machine->driver_data;
	
	switch (offset)
	{
		case 0x00:
			state->IRQ_count_latch = (state->IRQ_count_latch & 0xff00) | data;
			break;
		case 0x01:
			state->IRQ_count_latch = (state->IRQ_count_latch & 0x00ff) | (data << 8);
			break;
		case 0x02:
			state->IRQ_count = state->IRQ_count_latch;
			state->IRQ_enable = data;
			break;
		case 0x03:
			// d0 = sound io (1 = enable)
			// d1 = disk io (1 = enable)
			break;
		case 0x04:
			/* write data out to disk */
			break;
		case 0x05:
			state->fds_motor_on = BIT(data, 0);
			
			if (BIT(data, 1))
				state->fds_head_position = 0;
			
			state->fds_read_mode = BIT(data, 2);
			set_nt_mirroring(space->machine, BIT(data, 3) ? PPU_MIRROR_HORZ : PPU_MIRROR_VERT);
			
			if ((!(data & 0x40)) && (state->fds_write_reg & 0x40))
				state->fds_head_position -= 2; // ???
			
			state->IRQ_enable_latch = BIT(data, 7);
			state->fds_write_reg = data;
			break;
	}
}

static void nes_load_proc( running_device *image )
{
	nes_state *state = (nes_state *)image->machine->driver_data;
	int header = 0;
	state->fds_sides = 0;

	if (image_length(image) % 65500)
		header = 0x10;

	state->fds_sides = (image_length(image) - header) / 65500;

	if (state->fds_data == NULL)
		state->fds_data = (UINT8*)image_malloc(image, state->fds_sides * 65500);	// I don't think we can arrive here ever, probably it can be removed...

	/* if there is an header, skip it */
	image_fseek(image, header, SEEK_SET);

	image_fread(image, state->fds_data, 65500 * state->fds_sides);
	return;
}

static void nes_unload_proc( running_device *image )
{
	nes_state *state = (nes_state *)image->machine->driver_data;

	/* TODO: should write out changes here as well */
	state->fds_sides =  0;
}

DRIVER_INIT( famicom )
{
	nes_state *state = (nes_state *)machine->driver_data;
	int i;

	/* clear some of the variables we don't use */
	state->trainer = 0;
	state->battery = 0;
	state->prg_ram = 0;
	state->four_screen_vram = 0;
	state->hard_mirroring = 0;
	state->prg_chunks = state->chr_chunks = 0;

	/* initialize the disk system */
	state->disk_expansion = 1;
	state->pcb_id = NO_BOARD;

	state->fds_sides = 0;
	state->fds_last_side = 0;
	state->fds_count = 0;
	state->fds_motor_on = 0;
	state->fds_door_closed = 0;
	state->fds_current_side = 1;
	state->fds_head_position = 0;
	state->fds_status0 = 0;
	state->fds_read_mode = state->fds_write_reg = 0;
	
	state->fds_data = auto_alloc_array_clear(machine, UINT8, 65500 * 2);
	state->fds_ram = auto_alloc_array_clear(machine, UINT8, 0x8000);
	state_save_register_global_pointer(machine, state->fds_ram, 0x8000);

	// setup CHR accesses to 8k VRAM
	state->vram = memory_region(machine, "gfx2");
	for (i = 0; i < 8; i++)
	{
		state->chr_map[i].source = CHRRAM;
		state->chr_map[i].origin = i * 0x400; // for save state uses!
		state->chr_map[i].access = &state->vram[state->chr_map[i].origin];
	}
	
	floppy_install_load_proc(floppy_get_device(machine, 0), nes_load_proc);
	floppy_install_unload_proc(floppy_get_device(machine, 0), nes_unload_proc);
}

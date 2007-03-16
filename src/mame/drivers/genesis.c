/* Some Generic Sega Genesis stuff used by Megatech / Megaplay etc. */

/*

Genesis hardware games are in

megatech.c
megaplay.c
gene_sm.c
segac2.c

*/

#include "driver.h"
#include "genesis.h"

#define MASTER_CLOCK		53693100


/******************** Sega Genesis ******************************/

/* Genesis based */
static unsigned int	z80_68000_latch			= 0;
static unsigned int	z80_latch_bitcount		= 0;

/* interrupt states */
static UINT8		irq2_int;			/* INT2 */
static UINT8		scanline_int;		/* INT4 - programmable */
static UINT8		vblank_int;			/* INT6 - on every VBLANK */

static mame_timer *	scan_timer;


static int z80running;
UINT16 *genesis_68k_ram;
unsigned char *genesis_z80_ram;



/******************************************************************************
    Interrupt handling
*******************************************************************************

    The Genesis uses 2 Different Interrupts, IRQ4 and IRQ6.
    The C2 system uses IRQ2 as well.

    IRQ6 = Vblank, this happens after the last visible line of the display has
            been drawn (after line 224)

    IRQ4 = H-Int, this happens based upon the value in H-Int Counter.  If the
            Horizontal Interrupt is enabled and the Counter Value = 0 there
            will be a Level 4 Interrupt Triggered

    IRQ2 = sound int, generated by the YM3438

    --------

    More H-Counter Information:

    Providing Horizontal Interrupts are active the H-Counter will be loaded
    with the value stored in register #10 (0x0A) at the following times:
        (1) At the top of the display, before drawing the first line
        (2) When the counter has expired
        (3) During the VBlank Period (lines 224-261)
    The Counter is decreased by 1 after every line.

******************************************************************************/

/* call this whenever the interrupt state has changed */
static void update_interrupts(void)
{
	int level = 0;

	/* determine which interrupt is active */
	if (irq2_int) level = 2;
	if (scanline_int) level = 4;
	if (vblank_int) level = 6;

	/* either set or clear the appropriate lines */
	if (level)
		cpunum_set_input_line(0, level, ASSERT_LINE);
	else
		cpunum_set_input_line(0, 7, CLEAR_LINE);
}


/* timer callback to turn off the IRQ4 signal after a short while */
static void vdp_int4_off(int param)
{
	scanline_int = 0;
	update_interrupts();
}


/* timer callback to handle reloading the H counter and generate IRQ4 */
void vdp_reload_counter(int scanline)
{
	/* generate an int if they're enabled */
	if (genesis_vdp_regs[0] & 0x10)/* && !(misc_io_data[7] & 0x10))*/
		if (scanline != 0 || genesis_vdp_regs[10] == 0)
		{
			scanline_int = 1;
			update_interrupts();
			mame_timer_set(video_screen_get_time_until_pos(0, scanline + 1, 0), 0, vdp_int4_off);
		}

	/* advance to the next scanline */
	/* behavior 2: 0 count means interrupt after one scanline */
	/* (this behavior matches the Sega C2 emulator) */
	scanline += genesis_vdp_regs[10] + 1;
	if (scanline >= 224)
		scanline = 0;

	/* set a timer */
	mame_timer_adjust(scan_timer, video_screen_get_time_until_pos(0, scanline, 320), scanline, time_zero);
}


/* timer callback to turn off the IRQ6 signal after a short while */
static void vdp_int6_off(int param)
{
	vblank_int = 0;
	update_interrupts();
}


/* interrupt callback to generate the VBLANK interrupt */
INTERRUPT_GEN( genesis_vblank_interrupt )
{
	/* generate the interrupt */
	vblank_int = 1;
	update_interrupts();

	/* set a timer to turn it off */
	mame_timer_set(video_screen_get_time_until_pos(0, video_screen_get_vpos(0), 22), 0, vdp_int6_off);
}


/* interrupt callback to generate the YM3438 interrupt */
void genesis_irq2_interrupt(int state)
{
	irq2_int = state;
	update_interrupts();
}


MACHINE_START( genesis )
{
	state_save_register_global(irq2_int);
	state_save_register_global(scanline_int);
	state_save_register_global(vblank_int);
	return 0;
}


MACHINE_RESET( genesis )
{
	/* C2 doesn't have a Z80, so we can't just assume */
	if (Machine->drv->cpu[1].cpu_type == CPU_Z80)
	{
	    /* the following ensures that the Z80 begins without running away from 0 */
		/* 0x76 is just a forced 'halt' as soon as the CPU is initially run */
	    genesis_z80_ram[0] = 0x76;
		genesis_z80_ram[0x38] = 0x76;

		cpunum_set_input_line(1, INPUT_LINE_HALT, ASSERT_LINE);

		z80running = 0;
	}

	logerror("Machine init\n");

	/* set the first scanline 0 timer to go off */
	scan_timer = mame_timer_alloc(vdp_reload_counter);
	mame_timer_adjust(scan_timer, video_screen_get_time_until_pos(0, 0, 320), 0, time_zero);
}


/* from MESS */
READ16_HANDLER(genesis_ctrl_r)
{
/*  int returnval; */

/*  logerror("genesis_ctrl_r %x\n", offset); */
	switch (offset)
	{
	case 0:							/* DRAM mode is write only */
		return 0xffff;
		break;
	case 0x80:						/* return Z80 CPU Function Stop Accessible or not */
		/* logerror("Returning z80 state\n"); */
		return (z80running ? 0x0100 : 0x0);
		break;
	case 0x100:						/* Z80 CPU Reset - write only */
		return 0xffff;
		break;
	}
	return 0x00;

}

/* from MESS */
WRITE16_HANDLER(genesis_ctrl_w)
{
	data &= ~mem_mask;

/*  logerror("genesis_ctrl_w %x, %x\n", offset, data); */

	switch (offset)
	{
	case 0:							/* set DRAM mode... we have to ignore this for production cartridges */
		return;
		break;
	case 0x80:						/* Z80 BusReq */
		if (data == 0x100)
		{
			z80running = 0;
			cpunum_set_input_line(1, INPUT_LINE_HALT, ASSERT_LINE);	/* halt Z80 */
			/* logerror("z80 stopped by 68k BusReq\n"); */
		}
		else
		{
			z80running = 1;
//          memory_set_bankptr(1, &genesis_z80_ram[0]);

			cpunum_set_input_line(1, INPUT_LINE_HALT, CLEAR_LINE);
			/* logerror("z80 started, BusReq ends\n"); */
		}
		return;
		break;
	case 0x100:						/* Z80 CPU Reset */
		if (data == 0x00)
		{
			cpunum_set_input_line(1, INPUT_LINE_HALT, ASSERT_LINE);
			cpunum_set_input_line(1, INPUT_LINE_RESET, PULSE_LINE);

			cpunum_set_input_line(1, INPUT_LINE_HALT, ASSERT_LINE);
			/* logerror("z80 reset, ram is %p\n", &genesis_z80_ram[0]); */
			z80running = 0;
			return;
		}
		else
		{
			/* logerror("z80 out of reset\n"); */
		}
		return;

		break;
	}
}

READ16_HANDLER ( genesis_68k_to_z80_r )
{
	offset *= 2;
	offset &= 0x7fff;

	/* Shared Ram */
	if ((offset >= 0x0000) && (offset <= 0x3fff))
	{
		offset &=0x1fff;
//      logerror("soundram_r returning %x\n",(gen_z80_shared[offset] << 8) + gen_z80_shared[offset+1]);
		return (genesis_z80_ram[offset] << 8) + genesis_z80_ram[offset+1];
	}


	/* YM2610 */
	if ((offset >= 0x4000) && (offset <= 0x5fff))
	{
		switch (offset & 3)
		{
		case 0:
			if (ACCESSING_MSB)	 return YM3438_status_port_0_A_r(0) << 8;
			else 				 return YM3438_read_port_0_r(0);
			break;
		case 2:
			if (ACCESSING_MSB)	return YM3438_status_port_0_B_r(0) << 8;
			else 				return 0;
			break;
		}
	}

	/* Bank Register */
	if ((offset >= 0x6000) && (offset <= 0x60ff))
	{

	}

	/* Unused / Illegal */
	if ((offset >= 0x6100) && (offset <= 0x7eff))
	{
		/* nothing */
	}

	/* VDP */
	if ((offset >= 0x7f00) && (offset <= 0x7fff))
	{

	}

	return 0x0000;
}


READ16_HANDLER ( megaplay_68k_to_z80_r )
{
	offset *= 2;
	offset &= 0x7fff;

	/* Shared Ram */
	if ((offset >= 0x0000) && (offset <= 0x1fff))
	{
		offset &=0x1fff;
//      logerror("soundram_r returning %x\n",(gen_z80_shared[offset] << 8) + gen_z80_shared[offset+1]);
		return (genesis_z80_ram[offset] << 8) + genesis_z80_ram[offset+1];
	}

	if ((offset >= 0x2000) && (offset <= 0x3fff))
	{
		offset &=0x1fff;
//      if(offset == 0)
//          return (readinputport(8) << 8) ^ 0xff00;
		return (ic36_ram[offset] << 8) + ic36_ram[offset+1];
	}


	/* YM2610 */
	if ((offset >= 0x4000) && (offset <= 0x5fff))
	{
		switch (offset & 3)
		{
		case 0:
			if (ACCESSING_MSB)	 return YM3438_status_port_0_A_r(0) << 8;
			else 				 return YM3438_read_port_0_r(0);
			break;
		case 2:
			if (ACCESSING_MSB)	return YM3438_status_port_0_B_r(0) << 8;
			else 				return 0;
			break;
		}
	}

	/* Bank Register */
	if ((offset >= 0x6000) && (offset <= 0x60ff))
	{

	}

	/* Unused / Illegal */
	if ((offset >= 0x6100) && (offset <= 0x7eff))
	{
		/* nothing */
	}

	/* VDP */
	if ((offset >= 0x7f00) && (offset <= 0x7fff))
	{

	}

	return 0x0000;
}

WRITE16_HANDLER ( megaplay_68k_to_z80_w )
{
	offset *= 2;
	offset &= 0x7fff;

	/* Shared Ram */
	if ((offset >= 0x0000) && (offset <= 0x1fff))
	{
		offset &=0x1fff;

	if (ACCESSING_LSB) genesis_z80_ram[offset+1] = data & 0xff;
	if (ACCESSING_MSB) genesis_z80_ram[offset] = (data >> 8) & 0xff;
	}

	if ((offset >= 0x2000) && (offset <= 0x3fff))
	{
		offset &=0x1fff;

	if (ACCESSING_LSB) ic36_ram[offset+1] = data & 0xff;
	if (ACCESSING_MSB) ic36_ram[offset] = (data >> 8) & 0xff;
	}


	/* YM2610 */
	if ((offset >= 0x4000) && (offset <= 0x5fff))
	{
		switch (offset & 3)
		{
		case 0:
			if (ACCESSING_MSB)	YM3438_control_port_0_A_w	(0,	(data >> 8) & 0xff);
			else 				YM3438_data_port_0_A_w		(0,	(data >> 0) & 0xff);
			break;
		case 2:
			if (ACCESSING_MSB)	YM3438_control_port_0_B_w	(0,	(data >> 8) & 0xff);
			else 				YM3438_data_port_0_B_w		(0,	(data >> 0) & 0xff);
			break;
		}
	}

	/* Bank Register */
	if ((offset >= 0x6000) && (offset <= 0x60ff))
	{

	}

	/* Unused / Illegal */
	if ((offset >= 0x6100) && (offset <= 0x7eff))
	{
		/* nothing */
	}

	/* VDP */
	if ((offset >= 0x7f00) && (offset <= 0x7fff))
	{
		offset &= 0x1f;

		if ( (offset >= 0x10) && (offset <=0x17) )
		{
			if (ACCESSING_LSB) SN76496_0_w(0, data & 0xff);
			if (ACCESSING_MSB) SN76496_0_w(0, (data >>8) & 0xff);
		}

	}
}

/* Gen I/O */

/*
cgfm info

$A10001 Version
$A10003 Port A data
$A10005 Port B data
$A10007 Port C data
$A10009 Port A control
$A1000B Port B control
$A1000D Port C control
$A1000F Port A TxData
$A10011 Port A RxData
$A10013 Port A serial control
$A10015 Port B TxData
$A10017 Port B RxData
$A10019 Port B serial control
$A1001B Port C TxData
$A1001D Port C RxData
$A1001F Port C serial control

*/

UINT16 *genesis_io_ram;

READ16_HANDLER ( genesis_io_r )
{
	/* 8-bit only, data is mirrored in both halves */

	UINT8 return_value = 0;

	switch (offset)
	{
		case 0:
		/* Charles MacDonald ( http://cgfm2.emuviews.com/ )
            D7 : Console is 1= Export (USA, Europe, etc.) 0= Domestic (Japan)
            D6 : Video type is 1= PAL, 0= NTSC
            D5 : Sega CD unit is 1= not present, 0= connected.
            D4 : Unused (always returns zero)
            D3 : Bit 3 of version number
            D2 : Bit 2 of version number
            D1 : Bit 1 of version number
            D0 : Bit 0 of version number
        */
			return_value = 0x80; /* ? megatech is usa? */
			break;

		case 1: /* port A data (joypad 1) */

			if (genesis_io_ram[offset] & 0x40)
			{
				int iport = readinputport(9);
				return_value = iport & 0x3f;
			}
			else
			{
				int iport1 = readinputport(12);
				int iport2 = readinputport(7) >> 1;
				return_value = (iport1 & 0x10) + (iport2 & 0x20);
				if(iport1 & 0x10 || iport2 & 0x20)
					return_value+=1;
			}

			return_value = (genesis_io_ram[offset] & 0x80) | return_value;
//          logerror ("reading joypad 1 , type %02x %02x\n",genesis_io_ram[offset] & 0x80, return_value &0x7f);
			if(bios_ctrl_inputs & 0x04) return_value = 0xff;
			break;

		case 2: /* port B data (joypad 2) */

			if (genesis_io_ram[offset] & 0x40)
			{
				int iport1 = (readinputport(9) & 0xc0) >> 6;
				int iport2 = (readinputport(8) & 0x0f) << 2;
				return_value = (iport1 + iport2) & 0x3f;
			}
			else
			{
				int iport1 = readinputport(12) << 2;
				int iport2 = readinputport(7) >> 2;
				return_value = (iport1 & 0x10) + (iport2 & 0x20);
				if(iport1 & 0x10 || iport2 & 0x20)
					return_value+=1;
			}
			return_value = (genesis_io_ram[offset] & 0x80) | return_value;
//          logerror ("reading joypad 2 , type %02x %02x\n",genesis_io_ram[offset] & 0x80, return_value &0x7f);
			if(bios_ctrl_inputs & 0x04) return_value = 0xff;
			break;

		default:
			return_value = 0xe0;

	}
	return return_value | return_value << 8;
}

READ16_HANDLER ( megaplay_genesis_io_r )
{
	/* 8-bit only, data is mirrored in both halves */

	UINT8 return_value = 0;

	switch (offset)
	{
		case 0:
		/* Charles MacDonald ( http://cgfm2.emuviews.com/ )
            D7 : Console is 1= Export (USA, Europe, etc.) 0= Domestic (Japan)
            D6 : Video type is 1= PAL, 0= NTSC
            D5 : Sega CD unit is 1= not present, 0= connected.
            D4 : Unused (always returns zero)
            D3 : Bit 3 of version number
            D2 : Bit 2 of version number
            D1 : Bit 1 of version number
            D0 : Bit 0 of version number
        */
			return_value = 0x80; /* ? megatech is usa? */
			break;

		case 1: /* port A data (joypad 1) */

			if (genesis_io_ram[offset] & 0x40)
				return_value = readinputport(1) & (genesis_io_ram[4]^0xff);
			else
			{
				return_value = readinputport(2) & (genesis_io_ram[4]^0xff);
				return_value |= readinputport(1) & 0x03;
			}
			return_value = (genesis_io_ram[offset] & 0x80) | return_value;
//          logerror ("reading joypad 1 , type %02x %02x\n",genesis_io_ram[offset] & 0xb0, return_value &0x7f);
			break;

		case 2: /* port B data (joypad 2) */

			if (genesis_io_ram[offset] & 0x40)
				return_value = readinputport(3) & (genesis_io_ram[5]^0xff);
			else
			{
				return_value = readinputport(4) & (genesis_io_ram[5]^0xff);
				return_value |= readinputport(3) & 0x03;
			}
			return_value = (genesis_io_ram[offset] & 0x80) | return_value;
//          logerror ("reading joypad 2 , type %02x %02x\n",genesis_io_ram[offset] & 0xb0, return_value &0x7f);
			break;

//      case 3: /* port C data */
//          return_value = bios_6402 << 3;
//          break;

	default:
			return_value = genesis_io_ram[offset];

	}
	return return_value | return_value << 8;
}

WRITE16_HANDLER ( genesis_io_w )
{
//  logerror ("write io offset :%02x data %04x PC: 0x%06x\n",offset,data,activecpu_get_previouspc());

	switch (offset)
	{
		case 0x00:
		/*??*/
		break;

		case 0x01:/* port A data */
		genesis_io_ram[offset] = (data & (genesis_io_ram[0x04])) | (genesis_io_ram[offset] & ~(genesis_io_ram[0x04]));
		break;

		case 0x02: /* port B data */
		genesis_io_ram[offset] = (data & (genesis_io_ram[0x05])) | (genesis_io_ram[offset] & ~(genesis_io_ram[0x05]));
		break;

		case 0x03: /* port C data */
		genesis_io_ram[offset] = (data & (genesis_io_ram[0x06])) | (genesis_io_ram[offset] & ~(genesis_io_ram[0x06]));
		bios_6204 = data & 0x07;
		break;

		case 0x04: /* port A control */
		genesis_io_ram[offset] = data;
		break;

		case 0x05: /* port B control */
		genesis_io_ram[offset] = data;
		break;

		case 0x06: /* port C control */
		genesis_io_ram[offset] = data;
		break;

		case 0x07: /* port A TxData */
		genesis_io_ram[offset] = data;
		break;

		default:
		genesis_io_ram[offset] = data;
	}

}

static ADDRESS_MAP_START( genesis_readmem, ADDRESS_SPACE_PROGRAM, 16 )
	AM_RANGE(0x000000, 0x3fffff) AM_READ(MRA16_ROM)					/* Cartridge Program Rom */
	AM_RANGE(0xa10000, 0xa1001f) AM_READ(genesis_io_r)				/* Genesis Input */
	AM_RANGE(0xa00000, 0xa0ffff) AM_READ(genesis_68k_to_z80_r)
	AM_RANGE(0xc00000, 0xc0001f) AM_READ(genesis_vdp_r)				/* VDP Access */
	AM_RANGE(0xfe0000, 0xfeffff) AM_READ(MRA16_BANK3)				/* Main Ram */
	AM_RANGE(0xff0000, 0xffffff) AM_READ(MRA16_RAM)					/* Main Ram */
ADDRESS_MAP_END


static ADDRESS_MAP_START( genesis_writemem, ADDRESS_SPACE_PROGRAM, 16 )
	AM_RANGE(0x000000, 0x3fffff) AM_WRITE(MWA16_ROM)					/* Cartridge Program Rom */
	AM_RANGE(0xa10000, 0xa1001f) AM_WRITE(genesis_io_w) AM_BASE(&genesis_io_ram)				/* Genesis Input */
	AM_RANGE(0xa11000, 0xa11203) AM_WRITE(genesis_ctrl_w)
	AM_RANGE(0xa00000, 0xa0ffff) AM_WRITE(megaplay_68k_to_z80_w)
	AM_RANGE(0xc00000, 0xc0001f) AM_WRITE(genesis_vdp_w)				/* VDP Access */
	AM_RANGE(0xfe0000, 0xfeffff) AM_WRITE(MWA16_BANK3)				/* Main Ram */
	AM_RANGE(0xff0000, 0xffffff) AM_WRITE(MWA16_RAM) AM_BASE(&genesis_68k_ram)/* Main Ram */
ADDRESS_MAP_END

/* Z80 Sound Hardware - based on MESS code, to be improved, it can do some strange things */

#ifdef LSB_FIRST
	#define BYTE_XOR(a) ((a) ^ 1)
#else
	#define BYTE_XOR(a) (a)
#endif



static WRITE8_HANDLER ( genesis_bank_select_w ) /* note value will be meaningless unless all bits are correctly set in */
{
	if (offset !=0 ) return;
//  if (!z80running) logerror("undead Z80 latch write!\n");
	if (z80_latch_bitcount == 0) z80_68000_latch = 0;

	z80_68000_latch = z80_68000_latch | ((( ((unsigned char)data) & 0x01) << (15+z80_latch_bitcount)));
 	logerror("value %x written to latch\n", data);
	z80_latch_bitcount++;
	if (z80_latch_bitcount == 9)
	{
		z80_latch_bitcount = 0;
		logerror("latch set, value %x\n", z80_68000_latch);
	}
}

READ8_HANDLER ( genesis_z80_r )
{
	offset += 0x4000;

	/* YM2610 */
	if ((offset >= 0x4000) && (offset <= 0x5fff))
	{
		switch (offset & 3)
		{
		case 0: return YM3438_status_port_0_A_r(0);
		case 1: return YM3438_read_port_0_r(0);
		case 2: return YM3438_status_port_0_B_r(0);
		case 3: return 0;
		}
	}

	/* Bank Register */
	if ((offset >= 0x6000) && (offset <= 0x60ff))
	{

	}

	/* Unused / Illegal */
	if ((offset >= 0x6100) && (offset <= 0x7eff))
	{
		/* nothing */
	}

	/* VDP */
	if ((offset >= 0x7f00) && (offset <= 0x7fff))
	{

	}

	return 0x00;
}

WRITE8_HANDLER ( genesis_z80_w )
{
	offset += 0x4000;

	/* YM2610 */
	if ((offset >= 0x4000) && (offset <= 0x5fff))
	{
		switch (offset & 3)
		{
		case 0: YM3438_control_port_0_A_w	(0,	data);
			break;
		case 1: YM3438_data_port_0_A_w		(0, data);
			break;
		case 2: YM3438_control_port_0_B_w	(0,	data);
			break;
		case 3: YM3438_data_port_0_B_w		(0,	data);
			break;
		}
	}

	/* Bank Register */
	if ((offset >= 0x6000) && (offset <= 0x60ff))
	{
		genesis_bank_select_w(offset & 0xff, data);
	}

	/* Unused / Illegal */
	if ((offset >= 0x6100) && (offset <= 0x7eff))
	{
		/* nothing */
	}

	/* VDP */
	if ((offset >= 0x7f00) && (offset <= 0x7fff))
	{

	}
}

READ8_HANDLER ( genesis_z80_bank_r )
{
	int address = (z80_68000_latch) + (offset & 0x7fff);

	if (!z80running) logerror("undead Z80->68000 read!\n");

	if (z80_latch_bitcount != 0) logerror("reading whilst latch being set!\n");

	logerror("z80 read from address %x\n", address);

	/* Read the data out of the 68k ROM */
	if (address < 0x400000) return memory_region(REGION_CPU1)[BYTE_XOR(address)];
	/* else read the data out of the 68k RAM */
//  else if (address > 0xff0000) return genesis_68k_ram[BYTE_XOR(offset)];

	return -1;
}



static ADDRESS_MAP_START( genesis_z80_readmem, ADDRESS_SPACE_PROGRAM, 8 )
 	AM_RANGE(0x0000, 0x1fff) AM_READ(MRA8_BANK1)
 	AM_RANGE(0x2000, 0x3fff) AM_READ(MRA8_BANK2) /* mirror */
	AM_RANGE(0x4000, 0x7fff) AM_READ(genesis_z80_r)
	AM_RANGE(0x8000, 0xffff) AM_READ(genesis_z80_bank_r)
ADDRESS_MAP_END

static ADDRESS_MAP_START( genesis_z80_writemem, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x1fff) AM_WRITE(MWA8_BANK1) AM_BASE(&genesis_z80_ram)
 	AM_RANGE(0x2000, 0x3fff) AM_WRITE(MWA8_BANK2) /* mirror */
	AM_RANGE(0x4000, 0x7fff) AM_WRITE(genesis_z80_w)
 // AM_RANGE(0x8000, 0xffff) AM_WRITE(genesis_z80_bank_w)
ADDRESS_MAP_END

static MACHINE_DRIVER_START( genesis_base )
	/*basic machine hardware */
	MDRV_CPU_ADD_TAG("main", M68000, MASTER_CLOCK / 7)
	MDRV_CPU_PROGRAM_MAP(genesis_readmem, genesis_writemem)
	MDRV_CPU_VBLANK_INT(genesis_vblank_interrupt,1)

	MDRV_CPU_ADD_TAG("sound", Z80, MASTER_CLOCK / 15)
	MDRV_CPU_PROGRAM_MAP(genesis_z80_readmem, genesis_z80_writemem)
	MDRV_CPU_VBLANK_INT(irq0_line_hold, 1) /* from vdp at scanline 0xe0 */

	MDRV_SCREEN_REFRESH_RATE(60)

	MDRV_INTERLEAVE(100)

	MDRV_MACHINE_START(genesis)
	MDRV_MACHINE_RESET(genesis)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_HAS_SHADOWS | VIDEO_HAS_HIGHLIGHTS)
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_SIZE(342,262)
	MDRV_SCREEN_VISIBLE_AREA(0, 319, 0, 223)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_START(genesis)
	MDRV_VIDEO_UPDATE(genesis)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")

	MDRV_SOUND_ADD(YM3438, MASTER_CLOCK/7)
	MDRV_SOUND_ROUTE(0, "mono", 0.50)
	MDRV_SOUND_ROUTE(1, "mono", 0.50)
MACHINE_DRIVER_END


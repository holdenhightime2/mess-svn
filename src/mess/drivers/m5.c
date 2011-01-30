/***************************************************************************

    Sord m.5

	http://www.retropc.net/mm/m5/
	http://www.museo8bits.es/wiki/index.php/Sord_M5
	http://k5.web.klfree.net/content/view/10/11/
	http://k5.web.klfree.net/images/stories/sord/m5heap.htm

****************************************************************************/

/*

    TODO:

	- floppy
	- SI-5 serial interface (8251, ROM)
	- 64KB RAM expansions
	- PAL mode

*/

#define ADDRESS_MAP_MODERN

#include "emu.h"
#include "cpu/z80/z80.h"
#include "cpu/z80/z80daisy.h"
#include "formats/basicdsk.h"
#include "formats/sord_cas.h"
#include "imagedev/cartslot.h"
#include "imagedev/cassette.h"
#include "imagedev/flopdrv.h"
#include "machine/ctronics.h"
#include "machine/i8255a.h"
#include "machine/ram.h"
#include "machine/upd765.h"
#include "machine/z80ctc.h"
#include "sound/sn76496.h"
#include "video/tms9928a.h"
#include "includes/m5.h"



//**************************************************************************
//	MEMORY BANKING
//**************************************************************************

//-------------------------------------------------
//  sts_r - 
//-------------------------------------------------

READ8_MEMBER( m5_state::sts_r )
{
	/*

		bit		description
		
		0		cassette input
		1		busy
		2		
		3		
		4		
		5		
		6		
		7		RESET key

	*/

	UINT8 data = 0;

	// cassette input
	data |= cassette_input(m_cassette) >= 0 ? 1 : 0;

	// centronics busy
	data |= centronics_busy_r(m_centronics) << 1;

	// RESET key
	data |= input_port_read(machine, "RESET");
	
	return data;
}


//-------------------------------------------------
//  com_w - 
//-------------------------------------------------

WRITE8_MEMBER( m5_state::com_w )
{
	/*

		bit		description
		
		0		cassette output, centronics strobe
		1		cassette remote
		2		
		3		
		4		
		5		
		6		
		7		

	*/

	// cassette output
	cassette_output(m_cassette, BIT(data, 0) ? -1.0 : 1.0);

	// centronics strobe
	centronics_strobe_w(m_centronics, BIT(data, 0));

	// cassette remote
	cassette_change_state(m_cassette, BIT(data, 1) ? CASSETTE_MOTOR_ENABLED : CASSETTE_MOTOR_DISABLED, CASSETTE_MASK_MOTOR);
}



//**************************************************************************
//	FD-5
//**************************************************************************

//-------------------------------------------------
//  fd5_data_r - 
//-------------------------------------------------

READ8_MEMBER( m5_state::fd5_data_r )
{
	return m_fd5_data;
}


//-------------------------------------------------
//  fd5_data_w - 
//-------------------------------------------------

WRITE8_MEMBER( m5_state::fd5_data_w )
{
	m_fd5_data = data;
}


//-------------------------------------------------
//  fd5_com_r - 
//-------------------------------------------------

READ8_MEMBER( m5_state::fd5_com_r )
{
	/*

		bit		description
		
		0		
		1		
		2		
		3		
		4		
		5		
		6		
		7		

	*/

	return 0;
}


//-------------------------------------------------
//  fd5_com_w - 
//-------------------------------------------------

WRITE8_MEMBER( m5_state::fd5_com_w )
{
	/*

		bit		description
		
		0		
		1		
		2		
		3		
		4		
		5		
		6		
		7		

	*/
}


//-------------------------------------------------
//  fd5_com_w - 
//-------------------------------------------------

WRITE8_MEMBER( m5_state::fd5_ctrl_w )
{
	/*

		bit		description
		
		0		
		1		
		2		
		3		
		4		
		5		
		6		
		7		

	*/
}


//-------------------------------------------------
//  fd5_com_w - 
//-------------------------------------------------

WRITE8_MEMBER( m5_state::fd5_tc_w )
{
	upd765_tc_w(m_fdc, 1);
	upd765_tc_w(m_fdc, 0);
}



//**************************************************************************
//	ADDRESS MAPS
//**************************************************************************

//-------------------------------------------------
//  ADDRESS_MAP( m5_mem )
//-------------------------------------------------

static ADDRESS_MAP_START( m5_mem, ADDRESS_SPACE_PROGRAM, 8, m5_state )
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x0000, 0x1fff) AM_ROM
	AM_RANGE(0x2000, 0x6fff) AM_ROM
	AM_RANGE(0x7000, 0x7fff) AM_RAM
	AM_RANGE(0x8000, 0xffff) AM_RAM
ADDRESS_MAP_END


//-------------------------------------------------
//  ADDRESS_MAP( m5_io )
//-------------------------------------------------

static ADDRESS_MAP_START( m5_io, ADDRESS_SPACE_IO, 8, m5_state )
	ADDRESS_MAP_UNMAP_HIGH
	ADDRESS_MAP_GLOBAL_MASK(0xff)
	AM_RANGE(0x00, 0x03) AM_MIRROR(0x0c) AM_DEVREADWRITE_LEGACY(Z80CTC_TAG, z80ctc_r, z80ctc_w)
	AM_RANGE(0x10, 0x10) AM_MIRROR(0x0e) AM_READWRITE_LEGACY(TMS9928A_vram_r, TMS9928A_vram_w)
	AM_RANGE(0x11, 0x11) AM_MIRROR(0x0e) AM_READWRITE_LEGACY(TMS9928A_register_r, TMS9928A_register_w)
	AM_RANGE(0x20, 0x20) AM_MIRROR(0x0f) AM_DEVWRITE_LEGACY(SN76489AN_TAG, sn76496_w)
	AM_RANGE(0x30, 0x30) AM_READ_PORT("Y0") // 64KBF bank select
	AM_RANGE(0x31, 0x31) AM_READ_PORT("Y1")
	AM_RANGE(0x32, 0x32) AM_READ_PORT("Y2")
	AM_RANGE(0x33, 0x33) AM_READ_PORT("Y3")
	AM_RANGE(0x34, 0x34) AM_READ_PORT("Y4")
	AM_RANGE(0x35, 0x35) AM_READ_PORT("Y5")
	AM_RANGE(0x36, 0x36) AM_READ_PORT("Y6")
	AM_RANGE(0x37, 0x37) AM_READ_PORT("JOY")
	AM_RANGE(0x40, 0x40) AM_MIRROR(0x0f) AM_DEVWRITE_LEGACY(CENTRONICS_TAG, centronics_data_w)
	AM_RANGE(0x50, 0x50) AM_MIRROR(0x0f) AM_READWRITE(sts_r, com_w)
//	AM_RANGE(0x60, 0x63) SIO
//	AM_RANGE(0x6c, 0x6c) EM-64/64KBI bank select
	AM_RANGE(0x70, 0x73) AM_MIRROR(0x0c) AM_DEVREADWRITE_LEGACY(I8255A_TAG, i8255a_r, i8255a_w)
//	AM_RANGE(0x7f, 0x7f) 64KRD/64KRX bank select
ADDRESS_MAP_END


//-------------------------------------------------
//  ADDRESS_MAP( fd5_mem )
//-------------------------------------------------

static ADDRESS_MAP_START( fd5_mem, ADDRESS_SPACE_PROGRAM, 8, m5_state )
	AM_RANGE(0x0000, 0x3fff) AM_ROM
	AM_RANGE(0x4000, 0xffff) AM_RAM
ADDRESS_MAP_END


//-------------------------------------------------
//  ADDRESS_MAP( fd5_io )
//-------------------------------------------------

static ADDRESS_MAP_START( fd5_io, ADDRESS_SPACE_IO, 8, m5_state )
	ADDRESS_MAP_GLOBAL_MASK(0xff)
	AM_RANGE(0x00, 0x00) AM_DEVREAD_LEGACY(UPD765_TAG, upd765_status_r)
	AM_RANGE(0x01, 0x01) AM_DEVREADWRITE_LEGACY(UPD765_TAG, upd765_data_r, upd765_data_w)
	AM_RANGE(0x10, 0x10) AM_READWRITE(fd5_data_r, fd5_data_w)
	AM_RANGE(0x20, 0x20) AM_WRITE(fd5_com_w)
	AM_RANGE(0x30, 0x30) AM_READ(fd5_com_r)
	AM_RANGE(0x40, 0x40) AM_WRITE(fd5_ctrl_w)
	AM_RANGE(0x50, 0x50) AM_WRITE(fd5_tc_w)
ADDRESS_MAP_END



//**************************************************************************
//	INPUT PORTS
//**************************************************************************

//-------------------------------------------------
//  INPUT_PORTS( m5 )
//-------------------------------------------------

static INPUT_PORTS_START( m5 )
	PORT_START("Y0")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Ctrl") PORT_CODE(KEYCODE_LCONTROL) PORT_CHAR(UCHAR_SHIFT_2)
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Func") PORT_CODE(KEYCODE_TAB) PORT_CHAR(UCHAR_MAMEKEY(F1))
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_LSHIFT)		PORT_CHAR(UCHAR_SHIFT_1)
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_RSHIFT)		PORT_CHAR(UCHAR_MAMEKEY(RSHIFT))
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_SPACE)		PORT_CHAR(' ')
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_ENTER)		PORT_CHAR(13)

	PORT_START("Y1")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_1)			PORT_CHAR('1') PORT_CHAR('!')
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_2)			PORT_CHAR('2') PORT_CHAR('"')
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_3)			PORT_CHAR('3') PORT_CHAR('#')
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_4)			PORT_CHAR('4') PORT_CHAR('$')
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_5)			PORT_CHAR('5') PORT_CHAR('%')
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_6)			PORT_CHAR('6') PORT_CHAR('&')
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_7)			PORT_CHAR('7') PORT_CHAR('\'')
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_8)			PORT_CHAR('8') PORT_CHAR('(')

	PORT_START("Y2")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_Q)			PORT_CHAR('q') PORT_CHAR('Q')
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_W)			PORT_CHAR('w') PORT_CHAR('W')
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_E)			PORT_CHAR('e') PORT_CHAR('E')
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_R)			PORT_CHAR('r') PORT_CHAR('R')
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_T)			PORT_CHAR('t') PORT_CHAR('T')
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_Y)			PORT_CHAR('y') PORT_CHAR('Y')
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_U)			PORT_CHAR('u') PORT_CHAR('U')
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_I)			PORT_CHAR('i') PORT_CHAR('I')

	PORT_START("Y3")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_A)			PORT_CHAR('a') PORT_CHAR('A')
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_S)			PORT_CHAR('s') PORT_CHAR('S')
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_D)			PORT_CHAR('d') PORT_CHAR('D')
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_F)			PORT_CHAR('f') PORT_CHAR('F')
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_G)			PORT_CHAR('g') PORT_CHAR('G')
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_H)			PORT_CHAR('h') PORT_CHAR('H')
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_J)			PORT_CHAR('j') PORT_CHAR('J')
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_K)			PORT_CHAR('k') PORT_CHAR('K')

	PORT_START("Y4")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_Z)			PORT_CHAR('z') PORT_CHAR('Z')
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_X)			PORT_CHAR('x') PORT_CHAR('X')
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_C)			PORT_CHAR('c') PORT_CHAR('C')
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_V)			PORT_CHAR('v') PORT_CHAR('V')
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_B)			PORT_CHAR('b') PORT_CHAR('B')
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_N)			PORT_CHAR('n') PORT_CHAR('N')
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_M)			PORT_CHAR('m') PORT_CHAR('M')
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_COMMA)		PORT_CHAR(',') PORT_CHAR('<')

	PORT_START("Y5")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_9)			PORT_CHAR('9') PORT_CHAR(')')
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_0)			PORT_CHAR('0')
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_MINUS)		PORT_CHAR('-') PORT_CHAR('=')
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_EQUALS)	PORT_CHAR('^') PORT_CHAR('~')
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_STOP)		PORT_CHAR('.') PORT_CHAR('>')
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_SLASH)		PORT_CHAR('/') PORT_CHAR('?') PORT_CHAR(UCHAR_MAMEKEY(DOWN))
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("_  Triangle") PORT_CODE(KEYCODE_TILDE) PORT_CHAR('_')
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_BACKSLASH2)	PORT_CHAR('\\') PORT_CHAR('|')

	PORT_START("Y6")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_O)			PORT_CHAR('o') PORT_CHAR('O')
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_P)			PORT_CHAR('p') PORT_CHAR('P')
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_OPENBRACE)	PORT_CHAR('@') PORT_CHAR('`') PORT_CHAR(UCHAR_MAMEKEY(UP))
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_CLOSEBRACE)	PORT_CHAR('[') PORT_CHAR('{')
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_L)			PORT_CHAR('l') PORT_CHAR('L')
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_COLON)		PORT_CHAR(';') PORT_CHAR('+') PORT_CHAR(UCHAR_MAMEKEY(LEFT))
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_QUOTE)		PORT_CHAR(':') PORT_CHAR('*') PORT_CHAR(UCHAR_MAMEKEY(RIGHT))
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_BACKSLASH)	PORT_CHAR(']') PORT_CHAR('}')

	PORT_START("JOY")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT) PORT_PLAYER(1)
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP)    PORT_PLAYER(1)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT)  PORT_PLAYER(1)
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN)  PORT_PLAYER(1)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT) PORT_PLAYER(2)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP)    PORT_PLAYER(2)
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT)  PORT_PLAYER(2)
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN)  PORT_PLAYER(2)

	PORT_START("RESET")
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Reset") PORT_CODE(KEYCODE_ESC) PORT_CHAR(UCHAR_MAMEKEY(ESC)) /* 1st line, 1st key from right! */
INPUT_PORTS_END



//**************************************************************************
//	DEVICE CONFIGURATION
//**************************************************************************

//-------------------------------------------------
//  Z80CTC_INTERFACE( ctc_intf )
//-------------------------------------------------

// CK0 = EXINT, CK1 = GND, CK2 = TCK, CK3 = VDP INT

static Z80CTC_INTERFACE( ctc_intf )
{
	0,
	DEVCB_CPU_INPUT_LINE(Z80_TAG, INPUT_LINE_IRQ0),
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL // EXCLK
};


//-------------------------------------------------
//  cassette_config cassette_intf
//-------------------------------------------------

static const cassette_config cassette_intf =
{
	sordm5_cassette_formats,
	NULL,
	(cassette_state)(CASSETTE_PLAY),
	NULL
};


//-------------------------------------------------
//  TMS9928a_interface vdp_intf
//-------------------------------------------------

static void sordm5_video_interrupt_callback(running_machine *machine, int state)
{
	m5_state *driver_state = machine->driver_data<m5_state>();

	if (state)
	{
		z80ctc_trg3_w(driver_state->m_ctc, 1);
		z80ctc_trg3_w(driver_state->m_ctc, 0);
	}
}

static INTERRUPT_GEN( sord_interrupt )
{
	TMS9928A_interrupt(device->machine);
}

static const TMS9928a_interface vdp_intf =
{
	TMS9929A,
	0x4000,
	0, 0,
	sordm5_video_interrupt_callback
};


//-------------------------------------------------
//  I8255A_INTERFACE( ppi_intf )
//-------------------------------------------------

static I8255A_INTERFACE( ppi_intf )
{
	DEVCB_DRIVER_MEMBER(m5_state, fd5_data_r),
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_DRIVER_MEMBER(m5_state, fd5_data_w),
	DEVCB_NULL,
	DEVCB_NULL
};


//-------------------------------------------------
//  upd765_interface fdc_intf
//-------------------------------------------------

static FLOPPY_OPTIONS_START( m5 )
	FLOPPY_OPTION( m5, "dsk", "Sord M5 disk image", basicdsk_identify_default, basicdsk_construct_default,
		HEADS([1])
		TRACKS([40])
		SECTORS([18])
		SECTOR_LENGTH([256])
		FIRST_SECTOR_ID([1]))
FLOPPY_OPTIONS_END

static const floppy_config m5_floppy_config =
{
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,
	FLOPPY_STANDARD_5_25_SSDD_40,
	FLOPPY_OPTIONS_NAME(m5),
	NULL
};

static const struct upd765_interface fdc_intf =
{
	DEVCB_CPU_INPUT_LINE(Z80_FD5_TAG, INPUT_LINE_IRQ0),
	DEVCB_NULL,
	NULL,
	UPD765_RDY_PIN_CONNECTED,
	{ FLOPPY_0, NULL, NULL, NULL }
};


//-------------------------------------------------
//  z80_daisy_config m5_daisy_chain
//-------------------------------------------------

static const z80_daisy_config m5_daisy_chain[] =
{
	{ Z80CTC_TAG },
	{ NULL }
};



//**************************************************************************
//	MACHINE INITIALIZATION
//**************************************************************************

//-------------------------------------------------
//  MACHINE_START( m5 )
//-------------------------------------------------

void m5_state::machine_start()
{
	address_space *program = cpu_get_address_space(m_maincpu, ADDRESS_SPACE_PROGRAM);

	// configure VDP
	TMS9928A_configure(&vdp_intf);

	// configure RAM
	switch (ram_get_size(m_ram))
	{
	case 4*1024:
		memory_unmap_readwrite(program, 0x8000, 0xffff, 0, 0);
		break;

	case 36*1024:
		break;

	case 68*1024:
		break;
	}

	// register for state saving
	state_save_register_global(machine, m_fd5_data);
}


//-------------------------------------------------
//  MACHINE_RESET( m5 )
//-------------------------------------------------

void m5_state::machine_reset()
{
	TMS9928A_reset();
}



//**************************************************************************
//	MACHINE CONFIGURATION
//**************************************************************************

//-------------------------------------------------
//  MACHINE_CONFIG( m5 )
//-------------------------------------------------

static MACHINE_CONFIG_START( m5, m5_state )
	// basic machine hardware
	MCFG_CPU_ADD(Z80_TAG, Z80, XTAL_14_31818MHz/4)
	MCFG_CPU_PROGRAM_MAP(m5_mem)
	MCFG_CPU_IO_MAP(m5_io)
	MCFG_CPU_VBLANK_INT(SCREEN_TAG, sord_interrupt)
	MCFG_CPU_CONFIG(m5_daisy_chain)

	MCFG_CPU_ADD(Z80_FD5_TAG, Z80, XTAL_14_31818MHz/4)
	MCFG_CPU_PROGRAM_MAP(fd5_mem)
	MCFG_CPU_IO_MAP(fd5_io)

	// video hardware
	MCFG_FRAGMENT_ADD(tms9928a)
	MCFG_SCREEN_MODIFY(SCREEN_TAG)
	MCFG_SCREEN_REFRESH_RATE((float)XTAL_10_738635MHz/2/342/262)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(2500)) // not accurate

	// sound hardware
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD(SN76489AN_TAG, SN76489A, XTAL_14_31818MHz/4)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 1.00)

	// devices
	MCFG_Z80CTC_ADD(Z80CTC_TAG, XTAL_14_31818MHz/4, ctc_intf)
	MCFG_CENTRONICS_ADD(CENTRONICS_TAG, standard_centronics)
	MCFG_CASSETTE_ADD(CASSETTE_TAG, cassette_intf)
	MCFG_I8255A_ADD(I8255A_TAG, ppi_intf)
	MCFG_UPD765A_ADD(UPD765_TAG, fdc_intf)
	MCFG_FLOPPY_DRIVE_ADD(FLOPPY_0, m5_floppy_config)

	// cartridge
	MCFG_CARTSLOT_ADD("cart")
	MCFG_CARTSLOT_EXTENSION_LIST("bin,rom")
	MCFG_CARTSLOT_MANDATORY
	MCFG_CARTSLOT_INTERFACE("m5_cart")

	// software lists
	MCFG_SOFTWARE_LIST_ADD("cart_list","sordm5")

	// internal ram
	MCFG_RAM_ADD(RAM_TAG)
	MCFG_RAM_DEFAULT_SIZE("4K")
	MCFG_RAM_EXTRA_OPTIONS("36K,68K")
MACHINE_CONFIG_END



//**************************************************************************
//	ROMS
//**************************************************************************

//-------------------------------------------------
//  ROM( m5 )
//-------------------------------------------------

ROM_START( m5 )
	ROM_REGION( 0x7000, Z80_TAG, ROMREGION_ERASEFF )
    ROM_LOAD( "sordjap.ic21", 0x0000, 0x2000, CRC(92cf9353) SHA1(b0a4b3658fde68cb1f344dfb095bac16a78e9b3e) )
	ROM_CART_LOAD( "cart",   0x2000, 0x5000, ROM_NOMIRROR | ROM_OPTIONAL )

	ROM_REGION( 0x4000, Z80_FD5_TAG, 0 )
	ROM_LOAD( "sordfd5.rom", 0x0000, 0x4000, BAD_DUMP CRC(aa172b1b) SHA1(007cceb12528ca7f3e381578da88a8fcd5bc3a20) ) // parsed from disassembly
ROM_END


//-------------------------------------------------
//  ROM( m5p )
//-------------------------------------------------

ROM_START( m5p )
	ROM_REGION( 0x7000, Z80_TAG, ROMREGION_ERASEFF )
	ROM_LOAD( "sordint.ic21", 0x0000, 0x2000, CRC(78848d39) SHA1(ac042c4ae8272ad6abe09ae83492ef9a0026d0b2) )
	ROM_CART_LOAD( "cart",   0x2000, 0x5000, ROM_NOMIRROR | ROM_OPTIONAL )

	ROM_REGION( 0x4000, Z80_FD5_TAG, 0 )
	ROM_LOAD( "sordfd5.rom", 0x0000, 0x4000, BAD_DUMP CRC(aa172b1b) SHA1(007cceb12528ca7f3e381578da88a8fcd5bc3a20) ) // parsed from disassembly
ROM_END



//**************************************************************************
//	SYSTEM DRIVERS
//**************************************************************************

//    YEAR  NAME    PARENT  COMPAT  MACHINE INPUT   INIT    COMPANY     FULLNAME            FLAGS
COMP( 1983, m5,		0,		0,		m5,		m5,		0,		"Sord",		"m.5 (Japan)",		0 )
COMP( 1983, m5p,	m5,		0,		m5,		m5,		0,		"Sord",		"m.5 (Europe)",		0 )

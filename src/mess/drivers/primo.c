/*******************************************************************************

Primo driver by Krzysztof Strzecha

What's new:
-----------
2008.08.31 -    Added new ROMs including B32 and B48 [Miodrag Milanovic]
2005.05.19 -    Primo B-32 and B-48 testdrivers added.
2005.05.15 -    EPROM+RAM expansion.
        Support for .pp files improved.
        Some cleanups.
2005.05.12 -    Memory fixed for A-48 model what make it fully working.
        Fixed address of second video memory area.

To do:
    1. Disk
    2. V.24 / tape control
    3. CDOS autoboot
    4. Joystick
    5. Printer
    6. .PRI format

Primo variants:
    A-32 - 16kB RAM + 16KB ROM
    A-48 - 32kB RAM + 16KB ROM
    A-64 - 48kB RAM + 16KB ROM
    B-64 - 48kB RAM + 16KB ROM

CPU:
    U880D (DDR clone of Z80), 2.5 MHz

Memory map:
    A-32
    0000-3fff ROM
    4000-7fff RAM
    8000-bfff not mapped
    c000-ffff not mapped

    A-48
    0000-3fff ROM
    4000-7fff RAM
    8000-bfff RAM
    c000-ffff not mapped

    A-64, B-64
    0000-3fff ROM
    4000-7fff RAM
    8000-bfff RAM
    c000-ffff RAM

I/O Ports:
    Write:
    00h-3fh
        bit 7 - NMI
                0 - disable
                1 - enable
        bit 6 - joystick register shift
        bit 5 - V.24 (2) / tape control
        bit 4 - speaker
        bit 3 - display page
                0 - secondary
                1 - primary
        bit 2 - V.24 (1) / tape control
        bit 1 - cassette output
        bit 0 /
                00 - -110mV
                01 - 0V
                10 - 0V
                11 - 110mV
    40h-ffh (B models only)
        bit 7 - not used
        bit 6 - not used
        bit 5 - SCLK
        bit 4 - SDATA
        bit 3 - not used
        bit 2 - SRQ
        bit 1 - ATN
        bit 0 - not used
    Read:
    00h-3fh
        bit 7 - not used
        bit 6 - not used
        bit 5 - VBLANK
        bit 4 - I4
        bit 3 - I3
        bit 2 - cassette input
        bit 1 - reset
        bit 0 - keyboard input
    40h-ffh (B models only)
        bit 7 - not used
        bit 6 - not used
        bit 5 - SCLK
        bit 4 - SDATA
        bit 3 - SRQ
        bit 2 - joystick 2
        bit 1 - ATN
        bit 0 - joystick 1

Interrupts:
    NMI - 20ms (50HZ), can be disbled/enabled by I/O write

*******************************************************************************/

#include "driver.h"
#include "sound/speaker.h"
#include "sound/wave.h"
#include "includes/primo.h"
#include "cpu/z80/z80.h"
#include "devices/cassette.h"
#include "devices/snapquik.h"
#include "devices/cartslot.h"
#include "formats/primoptp.h"

static ADDRESS_MAP_START( primoa_port, ADDRESS_SPACE_IO, 8 )
	ADDRESS_MAP_GLOBAL_MASK(0xff)
	AM_RANGE( 0x00, 0x3f ) AM_READWRITE( primo_be_1_r, primo_ki_1_w )
	AM_RANGE( 0xfd, 0xfd ) AM_WRITE( primo_FD_w )
ADDRESS_MAP_END

static ADDRESS_MAP_START( primob_port, ADDRESS_SPACE_IO, 8 )
	ADDRESS_MAP_GLOBAL_MASK(0xff)
	AM_RANGE( 0x00, 0x3f ) AM_READWRITE( primo_be_1_r, primo_ki_1_w )
	AM_RANGE( 0x40, 0x7f ) AM_READWRITE( primo_be_2_r, primo_ki_2_w )
	AM_RANGE( 0xfd, 0xfd ) AM_WRITE( primo_FD_w )
ADDRESS_MAP_END

static ADDRESS_MAP_START( primo32_mem, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE( 0x0000, 0x3fff ) AM_RAMBANK(1)
	AM_RANGE( 0x4000, 0x7fff ) AM_RAM AM_MIRROR ( 0x8000 )
ADDRESS_MAP_END

static ADDRESS_MAP_START( primo48_mem, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE( 0x0000, 0x3fff ) AM_RAMBANK(1)
	AM_RANGE( 0x4000, 0x7fff ) AM_RAM
	AM_RANGE( 0x8000, 0xbfff ) AM_RAM AM_MIRROR ( 0x4000 )
ADDRESS_MAP_END

static ADDRESS_MAP_START( primo64_mem, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE( 0x0000, 0x3fff ) AM_RAMBANK(1)
	AM_RANGE( 0x4000, 0xffff ) AM_RAM
ADDRESS_MAP_END

static INPUT_PORTS_START( primo )
	PORT_START( "IN0" )
		PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_Y)		PORT_CHAR('y') PORT_CHAR('Y')
		PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_UP)		PORT_CHAR(UCHAR_MAMEKEY(UP))
		PORT_BIT( 0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_S)		PORT_CHAR('s') PORT_CHAR('S')
		PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("SHIFT")	PORT_CODE(KEYCODE_LSHIFT) PORT_CODE(KEYCODE_RSHIFT) 	PORT_CHAR(UCHAR_SHIFT_1)
		PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_E)		PORT_CHAR('e') PORT_CHAR('E')
		PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("UPPER")	PORT_CODE(KEYCODE_LALT)		PORT_CHAR(UCHAR_MAMEKEY(LALT))
		PORT_BIT( 0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_W)		PORT_CHAR('w') PORT_CHAR('W')
		PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("CTR")	PORT_CODE(KEYCODE_LCONTROL)	PORT_CHAR(UCHAR_MAMEKEY(LCONTROL))
		PORT_BIT( 0x0100, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_D)		PORT_CHAR('d') PORT_CHAR('D')
		PORT_BIT( 0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_3)		PORT_CHAR('3') PORT_CHAR('#')
		PORT_BIT( 0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_X)		PORT_CHAR('x') PORT_CHAR('X')
		PORT_BIT( 0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_2)		PORT_CHAR('2') PORT_CHAR('\"')
		PORT_BIT( 0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_Q)		PORT_CHAR('q') PORT_CHAR('Q')
		PORT_BIT( 0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_1)		PORT_CHAR('1') PORT_CHAR('!')
		PORT_BIT( 0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_A)		PORT_CHAR('a') PORT_CHAR('A')
		PORT_BIT( 0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_DOWN)	PORT_CHAR(UCHAR_MAMEKEY(DOWN))

	PORT_START( "IN1" )
		PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_C)		PORT_CHAR('c') PORT_CHAR('C')
		PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_UNUSED )
		PORT_BIT( 0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_F)		PORT_CHAR('f') PORT_CHAR('F')
		PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_UNUSED )
		PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_R)		PORT_CHAR('r') PORT_CHAR('R')
		PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_UNUSED )
		PORT_BIT( 0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_T)		PORT_CHAR('t') PORT_CHAR('T')
		PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_7)		PORT_CHAR('7') PORT_CHAR('/')
		PORT_BIT( 0x0100, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_H)		PORT_CHAR('h') PORT_CHAR('H')
		PORT_BIT( 0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_SPACE)	PORT_CHAR(' ')
		PORT_BIT( 0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_B)		PORT_CHAR('b') PORT_CHAR('B')
		PORT_BIT( 0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_6)		PORT_CHAR('6') PORT_CHAR('&')
		PORT_BIT( 0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_G)		PORT_CHAR('g') PORT_CHAR('G')
		PORT_BIT( 0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_5)		PORT_CHAR('5') PORT_CHAR('%')
		PORT_BIT( 0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_V)		PORT_CHAR('v') PORT_CHAR('V')
		PORT_BIT( 0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_4)		PORT_CHAR('4') PORT_CHAR('$')

	PORT_START( "IN2" )
		PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_N)		PORT_CHAR('n') PORT_CHAR('N')
		PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_8)		PORT_CHAR('8') PORT_CHAR('(')
		PORT_BIT( 0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_Z)		PORT_CHAR('z') PORT_CHAR('Z')
		PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_MINUS)	PORT_CHAR('+') PORT_CHAR('?')
		PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_U)		PORT_CHAR('u') PORT_CHAR('U')
		PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_0)		PORT_CHAR('0') PORT_CHAR('=')
		PORT_BIT( 0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_J)		PORT_CHAR('j') PORT_CHAR('J')
		PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_HOME)	PORT_CHAR('>') PORT_CHAR('<')
		PORT_BIT( 0x0100, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_L)		PORT_CHAR('l') PORT_CHAR('L')
		PORT_BIT( 0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_SLASH)	PORT_CHAR('-') PORT_CHAR('|')
		PORT_BIT( 0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_K)		PORT_CHAR('k') PORT_CHAR('K')
		PORT_BIT( 0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_STOP)	PORT_CHAR('.') PORT_CHAR(':')
		PORT_BIT( 0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_M)		PORT_CHAR('m') PORT_CHAR('M')
		PORT_BIT( 0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_9)		PORT_CHAR('9') PORT_CHAR(')')
		PORT_BIT( 0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_I)		PORT_CHAR('i') PORT_CHAR('I')
		PORT_BIT( 0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_COMMA)	PORT_CHAR(',') PORT_CHAR(';')

	PORT_START( "IN3" )
		PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("?") PORT_CODE(KEYCODE_EQUALS)		PORT_CHAR(UCHAR_MAMEKEY(F2))
		PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_BACKSLASH)					PORT_CHAR('*') PORT_CHAR('\'')
		PORT_BIT( 0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_P)							PORT_CHAR('p') PORT_CHAR('P')
		PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("\xC2\xA3") PORT_CODE(KEYCODE_END)	PORT_CHAR(UCHAR_MAMEKEY(F3))
		PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_O)							PORT_CHAR('o') PORT_CHAR('O')
		PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("CLS") PORT_CODE(KEYCODE_BACKSPACE)	PORT_CHAR(UCHAR_MAMEKEY(BACKSPACE))
		PORT_BIT( 0x0040, IP_ACTIVE_HIGH, IPT_UNUSED )
		PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("RETURN") PORT_CODE(KEYCODE_ENTER)	PORT_CHAR(UCHAR_MAMEKEY(ENTER))
		PORT_BIT( 0x0100, IP_ACTIVE_HIGH, IPT_UNUSED )
		PORT_BIT( 0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_LEFT)						PORT_CHAR(UCHAR_MAMEKEY(LEFT))
		PORT_BIT( 0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("\xE2\x80\x9A") PORT_CODE(KEYCODE_COLON) PORT_CHAR(UCHAR_MAMEKEY(F4))
		PORT_BIT( 0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("\xC2\xA2") PORT_CODE(KEYCODE_CLOSEBRACE) PORT_CHAR(UCHAR_MAMEKEY(F5))
		PORT_BIT( 0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("\xC2\xA0") PORT_CODE(KEYCODE_QUOTE)	PORT_CHAR(UCHAR_MAMEKEY(F6))
		PORT_BIT( 0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_RIGHT)						PORT_CHAR(UCHAR_MAMEKEY(RIGHT))
		PORT_BIT( 0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("\xE2\x80\x9D") PORT_CODE(KEYCODE_OPENBRACE)	PORT_CHAR(UCHAR_MAMEKEY(F7))
		PORT_BIT( 0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("BRK")	PORT_CODE(KEYCODE_ESC)		PORT_CHAR(UCHAR_MAMEKEY(ESC))

	PORT_START( "RESET" )	/* IN4 */
		PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("Reset") PORT_CODE(KEYCODE_F1)			PORT_CHAR(UCHAR_MAMEKEY(F1))
		PORT_BIT( 0xfe, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START( "CPU_CLOCK" )	/* IN5 */
		PORT_CONFNAME(0x01, 0x00, "CPU clock" )
		PORT_CONFSETTING(	0x00, "2.50 MHz" )
		PORT_CONFSETTING(	0x01, "3.75 MHz" )

	PORT_START( "MEMORY_EXPANSION" )	/* IN6 */
		PORT_CONFNAME(0x01, 0x00, "EPROM+RAM Expansion" )
		PORT_CONFSETTING(	0x00, DEF_STR( On ) )
		PORT_CONFSETTING(	0x01, DEF_STR( Off ) )
INPUT_PORTS_END

static const struct CassetteOptions primo_cassette_options = {
	1,		/* channels */
	16,		/* bits per sample */
	22050	/* sample frequency */
};

static const cassette_config primo_cassette_config =
{
	primo_ptp_format,
	&primo_cassette_options,
	CASSETTE_STOPPED | CASSETTE_SPEAKER_ENABLED
};

static MACHINE_DRIVER_START( primoa32 )
	/* basic machine hardware */
	MDRV_CPU_ADD( "main", Z80, 2500000 )
	MDRV_CPU_PROGRAM_MAP( primo32_mem, 0 )
	MDRV_CPU_IO_MAP( primoa_port, 0 )
	MDRV_CPU_VBLANK_INT("screen", primo_vblank_interrupt)

	MDRV_MACHINE_RESET( primoa )

	/* video hardware */
	MDRV_SCREEN_ADD("screen", RASTER)
	MDRV_SCREEN_REFRESH_RATE( 50 )
	MDRV_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(2500)) /* not accurate */
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_SIZE( 256, 192 )
	MDRV_SCREEN_VISIBLE_AREA( 0, 256-1, 0, 192-1 )
	MDRV_PALETTE_LENGTH(2)
	MDRV_PALETTE_INIT(black_and_white)

	MDRV_VIDEO_UPDATE( primo )

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")
	MDRV_SOUND_ADD("cassette", WAVE, 0)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.50)
	MDRV_SOUND_ADD("speaker", SPEAKER, 0)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.50)

	/* snapshot/quickload */
	MDRV_SNAPSHOT_ADD(primo, "pss", 0)
	MDRV_QUICKLOAD_ADD(primo, "pp", 0)

	MDRV_CASSETTE_ADD( "cassette", primo_cassette_config )

	/* cartridge */
	MDRV_CARTSLOT_ADD("cart1")
	MDRV_CARTSLOT_EXTENSION_LIST("rom")
	MDRV_CARTSLOT_NOT_MANDATORY
	MDRV_CARTSLOT_ADD("cart2")
	MDRV_CARTSLOT_EXTENSION_LIST("rom")
	MDRV_CARTSLOT_NOT_MANDATORY
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( primoa48 )
	MDRV_IMPORT_FROM( primoa32 )
	MDRV_CPU_MODIFY( "main" )
	MDRV_CPU_PROGRAM_MAP( primo48_mem, 0 )
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( primoa64 )
	MDRV_IMPORT_FROM( primoa32 )
	MDRV_CPU_MODIFY( "main" )
	MDRV_CPU_PROGRAM_MAP( primo64_mem, 0 )
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( primob32 )
	MDRV_IMPORT_FROM( primoa32 )
	MDRV_CPU_MODIFY( "main" )
	MDRV_CPU_IO_MAP( primob_port, 0 )

	MDRV_MACHINE_RESET( primob )
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( primob48 )
	MDRV_IMPORT_FROM( primoa48 )
	MDRV_CPU_MODIFY( "main" )
	MDRV_CPU_IO_MAP( primob_port, 0 )

	MDRV_MACHINE_RESET( primob )
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( primob64 )
	MDRV_IMPORT_FROM( primoa64 )
	MDRV_CPU_MODIFY( "main" )
	MDRV_CPU_IO_MAP( primob_port, 0 )

	MDRV_MACHINE_RESET( primob )
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( primoc64 )
	MDRV_IMPORT_FROM( primoa64 )
	MDRV_CPU_MODIFY( "main" )
	MDRV_CPU_IO_MAP( primob_port, 0 )

	MDRV_MACHINE_RESET( primob )
MACHINE_DRIVER_END

ROM_START( primoa32 )
	ROM_REGION( 0x1c000, "main", 0 )
	ROM_SYSTEM_BIOS(0, "ver1", "ver 1")
	ROMX_LOAD( "a32_1.rom", 0x10000, 0x1000, CRC(4e91c1a4) SHA1(bf6e41b6b36a2556a50065e9acfd8cd57968f039), ROM_BIOS(1) )
	ROMX_LOAD( "a32_2.rom", 0x11000, 0x1000, CRC(81a8a0fb) SHA1(df75bd7774969cabb062e50da6004f2efbde489e), ROM_BIOS(1) )
	ROMX_LOAD( "a32_3.rom", 0x12000, 0x1000, CRC(a97de2f5) SHA1(743c76121f5b9e1eab35c8c00797311f58da5243), ROM_BIOS(1) )
	ROMX_LOAD( "a32_4.rom", 0x13000, 0x1000, CRC(70f84bc8) SHA1(9ae1c06531edf20c14ba47e78c0747dd2a92612a), ROM_BIOS(1) )
	ROM_SYSTEM_BIOS(1, "ver2", "ver 2")
	ROMX_LOAD( "a32-v2.rom",0x10000, 0x4000, CRC(1544b8cc) SHA1(6a9e67027a349fa3d14cb3624099b0770564c5dd), ROM_BIOS(2) )
	ROM_CART_LOAD("cart2", 0x14000, 0x4000, ROM_FILL_FF | ROM_OPTIONAL)
	ROM_CART_LOAD("cart1", 0x18000, 0x4000, ROM_FILL_FF | ROM_OPTIONAL)
ROM_END

ROM_START( primoa48 )
	ROM_REGION( 0x1c000, "main", 0 )
	ROM_SYSTEM_BIOS(0, "ver1", "ver 1")
	ROMX_LOAD( "a48_1.rom", 0x10000, 0x1000, CRC(7de6ad6f) SHA1(f2fd6fac4f9bc57c646efe40281758bb7c3f56e1), ROM_BIOS(1) )
	ROMX_LOAD( "a48_2.rom", 0x11000, 0x1000, CRC(81a8a0fb) SHA1(df75bd7774969cabb062e50da6004f2efbde489e), ROM_BIOS(1) )
	ROMX_LOAD( "a48_3.rom", 0x12000, 0x1000, CRC(a97de2f5) SHA1(743c76121f5b9e1eab35c8c00797311f58da5243), ROM_BIOS(1) )
	ROMX_LOAD( "a48_4.rom", 0x13000, 0x1000, CRC(e4d0c452) SHA1(4a98ff7502f1236445250d6b4e1c34850734350e), ROM_BIOS(1) )
	ROM_SYSTEM_BIOS(1, "ver2", "ver 2")
	ROMX_LOAD( "a48-v2.rom",0x10000, 0x4000, CRC(3fed4020) SHA1(a78351a9a0e114e59de019fd7177ce6e37f98163), ROM_BIOS(2) )	
	ROM_CART_LOAD("cart2", 0x14000, 0x4000, ROM_FILL_FF | ROM_OPTIONAL)	
	ROM_CART_LOAD("cart1", 0x18000, 0x4000, ROM_FILL_FF | ROM_OPTIONAL)
ROM_END

ROM_START( primoa64 )
	ROM_REGION( 0x1c000, "main", 0 )
	ROM_SYSTEM_BIOS(0, "ver1", "ver 1")
	ROMX_LOAD( "a64_1.rom", 0x10000, 0x1000, CRC(6a7a9b9b) SHA1(e9ce16f90d9a799a26a9cef09d9ee6a6d7749484), ROM_BIOS(1) )
	ROMX_LOAD( "a64_2.rom", 0x11000, 0x1000, CRC(81a8a0fb) SHA1(df75bd7774969cabb062e50da6004f2efbde489e), ROM_BIOS(1) )
	ROMX_LOAD( "a64_3.rom", 0x12000, 0x1000, CRC(a97de2f5) SHA1(743c76121f5b9e1eab35c8c00797311f58da5243), ROM_BIOS(1) )
	ROMX_LOAD( "a64_4.rom", 0x13000, 0x1000, CRC(e4d0c452) SHA1(4a98ff7502f1236445250d6b4e1c34850734350e), ROM_BIOS(1) )
	ROM_SYSTEM_BIOS(1, "ver2", "ver 2")
	ROMX_LOAD( "a64-v2.rom",0x10000, 0x4000, CRC(93e0343a) SHA1(4f6b169668424109c4bca495c849d7cf0ddacad8), ROM_BIOS(2) )	
	ROM_CART_LOAD("cart2", 0x14000, 0x4000, ROM_FILL_FF | ROM_OPTIONAL)
	ROM_CART_LOAD("cart1", 0x18000, 0x4000, ROM_FILL_FF | ROM_OPTIONAL)
ROM_END

ROM_START( primob32 )
	ROM_REGION( 0x1c000, "main", 0 )
	ROM_SYSTEM_BIOS(0, "ver1", "ver 1")
	ROMX_LOAD( "b32.rom",   0x10000, 0x4000, CRC(f594d2bb) SHA1(b74961dba008a1a6f15a22ddbd1b89acd7e286c2), ROM_BIOS(1) )	
	ROM_SYSTEM_BIOS(1, "ver2", "ver 2")
	ROMX_LOAD( "b32-v2.rom",0x10000, 0x4000, CRC(b22a5d9a) SHA1(e048724f338b1f4e2602960cfd6f42cc49e85d75), ROM_BIOS(2) )	
	ROM_CART_LOAD("cart2", 0x14000, 0x4000, ROM_FILL_FF | ROM_OPTIONAL)
	ROM_CART_LOAD("cart1", 0x18000, 0x4000, ROM_FILL_FF | ROM_OPTIONAL)
ROM_END

ROM_START( primob48 )
	ROM_REGION( 0x1c000, "main", 0 )
	ROM_SYSTEM_BIOS(0, "ver1", "ver 1")
	ROMX_LOAD( "b48.rom",   0x10000, 0x4000, CRC(df3d2a57) SHA1(ab9413aa9d7749d30a486da00bc8c6d178a2172c), ROM_BIOS(1) )	
	ROM_SYSTEM_BIOS(1, "ver2", "ver 2")
	ROMX_LOAD( "b48-v2.rom",0x10000, 0x4000, CRC(9883a576) SHA1(aeb142ed1e9494371bf34dda600e32b17d454f78), ROM_BIOS(2) )	
	ROM_CART_LOAD("cart2", 0x14000, 0x4000, ROM_FILL_FF | ROM_OPTIONAL)
	ROM_CART_LOAD("cart1", 0x18000, 0x4000, ROM_FILL_FF | ROM_OPTIONAL)
ROM_END

ROM_START( primob64 )
	ROM_REGION( 0x1c000, "main", 0 )
	ROM_SYSTEM_BIOS(0, "ver1", "ver 1")
	ROMX_LOAD( "b64.rom",   0x10000, 0x4000, CRC(73305e4d) SHA1(c090c3430cdf19eed8363377b981e1c21a4ed169), ROM_BIOS(1) )
	ROM_SYSTEM_BIOS(1, "ver2", "ver 2")
	ROMX_LOAD( "b64-v2.rom",0x10000, 0x4000, CRC(348ed16c) SHA1(abf75fcdaa817abd133d66223ca2608853748557), ROM_BIOS(2) )	
	ROM_SYSTEM_BIOS(2, "cdos", "CDOS")
	ROMX_LOAD( "b64-cdos.rom",0x10000, 0x4000, CRC(73305e4d) SHA1(c090c3430cdf19eed8363377b981e1c21a4ed169), ROM_BIOS(3) )	
	ROM_CART_LOAD("cart2", 0x14000, 0x4000, ROM_FILL_FF | ROM_OPTIONAL)
	ROM_CART_LOAD("cart1", 0x18000, 0x4000, ROM_FILL_FF | ROM_OPTIONAL)
ROM_END

ROM_START( primoc64 )
	ROM_REGION( 0x1c000, "main", 0 )
	ROM_SYSTEM_BIOS(0, "ver1", "ver 1")
	ROMX_LOAD( "c64_1.rom", 0x10000, 0x1000, CRC(c22290ea) SHA1(af5c73f6d0f7a987c4f082a5cb69e3f016127d57), ROM_BIOS(1) )
	ROMX_LOAD( "c64_2.rom", 0x11000, 0x1000, CRC(0756b56e) SHA1(589dbdb7c43ca7ca29ed1e56e080adf8c069e407), ROM_BIOS(1) )
	ROMX_LOAD( "c64_3.rom", 0x12000, 0x1000, CRC(fc56e0af) SHA1(b547fd270d3413400bc800f5b7ea9153b7a59bff), ROM_BIOS(1) )
	ROMX_LOAD( "c64_4.rom", 0x13000, 0x1000, CRC(3770e3e6) SHA1(792cc71d8f89eb447f94aded5afc70d626a26030), ROM_BIOS(1) )
	ROM_SYSTEM_BIOS(1, "ver2", "ver 2")
	ROMX_LOAD( "c64-v2.rom",0x10000, 0x4000, CRC(356ed18a) SHA1(613324f2bab94e427e9c1243c766dcf8486e0fb4), ROM_BIOS(2) )	
	ROM_CART_LOAD("cart2", 0x14000, 0x4000, ROM_FILL_FF | ROM_OPTIONAL)
	ROM_CART_LOAD("cart1", 0x18000, 0x4000, ROM_FILL_FF | ROM_OPTIONAL)
ROM_END


/*static void primo_cartslot_getinfo(const mess_device_class *devclass, UINT32 state, union devinfo *info)
{
	switch(state)
	{
		case MESS_DEVINFO_STR_DESCRIPTION+0:					strcpy(info->s = device_temp_str(), "EPROM Expansion Bank #1"); break;
		case MESS_DEVINFO_STR_DESCRIPTION+1:					strcpy(info->s = device_temp_str(), "EPROM Expansion Bank #2"); break;
		case MESS_DEVINFO_STR_DESCRIPTION+2:					strcpy(info->s = device_temp_str(), "EPROM Expansion Bank #3"); break;
		case MESS_DEVINFO_STR_DESCRIPTION+3:					strcpy(info->s = device_temp_str(), "EPROM Expansion Bank #4"); break;

		default:										cartslot_device_getinfo(devclass, state, info); break;
	}
}

*/

/*     YEAR  NAME      PARENT    COMPAT MACHINE   INPUT  INIT     CONFIG COMPANY  FULLNAME */
COMP ( 1984, primoa32, 0,        0,     primoa32, primo, primo32, 0, "Microkey", "Primo A-32" , 0)
COMP ( 1984, primoa48, primoa32, 0,     primoa48, primo, primo48, 0, "Microkey", "Primo A-48" , 0)
COMP ( 1984, primoa64, primoa32, 0,     primoa64, primo, primo64, 0, "Microkey", "Primo A-64" , 0)
COMP ( 1984, primob32, primoa32, 0,     primob32, primo, primo32, 0, "Microkey", "Primo B-32" , 0)
COMP ( 1984, primob48, primoa32, 0,     primob48, primo, primo48, 0, "Microkey", "Primo B-48" , 0)
COMP ( 1984, primob64, primoa32, 0,     primob64, primo, primo64, 0, "Microkey", "Primo B-64" , 0)
COMP ( 1984, primoc64, primoa32, 0,     primoc64, primo, primo64, 0, "Microkey", "Primo C-64" , GAME_NOT_WORKING)

/***************************************************************************

        Plan-80

        06/12/2009 Skeleton driver.

****************************************************************************/
#define ADDRESS_MAP_MODERN


#include "emu.h"
#include "cpu/i8085/i8085.h"


class plan80_state : public driver_device
{
public:
	plan80_state(running_machine &machine, const driver_device_config_base &config)
		: driver_device(machine, config),
		m_maincpu(*this, "maincpu")
	{ }

	required_device<cpu_device> m_maincpu;
	UINT8* m_videoram;
};


static ADDRESS_MAP_START(plan80_mem, ADDRESS_SPACE_PROGRAM, 8, plan80_state)
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x0000, 0x07ff) AM_RAMBANK("boot")
	AM_RANGE(0x0800, 0xefff) AM_RAM
	AM_RANGE(0xf000, 0xf7ff) AM_RAM AM_BASE(m_videoram)
	AM_RANGE(0xf800, 0xffff) AM_ROM
ADDRESS_MAP_END

static ADDRESS_MAP_START( plan80_io , ADDRESS_SPACE_IO, 8, plan80_state)
	ADDRESS_MAP_UNMAP_HIGH
ADDRESS_MAP_END

/* Input ports */
static INPUT_PORTS_START( plan80 )
INPUT_PORTS_END


/* after the first 4 bytes have been read from ROM, switch the ram back in */
static TIMER_CALLBACK( plan80_boot )
{
	memory_set_bank(machine, "boot", 0);
}

static MACHINE_RESET(plan80)
{
	memory_set_bank(machine, "boot", 1);
	machine->scheduler().timer_set(attotime::from_usec(10), FUNC(plan80_boot));
}

DRIVER_INIT( plan80 )
{
	UINT8 *RAM = machine->region("maincpu")->base();
	memory_configure_bank(machine, "boot", 0, 2, &RAM[0x0000], 0xf800);
}

static VIDEO_START( plan80 )
{
}

static VIDEO_UPDATE( plan80 )
{
	plan80_state *state = screen->machine->driver_data<plan80_state>();
	UINT8 *gfx = screen->machine->region("gfx")->base();
	int x,y,j,b;
	UINT16 addr;

	for(y = 0; y < 32; y++ )
	{
		addr = y*64;
		for(x = 0; x < 48; x++ )
		{
			UINT8 code = state->m_videoram[addr + x];
			for(j = 0; j < 8; j++ )
			{
				UINT8 val = gfx[code*8 + j];
				if (BIT(code,7))  val ^= 0xff;
				for(b = 0; b < 6; b++ )
				{
					*BITMAP_ADDR16(bitmap, y*8+j, x*6+b ) = (val >> (5-b)) & 1;
				}
			}
		}
	}
	return 0;
}

/* F4 Character Displayer */
static const gfx_layout plan80_charlayout =
{
	8, 8,					/* 8 x 8 characters */
	256,					/* 256 characters */
	1,					/* 1 bits per pixel */
	{ 0 },					/* no bitplanes */
	/* x offsets */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	/* y offsets */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8					/* every char takes 8 bytes */
};

static GFXDECODE_START( plan80 )
	GFXDECODE_ENTRY( "gfx", 0x0000, plan80_charlayout, 0, 1 )
GFXDECODE_END


static MACHINE_CONFIG_START( plan80, plan80_state )
	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu",I8080, 2048000)
	MCFG_CPU_PROGRAM_MAP(plan80_mem)
	MCFG_CPU_IO_MAP(plan80_io)

	MCFG_MACHINE_RESET(plan80)

	/* video hardware */
	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(2500)) /* not accurate */
	MCFG_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MCFG_SCREEN_SIZE(48*6, 32*8)
	MCFG_SCREEN_VISIBLE_AREA(0, 48*6-1, 0, 32*8-1)
	MCFG_GFXDECODE(plan80)
	MCFG_PALETTE_LENGTH(2)
	MCFG_PALETTE_INIT(black_and_white)

	MCFG_VIDEO_START(plan80)
	MCFG_VIDEO_UPDATE(plan80)
MACHINE_CONFIG_END

/* ROM definition */
ROM_START( plan80 )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "pl80mon.bin", 0xf800, 0x0800, CRC(433fb685) SHA1(43d53c35544d3a197ab71b6089328d104535cfa5))

	ROM_REGION( 0x10000, "spare", 0 )
	ROM_LOAD_OPTIONAL( "pl80mod.bin", 0xf000, 0x0800, CRC(6bdd7136) SHA1(721eab193c33c9330e0817616d3d2b601285fe50))

	ROM_REGION( 0x0800, "gfx", 0 )
	ROM_LOAD( "pl80gzn.bin", 0x0000, 0x0800, CRC(b4ddbdb6) SHA1(31bf9cf0f2ed53f48dda29ea830f74cea7b9b9b2))
ROM_END

/* Driver */

/*    YEAR  NAME    PARENT  COMPAT   MACHINE    INPUT    INIT     COMPANY          FULLNAME       FLAGS */
COMP( 1988, plan80,  0,       0,     plan80,    plan80, plan80,   "Tesla Eltos",   "Plan-80", GAME_NOT_WORKING | GAME_NO_SOUND)


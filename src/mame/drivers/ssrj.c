/***********************************
 Super Speed Race Jr (c) 1985 Taito
 driver by  Tomasz Slanina


 TODO:
 - colors (missing proms?)
 - dips
 - proper video hw emulation
 - controls (is there START button ?)

HW info :

    0000-7fff ROM
    c000-dfff VRAM ( 4 tilemaps (4 x $800) )
    e000-e7ff RAM
    e800-efff SCROLL RAM
    f003      ??
  f400-f401 AY 8910
  fc00      ??
  f800      ??

 Scroll RAM contains x and y offsets for each tileline,
 as well as other data (priroities ? additional flags ?)
 All moving obejcts (cars, etc) are displayed on tilemap 3.

 ------------------------------------
 Cheat :  $e210 - timer

************************************/
#if 0

#include "driver.h"
#include "cpu/z80/z80.h"
#include "sound/ay8910.h"

extern UINT8 *ssrj_vram1,*ssrj_vram2,*ssrj_vram3,*ssrj_vram4,*ssrj_scrollram;

WRITE8_HANDLER(ssrj_vram1_w);
WRITE8_HANDLER(ssrj_vram2_w);
WRITE8_HANDLER(ssrj_vram4_w);

VIDEO_START( ssrj );
VIDEO_UPDATE( ssrj );
PALETTE_INIT( ssrj );

static int oldport;

static MACHINE_RESET(ssrj)
{
	UINT8 *rom = memory_region(machine, "maincpu");
	memset(&rom[0xc000], 0 ,0x3fff); /* req for some control types */
	oldport = 0x80;
}

static READ8_HANDLER(ssrj_wheel_r)
{
	int port = input_port_read(space->machine, "IN1") - 0x80;
	int retval = port-oldport;
	oldport = port;
	return retval;
}

static ADDRESS_MAP_START( ssrj_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x7fff) AM_ROM
	AM_RANGE(0xc000, 0xc7ff) AM_RAM_WRITE(ssrj_vram1_w) AM_BASE(&ssrj_vram1)
	AM_RANGE(0xc800, 0xcfff) AM_RAM_WRITE(ssrj_vram2_w) AM_BASE(&ssrj_vram2)
	AM_RANGE(0xd000, 0xd7ff) AM_RAM AM_BASE(&ssrj_vram3)
	AM_RANGE(0xd800, 0xdfff) AM_RAM_WRITE(ssrj_vram4_w) AM_BASE(&ssrj_vram4)
	AM_RANGE(0xe000, 0xe7ff) AM_RAM
	AM_RANGE(0xe800, 0xefff) AM_RAM AM_BASE(&ssrj_scrollram)
	AM_RANGE(0xf000, 0xf000) AM_READ_PORT("IN0")
	AM_RANGE(0xf001, 0xf001) AM_READ(ssrj_wheel_r)
	AM_RANGE(0xf002, 0xf002) AM_READ_PORT("IN2")
	AM_RANGE(0xf003, 0xf003) AM_WRITENOP /* unknown */
	AM_RANGE(0xf401, 0xf401) AM_DEVREAD("ay", ay8910_r)
	AM_RANGE(0xf400, 0xf401) AM_DEVWRITE("ay", ay8910_address_data_w)
	AM_RANGE(0xf800, 0xf800) AM_WRITENOP /* wheel ? */
	AM_RANGE(0xfc00, 0xfc00) AM_WRITENOP /* unknown */
ADDRESS_MAP_END

static INPUT_PORTS_START( ssrj )
	PORT_START("IN0")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0xe0, 0x00, IPT_PEDAL ) PORT_MINMAX(0,0xe0) PORT_SENSITIVITY(50) PORT_KEYDELTA(0x20)

	PORT_START("IN1")
	PORT_BIT( 0xff, 0x00, IPT_DIAL  ) PORT_SENSITIVITY(50) PORT_KEYDELTA(4) PORT_REVERSE

	PORT_START("IN2")
	PORT_BIT( 0xf, IP_ACTIVE_LOW, IPT_BUTTON2  )  /* code @ $eef  , tested when controls = type4 */
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Difficulty ) ) /* ??? code @ $62c */
	PORT_DIPSETTING(	0x10, DEF_STR( Easy ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Normal ) )
	PORT_DIPSETTING(	0x20, DEF_STR( Difficult ) )
	PORT_DIPSETTING(	0x30, DEF_STR( Very_Difficult ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(	0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "No Hit" )
	PORT_DIPSETTING(	0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )

	PORT_START("IN3")
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(	0x07, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(	0x06, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(	0x05, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(	0x03, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )
	PORT_DIPSETTING(	0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Controls ) ) /* 'press button to start' message, and wait for button2 */
	PORT_DIPSETTING(	0x00, "Type 1" )
	PORT_DIPSETTING(	0x10, "Type 2" )
	PORT_DIPSETTING(	0x20, "Type 3" )
	PORT_DIPSETTING(	0x30, "Type 4" )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* sometimes hangs after game over ($69b) */
INPUT_PORTS_END

static const gfx_layout charlayout =
{
	8,8,	/* 8*8 characters */
	RGN_FRAC(1,3),	/* 1024 characters */
	3,	/* 3 bits per pixel */
	{ 0, RGN_FRAC(2,3), RGN_FRAC(1,3) },	/* the bitplanes are separated */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	8*8	/* every char takes 8 consecutive bytes */
};

static GFXDECODE_START( ssrj )
	GFXDECODE_ENTRY( "gfx1", 0, charlayout,     0, 8*4 )
GFXDECODE_END

static const ay8910_interface ay8910_config =
{
	AY8910_LEGACY_OUTPUT,
	AY8910_DEFAULT_LOADS,
	DEVCB_NULL, /* not used ? */
	DEVCB_INPUT_PORT("IN3"),
	DEVCB_NULL,
	DEVCB_NULL
};


static MACHINE_DRIVER_START( ssrj )

	/* basic machine hardware */
	MDRV_CPU_ADD("maincpu", Z80,8000000/2)
	MDRV_CPU_PROGRAM_MAP(ssrj_map)
	MDRV_CPU_VBLANK_INT("screen", irq0_line_hold)

	/* video hardware */
	MDRV_SCREEN_ADD("screen", RASTER)
	MDRV_SCREEN_REFRESH_RATE(60)
	MDRV_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(0))
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_SCREEN_VISIBLE_AREA(2*8, 30*8-1, 3*8, 32*8-1)

	MDRV_GFXDECODE(ssrj)
	MDRV_PALETTE_LENGTH(128)
	MDRV_PALETTE_INIT(ssrj)

	MDRV_VIDEO_START(ssrj)
	MDRV_VIDEO_UPDATE(ssrj)
//  MDRV_ASPECT_RATIO(3,4)

	MDRV_MACHINE_RESET(ssrj)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")

	MDRV_SOUND_ADD("ay", AY8910, 8000000/5)
	MDRV_SOUND_CONFIG(ay8910_config)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.30)
MACHINE_DRIVER_END

/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( ssrj )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "a40-01.bin",   0x0000, 0x4000, CRC(1ff7dbff) SHA1(a9e676ee087141d62f880cd98e7748db1e6e9461) )
	ROM_LOAD( "a40-02.bin",   0x4000, 0x4000, CRC(bbb36f9f) SHA1(9f85bac639d18ee932273a6c00b36ac969e69bb8) )

	ROM_REGION( 0x6000, "gfx1", 0 )
	ROM_LOAD( "a40-03.bin",   0x0000, 0x2000, CRC(3753182a) SHA1(3eda34f967563b11416344da87b7be46cbecff2b) )
	ROM_LOAD( "a40-04.bin",   0x2000, 0x2000, CRC(96471816) SHA1(e24b690085602b8bde079e596c2879deab128c83) )
	ROM_LOAD( "a40-05.bin",   0x4000, 0x2000, CRC(dce9169e) SHA1(2cdda1453b2913fad931788e1db0bc01ce923a04) )

	ROM_REGION( 0x100, "proms", 0 )
	ROM_LOAD( "proms",  0x0000, 0x0100, NO_DUMP )

ROM_END
#endif


//GAME( 1985, ssrj,  0,       ssrj,  ssrj,  0, ORIENTATION_FLIP_X, "Taito Corporation", "Super Speed Race Junior (Japan)",GAME_WRONG_COLORS|GAME_IMPERFECT_GRAPHICS )

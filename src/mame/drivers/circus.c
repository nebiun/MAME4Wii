/***************************************************************************

Circus memory map

driver by Mike Coates

0000-00FF Base Page RAM
0100-01FF Stack RAM
1000-1FFF ROM
2000      Clown Vertical Position
3000      Clown Horizontal Position
4000-43FF Video RAM
8000      Clown Rotation and Audio Controls
F000-FFF7 ROM
FFF8-FFFF Interrupt and Reset Vectors

A000      Control Switches
C000      Option Switches
D000      Paddle Position and Interrupt Reset

  CHANGES:
  MAB 09 MAR 99 - changed overlay support to use artwork functions
  AAT 12 MAY 02 - General: fixed red and white hue and removed dirty flags.
                  Ripcord: rewrote video driver, fixed gameplay, flipped
                           control and tuned DIP switches.
                  (Thanks Gregf for introducing these cool classics.)

    Notes:
        * Circus: Taito licensed and releasd the game as "Acrobat TV"

    2008-07
    Dip locations verified with the manuals.

***************************************************************************/

#include "driver.h"
#include "cpu/m6502/m6502.h"
#include "deprecat.h"
#include "sound/samples.h"
#include "circus.h"

#include "circus.lh"
#include "crash.lh"

#if 0
static int circus_interrupt;

static READ8_HANDLER( ripcord_IN2_r )
{
	circus_interrupt ++;
	logerror("circus_int: %02x\n", circus_interrupt);
	return readinputport (2);
}
#endif



static ADDRESS_MAP_START( circus_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x01ff) AM_RAM
	AM_RANGE(0x1000, 0x1fff) AM_ROM
	AM_RANGE(0x2000, 0x2000) AM_WRITE(circus_clown_x_w)
	AM_RANGE(0x3000, 0x3000) AM_WRITE(circus_clown_y_w)
	AM_RANGE(0x4000, 0x43ff) AM_RAM_WRITE(circus_videoram_w) AM_BASE(&videoram)
	AM_RANGE(0x8000, 0x8000) AM_RAM_WRITE(circus_clown_z_w)
	AM_RANGE(0xa000, 0xa000) AM_READ_PORT("INPUTS")
	AM_RANGE(0xc000, 0xc000) AM_READ_PORT("DSW")
	AM_RANGE(0xd000, 0xd000) AM_READ_PORT("PADDLE")
//  AM_RANGE(0xd000, 0xd000) AM_READ(ripcord_IN2_r)
	AM_RANGE(0xf000, 0xffff) AM_ROM
ADDRESS_MAP_END


static INPUT_PORTS_START( circus )
	PORT_START("INPUTS")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x7c, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN1 )

	PORT_START("DSW")
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) ) PORT_DIPLOCATION("14A:6,7")
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x02, "7" )
	PORT_DIPSETTING(    0x03, "9" )
	PORT_DIPNAME( 0x0c, 0x04, DEF_STR( Coinage ) ) PORT_DIPLOCATION("14A:4,5")
//  PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x00, "High Score" ) PORT_DIPLOCATION("14A:3")
	PORT_DIPSETTING(    0x10, "Credit Awarded" )
	PORT_DIPSETTING(    0x00, "No Award" )
	PORT_DIPNAME( 0x20, 0x00, "Bonus" ) PORT_DIPLOCATION("14A:2")
	PORT_DIPSETTING(    0x00, "Single Line" )
	PORT_DIPSETTING(    0x20, "Super Bonus" )
	PORT_DIPUNKNOWN_DIPLOC( 0x40, 0x00, "14A:1" ) /* Not mentioned in the manual */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_VBLANK )

	PORT_START("PADDLE")
	PORT_BIT( 0xff, 115, IPT_PADDLE ) PORT_MINMAX(64,167) PORT_SENSITIVITY(30) PORT_KEYDELTA(10) PORT_CENTERDELTA(0)
INPUT_PORTS_END

#if 0
static INPUT_PORTS_START( robotbwl )
	PORT_START("INPUTS")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("Hook Right") PORT_CODE(KEYCODE_X)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("Hook Left") PORT_CODE(KEYCODE_Z)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN1 )

	PORT_START("DSW")
	PORT_DIPUNKNOWN_DIPLOC( 0x01, 0x00, "14A:7" ) /* Manual says it's unused */
	PORT_DIPUNKNOWN_DIPLOC( 0x02, 0x00, "14A:6" ) /* Manual says it's unused */
	PORT_DIPNAME( 0x04, 0x04, "Beer Frame" ) PORT_DIPLOCATION("14A:5")
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x18, 0x08, DEF_STR( Coinage ) ) PORT_DIPLOCATION("14A:3,4")
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_2C ) )
//  PORT_DIPSETTING(    0x18, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x60, 0x00, "Bowl Timer" ) PORT_DIPLOCATION("14A:1,2")
	PORT_DIPSETTING(    0x00, "3 seconds" )
	PORT_DIPSETTING(    0x20, "5 seconds" )
	PORT_DIPSETTING(    0x40, "7 seconds" )
	PORT_DIPSETTING(    0x60, "9 seconds" )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START("PADDLE")
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_UNUSED)
INPUT_PORTS_END
#endif

static INPUT_PORTS_START( crash )
	PORT_START("INPUTS")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_4WAY
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_4WAY
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_4WAY
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_4WAY
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN1 )

	PORT_START("DSW")
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) ) PORT_DIPLOCATION("14A:6,7")
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x03, "5" )
	PORT_DIPNAME( 0x0C, 0x04, DEF_STR( Coinage ) ) PORT_DIPLOCATION("14A:4,5")
//  PORT_DIPSETTING(    0x0c, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x00, "High Score" ) PORT_DIPLOCATION("14A:3")
	PORT_DIPSETTING(    0x00, "No Award" )
	PORT_DIPSETTING(    0x10, "Credit Awarded" )
	PORT_BIT( 0x60, IP_ACTIVE_HIGH, IPT_UNKNOWN )	// "14A:1,2" not mentioned in the manual
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START("PADDLE")
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_UNUSED)

	PORT_START("R63")
	PORT_ADJUSTER( 90, "R63 - Music Volume" )

	PORT_START("R39")
	PORT_ADJUSTER( 40, "R39 - Beeper Volume" )
INPUT_PORTS_END


static INPUT_PORTS_START( ripcord )
	PORT_START("INPUTS")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN1 )

	PORT_START("DSW")
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) ) PORT_DIPLOCATION("14A:6,7")
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x02, "7" )
	PORT_DIPSETTING(    0x03, "9" )
	PORT_DIPNAME( 0x0c, 0x04, DEF_STR( Coinage ) ) PORT_DIPLOCATION("14A:4,5")
//  PORT_DIPSETTING(    0x0c, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x00, "High Score" ) PORT_DIPLOCATION("14A:3")
	PORT_DIPSETTING(    0x10, "Award Credit" )
	PORT_DIPSETTING(    0x00, "No Award" )
	/* "14A:1,2" switches are not mentioned in the manual */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_VBLANK )

	PORT_START("PADDLE")
	PORT_BIT( 0xff, 115, IPT_PADDLE ) PORT_MINMAX(64,167) PORT_SENSITIVITY(30) PORT_KEYDELTA(10) PORT_CENTERDELTA(0) PORT_REVERSE
INPUT_PORTS_END


static const gfx_layout charlayout =
{
	8,8,    /* 8*8 characters */
	256,    /* 256 characters */
	1,              /* 1 bit per pixel */
	{ 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8     /* every char takes 8 consecutive bytes */
};

static const gfx_layout clownlayout =
{
	16,16,  /* 16*16 characters */
	16,             /* 16 characters */
	1,              /* 1 bit per pixel */
	{ 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7,
	  16*8, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
	  8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	16*16   /* every char takes 64 consecutive bytes */
};

#if 0
static const gfx_layout robotlayout =
{
	8,8,  /* 16*16 characters */
	1,      /* 1 character */
	1,      /* 1 bit per pixel */
	{ 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8
};
#endif

static GFXDECODE_START( circus )
	GFXDECODE_ENTRY( "gfx1", 0, charlayout,  0, 1 )
	GFXDECODE_ENTRY( "gfx2", 0, clownlayout, 0, 1 )
GFXDECODE_END

#if 0
static GFXDECODE_START( robotbwl )
	GFXDECODE_ENTRY( "gfx1", 0, charlayout,  0, 1 )
	GFXDECODE_ENTRY( "gfx2", 0, robotlayout, 0, 1 )
GFXDECODE_END
#endif


/***************************************************************************
  Machine drivers
***************************************************************************/
#if 0
static INTERRUPT_GEN( ripcord_interrupt )
{
	circus_interrupt = 0;
}
#endif

static MACHINE_DRIVER_START( circus )

	/* basic machine hardware */
	MDRV_CPU_ADD("maincpu", M6502, XTAL_11_289MHz / 16) /* 705.562kHz */
	MDRV_CPU_PROGRAM_MAP(circus_map)
	MDRV_CPU_VBLANK_INT("screen", irq0_line_hold)

	/* video hardware */
	MDRV_SCREEN_ADD("screen", RASTER)
	MDRV_SCREEN_REFRESH_RATE(57)
	MDRV_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(3500)  /* frames per second, vblank duration (complete guess) */)
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_SCREEN_VISIBLE_AREA(0*8, 31*8-1, 0*8, 32*8-1)

	MDRV_GFXDECODE(circus)
	MDRV_PALETTE_LENGTH(2)

	MDRV_PALETTE_INIT(black_and_white)
	MDRV_VIDEO_START(circus)
	MDRV_VIDEO_UPDATE(circus)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")

	MDRV_SOUND_ADD("samples", SAMPLES, 0)
	MDRV_SOUND_CONFIG(circus_samples_interface)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.80)

	MDRV_SOUND_ADD("discrete", DISCRETE, 0)
	MDRV_SOUND_CONFIG_DISCRETE(circus)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.50)
MACHINE_DRIVER_END

#if 0
static MACHINE_DRIVER_START( robotbwl )

	/* basic machine hardware */
	MDRV_CPU_ADD("maincpu", M6502, XTAL_11_289MHz / 16) /* 705.562kHz */
	MDRV_CPU_PROGRAM_MAP(circus_map)
	MDRV_CPU_VBLANK_INT("screen", irq0_line_hold)

	/* video hardware */
	MDRV_SCREEN_ADD("screen", RASTER)
	MDRV_SCREEN_REFRESH_RATE(57)
	MDRV_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(3500)  /* frames per second, vblank duration (complete guess) */)
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_SCREEN_VISIBLE_AREA(0*8, 31*8-1, 0*8, 32*8-1)

	MDRV_GFXDECODE(robotbwl)
	MDRV_PALETTE_LENGTH(2)

	MDRV_PALETTE_INIT(black_and_white)
	MDRV_VIDEO_START(circus)
	MDRV_VIDEO_UPDATE(robotbwl)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")

	MDRV_SOUND_ADD("samples", SAMPLES, 0)
	MDRV_SOUND_CONFIG(robotbwl_samples_interface)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.80)

	MDRV_SOUND_ADD("discrete", DISCRETE, 0)
	MDRV_SOUND_CONFIG_DISCRETE(robotbwl)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 1.0)
MACHINE_DRIVER_END
#endif

static MACHINE_DRIVER_START( crash )

	/* basic machine hardware */
	MDRV_CPU_ADD("maincpu", M6502, XTAL_11_289MHz / 16) /* 705.562kHz */
	MDRV_CPU_PROGRAM_MAP(circus_map)
	MDRV_CPU_VBLANK_INT_HACK(irq0_line_hold,2)

	/* video hardware */
	MDRV_SCREEN_ADD("screen", RASTER)
	MDRV_SCREEN_REFRESH_RATE(57)
	MDRV_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(3500)  /* frames per second, vblank duration (complete guess) */)
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_SCREEN_VISIBLE_AREA(0*8, 31*8-1, 0*8, 32*8-1)

	MDRV_GFXDECODE(circus)
	MDRV_PALETTE_LENGTH(2)

	MDRV_PALETTE_INIT(black_and_white)
	MDRV_VIDEO_START(circus)
	MDRV_VIDEO_UPDATE(crash)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")

	MDRV_SOUND_ADD("samples", SAMPLES, 0)
	MDRV_SOUND_CONFIG(crash_samples_interface)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.80)

	MDRV_SOUND_ADD("discrete", DISCRETE, 0)
	MDRV_SOUND_CONFIG_DISCRETE(crash)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.80)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( ripcord )

	/* basic machine hardware */
	MDRV_CPU_ADD("maincpu", M6502, XTAL_11_289MHz / 16) /* 705.562kHz */
	MDRV_CPU_PROGRAM_MAP(circus_map)
	//MDRV_CPU_VBLANK_INT("screen", ripcord_interrupt) //AT

	/* video hardware */
	MDRV_SCREEN_ADD("screen", RASTER)
	MDRV_SCREEN_REFRESH_RATE(57)
	MDRV_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(3500)  /* frames per second, vblank duration (complete guess) */)
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_SCREEN_VISIBLE_AREA(0*8, 31*8-1, 0*8, 32*8-1)

	MDRV_GFXDECODE(circus)
	MDRV_PALETTE_LENGTH(2)

	MDRV_PALETTE_INIT(black_and_white)
	MDRV_VIDEO_START(circus)
	MDRV_VIDEO_UPDATE(ripcord)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")

	MDRV_SOUND_ADD("samples", SAMPLES, 0)
	MDRV_SOUND_CONFIG(ripcord_samples_interface)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.80)

	MDRV_SOUND_ADD("discrete", DISCRETE, 0)
	MDRV_SOUND_CONFIG_DISCRETE(circus)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.50)
MACHINE_DRIVER_END



ROM_START( circus )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "9004.1a",    0x1000, 0x0200, CRC(7654ea75) SHA1(fa29417618157002b8ecb21f4c15104c8145a742) ) /* Code */
	ROM_LOAD( "9005.2a",    0x1200, 0x0200, CRC(b8acdbc5) SHA1(634bb11089f7a57a316b6829954cc4da4523f267) )
	ROM_LOAD( "9006.3a",    0x1400, 0x0200, CRC(901dfff6) SHA1(c1f48845456e88d54981608afd00ddb92d97da99) )
	ROM_LOAD( "9007.5a",    0x1600, 0x0200, CRC(9dfdae38) SHA1(dc59a5f90a5a49fa071aada67eda768d3ecef010) )
	ROM_LOAD( "9008.6a",    0x1800, 0x0200, CRC(c8681cf6) SHA1(681cfea75bee8a86f9f4645e6c6b94b44762dae9) )
	ROM_LOAD( "9009.7a",    0x1a00, 0x0200, CRC(585f633e) SHA1(46133409f42e8cbc095dde576ce07d97b235972d) )
	ROM_LOAD( "9010.8a",    0x1c00, 0x0200, CRC(69cc409f) SHA1(b77289e62313e8535ce40686df7238aa9c0035bc) )
	ROM_LOAD( "9011.9a",    0x1e00, 0x0200, CRC(aff835eb) SHA1(d6d95510d4a046f48358fef01103bcc760eb71ed) )
	ROM_RELOAD(               0xfe00, 0x0200 ) /* for the reset and interrupt vectors */

	ROM_REGION( 0x0800, "gfx1", 0 )
	ROM_LOAD( "9003.4c",    0x0000, 0x0200, CRC(6efc315a) SHA1(d5a4a64a901853fff56df3c65512afea8336aad2) )  /* Character Set */
	ROM_LOAD( "9002.3c",    0x0200, 0x0200, CRC(30d72ef5) SHA1(45fc8285e213bf3906a26205a8c0b22f311fd6c3) )
	ROM_LOAD( "9001.2c",    0x0400, 0x0200, CRC(361da7ee) SHA1(6e6fe5b37ccb4c11aa4abbd9b7df772953abfe7e) )
	ROM_LOAD( "9000.1c",    0x0600, 0x0200, CRC(1f954bb3) SHA1(62a958b48078caa639b96f62a690583a1c8e83f5) )

	ROM_REGION( 0x0200, "gfx2", 0 )
	ROM_LOAD( "9012.14d",   0x0000, 0x0200, CRC(2fde3930) SHA1(a21e2d342f16a39a07edf4bea8d698a52216ecba) )  /* Clown */
ROM_END

#if 0
ROM_START( circusse )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "93448.1a",    0x1000, 0x0200, CRC(44d65ccd) SHA1(0eb2515444486a4656a4accec555501e75b39a74) ) /* Code */
	ROM_LOAD( "93448.2a",    0x1200, 0x0200, CRC(b8acdbc5) SHA1(634bb11089f7a57a316b6829954cc4da4523f267) )
	ROM_LOAD( "93448.3a",    0x1400, 0x0200, CRC(f2e25f7a) SHA1(6441e39fc7f710442dd6a3a047826862b0481c58) )
	ROM_LOAD( "93448.5a",    0x1600, 0x0200, CRC(9dfdae38) SHA1(dc59a5f90a5a49fa071aada67eda768d3ecef010) )
	ROM_LOAD( "93448.6a",    0x1800, 0x0200, CRC(c8681cf6) SHA1(681cfea75bee8a86f9f4645e6c6b94b44762dae9) )
	ROM_LOAD( "93448.7a",    0x1a00, 0x0200, CRC(585f633e) SHA1(46133409f42e8cbc095dde576ce07d97b235972d) )
	ROM_LOAD( "93448.8a",    0x1c00, 0x0200, CRC(d7c0dc05) SHA1(cc6f7d16ca4be74370c305c34aa1a2e338d2c41f) )
	ROM_LOAD( "93448.9a",    0x1e00, 0x0200, CRC(aff835eb) SHA1(d6d95510d4a046f48358fef01103bcc760eb71ed) )
	ROM_RELOAD(               0xfe00, 0x0200 ) /* for the reset and interrupt vectors */

	ROM_REGION( 0x0800, "gfx1", 0 )
	ROM_LOAD( "93448.4c",    0x0000, 0x0200, CRC(6efc315a) SHA1(d5a4a64a901853fff56df3c65512afea8336aad2) )  /* Character Set */
	ROM_LOAD( "93448.3c",    0x0200, 0x0200, CRC(30d72ef5) SHA1(45fc8285e213bf3906a26205a8c0b22f311fd6c3) )
	ROM_LOAD( "93448.2c",    0x0400, 0x0200, CRC(361da7ee) SHA1(6e6fe5b37ccb4c11aa4abbd9b7df772953abfe7e) )
	ROM_LOAD( "93448.1c",    0x0600, 0x0200, CRC(1f954bb3) SHA1(62a958b48078caa639b96f62a690583a1c8e83f5) )

	ROM_REGION( 0x0200, "gfx2", 0 )
	ROM_LOAD( "93448.14d",   0x0000, 0x0200, CRC(2fde3930) SHA1(a21e2d342f16a39a07edf4bea8d698a52216ecba) )  /* Clown */
ROM_END
#endif
#if 0
ROM_START( robotbwl )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "robotbwl.1a",  0xf000, 0x0200, CRC(df387a0b) SHA1(97291f1a93cbbff987b0fbc16c2e87ad0db96e12) ) /* Code */
	ROM_LOAD( "robotbwl.2a",  0xf200, 0x0200, CRC(c948274d) SHA1(1bf8c6e994d601d4e6d30ca2a9da97e140ff5eee) )
	ROM_LOAD( "robotbwl.3a",  0xf400, 0x0200, CRC(8fdb3ec5) SHA1(a9290edccb8f75e7ec91416d46617516260d5944) )
	ROM_LOAD( "robotbwl.5a",  0xf600, 0x0200, CRC(ba9a6929) SHA1(9cc6e85431b5d82bf3a624f7b35ddec399ad6c80) )
	ROM_LOAD( "robotbwl.6a",  0xf800, 0x0200, CRC(16fd8480) SHA1(935bb0c87d25086f326571c83f94f831b1a8b036) )
	ROM_LOAD( "robotbwl.7a",  0xfa00, 0x0200, CRC(4cadbf06) SHA1(380c10aa83929bfbfd89facb252e68c307545755) )
	ROM_LOAD( "robotbwl.8a",  0xfc00, 0x0200, CRC(bc809ed3) SHA1(2bb4cdae8c9619eebea30cc323960a46a509bb58) )
	ROM_LOAD( "robotbwl.9a",  0xfe00, 0x0200, CRC(07487e27) SHA1(b5528fb3fec474df2b66f36e28df13a7e81f9ce3) )

	ROM_REGION( 0x0800, "gfx1", 0 )
	ROM_LOAD( "robotbwl.4c",  0x0000, 0x0200, CRC(a5f7acb9) SHA1(556dd34d0fa50415b128477e208e96bf0c050c2c) )  /* Character Set */
	ROM_LOAD( "robotbwl.3c",  0x0200, 0x0200, CRC(d5380c9b) SHA1(b9670e87011a1b3aebd1d386f1fe6a74f8c77be9) )
	ROM_LOAD( "robotbwl.2c",  0x0400, 0x0200, CRC(47b3e39c) SHA1(393c680fba3bd384e2c773150c3bae4d735a91bf) )
	ROM_LOAD( "robotbwl.1c",  0x0600, 0x0200, CRC(b2991e7e) SHA1(32b6d42bb9312d6cbe5b4113fcf2262bfeef3777) )

	ROM_REGION( 0x0020, "gfx2", ROMREGION_INVERT )
	ROM_LOAD( "robotbwl.14d", 0x0000, 0x0020, CRC(a402ac06) SHA1(3bd75630786bcc86d9e9fbc826adc909eef9b41f) )  /* Ball */
ROM_END
#endif

ROM_START( crash )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "crash.a1",     0x1000, 0x0200, CRC(b9571203) SHA1(1299e476598d07a67aa1640f3320de1198280296) ) /* Code */
	ROM_LOAD( "crash.a2",     0x1200, 0x0200, CRC(b4581a95) SHA1(b3662bda5013443a56eabbe21fefa91e255e18e7) )
	ROM_LOAD( "crash.a3",     0x1400, 0x0200, CRC(597555ae) SHA1(39a6d10e229be0e0d52b1061f2aa2f678b351f0b) )
	ROM_LOAD( "crash.a4",     0x1600, 0x0200, CRC(0a15d69f) SHA1(c3a7b5ce4406cce511108e5c015b1dd5587b75ed) )
	ROM_LOAD( "crash.a5",     0x1800, 0x0200, CRC(a9c7a328) SHA1(2f21ee58ba117bf4fe9101373c55449217a08da6) )
	ROM_LOAD( "crash.a6",     0x1a00, 0x0200, CRC(c7d62d27) SHA1(974800cbeba2f2d0d796200d235371e2ce3a1d28) )
	ROM_LOAD( "crash.a7",     0x1c00, 0x0200, CRC(5e5af244) SHA1(9ea27241a5ac97b260599d56f60bf9ec3ffcac7f) )
	ROM_LOAD( "crash.a8",     0x1e00, 0x0200, CRC(3dc50839) SHA1(5782ea7d70e5cbe8b8245ed1075ce92b57cc6ddf) )
	ROM_RELOAD(               0xfe00, 0x0200 ) /* for the reset and interrupt vectors */

	ROM_REGION( 0x0800, "gfx1", 0 )
	ROM_LOAD( "crash.c4",     0x0000, 0x0200, CRC(ba16f9e8) SHA1(fdbf8d36993196552ddb7729750420f8e31eee70) )  /* Character Set */
	ROM_LOAD( "crash.c3",     0x0200, 0x0200, CRC(3c8f7560) SHA1(ce4023167a0b4b912bbbc70b00fd3b462990a04c) )
	ROM_LOAD( "crash.c2",     0x0400, 0x0200, CRC(38f3e4ed) SHA1(4e537402c09b58997bc45498fd721d83a0eac3a7) )
	ROM_LOAD( "crash.c1",     0x0600, 0x0200, CRC(e9adf1e1) SHA1(c1f6d2a3be1e9b35c8675d1e3f57e6a85ddd99fd) )

	ROM_REGION( 0x0200, "gfx2", 0 )
	ROM_LOAD( "crash.d14",    0x0000, 0x0200, CRC(833f81e4) SHA1(78a0ace3510546691ecaf6f6275cb3269495edc9) )  /* Cars */
ROM_END

ROM_START( ripcord )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "9027.1a",      0x1000, 0x0200, CRC(56b8dc06) SHA1(5432e4f2e321805a8dc9cfce20b8372793a9a4dd) ) /* Code */
	ROM_LOAD( "9028.2a",      0x1200, 0x0200, CRC(a8a78a30) SHA1(e6ddcba608f9b34e07a5402872793dafe5054156) )
	ROM_LOAD( "9029.4a",      0x1400, 0x0200, CRC(fc5c8e07) SHA1(4784a868491393f42520f6609266ffab21661ec3) )
	ROM_LOAD( "9030.5a",      0x1600, 0x0200, CRC(b496263c) SHA1(36321aa6d18e7c35461c1d445d2682d61279a8c7) )
	ROM_LOAD( "9031.6a",      0x1800, 0x0200, CRC(cdc7d46e) SHA1(369bb119320cd737641a5bf64d51c9b552578f8a) )
	ROM_LOAD( "9032.7a",      0x1a00, 0x0200, CRC(a6588bec) SHA1(76321ab29329b6291e4d4731bb445a6ac4ce2d86) )
	ROM_LOAD( "9033.8a",      0x1c00, 0x0200, CRC(fd49b806) SHA1(5205ee8e9cec53be6e79e0183bc1e9d96c8c2e55) )
	ROM_LOAD( "9034.9a",      0x1e00, 0x0200, CRC(7caf926d) SHA1(f51d010ce1909e21e04313e4262c70ab948c14e0) )
	ROM_RELOAD(               0xfe00, 0x0200 ) /* for the reset and interrupt vectors */

	ROM_REGION( 0x0800, "gfx1", 0 )
	ROM_LOAD( "9026.5c",      0x0000, 0x0200, CRC(06e7adbb) SHA1(0c119743eacc30d6d9eb50dfee0746b69bb17377) )  /* Character Set */
	ROM_LOAD( "9025.4c",      0x0200, 0x0200, CRC(3129527e) SHA1(3d0519811c9e4a5645f5c54ed8f0b411cdc5d54b) )
	ROM_LOAD( "9024.2c",      0x0400, 0x0200, CRC(bcb88396) SHA1(d92dff2436f58d977f9196a88fa7701c3032ef7d) )
	ROM_LOAD( "9023.1c",      0x0600, 0x0200, CRC(9f86ed5b) SHA1(fbe38c6d63887e603d919b0ab2216cd44b8955e4) )

	ROM_REGION( 0x0200, "gfx2", 0 )
	ROM_LOAD( "9035.14d",     0x0000, 0x0200, CRC(c9979802) SHA1(cf6dfad0821fa736c8fcf8735792054858232806) )
ROM_END


static DRIVER_INIT( circus )
{
	circus_game = 1;
}

#if 0
static DRIVER_INIT( robotbwl )
{
	circus_game = 2;
}
#endif

static DRIVER_INIT( crash )
{
	circus_game = 3;
}
static DRIVER_INIT( ripcord )
{
	circus_game = 4;
}


GAMEL(1977, circus,   0,      circus,   circus,   circus,   ROT0, "Exidy / Taito", "Circus / Acrobat TV", GAME_SUPPORTS_SAVE | GAME_IMPERFECT_SOUND, layout_circus )
//GAMEL(1977, circusse, circus, circus,   circus,   circus,   ROT0, "[Exidy] (Sub-Electro bootleg)", "Circus (Sub-Electro bootleg)", GAME_SUPPORTS_SAVE | GAME_IMPERFECT_SOUND, layout_circus ) // looks like a text hack, but we've seen 2 identical copies so it's worth supporting
//GAME( 1977, robotbwl, 0,      robotbwl, robotbwl, robotbwl, ROT0, "Exidy", "Robot Bowl", GAME_SUPPORTS_SAVE | GAME_IMPERFECT_SOUND )
GAMEL(1979, crash,    0,      crash,    crash,    crash,    ROT0, "Exidy", "Crash", GAME_SUPPORTS_SAVE | GAME_IMPERFECT_SOUND, layout_crash )
GAME( 1979, ripcord,  0,      ripcord,  ripcord,  ripcord,  ROT0, "Exidy", "Rip Cord", GAME_SUPPORTS_SAVE | GAME_IMPERFECT_SOUND )

/***************************************************************************

    Haunted Castle

    Emulation by Bryan McPhail, mish@tendril.co.uk

***************************************************************************/

#include "driver.h"
#include "cpu/konami/konami.h"
#include "cpu/z80/z80.h"
#include "sound/3812intf.h"
#include "sound/k007232.h"
#include "sound/k051649.h"
#include "konamipt.h"

PALETTE_INIT( hcastle );
VIDEO_UPDATE( hcastle );
VIDEO_START( hcastle );

extern UINT8 *hcastle_pf1_videoram,*hcastle_pf2_videoram;

WRITE8_HANDLER( hcastle_pf1_video_w );
WRITE8_HANDLER( hcastle_pf2_video_w );
READ8_HANDLER( hcastle_gfxbank_r );
WRITE8_HANDLER( hcastle_gfxbank_w );
WRITE8_HANDLER( hcastle_pf1_control_w );
WRITE8_HANDLER( hcastle_pf2_control_w );

static WRITE8_HANDLER( hcastle_bankswitch_w )
{
	UINT8 *RAM = memory_region(space->machine, "maincpu");
	int bankaddress;

	bankaddress = 0x10000 + (data & 0x1f) * 0x2000;
	memory_set_bankptr(space->machine, 1,&RAM[bankaddress]);
}

static WRITE8_HANDLER( hcastle_soundirq_w )
{
	cputag_set_input_line(space->machine, "audiocpu", 0, HOLD_LINE );
}

static WRITE8_HANDLER( hcastle_coin_w )
{
	coin_counter_w(0,data & 0x40);
	coin_counter_w(1,data & 0x80);
}



static ADDRESS_MAP_START( hcastle_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x0007) AM_WRITE(hcastle_pf1_control_w)
	AM_RANGE(0x0020, 0x003f) AM_RAM	/* rowscroll? */
	AM_RANGE(0x0200, 0x0207) AM_WRITE(hcastle_pf2_control_w)
	AM_RANGE(0x0220, 0x023f) AM_RAM /* rowscroll? */
	AM_RANGE(0x0400, 0x0400) AM_WRITE(hcastle_bankswitch_w)
	AM_RANGE(0x0404, 0x0404) AM_WRITE(soundlatch_w)
	AM_RANGE(0x0408, 0x0408) AM_WRITE(hcastle_soundirq_w)
	AM_RANGE(0x040c, 0x040c) AM_WRITE(watchdog_reset_w)
	AM_RANGE(0x0410, 0x0410) AM_READ_PORT("SYSTEM") AM_WRITE(hcastle_coin_w)
	AM_RANGE(0x0411, 0x0411) AM_READ_PORT("P1")
	AM_RANGE(0x0412, 0x0412) AM_READ_PORT("P2")
	AM_RANGE(0x0413, 0x0413) AM_READ_PORT("DSW3")
	AM_RANGE(0x0414, 0x0414) AM_READ_PORT("DSW1")
	AM_RANGE(0x0415, 0x0415) AM_READ_PORT("DSW2")
	AM_RANGE(0x0418, 0x0418) AM_READWRITE(hcastle_gfxbank_r, hcastle_gfxbank_w)
	AM_RANGE(0x0600, 0x06ff) AM_RAM AM_BASE(&paletteram)
	AM_RANGE(0x0700, 0x1fff) AM_RAM
	AM_RANGE(0x2000, 0x2fff) AM_RAM_WRITE(hcastle_pf1_video_w) AM_BASE(&hcastle_pf1_videoram)
	AM_RANGE(0x3000, 0x3fff) AM_RAM AM_BASE(&spriteram) AM_SIZE(&spriteram_size)
	AM_RANGE(0x4000, 0x4fff) AM_RAM_WRITE(hcastle_pf2_video_w) AM_BASE(&hcastle_pf2_videoram)
	AM_RANGE(0x5000, 0x5fff) AM_RAM AM_BASE(&spriteram_2) AM_SIZE(&spriteram_2_size)
	AM_RANGE(0x6000, 0x7fff) AM_ROMBANK(1)
	AM_RANGE(0x8000, 0xffff) AM_ROM
ADDRESS_MAP_END

/*****************************************************************************/

static WRITE8_DEVICE_HANDLER( sound_bank_w )
{
	int bank_A=(data&0x3);
	int bank_B=((data>>2)&0x3);
	k007232_set_bank(device, bank_A, bank_B );
}

static ADDRESS_MAP_START( sound_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x7fff) AM_ROM
	AM_RANGE(0x8000, 0x87ff) AM_RAM
	AM_RANGE(0x9800, 0x987f) AM_DEVWRITE("konami2", k051649_waveform_w)
	AM_RANGE(0x9880, 0x9889) AM_DEVWRITE("konami2", k051649_frequency_w)
	AM_RANGE(0x988a, 0x988e) AM_DEVWRITE("konami2", k051649_volume_w)
	AM_RANGE(0x988f, 0x988f) AM_DEVWRITE("konami2", k051649_keyonoff_w)
	AM_RANGE(0xa000, 0xa001) AM_DEVREADWRITE("ym", ym3812_r, ym3812_w)
	AM_RANGE(0xb000, 0xb00d) AM_DEVREADWRITE("konami1", k007232_r, k007232_w)
	AM_RANGE(0xc000, 0xc000) AM_DEVWRITE("konami1", sound_bank_w) /* 7232 bankswitch */
	AM_RANGE(0xd000, 0xd000) AM_READ(soundlatch_r)
ADDRESS_MAP_END

/*****************************************************************************/

static INPUT_PORTS_START( hcastle )
	PORT_START("SYSTEM")
	KONAMI8_SYSTEM_UNK

	PORT_START("P1")
	KONAMI8_MONO_B12_UNK

	PORT_START("P2")
	KONAMI8_COCKTAIL_B12_UNK

	PORT_START("DSW1")
	KONAMI_COINAGE_LOC(DEF_STR( Free_Play ), "Invalid", SW1)
	/* "Invalid" = both coin slots disabled */

	PORT_START("DSW2")
	PORT_DIPUNUSED_DIPLOC( 0x01, 0x01, "SW2:1" )		/* Listed as "Unused" */
	PORT_DIPUNUSED_DIPLOC( 0x02, 0x02, "SW2:2" )		/* Listed as "Unused" */
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Cabinet ) )		PORT_DIPLOCATION("SW2:3")
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x18, 0x10, "Difficulty 1 (Game)" )		PORT_DIPLOCATION("SW2:4,5")	/* Overall difficulty of game */
	PORT_DIPSETTING(    0x18, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Difficult ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Very_Difficult ) )
	PORT_DIPNAME( 0x60, 0x40, "Difficulty 2 (Strength)" )		PORT_DIPLOCATION("SW2:6,7")	/* Listed in manual as "Strength of Player" */
	PORT_DIPSETTING(    0x00, "Very Weak" )							/* Takes most damage per hit */
	PORT_DIPSETTING(    0x20, "Weak" )							/* Takes more damage per hit */
	PORT_DIPSETTING(    0x40, DEF_STR( Normal ) )						/* Takes average damage per hit */
	PORT_DIPSETTING(    0x60, "Strong" )							/* Takes least damage per hit */
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )	PORT_DIPLOCATION("SW2:8")
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START("DSW3")
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )	PORT_DIPLOCATION("SW3:1")
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Upright Controls" )		PORT_DIPLOCATION("SW3:2")
	PORT_DIPSETTING(    0x02, DEF_STR( Single ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Dual ) )
	PORT_SERVICE_DIPLOC(  0x04, IP_ACTIVE_LOW, "SW3:3" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Allow_Continue ) )	PORT_DIPLOCATION("SW3:4")
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x08, "Up to 3 Times" )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

/*****************************************************************************/

static const gfx_layout charlayout =
{
	8,8,
	32768,
	4,
	{ 0, 1, 2, 3 },
	{ 2*4, 3*4, 0*4, 1*4, 6*4, 7*4, 4*4, 5*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8
};

static GFXDECODE_START( hcastle )
	GFXDECODE_ENTRY( "gfx1", 0, charlayout,       0, 8*16 )	/* 007121 #0 */
	GFXDECODE_ENTRY( "gfx2", 0, charlayout, 8*16*16, 8*16 )	/* 007121 #1 */
GFXDECODE_END

/*****************************************************************************/

static void irqhandler(const device_config *device, int linestate)
{
//  cputag_set_input_line(device->machine, "audiocpu", 0, linestate);
}

static void volume_callback(const device_config *device, int v)
{
	k007232_set_volume(device,0,(v >> 4) * 0x11,0);
	k007232_set_volume(device,1,0,(v & 0x0f) * 0x11);
}

static const k007232_interface k007232_config =
{
	volume_callback	/* external port callback */
};

static const ym3812_interface ym3812_config =
{
	irqhandler
};

static MACHINE_DRIVER_START( hcastle )

	/* basic machine hardware */
	MDRV_CPU_ADD("maincpu", KONAMI, 3000000)	/* Derived from 24 MHz clock */
	MDRV_CPU_PROGRAM_MAP(hcastle_map)
	MDRV_CPU_VBLANK_INT("screen", irq0_line_hold)

	MDRV_CPU_ADD("audiocpu", Z80, 3579545)
	MDRV_CPU_PROGRAM_MAP(sound_map)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_BUFFERS_SPRITERAM)

	MDRV_SCREEN_ADD("screen", RASTER)
	MDRV_SCREEN_REFRESH_RATE(59)
	MDRV_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(0)	/* frames per second verified by comparison with real board */)
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_SCREEN_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)

	MDRV_GFXDECODE(hcastle)
	MDRV_PALETTE_LENGTH(2*8*16*16)

	MDRV_PALETTE_INIT(hcastle)
	MDRV_VIDEO_START(hcastle)
	MDRV_VIDEO_UPDATE(hcastle)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")

	MDRV_SOUND_ADD("konami1", K007232, 3579545)
	MDRV_SOUND_CONFIG(k007232_config)
	MDRV_SOUND_ROUTE(0, "mono", 0.44)
	MDRV_SOUND_ROUTE(1, "mono", 0.50)

	MDRV_SOUND_ADD("ym", YM3812, 3579545)
	MDRV_SOUND_CONFIG(ym3812_config)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.70)

	MDRV_SOUND_ADD("konami2", K051649, 3579545/2)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.45)
MACHINE_DRIVER_END

/***************************************************************************/

ROM_START( hcastle )
	ROM_REGION( 0x30000, "maincpu", 0 )
	ROM_LOAD( "m03.k12",      0x08000, 0x08000, CRC(d85e743d) SHA1(314e2a2bbe650540306b85c8b89ec5bcaef11a0d) )
	ROM_LOAD( "b06.k8",       0x10000, 0x20000, CRC(abd07866) SHA1(a261d0cd90f5909abd06e8b691669e63d890c3be) )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "768.e01",      0x00000, 0x08000, CRC(b9fff184) SHA1(c55f468c0da6afdaa2af65a111583c0c42868bd1) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD( "768c09.g21",   0x000000, 0x80000, CRC(e3be3fdd) SHA1(01a686af33a0a700066b1a5334d8552454ff186f) )
	ROM_LOAD( "768c08.g19",   0x080000, 0x80000, CRC(9633db8b) SHA1(fe1b117c2566288b88f000106c649c2fa5648ddc) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD( "768c04.j5",    0x000000, 0x80000, CRC(2960680e) SHA1(72e1f025496c907de8516e3b5f1781e73d5b2c6c) )
	ROM_LOAD( "768c05.j6",    0x080000, 0x80000, CRC(65a2f227) SHA1(43f368e533d6a164dc68d54130b81883e0d1bafe) )

	ROM_REGION( 0x0500, "proms", 0 )
	ROM_LOAD( "768c13.j21",   0x0000, 0x0100, CRC(f5de80cb) SHA1(e8cc3e14a5d23b25fb7bf790e64786c6aa2df8b7) )	/* 007121 #0 sprite lookup table */
	ROM_LOAD( "768c14.j22",   0x0100, 0x0100, CRC(b32071b7) SHA1(09a699a3f20c155eae1e63429f03ed91abc54784) )	/* 007121 #0 char lookup table */
	ROM_LOAD( "768c11.i4",    0x0200, 0x0100, CRC(f5de80cb) SHA1(e8cc3e14a5d23b25fb7bf790e64786c6aa2df8b7) )	/* 007121 #1 sprite lookup table (same) */
	ROM_LOAD( "768c10.i3",    0x0300, 0x0100, CRC(b32071b7) SHA1(09a699a3f20c155eae1e63429f03ed91abc54784) )	/* 007121 #1 char lookup table (same) */
	ROM_LOAD( "768b12.d20",   0x0400, 0x0100, CRC(362544b8) SHA1(744c8d2ccfa980fc9a7354b4d241c569b3c1fffe) )	/* priority encoder (not used) */

	ROM_REGION( 0x80000, "konami1", 0 )	/* 512k for the samples */
	ROM_LOAD( "768c07.e17",   0x00000, 0x80000, CRC(01f9889c) SHA1(01252d2ce7b14cfbe39ac8d7a5bd7417f1c2fc22) )
ROM_END

#if 0
ROM_START( hcastleo )
	ROM_REGION( 0x30000, "maincpu", 0 )
	ROM_LOAD( "768.k03",      0x08000, 0x08000, CRC(40ce4f38) SHA1(1ab6d62a75c818b2ccbbb714373d6c7418500eb7) )
	ROM_LOAD( "768.g06",      0x10000, 0x20000, CRC(cdade920) SHA1(e15b7458ded4e4c811a737575ec3f16e5eec4121) )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "768.e01",      0x00000, 0x08000, CRC(b9fff184) SHA1(c55f468c0da6afdaa2af65a111583c0c42868bd1) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD( "768c09.g21",   0x000000, 0x80000, CRC(e3be3fdd) SHA1(01a686af33a0a700066b1a5334d8552454ff186f) )
	ROM_LOAD( "768c08.g19",   0x080000, 0x80000, CRC(9633db8b) SHA1(fe1b117c2566288b88f000106c649c2fa5648ddc) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD( "768c04.j5",    0x000000, 0x80000, CRC(2960680e) SHA1(72e1f025496c907de8516e3b5f1781e73d5b2c6c) )
	ROM_LOAD( "768c05.j6",    0x080000, 0x80000, CRC(65a2f227) SHA1(43f368e533d6a164dc68d54130b81883e0d1bafe) )

	ROM_REGION( 0x0500, "proms", 0 )
	ROM_LOAD( "768c13.j21",   0x0000, 0x0100, CRC(f5de80cb) SHA1(e8cc3e14a5d23b25fb7bf790e64786c6aa2df8b7) )	/* 007121 #0 sprite lookup table */
	ROM_LOAD( "768c14.j22",   0x0100, 0x0100, CRC(b32071b7) SHA1(09a699a3f20c155eae1e63429f03ed91abc54784) )	/* 007121 #0 char lookup table */
	ROM_LOAD( "768c11.i4",    0x0200, 0x0100, CRC(f5de80cb) SHA1(e8cc3e14a5d23b25fb7bf790e64786c6aa2df8b7) )	/* 007121 #1 sprite lookup table (same) */
	ROM_LOAD( "768c10.i3",    0x0300, 0x0100, CRC(b32071b7) SHA1(09a699a3f20c155eae1e63429f03ed91abc54784) )	/* 007121 #1 char lookup table (same) */
	ROM_LOAD( "768b12.d20",   0x0400, 0x0100, CRC(362544b8) SHA1(744c8d2ccfa980fc9a7354b4d241c569b3c1fffe) )	/* priority encoder (not used) */

	ROM_REGION( 0x80000, "konami1", 0 )	/* 512k for the samples */
	ROM_LOAD( "768c07.e17",   0x00000, 0x80000, CRC(01f9889c) SHA1(01252d2ce7b14cfbe39ac8d7a5bd7417f1c2fc22) )
ROM_END
#endif
#if 0
ROM_START( hcastlej )
	ROM_REGION( 0x30000, "maincpu", 0 )
	ROM_LOAD( "768p03.k12",0x08000, 0x08000, CRC(d509e340) SHA1(3a8078bd89a80ab9529e4ee8658fcafb8dd65258) )
	ROM_LOAD( "768j06.k8", 0x10000, 0x20000, CRC(42283c3e) SHA1(565a2eb607e262484f48919536c045d515cff89f) )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "768.e01",   0x00000, 0x08000, CRC(b9fff184) SHA1(c55f468c0da6afdaa2af65a111583c0c42868bd1) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD( "768c09.g21",   0x000000, 0x80000, CRC(e3be3fdd) SHA1(01a686af33a0a700066b1a5334d8552454ff186f) )
	ROM_LOAD( "768c08.g19",   0x080000, 0x80000, CRC(9633db8b) SHA1(fe1b117c2566288b88f000106c649c2fa5648ddc) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD( "768c04.j5",    0x000000, 0x80000, CRC(2960680e) SHA1(72e1f025496c907de8516e3b5f1781e73d5b2c6c) )
	ROM_LOAD( "768c05.j6",    0x080000, 0x80000, CRC(65a2f227) SHA1(43f368e533d6a164dc68d54130b81883e0d1bafe) )

	ROM_REGION( 0x0500, "proms", 0 )
	ROM_LOAD( "768c13.j21",   0x0000, 0x0100, CRC(f5de80cb) SHA1(e8cc3e14a5d23b25fb7bf790e64786c6aa2df8b7) )	/* 007121 #0 sprite lookup table */
	ROM_LOAD( "768c14.j22",   0x0100, 0x0100, CRC(b32071b7) SHA1(09a699a3f20c155eae1e63429f03ed91abc54784) )	/* 007121 #0 char lookup table */
	ROM_LOAD( "768c11.i4",    0x0200, 0x0100, CRC(f5de80cb) SHA1(e8cc3e14a5d23b25fb7bf790e64786c6aa2df8b7) )	/* 007121 #1 sprite lookup table (same) */
	ROM_LOAD( "768c10.i3",    0x0300, 0x0100, CRC(b32071b7) SHA1(09a699a3f20c155eae1e63429f03ed91abc54784) )	/* 007121 #1 char lookup table (same) */
	ROM_LOAD( "768b12.d20",   0x0400, 0x0100, CRC(362544b8) SHA1(744c8d2ccfa980fc9a7354b4d241c569b3c1fffe) )	/* priority encoder (not used) */

	ROM_REGION( 0x80000, "konami1", 0 )	/* 512k for the samples */
	ROM_LOAD( "768c07.e17",   0x00000, 0x80000, CRC(01f9889c) SHA1(01252d2ce7b14cfbe39ac8d7a5bd7417f1c2fc22) )
ROM_END
#endif
#if 0
ROM_START( hcastljo )
	ROM_REGION( 0x30000, "maincpu", 0 )
	ROM_LOAD( "768n03.k12",0x08000, 0x08000, CRC(3e4dca2a) SHA1(cd70fdc42b970b89ae16ab6c81d1a5003fa53dbd) )
	ROM_LOAD( "768j06.k8", 0x10000, 0x20000, CRC(42283c3e) SHA1(565a2eb607e262484f48919536c045d515cff89f) )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "768.e01",   0x00000, 0x08000, CRC(b9fff184) SHA1(c55f468c0da6afdaa2af65a111583c0c42868bd1) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD( "768c09.g21",   0x000000, 0x80000, CRC(e3be3fdd) SHA1(01a686af33a0a700066b1a5334d8552454ff186f) )
	ROM_LOAD( "768c08.g19",   0x080000, 0x80000, CRC(9633db8b) SHA1(fe1b117c2566288b88f000106c649c2fa5648ddc) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD( "768c04.j5",    0x000000, 0x80000, CRC(2960680e) SHA1(72e1f025496c907de8516e3b5f1781e73d5b2c6c) )
	ROM_LOAD( "768c05.j6",    0x080000, 0x80000, CRC(65a2f227) SHA1(43f368e533d6a164dc68d54130b81883e0d1bafe) )

	ROM_REGION( 0x0500, "proms", 0 )
	ROM_LOAD( "768c13.j21",   0x0000, 0x0100, CRC(f5de80cb) SHA1(e8cc3e14a5d23b25fb7bf790e64786c6aa2df8b7) )	/* 007121 #0 sprite lookup table */
	ROM_LOAD( "768c14.j22",   0x0100, 0x0100, CRC(b32071b7) SHA1(09a699a3f20c155eae1e63429f03ed91abc54784) )	/* 007121 #0 char lookup table */
	ROM_LOAD( "768c11.i4",    0x0200, 0x0100, CRC(f5de80cb) SHA1(e8cc3e14a5d23b25fb7bf790e64786c6aa2df8b7) )	/* 007121 #1 sprite lookup table (same) */
	ROM_LOAD( "768c10.i3",    0x0300, 0x0100, CRC(b32071b7) SHA1(09a699a3f20c155eae1e63429f03ed91abc54784) )	/* 007121 #1 char lookup table (same) */
	ROM_LOAD( "768b12.d20",   0x0400, 0x0100, CRC(362544b8) SHA1(744c8d2ccfa980fc9a7354b4d241c569b3c1fffe) )	/* priority encoder (not used) */

	ROM_REGION( 0x80000, "konami1", 0 )	/* 512k for the samples */
	ROM_LOAD( "768c07.e17",   0x00000, 0x80000, CRC(01f9889c) SHA1(01252d2ce7b14cfbe39ac8d7a5bd7417f1c2fc22) )
ROM_END
#endif

GAME( 1988, hcastle,  0,       hcastle, hcastle, 0, ROT0, "Konami", "Haunted Castle (version M)", 0 )
//GAME( 1988, hcastleo, hcastle, hcastle, hcastle, 0, ROT0, "Konami", "Haunted Castle (version K)", 0 )
//GAME( 1988, hcastlej, hcastle, hcastle, hcastle, 0, ROT0, "Konami", "Akuma-Jou Dracula (Japan version P)", 0 )
//GAME( 1988, hcastljo, hcastle, hcastle, hcastle, 0, ROT0, "Konami", "Akuma-Jou Dracula (Japan version N)", 0 )

/*
Lady Frog (c) 1990 Mondial Games
(there's  "(c) Alfa Tecnology" in the ROM)
driver by Tomasz Slanina

'N.Y. Captor' (TAITO) hardware , without sub cpu.

Sound rom is 'borrowed' from NYC.
1.115 = a80_16.i26 + a80_17.i25

PCB Layout
|-------------------------------------------------|
|18MHz                          1       M5232     |
|                                          LM3900 |
|                               6116    Z80-2     |
|                     6116                   8MHz |
|                         8MHz        N5C090-60   |
|           2148                      AY-3-8910   |
|           2148                                  |
|                                         LM3900  |
|                                                 |
|                             Z80-1               |
|                 2148         2                  |
|                 2148        6264                |
|6      3         2148                           J|
|7      4                                        A|
|8      5                                        M|
|                                                M|
|           2148                   DSWB  DSWA    A|
|           2148                                  |
|           2148                                  |
|-------------------------------------------------|

Notes:
      Z80-1 clock: 4.000MHz
      Z80-2 clock: 4.000MHz
      AY-3-8910 clock: 2.000MHz
      OKI M5232 clock: 2.000MHz
      VSync: 60Hz
      HSync: 15.68kHz

      N5C090-60: iNTEL simple PLD (PLCC44), 100% compatible with Altera EP900

*/

/* set to 1 for real screen size - two more tile columns on right side = black(title)/garbage(game) */
#define ladyfrog_scr_size 0

#include "driver.h"
#include "cpu/z80/z80.h"
#include "deprecat.h"
#include "sound/ay8910.h"
#include "sound/msm5232.h"

VIDEO_START( ladyfrog );
VIDEO_UPDATE( ladyfrog );

extern UINT8 *ladyfrog_scrlram;
static int sound_nmi_enable=0,pending_nmi=0;
static int snd_flag;
static UINT8 snd_data;

WRITE8_HANDLER( ladyfrog_videoram_w );
WRITE8_HANDLER( ladyfrog_spriteram_w );
WRITE8_HANDLER( ladyfrog_palette_w );
WRITE8_HANDLER( ladyfrog_gfxctrl_w );
WRITE8_HANDLER( ladyfrog_gfxctrl2_w );
WRITE8_HANDLER( ladyfrog_scrlram_w );

READ8_HANDLER( ladyfrog_spriteram_r );
READ8_HANDLER( ladyfrog_palette_r );
READ8_HANDLER( ladyfrog_scrlram_r );
READ8_HANDLER( ladyfrog_videoram_r );

static READ8_HANDLER( from_snd_r )
{
	snd_flag=0;
	return snd_data;
}

static WRITE8_HANDLER( to_main_w )
{
	snd_data = data;
	snd_flag = 2;

}

static WRITE8_HANDLER( sound_cpu_reset_w )
{
	cputag_set_input_line(space->machine, "audiocpu", INPUT_LINE_RESET, (data & 1 ) ? ASSERT_LINE : CLEAR_LINE);
}

static TIMER_CALLBACK( nmi_callback )
{
	if (sound_nmi_enable) cputag_set_input_line(machine, "audiocpu", INPUT_LINE_NMI, PULSE_LINE);
	else pending_nmi = 1;
}

static WRITE8_HANDLER( sound_command_w )
{
	soundlatch_w(space, 0, data);
	timer_call_after_resynch(space->machine, NULL, data, nmi_callback);
}

static WRITE8_HANDLER( nmi_disable_w )
{
	sound_nmi_enable = 0;
}

static WRITE8_HANDLER( nmi_enable_w )
{
	sound_nmi_enable = 1;
	if (pending_nmi)
	{
		cputag_set_input_line(space->machine, "audiocpu", INPUT_LINE_NMI, PULSE_LINE);
		pending_nmi = 0;
	}
}

static WRITE8_DEVICE_HANDLER(unk_w)
{

}

static const ay8910_interface ay8910_config =
{
	AY8910_LEGACY_OUTPUT,
	AY8910_DEFAULT_LOADS,
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_HANDLER(unk_w),
	DEVCB_HANDLER(unk_w)
};

static const msm5232_interface msm5232_config =
{
	{ 0.65e-6, 0.65e-6, 0.65e-6, 0.65e-6, 0.65e-6, 0.65e-6, 0.65e-6, 0.65e-6 }
};

static READ8_HANDLER( snd_flag_r )
{
	return snd_flag | 0xfd;
}

static ADDRESS_MAP_START( ladyfrog_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0xbfff) AM_ROM
	AM_RANGE(0xc000, 0xc07f) AM_RAM
	AM_RANGE(0xc080, 0xc87f) AM_READ(ladyfrog_videoram_r) AM_WRITE(ladyfrog_videoram_w) AM_BASE(&videoram) AM_SIZE(&videoram_size)
	AM_RANGE(0xd000, 0xd000) AM_WRITE(ladyfrog_gfxctrl2_w)
	AM_RANGE(0xd400, 0xd400) AM_READ(from_snd_r) AM_WRITE(sound_command_w)
	AM_RANGE(0xd401, 0xd401) AM_READ(snd_flag_r)
	AM_RANGE(0xd403, 0xd403) AM_WRITE(sound_cpu_reset_w)
	AM_RANGE(0xd800, 0xd800) AM_READ_PORT("DSW1")
	AM_RANGE(0xd801, 0xd801) AM_READ_PORT("DSW2")
	AM_RANGE(0xd804, 0xd804) AM_READ_PORT("INPUTS")
	AM_RANGE(0xd806, 0xd806) AM_READ_PORT("SYSTEM")
	AM_RANGE(0xdc00, 0xdc9f) AM_READ(ladyfrog_spriteram_r) AM_WRITE(ladyfrog_spriteram_w)
	AM_RANGE(0xdca0, 0xdcbf) AM_READ(ladyfrog_scrlram_r)  AM_WRITE(ladyfrog_scrlram_w) AM_BASE(&ladyfrog_scrlram)
	AM_RANGE(0xdcc0, 0xdcff) AM_RAM
	AM_RANGE(0xdd00, 0xdeff) AM_READ(ladyfrog_palette_r)  AM_WRITE(ladyfrog_palette_w)
	AM_RANGE(0xd0d0, 0xd0d0) AM_READNOP /* code jumps to ASCII text "Alfa tecnology"  @ $b7 */
	AM_RANGE(0xdf03, 0xdf03) AM_WRITE(ladyfrog_gfxctrl_w)
	AM_RANGE(0xe000, 0xffff) AM_RAM
ADDRESS_MAP_END

static ADDRESS_MAP_START( ladyfrog_sound_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0xbfff) AM_ROM
	AM_RANGE(0xc000, 0xc7ff) AM_RAM
	AM_RANGE(0xc800, 0xc801) AM_WRITENOP
	AM_RANGE(0xc802, 0xc803) AM_DEVWRITE("ay", ay8910_address_data_w)
	AM_RANGE(0xc900, 0xc90d) AM_DEVWRITE("msm", msm5232_w)
	AM_RANGE(0xca00, 0xca00) AM_WRITENOP
	AM_RANGE(0xcb00, 0xcb00) AM_WRITENOP
	AM_RANGE(0xcc00, 0xcc00) AM_WRITENOP
	AM_RANGE(0xd000, 0xd000) AM_READ(soundlatch_r) AM_WRITE(to_main_w)
	AM_RANGE(0xd200, 0xd200) AM_READNOP AM_WRITE(nmi_enable_w)
	AM_RANGE(0xd400, 0xd400) AM_WRITE(nmi_disable_w)
	AM_RANGE(0xd600, 0xd600) AM_WRITENOP
	AM_RANGE(0xe000, 0xefff) AM_NOP
ADDRESS_MAP_END


static INPUT_PORTS_START( ladyfrog )
	PORT_START("DSW1")
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPSETTING(    0x01, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x03, "1" )
	PORT_DIPNAME( 0x04, 0x00, "Clear 'doors' after life lost" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Allow_Continue ) )
	PORT_DIPSETTING(    0x20, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_3C ) )

	PORT_START("DSW2")
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x03, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START("INPUTS")
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_4WAY
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_4WAY
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_4WAY
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_4WAY

	PORT_START("SYSTEM")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 )
INPUT_PORTS_END


static const gfx_layout charlayout =
{
	8,8,
	RGN_FRAC(1,2),
	4,
	{ RGN_FRAC(1,2)+0, RGN_FRAC(1,2)+4, 0, 4 },
	{ 3, 2, 1, 0, 8+3, 8+2, 8+1, 8+0 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8
};

static const gfx_layout spritelayout =
{
	16,16,
	RGN_FRAC(1,2),
	4,
	{ RGN_FRAC(1,2)+0, RGN_FRAC(1,2)+4, 0, 4 },
	{ 3, 2, 1, 0, 8+3, 8+2, 8+1, 8+0,
			16*8+3, 16*8+2, 16*8+1, 16*8+0, 16*8+8+3, 16*8+8+2, 16*8+8+1, 16*8+8+0 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			16*16, 17*16, 18*16, 19*16, 20*16, 21*16, 22*16, 23*16 },
	64*8
};



static GFXDECODE_START( ladyfrog )
	GFXDECODE_ENTRY( "gfx1", 0, charlayout,     0, 16 )
	GFXDECODE_ENTRY( "gfx1", 0, spritelayout, 256, 16 )
GFXDECODE_END

static MACHINE_DRIVER_START( ladyfrog )
	MDRV_CPU_ADD("maincpu", Z80,8000000/2)
	MDRV_CPU_PROGRAM_MAP(ladyfrog_map)
	MDRV_CPU_VBLANK_INT("screen", irq0_line_hold)

	MDRV_CPU_ADD("audiocpu", Z80,8000000/2)
	MDRV_CPU_PROGRAM_MAP(ladyfrog_sound_map)
	MDRV_CPU_VBLANK_INT_HACK(irq0_line_hold,2)

	/* video hardware */
	MDRV_QUANTUM_TIME(HZ(6000))


	MDRV_SCREEN_ADD("screen", RASTER)
	MDRV_SCREEN_REFRESH_RATE(60)
	MDRV_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(0))
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_SIZE(32*8, 32*8)
#if ladyfrog_scr_size
	MDRV_SCREEN_VISIBLE_AREA(0*8, 32*8-1, 2*8, 29*8-1)
#else
	MDRV_SCREEN_VISIBLE_AREA(0*8, 32*8-1, 2*8, 27*8-1)
#endif

	MDRV_GFXDECODE(ladyfrog)
	MDRV_PALETTE_LENGTH(512)

	MDRV_VIDEO_START(ladyfrog)
	MDRV_VIDEO_UPDATE(ladyfrog)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")

	MDRV_SOUND_ADD("ay", AY8910, 8000000/4)
	MDRV_SOUND_CONFIG(ay8910_config)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.15)

	MDRV_SOUND_ADD("msm", MSM5232, 2000000)
	MDRV_SOUND_CONFIG(msm5232_config)
	MDRV_SOUND_ROUTE(0, "mono", 1.0)	// pin 28  2'-1
	MDRV_SOUND_ROUTE(1, "mono", 1.0)	// pin 29  4'-1
	MDRV_SOUND_ROUTE(2, "mono", 1.0)	// pin 30  8'-1
	MDRV_SOUND_ROUTE(3, "mono", 1.0)	// pin 31 16'-1
	MDRV_SOUND_ROUTE(4, "mono", 1.0)	// pin 36  2'-2
	MDRV_SOUND_ROUTE(5, "mono", 1.0)	// pin 35  4'-2
	MDRV_SOUND_ROUTE(6, "mono", 1.0)	// pin 34  8'-2
	MDRV_SOUND_ROUTE(7, "mono", 1.0)	// pin 33 16'-2
	// pin 1 SOLO  8'       not mapped
	// pin 2 SOLO 16'       not mapped
	// pin 22 Noise Output  not mapped
MACHINE_DRIVER_END


ROM_START( ladyfrog )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "2.107",   0x0000, 0x10000, CRC(fa4466e6) SHA1(08e5cc8e1d3c845bc9c253267f2683671bffa9f2) )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "1.115",   0x0000, 0x8000, CRC(b0932498) SHA1(13d90698f2682e64ff3597c9267ca9d33a6d62ba) ) /* NY Captor*/

	ROM_REGION( 0x60000, "gfx1", ROMREGION_INVERT )
	ROM_LOAD( "3.32",   0x30000, 0x10000, CRC(8a27fc0a) SHA1(36e0365776e61ef830451e6351eca6b6c742086f) )
	ROM_LOAD( "4.33",   0x40000, 0x10000, CRC(e1a137d3) SHA1(add8140a9366a0d343b611ced10c804d3fb04c03) )
	ROM_LOAD( "5.34",   0x50000, 0x10000, CRC(7816925f) SHA1(037a69243b35e1739e5d7288e279d0d4289c61ed) )
	ROM_LOAD( "6.8",    0x00000, 0x10000, CRC(61b3baaa) SHA1(d65a235dbbb96c11e8307aa457d1c06f20eb8d5a) )
	ROM_LOAD( "7.9",    0x10000, 0x10000, CRC(88aaff58) SHA1(dfb143ef452dec530adf8b35a50a82d08f47d107) )
	ROM_LOAD( "8.10",   0x20000, 0x10000, CRC(8c73baa1) SHA1(50fb408be181ef3c125dee23b04daeb010c9f276) )
ROM_END

GAME( 1990, ladyfrog, 0,       ladyfrog,  ladyfrog, 0, ORIENTATION_SWAP_XY, "Mondial Games", "Lady Frog", 0)

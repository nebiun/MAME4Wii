/*****************************************************************************

Mahjong Sisters (c) 1986 Toa Plan

    Driver by Uki

*****************************************************************************/
#if 0

#include "driver.h"
#include "cpu/z80/z80.h"
#include "deprecat.h"
#include "sound/dac.h"
#include "sound/ay8910.h"

#define MCLK 12000000

extern int mjsister_flip_screen;
extern int mjsister_video_enable;
extern int mjsister_screen_redraw;

extern int mjsister_vrambank;
extern int mjsister_colorbank;

VIDEO_START( mjsister );
VIDEO_UPDATE( mjsister );
WRITE8_HANDLER( mjsister_videoram_w );

static int mjsister_input_sel1;
static int mjsister_input_sel2;

static int rombank0,rombank1;

static UINT32 dac_adr,dac_bank,dac_adr_s,dac_adr_e,dac_busy;

/****************************************************************************/

static TIMER_CALLBACK( dac_callback )
{
	UINT8 *DACROM = memory_region(machine, "samples");

	dac_data_w(devtag_get_device(machine, "dac"),DACROM[(dac_bank * 0x10000 + dac_adr++) & 0x1ffff]);

	if (((dac_adr & 0xff00 ) >> 8) !=  dac_adr_e )
		timer_set(machine, attotime_mul(ATTOTIME_IN_HZ(MCLK), 1024), NULL, 0, dac_callback);
	else
		dac_busy = 0;
}

static WRITE8_HANDLER( mjsister_dac_adr_s_w )
{
	dac_adr_s = data;
}

static WRITE8_HANDLER( mjsister_dac_adr_e_w )
{
	dac_adr_e = data;
	dac_adr = dac_adr_s << 8;

	if (dac_busy == 0)
		timer_call_after_resynch(space->machine, NULL, 0,dac_callback);

	dac_busy = 1;
}

static MACHINE_RESET( mjsister )
{
	dac_busy = 0;
}

static WRITE8_HANDLER( mjsister_banksel1_w )
{
	UINT8 *BANKROM = memory_region(space->machine, "maincpu");
	int tmp = mjsister_colorbank;

	switch (data)
	{
		case 0x0: rombank0 = 0 ; break;
		case 0x1: rombank0 = 1 ; break;

		case 0x2: mjsister_flip_screen = 0 ; break;
		case 0x3: mjsister_flip_screen = 1 ; break;

		case 0x4: mjsister_colorbank &=0xfe; break;
		case 0x5: mjsister_colorbank |=0x01; break;
		case 0x6: mjsister_colorbank &=0xfd; break;
		case 0x7: mjsister_colorbank |=0x02; break;
		case 0x8: mjsister_colorbank &=0xfb; break;
		case 0x9: mjsister_colorbank |=0x04; break;

		case 0xa: mjsister_video_enable = 0 ; break;
		case 0xb: mjsister_video_enable = 1 ; break;

		case 0xe: mjsister_vrambank = 0 ; break;
		case 0xf: mjsister_vrambank = 1 ; break;

		default:
			logerror("%04x p30_w:%02x\n",cpu_get_pc(space->cpu),data);
	}

	if (tmp != mjsister_colorbank)
		mjsister_screen_redraw = 1;

	memory_set_bankptr(space->machine, 1,&BANKROM[rombank0*0x10000+rombank1*0x8000]+0x10000);
}

static WRITE8_HANDLER( mjsister_banksel2_w )
{
	UINT8 *BANKROM = memory_region(space->machine, "maincpu");

	switch (data)
	{
		case 0xa: dac_bank = 0; break;
		case 0xb: dac_bank = 1; break;

		case 0xc: rombank1 = 0; break;
		case 0xd: rombank1 = 1; break;

		default:
			logerror("%04x p31_w:%02x\n",cpu_get_pc(space->cpu),data);
	}

	memory_set_bankptr(space->machine, 1,&BANKROM[rombank0*0x10000+rombank1*0x8000]+0x10000);
}

static WRITE8_HANDLER( mjsister_input_sel1_w )
{
	mjsister_input_sel1 = data;
}

static WRITE8_HANDLER( mjsister_input_sel2_w )
{
	mjsister_input_sel2 = data;
}

static READ8_HANDLER( mjsister_keys_r )
{
	int p,i,ret = 0;
	static const char *const keynames[] = { "KEY0", "KEY1", "KEY2", "KEY3", "KEY4", "KEY5" };

	p = mjsister_input_sel1 & 0x3f;
//  p |= ((mjsister_input_sel2 & 8) << 4) | ((mjsister_input_sel2 & 0x20) << 1);

	for (i=0; i<6; i++)
	{
		if (p & (1 << i))
			ret |= input_port_read(space->machine, keynames[i]);
	}

	return ret;
}

/****************************************************************************/

static ADDRESS_MAP_START( mjsister_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x77ff) AM_ROM
	AM_RANGE(0x7800, 0x7fff) AM_RAM
	AM_RANGE(0x8000, 0xffff) AM_ROMBANK(1) AM_WRITE(mjsister_videoram_w)
ADDRESS_MAP_END

static ADDRESS_MAP_START( mjsister_io_map, ADDRESS_SPACE_IO, 8 )
	ADDRESS_MAP_GLOBAL_MASK(0xff)
	AM_RANGE(0x00, 0x01) AM_WRITENOP /* HD46505? */
	AM_RANGE(0x10, 0x10) AM_DEVWRITE("ay", ay8910_address_w)
	AM_RANGE(0x11, 0x11) AM_DEVREAD("ay", ay8910_r)
	AM_RANGE(0x12, 0x12) AM_DEVWRITE("ay", ay8910_data_w)
	AM_RANGE(0x20, 0x20) AM_READ(mjsister_keys_r)
	AM_RANGE(0x21, 0x21) AM_READ_PORT("IN0")
	AM_RANGE(0x30, 0x30) AM_WRITE(mjsister_banksel1_w)
	AM_RANGE(0x31, 0x31) AM_WRITE(mjsister_banksel2_w)
	AM_RANGE(0x32, 0x32) AM_WRITE(mjsister_input_sel1_w)
	AM_RANGE(0x33, 0x33) AM_WRITE(mjsister_input_sel2_w)
	AM_RANGE(0x34, 0x34) AM_WRITE(mjsister_dac_adr_s_w)
	AM_RANGE(0x35, 0x35) AM_WRITE(mjsister_dac_adr_e_w)
ADDRESS_MAP_END


/****************************************************************************/

static INPUT_PORTS_START( mjsister )

	PORT_START("DSW1")	/* DSW1 (0) */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(	0x03, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(	0x01, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(	0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(	0x04, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x08, 0x08, "Unknown 1-4" )
	PORT_DIPSETTING(	0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Unknown 1-5" )
	PORT_DIPSETTING(	0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Unknown 1-6" )
	PORT_DIPSETTING(	0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* service mode */
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(	0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )

	PORT_START("DSW2")	/* DSW2 (1) */
	PORT_DIPNAME( 0x01, 0x01, "Unknown 2-1" )
	PORT_DIPSETTING(	0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Unknown 2-2" )
	PORT_DIPSETTING(	0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Unknown 2-3" )
	PORT_DIPSETTING(	0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Unknown 2-4" )
	PORT_DIPSETTING(	0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Unknown 2-5" )
	PORT_DIPSETTING(	0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Unknown 2-6" )
	PORT_DIPSETTING(	0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Unknown 2-7" )
	PORT_DIPSETTING(	0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Unknown 2-8" )
	PORT_DIPSETTING(	0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )

	PORT_START("IN0")	/* (2) */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* memory reset 1 */
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* analyzer */
	PORT_SERVICE( 0x08, IP_ACTIVE_HIGH )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* memory reset 2 */
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* pay out */
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* hopper */

	PORT_START("KEY0")	/* (3) PORT 1-0 */
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_MAHJONG_A )
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_MAHJONG_B )
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_MAHJONG_C )
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_MAHJONG_D )
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_MAHJONG_LAST_CHANCE )
	PORT_BIT( 0xe0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("KEY1")	/* (4) PORT 1-1 */
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_MAHJONG_E )
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_MAHJONG_F )
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_MAHJONG_G )
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_MAHJONG_H )
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_MAHJONG_SCORE )
	PORT_BIT( 0xe0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("KEY2")	/* (5) PORT 1-2 */
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_MAHJONG_I )
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_MAHJONG_J )
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_MAHJONG_K )
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_MAHJONG_L )
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_MAHJONG_DOUBLE_UP )
	PORT_BIT( 0xe0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("KEY3")	/* (6) PORT 1-3 */
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_MAHJONG_M )
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_MAHJONG_N )
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_MAHJONG_CHI )
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_MAHJONG_PON )
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_MAHJONG_FLIP_FLOP )
	PORT_BIT( 0xe0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("KEY4")	/* (7) PORT 1-4 */
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_MAHJONG_KAN )
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_MAHJONG_REACH )
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_MAHJONG_RON )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_MAHJONG_BIG )
	PORT_BIT( 0xe0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("KEY5")	/* (8) PORT 1-5 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_MAHJONG_BET )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_MAHJONG_SMALL )
	PORT_BIT( 0xe0, IP_ACTIVE_HIGH, IPT_UNUSED )

INPUT_PORTS_END

/****************************************************************************/

static const ay8910_interface ay8910_config =
{
	AY8910_LEGACY_OUTPUT,
	AY8910_DEFAULT_LOADS,
	DEVCB_INPUT_PORT("DSW1"),
	DEVCB_INPUT_PORT("DSW2"),
	DEVCB_NULL,
	DEVCB_NULL
};

static MACHINE_DRIVER_START( mjsister )

	/* basic machine hardware */
	MDRV_CPU_ADD("maincpu", Z80, MCLK/2) /* 6.000 MHz */
	MDRV_CPU_PROGRAM_MAP(mjsister_map)
	MDRV_CPU_IO_MAP(mjsister_io_map)
	MDRV_CPU_VBLANK_INT_HACK(irq0_line_hold,2)

	MDRV_MACHINE_RESET(mjsister)

	/* video hardware */
	MDRV_SCREEN_ADD("screen", RASTER)
	MDRV_SCREEN_REFRESH_RATE(60)
	MDRV_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(2500) /* not accurate */)
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_SIZE(256+4, 256)
	MDRV_SCREEN_VISIBLE_AREA(0, 255+4, 8, 247)

	MDRV_PALETTE_INIT(RRRR_GGGG_BBBB)
	MDRV_PALETTE_LENGTH(256)

	MDRV_VIDEO_START(mjsister)
	MDRV_VIDEO_UPDATE(mjsister)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")

	MDRV_SOUND_ADD("ay", AY8910, MCLK/8)
	MDRV_SOUND_CONFIG(ay8910_config)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.15)

	MDRV_SOUND_ADD("dac", DAC, 0)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 1.0)
MACHINE_DRIVER_END

/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( mjsister )
	ROM_REGION( 0x30000, "maincpu", 0 )   /* CPU */
	ROM_LOAD( "ms00.bin",  0x00000, 0x08000, CRC(9468c33b) SHA1(63aecdcaa8493d58549dfd1d217743210cf953bc) )
	ROM_LOAD( "ms01t.bin", 0x10000, 0x10000, CRC(a7b6e530) SHA1(fda9bea214968a8814d2c43226b3b32316581050) ) /* banked */
	ROM_LOAD( "ms02t.bin", 0x20000, 0x10000, CRC(7752b5ba) SHA1(84dcf27a62eb290ba07c85af155897ec72f320a8) ) /* banked */

	ROM_REGION( 0x20000, "samples", 0 ) /* samples */
	ROM_LOAD( "ms03.bin", 0x00000,  0x10000, CRC(10a68e5e) SHA1(a0e2fa34c1c4f34642f65fbf17e9da9c2554a0c6) )
	ROM_LOAD( "ms04.bin", 0x10000,  0x10000, CRC(641b09c1) SHA1(15cde906175bcb5190d36cc91cbef003ef91e425) )

	ROM_REGION( 0x00400, "proms", 0 ) /* color PROMs */
	ROM_LOAD( "ms05.bpr", 0x0000,  0x0100, CRC(dd231a5f) SHA1(be008593ac8ba8f5a1dd5b188dc7dc4c03016805) ) // R
	ROM_LOAD( "ms06.bpr", 0x0100,  0x0100, CRC(df8e8852) SHA1(842a891440aef55a560d24c96f249618b9f4b97f) ) // G
	ROM_LOAD( "ms07.bpr", 0x0200,  0x0100, CRC(6cb3a735) SHA1(468ae3d40552dc2ec24f5f2988850093d73948a6) ) // B
	ROM_LOAD( "ms08.bpr", 0x0300,  0x0100, CRC(da2b3b38) SHA1(4de99c17b227653bc1b904f1309f447f5a0ab516) ) // ?
ROM_END
#endif


//GAME( 1986, mjsister, 0, mjsister, mjsister, 0, ROT0, "Toaplan", "Mahjong Sisters (Japan)", 0 )

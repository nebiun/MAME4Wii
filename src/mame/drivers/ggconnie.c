/****************************************************************************

    Go! Go! Connie chan Jaka Jaka Janken

    Driver by Mariusz Wojcieszek

    EC9601

    Hudson Chip
    CPU  :Hu6280
    Video:Hu6202,Hu6260,Hu6270

    OSC  :21.47727MHz
    Other:XILINX XC7336-15,OKI M6295


****************************************************************************/
#if 0

#include "driver.h"
#include "deprecat.h"
#include "machine/pcecommn.h"
#include "video/vdc.h"
#include "cpu/h6280/h6280.h"
#include "sound/c6280.h"

static INPUT_PORTS_START(ggconnie)
    PORT_START("IN0")
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON6 ) PORT_NAME( "Medal" )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN1 ) /* 100 Yen */
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 ) /* 10 Yen */
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 ) /* run */
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON5 ) PORT_NAME( DEF_STR(Test) )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON4 ) PORT_NAME("Hopper")
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("IN1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("SWA")
	PORT_DIPNAME(0x03, 0x03, "Coin Set")
	PORT_DIPSETTING(0x03, DEF_STR(1C_1C))
	PORT_DIPNAME(0x1c, 0x1c, "100 Yen -> Coin" )
	PORT_DIPSETTING(0x00, "11 Coin")
	PORT_DIPSETTING(0x04, "12 Coin")
	PORT_DIPSETTING(0x08, "0 Coin")
	PORT_DIPSETTING(0x0c, "5 Coin")
	PORT_DIPSETTING(0x10, "6 Coin")
	PORT_DIPSETTING(0x14, "7 Coin")
	PORT_DIPSETTING(0x18, "8 Coin")
	PORT_DIPSETTING(0x1c, "10 Coin")
	PORT_DIPUNUSED(0x20, IP_ACTIVE_LOW )
	PORT_DIPUNUSED(0x40, IP_ACTIVE_LOW )
	PORT_DIPUNUSED(0x80, IP_ACTIVE_LOW )

	PORT_START("SWB")
	PORT_DIPNAME(0x07, 0x07, "Payout")
	PORT_DIPSETTING(0x00, "85%")
	PORT_DIPSETTING(0x01, "90%")
	PORT_DIPSETTING(0x02, "55%")
	PORT_DIPSETTING(0x03, "60%")
	PORT_DIPSETTING(0x04, "65%")
	PORT_DIPSETTING(0x05, "70%")
	PORT_DIPSETTING(0x06, "75%")
	PORT_DIPSETTING(0x07, "80%")
	PORT_DIPNAME(0x18, 0x18, DEF_STR(Difficulty))
	PORT_DIPSETTING(0x00, DEF_STR(Easy))
	PORT_DIPSETTING(0x08, DEF_STR(Very_Hard))
	PORT_DIPSETTING(0x10, DEF_STR(Hard))
	PORT_DIPSETTING(0x18, DEF_STR(Normal))
	PORT_DIPNAME(0x20, 0x20, "Payout Info")
	PORT_DIPSETTING(0x00, DEF_STR(Off))
	PORT_DIPSETTING(0x20, DEF_STR(On))
	PORT_DIPNAME(0xc0, 0xc0, "Rate")
	PORT_DIPSETTING(0x00, "Few" )
	PORT_DIPSETTING(0x40, "Most" )
	PORT_DIPSETTING(0x80, "More" )
	PORT_DIPSETTING(0xc0, DEF_STR(Normal))

	PORT_START("SWC")
	PORT_DIPNAME(0x03, 0x03, "Demo Sound" )
	PORT_DIPSETTING(0x00, DEF_STR(Off) )
	PORT_DIPSETTING(0x01, "3/1" )
	PORT_DIPSETTING(0x02, "2/1" )
	PORT_DIPSETTING(0x03, "1/1" )
	PORT_DIPNAME(0x0c, 0x0c, "Start Time" )
	PORT_DIPSETTING(0x00, "4 sec" )
	PORT_DIPSETTING(0x04, "8 sec" )
	PORT_DIPSETTING(0x08, "6 sec" )
	PORT_DIPSETTING(0x0c, "5 sec" )
	PORT_DIPUNUSED(0x10, IP_ACTIVE_LOW )
	PORT_DIPUNUSED(0x20, IP_ACTIVE_LOW )
	PORT_DIPNAME(0x40, 0x00, "RAM Clear" )
	PORT_DIPSETTING(0x40, DEF_STR(Off) )
	PORT_DIPSETTING(0x00, DEF_STR(On) )
	PORT_SERVICE(0x80, IP_ACTIVE_LOW)

INPUT_PORTS_END

static WRITE8_HANDLER(lamp_w)
{
	output_set_value("lamp", !BIT(data,0));
}

static WRITE8_HANDLER(output_w)
{
	// written in "Output Test" in test mode
}

static ADDRESS_MAP_START( sgx_mem , ADDRESS_SPACE_PROGRAM, 8)
	AM_RANGE( 0x000000, 0x0fffff) AM_ROM
	AM_RANGE( 0x110000, 0x1edfff) AM_NOP
	AM_RANGE( 0x1ee800, 0x1effff) AM_NOP
	AM_RANGE( 0x1f0000, 0x1f5fff) AM_RAM AM_BASE(&pce_user_ram)
	AM_RANGE( 0x1f7000, 0x1f7000) AM_READ_PORT("SWA")
	AM_RANGE( 0x1f7100, 0x1f7100) AM_READ_PORT("SWB")
	AM_RANGE( 0x1f7200, 0x1f7200) AM_READ_PORT("SWC")
	AM_RANGE( 0x1f7700, 0x1f7700) AM_READ_PORT("IN1")
	AM_RANGE( 0x1f7800, 0x1f7800) AM_WRITE(output_w)
	AM_RANGE( 0x1fe000, 0x1fe007) AM_READWRITE(vdc_0_r, vdc_0_w) AM_MIRROR(0x03e0)
	AM_RANGE( 0x1fe008, 0x1fe00f) AM_READWRITE(vpc_r, vpc_w) AM_MIRROR(0x03e0)
	AM_RANGE( 0x1fe010, 0x1fe017) AM_READWRITE(vdc_1_r, vdc_1_w) AM_MIRROR(0x03e0)
	AM_RANGE( 0x1fe400, 0x1fe7ff) AM_READWRITE(vce_r, vce_w)
	AM_RANGE( 0x1fe800, 0x1febff) AM_DEVREADWRITE("c6280", c6280_r, c6280_w)
	AM_RANGE( 0x1fec00, 0x1fefff) AM_READWRITE(h6280_timer_r, h6280_timer_w)
	AM_RANGE( 0x1ff000, 0x1ff000) AM_READ_PORT("IN0") AM_WRITE(lamp_w)
	AM_RANGE( 0x1ff400, 0x1ff7ff) AM_READWRITE(h6280_irq_status_r, h6280_irq_status_w )
ADDRESS_MAP_END

static ADDRESS_MAP_START( sgx_io , ADDRESS_SPACE_IO, 8)
	AM_RANGE( 0x00, 0x03) AM_READWRITE( sgx_vdc_r, sgx_vdc_w )
ADDRESS_MAP_END

static const c6280_interface c6280_config =
{
	"maincpu"
};

static MACHINE_DRIVER_START( ggconnie )
	/* basic machine hardware */
	MDRV_CPU_ADD("maincpu", H6280, PCE_MAIN_CLOCK/3)
	MDRV_CPU_PROGRAM_MAP(sgx_mem)
	MDRV_CPU_IO_MAP(sgx_io)
	MDRV_CPU_VBLANK_INT_HACK(sgx_interrupt, VDC_LPF)

	MDRV_QUANTUM_TIME(HZ(60))

    /* video hardware */

	MDRV_SCREEN_ADD("screen", RASTER)
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_RAW_PARAMS(PCE_MAIN_CLOCK/2, VDC_WPF, 70, 70 + 512 + 32, VDC_LPF, 14, 14+242)

	/* MDRV_GFXDECODE( pce_gfxdecodeinfo ) */
	MDRV_PALETTE_LENGTH(1024)
	MDRV_PALETTE_INIT( vce )

	MDRV_VIDEO_START( pce )
	MDRV_VIDEO_UPDATE( pce )

	MDRV_SPEAKER_STANDARD_STEREO("lspeaker","rspeaker")
	MDRV_SOUND_ADD("c6280", C6280, PCE_MAIN_CLOCK/6)
	MDRV_SOUND_CONFIG(c6280_config)
	MDRV_SOUND_ROUTE(0, "lspeaker", 1.00)
	MDRV_SOUND_ROUTE(1, "rspeaker", 1.00)

MACHINE_DRIVER_END

ROM_START(ggconnie)
	ROM_REGION( 0x100000, "maincpu", 0 )
	ROM_LOAD( "prg0_u3_ver.2.bin", 0x00000, 0x80000, CRC(5e104855) SHA1(3ab2b1ec1fc3aefbb57d9b2ba272e75b34b69383) )
	ROM_LOAD( "prg1_u4.bin", 0x80000, 0x80000, CRC(513f0b18) SHA1(44c61dc1a06bb4c8b4840ea6a372f92114888490) )

	ROM_REGION( 0x80000, "oki", 0 )
	ROM_LOAD( "adpcm_u31.bin", 0x0000, 0x80000, CRC(de514c2b) SHA1(da73aa825d73646f556f6d4dbb46f43acf7c3357) )
ROM_END

static DRIVER_INIT(ggconnie)
{
	DRIVER_INIT_CALL(pce);
}
#endif


//GAME( 19??, ggconnie, 0, ggconnie, ggconnie, ggconnie, ROT0, "Capcom", "Go! Go! Connie chan Jaka Jaka Janken", GAME_NO_SOUND | GAME_NOT_WORKING )

/***************************************************************************

Vendetta (GX081) (c) 1991 Konami

Preliminary driver by:
Ernesto Corvi
someone@secureshell.com

Notes:
- collision detection is handled by a protection chip. Its emulation might
  not be 100% accurate.

// ********************************************************************************
//   Game driver for "ESCAPE KIDS (TM)"  (KONAMI, 1991)
// --------------------------------------------------------------------------------
//
//            This driver was made on the basis of 'src/drivers/vendetta.c' file.
//                                         Driver by OHSAKI Masayuki (2002/08/13)
//
// ********************************************************************************


// ***** NOTES *****
//      -------
//  1) ESCAPE KIDS uses 053246's unknown function. (see video/konamiic.c)
//                   (053246 register #5  UnKnown Bit #5, #3, #2 always set "1")


// ***** On the "error.log" *****
//      --------------------
//  1) "YM2151 Write 00 to undocumented register #xx" (xx=00-1f)
//                Why???

//  2) "xxxx: read from unknown 052109 address yyyy"
//  3) "xxxx: write zz to unknown 052109 address yyyy"
//                These are video/konamiic.c's message.
//                "video/konamiic.c" checks 052109 RAM area access.
//                If accessed over 0x1800 (0x3800), logged 2) or 3) messages.
//                Escape Kids use 0x1800-0x19ff and 0x3800-0x39ff area.


// ***** UnEmulated *****
//      ------------
//  1) 0x3fc0-0x3fcf (052109 RAM area) access (053252 ???)
//  2) 0x7c00 (Banked ROM area) access to data WRITE (???)
//  3) 0x3fda (053248 RAM area) access to data WRITE (Watchdog ???)


// ***** ESCAPE KIDS PCB layout/ Need to dump *****
//      --------------------------------------
//   (Parts side view)
//   +-------------------------------------------------------+
//   |   R          ROM9                               [CN1] |  CN1:Player4 Input?
//   |   O                                             [CN2] |           (Labeled '4P')
//   |   M          ROM8                       ROM1    [SW1] |  CN2:Player3 Input?
//   |   7                              [CUS1]             +-+           (Labeled '3P')
//   |        [CUS7]   [CUS8]                              +-+  CN3:Stereo sound out
//   | R                                       [CUS2]        |
//   | O                                                   J |  SW1:Test Switch
//   | M                                                   A |
//   | 6    [CUS6]                                         M | ***  Custom Chips  ***
//   |                                                     M |      CUS1: 053248
//   | R                                                   A |      CUS2: 053252
//   | O    [CUS5]                                        56P|      CUS3: 053260
//   | M                                                     |      CUS4: 053246
//   | 5                           ROM2  [ Z80 ]           +-+      CUS5: 053247
//   |                                                     +-+      CUS6: 053251
//   | R    [CUS4]                     [CUS3] [YM2151] [CN3] |      CUS7: 051962
//   | O                                                     |      CUS8: 052109
//   | M                                 ROM3                |
//   | 4                                         [Sound AMP] |
//   +-------------------------------------------------------+
//
//  ***  Dump ROMs  ***
//     1) ROM1 (17C)  32Pin 1Mbit UV-EPROM          -> save "975r01" file
//     2) ROM2 ( 5F)  28Pin 512Kbit One-Time PROM   -> save "975f02" file
//     3) ROM3 ( 1D)  40Pin 4Mbit MASK ROM          -> save "975c03" file
//     4) ROM4 ( 3K)  42Pin 8Mbit MASK ROM          -> save "975c04" file
//     5) ROM5 ( 8L)  42Pin 8Mbit MASK ROM          -> save "975c05" file
//     6) ROM6 (12M)  42Pin 8Mbit MASK ROM          -> save "975c06" file
//     7) ROM7 (16K)  42Pin 8Mbit MASK ROM          -> save "975c07" file
//     8) ROM8 (16I)  40Pin 4Mbit MASK ROM          -> save "975c08" file
//     9) ROM9 (18I)  40Pin 4Mbit MASK ROM          -> save "975c09" file
//                                                        vvvvvvvvvvvv
//                                                        esckidsj.zip

***************************************************************************/
#if 0

#include "driver.h"
#include "cpu/z80/z80.h"
#include "video/konamiic.h"
#include "cpu/konami/konami.h" /* for the callback and the firq irq definition */
#include "machine/eeprom.h"
#include "sound/2151intf.h"
#include "sound/k053260.h"
#include "konamipt.h"

/* prototypes */
static MACHINE_RESET( vendetta );
static KONAMI_SETLINES_CALLBACK( vendetta_banking );
static void vendetta_video_banking( running_machine *machine, int select );

VIDEO_START( vendetta );
VIDEO_START( esckids );
VIDEO_UPDATE( vendetta );


/***************************************************************************

  EEPROM

***************************************************************************/

static int init_eeprom_count;


static const eeprom_interface eeprom_intf =
{
	7,				/* address bits */
	8,				/* data bits */
	"011000",		/*  read command */
	"011100",		/* write command */
	0,				/* erase command */
	"0100000000000",/* lock command */
	"0100110000000" /* unlock command */
};

static NVRAM_HANDLER( vendetta )
{
	if (read_or_write)
		eeprom_save(file);
	else
	{
		eeprom_init(machine, &eeprom_intf);

		if (file)
		{
			init_eeprom_count = 0;
			eeprom_load(file);
		}
		else
			init_eeprom_count = 1000;
	}
}

static READ8_HANDLER( vendetta_eeprom_r )
{
	int res = 0;

	res |= 0x02;	//konami_eeprom_ack() << 5;     /* add the ack */

	res |= input_port_read(space->machine, "EEPROM") & 0x0d;	/* test switch */

	if (init_eeprom_count)
	{
		init_eeprom_count--;
		res &= 0xfb;
	}
	return res;
}

static int irq_enabled;

static WRITE8_HANDLER( vendetta_eeprom_w )
{
	/* bit 0 - VOC0 - Video banking related */
	/* bit 1 - VOC1 - Video banking related */
	/* bit 2 - MSCHNG - Mono Sound select (Amp) */
	/* bit 3 - EEPCS - Eeprom CS */
	/* bit 4 - EEPCLK - Eeprom CLK */
	/* bit 5 - EEPDI - Eeprom data */
	/* bit 6 - IRQ enable */
	/* bit 7 - Unused */

	if ( data == 0xff ) /* this is a bug in the eeprom write code */
		return;

	/* EEPROM */
	eeprom_write_bit(data & 0x20);
	eeprom_set_clock_line((data & 0x10) ? ASSERT_LINE : CLEAR_LINE);
	eeprom_set_cs_line((data & 0x08) ? CLEAR_LINE : ASSERT_LINE);

	irq_enabled = ( data >> 6 ) & 1;

	vendetta_video_banking( space->machine, data & 1 );
}

/********************************************/

static READ8_HANDLER( vendetta_K052109_r )
{
	return K052109_r( space, offset + 0x2000 );
}
//static WRITE8_HANDLER( vendetta_K052109_w ) { K052109_w( machine, offset + 0x2000, data ); }
static WRITE8_HANDLER( vendetta_K052109_w )
{
	// *************************************************************************************
	// *  Escape Kids uses 052109's mirrored Tilemap ROM bank selector, but only during    *
	// *  Tilemap MASK-ROM Test       (0x1d80<->0x3d80, 0x1e00<->0x3e00, 0x1f00<->0x3f00)  *
	// *************************************************************************************
	if ( ( offset == 0x1d80 ) || ( offset == 0x1e00 ) || ( offset == 0x1f00 ) )
		K052109_w( space, offset, data );
	K052109_w( space, offset + 0x2000, data );
}

static offs_t video_banking_base;

static void vendetta_video_banking( running_machine *machine, int select )
{
	if ( select & 1 )
	{
		memory_install_readwrite8_handler(cputag_get_address_space(machine, "maincpu", ADDRESS_SPACE_PROGRAM), video_banking_base + 0x2000, video_banking_base + 0x2fff, 0, 0, (read8_space_func)SMH_BANK(4), paletteram_xBBBBBGGGGGRRRRR_be_w );
		memory_install_readwrite8_handler(cputag_get_address_space(machine, "maincpu", ADDRESS_SPACE_PROGRAM), video_banking_base + 0x0000, video_banking_base + 0x0fff, 0, 0, K053247_r, K053247_w );
		memory_set_bankptr(machine, 4, paletteram);
	}
	else
	{
		memory_install_readwrite8_handler(cputag_get_address_space(machine, "maincpu", ADDRESS_SPACE_PROGRAM), video_banking_base + 0x2000, video_banking_base + 0x2fff, 0, 0, vendetta_K052109_r, vendetta_K052109_w );
		memory_install_readwrite8_handler(cputag_get_address_space(machine, "maincpu", ADDRESS_SPACE_PROGRAM), video_banking_base + 0x0000, video_banking_base + 0x0fff, 0, 0, K052109_r, K052109_w );
	}
}

static WRITE8_HANDLER( vendetta_5fe0_w )
{
	/* bit 0,1 coin counters */
	coin_counter_w(0,data & 0x01);
	coin_counter_w(1,data & 0x02);

	/* bit 2 = BRAMBK ?? */

	/* bit 3 = enable char ROM reading through the video RAM */
	K052109_set_RMRD_line((data & 0x08) ? ASSERT_LINE : CLEAR_LINE);

	/* bit 4 = INIT ?? */

	/* bit 5 = enable sprite ROM reading */
	K053246_set_OBJCHA_line((data & 0x20) ? ASSERT_LINE : CLEAR_LINE);
}

static TIMER_CALLBACK( z80_nmi_callback )
{
	cputag_set_input_line(machine, "audiocpu", INPUT_LINE_NMI, ASSERT_LINE );
}

static WRITE8_HANDLER( z80_arm_nmi_w )
{
	cputag_set_input_line(space->machine, "audiocpu", INPUT_LINE_NMI, CLEAR_LINE );

	timer_set( space->machine, ATTOTIME_IN_USEC( 25 ), NULL, 0, z80_nmi_callback );
}

static WRITE8_HANDLER( z80_irq_w )
{
	cputag_set_input_line_and_vector(space->machine, "audiocpu", 0, HOLD_LINE, 0xff );
}

static READ8_HANDLER( vendetta_sound_interrupt_r )
{
	cputag_set_input_line_and_vector(space->machine, "audiocpu", 0, HOLD_LINE, 0xff );
	return 0x00;
}

static READ8_DEVICE_HANDLER( vendetta_sound_r )
{
	return k053260_r(device, 2 + offset);
}

/********************************************/

static ADDRESS_MAP_START( main_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x1fff) AM_ROMBANK(1)
	AM_RANGE(0x2000, 0x3fff) AM_RAM
	AM_RANGE(0x5f80, 0x5f9f) AM_READWRITE(K054000_r, K054000_w)
	AM_RANGE(0x5fa0, 0x5faf) AM_WRITE(K053251_w)
	AM_RANGE(0x5fb0, 0x5fb7) AM_WRITE(K053246_w)
	AM_RANGE(0x5fc0, 0x5fc0) AM_READ_PORT("P1")
	AM_RANGE(0x5fc1, 0x5fc1) AM_READ_PORT("P2")
	AM_RANGE(0x5fc2, 0x5fc2) AM_READ_PORT("P3")
	AM_RANGE(0x5fc3, 0x5fc3) AM_READ_PORT("P4")
	AM_RANGE(0x5fd0, 0x5fd0) AM_READ(vendetta_eeprom_r) /* vblank, service */
	AM_RANGE(0x5fd1, 0x5fd1) AM_READ_PORT("SERVICE")
	AM_RANGE(0x5fe0, 0x5fe0) AM_WRITE(vendetta_5fe0_w)
	AM_RANGE(0x5fe2, 0x5fe2) AM_WRITE(vendetta_eeprom_w)
	AM_RANGE(0x5fe4, 0x5fe4) AM_READWRITE(vendetta_sound_interrupt_r, z80_irq_w)
	AM_RANGE(0x5fe6, 0x5fe7) AM_DEVREADWRITE("konami", vendetta_sound_r, k053260_w)
	AM_RANGE(0x5fe8, 0x5fe9) AM_READ(K053246_r)
	AM_RANGE(0x5fea, 0x5fea) AM_READ(watchdog_reset_r)
	/* what is the desired effect of overlapping these memory regions anyway? */
	AM_RANGE(0x4000, 0x4fff) AM_RAMBANK(3)
	AM_RANGE(0x6000, 0x6fff) AM_RAMBANK(2)
	AM_RANGE(0x4000, 0x7fff) AM_READWRITE(K052109_r, K052109_w)
	AM_RANGE(0x8000, 0xffff) AM_ROM
ADDRESS_MAP_END

static ADDRESS_MAP_START( esckids_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x1fff) AM_RAM							// 053248 64K SRAM
	AM_RANGE(0x3f80, 0x3f80) AM_READ_PORT("P1")
	AM_RANGE(0x3f81, 0x3f81) AM_READ_PORT("P2")
	AM_RANGE(0x3f82, 0x3f82) AM_READ_PORT("P3")				// ???  (But not used)
	AM_RANGE(0x3f83, 0x3f83) AM_READ_PORT("P4")				// ???  (But not used)
	AM_RANGE(0x3f92, 0x3f92) AM_READ(vendetta_eeprom_r)		// vblank, TEST SW on PCB
	AM_RANGE(0x3f93, 0x3f93) AM_READ_PORT("SERVICE")
	AM_RANGE(0x3fa0, 0x3fa7) AM_WRITE(K053246_w)			// 053246 (Sprite)
	AM_RANGE(0x3fb0, 0x3fbf) AM_WRITE(K053251_w)			// 053251 (Priority Encoder)
	AM_RANGE(0x3fc0, 0x3fcf) AM_WRITENOP				// Not Emulated (053252 ???)
	AM_RANGE(0x3fd0, 0x3fd0) AM_WRITE(vendetta_5fe0_w)		// Coin Counter, 052109 RMRD, 053246 OBJCHA
	AM_RANGE(0x3fd2, 0x3fd2) AM_WRITE(vendetta_eeprom_w)	// EEPROM, Video banking
	AM_RANGE(0x3fd4, 0x3fd4) AM_READWRITE(vendetta_sound_interrupt_r, z80_irq_w)			// Sound
	AM_RANGE(0x3fd6, 0x3fd7) AM_DEVREADWRITE("konami", vendetta_sound_r, k053260_w)		// Sound
	AM_RANGE(0x3fd8, 0x3fd9) AM_READ(K053246_r)				// 053246 (Sprite)
	AM_RANGE(0x3fda, 0x3fda) AM_WRITENOP				// Not Emulated (Watchdog ???)
	/* what is the desired effect of overlapping these memory regions anyway? */
	AM_RANGE(0x2000, 0x2fff) AM_RAMBANK(3)					// 052109 (Tilemap) 0x0000-0x0fff
	AM_RANGE(0x4000, 0x4fff) AM_RAMBANK(2)					// 052109 (Tilemap) 0x2000-0x3fff, Tilemap MASK-ROM bank selector (MASK-ROM Test)
	AM_RANGE(0x2000, 0x5fff) AM_READWRITE(K052109_r, K052109_w)			// 052109 (Tilemap)
	AM_RANGE(0x6000, 0x7fff) AM_ROMBANK(1)					// 053248 '975r01' 1M ROM (Banked)
	AM_RANGE(0x8000, 0xffff) AM_ROM							// 053248 '975r01' 1M ROM (0x18000-0x1ffff)
ADDRESS_MAP_END


static ADDRESS_MAP_START( sound_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0xefff) AM_ROM
	AM_RANGE(0xf000, 0xf7ff) AM_RAM
	AM_RANGE(0xf800, 0xf801) AM_DEVREADWRITE("ym", ym2151_r, ym2151_w)
	AM_RANGE(0xfa00, 0xfa00) AM_WRITE(z80_arm_nmi_w)
	AM_RANGE(0xfc00, 0xfc2f) AM_DEVREADWRITE("konami", k053260_r, k053260_w)
ADDRESS_MAP_END


/***************************************************************************

    Input Ports

***************************************************************************/

static INPUT_PORTS_START( vendet4p )
	PORT_START("P1")
	KONAMI8_RL_B12_COIN(1)

	PORT_START("P2")
	KONAMI8_RL_B12_COIN(2)

	PORT_START("P3")
	KONAMI8_RL_B12_COIN(3)

	PORT_START("P4")
	KONAMI8_RL_B12_COIN(4)

	PORT_START("SERVICE")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("EEPROM")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN ) PORT_CUSTOM(eeprom_bit_r, NULL)	/* EEPROM data */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* EEPROM ready */
	PORT_SERVICE_NO_TOGGLE(0x04, IP_ACTIVE_LOW)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_VBLANK ) /* not really vblank, object related. Its timed, otherwise sprites flicker */
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

static INPUT_PORTS_START( vendetta )
	PORT_INCLUDE( vendet4p )

	PORT_MODIFY("P3")
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_MODIFY("P4")
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_MODIFY("SERVICE")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

static INPUT_PORTS_START( esckids )
	PORT_START("P1")
	KONAMI8_RL_B12_COIN(1)		// Player 1 Control

	PORT_START("P2")
	KONAMI8_RL_B12_COIN(2)		// Player 2 Control

	PORT_START("P3")
	KONAMI8_RL_B12_COIN(3)		// Player 3 Control ???  (Not used)

	PORT_START("P4")
	KONAMI8_RL_B12_COIN(4)		// Player 4 Control ???  (Not used)

	PORT_START("SERVICE")		// Start, Service
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("EEPROM")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN ) PORT_CUSTOM(eeprom_bit_r, NULL)	/* EEPROM data */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* EEPROM ready */
	PORT_SERVICE_NO_TOGGLE(0x04, IP_ACTIVE_LOW)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_VBLANK ) /* not really vblank, object related. Its timed, otherwise sprites flicker */
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

static INPUT_PORTS_START( esckidsj )
	PORT_INCLUDE( esckids )

	PORT_MODIFY("P3")
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_MODIFY("P4")
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

/***************************************************************************

    Machine Driver

***************************************************************************/

static INTERRUPT_GEN( vendetta_irq )
{
	if (irq_enabled)
		cpu_set_input_line(device, KONAMI_IRQ_LINE, HOLD_LINE);
}

static MACHINE_DRIVER_START( vendetta )

	/* basic machine hardware */
	MDRV_CPU_ADD("maincpu", KONAMI, 6000000)	/* this is strange, seems an overclock but */
//  MDRV_CPU_ADD("maincpu", KONAMI, 3000000)   /* is needed to have correct music speed */
	MDRV_CPU_PROGRAM_MAP(main_map)
	MDRV_CPU_VBLANK_INT("screen", vendetta_irq)

	MDRV_CPU_ADD("audiocpu", Z80, 3579545)	/* verified with PCB */
	MDRV_CPU_PROGRAM_MAP(sound_map)
                            /* interrupts are triggered by the main CPU */

	MDRV_MACHINE_RESET(vendetta)
	MDRV_NVRAM_HANDLER(vendetta)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_HAS_SHADOWS)

	MDRV_SCREEN_ADD("screen", RASTER)
	MDRV_SCREEN_REFRESH_RATE(60)
	MDRV_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(2500) /* not accurate */)
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_SIZE(64*8, 32*8)
	MDRV_SCREEN_VISIBLE_AREA(13*8, (64-13)*8-1, 2*8, 30*8-1 )

	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_START(vendetta)
	MDRV_VIDEO_UPDATE(vendetta)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_STEREO("lspeaker", "rspeaker")

	MDRV_SOUND_ADD("ym", YM2151, 3579545)	/* verified with PCB */
	MDRV_SOUND_ROUTE(0, "lspeaker", 1.0)
	MDRV_SOUND_ROUTE(1, "rspeaker", 1.0)

	MDRV_SOUND_ADD("konami", K053260, 3579545)	/* verified with PCB */
	MDRV_SOUND_ROUTE(0, "lspeaker", 0.75)
	MDRV_SOUND_ROUTE(1, "rspeaker", 0.75)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( esckids )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(vendetta)
	MDRV_CPU_MODIFY("maincpu")
	MDRV_CPU_PROGRAM_MAP(esckids_map)

	MDRV_SCREEN_MODIFY("screen")
//MDRV_SCREEN_VISIBLE_AREA(13*8, (64-13)*8-1, 2*8, 30*8-1 )    /* black areas on the edges */
	MDRV_SCREEN_VISIBLE_AREA(14*8, (64-14)*8-1, 2*8, 30*8-1 )

	MDRV_VIDEO_START(esckids)

MACHINE_DRIVER_END



/***************************************************************************

  Game ROMs

***************************************************************************/

ROM_START( vendetta )
	ROM_REGION( 0x49000, "maincpu", 0 ) /* code + banked roms + banked ram */
	ROM_LOAD( "081t01", 0x10000, 0x38000, CRC(e76267f5) SHA1(efef6c2edb4c181374661f358dad09123741b63d) )
	ROM_CONTINUE(		0x08000, 0x08000 )

	ROM_REGION( 0x10000, "audiocpu", 0 ) /* 64k for the sound CPU */
	ROM_LOAD( "081b02", 0x000000, 0x10000, CRC(4c604d9b) SHA1(22d979f5dbde7912dd927bf5538fdbfc5b82905e) )

	ROM_REGION( 0x100000, "gfx1", 0 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "081a09", 0x000000, 0x080000, CRC(b4c777a9) SHA1(cc2b1dff4404ecd72b604e25d00fffdf7f0f8b52) ) /* characters */
	ROM_LOAD( "081a08", 0x080000, 0x080000, CRC(272ac8d9) SHA1(2da12fe4c13921bf0d4ebffec326f8d207ec4fad) ) /* characters */

	ROM_REGION( 0x400000, "gfx2", 0 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "081a04", 0x000000, 0x100000, CRC(464b9aa4) SHA1(28066ff0a07c3e56e7192918a882778c1b316b37) ) /* sprites */
	ROM_LOAD( "081a05", 0x100000, 0x100000, CRC(4e173759) SHA1(ce803f2aca7d7dedad00ab30e112443848747bd2) ) /* sprites */
	ROM_LOAD( "081a06", 0x200000, 0x100000, CRC(e9fe6d80) SHA1(2b7fc9d7fe43cd85dc8b975fe639c273cb0d9256) ) /* sprites */
	ROM_LOAD( "081a07", 0x300000, 0x100000, CRC(8a22b29a) SHA1(be539f21518e13038ab1d4cc2b2a901dd3e621f4) ) /* sprites */

	ROM_REGION( 0x100000, "konami", 0 ) /* 053260 samples */
	ROM_LOAD( "081a03", 0x000000, 0x100000, CRC(14b6baea) SHA1(fe15ee57f19f5acaad6c1642d51f390046a7468a) )
ROM_END

ROM_START( vendettar )
	ROM_REGION( 0x49000, "maincpu", 0 ) /* code + banked roms + banked ram */
	ROM_LOAD( "081r01", 0x10000, 0x38000, CRC(84796281) SHA1(e4330c6eaa17adda5b4bd3eb824388c89fb07918) )
	ROM_CONTINUE(		0x08000, 0x08000 )

	ROM_REGION( 0x10000, "audiocpu", 0 ) /* 64k for the sound CPU */
	ROM_LOAD( "081b02", 0x000000, 0x10000, CRC(4c604d9b) SHA1(22d979f5dbde7912dd927bf5538fdbfc5b82905e) )

	ROM_REGION( 0x100000, "gfx1", 0 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "081a09", 0x000000, 0x080000, CRC(b4c777a9) SHA1(cc2b1dff4404ecd72b604e25d00fffdf7f0f8b52) ) /* characters */
	ROM_LOAD( "081a08", 0x080000, 0x080000, CRC(272ac8d9) SHA1(2da12fe4c13921bf0d4ebffec326f8d207ec4fad) ) /* characters */

	ROM_REGION( 0x400000, "gfx2", 0 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "081a04", 0x000000, 0x100000, CRC(464b9aa4) SHA1(28066ff0a07c3e56e7192918a882778c1b316b37) ) /* sprites */
	ROM_LOAD( "081a05", 0x100000, 0x100000, CRC(4e173759) SHA1(ce803f2aca7d7dedad00ab30e112443848747bd2) ) /* sprites */
	ROM_LOAD( "081a06", 0x200000, 0x100000, CRC(e9fe6d80) SHA1(2b7fc9d7fe43cd85dc8b975fe639c273cb0d9256) ) /* sprites */
	ROM_LOAD( "081a07", 0x300000, 0x100000, CRC(8a22b29a) SHA1(be539f21518e13038ab1d4cc2b2a901dd3e621f4) ) /* sprites */

	ROM_REGION( 0x100000, "konami", 0 ) /* 053260 samples */
	ROM_LOAD( "081a03", 0x000000, 0x100000, CRC(14b6baea) SHA1(fe15ee57f19f5acaad6c1642d51f390046a7468a) )
ROM_END

ROM_START( vendetta2p )
	ROM_REGION( 0x49000, "maincpu", 0 ) /* code + banked roms + banked ram */
	ROM_LOAD( "081w01", 0x10000, 0x38000, CRC(cee57132) SHA1(8b6413877e127511daa76278910c2ee3247d613a) )
	ROM_CONTINUE(		0x08000, 0x08000 )

	ROM_REGION( 0x10000, "audiocpu", 0 ) /* 64k for the sound CPU */
	ROM_LOAD( "081b02", 0x000000, 0x10000, CRC(4c604d9b) SHA1(22d979f5dbde7912dd927bf5538fdbfc5b82905e) )

	ROM_REGION( 0x100000, "gfx1", 0 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "081a09", 0x000000, 0x080000, CRC(b4c777a9) SHA1(cc2b1dff4404ecd72b604e25d00fffdf7f0f8b52) ) /* characters */
	ROM_LOAD( "081a08", 0x080000, 0x080000, CRC(272ac8d9) SHA1(2da12fe4c13921bf0d4ebffec326f8d207ec4fad) ) /* characters */

	ROM_REGION( 0x400000, "gfx2", 0 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "081a04", 0x000000, 0x100000, CRC(464b9aa4) SHA1(28066ff0a07c3e56e7192918a882778c1b316b37) ) /* sprites */
	ROM_LOAD( "081a05", 0x100000, 0x100000, CRC(4e173759) SHA1(ce803f2aca7d7dedad00ab30e112443848747bd2) ) /* sprites */
	ROM_LOAD( "081a06", 0x200000, 0x100000, CRC(e9fe6d80) SHA1(2b7fc9d7fe43cd85dc8b975fe639c273cb0d9256) ) /* sprites */
	ROM_LOAD( "081a07", 0x300000, 0x100000, CRC(8a22b29a) SHA1(be539f21518e13038ab1d4cc2b2a901dd3e621f4) ) /* sprites */

	ROM_REGION( 0x100000, "konami", 0 ) /* 053260 samples */
	ROM_LOAD( "081a03", 0x000000, 0x100000, CRC(14b6baea) SHA1(fe15ee57f19f5acaad6c1642d51f390046a7468a) )
ROM_END

ROM_START( vendetta2pu )
	ROM_REGION( 0x49000, "maincpu", 0 ) /* code + banked roms + banked ram */
	ROM_LOAD( "081u01", 0x10000, 0x38000, CRC(b4d9ade5) SHA1(fbd543738cb0b68c80ff05eed7849b608de03395) )
	ROM_CONTINUE(		0x08000, 0x08000 )

	ROM_REGION( 0x10000, "audiocpu", 0 ) /* 64k for the sound CPU */
	ROM_LOAD( "081b02", 0x000000, 0x10000, CRC(4c604d9b) SHA1(22d979f5dbde7912dd927bf5538fdbfc5b82905e) )

	ROM_REGION( 0x100000, "gfx1", 0 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "081a09", 0x000000, 0x080000, CRC(b4c777a9) SHA1(cc2b1dff4404ecd72b604e25d00fffdf7f0f8b52) ) /* characters */
	ROM_LOAD( "081a08", 0x080000, 0x080000, CRC(272ac8d9) SHA1(2da12fe4c13921bf0d4ebffec326f8d207ec4fad) ) /* characters */

	ROM_REGION( 0x400000, "gfx2", 0 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "081a04", 0x000000, 0x100000, CRC(464b9aa4) SHA1(28066ff0a07c3e56e7192918a882778c1b316b37) ) /* sprites */
	ROM_LOAD( "081a05", 0x100000, 0x100000, CRC(4e173759) SHA1(ce803f2aca7d7dedad00ab30e112443848747bd2) ) /* sprites */
	ROM_LOAD( "081a06", 0x200000, 0x100000, CRC(e9fe6d80) SHA1(2b7fc9d7fe43cd85dc8b975fe639c273cb0d9256) ) /* sprites */
	ROM_LOAD( "081a07", 0x300000, 0x100000, CRC(8a22b29a) SHA1(be539f21518e13038ab1d4cc2b2a901dd3e621f4) ) /* sprites */

	ROM_REGION( 0x100000, "konami", 0 ) /* 053260 samples */
	ROM_LOAD( "081a03", 0x000000, 0x100000, CRC(14b6baea) SHA1(fe15ee57f19f5acaad6c1642d51f390046a7468a) )
ROM_END

ROM_START( vendetta2pd )
	ROM_REGION( 0x49000, "maincpu", 0 ) /* code + banked roms + banked ram */
	ROM_LOAD( "081d01", 0x10000, 0x38000, CRC(335da495) SHA1(ea74680eb898aeecf9f1eec95f151bcf66e6b6cb) )
	ROM_CONTINUE(		0x08000, 0x08000 )

	ROM_REGION( 0x10000, "audiocpu", 0 ) /* 64k for the sound CPU */
	ROM_LOAD( "081b02", 0x000000, 0x10000, CRC(4c604d9b) SHA1(22d979f5dbde7912dd927bf5538fdbfc5b82905e) )

	ROM_REGION( 0x100000, "gfx1", 0 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "081a09", 0x000000, 0x080000, CRC(b4c777a9) SHA1(cc2b1dff4404ecd72b604e25d00fffdf7f0f8b52) ) /* characters */
	ROM_LOAD( "081a08", 0x080000, 0x080000, CRC(272ac8d9) SHA1(2da12fe4c13921bf0d4ebffec326f8d207ec4fad) ) /* characters */

	ROM_REGION( 0x400000, "gfx2", 0 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "081a04", 0x000000, 0x100000, CRC(464b9aa4) SHA1(28066ff0a07c3e56e7192918a882778c1b316b37) ) /* sprites */
	ROM_LOAD( "081a05", 0x100000, 0x100000, CRC(4e173759) SHA1(ce803f2aca7d7dedad00ab30e112443848747bd2) ) /* sprites */
	ROM_LOAD( "081a06", 0x200000, 0x100000, CRC(e9fe6d80) SHA1(2b7fc9d7fe43cd85dc8b975fe639c273cb0d9256) ) /* sprites */
	ROM_LOAD( "081a07", 0x300000, 0x100000, CRC(8a22b29a) SHA1(be539f21518e13038ab1d4cc2b2a901dd3e621f4) ) /* sprites */

	ROM_REGION( 0x100000, "konami", 0 ) /* 053260 samples */
	ROM_LOAD( "081a03", 0x000000, 0x100000, CRC(14b6baea) SHA1(fe15ee57f19f5acaad6c1642d51f390046a7468a) )
ROM_END

ROM_START( vendettaj )
	ROM_REGION( 0x49000, "maincpu", 0 ) /* code + banked roms + banked ram */
	ROM_LOAD( "081p01", 0x10000, 0x38000, CRC(5fe30242) SHA1(2ea98e66637fa2ad60044b1a2b0dd158a82403a2) )
	ROM_CONTINUE(		0x08000, 0x08000 )

	ROM_REGION( 0x10000, "audiocpu", 0 ) /* 64k for the sound CPU */
	ROM_LOAD( "081b02", 0x000000, 0x10000, CRC(4c604d9b) SHA1(22d979f5dbde7912dd927bf5538fdbfc5b82905e) )

	ROM_REGION( 0x100000, "gfx1", 0 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "081a09", 0x000000, 0x080000, CRC(b4c777a9) SHA1(cc2b1dff4404ecd72b604e25d00fffdf7f0f8b52) ) /* characters */
	ROM_LOAD( "081a08", 0x080000, 0x080000, CRC(272ac8d9) SHA1(2da12fe4c13921bf0d4ebffec326f8d207ec4fad) ) /* characters */

	ROM_REGION( 0x400000, "gfx2", 0 ) /* graphics ( don't dispose as the program can read them ) */
	ROM_LOAD( "081a04", 0x000000, 0x100000, CRC(464b9aa4) SHA1(28066ff0a07c3e56e7192918a882778c1b316b37) ) /* sprites */
	ROM_LOAD( "081a05", 0x100000, 0x100000, CRC(4e173759) SHA1(ce803f2aca7d7dedad00ab30e112443848747bd2) ) /* sprites */
	ROM_LOAD( "081a06", 0x200000, 0x100000, CRC(e9fe6d80) SHA1(2b7fc9d7fe43cd85dc8b975fe639c273cb0d9256) ) /* sprites */
	ROM_LOAD( "081a07", 0x300000, 0x100000, CRC(8a22b29a) SHA1(be539f21518e13038ab1d4cc2b2a901dd3e621f4) ) /* sprites */

	ROM_REGION( 0x100000, "konami", 0 ) /* 053260 samples */
	ROM_LOAD( "081a03", 0x000000, 0x100000, CRC(14b6baea) SHA1(fe15ee57f19f5acaad6c1642d51f390046a7468a) )
ROM_END

ROM_START( esckids )
	ROM_REGION( 0x049000, "maincpu", 0 )		// Main CPU (053248) Code & Banked (1M x 1)
	ROM_LOAD( "17c.bin", 0x010000, 0x018000, CRC(9dfba99c) SHA1(dbcb89aad5a9addaf7200b2524be999877313a6e) )
	ROM_CONTINUE(		0x008000, 0x008000 )

	ROM_REGION( 0x010000, "audiocpu", 0 )		// Sound CPU (Z80) Code (512K x 1)
	ROM_LOAD( "975f02", 0x000000, 0x010000, CRC(994fb229) SHA1(bf194ae91240225b8edb647b1a62cd83abfa215e) )

	ROM_REGION( 0x100000, "gfx1", 0 )		// Tilemap MASK-ROM (4M x 2)
	ROM_LOAD( "975c09", 0x000000, 0x080000, CRC(bc52210e) SHA1(301a3892d250495c2e849d67fea5f01fb0196bed) )
	ROM_LOAD( "975c08", 0x080000, 0x080000, CRC(fcff9256) SHA1(b60d29f4d04f074120d4bb7f2a71b9e9bf252d33) )

	ROM_REGION( 0x400000, "gfx2", 0 )		// Sprite MASK-ROM (8M x 4)
	ROM_LOAD( "975c04", 0x000000, 0x100000, CRC(15688a6f) SHA1(a445237a11e5f98f0f9b2573a7ef0583366a137e) )
	ROM_LOAD( "975c05", 0x100000, 0x100000, CRC(1ff33bb7) SHA1(eb17da33ba2769ea02f91fece27de2e61705e75a) )
	ROM_LOAD( "975c06", 0x200000, 0x100000, CRC(36d410f9) SHA1(2b1fd93c11839480aa05a8bf27feef7591704f3d) )
	ROM_LOAD( "975c07", 0x300000, 0x100000, CRC(97ec541e) SHA1(d1aa186b17cfe6e505f5b305703319299fa54518) )

	ROM_REGION( 0x100000, "konami", 0 )	// Samples MASK-ROM (4M x 1)
	ROM_LOAD( "975c03", 0x000000, 0x080000, CRC(dc4a1707) SHA1(f252d08483fd664f8fc03bf8f174efd452b4cdc5) )
ROM_END


ROM_START( esckidsj )
	ROM_REGION( 0x049000, "maincpu", 0 )		// Main CPU (053248) Code & Banked (1M x 1)
	ROM_LOAD( "975r01", 0x010000, 0x018000, CRC(7b5c5572) SHA1(b94b58c010539926d112c2dfd80bcbad76acc986) )
	ROM_CONTINUE(		0x008000, 0x008000 )

	ROM_REGION( 0x010000, "audiocpu", 0 )		// Sound CPU (Z80) Code (512K x 1)
	ROM_LOAD( "975f02", 0x000000, 0x010000, CRC(994fb229) SHA1(bf194ae91240225b8edb647b1a62cd83abfa215e) )

	ROM_REGION( 0x100000, "gfx1", 0 )		// Tilemap MASK-ROM (4M x 2)
	ROM_LOAD( "975c09", 0x000000, 0x080000, CRC(bc52210e) SHA1(301a3892d250495c2e849d67fea5f01fb0196bed) )
	ROM_LOAD( "975c08", 0x080000, 0x080000, CRC(fcff9256) SHA1(b60d29f4d04f074120d4bb7f2a71b9e9bf252d33) )

	ROM_REGION( 0x400000, "gfx2", 0 )		// Sprite MASK-ROM (8M x 4)
	ROM_LOAD( "975c04", 0x000000, 0x100000, CRC(15688a6f) SHA1(a445237a11e5f98f0f9b2573a7ef0583366a137e) )
	ROM_LOAD( "975c05", 0x100000, 0x100000, CRC(1ff33bb7) SHA1(eb17da33ba2769ea02f91fece27de2e61705e75a) )
	ROM_LOAD( "975c06", 0x200000, 0x100000, CRC(36d410f9) SHA1(2b1fd93c11839480aa05a8bf27feef7591704f3d) )
	ROM_LOAD( "975c07", 0x300000, 0x100000, CRC(97ec541e) SHA1(d1aa186b17cfe6e505f5b305703319299fa54518) )

	ROM_REGION( 0x100000, "konami", 0 )	// Samples MASK-ROM (4M x 1)
	ROM_LOAD( "975c03", 0x000000, 0x080000, CRC(dc4a1707) SHA1(f252d08483fd664f8fc03bf8f174efd452b4cdc5) )
ROM_END


/***************************************************************************

  Game driver(s)

***************************************************************************/

static KONAMI_SETLINES_CALLBACK( vendetta_banking )
{
	UINT8 *RAM = memory_region(device->machine, "maincpu");

	if ( lines >= 0x1c )
	{
		logerror("PC = %04x : Unknown bank selected %02x\n", cpu_get_pc(device), lines );
	}
	else
		memory_set_bankptr(device->machine,  1, &RAM[ 0x10000 + ( lines * 0x2000 ) ] );
}

static MACHINE_RESET( vendetta )
{
	konami_configure_set_lines(cputag_get_cpu(machine, "maincpu"), vendetta_banking);

	paletteram = &memory_region(machine, "maincpu")[0x48000];
	irq_enabled = 0;

	/* init banks */
	memory_set_bankptr(machine,  1, &memory_region(machine, "maincpu")[0x10000] );
	vendetta_video_banking( machine, 0 );
}


static DRIVER_INIT( vendetta )
{
	video_banking_base = 0x4000;
	konami_rom_deinterleave_2(machine, "gfx1");
	konami_rom_deinterleave_4(machine, "gfx2");
}

static DRIVER_INIT( esckids )
{
	video_banking_base = 0x2000;
	konami_rom_deinterleave_2(machine, "gfx1");
	konami_rom_deinterleave_4(machine, "gfx2");
}
#endif


//GAME( 1991, vendetta,    0,        vendetta, vendet4p, vendetta, ROT0, "Konami", "Vendetta (World 4 Players ver. T)", 0 )
//GAME( 1991, vendettar,   vendetta, vendetta, vendet4p, vendetta, ROT0, "Konami", "Vendetta (World 4 Players ver. R)", 0 )
//GAME( 1991, vendetta2p,  vendetta, vendetta, vendetta, vendetta, ROT0, "Konami", "Vendetta (World 2 Players ver. W)", 0 )
//GAME( 1991, vendetta2pu, vendetta, vendetta, vendetta, vendetta, ROT0, "Konami", "Vendetta (Asia 2 Players ver. U)", 0 )
//GAME( 1991, vendetta2pd, vendetta, vendetta, vendetta, vendetta, ROT0, "Konami", "Vendetta (Asia 2 Players ver. D)", 0 )
//GAME( 1991, vendettaj,   vendetta, vendetta, vendetta, vendetta, ROT0, "Konami", "Crime Fighters 2 (Japan 2 Players ver. P)", 0 )
//GAME( 1991, esckids,     0,        esckids,  esckids,  esckids,  ROT0, "Konami", "Escape Kids (Asia, 4 Players)", 0 )
//GAME( 1991, esckidsj,    esckids,  esckids,  esckidsj, esckids,  ROT0, "Konami", "Escape Kids (Japan, 2 Players)", 0 )

/***************************************************************************

                          -= Yun Sung 8 Bit Games =-

                    driver by   Luca Elia (l.elia@tin.it)


Main  CPU    :  Z80B
Sound CPU    :  Z80A
Video Chips  :  ?
Sound Chips  :  OKI M5205 + YM3812

---------------------------------------------------------------------------
Year + Game         Board#
---------------------------------------------------------------------------
95  Cannon Ball      YS-ROCK-970712 or 940712?
95  Magix / Rock     YS-ROCK-970712 or 940712?
94? Rock Tris        YS-ROCK-940712
---------------------------------------------------------------------------

Notes:

- "Magix" can change title to "Rock" through a DSW
- In service mode press Service Coin (e.g. '9')

To Do:

- Better Sound

***************************************************************************/

#include "driver.h"
#include "cpu/z80/z80.h"
#include "sound/3812intf.h"
#include "sound/msm5205.h"

/* Variables defined in video: */

extern UINT8 *yunsung8_videoram_0, *yunsung8_videoram_1;
extern int yunsung8_layers_ctrl;

/* Functions defined in video: */

WRITE8_HANDLER( yunsung8_videobank_w );

READ8_HANDLER ( yunsung8_videoram_r );
WRITE8_HANDLER( yunsung8_videoram_w );

WRITE8_HANDLER( yunsung8_flipscreen_w );

VIDEO_START( yunsung8 );
VIDEO_UPDATE( yunsung8 );



static MACHINE_RESET( yunsung8 )
{
	const address_space *space = cputag_get_address_space(machine, "maincpu", ADDRESS_SPACE_PROGRAM);
	UINT8* yunsung8_videoram = auto_alloc_array(machine, UINT8, 0x4000);

	yunsung8_videoram_0 = yunsung8_videoram + 0x0000;	// Ram is banked
	yunsung8_videoram_1 = yunsung8_videoram + 0x2000;
	yunsung8_videobank_w(space, 0, 0);
}


/***************************************************************************


                            Memory Maps - Main CPU


***************************************************************************/


static WRITE8_HANDLER( yunsung8_bankswitch_w )
{
	UINT8 *RAM = memory_region(space->machine, "maincpu");

	int bank				=	data & 7;		// ROM bank
	yunsung8_layers_ctrl	=	data & 0x30;	// Layers enable

	if (data & ~0x37)	logerror("CPU #0 - PC %04X: Bank %02X\n",cpu_get_pc(space->cpu),data);

	if (bank < 3)	RAM = &RAM[0x4000 * bank];
	else			RAM = &RAM[0x4000 * (bank-3) + 0x10000];

	memory_set_bankptr(space->machine, 1, RAM);
}

/*
    Banked Video RAM:

    c000-c7ff   Palette (bit 1 of port 0 switches between 2 banks)

    c800-cfff   Color   (bit 0 of port 0 switches between 2 banks)
    d000-dfff   Tiles   ""
*/

static ADDRESS_MAP_START( main_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0001, 0x0001) AM_WRITE(yunsung8_bankswitch_w	)	// ROM Bank (again?)
	AM_RANGE(0x8000, 0xbfff) AM_READ(SMH_BANK(1)				)	// Banked ROM
	AM_RANGE(0x0000, 0xbfff) AM_ROM
	AM_RANGE(0xc000, 0xdfff) AM_READWRITE(yunsung8_videoram_r, yunsung8_videoram_w	)	// Video RAM (Banked)
	AM_RANGE(0xe000, 0xffff) AM_RAM
ADDRESS_MAP_END


static ADDRESS_MAP_START( port_map, ADDRESS_SPACE_IO, 8 )
	ADDRESS_MAP_GLOBAL_MASK(0xff)
	AM_RANGE(0x00, 0x00) AM_READ_PORT("SYSTEM") AM_WRITE(yunsung8_videobank_w)	// video RAM bank
	AM_RANGE(0x01, 0x01) AM_READ_PORT("P1") AM_WRITE(yunsung8_bankswitch_w	)	// ROM Bank + Layers Enable
	AM_RANGE(0x02, 0x02) AM_READ_PORT("P2") AM_WRITE(soundlatch_w			)	// To Sound CPU
	AM_RANGE(0x03, 0x03) AM_READ_PORT("DSW1")
	AM_RANGE(0x04, 0x04) AM_READ_PORT("DSW2")
	AM_RANGE(0x06, 0x06) AM_WRITE(yunsung8_flipscreen_w						)	// Flip Screen
	AM_RANGE(0x07, 0x07) AM_WRITE(SMH_NOP									)	// ? (end of IRQ, random value)
ADDRESS_MAP_END



/***************************************************************************


                            Memory Maps - Sound CPU


***************************************************************************/


static int adpcm;

static WRITE8_DEVICE_HANDLER( yunsung8_sound_bankswitch_w )
{
	UINT8 *RAM = memory_region(device->machine, "audiocpu");
	int bank = data & 7;

	if ( bank != (data&(~0x20)) ) 	logerror("%s: Bank %02X\n",cpuexec_describe_context(device->machine),data);

	if (bank < 3)	RAM = &RAM[0x4000 * bank];
	else			RAM = &RAM[0x4000 * (bank-3) + 0x10000];

	memory_set_bankptr(device->machine, 2, RAM);

	msm5205_reset_w(device,data & 0x20);
}

static WRITE8_HANDLER( yunsung8_adpcm_w )
{
	/* Swap the nibbles */
	adpcm = ((data&0xf)<<4) | ((data >>4)&0xf);
}



static ADDRESS_MAP_START( sound_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x7fff) AM_ROM
	AM_RANGE(0x8000, 0xbfff) AM_READWRITE(SMH_BANK(2),SMH_ROM		)	// Banked ROM
	AM_RANGE(0xe000, 0xe000) AM_DEVWRITE("msm", yunsung8_sound_bankswitch_w	)	// ROM Bank
	AM_RANGE(0xe400, 0xe400) AM_WRITE(yunsung8_adpcm_w				)
	AM_RANGE(0xec00, 0xec01) AM_DEVWRITE("ym", ym3812_w		)
	AM_RANGE(0xf000, 0xf7ff) AM_RAM
	AM_RANGE(0xf800, 0xf800) AM_READ(soundlatch_r					)	// From Main CPU
ADDRESS_MAP_END




/***************************************************************************


                                Input Ports


***************************************************************************/

/***************************************************************************
                                    Magix
***************************************************************************/
#if 0
static INPUT_PORTS_START( magix )
	PORT_START("SYSTEM")
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x02, IP_ACTIVE_LOW, IPT_START2   )
	PORT_BIT(  0x04, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x08, IP_ACTIVE_LOW, IPT_START1   )
	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_COIN1    )

	PORT_START("P1")
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x02, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x04, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(1)	// same as button1 !?
	PORT_BIT(  0x08, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(1)
	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_PLAYER(1)
	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_PLAYER(1)
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_PLAYER(1)
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_PLAYER(1)

	PORT_START("P2")
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x02, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x04, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)	// same as button1 !?
	PORT_BIT(  0x08, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_PLAYER(2)
	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_PLAYER(2)
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_PLAYER(2)
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_PLAYER(2)

	PORT_START("DSW1")
	PORT_SERVICE( 0x01, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x06, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_4C ) )

	PORT_START("DSW2")
	PORT_DIPNAME( 0x01, 0x01, "Title" )
	PORT_DIPSETTING(    0x01, "Magix" )
	PORT_DIPSETTING(    0x00, "Rock" )
	PORT_DIPNAME( 0x02, 0x02, "Unknown 2-1" )	// the rest seems unused
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Unknown 2-2" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Unknown 2-3" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Unknown 2-4" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Unknown 2-5" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Unknown 2-6" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Unknown 2-7" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END
#endif

/***************************************************************************
                                Cannon Ball
***************************************************************************/

static INPUT_PORTS_START( cannball )
	PORT_START("SYSTEM")
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x02, IP_ACTIVE_LOW, IPT_START2   )
	PORT_BIT(  0x04, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x08, IP_ACTIVE_LOW, IPT_START1   )
	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_COIN1    )

	PORT_START("P1")
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x02, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(1)
	PORT_BIT(  0x04, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(1)
	PORT_BIT(  0x08, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(1)
	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_PLAYER(1)
	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_PLAYER(1)
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_PLAYER(1)
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_PLAYER(1)

	PORT_START("P2")
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x02, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(2)
	PORT_BIT(  0x04, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
	PORT_BIT(  0x08, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_PLAYER(2)
	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_PLAYER(2)
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_PLAYER(2)
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_PLAYER(2)

	PORT_START("DSW1")
	PORT_SERVICE( 0x01, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x06, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_4C ) )

	PORT_START("DSW2")
	PORT_DIPNAME( 0x01, 0x01, "Unknown 2-0" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Unknown 2-1" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, "Bombs" )
	PORT_DIPSETTING(    0x04, "1" )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x10, 0x10, "Unknown 2-4" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Unknown 2-5" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Unknown 2-6" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Unknown 2-7" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

#if 0
static INPUT_PORTS_START( cannbalv )
	PORT_INCLUDE(cannball)
	PORT_MODIFY("SYSTEM")
	PORT_BIT(  0x40, IP_ACTIVE_HIGH, IPT_SPECIAL  ) // always activated, otherwise the game resets. a simple check for horizontal / vertical version of the game?
INPUT_PORTS_END
#endif
/***************************************************************************
                                    Rock Tris
***************************************************************************/
#if 0
static INPUT_PORTS_START( rocktris )
	PORT_START("SYSTEM")
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x02, IP_ACTIVE_LOW, IPT_START2   )
	PORT_BIT(  0x04, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x08, IP_ACTIVE_LOW, IPT_START1   )
	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_SERVICE1 ) PORT_IMPULSE(1)
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_COIN1    ) PORT_IMPULSE(1)

	PORT_START("P1")
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x02, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x04, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(1)	// same as button1 !?
	PORT_BIT(  0x08, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(1)
	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_PLAYER(1)
	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_PLAYER(1)
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_PLAYER(1)
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_PLAYER(1)

	PORT_START("P2")
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x02, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT(  0x04, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)	// same as button1 !?
	PORT_BIT(  0x08, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_PLAYER(2)
	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_PLAYER(2)
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_PLAYER(2)
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_PLAYER(2)

	PORT_START("DSW1")
	PORT_SERVICE( 0x01, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x06, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_4C ) )

	PORT_START("DSW2")
	PORT_DIPNAME( 0x01, 0x01, "Unknown 2-0" ) 	// the rest seems unused
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Unknown 2-1" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Unknown 2-2" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Unknown 2-3" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Unknown 2-4" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Unknown 2-5" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Unknown 2-6" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Unknown 2-7" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END
#endif
/***************************************************************************


                                Graphics Layouts


***************************************************************************/

/* 8x8x4 tiles in 2 roms */
static const gfx_layout layout_8x8x4 =
{
	8,8,
	RGN_FRAC(1,2),
	4,
	{ STEP4(0,1) },
	{ RGN_FRAC(1,2)+1*4,RGN_FRAC(1,2)+0*4,1*4,0*4, RGN_FRAC(1,2)+3*4,RGN_FRAC(1,2)+2*4,3*4,2*4},
	{ STEP8(0,16) },
	8*8*4/2
};

/* 8x8x8 tiles in 4 roms */
static const gfx_layout layout_8x8x8 =
{
	8,8,
	RGN_FRAC(1,4),
	8,
	{ STEP8(0,1) },
	{ RGN_FRAC(0,4) + 0*8, RGN_FRAC(1,4) + 0*8, RGN_FRAC(2,4) + 0*8, RGN_FRAC(3,4) + 0*8,
	  RGN_FRAC(0,4) + 1*8, RGN_FRAC(1,4) + 1*8, RGN_FRAC(2,4) + 1*8, RGN_FRAC(3,4) + 1*8 },
	{ STEP8(0,16) },
	8*8*8/4
};

static GFXDECODE_START( yunsung8 )
	GFXDECODE_ENTRY( "gfx1", 0, layout_8x8x8, 0, 0x08 ) // [0] Tiles (Background)
	GFXDECODE_ENTRY( "gfx2", 0, layout_8x8x4, 0,	0x40 ) // [1] Tiles (Text)
GFXDECODE_END



/***************************************************************************


                                Machine Drivers


***************************************************************************/


static void yunsung8_adpcm_int(const device_config *device)
{
	static int toggle = 0;

	msm5205_data_w(device, adpcm >> 4);
	adpcm <<= 4;

	toggle ^= 1;
	if (toggle)
		cputag_set_input_line(device->machine, "audiocpu", INPUT_LINE_NMI, PULSE_LINE);
}

static const msm5205_interface yunsung8_msm5205_interface =
{
	yunsung8_adpcm_int,	/* interrupt function */
	MSM5205_S96_4B		/* 4KHz, 4 Bits */
};


static MACHINE_DRIVER_START( yunsung8 )

	/* basic machine hardware */
	MDRV_CPU_ADD("maincpu", Z80, 8000000)			/* Z80B */
	MDRV_CPU_PROGRAM_MAP(main_map)
	MDRV_CPU_IO_MAP(port_map)
	MDRV_CPU_VBLANK_INT("screen", irq0_line_hold)	/* No nmi routine */

	MDRV_CPU_ADD("audiocpu", Z80, 4000000)			/* ? */
	MDRV_CPU_PROGRAM_MAP(sound_map)
	MDRV_CPU_VBLANK_INT("screen", irq0_line_hold)	/* NMI caused by the MSM5205? */

	MDRV_MACHINE_RESET(yunsung8)

	/* video hardware */
	MDRV_SCREEN_ADD("screen", RASTER)
	MDRV_SCREEN_REFRESH_RATE(60)
	MDRV_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(0))
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_SIZE(512, 256)
	MDRV_SCREEN_VISIBLE_AREA(0+64, 512-64-1, 0+8, 256-8-1)

	MDRV_GFXDECODE(yunsung8)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_START(yunsung8)
	MDRV_VIDEO_UPDATE(yunsung8)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_STEREO("lspeaker", "rspeaker")

	MDRV_SOUND_ADD("ym", YM3812, 4000000)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "lspeaker", 1.0)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "rspeaker", 1.0)

	MDRV_SOUND_ADD("msm", MSM5205, XTAL_400kHz) /* verified on pcb */
	MDRV_SOUND_CONFIG(yunsung8_msm5205_interface)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "lspeaker", 0.80)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "rspeaker", 0.80)
MACHINE_DRIVER_END


/***************************************************************************


                                ROMs Loading


***************************************************************************/

/***************************************************************************

                                    Magix

Yun Sung, 1995.
CPU : Z80B
SND : Z80A + YM3812 + Oki M5205
OSC : 16.000

***************************************************************************/
#if 0
ROM_START( magix )

	ROM_REGION( 0x24000, "maincpu", 0 )		/* Main Z80 Code */
	ROM_LOAD( "yunsung8.07", 0x00000, 0x0c000, CRC(d4d0b68b) SHA1(d7e1fb57a14f8b822791b98cecc6d5a053a89e0f) )
	ROM_CONTINUE(         0x10000, 0x14000             )

	ROM_REGION( 0x24000, "audiocpu", 0 )		/* Sound Z80 Code */
	ROM_LOAD( "yunsung8.08", 0x00000, 0x0c000, CRC(6fd60be9) SHA1(87622dc2967842629e90a02b415bec86cc26cbc7) )
	ROM_CONTINUE(         0x10000, 0x14000             )

	ROM_REGION( 0x200000, "gfx1", 0 )	/* Background */
	ROM_LOAD( "yunsung8.04",  0x000000, 0x80000, CRC(0a100d2b) SHA1(c36a2489748c8ac7b6d7457ad09d8153707c85be) )
	ROM_LOAD( "yunsung8.03",  0x080000, 0x80000, CRC(c8cb0373) SHA1(339c4e0fef44da3cab615e07dc8739bd925ebf28) )
	ROM_LOAD( "yunsung8.02",  0x100000, 0x80000, CRC(09efb8e5) SHA1(684bb5c4b579f8c77e79aab4decbefea495d9474) )
	ROM_LOAD( "yunsung8.01",  0x180000, 0x80000, CRC(4590d782) SHA1(af875166207793572b9ecf01bb6a24feba562a96) )

	ROM_REGION( 0x40000, "gfx2", 0 )	/* Text */
	ROM_LOAD( "yunsung8.05", 0x00000, 0x20000, CRC(862d378c) SHA1(a4e2cf14b5b25c6b8725dd285ddea65ce9ee257a) )	// only first $8000 bytes != 0
	ROM_LOAD( "yunsung8.06", 0x20000, 0x20000, CRC(8b2ab901) SHA1(1a5c05dd0cf830b645357a62d8e6e876b44c6b7f) )	// only first $8000 bytes != 0

ROM_END
#endif

/***************************************************************************

                                Cannon Ball

01, 02, 03, 04  are 27c020
05, 06, 07, 08  are 27c010
2 pals used

Z80b PROGRAM, Z80b SOUND

Cy7c384A
16MHz

***************************************************************************/

ROM_START( cannball )

	ROM_REGION( 0x24000, "maincpu", 0 )		/* Main Z80 Code */
	ROM_LOAD( "cannball.07", 0x00000, 0x0c000, CRC(17db56b4) SHA1(032e3dbde0b0e315dcb5f2b31f57e75e78818f2d) )
	ROM_CONTINUE(            0x10000, 0x14000             )

	ROM_REGION( 0x24000, "audiocpu", 0 )		/* Sound Z80 Code */
	ROM_LOAD( "cannball.08", 0x00000, 0x0c000, CRC(11403875) SHA1(9f583bc4f08e7aef3fd0f3fe3f31cce1d226641a) )
	ROM_CONTINUE(            0x10000, 0x14000             )

	ROM_REGION( 0x100000, "gfx1", 0 )	/* Background */
	ROM_LOAD( "cannball.01",  0x000000, 0x40000, CRC(2d7785e4) SHA1(9911354c0be192506f8bfca3e85ede0bbc4828d5) )
	ROM_LOAD( "cannball.02",  0x040000, 0x40000, CRC(24df387e) SHA1(5f4afe11feb367ca3b3c4f5eb37a6b6c4edb83bb) )
	ROM_LOAD( "cannball.03",  0x080000, 0x40000, CRC(4d62f192) SHA1(8c60b9b4b36c13c2d145c49413580a10e71eb283) )
	ROM_LOAD( "cannball.04",  0x0c0000, 0x40000, CRC(37cf8b12) SHA1(f93df8e0babe2c4ec996aa3c2a48bf40a5a02e62) )

	ROM_REGION( 0x40000, "gfx2", 0 )	/* Text */
	ROM_LOAD( "cannball.05", 0x00000, 0x20000, CRC(87c1f1fa) SHA1(dbc568d2133734e41b69fd8d18b76531648b32ef) )
	ROM_LOAD( "cannball.06", 0x20000, 0x20000, CRC(e722bee8) SHA1(3aed7df9df81a6776b6bf2f5b167965b0d689216) )

ROM_END

#if 0
ROM_START( cannballv )

	ROM_REGION( 0x24000, "maincpu", 0 )		/* Main Z80 Code */
	ROM_LOAD( "yunsung1", 0x00000, 0x0c000, CRC(f7398b0d) SHA1(f2cdb9c4662cd325376d25ae9611f689605042db) )
	ROM_CONTINUE(            0x10000, 0x14000             )

	ROM_REGION( 0x24000, "audiocpu", 0 )		/* Sound Z80 Code */
	ROM_LOAD( "yunsung8", 0x00000, 0x0c000, CRC(11403875) SHA1(9f583bc4f08e7aef3fd0f3fe3f31cce1d226641a) )
	ROM_CONTINUE(            0x10000, 0x14000             )

	ROM_REGION( 0x200000, "gfx1", 0 )	/* Background */
	ROM_LOAD( "yunsung7",  0x000000, 0x80000, CRC(a5f1a648) SHA1(7a5bf5bc0ad257ccb12104512e98dfb3525babfc) )
	ROM_LOAD( "yunsung6",  0x080000, 0x80000, CRC(8baa686e) SHA1(831c3e2864d262bf5429dca6653c83dc976e610e) )
	ROM_LOAD( "yunsung5",  0x100000, 0x80000, CRC(a7f2ce51) SHA1(81632aca067f2c8c45488266c4489d9af24fb552) )
	ROM_LOAD( "yunsung4",  0x180000, 0x80000, CRC(74bef793) SHA1(6208580ce747cec3d410ce3c71e07aa570b9121d) )

	ROM_REGION( 0x40000, "gfx2", 0 )	/* Text */
	ROM_LOAD( "yunsung3", 0x00000, 0x20000, CRC(8217abbe) SHA1(1a459a816a1aa5b68858e39c4a21bd78ee78dcab) )
	ROM_LOAD( "yunsung2", 0x20000, 0x20000, CRC(76de1045) SHA1(a3845ee1874e6ec0ce26e6e73e4643243779e70d) )

ROM_END
#endif

/***************************************************************************

Rock Tris by Yunsung

Pcb marked Ys-rock 940712

pcb has no sound, the osc for the msm5205 has been cut so I don't know the frequency

Pcb has no markings in the sockets and eproms have anonymous stickers

2x z80
1x 16mhz
1x UA010
1x msm5205
1x fpga
1x dipswitch (there is place for another one, but the space is empty and
they jumpered the first position)

***************************************************************************/
#if 0
ROM_START( rocktris )

	ROM_REGION( 0x24000, "maincpu", 0 )		/* Main Z80 Code */
	ROM_LOAD( "cpu.bin",     0x00000, 0x0c000, CRC(46e3b79c) SHA1(81a587b9f986c4e39b1888ec6ed6b86d1469b9a0) )
	ROM_CONTINUE(         0x10000, 0x14000             )

	ROM_REGION( 0x24000, "audiocpu", 0 )		/* Sound Z80 Code */
	ROM_LOAD( "cpu2.bin",    0x00000, 0x0c000, CRC(3a78a4cf) SHA1(f643c7a217cbb71f3a03f1f4a16545c546332819) )
	ROM_CONTINUE(         0x10000, 0x14000             )

	ROM_REGION( 0x200000, "gfx1", 0 )	/* Background */
	ROM_LOAD( "gfx4.bin",     0x000000, 0x80000, CRC(abb49cac) SHA1(e2d766e950df398a8ec8b6888e128ffc3bdf1ce9) )
	ROM_LOAD( "gfx3.bin",     0x080000, 0x80000, CRC(70a6ad52) SHA1(04cd58d3f885dd7c2fb1061f93d3ae3a418ad762) )
	ROM_LOAD( "gfx2.bin",     0x100000, 0x80000, CRC(fcc9ec97) SHA1(1f09452988e3fa976b233e3b458c7a60977b76aa) )
	ROM_LOAD( "gfx1.bin",     0x180000, 0x80000, CRC(4295034d) SHA1(9bdbbcdb46eb659a13b77c5bb26c9d8ad43731a7) )

	ROM_REGION( 0x40000, "gfx2", 0 )	/* Text */
	ROM_LOAD( "gfx5.bin",     0x00000, 0x20000, CRC(058ee379) SHA1(57088bb02c56212979b9119b773eedc31af17e50) )
	ROM_LOAD( "gfx6.bin",     0x20000, 0x20000, CRC(593cbd39) SHA1(4d60b5811118f3f22f6f3b300a4daec158456b72) )

ROM_END
#endif

/***************************************************************************


                                Game Drivers


***************************************************************************/

GAME( 1995,  cannball, 0,        yunsung8, cannball, 0, ROT0,   "Yun Sung / Soft Vision", "Cannon Ball (Yun Sung) (horizontal)",  GAME_IMPERFECT_SOUND )
//GAME( 1995,  cannballv,cannball, yunsung8, cannbalv, 0, ROT270, "Yun Sung / T&K",         "Cannon Ball (Yun Sung) (vertical)",  GAME_IMPERFECT_SOUND )
//GAME( 1995,  magix,    0,        yunsung8, magix,    0, ROT0,   "Yun Sung",               "Magix / Rock", GAME_IMPERFECT_SOUND ) // Title: DSW
//GAME( 1994?, rocktris, 0,        yunsung8, rocktris, 0, ROT0,   "Yun Sung",               "Rock Tris",    GAME_IMPERFECT_SOUND )

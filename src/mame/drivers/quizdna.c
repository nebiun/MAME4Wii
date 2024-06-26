/*****************************************************************************

Quiz DNA no Hanran (c) 1992 Face
Quiz Gakuen Paradise (c) 1991 NMK
Quiz Gekiretsu Scramble (Gakuen Paradise 2) (c) 1993 Face

    Driver by Uki

*****************************************************************************/
#if 0

#include "driver.h"
#include "cpu/z80/z80.h"
#include "sound/2203intf.h"
#include "sound/okim6295.h"

#define MCLK 16000000

VIDEO_START( quizdna );
VIDEO_UPDATE( quizdna );

WRITE8_HANDLER( quizdna_fg_ram_w );
WRITE8_HANDLER( quizdna_bg_ram_w );
WRITE8_HANDLER( quizdna_bg_yscroll_w );
WRITE8_HANDLER( quizdna_bg_xscroll_w );
WRITE8_HANDLER( quizdna_screen_ctrl_w );

WRITE8_HANDLER( paletteram_xBGR_RRRR_GGGG_BBBB_w );


static WRITE8_HANDLER( quizdna_rombank_w )
{
	UINT8 *ROM = memory_region(space->machine, "maincpu");
	memory_set_bankptr(space->machine, 1,&ROM[0x10000+0x4000*(data & 0x3f)]);
}

static WRITE8_HANDLER( gekiretu_rombank_w )
{
	UINT8 *ROM = memory_region(space->machine, "maincpu");
	memory_set_bankptr(space->machine, 1,&ROM[0x10000+0x4000*((data & 0x3f) ^ 0x0a)]);
}

/****************************************************************************/

static ADDRESS_MAP_START( quizdna_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x7fff) AM_ROM
	AM_RANGE(0x8000, 0xbfff) AM_ROMBANK(1)
	AM_RANGE(0x8000, 0x9fff) AM_WRITE(quizdna_fg_ram_w)
	AM_RANGE(0xa000, 0xbfff) AM_WRITE(quizdna_bg_ram_w)
	AM_RANGE(0xc000, 0xdfff) AM_RAM
	AM_RANGE(0xe000, 0xe1ff) AM_RAM AM_BASE(&spriteram) AM_SIZE(&spriteram_size)
	AM_RANGE(0xe200, 0xefff) AM_RAM
	AM_RANGE(0xf000, 0xffff) AM_RAM_WRITE(paletteram_xBGR_RRRR_GGGG_BBBB_w) AM_BASE(&paletteram)
ADDRESS_MAP_END

static ADDRESS_MAP_START( gekiretu_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x7fff) AM_ROM
	AM_RANGE(0x8000, 0xbfff) AM_ROMBANK(1)
	AM_RANGE(0x8000, 0x9fff) AM_WRITE(quizdna_fg_ram_w)
	AM_RANGE(0xa000, 0xbfff) AM_WRITE(quizdna_bg_ram_w)
	AM_RANGE(0xc000, 0xdfff) AM_RAM
	AM_RANGE(0xe000, 0xefff) AM_RAM_WRITE(paletteram_xBGR_RRRR_GGGG_BBBB_w) AM_BASE(&paletteram)
	AM_RANGE(0xf000, 0xf1ff) AM_RAM AM_BASE(&spriteram) AM_SIZE(&spriteram_size)
	AM_RANGE(0xf200, 0xffff) AM_RAM
ADDRESS_MAP_END

static ADDRESS_MAP_START( quizdna_io_map, ADDRESS_SPACE_IO, 8 )
	ADDRESS_MAP_GLOBAL_MASK(0xff)
	AM_RANGE(0x02, 0x03) AM_WRITE(quizdna_bg_xscroll_w)
	AM_RANGE(0x04, 0x04) AM_WRITE(quizdna_bg_yscroll_w)
	AM_RANGE(0x05, 0x06) AM_WRITENOP /* unknown */
	AM_RANGE(0x80, 0x80) AM_READ_PORT("P1")
	AM_RANGE(0x81, 0x81) AM_READ_PORT("P2")
	AM_RANGE(0x90, 0x90) AM_READ_PORT("SYSTEM")
	AM_RANGE(0x91, 0x91) AM_READ_PORT("SERVICE")
	AM_RANGE(0xc0, 0xc0) AM_WRITE(quizdna_rombank_w)
	AM_RANGE(0xd0, 0xd0) AM_WRITE(quizdna_screen_ctrl_w)
	AM_RANGE(0xe0, 0xe1) AM_DEVREADWRITE("ym", ym2203_r, ym2203_w)
	AM_RANGE(0xf0, 0xf0) AM_DEVREADWRITE("oki", okim6295_r, okim6295_w)
ADDRESS_MAP_END

static ADDRESS_MAP_START( gakupara_io_map, ADDRESS_SPACE_IO, 8 )
	ADDRESS_MAP_GLOBAL_MASK(0xff)
	AM_RANGE(0x00, 0x01) AM_WRITE(quizdna_bg_xscroll_w)
	AM_RANGE(0x02, 0x02) AM_WRITE(quizdna_bg_yscroll_w)
	AM_RANGE(0x03, 0x04) AM_WRITENOP /* unknown */
	AM_RANGE(0x80, 0x80) AM_READ_PORT("P1")
	AM_RANGE(0x81, 0x81) AM_READ_PORT("P2")
	AM_RANGE(0x90, 0x90) AM_READ_PORT("SYSTEM")
	AM_RANGE(0x91, 0x91) AM_READ_PORT("SERVICE")
	AM_RANGE(0xc0, 0xc0) AM_WRITE(quizdna_rombank_w)
	AM_RANGE(0xd0, 0xd0) AM_WRITE(quizdna_screen_ctrl_w)
	AM_RANGE(0xe0, 0xe1) AM_DEVREADWRITE("ym", ym2203_r, ym2203_w)
	AM_RANGE(0xf0, 0xf0) AM_DEVREADWRITE("oki", okim6295_r, okim6295_w)
ADDRESS_MAP_END

static ADDRESS_MAP_START( gekiretu_io_map, ADDRESS_SPACE_IO, 8 )
	ADDRESS_MAP_GLOBAL_MASK(0xff)
	AM_RANGE(0x02, 0x03) AM_WRITE(quizdna_bg_xscroll_w)
	AM_RANGE(0x04, 0x04) AM_WRITE(quizdna_bg_yscroll_w)
	AM_RANGE(0x05, 0x06) AM_WRITENOP /* unknown */
	AM_RANGE(0x80, 0x80) AM_READ_PORT("P1")
	AM_RANGE(0x81, 0x81) AM_READ_PORT("P2")
	AM_RANGE(0x90, 0x90) AM_READ_PORT("SYSTEM")
	AM_RANGE(0x91, 0x91) AM_READ_PORT("SERVICE")
	AM_RANGE(0xc0, 0xc0) AM_WRITE(gekiretu_rombank_w)
	AM_RANGE(0xd0, 0xd0) AM_WRITE(quizdna_screen_ctrl_w)
	AM_RANGE(0xe0, 0xe1) AM_DEVREADWRITE("ym", ym2203_r, ym2203_w)
	AM_RANGE(0xf0, 0xf0) AM_DEVREADWRITE("oki", okim6295_r, okim6295_w)
ADDRESS_MAP_END


/****************************************************************************/

static INPUT_PORTS_START( quizdna )
	PORT_START("DSW2")
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 9C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 8C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 7C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Unknown 2-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START("DSW3")
	PORT_DIPNAME( 0x03, 0x02, "Timer" )
	PORT_DIPSETTING(    0x03, "Slow" )
	PORT_DIPSETTING(    0x02, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x01, "Fast" )
	PORT_DIPSETTING(    0x00, "Very Fast" )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "1" )
	PORT_DIPSETTING(    0x04, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x30, "Every 500k" )
	PORT_DIPSETTING(    0x20, "Every 1000k" )
	PORT_DIPSETTING(    0x10, "Every 2000k" )
	PORT_DIPSETTING(    0x00, DEF_STR( None ) )
	PORT_DIPNAME( 0x40, 0x40, "Carat" )
	PORT_DIPSETTING(    0x40, "20" )
	PORT_DIPSETTING(    0x00, "0" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Allow_Continue ) )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Yes ) )

	PORT_START("P1")
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("P2")
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(2)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 ) PORT_PLAYER(2)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("SYSTEM")
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START("SERVICE")
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

static INPUT_PORTS_START( gakupara )
    PORT_START("DSW2")
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, "10 Coins/1 Credit" )
	PORT_DIPSETTING(    0x01, DEF_STR( 9C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 8C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 7C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x10, 0x00, "Demo Music" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0x80, "Timer" )
	PORT_DIPSETTING(    0xc0, "Slow" )
	PORT_DIPSETTING(    0x80, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x40, "Fast" )
	PORT_DIPSETTING(    0x00, "Very Fast" )

	PORT_START("DSW3")
	PORT_SERVICE( 0x01, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x02, 0x02, "Unknown 3-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Unknown 3-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Unknown 3-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Unknown 3-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Unknown 3-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Unknown 3-7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Unknown 3-8" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START("P1")
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("P2")
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(2)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 ) PORT_PLAYER(2)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("SYSTEM")
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START("SERVICE")
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

static INPUT_PORTS_START( gekiretu )
	PORT_START("DSW2")
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 9C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 8C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 7C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Unknown 2-7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START("DSW3")
	PORT_DIPNAME( 0x03, 0x03, "Timer" )
	PORT_DIPSETTING(    0x03, "Slow" )
	PORT_DIPSETTING(    0x02, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x01, "Fast" )
	PORT_DIPSETTING(    0x00, "Very Fast" )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x04, "1" )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x10, 0x10, "Unknown 3-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Unknown 3-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Unknown 3-7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Allow_Continue ) )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Yes ) )

	PORT_START("P1")
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("P2")
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(2)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4 ) PORT_PLAYER(2)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("SYSTEM")
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START("SERVICE")
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

/****************************************************************************/

static const gfx_layout fglayout =
{
	16,8,     /* 16*8 characters */
	8192*2,   /* 16384 characters */
	1,        /* 1 bit per pixel */
	{0},
	{ STEP16(0,1) },
	{ STEP8(0,16) },
	16*8
};

static const gfx_layout bglayout =
{
	8,8,        /* 8*8 characters */
	32768+1024, /* 32768+1024 characters */
	4,          /* 4 bits per pixel */
	{0,1,2,3},
	{ STEP8(0,4) },
	{ STEP8(0,32) },
	8*8*4
};

static const gfx_layout objlayout =
{
	16,16,    /* 16*16 characters */
	8192+256, /* 8192+256 characters */
	4,        /* 4 bits per pixel */
	{0,1,2,3},
	{ STEP16(0,4) },
	{ STEP16(0,64) },
	16*16*4
};

static GFXDECODE_START( quizdna )
	GFXDECODE_ENTRY( "gfx1", 0x0000, fglayout,  0x7e0,  16 )
	GFXDECODE_ENTRY( "gfx2", 0x0000, bglayout,  0x000, 128 )
	GFXDECODE_ENTRY( "gfx3", 0x0000, objlayout, 0x600,  32 )
GFXDECODE_END


static const ym2203_interface ym2203_config =
{
	{
		AY8910_LEGACY_OUTPUT,
		AY8910_DEFAULT_LOADS,
		DEVCB_INPUT_PORT("DSW3"),
		DEVCB_INPUT_PORT("DSW2"),
		DEVCB_NULL,
		DEVCB_NULL
	},
	NULL
};


static MACHINE_DRIVER_START( quizdna )

	/* basic machine hardware */
	MDRV_CPU_ADD("maincpu", Z80, MCLK/2) /* 8.000 MHz */
	MDRV_CPU_PROGRAM_MAP(quizdna_map)
	MDRV_CPU_IO_MAP(quizdna_io_map)
	MDRV_CPU_VBLANK_INT("screen", irq0_line_hold)

	/* video hardware */
	MDRV_SCREEN_ADD("screen", RASTER)
	MDRV_SCREEN_REFRESH_RATE(60)
	MDRV_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(2500) /* not accurate */)
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_SIZE(64*8, 32*8)
	MDRV_SCREEN_VISIBLE_AREA(8*8, 56*8-1, 2*8, 30*8-1)

	MDRV_GFXDECODE(quizdna)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_START(quizdna)
	MDRV_VIDEO_UPDATE(quizdna)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")

	MDRV_SOUND_ADD("ym", YM2203, MCLK/4)
	MDRV_SOUND_CONFIG(ym2203_config)
	MDRV_SOUND_ROUTE(0, "mono", 0.10)
	MDRV_SOUND_ROUTE(1, "mono", 0.10)
	MDRV_SOUND_ROUTE(2, "mono", 0.10)
	MDRV_SOUND_ROUTE(3, "mono", 0.40)

	MDRV_SOUND_ADD("oki", OKIM6295, (MCLK/1024)*132)
	MDRV_SOUND_CONFIG(okim6295_interface_pin7high) // clock frequency & pin 7 not verified
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.30)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( gakupara )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(quizdna)

	MDRV_CPU_MODIFY("maincpu")
	MDRV_CPU_IO_MAP(gakupara_io_map)

MACHINE_DRIVER_END

static MACHINE_DRIVER_START( gekiretu )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(quizdna)

	MDRV_CPU_MODIFY("maincpu")
	MDRV_CPU_PROGRAM_MAP(gekiretu_map)
	MDRV_CPU_IO_MAP(gekiretu_io_map)

MACHINE_DRIVER_END


/****************************************************************************/

ROM_START( quizdna )
	ROM_REGION( 0xd0000, "maincpu", 0 ) /* CPU */
	ROM_LOAD( "quiz2-pr.28",  0x00000,  0x08000, CRC(a428ede4) SHA1(cdca3bd84b2ea421fb05502ea29e9eb605e574eb) )
	ROM_CONTINUE(             0x18000,  0x78000 ) /* banked */
	/* empty */

	ROM_REGION( 0x40000, "gfx1", 0 ) /* fg */
	ROM_LOAD( "quiz2.102",    0x00000,  0x20000, CRC(62402ac9) SHA1(bf52d22b119d54410dad4949b0687bb0edf3e143) )
	/* empty */

	ROM_REGION( 0x108000, "gfx2", 0 ) /* bg */
	ROM_LOAD( "quiz2-bg.100", 0x000000,  0x100000, CRC(f1d0cac2) SHA1(26d25c1157d1916dfe4496c6cf119c4a9339e31c) )
	/* empty */

	ROM_REGION( 0x108000, "gfx3", 0 ) /* obj */
	ROM_LOAD( "quiz2-ob.98",  0x000000,  0x100000, CRC(682f19a6) SHA1(6b8e6e583f423cf8ef9095f2c300201db7d7b8b3) )
	ROM_LOAD( "quiz2ob2.97",  0x100000,  0x008000, CRC(03736b1a) SHA1(bc42ac293260f58a8a138702d890f69aec99c05e) )

	ROM_REGION( 0x80000, "oki", 0 ) /* samples */
	ROM_LOAD( "quiz2-sn.32",  0x000000,  0x040000, CRC(1c044637) SHA1(dc749295e149f968495272f1a3ec27c6b719be8e) )

	ROM_REGION( 0x00020, "user1", 0 ) /* fg control */
	ROM_LOAD( "quiz2.148",    0x000000,  0x000020, CRC(91267e8a) SHA1(ae5bd8efea5322c4d9986d06680a781392f9a642) )
ROM_END

ROM_START( gakupara )
	ROM_REGION( 0xd0000, "maincpu", 0 ) /* CPU */
	ROM_LOAD( "u28.bin",  0x00000,  0x08000, CRC(72124bb8) SHA1(e734acff7e9d6b8c6a95c76860732320a2e3a828) )
	ROM_CONTINUE(         0x18000,  0x78000 )             /* banked */
	ROM_LOAD( "u29.bin",  0x90000,  0x40000, CRC(09f4948e) SHA1(21ccf5af6935cf40c0cf73fbee14bff3c4e1d23d) ) /* banked */

	ROM_REGION( 0x40000, "gfx1", 0 ) /* fg */
	ROM_LOAD( "u102.bin", 0x00000,  0x20000, CRC(62402ac9) SHA1(bf52d22b119d54410dad4949b0687bb0edf3e143) )
	ROM_LOAD( "u103.bin", 0x20000,  0x20000, CRC(38644251) SHA1(ebfdc43c38e1380709ed08575c346b2467ad1592) )

	ROM_REGION( 0x108000, "gfx2", 0 ) /* bg */
	ROM_LOAD( "u100.bin", 0x000000,  0x100000, CRC(f9d886ea) SHA1(d7763f54a165af720216b96e601a66fbc59e3568) )
	ROM_LOAD( "u99.bin",  0x100000,  0x008000, CRC(ac224d0a) SHA1(f187c3b74bc18606d0fe638f6a829f71c109998d) )

	ROM_REGION( 0x108000, "gfx3", 0 ) /* obj */
	ROM_LOAD( "u98.bin",  0x000000,  0x100000, CRC(a6e8cb56) SHA1(2fc85c1769513cc7aa5e23afaf0c99c38de9b855) )
	ROM_LOAD( "u97.bin",  0x100000,  0x008000, CRC(9dacd5c9) SHA1(e40211059e71408be3d67807463304f4d4ecae37) )

	ROM_REGION( 0x80000, "oki", 0 ) /* samples */
	ROM_LOAD( "u32.bin",  0x000000,  0x040000, CRC(eb03c535) SHA1(4d6c749ccab4681eee0a1fb243e9f3dbe61b9f94) )

	ROM_REGION( 0x00020, "user1", 0 ) /* fg control */
	ROM_LOAD( "u148.bin", 0x000000,  0x000020, CRC(971df9d2) SHA1(280f5b386922b9902ca9211c719642c2bd0ba899) )
ROM_END

ROM_START( gekiretu )
	ROM_REGION( 0xd0000, "maincpu", 0 ) /* CPU */
	ROM_LOAD( "quiz3-pr.28",  0x00000,  0x08000, CRC(a761e86f) SHA1(85331ef53598491e78c2d123b1ebd358aff46436) )
	ROM_CONTINUE(             0x18000,  0x78000 ) /* banked */
	/* empty */

	ROM_REGION( 0x40000, "gfx1", 0 ) /* fg */
	ROM_LOAD( "quiz3.102",    0x00000,  0x20000, CRC(62402ac9) SHA1(bf52d22b119d54410dad4949b0687bb0edf3e143) )
	/* empty */

	ROM_REGION( 0x108000, "gfx2", 0 ) /* bg */
	ROM_LOAD( "quiz3-bg.100", 0x000000,  0x100000, CRC(cb9272fd) SHA1(cfc1ff93d1fdc7d144e161a77e534cea75d7f181) )
	/* empty */

	ROM_REGION( 0x108000, "gfx3", 0 ) /* obj */
	ROM_LOAD( "quiz3-ob.98",  0x000000,  0x100000, CRC(01bed020) SHA1(5cc56c8823ee5e538371debe1cbeb57c4976677b) )
	/* empty */

	ROM_REGION( 0x80000, "oki", 0 ) /* samples */
	ROM_LOAD( "quiz3-sn.32",  0x000000,  0x040000, CRC(36dca582) SHA1(2607602e942244cfaae931da2ad36da9a8f855f7) )

	ROM_REGION( 0x00020, "user1", 0 ) /* fg control */
	ROM_LOAD( "quiz3.148",    0x000000,  0x000020, CRC(91267e8a) SHA1(ae5bd8efea5322c4d9986d06680a781392f9a642) )
ROM_END
#endif


//GAME( 1991, gakupara, 0, gakupara, gakupara, 0, ROT0, "NMK",  "Quiz Gakuen Paradise (Japan)", 0 )
//GAME( 1992, quizdna,  0, quizdna,  quizdna,  0, ROT0, "Face", "Quiz DNA no Hanran (Japan)", 0 )
//GAME( 1992, gekiretu, 0, gekiretu, gekiretu, 0, ROT0, "Face", "Quiz Gekiretsu Scramble (Japan)", 0 )

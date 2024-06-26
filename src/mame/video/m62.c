/***************************************************************************

Video Hardware for Irem Games:
Battle Road, Lode Runner, Kid Niki, Spelunker

Tile/sprite priority system (for the Kung Fu Master M62 board):
- Tiles with color code >= N (where N is set by jumpers) have priority over
  sprites. Only bits 1-4 of the color code are used, bit 0 is ignored.

- Two jumpers select whether bit 5 of the sprite color code should be used
  to index the high address pin of the color PROMs, or to select high
  priority over tiles (or both, but is this used by any game?)

***************************************************************************/

#include "driver.h"
#include "m62.h"
#include "video/resnet.h"

UINT8 *m62_tileram;
UINT8 *m62_textram;
UINT8 *horizon_scrollram;

static tilemap *m62_background;
static tilemap *m62_foreground;
static UINT8 flipscreen;
static const UINT8 *sprite_height_prom;
static INT32 m62_background_hscroll;
static INT32 m62_background_vscroll;

static int ldrun3_topbottom_mask;

static UINT8 *irem_textram;
static size_t irem_textram_size;

static UINT8 kidniki_background_bank;
static INT32 kidniki_text_vscroll;

static INT32 spelunkr_palbank;



/***************************************************************************

  Convert the color PROMs into a more useable format.

  Kung Fu Master has a six 256x4 palette PROMs (one per gun; three for
  characters, three for sprites).
  I don't know the exact values of the resistors between the RAM and the
  RGB output. I assumed these values (the same as Commando)

  bit 3 -- 220 ohm resistor  -- RED/GREEN/BLUE
        -- 470 ohm resistor  -- RED/GREEN/BLUE
        -- 1  kohm resistor  -- RED/GREEN/BLUE
  bit 0 -- 2.2kohm resistor  -- RED/GREEN/BLUE

  The resistor values could be verified on a hires pcb picture (spelunkr).
  The schematics also exhibit one pulldown for the sprite color guns.
  Since only either the sprite or tile gun is active, i.e. the other is
  in tri-state, this pulldown resistor also applies to the tile color guns.

  Which of the sprite/tilemap color guns is in tri-state mode is determined
  by a pal. There is no good dump of this pal. If both sprite and tilemap
  should be active (i.e. not in tristate) at a time, this would due to the
  pulldown resistor have slightly darker colors as a consequence.

  This can explain the spelunkr bug m62_0116u4gre.

  The kidnike bug kidniki0104u3gre even looks worse and may imho only explained
  by subpixel effects, i.e. delays when switching the proms into tristate.

  Since there are no dumps of the "priority" pal, the above is
  speculation.

  Priority PAL

  The PAL is at location 4F on the "T" board. The PAL's input are 3 bits
  from tilemap 0, 3 bits from tilemap 1 and A11-A14:

  CB0A =>   7     20  GND
  CB1A =>   8?    14  ==> PROM CS
  CB2A =>   9  P  19  ==> Tilemap select 0/1 / to connector
  C14A =>  12  A  11  <== Sprite priority out / to connector
  C13A =>   4  L  18? ==> Sprite priority in / to connector
  C12A =>   5     10  GND
  C11A =>   6
  CB0D =>   1
  CB1D =>   2
  CB2D =>   3

***************************************************************************/


static const res_net_info m62_tile_net_info =
{
	RES_NET_VCC_5V | RES_NET_VIN_TTL_OUT,
	{
		{ RES_NET_AMP_NONE, 0, 470, 4, { 2200, 1000, 470, 220 } },
		{ RES_NET_AMP_NONE, 0, 470, 4, { 2200, 1000, 470, 220 } },
		{ RES_NET_AMP_NONE, 0, 470, 4, { 2200, 1000, 470, 220 } }
	}
};


static const res_net_info m62_sprite_net_info =
{
	RES_NET_VCC_5V | RES_NET_VIN_TTL_OUT,
	{
		{ RES_NET_AMP_NONE, 0, 470, 4, { 2200, 1000, 470, 220 } },
		{ RES_NET_AMP_NONE, 0, 470, 4, { 2200, 1000, 470, 220 } },
		{ RES_NET_AMP_NONE, 0, 470, 4, { 2200, 1000, 470, 220 } }
	}
};

#if 0
/* this is a complete guess */
static const res_net_info battroad_char_net_info =
{
	RES_NET_VCC_5V | RES_NET_VIN_TTL_OUT,
	{
		{ RES_NET_AMP_NONE, 0, 0, 3, { 1000, 470, 220 } },
		{ RES_NET_AMP_NONE, 0, 0, 3, { 1000, 470, 220 } },
		{ RES_NET_AMP_NONE, 0, 0, 2, {       470, 220 } }
	}
};
#endif


static const res_net_decode_info m62_tile_decode_info =
{
	1,					/* single PROM per color */
	0x000, 0x0ff,		/* start/end */
	/*  R      G      B */
	{ 0x000, 0x200, 0x400 }, /* offsets */
	{     0,     0,     0 }, /* shifts */
	{  0x0f,  0x0f,  0x0f }  /* masks */
};


static const res_net_decode_info m62_sprite_decode_info =
{
	1,					/* single PROM per color */
	0x000, 0x0ff,		/* start/end */
	/*  R      G      B */
	{ 0x100, 0x300, 0x500 }, /* offsets */
	{     0,     0,     0 }, /* shifts */
	{  0x0f,  0x0f,  0x0f }  /* masks */
};


static const res_net_decode_info lotlot_tile_decode_info =
{
	1,					/* single PROM per color */
	0x000, 0x17f,		/* start/end */
	/*  R      G      B */
	{ 0x000, 0x300, 0x600 }, /* offsets */
	{     0,     0,     0 }, /* shifts */
	{  0x0f,  0x0f,  0x0f }  /* masks */
};


static const res_net_decode_info lotlot_sprite_decode_info =
{
	1,					/* single PROM per color */
	0x000, 0x17f,		/* start/end */
	/*  R      G      B */
	{ 0x180, 0x480, 0x780 }, /* offsets */
	{     0,     0,     0 }, /* shifts */
	{  0x0f,  0x0f,  0x0f }  /* masks */
};


static const res_net_decode_info battroad_char_decode_info =
{
	1,					/* single PROM per color */
	0x000, 0x01f,		/* start/end */
	/*  R      G      B */
	{ 0x600, 0x600, 0x600 }, /* offsets */
	{     0,     3,     6 }, /* shifts */
	{  0x07,  0x07,  0x03 }  /* masks */
};


static const res_net_decode_info spelunk2_tile_decode_info =
{
	1,					/* single PROM per color */
	0x000, 0x1ff,		/* start/end */
	/*  R      G      B */
	{ 0x000, 0x000, 0x200 }, /* offsets */
	{     0,     4,     0 }, /* shifts */
	{  0x0f,  0x0f,  0x0f }  /* masks */
};


static const res_net_decode_info spelunk2_sprite_decode_info =
{
	1,					/* single PROM per color */
	0x000, 0x0ff,		/* start/end */
	/*  R      G      B */
	{ 0x400, 0x500, 0x600 }, /* offsets */
	{     0,     0,     0 }, /* shifts */
	{  0x0f,  0x0f,  0x0f }  /* masks */
};


PALETTE_INIT( m62 )
{
	rgb_t *rgb;

	rgb = compute_res_net_all(color_prom, &m62_tile_decode_info, &m62_tile_net_info);
	palette_set_colors(machine, 0x000, rgb, 0x100);
	free(rgb);

	rgb = compute_res_net_all(color_prom, &m62_sprite_decode_info, &m62_sprite_net_info);
	palette_set_colors(machine, 0x100, rgb, 0x100);
	free(rgb);

	palette_normalize_range(machine->palette, 0x000, 0x1ff, 0x00, 0xff);

	/* we'll need this at run time */
	sprite_height_prom = color_prom + 0x600;
}


PALETTE_INIT( lotlot )
{
	rgb_t *rgb;

	rgb = compute_res_net_all(color_prom, &lotlot_tile_decode_info, &m62_tile_net_info);
	palette_set_colors(machine, 0x000, rgb, 0x180);
	free(rgb);

	rgb = compute_res_net_all(color_prom, &lotlot_sprite_decode_info, &m62_sprite_net_info);
	palette_set_colors(machine, 0x180, rgb, 0x180);
	free(rgb);

	palette_normalize_range(machine->palette, 0x000, 0x2ff, 0x00, 0xff);

	/* we'll need this at run time */
	sprite_height_prom = color_prom + 0x900;
}


PALETTE_INIT( battroad )
{
	rgb_t *rgb;

	rgb = compute_res_net_all(color_prom, &m62_tile_decode_info, &m62_tile_net_info);
	palette_set_colors(machine, 0x000, rgb, 0x100);
	free(rgb);

	rgb = compute_res_net_all(color_prom, &m62_sprite_decode_info, &m62_sprite_net_info);
	palette_set_colors(machine, 0x100, rgb, 0x100);
	free(rgb);

	rgb = compute_res_net_all(color_prom, &battroad_char_decode_info, &m62_tile_net_info);
	palette_set_colors(machine, 0x200, rgb, 0x020);
	free(rgb);

	palette_normalize_range(machine->palette, 0x000, 0x21f, 0x00, 0xff);

	sprite_height_prom = color_prom + 0x620;	/* we'll need this at run time */
}


PALETTE_INIT( spelunk2 )
{
	rgb_t *rgb;

	rgb = compute_res_net_all(color_prom, &spelunk2_tile_decode_info, &m62_tile_net_info);
	palette_set_colors(machine, 0x000, rgb, 0x200);
	free(rgb);

	rgb = compute_res_net_all(color_prom, &spelunk2_sprite_decode_info, &m62_sprite_net_info);
	palette_set_colors(machine, 0x200, rgb, 0x100);
	free(rgb);

	palette_normalize_range(machine->palette, 0x000, 0x2ff, 0x00, 0xff);

	/* we'll need this at run time */
	sprite_height_prom = color_prom + 0x700;
}



static void register_savestate(running_machine *machine)
{
	state_save_register_global(machine, flipscreen);
	state_save_register_global(machine, kidniki_background_bank);
	state_save_register_global(machine, m62_background_hscroll);
	state_save_register_global(machine, m62_background_vscroll);
	state_save_register_global(machine, kidniki_text_vscroll);
	state_save_register_global(machine, spelunkr_palbank);
	state_save_register_global_pointer(machine, irem_textram, irem_textram_size);
}

WRITE8_HANDLER( m62_flipscreen_w )
{
	/* screen flip is handled both by software and hardware */
	data ^= ~input_port_read(space->machine, "DSW2") & 1;

	flipscreen = data & 0x01;
	if (flipscreen)
		tilemap_set_flip_all(space->machine, TILEMAP_FLIPX | TILEMAP_FLIPY);
	else
		tilemap_set_flip_all(space->machine, 0);

	coin_counter_w(0,data & 2);
	coin_counter_w(1,data & 4);
}

WRITE8_HANDLER( m62_hscroll_low_w )
{
	m62_background_hscroll = ( m62_background_hscroll & 0xff00 ) | data;
}

WRITE8_HANDLER( m62_hscroll_high_w )
{
	m62_background_hscroll = ( m62_background_hscroll & 0xff ) | ( data << 8 );
}

WRITE8_HANDLER( m62_vscroll_low_w )
{
	m62_background_vscroll = ( m62_background_vscroll & 0xff00 ) | data;
}

WRITE8_HANDLER( m62_vscroll_high_w )
{
	m62_background_vscroll = ( m62_background_vscroll & 0xff ) | ( data << 8 );
}

WRITE8_HANDLER( m62_tileram_w )
{
	m62_tileram[ offset ] = data;
	tilemap_mark_tile_dirty( m62_background, offset >> 1 );
}

WRITE8_HANDLER( m62_textram_w )
{
	m62_textram[ offset ] = data;
	tilemap_mark_tile_dirty( m62_foreground, offset >> 1 );
}


static void draw_sprites(running_machine *machine, bitmap_t *bitmap, const rectangle *cliprect, int colormask, int prioritymask, int priority)
{
	int offs;

	for (offs = 0;offs < spriteram_size;offs += 8)
	{
		int i,incr,code,col,flipx,flipy,sx,sy;

		if( ( spriteram[offs] & prioritymask ) == priority )
		{
			code = spriteram[offs+4] + ((spriteram[offs+5] & 0x07) << 8);
			col = spriteram[offs+0] & colormask;
			sx = 256 * (spriteram[offs+7] & 1) + spriteram[offs+6],
			sy = 256+128-15 - (256 * (spriteram[offs+3] & 1) + spriteram[offs+2]),
			flipx = spriteram[offs+5] & 0x40;
			flipy = spriteram[offs+5] & 0x80;

			i = sprite_height_prom[(code >> 5) & 0x1f];
			if (i == 1)	/* double height */
			{
				code &= ~1;
				sy -= 16;
			}
			else if (i == 2)	/* quadruple height */
			{
				i = 3;
				code &= ~3;
				sy -= 3*16;
			}

			if (flipscreen)
			{
				sx = 496 - sx;
				sy = 242 - i*16 - sy;	/* sprites are slightly misplaced by the hardware */
				flipx = !flipx;
				flipy = !flipy;
			}

			if (flipy)
			{
				incr = -1;
				code += i;
			}
			else incr = 1;

			do
			{
				drawgfx_transpen(bitmap,cliprect,machine->gfx[1],
						code + i * incr,col,
						flipx,flipy,
						sx,sy + 16 * i,0);

				i--;
			} while (i >= 0);
		}
	}
}

static void m62_start( running_machine *machine, tile_get_info_func tile_get_info, int rows, int cols, int x1, int y1, int x2, int y2 )
{
	m62_background = tilemap_create( machine, tile_get_info, tilemap_scan_rows,  x1, y1, x2, y2 );

	m62_background_hscroll = 0;
	m62_background_vscroll = 0;

	register_savestate(machine);

	if( rows != 0 )
	{
		tilemap_set_scroll_rows( m62_background, rows );
	}
	if( cols != 0 )
	{
		tilemap_set_scroll_cols( m62_background, cols );
	}
}

static void m62_textlayer( running_machine *machine, tile_get_info_func tile_get_info, int rows, int cols, int x1, int y1, int x2, int y2 )
{
	m62_foreground = tilemap_create( machine, tile_get_info, tilemap_scan_rows,  x1, y1, x2, y2 );

	if( rows != 0 )
	{
		tilemap_set_scroll_rows( m62_foreground, rows );
	}
	if( cols != 0 )
	{
		tilemap_set_scroll_cols( m62_foreground, cols );
	}
}

WRITE8_HANDLER( kungfum_tileram_w )
{
	m62_tileram[ offset ] = data;
	tilemap_mark_tile_dirty( m62_background, offset & 0x7ff );
}

static TILE_GET_INFO( get_kungfum_bg_tile_info )
{
	int code;
	int color;
	int flags;
	code = m62_tileram[ tile_index ];
	color = m62_tileram[ tile_index + 0x800 ];
	flags = 0;
	if( ( color & 0x20 ) )
	{
		flags |= TILE_FLIPX;
	}
	SET_TILE_INFO( 0, code | ( ( color & 0xc0 ) << 2 ), color & 0x1f, flags );

	/* is the following right? */
	if( ( tile_index / 64 ) < 6 || ( ( color & 0x1f ) >> 1 ) > 0x0c )
	{
		tileinfo->category = 1;
	}
	else
	{
		tileinfo->category = 0;
	}
}

VIDEO_START( kungfum )
{
	m62_start( machine, get_kungfum_bg_tile_info, 32, 0, 8, 8, 64, 32 );
}

VIDEO_UPDATE( kungfum )
{
	int i;
	for( i = 0; i < 6; i++ )
	{
		tilemap_set_scrollx( m62_background, i, 0 );
	}
	for( i = 6; i < 32; i++ )
	{
		tilemap_set_scrollx( m62_background, i, m62_background_hscroll );
	}
	tilemap_draw( bitmap, cliprect, m62_background, 0, 0 );
	draw_sprites( screen->machine, bitmap, cliprect, 0x1f, 0x00, 0x00 );
	tilemap_draw( bitmap, cliprect, m62_background, 1, 0 );
	return 0;
}



static TILE_GET_INFO( get_ldrun_bg_tile_info )
{
	int code;
	int color;
	int flags;
	code = m62_tileram[ tile_index << 1 ];
	color = m62_tileram[ ( tile_index << 1 ) | 1 ];
	flags = 0;
	if( ( color & 0x20 ) )
	{
		flags |= TILE_FLIPX;
	}
	SET_TILE_INFO( 0, code | ( ( color & 0xc0 ) << 2 ), color & 0x1f, flags );
	if( ( ( color & 0x1f ) >> 1 ) >= 0x0c )
	{
		tileinfo->group = 1;
	}
	else
	{
		tileinfo->group = 0;
	}
}

VIDEO_START( ldrun )
{
	m62_start( machine, get_ldrun_bg_tile_info, 1, 1, 8, 8, 64, 32 );
	tilemap_set_transmask(m62_background,0,0xffff,0x0000);	/* split type 0 is totally transparent in front half */
	tilemap_set_transmask(m62_background,1,0x0001,0xfffe);	/* split type 1 has pen 0 transparent in front half */
}

VIDEO_UPDATE( ldrun )
{
	tilemap_set_scrollx( m62_background, 0, m62_background_hscroll );
	tilemap_set_scrolly( m62_background, 0, m62_background_vscroll );

	tilemap_draw( bitmap, cliprect, m62_background, TILEMAP_DRAW_LAYER1, 0 );
	draw_sprites( screen->machine, bitmap, cliprect, 0x0f, 0x10, 0x00 );
	tilemap_draw( bitmap, cliprect, m62_background, TILEMAP_DRAW_LAYER0, 0 );
	draw_sprites( screen->machine, bitmap, cliprect, 0x0f, 0x10, 0x10 );
	return 0;
}

static TILE_GET_INFO( get_ldrun2_bg_tile_info )
{
	int code;
	int color;
	int flags;
	code = m62_tileram[ tile_index << 1 ];
	color = m62_tileram[ ( tile_index << 1 ) | 1 ];
	flags = 0;
	if( ( color & 0x20 ) )
	{
		flags |= TILE_FLIPX;
	}
	SET_TILE_INFO( 0, code | ( ( color & 0xc0 ) << 2 ), color & 0x1f, flags );
	if( ( ( color & 0x1f ) >> 1 ) >= 0x04 )
	{
		tileinfo->group = 1;
	}
	else
	{
		tileinfo->group = 0;
	}
}

VIDEO_START( ldrun2 )
{
	m62_start( machine, get_ldrun2_bg_tile_info, 1, 1, 8, 8, 64, 32 );
	tilemap_set_transmask(m62_background,0,0xffff,0x0000);	/* split type 0 is totally transparent in front half */
	tilemap_set_transmask(m62_background,1,0x0001,0xfffe);	/* split type 1 has pen 0 transparent in front half */
}



WRITE8_HANDLER( ldrun3_topbottom_mask_w )
{
	ldrun3_topbottom_mask = data & 1;
}

VIDEO_UPDATE( ldrun3 )
{
	VIDEO_UPDATE_CALL(ldrun);

	if (ldrun3_topbottom_mask)
	{
		rectangle my_cliprect = *cliprect;

		my_cliprect.min_y = 0*8;
		my_cliprect.max_y = 1*8-1;
		bitmap_fill(bitmap,&my_cliprect,get_black_pen(screen->machine));

		my_cliprect.min_y = 31*8;
		my_cliprect.max_y = 32*8-1;
		bitmap_fill(bitmap,&my_cliprect,get_black_pen(screen->machine));
	}

	return 0;
}


static TILE_GET_INFO( get_battroad_bg_tile_info )
{
	int code;
	int color;
	int flags;
	code = m62_tileram[ tile_index << 1 ];
	color = m62_tileram[ ( tile_index << 1 ) | 1 ];
	flags = 0;
	if( ( color & 0x20 ) )
	{
		flags |= TILE_FLIPX;
	}
	SET_TILE_INFO( 0, code | ( ( color & 0x40 ) << 3 ) | ( ( color & 0x10 ) << 4 ), color & 0x0f, flags );
	if( ( ( color & 0x1f ) >> 1 ) >= 0x04 )
	{
		tileinfo->group = 1;
	}
	else
	{
		tileinfo->group = 0;
	}
}

static TILE_GET_INFO( get_battroad_fg_tile_info )
{
	int code;
	int color;
	code = m62_textram[ tile_index << 1 ];
	color = m62_textram[ ( tile_index << 1 ) | 1 ];
	SET_TILE_INFO( 2, code | ( ( color & 0x40 ) << 3 ) | ( ( color & 0x10 ) << 4 ), color & 0x0f, 0 );
}

VIDEO_START( battroad )
{
	m62_start( machine, get_battroad_bg_tile_info, 1, 1, 8, 8, 64, 32 );
	m62_textlayer( machine, get_battroad_fg_tile_info, 1, 1, 8, 8, 32, 32 );
	tilemap_set_transmask(m62_background,0,0xffff,0x0000);	/* split type 0 is totally transparent in front half */
	tilemap_set_transmask(m62_background,1,0x0001,0xfffe);	/* split type 1 has pen 0 transparent in front half */
}

VIDEO_UPDATE( battroad )
{
	tilemap_set_scrollx( m62_background, 0, m62_background_hscroll );
	tilemap_set_scrolly( m62_background, 0, m62_background_vscroll );
	tilemap_set_scrollx( m62_foreground, 0, 128 );
	tilemap_set_scrolly( m62_foreground, 0, 0 );
	tilemap_set_transparent_pen( m62_foreground, 0 );

	tilemap_draw( bitmap, cliprect, m62_background, TILEMAP_DRAW_LAYER1, 0 );
	draw_sprites( screen->machine, bitmap, cliprect, 0x0f, 0x10, 0x00 );
	tilemap_draw( bitmap, cliprect, m62_background, TILEMAP_DRAW_LAYER0, 0 );
	draw_sprites( screen->machine, bitmap, cliprect, 0x0f, 0x10, 0x10 );
	tilemap_draw( bitmap, cliprect, m62_foreground, 0, 0 );
	return 0;
}


/* almost identical but scrolling background, more characters, */
/* no char x flip, and more sprites */
static TILE_GET_INFO( get_ldrun4_bg_tile_info )
{
	int code;
	int color;
	code = m62_tileram[ tile_index << 1 ];
	color = m62_tileram[ ( tile_index << 1 ) | 1 ];
	SET_TILE_INFO( 0, code | ( ( color & 0xc0 ) << 2 ) | ( ( color & 0x20 ) << 5 ), color & 0x1f, 0 );
}

VIDEO_START( ldrun4 )
{
	m62_start( machine, get_ldrun4_bg_tile_info, 1, 0, 8, 8, 64, 32 );
}

VIDEO_UPDATE( ldrun4 )
{
	tilemap_set_scrollx( m62_background, 0, m62_background_hscroll-2 );

	tilemap_draw( bitmap, cliprect, m62_background, 0, 0 );
	draw_sprites( screen->machine, bitmap, cliprect, 0x1f, 0x00, 0x00 );
	return 0;
}


static TILE_GET_INFO( get_lotlot_bg_tile_info )
{
	int code;
	int color;
	int flags;
	code = m62_tileram[ tile_index << 1 ];
	color = m62_tileram[ ( tile_index << 1 ) | 1 ];
	flags = 0;
	if( ( color & 0x20 ) )
	{
		flags |= TILE_FLIPX;
	}
	SET_TILE_INFO( 0, code | ( ( color & 0xc0 ) << 2 ), color & 0x1f, flags );
}

static TILE_GET_INFO( get_lotlot_fg_tile_info )
{
	int code;
	int color;
	code = m62_textram[ tile_index << 1 ];
	color = m62_textram[ ( tile_index << 1 ) | 1 ];
	SET_TILE_INFO( 2, code | ( ( color & 0xc0 ) << 2 ), color & 0x1f, 0 );
}

VIDEO_START( lotlot )
{
	m62_start( machine, get_lotlot_bg_tile_info, 1, 1, 12, 10, 32, 64 );
	m62_textlayer( machine, get_lotlot_fg_tile_info, 1, 1, 12, 10, 32, 64 );
}

VIDEO_UPDATE( lotlot )
{
	tilemap_set_scrollx( m62_background, 0, m62_background_hscroll - 64 );
	tilemap_set_scrolly( m62_background, 0, m62_background_vscroll + 32 );
	tilemap_set_scrollx( m62_foreground, 0, -64 );
	tilemap_set_scrolly( m62_foreground, 0, 32 );
	tilemap_set_transparent_pen( m62_foreground, 0 );

	tilemap_draw( bitmap, cliprect, m62_background, 0, 0 );
	tilemap_draw( bitmap, cliprect, m62_foreground, 0, 0 );
	draw_sprites( screen->machine, bitmap, cliprect, 0x1f, 0x00, 0x00 );
	return 0;
}


WRITE8_HANDLER( kidniki_text_vscroll_low_w )
{
	kidniki_text_vscroll = (kidniki_text_vscroll & 0xff00) | data;
}

WRITE8_HANDLER( kidniki_text_vscroll_high_w )
{
	kidniki_text_vscroll = (kidniki_text_vscroll & 0xff) | (data << 8);
}

WRITE8_HANDLER( kidniki_background_bank_w )
{
	if (kidniki_background_bank != (data & 1))
	{
		kidniki_background_bank = data & 1;
		tilemap_mark_all_tiles_dirty(m62_background);
	}
}

static TILE_GET_INFO( get_kidniki_bg_tile_info )
{
	int code;
	int color;
	code = m62_tileram[ tile_index << 1 ];
	color = m62_tileram[ ( tile_index << 1 ) | 1 ];
	SET_TILE_INFO( 0, code | ( ( color & 0xe0 ) << 3 ) | ( kidniki_background_bank << 11 ), color & 0x1f, 0);
	tileinfo->group = ( ( color & 0xe0 ) == 0xe0 ) ? 1 : 0;
}

static TILE_GET_INFO( get_kidniki_fg_tile_info )
{
	int code;
	int color;
	code = m62_textram[ tile_index << 1 ];
	color = m62_textram[ ( tile_index << 1 ) | 1 ];
	SET_TILE_INFO( 2, code | ( ( color & 0xc0 ) << 2 ), color & 0x1f, 0 );
}

VIDEO_START( kidniki )
{
	m62_background = tilemap_create( machine, get_kidniki_bg_tile_info, tilemap_scan_rows,  8, 8, 64, 32 );

	m62_background_hscroll = 0;
	m62_background_vscroll = 0;

	tilemap_set_transmask(m62_background,0,0xffff,0x0000);	/* split type 0 is totally transparent in front half */
	tilemap_set_transmask(m62_background,1,0x0001,0xfffe);	/* split type 1 has pen 0 transparent in front half */

	register_savestate(machine);

	m62_textlayer( machine, get_kidniki_fg_tile_info, 1, 1, 12, 8, 32, 64 );
}

VIDEO_UPDATE( kidniki )
{
	tilemap_set_scrollx( m62_background, 0, m62_background_hscroll );
	tilemap_set_scrollx( m62_foreground, 0, -64 );
	tilemap_set_scrolly( m62_foreground, 0, kidniki_text_vscroll + 128 );
	tilemap_set_transparent_pen( m62_foreground, 0 );

	tilemap_draw( bitmap, cliprect, m62_background, TILEMAP_DRAW_LAYER1, 0 );
	draw_sprites( screen->machine, bitmap, cliprect, 0x1f, 0x00, 0x00 );
	tilemap_draw( bitmap, cliprect, m62_background, TILEMAP_DRAW_LAYER0, 0 );
	tilemap_draw( bitmap, cliprect, m62_foreground, 0, 0 );
	return 0;
}


WRITE8_HANDLER( spelunkr_palbank_w )
{
	if (spelunkr_palbank != (data & 0x01))
	{
		spelunkr_palbank = data & 0x01;
		tilemap_mark_all_tiles_dirty(m62_background);
		tilemap_mark_all_tiles_dirty(m62_foreground);
	}
}

static TILE_GET_INFO( get_spelunkr_bg_tile_info )
{
	int code;
	int color;
	code = m62_tileram[ tile_index << 1 ];
	color = m62_tileram[ ( tile_index << 1 ) | 1 ];
	SET_TILE_INFO( 0, code | ( ( color & 0x10 ) << 4 ) | ( ( color & 0x20 ) << 6 ) | ( ( color & 0xc0 ) << 3 ), ( color & 0x0f ) | ( spelunkr_palbank << 4 ), 0 );
}

static TILE_GET_INFO( get_spelunkr_fg_tile_info )
{
	int code;
	int color;
	code = m62_textram[ tile_index << 1 ];
	color = m62_textram[ ( tile_index << 1 ) | 1 ];
if (color&0xe0) popmessage("fg tilemap %x %x",tile_index,color&0xe0);
	SET_TILE_INFO( 2, code | ( ( color & 0x10 ) << 4 ), ( color & 0x0f ) | ( spelunkr_palbank << 4 ), 0 );
}

VIDEO_START( spelunkr )
{
	m62_start( machine, get_spelunkr_bg_tile_info, 1, 1, 8, 8, 64, 64 );
	m62_textlayer( machine, get_spelunkr_fg_tile_info, 1, 1, 12, 8, 32, 32 );
}

VIDEO_UPDATE( spelunkr )
{
	tilemap_set_scrollx( m62_background, 0, m62_background_hscroll );
	tilemap_set_scrolly( m62_background, 0, m62_background_vscroll + 128 );
	tilemap_set_scrollx( m62_foreground, 0, -64 );
	tilemap_set_scrolly( m62_foreground, 0, 0 );
	tilemap_set_transparent_pen( m62_foreground, 0 );

	tilemap_draw( bitmap, cliprect, m62_background, 0, 0 );
	draw_sprites( screen->machine, bitmap, cliprect, 0x1f, 0x00, 0x00 );
	tilemap_draw( bitmap, cliprect, m62_foreground, 0, 0 );
	return 0;
}


WRITE8_HANDLER( spelunk2_gfxport_w )
{
	m62_hscroll_high_w(space,0,(data&2)>>1);
	m62_vscroll_high_w(space,0,(data&1));
	if (spelunkr_palbank != ((data & 0x0c) >> 2))
	{
		spelunkr_palbank = (data & 0x0c) >> 2;
		tilemap_mark_all_tiles_dirty(m62_background);
		tilemap_mark_all_tiles_dirty(m62_foreground);
	}
}

static TILE_GET_INFO( get_spelunk2_bg_tile_info )
{
	int code;
	int color;
	code = m62_tileram[ tile_index << 1 ];
	color = m62_tileram[ ( tile_index << 1 ) | 1 ];
	SET_TILE_INFO( 0, code | ( ( color & 0xf0 ) << 4 ), ( color & 0x0f ) | ( spelunkr_palbank << 4 ), 0 );
}

VIDEO_START( spelunk2 )
{
	m62_start( machine, get_spelunk2_bg_tile_info, 1, 1, 8, 8, 64, 64 );
	m62_textlayer( machine, get_spelunkr_fg_tile_info, 1, 1, 12, 8, 32, 32 );
}

VIDEO_UPDATE( spelunk2 )
{
	tilemap_set_scrollx( m62_background, 0, m62_background_hscroll - 1);
	tilemap_set_scrolly( m62_background, 0, m62_background_vscroll + 128 );
	tilemap_set_scrollx( m62_foreground, 0, -65 );
	tilemap_set_scrolly( m62_foreground, 0, 0 );
	tilemap_set_transparent_pen( m62_foreground, 0 );

	tilemap_draw( bitmap, cliprect, m62_background, 0, 0 );
	draw_sprites( screen->machine, bitmap, cliprect, 0x1f, 0x00, 0x00 );
	tilemap_draw( bitmap, cliprect, m62_foreground, 0, 0 );
	return 0;
}


static TILE_GET_INFO( get_youjyudn_bg_tile_info )
{
	int code;
	int color;
	code = m62_tileram[ tile_index << 1 ];
	color = m62_tileram[ ( tile_index << 1 ) | 1 ];
	SET_TILE_INFO( 0, code | ( ( color & 0x60 ) << 3 ), color & 0x1f, 0 );
	if( ( ( color & 0x1f ) >> 1 ) >= 0x08 )
	{
		tileinfo->group = 1;
	}
	else
	{
		tileinfo->group = 0;
	}
}

static TILE_GET_INFO( get_youjyudn_fg_tile_info )
{
	int code;
	int color;
	code = m62_textram[ tile_index << 1 ];
	color = m62_textram[ ( tile_index << 1 ) | 1 ];
	SET_TILE_INFO( 2, code | ( ( color & 0xc0 ) << 2 ), ( color & 0x0f ), 0 );
}

VIDEO_START( youjyudn )
{
	m62_start( machine, get_youjyudn_bg_tile_info, 1, 0, 8, 16, 64, 16 );
	m62_textlayer( machine, get_youjyudn_fg_tile_info, 1, 1, 12, 8, 32, 32 );
	tilemap_set_transmask(m62_background,0,0xffff,0x0000);	/* split type 0 is totally transparent in front half */
	tilemap_set_transmask(m62_background,1,0x0001,0xfffe);	/* split type 1 has pen 0 transparent in front half */
}

VIDEO_UPDATE( youjyudn )
{
	tilemap_set_scrollx( m62_background, 0, m62_background_hscroll );
	tilemap_set_scrollx( m62_foreground, 0, -64 );
	tilemap_set_scrolly( m62_foreground, 0, 0 );
	tilemap_set_transparent_pen( m62_foreground, 0 );

	tilemap_draw( bitmap, cliprect, m62_background, TILEMAP_DRAW_LAYER1, 0 );
	draw_sprites( screen->machine, bitmap, cliprect, 0x1f, 0x00, 0x00 );
	tilemap_draw( bitmap, cliprect, m62_background, TILEMAP_DRAW_LAYER0, 0 );
	tilemap_draw( bitmap, cliprect, m62_foreground, 0, 0 );
	return 0;
}


WRITE8_HANDLER( horizon_scrollram_w )
{
	horizon_scrollram[ offset ] = data;
}

static TILE_GET_INFO( get_horizon_bg_tile_info )
{
	int code;
	int color;
	code = m62_tileram[ tile_index << 1 ];
	color = m62_tileram[ ( tile_index << 1 ) | 1 ];
	SET_TILE_INFO( 0, code | ( ( color & 0xc0 ) << 2 ) | ( ( color & 0x20 ) << 5 ), color & 0x1f, 0 );
	if( ( ( color & 0x1f ) >> 1 ) >= 0x08 )
	{
		tileinfo->group = 1;
	}
	else
	{
		tileinfo->group = 0;
	}
}

VIDEO_START( horizon )
{
	m62_start( machine, get_horizon_bg_tile_info, 32, 0, 8, 8, 64, 32 );
	tilemap_set_transmask(m62_background,0,0xffff,0x0000);	/* split type 0 is totally transparent in front half */
	tilemap_set_transmask(m62_background,1,0x0001,0xfffe);	/* split type 1 has pen 0 transparent in front half */
}

VIDEO_UPDATE( horizon )
{
	int i;
	for( i = 0; i < 32; i++ )
	{
		tilemap_set_scrollx( m62_background, i, horizon_scrollram[ i << 1 ] | ( horizon_scrollram[ ( i << 1 ) | 1 ] << 8 ) );
	}
	tilemap_draw( bitmap, cliprect, m62_background, TILEMAP_DRAW_LAYER1, 0 );
	draw_sprites( screen->machine, bitmap, cliprect, 0x1f, 0x00, 0x00 );
	tilemap_draw( bitmap, cliprect, m62_background, TILEMAP_DRAW_LAYER0, 0 );
	return 0;
}

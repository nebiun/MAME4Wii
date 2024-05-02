/***************************************************************************

  video.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"

UINT16 *lastduel_vram,*lastduel_scroll2,*lastduel_scroll1;

static tilemap *bg_tilemap,*fg_tilemap,*tx_tilemap;

static int sprite_flipy_mask,sprite_pri_mask,tilemap_priority;



/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static TILE_GET_INFO( ld_get_bg_tile_info )
{
	int tile = lastduel_scroll2[2*tile_index] & 0x1fff;
	int color = lastduel_scroll2[2*tile_index+1];
	SET_TILE_INFO(
			2,
			tile,color & 0xf,
			TILE_FLIPYX((color & 0x60) >> 5));
}

static TILE_GET_INFO( ld_get_fg_tile_info )
{
	int tile = lastduel_scroll1[2*tile_index] & 0x1fff;
	int color = lastduel_scroll1[2*tile_index+1];
	SET_TILE_INFO(
			3,
			tile,
			color & 0xf,
			TILE_FLIPYX((color & 0x60) >> 5));
	tileinfo->group = (color & 0x80) >> 7;
}

static TILE_GET_INFO( get_bg_tile_info )
{
	int tile = lastduel_scroll2[tile_index] & 0x1fff;
	int color = lastduel_scroll2[tile_index+0x0800];
	SET_TILE_INFO(
			2,
			tile,
			color & 0xf,
			TILE_FLIPYX((color & 0x60) >> 5));
}

static TILE_GET_INFO( get_fg_tile_info )
{
	int tile = lastduel_scroll1[tile_index] & 0x1fff;
	int color = lastduel_scroll1[tile_index+0x0800];
	SET_TILE_INFO(
			3,
			tile,
			color & 0xf,
			TILE_FLIPYX((color & 0x60) >> 5));
	tileinfo->group = (color & 0x10) >> 4;
}

static TILE_GET_INFO( get_fix_info )
{
	int tile = lastduel_vram[tile_index];
	SET_TILE_INFO(
			1,
			tile & 0x7ff,
			tile>>12,
			(tile & 0x800) ? TILE_FLIPY : 0);
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

VIDEO_START( lastduel )
{
	bg_tilemap = tilemap_create(machine, ld_get_bg_tile_info,tilemap_scan_rows,16,16,64,64);
	fg_tilemap = tilemap_create(machine, ld_get_fg_tile_info,tilemap_scan_rows,16,16,64,64);
	tx_tilemap = tilemap_create(machine, get_fix_info,tilemap_scan_rows,8,8,64,32);

	tilemap_set_transmask(fg_tilemap,0,0xffff,0x0001);
	tilemap_set_transmask(fg_tilemap,1,0xf07f,0x0f81);
	tilemap_set_transparent_pen(tx_tilemap,3);

	sprite_flipy_mask = 0x40;
	sprite_pri_mask = 0x00;
	tilemap_priority = 0;
}

VIDEO_START( madgear )
{
	bg_tilemap = tilemap_create(machine, get_bg_tile_info,tilemap_scan_cols,16,16,64,32);
	fg_tilemap = tilemap_create(machine, get_fg_tile_info,tilemap_scan_cols,16,16,64,32);
	tx_tilemap = tilemap_create(machine, get_fix_info,tilemap_scan_rows,8,8,64,32);

	tilemap_set_transmask(fg_tilemap,0,0xffff,0x8000);
	tilemap_set_transmask(fg_tilemap,1,0x80ff,0xff00);
	tilemap_set_transparent_pen(tx_tilemap,3);
	tilemap_set_transparent_pen(bg_tilemap,15);

	sprite_flipy_mask = 0x80;
	sprite_pri_mask = 0x10;
	tilemap_priority = 0;
}



/***************************************************************************

  Memory handlers

***************************************************************************/

WRITE16_HANDLER( lastduel_flip_w )
{
	if (ACCESSING_BITS_0_7)
	{
		flip_screen_set(space->machine, data & 0x01);

		coin_lockout_w(0,~data & 0x10);
		coin_lockout_w(1,~data & 0x20);
		coin_counter_w(0,data & 0x40);
		coin_counter_w(1,data & 0x80);
	}
}

WRITE16_HANDLER( lastduel_scroll_w )
{
	static UINT16 scroll[8];

	data = COMBINE_DATA(&scroll[offset]);
	switch (offset)
	{
		case 0: tilemap_set_scrolly(fg_tilemap,0,data); break;
		case 1: tilemap_set_scrollx(fg_tilemap,0,data); break;
		case 2: tilemap_set_scrolly(bg_tilemap,0,data); break;
		case 3: tilemap_set_scrollx(bg_tilemap,0,data); break;
		case 7: tilemap_priority=data; break;
		default:
			logerror("Unmapped video write %d %04x\n", offset, data);
			break;
	}
}

WRITE16_HANDLER( lastduel_scroll1_w )
{
	COMBINE_DATA(&lastduel_scroll1[offset]);
	tilemap_mark_tile_dirty(fg_tilemap,offset/2);
}

WRITE16_HANDLER( lastduel_scroll2_w )
{
	COMBINE_DATA(&lastduel_scroll2[offset]);
	tilemap_mark_tile_dirty(bg_tilemap,offset/2);
}

WRITE16_HANDLER( lastduel_vram_w )
{
	COMBINE_DATA(&lastduel_vram[offset]);
	tilemap_mark_tile_dirty(tx_tilemap,offset);
}

WRITE16_HANDLER( madgear_scroll1_w )
{
	COMBINE_DATA(&lastduel_scroll1[offset]);
	tilemap_mark_tile_dirty(fg_tilemap,offset & 0x7ff);
}

WRITE16_HANDLER( madgear_scroll2_w )
{
	COMBINE_DATA(&lastduel_scroll2[offset]);
	tilemap_mark_tile_dirty(bg_tilemap,offset & 0x7ff);
}

WRITE16_HANDLER( lastduel_palette_word_w )
{
	int red, green, blue, bright;
	data = COMBINE_DATA(&paletteram16[offset]);

	// Brightness parameter interpreted same way as CPS1
	bright = 0x10 + (data&0x0f);

	red   = ((data>>12)&0x0f) * bright * 0x11 / 0x1f;
	green = ((data>>8 )&0x0f) * bright * 0x11 / 0x1f;
	blue  = ((data>>4 )&0x0f) * bright * 0x11 / 0x1f;

	palette_set_color (space->machine, offset, MAKE_RGB(red, green, blue));
}

/***************************************************************************

  Display refresh

***************************************************************************/

static void draw_sprites(running_machine *machine, bitmap_t *bitmap, const rectangle *cliprect, int pri)
{
	int offs;

	if (!sprite_pri_mask)
		if (pri == 1) return;	/* only low priority sprites in lastduel */

	for(offs=0x400-4;offs>=0;offs-=4)
	{
		int attr,sy,sx,flipx,flipy,code,color;

		attr = buffered_spriteram16[offs+1];
		if (sprite_pri_mask)	/* only madgear seems to have this */
		{
			if (pri==1 && (attr & sprite_pri_mask)) continue;
			if (pri==0 && !(attr & sprite_pri_mask)) continue;
		}

		code = buffered_spriteram16[offs];
		sx = buffered_spriteram16[offs+3] & 0x1ff;
		sy = buffered_spriteram16[offs+2] & 0x1ff;
		if (sy > 0x100)
			sy -= 0x200;

		flipx = attr & 0x20;
		flipy = attr & sprite_flipy_mask;	/* 0x40 for lastduel, 0x80 for madgear */
		color = attr & 0x0f;

		if (flip_screen_get(machine))
		{
			sx = 496 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		drawgfx_transpen(bitmap,cliprect,
				machine->gfx[0],
				code,
				color,
				flipx,flipy,
				sx,sy,15);
	}
}

VIDEO_UPDATE( lastduel )
{
	tilemap_draw(bitmap,cliprect,bg_tilemap,0,0);
	tilemap_draw(bitmap,cliprect,fg_tilemap,TILEMAP_DRAW_LAYER1,0);
	draw_sprites(screen->machine,bitmap,cliprect,0);
	tilemap_draw(bitmap,cliprect,fg_tilemap,TILEMAP_DRAW_LAYER0,0);
	draw_sprites(screen->machine,bitmap,cliprect,1);
	tilemap_draw(bitmap,cliprect,tx_tilemap,0,0);
	return 0;
}

VIDEO_UPDATE( madgear )
{
	if (tilemap_priority)
	{
		tilemap_draw(bitmap,cliprect,fg_tilemap,TILEMAP_DRAW_LAYER1 | TILEMAP_DRAW_OPAQUE,0);
		draw_sprites(screen->machine,bitmap,cliprect,0);
		tilemap_draw(bitmap,cliprect,fg_tilemap,TILEMAP_DRAW_LAYER0,0);
		tilemap_draw(bitmap,cliprect,bg_tilemap,0,0);
		draw_sprites(screen->machine,bitmap,cliprect,1);
	}
	else
	{
		tilemap_draw(bitmap,cliprect,bg_tilemap,TILEMAP_DRAW_OPAQUE,0);
		tilemap_draw(bitmap,cliprect,fg_tilemap,TILEMAP_DRAW_LAYER1,0);
		draw_sprites(screen->machine,bitmap,cliprect,0);
		tilemap_draw(bitmap,cliprect,fg_tilemap,TILEMAP_DRAW_LAYER0,0);
		draw_sprites(screen->machine,bitmap,cliprect,1);
	}
	tilemap_draw(bitmap,cliprect,tx_tilemap,0,0);
	return 0;
}

VIDEO_EOF( lastduel )
{
	const address_space *space = cputag_get_address_space(machine, "maincpu", ADDRESS_SPACE_PROGRAM);

	/* Spriteram is always 1 frame ahead, suggesting buffering.  I can't find
        a register to control this so I assume it happens automatically
        every frame at the end of vblank */
	buffer_spriteram16_w(space,0,0,0xffff);
}

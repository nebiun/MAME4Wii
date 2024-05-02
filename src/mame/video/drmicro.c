/*******************************************************************************

Dr. Micro (c) 1983 Sanritsu

Video hardware
        driver by Uki

*******************************************************************************/

#include "driver.h"

static int flipscreen;

static UINT8 *drmicro_videoram;
static tilemap *drmicro_bg1;
static tilemap *drmicro_bg2;

/****************************************************************************/

void drmicro_flip_w( running_machine *machine, int flip )
{
	flipscreen = flip ? 1 : 0;
	flip_screen_set(machine, flip);
}

WRITE8_HANDLER( drmicro_videoram_w )
{
	drmicro_videoram[offset] = data;

	if (offset<0x800)
		tilemap_mark_tile_dirty(drmicro_bg2,(offset & 0x3ff));
	else
		tilemap_mark_tile_dirty(drmicro_bg1,(offset & 0x3ff));
}


/****************************************************************************/

static TILE_GET_INFO( get_bg1_tile_info )
{
	int code,col,flags;

	code = drmicro_videoram[tile_index + 0x0800];
	col  = drmicro_videoram[tile_index + 0x0c00];

	code += (col & 0xc0) << 2;
	flags = ((col & 0x20) ? TILEMAP_FLIPY : 0) | ((col & 0x10) ? TILEMAP_FLIPX : 0);
	col &= 0x0f;

	SET_TILE_INFO( 0, code, col, flags);
}

static TILE_GET_INFO( get_bg2_tile_info )
{
	int code,col,flags;

	code = drmicro_videoram[tile_index + 0x0000];
	col  = drmicro_videoram[tile_index + 0x0400];

	code += (col & 0xc0) << 2;
	flags = ((col & 0x20) ? TILEMAP_FLIPY : 0) | ((col & 0x10) ? TILEMAP_FLIPX : 0);
	col &= 0x0f;

	SET_TILE_INFO( 1, code, col, flags);
}

/****************************************************************************/

PALETTE_INIT( drmicro )
{
	int i;

	/* allocate the colortable */
	machine->colortable = colortable_alloc(machine, 0x20);

	/* create a lookup table for the palette */
	for (i = 0; i < 0x20; i++)
	{
		int bit0, bit1, bit2;
		int r, g, b;

		/* red component */
		bit0 = (color_prom[i] >> 0) & 0x01;
		bit1 = (color_prom[i] >> 1) & 0x01;
		bit2 = (color_prom[i] >> 2) & 0x01;
		r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		/* green component */
		bit0 = (color_prom[i] >> 3) & 0x01;
		bit1 = (color_prom[i] >> 4) & 0x01;
		bit2 = (color_prom[i] >> 5) & 0x01;
		g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		/* blue component */
		bit0 = 0;
		bit1 = (color_prom[i] >> 6) & 0x01;
		bit2 = (color_prom[i] >> 7) & 0x01;
		b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		colortable_palette_set_color(machine->colortable, i, MAKE_RGB(r, g, b));
	}

	/* color_prom now points to the beginning of the lookup table */
	color_prom += 0x20;

	for (i = 0; i < 0x200; i++)
	{
		UINT8 ctabentry = color_prom[i] & 0x0f;
		colortable_entry_set_value(machine->colortable, i, ctabentry);
	}
}

VIDEO_START( drmicro)
{
	drmicro_videoram = auto_alloc_array(machine, UINT8, 0x1000);

	drmicro_bg1 = tilemap_create(machine, get_bg1_tile_info, tilemap_scan_rows, 8,8,32,32);
	drmicro_bg2 = tilemap_create(machine, get_bg2_tile_info, tilemap_scan_rows, 8,8,32,32);

	tilemap_set_transparent_pen(drmicro_bg2,0);
}

VIDEO_UPDATE( drmicro )
{
	int offs,adr,g;
	int chr,col,attr;
	int x,y,fx,fy;

	tilemap_draw(bitmap, cliprect, drmicro_bg1, 0, 0);
	tilemap_draw(bitmap, cliprect, drmicro_bg2, 0, 0);

	/* draw sprites */

	for (g=0;g<2;g++)
	{
		adr = 0x800*g;

		for (offs=0x00; offs<0x20; offs +=4)
		{
			x =    drmicro_videoram[offs + adr + 3];
			y =    drmicro_videoram[offs + adr + 0];
			attr = drmicro_videoram[offs + adr + 2];
			chr =  drmicro_videoram[offs + adr + 1];

			fx = (chr & 0x01) ^ flipscreen;
			fy = ((chr & 0x02) >> 1) ^ flipscreen;

			chr =  (chr >> 2) | (attr & 0xc0);

			col = (attr & 0x0f) + 0x00;

			if (!flipscreen)
				y = (240-y) & 0xff;
			else
				x = (240-x) & 0xff;

			drawgfx_transpen(bitmap,cliprect,screen->machine->gfx[3-g],
					chr,
					col,
					fx,fy,
					x,y,0);

			if (x>240)
			{
				drawgfx_transpen(bitmap,cliprect,screen->machine->gfx[3-g],
						chr,
						col,
						fx,fy,
						x-256,y,0);
			}
		}
	}
	return 0;
}


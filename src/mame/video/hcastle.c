/***************************************************************************

    Haunted Castle video emulation

***************************************************************************/

#include "driver.h"
#include "video/konamiic.h"

UINT8 *hcastle_pf1_videoram,*hcastle_pf2_videoram;
static int gfx_bank;

static tilemap *fg_tilemap,*bg_tilemap;
static int pf2_bankbase,pf1_bankbase;


PALETTE_INIT( hcastle )
{
	int chip;

	/* allocate the colortable */
	machine->colortable = colortable_alloc(machine, 0x80);

	for (chip = 0; chip < 2; chip++)
	{
		int pal;

		for (pal = 0; pal < 8; pal++)
		{
			int i;
			int clut = (chip << 1) | (pal & 1);

			for (i = 0; i < 0x100; i++)
			{
				UINT8 ctabentry;

				if (((pal & 0x01) == 0) && (color_prom[(clut << 8) | i] == 0))
					ctabentry = 0;
				else
					ctabentry = (pal << 4) | (color_prom[(clut << 8) | i] & 0x0f);

				colortable_entry_set_value(machine->colortable, (chip << 11) | (pal << 8) | i, ctabentry);
			}
		}
	}
}


static void set_pens(colortable_t *colortable)
{
	int i;

	for (i = 0x00; i < 0x100; i += 2)
	{
		UINT16 data = paletteram[i | 1] | (paletteram[i] << 8);

		rgb_t color = MAKE_RGB(pal5bit(data >> 0), pal5bit(data >> 5), pal5bit(data >> 10));

		colortable_palette_set_color(colortable, i >> 1, color);
	}
}



/***************************************************************************

    Callbacks for the TileMap code

***************************************************************************/

static TILEMAP_MAPPER( tilemap_scan )
{
	/* logical (col,row) -> memory offset */
	return (col & 0x1f) + ((row & 0x1f) << 5) + ((col & 0x20) << 6);	/* skip 0x400 */
}

static TILE_GET_INFO( get_fg_tile_info )
{
	int bit0 = (K007121_ctrlram[0][0x05] >> 0) & 0x03;
	int bit1 = (K007121_ctrlram[0][0x05] >> 2) & 0x03;
	int bit2 = (K007121_ctrlram[0][0x05] >> 4) & 0x03;
	int bit3 = (K007121_ctrlram[0][0x05] >> 6) & 0x03;
	int attr = hcastle_pf1_videoram[tile_index];
	int tile = hcastle_pf1_videoram[tile_index + 0x400];
	int color = attr & 0x7;
	int bank =  ((attr & 0x80) >> 7) |
				((attr >> (bit0+2)) & 0x02) |
				((attr >> (bit1+1)) & 0x04) |
				((attr >> (bit2  )) & 0x08) |
				((attr >> (bit3-1)) & 0x10);

	SET_TILE_INFO(
			0,
			tile + bank*0x100 + pf1_bankbase,
			((K007121_ctrlram[0][6]&0x30)*2+16) + color,
			0);
}

static TILE_GET_INFO( get_bg_tile_info )
{
	int bit0 = (K007121_ctrlram[1][0x05] >> 0) & 0x03;
	int bit1 = (K007121_ctrlram[1][0x05] >> 2) & 0x03;
	int bit2 = (K007121_ctrlram[1][0x05] >> 4) & 0x03;
	int bit3 = (K007121_ctrlram[1][0x05] >> 6) & 0x03;
	int attr = hcastle_pf2_videoram[tile_index];
	int tile = hcastle_pf2_videoram[tile_index + 0x400];
	int color = attr & 0x7;
	int bank =  ((attr & 0x80) >> 7) |
				((attr >> (bit0+2)) & 0x02) |
				((attr >> (bit1+1)) & 0x04) |
				((attr >> (bit2  )) & 0x08) |
				((attr >> (bit3-1)) & 0x10);

	SET_TILE_INFO(
			1,
			tile + bank*0x100 + pf2_bankbase,
			((K007121_ctrlram[1][6]&0x30)*2+16) + color,
			0);
}



/***************************************************************************

    Start the video hardware emulation.

***************************************************************************/

VIDEO_START( hcastle )
{
	fg_tilemap = tilemap_create(machine, get_fg_tile_info,tilemap_scan,8,8,64,32);
	bg_tilemap = tilemap_create(machine, get_bg_tile_info,tilemap_scan,     8,8,64,32);

	tilemap_set_transparent_pen(fg_tilemap,0);
}



/***************************************************************************

    Memory handlers

***************************************************************************/

WRITE8_HANDLER( hcastle_pf1_video_w )
{
	hcastle_pf1_videoram[offset] = data;
	tilemap_mark_tile_dirty(fg_tilemap,offset & 0xbff);
}

WRITE8_HANDLER( hcastle_pf2_video_w )
{
	hcastle_pf2_videoram[offset] = data;
	tilemap_mark_tile_dirty(bg_tilemap,offset & 0xbff);
}

WRITE8_HANDLER( hcastle_gfxbank_w )
{
	gfx_bank = data;
}

READ8_HANDLER( hcastle_gfxbank_r )
{
	return gfx_bank;
}

WRITE8_HANDLER( hcastle_pf1_control_w )
{
	if (offset==3)
	{
		if ((data&0x8)==0)
			buffer_spriteram(spriteram+0x800,0x800);
		else
			buffer_spriteram(spriteram,0x800);
	}
	else if (offset == 7)
	{
		tilemap_set_flip(fg_tilemap, (data & 0x08) ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);
	}
	K007121_ctrl_0_w(space,offset,data);
}

WRITE8_HANDLER( hcastle_pf2_control_w )
{
	if (offset==3)
	{
		if ((data&0x8)==0)
			buffer_spriteram_2(spriteram_2+0x800,0x800);
		else
			buffer_spriteram_2(spriteram_2,0x800);
	}
	else if (offset == 7)
	{
		tilemap_set_flip(bg_tilemap, (data & 0x08) ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);
	}
	K007121_ctrl_1_w(space,offset,data);
}

/*****************************************************************************/

static void draw_sprites(running_machine *machine, bitmap_t *bitmap, const rectangle *cliprect, UINT8 *sbank, int bank )
{
	int bank_base = (bank == 0) ? 0x4000 * (gfx_bank & 1) : 0;
	K007121_sprites_draw(bank,bitmap,cliprect,machine->gfx,machine->colortable,sbank,(K007121_ctrlram[bank][6]&0x30)*2,0,bank_base,-1);
}

/*****************************************************************************/

VIDEO_UPDATE( hcastle )
{
	static int old_pf1,old_pf2;

	set_pens(screen->machine->colortable);

	pf1_bankbase = 0x0000;
	pf2_bankbase = 0x4000 * ((gfx_bank & 2) >> 1);

	if (K007121_ctrlram[0][3] & 0x01) pf1_bankbase += 0x2000;
	if (K007121_ctrlram[1][3] & 0x01) pf2_bankbase += 0x2000;

	if (pf1_bankbase != old_pf1)
		tilemap_mark_all_tiles_dirty(fg_tilemap);

	if (pf2_bankbase != old_pf2)
		tilemap_mark_all_tiles_dirty(bg_tilemap);

	old_pf1 = pf1_bankbase;
	old_pf2 = pf2_bankbase;

	tilemap_set_scrolly(bg_tilemap,0,K007121_ctrlram[1][2]);
	tilemap_set_scrollx(bg_tilemap,0,((K007121_ctrlram[1][1]<<8)+K007121_ctrlram[1][0]));
	tilemap_set_scrolly(fg_tilemap,0,K007121_ctrlram[0][2]);
	tilemap_set_scrollx(fg_tilemap,0,((K007121_ctrlram[0][1]<<8)+K007121_ctrlram[0][0]));

//  /* Sprite priority */
//  if (K007121_ctrlram[0][3]&0x20)
	if ((gfx_bank & 0x04) == 0)
	{
		tilemap_draw(bitmap,cliprect,bg_tilemap,0,0);
		draw_sprites(screen->machine,bitmap,cliprect, buffered_spriteram, 0 );
		draw_sprites(screen->machine,bitmap,cliprect, buffered_spriteram_2, 1 );
		tilemap_draw(bitmap,cliprect,fg_tilemap,0,0);
	}
	else
	{
		tilemap_draw(bitmap,cliprect,bg_tilemap,0,0);
		tilemap_draw(bitmap,cliprect,fg_tilemap,0,0);
		draw_sprites(screen->machine,bitmap,cliprect, buffered_spriteram, 0 );
		draw_sprites(screen->machine,bitmap,cliprect, buffered_spriteram_2, 1 );
	}
	return 0;
}

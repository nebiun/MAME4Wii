/***************************************************************************

    video/contra.c

***************************************************************************/

#include "driver.h"
#include "video/konamiic.h"

//static int spriteram_offset;
static UINT8 *private_spriteram_2,*private_spriteram;

UINT8 *contra_fg_vram,*contra_fg_cram;
UINT8 *contra_text_vram,*contra_text_cram;
UINT8 *contra_bg_vram,*contra_bg_cram;

static tilemap *bg_tilemap, *fg_tilemap, *tx_tilemap;
static rectangle bg_clip, fg_clip, tx_clip;

/***************************************************************************
**
**  Contra has palette RAM, but it also has four lookup table PROMs
**
**  0   sprites #0
**  1   tiles   #0
**  2   sprites #1
**  3   tiles   #1
**
***************************************************************************/

PALETTE_INIT( contra )
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
		UINT16 data = paletteram[i] | (paletteram[i | 1] << 8);

		rgb_t color = MAKE_RGB(pal5bit(data >> 0), pal5bit(data >> 5), pal5bit(data >> 10));

		colortable_palette_set_color(colortable, i >> 1, color);
	}
}



/***************************************************************************

    Callbacks for the TileMap code

***************************************************************************/

static TILE_GET_INFO( get_fg_tile_info )
{
	int attr = contra_fg_cram[tile_index];
	int bit0 = (K007121_ctrlram[0][0x05] >> 0) & 0x03;
	int bit1 = (K007121_ctrlram[0][0x05] >> 2) & 0x03;
	int bit2 = (K007121_ctrlram[0][0x05] >> 4) & 0x03;
	int bit3 = (K007121_ctrlram[0][0x05] >> 6) & 0x03;
	int bank = ((attr & 0x80) >> 7) |
			((attr >> (bit0+2)) & 0x02) |
			((attr >> (bit1+1)) & 0x04) |
			((attr >> (bit2  )) & 0x08) |
			((attr >> (bit3-1)) & 0x10) |
			((K007121_ctrlram[0][0x03] & 0x01) << 5);
	int mask = (K007121_ctrlram[0][0x04] & 0xf0) >> 4;

	bank = (bank & ~(mask << 1)) | ((K007121_ctrlram[0][0x04] & mask) << 1);

	SET_TILE_INFO(
			0,
			contra_fg_vram[tile_index]+bank*256,
			((K007121_ctrlram[0][6]&0x30)*2+16)+(attr&7),
			0);
}

static TILE_GET_INFO( get_bg_tile_info )
{
	int attr = contra_bg_cram[tile_index];
	int bit0 = (K007121_ctrlram[1][0x05] >> 0) & 0x03;
	int bit1 = (K007121_ctrlram[1][0x05] >> 2) & 0x03;
	int bit2 = (K007121_ctrlram[1][0x05] >> 4) & 0x03;
	int bit3 = (K007121_ctrlram[1][0x05] >> 6) & 0x03;
	int bank = ((attr & 0x80) >> 7) |
			((attr >> (bit0+2)) & 0x02) |
			((attr >> (bit1+1)) & 0x04) |
			((attr >> (bit2  )) & 0x08) |
			((attr >> (bit3-1)) & 0x10) |
			((K007121_ctrlram[1][0x03] & 0x01) << 5);
	int mask = (K007121_ctrlram[1][0x04] & 0xf0) >> 4;

	bank = (bank & ~(mask << 1)) | ((K007121_ctrlram[0][0x04] & mask) << 1);

	SET_TILE_INFO(
			1,
			contra_bg_vram[tile_index]+bank*256,
			((K007121_ctrlram[1][6]&0x30)*2+16)+(attr&7),
			0);
}

static TILE_GET_INFO( get_tx_tile_info )
{
	int attr = contra_text_cram[tile_index];
	int bit0 = (K007121_ctrlram[0][0x05] >> 0) & 0x03;
	int bit1 = (K007121_ctrlram[0][0x05] >> 2) & 0x03;
	int bit2 = (K007121_ctrlram[0][0x05] >> 4) & 0x03;
	int bit3 = (K007121_ctrlram[0][0x05] >> 6) & 0x03;
	int bank = ((attr & 0x80) >> 7) |
			((attr >> (bit0+2)) & 0x02) |
			((attr >> (bit1+1)) & 0x04) |
			((attr >> (bit2  )) & 0x08) |
			((attr >> (bit3-1)) & 0x10);
	SET_TILE_INFO(
			0,
			contra_text_vram[tile_index]+bank*256,
			((K007121_ctrlram[0][6]&0x30)*2+16)+(attr&7),
			0);
}


/***************************************************************************

    Start the video hardware emulation.

***************************************************************************/

VIDEO_START( contra )
{
	bg_tilemap = tilemap_create(machine, get_bg_tile_info,tilemap_scan_rows,     8,8,32,32);
	fg_tilemap = tilemap_create(machine, get_fg_tile_info,tilemap_scan_rows,8,8,32,32);
	tx_tilemap = tilemap_create(machine, get_tx_tile_info,tilemap_scan_rows,     8,8,32,32);

	private_spriteram = auto_alloc_array(machine, UINT8, 0x800);
	private_spriteram_2 = auto_alloc_array(machine, UINT8, 0x800);

	bg_clip = *video_screen_get_visible_area(machine->primary_screen);
	bg_clip.min_x += 40;

	fg_clip = bg_clip;

	tx_clip = *video_screen_get_visible_area(machine->primary_screen);
	tx_clip.max_x = 39;
	tx_clip.min_x = 0;

	tilemap_set_transparent_pen(fg_tilemap,0);
}


/***************************************************************************

    Memory handlers

***************************************************************************/

WRITE8_HANDLER( contra_fg_vram_w )
{
	contra_fg_vram[offset] = data;
	tilemap_mark_tile_dirty(fg_tilemap,offset);
}

WRITE8_HANDLER( contra_fg_cram_w ){
	contra_fg_cram[offset] = data;
	tilemap_mark_tile_dirty(fg_tilemap,offset);
}

WRITE8_HANDLER( contra_bg_vram_w )
{
	contra_bg_vram[offset] = data;
	tilemap_mark_tile_dirty(bg_tilemap,offset);
}

WRITE8_HANDLER( contra_bg_cram_w )
{
	contra_bg_cram[offset] = data;
	tilemap_mark_tile_dirty(bg_tilemap,offset);
}

WRITE8_HANDLER( contra_text_vram_w )
{
	contra_text_vram[offset] = data;
	tilemap_mark_tile_dirty(tx_tilemap,offset);
}

WRITE8_HANDLER( contra_text_cram_w )
{
	contra_text_cram[offset] = data;
	tilemap_mark_tile_dirty(tx_tilemap,offset);
}

WRITE8_HANDLER( contra_K007121_ctrl_0_w )
{
	if (offset == 3)
	{
		if ((data&0x8)==0)
			memcpy(private_spriteram,spriteram+0x800,0x800);
		else
			memcpy(private_spriteram,spriteram,0x800);
	}
	if (offset == 6)
	{
		if (K007121_ctrlram[0][6] != data)
			tilemap_mark_all_tiles_dirty( fg_tilemap );
	}
	if (offset == 7)
		tilemap_set_flip(fg_tilemap,(data & 0x08) ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);

	K007121_ctrl_0_w(space,offset,data);
}

WRITE8_HANDLER( contra_K007121_ctrl_1_w )
{
	if (offset == 3)
	{
		if ((data&0x8)==0)
			memcpy(private_spriteram_2,spriteram+0x2800,0x800);
		else
			memcpy(private_spriteram_2,spriteram+0x2000,0x800);
	}
	if (offset == 6)
	{
		if (K007121_ctrlram[1][6] != data )
			tilemap_mark_all_tiles_dirty( bg_tilemap );
	}
	if (offset == 7)
		tilemap_set_flip(bg_tilemap,(data & 0x08) ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);

	K007121_ctrl_1_w(space,offset,data);
}



/***************************************************************************

    Display Refresh

***************************************************************************/

static void draw_sprites(running_machine *machine, bitmap_t *bitmap, const rectangle *cliprect, int bank )
{
	const UINT8 *source;
	int base_color = (K007121_ctrlram[bank][6]&0x30)*2;

	if (bank==0) source=private_spriteram;
	else source=private_spriteram_2;

	K007121_sprites_draw(bank,bitmap,cliprect,machine->gfx,machine->colortable,source,base_color,40,0,-1);
}

VIDEO_UPDATE( contra )
{
	rectangle bg_finalclip = bg_clip;
	rectangle fg_finalclip = fg_clip;
	rectangle tx_finalclip = tx_clip;

	sect_rect(&bg_finalclip, cliprect);
	sect_rect(&fg_finalclip, cliprect);
	sect_rect(&tx_finalclip, cliprect);

	set_pens(screen->machine->colortable);

	tilemap_set_scrollx( fg_tilemap,0, K007121_ctrlram[0][0x00] - 40 );
	tilemap_set_scrolly( fg_tilemap,0, K007121_ctrlram[0][0x02] );
	tilemap_set_scrollx( bg_tilemap,0, K007121_ctrlram[1][0x00] - 40 );
	tilemap_set_scrolly( bg_tilemap,0, K007121_ctrlram[1][0x02] );

	tilemap_draw( bitmap,&bg_finalclip, bg_tilemap, 0 ,0);
	tilemap_draw( bitmap,&fg_finalclip, fg_tilemap, 0 ,0);
	draw_sprites( screen->machine,bitmap,cliprect, 0 );
	draw_sprites( screen->machine,bitmap,cliprect, 1 );
	tilemap_draw( bitmap,&tx_finalclip, tx_tilemap, 0 ,0);
	return 0;
}

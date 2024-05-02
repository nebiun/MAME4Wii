#include "driver.h"

/* Sprites probably need to be delayed */
/* Some scroll layers may need to be offset slightly? */
/* Check Sprite Colours after redump */
/* Clean Up */
/* is theres a bg colour register? */

static tilemap *fg_tilemap,*fg2_tilemap,*fg3_tilemap;
extern UINT32 *silkroad_vidram,*silkroad_vidram2,*silkroad_vidram3, *silkroad_sprram, *silkroad_regs;

static void draw_sprites(running_machine *machine, bitmap_t *bitmap, const rectangle *cliprect)
{
	const gfx_element *gfx = machine->gfx[0];
	UINT32 *source = silkroad_sprram;
	UINT32 *finish = source + 0x1000/4;

	while( source < finish )
	{

		int xpos = (source[0] & 0x01ff0000) >> 16;
		int ypos = (source[0] & 0x0000ffff);
		int tileno = (source[1] & 0xffff0000) >> 16;
		int attr = (source[1] & 0x0000ffff);
		int flipx = (attr & 0x0080);
		int width = ((attr & 0x0f00) >> 8) + 1;
		int wcount;
		int color = (attr & 0x003f) ;
		int pri		 =	((attr & 0x1000)>>12);	// Priority (1 = Low)
		int pri_mask =	~((1 << (pri+1)) - 1);	// Above the first "pri" levels

		// attr & 0x2000 -> another priority bit?

		if ( (source[1] & 0xff00) == 0xff00 ) break;

		if ( (attr & 0x8000) == 0x8000 ) tileno+=0x10000;

		if (!flipx) {
			for (wcount=0;wcount<width;wcount++) {
			pdrawgfx_transpen(bitmap,cliprect,gfx,tileno+wcount,color,0,0,xpos+wcount*16+8,ypos,machine->priority_bitmap,pri_mask,0);
			}

		} else {

			for (wcount=width;wcount>0;wcount--) {
			pdrawgfx_transpen(bitmap,cliprect,gfx,tileno+(width-wcount),color,1,0,xpos+wcount*16-16+8,ypos,machine->priority_bitmap,pri_mask,0);
			}

		}

		source += 2;
	}
}


static TILE_GET_INFO( get_fg_tile_info )
{

	int code = ((silkroad_vidram[tile_index] & 0xffff0000) >> 16 );
	int color = ((silkroad_vidram[tile_index] & 0x000001f));
	int flipx =  ((silkroad_vidram[tile_index] & 0x0000080) >> 7);

	code += 0x18000;

	SET_TILE_INFO(
			0,
			code,
			color,
			TILE_FLIPYX(flipx));
}



WRITE32_HANDLER( silkroad_fgram_w )
{
	COMBINE_DATA(&silkroad_vidram[offset]);
	tilemap_mark_tile_dirty(fg_tilemap,offset);
}

static TILE_GET_INFO( get_fg2_tile_info )
{
	int code = ((silkroad_vidram2[tile_index] & 0xffff0000) >> 16 );
	int color = ((silkroad_vidram2[tile_index] & 0x000001f));
	int flipx =  ((silkroad_vidram2[tile_index] & 0x0000080) >> 7);
	code += 0x18000;
	SET_TILE_INFO(
			0,
			code,
			color,
			TILE_FLIPYX(flipx));
}



WRITE32_HANDLER( silkroad_fgram2_w )
{
	COMBINE_DATA(&silkroad_vidram2[offset]);
	tilemap_mark_tile_dirty(fg2_tilemap,offset);
}

static TILE_GET_INFO( get_fg3_tile_info )
{
	int code = ((silkroad_vidram3[tile_index] & 0xffff0000) >> 16 );
	int color = ((silkroad_vidram3[tile_index] & 0x000001f));
	int flipx =  ((silkroad_vidram3[tile_index] & 0x0000080) >> 7);
	code += 0x18000;
	SET_TILE_INFO(
			0,
			code,
			color,
			TILE_FLIPYX(flipx));
}



WRITE32_HANDLER( silkroad_fgram3_w )
{
	COMBINE_DATA(&silkroad_vidram3[offset]);
	tilemap_mark_tile_dirty(fg3_tilemap,offset);
}

VIDEO_START(silkroad)
{
	fg_tilemap  = tilemap_create(machine, get_fg_tile_info, tilemap_scan_rows,16,16,64, 64);
	fg2_tilemap = tilemap_create(machine, get_fg2_tile_info,tilemap_scan_rows,16,16,64, 64);
	fg3_tilemap = tilemap_create(machine, get_fg3_tile_info,tilemap_scan_rows,16,16,64, 64);

	tilemap_set_transparent_pen(fg_tilemap,0);
	tilemap_set_transparent_pen(fg2_tilemap,0);
	tilemap_set_transparent_pen(fg3_tilemap,0);
}

VIDEO_UPDATE(silkroad)
{
	bitmap_fill(screen->machine->priority_bitmap,cliprect,0);
	bitmap_fill(bitmap,cliprect,0x7c0);

	tilemap_set_scrollx( fg_tilemap, 0, ((silkroad_regs[0] & 0xffff0000) >> 16) );
	tilemap_set_scrolly( fg_tilemap, 0, (silkroad_regs[0] & 0x0000ffff) >> 0 );

	tilemap_set_scrolly( fg3_tilemap, 0, (silkroad_regs[1] & 0xffff0000) >> 16 );
	tilemap_set_scrollx( fg3_tilemap, 0, (silkroad_regs[2] & 0xffff0000) >> 16 );

	tilemap_set_scrolly( fg2_tilemap, 0, ((silkroad_regs[5] & 0xffff0000) >> 16));
	tilemap_set_scrollx( fg2_tilemap, 0, (silkroad_regs[2] & 0x0000ffff) >> 0 );

	tilemap_draw(bitmap,cliprect,fg_tilemap, 0,0);
	tilemap_draw(bitmap,cliprect,fg2_tilemap,0,1);
	tilemap_draw(bitmap,cliprect,fg3_tilemap,0,2);
	draw_sprites(screen->machine,bitmap,cliprect);

/*
    popmessage ("Regs %08x %08x %08x %08x %08x",
    silkroad_regs[0],
    silkroad_regs[1],
    silkroad_regs[2],
    silkroad_regs[4],
    silkroad_regs[5]
    );
*/
	return 0;
}

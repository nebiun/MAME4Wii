#include "driver.h"
#include "includes/taito_l.h"


extern UINT8 *taitol_rambanks;

static tilemap *bg18_tilemap;
static tilemap *bg19_tilemap;
static tilemap *ch1a_tilemap;

static int cur_ctrl = 0;
static int horshoes_gfxbank = 0;
static int bankc[4];
static int flipscreen;
#define SPRITERAM_SIZE 0x400
static UINT8 buff_spriteram[SPRITERAM_SIZE];


/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static TILE_GET_INFO( get_bg18_tile_info )
{
	int attr = taitol_rambanks[2*tile_index+0x8000+1];
	int code = taitol_rambanks[2*tile_index+0x8000]
			| ((attr & 0x03) << 8)
			| ((bankc[(attr & 0xc) >> 2]) << 10)
			| (horshoes_gfxbank << 12);

	SET_TILE_INFO(
			0,
			code,
			(attr & 0xf0) >> 4,
			0);
}

static TILE_GET_INFO( get_bg19_tile_info )
{
	int attr = taitol_rambanks[2*tile_index+0x9000+1];
	int code = taitol_rambanks[2*tile_index+0x9000]
			| ((attr & 0x03) << 8)
			| ((bankc[(attr & 0xc) >> 2]) << 10)
			| (horshoes_gfxbank << 12);

	SET_TILE_INFO(
			0,
			code,
			(attr & 0xf0) >> 4,
			0);
}

static TILE_GET_INFO( get_ch1a_tile_info )
{
	int attr = taitol_rambanks[2*tile_index+0xa000+1];
	int code = taitol_rambanks[2*tile_index+0xa000]|((attr&0x01)<<8)|((attr&0x04)<<7);

	SET_TILE_INFO(
			2,
			code,
			(attr & 0xf0) >> 4,
			0);
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

VIDEO_START( taitol )
{
	int i;

	bg18_tilemap = tilemap_create(machine, get_bg18_tile_info,tilemap_scan_rows,8,8,64,32);
	bg19_tilemap = tilemap_create(machine, get_bg19_tile_info,tilemap_scan_rows,     8,8,64,32);
	ch1a_tilemap = tilemap_create(machine, get_ch1a_tile_info,tilemap_scan_rows,8,8,64,32);

	bankc[0] = bankc[1] = bankc[2] = bankc[3] = 0;
	horshoes_gfxbank = 0;
	cur_ctrl = 0;

	tilemap_set_transparent_pen(bg18_tilemap,0);
	tilemap_set_transparent_pen(ch1a_tilemap,0);

	for (i=0;i<256;i++)
		palette_set_color(machine, i, MAKE_RGB(0, 0, 0));

	tilemap_set_scrolldx(ch1a_tilemap,-8,-8);
	tilemap_set_scrolldx(bg18_tilemap,28,-11);
	tilemap_set_scrolldx(bg19_tilemap,38,-21);
}



/***************************************************************************

  Memory handlers

***************************************************************************/

WRITE8_HANDLER( horshoes_bankg_w )
{
	if (horshoes_gfxbank != data)
	{
		horshoes_gfxbank = data;

		tilemap_mark_all_tiles_dirty(bg18_tilemap);
		tilemap_mark_all_tiles_dirty(bg19_tilemap);
	}
}

WRITE8_HANDLER( taitol_bankc_w )
{
	if (bankc[offset] != data)
	{
		bankc[offset] = data;
//      logerror("Bankc %d, %02x (%04x)\n", offset, data, cpu_get_pc(space->cpu));

		tilemap_mark_all_tiles_dirty(bg18_tilemap);
		tilemap_mark_all_tiles_dirty(bg19_tilemap);
	}
}

READ8_HANDLER( taitol_bankc_r )
{
	return bankc[offset];
}


WRITE8_HANDLER( taitol_control_w )
{
//  logerror("Control Write %02x (%04x)\n", data, cpu_get_pc(space->cpu));

	cur_ctrl = data;
//popmessage("%02x",data);

	/* bit 0 unknown */

	/* bit 1 unknown */

	/* bit 3 controls sprite/tile priority - handled in vh_screenrefresh() */

	/* bit 4 flip screen */
	flipscreen = data & 0x10;
	tilemap_set_flip_all(space->machine,flipscreen ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);

	/* bit 5 display enable - handled in vh_screenrefresh() */
}

READ8_HANDLER( taitol_control_r )
{
//  logerror("Control Read %02x (%04x)\n", cur_ctrl, cpu_get_pc(space->cpu));
	return cur_ctrl;
}

void taitol_chardef14_m(running_machine *machine, int offset)
{
	gfx_element_mark_dirty(machine->gfx[2], offset/32 + 0);
}

void taitol_chardef15_m(running_machine *machine, int offset)
{
	gfx_element_mark_dirty(machine->gfx[2], offset/32 + 128);
}

void taitol_chardef16_m(running_machine *machine, int offset)
{
	gfx_element_mark_dirty(machine->gfx[2], offset/32 + 256);
}

void taitol_chardef17_m(running_machine *machine, int offset)
{
	gfx_element_mark_dirty(machine->gfx[2], offset/32 + 384);
}

void taitol_chardef1c_m(running_machine *machine, int offset)
{
	gfx_element_mark_dirty(machine->gfx[2], offset/32 + 512);
}

void taitol_chardef1d_m(running_machine *machine, int offset)
{
	gfx_element_mark_dirty(machine->gfx[2], offset/32 + 640);
}

void taitol_chardef1e_m(running_machine *machine, int offset)
{
	gfx_element_mark_dirty(machine->gfx[2], offset/32 + 768);
}

void taitol_chardef1f_m(running_machine *machine, int offset)
{
	gfx_element_mark_dirty(machine->gfx[2], offset/32 + 896);
}

void taitol_bg18_m(running_machine *machine, int offset)
{
	tilemap_mark_tile_dirty(bg18_tilemap,offset/2);
}

void taitol_bg19_m(running_machine *machine, int offset)
{
	tilemap_mark_tile_dirty(bg19_tilemap,offset/2);
}

void taitol_char1a_m(running_machine *machine, int offset)
{
	tilemap_mark_tile_dirty(ch1a_tilemap,offset/2);
}

void taitol_obj1b_m(running_machine *machine, int offset)
{
#if 0
	if (offset>=0x3f0 && offset<=0x3ff)
	{
		/* scroll, handled in vh-screenrefresh */
	}
#endif
}



/***************************************************************************

  Display refresh

***************************************************************************/

/*
    Sprite format:
    00: xxxxxxxx tile number (low)
    01: xxxxxxxx tile number (high)
    02: ----xxxx color
        ----x--- priority
    03: -------x flip x
        ------x- flip y
    04: xxxxxxxx x position (low)
    05: -------x x position (high)
    06: xxxxxxxx y position
    07: xxxxxxxx unknown / ignored? Seems just garbage in many cases, e.g
                 plgirs2 bullets and raimais big bosses.
*/

static void draw_sprites(running_machine *machine, bitmap_t *bitmap,const rectangle *cliprect)
{
	int offs;


	/* at spriteram + 0x3f0 and 03f8 are the tilemap control registers;
        spriteram + 0x3e8 seems to be unused
    */
	for (offs = 0;offs < SPRITERAM_SIZE-3*8;offs += 8)
	{
		int code,color,sx,sy,flipx,flipy;

		color = buff_spriteram[offs + 2] & 0x0f;
		code = buff_spriteram[offs] | (buff_spriteram[offs + 1] << 8);

		code |= (horshoes_gfxbank & 0x03) << 10;

		sx = buff_spriteram[offs + 4] | ((buff_spriteram[offs + 5] & 1) << 8);
		sy = buff_spriteram[offs + 6];
		if (sx >= 320) sx -= 512;
		flipx = buff_spriteram[offs + 3] & 0x01;
		flipy = buff_spriteram[offs + 3] & 0x02;

		if (flipscreen)
		{
			sx = 304 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		pdrawgfx_transpen(bitmap,cliprect,machine->gfx[1],
				code,
				color,
				flipx,flipy,
				sx,sy,
				machine->priority_bitmap,
				(color & 0x08) ? 0xaa : 0x00,0);
	}
}


VIDEO_UPDATE( taitol )
{
	int dx,dy;


	dx = taitol_rambanks[0xb3f4]|(taitol_rambanks[0xb3f5]<<8);
	if (flipscreen)
		dx = ((dx & 0xfffc) | ((dx - 3) & 0x0003)) ^ 0xf;
	dy = taitol_rambanks[0xb3f6];
	tilemap_set_scrollx(bg18_tilemap,0,-dx);
	tilemap_set_scrolly(bg18_tilemap,0,-dy);

	dx = taitol_rambanks[0xb3fc]|(taitol_rambanks[0xb3fd]<<8);
	if (flipscreen)
		dx = ((dx & 0xfffc) | ((dx - 3) & 0x0003)) ^ 0xf;
	dy = taitol_rambanks[0xb3fe];
	tilemap_set_scrollx(bg19_tilemap,0,-dx);
	tilemap_set_scrolly(bg19_tilemap,0,-dy);


	if (cur_ctrl & 0x20)	/* display enable */
	{
		bitmap_fill(screen->machine->priority_bitmap,cliprect,0);

		tilemap_draw(bitmap,cliprect,bg19_tilemap,0,0);

		if (cur_ctrl & 0x08)	/* sprites always over BG1 */
			tilemap_draw(bitmap,cliprect,bg18_tilemap,0,0);
		else					/* split priority */
			tilemap_draw(bitmap,cliprect,bg18_tilemap,0,1);
		draw_sprites(screen->machine, bitmap,cliprect);

		tilemap_draw(bitmap,cliprect,ch1a_tilemap,0,0);
	}
	else
		bitmap_fill(bitmap,cliprect,screen->machine->pens[0]);
	return 0;
}



VIDEO_EOF( taitol )
{
	UINT8 *spriteram = taitol_rambanks + 0xb000;

	memcpy(buff_spriteram,spriteram,SPRITERAM_SIZE);
}

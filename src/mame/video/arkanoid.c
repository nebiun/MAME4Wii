/***************************************************************************

  video.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "includes/arkanoid.h"

static UINT8 gfxbank, palettebank;

static tilemap *bg_tilemap;

WRITE8_HANDLER( arkanoid_videoram_w )
{
	videoram[offset] = data;
	tilemap_mark_tile_dirty(bg_tilemap, offset / 2);
}

WRITE8_HANDLER( arkanoid_d008_w )
{
	int bank;

	/* bits 0 and 1 flip X and Y, I don't know which is which */
	if (flip_screen_x_get(space->machine) != (data & 0x01))
	{
		flip_screen_x_set(space->machine, data & 0x01);
		tilemap_mark_all_tiles_dirty(bg_tilemap);
	}

	if (flip_screen_y_get(space->machine) != (data & 0x02))
	{
		flip_screen_y_set(space->machine, data & 0x02);
		tilemap_mark_all_tiles_dirty(bg_tilemap);
	}

	/* bit 2 selects the input paddle */
	arkanoid_paddle_select = data & 0x04;

	/* bit 3 is coin lockout (but not the service coin) */
	coin_lockout_w(0, !(data & 0x08));
	coin_lockout_w(1, !(data & 0x08));

	/* bit 4 is unknown */

	/* bits 5 and 6 control gfx bank and palette bank. They are used together */
	/* so I don't know which is which. */
	bank = (data & 0x20) >> 5;

	if (gfxbank != bank)
	{
		gfxbank = bank;
		tilemap_mark_all_tiles_dirty(bg_tilemap);
	}

	bank = (data & 0x40) >> 6;

	if (palettebank != bank)
	{
		palettebank = bank;
		tilemap_mark_all_tiles_dirty(bg_tilemap);
	}

	/* BM:  bit 7 is suspected to be MCU reset, the evidence for this is that
     the games tilt mode reset sequence shows the main CPU must be able to
     directly control the reset line of the MCU, else the game will crash
     leaving the tilt screen (as the MCU is now out of sync with main CPU
     which resets itself).  This bit is the likely candidate as it is flipped
     early in bootup just prior to accessing the MCU for the first time. */
	if (cputag_get_cpu(space->machine, "mcu") != NULL)	// Bootlegs don't have the MCU but still set this bit
		cputag_set_input_line(space->machine, "mcu", INPUT_LINE_RESET, (data & 0x80) ? CLEAR_LINE : ASSERT_LINE);
}

/* different hook-up, everything except for bits 0-1 and 7 aren't tested afaik. */
WRITE8_HANDLER( tetrsark_d008_w )
{
	int bank;

	/* bits 0 and 1 flip X and Y, I don't know which is which */
	if (flip_screen_x_get(space->machine) != (data & 0x01))
	{
		flip_screen_x_set(space->machine, data & 0x01);
		tilemap_mark_all_tiles_dirty(bg_tilemap);
	}

	if (flip_screen_y_get(space->machine) != (data & 0x02))
	{
		flip_screen_y_set(space->machine, data & 0x02);
		tilemap_mark_all_tiles_dirty(bg_tilemap);
	}

	/* bit 2 selects the input paddle? */
	arkanoid_paddle_select = data & 0x04;

	/* bit 3-4 is unknown? */

	/* bits 5 and 6 control gfx bank and palette bank. They are used together */
	/* so I don't know which is which.? */
	bank = (data & 0x20) >> 5;

	if (gfxbank != bank)
	{
		gfxbank = bank;
		tilemap_mark_all_tiles_dirty(bg_tilemap);
	}

	bank = (data & 0x40) >> 6;

	if (palettebank != bank)
	{
		palettebank = bank;
		tilemap_mark_all_tiles_dirty(bg_tilemap);
	}

	/* bit 7 is coin lockout (but not the service coin) */
	coin_lockout_w(0, !(data & 0x80));
	coin_lockout_w(1, !(data & 0x80));
}


static TILE_GET_INFO( get_bg_tile_info )
{
	int offs = tile_index * 2;
	int code = videoram[offs + 1] + ((videoram[offs] & 0x07) << 8) + 2048 * gfxbank;
	int color = ((videoram[offs] & 0xf8) >> 3) + 32 * palettebank;

	SET_TILE_INFO(0, code, color, 0);
}

VIDEO_START( arkanoid )
{
	bg_tilemap = tilemap_create(machine, get_bg_tile_info, tilemap_scan_rows,
		 8, 8, 32, 32);

	state_save_register_global(machine, gfxbank);
	state_save_register_global(machine, palettebank);
}

static void draw_sprites(running_machine *machine, bitmap_t *bitmap, const rectangle *cliprect)
{
	int offs;

	for (offs = 0;offs < spriteram_size;offs += 4)
	{
		int sx,sy,code;

		sx = spriteram[offs];
		sy = 248 - spriteram[offs + 1];
		if (flip_screen_x_get(machine)) sx = 248 - sx;
		if (flip_screen_y_get(machine)) sy = 248 - sy;

		code = spriteram[offs + 3] + ((spriteram[offs + 2] & 0x03) << 8) + 1024 * gfxbank;

		drawgfx_transpen(bitmap,cliprect,machine->gfx[0],
				2 * code,
				((spriteram[offs + 2] & 0xf8) >> 3) + 32 * palettebank,
				flip_screen_x_get(machine),flip_screen_y_get(machine),
				sx,sy + (flip_screen_y_get(machine) ? 8 : -8),0);
		drawgfx_transpen(bitmap,cliprect,machine->gfx[0],
				2 * code + 1,
				((spriteram[offs + 2] & 0xf8) >> 3) + 32 * palettebank,
				flip_screen_x_get(machine),flip_screen_y_get(machine),
				sx,sy,0);
	}
}


VIDEO_UPDATE( arkanoid )
{
	tilemap_draw(bitmap, cliprect, bg_tilemap, 0, 0);
	draw_sprites(screen->machine, bitmap, cliprect);
	return 0;
}

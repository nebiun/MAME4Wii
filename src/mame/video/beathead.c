/***************************************************************************

    Atari "Stella on Steroids" hardware

****************************************************************************/

#include "driver.h"
#include "beathead.h"



/*************************************
 *
 *  Globals we own
 *
 *************************************/

UINT32 *	beathead_vram_bulk_latch;
UINT32 *	beathead_palette_select;



/*************************************
 *
 *  Statics
 *
 *************************************/

static UINT32				finescroll;
static offs_t				vram_latch_offset;

static offs_t				hsyncram_offset;
static offs_t				hsyncram_start;
static UINT8 *				hsyncram;



/*************************************
 *
 *  Video start/stop
 *
 *************************************/

VIDEO_START( beathead )
{
	hsyncram = auto_alloc_array(machine, UINT8, 0x800);
}



/*************************************
 *
 *  VRAM handling
 *
 *************************************/

WRITE32_HANDLER( beathead_vram_transparent_w )
{
	/* writes to this area appear to handle transparency */
	if (!(data & 0x000000ff)) mem_mask &= ~0x000000ff;
	if (!(data & 0x0000ff00)) mem_mask &= ~0x0000ff00;
	if (!(data & 0x00ff0000)) mem_mask &= ~0x00ff0000;
	if (!(data & 0xff000000)) mem_mask &= ~0xff000000;
	COMBINE_DATA(&videoram32[offset]);
}


WRITE32_HANDLER( beathead_vram_bulk_w )
{
	/* it appears that writes to this area pass in a mask for 4 words in VRAM */
	/* allowing them to be filled from a preset latch */
	offset &= ~3;
	data = data & mem_mask & 0x0f0f0f0f;

	/* for now, just handle the bulk fill case; the others we'll catch later */
	if (data == 0x0f0f0f0f)
		videoram32[offset+0] =
		videoram32[offset+1] =
		videoram32[offset+2] =
		videoram32[offset+3] = *beathead_vram_bulk_latch;
	else
		logerror("Detected bulk VRAM write with mask %08x\n", data);
}


WRITE32_HANDLER( beathead_vram_latch_w )
{
	/* latch the address */
	vram_latch_offset = (4 * offset) & 0x7ffff;
}


WRITE32_HANDLER( beathead_vram_copy_w )
{
	/* copy from VRAM to VRAM, for 1024 bytes */
	offs_t dest_offset = (4 * offset) & 0x7ffff;
	memcpy(&videoram32[dest_offset / 4], &videoram32[vram_latch_offset / 4], 0x400);
}



/*************************************
 *
 *  Scroll offset handling
 *
 *************************************/

WRITE32_HANDLER( beathead_finescroll_w )
{
	UINT32 oldword = finescroll;
	UINT32 newword = COMBINE_DATA(&finescroll);

	/* if VBLANK is going off on a scanline other than the last, suspend time */
	if ((oldword & 8) && !(newword & 8) && video_screen_get_vpos(space->machine->primary_screen) != 261)
	{
		logerror("Suspending time! (scanline = %d)\n", video_screen_get_vpos(space->machine->primary_screen));
		cputag_set_input_line(space->machine, "maincpu", INPUT_LINE_HALT, ASSERT_LINE);
	}
}



/*************************************
 *
 *  Palette handling
 *
 *************************************/

WRITE32_HANDLER( beathead_palette_w )
{
	int newword = COMBINE_DATA(&paletteram32[offset]);
	int r = ((newword >> 9) & 0x3e) | ((newword >> 15) & 0x01);
	int g = ((newword >> 4) & 0x3e) | ((newword >> 15) & 0x01);
	int b = ((newword << 1) & 0x3e) | ((newword >> 15) & 0x01);
	palette_set_color_rgb(space->machine, offset, pal6bit(r), pal6bit(g), pal6bit(b));
}



/*************************************
 *
 *  HSYNC RAM handling
 *
 *************************************/

READ32_HANDLER( beathead_hsync_ram_r )
{
	/* offset 0 is probably write-only */
	if (offset == 0)
		logerror("%08X:Unexpected HSYNC RAM read at offset 0\n", cpu_get_previouspc(space->cpu));

	/* offset 1 reads the data */
	else
		return hsyncram[hsyncram_offset];

	return 0;
}

WRITE32_HANDLER( beathead_hsync_ram_w )
{
	/* offset 0 selects the address, and can specify the start address */
	if (offset == 0)
	{
		COMBINE_DATA(&hsyncram_offset);
		if (hsyncram_offset & 0x800)
			hsyncram_start = hsyncram_offset & 0x7ff;
	}

	/* offset 1 writes the data */
	else
		COMBINE_DATA(&hsyncram[hsyncram_offset]);
}



/*************************************
 *
 *  Main screen refresher
 *
 *************************************/

VIDEO_UPDATE( beathead )
{
	int x, y;

	/* generate the final screen */
	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		pen_t pen_base = (*beathead_palette_select & 0x7f) * 256;
		UINT16 scanline[336];

		/* blanking */
		if (finescroll & 8)
			for (x = cliprect->min_x; x <= cliprect->max_x; x++)
				scanline[x] = pen_base;

		/* non-blanking */
		else
		{
			offs_t scanline_offset = vram_latch_offset + (finescroll & 3);
			offs_t src = scanline_offset + cliprect->min_x;

			/* unswizzle the scanline first */
			for (x = cliprect->min_x; x <= cliprect->max_x; x++)
				scanline[x] = pen_base | ((UINT8 *)videoram32)[BYTE4_XOR_LE(src++)];
		}

		/* then draw it */
		draw_scanline16(bitmap, cliprect->min_x, y, cliprect->max_x - cliprect->min_x + 1, &scanline[cliprect->min_x], NULL);
	}
	return 0;
}

/***************************************************************************

    bitmap.c

    Core bitmap routines.

****************************************************************************

    Copyright Aaron Giles
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:

        * Redistributions of source code must retain the above copyright
          notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
          notice, this list of conditions and the following disclaimer in
          the documentation and/or other materials provided with the
          distribution.
        * Neither the name 'MAME' nor the names of its contributors may be
          used to endorse or promote products derived from this software
          without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY AARON GILES ''AS IS'' AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL AARON GILES BE LIABLE FOR ANY DIRECT,
    INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
    STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
    IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/
#include "mamecore.h"
#include "bitmap.h"



/***************************************************************************
    BITMAP ALLOCATION/CONFIGURATION
***************************************************************************/

/*-------------------------------------------------
    bitmap_alloc_slop -- allocate a new bitmap with
    additional slop on the borders
-------------------------------------------------*/
bitmap_t *_bitmap_alloc_slop(int width, int height, int xslop, int yslop, bitmap_format format, const char *file, int line)
{
	int bpp = bitmap_format_to_bpp(format);
	size_t allocbytes;
	bitmap_t *bitmap;
	int rowpixels;

	/* fail if invalid format */
	if (bpp == 0)
		return NULL;

	/* allocate the bitmap itself */
	bitmap = (bitmap_t *)_wii_malloc(sizeof(*bitmap),file,line);
	if (bitmap == NULL) {
		/* otherwise, die horribly */
		fatalerror("Failed to allocate %d bytes (%s:%d)", (int)sizeof(*bitmap),file,line);
		return NULL;
	}
	memset(bitmap, 0, sizeof(*bitmap));

	/* round the width to a multiple of 8 and add some padding */
	rowpixels = (width + 2 * xslop + 7) & ~7;

	/* allocate memory for the bitmap itself */
	allocbytes = rowpixels * (height + 2 * yslop) * bpp / 8;
	bitmap->alloc = _wii_malloc(allocbytes,file,line);
	if (bitmap->alloc == NULL)
	{
		free(bitmap);
		fatalerror("Failed to allocate %d bytes (%s:%d)", (int)allocbytes,file,line);
		return NULL;
	}

	/* clear to 0 by default */
	memset(bitmap->alloc, 0, allocbytes);

	/* fill in the data */
	bitmap->format = format;
	bitmap->width = width;
	bitmap->height = height;
	bitmap->bpp = bpp;
	bitmap->rowpixels = rowpixels;
	bitmap->base = (UINT8 *)bitmap->alloc + (rowpixels * yslop + xslop) * (bpp / 8);
	bitmap->cliprect.min_x = 0;
	bitmap->cliprect.max_x = width - 1;
	bitmap->cliprect.min_y = 0;
	bitmap->cliprect.max_y = height - 1;

	return bitmap;
}


/*-------------------------------------------------
    bitmap_wrap -- wrap an existing memory buffer
    as a bitmap
-------------------------------------------------*/

bitmap_t *_bitmap_wrap(void *base, int width, int height, int rowpixels, bitmap_format format, const char *file, int line)
{
	int bpp = bitmap_format_to_bpp(format);
	bitmap_t *bitmap;

	/* fail if invalid format */
	if (bpp == 0)
		return NULL;

	/* allocate memory */
	bitmap = (bitmap_t *)_wii_malloc(sizeof(*bitmap), file, line);
	if (bitmap == NULL)
		return NULL;
	memset(bitmap, 0, sizeof(*bitmap));

	/* fill in the data */
	bitmap->format = format;
	bitmap->width = width;
	bitmap->height = height;
	bitmap->bpp = bpp;
	bitmap->rowpixels = rowpixels;
	bitmap->base = base;
	bitmap->cliprect.min_x = 0;
	bitmap->cliprect.max_x = width - 1;
	bitmap->cliprect.min_y = 0;
	bitmap->cliprect.max_y = height - 1;

	return bitmap;
}


/*-------------------------------------------------
    bitmap_set_palette -- associate a palette with
    a bitmap
-------------------------------------------------*/

void bitmap_set_palette(bitmap_t *bitmap, palette_t *palette)
{
	/* first dereference any existing palette */
	if (bitmap->palette != NULL)
	{
		palette_deref(bitmap->palette);
		bitmap->palette = NULL;
	}

	/* then reference any new palette */
	if (palette != NULL)
	{
		palette_ref(palette);
		bitmap->palette = palette;
	}
}


/*-------------------------------------------------
    bitmap_free -- release memory allocated for
    a bitmap
-------------------------------------------------*/

void bitmap_free(bitmap_t *bitmap)
{
	/* dereference the palette */
	if (bitmap->palette != NULL)
		palette_deref(bitmap->palette);

	/* free any allocated memory */
	if (bitmap->alloc != NULL)
		free(bitmap->alloc);

	/* free the bitmap */
	free(bitmap);
}



/***************************************************************************
    BITMAP DRAWING
***************************************************************************/

/*-------------------------------------------------
    bitmap_fill -- fill a bitmap with a solid
    color
-------------------------------------------------*/

void bitmap_fill(bitmap_t *dest, const rectangle *cliprect, UINT32 color)
{
	rectangle fill = dest->cliprect;
	int x, y;

	/* if we have a cliprect, intersect with that */
	if (cliprect != NULL)
		sect_rect(&fill, cliprect);

	/* early out if nothing to do */
	if (fill.min_x > fill.max_x || fill.min_y > fill.max_y)
		return;

	/* based on the bpp go from there */
	switch (dest->bpp)
	{
		case 8:
			/* 8bpp always uses memset */
			for (y = fill.min_y; y <= fill.max_y; y++)
				memset(BITMAP_ADDR8(dest, y, fill.min_x), (UINT8)color, fill.max_x + 1 - fill.min_x);
			break;

		case 16:
			/* 16bpp can use memset if the bytes are equal */
			if ((UINT8)(color >> 8) == (UINT8)color)
			{
				for (y = fill.min_y; y <= fill.max_y; y++)
					memset(BITMAP_ADDR16(dest, y, fill.min_x), (UINT8)color, (fill.max_x + 1 - fill.min_x) * 2);
			}
			else
			{
				UINT16 *destrow, *destrow0;

				/* Fill the first line the hard way */
				destrow  = BITMAP_ADDR16(dest, fill.min_y, 0);
				for (x = fill.min_x; x <= fill.max_x; x++)
					destrow[x] = (UINT16)color;

				/* For the other lines, just copy the first one */
				destrow0 = BITMAP_ADDR16(dest, fill.min_y, fill.min_x);
				for (y = fill.min_y + 1; y <= fill.max_y; y++)
				{
					destrow = BITMAP_ADDR16(dest, y, fill.min_x);
					memcpy(destrow, destrow0, (fill.max_x + 1 - fill.min_x) * 2);
				}
			}
			break;

		case 32:
			/* 32bpp can use memset if the bytes are equal */
			if ((UINT8)(color >> 8) == (UINT8)color && (UINT16)(color >> 16) == (UINT16)color)
			{
				for (y = fill.min_y; y <= fill.max_y; y++)
					memset(BITMAP_ADDR32(dest, y, fill.min_x), (UINT8)color, (fill.max_x + 1 - fill.min_x) * 4);
			}
			else
			{
				UINT32 *destrow, *destrow0;

				/* Fill the first line the hard way */
				destrow  = BITMAP_ADDR32(dest, fill.min_y, 0);
				for (x = fill.min_x; x <= fill.max_x; x++)
					destrow[x] = (UINT32)color;

				/* For the other lines, just copy the first one */
				destrow0 = BITMAP_ADDR32(dest, fill.min_y, fill.min_x);
				for (y = fill.min_y + 1; y <= fill.max_y; y++)
				{
					destrow = BITMAP_ADDR32(dest, y, fill.min_x);
					memcpy(destrow, destrow0, (fill.max_x + 1 - fill.min_x) * 4);
				}
			}
			break;

		case 64:
			/* 64bpp can use memset if the bytes are equal */
			if ((UINT8)(color >> 8) == (UINT8)color && (UINT16)(color >> 16) == (UINT16)color)
			{
				for (y = fill.min_y; y <= fill.max_y; y++)
					memset(BITMAP_ADDR64(dest, y, fill.min_x), (UINT8)color, (fill.max_x + 1 - fill.min_x) * 4);
			}
			else
			{
				UINT64 *destrow, *destrow0;

				/* Fill the first line the hard way */
				destrow  = BITMAP_ADDR64(dest, fill.min_y, 0);
				for (x = fill.min_x; x <= fill.max_x; x++)
					destrow[x] = (UINT64)color;

				/* For the other lines, just copy the first one */
				destrow0 = BITMAP_ADDR64(dest, fill.min_y, fill.min_x);
				for (y = fill.min_y + 1; y <= fill.max_y; y++)
				{
					destrow = BITMAP_ADDR64(dest, y, fill.min_x);
					memcpy(destrow, destrow0, (fill.max_x + 1 - fill.min_x) * 4);
				}
			}
			break;
	}
}



/***************************************************************************
    BITMAP UTILITIES
***************************************************************************/

/*-------------------------------------------------
    bitmap_format_to_bpp - given a format, return
    the bpp
-------------------------------------------------*/

int bitmap_format_to_bpp(bitmap_format format)
{
	/* choose a depth for the format */
	switch (format)
	{
		case BITMAP_FORMAT_INDEXED8:
			return 8;

		case BITMAP_FORMAT_INDEXED16:
		case BITMAP_FORMAT_RGB15:
		case BITMAP_FORMAT_YUY16:
			return 16;

		case BITMAP_FORMAT_INDEXED32:
		case BITMAP_FORMAT_RGB32:
		case BITMAP_FORMAT_ARGB32:
			return 32;

		case BITMAP_FORMAT_INDEXED64:
			return 64;

		default:
			break;
	}
	return 0;
}

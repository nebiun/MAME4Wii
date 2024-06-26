/***************************************************************************

                      -= IGS Blitter Based Hardware =-

                    driver by   Luca Elia (l.elia@tin.it)
            code decrypted by   Olivier Galibert


CPU     :   68000
Sound   :   M6295 or ICS2115 + Optional FM
Video   :   IGS011
NVRAM   :   Battery for main RAM

---------------------------------------------------------------------------
Year + Game               FM Sound    Chips
---------------------------------------------------------------------------
1995  Da Ban Cheng        -           IGS011
1995  Long Hu Bang        -           IGS011
1995  Zhong Guo Long      YM3812      IGS003, IGS011, IGS012, IGSD0301
1996  Long Hu Bang II     YM2413      IGS011
1996  Wan Li Chang Cheng  -           ?
1996  Xing Yen Man Guan   -           ?
1996  Virtua Bowling      -           IGS011, IGS012
---------------------------------------------------------------------------

To do:

- Protection emulation instead of patching the roms
- A few graphical bugs
- vbowl, vbowlj: trackball support, sound is slow and low volume

- lhb: in the copyright screen the '5' in '1995' is drawn by the cpu on layer 5,
  but with wrong colors (since the top nibble of the affected pixels is left to 0xf)
  (drgnwrld is like this too, maybe hacked, or a cheap year replacement by IGS)

- dbc: in the title screen the '5' in '1995' is drawn by the cpu with wrong colors.
  (see above comment)
  Also the background palette is wrong since the fade routine is called with wrong
  parameters, but in this case the PCB does the same.

Notes:

- In most games, keep test button pressed during boot for another test mode

***************************************************************************/

#include "driver.h"
#include "cpu/m68000/m68000.h"
#include "deprecat.h"
#include "sound/okim6295.h"
#include "sound/2413intf.h"
#include "sound/3812intf.h"
#include "sound/ics2115.h"

/***************************************************************************

    Video

    There are 8 non scrolling layers as big as the screen (512 x 256).
    Each layer has 256 colors and its own palette.

    There are 8 priority codes with RAM associated to each (8 x 256 values).
    For each screen position, to determine which pixel to display, the video
    chip associates a bit to the opacity of that pixel for each layer
    (1 = trasparent) to form an address into the selected priority RAM.
    The value at that address (0-7) is the topmost layer.

***************************************************************************/

static UINT8 *layer[8];

static UINT16 igs_priority, *igs_priority_ram;

static UINT8 chmplst2_pen_hi;	// high 3 bits of pens (chmplst2 only)


static WRITE16_HANDLER( igs_priority_w )
{
	COMBINE_DATA(&igs_priority);

//  logerror("%06x: igs_priority = %02x\n", cpu_get_pc(space->cpu), igs_priority);

	if (data & ~0x7)
		logerror("%06x: warning, unknown bits written to igs_priority = %02x\n", cpu_get_pc(space->cpu), igs_priority);
}


static VIDEO_START(igs)
{
	int i;

	for (i = 0; i < 8; i++)
	{
		layer[i] = auto_alloc_array(machine, UINT8, 512 * 256);
	}

	chmplst2_pen_hi = 0;
}

static VIDEO_UPDATE(igs)
{
#ifdef MAME_DEBUG
	int layer_enable = -1;
#endif

	int x,y,l,scr_addr,pri_addr;
	UINT16 *pri_ram;

#ifdef MAME_DEBUG
	if (input_code_pressed(screen->machine, KEYCODE_Z))
	{
		int mask = 0;
		if (input_code_pressed(screen->machine, KEYCODE_Q))	mask |= 0x01;
		if (input_code_pressed(screen->machine, KEYCODE_W))	mask |= 0x02;
		if (input_code_pressed(screen->machine, KEYCODE_E))	mask |= 0x04;
		if (input_code_pressed(screen->machine, KEYCODE_R))	mask |= 0x08;
		if (input_code_pressed(screen->machine, KEYCODE_A))	mask |= 0x10;
		if (input_code_pressed(screen->machine, KEYCODE_S))	mask |= 0x20;
		if (input_code_pressed(screen->machine, KEYCODE_D))	mask |= 0x40;
		if (input_code_pressed(screen->machine, KEYCODE_F))	mask |= 0x80;
		if (mask)	layer_enable &= mask;
	}
#endif

	pri_ram = &igs_priority_ram[(igs_priority & 7) * 512/2];

	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		for (x = cliprect->min_x; x <= cliprect->max_x; x++)
		{
			scr_addr = x + y * 512;
			pri_addr = 0xff;

			for (l = 0; l < 8; l++)
			{
				if (	(layer[l][scr_addr] != 0xff)
#ifdef MAME_DEBUG
						&& (layer_enable & (1 << l))
#endif
					)
					pri_addr &= ~(1 << l);
			}


			l	=	pri_ram[pri_addr] & 7;

#ifdef MAME_DEBUG
			if ((layer_enable != -1) && (pri_addr == 0xff))
				*BITMAP_ADDR16(bitmap, y, x) = get_black_pen(screen->machine);
			else
#endif
				*BITMAP_ADDR16(bitmap, y, x) = layer[l][scr_addr] | (l << 8);
		}
	}
	return 0;
}

/***************************************************************************

    In addition to the blitter, the CPU can also read from and write to
    the framebuffers for the 8 layers, seen as 0x100000 bytes in memory.
    The first half contains layers 0-3. Layers 4-7 are in the other half.

    The layers are interleaved:

    - bytes 0x00000-0x00003 contain the 1st pixel of layer 0,1,2,3
    - bytes 0x00004-0x00007 contain the 2nd pixel of layer 0,1,2,3
    ...
    - bytes 0x80000-0x80003 contain the 1st pixel of layer 4,5,6,7
    - bytes 0x80004-0x80007 contain the 2nd pixel of layer 4,5,6,7

    and so on.

***************************************************************************/

static READ16_HANDLER( igs_layers_r )
{
	int layer0 = ((offset & (0x80000/2)) ? 4 : 0) + ((offset & 1) ? 0 : 2);

	UINT8 *l0 = layer[layer0];
	UINT8 *l1 = layer[layer0+1];

	offset >>= 1;
	offset &= 0x1ffff;

	return (l0[offset] << 8) | l1[offset];
}

static WRITE16_HANDLER( igs_layers_w )
{
	UINT16 word;

	int layer0 = ((offset & (0x80000/2)) ? 4 : 0) + ((offset & 1) ? 0 : 2);

	UINT8 *l0 = layer[layer0];
	UINT8 *l1 = layer[layer0+1];

	offset >>= 1;
	offset &= 0x1ffff;

	word = (l0[offset] << 8) | l1[offset];
	COMBINE_DATA(&word);
	l0[offset] = word >> 8;
	l1[offset] = word;
}

/***************************************************************************

    Blitter

***************************************************************************/

static struct
{

	UINT16	x, y, w, h,
			gfx_lo, gfx_hi,
			depth,
			pen,
			flags;

}	blitter;


static WRITE16_HANDLER( igs_blit_x_w )		{	COMBINE_DATA(&blitter.x);		}
static WRITE16_HANDLER( igs_blit_y_w )		{	COMBINE_DATA(&blitter.y);		}
static WRITE16_HANDLER( igs_blit_gfx_lo_w )	{	COMBINE_DATA(&blitter.gfx_lo);	}
static WRITE16_HANDLER( igs_blit_gfx_hi_w )	{	COMBINE_DATA(&blitter.gfx_hi);	}
static WRITE16_HANDLER( igs_blit_w_w )		{	COMBINE_DATA(&blitter.w);		}
static WRITE16_HANDLER( igs_blit_h_w )		{	COMBINE_DATA(&blitter.h);		}
static WRITE16_HANDLER( igs_blit_depth_w )	{	COMBINE_DATA(&blitter.depth);	}
static WRITE16_HANDLER( igs_blit_pen_w )	{	COMBINE_DATA(&blitter.pen);		}


static WRITE16_HANDLER( igs_blit_flags_w )
{
	int x, xstart, xend, xinc, flipx;
	int y, ystart, yend, yinc, flipy;
	int depth4, clear, opaque, z;
	UINT8 trans_pen, clear_pen, pen_hi, *dest;
	UINT8 pen = 0;

	UINT8 *gfx		=	memory_region(space->machine, "gfx1");
	UINT8 *gfx2		=	memory_region(space->machine, "gfx2");
	int gfx_size	=	memory_region_length(space->machine, "gfx1");
	int gfx2_size	=	memory_region_length(space->machine, "gfx2");

	const rectangle *clip = video_screen_get_visible_area(space->machine->primary_screen);

	COMBINE_DATA(&blitter.flags);

	logerror("%06x: blit x %03x, y %03x, w %03x, h %03x, gfx %03x%04x, depth %02x, pen %02x, flags %03x\n", cpu_get_pc(space->cpu),
					blitter.x,blitter.y,blitter.w,blitter.h,blitter.gfx_hi,blitter.gfx_lo,blitter.depth,blitter.pen,blitter.flags);

	dest	=	layer[	   blitter.flags & 0x0007	];
	opaque	=			 !(blitter.flags & 0x0008);
	clear	=			   blitter.flags & 0x0010;
	flipx	=			   blitter.flags & 0x0020;
	flipy	=			   blitter.flags & 0x0040;
	if					(!(blitter.flags & 0x0400)) return;

	pen_hi	=	(chmplst2_pen_hi & 0x07) << 5;

	// pixel address
	z		=	blitter.gfx_lo  + (blitter.gfx_hi << 16);

	// what were they smoking???
	depth4	=	!((blitter.flags & 0x7) < (4 - (blitter.depth & 0x7))) ||
				(z & 0x800000);		// see chmplst2

	z &= 0x7fffff;

	if (depth4)
	{
		z	*=	2;
		if (gfx2 && (blitter.gfx_hi & 0x80))	trans_pen = 0x1f;	// chmplst2
		else									trans_pen = 0x0f;

		clear_pen = blitter.pen | 0xf0;
	}
	else
	{
		if (gfx2)	trans_pen = 0x1f;	// vbowl
		else		trans_pen = 0xff;

		clear_pen = blitter.pen;
	}

	xstart = (blitter.x & 0x1ff) - (blitter.x & 0x200);
	ystart = (blitter.y & 0x0ff) - (blitter.y & 0x100);

	if (flipx)	{ xend = xstart - (blitter.w & 0x1ff) - 1;	xinc = -1; }
	else		{ xend = xstart + (blitter.w & 0x1ff) + 1;	xinc =  1; }

	if (flipy)	{ yend = ystart - (blitter.h & 0x0ff) - 1;	yinc = -1; }
	else		{ yend = ystart + (blitter.h & 0x0ff) + 1;	yinc =  1; }

	for (y = ystart; y != yend; y += yinc)
	{
		for (x = xstart; x != xend; x += xinc)
		{
			// fetch the pixel
			if (!clear)
			{
				if (depth4)		pen = (gfx[(z/2)%gfx_size] >> ((z&1)?4:0)) & 0x0f;
				else			pen = gfx[z%gfx_size];

				if ( gfx2 )
				{
					pen &= 0x0f;
					if ( gfx2[(z/8)%gfx2_size] & (1 << (z & 7)) )
						pen |= 0x10;
				}
			}

			// plot it
			if (x >= clip->min_x && x <= clip->max_x && y >= clip->min_y && y <= clip->max_y)
			{
				if      (clear)				dest[x + y * 512] = clear_pen;
				else if (pen != trans_pen)	dest[x + y * 512] = pen | pen_hi;
				else if (opaque)			dest[x + y * 512] = 0xff;
			}

			z++;
		}
	}

	#ifdef MAME_DEBUG
#if 1
	if (input_code_pressed(space->machine, KEYCODE_Z))
	{	char buf[20];
		sprintf(buf, "%02X%02X",blitter.depth,blitter.flags&0xff);
//      ui_draw_text(buf, blitter.x, blitter.y);    // crashes mame!
	}
#endif
	#endif
}

/***************************************************************************

    Common functions

***************************************************************************/

// Inputs

static UINT16 igs_dips_sel, igs_input_sel;

static WRITE16_HANDLER( igs_dips_w )
{
	COMBINE_DATA(&igs_dips_sel);
}

#define IGS_DIPS_R( NUM )																\
static READ16_HANDLER( igs_##NUM##_dips_r )												\
{																						\
	int i;																				\
	UINT16 ret=0;																		\
	static const char *const dipnames[] = { "DSW1", "DSW2", "DSW3", "DSW4", "DSW5" };	\
																						\
	for (i = 0; i < NUM; i++)															\
		if ((~igs_dips_sel) & (1 << i) )												\
			ret = input_port_read(space->machine, dipnames[i]);								\
																						\
	/* 0x0100 is blitter busy */														\
	return 	(ret & 0xff) | 0x0000;														\
}

// Games have 3 to 5 dips
IGS_DIPS_R( 3 )
IGS_DIPS_R( 4 )
#if 0
IGS_DIPS_R( 5 )
#endif

// Palette r5g5b5
// offset+0x000: xRRRRRGG
// offset+0x800: GGGBBBBB

static WRITE16_HANDLER( igs_palette_w )
{
	int rgb;

	COMBINE_DATA(&paletteram16[offset]);

	rgb = (paletteram16[offset & 0x7ff] & 0xff) | ((paletteram16[offset | 0x800] & 0xff) << 8);
	palette_set_color_rgb(space->machine,offset & 0x7ff,pal5bit(rgb >> 0),pal5bit(rgb >> 5),pal5bit(rgb >> 10));
}



/***************************************************************************

    Code Decryption

***************************************************************************/

static void grtwall_decrypt(running_machine *machine)
{
	int i;
	UINT16 *src = (UINT16 *) (memory_region(machine, "maincpu"));

	int rom_size = 0x80000;

	for (i=0; i<rom_size/2; i++)
	{
		UINT16 x = src[i];

    	if ((i & 0x2000) == 0x0000 || (i & 0x0004) == 0x0000 || (i & 0x0090) == 0x0000)
			x ^= 0x0004;
    	if ((i & 0x0100) == 0x0100 || (i & 0x0040) == 0x0040 || (i & 0x0012) == 0x0012)
			x ^= 0x0020;
    	if ((i & 0x2400) == 0x0000 || (i & 0x4100) == 0x4100 || ((i & 0x2000) == 0x2000 && (i & 0x0c00) != 0x0000))
			x ^= 0x0200;
    	if ((x & 0x0024) == 0x0004 || (x & 0x0024) == 0x0020)
			x ^= 0x0024;
		src[i] = x;
	}
}

#if 0
static void lhb_decrypt(running_machine *machine)
{
	int i;
	UINT16 *src = (UINT16 *) (memory_region(machine, "maincpu"));

	int rom_size = 0x80000;

	for (i=0; i<rom_size/2; i++)
	{
		UINT16 x = src[i];

		if ((i & 0x1100) != 0x0100)
			x ^= 0x0200;

		if ((i & 0x0150) != 0x0000 && (i & 0x0152) != 0x0010)
			x ^= 0x0004;

		if ((i & 0x2084) != 0x2084 && (i & 0x2094) != 0x2014)
			x ^= 0x0020;

		src[i] = x;
	}
}
#endif
#if 0
static void drgnwrld_type3_decrypt(running_machine *machine)
{
	int i;
	UINT16 *src = (UINT16 *) (memory_region(machine, "maincpu"));

	int rom_size = 0x80000;

	for (i=0; i<rom_size/2; i++)
	{
		UINT16 x = src[i];

		if ((i & 0x2000) == 0x0000 || (i & 0x0004) == 0x0000 || (i & 0x0090) == 0x0000)
			x ^= 0x0004;

		if ((i & 0x0100) == 0x0100 || (i & 0x0040) == 0x0040 || (i & 0x0012) == 0x0012)
			x ^= 0x0020;

		if ((((i & 0x1000) == 0x1000) ^ ((i & 0x0100) == 0x0100))
			|| (i & 0x0880) == 0x0800 || (i & 0x0240) == 0x0240)
				x ^= 0x0200;

		if ((x & 0x0024) == 0x0004 || (x & 0x0024) == 0x0020)
			x ^= 0x0024;

		src[i] = x;
	}
}
#endif
#if 0
static void drgnwrld_type2_decrypt(running_machine *machine)
{
	int i;
	UINT16 *src = (UINT16 *) (memory_region(machine, "maincpu"));

	int rom_size = 0x80000;

	for (i=0; i<rom_size/2; i++)
	{
		UINT16 x = src[i];

		if(((i & 0x000090) == 0x000000) || ((i & 0x002004) != 0x002004))
		  x ^= 0x0004;

		if((((i & 0x000050) == 0x000000) || ((i & 0x000142) != 0x000000)) && ((i & 0x000150) != 0x000000))
		  x ^= 0x0020;

		if(((i & 0x004280) == 0x004000) || ((i & 0x004080) == 0x000000))
		  x ^= 0x0200;

		if((i & 0x0011a0) != 0x001000)
		  x ^= 0x0200;

		if((i & 0x000180) == 0x000100)
		  x ^= 0x0200;

		if((x & 0x0024) == 0x0020 || (x & 0x0024) == 0x0004)
		  x ^= 0x0024;

		src[i] = x;
	}
}
#endif
#if 0
static void drgnwrld_type1_decrypt(running_machine *machine)
{
	int i;
	UINT16 *src = (UINT16 *) (memory_region(machine, "maincpu"));

	int rom_size = 0x80000;

	for (i=0; i<rom_size/2; i++)
	{
		UINT16 x = src[i];

		if ((i & 0x2000) == 0x0000 || (i & 0x0004) == 0x0000 || (i & 0x0090) == 0x0000)
			x ^= 0x0004;

		if ((i & 0x0100) == 0x0100 || (i & 0x0040) == 0x0040 || (i & 0x0012) == 0x0012)
			x ^= 0x0020;
/*
        if ((((i & 0x1000) == 0x1000) ^ ((i & 0x0100) == 0x0100))
            || (i & 0x0880) == 0x0800 || (i & 0x0240) == 0x0240)
                x ^= 0x0200;
*/
		if ((x & 0x0024) == 0x0004 || (x & 0x0024) == 0x0020)
			x ^= 0x0024;

		src[i] = x;
	}
}
#endif

static void chmplst2_decrypt(running_machine *machine)
{
	int i,j;
	int rom_size = 0x80000;
	UINT16 *src = (UINT16 *) (memory_region(machine, "maincpu"));
	UINT16 *result_data = alloc_array_or_die(UINT16, rom_size/2);

 	for (i=0; i<rom_size/2; i++)
	{
		UINT16 x = src[i];

		if ((i & 0x0054) != 0x0000 && (i & 0x0056) != 0x0010)
			x ^= 0x0004;

		if ((i & 0x0204) == 0x0000)
 			x ^= 0x0008;

		if ((i & 0x3080) != 0x3080 && (i & 0x3090) != 0x3010)
			x ^= 0x0020;

		j = BITSWAP24(i, 23,22,21,20,19,18,17,16,15,14,13, 8, 11,10, 9, 2, 7,6,5,4,3, 12, 1,0);

		result_data[j] = x;
	}

	memcpy(src,result_data,rom_size);

	free(result_data);
}

#if 0
static void vbowlj_decrypt(running_machine *machine)
{
	int i;
	UINT16 *src = (UINT16 *) (memory_region(machine, "maincpu"));

	int rom_size = 0x80000;

	for(i=0; i<rom_size/2; i++)
	{
		UINT16 x = src[i];

		if((i & 0x4100) == 0x0100)
			x ^= 0x0200;

		if((i & 0x4000) == 0x4000 && (i & 0x0300) != 0x0100)
			x ^= 0x0200;

		if((i & 0x5700) == 0x5100)
			x ^= 0x0200;

		if((i & 0x5500) == 0x1000)
			x ^= 0x0200;

		if((i & 0x0140) != 0x0000 || (i & 0x0012) == 0x0012)
			x ^= 0x0004;

		if((i & 0x2004) != 0x2004 || (i & 0x0090) == 0x0000)
			x ^= 0x0020;

	    src[i] = x;
	  }
}
#endif
#if 0
static void dbc_decrypt(running_machine *machine)
{
	int i;
	UINT16 *src = (UINT16 *) (memory_region(machine, "maincpu"));

	int rom_size = 0x80000;

	for (i=0; i<rom_size/2; i++)
	{
		UINT16 x = src[i];

		if( i & 0x1000/2 )
		{
			if( ~i & 0x400/2 )
				x ^= 0x0200;
		}

		if( i & 0x4000/2 )
		{
			if( i & 0x100/2 )
			{
				if( ~i & 0x08/2 )
					x ^= 0x0020;
			}
			else
			{
				if( ~i & 0x28/2 )
					x ^= 0x0020;
			}
		}
		else
		{
			x ^= 0x0020;
		}

		if( i & 0x200/2 )
		{
			x ^= 0x0004;
		}
		else
		{
			if( (i & 0x80/2) == 0x80/2 || (i & 0x24/2) == 0x24/2 )
				x ^= 0x0004;
		}

		src[i] = x;
	}
}
#endif

/***************************************************************************

    Gfx Decryption

***************************************************************************/


static void chmplst2_decrypt_gfx(running_machine *machine)
{
	int i;
	unsigned rom_size = 0x200000;
	UINT8 *src = (UINT8 *) (memory_region(machine, "gfx1"));
	UINT8 *result_data = alloc_array_or_die(UINT8, rom_size);

	for (i=0; i<rom_size; i++)
    	result_data[i] = src[BITSWAP24(i, 23,22,21,20, 19, 17,16,15, 13,12, 10,9,8,7,6,5,4, 2,1, 3, 11, 14, 18, 0)];

	memcpy(src,result_data,rom_size);

	free(result_data);
}

#if 0
static void drgnwrld_gfx_decrypt(running_machine *machine)
{
	int i;
	unsigned rom_size = 0x400000;
	UINT8 *src = (UINT8 *) (memory_region(machine, "gfx1"));
	UINT8 *result_data = alloc_array_or_die(UINT8, rom_size);

 	for (i=0; i<rom_size; i++)
    	result_data[i] = src[BITSWAP24(i, 23,22,21,20,19,18,17,16,15, 12, 13, 14, 11,10,9,8,7,6,5,4,3,2,1,0)];

	memcpy(src,result_data,rom_size);

	free(result_data);
}
#endif

/***************************************************************************

    Protection & I/O

***************************************************************************/

static UINT16 igs_magic[2];

static WRITE16_HANDLER( chmplst2_magic_w )
{
	COMBINE_DATA(&igs_magic[offset]);

	if (offset == 0)
		return;

	switch(igs_magic[0])
	{
		case 0x00:
			COMBINE_DATA(&igs_input_sel);

			if (ACCESSING_BITS_0_7)
			{
				coin_counter_w(0,	data & 0x20);
				//  coin out        data & 0x40
			}

			if ( igs_input_sel & ~0x7f )
				logerror("%06x: warning, unknown bits written in igs_input_sel = %02x\n", cpu_get_pc(space->cpu), igs_input_sel);

//          popmessage("sel2 %02x",igs_input_sel&~0x1f);
			break;

		case 0x02:
			if (ACCESSING_BITS_0_7)
			{
				chmplst2_pen_hi = data & 0x07;

				okim6295_set_bank_base(devtag_get_device(space->machine, "oki"), (data & 0x08) ? 0x40000 : 0);
			}

			if ( chmplst2_pen_hi & ~0xf )
				logerror("%06x: warning, unknown bits written in chmplst2_pen_hi = %02x\n", cpu_get_pc(space->cpu), chmplst2_pen_hi);

//          popmessage("oki %02x",chmplst2_pen_hi & 0x08);
			break;

		default:
			logerror("%06x: warning, writing to igs_magic %02x = %02x\n", cpu_get_pc(space->cpu), igs_magic[0], data);
	}
}

static READ16_HANDLER( chmplst2_magic_r )
{
	switch(igs_magic[0])
	{
		case 0x01:
			if (~igs_input_sel & 0x01)	return input_port_read(space->machine, "KEY0");
			if (~igs_input_sel & 0x02)	return input_port_read(space->machine, "KEY1");
			if (~igs_input_sel & 0x04)	return input_port_read(space->machine, "KEY2");
			if (~igs_input_sel & 0x08)	return input_port_read(space->machine, "KEY3");
			if (~igs_input_sel & 0x10)	return input_port_read(space->machine, "KEY4");
			/* fall through */
		default:
			logerror("%06x: warning, reading with igs_magic = %02x\n", cpu_get_pc(space->cpu), igs_magic[0]);
			break;

		case 0x03:	return 0xff;	// ?

		// Protection:
		// 0544FE: 20 21 22 24 25 26 27 28 2A 2B 2C 2D 2E 30 31 32 33 34
		// 0544EC: 49 47 53 41 41 7F 41 41 3E 41 49 F9 0A 26 49 49 49 32

		case 0x20:	return 0x49;
		case 0x21:	return 0x47;
		case 0x22:	return 0x53;

		case 0x24:	return 0x41;
		case 0x25:	return 0x41;
		case 0x26:	return 0x7f;
		case 0x27:	return 0x41;
		case 0x28:	return 0x41;

		case 0x2a:	return 0x3e;
		case 0x2b:	return 0x41;
		case 0x2c:	return 0x49;
		case 0x2d:	return 0xf9;
		case 0x2e:	return 0x0a;

		case 0x30:	return 0x26;
		case 0x31:	return 0x49;
		case 0x32:	return 0x49;
		case 0x33:	return 0x49;
		case 0x34:	return 0x32;
	}

	return 0;
}

#if 0
static WRITE16_HANDLER( drgnwrld_magic_w )
{
	COMBINE_DATA(&igs_magic[offset]);

	if (offset == 0)
		return;

	switch(igs_magic[0])
	{

		case 0x00:
			if (ACCESSING_BITS_0_7)
				coin_counter_w(0,data & 2);

			if (data & ~0x2)
				logerror("%06x: warning, unknown bits written in coin counter = %02x\n", cpu_get_pc(space->cpu), data);

			break;

		default:
//          popmessage("magic %x <- %04x",igs_magic[0],data);
			logerror("%06x: warning, writing to igs_magic %02x = %02x\n", cpu_get_pc(space->cpu), igs_magic[0], data);
	}
}
#endif
#if 0
static READ16_HANDLER( drgnwrld_magic_r )
{
	switch(igs_magic[0])
	{
		case 0x00:	return input_port_read(space->machine, "IN0");
		case 0x01:	return input_port_read(space->machine, "IN1");
		case 0x02:	return input_port_read(space->machine, "IN2");

		case 0x20:	return 0x49;
		case 0x21:	return 0x47;
		case 0x22:	return 0x53;

		case 0x24:	return 0x41;
		case 0x25:	return 0x41;
		case 0x26:	return 0x7f;
		case 0x27:	return 0x41;
		case 0x28:	return 0x41;

		case 0x2a:	return 0x3e;
		case 0x2b:	return 0x41;
		case 0x2c:	return 0x49;
		case 0x2d:	return 0xf9;
		case 0x2e:	return 0x0a;

		case 0x30:	return 0x26;
		case 0x31:	return 0x49;
		case 0x32:	return 0x49;
		case 0x33:	return 0x49;
		case 0x34:	return 0x32;

		default:
			logerror("%06x: warning, reading with igs_magic = %02x\n", cpu_get_pc(space->cpu), igs_magic[0]);
	}

	return 0;
}
#endif

static WRITE16_HANDLER( grtwall_magic_w )
{
	COMBINE_DATA(&igs_magic[offset]);

	if (offset == 0)
		return;

	switch(igs_magic[0])
	{
		case 0x02:
			if (ACCESSING_BITS_0_7)
			{
				coin_counter_w(0,data & 0x01);

				okim6295_set_bank_base(devtag_get_device(space->machine, "oki"), (data & 0x10) ? 0x40000 : 0);
			}

			if (data & ~0x11)
				logerror("%06x: warning, unknown bits written in coin counter = %02x\n", cpu_get_pc(space->cpu), data);

//          popmessage("coin %02x",data);
			break;

		default:
			logerror("%06x: warning, writing to igs_magic %02x = %02x\n", cpu_get_pc(space->cpu), igs_magic[0], data);
	}
}

static READ16_HANDLER( grtwall_magic_r )
{
	switch(igs_magic[0])
	{
		case 0x00:	return input_port_read(space->machine, "IN0");

		case 0x20:	return 0x49;
		case 0x21:	return 0x47;
		case 0x22:	return 0x53;

		case 0x24:	return 0x41;
		case 0x25:	return 0x41;
		case 0x26:	return 0x7f;
		case 0x27:	return 0x41;
		case 0x28:	return 0x41;

		case 0x2a:	return 0x3e;
		case 0x2b:	return 0x41;
		case 0x2c:	return 0x49;
		case 0x2d:	return 0xf9;
		case 0x2e:	return 0x0a;

		case 0x30:	return 0x26;
		case 0x31:	return 0x49;
		case 0x32:	return 0x49;
		case 0x33:	return 0x49;
		case 0x34:	return 0x32;

		default:
			logerror("%06x: warning, reading with igs_magic = %02x\n", cpu_get_pc(space->cpu), igs_magic[0]);
	}

	return 0;
}

#if 0
static WRITE16_DEVICE_HANDLER( lhb_okibank_w )
{
	if (ACCESSING_BITS_8_15)
	{
		okim6295_set_bank_base(device, (data & 0x200) ? 0x40000 : 0);
	}

	if ( data & (~0x200) )
		logerror("%s: warning, unknown bits written in oki bank = %02x\n", cpuexec_describe_context(device->machine), data);

//  popmessage("oki %04x",data);
}
#endif
#if 0
static WRITE16_HANDLER( lhb_inputs_w )
{
	COMBINE_DATA(&igs_input_sel);

	if (ACCESSING_BITS_0_7)
	{
		coin_counter_w(0,			 data & 0x20	);
		//  coin out                 data & 0x40
		//  pay out?                 data & 0x80
	}

	if ( igs_input_sel & (~0x7f) )
		logerror("%06x: warning, unknown bits written in igs_input_sel = %02x\n", cpu_get_pc(space->cpu), igs_input_sel);

//  popmessage("sel2 %02x",igs_input_sel&~0x1f);
}
#endif
#if 0
static READ16_HANDLER( lhb_inputs_r )
{
	switch(offset)
	{
		case 0:		return igs_input_sel;

		case 1:
			if (~igs_input_sel & 0x01)	return input_port_read(space->machine, "KEY0");
			if (~igs_input_sel & 0x02)	return input_port_read(space->machine, "KEY1");
			if (~igs_input_sel & 0x04)	return input_port_read(space->machine, "KEY2");
			if (~igs_input_sel & 0x08)	return input_port_read(space->machine, "KEY3");
			if (~igs_input_sel & 0x10)	return input_port_read(space->machine, "KEY4");

			logerror("%06x: warning, reading with igs_input_sel = %02x\n", cpu_get_pc(space->cpu), igs_input_sel);
			break;
	}
	return 0;
}
#endif
#if 0
static WRITE16_HANDLER( vbowl_magic_w )
{
	COMBINE_DATA(&igs_magic[offset]);

	if (offset == 0)
		return;

	switch(igs_magic[0])
	{
		case 0x02:
			if (ACCESSING_BITS_0_7)
			{
				coin_counter_w(0,data & 1);
				coin_counter_w(1,data & 2);
			}

			if (data & ~0x3)
				logerror("%06x: warning, unknown bits written in coin counter = %02x\n", cpu_get_pc(space->cpu), data);

			break;

		default:
//          popmessage("magic %x <- %04x",igs_magic[0],data);
			logerror("%06x: warning, writing to igs_magic %02x = %02x\n", cpu_get_pc(space->cpu), igs_magic[0], data);
	}
}
#endif
#if 0
static READ16_HANDLER( vbowl_magic_r )
{
	switch(igs_magic[0])
	{
		case 0x00:	return input_port_read(space->machine, "IN0");
		case 0x01:	return input_port_read(space->machine, "IN1");

		case 0x20:	return 0x49;
		case 0x21:	return 0x47;
		case 0x22:	return 0x53;

		case 0x24:	return 0x41;
		case 0x25:	return 0x41;
		case 0x26:	return 0x7f;
		case 0x27:	return 0x41;
		case 0x28:	return 0x41;

		case 0x2a:	return 0x3e;
		case 0x2b:	return 0x41;
		case 0x2c:	return 0x49;
		case 0x2d:	return 0xf9;
		case 0x2e:	return 0x0a;

		case 0x30:	return 0x26;
		case 0x31:	return 0x49;
		case 0x32:	return 0x49;
		case 0x33:	return 0x49;
		case 0x34:	return 0x32;

		default:
			logerror("%06x: warning, reading with igs_magic = %02x\n", cpu_get_pc(space->cpu), igs_magic[0]);
	}

	return 0;
}
#endif
#if 0
static WRITE16_HANDLER( xymg_magic_w )
{
	COMBINE_DATA(&igs_magic[offset]);

	if (offset == 0)
		return;

	switch(igs_magic[0])
	{
		case 0x01:
			COMBINE_DATA(&igs_input_sel);

			if (ACCESSING_BITS_0_7)
			{
				coin_counter_w(0,	data & 0x20);
				//  coin out        data & 0x40
			}

			if ( igs_input_sel & ~0x3f )
				logerror("%06x: warning, unknown bits written in igs_input_sel = %02x\n", cpu_get_pc(space->cpu), igs_input_sel);

//          popmessage("sel2 %02x",igs_input_sel&~0x1f);
			break;

		default:
			logerror("%06x: warning, writing to igs_magic %02x = %02x\n", cpu_get_pc(space->cpu), igs_magic[0], data);
	}
}
#endif
#if 0
static READ16_HANDLER( xymg_magic_r )
{
	switch(igs_magic[0])
	{
		case 0x00:	return input_port_read(space->machine, "COIN");

		case 0x02:
			if (~igs_input_sel & 0x01)	return input_port_read(space->machine, "KEY0");
			if (~igs_input_sel & 0x02)	return input_port_read(space->machine, "KEY1");
			if (~igs_input_sel & 0x04)	return input_port_read(space->machine, "KEY2");
			if (~igs_input_sel & 0x08)	return input_port_read(space->machine, "KEY3");
			if (~igs_input_sel & 0x10)	return input_port_read(space->machine, "KEY4");
			/* fall through */

		case 0x20:	return 0x49;
		case 0x21:	return 0x47;
		case 0x22:	return 0x53;

		case 0x24:	return 0x41;
		case 0x25:	return 0x41;
		case 0x26:	return 0x7f;
		case 0x27:	return 0x41;
		case 0x28:	return 0x41;

		case 0x2a:	return 0x3e;
		case 0x2b:	return 0x41;
		case 0x2c:	return 0x49;
		case 0x2d:	return 0xf9;
		case 0x2e:	return 0x0a;

		case 0x30:	return 0x26;
		case 0x31:	return 0x49;
		case 0x32:	return 0x49;
		case 0x33:	return 0x49;
		case 0x34:	return 0x32;

		default:
			logerror("%06x: warning, reading with igs_magic = %02x\n", cpu_get_pc(space->cpu), igs_magic[0]);
			break;
	}

	return 0;
}
#endif

/***************************************************************************

    Memory Maps

***************************************************************************/
#if 0
static ADDRESS_MAP_START( drgnwrld, ADDRESS_SPACE_PROGRAM, 16 )
	AM_RANGE( 0x000000, 0x07ffff ) AM_ROM
	AM_RANGE( 0x100000, 0x103fff ) AM_RAM AM_BASE( &generic_nvram16 ) AM_SIZE( &generic_nvram_size )
	AM_RANGE( 0x200000, 0x200fff ) AM_RAM AM_BASE( &igs_priority_ram )
	AM_RANGE( 0x400000, 0x401fff ) AM_RAM_WRITE( igs_palette_w ) AM_BASE( &paletteram16 )
	AM_RANGE( 0x500000, 0x500001 ) AM_READ_PORT( "COIN" )
	AM_RANGE( 0x600000, 0x600001 ) AM_DEVREADWRITE8( "oki", okim6295_r, okim6295_w, 0x00ff )
	AM_RANGE( 0x700000, 0x700003 ) AM_DEVWRITE8( "ym", ym3812_w, 0x00ff )
	AM_RANGE( 0x800000, 0x800003 ) AM_WRITE( drgnwrld_magic_w )
	AM_RANGE( 0x800002, 0x800003 ) AM_READ ( drgnwrld_magic_r )
	AM_RANGE( 0xa20000, 0xa20001 ) AM_WRITE( igs_priority_w )
	AM_RANGE( 0xa40000, 0xa40001 ) AM_WRITE( igs_dips_w )
	AM_RANGE( 0xa58000, 0xa58001 ) AM_WRITE( igs_blit_x_w )
	AM_RANGE( 0xa58800, 0xa58801 ) AM_WRITE( igs_blit_y_w )
	AM_RANGE( 0xa59000, 0xa59001 ) AM_WRITE( igs_blit_w_w )
	AM_RANGE( 0xa59800, 0xa59801 ) AM_WRITE( igs_blit_h_w )
	AM_RANGE( 0xa5a000, 0xa5a001 ) AM_WRITE( igs_blit_gfx_lo_w )
	AM_RANGE( 0xa5a800, 0xa5a801 ) AM_WRITE( igs_blit_gfx_hi_w )
	AM_RANGE( 0xa5b000, 0xa5b001 ) AM_WRITE( igs_blit_flags_w )
	AM_RANGE( 0xa5b800, 0xa5b801 ) AM_WRITE( igs_blit_pen_w )
	AM_RANGE( 0xa5c000, 0xa5c001 ) AM_WRITE( igs_blit_depth_w )
	AM_RANGE( 0xa88000, 0xa88001 ) AM_READ( igs_3_dips_r )
ADDRESS_MAP_END
#endif

static ADDRESS_MAP_START( chmplst2, ADDRESS_SPACE_PROGRAM, 16 )
	AM_RANGE( 0x000000, 0x07ffff ) AM_ROM
	AM_RANGE( 0x100000, 0x103fff ) AM_RAM AM_BASE( &generic_nvram16 ) AM_SIZE( &generic_nvram_size )
	AM_RANGE( 0x200000, 0x200001 ) AM_DEVREADWRITE8( "oki", okim6295_r, okim6295_w, 0x00ff )
	AM_RANGE( 0x204000, 0x204003 ) AM_DEVWRITE8( "ym", ym2413_w, 0x00ff )
	AM_RANGE( 0x208000, 0x208003 ) AM_WRITE( chmplst2_magic_w )
	AM_RANGE( 0x208002, 0x208003 ) AM_READ ( chmplst2_magic_r )
	AM_RANGE( 0x20c000, 0x20cfff ) AM_RAM AM_BASE(&igs_priority_ram)
	AM_RANGE( 0x210000, 0x211fff ) AM_RAM_WRITE( igs_palette_w ) AM_BASE( &paletteram16 )
	AM_RANGE( 0x214000, 0x214001 ) AM_READ_PORT( "COIN" )
	AM_RANGE( 0x300000, 0x3fffff ) AM_READWRITE( igs_layers_r, igs_layers_w )
	AM_RANGE( 0xa20000, 0xa20001 ) AM_WRITE( igs_priority_w )
	AM_RANGE( 0xa40000, 0xa40001 ) AM_WRITE( igs_dips_w )
	AM_RANGE( 0xa58000, 0xa58001 ) AM_WRITE( igs_blit_x_w )
	AM_RANGE( 0xa58800, 0xa58801 ) AM_WRITE( igs_blit_y_w )
	AM_RANGE( 0xa59000, 0xa59001 ) AM_WRITE( igs_blit_w_w )
	AM_RANGE( 0xa59800, 0xa59801 ) AM_WRITE( igs_blit_h_w )
	AM_RANGE( 0xa5a000, 0xa5a001 ) AM_WRITE( igs_blit_gfx_lo_w )
	AM_RANGE( 0xa5a800, 0xa5a801 ) AM_WRITE( igs_blit_gfx_hi_w )
	AM_RANGE( 0xa5b000, 0xa5b001 ) AM_WRITE( igs_blit_flags_w )
	AM_RANGE( 0xa5b800, 0xa5b801 ) AM_WRITE( igs_blit_pen_w )
	AM_RANGE( 0xa5c000, 0xa5c001 ) AM_WRITE( igs_blit_depth_w )
	AM_RANGE( 0xa88000, 0xa88001 ) AM_READ( igs_3_dips_r )
ADDRESS_MAP_END


static ADDRESS_MAP_START( grtwall, ADDRESS_SPACE_PROGRAM, 16 )
	AM_RANGE( 0x000000, 0x07ffff ) AM_ROM
	AM_RANGE( 0x100000, 0x103fff ) AM_RAM AM_BASE( &generic_nvram16 ) AM_SIZE( &generic_nvram_size )
	AM_RANGE( 0x200000, 0x200fff ) AM_RAM AM_BASE( &igs_priority_ram )
	AM_RANGE( 0x300000, 0x3fffff ) AM_READWRITE( igs_layers_r, igs_layers_w )
	AM_RANGE( 0x400000, 0x401fff ) AM_RAM_WRITE( igs_palette_w ) AM_BASE( &paletteram16 )
	AM_RANGE( 0x520000, 0x520001 ) AM_READ_PORT( "COIN" )
	AM_RANGE( 0x600000, 0x600001 ) AM_DEVREADWRITE8( "oki", okim6295_r, okim6295_w, 0x00ff )
	AM_RANGE( 0x800000, 0x800003 ) AM_WRITE( grtwall_magic_w )
	AM_RANGE( 0x800002, 0x800003 ) AM_READ ( grtwall_magic_r )
	AM_RANGE( 0xa20000, 0xa20001 ) AM_WRITE( igs_priority_w )
	AM_RANGE( 0xa40000, 0xa40001 ) AM_WRITE( igs_dips_w )
	AM_RANGE( 0xa58000, 0xa58001 ) AM_WRITE( igs_blit_x_w )
	AM_RANGE( 0xa58800, 0xa58801 ) AM_WRITE( igs_blit_y_w )
	AM_RANGE( 0xa59000, 0xa59001 ) AM_WRITE( igs_blit_w_w )
	AM_RANGE( 0xa59800, 0xa59801 ) AM_WRITE( igs_blit_h_w )
	AM_RANGE( 0xa5a000, 0xa5a001 ) AM_WRITE( igs_blit_gfx_lo_w )
	AM_RANGE( 0xa5a800, 0xa5a801 ) AM_WRITE( igs_blit_gfx_hi_w )
	AM_RANGE( 0xa5b000, 0xa5b001 ) AM_WRITE( igs_blit_flags_w )
	AM_RANGE( 0xa5b800, 0xa5b801 ) AM_WRITE( igs_blit_pen_w )
	AM_RANGE( 0xa5c000, 0xa5c001 ) AM_WRITE( igs_blit_depth_w )
	AM_RANGE( 0xa88000, 0xa88001 ) AM_READ( igs_4_dips_r )
ADDRESS_MAP_END

// Only values 0 and 7 are written (1 bit per irq source?)
#if 0
static UINT16 lhb_irq_enable;
#endif
#if 0
static WRITE16_HANDLER( lhb_irq_enable_w )
{
	COMBINE_DATA( &lhb_irq_enable );
}
#endif
#if 0
static ADDRESS_MAP_START( lhb, ADDRESS_SPACE_PROGRAM, 16 )
	AM_RANGE( 0x010000, 0x010001 ) AM_DEVWRITE( "oki", lhb_okibank_w )
	AM_RANGE( 0x000000, 0x07ffff ) AM_ROM
	AM_RANGE( 0x100000, 0x103fff ) AM_RAM AM_BASE( &generic_nvram16 ) AM_SIZE( &generic_nvram_size )
	AM_RANGE( 0x200000, 0x200fff ) AM_RAM AM_BASE( &igs_priority_ram )
	AM_RANGE( 0x300000, 0x3fffff ) AM_READWRITE( igs_layers_r, igs_layers_w )
	AM_RANGE( 0x400000, 0x401fff ) AM_RAM_WRITE( igs_palette_w ) AM_BASE( &paletteram16 )
	AM_RANGE( 0x600000, 0x600001 ) AM_DEVREADWRITE8( "oki", okim6295_r, okim6295_w, 0x00ff )
	AM_RANGE( 0x700000, 0x700001 ) AM_READ_PORT( "COIN" )
	AM_RANGE( 0x700002, 0x700005 ) AM_READ ( lhb_inputs_r )
	AM_RANGE( 0x700002, 0x700003 ) AM_WRITE( lhb_inputs_w )
	AM_RANGE( 0x820000, 0x820001 ) AM_WRITE( igs_priority_w )
	AM_RANGE( 0x838000, 0x838001 ) AM_WRITE( lhb_irq_enable_w )
	AM_RANGE( 0x840000, 0x840001 ) AM_WRITE( igs_dips_w )
	AM_RANGE( 0x858000, 0x858001 ) AM_WRITE( igs_blit_x_w )
	AM_RANGE( 0x858800, 0x858801 ) AM_WRITE( igs_blit_y_w )
	AM_RANGE( 0x859000, 0x859001 ) AM_WRITE( igs_blit_w_w )
	AM_RANGE( 0x859800, 0x859801 ) AM_WRITE( igs_blit_h_w )
	AM_RANGE( 0x85a000, 0x85a001 ) AM_WRITE( igs_blit_gfx_lo_w )
	AM_RANGE( 0x85a800, 0x85a801 ) AM_WRITE( igs_blit_gfx_hi_w )
	AM_RANGE( 0x85b000, 0x85b001 ) AM_WRITE( igs_blit_flags_w )
	AM_RANGE( 0x85b800, 0x85b801 ) AM_WRITE( igs_blit_pen_w )
	AM_RANGE( 0x85c000, 0x85c001 ) AM_WRITE( igs_blit_depth_w )
	AM_RANGE( 0x888000, 0x888001 ) AM_READ( igs_5_dips_r )
ADDRESS_MAP_END
#endif
#if 0
static READ16_DEVICE_HANDLER( ics2115_word_r )
{
	switch(offset)
	{
		case 0:	return ics2115_r(device,0);
		case 1:	return ics2115_r(device,1);
		case 2:	return (ics2115_r(device,3) << 8) | ics2115_r(device,2);
	}
	return 0xff;
}
#endif
#if 0
static WRITE16_DEVICE_HANDLER( ics2115_word_w )
{
	switch(offset)
	{
		case 1:
			if (ACCESSING_BITS_0_7)		ics2115_w(device,1,data);
			break;
		case 2:
			if (ACCESSING_BITS_0_7)		ics2115_w(device,2,data);
			if (ACCESSING_BITS_8_15)	ics2115_w(device,3,data>>8);
			break;
	}
}
#endif
#if 0
static READ16_HANDLER( vbowl_unk_r )
{
	return 0xffff;
}
#endif
#if 0
static UINT16 *vbowl_trackball;
#endif
#if 0
static VIDEO_EOF( vbowl )
{
	vbowl_trackball[0] = vbowl_trackball[1];
	vbowl_trackball[1] = (input_port_read(machine, "AN1") << 8) | input_port_read(machine, "AN0");
}
#endif
#if 0
static WRITE16_HANDLER( vbowl_pen_hi_w )
{
	if (ACCESSING_BITS_0_7)
	{
		chmplst2_pen_hi = data & 0x07;
	}

	if (data & ~0x7)
		logerror("%06x: warning, unknown bits written to pen_hi = %04x\n", cpu_get_pc(space->cpu), igs_priority);
}
#endif
#if 0
static WRITE16_HANDLER( vbowl_link_0_w )	{ }
static WRITE16_HANDLER( vbowl_link_1_w )	{ }
static WRITE16_HANDLER( vbowl_link_2_w )	{ }
static WRITE16_HANDLER( vbowl_link_3_w )	{ }
#endif
#if 0
static ADDRESS_MAP_START( vbowl, ADDRESS_SPACE_PROGRAM, 16 )
	AM_RANGE( 0x000000, 0x07ffff ) AM_ROM
	AM_RANGE( 0x100000, 0x103fff ) AM_RAM AM_BASE( &generic_nvram16 ) AM_SIZE( &generic_nvram_size )
	AM_RANGE( 0x200000, 0x200fff ) AM_RAM AM_BASE( &igs_priority_ram )
	AM_RANGE( 0x300000, 0x3fffff ) AM_READWRITE( igs_layers_r, igs_layers_w )
	AM_RANGE( 0x400000, 0x401fff ) AM_RAM_WRITE( igs_palette_w ) AM_BASE( &paletteram16 )
	AM_RANGE( 0x520000, 0x520001 ) AM_READ_PORT( "COIN" )
	AM_RANGE( 0x600000, 0x600007 ) AM_DEVREADWRITE( "ics", ics2115_word_r, ics2115_word_w )
	AM_RANGE( 0x700000, 0x700003 ) AM_RAM AM_BASE( &vbowl_trackball )
	AM_RANGE( 0x700004, 0x700005 ) AM_WRITE( vbowl_pen_hi_w )
	AM_RANGE( 0x800000, 0x800003 ) AM_WRITE( vbowl_magic_w )
	AM_RANGE( 0x800002, 0x800003 ) AM_READ( vbowl_magic_r )

	AM_RANGE( 0xa00000, 0xa00001 ) AM_WRITE( vbowl_link_0_w )
	AM_RANGE( 0xa08000, 0xa08001 ) AM_WRITE( vbowl_link_1_w )
	AM_RANGE( 0xa10000, 0xa10001 ) AM_WRITE( vbowl_link_2_w )
	AM_RANGE( 0xa18000, 0xa18001 ) AM_WRITE( vbowl_link_3_w )

	AM_RANGE( 0xa20000, 0xa20001 ) AM_WRITE( igs_priority_w )
	AM_RANGE( 0xa40000, 0xa40001 ) AM_WRITE( igs_dips_w )
	AM_RANGE( 0xa58000, 0xa58001 ) AM_WRITE( igs_blit_x_w )
	AM_RANGE( 0xa58800, 0xa58801 ) AM_WRITE( igs_blit_y_w )
	AM_RANGE( 0xa59000, 0xa59001 ) AM_WRITE( igs_blit_w_w )
	AM_RANGE( 0xa59800, 0xa59801 ) AM_WRITE( igs_blit_h_w )
	AM_RANGE( 0xa5a000, 0xa5a001 ) AM_WRITE( igs_blit_gfx_lo_w )
	AM_RANGE( 0xa5a800, 0xa5a801 ) AM_WRITE( igs_blit_gfx_hi_w )
	AM_RANGE( 0xa5b000, 0xa5b001 ) AM_WRITE( igs_blit_flags_w )
	AM_RANGE( 0xa5b800, 0xa5b801 ) AM_WRITE( igs_blit_pen_w )
	AM_RANGE( 0xa5c000, 0xa5c001 ) AM_WRITE( igs_blit_depth_w )

	AM_RANGE( 0xa80000, 0xa80001 ) AM_READ( vbowl_unk_r )
	AM_RANGE( 0xa88000, 0xa88001 ) AM_READ( igs_4_dips_r )
	AM_RANGE( 0xa90000, 0xa90001 ) AM_READ( vbowl_unk_r )
	AM_RANGE( 0xa98000, 0xa98001 ) AM_READ( vbowl_unk_r )
ADDRESS_MAP_END
#endif
#if 0
static ADDRESS_MAP_START( xymg, ADDRESS_SPACE_PROGRAM, 16 )
	AM_RANGE( 0x010000, 0x010001 ) AM_DEVWRITE( "oki", lhb_okibank_w )
	AM_RANGE( 0x000000, 0x07ffff ) AM_ROM
	AM_RANGE( 0x100000, 0x103fff ) AM_RAM
	AM_RANGE( 0x200000, 0x200fff ) AM_RAM AM_BASE( &igs_priority_ram )
	AM_RANGE( 0x300000, 0x3fffff ) AM_READWRITE( igs_layers_r, igs_layers_w )
	AM_RANGE( 0x400000, 0x401fff ) AM_RAM_WRITE( igs_palette_w ) AM_BASE( &paletteram16 )
	AM_RANGE( 0x600000, 0x600001 ) AM_DEVREADWRITE8( "oki", okim6295_r, okim6295_w, 0x00ff )
	AM_RANGE( 0x700000, 0x700003 ) AM_WRITE( xymg_magic_w )
	AM_RANGE( 0x700002, 0x700003 ) AM_READ ( xymg_magic_r )
	AM_RANGE( 0x820000, 0x820001 ) AM_WRITE( igs_priority_w )
	AM_RANGE( 0x840000, 0x840001 ) AM_WRITE( igs_dips_w )
	AM_RANGE( 0x858000, 0x858001 ) AM_WRITE( igs_blit_x_w )
	AM_RANGE( 0x858800, 0x858801 ) AM_WRITE( igs_blit_y_w )
	AM_RANGE( 0x859000, 0x859001 ) AM_WRITE( igs_blit_w_w )
	AM_RANGE( 0x859800, 0x859801 ) AM_WRITE( igs_blit_h_w )
	AM_RANGE( 0x85a000, 0x85a001 ) AM_WRITE( igs_blit_gfx_lo_w )
	AM_RANGE( 0x85a800, 0x85a801 ) AM_WRITE( igs_blit_gfx_hi_w )
	AM_RANGE( 0x85b000, 0x85b001 ) AM_WRITE( igs_blit_flags_w )
	AM_RANGE( 0x85b800, 0x85b801 ) AM_WRITE( igs_blit_pen_w )
	AM_RANGE( 0x85c000, 0x85c001 ) AM_WRITE( igs_blit_depth_w )
	AM_RANGE( 0x888000, 0x888001 ) AM_READ( igs_3_dips_r )
	AM_RANGE( 0x1f0000, 0x1f3fff ) AM_RAM AM_BASE( &generic_nvram16 ) AM_SIZE( &generic_nvram_size ) // extra ram
ADDRESS_MAP_END
#endif

/***************************************************************************

    Input Ports

***************************************************************************/
#if 0
static INPUT_PORTS_START( drgnwrldc )
	PORT_START("DSW1")
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x18, DEF_STR( Normal  ) )	// 513
	PORT_DIPSETTING(    0x10, DEF_STR( Hard    ) )	// 627
	PORT_DIPSETTING(    0x08, DEF_STR( Harder  ) )	// 741
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )	// 855
	PORT_DIPUNKNOWN( 0x20, 0x20 )
	PORT_DIPUNKNOWN( 0x40, 0x40 )
	PORT_DIPUNKNOWN( 0x80, 0x80 )

	PORT_START("DSW2")
	PORT_DIPNAME( 0x01, 0x01, "Open Girl?" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Sex Question?" )	// "background" in test mode
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Background" )	// "sex question" in test mode
	PORT_DIPSETTING(    0x04, "Girl" )
	PORT_DIPSETTING(    0x00, "Landscape" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Tiles" )
	PORT_DIPSETTING(    0x10, "Mahjong" )
	PORT_DIPSETTING(    0x00, "Symbols" )
	PORT_DIPUNKNOWN( 0x20, 0x20 )
	PORT_DIPNAME( 0x40, 0x40, "Bang Turtle?" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Test?" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

  	PORT_START("DSW3")
	PORT_DIPUNKNOWN( 0x01, 0x01 )
	PORT_DIPUNKNOWN( 0x02, 0x02 )
	PORT_DIPUNKNOWN( 0x04, 0x04 )
	PORT_DIPUNKNOWN( 0x08, 0x08 )
	PORT_DIPUNKNOWN( 0x10, 0x10 )
	PORT_DIPUNKNOWN( 0x20, 0x20 )
	PORT_DIPUNKNOWN( 0x40, 0x40 )
	PORT_DIPUNKNOWN( 0x80, 0x80 )

	PORT_START("COIN")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_SERVICE_NO_TOGGLE( 0x04, IP_ACTIVE_LOW )	// keep pressed while booting
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE2 )	// used?
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE3 )	// used?
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("IN0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )	// press in girl test to pause, button 3 advances
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("IN1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_PLAYER(2)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_PLAYER(2)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(2)

	PORT_START("IN2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_PLAYER(2)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_PLAYER(2)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
INPUT_PORTS_END
#endif
#if 0
static INPUT_PORTS_START( drgnwrld )
	PORT_START("DSW1")
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x18, DEF_STR( Normal  ) )	// 513
	PORT_DIPSETTING(    0x10, DEF_STR( Hard    ) )	// 627
	PORT_DIPSETTING(    0x08, DEF_STR( Harder  ) )	// 741
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )	// 855
	PORT_DIPUNKNOWN( 0x20, 0x20 )
	PORT_DIPUNKNOWN( 0x40, 0x40 )
	PORT_DIPUNKNOWN( 0x80, 0x80 )

	PORT_START("DSW2")
	PORT_DIPNAME( 0x01, 0x01, "Background" )
	PORT_DIPSETTING(    0x01, "Girl" )
	PORT_DIPSETTING(    0x00, "Landscape" )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Tiles" )
	PORT_DIPSETTING(    0x04, "Mahjong" )
	PORT_DIPSETTING(    0x00, "Symbols" )
	PORT_DIPNAME( 0x08, 0x08, "Send Boom?" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPUNKNOWN( 0x10, 0x10 )
	PORT_DIPUNKNOWN( 0x20, 0x20 )
	PORT_DIPUNKNOWN( 0x40, 0x40 )
	PORT_DIPNAME( 0x80, 0x80, "Test?" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

  	PORT_START("DSW3")
	PORT_DIPUNKNOWN( 0x01, 0x01 )
	PORT_DIPUNKNOWN( 0x02, 0x02 )
	PORT_DIPUNKNOWN( 0x04, 0x04 )
	PORT_DIPUNKNOWN( 0x08, 0x08 )
	PORT_DIPUNKNOWN( 0x10, 0x10 )
	PORT_DIPUNKNOWN( 0x20, 0x20 )
	PORT_DIPUNKNOWN( 0x40, 0x40 )
	PORT_DIPUNKNOWN( 0x80, 0x80 )

	PORT_START("COIN")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_SERVICE_NO_TOGGLE( 0x04, IP_ACTIVE_LOW )	// keep pressed while booting
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE2 )	// used?
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE3 )	// used?
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("IN0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )	// press in girl test to pause, button 3 advances
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("IN1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_PLAYER(2)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_PLAYER(2)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(2)

	PORT_START("IN2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_PLAYER(2)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_PLAYER(2)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
INPUT_PORTS_END
#endif

static INPUT_PORTS_START( chmplst2 )
	PORT_START("DSW1")
	PORT_DIPNAME( 0x07, 0x02, "Pay Out (%)" )
	PORT_DIPSETTING(    0x07, "50" )
	PORT_DIPSETTING(    0x06, "54" )
	PORT_DIPSETTING(    0x05, "58" )
	PORT_DIPSETTING(    0x04, "62" )
	PORT_DIPSETTING(    0x03, "66" )
	PORT_DIPSETTING(    0x02, "70" )
	PORT_DIPSETTING(    0x01, "74" )
	PORT_DIPSETTING(    0x00, "78" )
	PORT_DIPNAME( 0x08, 0x08, "Odds Rate" )
	PORT_DIPSETTING(    0x08, "1,2,3,5,8,15,30,50" )
	PORT_DIPSETTING(    0x00, "1,2,3,4,5,6,7,8" )
	PORT_DIPNAME( 0x10, 0x10, "Max Bet" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPSETTING(    0x10, "10" )
	PORT_DIPNAME( 0x60, 0x60, "Min Credits To Start" )
	PORT_DIPSETTING(    0x60, "1" )
	PORT_DIPSETTING(    0x40, "2" )
	PORT_DIPSETTING(    0x20, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPUNKNOWN( 0x80, 0x80 )

	PORT_START("DSW2")
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coinage ) )	// Only when bit 4 = 1
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x04, 0x04, "Credits Per Note" )	// Only when bit 4 = 0
	PORT_DIPSETTING(    0x04, "10" )
	PORT_DIPSETTING(    0x00, "100" )
	PORT_DIPNAME( 0x08, 0x08, "Max Note Credits" )
	PORT_DIPSETTING(    0x08, "100" )
	PORT_DIPSETTING(    0x00, "500" )
	PORT_DIPNAME( 0x10, 0x10, "Money Type" )	// Decides whether to use bits 0&1 or bit 2
	PORT_DIPSETTING(    0x10, "Coins" )
	PORT_DIPSETTING(    0x00, "Notes" )
	PORT_DIPUNKNOWN( 0x20, 0x20 )
	PORT_DIPUNKNOWN( 0x40, 0x40 )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

  	PORT_START("DSW3")
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x03, "500" )
	PORT_DIPSETTING(    0x02, "1000" )
	PORT_DIPSETTING(    0x01, "2000" )
	PORT_DIPSETTING(    0x00, "30000" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x0c, "0" )
	PORT_DIPSETTING(    0x08, "1" )
	PORT_DIPSETTING(    0x04, "2" )
//  PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPNAME( 0x70, 0x70, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x70, "1 : 1" )
	PORT_DIPSETTING(    0x60, "1 : 2" )
	PORT_DIPSETTING(    0x50, "1 : 5" )
	PORT_DIPSETTING(    0x40, "1 : 6" )
	PORT_DIPSETTING(    0x30, "1 : 7" )
	PORT_DIPSETTING(    0x20, "1 : 8" )
	PORT_DIPSETTING(    0x10, "1 : 9" )
	PORT_DIPSETTING(    0x00, "1 : 10" )
	PORT_DIPUNKNOWN( 0x80, 0x80 )

	PORT_START("COIN")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE1 )	// data clear
	PORT_SERVICE_NO_TOGGLE( 0x04, IP_ACTIVE_LOW )	// keep pressed while booting
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE4 )	// haba?
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE2 )	// stats
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE3 )	// clear coin
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("KEY0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_MAHJONG_A )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_MAHJONG_E )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_MAHJONG_I )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_MAHJONG_M )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_MAHJONG_KAN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )	// ? set to 0 both
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )	// ? and you can't start a game

	PORT_START("KEY1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_MAHJONG_B )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_MAHJONG_F )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_MAHJONG_J )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_MAHJONG_N )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_MAHJONG_REACH )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_MAHJONG_BET )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("KEY2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_MAHJONG_C )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_MAHJONG_G )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_MAHJONG_K )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_MAHJONG_CHI )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_MAHJONG_RON )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("KEY3")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_MAHJONG_D )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_MAHJONG_H )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_MAHJONG_L )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_MAHJONG_PON )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("KEY4")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

#if 0
static INPUT_PORTS_START( drgnwrldj )
	PORT_START("DSW1")
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x18, DEF_STR( Normal  ) )	// 513
	PORT_DIPSETTING(    0x10, DEF_STR( Hard    ) )	// 627
	PORT_DIPSETTING(    0x08, DEF_STR( Harder  ) )	// 741
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )	// 855
	PORT_DIPUNKNOWN( 0x20, 0x20 )
	PORT_DIPUNKNOWN( 0x40, 0x40 )
	PORT_DIPUNKNOWN( 0x80, 0x80 )

	PORT_START("DSW2")
	PORT_DIPNAME( 0x01, 0x01, "Open Girl?" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Background" )
	PORT_DIPSETTING(    0x02, "Girl" )
	PORT_DIPSETTING(    0x00, "Landscape" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Bang Turtle?" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Send Boom?" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPUNKNOWN( 0x20, 0x20 )
	PORT_DIPUNKNOWN( 0x40, 0x40 )
	PORT_DIPNAME( 0x80, 0x80, "Test?" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

  	PORT_START("DSW3")
	PORT_DIPUNKNOWN( 0x01, 0x01 )
	PORT_DIPUNKNOWN( 0x02, 0x02 )
	PORT_DIPUNKNOWN( 0x04, 0x04 )
	PORT_DIPUNKNOWN( 0x08, 0x08 )
	PORT_DIPUNKNOWN( 0x10, 0x10 )
	PORT_DIPUNKNOWN( 0x20, 0x20 )
	PORT_DIPUNKNOWN( 0x40, 0x40 )
	PORT_DIPUNKNOWN( 0x80, 0x80 )

	PORT_START("COIN")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_SERVICE_NO_TOGGLE( 0x04, IP_ACTIVE_LOW )	// keep pressed while booting
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE2 )	// used?
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE3 )	// used?
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("IN0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )	// press in girl test to pause, button 3 advances
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("IN1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_PLAYER(2)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_PLAYER(2)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(2)

	PORT_START("IN2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_PLAYER(2)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_PLAYER(2)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
INPUT_PORTS_END
#endif

static INPUT_PORTS_START( grtwall )
	PORT_START("DSW1")
	PORT_DIPUNKNOWN( 0x01, 0x01 )
	PORT_DIPUNKNOWN( 0x02, 0x02 )
	PORT_DIPUNKNOWN( 0x04, 0x04 )
	PORT_DIPUNKNOWN( 0x08, 0x08 )
	PORT_DIPUNKNOWN( 0x10, 0x10 )
	PORT_DIPUNKNOWN( 0x20, 0x20 )
	PORT_DIPUNKNOWN( 0x40, 0x40 )
	PORT_DIPUNKNOWN( 0x80, 0x80 )

	PORT_START("DSW2")
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x03, "1000" )
	PORT_DIPSETTING(    0x02, "1500" )
	PORT_DIPSETTING(    0x01, "2000" )
	PORT_DIPSETTING(    0x00, "3000" )
	PORT_DIPNAME( 0x0c, 0x0c, "Min Credits To Start" )
	PORT_DIPSETTING(    0x0c, "1" )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPUNKNOWN( 0x10, 0x10 )		// shown in test mode
	PORT_DIPUNKNOWN( 0x20, 0x20 )
	PORT_DIPNAME( 0x40, 0x40, "Hide Title" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPUNKNOWN( 0x80, 0x80 )

  	PORT_START("DSW3")
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x0c, 0x0c, "Credits Per Note" )
	PORT_DIPSETTING(    0x0c, "10" )
	PORT_DIPSETTING(    0x08, "20" )
	PORT_DIPSETTING(    0x04, "50" )
	PORT_DIPSETTING(    0x00, "100" )
	PORT_DIPNAME( 0x10, 0x10, "Max Note Credits" )
	PORT_DIPSETTING(    0x10, "500" )
	PORT_DIPSETTING(    0x00, "9999" )
	PORT_DIPNAME( 0x20, 0x20, "Money Type" )
	PORT_DIPSETTING(    0x20, "Coins" )	// use bits 0-1
	PORT_DIPSETTING(    0x00, "Notes" )	// use bits 2-3
	PORT_DIPUNKNOWN( 0x40, 0x40 )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START("DSW4")
	PORT_DIPUNKNOWN( 0x01, 0x01 )
	PORT_DIPUNKNOWN( 0x02, 0x02 )
	PORT_DIPUNKNOWN( 0x04, 0x04 )
	PORT_DIPUNKNOWN( 0x08, 0x08 )
	PORT_DIPUNKNOWN( 0x10, 0x10 )
	PORT_DIPUNKNOWN( 0x20, 0x20 )
	PORT_DIPUNKNOWN( 0x40, 0x40 )
	PORT_DIPUNKNOWN( 0x80, 0x80 )

	PORT_START("COIN")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1    )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2    )
	PORT_SERVICE_NO_TOGGLE( 0x04, IP_ACTIVE_LOW )	// keep pressed while booting
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_OTHER    ) PORT_NAME("1") PORT_CODE(KEYCODE_1_PAD)	// shown in test mode
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_OTHER    ) PORT_NAME("2") PORT_CODE(KEYCODE_2_PAD)	// shown in test mode
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN  )

	PORT_START("IN0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 )
INPUT_PORTS_END

#if 0
static INPUT_PORTS_START( lhb )
	PORT_START("DSW1")
	PORT_DIPNAME( 0x0f, 0x07, "Pay Out (%)" )
	PORT_DIPSETTING(    0x0f, "96" )
	PORT_DIPSETTING(    0x0e, "93" )
	PORT_DIPSETTING(    0x0d, "90" )
	PORT_DIPSETTING(    0x0c, "87" )
	PORT_DIPSETTING(    0x0b, "84" )
	PORT_DIPSETTING(    0x0a, "81" )
	PORT_DIPSETTING(    0x09, "78" )
	PORT_DIPSETTING(    0x08, "75" )
	PORT_DIPSETTING(    0x07, "71" )
	PORT_DIPSETTING(    0x06, "68" )
	PORT_DIPSETTING(    0x05, "65" )
	PORT_DIPSETTING(    0x04, "62" )
	PORT_DIPSETTING(    0x03, "59" )
	PORT_DIPSETTING(    0x02, "56" )
	PORT_DIPSETTING(    0x01, "53" )
	PORT_DIPSETTING(    0x00, "50" )
	PORT_DIPNAME( 0x30, 0x30, "YAKUMAN Point" )
	PORT_DIPSETTING(    0x30, "1" )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x10, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0xc0, 0xc0, "Max Bet" )
	PORT_DIPSETTING(    0xc0, "1" )
	PORT_DIPSETTING(    0x80, "5" )
	PORT_DIPSETTING(    0x40, "10" )
	PORT_DIPSETTING(    0x00, "20" )

	PORT_START("DSW2")
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x0c, 0x0c, "Min Credits To Start" )
	PORT_DIPSETTING(    0x0c, "1" )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x70, 0x70, "DAI MANGUAN Cycle" )
	PORT_DIPSETTING(    0x70, "300" )
//  PORT_DIPSETTING(    0x60, "300" )
//  PORT_DIPSETTING(    0x50, "300" )
//  PORT_DIPSETTING(    0x40, "300" )
//  PORT_DIPSETTING(    0x30, "300" )
//  PORT_DIPSETTING(    0x20, "300" )
//  PORT_DIPSETTING(    0x10, "300" )
//  PORT_DIPSETTING(    0x00, "300" )
	PORT_DIPNAME( 0x80, 0x80, "DAI MANGUAN Times" )
	PORT_DIPSETTING(    0x80, "2" )
//  PORT_DIPSETTING(    0x00, "2" )

  	PORT_START("DSW3")
	PORT_DIPNAME( 0x03, 0x03, "Max Credit" )
	PORT_DIPSETTING(    0x03, "1000" )
	PORT_DIPSETTING(    0x02, "2000" )
	PORT_DIPSETTING(    0x01, "5000" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPNAME( 0x0c, 0x0c, "Max Note" )
	PORT_DIPSETTING(    0x0c, "1000" )
	PORT_DIPSETTING(    0x08, "2000" )
	PORT_DIPSETTING(    0x04, "5000" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPNAME( 0x10, 0x10, "CPU Strength" )
	PORT_DIPSETTING(    0x10, "Strong" )
	PORT_DIPSETTING(    0x00, "Weak" )
	PORT_DIPNAME( 0x20, 0x20, "Money Type" )
	PORT_DIPSETTING(    0x20, "Coins" )
	PORT_DIPSETTING(    0x00, "Notes" )
	PORT_DIPNAME( 0xc0, 0xc0, "DONDEN Times" )
	PORT_DIPSETTING(    0xc0, "0" )
	PORT_DIPSETTING(    0x80, "3" )
	PORT_DIPSETTING(    0x40, "5" )
	PORT_DIPSETTING(    0x00, "8" )

	PORT_START("DSW4")
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, "In Game Music" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x00, "Girls" )
	PORT_DIPSETTING(    0x0c, DEF_STR( No ) )
	PORT_DIPSETTING(    0x08, "Dressed" )
	PORT_DIPSETTING(    0x04, "Underwear" )
	PORT_DIPSETTING(    0x00, "Nude" )
	PORT_DIPNAME( 0x10, 0x10, "Note Rate" )
	PORT_DIPSETTING(    0x10, "5" )
	PORT_DIPSETTING(    0x00, "10" )
	PORT_DIPNAME( 0x20, 0x20, "Pay Out" )
	PORT_DIPSETTING(    0x20, "Score" )
	PORT_DIPSETTING(    0x00, "Coin" )
	PORT_DIPNAME( 0x40, 0x40, "Coin In" )
	PORT_DIPSETTING(    0x40, "Credit" )
	PORT_DIPSETTING(    0x00, "Score" )
	PORT_DIPNAME( 0x80, 0x80, "Last Chance" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Yes ) )

	PORT_START("DSW5")
	PORT_DIPNAME( 0x01, 0x01, "In-Game Bet" )
	PORT_DIPSETTING(    0x01, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPUNKNOWN( 0x02, 0x02 )
	PORT_DIPUNKNOWN( 0x04, 0x04 )
	PORT_DIPUNKNOWN( 0x08, 0x08 )
	PORT_DIPUNKNOWN( 0x10, 0x10 )
	PORT_DIPUNKNOWN( 0x20, 0x20 )
	PORT_DIPUNKNOWN( 0x40, 0x40 )
	PORT_DIPUNKNOWN( 0x80, 0x80 )

	PORT_START("COIN")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE2 )	// system reset
	PORT_SERVICE_NO_TOGGLE( 0x04, IP_ACTIVE_LOW )	// keep pressed while booting
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )	// stats
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1    ) PORT_IMPULSE(5)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE3 )	// clear coins
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_OTHER    ) PORT_NAME("0") PORT_CODE(KEYCODE_0_PAD)	// shown in test mode
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN  )

	PORT_START("KEY0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_MAHJONG_A )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_MAHJONG_E )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_MAHJONG_I )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_MAHJONG_M )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_MAHJONG_KAN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("KEY1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_MAHJONG_B )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_MAHJONG_F )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_MAHJONG_J )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_MAHJONG_N )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_MAHJONG_REACH )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_MAHJONG_BET )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("KEY2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_MAHJONG_C )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_MAHJONG_G )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_MAHJONG_K )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_MAHJONG_CHI )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_MAHJONG_RON )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("KEY3")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_MAHJONG_D )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_MAHJONG_H )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_MAHJONG_L )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_MAHJONG_PON )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("KEY4")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_MAHJONG_LAST_CHANCE )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_MAHJONG_SCORE )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_MAHJONG_DOUBLE_UP )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )	// shown in test mode
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_MAHJONG_BIG )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_MAHJONG_SMALL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END
#endif
#if 0
static INPUT_PORTS_START( vbowl )
	PORT_START("DSW1")
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_5C ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Special Picture" ) /* Sexy Interlude pics */
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x20, "Open Picture" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Controls ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Joystick ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Trackball ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START("DSW2")
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x03, DEF_STR( Easy   ) )	// 5
	PORT_DIPSETTING(    0x02, DEF_STR( Normal ) )	// 7
	PORT_DIPSETTING(    0x01, DEF_STR( Medium ) )	// 9
	PORT_DIPSETTING(    0x00, DEF_STR( Hard   ) )	// 11
	PORT_DIPNAME( 0x04, 0x04, "Spares To Win (Frames 1-5)" )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x18, 0x18, "Points To Win (Frames 6-10)" )
	PORT_DIPSETTING(    0x18, "160" )
	PORT_DIPSETTING(    0x10, "170" )
	PORT_DIPSETTING(    0x08, "180" )
	PORT_DIPSETTING(    0x00, "190" )
	PORT_DIPUNKNOWN( 0x20, 0x20 )
	PORT_DIPUNKNOWN( 0x40, 0x40 )
	PORT_DIPUNKNOWN( 0x80, 0x80 )

  	PORT_START("DSW3")
	PORT_DIPNAME( 0x03, 0x03, "Cabinet ID" )
	PORT_DIPSETTING(    0x03, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x04, 0x04, "Linked Cabinets" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPUNKNOWN( 0x08, 0x08 )
	PORT_DIPUNKNOWN( 0x10, 0x10 )
	PORT_DIPUNKNOWN( 0x20, 0x20 )
	PORT_DIPUNKNOWN( 0x40, 0x40 )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START("DSW4")
	PORT_DIPUNKNOWN( 0x01, 0x01 )
	PORT_DIPUNKNOWN( 0x02, 0x02 )
	PORT_DIPUNKNOWN( 0x04, 0x04 )
	PORT_DIPUNKNOWN( 0x08, 0x08 )
	PORT_DIPUNKNOWN( 0x10, 0x10 )
	PORT_DIPUNKNOWN( 0x20, 0x20 )
	PORT_DIPUNKNOWN( 0x40, 0x40 )
	PORT_DIPUNKNOWN( 0x80, 0x80 )

	PORT_START("COIN")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("IN0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 )

	PORT_START("IN1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_PLAYER(2)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_PLAYER(2)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_PLAYER(2)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_PLAYER(2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(2)

	PORT_START("AN0")
    PORT_BIT( 0xff, 0x00, IPT_TRACKBALL_X ) PORT_SENSITIVITY(30) PORT_KEYDELTA(30) PORT_PLAYER(1)

	PORT_START("AN1")
    PORT_BIT( 0xff, 0x00, IPT_TRACKBALL_Y ) PORT_SENSITIVITY(30) PORT_KEYDELTA(30) PORT_PLAYER(1)
INPUT_PORTS_END
#endif
#if 0
static INPUT_PORTS_START( vbowlj )
	PORT_START("DSW1")
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_5C ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Special Picture" ) /* Sexy Interlude pics */
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Controls ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Joystick ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Trackball ) )
	PORT_DIPUNKNOWN( 0x40, 0x40 )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START("DSW2")
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x03, DEF_STR( Easy   ) )	// 5
	PORT_DIPSETTING(    0x02, DEF_STR( Normal ) )	// 7
	PORT_DIPSETTING(    0x01, DEF_STR( Medium ) )	// 9
	PORT_DIPSETTING(    0x00, DEF_STR( Hard   ) )	// 11
	PORT_DIPNAME( 0x04, 0x04, "Spares To Win (Frames 1-5)" )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x18, 0x18, "Points To Win (Frames 6-10)" )
	PORT_DIPSETTING(    0x18, "160" )
	PORT_DIPSETTING(    0x10, "170" )
	PORT_DIPSETTING(    0x08, "180" )
	PORT_DIPSETTING(    0x00, "190" )
	PORT_DIPUNKNOWN( 0x20, 0x20 )
	PORT_DIPUNKNOWN( 0x40, 0x40 )
	PORT_DIPUNKNOWN( 0x80, 0x80 )

  	PORT_START("DSW3")
	PORT_DIPNAME( 0x03, 0x03, "Cabinet ID" )
	PORT_DIPSETTING(    0x03, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x04, 0x04, "Linked Cabinets" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPUNKNOWN( 0x08, 0x08 )
	PORT_DIPUNKNOWN( 0x10, 0x10 )
	PORT_DIPUNKNOWN( 0x20, 0x20 )
	PORT_DIPUNKNOWN( 0x40, 0x40 )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START("DSW4")
	PORT_DIPUNKNOWN( 0x01, 0x01 )
	PORT_DIPUNKNOWN( 0x02, 0x02 )
	PORT_DIPUNKNOWN( 0x04, 0x04 )
	PORT_DIPUNKNOWN( 0x08, 0x08 )
	PORT_DIPUNKNOWN( 0x10, 0x10 )
	PORT_DIPUNKNOWN( 0x20, 0x20 )
	PORT_DIPUNKNOWN( 0x40, 0x40 )
	PORT_DIPUNKNOWN( 0x80, 0x80 )

	PORT_START("COIN")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("IN0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 )

	PORT_START("IN1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_PLAYER(2)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_PLAYER(2)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_PLAYER(2)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_PLAYER(2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(2)

	PORT_START("AN0")
    PORT_BIT( 0xff, 0x00, IPT_TRACKBALL_X ) PORT_SENSITIVITY(30) PORT_KEYDELTA(30) PORT_PLAYER(1)

	PORT_START("AN1")
    PORT_BIT( 0xff, 0x00, IPT_TRACKBALL_Y ) PORT_SENSITIVITY(30) PORT_KEYDELTA(30) PORT_PLAYER(1)
INPUT_PORTS_END
#endif
#if 0
static INPUT_PORTS_START( xymg )
	PORT_START("DSW1")
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x0c, 0x0c, "Credits Per Note" )
	PORT_DIPSETTING(    0x0c, "10" )
	PORT_DIPSETTING(    0x08, "20" )
	PORT_DIPSETTING(    0x04, "50" )
	PORT_DIPSETTING(    0x00, "100" )
	PORT_DIPNAME( 0x10, 0x10, "Max Note Credits" )
	PORT_DIPSETTING(    0x10, "500" )
	PORT_DIPSETTING(    0x00, "9999" )
	PORT_DIPNAME( 0x20, 0x20, "Money Type" )
	PORT_DIPSETTING(    0x20, "Coins" )	// use bits 0-1
	PORT_DIPSETTING(    0x00, "Notes" )	// use bits 2-3
	PORT_DIPUNKNOWN( 0x40, 0x40 )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START("DSW2")
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x03, "1000" )
	PORT_DIPSETTING(    0x02, "1500" )
	PORT_DIPSETTING(    0x01, "2000" )
	PORT_DIPSETTING(    0x00, "3000" )
	PORT_DIPNAME( 0x0c, 0x0c, "Min Credits To Start" )
	PORT_DIPSETTING(    0x0c, "1" )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )	// shown in test mode
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPUNKNOWN( 0x20, 0x20 )
	PORT_DIPUNKNOWN( 0x40, 0x40 )
	PORT_DIPUNKNOWN( 0x80, 0x80 )

  	PORT_START("DSW3")
	PORT_DIPUNKNOWN( 0x01, 0x01 )
	PORT_DIPUNKNOWN( 0x02, 0x02 )
	PORT_DIPUNKNOWN( 0x04, 0x04 )
	PORT_DIPUNKNOWN( 0x08, 0x08 )
	PORT_DIPUNKNOWN( 0x10, 0x10 )
	PORT_DIPUNKNOWN( 0x20, 0x20 )
	PORT_DIPUNKNOWN( 0x40, 0x40 )
	PORT_DIPUNKNOWN( 0x80, 0x80 )

	PORT_START("COIN")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_SERVICE_NO_TOGGLE( 0x04, IP_ACTIVE_LOW )	// keep pressed while booting
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )	// stats
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE3 )	// clear coin
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("KEY0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_MAHJONG_A )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_MAHJONG_E )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_MAHJONG_I )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_MAHJONG_M )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_MAHJONG_KAN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("KEY1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_MAHJONG_B )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_MAHJONG_F )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_MAHJONG_J )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_MAHJONG_N )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_MAHJONG_REACH )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_MAHJONG_BET )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("KEY2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_MAHJONG_C )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_MAHJONG_G )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_MAHJONG_K )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_MAHJONG_CHI )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_MAHJONG_RON )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("KEY3")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_MAHJONG_D )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_MAHJONG_H )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_MAHJONG_L )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_MAHJONG_PON )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("KEY4")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_MAHJONG_LAST_CHANCE )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_MAHJONG_SCORE )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_MAHJONG_DOUBLE_UP )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_MAHJONG_BIG )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_MAHJONG_SMALL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END
#endif

/***************************************************************************

    Machine Drivers

***************************************************************************/

/*
// for debugging

static const gfx_layout gfxlayout_8x8x4 =
{
    8,8,
    RGN_FRAC(1,1),
    4,
    { STEP4(0,1) },
    { 4, 0, 12,  8, 20,16, 28,24 },
    { STEP8(0,8*4) },
    8*8*4
};
static const gfx_layout gfxlayout_16x16x4 =
{
    16,16,
    RGN_FRAC(1,1),
    4,
    { STEP4(0,1) },
    {  4, 0, 12, 8, 20,16, 28,24,
      36,32, 44,40, 52,48, 60,56    },
    { STEP16(0,16*4) },
    16*16*4
};
static const gfx_layout gfxlayout_8x8x8 =
{
    8,8,
    RGN_FRAC(1,1),
    8,
    { STEP8(0,1) },
    { STEP8(0,8) },
    { STEP8(0,8*8) },
    8*8*8
};
static const gfx_layout gfxlayout_16x16x8 =
{
    16,16,
    RGN_FRAC(1,1),
    8,
    { STEP8(0,1) },
    { STEP16(0,8) },
    { STEP16(0,16*8) },
    16*16*8
};
static const gfx_layout gfxlayout_16x16x1 =
{
    16,16,
    RGN_FRAC(1,1),
    1,
    { 0 },
    { STEP16(15,-1) },
    { STEP16(0,16*1) },
    16*16*1
};

static GFXDECODE_START( igs_blit )
    GFXDECODE_ENTRY( "gfx1", 0, gfxlayout_8x8x4,   0, 0x80  )
    GFXDECODE_ENTRY( "gfx1", 0, gfxlayout_16x16x4, 0, 0x80  )
    GFXDECODE_ENTRY( "gfx1", 0, gfxlayout_8x8x8,   0, 0x08  )
    GFXDECODE_ENTRY( "gfx1", 0, gfxlayout_16x16x8, 0, 0x08  )
GFXDECODE_END
static GFXDECODE_START( chmplst2 )
    GFXDECODE_ENTRY( "gfx1", 0, gfxlayout_8x8x4,   0, 0x80  )
    GFXDECODE_ENTRY( "gfx1", 0, gfxlayout_16x16x4, 0, 0x80  )
    GFXDECODE_ENTRY( "gfx1", 0, gfxlayout_8x8x8,   0, 0x08  )
    GFXDECODE_ENTRY( "gfx1", 0, gfxlayout_16x16x8, 0, 0x08  )
    GFXDECODE_ENTRY( "gfx2", 0, gfxlayout_16x16x1, 0, 0x80  )
GFXDECODE_END
*/

static MACHINE_DRIVER_START( igs_base )
	MDRV_CPU_ADD("maincpu",M68000, 22000000/3)

	MDRV_NVRAM_HANDLER(generic_0fill)

//  MDRV_GFXDECODE(igs_blit)


	MDRV_SCREEN_ADD("screen", RASTER)
	MDRV_SCREEN_REFRESH_RATE(60)
	MDRV_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(0))
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_SIZE(512, 256)
	MDRV_SCREEN_VISIBLE_AREA(0, 512-1, 0, 240-1)

	MDRV_PALETTE_LENGTH(0x800)

	MDRV_VIDEO_START( igs )
	MDRV_VIDEO_UPDATE( igs )

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")

	MDRV_SOUND_ADD("oki", OKIM6295, 1047600)
	MDRV_SOUND_CONFIG(okim6295_interface_pin7high)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 1.0)
MACHINE_DRIVER_END


static INTERRUPT_GEN( chmplst2_interrupt )
{
	switch (cpu_getiloops(device))
	{
		case 0:	cpu_set_input_line(device, 6, HOLD_LINE);	break;
		default:
		case 1:	cpu_set_input_line(device, 5, HOLD_LINE);	break;
	}
}

static MACHINE_DRIVER_START( chmplst2 )
	MDRV_IMPORT_FROM(igs_base)
	MDRV_CPU_MODIFY("maincpu")
	MDRV_CPU_PROGRAM_MAP(chmplst2)
	MDRV_CPU_VBLANK_INT_HACK(chmplst2_interrupt,1+4)	// lev5 frequency drives the music tempo

//  MDRV_GFXDECODE(chmplst2)

	MDRV_SOUND_ADD("ym", YM2413, 3579545)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 2.0)
MACHINE_DRIVER_END

#if 0
static MACHINE_DRIVER_START( drgnwrld )
	MDRV_IMPORT_FROM(igs_base)
	MDRV_CPU_MODIFY("maincpu")
	MDRV_CPU_PROGRAM_MAP(drgnwrld)
	MDRV_CPU_VBLANK_INT_HACK(chmplst2_interrupt,1+4)	// lev5 frequency drives the music tempo

	MDRV_SOUND_ADD("ym", YM3812, 3579545)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 2.0)
MACHINE_DRIVER_END
#endif

static INTERRUPT_GEN( grtwall_interrupt )
{
	switch (cpu_getiloops(device))
	{
		case 0:	cpu_set_input_line(device, 3, HOLD_LINE);	break;
		case 1:	cpu_set_input_line(device, 6, HOLD_LINE);	break;
	}
}

static MACHINE_DRIVER_START( grtwall )
	MDRV_IMPORT_FROM(igs_base)
	MDRV_CPU_MODIFY("maincpu")
	MDRV_CPU_PROGRAM_MAP(grtwall)
	MDRV_CPU_VBLANK_INT_HACK(grtwall_interrupt,2)
MACHINE_DRIVER_END

#if 0
static INTERRUPT_GEN( lhb_interrupt )
{
	if (!lhb_irq_enable)
		return;

	switch (cpu_getiloops(device))
	{
		case 0:	cpu_set_input_line(device, 3, HOLD_LINE);	break;
		case 2:	cpu_set_input_line(device, 6, HOLD_LINE);	break;
		default:
				// It reads the inputs. Must be called more than once for test mode on boot to work
				cpu_set_input_line(device, 5, HOLD_LINE);	break;
	}
}
#endif
#if 0
static MACHINE_DRIVER_START( lhb )
	MDRV_IMPORT_FROM(igs_base)
	MDRV_CPU_MODIFY("maincpu")
	MDRV_CPU_PROGRAM_MAP(lhb)
	MDRV_CPU_VBLANK_INT_HACK(lhb_interrupt,3+1)
MACHINE_DRIVER_END
#endif
#if 0
static void sound_irq(const device_config *device, int state)
{
//   cputag_set_input_line(machine, "maincpu", 3, state);
}
#endif
#if 0
static const ics2115_interface vbowl_ics2115_interface = {
	sound_irq
};
#endif
#if 0
static INTERRUPT_GEN( vbowl_interrupt )
{
	switch (cpu_getiloops(device))
	{
		case 0:	cpu_set_input_line(device, 4, HOLD_LINE);	break;
		case 1:	cpu_set_input_line(device, 5, HOLD_LINE);	break;
		case 2:	cpu_set_input_line(device, 6, HOLD_LINE);	break;
		default:
		case 3:	cpu_set_input_line(device, 3, HOLD_LINE);	break;	// sound
	}
}
#endif
#if 0
static MACHINE_DRIVER_START( vbowl )
	MDRV_IMPORT_FROM(igs_base)
	MDRV_CPU_MODIFY("maincpu")
	MDRV_CPU_PROGRAM_MAP(vbowl)
	MDRV_CPU_VBLANK_INT_HACK(vbowl_interrupt,3+4)

	MDRV_VIDEO_EOF(vbowl)	// trackball
//  MDRV_GFXDECODE(chmplst2)

	MDRV_DEVICE_REMOVE("oki")
	MDRV_SOUND_ADD("ics", ICS2115, 0)
	MDRV_SOUND_CONFIG(vbowl_ics2115_interface)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 5.0)
MACHINE_DRIVER_END
#endif
#if 0
static MACHINE_DRIVER_START( xymg )
	MDRV_IMPORT_FROM(igs_base)
	MDRV_CPU_MODIFY("maincpu")
	MDRV_CPU_PROGRAM_MAP(xymg)
	MDRV_CPU_VBLANK_INT_HACK(grtwall_interrupt,2)
MACHINE_DRIVER_END
#endif

static DRIVER_INIT( chmplst2 )
{
	UINT16 *rom = (UINT16 *) memory_region(machine, "maincpu");

	chmplst2_decrypt(machine);
	chmplst2_decrypt_gfx(machine);

	// PROTECTION CHECKS
	rom[0x034f4/2]	=	0x4e71;		// 0034F4: 660E    bne 3504   (rom test, fills palette with white otherwise)
	rom[0x03502/2]	=	0x6032;		// 003502: 6732    beq 3536   (rom test, fills palette with white otherwise)
	rom[0x1afea/2]	=	0x6034;		// 01AFEA: 6734    beq 1b020  (fills palette with black otherwise)
	rom[0x24b8a/2]	=	0x6036;		// 024B8A: 6736    beq 24bc2  (fills palette with green otherwise)
	rom[0x29ef8/2]	=	0x6036;		// 029EF8: 6736    beq 29f30  (fills palette with red otherwise)
	rom[0x2e69c/2]	=	0x6036;		// 02E69C: 6736    beq 2e6d4  (fills palette with green otherwise)
	rom[0x2fe96/2]	=	0x6036;		// 02FE96: 6736    beq 2fece  (fills palette with red otherwise)
	rom[0x325da/2]	=	0x6036;		// 0325DA: 6736    beq 32612  (fills palette with green otherwise)
	rom[0x3d80a/2]	=	0x6034;		// 03D80A: 6734    beq 3d840  (fills palette with black otherwise)
	rom[0x3ed80/2]	=	0x6036;		// 03ED80: 6736    beq 3edb8  (fills palette with red otherwise)
	rom[0x41d72/2]	=	0x6034;		// 041D72: 6734    beq 41da8  (fills palette with black otherwise)
	rom[0x44834/2]	=	0x6034;		// 044834: 6734    beq 4486a  (fills palette with black otherwise)
}

#if 0
static DRIVER_INIT( drgnwrld )
{
	UINT16 *rom = (UINT16 *) memory_region(machine, "maincpu");

	drgnwrld_type1_decrypt(machine);
	drgnwrld_gfx_decrypt(machine);

	// PROTECTION CHECKS
	rom[0x032ee/2]	=	0x606c;		// 0032EE: 676C        beq 335c     (ASIC11 CHECK PORT ERROR 3)
	rom[0x23d5e/2]	=	0x606c;		// 023D5E: 676C        beq 23dcc    (CHECK PORT ERROR 1)
	rom[0x23fd0/2]	=	0x606c;		// 023FD0: 676C        beq 2403e    (CHECK PORT ERROR 2)
	rom[0x24170/2]	=	0x606c;		// 024170: 676C        beq 241de    (CHECK PORT ERROR 3)
	rom[0x24348/2]	=	0x606c;		// 024348: 676C        beq 243b6    (ASIC11 CHECK PORT ERROR 4)
	rom[0x2454e/2]	=	0x606c;		// 02454E: 676C        beq 245bc    (ASIC11 CHECK PORT ERROR 3)
	rom[0x246cc/2]	=	0x606c;		// 0246CC: 676C        beq 2473a    (ASIC11 CHECK PORT ERROR 2)
	rom[0x24922/2]	=	0x606c;		// 024922: 676C        beq 24990    (ASIC11 CHECK PORT ERROR 1)
	rom[0x24b66/2]	=	0x606c;		// 024B66: 676C        beq 24bd4    (ASIC12 CHECK PORT ERROR 4)
	rom[0x24de2/2]	=	0x606c;		// 024DE2: 676C        beq 24e50    (ASIC12 CHECK PORT ERROR 3)
	rom[0x2502a/2]	=	0x606c;		// 02502A: 676C        beq 25098    (ASIC12 CHECK PORT ERROR 2)
	rom[0x25556/2]	=	0x6000;		// 025556: 6700 E584   beq 23adc    (ASIC12 CHECK PORT ERROR 1)
	rom[0x2a16c/2]	=	0x606c;		// 02A16C: 676C        beq 2a1da    (ASIC11 CHECK PORT ERROR 2)
}
#endif
#if 0
static DRIVER_INIT( drgnwrldv30 )
{
	UINT16 *rom = (UINT16 *) memory_region(machine, "maincpu");

	drgnwrld_type1_decrypt(machine);
	drgnwrld_gfx_decrypt(machine);

	// PROTECTION CHECKS
	rom[0x032ee/2]	=	0x606c;		// 0032EE: 676C        beq 335c     (ASIC11 CHECK PORT ERROR 3)
	rom[0x23d5e/2]	=	0x606c;		// 023D5E: 676C        beq 23dcc    (CHECK PORT ERROR 1)
	rom[0x23fd0/2]	=	0x606c;		// 023FD0: 676C        beq 2403e    (CHECK PORT ERROR 2)
	rom[0x24170/2]	=	0x606c;		// 024170: 676C        beq 241de    (CHECK PORT ERROR 3)
	rom[0x24348/2]	=	0x606c;		// 024348: 676C        beq 243b6    (ASIC11 CHECK PORT ERROR 4)
	rom[0x2454e/2]	=	0x606c;		// 02454E: 676C        beq 245bc    (ASIC11 CHECK PORT ERROR 3)
	rom[0x246cc/2]	=	0x606c;		// 0246CC: 676C        beq 2473a    (ASIC11 CHECK PORT ERROR 2)
	rom[0x24922/2]	=	0x606c;		// 024922: 676C        beq 24990    (ASIC11 CHECK PORT ERROR 1)
	rom[0x24b66/2]	=	0x606c;		// 024B66: 676C        beq 24bd4    (ASIC12 CHECK PORT ERROR 4)
	rom[0x24de2/2]	=	0x606c;		// 024DE2: 676C        beq 24e50    (ASIC12 CHECK PORT ERROR 3)
	rom[0x2502a/2]	=	0x606c;		// 02502A: 676C        beq 25098    (ASIC12 CHECK PORT ERROR 2)
	rom[0x25556/2]	=	0x6000;		// 025556: 6700 E584   beq 23adc    (ASIC12 CHECK PORT ERROR 1)
	// different from drgnwrld:
	rom[0x2a162/2]	=	0x606c;		// 02A162: 676C        beq 2a1d0    (ASIC11 CHECK PORT ERROR 2)
}
#endif
#if 0
static DRIVER_INIT( drgnwrldv21 )
{
	UINT16 *rom = (UINT16 *) memory_region(machine, "maincpu");

	drgnwrld_type2_decrypt(machine);
	drgnwrld_gfx_decrypt(machine);

	// PROTECTION CHECKS
	rom[0x032ee/2]	=	0x606c;		// 0032EE: 676C        beq 335c     (ASIC11 CHECK PORT ERROR 3)
	rom[0x11ca8/2]	=	0x606c;		// ??
	rom[0x23d5e/2]	=	0x606c;		// 023D5E: 676C        beq 23dcc    (CHECK PORT ERROR 1)
	rom[0x23fd0/2]	=	0x606c;		// 023FD0: 676C        beq 2403e    (CHECK PORT ERROR 2)
	rom[0x24170/2]	=	0x606c;		// 024170: 676C        beq 241de    (CHECK PORT ERROR 3)
	rom[0x24348/2]	=	0x606c;		// 024348: 676C        beq 243b6    (ASIC11 CHECK PORT ERROR 4)
	rom[0x2454e/2]	=	0x606c;		// 02454E: 676C        beq 245bc    (ASIC11 CHECK PORT ERROR 3)
	rom[0x246cc/2]	=	0x606c;		// 0246CC: 676C        beq 2473a    (ASIC11 CHECK PORT ERROR 2)
	rom[0x24922/2]	=	0x606c;		// 024922: 676C        beq 24990    (ASIC11 CHECK PORT ERROR 1)
	rom[0x24b66/2]	=	0x606c;		// 024B66: 676C        beq 24bd4    (ASIC12 CHECK PORT ERROR 4)
	rom[0x24de2/2]	=	0x606c;		// 024DE2: 676C        beq 24e50    (ASIC12 CHECK PORT ERROR 3)
	rom[0x2502a/2]	=	0x606c;		// 02502A: 676C        beq 25098    (ASIC12 CHECK PORT ERROR 2)
	rom[0x25556/2]	=	0x6000;		// 025556: 6700 E584   beq 23adc    (ASIC12 CHECK PORT ERROR 1)
	rom[0x269de/2]	=	0x606c;		// ??
	rom[0x2766a/2]	=	0x606c;		// ??
	rom[0x2a830/2]	=	0x606c;		// ??
}
#endif
#if 0
static DRIVER_INIT( drgnwrldv10c )
{
	UINT16 *rom = (UINT16 *) memory_region(machine, "maincpu");

	drgnwrld_type1_decrypt(machine);
	drgnwrld_gfx_decrypt(machine);

	// PROTECTION CHECKS
	rom[0x033d2/2]	=	0x606c;		// 0033D2: 676C        beq 3440     (ASIC11 CHECK PORT ERROR 3)
	rom[0x23d0e/2]	=	0x606c;		// 023D0E: 676C        beq 23d7c    (CHECK PORT ERROR 1)
	rom[0x23f58/2]	=	0x606c;		// 023F58: 676C        beq 23fc6    (CHECK PORT ERROR 2)
	rom[0x240d0/2]	=	0x606c;		// 0240D0: 676C        beq 2413e    (CHECK PORT ERROR 3)
	rom[0x242a8/2]	=	0x606c;		// 0242A8: 676C        beq 24316    (ASIC11 CHECK PORT ERROR 4)
	rom[0x244ae/2]	=	0x606c;		// 0244AE: 676C        beq 2451c    (ASIC11 CHECK PORT ERROR 3)
	rom[0x2462c/2]	=	0x606c;		// 02462C: 676C        beq 2469a    (ASIC11 CHECK PORT ERROR 2)
	rom[0x24882/2]	=	0x606c;		// 024882: 676C        beq 248f0    (ASIC11 CHECK PORT ERROR 1)
	rom[0x24ac6/2]	=	0x606c;		// 024AC6: 676C        beq 24b34    (ASIC12 CHECK PORT ERROR 4)
	rom[0x24d42/2]	=	0x606c;		// 024D42: 676C        beq 24db0    (ASIC12 CHECK PORT ERROR 3)
	rom[0x24f8a/2]	=	0x606c;		// 024F8A: 676C        beq 24ff8    (ASIC12 CHECK PORT ERROR 2)
	rom[0x254b6/2]	=	0x6000;		// 0254B6: 6700 E5FC   beq 23ab4    (ASIC12 CHECK PORT ERROR 1)
	rom[0x2a23a/2]	=	0x606c;		// 02A23A: 676C        beq 2a2a8    (ASIC11 CHECK PORT ERROR 2)

}
#endif
#if 0
static DRIVER_INIT( drgnwrldv11h )
{
	drgnwrld_type1_decrypt(machine);
	drgnwrld_gfx_decrypt(machine);

	// PROTECTION CHECKS
	// the protection checks are already pathed out like we do!
}
#endif
#if 0
static DRIVER_INIT( drgnwrldv21j )
{
	UINT16 *rom = (UINT16 *) memory_region(machine, "maincpu");

	drgnwrld_type3_decrypt(machine);
	drgnwrld_gfx_decrypt(machine);

	// PROTECTION CHECKS
	rom[0x033d2/2]	=	0x606c;		// 0033D2: 676C        beq 3440     (ASIC11 CHECK PORT ERROR 3)
	rom[0x11c74/2]	=	0x606c;		// 011C74: 676C        beq 11ce2    (CHECK PORT ERROR 1)
	rom[0x23d2a/2]	=	0x606c;		// 023D2A: 676C        beq 23d98
	rom[0x23f68/2]	=	0x606c;		// 023F68: 676C        beq 23fd6
	rom[0x240d4/2]	=	0x606c;		// 0240D4: 676C        beq 24142    (CHECK PORT ERROR 3)
	rom[0x242ac/2]	=	0x606c;		// 0242AC: 676C        beq 2431a
	rom[0x244b2/2]	=	0x606c;		// 0244B2: 676C        beq 24520
	rom[0x24630/2]	=	0x606c;		// 024630: 676C        beq 2469e
	rom[0x24886/2]	=	0x606c;		// 024886: 676C        beq 248f4
	rom[0x24aca/2]	=	0x606c;		// 024ACA: 676C        beq 24b38
	rom[0x24d46/2]	=	0x606c;		// 024D46: 676C        beq 24db4
	rom[0x24f8e/2]	=	0x606c;		// 024F8E: 676C        beq 24ffc
	rom[0x254ba/2]	=	0x6000;		// 0254BA: 6700 E620   beq 23adc    (ASIC12 CHECK PORT ERROR 1)
	rom[0x26a52/2]	=	0x606c;		// 026A52: 676C        beq 26ac0    (ASIC12 CHECK PORT ERROR 1)
	rom[0x276aa/2]	=	0x606c;		// 0276AA: 676C        beq 27718    (CHECK PORT ERROR 3)
	rom[0x2a870/2]	=	0x606c;		// 02A870: 676C        beq 2a8de    (ASIC11 CHECK PORT ERROR 2)
}
#endif
#if 0
static DRIVER_INIT( drgnwrldv20j )
{
	UINT16 *rom = (UINT16 *) memory_region(machine, "maincpu");

	drgnwrld_type3_decrypt(machine);
	drgnwrld_gfx_decrypt(machine);

	// PROTECTION CHECKS
	rom[0x033d2/2]	=	0x606c;		// 0033D2: 676C        beq 3440     (ASIC11 CHECK PORT ERROR 3)
	rom[0x11c74/2]	=	0x606c;		// 011C74: 676C        beq 11ce2    (CHECK PORT ERROR 1)
	rom[0x23d2a/2]	=	0x606c;		// 023D2A: 676C        beq 23d98
	rom[0x23f68/2]	=	0x606c;		// 023F68: 676C        beq 23fd6
	rom[0x240d4/2]	=	0x606c;		// 0240D4: 676C        beq 24142    (CHECK PORT ERROR 3)
	rom[0x242ac/2]	=	0x606c;		// 0242AC: 676C        beq 2431a
	rom[0x244b2/2]	=	0x606c;		// 0244B2: 676C        beq 24520
	rom[0x24630/2]	=	0x606c;		// 024630: 676C        beq 2469e
	rom[0x24886/2]	=	0x606c;		// 024886: 676C        beq 248f4
	rom[0x24aca/2]	=	0x606c;		// 024ACA: 676C        beq 24b38
	rom[0x24d46/2]	=	0x606c;		// 024D46: 676C        beq 24db4
	rom[0x24f8e/2]	=	0x606c;		// 024F8E: 676C        beq 24ffc
	rom[0x254ba/2]	=	0x6000;		// 0254BA: 6700 E620   beq 23adc    (ASIC12 CHECK PORT ERROR 1)
	rom[0x26a52/2]	=	0x606c;		// 026A52: 676C        beq 26ac0    (ASIC12 CHECK PORT ERROR 1)
	// different from drgnwrldv21j:
	rom[0x276a0/2]	=	0x606c;		// 0276A0: 676C        beq 2770e    (CHECK PORT ERROR 3)
	rom[0x2a86e/2]	=	0x606c;		// 02A86E: 676C        beq 2a8dc    (ASIC11 CHECK PORT ERROR 2)
}
#endif

static DRIVER_INIT( grtwall )
{
	UINT16 *rom = (UINT16 *) memory_region(machine, "maincpu");

	grtwall_decrypt(machine);

	// PROTECTION CHECKS
	rom[0x16b96/2]	=	0x6000;		// 016B96: 6700 02FE    beq 16e96  (fills palette with red otherwise)
	rom[0x16e5e/2]	=	0x6036;		// 016E5E: 6736         beq 16e96  (fills palette with green otherwise)
	rom[0x17852/2]	=	0x6000;		// 017852: 6700 01F2    beq 17a46  (fills palette with green otherwise)
	rom[0x17a0e/2]	=	0x6036;		// 017A0E: 6736         beq 17a46  (fills palette with red otherwise)
	rom[0x23636/2]	=	0x6036;		// 023636: 6736         beq 2366e  (fills palette with red otherwise)
	rom[0x2b1e6/2]	=	0x6000;		// 02B1E6: 6700 0218    beq 2b400  (fills palette with green otherwise)
	rom[0x2f9f2/2]	=	0x6000;		// 02F9F2: 6700 04CA    beq 2febe  (fills palette with green otherwise)
	rom[0x2fb2e/2]	=	0x6000;		// 02FB2E: 6700 038E    beq 2febe  (fills palette with red otherwise)
	rom[0x2fcf2/2]	=	0x6000;		// 02FCF2: 6700 01CA    beq 2febe  (fills palette with red otherwise)
	rom[0x2fe86/2]	=	0x6036;		// 02FE86: 6736         beq 2febe  (fills palette with red otherwise)
	rom[0x3016e/2]	=	0x6000;		// 03016E: 6700 03F6    beq 30566  (fills palette with green otherwise)
	rom[0x303c8/2]	=	0x6000;		// 0303C8: 6700 019C    beq 30566  (fills palette with green otherwise)
	rom[0x3052e/2]	=	0x6036;		// 03052E: 6736         beq 30566  (fills palette with green otherwise)
}

#if 0
static DRIVER_INIT( lhb )
{
	UINT16 *rom = (UINT16 *) memory_region(machine, "maincpu");

	lhb_decrypt(machine);

	// PROTECTION CHECKS
	rom[0x2eef6/2]	=	0x4e75;		// 02EEF6: 4E56 FE00    link A6, #-$200  (fills palette with pink otherwise)
}
#endif
#if 0
static DRIVER_INIT( lhba )
{
	UINT16 *rom = (UINT16 *) memory_region(machine, "maincpu");

	lhb_decrypt(machine);

	// PROTECTION CHECKS
	rom[0x2e988/2]	=	0x4e75;		// 02E988: 4E56 FE00    link A6, #-$200  (fills palette with pink otherwise)
}
#endif
#if 0
static DRIVER_INIT( vbowl )
{
	UINT16 *rom = (UINT16 *) memory_region(machine, "maincpu");
	UINT8  *gfx = (UINT8 *)  memory_region(machine, "gfx1");
	int i;

	vbowlj_decrypt(machine);

	for (i = 0x400000-1; i >= 0; i--)
	{
		gfx[i * 2 + 1] = (gfx[i] & 0xf0) >> 4;
		gfx[i * 2 + 0] = (gfx[i] & 0x0f) >> 0;
	}

	// Patch the bad dump so that it doesn't reboot at the end of a game (the patched value is from vbowlj)
	rom[0x080e0/2] = 0xe549;	// 0080E0: 0449 dc.w $0449; ILLEGAL

	// PROTECTION CHECKS
	rom[0x3764/2] = 0x4e75;	// 003764: 4E56 0000 link    A6, #$0
}
#endif
#if 0
static DRIVER_INIT( vbowlj )
{
	UINT16 *rom = (UINT16 *) memory_region(machine, "maincpu");
	UINT8  *gfx = (UINT8 *)  memory_region(machine, "gfx1");
	int i;

	vbowlj_decrypt(machine);

	for (i = 0x400000-1; i >= 0; i--)
	{
		gfx[i * 2 + 1] = (gfx[i] & 0xf0) >> 4;
		gfx[i * 2 + 0] = (gfx[i] & 0x0f) >> 0;
	}

	// PROTECTION CHECKS
	rom[0x37b4/2] = 0x4e75;	// 0037B4: 4E56 0000 link    A6, #$0
}
#endif
#if 0
static DRIVER_INIT( xymg )
{
	UINT16 *rom = (UINT16 *) memory_region(machine, "maincpu");

	lhb_decrypt(machine);

	// PROTECTION CHECKS
	rom[0x00502/2]	=	0x6006;		// 000502: 6050         bra 554
	rom[0x0fc1c/2]	=	0x6036;		// 00FC1C: 6736         beq fc54  (fills palette with red otherwise)
	rom[0x1232a/2]	=	0x6036;		// 01232A: 6736         beq 12362 (fills palette with red otherwise)
	rom[0x18244/2]	=	0x6036;		// 018244: 6736         beq 1827c (fills palette with red otherwise)
	rom[0x1e15e/2]	=	0x6036;		// 01E15E: 6736         beq 1e196 (fills palette with red otherwise)
	rom[0x22286/2]	=	0x6000;		// 022286: 6700 02D2    beq 2255a (fills palette with green otherwise)
	rom[0x298ce/2]	=	0x6036;		// 0298CE: 6736         beq 29906 (fills palette with red otherwise)
	rom[0x2e07c/2]	=	0x6036;		// 02E07C: 6736         beq 2e0b4 (fills palette with red otherwise)
	rom[0x38f1c/2]	=	0x6000;		// 038F1C: 6700 071C    beq 3963a (ASIC11 ERROR 1)
	rom[0x390e8/2]	=	0x6000;		// 0390E8: 6700 0550    beq 3963a (ASIC11 ERROR 2)
	rom[0x3933a/2]	=	0x6000;		// 03933A: 6700 02FE    beq 3963a (ASIC11 ERROR 3)
	rom[0x3955c/2]	=	0x6000;		// 03955C: 6700 00DC    beq 3963a (ASIC11 ERROR 4)
	rom[0x397f4/2]	=	0x6000;		// 0397F4: 6700 02C0    beq 39ab6 (fills palette with green otherwise)
	rom[0x39976/2]	=	0x6000;		// 039976: 6700 013E    beq 39ab6 (fills palette with green otherwise)
	rom[0x39a7e/2]	=	0x6036;		// 039A7E: 6736         beq 39ab6 (fills palette with green otherwise)
	rom[0x4342c/2]	=	0x4e75;		// 04342C: 4E56 0000    link A6, #$0
	rom[0x49966/2]	=	0x6036;		// 049966: 6736         beq 4999e (fills palette with blue otherwise)
	rom[0x58140/2]	=	0x6036;		// 058140: 6736         beq 58178 (fills palette with red otherwise)
	rom[0x5e05a/2]	=	0x6036;		// 05E05A: 6736         beq 5e092 (fills palette with red otherwise)
	rom[0x5ebf0/2]	=	0x6000;		// 05EBF0: 6700 0208    beq 5edfa (fills palette with red otherwise)
	rom[0x5edc2/2]	=	0x6036;		// 05EDC2: 6736         beq 5edfa (fills palette with green otherwise)
	rom[0x5f71c/2]	=	0x6000;		// 05F71C: 6700 01F2    beq 5f910 (fills palette with green otherwise)
	rom[0x5f8d8/2]	=	0x6036;		// 05F8D8: 6736         beq 5f910 (fills palette with red otherwise)
	rom[0x64836/2]	=	0x6036;		// 064836: 6736         beq 6486e (fills palette with red otherwise)
}
#endif
#if 0
static DRIVER_INIT( dbc )
{
	UINT16 *rom = (UINT16 *) memory_region(machine, "maincpu");

	dbc_decrypt(machine);

	// PROTECTION CHECKS
	rom[0x04c42/2]	=	0x602e;		// 004C42: 6604         bne 4c48  (rom test error otherwise)
	rom[0x08694/2]	=	0x6008;		// 008694: 6408         bcc 869e  (fills screen with characters otherwise)
	rom[0x0a05e/2]	=	0x4e71;		// 00A05E: 6408         bcc a068  (fills screen with characters otherwise)
	rom[0x0bec2/2]	=	0x6008;		// 00BEC2: 6408         bcc becc  (fills screen with characters otherwise)
	rom[0x0c0d4/2]	=	0x600a;		// 00C0D4: 640A         bcc c0e0  (wrong game state otherwise)
	rom[0x0c0f0/2]	=	0x4e71;		// 00C0F0: 6408         bcc c0fa  (wrong palette otherwise)
	rom[0x0e292/2]	=	0x6008;		// 00E292: 6408         bcc e29c  (fills screen with characters otherwise)
	rom[0x11b42/2]	=	0x6008;		// 011B42: 6408         bcc 11b4c (wrong game state otherwise)
	rom[0x11b5c/2]	=	0x4e71;		// 011B5C: 6408         bcc 11b66 (wrong palette otherwise)
	rom[0x170ae/2]	=	0x4e71;		// 0170AE: 6408         bcc 170b8 (fills screen with characters otherwise)
	rom[0x1842a/2]	=	0x6024;		// 01842A: 6724         beq 18450 (ASIC11 ERROR otherwise)
	rom[0x18538/2]	=	0x6008;		// 018538: 6408         bcc 18542 (wrong game state otherwise)
	rom[0x18552/2]	=	0x4e71;		// 018552: 6408         bcc 1855c (wrong palette otherwise)
	rom[0x18c0e/2]	=	0x6006;		// 018C0E: 6406         bcc 18c16 (fills screen with characters otherwise)
	rom[0x1923e/2]	=	0x4e71;		// 01923E: 6408         bcc 19248 (fills screen with characters otherwise)

	// Fix for the palette fade on title screen
//  rom[0x19E90/2]  =   0x00ff;
}
#endif
/***************************************************************************

    ROMs Loading

***************************************************************************/

/***************************************************************************

Champion List II
IGS, 1996

PCB Layout
----------

IGS PCB NO-0115
|---------------------------------------------|
|                  M6295  IGSS0503.U38        |
|  UM3567  3.57945MHz                         |
|                          DSW3               |
|                          DSW2     PAL       |
| IGSM0502.U5              DSW1    6264       |
| IGSM0501.U7     PAL              6264       |
|                 PAL                         |
|                 PAL            IGS011       |
|                 PAL                         |
|                 PAL                         |
|                                             |
|   MC68HC000P10          22MHz  TC524258AZ-10|
|           6264         8255    TC524258AZ-10|
|    BATT   6264   MAJ2V185H.U29 TC524258AZ-10|
|                                TC524258AZ-10|
|---------------------------------------------|

Notes:
        68k clock: 7.3333MHz (i.e. 22/3)
      M6295 clock: 1.0476MHz (i.e. 22/21) \
         M6295 SS: HIGH                   / Therefore sampling freq = 7.936363636kHz (i.e. 1047600 / 132)
           UM3567: Compatible with YM2413, clock = 3.57945MHz
            HSync: 15.78kHz
            VSync: 60Hz

***************************************************************************/

ROM_START( chmplst2 )
	ROM_REGION( 0x80000, "maincpu", 0 )
	ROM_LOAD16_WORD_SWAP( "maj2v185h.u29", 0x00000, 0x80000, CRC(2572d59a) SHA1(1d5362e209dadf8b21c10d1351d4bb038bfcaaef) )

	ROM_REGION( 0x200000, "gfx1", 0 )
	ROM_LOAD( "igsm0501.u7", 0x00000, 0x200000, CRC(1c952bd6) SHA1(a6b6f1cdfb29647e81c032ffe59c94f1a10ceaf8) )

	ROM_REGION( 0x80000, "gfx2", 0 ) // high order bit of graphics (5th bit)
	/* these are identical ..seems ok as igs number is same, only ic changed */
	ROM_LOAD( "igsm0502.u4", 0x00000, 0x80000, CRC(5d73ae99) SHA1(7283aa3d6b15ceb95db80756892be46eb997ef15) )
	ROM_LOAD( "igsm0502.u5", 0x00000, 0x80000, CRC(5d73ae99) SHA1(7283aa3d6b15ceb95db80756892be46eb997ef15) )

	ROM_REGION( 0x80000, "oki", 0 )
	ROM_LOAD( "igss0503.u38", 0x00000, 0x80000, CRC(c9609c9c) SHA1(f036e682b792033409966e84292a69275eaa05e5) )	// 2 banks
ROM_END

/***************************************************************************

Dragon World (World, V0400)
(C) 1997 IGS / ALTA

Chips:
  1x 68000 (main)
  1x AC0A26 (equivalent to OKI M6295)(sound)
  1x 6564L (equivalent to YM3812)(sound)
  1x custom IGS003c (marked on PCB as 8255)
  1x oscillator 22.0000MHz (main)
  1x oscillator 3.579545MHz (sound)
  1x custom IGS011 (FPGA?)

ROMs:
  1x MX27C4096 (u3)(main) (dumped)
  1x custom IGSD0301 (mask rom) (not dumped yet)
  1x NEC D27C2001D (IGSS0302)(sound) (not dumped yet)

Notes:
  1x JAMMA edge connector
  1x trimmer (volume)
  3x 8 switches dips
  PCB serial number is: 0105-5

***************************************************************************/
#if 0
ROM_START( drgnwrld )
	ROM_REGION( 0x80000, "maincpu", 0 )
	ROM_LOAD16_WORD_SWAP( "chinadr-v0400.u3", 0x00000, 0x80000, CRC(a6daa2b8) SHA1(0cbfd001c1fd82a6385453d1c2a808add67746af) )

	ROM_REGION( 0x400000, "gfx1", 0 )
	ROM_LOAD( "igs-d0301.u39", 0x000000, 0x400000, CRC(78ab45d9) SHA1(c326ee9f150d766edd6886075c94dea3691b606d) )

	ROM_REGION( 0x40000, "oki", 0 )
	ROM_LOAD( "igs-s0302.u43", 0x00000, 0x40000, CRC(fde63ce1) SHA1(cc32d2cace319fe4d5d0aa96d7addb2d1def62f2) )
ROM_END
#endif
/***************************************************************************

Dragon World (World, V0300)
(C) 1995 IGS

Chips:
  1x MC68HC000P10 (main)
  1x CUSTOM IGS011

ROMs:
  1x MX27C4096 (main)

***************************************************************************/
#if 0
ROM_START( drgnwrldv30 )
	ROM_REGION( 0x80000, "maincpu", 0 )
	ROM_LOAD16_WORD_SWAP( "chinadr-v0300.u3", 0x00000, 0x80000, CRC(5ac243e5) SHA1(50cccff0307239187ac2b65331ad2bcc666f8033) )

	ROM_REGION( 0x400000, "gfx1", 0 )
	ROM_LOAD( "igs-d0301.u39", 0x000000, 0x400000, CRC(78ab45d9) SHA1(c326ee9f150d766edd6886075c94dea3691b606d) )

	ROM_REGION( 0x40000, "oki", 0 )
	ROM_LOAD( "igs-s0302.u43", 0x00000, 0x40000, CRC(fde63ce1) SHA1(cc32d2cace319fe4d5d0aa96d7addb2d1def62f2) )
ROM_END
#endif
#if 0
ROM_START( drgnwrldv21 )
	ROM_REGION( 0x80000, "maincpu", 0 )
	ROM_LOAD16_WORD_SWAP( "china-dr-v-0210.u3", 0x00000, 0x80000, CRC(60c2b018) SHA1(58563e3ccb51bd9d8362aa17c23743bb5a593c3b) )

	ROM_REGION( 0x400000, "gfx1", 0 )
	ROM_LOAD( "igs-d0301.u39", 0x000000, 0x400000, CRC(78ab45d9) SHA1(c326ee9f150d766edd6886075c94dea3691b606d) )

	ROM_REGION( 0x40000, "oki", 0 )
	ROM_LOAD( "china-dr-sp.u43", 0x00000, 0x40000, CRC(fde63ce1) SHA1(cc32d2cace319fe4d5d0aa96d7addb2d1def62f2) )
ROM_END
#endif

/***************************************************************************

Zhong Guo Long (China, V0303)
(C) 1995 IGS

Chips:
  CPU 1x MC68HC000P10 (main)
  1x AR17961-AP0642 (equivalent to OKI M6295)(sound)
  1x 6564L (equivalent to YM3812)(sound)
  1x LS138S (sound)
  1x LM7805CV (sound)
  1x UPC1242H (sound)
  1x custom IGS003 (marked on PCB as 8255)
  1x oscillator 22.0000MHz (main)
  1x oscillator 3.579545MHz (sound)
  1x custom IGS011 (FPGA?)

ROMs:
  1x maskrom 256x16 IGSD0303 (u3)(main)
  1x maskrom 2Mx16 UM23V32000 (IGSD0301)(u39)(gfx)
  1x empty socket for 27C040 (u44)
  1x maskrom NEC D27C2001D (IGSS0302)(u43)(sound)
  2x PAL16L8ACN (u17,u18)(read protected)
  2x PALATF22V10B (u15,u45)
  1x empty space for additional PALATV750 (u16)

Notes:
  1x JAMMA edge connector
  1x trimmer (volume)
  3x 8x2 switches DIP

The PCB is perfectly working, empty spaces and empty sockets are clearly intended to be empty.
25/07/2007 f205v Corrado Tomaselli Gnoppi

***************************************************************************/
#if 0
ROM_START( drgnwrldv10c )
	ROM_REGION( 0x80000, "maincpu", 0 )
	ROM_LOAD16_WORD_SWAP( "igs-d0303.u3", 0x00000, 0x80000, CRC(3b3c29bb) SHA1(77b7e58104314303985c283cce3aec40bd7b9334) )

	ROM_REGION( 0x400000, "gfx1", 0 )
	//ROM_LOAD( "igs-0301.u39", 0x000000, 0x400000, CRC(655ab941) SHA1(4bbefb27e8971446998508969661042c5111bc72) ) // bad dump
	ROM_LOAD( "igs-d0301.u39", 0x000000, 0x400000, CRC(78ab45d9) SHA1(c326ee9f150d766edd6886075c94dea3691b606d) )

	ROM_REGION( 0x40000, "oki", 0 )
	ROM_LOAD( "igs-s0302.u43", 0x00000, 0x40000, CRC(fde63ce1) SHA1(cc32d2cace319fe4d5d0aa96d7addb2d1def62f2) )

	ROM_REGION( 0x40000, "user1", 0 )
	ROM_LOAD( "ccdu15.u15", 0x000, 0x2e5, CRC(a15fce69) SHA1(3e38d75c7263bfb36aebdbbd55ebbdd7ca601633) )
	//ROM_LOAD( "ccdu17.u17.bad.dump", 0x000, 0x104, CRC(e9cd78fb) SHA1(557d3e7ef3b25c1338b24722cac91bca788c02b8) )
	//ROM_LOAD( "ccdu18.u18.bad.dump", 0x000, 0x104, CRC(e9cd78fb) SHA1(557d3e7ef3b25c1338b24722cac91bca788c02b8) )
	ROM_LOAD( "ccdu45.u45", 0x000, 0x2e5, CRC(a15fce69) SHA1(3e38d75c7263bfb36aebdbbd55ebbdd7ca601633) )
ROM_END
#endif
/***************************************************************************

Chuugokuryuu (china dragon jpn ver.)
(c)IGS
Distributed by ALTA

MAIN CPU   : 68000
I/O        : IGS003 (=8255)
SOUND ?    : 6564L  (=OPL?)  , AR17961 (=M6295?)
CRTC ?     : IGS011
SOUND CPU? : IGSD0301 (DIP 42P)
OSC        : 22Mhz , 3.579545Mhz
DIPSW      : 8bitx 3 (SW3 is not used)
OTHER      : IGS012

MAIN PRG   : "CHINA DRAGON U020J" (japan)
SOUND PRG? : "CHINA DRAGON SP"
SOUND DATA?: "CHINA DRAGON U44"

***************************************************************************/
#if 0
ROM_START( drgnwrldv20j )
	ROM_REGION( 0x80000, "maincpu", 0 )
	ROM_LOAD16_WORD_SWAP( "china_jp.v20", 0x00000, 0x80000, CRC(9e018d1a) SHA1(fe14e6344434cabf43685e50fd49c90f05f565be) )

	ROM_REGION( 0x420000, "gfx1", 0 )
	// igs-d0301.u39 wasn't in this set
	ROM_LOAD( "igs-d0301.u39", 0x000000, 0x400000, CRC(78ab45d9) SHA1(c326ee9f150d766edd6886075c94dea3691b606d) )
	ROM_LOAD( "china.u44",     0x400000, 0x020000, CRC(10549746) SHA1(aebd83796679c85b43ad514b2771897f94e61294) ) // 1xxxxxxxxxxxxxxxx = 0x00

	ROM_REGION( 0x40000, "oki", 0 )
	ROM_LOAD( "igs-s0302.u43", 0x00000, 0x40000, CRC(fde63ce1) SHA1(cc32d2cace319fe4d5d0aa96d7addb2d1def62f2) ) // original label: "sp"
ROM_END
#endif
#if 0
ROM_START( drgnwrldv21j )
	ROM_REGION( 0x80000, "maincpu", 0 )
	ROM_LOAD16_WORD_SWAP( "v-021j", 0x00000, 0x80000, CRC(2f87f6e4) SHA1(d43065b078fdd9605c121988ad3092dce6cf0bf1) )

	ROM_REGION( 0x420000, "gfx1", 0 )
	ROM_LOAD( "igs-d0301.u39", 0x000000, 0x400000, CRC(78ab45d9) SHA1(c326ee9f150d766edd6886075c94dea3691b606d) )
	ROM_LOAD( "cg",            0x400000, 0x020000, CRC(2dda0be3) SHA1(587b7cab747d4336515c98eb3365341bb6c7e5e4) )

	ROM_REGION( 0x40000, "oki", 0 )
	ROM_LOAD( "igs-s0302.u43", 0x00000, 0x40000, CRC(fde63ce1) SHA1(cc32d2cace319fe4d5d0aa96d7addb2d1def62f2) ) // original label: "sp"
ROM_END
#endif
/***************************************************************************

    Dong Fang Zhi Zhu (Hong Kong version of Zhong Guo Long, V011H)

***************************************************************************/
#if 0
ROM_START( drgnwrldv11h )
	ROM_REGION( 0x80000, "maincpu", 0 )
	ROM_LOAD16_WORD_SWAP( "c_drgn_hk.u3", 0x00000, 0x80000, CRC(182037ce) SHA1(141b698777533e57493e588d2526523d4bd3e17d) )

	ROM_REGION( 0x400000, "gfx1", 0 )
	ROM_LOAD( "igs-d0301.u39", 0x000000, 0x400000, CRC(78ab45d9) SHA1(c326ee9f150d766edd6886075c94dea3691b606d) )

	ROM_REGION( 0x40000, "oki", 0 )
	ROM_LOAD( "igs-s0302.u43", 0x00000, 0x40000, CRC(fde63ce1) SHA1(cc32d2cace319fe4d5d0aa96d7addb2d1def62f2) )
ROM_END
#endif

/***************************************************************************

    The Great Wall?

    Other files in the zip:

     5.942    16-6126.G16
    14.488    U3-9911.G22
    14.488    U4-82E6.G22
    14.488    U5-6C5E.G22

***************************************************************************/

ROM_START( grtwall )
	ROM_REGION( 0x80000, "maincpu", 0 )
	ROM_LOAD16_WORD_SWAP( "wlcc4096.rom", 0x00000, 0x80000, CRC(3b16729f) SHA1(4ef4e5cbd6ccc65775e36c2c8b459bc1767d6574) )
	ROM_CONTINUE        (                 0x00000, 0x80000 ) // 1ST+2ND IDENTICAL

	ROM_REGION( 0x280000, "gfx1", ROMREGION_ERASE00 )
	ROM_LOAD( "m0201-ig.160", 0x000000, 0x200000, CRC(ec54452c) SHA1(0ee7ffa3d4845af083944e64faf5a1c78247aaa2) )
	ROM_LOAD( "grtwall.gfx",  0x200000, 0x080000, CRC(1f7ad299) SHA1(ab0a8fb31906519b9352ba172def48456e8d565c) )

	ROM_REGION( 0x80000, "oki", 0 )
	ROM_LOAD( "040-c3c2.snd", 0x00000, 0x80000, CRC(220949aa) SHA1(1e0dba168a0687d32aaaed42714ae24358f4a3e7) ) // 2 banks
	ROM_CONTINUE(             0x00000, 0x80000 ) // 1ST+2ND IDENTICAL
ROM_END

/***************************************************************************

    Long Hu Bang (V035C)

    Other files in the zip:

     5.938    16V8.jed
    14.464    LHB-U33.jed
    14.488    LHB-U34.jed
    14.488    LHB-U35.jed

***************************************************************************/
#if 0
ROM_START( lhb )
	ROM_REGION( 0x80000, "maincpu", 0 )
	// identical to LHB-4096
	ROM_LOAD16_WORD_SWAP( "v305j-409", 0x00000, 0x80000, CRC(701de8ef) SHA1(4a77160f642f4de02fa6fbacf595b75c0d4a505d) )

	ROM_REGION( 0x200000, "gfx1", 0 )
	ROM_LOAD( "m0201-ig.160", 0x000000, 0x200000, CRC(ec54452c) SHA1(0ee7ffa3d4845af083944e64faf5a1c78247aaa2) )

	ROM_REGION( 0x80000, "oki", 0 )
	// identical to 040-c3c2.snd
	ROM_LOAD( "m0202.snd", 0x00000, 0x80000, CRC(220949aa) SHA1(1e0dba168a0687d32aaaed42714ae24358f4a3e7) ) // 2 banks
	ROM_CONTINUE(          0x00000, 0x80000 ) // 1ST+2ND IDENTICAL
ROM_END
#endif
/***************************************************************************

Long Hu Bang (V033C)

PCB Layout
----------

IGS PCB NO-T0093
|---------------------------------------|
|uPD1242H     VOL       DSW5            |
|  IGS_M0202                            |
|             AR17961   DSW4            |
|                             CY7C185   |
|                       DSW3            |
|      8255             DSW2  CY7C185   |
|                       DSW1            |
|1                                      |
|8                           DIP32      |
|W             |-------|                |
|A             |       |     IGS_M0201  |
|Y  BATTERY    |IGS011 |                |
|              |       |          PAL   |
|              |-------|                |
|                         TC524256      |
|    MAJ_V-033C                         |
|                         TC524256      |
|1      6264                            |
|0                        TC524256      |
|W      6264    22.285MHz               |
|A         PAL            TC524256      |
|Y         PAL                          |
| SPDT_SW  PAL      68000               |
|---------------------------------------|
Notes:
      Uses common 10-way/18-way Mahjong pinout
      TC524256 - Toshiba TC524256BZ-80 256k x4 Dual Port VRAM (ZIP28)
      CY7C185  - Cypress CY7C185-20PC 8k x8 SRAM (DIP28)
      6264     - UT6264PC-70LL 8k x8 SRAM (DIP28)
      IGS011   - Custom IGS IC (QFP160)
      AR17961  - == OkiM6295 (QFP44)
      DIP32    - Empty socket, maybe a ROM missing, maybe not used?

      ROMs -
            MAJ_V-033C - Main Program (27C4096)
            IGS_M0201  - Graphics (16M maskROM)
            IGS_M0202  - OKI samples (4M maskROM)

***************************************************************************/
#if 0
ROM_START( lhba )
	ROM_REGION( 0x80000, "maincpu", 0 )
	ROM_LOAD16_WORD_SWAP( "maj_v-033c.u30", 0x00000, 0x80000, CRC(02a0b716) SHA1(cd0ee32ea69f66768196b0e9b4df0fae3af84ed3) )

	ROM_REGION( 0x200000, "gfx1", 0 )
	ROM_LOAD( "igs_m0201.u15", 0x000000, 0x200000, CRC(ec54452c) SHA1(0ee7ffa3d4845af083944e64faf5a1c78247aaa2) )

	ROM_REGION( 0x80000, "oki", 0 )
	// identical to 040-c3c2.snd
	ROM_LOAD( "igs_m0202.u39", 0x00000, 0x80000, CRC(106ac5f7) SHA1(5796a880c3424e3d2251b2223a0e594957afecaf) ) // 2 banks

ROM_END
#endif
/***************************************************************************

Da Ban Cheng

PCB Layout
----------

IGS PCB NO-T0084-1
|---------------------------------------|
|uPD1242H     VOL       DSW5            |
|  IGS_M0202                            |
|             AR17961   DSW4            |
|                             CY7C185   |
|                       DSW3            |
|      8255             DSW2  CY7C185   |
|                       DSW1            |
|1                                      |
|8                           MAJ-H_CG   |
|W    PAL      |-------|                |
|A             |       |     IGS_M0201  |
|Y    PAL      |IGS011 |                |
|              |       |          PAL   |
|     PAL      |-------|                |
|                         TC524256      |
|     6264                              |
|                         TC524256      |
|1    6264                              |
|0           22.0994MHz   TC524256      |
|W    MAJ-H_V027H                       |
|A                        TC524256      |
|Y         BATTERY                      |
| SPDT_SW           68000               |
|---------------------------------------|
Notes:
      Uses common 10-way/18-way Mahjong pinout
      TC524256 - Toshiba TC524256BZ-80 256k x4 Dual Port VRAM (ZIP28)
      CY7C185  - Cypress CY7C185-20PC 8k x8 SRAM (DIP28)
      6264     - UT6264PC-70LL 8k x8 SRAM (DIP28)
      IGS011   - Custom IGS IC (QFP160)
      AR17961  - == OkiM6295 (QFP44)

      ROMs -
            MAJ-H_V027H- Main Program (27C4096)
            IGS_M0201  - Graphics (16M maskROM)
            IGS_M0202  - OKI samples (4M maskROM)
            MAJ-H_CG   - Graphics (27c4001 EPROM)

***************************************************************************/
#if 0
ROM_START( dbc )
	ROM_REGION( 0x80000, "maincpu", 0 )
	ROM_LOAD16_WORD_SWAP( "maj-h_v027h.u30", 0x00000, 0x80000, CRC(5d5ccd5b) SHA1(7a1223923f9a5825fd919ae9a36912284e705382) )

	ROM_REGION( 0x280000, "gfx1", 0 )
	ROM_LOAD( "igs_m0201.u15", 0x000000, 0x200000, CRC(ec54452c) SHA1(0ee7ffa3d4845af083944e64faf5a1c78247aaa2) )
	ROM_LOAD( "maj-h_cg.u8",   0x200000, 0x080000, CRC(ee45cc46) SHA1(ed011f758a02026222994aaea0677a4e9580fbda) )	// 1xxxxxxxxxxxxxxxxxx = 0x00

	ROM_REGION( 0x80000, "oki", 0 )
	ROM_LOAD( "igs_m0202.u39", 0x00000, 0x80000, CRC(106ac5f7) SHA1(5796a880c3424e3d2251b2223a0e594957afecaf) ) // 2 banks
ROM_END
#endif
/***************************************************************************

Virtua Bowling by IGS

PCB # 0101

U45  (27c240) is probably program
next to 68000 processor
U68,U69 probably images   (27c800 - mask)
U67, U66 sound and ????  (27c040)

ASIC chip used

SMD - custom chip IGS 011      F5XD  174
SMD - custom --near sound section - unknown -- i.d. rubbed off
SMD - custom  -- near inputs and 68000  IGS 012    9441EK001

XTL near sound 33.868mhz
XTL near 68000  22.0000mhz

there are 4 banks of 8 dip switches

***************************************************************************/
#if 0
ROM_START( vbowl )
	ROM_REGION( 0x80000, "maincpu", 0 )
	ROM_LOAD16_WORD_SWAP( "bowlingv101xcm.u45", 0x00000, 0x80000, BAD_DUMP CRC(ab8e3f1f) SHA1(69159e22559d6a26fe2afafd770aa640c192ba4b) )

	ROM_REGION( 0x400000 * 2, "gfx1", 0)
	ROM_LOAD( "vrbowlng.u69", 0x000000, 0x400000, CRC(b0d339e8) SHA1(a26a5e0202a78e8cdc562b10d64e14eadfa4e115) )
	// extra space to expand every 4 bits to 8

	ROM_REGION( 0x100000, "gfx2", ROMREGION_INVERT )
	ROM_LOAD( "vrbowlng.u68", 0x000000, 0x100000, CRC(b0ce27e7) SHA1(6d3ef97edd606f384b1e05b152fbea12714887b7) )

	ROM_REGION( 0x400000, "ics", 0 )
	ROM_LOAD( "vrbowlng.u67", 0x00000, 0x80000, CRC(53000936) SHA1(e50c6216f559a9248c095bdfae05c3be4be79ff3) )	// 8 bit signed mono & u-law
	ROM_LOAD( "vrbowlng.u66", 0x80000, 0x80000, CRC(f62cf8ed) SHA1(c53e47e2c619ed974ad40ee4aaa4a35147ea8311) )	// 8 bit signed mono
	ROM_COPY( "ics", 0, 0x100000,0x100000)
	ROM_COPY( "ics", 0, 0x200000,0x100000)
	ROM_COPY( "ics", 0, 0x300000,0x100000)
ROM_END
#endif
#if 0
ROM_START( vbowlj )
	ROM_REGION( 0x80000, "maincpu", 0 )
	ROM_LOAD16_WORD_SWAP( "vrbowlng.u45", 0x00000, 0x80000, CRC(091c19c1) SHA1(5a7bfbee357122e9061b38dfe988c3853b0984b0) ) // second half all 00

	ROM_REGION( 0x400000 * 2, "gfx1", 0)
	ROM_LOAD( "vrbowlng.u69", 0x000000, 0x400000, CRC(b0d339e8) SHA1(a26a5e0202a78e8cdc562b10d64e14eadfa4e115) )
	// extra space to expand every 4 bits to 8

	ROM_REGION( 0x100000, "gfx2", ROMREGION_INVERT )
	ROM_LOAD( "vrbowlng.u68", 0x000000, 0x100000, CRC(b0ce27e7) SHA1(6d3ef97edd606f384b1e05b152fbea12714887b7) )

	ROM_REGION( 0x400000, "ics", 0 )
	ROM_LOAD( "vrbowlng.u67", 0x00000, 0x80000, CRC(53000936) SHA1(e50c6216f559a9248c095bdfae05c3be4be79ff3) )	// 8 bit signed mono & u-law
	ROM_LOAD( "vrbowlng.u66", 0x80000, 0x80000, CRC(f62cf8ed) SHA1(c53e47e2c619ed974ad40ee4aaa4a35147ea8311) )	// 8 bit signed mono
	ROM_COPY( "ics", 0, 0x100000,0x100000)
	ROM_COPY( "ics", 0, 0x200000,0x100000)
	ROM_COPY( "ics", 0, 0x300000,0x100000)
ROM_END
#endif

/***************************************************************************

    Xing Yen Man Guan

    Other files in the zip:

    14.484 U33-82E6.jed
    14.484 U34-1.jed
    14.484 U35-7068.jed

***************************************************************************/
#if 0
ROM_START( xymg )
	ROM_REGION( 0x80000, "maincpu", 0 )
	ROM_LOAD16_WORD_SWAP( "u30-ebac.rom", 0x00000, 0x80000, CRC(7d272b6f) SHA1(15fd1be23cabdc77b747541f5cd9fed6b08be4ad) )

	ROM_REGION( 0x280000, "gfx1", 0 )
	ROM_LOAD( "m0201-ig.160", 0x000000, 0x200000, CRC(ec54452c) SHA1(0ee7ffa3d4845af083944e64faf5a1c78247aaa2) )
	ROM_LOAD( "ygxy-u8.rom",  0x200000, 0x080000, CRC(56a2706f) SHA1(98bf4b3153eef53dd449e2538b4b7ff2cc2fe6fa) )

	ROM_REGION( 0x80000, "oki", 0 )
	// identical to 040-c3c2.snd
	ROM_LOAD( "m0202.snd", 0x00000, 0x80000, CRC(220949aa) SHA1(1e0dba168a0687d32aaaed42714ae24358f4a3e7) ) // 2 banks
	ROM_CONTINUE(          0x00000, 0x80000 ) // 1ST+2ND IDENTICAL
ROM_END
#endif

/***************************************************************************

    Game Drivers

***************************************************************************/

//GAME( 1995, lhb,      0,        lhb,      lhb,      lhb,      ROT0, "IGS",        "Long Hu Bang (V035C)",                 0 )
//GAME( 1995, lhba,     lhb,      lhb,      lhb,      lhba,     ROT0, "IGS",        "Long Hu Bang (V033C)",                 0 )
//GAME( 1995, dbc,      0,        lhb,      lhb,      dbc,      ROT0, "IGS",        "Da Ban Cheng (V027H)",                 0 )
GAME( 1996, chmplst2, 0,        chmplst2, chmplst2, chmplst2, ROT0, "IGS",        "Long Hu Bang II (V185H)",              0 )
//GAME( 1996, xymg,     0,        xymg,     xymg,     xymg,     ROT0, "IGS",        "Xing Yun Man Guan (V651C)",            0 )
GAME( 1996, grtwall,  xymg,     grtwall,  grtwall,  grtwall,  ROT0, "IGS",        "Wan Li Chang Cheng (V638C)",           0 )
//GAME( 1996, vbowl,    0,        vbowl,    vbowl,    vbowl,    ROT0, "IGS",        "Virtua Bowling (World, V101XCM)",      GAME_IMPERFECT_SOUND )
//GAME( 1996, vbowlj,   vbowl,    vbowl,    vbowlj,   vbowlj,   ROT0, "IGS / Alta", "Virtua Bowling (Japan, V100JCM)",      GAME_IMPERFECT_SOUND )

//GAME( 1997, drgnwrld,     0,        drgnwrld, drgnwrld,  drgnwrld,     ROT0, "IGS",        "Dragon World (World, V040O)",          0 )
//GAME( 1995, drgnwrldv30,  drgnwrld, drgnwrld, drgnwrld,  drgnwrldv30,  ROT0, "IGS",        "Dragon World (World, V030O)",          0 )
//GAME( 1995, drgnwrldv21,  drgnwrld, drgnwrld, drgnwrld,  drgnwrldv21,  ROT0, "IGS",        "Dragon World (World, V021O)",          0 )
//GAME( 1995, drgnwrldv21j, drgnwrld, drgnwrld, drgnwrldj, drgnwrldv21j, ROT0, "IGS / Alta", "Zhong Guo Long (Japan, V021J)",        0 )
//GAME( 1995, drgnwrldv20j, drgnwrld, drgnwrld, drgnwrldj, drgnwrldv20j, ROT0, "IGS / Alta", "Zhong Guo Long (Japan, V020J)",        0 )
//GAME( 1995, drgnwrldv10c, drgnwrld, drgnwrld, drgnwrldc, drgnwrldv10c, ROT0, "IGS",        "Zhong Guo Long (China, V010C)",        0 )
//GAME( 1995, drgnwrldv11h, drgnwrld, drgnwrld, drgnwrldc, drgnwrldv11h, ROT0, "IGS",        "Dong Fang Zhi Zhu (Hong Kong, V011H)", 0 )

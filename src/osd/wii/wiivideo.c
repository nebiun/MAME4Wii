//============================================================
//
//  Copyright (c) 1996-2009, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//  MAME Wii by Toad King
//
//============================================================
#include <ogcsys.h>
#include <unistd.h>
#include "osdcore.h"
#include "wiimame.h"
#include "wiivideo.h"
#include "render.h"
#include "mame.h"
#include "wiimame.h"

#define WII_VIDEO_SLOT		(256*3)

typedef struct _gx_tex gx_tex;
struct _gx_tex
{
	gx_tex *next;
	void *addr;
	void *data;
	u32 size;
	u8 format;
};

static GXRModeObj *vmode;
static GXTexObj texObj;
static GXTexObj blankTex;

static void *xfb[2];
static int currfb;
static unsigned char *gp_fifo;

static int screen_width;
static int screen_height;
static int hofs;
static int vofs;

static gx_tex *firstTex = NULL;
static gx_tex *lastTex = NULL;
static gx_tex *firstScreenTex = NULL;
static gx_tex *lastScreenTex = NULL;

static const render_primitive_list *currlist[WII_VIDEO_SLOT] = { NULL };
static const render_primitive_list **r_index, **w_index;
static lwp_t vidthread = LWP_THREAD_NULL;
static lwpq_t videoqueue;
static bool wii_stopping = false;
static int _wii_screen_width;
	
static unsigned char blanktex[2*16] __attribute__((aligned(32))) = {
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};

#define DEFAULT_FIFO_SIZE	(256*1024)

int wii_screen_width(void)
{
	return _wii_screen_width;
}

void wii_init_dimensions(void)
{
	screen_width = wii_screen_width();
	screen_height = 480;
	hofs = (screen_width - screen_width * options_get_float(mame_options(), "safearea")) / 2;
	vofs = (screen_height - screen_height * options_get_float(mame_options(), "safearea")) / 2;
	screen_width -= hofs * 2;
	screen_height -= vofs * 2;
}

/* adapted from rendersw.c, might not work because as far as I can tell, only
   laserdisc uses YCbCr textures, and we don't support that be default */

static inline u8 clamp16_shift8(u32 x)
{
	return (((s32) x < 0) ? 0 : (x > 65535 ? 255: x >> 8));
}

static inline void GXGetRGBA5551_YUY16(u32 y, u16 *v0, u16 *v1)
{
	u8 cb = y >> 8;
	u8 cr = y >> 16;
	u32 r, g, b, common;
	u32 yuy_rgb = 0;

	common = 298 * y - 56992;
	r = (common +            409 * cr);
	g = (common - 100 * cb - 208 * cr + 91776);
	b = (common + 516 * cb - 13696);

	yuy_rgb = MAKE_RGB(clamp16_shift8(r), clamp16_shift8(g), clamp16_shift8(b)) | 0xFF;

	*v0 = (u16)((yuy_rgb >> 16) & 0x0000FFFF);
	*v1 = (u16)(yuy_rgb & 0x0000FFFF);
}

/* heavily adapted from Wii64 */

static inline u16 GXGetRGBA5551_RGB5A3(u16 c)
{
	if ((c&1) != 0)
		c = 0x8000|(((c>>11)&0x1F)<<10)|(((c>>6)&0x1F)<<5)|((c>>1)&0x1F);   //opaque texel
	else
		c = 0x0000|(((c>>12)&0xF)<<8)|(((c>>7)&0xF)<<4)|((c>>2)&0xF);   //transparent texel
	return (u16) c;
}

static inline void GXGetRGBA8888_RGBA8(u32 c, u16 *v0, u16 *v1)
{
	*v1 = (u16)(c & 0x0000FFFF);			/* GGBB */
	*v0 = (u16)((c >> 16) & 0x0000FFFF);	/* AARR */
}

static inline u16 GXGetRGBA5551_PALETTE16(u16 c, int i, const rgb_t *palette)
{
	u32 rgb = palette[c];
	if (i == TEXFORMAT_PALETTE16)
		return rgb_to_rgb15(rgb) | (1 << 15);

	return (u16)(((RGB_RED(rgb) >> 4) << 8) | ((RGB_GREEN(rgb) >> 4) << 4) | ((RGB_BLUE(rgb) >> 4) << 0) | ((RGB_ALPHA(rgb) >> 5) << 12));
}

static inline gx_tex *create_texture(render_primitive *prim)
{
	int j, k, l, x, y, tx, ty, bpp;
	int flag = PRIMFLAG_GET_TEXFORMAT(prim->flags);
	int rawwidth = prim->texture.width;
	int rawheight = prim->texture.height;
	int width = ((rawwidth + 3) & (~3));
	int height = ((rawheight + 3) & (~3));
	u8 *data = prim->texture.base;
	void *src;
	u16 *fixed;
	gx_tex *newTex = NULL;

	j = 0;

	switch(flag)
	{
	case TEXFORMAT_ARGB32:
	case TEXFORMAT_RGB32:
	//	wii_debug("%s: TEXFORMAT_ARGB32 %d\n",__FUNCTION__, flag);
		newTex = malloc(sizeof(*newTex));
		newTex->format = GX_TF_RGBA8;
		bpp = 4;
		newTex->size = height * width * bpp;
		fixed = memalign(32, newTex->size);

		for (y = 0; y < height; y += 4)
		{
			for (x = 0; x < width; x += 4)
			{
				for (k = 0; k < 4; k++)
				{
					ty = y + k;
					src = &data[bpp * prim->texture.rowpixels * ty];
					for (l = 0; l < 4; l++)
					{
						tx = x + l;
						if (ty >= rawheight || tx >= rawwidth)
						{
							fixed[j] = 0x0000;
							fixed[j+16] = 0x0000;
						}
						else
						{
							GXGetRGBA8888_RGBA8( *(((u32*)src)+tx), &fixed[j], &fixed[j+16]);
						}
						j++;
					}
				}
				j += 16;
			}
		}
		break;
	case TEXFORMAT_PALETTE16:
	case TEXFORMAT_PALETTEA16:
	//	wii_debug("%s: PALETTE16\n",__FUNCTION__);
		newTex = malloc(sizeof(*newTex));
		newTex->format = GX_TF_RGB5A3;
		bpp = 2;
		newTex->size = height * width * bpp;
		fixed = memalign(32, newTex->size);

		for (y = 0; y < height; y += 4)
		{
			for (x = 0; x < width; x += 4)
			{
				for (k = 0; k < 4; k++)
				{
					ty = y + k;
					src = &data[bpp * prim->texture.rowpixels * ty];
					for (l = 0; l < 4; l++)
					{
						tx = x + l;
						if (ty >= rawheight || tx >= rawwidth)
						{
							fixed[j++] = 0x0000;
						}
						else
						{
							fixed[j++] = GXGetRGBA5551_PALETTE16( *(((u16 *)src)+tx), flag, prim->texture.palette);
						}
					}
				}
			}
		}
		break;
	case TEXFORMAT_RGB15:
	//	wii_debug("%s: RGB15\n",__FUNCTION__);
		newTex = malloc(sizeof(*newTex));
		newTex->format = GX_TF_RGB5A3;
		bpp = 2;
		newTex->size = height * width * bpp;
		fixed = memalign(32, newTex->size);

		for (y = 0; y < height; y += 4)
		{
			for (x = 0; x < width; x += 4)
			{
				for (k = 0; k < 4; k++)
				{
					ty = y + k;
					src = &data[bpp * prim->texture.rowpixels * ty];
					for (l = 0; l < 4; l++)
					{
						tx = x + l;
						if (ty >= rawheight || tx >= rawwidth)
						{
							fixed[j++] = 0x0000;
						}
						else
						{
							fixed[j++] = GXGetRGBA5551_RGB5A3( *(((u16*)src)+tx));
						}
					}
				}
			}
		}
		break;
	case TEXFORMAT_YUY16:
	//	wii_debug("%s: YUY16\n",__FUNCTION__);
		newTex = malloc(sizeof(*newTex));
		newTex->format = GX_TF_RGBA8;
		bpp = 4;
		newTex->size = height * width * bpp;
		fixed = memalign(32, newTex->size);

		for (y = 0; y < height; y += 4)
		{
			for (x = 0; x < width; x += 4)
			{
				for (k = 0; k < 4; k++)
				{
					ty = y + k;
					src = &data[bpp * prim->texture.rowpixels * ty];
					for (l = 0; l < 4; l++)
					{
						tx = x + l;
						if (ty >= rawheight || tx >= rawwidth)
						{
							fixed[j] = 0x0000;
							fixed[j+16] = 0x0000;
						}
						else
						{
							GXGetRGBA5551_YUY16( *(((u32*)src)+tx), &fixed[j], &fixed[j+16]);
						}
						j++;
					}
				}
				j += 16;
			}
		}
		break;
	default:
#ifdef WIIVIDEO_DEBUG
		wii_debug("%s: unknown flag %d\n", __FUNCTION__, flag);
#endif
		break;
	}

	if(newTex != NULL) {
		newTex->data = fixed;
		newTex->addr = &(*data);
		newTex->next = NULL;

		if (PRIMFLAG_GET_SCREENTEX(prim->flags))
		{
			if (firstScreenTex == NULL)
			{
				firstScreenTex = newTex;
			}
			else
			{
				lastScreenTex->next = newTex;
			}
			lastScreenTex = newTex;
		}
		else
		{
			if (firstTex == NULL)
			{
				firstTex = newTex;
			}
			else
			{
				lastTex->next = newTex;
			}
		}
		lastTex = newTex;
	}

	return newTex;
}

static inline gx_tex *get_texture(render_primitive *prim)
{
	gx_tex *t = firstTex;

	if (! PRIMFLAG_GET_SCREENTEX(prim->flags))
	{
		while (t != NULL) {
			if (t->addr == prim->texture.base)
			{
				return t;
			}
			t = t->next;
		}
	}

	return create_texture(prim);
}

static inline void prep_texture(render_primitive *prim)
{
	gx_tex *newTex = get_texture(prim);

	DCFlushRange(newTex->data, newTex->size);
	GX_InitTexObj(&texObj, newTex->data, prim->texture.width, prim->texture.height, newTex->format, GX_CLAMP, GX_CLAMP, GX_FALSE);

//	if (PRIMFLAG_GET_SCREENTEX(prim->flags))
		GX_InitTexObjLOD(&texObj, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, GX_DISABLE, GX_DISABLE, GX_ANISO_1);

	GX_LoadTexObj(&texObj, GX_TEXMAP0);
}

static void clearTexs(void)
{
	gx_tex *t = firstTex;

	while (t != NULL)
	{
		gx_tex *n;

		n = t->next;
		free(t->data);
		free(t);
		t = n;
	}

	firstTex = NULL;
	lastTex = NULL;
}

static void clearScreenTexs(void)
{
	gx_tex *t = firstScreenTex;

	while (t != NULL)
	{
		gx_tex *n;

		n = t->next;
		free(t->data);
		free(t);
		t = n;
	}

	firstScreenTex = NULL;
	lastScreenTex = NULL;
}

static void *wii_video_thread(void *arg)
{
	wii_debug("%s: Video thread started\n",__FUNCTION__);
	GX_SetCurrentGXThread();
	
	while (!wii_stopping) {
		if(*r_index != NULL) {
			render_primitive *prim;
			
			osd_lock_acquire((*r_index)->lock);

			for (prim = (*r_index)->head; prim != NULL; prim = prim->next) {
				u8 r, g, b, a;

				r = (u8)(255.0f * prim->color.r);
				g = (u8)(255.0f * prim->color.g);
				b = (u8)(255.0f * prim->color.b);
				a = (u8)(255.0f * prim->color.a);

				switch (PRIMFLAG_GET_BLENDMODE(prim->flags))
				{
				case BLENDMODE_NONE:
				//	wii_debug("%s: BLENDMODE_NONE\n",__FUNCTION__);
					GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
					break;
				case BLENDMODE_ALPHA:
				//	wii_debug("%s: BLENDMODE_ALPHA\n",__FUNCTION__);
					GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
					break;
				case BLENDMODE_RGB_MULTIPLY:
				//	wii_debug("%s: BLENDMODE_RGB_MULTIPLY\n",__FUNCTION__);
					GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCCLR, GX_BL_ZERO, GX_LO_AND);
					break;
				case BLENDMODE_ADD:
				//	wii_debug("%s: BLENDMODE_ADD\n",__FUNCTION__);
					GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);
					break;
				default:
					wii_debug("%s: BLENDMODE unknown %d\n",__FUNCTION__,PRIMFLAG_GET_BLENDMODE(prim->flags));
					break;
				}

				switch (prim->type)
				{
				case RENDER_PRIMITIVE_LINE:
					GX_LoadTexObj(&blankTex, GX_TEXMAP0);
					if((prim->bounds.x0 == prim->bounds.x1) &&
					   (prim->bounds.y0 == prim->bounds.y1)) {
						GX_SetPointSize((u8)(prim->width * 16.0f), GX_TO_ZERO);
						GX_Begin(GX_POINTS, GX_VTXFMT0, 1);
							GX_Position2f32(prim->bounds.x0+hofs, prim->bounds.y0+vofs);
							GX_Color4u8(r, g, b, a);
							GX_TexCoord2f32(0, 0);
						GX_End();
					}
					else {
						GX_SetLineWidth((u8)(prim->width * 16.0f), GX_TO_ZERO);
						GX_Begin(GX_LINES, GX_VTXFMT0, 2);
							GX_Position2f32(prim->bounds.x0+hofs, prim->bounds.y0+vofs);
							GX_Color4u8(r, g, b, a);
							GX_TexCoord2f32(0, 0);
							GX_Position2f32(prim->bounds.x1+hofs, prim->bounds.y1+vofs);
							GX_Color4u8(r, g, b, a);
							GX_TexCoord2f32(0, 0);
						GX_End();
					}

					break;
				case RENDER_PRIMITIVE_QUAD:
					if (prim->texture.base != NULL) {
						prep_texture(prim);
						GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
							GX_Position2f32(prim->bounds.x0+hofs, prim->bounds.y0+vofs);
							GX_Color4u8(r, g, b, a);
							GX_TexCoord2f32(prim->texcoords.tl.u, prim->texcoords.tl.v);
							GX_Position2f32(prim->bounds.x0+hofs, prim->bounds.y1+vofs);
							GX_Color4u8(r, g, b, a);
							GX_TexCoord2f32(prim->texcoords.bl.u, prim->texcoords.bl.v);
							GX_Position2f32(prim->bounds.x1+hofs, prim->bounds.y1+vofs);
							GX_Color4u8(r, g, b, a);
							GX_TexCoord2f32(prim->texcoords.br.u, prim->texcoords.br.v);
							GX_Position2f32(prim->bounds.x1+hofs, prim->bounds.y0+vofs);
							GX_Color4u8(r, g, b, a);
							GX_TexCoord2f32(prim->texcoords.tr.u, prim->texcoords.tr.v);
						GX_End();
					}
					else {
						GX_LoadTexObj(&blankTex, GX_TEXMAP0);
						GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
							GX_Position2f32(prim->bounds.x0+hofs, prim->bounds.y0+vofs);
							GX_Color4u8(r, g, b, a);
							GX_TexCoord2f32(0, 0);
							GX_Position2f32(prim->bounds.x0+hofs, prim->bounds.y1+vofs);
							GX_Color4u8(r, g, b, a);
							GX_TexCoord2f32(0, 0);
							GX_Position2f32(prim->bounds.x1+hofs, prim->bounds.y1+vofs);
							GX_Color4u8(r, g, b, a);
							GX_TexCoord2f32(0, 0);
							GX_Position2f32(prim->bounds.x1+hofs, prim->bounds.y0+vofs);
							GX_Color4u8(r, g, b, a);
							GX_TexCoord2f32(0, 0);
						GX_End();
					}
					break;
				default:
					wii_debug("%s: RENDER PRIMITIVE unknown %d\n",__FUNCTION__,prim->type);
					break;
				}
			}
			osd_lock_release((*r_index)->lock);
			*r_index = NULL;
			r_index++;
			if(r_index >= &currlist[WII_VIDEO_SLOT])
				r_index = &currlist[0];
			GX_DrawDone();
			clearScreenTexs();
			currfb ^= 1;
			GX_CopyDisp(xfb[currfb],GX_TRUE);
;	
			VIDEO_SetNextFramebuffer(xfb[currfb]);

			VIDEO_Flush();
			VIDEO_WaitVSync();
		}
		else {
			LWP_ThreadSleep(videoqueue);
		}
	}
	
	return (void *)0;
}

void wii_video_render(render_target *target, int flag)
{
	if(! flag) {
		int done=0;
		
		do {	
			if(*w_index == NULL) {
				const render_primitive_list *primlist = NULL;

				primlist = render_target_get_primitives(target);
				if(primlist != NULL) {	
					// make that the size of our target
					render_target_set_bounds(target, screen_width, screen_height, 0);
					*w_index = primlist;
					w_index++;
					if(w_index >= &currlist[WII_VIDEO_SLOT])
						w_index = &currlist[0];
					LWP_ThreadSignal(videoqueue);
				}
				done++;
			}
			
		//	LWP_YieldThread();
		} while(!done);
	}
}

void wii_setup_video(void)
{
	u32 xfbHeight;
	f32 yscale;
	Mtx44 perspective;
	Mtx GXmodelView2D;
	GXColor background = {0, 0, 0, 0xff};

	currfb = 0;

//	VIDEO_SetBlack(true);
	vmode = VIDEO_GetPreferredMode(NULL);
	_wii_screen_width = vmode->viWidth;
	
	VIDEO_Configure(vmode);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wbad-function-cast"
	xfb[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(vmode));
	xfb[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(vmode));
#pragma GCC diagnostic pop
	VIDEO_ClearFrameBuffer(vmode, xfb[0], COLOR_BLACK);
	VIDEO_ClearFrameBuffer(vmode, xfb[1], COLOR_BLACK);

	VIDEO_SetNextFramebuffer(xfb[currfb]);

	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (vmode->viTVMode & VI_NON_INTERLACE) 
		VIDEO_WaitVSync();
	else 
		while (VIDEO_GetNextField()) 
			VIDEO_WaitVSync();

	gp_fifo = memalign(32, DEFAULT_FIFO_SIZE);
	memset(gp_fifo, 0, DEFAULT_FIFO_SIZE);
	GX_Init(gp_fifo, DEFAULT_FIFO_SIZE);

	GX_SetCopyClear(background, GX_MAX_Z24);

	// other gx setup
	yscale = GX_GetYScaleFactor(vmode->efbHeight,vmode->xfbHeight);
	xfbHeight = GX_SetDispCopyYScale(yscale);
	GX_SetScissor(0,0,vmode->fbWidth,vmode->efbHeight);
	GX_SetDispCopySrc(0,0,vmode->fbWidth,vmode->efbHeight);
	GX_SetDispCopyDst(vmode->fbWidth,xfbHeight);
	GX_SetCopyFilter(vmode->aa,vmode->sample_pattern,GX_TRUE,vmode->vfilter);
	GX_SetFieldMode(vmode->field_rendering,((vmode->viHeight==2*vmode->xfbHeight)?GX_ENABLE:GX_DISABLE));

	if (vmode->aa)
		GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
	else
		GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);

	GX_SetCullMode(GX_CULL_NONE);
	GX_CopyDisp(xfb[currfb],GX_TRUE);
	GX_SetDispCopyGamma(GX_GM_1_0);

	GX_SetNumChans(1);
	GX_SetNumTexGens(1);
	GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);

	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GX_SetAlphaUpdate(GX_TRUE);
	GX_SetColorUpdate(GX_TRUE);

	guOrtho(perspective, 0.0F, vmode->efbHeight, 0.0F, wii_screen_width()-1, 0.0F, 300.0F);
	GX_LoadProjectionMtx(perspective, GX_ORTHOGRAPHIC);

	guMtxIdentity(GXmodelView2D);
	guMtxTransApply (GXmodelView2D, GXmodelView2D, 0.0F, 0.0F, -5.0F);
	GX_LoadPosMtxImm(GXmodelView2D,GX_PNMTX0);

	GX_SetViewport(20,20,vmode->fbWidth-20,vmode->efbHeight-40,0,1);
	GX_InvVtxCache();
	GX_ClearVtxDesc();
	GX_InvalidateTexAll();

	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

	VIDEO_SetBlack(false);

	GX_InitTexObj(&blankTex, blanktex, 1, 1, GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);
	w_index = &currlist[0];
	r_index = w_index;

	if (vidthread == LWP_THREAD_NULL) {
		LWP_InitQueue(&videoqueue);
		LWP_CreateThread(&vidthread, wii_video_thread, NULL, NULL, 0, VIDEOTH_PRIORITY);
	}
}

static void wii_video_cleanup(running_machine *machine)
{
	wii_debug("%s: start\n",__FUNCTION__);
	memset(currlist,0,sizeof(currlist));
	w_index = &currlist[0];
	r_index = w_index;
	clearTexs();
}

void wii_init_video(running_machine *machine)
{
	add_exit_callback(machine, wii_video_cleanup);
}

void wii_shutdown_video(void)
{
	wii_debug("%s: start\n",__FUNCTION__);
	
	if(vidthread != LWP_THREAD_NULL) {
		void *status;
		
		wii_stopping = true;
		LWP_ThreadSignal(videoqueue);
		LWP_JoinThread(vidthread, &status);
		LWP_CloseQueue(videoqueue);
		vidthread = LWP_THREAD_NULL;
		wii_debug("%s: video thread turned off\n",__FUNCTION__);
	}
	clearTexs();
	clearScreenTexs();

	GX_AbortFrame();
	GX_Flush();

	VIDEO_SetBlack(true);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	VIDEO_SetNextFramebuffer(xfb[currfb^1]);
	VIDEO_SetBlack(true);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	wii_debug("%s: end\n",__FUNCTION__);
}

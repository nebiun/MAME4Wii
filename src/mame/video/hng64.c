#include "driver.h"

#include <math.h>


// !!! I'm sure this isn't right !!!
UINT32 hng64_dls[2][0x81] ;

static int frameCount = 0 ;

UINT32* hng64_videoram;
tilemap *hng64_tilemap0;
tilemap *hng64_tilemap1;
tilemap *hng64_tilemap2;
tilemap *hng64_tilemap3;

UINT32 *hng64_spriteram;
UINT32 *hng64_videoregs;

UINT32 *hng64_tcram ;

/* HAAAACK to make the floor 'work' */
UINT32 hng64_hackTilemap3, hng64_hackTm3Count, hng64_rowScrollOffset ;


static void  matmul4( float *product, const float *a, const float *b ) ;
static void  vecmatmul4( float *product, const float *a, const float *b) ;
//static float vecDotProduct( const float *a, const float *b) ;
//static void normalize(float* x) ;

// 3d helpers
static float uToF(UINT16 input) ;
static void SetIdentity(float *matrix) ;


//static void plot(INT32 x, INT32 y, INT32 color, bitmap_t *bitmap) ;
//static void drawline2d(INT32 x0, INT32 y0, INT32 x1, INT32 y1, INT32 color, bitmap_t *bitmap) ;

static float *depthBuffer ;
static struct polygon *polys ;

//static int tilemap2Offset = 0x10000 ;


#define WORD_AT(BUFFER,OFFSET) ( (BUFFER[OFFSET] << 8) | BUFFER[OFFSET+1] )


/*
 * Sprite Format
 * ------------------
 *
 * UINT32 | Bytes    | Use
 * -------+-76543210-+----------------
 *   0    | xxxx---- | y position
 *   0    | ----xxxx | x position
 *   1    | xxxx---- | y zoom
 *   1    | ----xxxx | x zoom
 *   2    | ------x- | x chain
 *   2    | -------x | y chain
 *   2    | xxxx---- | end of sprite list - though it surely contains more info !
 *   2    | ----oo-- | not used ??
 *   3    | --xx---- | palette entry
 *   3    | --x----- | (bit 0x8) - maybe a graphics bank selector
 *   3    | oo--oooo | not used  ?
 *   4    | --xxxxxx | tile number
 *   4    | --x----- | (bit 0x2) - x flip
 *   4    | --x----- | (bit 0x1) - y flip
 *   4    | oo------ | not used ??
 *   5    | oooooooo | not used ??
 *   6    | oooooooo | not used ??
 *   7    | oooooooo | not used ??
 */

/* xxxx---- | I think this part of UINT32 2 is interesting as more than just a list end marker (AJG)
 */

static void draw_sprites(running_machine *machine, bitmap_t *bitmap, const rectangle *cliprect)
{
	const gfx_element *gfx;
	UINT32 *source = hng64_spriteram;
	UINT32 *finish = hng64_spriteram + 0xb000/4;

	/* find end of list? */
	while( source<finish)
	{
		int endlist;

		endlist=(source[2]&0xffff0000)>>16;
		if (endlist == 0x07ff) break;
		source+=8;
	}

//  for (int iii = 0; iii < 0x0f; iii++)
//      mame_printf_debug("%.8x ", hng64_videoregs[iii]) ;

//  mame_printf_debug("\n") ;

	finish = hng64_spriteram;

	/* draw backwards .. */
	while( source>finish )
	{
		int xpos, ypos, tileno,chainx,chainy,xflip;
		int xdrw,ydrw,pal,xinc,yinc,yflip;
		UINT32 zoomx,zoomy;
		//float foomX, foomY;
		source-=8;

		ypos = (source[0]&0xffff0000)>>16;
		xpos = (source[0]&0x0000ffff)>>0;
		tileno=(source[4]&0x00ffffff);
		chainx=(source[2]&0x000000f0)>>4;
		chainy=(source[2]&0x0000000f);

		zoomy = (source[1]&0xffff0000)>>16;
		zoomx = (source[1]&0x0000ffff)>>0;

		pal =(source[3]&0x00ff0000)>>16;
		xflip=(source[4]&0x02000000)>>25;
		yflip=(source[4]&0x01000000)>>24;

		if(xpos&0x8000) xpos -=0x10000;
		if(ypos&0x8000) ypos -=0x10000;


//      if (!(source[4] == 0x00000000 || source[4] == 0x000000aa))
//          mame_printf_debug("unknown : %.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x \n", source[0], source[1], source[2], source[3],
//                                                                         source[4], source[5], source[6], source[7]) ;

		/* Calculate the zoom */
		/* First, prevent any possible divide by zero errors */

#if 0
		if(!zoomx) zoomx=0x1000;
		if(!zoomy) zoomy=0x1000;

		foomX = (float)(0x1000) / (float)zoomx ;
		foomY = (float)(0x1000) / (float)zoomy ;

		zoomx = ((int)foomX) << 16 ;
		zoomy = ((int)foomY) << 16 ;

		zoomx += (int)((foomX - floor(foomX)) * (float)0x10000) ;
		zoomy += (int)((foomY - floor(foomY)) * (float)0x10000) ;
#endif

		zoomx = 0x10000;
		zoomy = 0x10000;

		if (source[3]&0x00800000) // maybe ..
		{
			gfx= machine->gfx[4];
		}
		else
		{
			gfx= machine->gfx[5];
			tileno>>=1;

			// Just a big hack to make the top and bottom tiles in the intro not crash (a pal value of 0x70 is bad)
			// Is there a problem with draw_sprites?  Doubtful...
			if (source[2] == 0x00080000)
				pal >>=4;
		}

		// Accomodate for chaining and flipping
		if(xflip)
		{
			//xinc=-(int)(16.0f*foomX);

			xinc=-16;
			xpos-=xinc*chainx;
		}
		else
		{
			//xinc=(int)(16.0f*foomX);
			xinc = 16;
		}

		if(yflip)
		{
			//yinc=-(int)(16.0f*foomY);
			yinc = -16;
			ypos-=yinc*chainy;
		}
		else
		{
			yinc = 16;
			//yinc=(int)(16.0f*foomY);
		}


//      if (((source[2] & 0xffff0000) >> 16) == 0x0001)
//      {
//          usrintf_showmessage("T %.8x %.8x %.8x %.8x %.8x", source[0], source[1], source[2], source[3], source[4]) ;
//          // usrintf_showmessage("T %.8x %.8x %.8x %.8x %.8x", source[0], source[1], source[2], source[3], source[4]) ;
//      }

		for(ydrw=0;ydrw<=chainy;ydrw++)
		{
			for(xdrw=0;xdrw<=chainx;xdrw++)
			{
				drawgfxzoom_transpen(bitmap,cliprect,gfx,tileno,pal,xflip,yflip,xpos+(xinc*xdrw),ypos+(yinc*ydrw),zoomx,zoomy/*0x10000*/,0);
				tileno++;
			}
		}
	}
}


/* Transition_Control Memory Region Map
 * ------------------------------
 *
 *  UINT32 | Bytes    | Use
 *  -------+-76543210-+----------
 *       0 |          |
 *       1 |          |
 *       2 |          |
 *       3 |          |
 *       4 |          |
 *       5 |          |
 *       6 | --xxxxxx | I popped into Buriki and saw some of these values changing to the same as 7.  hmmmm...
 *       7 | --xxxxxx | Almost certainly RGB darkening
 *       8 |          |
 *       9 |          |
 *      10 | --xxxxxx | Almost certainly RGB brightening
 *      11 | xxxxxxxx | Unknown - looks like an ARGB value - it seems to change when the scene changes
 *      12 |          |
 *      13 |          |
 *      14 |          |
 *      15 |          |
 *
 *  Various bits change depending on what is happening in the scene.
 *  These bits may set which 'layer' is affected by the blending.
 *  Or maybe they adjust the scale of the lightening and darkening...
 *  Or maybe it switches from fading by scaling to fading using absolute addition and subtraction...
 *  Or maybe they set transition type (there seems to be a cute scaling-squares transition in there somewhere)...
 */
static void transition_control(bitmap_t *bitmap, const rectangle *cliprect)
{
	int i, j ;

//  float colorScaleR, colorScaleG, colorScaleB ;
//  float finR, finG, finB ;
	INT32 finR, finG, finB ;

	INT32 darkR, darkG, darkB ;
	INT32 brigR, brigG, brigB ;

	// If either of the fading memory regions is non-zero...
	if (hng64_tcram[0x00000007] != 0x00000000 || hng64_tcram[0x0000000a] != 0x00000000)
	{
		darkR = (INT32)( hng64_tcram[0x00000007]        & 0xff) ;
		darkG = (INT32)((hng64_tcram[0x00000007] >> 8)  & 0xff) ;
		darkB = (INT32)((hng64_tcram[0x00000007] >> 16) & 0xff) ;

		brigR = (INT32)( hng64_tcram[0x0000000a]        & 0xff) ;
		brigG = (INT32)((hng64_tcram[0x0000000a] >> 8)  & 0xff) ;
		brigB = (INT32)((hng64_tcram[0x0000000a] >> 16) & 0xff) ;

		for (i = cliprect->min_x; i < cliprect->max_x; i++)
		{
			for (j = cliprect->min_y; j < cliprect->max_y; j++)
			{
				UINT32* thePixel = BITMAP_ADDR32(bitmap, j, i);

				finR = (INT32)RGB_RED(*thePixel) ;
				finG = (INT32)RGB_GREEN(*thePixel) ;
				finB = (INT32)RGB_BLUE(*thePixel) ;

				/*
                // Apply the darkening pass (0x07)...
                colorScaleR = 1.0f - (float)( hng64_tcram[0x00000007] & 0xff)        / 255.0f ;
                colorScaleG = 1.0f - (float)((hng64_tcram[0x00000007] >> 8)  & 0xff) / 255.0f ;
                colorScaleB = 1.0f - (float)((hng64_tcram[0x00000007] >> 16) & 0xff) / 255.0f ;

                finR = ((float)RGB_RED(*thePixel)   * colorScaleR) ;
                finG = ((float)RGB_GREEN(*thePixel) * colorScaleG) ;
                finB = ((float)RGB_BLUE(*thePixel)  * colorScaleB) ;


                // Apply the lightening pass (0x0a)...
                colorScaleR = 1.0f + (float)( hng64_tcram[0x0000000a] & 0xff)        / 255.0f ;
                colorScaleG = 1.0f + (float)((hng64_tcram[0x0000000a] >> 8)  & 0xff) / 255.0f ;
                colorScaleB = 1.0f + (float)((hng64_tcram[0x0000000a] >> 16) & 0xff) / 255.0f ;

                finR *= colorScaleR ;
                finG *= colorScaleG ;
                finB *= colorScaleB ;


                // Clamp
                if (finR > 255.0f) finR = 255.0f ;
                if (finG > 255.0f) finG = 255.0f ;
                if (finB > 255.0f) finB = 255.0f ;
                */


				// Subtractive fading
				if (hng64_tcram[0x00000007] != 0x00000000)
				{
					finR -= darkR ;
					finG -= darkG ;
					finB -= darkB ;
				}

				// Additive fading
				if (hng64_tcram[0x0000000a] != 0x00000000)
				{
					finR += brigR ;
					finG += brigG ;
					finB += brigB ;
				}

				// Clamp the high end
				if (finR > 255) finR = 255 ;
				if (finG > 255) finG = 255 ;
				if (finB > 255) finB = 255 ;

				// Clamp the low end
				if (finR < 0) finR = 0 ;
				if (finG < 0) finG = 0 ;
				if (finB < 0) finB = 0 ;

				*thePixel = MAKE_ARGB(255, (UINT8)finR, (UINT8)finG, (UINT8)finB) ;
			}
		}
	}
}


/*
 * 3d 'Sprite' Format
 * ------------------
 *
 * (documented below)
 *
 */

#define MAX_ONSCREEN_POLYS (10000)

struct polyVert
{
	float worldCoords[4] ;		// World space coordinates (X Y Z 1.0)

	float texCoords[4] ;		// Texture coordinates (U V 0 1.0) -> OpenGL style...

	float normal[4] ;			// Normal (X Y Z 1.0)
	float clipCoords[4] ;		// Homogeneous screen space coordinates (X Y Z W)

	float light[3] ;			// The intensity of the illumination at this point
} ;

struct polygon
{
	int n ;							// Number of sides
	struct polyVert vert[10] ;		// Vertices (maximum number per polygon is 10 -> 3+6)

	float faceNormal[4] ;			// Normal of the face overall - for calculating visibility and flat-shading...
	int visible ;					// Polygon visibility in scene

	INT8 texIndex ;					// Which texture to draw from (0x00-0x0f)
	INT8 texType ;					// How to index into the texture
	UINT8 palIndex ;				// Which palette to use when rasterizing
} ;


static void PerformFrustumClip(struct polygon *p) ;

//static void DrawWireframe(struct polygon *p, bitmap_t *bitmap) ;
static void DrawShaded(running_machine *machine, struct polygon *p, bitmap_t *bitmap) ;


static void draw3d(running_machine *machine, bitmap_t *bitmap, const rectangle *cliprect )
{
	int i,j,k,l,m ;

	float projectionMatrix[16] ;
	float modelViewMatrix[16] ;
	float cameraMatrix[16] ;
	float objectMatrix[16] ;

	int paletteState = 0x00 ;

	UINT32 numPolys = 0 ;

	struct polygon lastPoly = { 0 } ;
	const rectangle *visarea = video_screen_get_visible_area(machine->primary_screen);

	// Set some matrices to the identity...
	SetIdentity(projectionMatrix) ;
	SetIdentity(modelViewMatrix) ;
	SetIdentity(cameraMatrix) ;
	SetIdentity(objectMatrix) ;

	// Display list 2 comes after display list 1.  Go figure.
	for (j = 1; j >= 0; j--)
	{
		UINT32 *workingList = hng64_dls[j] ;

		for (i = 0; i < 0x80; i += 0x08)
		{
			float left, right, top, bottom, near_, far_ ;
			UINT8 *threeDRoms ;
			UINT8 *threeDPointer ;
			UINT32 threeDOffset ;
			UINT32 size[4] ;
			UINT32 address[4] ;
			UINT32 megaOffset ;
			float eyeCoords[4] ;			// objectCoords transformed by the modelViewMatrix
			// float clipCoords[4] ;        // eyeCoords transformed by the projectionMatrix
			float ndCoords[4] ;				// normalized device coordinates/clipCoordinates (x/w, y/w, z/w)
			float windowCoords[4] ;			// mapped ndCoordinates to screen space
			float cullRay[4] ;

			// Debug...
//          mame_printf_debug("Element %.2d (%d) : %.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x\n", i/0x08, j,
//                                     (UINT32)hng64_dls[j][i+0], (UINT32)hng64_dls[j][i+1], (UINT32)hng64_dls[j][i+2], (UINT32)hng64_dls[j][i+3],
//                                     (UINT32)hng64_dls[j][i+4], (UINT32)hng64_dls[j][i+5], (UINT32)hng64_dls[j][i+6], (UINT32)hng64_dls[j][i+7]) ;

			// Depending on what the initial flags are, do sumthin'...
			switch((workingList[i+0] & 0xffff0000) >> 16)
			{
			case 0x0012:
				// UNKNOWN - seems an awful lot parameters for a projection matrix though...

				;

				// It changes when 'How to play' is on the screen...  not too much, but if this is right, the aspect
				// ratio is different...

				// Heisted from GLFrustum - 6 parameters...
				left   = uToF( workingList[i+5] & 0x0000ffff) ;
				right  = uToF((workingList[i+5] & 0xffff0000) >> 16) ;
				top    = uToF((workingList[i+6] & 0xffff0000) >> 16) ;
				bottom = uToF( workingList[i+6] & 0x0000ffff) ;
				near_   = uToF((workingList[i+3] & 0xffff0000) >> 16) ;
				far_    = uToF( workingList[i+3] & 0x0000ffff) ;

				// It's almost .always. these values in fatfurwa...
				// 0.070313    0.000000 [0]     (scaled by 128)
				// 0.000000   10.000000 [1]
				// 0.000000    0.500000 [2]
				// 2.000000   11.062500 [3]
				// 10.000000  11.000000 [4]
				// 1.000000   -1.000000 [5]
				// 0.875000   -0.875000 [6]
				// 0.000000    0.000000 [7]

				projectionMatrix[0]  = (2.0f*near_)/(right-left) ;
				projectionMatrix[1]  = 0.0f ;
				projectionMatrix[2]  = 0.0f ;
				projectionMatrix[3]  = 0.0f ;

				projectionMatrix[4]  = 0.0f ;
				projectionMatrix[5]  = (2.0f*near_)/(top-bottom) ;
				projectionMatrix[6]  = 0.0f ;
				projectionMatrix[7]  = 0.0f ;

				projectionMatrix[8]  = (right+left)/(right-left) ;
				projectionMatrix[9]  = (top+bottom)/(top-bottom) ;
				projectionMatrix[10] = -((far_+near_)/(far_-near_)) ;
				projectionMatrix[11] = -1.0f ;

				projectionMatrix[12] = 0.0f ;
				projectionMatrix[13] = 0.0f ;
				projectionMatrix[14] = -((2.0f*far_*near_)/(far_-near_)) ;
				projectionMatrix[15] = 0.0f ;

				/*
                int xxx ;
                for (xxx = 0; xxx < 16; xxx++)
                    mame_printf_debug("%f ", projectionMatrix[xxx]) ;
                mame_printf_debug("\n") ;

                mame_printf_debug("Vars   : %f %f %f %f %f %f\n", left, right, top, bottom, near, far) ;
                mame_printf_debug("Camera : %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f\n",
                                 uToF((workingList[i+0] & 0xffff0000) >> 16)*128, uToF( workingList[i+0] & 0x0000ffff)*128,
                                 uToF((workingList[i+1] & 0xffff0000) >> 16)*128, uToF( workingList[i+1] & 0x0000ffff)*128,

                                 uToF((workingList[i+2] & 0xffff0000) >> 16)*128, uToF( workingList[i+2] & 0x0000ffff)*128,
                                 uToF((workingList[i+3] & 0xffff0000) >> 16)*128, uToF( workingList[i+3] & 0x0000ffff)*128,

                                 uToF((workingList[i+4] & 0xffff0000) >> 16)*128, uToF( workingList[i+4] & 0x0000ffff)*128,
                                 uToF((workingList[i+5] & 0xffff0000) >> 16)*128, uToF( workingList[i+5] & 0x0000ffff)*128,

                                 uToF((workingList[i+6] & 0xffff0000) >> 16)*128, uToF( workingList[i+6] & 0x0000ffff)*128,
                                 uToF((workingList[i+7] & 0xffff0000) >> 16)*128, uToF( workingList[i+7] & 0x0000ffff)*128) ;
                */

				break ;

			case 0x0001:
				// CAMERA TRANSFORMATION MATRIX
				cameraMatrix[0]  = uToF( workingList[i+0] & 0x0000ffff) ;
				cameraMatrix[4]  = uToF((workingList[i+1] & 0xffff0000) >> 16) ;
				cameraMatrix[8]  = uToF( workingList[i+1] & 0x0000ffff) ;
				cameraMatrix[3]  = 0.0f ;

				cameraMatrix[1]  = uToF((workingList[i+2] & 0xffff0000) >> 16) ;
				cameraMatrix[5]  = uToF( workingList[i+2] & 0x0000ffff) ;
				cameraMatrix[9]  = uToF((workingList[i+3] & 0xffff0000) >> 16) ;
				cameraMatrix[7]  = 0.0f ;

				cameraMatrix[2]  = uToF( workingList[i+3] & 0x0000ffff) ;
				cameraMatrix[6]  = uToF((workingList[i+4] & 0xffff0000) >> 16) ;
				cameraMatrix[10] = uToF( workingList[i+4] & 0x0000ffff) ;
				cameraMatrix[11] = 0.0f ;

				cameraMatrix[12] = uToF((workingList[i+5] & 0xffff0000) >> 16) ;
				cameraMatrix[13] = uToF( workingList[i+5] & 0x0000ffff) ;
				cameraMatrix[14] = uToF((workingList[i+6] & 0xffff0000) >> 16) ;
				cameraMatrix[15] = 1.0f ;
				break ;

			case 0x0010:
				// UNKNOWN - light maybe
				break ;

			case 0x0011:
				// Model Flags?
				// 00110000 00000000 00000100 01000100 0400-0000 00007fff 00000000 00000020
				// ----                                pal  ---- -------- -------- -------- not used (known)
				paletteState = (workingList[i+4] & 0xff000000) >> 24 ;

				break ;

			case 0x0100:
				// GEOMETRY
				;

				threeDRoms = memory_region(machine, "verts") ;

				/////////////////////////
				// GET THE HEADER INFO //
				/////////////////////////

				// 3d ROM Offset
				// !!! This might be more than just 20 bits...
				threeDOffset = workingList[i+1] & 0x000fffff ;
				threeDOffset = (threeDOffset<<1) * 3 ;

				threeDPointer = &threeDRoms[threeDOffset] ;


				// 66 byte versus 48 byte chunk flag
				//    WRONG !  I think it's something to do with lighting...
				//    it's 0 for the 66-byte lit globe in the character select and 0 for something in terry's hand...
				// if (workingList[i+0] & 0x00000010)


				//////////////////////////////////////////
				// GET THE OBJECT TRANSFORMATION MATRIX //
				//////////////////////////////////////////

				objectMatrix[8 ] = uToF( workingList[i+3] & 0x0000ffff) ;
				objectMatrix[4 ] = uToF((workingList[i+4] & 0xffff0000) >> 16) ;
				objectMatrix[0 ] = uToF( workingList[i+4] & 0x0000ffff) ;
				objectMatrix[3]  = 0.0f ;

				objectMatrix[9 ] = uToF((workingList[i+5] & 0xffff0000) >> 16) ;
				objectMatrix[5 ] = uToF( workingList[i+5] & 0x0000ffff) ;
				objectMatrix[1 ] = uToF((workingList[i+6] & 0xffff0000) >> 16) ;
				objectMatrix[7]  = 0.0f ;

				objectMatrix[10] = uToF( workingList[i+6] & 0x0000ffff) ;
				objectMatrix[6 ] = uToF((workingList[i+7] & 0xffff0000) >> 16) ;
				objectMatrix[2 ] = uToF( workingList[i+7] & 0x0000ffff) ;
				objectMatrix[11] = 0.0f ;

				objectMatrix[12] = uToF((workingList[i+2] & 0xffff0000) >> 16) ;
				objectMatrix[13] = uToF( workingList[i+2] & 0x0000ffff) ;
				objectMatrix[14] = uToF((workingList[i+3] & 0xffff0000) >> 16) ;
				objectMatrix[15] = 1.0f ;



				//////////////////////////////////////////////////////////
				// EXTRACT DATA FROM THE ADDRESS POINTED TO IN THE FILE //
				//////////////////////////////////////////////////////////

				// Okay, there are 4 hunks per address.  They all seem good too...

				// First tell me how many 'chunks' are at each address...
				size[0] = WORD_AT( threeDPointer, ( 6<<1) ) ;
				size[1] = WORD_AT( threeDPointer, ( 7<<1) ) ;
				size[2] = WORD_AT( threeDPointer, ( 9<<1) ) ;
				size[3] = WORD_AT( threeDPointer, (10<<1) ) ;

				megaOffset = ( WORD_AT(threeDRoms, (threeDOffset + 4)) ) << 16 ;
				address[0] = megaOffset | WORD_AT(threeDPointer, (0<<1)) ;
				address[1] = megaOffset | WORD_AT(threeDPointer, (1<<1)) ;
				address[2] = megaOffset | WORD_AT(threeDPointer, (3<<1)) ;
				address[3] = megaOffset | WORD_AT(threeDPointer, (4<<1)) ;

				// DEBUG
				// mame_printf_debug("%.5x %.3x %.5x %.3x %.5x %.3x %.5x %.3x\n", address[0], size[0], address[1], size[1], address[2], size[2], address[3], size[3]) ;
				// !! END DEBUG !!



				////////////////////////////////////
				// A FEW 'GLOBAL' TRANSFORMATIONS //
				////////////////////////////////////

				// Now perform the world transformations...
				// !! Can eliminate this step with a matrix stack (maybe necessary?) !!
				SetIdentity(modelViewMatrix) ;
				matmul4(modelViewMatrix, modelViewMatrix, cameraMatrix) ;
				matmul4(modelViewMatrix, modelViewMatrix, objectMatrix) ;

				for (k = 0; k < 4; k++)										// For all 4 chunks
				{
					threeDPointer = &threeDRoms[(address[k]<<1) * 3] ;

					for (l = 0; l < size[k]; l++)
					{
						////////////////////////////////////////////
						// GATHER A SINGLE TRIANGLE'S INFORMATION //
						////////////////////////////////////////////

						UINT8 triangleType = threeDPointer[1] ;

						UINT8 numVertices = 3 ;
						UINT8 chunkLength = 0 ;

						// Some chunks only have 1 vertex (they act as a vertex fan)
						if (triangleType == 0x97 ||
							triangleType == 0x87 ||
							triangleType == 0xd7 ||
							triangleType == 0x96)
							numVertices = 1 ;

						// Get which texture this polygon refers to...
						// In fatfur it's 0xc for the smooth-shaded earth       - maybe this is for all things with alpha - check the hair at some point...
						//                0x9 for the untextured buildings
						//                0xd for the 'explosion' of the HNG64
						//            and 0x8 everywhere else...
						//        they're 0x8 in the buriki intro too (those are 66-byte chunks!)
						polys[numPolys].texType = ((threeDPointer[2] & 0xf0) >> 4);

						if (polys[numPolys].texType == 0x8 || polys[numPolys].texType == 0xc)		//  || polys[numPolys].texType == 0x9
							polys[numPolys].texIndex = threeDPointer[3] & 0x0f ;
						else
							polys[numPolys].texIndex = -1 ;

						// Set the polygon's palette
						polys[numPolys].palIndex = paletteState ;

						for (m = 0; m < numVertices; m++)								// For all vertices of the chunk
						{
							switch(triangleType)
							{
							// 42 byte chunk
							case 0x04:
							case 0x0e:
								polys[numPolys].vert[m].worldCoords[0] = uToF(WORD_AT(threeDPointer, ((3<<1) + (6<<1)*m)) ) ;
								polys[numPolys].vert[m].worldCoords[1] = uToF(WORD_AT(threeDPointer, ((4<<1) + (6<<1)*m)) ) ;
								polys[numPolys].vert[m].worldCoords[2] = uToF(WORD_AT(threeDPointer, ((5<<1) + (6<<1)*m)) ) ;
								polys[numPolys].vert[m].worldCoords[3] = 1.0f ;
								polys[numPolys].n = 3 ;

								// !! What is the first coordinate here (6) - maybe denotes size of chunk? !!
								polys[numPolys].vert[m].texCoords[0] = uToF(WORD_AT(threeDPointer, ((7<<1) + (6<<1)*m)) ) ;
								polys[numPolys].vert[m].texCoords[1] = uToF(WORD_AT(threeDPointer, ((8<<1) + (6<<1)*m)) ) ;
								polys[numPolys].vert[m].texCoords[2] = 0.0f ;
								polys[numPolys].vert[m].texCoords[3] = 1.0f ;

								polys[numPolys].vert[m].normal[0] = uToF(WORD_AT(threeDPointer, (21<<1) )) ;
								polys[numPolys].vert[m].normal[1] = uToF(WORD_AT(threeDPointer, (22<<1) )) ;
								polys[numPolys].vert[m].normal[2] = uToF(WORD_AT(threeDPointer, (23<<1) )) ;
								polys[numPolys].vert[m].normal[3] = 0.0f ;

								// !!! DUMB !!!
								polys[numPolys].vert[m].light[0] = polys[numPolys].vert[m].texCoords[0] * 255.0f ;
								polys[numPolys].vert[m].light[1] = polys[numPolys].vert[m].texCoords[1] * 255.0f ;
								polys[numPolys].vert[m].light[2] = polys[numPolys].vert[m].texCoords[2] * 255.0f ;

								// Redundantly called, but it works...
								polys[numPolys].faceNormal[0] = polys[numPolys].vert[m].normal[0] ;
								polys[numPolys].faceNormal[1] = polys[numPolys].vert[m].normal[1] ;
								polys[numPolys].faceNormal[2] = polys[numPolys].vert[m].normal[2] ;
								polys[numPolys].faceNormal[3] = 0.0f ;

								chunkLength = (24<<1) ;
								break ;

							// 66 byte chunk
							case 0x05:
							case 0x0f:
								polys[numPolys].vert[m].worldCoords[0] = uToF(WORD_AT(threeDPointer, ((3<<1) + (9<<1)*m)) ) ;
								polys[numPolys].vert[m].worldCoords[1] = uToF(WORD_AT(threeDPointer, ((4<<1) + (9<<1)*m)) ) ;
								polys[numPolys].vert[m].worldCoords[2] = uToF(WORD_AT(threeDPointer, ((5<<1) + (9<<1)*m)) ) ;
								polys[numPolys].vert[m].worldCoords[3] = 1.0f ;
								polys[numPolys].n = 3 ;

								// !! See above - (6) - why? !!
								polys[numPolys].vert[m].texCoords[0] = uToF(WORD_AT(threeDPointer, ((7<<1) + (9<<1)*m)) ) ;
								polys[numPolys].vert[m].texCoords[1] = uToF(WORD_AT(threeDPointer, ((8<<1) + (9<<1)*m)) ) ;
								polys[numPolys].vert[m].texCoords[2] = 0.0f ;
								polys[numPolys].vert[m].texCoords[3] = 1.0f ;

								polys[numPolys].vert[m].normal[0] = uToF(WORD_AT(threeDPointer, ((9<<1) + (9<<1)*m)) ) ;
								polys[numPolys].vert[m].normal[1] = uToF(WORD_AT(threeDPointer, ((10<<1) + (9<<1)*m)) ) ;
								polys[numPolys].vert[m].normal[2] = uToF(WORD_AT(threeDPointer, ((11<<1) + (9<<1)*m)) ) ;
								polys[numPolys].vert[m].normal[3] = 0.0f ;

								// !!! DUMB !!!
								polys[numPolys].vert[m].light[0] = polys[numPolys].vert[m].texCoords[0] * 255.0f ;
								polys[numPolys].vert[m].light[1] = polys[numPolys].vert[m].texCoords[1] * 255.0f ;
								polys[numPolys].vert[m].light[2] = polys[numPolys].vert[m].texCoords[2] * 255.0f ;

								// Redundantly called, but it works...
								polys[numPolys].faceNormal[0] = uToF(WORD_AT(threeDPointer, (30<<1) )) ;
								polys[numPolys].faceNormal[1] = uToF(WORD_AT(threeDPointer, (31<<1) )) ;
								polys[numPolys].faceNormal[2] = uToF(WORD_AT(threeDPointer, (32<<1) )) ;
								polys[numPolys].faceNormal[3] = 0.0f ;

								chunkLength = (33<<1) ;
								break ;

							// 30 byte chunk
							case 0x97:
							case 0x87:
							case 0xd7:

								// Copy over the proper vertices from the previous triangle...
								memcpy(&polys[numPolys].vert[1], &lastPoly.vert[0], sizeof(struct polyVert)) ;
								memcpy(&polys[numPolys].vert[2], &lastPoly.vert[2], sizeof(struct polyVert)) ;

								// Fill in the appropriate data...
								polys[numPolys].vert[0].worldCoords[0] = uToF(WORD_AT(threeDPointer, (3<<1) )) ;
								polys[numPolys].vert[0].worldCoords[1] = uToF(WORD_AT(threeDPointer, (4<<1) )) ;
								polys[numPolys].vert[0].worldCoords[2] = uToF(WORD_AT(threeDPointer, (5<<1) )) ;
								polys[numPolys].vert[0].worldCoords[3] = 1.0f ;
								polys[numPolys].n = 3 ;

								// !! See above - (6) - why? !!
								polys[numPolys].vert[0].texCoords[0] = uToF(WORD_AT(threeDPointer, (7<<1) )) ;
								polys[numPolys].vert[0].texCoords[1] = uToF(WORD_AT(threeDPointer, (8<<1) )) ;
								polys[numPolys].vert[0].texCoords[2] = 0.0f ;
								polys[numPolys].vert[0].texCoords[3] = 1.0f ;

								polys[numPolys].vert[0].normal[0] = uToF(WORD_AT(threeDPointer, (9<<1) )) ;
								polys[numPolys].vert[0].normal[1] = uToF(WORD_AT(threeDPointer, (10<<1) )) ;
								polys[numPolys].vert[0].normal[2] = uToF(WORD_AT(threeDPointer, (11<<1) )) ;
								polys[numPolys].vert[0].normal[3] = 0.0f ;

								polys[numPolys].vert[0].light[0] = polys[numPolys].vert[0].texCoords[0] * 255.0f ;
								polys[numPolys].vert[0].light[1] = polys[numPolys].vert[0].texCoords[1] * 255.0f ;
								polys[numPolys].vert[0].light[2] = polys[numPolys].vert[0].texCoords[2] * 255.0f ;

								polys[numPolys].faceNormal[0] = uToF(WORD_AT(threeDPointer, (12<<1) )) ;
								polys[numPolys].faceNormal[1] = uToF(WORD_AT(threeDPointer, (13<<1) )) ;
								polys[numPolys].faceNormal[2] = uToF(WORD_AT(threeDPointer, (14<<1) )) ;
								polys[numPolys].faceNormal[3] = 0.0f ;

								chunkLength = (15<<1) ;
								break ;

							// 18 byte chunk
							case 0x96:

								// Copy over the proper vertices from the previous triangle...
								memcpy(&polys[numPolys].vert[1], &lastPoly.vert[0], sizeof(struct polyVert)) ;
								memcpy(&polys[numPolys].vert[2], &lastPoly.vert[2], sizeof(struct polyVert)) ;

								// !!! Too lazy to have finished this yet !!!

								polys[numPolys].vert[0].worldCoords[0] = uToF(WORD_AT(threeDPointer, (3<<1))) ;
								polys[numPolys].vert[0].worldCoords[1] = uToF(WORD_AT(threeDPointer, (4<<1))) ;
								polys[numPolys].vert[0].worldCoords[2] = uToF(WORD_AT(threeDPointer, (5<<1))) ;
								polys[numPolys].vert[0].worldCoords[3] = 1.0f ;
								polys[numPolys].n = 3 ;

								// !! See above - (6) - why? !!
								polys[numPolys].vert[0].texCoords[0] = uToF(WORD_AT(threeDPointer, (7<<1))) ;
								polys[numPolys].vert[0].texCoords[1] = uToF(WORD_AT(threeDPointer, (8<<1))) ;
								polys[numPolys].vert[0].texCoords[2] = 0.0f ;
								polys[numPolys].vert[0].texCoords[3] = 1.0f ;

								// !!! DUMB !!!
								polys[numPolys].vert[0].light[0] = polys[numPolys].vert[0].texCoords[0] * 255.0f ;
								polys[numPolys].vert[0].light[1] = polys[numPolys].vert[0].texCoords[1] * 255.0f ;
								polys[numPolys].vert[0].light[2] = polys[numPolys].vert[0].texCoords[2] * 255.0f ;

								// This normal could be right, but I'm not entirely sure - there is no normal in the 18 bytes!
								polys[numPolys].vert[0].normal[0] = lastPoly.faceNormal[0] ;
								polys[numPolys].vert[0].normal[1] = lastPoly.faceNormal[1] ;
								polys[numPolys].vert[0].normal[2] = lastPoly.faceNormal[2] ;
								polys[numPolys].vert[0].normal[3] = lastPoly.faceNormal[3] ;

								polys[numPolys].faceNormal[0] = lastPoly.faceNormal[0] ;
								polys[numPolys].faceNormal[1] = lastPoly.faceNormal[1] ;
								polys[numPolys].faceNormal[2] = lastPoly.faceNormal[2] ;
								polys[numPolys].faceNormal[3] = lastPoly.faceNormal[3] ;

								chunkLength = (9<<1) ;
								break ;

							default:
								mame_printf_debug("UNKNOWN geometry CHUNK TYPE : %x\n", triangleType) ;
								break ;
							}
						}

						polys[numPolys].visible = 1 ;

						memcpy(&lastPoly, &polys[numPolys], sizeof(struct polygon)) ;




						// THE HNG64 HARDWARE DOES NOT SEEM TO BACKFACE CULL //

						///////////////////
						// BACKFACE CULL //
						///////////////////
/*
                        float cullRay[4] ;
                        float cullNorm[4] ;

                        // Cast a ray out of the camera towards the polygon's point in eyespace.
                        vecmatmul4(cullRay, modelViewMatrix, polys[numPolys].vert[0].worldCoords) ;
                        normalize(cullRay) ;
                        // Dot product that with the normal to see if you're negative...
                        vecmatmul4(cullNorm, modelViewMatrix, polys[numPolys].faceNormal) ;

                        float result = vecDotProduct(cullRay, cullNorm) ;

                        if (result < 0.0f)
                            polys[numPolys].visible = 1 ;
                        else
                            polys[numPolys].visible = 0 ;
*/

						////////////////////////////
						// BEHIND-THE-CAMERA CULL //
						////////////////////////////
						vecmatmul4(cullRay, modelViewMatrix, polys[numPolys].vert[0].worldCoords) ;

						if (cullRay[2] > 0.0f)				// Camera is pointing down -Z
						{
							polys[numPolys].visible = 0 ;
						}


						//////////////////////////////////////////////////////////
						// TRANSFORM THE TRIANGLE INTO HOMOGENEOUS SCREEN SPACE //
						//////////////////////////////////////////////////////////

						if (polys[numPolys].visible)
						{
							for (m = 0; m < polys[numPolys].n; m++)
							{
								// Transform and project the vertex into pre-divided homogeneous coordinates...
								vecmatmul4(eyeCoords, modelViewMatrix, polys[numPolys].vert[m].worldCoords) ;
								vecmatmul4(polys[numPolys].vert[m].clipCoords, projectionMatrix, eyeCoords) ;
							}

							if (polys[numPolys].visible)
							{
								// Clip the triangles to the view frustum...
								PerformFrustumClip(&polys[numPolys]) ;

								for (m = 0; m < polys[numPolys].n; m++)
								{
									// Convert into normalized device coordinates...
									ndCoords[0] = polys[numPolys].vert[m].clipCoords[0] / polys[numPolys].vert[m].clipCoords[3] ;
									ndCoords[1] = polys[numPolys].vert[m].clipCoords[1] / polys[numPolys].vert[m].clipCoords[3] ;
									ndCoords[2] = polys[numPolys].vert[m].clipCoords[2] / polys[numPolys].vert[m].clipCoords[3] ;
									ndCoords[3] = polys[numPolys].vert[m].clipCoords[3] ;

									// Final pixel values are garnered here :
									windowCoords[0] = (ndCoords[0]+1.0f) * ((float)(visarea->max_x) / 2.0f) + 0.0f ;
									windowCoords[1] = (ndCoords[1]+1.0f) * ((float)(visarea->max_y) / 2.0f) + 0.0f ;
									windowCoords[2] = (ndCoords[2]+1.0f) * 0.5f ;

									windowCoords[1] = (float)visarea->max_y - windowCoords[1] ;		// Flip Y

									// Store the points in a list for later use...
									polys[numPolys].vert[m].clipCoords[0] = windowCoords[0] ;
									polys[numPolys].vert[m].clipCoords[1] = windowCoords[1] ;
									polys[numPolys].vert[m].clipCoords[2] = windowCoords[2] ;
									polys[numPolys].vert[m].clipCoords[3] = ndCoords[3] ;
								}
							}
						}


/*
                        // DEBUG
                        if (chunkLength == (9 << 1))
                        {
                            mame_printf_debug("Chunk : ") ;
                            for (int ajg = 0; ajg < chunkLength; ajg+=2)
                                mame_printf_debug("%.2x%.2x ", threeDPointer[ajg], threeDPointer[ajg+1]) ;
                            mame_printf_debug("\n") ;
                        }
                        // END DEBUG
*/

						// Advance to the next polygon chunk...
						threeDPointer += chunkLength ;

						numPolys++ ;				// Add one more to the display list...
					}
				}

				break ;

			default:
				break ;
			}
		}

		// Don't forget about this !!!
		//mame_printf_debug("                 %.8x\n\n", (UINT32)hng64_dls[j][0x80]) ;

	}


	/////////////////////////////////////////////////
	// FINALLY RENDER THE TRIANGLES INTO THE FRAME //
	/////////////////////////////////////////////////

	// Reset the depth buffer...
	for (i = 0; i < (visarea->max_x)*(visarea->max_y); i++)
		depthBuffer[i] = 100.0f ;

	for (i = 0; i < numPolys; i++)
	{
		if (polys[i].visible)
		{
			//DrawWireframe(&polys[i], bitmap) ;
			DrawShaded(machine, &polys[i], bitmap) ;
		}
	}

	// usrintf_showmessage("%d", numPolys) ;

	// Clear each of the display list buffers after drawing...
	for (i = 0; i < 0x81; i++)
	{
		hng64_dls[0][i] = 0 ;
		hng64_dls[1][i] = 0 ;
	}
}


/* 8x8x4bpp layer */
static TILE_GET_INFO( get_hng64_tile0_info )
{
	UINT16 tilemapinfo = (hng64_videoregs[0x02]&0xffff0000)>>16;
	int tileno,pal, flip;

	tileno = hng64_videoram[tile_index+(0x00000/4)];
	// pppppppp ff--atttt tttttttt tttttttt

	pal = (tileno&0xff000000)>>24;
	flip =(tileno&0x00c00000)>>22;

	if (tileno&0x200000)
	{
		tileno = (tileno & hng64_videoregs[0x0b]) | hng64_videoregs[0x0c];
	}

	tileno &= 0x1fffff;

	if (tilemapinfo&0x400)
	{
		SET_TILE_INFO(1,tileno>>1,pal>>4,TILE_FLIPYX(flip));
	}
	else
	{
		SET_TILE_INFO(0,tileno, pal,TILE_FLIPYX(flip));
	}
}

/* 16x16 tiles, 8bpp layer */
static TILE_GET_INFO( get_hng64_tile1_info )
{
	UINT16 tilemapinfo = (hng64_videoregs[0x02]&0x0000ffff)>>0;
	int tileno,pal, flip;

	tileno = hng64_videoram[tile_index+(0x10000/4)];
	// pppppppp ff--atttt tttttttt tttttttt

	pal = (tileno&0xff000000)>>24;
	flip =(tileno&0x00c00000)>>22;

	if (tileno&0x200000)
	{
		tileno = (tileno & hng64_videoregs[0x0b]) | hng64_videoregs[0x0c];
	}

	tileno &= 0x1fffff;

	if (tilemapinfo&0x400)
	{
		SET_TILE_INFO(3,tileno>>3,pal>>4,TILE_FLIPYX(flip));
	}
	else
	{
		SET_TILE_INFO(2,tileno>>2, pal,TILE_FLIPYX(flip));
	}
}

/* 16x16 tiles, 8bpp layer */
static TILE_GET_INFO( get_hng64_tile2_info )
{
	UINT16 tilemapinfo = (hng64_videoregs[0x03]&0xffff0000)>>16;
	int tileno,pal, flip;

	tileno = hng64_videoram[tile_index+(0x20000/4)];
	// pppppppp ff--atttt tttttttt tttttttt

	pal = (tileno&0xff000000)>>24;
	flip =(tileno&0x00c00000)>>22;

	if (tileno&0x200000)
	{
		tileno = (tileno & hng64_videoregs[0x0b]) | hng64_videoregs[0x0c];
	}

	tileno &= 0x1fffff;

	if (tilemapinfo&0x400)
	{
		SET_TILE_INFO(3,tileno>>3,pal>>4,TILE_FLIPYX(flip));
	}
	else
	{
		SET_TILE_INFO(2,tileno>>2, pal,TILE_FLIPYX(flip));
	}
}

/* 16x16 tiles, 8bpp layer */
static TILE_GET_INFO( get_hng64_tile3_info )
{
	UINT16 tilemapinfo = (hng64_videoregs[0x03]&0x0000ffff)>>0;
	int tileno,pal, flip;

	tileno = hng64_videoram[tile_index+(0x30000/4)];
	// pppppppp ff--atttt tttttttt tttttttt

	pal = (tileno&0xff000000)>>24;
	flip =(tileno&0x00c00000)>>22;

	if (tileno&0x200000)
	{
		tileno = (tileno & hng64_videoregs[0x0b]) | hng64_videoregs[0x0c];
	}

	tileno &= 0x1fffff;

	if (tilemapinfo&0x400)
	{
		SET_TILE_INFO(3,tileno>>3,pal>>4,TILE_FLIPYX(flip));
	}
	else
	{
		SET_TILE_INFO(2,tileno>>2, pal,TILE_FLIPYX(flip));
	}
}



static void hng64_drawtilemap( bitmap_t *bitmap, const rectangle *cliprect, int scrollbase, tilemap* tilemap )
{
	int xscroll,yscroll,xzoom,yzoom;

	xscroll = (INT16)(hng64_videoram[(0x40000+(scrollbase<<4))/4]>>16);
	// ???  = (INT16)(hng64_videoram[(0x40004+(scrollbase<<4))/4]>>16);
	yscroll = (INT16)(hng64_videoram[(0x40008+(scrollbase<<4))/4]>>16);
//  xzoom   = (INT16)(hng64_videoram[(0x40010+(scrollbase<<4))/4]>>16);
//  yzoom   = (INT16)(hng64_videoram[(0x4000c+(scrollbase<<4))/4]>>16);
	// ???  = (INT16)(hng64_videoram[(0x40014+(scrollbase<<4))/4]>>16);
	// ???  = (INT16)(hng64_videoram[(0x40018+(scrollbase<<4))/4]>>16);
	// ???  = (INT16)(hng64_videoram[(0x4001c+(scrollbase<<4))/4]>>16);

	xscroll <<=16;
	yscroll <<=16;

	xzoom = 0x10000;
	yzoom = 0x10000;


	tilemap_draw_roz(bitmap,cliprect,tilemap,xscroll,yscroll,
			xzoom,0,0,yzoom,
			1,
			0,0);
}



/*
 * Video Regs Format
 * ------------------
 *
 * UINT32 | Bytes    | Use
 * -------+-76543210-+----------------
 *   0    | oooooooo | unknown - always seems to be 04060000 (fatfurwa) and 00060000 (buriki)
 *   1    | xxxx---- | looks like it's 0001 most (all) of the time - turns off in buriki intro
 *   1    | ----oooo | unknown - always seems to be 0000 (fatfurwa)
 *   2    | xxxx---- | tilemap0 per layer flags
 *   2    | ----xxxx | tilemap1 per layer flags
 *   3    | xxxx---- | tilemap2 per layer flags
 *   3    | ----xxxx | tilemap3 per layer flags
 *   4    | xxxx---- | tilemap0 offset into tilemap RAM?
 *   4    | ----xxxx | tilemap1 offset into tilemap RAM
 *   5    | xxxx---- | tilemap3 offset into tilemap RAM
 *   5    | ----xxxx | tilemap4 offset into tilemap RAM?
 *   6    | oooooooo | unknown - always seems to be 000001ff (fatfurwa)
 *   7    | oooooooo | unknown - always seems to be 000001ff (fatfurwa)
 *   8    | oooooooo | unknown - always seems to be 80008000 (fatfurwa)
 *   9    | oooooooo | unknown - always seems to be 00000000 (fatfurwa)
 *   a    | oooooooo | unknown - always seems to be 00000000 (fatfurwa)
 *   b    | mmmmmmmm | auto animation mask for tilemaps, - use these bits from the original tile number
 *   c    | xxxxxxxx | auto animation bits for tilemaps, - merge in these bits to auto animate the tilemap
 *   d    | oooooooo | not used ??
 *   e    | oooooooo | not used ??
 */


VIDEO_UPDATE( hng64 )
{
	bitmap_fill(bitmap, 0, get_black_pen(screen->machine));

	// the tilemap auto animation and moveable tilebases means that they end up
	// dirty most frames anyway, even with manual tracking
	tilemap_mark_all_tiles_dirty (hng64_tilemap3);
	tilemap_mark_all_tiles_dirty (hng64_tilemap2);
	tilemap_mark_all_tiles_dirty (hng64_tilemap1);
	tilemap_mark_all_tiles_dirty (hng64_tilemap0);


	hng64_drawtilemap(bitmap,cliprect, (hng64_videoregs[0x05]&0x00003fff)>>0,  hng64_tilemap3);
	hng64_drawtilemap(bitmap,cliprect, (hng64_videoregs[0x05]&0x3fff0000)>>16, hng64_tilemap2);
	hng64_drawtilemap(bitmap,cliprect, (hng64_videoregs[0x04]&0x00003fff)>>0,  hng64_tilemap1);
	hng64_drawtilemap(bitmap,cliprect, (hng64_videoregs[0x04]&0x3fff0000)>>16, hng64_tilemap0);

	draw_sprites(screen->machine, bitmap,cliprect);

	// 3d really shouldn't be last, but you don't see some cool stuff right now if it's put before sprites :)...
	draw3d(screen->machine, bitmap, cliprect);

	transition_control(bitmap, cliprect) ;

	/*
    popmessage("%08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x",
    hng64_videoregs[0x00],
    hng64_videoregs[0x01],
    hng64_videoregs[0x02],
    hng64_videoregs[0x03],
    hng64_videoregs[0x04],
    hng64_videoregs[0x05],
    hng64_videoregs[0x06],
    hng64_videoregs[0x07],
    hng64_videoregs[0x08],
    hng64_videoregs[0x09],
    hng64_videoregs[0x0a],
    hng64_videoregs[0x0b],
    hng64_videoregs[0x0c],
    hng64_videoregs[0x0d],
    hng64_videoregs[0x0e]);
    */

	// tilemap0 per layer flags
	// 0840 - startup tests, 8x8x4 layer
	// 0cc0 - beast busters 2, 8x8x8 layer
	// 0860 - fatal fury wa
	// 08e0 - fatal fury wa during transitions
	// 0940 - samurai shodown 64
	// 0880 - buriki

	// ---l rb?? ???? ????
	// l = floor effects / linescroll enable  (buriki on tilemap1, fatal fury on tilemap3)
	// r = tile size?
	// b = 4bpp/8bpp ?  (beast busters, samsh64, sasm64 2 switch it for some screens)




	popmessage("0: %04x  1: %04x   2: %04x  3: %04x\n", (hng64_videoregs[0x02]&0xffff0000)>>16, (hng64_videoregs[0x02]&0x0000ffff)>>0, (hng64_videoregs[0x03]&0xffff0000)>>16, (hng64_videoregs[0x03]&0x0000ffff)>>0);



//  mame_printf_debug("FRAME DONE %d\n", frameCount) ;
	frameCount++ ;

	return 0;
}

VIDEO_START( hng64 )
{
	const rectangle *visarea = video_screen_get_visible_area(machine->primary_screen);

	hng64_tilemap0 = tilemap_create(machine, get_hng64_tile0_info, tilemap_scan_rows,  8,   8, 128,128); /* 128x128x4 = 0x10000 */
	hng64_tilemap1 = tilemap_create(machine, get_hng64_tile1_info, tilemap_scan_rows,  16, 16, 128,128); /* 128x128x4 = 0x10000 */
	hng64_tilemap2 = tilemap_create(machine, get_hng64_tile2_info, tilemap_scan_rows,  16, 16, 128,128); /* 128x128x4 = 0x10000 */
	hng64_tilemap3 = tilemap_create(machine, get_hng64_tile3_info, tilemap_scan_rows,  16, 16, 128,128); /* 128x128x4 = 0x10000 */
	tilemap_set_transparent_pen(hng64_tilemap0,0);
	tilemap_set_transparent_pen(hng64_tilemap1,0);
	tilemap_set_transparent_pen(hng64_tilemap2,0);
	tilemap_set_transparent_pen(hng64_tilemap3,0);

	// 3d Buffer Allocation
	depthBuffer = auto_alloc_array(machine, float, (visarea->max_x)*(visarea->max_y)) ;

	// The general display list of polygons in the scene...
	// !! This really should be a dynamic array !!
	polys = auto_alloc_array(machine, struct polygon, MAX_ONSCREEN_POLYS)  ;
}

///////////////
// UTILITIES //
///////////////

/* 4x4 matrix multiplication */
static void matmul4( float *product, const float *a, const float *b )
{
   int i;
   for (i = 0; i < 4; i++)
   {
      const float ai0 = a[0  + i] ;
	  const float ai1 = a[4  + i] ;
	  const float ai2 = a[8  + i] ;
	  const float ai3 = a[12 + i] ;

	  product[0  + i] = ai0 * b[0 ] + ai1 * b[1 ] + ai2 * b[2 ] + ai3 * b[3 ] ;
	  product[4  + i] = ai0 * b[4 ] + ai1 * b[5 ] + ai2 * b[6 ] + ai3 * b[7 ] ;
	  product[8  + i] = ai0 * b[8 ] + ai1 * b[9 ] + ai2 * b[10] + ai3 * b[11] ;
	  product[12 + i] = ai0 * b[12] + ai1 * b[13] + ai2 * b[14] + ai3 * b[15] ;
   }
}

/* vector by 4x4 matrix multiply */
static void vecmatmul4( float *product, const float *a, const float *b)
{
	const float bi0 = b[0] ;
	const float bi1 = b[1] ;
	const float bi2 = b[2] ;
	const float bi3 = b[3] ;

	product[0] = bi0 * a[0] + bi1 * a[4] + bi2 * a[8 ] + bi3 * a[12];
	product[1] = bi0 * a[1] + bi1 * a[5] + bi2 * a[9 ] + bi3 * a[13];
	product[2] = bi0 * a[2] + bi1 * a[6] + bi2 * a[10] + bi3 * a[14];
	product[3] = bi0 * a[3] + bi1 * a[7] + bi2 * a[11] + bi3 * a[15];
}

#ifdef UNUSED_FUNCTION
static float vecDotProduct( const float *a, const float *b)
{
	return ((a[0]*b[0]) + (a[1]*b[1]) + (a[2]*b[2])) ;
}
#endif

static void SetIdentity(float *matrix)
{
	int i ;

	for (i = 0; i < 16; i++)
	{
		matrix[i] = 0.0f ;
	}

	matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.0f ;
}

static float uToF(UINT16 input)
{
	float retVal ;

	retVal = (float)((INT16)input) / 32768.0f ;

/*
    if ((INT16)input < 0)
        retVal = (float)((INT16)input) / 32768.0f ;
    else
        retVal = (float)((INT16)input) / 32767.0f ;
*/

	return retVal ;
}

#ifdef UNUSED_FUNCTION
static void normalize(float* x)
{
	double l2 = (x[0]*x[0]) + (x[1]*x[1]) + (x[2]*x[2]);
	double l=sqrt(l2) ;

	x[0] = (float)(x[0] / l) ;
	x[1] = (float)(x[1] / l) ;
	x[2] = (float)(x[2] / l) ;
}
#endif



///////////////////////////
// POLYGON CLIPPING CODE //
///////////////////////////

///////////////////////////////////////////////////////////////////////////////////
// The remainder of the code in this file is heavily                             //
//   influenced by, and sometimes copied verbatim from Andrew Zaferakis' SoftGL  //
//   rasterizing system.  http://www.cs.unc.edu/~andrewz/comp236/                //
//                                                                               //
//   Andrew granted permission for its use in MAME in October of 2004.           //
///////////////////////////////////////////////////////////////////////////////////

// Refer to the clipping planes as numbers
#define HNG64_LEFT   0
#define HNG64_RIGHT  1
#define HNG64_TOP    2
#define HNG64_BOTTOM 3
#define HNG64_NEAR   4
#define HNG64_FAR    5


static int Inside(struct polyVert *v, int plane)
{
	switch(plane)
	{
	case HNG64_LEFT:
		return (v->clipCoords[0] >= -v->clipCoords[3]) ? 1 : 0 ;
	case HNG64_RIGHT:
		return (v->clipCoords[0] <=  v->clipCoords[3]) ? 1 : 0 ;

	case HNG64_TOP:
		return (v->clipCoords[1] <=  v->clipCoords[3]) ? 1 : 0 ;
	case HNG64_BOTTOM:
		return (v->clipCoords[1] >= -v->clipCoords[3]) ? 1 : 0 ;

	case HNG64_NEAR:
		return (v->clipCoords[2] <=  v->clipCoords[3]) ? 1 : 0;

	case HNG64_FAR:
		return (v->clipCoords[2] >= -v->clipCoords[3]) ? 1 : 0;

	}

	return 0 ;
}

static void Intersect(struct polyVert *input0, struct polyVert *input1, struct polyVert *output, int plane)
{
	float t = 0.0f ;

	float *Iv0 = input0->clipCoords ;
	float *Iv1 = input1->clipCoords ;
	float *Ov  = output->clipCoords ;

	float *It0 = input0->texCoords ;
	float *It1 = input1->texCoords ;
	float *Ot  = output->texCoords ;

	float *Il0 = input0->light ;
	float *Il1 = input1->light ;
	float *Ol  = output->light ;

	switch(plane)
	{
	case HNG64_LEFT:
		t = (Iv0[0]+Iv0[3]) / (-Iv1[3]+Iv0[3]-Iv1[0]+Iv0[0]);
		break;
	case HNG64_RIGHT:
		t = (Iv0[0]-Iv0[3]) / (Iv1[3]-Iv0[3]-Iv1[0]+Iv0[0]);
		break;
	case HNG64_TOP:
		t = (Iv0[1]-Iv0[3]) / (Iv1[3]-Iv0[3]-Iv1[1]+Iv0[1]);
		break;
	case HNG64_BOTTOM:
		t = (Iv0[1]+Iv0[3]) / (-Iv1[3]+Iv0[3]-Iv1[1]+Iv0[1]);
		break;
	case HNG64_NEAR:
		t = (Iv0[2]-Iv0[3]) / (Iv1[3]-Iv0[3]-Iv1[2]+Iv0[2]);
		break;
	case HNG64_FAR:
		t = (Iv0[2]+Iv0[3]) / (-Iv1[3]+Iv0[3]-Iv1[2]+Iv0[2]);
		break;
	}

	Ov[0] = Iv0[0] + (Iv1[0] - Iv0[0]) * t ;
	Ov[1] = Iv0[1] + (Iv1[1] - Iv0[1]) * t ;
	Ov[2] = Iv0[2] + (Iv1[2] - Iv0[2]) * t ;
	Ov[3] = Iv0[3] + (Iv1[3] - Iv0[3]) * t ;

	Ot[0] = It0[0] + (It1[0] - It0[0]) * t ;
	Ot[1] = It0[1] + (It1[1] - It0[1]) * t ;
	Ot[2] = It0[2] + (It1[2] - It0[2]) * t ;
	Ot[3] = It0[3] + (It1[3] - It0[3]) * t ;

	Ol[0] = Il0[0] + (Il1[0] - Il0[0]) * t ;
	Ol[1] = Il0[1] + (Il1[1] - Il0[1]) * t ;
	Ol[2] = Il0[2] + (Il1[2] - Il0[2]) * t ;
}



static void PerformFrustumClip(struct polygon *p)
{
	int i, j ;
	int k ;

	//////////////////////////////////////////////////////////////////////////
	// Clip against the volumes defined by the homogeneous clip coordinates //
	//////////////////////////////////////////////////////////////////////////

	struct polygon temp ;

	struct polyVert *v0 ;
	struct polyVert *v1 ;
	struct polyVert *tv ;

	temp.n = 0;

	// Skip near and far clipping planes ?
	for (j = 0; j <= HNG64_BOTTOM; j++)
	{
		for (i = 0; i < p->n; i++)
		{
			k = (i+1) % p->n; // Index of next vertex

			v0 = &p->vert[i] ;
			v1 = &p->vert[k] ;

			tv = &temp.vert[temp.n] ;

			if (Inside(v0, j) && Inside(v1, j))							// Edge is completely inside the volume...
			{
				memcpy(tv, v1, sizeof(struct polyVert)) ;
				temp.n++;
			}

			else if (Inside(v0, j) && !Inside(v1, j))					// Edge goes from in to out...
			{
				Intersect(v0, v1, tv, j) ;
				temp.n++;
			}

			else if (!Inside(v0, j) && Inside(v1, j))					// Edge goes from out to in...
			{
				Intersect(v0, v1, tv, j) ;
				memcpy(&temp.vert[temp.n+1], v1, sizeof(struct polyVert)) ;
				temp.n+=2;
			}
		}

		p->n = temp.n;

		for (i = 0; i < temp.n; i++)
		{
			memcpy(&p->vert[i], &temp.vert[i], sizeof(struct polyVert)) ;
		}

		temp.n = 0 ;
	}
}


//////////////////////////////
// POLYGON RASTERIZING CODE //
//////////////////////////////

/////////////////////////
// wireframe rendering //
/////////////////////////

#ifdef UNUSED_FUNCTION
static void plot(INT32 x, INT32 y, INT32 color, bitmap_t *bitmap)
{
	*BITMAP_ADDR32(bitmap, y, x) = MAKE_ARGB((UINT8)255, (UINT8)color, (UINT8)color, (UINT8)color) ;
}

// Stolen from http://en.wikipedia.org/wiki/Bresenham's_line_algorithm (no copyright denoted) - the non-optimized version
static void drawline2d(INT32 x0, INT32 y0, INT32 x1, INT32 y1, INT32 color, bitmap_t *bitmap)
{
#define SWAP(a,b) tmpswap = a; a = b; b = tmpswap;

	INT32 i;
	INT32 steep = 1;
	INT32 sx, sy;  /* step positive or negative (1 or -1) */
	INT32 dx, dy;  /* delta (difference in X and Y between points) */
	INT32 e;

	/*
    * inline swap. On some architectures, the XOR trick may be faster
    */
	INT32 tmpswap;

	/*
    * optimize for vertical and horizontal lines here
    */

	dx = abs(x1 - x0);
	sx = ((x1 - x0) > 0) ? 1 : -1;
	dy = abs(y1 - y0);
	sy = ((y1 - y0) > 0) ? 1 : -1;

	if (dy > dx)
	{
		steep = 0;
		SWAP(x0, y0);
		SWAP(dx, dy);
		SWAP(sx, sy);
	}

	e = (dy << 1) - dx;

	for (i = 0; i < dx; i++)
	{
		if (steep)
		{
			plot(x0,y0,color, bitmap);
		}
		else
		{
			plot(y0,x0,color, bitmap);
		}
		while (e >= 0)
		{
			y0 += sy;
			e -= (dx << 1);
		}

		x0 += sx;
		e += (dy << 1);
	}
#undef SWAP
}


static void DrawWireframe(struct polygon *p, bitmap_t *bitmap)
{
	int j;
	for (j = 0; j < p->n; j++)
	{
		// mame_printf_debug("now drawing : %f %f %f, %f %f %f\n", p->vert[j].clipCoords[0], p->vert[j].clipCoords[1], p->vert[j].clipCoords[2], p->vert[(j+1)%p->n].clipCoords[0], p->vert[(j+1)%p->n].clipCoords[1], p->vert[(j+1)%p->n].clipCoords[2]) ;
		// mame_printf_debug("%f %f %f %f\n", p->vert[j].clipCoords[0], p->vert[j].clipCoords[1], p->vert[(j+1)%p->n].clipCoords[0], p->vert[(j+1)%p->n].clipCoords[1]) ;
		drawline2d(p->vert[j].clipCoords[0], p->vert[j].clipCoords[1], p->vert[(j+1)%p->n].clipCoords[0], p->vert[(j+1)%p->n].clipCoords[1], 255, bitmap) ;
	}

	// SHOWS THE CLIPPING //
	/*
    for (int j = 1; j < p->n-1; j++)
    {
        drawline2d(p->vert[0].clipCoords[0],   p->vert[0].clipCoords[1],   p->vert[j].clipCoords[0],   p->vert[j].clipCoords[1],   255, bitmap) ;
        drawline2d(p->vert[j].clipCoords[0],   p->vert[j].clipCoords[1],   p->vert[j+1].clipCoords[0], p->vert[j+1].clipCoords[1], 255, bitmap) ;
        drawline2d(p->vert[j+1].clipCoords[0], p->vert[j+1].clipCoords[1], p->vert[0].clipCoords[0],   p->vert[0].clipCoords[1],   255, bitmap) ;
    }
    */
}
#endif


///////////////////////
// polygon rendering //
///////////////////////

static void RasterizeTriangle_SMOOTH_TEX_PC(running_machine *machine, bitmap_t *Color,
                                     float A[4], float B[4], float C[4],
                                     float Ca[3], float Cb[3], float Cc[3], // PER-VERTEX RGB COLORS
                                     float Ta[2], float Tb[2], float Tc[2], // PER-VERTEX (S,T) TEX-COORDS
                                     int Wrapping, int Filtering, int Function) ;

static void DrawShaded(running_machine *machine, struct polygon *p, bitmap_t *bitmap)
{
	// The perspective-correct texture divide...
	// !!! There is a very good chance the HNG64 hardware does not do perspective-correct texture-mapping !!!
	int j;
	for (j = 0; j < p->n; j++)
	{
		p->vert[j].clipCoords[3] = 1.0f / p->vert[j].clipCoords[3] ;
		p->vert[j].light[0]      = p->vert[j].light[0]     * p->vert[j].clipCoords[3] ;
		p->vert[j].light[1]      = p->vert[j].light[1]     * p->vert[j].clipCoords[3] ;
		p->vert[j].light[2]      = p->vert[j].light[2]     * p->vert[j].clipCoords[3] ;
		p->vert[j].texCoords[0]  = p->vert[j].texCoords[0] * p->vert[j].clipCoords[3] ;
		p->vert[j].texCoords[1]  = p->vert[j].texCoords[1] * p->vert[j].clipCoords[3] ;
	}

	for (j = 1; j < p->n-1; j++)
	{
		RasterizeTriangle_SMOOTH_TEX_PC(machine, bitmap,
										p->vert[0].clipCoords, p->vert[j].clipCoords, p->vert[j+1].clipCoords,
										p->vert[0].light,      p->vert[j].light,      p->vert[j+1].light,
										p->vert[0].texCoords,  p->vert[j].texCoords,  p->vert[j+1].texCoords,
										p->texType, p->palIndex, p->texIndex) ;

	}
}


/*********************************************************************/
/**   FillSmoothTexPCHorizontalLine                                 **/
/**     Input: Color Buffer (framebuffer), depth buffer, width and  **/
/**            height of framebuffer, starting, and ending values   **/
/**            for x and y, constant y.  Fills horizontally with    **/
/**            z,r,g,b interpolation.                               **/
/**                                                                 **/
/**     Output: none                                                **/
/*********************************************************************/
INLINE void FillSmoothTexPCHorizontalLine(running_machine *machine, bitmap_t *Color,
					  int Wrapping, int Filtering, int Function,
					  int x_start, int x_end, int y, float z_start, float z_delta,
					  float w_start, float w_delta, float r_start, float r_delta,
					  float g_start, float g_delta, float b_start, float b_delta,
					  float s_start, float s_delta, float t_start, float t_delta)
{
	float *dp = &(depthBuffer[y*video_screen_get_visible_area(machine->primary_screen)->max_x+x_start]);

	const UINT8 *gfx = memory_region(machine, "textures");
	const UINT8 *textureOffset ;
	UINT8 paletteEntry ;
	float t_coord, s_coord ;

	if (Function >= 0) textureOffset = &gfx[Function * 1024 * 1024] ;
	else               textureOffset = 0x00 ;


	for ( ; x_start <= x_end; x_start++)
	{
		if (z_start < (*dp))
		{
			// MULTIPLY BACK THROUGH BY W
			t_coord = t_start / w_start;
			s_coord = s_start / w_start;

			// GET THE TEXTURE INDEX
			if (Function >= 0)
			{
				if (Wrapping == 0x8 || Wrapping == 0xc)
					paletteEntry = textureOffset[(((int)(s_coord*1024.0f))*1024 + (int)(t_coord*1024.0f))] ;
				else
					paletteEntry = textureOffset[(((int)(s_coord*512.0f))*1024 + (int)(t_coord*512.0f))] ;

				// Naieve Alpha Implementation (?) - don't draw if you're at texture index 0...
				if (paletteEntry != 0)
				{
					// Greyscale texture - for Buriki...
					// *BITMAP_ADDR32(Color, y, x_start) = MAKE_ARGB(255, (UINT8)paletteEntry, (UINT8)paletteEntry, (UINT8)paletteEntry) ;

					*BITMAP_ADDR32(Color, y, x_start) = machine->pens[(128*(Filtering))+paletteEntry] ;
					*dp = z_start ;
				}
			}
			else
			{
				*BITMAP_ADDR32(Color, y, x_start) = MAKE_ARGB(255, (UINT8)(r_start/w_start), (UINT8)(g_start/w_start), (UINT8)(b_start/w_start)) ;
				*dp = z_start;
			}
		}
		dp++;
		z_start += z_delta;
		w_start += w_delta;
		r_start += r_delta;
		g_start += g_delta;
		b_start += b_delta;
		s_start += s_delta;
		t_start += t_delta;
	}
}

//----------------------------------------------------------------------------
// Given 3D triangle ABC in screen space with clipped coordinates within the following
// bounds: x in [0,W], y in [0,H], z in [0,1]. The origin for (x,y) is in the bottom
// left corner of the pixel grid. z=0 is the near plane and z=1 is the far plane,
// so lesser values are closer. The coordinates of the pixels are evenly spaced
// in x and y 1 units apart starting at the bottom-left pixel with coords
// (0.5,0.5). In other words, the pixel sample point is in the center of the
// rectangular grid cell containing the pixel sample. The framebuffer has
// dimensions width x height (WxH). The Color buffer is a 1D array (row-major
// order) with 3 unsigned chars per pixel (24-bit color). The Depth buffer is
// a 1D array (also row-major order) with a float value per pixel
// For a pixel location (x,y) we can obtain
// the Color and Depth array locations as: Color[(((int)y)*W+((int)x))*3]
// (for the red value, green is offset +1, and blue is offset +2 and
// Depth[((int)y)*W+((int)x)]. Fills the pixels contained in the triangle
// with the global current color and the properly linearly interpolated depth
// value (performs Z-buffer depth test before writing new pixel).
// Pixel samples that lie inside the triangle edges are filled with
// a bias towards the minimum values (samples that lie exactly on a triangle
// edge are filled only for minimum x values along a horizontal span and for
// minimum y values, samples lying on max values are not filled).
// Per-vertex colors are RGB floating point triplets in [0.0,255.0]. The vertices
// include their w-components for use in linearly interpolating perspectively
// correct color (RGB) and texture-coords (st) across the face of the triangle.
// A texture image of RGB floating point triplets of size TWxWH is also given.
// Texture colors are normalized RGB values in [0,1].
//   clamp and repeat wrapping modes : Wrapping={0,1}
//   nearest and bilinear filtering: Filtering={0,1}
//   replace and modulate application modes: Function={0,1}
//---------------------------------------------------------------------------
static void RasterizeTriangle_SMOOTH_TEX_PC(running_machine *machine, bitmap_t *Color,
                                     float A[4], float B[4], float C[4],
                                     float Ca[3], float Cb[3], float Cc[3], // PER-VERTEX RGB COLORS
                                     float Ta[2], float Tb[2], float Tc[2], // PER-VERTEX (S,T) TEX-COORDS
                                     int Wrapping, int Filtering, int Function)
{
	// Get our order of points by increasing y-coord
	float *p_min = ((A[1] <= B[1]) && (A[1] <= C[1])) ? A : ((B[1] <= A[1]) && (B[1] <= C[1])) ? B : C;
	float *p_max = ((A[1] >= B[1]) && (A[1] >= C[1])) ? A : ((B[1] >= A[1]) && (B[1] >= C[1])) ? B : C;
	float *p_mid = ((A != p_min) && (A != p_max)) ? A : ((B != p_min) && (B != p_max)) ? B : C;

	// Perspectively correct color interpolation, interpolate r/w, g/w, b/w, then divide by 1/w at each pixel (A[3] = 1/w)
	float ca[3], cb[3], cc[3];
	float ta[2], tb[2], tc[2];

	float *c_min;
	float *c_mid;
	float *c_max;

	// We must keep the tex coords straight with the point ordering
	float *t_min;
	float *t_mid;
	float *t_max;

	// Find out control points for y, this divides the triangle into upper and lower
	int   y_min;
	int   y_max;
	int   y_mid;

	// Compute the slopes of each line, and color this is used to determine the interpolation
	float x1_slope;
	float x2_slope;
	float z1_slope;
	float z2_slope;
	float w1_slope;
	float w2_slope;
	float r1_slope;
	float r2_slope;
	float g1_slope;
	float g2_slope;
	float b1_slope;
	float b2_slope;
	float s1_slope;
	float s2_slope;
	float t1_slope;
	float t2_slope;

	// Compute the t values used in the equation Ax = Ax + (Bx - Ax)*t
	// We only need one t, because it is only used to compute the start.
	// Create storage for the interpolated x and z values for both lines
	// also for the RGB interpolation
	float t;
	float x1_interp;
	float z1_interp;
	float w1_interp;
	float r1_interp;
	float g1_interp;
	float b1_interp;
	float s1_interp;
	float t1_interp;

	float x2_interp;
	float z2_interp;
	float w2_interp;
	float r2_interp;
	float g2_interp;
	float b2_interp;
	float s2_interp;
	float t2_interp;

	// Create storage for the horizontal interpolation of z and RGB color and its starting points
	// This is used to fill the triangle horizontally
	int   x_start,     x_end;
	float z_interp_x,  z_delta_x;
	float w_interp_x,  w_delta_x;
	float r_interp_x,  r_delta_x;
	float g_interp_x,  g_delta_x;
	float b_interp_x,  b_delta_x;
	float s_interp_x,  s_delta_x;
	float t_interp_x,  t_delta_x;

	ca[0] = Ca[0]; ca[1] = Ca[1]; ca[2] = Ca[2];
	cb[0] = Cb[0]; cb[1] = Cb[1]; cb[2] = Cb[2];
	cc[0] = Cc[0]; cc[1] = Cc[1]; cc[2] = Cc[2];

	// Perspectively correct tex interpolation, interpolate s/w, t/w, then divide by 1/w at each pixel (A[3] = 1/w)
	ta[0] = Ta[0]; ta[1] = Ta[1];
	tb[0] = Tb[0]; tb[1] = Tb[1];
	tc[0] = Tc[0]; tc[1] = Tc[1];

	// We must keep the colors straight with the point ordering
	c_min = (p_min == A) ? ca : (p_min == B) ? cb : cc;
	c_mid = (p_mid == A) ? ca : (p_mid == B) ? cb : cc;
	c_max = (p_max == A) ? ca : (p_max == B) ? cb : cc;

	// We must keep the tex coords straight with the point ordering
	t_min = (p_min == A) ? ta : (p_min == B) ? tb : tc;
	t_mid = (p_mid == A) ? ta : (p_mid == B) ? tb : tc;
	t_max = (p_max == A) ? ta : (p_max == B) ? tb : tc;

	// Find out control points for y, this divides the triangle into upper and lower
	y_min  = (((int)p_min[1]) + 0.5 >= p_min[1]) ? p_min[1] : ((int)p_min[1]) + 1;
	y_max  = (((int)p_max[1]) + 0.5 <  p_max[1]) ? p_max[1] : ((int)p_max[1]) - 1;
	y_mid  = (((int)p_mid[1]) + 0.5 >= p_mid[1]) ? p_mid[1] : ((int)p_mid[1]) + 1;

	// Compute the slopes of each line, and color this is used to determine the interpolation
	x1_slope = (p_max[0] - p_min[0]) / (p_max[1] - p_min[1]);
	x2_slope = (p_mid[0] - p_min[0]) / (p_mid[1] - p_min[1]);
	z1_slope = (p_max[2] - p_min[2]) / (p_max[1] - p_min[1]);
	z2_slope = (p_mid[2] - p_min[2]) / (p_mid[1] - p_min[1]);
	w1_slope = (p_max[3] - p_min[3]) / (p_max[1] - p_min[1]);
	w2_slope = (p_mid[3] - p_min[3]) / (p_mid[1] - p_min[1]);
	r1_slope = (c_max[0] - c_min[0]) / (p_max[1] - p_min[1]);
	r2_slope = (c_mid[0] - c_min[0]) / (p_mid[1] - p_min[1]);
	g1_slope = (c_max[1] - c_min[1]) / (p_max[1] - p_min[1]);
	g2_slope = (c_mid[1] - c_min[1]) / (p_mid[1] - p_min[1]);
	b1_slope = (c_max[2] - c_min[2]) / (p_max[1] - p_min[1]);
	b2_slope = (c_mid[2] - c_min[2]) / (p_mid[1] - p_min[1]);
	s1_slope = (t_max[0] - t_min[0]) / (p_max[1] - p_min[1]);
	s2_slope = (t_mid[0] - t_min[0]) / (p_mid[1] - p_min[1]);
	t1_slope = (t_max[1] - t_min[1]) / (p_max[1] - p_min[1]);
	t2_slope = (t_mid[1] - t_min[1]) / (p_mid[1] - p_min[1]);

	// Compute the t values used in the equation Ax = Ax + (Bx - Ax)*t
	// We only need one t, because it is only used to compute the start.
	// Create storage for the interpolated x and z values for both lines
	// also for the RGB interpolation
	t = (((float)y_min) + 0.5 - p_min[1]) / (p_max[1] - p_min[1]);
	x1_interp = p_min[0] + (p_max[0] - p_min[0]) * t;
	z1_interp = p_min[2] + (p_max[2] - p_min[2]) * t;
	w1_interp = p_min[3] + (p_max[3] - p_min[3]) * t;
	r1_interp = c_min[0] + (c_max[0] - c_min[0]) * t;
	g1_interp = c_min[1] + (c_max[1] - c_min[1]) * t;
	b1_interp = c_min[2] + (c_max[2] - c_min[2]) * t;
	s1_interp = t_min[0] + (t_max[0] - t_min[0]) * t;
	t1_interp = t_min[1] + (t_max[1] - t_min[1]) * t;

	t = (((float)y_min) + 0.5 - p_min[1]) / (p_mid[1] - p_min[1]);
	x2_interp = p_min[0] + (p_mid[0] - p_min[0]) * t;
	z2_interp = p_min[2] + (p_mid[2] - p_min[2]) * t;
	w2_interp = p_min[3] + (p_mid[3] - p_min[3]) * t;
	r2_interp = c_min[0] + (c_mid[0] - c_min[0]) * t;
	g2_interp = c_min[1] + (c_mid[1] - c_min[1]) * t;
	b2_interp = c_min[2] + (c_mid[2] - c_min[2]) * t;
	s2_interp = t_min[0] + (t_mid[0] - t_min[0]) * t;
	t2_interp = t_min[1] + (t_mid[1] - t_min[1]) * t;

	// First work on the bottom half of the triangle
	// I'm using y_min as the incrementer because it saves space and we don't need it anymore
	for ( ; y_min < y_mid; y_min++) {

		// We always want to fill left to right, so we have 2 main cases
		// Compute the integer starting and ending points and the appropriate z by
		// interpolating.  Remember the pixels are in the middle of the grid, i.e. (0.5,0.5,0.5)
		if (x1_interp < x2_interp) {
			x_start    = ((((int)x1_interp) + 0.5) >= x1_interp) ? x1_interp : ((int)x1_interp) + 1;
			x_end      = ((((int)x2_interp) + 0.5) <  x2_interp) ? x2_interp : ((int)x2_interp) - 1;
			z_delta_x  = (z2_interp - z1_interp) / (x2_interp - x1_interp);
			w_delta_x  = (w2_interp - w1_interp) / (x2_interp - x1_interp);
			r_delta_x  = (r2_interp - r1_interp) / (x2_interp - x1_interp);
			g_delta_x  = (g2_interp - g1_interp) / (x2_interp - x1_interp);
			b_delta_x  = (b2_interp - b1_interp) / (x2_interp - x1_interp);
			s_delta_x  = (s2_interp - s1_interp) / (x2_interp - x1_interp);
			t_delta_x  = (t2_interp - t1_interp) / (x2_interp - x1_interp);
			t          = (x_start + 0.5 - x1_interp) / (x2_interp - x1_interp);
			z_interp_x = z1_interp + (z2_interp - z1_interp) * t;
			w_interp_x = w1_interp + (w2_interp - w1_interp) * t;
			r_interp_x = r1_interp + (r2_interp - r1_interp) * t;
			g_interp_x = g1_interp + (g2_interp - g1_interp) * t;
			b_interp_x = b1_interp + (b2_interp - b1_interp) * t;
			s_interp_x = s1_interp + (s2_interp - s1_interp) * t;
			t_interp_x = t1_interp + (t2_interp - t1_interp) * t;

		} else {
			x_start    = ((((int)x2_interp) + 0.5) >= x2_interp) ? x2_interp : ((int)x2_interp) + 1;
			x_end      = ((((int)x1_interp) + 0.5) <  x1_interp) ? x1_interp : ((int)x1_interp) - 1;
			z_delta_x  = (z1_interp - z2_interp) / (x1_interp - x2_interp);
			w_delta_x  = (w1_interp - w2_interp) / (x1_interp - x2_interp);
			r_delta_x  = (r1_interp - r2_interp) / (x1_interp - x2_interp);
			g_delta_x  = (g1_interp - g2_interp) / (x1_interp - x2_interp);
			b_delta_x  = (b1_interp - b2_interp) / (x1_interp - x2_interp);
			s_delta_x  = (s1_interp - s2_interp) / (x1_interp - x2_interp);
			t_delta_x  = (t1_interp - t2_interp) / (x1_interp - x2_interp);
			t          = (x_start + 0.5 - x2_interp) / (x1_interp - x2_interp);
			z_interp_x = z2_interp + (z1_interp - z2_interp) * t;
			w_interp_x = w2_interp + (w1_interp - w2_interp) * t;
			r_interp_x = r2_interp + (r1_interp - r2_interp) * t;
			g_interp_x = g2_interp + (g1_interp - g2_interp) * t;
			b_interp_x = b2_interp + (b1_interp - b2_interp) * t;
			s_interp_x = s2_interp + (s1_interp - s2_interp) * t;
			t_interp_x = t2_interp + (t1_interp - t2_interp) * t;
		}

		// Pass the horizontal line to the filler, this could be put in the routine
		// then interpolate for the next values of x and z
		FillSmoothTexPCHorizontalLine(machine, Color, Wrapping, Filtering, Function,
			x_start, x_end, y_min, z_interp_x, z_delta_x, w_interp_x, w_delta_x,
			r_interp_x, r_delta_x, g_interp_x, g_delta_x, b_interp_x, b_delta_x,
			s_interp_x, s_delta_x, t_interp_x, t_delta_x);
		x1_interp += x1_slope;   z1_interp += z1_slope;
		x2_interp += x2_slope;   z2_interp += z2_slope;
		r1_interp += r1_slope;   r2_interp += r2_slope;
		g1_interp += g1_slope;   g2_interp += g2_slope;
		b1_interp += b1_slope;   b2_interp += b2_slope;
		w1_interp += w1_slope;   w2_interp += w2_slope;
		s1_interp += s1_slope;   s2_interp += s2_slope;
		t1_interp += t1_slope;   t2_interp += t2_slope;
	}

	// Now do the same thing for the top half of the triangle.
	// We only need to recompute the x2 line because it changes at the midpoint
	x2_slope = (p_max[0] - p_mid[0]) / (p_max[1] - p_mid[1]);
	z2_slope = (p_max[2] - p_mid[2]) / (p_max[1] - p_mid[1]);
	w2_slope = (p_max[3] - p_mid[3]) / (p_max[1] - p_mid[1]);
	r2_slope = (c_max[0] - c_mid[0]) / (p_max[1] - p_mid[1]);
	g2_slope = (c_max[1] - c_mid[1]) / (p_max[1] - p_mid[1]);
	b2_slope = (c_max[2] - c_mid[2]) / (p_max[1] - p_mid[1]);
	s2_slope = (t_max[0] - t_mid[0]) / (p_max[1] - p_mid[1]);
	t2_slope = (t_max[1] - t_mid[1]) / (p_max[1] - p_mid[1]);

	t = (((float)y_mid) + 0.5 - p_mid[1]) / (p_max[1] - p_mid[1]);
	x2_interp = p_mid[0] + (p_max[0] - p_mid[0]) * t;
	z2_interp = p_mid[2] + (p_max[2] - p_mid[2]) * t;
	w2_interp = p_mid[3] + (p_max[3] - p_mid[3]) * t;
	r2_interp = c_mid[0] + (c_max[0] - c_mid[0]) * t;
	g2_interp = c_mid[1] + (c_max[1] - c_mid[1]) * t;
	b2_interp = c_mid[2] + (c_max[2] - c_mid[2]) * t;
	s2_interp = t_mid[0] + (t_max[0] - t_mid[0]) * t;
	t2_interp = t_mid[1] + (t_max[1] - t_mid[1]) * t;

	// We've seen this loop before haven't we?
	// I'm using y_mid as the incrementer because it saves space and we don't need it anymore
	for ( ; y_mid <= y_max; y_mid++) {

		if (x1_interp < x2_interp) {
			x_start    = ((((int)x1_interp) + 0.5) >= x1_interp) ? x1_interp : ((int)x1_interp) + 1;
			x_end      = ((((int)x2_interp) + 0.5) <  x2_interp) ? x2_interp : ((int)x2_interp) - 1;
			z_delta_x  = (z2_interp - z1_interp) / (x2_interp - x1_interp);
			w_delta_x  = (w2_interp - w1_interp) / (x2_interp - x1_interp);
			r_delta_x  = (r2_interp - r1_interp) / (x2_interp - x1_interp);
			g_delta_x  = (g2_interp - g1_interp) / (x2_interp - x1_interp);
			b_delta_x  = (b2_interp - b1_interp) / (x2_interp - x1_interp);
			s_delta_x  = (s2_interp - s1_interp) / (x2_interp - x1_interp);
			t_delta_x  = (t2_interp - t1_interp) / (x2_interp - x1_interp);
			t          = (x_start + 0.5 - x1_interp) / (x2_interp - x1_interp);
			z_interp_x = z1_interp + (z2_interp - z1_interp) * t;
			w_interp_x = w1_interp + (w2_interp - w1_interp) * t;
			r_interp_x = r1_interp + (r2_interp - r1_interp) * t;
			g_interp_x = g1_interp + (g2_interp - g1_interp) * t;
			b_interp_x = b1_interp + (b2_interp - b1_interp) * t;
			s_interp_x = s1_interp + (s2_interp - s1_interp) * t;
			t_interp_x = t1_interp + (t2_interp - t1_interp) * t;

		} else {
			x_start    = ((((int)x2_interp) + 0.5) >= x2_interp) ? x2_interp : ((int)x2_interp) + 1;
			x_end      = ((((int)x1_interp) + 0.5) <  x1_interp) ? x1_interp : ((int)x1_interp) - 1;
			z_delta_x  = (z1_interp - z2_interp) / (x1_interp - x2_interp);
			w_delta_x  = (w1_interp - w2_interp) / (x1_interp - x2_interp);
			r_delta_x  = (r1_interp - r2_interp) / (x1_interp - x2_interp);
			g_delta_x  = (g1_interp - g2_interp) / (x1_interp - x2_interp);
			b_delta_x  = (b1_interp - b2_interp) / (x1_interp - x2_interp);
			s_delta_x  = (s1_interp - s2_interp) / (x1_interp - x2_interp);
			t_delta_x  = (t1_interp - t2_interp) / (x1_interp - x2_interp);
			t          = (x_start + 0.5 - x2_interp) / (x1_interp - x2_interp);
			z_interp_x = z2_interp + (z1_interp - z2_interp) * t;
			w_interp_x = w2_interp + (w1_interp - w2_interp) * t;
			r_interp_x = r2_interp + (r1_interp - r2_interp) * t;
			g_interp_x = g2_interp + (g1_interp - g2_interp) * t;
			b_interp_x = b2_interp + (b1_interp - b2_interp) * t;
			s_interp_x = s2_interp + (s1_interp - s2_interp) * t;
			t_interp_x = t2_interp + (t1_interp - t2_interp) * t;
		}

		// Pass the horizontal line to the filler, this could be put in the routine
		// then interpolate for the next values of x and z
		FillSmoothTexPCHorizontalLine(machine, Color, Wrapping, Filtering, Function,
			x_start, x_end, y_mid, z_interp_x, z_delta_x, w_interp_x, w_delta_x,
			r_interp_x, r_delta_x, g_interp_x, g_delta_x, b_interp_x, b_delta_x,
			s_interp_x, s_delta_x, t_interp_x, t_delta_x);
		x1_interp += x1_slope;   z1_interp += z1_slope;
		x2_interp += x2_slope;   z2_interp += z2_slope;
		r1_interp += r1_slope;   r2_interp += r2_slope;
		g1_interp += g1_slope;   g2_interp += g2_slope;
		b1_interp += b1_slope;   b2_interp += b2_slope;
		w1_interp += w1_slope;   w2_interp += w2_slope;
		s1_interp += s1_slope;   s2_interp += s2_slope;
		t1_interp += t1_slope;   t2_interp += t2_slope;
	}
}

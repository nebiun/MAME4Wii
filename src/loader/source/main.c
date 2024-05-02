#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <fat.h>

#include "run_dol.h"

static void *xfb = NULL;

u32 first_frame = 1;
GXRModeObj *rmode;

int main(int argc, char *argv[]) 
{
	VIDEO_Init();

	rmode = VIDEO_GetPreferredMode( NULL );
	
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
		
	VIDEO_Configure(rmode);
		
	VIDEO_SetNextFramebuffer(xfb);
	
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();
	fatInitDefault();
	
	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*2);
	setvbuf(stdout, NULL , _IONBF, 0);
	
	if(argc > 0) {
		while(1) {
			if(strcmp(argv[2],"menu") == 0)
				printf("Loading %s...\n",argv[2]);
			else
				printf("Loading %s...\n",argv[3]);
			runDOL (argv[1], argc-2, (const char **)&argv[2]);
			exit(1);
			VIDEO_WaitVSync();
		}
	}	
}

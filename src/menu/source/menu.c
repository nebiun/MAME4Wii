/* MAME4WII
 *
 */

#include <grrlib.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <wiiuse/wpad.h>
#include "font_ttf.h"
#include "mame4wii_png.h"
#include "keyb_png.h"
#include "pointer_png.h"
#include "scroll_png.h"
#include "cursor_png.h"
#include "run_dol.h"
#include "game_list.h"

#define M4W_MAX_INPUT		11
#define M4W_MAX_LINES		16
#define M4W_POSKEY_X		396
#define M4W_POSKEY_Y		168
#define M4W_DIMKEY_X		37
#define M4W_DIMKEY_Y		38
#define M4W_DIMBAR_Y		476.0		
#define M4W_MAXLNAME		32

static GRRLIB_ttfFont *ttf_font;
static GRRLIB_texImg *logo;

static char input_table[8][6] = {
	{ 0x15, 0x15, 0x15, 0x15, 0x15, 0x18 },
	{ '1', '2', '3', '4', '5', 0x08 },
	{ '6', '7', '8', '9', '0', '-' },
	{ 'A', 'B', 'C', 'D', 'E', 'F' },
	{ 'G', 'H', 'I', 'J', 'K', 'L' },
	{ 'M', 'N', 'O', 'P', 'Q', 'R' },
	{ 'S', 'T', 'U', 'V', 'W', 'X' },
	{ 'Y', 'Z', ':', '.', ' ', ' ' } 
};

static void _DrawRectangle(const int x, const int y, const int width, const int height, const u32 color, const bool filled)
{
	GRRLIB_Rectangle ((f32)x, (f32)y, (f32)width, (f32)height, color, filled);
}

void DrawRectangle(int x, int y, int width, int height, u32 color, int thickness)
{
	if(thickness == 0) {
		_DrawRectangle (x, y, width, height, color, true);
	}
	else {
		_DrawRectangle (x, y, width, thickness, color, true);
		_DrawRectangle (x, y+thickness, thickness, height-(2*thickness), color, true);
		_DrawRectangle (x+width-thickness, y+thickness, thickness, height-(2*thickness), color, true);
		_DrawRectangle (x, y+height-thickness, width, thickness, color, true);
	}
}

void print_ttf(int x, int y, const u32 color, const char *format, ...)
{
	char tmpbuf[256];
	va_list aptr;
	
	va_start(aptr, format);
	vsnprintf(tmpbuf,sizeof(tmpbuf), format, aptr);
	va_end(aptr);

	GRRLIB_PrintfTTF(x, y, ttf_font, tmpbuf, M4W_TEXT_HEIGHT, color);
}

void show_logo(void)
{
	GRRLIB_DrawImg(0,0,logo,0,1,1,0xffffffff);
}
	
static void get_interval(int current, int max, int *start, int *end)
{
	*start = current - M4W_MAX_LINES/2;
	*end = *start + M4W_MAX_LINES;
}

static void help()
{
	const int x = 10;
	u32 btn;
	
	do {
		int j;
		
		WPAD_ScanPads();
		
		show_logo();
		j = 0;
		GRRLIB_PrintfTTF(x, logo->h + 4 + 24*j, ttf_font, "Colors meaning", 20, M4W_TEXT_COLOR);
		j += 2;
		GRRLIB_PrintfTTF(x+10, logo->h + 4 + 24*j, ttf_font, "* Game available", 20, M4W_GAMES_COLOR);
		j++;
		GRRLIB_PrintfTTF(x+10, logo->h + 4 + 24*j, ttf_font, "* Game not working in MAME4Wii (but you can try it...)", 20, M4W_NOTWORK_COLOR);
		j++;
		GRRLIB_PrintfTTF(x+10, logo->h + 4 + 24*j, ttf_font, "* Game already crashed", 20, M4W_CRASH_COLOR);
		j++;
		GRRLIB_PrintfTTF(x+10, logo->h + 4 + 24*j, ttf_font, "* Game ROM missing", 20, M4W_MISSING_COLOR);				
		j++;
		GRRLIB_PrintfTTF(x+10, logo->h + 4 + 24*j, ttf_font, "* Game engine missing", 20, M4W_NOLIB_COLOR);				
		j++;
		GRRLIB_PrintfTTF(x+10, logo->h + 4 + 24*j, ttf_font, "* Current game selected (Press B to start)", 20, M4W_CURRENT_COLOR);				
		j += 3;
		GRRLIB_PrintfTTF(x, logo->h + 4 + 24*j, ttf_font, "Press A to return", 20, M4W_TEXT_COLOR);			

		btn = WPAD_ButtonsDown(0);

		GRRLIB_Render(); 
		
	} while((btn & WPAD_BUTTON_A) == 0);
}

int main(int argc, char *argv[]) 
{
	char img_path[PATH_MAX];
	char noimg_path[PATH_MAX];
	char pattern[M4W_MAX_INPUT+1] = { '\0' };
	GRRLIB_texImg *img = NULL;
	GRRLIB_texImg *keyb, *pointer, *scroll, *cursor;
	game_entry_t *game_list;
	int resx = 640, resy = 480;
	ir_t ir;
	int n_games;
	int pattern_idx = 0;
	int current;
	int start, end;
	bool pointer_flag = true;

	// Inizializza la grafica
	GRRLIB_Init();
	
	// inizializza i Wiimote
	WPAD_Init();
	//return data for wii remotes should contain Button Data, Accelerometer, and IR
	WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
	//set IR resolution to 640 width and 480 height
	WPAD_SetVRes(WPAD_CHAN_ALL, resx , resy);
	
	ttf_font = GRRLIB_LoadTTF(font_ttf, font_ttf_size);
	logo = GRRLIB_LoadTexture(mame4wii_png);
	keyb = GRRLIB_LoadTexture(keyb_png);
	scroll = GRRLIB_LoadTexture(scroll_png);
	cursor = GRRLIB_LoadTexture(cursor_png);
	pointer = GRRLIB_LoadTexture(pointer_png);
	
	GRRLIB_FillScreen(RGBA(0x00,0x00,0x00,0x00));	// black;
	
	// Check default snap
	snprintf(noimg_path,sizeof(noimg_path),"%s/%s/nosnap.png",M4W_HOMEUSB,M4W_SNAP);
	if(access(noimg_path, R_OK) != 0) {
		snprintf(noimg_path,sizeof(noimg_path),"%s/%s/nosnap.png",M4W_HOME,M4W_SNAP);
	}

	current = 0;
	game_list = initGamelist(&n_games);
	if(argc > 1) {
		game_entry_t *game_found;
		
		game_found = searchPattern(argv[1],M4W_BYROM);
		if(game_found != NULL) {
			current = game_found - game_list;
		}
	}	
	get_interval(current, n_games, &start, &end);
	
	while(1) {
		game_entry_t *game_found = NULL;
		u32 btn;
		int last;
		int input, slide;
		
		WPAD_ScanPads();  // Leggi i controlli
		WPAD_IR(0, &ir);
		btn = WPAD_ButtonsDown(0);
		
		// Input
		input = 0;
		if(pointer_flag == true) {
			if( GRRLIB_PtInRect(M4W_POSKEY_X,M4W_POSKEY_Y, keyb->w, keyb->h, ir.x, ir.y) ) {
				for(int x=0; x<6; x++) {
					for(int y=0; y<8; y++) {
						if(GRRLIB_PtInRect(M4W_POSKEY_X+x*M4W_DIMKEY_X,M4W_POSKEY_Y+y*M4W_DIMKEY_Y, M4W_DIMKEY_X, M4W_DIMKEY_Y, ir.x, ir.y) ) {
							input = input_table[y][x];
						}
					}
				}
			}
		}			
		
		last = current;
		if((pattern_idx > 0) && (input)) {
			game_found = searchPattern(pattern,M4W_BYDESC);
			if(game_found != NULL) {
				current = game_found - game_list;
			}
		}
		
		slide=0;
		if(pointer_flag == true) {		
			if(GRRLIB_PtInRect(M4W_POSKEY_X-scroll->w,0, scroll->w, 32, ir.x, ir.y) ) {
				slide--;
			}
			else if(GRRLIB_PtInRect(M4W_POSKEY_X-scroll->w,M4W_DIMBAR_Y-32, scroll->w, 32, ir.x, ir.y) ) {
				slide++;
			}
		}

		if (btn & WPAD_BUTTON_A) {
			if(input) {
				switch(input) {
				case 0:
				case 0x15:
					break;
				case 0x08:
					if(pattern_idx > 0) {
						pattern_idx--;
						pattern[pattern_idx] = '\0';
					}
					break;
				case 0x18:
					pattern_idx=0;
					pattern[pattern_idx] = '\0';
					break;
				default:
					if(pattern_idx < M4W_MAX_INPUT) {
						pattern[pattern_idx] = input;
						pattern_idx++;
						pattern[pattern_idx] = '\0';
					}
					break;
				}
			}
			else if(slide != 0) {
				current += (M4W_MAX_LINES/2)*slide;
			}
			else {
				pointer_flag = (pointer_flag == true) ? false : true;
			}
		}
		if (btn & WPAD_BUTTON_B) {
			char exe_path[PATH_MAX];
			char loader_path[PATH_MAX];
			const char *args[5];
			const int nargs=4;
			int err;
		
			if(! (game_list[current].flags & M4W_FLAGS_CHECKED))
				check_game(&game_list[current]);
				
			if(M4W_RUNNABLE(game_list[current].flags)) {	
				// Run the .dol
				if(game_list[current].flags & M4W_FLAGS_DOLONUSB) {
					snprintf(exe_path,sizeof(exe_path),"%s/%s/%s",M4W_HOMEUSB, M4W_LIBS, game_list[current].exe);
					snprintf(loader_path,sizeof(loader_path),"%s/%s/loader.dol",M4W_HOMEUSB, M4W_LIBS);
				}
				else {
					snprintf(exe_path,sizeof(exe_path),"%s/%s/%s",M4W_HOME, M4W_LIBS, game_list[current].exe);
					snprintf(loader_path,sizeof(loader_path),"%s/%s/loader.dol",M4W_HOME, M4W_LIBS);
				}
				args[0] = "loader";
				args[1] = exe_path;
				args[2] = "mame4wii";
				args[3] = game_list[current].rom;

#ifdef MAME4WII_LOADER_DEBUG			
				while(1) {					
					print_ttf(20, 100, M4W_TEXT_COLOR, "exe %s", exe_path);
					print_ttf(20, 130, M4W_TEXT_COLOR, "nargs %d", nargs);
					for(int i=0;i<nargs; i++) {
						print_ttf(20, 160+30*i, M4W_TEXT_COLOR, "args[%d] = %s", i, args[i]);
					}
					GRRLIB_Render(); 
				}
#endif
				GRRLIB_FillScreen(RGBA(0x00,0x00,0x00,0x00));	// black;
				print_ttf(20, 200, M4W_TEXT_COLOR, "Loading %s...", game_list[current].rom);
				GRRLIB_Render();
				
				for(int i=0; i<nargs; i++) {
					m4w_debug("arg[%d] = %s\n", i, args[i]);
				}
				err = runDOL (loader_path, nargs, args);
				if(err != 0) {
					m4w_debug("runDOL rtn = %d", err);
					print_ttf(20, 250, M4W_TEXT_COLOR, "runDOL rtn = %d", err);
					GRRLIB_Render();
					sleep(5);
				}
			}
		}

		// Se premiamo home usciamo dal ciclo
		if (btn & WPAD_BUTTON_HOME)
			break;
		else if (btn & WPAD_BUTTON_UP) {
			pattern_idx=0;
			pattern[pattern_idx] = '\0';
			current--;
		}
		else if (btn & WPAD_BUTTON_DOWN) {
			pattern_idx=0;
			pattern[pattern_idx] = '\0';
			current++;		
		}
		else if (btn & WPAD_BUTTON_PLUS) {
			pattern_idx=0;
			pattern[pattern_idx] = '\0';
			current += M4W_MAX_LINES;
		}
		else if (btn & WPAD_BUTTON_MINUS) {
			pattern_idx=0;
			pattern[pattern_idx] = '\0';
			current -= M4W_MAX_LINES;
		}
		else if (btn & WPAD_BUTTON_2) {
			help();
		}
						
		if(current >= n_games)
			current = 0;
		else if(current < 0)
			current = n_games-1;
		
		if(current != last) {
			get_interval(current, n_games, &start, &end);
			GRRLIB_FreeTexture(img);
			img = NULL;
		}
		
		show_logo();
		
		GRRLIB_DrawImg(M4W_POSKEY_X,M4W_POSKEY_Y,keyb,0,1,1,0xffffffff);
		GRRLIB_DrawImg(M4W_POSKEY_X-scroll->w,0,scroll,0,1,1, (slide != 0) ? 0xffffffff : 0xffffff8f);
		GRRLIB_DrawImg(M4W_POSKEY_X-cursor->w,6+((M4W_DIMBAR_Y-24)/(n_games+1))*current,cursor,0,1,1, 0xffffffff);
		
		GRRLIB_PrintfTTF(M4W_POSKEY_X+10, M4W_POSKEY_Y+10, ttf_font, pattern, 20, 
						(game_found == NULL) ? M4W_CRASH_COLOR : M4W_GAMES_COLOR);
		
		for(int i=start, j=0; i<end; i++, j++) {
			if((i >= 0) && (i < n_games)) { 
				char cut_name[64];
				u32 width;
				u32 color;
				
				strncpy(cut_name, game_list[i].desc, sizeof(cut_name)-1);
#if 0
				sprintf(cut_name,"%x %s", game_list[i].flags, game_list[i].desc);
#endif
				cut_name[M4W_MAXLNAME] = '\0';	// Trunk the name, if needed
							
				if(! (game_list[i].flags & M4W_FLAGS_CHECKED))
					check_game(&game_list[i]);

				if(game_list[i].flags & M4W_FLAGS_CRASHED) {
					color = M4W_CRASH_COLOR;
				}
				else if(game_list[i].flags & M4W_FLAGS_ROMMISSING) {
					color = M4W_MISSING_COLOR;
				}
				else if(game_list[i].flags & M4W_FLAGS_DOLMISSING) {
					color = M4W_NOLIB_COLOR;
				}
				else if(game_list[i].flags & M4W_FLAGS_NOTWORKING) {
					color = M4W_NOTWORK_COLOR;
				}
				else if(game_list[i].flags & M4W_FLAGS_TOOBIG) {
					color = M4W_CRASH_COLOR;
				}
				else {
					color = M4W_GAMES_COLOR;
				}
				
				if(i == current)
					color = M4W_CURRENT_COLOR;
				
				width = GRRLIB_PrintfTTF(4, logo->h + 4 + 24*j, ttf_font, cut_name, 20, color);
				if(pointer_flag == true) {				
					if( GRRLIB_PtInRect(4,logo->h + 4 + 24*j, width, 15, ir.x, ir.y) ) {
						current = i;
						GRRLIB_FreeTexture(img);
						img = NULL;
					}
				}
			}
		}
		
		if(img == NULL) {					
			if(game_list[current].flags & M4W_FLAGS_PNGONUSB)
				snprintf(img_path,sizeof(img_path),"%s/%s/%s.png", M4W_HOMEUSB, M4W_SNAP, game_list[current].rom);
			else if(game_list[current].flags & M4W_FLAGS_PNGONSD)
				snprintf(img_path,sizeof(img_path),"%s/%s/%s.png", M4W_HOME, M4W_SNAP, game_list[current].rom);
			else
				snprintf(img_path,sizeof(img_path),noimg_path);
			
			img = GRRLIB_LoadTextureFromFile(img_path);
		}
		GRRLIB_DrawImg(M4W_POSKEY_X,0,img,0,1,1,0xffffffff);

			
	//	print_ttf(20, 400, M4W_TEXT_COLOR, "input = %d idx = %d (%s)", input, pattern_idx, pattern);
	//	print_ttf(20, 400, M4W_TEXT_COLOR, "argc = %d argv = <%s>", argc, (argc > 1) ? argv[1] : "empty");
		if(pointer_flag == true) {
			GRRLIB_DrawImg(ir.x, ir.y, pointer, 0, 1, 1, (input || slide) ? 0xFF0000FF : 0xFFFFFFFF);
		}
		else {
			GRRLIB_DrawImg(ir.x, ir.y, pointer, 0, 1, 1, 0xFFFFFF3F);
		}
		GRRLIB_Render();  // Aggiorniamo lo schermo
	}

	GRRLIB_Exit(); // Rilasciamo la memoria

	exit(0);  // Usa exit per uscire dal programma (non usare return nel main)
}

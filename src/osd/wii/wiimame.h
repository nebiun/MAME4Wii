//============================================================
//
//  Copyright (c) 1996-2009, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//  MAME Wii by Toad King
//
//============================================================
#ifndef _WIIMAME_H_
#define _WIIMAME_H_
#undef malloc
#undef free
#undef calloc
#undef realloc

#include <stdlib.h>
#include <malloc.h>
#include <stdarg.h>

//#define WIIMAIN_DEBUG	1
//#define WIIAUDIO_DEBUG	1
//#define WIIDIR_DEBUG	1
//#define WIIFILE_DEBUG	1
//#define WIIINPUT_DEBUG	1
//#define WIIMISC_DEBUG	1
//#define WIISYNC_DEBUG	1
//#define WIITHREAD_DEBUG	1
//#define WIITIME_DEBUG	1
//#define WIIVIDEO_DEBUG	1
//#define WIIWORK_DEBUG	1
//#define WII_USEUSB	1

#define wiiWordRound(n)		( (n) + (sizeof(u32) - ((n)%sizeof(u32))) )
#define STACKSIZE       8192

#define AUDIOTH_PRIORITY	70		
#define INPUTTH_PRIORITY	68	
#define VIDEOTH_PRIORITY	67	
#define OTHERTH_PRIORITY	80	

extern void *wii_calloc(size_t);
extern void *wii_malloc(size_t size, const char *src, int line);
extern void wii_free(void *addr, const char *src, int line);
extern void wii_debug(const char *, ...);
extern void wii_printf(const char *, ...);

#endif

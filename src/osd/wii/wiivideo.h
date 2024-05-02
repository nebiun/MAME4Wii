//============================================================
//
//  Copyright (c) 1996-2009, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//  MAME Wii by Toad King
//
//============================================================

#ifndef _WIIVIDEO_H_
#define _WIIVIDEO_H_
#include "render.h"

extern void wii_init_video(running_machine *machine);
extern void wii_init_dimensions(void);
extern void wii_setup_video(void);
extern void wii_video_render(render_target *target, int flag);
extern void wii_shutdown_video(void);
extern int wii_screen_width(void);
#endif

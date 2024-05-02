//============================================================
//
//  Copyright (c) 1996-2009, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//  MAME Wii by Toad King
//
//============================================================

#ifndef _WIIAUDIO_H_
#define _WIIAUDIO_H_
#include "mame.h"

extern void wii_init_audio(running_machine *machine);
extern void wii_setup_audio(void);
extern void wii_shutdown_audio(void);
extern void wii_audio_cleanup(running_machine *machine);
#endif

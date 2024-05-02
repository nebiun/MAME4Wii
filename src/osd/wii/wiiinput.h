//============================================================
//
//  Copyright (c) 1996-2009, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//  MAME Wii by Toad King
//
//============================================================
#ifndef _WIIINPUT_H_
#define _WIIINPUT_H_

#include "mame.h"

extern void wii_init_input(running_machine *machine);
extern void wii_shutdown_input(void);
extern void wii_update_input(void);
#endif

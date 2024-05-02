//============================================================
//
//  Copyright (c) 1996-2009, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//  MAME Wii by Toad King
//
//============================================================
#include "osdcore.h"
#include <ogcsys.h>

//============================================================
//  osd_alloc_executable
//============================================================
void *osd_alloc_executable(size_t size)
{
#ifdef WIIMISC_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif
	// to use this version of the code, we have to assume that
	// code injected into a malloc'ed region can be safely executed
	return malloc(size);
}

//============================================================
//  osd_free_executable
//============================================================
void osd_free_executable(void *ptr, size_t size)
{
#ifdef WIIMISC_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif
	free(ptr);
}

//============================================================
//  osd_break_into_debugger
//============================================================
void osd_break_into_debugger(const char *message)
{
#ifdef WIIMISC_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif
	// there is no standard way to do this, so ignore it
}

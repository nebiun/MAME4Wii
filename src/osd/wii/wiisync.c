//============================================================
//
//  Copyright (c) 1996-2009, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//  MAME Wii by Toad King
//
//============================================================
#include "osdcore.h"
#include "wiimame.h"
#include <ogcsys.h>

struct _osd_lock {
	mutex_t lock;
};

//============================================================
//  osd_lock_alloc
//============================================================
osd_lock *osd_lock_alloc(void)
{
	osd_lock *lock;
#ifdef WIISYNC_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif	
	lock = (osd_lock *)wii_calloc(sizeof(*lock));

	LWP_MutexInit(&(lock->lock), 0);
	return lock;
}

//============================================================
//  osd_lock_acquire
//============================================================
void osd_lock_acquire(osd_lock *lock)
{
#ifdef WIISYNC_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif	
	LWP_MutexLock(lock->lock);
}

//============================================================
//  osd_lock_try
//============================================================
int osd_lock_try(osd_lock *lock)
{
#ifdef WIISYNC_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif	
	return (LWP_MutexTryLock(lock->lock) == 0) ? TRUE : FALSE;
}

//============================================================
//  osd_lock_release
//============================================================
void osd_lock_release(osd_lock *lock)
{
#ifdef WIISYNC_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif	
	LWP_MutexUnlock(lock->lock);
}

//============================================================
//  osd_lock_free
//============================================================
void osd_lock_free(osd_lock *lock)
{
#ifdef WIISYNC_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif	
	LWP_MutexDestroy(lock->lock);
	free(lock);
	lock = 0;
}

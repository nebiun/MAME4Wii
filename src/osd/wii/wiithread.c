//============================================================
//
//  sdlsync.c - SDL core synchronization functions
//
//  Copyright (c) 1996-2007, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//  SDLMAME by Olivier Galibert and R. Belmont
//
//============================================================

// standard C headers
#include <unistd.h>
#include <time.h>
#include <ogcsys.h>
#include <ogc/cond.h>

// MAME headers
#include "osdcore.h"
#include "wiimame.h"
#include "wiithread.h"

typedef struct _hidden_mutex_t hidden_mutex_t;
struct _hidden_mutex_t {
	mutex_t			id;
	volatile INT32		locked;
	volatile INT32		threadid;
};
 
struct _osd_event {
	mutex_t		 	mutex;
	cond_t	 		cond;
	volatile INT32		autoreset;
	volatile INT32		signalled;
};

//============================================================
//  TYPE DEFINITIONS
//============================================================
struct _osd_thread {
	lwp_t		thread;
	osd_thread_callback callback;
	void *param;
	void *stack;
};

struct _osd_scalable_lock
{
	mutex_t		 	mutex;
};

//============================================================
//  Scalable Locks
//============================================================
osd_scalable_lock *osd_scalable_lock_alloc(void)
{
	osd_scalable_lock *lock;
#ifdef WIITHREAD_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif		
	lock = (osd_scalable_lock *)wii_calloc(sizeof(*lock));

	LWP_MutexInit(&(lock->mutex), 1);
	return lock;
}

INT32 osd_scalable_lock_acquire(osd_scalable_lock *lock)
{
#ifdef WIITHREAD_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif	
	LWP_MutexLock(lock->mutex);
	return 0;
}

void osd_scalable_lock_release(osd_scalable_lock *lock, INT32 myslot)
{
#ifdef WIITHREAD_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif	
	LWP_MutexUnlock(lock->mutex);
}

void osd_scalable_lock_free(osd_scalable_lock *lock)
{
#ifdef WIITHREAD_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif	
	LWP_MutexDestroy(lock->mutex);
	free(lock);
}

//============================================================
//  osd_event_alloc
//============================================================
osd_event *osd_event_alloc(int manualreset, int initialstate)
{
	osd_event *ev;
#ifdef WIITHREAD_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif
	ev = (osd_event *)wii_calloc(sizeof(osd_event));

	LWP_MutexInit(&(ev->mutex), 0);
	LWP_CondInit(&(ev->cond));
	ev->signalled = initialstate;
	ev->autoreset = !manualreset;
		
	return ev;
}

//============================================================
//  osd_event_free
//============================================================
void osd_event_free(osd_event *event)
{
#ifdef WIITHREAD_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif	
	LWP_MutexDestroy(event->mutex);
	LWP_CondDestroy(event->cond);
	free(event);
}

//============================================================
//  osd_event_set
//============================================================
void osd_event_set(osd_event *event)
{
#ifdef WIITHREAD_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif	
	LWP_MutexLock(event->mutex);
	if (event->signalled == FALSE)
	{
		event->signalled = TRUE;
		if (event->autoreset) 
		{
			LWP_CondSignal(event->cond);
		}
		else 
		{
			LWP_CondBroadcast(event->cond);
		}
	}
	LWP_MutexUnlock(event->mutex);
}

//============================================================
//  osd_event_reset
//============================================================
void osd_event_reset(osd_event *event)
{
#ifdef WIITHREAD_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif	
	LWP_MutexLock(event->mutex);
	event->signalled = FALSE;
	LWP_MutexUnlock(event->mutex);
}

//============================================================
//  osd_event_wait
//============================================================
int osd_event_wait(osd_event *event, osd_ticks_t timeout)
{
	struct timespec now;
	struct timespec abstime;
#ifdef WIITHREAD_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif	

	LWP_MutexLock(event->mutex);
	if (!timeout) 
	{
		if (!event->signalled) 
		{
			LWP_MutexUnlock(event->mutex);
				return FALSE;
		}
	}
	else 
	{
		if (!event->signalled) 
		{
			UINT64 msec = (timeout * 1000) / osd_ticks_per_second();
			
			do {
				int ret;
				clock_gettime(CLOCK_REALTIME, &now);

				abstime.tv_sec = now.tv_sec + (msec / 1000);
				abstime.tv_nsec = (now.tv_nsec + (msec % 1000) * 1000) * 1000;

				if (abstime.tv_nsec > 1000000000) 
				{
					abstime.tv_sec += 1;
					abstime.tv_nsec -= 1000000000;
				}

				ret = LWP_CondTimedWait(event->cond, event->mutex, &abstime);

				if (ret != 0) 
				{
					if (!event->signalled) 
					{
						LWP_MutexUnlock(event->mutex);
						return FALSE;
					}
					else 
					{
						break;
					}
				}
				if (ret == 0) 
				{
					break;
				}
				wii_debug("%s: Error %d while waiting for pthread_cond_timedwait:  %s\n", __FUNCTION__, 
					ret, strerror(ret));
			} while (TRUE);
		}
	}

	if (event->autoreset)
		event->signalled = 0;

	LWP_MutexUnlock(event->mutex);

	return TRUE;
}

//============================================================
//  osd_thread_create
//============================================================
static void *worker_thread_entry(void *param)
{
	osd_thread *thread = (osd_thread *)param;
	void *res;
	res = thread->callback(thread->param);
	return res;
}

osd_thread *osd_thread_create(osd_thread_callback callback, void *cbparam)
{
	osd_thread *thread;
#ifdef WIITHREAD_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif
	thread = (osd_thread *)wii_calloc(sizeof(osd_thread));
	thread->callback = callback;
	thread->param = cbparam;
	thread->stack = wii_calloc(STACKSIZE);
	if (LWP_CreateThread(&(thread->thread), worker_thread_entry, thread, thread->stack, STACKSIZE, OTHERTH_PRIORITY) < 0) 
	{
		free(thread->stack);
		free(thread);
		return NULL;
	}
	return thread;
}

//============================================================
//  osd_thread_adjust_priority
//============================================================
int osd_thread_adjust_priority(osd_thread *thread, int adjust)
{
#ifdef WIITHREAD_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif	
	return TRUE;
}

//============================================================
//  osd_thread_cpu_affinity
//============================================================
int osd_thread_cpu_affinity(osd_thread *thread, UINT32 mask)
{
#ifdef WIITHREAD_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif	
	return TRUE;
}

//============================================================
//  osd_thread_wait_free
//============================================================
void osd_thread_wait_free(osd_thread *thread)
{
	void *status;
#ifdef WIITHREAD_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif	
	LWP_JoinThread(thread->thread, &status);
	free(thread->stack);
	free(thread);
}

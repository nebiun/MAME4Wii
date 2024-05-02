//============================================================
//
//  Copyright (c) 1996-2009, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//  MAME Wii by Toad King
//
//============================================================
#include "osdepend.h"
#include "wiiaudio.h"
#include <gccore.h>
#include <ogcsys.h>
#include <gctypes.h>
#include "mame.h"
#include "wiimame.h"

#define CHUNK_SIZE	32
#define SZ_AUDIO_BUFFER	(CHUNK_SIZE*1000)

static char data[SZ_AUDIO_BUFFER] __attribute__((__aligned__(32)));
static char *w_pointer;
static char *r_pointer;
static bool dma_busy = false;
static bool wii_stopping = false;
static lwpq_t audioqueue;
static lwp_t audiothread = LWP_THREAD_NULL;
static bool mute = true;

static void _audio_clean(void)
{
	AUDIO_StopDMA();
	AUDIO_RegisterDMACallback(NULL);
	dma_busy = false;
	w_pointer = data;
	r_pointer = data;
	memset(data,0,sizeof(data));
}

static void dma_callback(void)
{
	LWP_ThreadSignal(audioqueue);
}

static void *wii_audio_thread(void *arg)
{
	wii_debug("%s: Audio thread started\n",__FUNCTION__);
	
	while (!wii_stopping)
	{	
		int len;

		LWP_ThreadSleep(audioqueue);
		
		len = w_pointer - r_pointer;
		if(len < 0)
			len = &data[SZ_AUDIO_BUFFER] - r_pointer;
	   
		if(len >= CHUNK_SIZE) {
			dma_busy = true;
			len -= len%CHUNK_SIZE;
//			wii_debug("%s - %p (%d)\n",__FUNCTION__,r_pointer,len);
			DCFlushRange(r_pointer, len);
			AUDIO_InitDMA((uint32_t)r_pointer, len);
			AUDIO_StartDMA();
			
			if( (r_pointer + len) >= &data[SZ_AUDIO_BUFFER])
				r_pointer = data;
			else
				r_pointer += len;	
		}
		else {
			dma_busy = false;
		}
	}
	wii_debug("%s: Audio thread finished\n",__FUNCTION__);
	return (void *)0;	
}

/* Wii uses silly R, L, R, L interleaving. */
static inline void copy_swapped(uint32_t *dst, const uint32_t *src, size_t size)
{
	do
	{
		uint32_t s = *src++;
		*dst++ = (s >> 16) | (s << 16);
	} while (--size);
}

void wii_shutdown_audio(void)
{
	wii_debug("%s: start\n",__FUNCTION__);

	if(audiothread != LWP_THREAD_NULL) {
		void *status;
		
		wii_stopping = true;
		LWP_ThreadSignal(audioqueue);
		LWP_JoinThread(audiothread, &status);
		LWP_CloseQueue(audioqueue);
		audiothread = LWP_THREAD_NULL;
		wii_debug("%s: audio thread turned off\n",__FUNCTION__);
	}
	_audio_clean();
	wii_debug("%s: end\n",__FUNCTION__);
}

void wii_audio_cleanup(running_machine *machine)
{
	wii_debug("%s: thread %s machine %p\n",__FUNCTION__, (audiothread != LWP_THREAD_NULL) ? "ON" : "OFF", machine);

	if(audiothread != LWP_THREAD_NULL) {
		_audio_clean();
	}
}

void wii_init_audio(running_machine *machine)
{
	wii_debug("%s: start %p\n",__FUNCTION__, machine);
	_audio_clean();
	
	add_exit_callback(machine, wii_audio_cleanup);
	AUDIO_Init(NULL);
	AUDIO_RegisterDMACallback(dma_callback);

	//ranges 0-32000 (default low) and 40000-47999 (in settings going down from 48000) -> set to 32000 hz
	if (machine->sample_rate <= 32000 || (machine->sample_rate >= 40000 && machine->sample_rate < 48000))
	{
		AUDIO_SetDSPSampleRate(AI_SAMPLERATE_32KHZ);
	}
	else //ranges 32001-39999 (in settings going up from 32000) and 48000-max (default high) -> set to 48000 hz
	{
		AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);
	}
	wii_stopping = false;
	
	wii_debug("%s: end %p\n",__FUNCTION__, machine);
}

void wii_setup_audio(void)
{
	if(audiothread == LWP_THREAD_NULL) {
		LWP_InitQueue(&audioqueue);
		LWP_CreateThread(&audiothread, wii_audio_thread, NULL, NULL, 0, AUDIOTH_PRIORITY);
	}
}

void osd_set_mastervolume(int attenuation)
{
//#ifdef WIIAUDIO_DEBUG	
	wii_debug("%s: Start %d\n",__FUNCTION__, attenuation);
//#endif
	if(attenuation == -32) {
		_audio_clean();
		mute = true;
	}
	else {
		mute = false;
	}
}

void osd_update_audio_stream(running_machine *machine, INT16 *buffer, int samples_this_frame)
{
	size_t frames = samples_this_frame;
	const uint32_t *buf = (uint32_t *)buffer;

	if(mute == true)
		return;
	
	while(frames)
	{
		int len;
		char *tmp_r;
		
		tmp_r = r_pointer;
		if(tmp_r > w_pointer)
			len = (tmp_r - w_pointer)/sizeof(uint32_t);
		else
			len = (&data[SZ_AUDIO_BUFFER] - w_pointer)/sizeof(uint32_t);
		
		if(len > frames)
			len = frames;
		
		copy_swapped((uint32_t *)w_pointer, buf, len);
		w_pointer += (len*sizeof(uint32_t));
		if(w_pointer >= &data[SZ_AUDIO_BUFFER])
			w_pointer = data;
		
		frames -= len;
		buf += len;
			
		if(dma_busy == false) {
			LWP_ThreadSignal(audioqueue);
		}
	}
}

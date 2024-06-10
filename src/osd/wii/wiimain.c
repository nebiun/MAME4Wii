//============================================================
//
//  Copyright (c) 1996-2009, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//  MAME Wii by Toad King
//
//============================================================

#include <ogcsys.h>
#include <wiiuse/wpad.h>
#include <fat.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <limits.h>
#include "wiimame.h"
#include "wiiaudio.h"
#include "wiiinput.h"
#include "wiivideo.h"
#include "osdepend.h"
#include "render.h"
#include "clifront.h"
#include "mame.h"
#include "emuopts.h"
#include "options.h"
#include "run_dol.h"

//============================================================
//  CONSTANTS
//============================================================
#define MAME4WII_LOGDIR		"logs"
#define MAME4WII_CRASHDIR	"crash"

//============================================================
//  GLOBALS
//============================================================
extern const char build_version[];

// a single rendering target
static render_target *our_target;

// the state of each key
//static UINT8 keyboard_state[KEY_TOTAL];

static running_machine *current_machine = NULL;
static BOOL shutting_down = FALSE;

#ifdef OSD_MEMORY_BUFFERS
typedef struct {
	void *addr;
	void **user_addr;
	size_t sz;
} wii_buffer_t;

#define N_MEMORY_BUFFER		64
static wii_buffer_t memory_buffers[N_MEMORY_BUFFER];
#endif

static const options_entry wii_mame_options[] =
{
	{ "initpath", ".;/mame4wii", 0, "path to ini files" },
	{ NULL, NULL, OPTION_HEADER, "WII OPTIONS" },
	{ "safearea(0.01-1)", "1.0", 0, "Adjust video for safe areas on older TVs (.9 or .85 are usually good values)" },
	{ NULL }
};

/* Wrappers */
void _wii_free(void *addr, const char *src, int line)
{
	if(addr != NULL) {
	//	wii_debug("%s: addr %p [%s:%d]\n",__FUNCTION__,addr,src,line);
		free(addr);
	}
}

void *_wii_malloc(size_t size, const char *src, int line)
{
	void *addr;
	
	if( (addr = malloc(size)) == NULL) {
		wii_debug("%s failed: size %zd [%s:%d]\n",__FUNCTION__,size,src,line);
		return NULL;
	}
//	wii_debug("%s: addr %p size %zd [%s:%d]\n",__FUNCTION__,addr,size,src,line);
	return addr;
}

void *_wii_realloc(void *ptr, size_t size, const char *src, int line)
{
	void *addr;
	
	if( (addr = realloc(ptr, size)) == NULL) {
		wii_debug("%s failed: ptr %p size %zd [%s:%d]\n",__FUNCTION__,ptr,size,src,line);
		return NULL;
	}
//	wii_debug("%s: addr %p size %zd [%s:%d]\n",__FUNCTION__,addr,size,src,line);
	return addr;
}

void *_wii_calloc(size_t nelem, size_t elsize, const char *src, int line)
{
	void *addr;
	
	if( (addr = calloc(nelem, elsize)) == NULL) {
		wii_debug("%s failed: nelem %zd elsize %zd (tot %zd)[%s:%d]\n",__FUNCTION__,
			nelem, elsize, nelem * elsize, src,line);
		return NULL;
	}
//	wii_debug("%s: addr %p size %zd [%s:%d]\n",__FUNCTION__,addr,nelem * elsize,src,line);
	return addr;	
}

/* End wrappers */

void *wii_calloc(size_t n)
{
	void *ptr;
	size_t m;
	
	m = wiiWordRound(n);
	ptr = memalign(32,m);
	if(ptr != NULL) 
	{
		memset(ptr,0,m);
	}
	return ptr;
}

void wii_debug(const char *format, ...)
{
	static const char *debug_file = MAME4WII_HOME"/"MAME4WII_LOGDIR"/mame4wii.log";
	static int first;
	static sem_t sem;
	va_list aptr;
	FILE *fp;

	if(first == 0) {
		fp = fopen(debug_file, "w+");
		first++;
		LWP_SemInit(&sem,1,1); 
	}
	else {
		fp = fopen(debug_file, "a+");
	}
	
	if(fp != NULL) {
		struct timeval tt;
		
		LWP_SemWait(sem);
		gettimeofday(&tt, NULL);
		fprintf(fp, "%lu.%06lu:", (unsigned long)tt.tv_sec, (unsigned long)tt.tv_usec);
		
		va_start(aptr, format);
		vfprintf(fp, format, aptr);
		va_end(aptr);
		fclose(fp);
		LWP_SemPost(sem);
	}
}

#ifdef OSD_MEMORY_BUFFERS
void *osd_get_memory_buffer(void **addr, size_t n)
{
	wii_buffer_t *new_addr = NULL;
	
	if(*addr != NULL) {
		for(wii_buffer_t *p=&memory_buffers[0]; p<&memory_buffers[N_MEMORY_BUFFER]; p++) {
			if((p->addr == *addr) && (p->sz == n)) {
				return p->addr;
			}
			if((new_addr == NULL) && (p->addr == NULL)) {
				new_addr = p;
			}
		}
	}
	else {
		for(wii_buffer_t *p=&memory_buffers[0]; p<&memory_buffers[N_MEMORY_BUFFER]; p++) {
			if(p->addr == NULL) {
				new_addr = p;
				break;
			}
		}
	}
	
	if(new_addr == NULL) {
		wii_debug("%s: no more buffers free\n",__FUNCTION__);
		return NULL;
	}

	new_addr->addr = wii_calloc(n);
	if(new_addr->addr == NULL) {
		wii_debug("%s: malloc failed (%d bytes)\n",__FUNCTION__, n);
		return NULL;
	}
			
	new_addr->user_addr = addr; 
	new_addr->sz = n;
	return new_addr->addr;
}

void wii_clean_memory_buffers(running_machine *machine)
{
//	wii_debug("%s: Start\n",__FUNCTION__);
	for(wii_buffer_t *p=&memory_buffers[0]; p<&memory_buffers[N_MEMORY_BUFFER]; p++) {
		if(p->addr != NULL) {
			free(p->addr);
		//	wii_debug("%s: free %p (%d)\n",__FUNCTION__,p->addr, p->sz );
			p->addr = NULL;
			*(p->user_addr) = NULL;
		}
	}
//	wii_debug("%s: end\n",__FUNCTION__);
}
#endif

void wii_printf(const char *format, ...)
{
	static int first=0;
	static const char *debug_file = MAME4WII_HOME"/"MAME4WII_LOGDIR"/mame4wii_info.txt";
	va_list aptr;
	FILE *fp;

	if(first == 0) {
		fp = fopen(debug_file, "wb");
		if(fp != NULL) {
			first++;
		}
	}
	else {
		fp = fopen(debug_file, "a+b");
	}
	
	if(fp != NULL) {	
		va_start(aptr, format);
		vfprintf(fp, format, aptr);
		va_end(aptr);
		fclose(fp);
	}
}

static void wii_shutdown(void)
{
	if (current_machine != NULL) {
		// mame_schedule_exit only returns to the game select screen if done in
		// game, so fake being out of agame before doing it
		options_set_string(mame_options(), OPTION_GAMENAME, "", OPTION_PRIORITY_CMDLINE);
		mame_schedule_exit(current_machine);	
	}
	shutting_down = TRUE;
}

static void wii_reset(unsigned int v,  void *p)
{
#ifdef WIIMAIN_DEBUG	
	wii_debug("%s: Start %p\n",__FUNCTION__, current_machine);
#endif
	if (current_machine != NULL)
		mame_schedule_exit(current_machine);
}

//============================================================
//  main
//============================================================
int main(int argc, char *argv[]) 
{
	static const char *vv="MAME4Wii v.%s\n";
	const char *args[5] = { NULL };
	int ret;
	const char *pwd = MAME4WII_HOME;

	if(argc == 0) {
		argv[0] = (char *)"";
		argc = 1;
	}
	
	L2Enhance();
	VIDEO_Init();
	WPAD_Init();
	PAD_Init();
	
	WPAD_SetPowerButtonCallback((WPADShutdownCallback) wii_shutdown);
	SYS_SetPowerCallback(wii_shutdown);
	SYS_SetResetCallback(wii_reset);

	fatInitDefault();
	if(argc == 1) {
		if (chdir(MAME4WII_HOMEUSB) == 0) {
			pwd = MAME4WII_HOMEUSB;
		}
		else {
			chdir(MAME4WII_HOME);
		}
	}
	else {
		char rom_path[PATH_MAX];
		
		// Check rom
		snprintf(rom_path,sizeof(rom_path),"%s/roms/%s.zip",MAME4WII_HOMEUSB,argv[1]);
		if(access(rom_path, R_OK) == 0) {
			pwd = MAME4WII_HOMEUSB;
		}
		chdir(pwd);
	}
	wii_debug(vv, build_version);
	wii_printf(vv, build_version);

	wii_setup_video();
	wii_setup_audio();
	wii_debug("%s: Enter main loop (argc=%d)\n", __FUNCTION__,argc);
	
	cli_info_listfull(NULL, "*");

	// cli_execute does the heavy lifting; if we have osd-specific options, we
	// would pass them as the third parameter here
	ret = cli_execute(argc, argv, wii_mame_options);
	wii_debug("%s: cli_execute %s rtn %d\n", __FUNCTION__, (argc > 1) ? argv[1] : "<none>", ret);
	
	wii_shutdown_input();
	wii_shutdown_audio();
	wii_shutdown_video();

	if(argc > 1) {
		char tmp_path[PATH_MAX];
		
		snprintf(tmp_path, sizeof(tmp_path), "%s/%s/%s", pwd, MAME4WII_CRASHDIR, argv[1]);
		if(ret != 0) {
			FILE *fp;
			if( (fp = fopen(tmp_path,"w")) != NULL) {
				const char *p;
				
				switch(ret) {
				case MAMERR_NONE:
					p = "no error";
					break;
				case MAMERR_FAILED_VALIDITY:
					p = "failed validity checks";
					break;
				case MAMERR_MISSING_FILES:
					p = "missing files";
					break;
				case MAMERR_FATALERROR:
					p = "some other fatal error";
					break;
				case MAMERR_DEVICE:
					p = "device initialization error (MESS-specific)";
					break;
				case MAMERR_NO_SUCH_GAME:
					p = "game was specified but doesn't exist";
					break;
				case MAMERR_INVALID_CONFIG:
					p = "some sort of error in configuration";
					break;
				case MAMERR_IDENT_NONROMS:
					p = "identified all non-ROM files";
					break;
				case MAMERR_IDENT_PARTIAL:
					p = "identified some files but not all";
					break;
				case MAMERR_IDENT_NONE:
					p = "identified no files";
					break;
				default:
					p = "<unknown error>";
					break;
				}
				
				fprintf(fp,"%s\n",build_version);
				fprintf(fp,"Crashed:%s (%d)\n",p, ret);
				fclose(fp);
			}
		}
		else {
			unlink(tmp_path);
		}
				
		args[0] = "loader";
		args[1] = "sd:/apps/mame4wii/boot.dol";
		args[2] = "menu";
		args[3] = argv[1];

		snprintf(tmp_path, sizeof(tmp_path), "%s/libs/loader.dol", pwd);
		
		ret = runDOL (tmp_path, 4, args);
		wii_debug("%s: runDOL %s rtn %d\n", __FUNCTION__, args[1], ret);
	}
	return 0;	/* Dovrebbe ritornare al menu` */
}

//============================================================
//  osd_init
//============================================================
void osd_init(running_machine *machine)
{
#ifdef WIIMAIN_DEBUG	
	wii_debug("%s: Start %p\n",__FUNCTION__, machine);
#endif
	current_machine = machine;
	our_target = render_target_alloc(machine, NULL, 0);

	if (our_target == NULL)
		fatalerror("Error creating render target");

	wii_init_dimensions();
	wii_init_input(machine);
	wii_init_audio(machine);
	wii_init_video(machine);
#ifdef OSD_MEMORY_BUFFERS
	add_exit_callback(machine, wii_clean_memory_buffers);
#endif
}

//============================================================
//  osd_wait_for_debugger
//============================================================
void osd_wait_for_debugger(const device_config *device, int firststop)
{
#ifdef WIIMAIN_DEBUG	
	wii_debug("%s: Start\n",__FUNCTION__);
#endif
	// we don't have a debugger, so we just return here
}

//============================================================
//  osd_update
//============================================================
void osd_update(running_machine *machine, int skip_redraw)
{
#ifdef WIIMAIN_DEBUG	
	wii_debug("%s: Start %p\n",__FUNCTION__, machine);
#endif
	wii_update_input();
	wii_video_render(our_target, skip_redraw);
}

//============================================================
//  osd_customize_input_type_list
//============================================================
void osd_customize_input_type_list(input_type_desc *typelist)
{
	// This function is called on startup, before reading the
	// configuration from disk. Scan the list, and change the
	// default control mappings you want. It is quite possible
	// you won't need to change a thing.
#ifdef WIIMAIN_DEBUG	
	wii_debug("%s: Start\n",__FUNCTION__);
#endif	
	input_type_desc *typedesc;
    // loop over the defaults
    for (typedesc = typelist; typedesc != NULL; typedesc = typedesc->next)
	{		
		int i;
#if 0		
		wii_debug("PRIMA: type=%d group=%d player=%d token=%s name=%s seq:", __FUNCTION__,
			typedesc->type, typedesc->group, typedesc->player, typedesc->token, typedesc->name);
		for(i=0; i< MAX_INPUT_CODE_IDX; i++) {
			int c=typedesc->seq[SEQ_TYPE_STANDARD].code[i];
			
			if( c == INTERNAL_CODE(0)) 
			{
				wii_debug("\n");
				break;
			}
			wii_debug("(%c d=%d, x=%d, i=%d, m=%d, o=%d)", INPUT_CODE_IS_INTERNAL(c) ? '!' : '*',
				INPUT_CODE_DEVCLASS(c), INPUT_CODE_DEVINDEX(c),  INPUT_CODE_ITEMCLASS(c), INPUT_CODE_MODIFIER(c), INPUT_CODE_ITEMID(c));  
		}		
#endif 		
		i=0;
		switch(typedesc->type) 
		{
		case IPT_JOYSTICK_UP:
		case IPT_JOYSTICKRIGHT_UP:
		case IPT_JOYSTICKLEFT_UP:
			switch(typedesc->player)
			{
			case 0:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_HAT1UP);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INTERNAL_CODE(3);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_YAXIS);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			case 1:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 1, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_HAT1UP);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INTERNAL_CODE(3);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 1, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_YAXIS);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			case 2:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 2, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_HAT1UP);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INTERNAL_CODE(3);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 2, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_YAXIS);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			case 3:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 3, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_HAT1UP);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INTERNAL_CODE(3);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 3, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_YAXIS);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			}
			break;
		case IPT_JOYSTICK_DOWN:
		case IPT_JOYSTICKRIGHT_DOWN:
		case IPT_JOYSTICKLEFT_DOWN:
			switch(typedesc->player)
			{
			case 0:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_HAT1DOWN);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INTERNAL_CODE(3);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_YAXIS);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			case 1:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 1, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_HAT1DOWN);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INTERNAL_CODE(3);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 1, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_YAXIS);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			case 2:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 2, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_HAT1DOWN);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INTERNAL_CODE(3);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 2, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_YAXIS);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			case 3:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 3, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_HAT1DOWN);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INTERNAL_CODE(3);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 3, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_YAXIS);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			}		
			break;
		case IPT_JOYSTICK_LEFT:
		case IPT_JOYSTICKRIGHT_LEFT:
		case IPT_JOYSTICKLEFT_LEFT:
			switch(typedesc->player)
			{
			case 0:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_HAT1LEFT);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INTERNAL_CODE(3);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_XAXIS);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			case 1:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 1, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_HAT1LEFT);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INTERNAL_CODE(3);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 1, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_XAXIS);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			case 2:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 2, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_HAT1LEFT);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INTERNAL_CODE(3);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 2, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_XAXIS);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			case 3:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 3, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_HAT1LEFT);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INTERNAL_CODE(3);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 3, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_XAXIS);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			}		
			break;		
		case IPT_JOYSTICK_RIGHT:
		case IPT_JOYSTICKRIGHT_RIGHT:
		case IPT_JOYSTICKLEFT_RIGHT:
			switch(typedesc->player)
			{
			case 0:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_HAT1RIGHT);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INTERNAL_CODE(3);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_XAXIS);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			case 1:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 1, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_HAT1RIGHT);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INTERNAL_CODE(3);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 1, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_XAXIS);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			case 2:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 2, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_HAT1RIGHT);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INTERNAL_CODE(3);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 2, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_XAXIS);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			case 3:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 3, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_HAT1RIGHT);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INTERNAL_CODE(3);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 3, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_XAXIS);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			}		
			break;
		case IPT_BUTTON1:
			switch(typedesc->player)
			{
			case 0:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON1);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			case 1:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 1, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON1);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			case 2:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 2, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON1);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			case 3:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 3, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON1);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			}		
			break;
		case IPT_BUTTON2:
			switch(typedesc->player)
			{
			case 0:	
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON3);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			case 1:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 1, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON3);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			case 2:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 2, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON3);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			case 3:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 3, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON3);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			}		
			break;
		case IPT_BUTTON3:
			switch(typedesc->player)
			{
			case 0:	
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON2);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			case 1:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 1, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON2);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			case 2:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 2, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON2);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			case 3:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 3, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON2);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			}		
			break;
		case IPT_BUTTON4:
			switch(typedesc->player)
			{
			case 0:	
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON4);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			case 1:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 1, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON4);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			case 2:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 2, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON4);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			case 3:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 3, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON4);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			}		
			break;
		case IPT_START1:
			switch(typedesc->player)
			{
			case 0:	
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON8);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			}
			break;
		case IPT_START2:
			switch(typedesc->player)
			{
			case 0:	
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 1, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON8);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			}
			break;
		case IPT_START3:
			switch(typedesc->player)
			{
			case 0:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 2, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON8);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			}
			break;	
		case IPT_START4:
			switch(typedesc->player)
			{
			case 0:	
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 3, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON8);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			}
			break;
		case IPT_COIN1:
			switch(typedesc->player)
			{
			case 0:	
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON5);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			}
			break;	
		case IPT_COIN2:
			switch(typedesc->player)
			{
			case 0:	
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 1, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON5);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			}
			break;		
		case IPT_COIN3:
			switch(typedesc->player)
			{
			case 0:	
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 2, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON5);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			}
			break;
		case IPT_COIN4:
			switch(typedesc->player)
			{
			case 0:	
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 3, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON5);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			}
			break;
		case IPT_UI_CONFIGURE:
			switch(typedesc->player)
			{
			case 0:	
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON6);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			}
			break;	
		case IPT_UI_UP:
			switch(typedesc->player)
			{
			case 0:	
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_HAT1UP);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INTERNAL_CODE(3);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_YAXIS);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			}
			break;
		case IPT_UI_DOWN:
			switch(typedesc->player)
			{
			case 0:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_HAT1DOWN);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INTERNAL_CODE(3);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_YAXIS);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			}
			break;
		case IPT_UI_LEFT:
			switch(typedesc->player)
			{
			case 0:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_HAT1LEFT);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INTERNAL_CODE(3);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_XAXIS);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			}
			break;	
		case IPT_UI_RIGHT:
			switch(typedesc->player)
			{
			case 0:	
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_HAT1RIGHT);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INTERNAL_CODE(3);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_XAXIS);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			}
			break;		
		case IPT_UI_PAGE_UP:
			switch(typedesc->player)
			{
			case 0:	
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON10);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			}
			break;	
		case IPT_UI_PAGE_DOWN:
			switch(typedesc->player)
			{
			case 0:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON11);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			}
			break;
		case IPT_UI_SELECT:
			switch(typedesc->player)
			{
			case 0:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON1);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			}
			break;	
		case IPT_UI_CANCEL:
			switch(typedesc->player)
			{
			case 0:
				typedesc->seq[SEQ_TYPE_STANDARD].code[i++] = INPUT_CODE( DEVICE_CLASS_JOYSTICK, 0, ITEM_CLASS_SWITCH, ITEM_MODIFIER_NONE, ITEM_ID_BUTTON7);
				typedesc->seq[SEQ_TYPE_STANDARD].code[i] = INTERNAL_CODE(0);
				break;
			}
			break;		
		}
#if 0		
		wii_debug("DOPO : type=%d group=%d player=%d token=%s name=%s seq:", __FUNCTION__,
			typedesc->type, typedesc->group, typedesc->player, typedesc->token, typedesc->name);
		for(i=0; i< MAX_INPUT_CODE_IDX; i++) {
			int c=typedesc->seq[SEQ_TYPE_STANDARD].code[i];
			
			if( c == INTERNAL_CODE(0)) 
			{
				wii_debug("\n");
				break;
			}
			wii_debug("(%c d=%d, x=%d, i=%d, m=%d, o=%d)", INPUT_CODE_IS_INTERNAL(c) ? '!' : '*',
				INPUT_CODE_DEVCLASS(c), INPUT_CODE_DEVINDEX(c),  INPUT_CODE_ITEMCLASS(c), INPUT_CODE_MODIFIER(c), INPUT_CODE_ITEMID(c));  
		}
#endif
	}	
}

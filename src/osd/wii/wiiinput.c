//============================================================
//
//  Copyright (c) 1996-2009, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//  MAME Wii by Toad King
//
//============================================================
#include <wiiuse/wpad.h>
#include <ogc/pad.h>
#include "osdepend.h"
#include "wiiinput.h"
#include "wiivideo.h"
#include "wiimame.h"

#define INPUTTH_ON	1
//#define GAMECUBE_PAD	1
#define NUNCHUK_AS_JOY	1

typedef struct {
	const char *name;
	const input_item_id item_id;
} button_t;			

/*  Gamecube/Wiimote+Nunchuck/Classic  */
static button_t buttons[11] = {
	{ "A/2/A", 				ITEM_ID_BUTTON1 },	// Fire 1
	{ "B/1/B",				ITEM_ID_BUTTON2 },	
	{ "X/B/X",				ITEM_ID_BUTTON3 },	// Fire 2
	{ "Y/A/Y",				ITEM_ID_BUTTON4 },	
	{ "Z/Minus/Minus",		ITEM_ID_BUTTON5 },	// Coin
	{ "L/Home/Home",		ITEM_ID_BUTTON6 },	// Configure
	{ "R/*/R",				ITEM_ID_BUTTON7 },	// Cancel
	{ "Start/Plus/Plus",	ITEM_ID_BUTTON8 },	// Start
	{ "*/*/L",				ITEM_ID_BUTTON9 },
	{ "*/Nunchuck C/ZL",	ITEM_ID_BUTTON10 },
	{ "*/Nunchuck Z/ZR",	ITEM_ID_BUTTON11 }
};

#ifdef GAMECUBE_PAD
static const u32 gamecube_buttons[11] = {
	PAD_BUTTON_A,
	PAD_BUTTON_B,
	PAD_BUTTON_X,
	PAD_BUTTON_Y,
	PAD_TRIGGER_Z,				// Coin
	PAD_TRIGGER_L,				// Configure
	PAD_TRIGGER_R,				// Cancel
	PAD_BUTTON_START,			// Start
	0,
	PAD_BUTTON_START|PAD_TRIGGER_R,	// Page UP
	PAD_BUTTON_START|PAD_TRIGGER_L	// Page DOWN	
};
#define GAMECUBE_BUTTON_MASK	(PAD_BUTTON_A|PAD_BUTTON_B|PAD_BUTTON_X|PAD_BUTTON_Y|PAD_TRIGGER_Z|PAD_TRIGGER_L|PAD_TRIGGER_R|PAD_BUTTON_START)	

static const u32 gamecube_directions[4] = {
	PAD_BUTTON_UP,
	PAD_BUTTON_DOWN,
	PAD_BUTTON_LEFT,
	PAD_BUTTON_RIGHT
};

#endif

static const u32 wiimote_buttons[11] = {
	WPAD_BUTTON_2,				// Fire A
	WPAD_BUTTON_A,				// Fire C
	WPAD_BUTTON_1,				// Fire B
	WPAD_BUTTON_MINUS,			// Fire D
	WPAD_BUTTON_B,				// Coin
	WPAD_BUTTON_B|WPAD_BUTTON_HOME,	// Configure
	WPAD_BUTTON_HOME,			// Return to menu
	WPAD_BUTTON_PLUS,			// Start
	0,
	WPAD_NUNCHUK_BUTTON_C,		// Page UP
	WPAD_NUNCHUK_BUTTON_Z		// Page DOWN
};
#define WIIMOTE_BUTTON_MASK		(WPAD_BUTTON_2|WPAD_BUTTON_1|WPAD_BUTTON_B|WPAD_BUTTON_A|WPAD_BUTTON_MINUS|WPAD_BUTTON_HOME|WPAD_BUTTON_PLUS|WPAD_NUNCHUK_BUTTON_C|WPAD_NUNCHUK_BUTTON_Z)	

/* wiimote works horizontaly, than directions are scrambled */
static const u32 wiimote_directions[4] = {
	WPAD_BUTTON_RIGHT,		// Up
	WPAD_BUTTON_LEFT,		// Down
	WPAD_BUTTON_UP,			// Left
	WPAD_BUTTON_DOWN		// Right
};

static const u32 classic_buttons[11] = {
	WPAD_CLASSIC_BUTTON_A,
	WPAD_CLASSIC_BUTTON_B,
	WPAD_CLASSIC_BUTTON_X,
	WPAD_CLASSIC_BUTTON_Y,	
	WPAD_CLASSIC_BUTTON_MINUS,	// Coin
	WPAD_CLASSIC_BUTTON_HOME,	// Configure
	WPAD_CLASSIC_BUTTON_FULL_R,	// Cancel
	WPAD_CLASSIC_BUTTON_PLUS,	// Start
	WPAD_CLASSIC_BUTTON_FULL_L,
	WPAD_CLASSIC_BUTTON_ZR,		// Page UP
	WPAD_CLASSIC_BUTTON_ZL		// Page DOWN
};
#define CLASSIC_BUTTON_MASK		(WPAD_CLASSIC_BUTTON_A|WPAD_CLASSIC_BUTTON_B|WPAD_CLASSIC_BUTTON_X|WPAD_CLASSIC_BUTTON_Y|WPAD_CLASSIC_BUTTON_MINUS|WPAD_CLASSIC_BUTTON_HOME|WPAD_CLASSIC_BUTTON_FULL_R|WPAD_CLASSIC_BUTTON_PLUS|WPAD_CLASSIC_BUTTON_FULL_L|WPAD_CLASSIC_BUTTON_ZL|WPAD_CLASSIC_BUTTON_ZR)

static const u32 classic_directions[4] = {
	WPAD_CLASSIC_BUTTON_UP,
	WPAD_CLASSIC_BUTTON_DOWN,
	WPAD_CLASSIC_BUTTON_LEFT,
	WPAD_CLASSIC_BUTTON_RIGHT
};

typedef struct {
	INT32 buttons[11];
	INT32 hats[4];			/* UP, DOWN, LEFT, RIGHT */
	INT32 axis[6];
	INT32 pointer[2];		/* x, y */
} wii_pad_t;

static wii_pad_t pad[4];
#ifdef INPUTTH_ON
static lwp_t inpthread = LWP_THREAD_NULL;
static BOOL wii_stopping = false;
static lwpq_t inpqueue;;
#endif

static inline INT32 joypad_get_state(void *device_internal, void *item_internal)
{
	return *((INT32 *)item_internal);
}

static void *wii_poll_input(void *arg)
{
#ifdef INPUTTH_ON
	wii_debug("%s: Input thread started\n",__FUNCTION__);
	
	while(!wii_stopping) {
#endif
		INT32 *p_button;
		const u32 *p_mask;
		wii_pad_t *ppad;

#ifdef INPUTTH_ON	
		LWP_ThreadSleep(inpqueue);
#endif

#ifdef GAMECUBE_PAD		
		if (PAD_ScanPads() != 0) 
		{
			for (int i = 0; i < 4; i++) 
			{	
				UINT16 btns = PAD_ButtonsHeld(i);

				ppad = &pad[i];
				
				p_button = ppad->buttons;
				p_mask = gamecube_buttons;
				for (int j = 0; j < 11; j++) {
					*p_button = ((btns & GAMECUBE_BUTTON_MASK) == *p_mask) ? 0x80 : 0;
					++p_button;
					++p_mask;
				}
				
				p_button = ppad->hats;
				p_mask = gamecube_directions;
				for (int j=0; j<4; j++) {
					*p_button = ((btns & *p_mask) != 0) ? 0x80 : 0;
					++p_button;
					++p_mask;
				}
				
				p_button = ppad->axis;
				*p_button = (int)(PAD_StickX(i) * 512);
				++p_button;
				*p_button = (int)(PAD_StickY(i) * -512);
				++p_button;
				*p_button = (int)(PAD_SubStickX(i) * 512);
				++p_button;
				*p_button = (int)(PAD_SubStickY(i) * -512);
				++p_button;
				*p_button = (int)(PAD_TriggerL(i) * 512);
				++p_button;
				*p_button = (int)(PAD_TriggerR(i) * 512);
			}	
		}
#endif		
		
		WPAD_ScanPads();

		for (int i = 0; i < 4; i++) 
		{
			WPADData *wd = WPAD_Data(i);
			
			if (wd->err != WPAD_ERR_NONE)
				continue;
			
			ppad = &pad[i];
			memset(ppad,0,sizeof(wii_pad_t));
			
			if(wd->exp.type == EXP_CLASSIC) 
			{
				p_button = ppad->buttons;
				p_mask = classic_buttons;
				for (int j = 0; j < 11; j++) {
					*p_button = ((wd->btns_h & CLASSIC_BUTTON_MASK) == *p_mask) ? 0x80 : 0;
					++p_button;
					++p_mask;
				}
				
				p_button = ppad->hats;
				p_mask = classic_directions;
				for (int j=0; j<4; j++) {
					*p_button = ((wd->btns_h & *p_mask) != 0) ? 0x80 : 0;
					++p_button;
					++p_mask;
				}

				p_button = ppad->axis;
				*p_button = (int)(wd->exp.classic.ljs.mag * sin((M_PI * wd->exp.classic.ljs.ang) / 180.0f) * 128.0f * 512);
				++p_button;
				*p_button = (int)(wd->exp.classic.ljs.mag * cos((M_PI * wd->exp.classic.ljs.ang) / 180.0f) * 128.0f * -512);
				++p_button;
				*p_button = (int)(wd->exp.classic.rjs.mag * sin((M_PI * wd->exp.classic.rjs.ang) / 180.0f) * 128.0f * 512);
				++p_button;
				*p_button = (int)(wd->exp.classic.rjs.mag * cos((M_PI * wd->exp.classic.rjs.ang) / 180.0f) * 128.0f * -512);
				++p_button;
				*p_button = (int)(wd->exp.classic.l_shoulder * 128.0f * 512);
				++p_button;
				*p_button = (int)(wd->exp.classic.r_shoulder * 128.0f * 512);
			}
			else // Wiimote
			{
				p_button = ppad->buttons;
				p_mask = wiimote_buttons;
				for (int j = 0; j < 11; j++) {
					*p_button = ((wd->btns_h & WIIMOTE_BUTTON_MASK) == *p_mask) ? 0x80 : 0;
					++p_button;
					++p_mask;
				}
				
				p_button = ppad->hats;
				p_mask = wiimote_directions;
				for (int j = 0; j < 4; j++) {
					*p_button = ((wd->btns_h & *p_mask) != 0) ? 0x80 : 0;
					++p_button;
					++p_mask;
				}				

				if(wd->exp.type == WPAD_EXP_NUNCHUK) // + Nunchuk
				{
					float ang = (M_PI * wd->exp.nunchuk.js.ang) / 180.0f;
					float mag = wd->exp.nunchuk.js.mag * 128.0f;
					
					ppad->axis[0] = (int)(mag * sin(ang) * 512);
					ppad->axis[1] = (int)(mag * cos(ang) * -512);
#ifdef NUNCHUK_AS_JOY
					ppad->buttons[0] |= ppad->buttons[9];
					ppad->buttons[9] = 0;
					ppad->buttons[2] |= ppad->buttons[10];
					ppad->buttons[10] = 0;
					
					if( !(ppad->hats[0] | ppad->hats[1]) ) {
						int y = (wd->exp.nunchuk.js.pos.y - wd->exp.nunchuk.js.center.y) & ~3;
#ifdef WIIINPUT_DEBUG
						if(y != 0)
							wii_debug("%s: y = %d\n",__FUNCTION__,y);
#endif
						if(y > 0)		// up
							ppad->hats[0] = 0x80;
						else if(y < 0)	// down
							ppad->hats[1] = 0x80;
					}
					
					if( !(ppad->hats[2] | ppad->hats[3]) ) {
						int x = (wd->exp.nunchuk.js.pos.x - wd->exp.nunchuk.js.center.x) & ~3;
#ifdef WIIINPUT_DEBUG
						if(x != 0)
							wii_debug("%s: x = %d\n",__FUNCTION__,x);
#endif
						if(x < 0)		// left
							ppad->hats[2] = 0x80;
						else if(x > 0)	// right
							ppad->hats[3] = 0x80;
					}
#endif
				}
				else {
					if(ppad->hats[0])	// right
						ppad->axis[0] = (127 * 512);
					else if(ppad->hats[1])	// left
						ppad->axis[0] = (-127 * 512);
					else
						ppad->axis[0] = 0;
					if(ppad->hats[2])	// up
						ppad->axis[1] = (127 * 512);
					else if(ppad->hats[3])	// down
						ppad->axis[1] = (-127 * 512);
					else
						ppad->axis[1] = 0;
				}
				ppad->axis[2] = 0;
				ppad->axis[3] = 0;
				ppad->axis[4] = 0;
				ppad->axis[5] = 0;
			}

			if (wd->ir.valid) 
			{
				int v = wii_screen_width() / 2;
				ppad->pointer[0] = (wd->ir.x - v) * INPUT_ABSOLUTE_MAX / v;
				ppad->pointer[1] = (wd->ir.y - 240) * INPUT_ABSOLUTE_MAX / 240;
			}
			else 
			{
				// Should make offscreen reloading work
				ppad->pointer[0] = INPUT_ABSOLUTE_MAX + 1;
				ppad->pointer[1] = INPUT_ABSOLUTE_MAX + 1;
			}
		}
#ifdef INPUTTH_ON
	}
#endif
	return NULL;
}

void wii_update_input(void)
{
#ifdef INPUTTH_ON
	LWP_ThreadSignal(inpqueue);
#else
	wii_poll_input(NULL);
#endif
}

void wii_init_input(running_machine *machine)
{	
	input_device_class_enable(machine, DEVICE_CLASS_LIGHTGUN, TRUE);
	input_device_class_enable(machine, DEVICE_CLASS_JOYSTICK, TRUE);

	WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetVRes(WPAD_CHAN_ALL, wii_screen_width(), 480);

	for (int i = 0; i < 4; i++) 
	{
		char name[20];
		int j;
		input_device *devinfo;
		wii_pad_t *ppad = &pad[i];
		
		snprintf(name, sizeof(name), "Controller %d", i + 1);
		devinfo = input_device_add(machine, DEVICE_CLASS_JOYSTICK, name, NULL);

		for (j = 0; j < 11; j++) {
			button_t *btn = &buttons[j];
			input_device_item_add(devinfo, btn->name, &ppad->buttons[j], btn->item_id, joypad_get_state);
		}
		
		input_device_item_add(devinfo, "D-Pad Up",    &ppad->hats[0], ITEM_ID_HAT1UP, joypad_get_state);
		input_device_item_add(devinfo, "D-Pad Down",  &ppad->hats[1], ITEM_ID_HAT1DOWN, joypad_get_state);
		input_device_item_add(devinfo, "D-Pad Left",  &ppad->hats[2], ITEM_ID_HAT1LEFT, joypad_get_state);
		input_device_item_add(devinfo, "D-Pad Right", &ppad->hats[3], ITEM_ID_HAT1RIGHT, joypad_get_state);
		input_device_item_add(devinfo, "Main/Nunchuck/Left X Axis", &ppad->axis[0], ITEM_ID_XAXIS, joypad_get_state);
		input_device_item_add(devinfo, "Main/Nunchuck/Left Y Axis", &ppad->axis[1], ITEM_ID_YAXIS, joypad_get_state);
		input_device_item_add(devinfo, "C/Right X Axis",    &ppad->axis[2], ITEM_ID_RXAXIS, joypad_get_state);
		input_device_item_add(devinfo, "C/Right Y Axis",    &ppad->axis[3], ITEM_ID_RYAXIS, joypad_get_state);
		input_device_item_add(devinfo, "L/L Analog",    &ppad->axis[4], ITEM_ID_ZAXIS, joypad_get_state);
		input_device_item_add(devinfo, "Y/R Analog",    &ppad->axis[5], ITEM_ID_RZAXIS, joypad_get_state);
		
		snprintf(name, sizeof(name), "Wiimote Pointer %d", i + 1);
		devinfo = input_device_add(machine, DEVICE_CLASS_LIGHTGUN, name, NULL);
		input_device_item_add(devinfo, "X", &ppad->pointer[0], ITEM_ID_XAXIS, joypad_get_state);
		input_device_item_add(devinfo, "Y", &ppad->pointer[1], ITEM_ID_YAXIS, joypad_get_state);	
	}
#ifdef INPUTTH_ON
	if (inpthread == LWP_THREAD_NULL) {
		LWP_InitQueue(&inpqueue);
		LWP_CreateThread(&inpthread, wii_poll_input, NULL, NULL, 0, INPUTTH_PRIORITY);
	}
#endif
}

void wii_shutdown_input(void)
{
	wii_debug("%s: start\n",__FUNCTION__);
#ifdef INPUTTH_ON	
	if(inpthread != LWP_THREAD_NULL) {
		void *status;
		
		wii_stopping = true;
		LWP_ThreadSignal(inpqueue);
		LWP_JoinThread(inpthread, &status);
		LWP_CloseQueue(inpqueue);
		inpthread = LWP_THREAD_NULL;
		wii_debug("%s: input thread turned off\n",__FUNCTION__);
	}
#endif
	wii_debug("%s: end\n",__FUNCTION__);
}

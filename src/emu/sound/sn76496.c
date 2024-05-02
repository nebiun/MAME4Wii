/***************************************************************************

  sn76496.c
  by Nicola Salmoria

  Routines to emulate the:
  Texas Instruments SN76489, SN76489A, SN76494/SN76496
  ( Also known as, or at least compatible with, the TMS9919.)
  and the Sega 'PSG' used on the Master System, Game Gear, and Megadrive/Genesis
  This chip is known as the Programmable Sound Generator, or PSG, and is a 4
  channel sound generator, with three squarewave channels and a noise/arbitrary
  duty cycle channel.

  Noise emulation for all chips should be accurate:
  SN76489 uses a 15-bit shift register with taps on bits D and E, output on /E
  * It uses a 15-bit ring buffer for periodic noise/arbitrary duty cycle.
  SN76489A uses a 16-bit shift register with taps on bits D and F, output on F
  * It uses a 16-bit ring buffer for periodic noise/arbitrary duty cycle.
  SN76494 and SN76496 are PROBABLY identical in operation to the SN76489A
  * They have an audio input line which is mixed with the 4 channels of output.
  Sega Master System III/MD/Genesis PSG uses a 16-bit shift register with taps
  on bits C and F, output on F
  * It uses a 16-bit ring buffer for periodic noise/arbitrary duty cycle.
  Sega Game Gear PSG is identical to the SMS3/MD/Genesis one except it has an
  extra register for mapping which channels go to which speaker.

  28/03/2005 : Sebastien Chevalier
  Update th SN76496Write func, according to SN76489 doc found on SMSPower.
   - On write with 0x80 set to 0, when LastRegister is other then TONE,
   the function is similar than update with 0x80 set to 1

  23/04/2007 : Lord Nightmare
  Major update, implement all three different noise generation algorithms and a
  set_variant call to discern among them.

  28/04/2009 : Lord Nightmare
  Add READY line readback; cleaned up struct a bit. Cleaned up comments.
  Add more TODOs. Fixed some unsaved savestate related stuff.

  TODO: * Implement a function for setting stereo regs for the game gear.
          Requires making the core support both mono and stereo, and have
          a select register which determines which channels go where.
        * Implement the TMS9919 and SN94624, which are earlier versions,
          possibly lacking the /8 clock divider, of the SN76489, and hence
          would have a max clock of 500Khz and 4 clocks per sample, as
          opposed to max of 4Mhz and 32 clocks per sample on the SN76489A
        * Implement the T6W28; has registers in a weird order, needs writes
          to be 'sanitized' first. Also is stereo, similar to game gear.
        * Implement the NCR 7496; Is probably 100% compatible with SN76496,
          but the whitenoise taps could be different. Needs someone with a
          Tandy 1200 or whatever it was which uses this to run some tests.
        * Factor out common code so that the SAA1099 can share some code.
***************************************************************************/

#include "sndintrf.h"
#include "streams.h"
#include "sn76496.h"


#define MAX_OUTPUT 0x7fff
#define STEP 0x10000
#define NOISEMODE (R->Register[6]&4)?1:0


typedef struct _sn76496_state sn76496_state;
struct _sn76496_state
{
	sound_stream * Channel;
	INT32 VolTable[16];	/* volume table (for 4-bit to db conversion)*/
	INT32 Register[8];	/* registers */
	INT32 LastRegister;	/* last register written */
	INT32 Volume[4];	/* db volume of voice 0-2 and noise */
	UINT32 RNG;			/* noise generator LFSR*/
	INT32 FeedbackMask;	/* mask for feedback */
	INT32 WhitenoiseTaps;	/* mask for white noise taps */
	INT32 WhitenoiseInvert;	/* white noise invert flag */
	INT32 Period[4];	/* Length of 1/2 of waveform */
	INT32 Count[4];		/* Position within the waveform */
	INT32 Output[4];	/* 1-bit output of each channel, pre-volume */
	INT32 CyclestoREADY;/* number of cycles until the READY line goes active */
};


INLINE sn76496_state *get_safe_token(const device_config *device)
{
	assert(device != NULL);
	assert(device->token != NULL);
	assert(device->type == SOUND);
	assert(sound_get_type(device) == SOUND_SN76496 ||
		   sound_get_type(device) == SOUND_SN76489 ||
		   sound_get_type(device) == SOUND_SN76489A ||
		   sound_get_type(device) == SOUND_SN76494 ||
		   sound_get_type(device) == SOUND_GAMEGEAR ||
		   sound_get_type(device) == SOUND_SMSIII);
	return (sn76496_state *)device->token;
}

READ8_DEVICE_HANDLER( sn76496_ready_r )
{
	sn76496_state *R = get_safe_token(device);
	stream_update(R->Channel);
	return (R->CyclestoREADY? 0 : 1);
}

WRITE8_DEVICE_HANDLER( sn76496_w )
{
	sn76496_state *R = get_safe_token(device);
	int n, r, c;


	/* update the output buffer before changing the registers */
	stream_update(R->Channel);

	/* set number of cycles until READY is active; this is always one
           'sample', i.e. it equals the clock divider exactly; until the
           clock divider is fully supported, we delay until one sample has
           played. The fact that this below is '2' and not '1' is because
           of a ?race condition? in the mess crvision driver, where after
           any sample is played at all, no matter what, the cycles_to_ready
           ends up never being not ready, unless this value is greater than
           1. Once the full clock divider stuff is written, this should no
           longer be an issue. */
	R->CyclestoREADY = 2;

	if (data & 0x80)
	{
		r = (data & 0x70) >> 4;
		R->LastRegister = r;
		R->Register[r] = (R->Register[r] & 0x3f0) | (data & 0x0f);
	}
	else
    {
		r = R->LastRegister;
	}
	c = r/2;
	switch (r)
	{
		case 0:	/* tone 0 : frequency */
		case 2:	/* tone 1 : frequency */
		case 4:	/* tone 2 : frequency */
		    if ((data & 0x80) == 0) R->Register[r] = (R->Register[r] & 0x0f) | ((data & 0x3f) << 4);
			R->Period[c] = STEP * R->Register[r];
			if (R->Period[c] == 0) R->Period[c] = STEP;
			if (r == 4)
			{
				/* update noise shift frequency */
				if ((R->Register[6] & 0x03) == 0x03)
					R->Period[3] = 2 * R->Period[2];
			}
			break;
		case 1:	/* tone 0 : volume */
		case 3:	/* tone 1 : volume */
		case 5:	/* tone 2 : volume */
		case 7:	/* noise  : volume */
			R->Volume[c] = R->VolTable[data & 0x0f];
			if ((data & 0x80) == 0) R->Register[r] = (R->Register[r] & 0x3f0) | (data & 0x0f);
			break;
		case 6:	/* noise  : frequency, mode */
			{
			        if ((data & 0x80) == 0) R->Register[r] = (R->Register[r] & 0x3f0) | (data & 0x0f);
				n = R->Register[6];
				/* N/512,N/1024,N/2048,Tone #3 output */
				R->Period[3] = ((n&3) == 3) ? 2 * R->Period[2] : (STEP << (5+(n&3)));
			        /* Reset noise shifter */
				R->RNG = R->FeedbackMask;
				R->Output[3] = R->RNG & 1;
			}
			break;
	}
}



static STREAM_UPDATE( SN76496Update )
{
	int i;
	sn76496_state *R = (sn76496_state *)param;
	stream_sample_t *buffer = outputs[0];


	/* If the volume is 0, increase the counter; this is more or less
    a speedup hack for when silence is to be output */
	for (i = 0;i < 4;i++)
	{
		if (R->Volume[i] == 0)
		{
			/* note that I do count += samples, NOT count = samples + 1.
            You might think it's the same since the volume is 0, but doing
            the latter could cause interferencies when the program is
            rapidly modulating the volume. */
			if (R->Count[i] <= samples*STEP) R->Count[i] += samples*STEP;
		}
	}

	while (samples > 0)
	{
		int vol[4];
		unsigned int out;
		int left;

		/* decrement Cycles to READY by one */
		if (R->CyclestoREADY >0) R->CyclestoREADY--;

		/* vol[] keeps track of how long each square wave stays */
		/* in the 1 position during the sample period. */
		vol[0] = vol[1] = vol[2] = vol[3] = 0;

		for (i = 0;i < 3;i++)
		{
			if (R->Output[i]) vol[i] += R->Count[i];
			R->Count[i] -= STEP;
			/* Period[i] is the half period of the square wave. Here, in each
            loop I add Period[i] twice, so that at the end of the loop the
            square wave is in the same status (0 or 1) it was at the start.
            vol[i] is also incremented by Period[i], since the wave has been 1
            exactly half of the time, regardless of the initial position.
            If we exit the loop in the middle, Output[i] has to be inverted
            and vol[i] incremented only if the exit status of the square
            wave is 1. */
			while (R->Count[i] <= 0)
			{
				R->Count[i] += R->Period[i];
				if (R->Count[i] > 0)
				{
					R->Output[i] ^= 1;
					if (R->Output[i]) vol[i] += R->Period[i];
					break;
				}
				R->Count[i] += R->Period[i];
				vol[i] += R->Period[i];
			}
			if (R->Output[i]) vol[i] -= R->Count[i];
		}

		left = STEP;
		do
		{
			int nextevent;


			if (R->Count[3] < left) nextevent = R->Count[3];
			else nextevent = left;

			if (R->Output[3]) vol[3] += R->Count[3];
			R->Count[3] -= nextevent;
			if (R->Count[3] <= 0)
			{
		        if (NOISEMODE == 1) /* White Noise Mode */
		        {
			        if (((R->RNG & R->WhitenoiseTaps) != R->WhitenoiseTaps) && ((R->RNG & R->WhitenoiseTaps) != 0)) /* crappy xor! */
					{
				        R->RNG >>= 1;
				        R->RNG |= R->FeedbackMask;
					}
					else
					{
				        R->RNG >>= 1;
					}
					R->Output[3] = R->WhitenoiseInvert ? !(R->RNG & 1) : R->RNG & 1;
				}
				else /* Periodic noise mode */
				{
			        if (R->RNG & 1)
					{
				        R->RNG >>= 1;
				        R->RNG |= R->FeedbackMask;
					}
					else
					{
				        R->RNG >>= 1;
					}
					R->Output[3] = R->RNG & 1;
				}
				R->Count[3] += R->Period[3];
				if (R->Output[3]) vol[3] += R->Period[3];
			}
			if (R->Output[3]) vol[3] -= R->Count[3];

			left -= nextevent;
		} while (left > 0);

		out = vol[0] * R->Volume[0] + vol[1] * R->Volume[1] +
				vol[2] * R->Volume[2] + vol[3] * R->Volume[3];

		if (out > MAX_OUTPUT * STEP) out = MAX_OUTPUT * STEP;

		*(buffer++) = out / STEP;

		samples--;
	}
}



static void SN76496_set_gain(sn76496_state *R,int gain)
{
	int i;
	double out;


	gain &= 0xff;

	/* increase max output basing on gain (0.2 dB per step) */
	out = MAX_OUTPUT / 3;
	while (gain-- > 0)
		out *= 1.023292992;	/* = (10 ^ (0.2/20)) */

	/* build volume table (2dB per step) */
	for (i = 0;i < 15;i++)
	{
		/* limit volume to avoid clipping */
		if (out > MAX_OUTPUT / 3) R->VolTable[i] = MAX_OUTPUT / 3;
		else R->VolTable[i] = out;

		out /= 1.258925412;	/* = 10 ^ (2/20) = 2dB */
	}
	R->VolTable[15] = 0;
}



static int SN76496_init(const device_config *device, sn76496_state *R)
{
	int sample_rate = device->clock/16;
	int i;

	R->Channel = stream_create(device,0,1,sample_rate,R,SN76496Update);

	for (i = 0;i < 4;i++) R->Volume[i] = 0;

	R->LastRegister = 0;
	for (i = 0;i < 8;i+=2)
	{
		R->Register[i] = 0;
		R->Register[i + 1] = 0x0f;	/* volume = 0 */
	}

	for (i = 0;i < 4;i++)
	{
		R->Output[i] = 0;
		R->Period[i] = R->Count[i] = STEP;
	}

	/* Default is SN76489 non-A */
	R->FeedbackMask = 0x4000;     /* mask for feedback */
	R->WhitenoiseTaps = 0x03;   /* mask for white noise taps */
	R->WhitenoiseInvert = 1; /* white noise invert flag */
	R->CyclestoREADY = 1; /* assume ready is not active immediately on init. is this correct?*/

	R->RNG = R->FeedbackMask;
	R->Output[3] = R->RNG & 1;

	return 0;
}


static void generic_start(const device_config *device, int feedbackmask, int noisetaps, int noiseinvert)
{
	sn76496_state *chip = get_safe_token(device);

	if (SN76496_init(device,chip) != 0)
		fatalerror("Error creating SN76496 chip");
	SN76496_set_gain(chip, 0);

	chip->FeedbackMask = feedbackmask;
	chip->WhitenoiseTaps = noisetaps;
	chip->WhitenoiseInvert = noiseinvert;

	state_save_register_device_item_array(device, 0, chip->VolTable);
	state_save_register_device_item_array(device, 0, chip->Register);
	state_save_register_device_item(device, 0, chip->LastRegister);
	state_save_register_device_item_array(device, 0, chip->Volume);
	state_save_register_device_item(device, 0, chip->RNG);
	state_save_register_device_item(device, 0, chip->FeedbackMask);
	state_save_register_device_item(device, 0, chip->WhitenoiseTaps);
	state_save_register_device_item(device, 0, chip->WhitenoiseInvert);
	state_save_register_device_item_array(device, 0, chip->Period);
	state_save_register_device_item_array(device, 0, chip->Count);
	state_save_register_device_item_array(device, 0, chip->Output);
	state_save_register_device_item(device, 0, chip->CyclestoREADY);
}


static DEVICE_START( sn76489 )
{
	generic_start(device, 0x4000, 0x03, TRUE);
}

static DEVICE_START( sn76489a )
{
	generic_start(device, 0x8000, 0x06, FALSE);
}

static DEVICE_START( sn76494 )
{
	generic_start(device, 0x8000, 0x06, FALSE);
}

static DEVICE_START( sn76496 )
{
	generic_start(device, 0x8000, 0x06, FALSE);
}

static DEVICE_START( gamegear )
{
	generic_start(device, 0x8000, 0x09, FALSE);
}

static DEVICE_START( smsiii )
{
	generic_start(device, 0x8000, 0x09, FALSE);
}


/**************************************************************************
 * Generic get_info
 **************************************************************************/

DEVICE_GET_INFO( sn76496 )
{
	switch (state)
	{
		/* --- the following bits of info are returned as 64-bit signed integers --- */
		case DEVINFO_INT_TOKEN_BYTES:					info->i = sizeof(sn76496_state); 				break;

		/* --- the following bits of info are returned as pointers to data or functions --- */
		case DEVINFO_FCT_START:							info->start = DEVICE_START_NAME( sn76496 );		break;
		case DEVINFO_FCT_STOP:							/* Nothing */									break;
		case DEVINFO_FCT_RESET:							/* Nothing */									break;

		/* --- the following bits of info are returned as NULL-terminated strings --- */
		case DEVINFO_STR_NAME:							strcpy(info->s, "SN76496");						break;
		case DEVINFO_STR_FAMILY:					strcpy(info->s, "TI PSG");						break;
		case DEVINFO_STR_VERSION:					strcpy(info->s, "1.1");							break;
		case DEVINFO_STR_SOURCE_FILE:						strcpy(info->s, __FILE__);						break;
		case DEVINFO_STR_CREDITS:					strcpy(info->s, "Copyright Nicola Salmoria and the MAME Team"); break;
	}
}

DEVICE_GET_INFO( sn76489 )
{
	switch (state)
	{
		case DEVINFO_FCT_START:							info->start = DEVICE_START_NAME( sn76489 );		break;
		case DEVINFO_STR_NAME:							strcpy(info->s, "SN76489");						break;
		default: 										DEVICE_GET_INFO_CALL(sn76496);						break;
	}
}

DEVICE_GET_INFO( sn76489a )
{
	switch (state)
	{
		case DEVINFO_FCT_START:							info->start = DEVICE_START_NAME( sn76489a );		break;
		case DEVINFO_STR_NAME:							strcpy(info->s, "SN76489A");					break;
		default: 										DEVICE_GET_INFO_CALL(sn76496);						break;
	}
}

DEVICE_GET_INFO( sn76494 )
{
	switch (state)
	{
		case DEVINFO_FCT_START:							info->start = DEVICE_START_NAME( sn76494 );		break;
		case DEVINFO_STR_NAME:							strcpy(info->s, "SN76494");						break;
		default: 										DEVICE_GET_INFO_CALL(sn76496);						break;
	}
}

DEVICE_GET_INFO( gamegear )
{
	switch (state)
	{
		case DEVINFO_FCT_START:							info->start = DEVICE_START_NAME( gamegear );		break;
		case DEVINFO_STR_NAME:							strcpy(info->s, "Game Gear PSG");				break;
		default: 										DEVICE_GET_INFO_CALL(sn76496);						break;
	}
}

DEVICE_GET_INFO( smsiii )
{
	switch (state)
	{
		case DEVINFO_FCT_START:							info->start = DEVICE_START_NAME( smsiii );			break;
		case DEVINFO_STR_NAME:							strcpy(info->s, "SMSIII PSG");					break;
		default: 										DEVICE_GET_INFO_CALL(sn76496);						break;
	}
}

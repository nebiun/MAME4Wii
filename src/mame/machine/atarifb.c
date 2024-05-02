/*************************************************************************

    Atari Football hardware

*************************************************************************/

#include "driver.h"
#include "deprecat.h"
#include "atarifb.h"
#include "sound/discrete.h"

static int CTRLD;
static int sign_x_1, sign_y_1;
static int sign_x_2, sign_y_2;
static int sign_x_3, sign_y_3;
static int sign_x_4, sign_y_4;


WRITE8_HANDLER( atarifb_out1_w )
{
	const device_config *discrete = devtag_get_device(space->machine, "discrete");

	CTRLD = data;

	discrete_sound_w(discrete, ATARIFB_WHISTLE_EN,  data & 0x01);		// Whistle
	discrete_sound_w(discrete, ATARIFB_HIT_EN,  data & 0x02);			// Hit
	discrete_sound_w(discrete, ATARIFB_ATTRACT_EN, data & 0x10);		// Attract
	discrete_sound_w(discrete, ATARIFB_NOISE_EN,  data & 0x04);		// Noise Enable / Kicker
}


WRITE8_HANDLER( atarifb4_out1_w )
{
	const device_config *discrete = devtag_get_device(space->machine, "discrete");

	CTRLD = data;

	discrete_sound_w(discrete, ATARIFB_WHISTLE_EN,  data & 0x01);		// Whistle
	discrete_sound_w(discrete, ATARIFB_HIT_EN,  data & 0x02);			// Hit
	discrete_sound_w(discrete, ATARIFB_ATTRACT_EN, data & 0x10);		// Attract
	discrete_sound_w(discrete, ATARIFB_NOISE_EN,  data & 0x04);		// Noise Enable / Kicker

	coin_counter_w (1, data & 0x80);
}


WRITE8_HANDLER( abaseb_out1_w )
{
	const device_config *discrete = devtag_get_device(space->machine, "discrete");

	CTRLD = data;

	discrete_sound_w(discrete, ATARIFB_WHISTLE_EN,  data & 0x01);		// Whistle
	discrete_sound_w(discrete, ATARIFB_HIT_EN,  data & 0x02);			// Hit
	discrete_sound_w(discrete, ATARIFB_ATTRACT_EN, data & 0x10);		// Attract
	discrete_sound_w(discrete, ATARIFB_NOISE_EN,  data & 0x04);		// Noise Enable / Kicker

	if (data & 0x80)
	{
		/* Invert video */
		palette_set_color(space->machine,1,MAKE_RGB(0x00,0x00,0x00)); /* black  */
		palette_set_color(space->machine,0,MAKE_RGB(0xff,0xff,0xff)); /* white  */
	}
	else
	{
		/* Regular video */
		palette_set_color(space->machine,0,MAKE_RGB(0x00,0x00,0x00)); /* black  */
		palette_set_color(space->machine,1,MAKE_RGB(0xff,0xff,0xff)); /* white  */
	}
}


WRITE8_HANDLER( soccer_out1_w )
{
	const device_config *discrete = devtag_get_device(space->machine, "discrete");

	CTRLD = data;

	/* bit 0 = whistle */
	/* bit 1 = hit */
	/* bit 2 = kicker */
	/* bit 3 = unused */
	/* bit 4 = 2/4 Player LED */	// Say what?
	/* bit 5-6 = trackball CTRL bits */
	/* bit 7 = Rule LED */

	discrete_sound_w(discrete, ATARIFB_WHISTLE_EN,  data & 0x01);		// Whistle
	discrete_sound_w(discrete, ATARIFB_HIT_EN,  data & 0x02);			// Hit
	discrete_sound_w(discrete, ATARIFB_ATTRACT_EN, data & 0x10);		// Attract
	discrete_sound_w(discrete, ATARIFB_NOISE_EN,  data & 0x04);		// Noise Enable / Kicker

//  set_led_status(0,data & 0x10);  // !!!!!!!!!! Is this correct????
	set_led_status(1,data & 0x80);
}


WRITE8_HANDLER( atarifb_out2_w )
{
	const device_config *discrete = devtag_get_device(space->machine, "discrete");

	discrete_sound_w(discrete, ATARIFB_CROWD_DATA, data & 0x0f);	// Crowd

	coin_counter_w (0, data & 0x10);
}


WRITE8_HANDLER( soccer_out2_w )
{
	const device_config *discrete = devtag_get_device(space->machine, "discrete");

	discrete_sound_w(discrete, ATARIFB_CROWD_DATA, data & 0x0f);	// Crowd

	coin_counter_w (0, data & 0x10);
	coin_counter_w (1, data & 0x20);
	coin_counter_w (2, data & 0x40);
}



/*************************************
 *
 *  LED control
 *
 *************************************/

WRITE8_HANDLER( atarifb_out3_w )
{
	int loop = cpu_getiloops(cputag_get_cpu(space->machine, "maincpu"));

	switch (loop)
	{
		case 0x00:
			/* Player 1 play select lamp */
			output_set_value("ledleft0", (data >> 0) & 1);
			output_set_value("ledleft1", (data >> 1) & 1);
			output_set_value("ledleft2", (data >> 2) & 1);
			output_set_value("ledleft3", (data >> 3) & 1);
			output_set_value("ledleft4", (data >> 4) & 1);
			break;
		case 0x01:
			break;
		case 0x02:
			/* Player 2 play select lamp */
			output_set_value("ledright0", (data >> 0) & 1);
			output_set_value("ledright1", (data >> 1) & 1);
			output_set_value("ledright2", (data >> 2) & 1);
			output_set_value("ledright3", (data >> 3) & 1);
			output_set_value("ledright4", (data >> 4) & 1);
			break;
		case 0x03:
			break;
	}
//  logerror("out3_w, %02x:%02x\n", loop, data);
}


READ8_HANDLER( atarifb_in0_r )
{
	if ((CTRLD & 0x20) == 0x00)
	{
		int val;

		val = (sign_y_2 >> 7) |
			  (sign_x_2 >> 6) |
			  (sign_y_1 >> 5) |
			  (sign_x_1 >> 4) |
			  input_port_read(space->machine, "IN0");
		return val;
	}
	else
	{
		static int counter_x,counter_y;
		int new_x,new_y;

		/* Read player 1 trackball */
		new_x = input_port_read(space->machine, "IN3");
		if (new_x != counter_x)
		{
			sign_x_1 = (new_x - counter_x) & 0x80;
			counter_x = new_x;
		}

		new_y = input_port_read(space->machine, "IN2");
		if (new_y != counter_y)
		{
			sign_y_1 = (new_y - counter_y) & 0x80;
			counter_y = new_y;
		}

		return (((counter_y & 0x0f) << 4) | (counter_x & 0x0f));
	}
}


READ8_HANDLER( atarifb_in2_r )
{
	if ((CTRLD & 0x20) == 0x00)
	{
		return input_port_read(space->machine, "IN1");
	}
	else
	{
		static int counter_x,counter_y;
		int new_x,new_y;

		/* Read player 2 trackball */
		new_x = input_port_read(space->machine, "IN5");
		if (new_x != counter_x)
		{
			sign_x_2 = (new_x - counter_x) & 0x80;
			counter_x = new_x;
		}

		new_y = input_port_read(space->machine, "IN4");
		if (new_y != counter_y)
		{
			sign_y_2 = (new_y - counter_y) & 0x80;
			counter_y = new_y;
		}

		return (((counter_y & 0x0f) << 4) | (counter_x & 0x0f));
	}
}

READ8_HANDLER( atarifb4_in0_r )
{
	/* LD1 and LD2 low, return sign bits */
	if ((CTRLD & 0x60) == 0x00)
	{
		int val;

		val = (sign_x_4 >> 7) |
			  (sign_y_4 >> 6) |
			  (sign_x_2 >> 5) |
			  (sign_y_2 >> 4) |
			  (sign_x_3 >> 3) |
			  (sign_y_3 >> 2) |
			  (sign_x_1 >> 1) |
			  (sign_y_1 >> 0);
		return val;
	}
	else if ((CTRLD & 0x60) == 0x60)
	/* LD1 and LD2 both high, return Team 1 right player (player 1) */
	{
		static int counter_x,counter_y;
		int new_x,new_y;

		/* Read player 1 trackball */
		new_x = input_port_read(space->machine, "IN3");
		if (new_x != counter_x)
		{
			sign_x_1 = (new_x - counter_x) & 0x80;
			counter_x = new_x;
		}

		new_y = input_port_read(space->machine, "IN2");
		if (new_y != counter_y)
		{
			sign_y_1 = (new_y - counter_y) & 0x80;
			counter_y = new_y;
		}

		return (((counter_y & 0x0f) << 4) | (counter_x & 0x0f));
	}
	else if ((CTRLD & 0x60) == 0x40)
	/* LD1 high, LD2 low, return Team 1 left player (player 2) */
	{
		static int counter_x,counter_y;
		int new_x,new_y;

		/* Read player 2 trackball */
		new_x = input_port_read(space->machine, "IN5");
		if (new_x != counter_x)
		{
			sign_x_2 = (new_x - counter_x) & 0x80;
			counter_x = new_x;
		}

		new_y = input_port_read(space->machine, "IN4");
		if (new_y != counter_y)
		{
			sign_y_2 = (new_y - counter_y) & 0x80;
			counter_y = new_y;
		}

		return (((counter_y & 0x0f) << 4) | (counter_x & 0x0f));
	}

	else return 0;
}


READ8_HANDLER( atarifb4_in2_r )
{
	if ((CTRLD & 0x40) == 0x00)
	{
		return input_port_read(space->machine, "IN1");
	}
	else if ((CTRLD & 0x60) == 0x60)
	/* LD1 and LD2 both high, return Team 2 right player (player 3) */
	{
		static int counter_x,counter_y;
		int new_x,new_y;

		/* Read player 3 trackball */
		new_x = input_port_read(space->machine, "IN7");
		if (new_x != counter_x)
		{
			sign_x_3 = (new_x - counter_x) & 0x80;
			counter_x = new_x;
		}

		new_y = input_port_read(space->machine, "IN6");
		if (new_y != counter_y)
		{
			sign_y_3 = (new_y - counter_y) & 0x80;
			counter_y = new_y;
		}

		return (((counter_y & 0x0f) << 4) | (counter_x & 0x0f));
	}
	else if ((CTRLD & 0x60) == 0x40)
	/* LD1 high, LD2 low, return Team 2 left player (player 4) */
	{
		static int counter_x,counter_y;
		int new_x,new_y;

		/* Read player 4 trackball */
		new_x = input_port_read(space->machine, "IN9");
		if (new_x != counter_x)
		{
			sign_x_4 = (new_x - counter_x) & 0x80;
			counter_x = new_x;
		}

		new_y = input_port_read(space->machine, "IN8");
		if (new_y != counter_y)
		{
			sign_y_4 = (new_y - counter_y) & 0x80;
			counter_y = new_y;
		}

		return (((counter_y & 0x0f) << 4) | (counter_x & 0x0f));
	}

	else return 0;
}



/***************************************************************************

GAME PLAN driver

driver by Chris Moore

Killer Comet memory map

MAIN CPU:

Address          Dir Data     Name      Description
---------------- --- -------- --------- -----------------------
00000-xxxxxxxxxx R/W xxxxxxxx RAM       can be either 256 bytes (2x2101) or 1kB (2x2114)
00001-----------              n.c.
00010-----------              n.c.
00011-----------              n.c.
00100-------xxxx R/W xxxxxxxx VIA 1     6522 for video interface
00101-------xxxx R/W xxxxxxxx VIA 2     6522 for I/O interface
00110-------xxxx R/W xxxxxxxx VIA 3     6522 for interface with audio CPU
00111-----------              n.c.
01--------------              n.c.
10--------------              n.c.
11000xxxxxxxxxxx R   xxxxxxxx ROM E2    program ROM
11001xxxxxxxxxxx R   xxxxxxxx ROM F2    program ROM
11010xxxxxxxxxxx R   xxxxxxxx ROM G2    program ROM
11011xxxxxxxxxxx R   xxxxxxxx ROM J2    program ROM
11100xxxxxxxxxxx R   xxxxxxxx ROM J1    program ROM
11101xxxxxxxxxxx R   xxxxxxxx ROM G1    program ROM
11110xxxxxxxxxxx R   xxxxxxxx ROM F1    program ROM
11111xxxxxxxxxxx R   xxxxxxxx ROM E1    program ROM


SOUND CPU:

Address          Dir Data     Name      Description
---------------- --- -------- --------- -----------------------
000-0----xxxxxxx R/W xxxxxxxx VIA 5     6532 internal RAM
000-1------xxxxx R/W xxxxxxxx VIA 5     6532 for interface with main CPU
001-------------              n.c.
010-------------              n.c.
011-------------              n.c.
100-------------              n.c.
101-----------xx R/W xxxxxxxx PSG 1     AY-3-8910
110-------------              n.c.
111--xxxxxxxxxxx R   xxxxxxxx ROM E1


Notes:
- There are two dip switch banks connected to the 8910 ports. They are only
  used for testing.

- Megatack's test mode reports the same fire buttons as Killer Comet, but this
  is wrong: there is only one fire button, not three.

- Megatack's actual name which displays proudly on the cover and everywhere in
  the manual as "MEGATTACK"

- Checked and verified DIPs from manuals and service mode for:
  Challenger
  Kaos
  Killer Comet
  Megattack
  Pot Of Gold (Leprechaun)


TODO:
- The board has, instead of a watchdog, a timed reset that has to be disabled
  on startup. The disable line is tied to CA2 of VIA2, but I don't see writes
  to that pin in the log. Missing support in machine/6522via.c?
- Kaos needs a kludge to avoid a deadlock (see the via_irq() function below).
  I don't know if this is a shortcoming of the driver or of 6522via.c.
- Investigate and document the 8910 dip switches
- Fix the input ports of Kaos

****************************************************************************/

#include "driver.h"
#include "cpu/m6502/m6502.h"
#include "machine/6532riot.h"
#include "machine/6522via.h"
#include "sound/ay8910.h"
#include "gameplan.h"



/*************************************
 *
 *  VIA 2 - I/O
 *
 *************************************/

static WRITE8_DEVICE_HANDLER( io_select_w )
{
	gameplan_state *state = (gameplan_state *)device->machine->driver_data;

	switch (data)
	{
	case 0x01: state->current_port = 0; break;
	case 0x02: state->current_port = 1; break;
	case 0x04: state->current_port = 2; break;
	case 0x08: state->current_port = 3; break;
	case 0x80: state->current_port = 4; break;
	case 0x40: state->current_port = 5; break;
	}
}


static READ8_DEVICE_HANDLER( io_port_r )
{
	static const char *const portnames[] = { "IN0", "IN1", "IN2", "IN3", "DSW0", "DSW1" };
	gameplan_state *state = (gameplan_state *)device->machine->driver_data;

	return input_port_read(device->machine, portnames[state->current_port]);
}


static WRITE8_DEVICE_HANDLER( coin_w )
{
	coin_counter_w(0, ~data & 1);
}


static const via6522_interface via_1_interface =
{
	DEVCB_HANDLER(io_port_r), DEVCB_NULL,	 /*inputs : A/B         */
	DEVCB_NULL, DEVCB_NULL, DEVCB_NULL, DEVCB_NULL,		 /*inputs : CA/B1,CA/B2 */
	DEVCB_NULL, DEVCB_HANDLER(io_select_w),	 /*outputs: A/B         */
	DEVCB_NULL, DEVCB_NULL, DEVCB_NULL, DEVCB_HANDLER(coin_w), /*outputs: CA/B1,CA/B2 */
	DEVCB_NULL				 /*irq                  */
};



/*************************************
 *
 *  VIA 3 - audio
 *
 *************************************/

static WRITE8_DEVICE_HANDLER( audio_reset_w )
{
	gameplan_state *state = (gameplan_state *)device->machine->driver_data;
	cputag_set_input_line(device->machine, "audiocpu", INPUT_LINE_RESET, data ? CLEAR_LINE : ASSERT_LINE);
	if (data == 0)
	{
		device_reset(state->riot);
		cpuexec_boost_interleave(device->machine, attotime_zero, ATTOTIME_IN_USEC(10));
	}
}


static WRITE8_DEVICE_HANDLER( audio_cmd_w )
{
	gameplan_state *state = (gameplan_state *)device->machine->driver_data;
	riot6532_porta_in_set(state->riot, data, 0x7f);
}


static WRITE8_DEVICE_HANDLER( audio_trigger_w )
{
	gameplan_state *state = (gameplan_state *)device->machine->driver_data;
	riot6532_porta_in_set(state->riot, data << 7, 0x80);
}


static const via6522_interface via_2_interface =
{
	DEVCB_NULL, DEVCB_MEMORY_HANDLER("maincpu", PROGRAM, soundlatch_r),				  /*inputs : A/B         */
	DEVCB_NULL, DEVCB_NULL, DEVCB_NULL, DEVCB_NULL,							  /*inputs : CA/B1,CA/B2 */
	DEVCB_HANDLER(audio_cmd_w), DEVCB_NULL,						  /*outputs: A/B         */
	DEVCB_NULL, DEVCB_NULL, DEVCB_HANDLER(audio_trigger_w), DEVCB_HANDLER(audio_reset_w), /*outputs: CA/B1,CA/B2 */
	DEVCB_NULL									  /*irq                  */
};



/*************************************
 *
 *  RIOT - audio
 *
 *************************************/

static WRITE_LINE_DEVICE_HANDLER( r6532_irq )
{
	cputag_set_input_line(device->machine, "audiocpu", 0, state);
	if (state == ASSERT_LINE)
		cpuexec_boost_interleave(device->machine, attotime_zero, ATTOTIME_IN_USEC(10));
}


static WRITE8_DEVICE_HANDLER( r6532_soundlatch_w )
{
	const address_space *space = cputag_get_address_space(device->machine, "maincpu", ADDRESS_SPACE_PROGRAM);
	soundlatch_w(space, 0, data);
}


static const riot6532_interface r6532_interface =
{
	DEVCB_NULL,							/* port A read handler */
	DEVCB_NULL,							/* port B read handler */
	DEVCB_NULL,							/* port A write handler */
	DEVCB_HANDLER(r6532_soundlatch_w),	/* port B write handler */
	DEVCB_LINE(r6532_irq)				/* IRQ callback */
};



/*************************************
 *
 *  Start
 *
 *************************************/

static MACHINE_START( gameplan )
{
	gameplan_state *state = (gameplan_state *)machine->driver_data;

	state->riot = devtag_get_device(machine, "riot");

	/* register for save states */
	state_save_register_global(machine, state->current_port);
}



/*************************************
 *
 *  Reset
 *
 *************************************/

static MACHINE_RESET( gameplan )
{
}



/*************************************
 *
 *  Main CPU memory handlers
 *
 *************************************/

static ADDRESS_MAP_START( gameplan_main_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x03ff) AM_MIRROR(0x1c00) AM_RAM
	AM_RANGE(0x2000, 0x200f) AM_MIRROR(0x07f0) AM_DEVREADWRITE("via6522_0", via_r, via_w)	/* VIA 1 */
	AM_RANGE(0x2800, 0x280f) AM_MIRROR(0x07f0) AM_DEVREADWRITE("via6522_1", via_r, via_w)	/* VIA 2 */
	AM_RANGE(0x3000, 0x300f) AM_MIRROR(0x07f0) AM_DEVREADWRITE("via6522_2", via_r, via_w)	/* VIA 3 */
	AM_RANGE(0x8000, 0xffff) AM_ROM
ADDRESS_MAP_END



/*************************************
 *
 *  Audio CPU memory handlers
 *
 *************************************/

static ADDRESS_MAP_START( gameplan_audio_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x007f) AM_MIRROR(0x1780) AM_RAM  /* 6532 internal RAM */
	AM_RANGE(0x0800, 0x081f) AM_MIRROR(0x17e0) AM_DEVREADWRITE("riot", riot6532_r, riot6532_w)
	AM_RANGE(0xa000, 0xa000) AM_MIRROR(0x1ffc) AM_DEVWRITE("ay", ay8910_address_w)
	AM_RANGE(0xa001, 0xa001) AM_MIRROR(0x1ffc) AM_DEVREAD("ay", ay8910_r)
	AM_RANGE(0xa002, 0xa002) AM_MIRROR(0x1ffc) AM_DEVWRITE("ay", ay8910_data_w)
	AM_RANGE(0xe000, 0xe7ff) AM_MIRROR(0x1800) AM_ROM
ADDRESS_MAP_END


/* same as Gameplan, but larger ROM */
static ADDRESS_MAP_START( leprechn_audio_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x007f) AM_MIRROR(0x1780) AM_RAM  /* 6532 internal RAM */
	AM_RANGE(0x0800, 0x081f) AM_MIRROR(0x17e0) AM_DEVREADWRITE("riot", riot6532_r, riot6532_w)
	AM_RANGE(0xa000, 0xa000) AM_MIRROR(0x1ffc) AM_DEVWRITE("ay", ay8910_address_w)
	AM_RANGE(0xa001, 0xa001) AM_MIRROR(0x1ffc) AM_DEVREAD("ay", ay8910_r)
	AM_RANGE(0xa002, 0xa002) AM_MIRROR(0x1ffc) AM_DEVWRITE("ay", ay8910_data_w)
	AM_RANGE(0xe000, 0xefff) AM_MIRROR(0x1000) AM_ROM
ADDRESS_MAP_END



/*************************************
 *
 *  Input ports
 *
 *************************************/

static INPUT_PORTS_START( killcom )
	PORT_START("IN0")	/* COL. A - from "TEST NO.7 - status locator - coin-door" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME("Do Tests") PORT_CODE(KEYCODE_F1)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME("Select Test") PORT_CODE(KEYCODE_F2)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN1 )

	PORT_START("IN1")	/* COL. B - from "TEST NO.7 - status locator - start sws." */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START("IN2")	/* COL. C - from "TEST NO.8 - status locator - player no.1" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )

	PORT_START("IN3")	/* COL. D - from "TEST NO.8 - status locator - player no.2" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON4 ) PORT_COCKTAIL
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_COCKTAIL
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_COCKTAIL
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_COCKTAIL
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_COCKTAIL
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_COCKTAIL
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_COCKTAIL
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_COCKTAIL

	PORT_START("DSW0")	/* DSW A - from "TEST NO.6 - dip switch A" */
	PORT_DIPNAME( 0x03, 0x03, "Coinage P1/P2" ) PORT_DIPLOCATION("SW1:1,2")
	PORT_DIPSETTING(    0x03, "1 Credit/2 Credits" )
	PORT_DIPSETTING(    0x02, "2 Credits/3 Credits" )
	PORT_DIPSETTING(    0x01, "2 Credits/4 Credits" )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unused ) ) PORT_DIPLOCATION("SW1:3")
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Lives ) ) PORT_DIPLOCATION("SW1:4")
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPSETTING(    0x08, "5" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW1:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW1:6" )
	PORT_DIPNAME( 0xc0, 0xc0, "Reaction" ) PORT_DIPLOCATION("SW1:7,8")
	PORT_DIPSETTING(    0xc0, "Slowest" )
	PORT_DIPSETTING(    0x80, "Slow" )
	PORT_DIPSETTING(    0x40, "Fast" )
	PORT_DIPSETTING(    0x00, "Fastest" )

	PORT_START("DSW1")	/* DSW B - from "TEST NO.6 - dip switch B" */
	PORT_DIPUNUSED_DIPLOC( 0x01, 0x01, "SW2:1" )
	PORT_DIPUNUSED_DIPLOC( 0x02, 0x02, "SW2:2" )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW2:3" )
	PORT_DIPUNUSED_DIPLOC( 0x08, 0x08, "SW2:4" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW2:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW2:6" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) ) PORT_DIPLOCATION("SW2:7")
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) ) PORT_DIPLOCATION("SW2:8")
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START("DSW2")	/* audio board DSW A */
	PORT_DIPUNUSED_DIPLOC( 0x01, 0x01, "SW3:1" )
	PORT_DIPUNUSED_DIPLOC( 0x02, 0x02, "SW3:2" )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW3:3" )
	PORT_DIPUNUSED_DIPLOC( 0x08, 0x08, "SW3:4" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW3:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW3:6" )
	PORT_DIPUNUSED_DIPLOC( 0x40, 0x40, "SW3:7" )
	PORT_DIPUNUSED_DIPLOC( 0x80, 0x80, "SW3:8" )

	PORT_START("DSW3")	/* audio board DSW B */
	PORT_DIPUNUSED_DIPLOC( 0x01, 0x01, "SW4:1" )
	PORT_DIPUNUSED_DIPLOC( 0x02, 0x02, "SW4:2" )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW4:3" )
	PORT_DIPUNUSED_DIPLOC( 0x08, 0x08, "SW4:4" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW4:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW4:6" )
	PORT_DIPUNUSED_DIPLOC( 0x40, 0x40, "SW4:7" )
	PORT_DIPUNUSED_DIPLOC( 0x80, 0x80, "SW4:8" )
INPUT_PORTS_END


static INPUT_PORTS_START( megatack )
	PORT_START("IN0")	/* COL. A - from "TEST NO.7 - status locator - coin-door" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME("Do Tests") PORT_CODE(KEYCODE_F1)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME("Select Test") PORT_CODE(KEYCODE_F2)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN1 )

	PORT_START("IN1")	/* COL. B - from "TEST NO.7 - status locator - start sws." */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START("IN2")	/* COL. C - from "TEST NO.8 - status locator - player no.1" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("IN3")	/* COL. D - from "TEST NO.8 - status locator - player no.2" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_COCKTAIL
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_COCKTAIL
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_COCKTAIL
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("DSW0")	/* DSW A - from "TEST NO.6 - dip switch A" */
	PORT_DIPNAME( 0x03, 0x03, "Coinage P1/P2" ) PORT_DIPLOCATION("SW1:1,2")
	PORT_DIPSETTING(    0x03, "1 Credit/2 Credits" )
	PORT_DIPSETTING(    0x02, "2 Credits/3 Credits" )
	PORT_DIPSETTING(    0x01, "2 Credits/4 Credits" )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW1:3" )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Lives ) ) PORT_DIPLOCATION("SW1:4")
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW1:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW1:6" )
	PORT_DIPUNUSED_DIPLOC( 0x40, 0x40, "SW1:7" )
	PORT_DIPUNUSED_DIPLOC( 0x80, 0x80, "SW1:8" )

	PORT_START("DSW1")	/* DSW B - from "TEST NO.6 - dip switch B" */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Bonus_Life ) ) PORT_DIPLOCATION("SW2:1,2,3")
	PORT_DIPSETTING(    0x07, "20000" )
	PORT_DIPSETTING(    0x06, "30000" )
	PORT_DIPSETTING(    0x05, "40000" )
	PORT_DIPSETTING(    0x04, "50000" )
	PORT_DIPSETTING(    0x03, "60000" )
	PORT_DIPSETTING(    0x02, "70000" )
	PORT_DIPSETTING(    0x01, "80000" )
	PORT_DIPSETTING(    0x00, "90000" )
	PORT_DIPUNUSED_DIPLOC( 0x08, 0x08, "SW2:4" )
	PORT_DIPNAME( 0x10, 0x10, "Monitor View" ) PORT_DIPLOCATION("SW2:5")
	PORT_DIPSETTING(    0x10, "Direct" )
	PORT_DIPSETTING(    0x00, "Mirror" )
	PORT_DIPNAME( 0x20, 0x20, "Monitor Orientation" ) PORT_DIPLOCATION("SW2:6")
	PORT_DIPSETTING(    0x20, "Horizontal" )
	PORT_DIPSETTING(    0x00, "Vertical" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) ) PORT_DIPLOCATION("SW2:7")
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) ) PORT_DIPLOCATION("SW2:8")
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START("DSW2")	/* audio board DSW A */
	PORT_DIPNAME( 0x01, 0x00, "Sound Test A 0" ) PORT_DIPLOCATION("SW3:1")
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, "Sound Test A 1" ) PORT_DIPLOCATION("SW3:2")
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, "Sound Test A 2" ) PORT_DIPLOCATION("SW3:3")
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, "Sound Test A 3" ) PORT_DIPLOCATION("SW3:4")
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, "Sound Test A 4" ) PORT_DIPLOCATION("SW3:5")
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, "Sound Test A 5" ) PORT_DIPLOCATION("SW3:6")
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Sound Test A 6" ) PORT_DIPLOCATION("SW3:7")
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Sound Test Enable" ) PORT_DIPLOCATION("SW3:8")
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START("DSW3")	/* audio board DSW B */
	PORT_DIPNAME( 0x01, 0x00, "Sound Test B 0" ) PORT_DIPLOCATION("SW4:1")
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, "Sound Test B 1" ) PORT_DIPLOCATION("SW4:2")
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, "Sound Test B 2" ) PORT_DIPLOCATION("SW4:3")
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, "Sound Test B 3" ) PORT_DIPLOCATION("SW4:4")
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, "Sound Test B 4" ) PORT_DIPLOCATION("SW4:5")
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, "Sound Test B 5" ) PORT_DIPLOCATION("SW4:6")
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Sound Test B 6" ) PORT_DIPLOCATION("SW4:7")
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, "Sound Test B 7" ) PORT_DIPLOCATION("SW4:8")
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END


static INPUT_PORTS_START( challeng )
	PORT_START("IN0")	/* COL. A - from "TEST NO.7 - status locator - coin-door" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME("Do Tests") PORT_CODE(KEYCODE_F1)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME("Select Test") PORT_CODE(KEYCODE_F2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START("IN1")	/* COL. B - from "TEST NO.7 - status locator - start sws." */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START("IN2")	/* COL. C - from "TEST NO.8 - status locator - player no.1" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("IN3")	/* COL. D - from "TEST NO.8 - status locator - player no.2" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_COCKTAIL
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_COCKTAIL
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_COCKTAIL
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_COCKTAIL
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_COCKTAIL
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("DSW0")	/* DSW A - from "TEST NO.6 - dip switch A" */
	PORT_DIPNAME( 0x03, 0x03, "Coinage P1/P2" ) PORT_DIPLOCATION("SW1:1,2")
	PORT_DIPSETTING(    0x03, "1 Credit/2 Credits" )
	PORT_DIPSETTING(    0x02, "2 Credits/3 Credits" )
	PORT_DIPSETTING(    0x01, "2 Credits/4 Credits" )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW1:3" )
	PORT_DIPUNUSED_DIPLOC( 0x08, 0x08, "SW1:4" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW1:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW1:6" )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Lives ) ) PORT_DIPLOCATION("SW1:7,8")
	PORT_DIPSETTING(    0xc0, "3" )
	PORT_DIPSETTING(    0x80, "4" )
	PORT_DIPSETTING(    0x40, "5" )
	PORT_DIPSETTING(    0x00, "6" )

// Manual states information which differs from actual settings for DSW1
// Switches 4 & 5 are factory settings and remain in the OFF position.
// Switches 6 & 7 are factory settings which remain in the ON position.

	PORT_START("DSW1")	/* DSW B - from "TEST NO.6 - dip switch B" */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Bonus_Life ) ) PORT_DIPLOCATION("SW2:1,2,3")
	PORT_DIPSETTING(    0x01, "20000" )
	PORT_DIPSETTING(    0x00, "30000" )
	PORT_DIPSETTING(    0x07, "40000" )
	PORT_DIPSETTING(    0x06, "50000" )
	PORT_DIPSETTING(    0x05, "60000" )
	PORT_DIPSETTING(    0x04, "70000" )
	PORT_DIPSETTING(    0x03, "80000" )
	PORT_DIPSETTING(    0x02, "90000" )
	PORT_DIPUNUSED_DIPLOC( 0x08, 0x08, "SW2:4" )
	PORT_DIPNAME( 0x10, 0x10, "Monitor View" ) PORT_DIPLOCATION("SW2:5")
	PORT_DIPSETTING(    0x10, "Direct" )
	PORT_DIPSETTING(    0x00, "Mirror" )
	PORT_DIPNAME( 0x20, 0x20, "Monitor Orientation" ) PORT_DIPLOCATION("SW2:6")
	PORT_DIPSETTING(    0x20, "Horizontal" )
	PORT_DIPSETTING(    0x00, "Vertical" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) ) PORT_DIPLOCATION("SW2:7")
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) ) PORT_DIPLOCATION("SW2:8")
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START("DSW2")	/* audio board DSW A */
	PORT_DIPNAME( 0x01, 0x00, "Sound Test A 0" ) PORT_DIPLOCATION("SW3:1")
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, "Sound Test A 1" ) PORT_DIPLOCATION("SW3:2")
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, "Sound Test A 2" ) PORT_DIPLOCATION("SW3:3")
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, "Sound Test A 3" ) PORT_DIPLOCATION("SW3:4")
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, "Sound Test A 4" ) PORT_DIPLOCATION("SW3:5")
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, "Sound Test A 5" ) PORT_DIPLOCATION("SW3:6")
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Sound Test A 6" ) PORT_DIPLOCATION("SW3:7")
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Sound Test Enable" ) PORT_DIPLOCATION("SW3:8")
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START("DSW3")	/* audio board DSW B */
	PORT_DIPNAME( 0x01, 0x00, "Sound Test B 0" ) PORT_DIPLOCATION("SW4:1")
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, "Sound Test B 1" ) PORT_DIPLOCATION("SW4:2")
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, "Sound Test B 2" ) PORT_DIPLOCATION("SW4:3")
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, "Sound Test B 3" ) PORT_DIPLOCATION("SW4:4")
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, "Sound Test B 4" ) PORT_DIPLOCATION("SW4:5")
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, "Sound Test B 5" ) PORT_DIPLOCATION("SW4:6")
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Sound Test B 6" ) PORT_DIPLOCATION("SW4:7")
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, "Sound Test B 7" ) PORT_DIPLOCATION("SW4:8")
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END


static INPUT_PORTS_START( kaos )
	PORT_START("IN0")	/* COL. A - from "TEST NO.7 - status locator - coin-door" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME("Do Tests") PORT_CODE(KEYCODE_F1)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME("Select Test") PORT_CODE(KEYCODE_F2)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN1 )

	PORT_START("IN1")	/* COL. B - from "TEST NO.7 - status locator - start sws." */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START("IN2")	/* COL. C - from "TEST NO.8 - status locator - player no.1" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("IN3")	/* COL. D - from "TEST NO.8 - status locator - player no.2" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_COCKTAIL
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_COCKTAIL
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_COCKTAIL
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_COCKTAIL
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_COCKTAIL
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("DSW0")
	PORT_DIPNAME( 0x0f, 0x0e, DEF_STR( Coinage ) ) PORT_DIPLOCATION("SW1:1,2,3,4")
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_8C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_9C ) )
	PORT_DIPSETTING(    0x05, "1 Coin/10 Credits" )
	PORT_DIPSETTING(    0x04, "1 Coin/11 Credits" )
	PORT_DIPSETTING(    0x03, "1 Coin/12 Credits" )
	PORT_DIPSETTING(    0x02, "1 Coin/13 Credits" )
	PORT_DIPSETTING(    0x01, "1 Coin/14 Credits" )
	PORT_DIPSETTING(    0x0f, DEF_STR( 2C_3C ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) ) PORT_DIPLOCATION("SW1:5")
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x60, 0x60, "Max Credits" ) PORT_DIPLOCATION("SW1:6,7")
	PORT_DIPSETTING(    0x60, "10" )
	PORT_DIPSETTING(    0x40, "20" )
	PORT_DIPSETTING(    0x20, "30" )
	PORT_DIPSETTING(    0x00, "40" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Free_Play ) ) PORT_DIPLOCATION("SW1:8")
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START("DSW1")
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Lives ) ) PORT_DIPLOCATION("SW2:1")
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x02, 0x00, "Speed" ) PORT_DIPLOCATION("SW2:2")
	PORT_DIPSETTING(    0x00, "Slow" )
	PORT_DIPSETTING(    0x02, "Fast" )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Bonus_Life ) ) PORT_DIPLOCATION("SW2:3,4")
	PORT_DIPSETTING(    0x0c, "No Bonus" )
	PORT_DIPSETTING(    0x08, "10k" )
	PORT_DIPSETTING(    0x04, "10k 30k" )
	PORT_DIPSETTING(    0x00, "10k 30k 60k" )
	PORT_DIPNAME( 0x10, 0x10, "Number of $" ) PORT_DIPLOCATION("SW2:5")
	PORT_DIPSETTING(    0x10, "8" )
	PORT_DIPSETTING(    0x00, "12" )
	PORT_DIPNAME( 0x20, 0x00, "Bonus erg" ) PORT_DIPLOCATION("SW2:6")
	PORT_DIPSETTING(    0x20, "Every other screen" )
	PORT_DIPSETTING(    0x00, "Every screen" )
	PORT_DIPUNUSED_DIPLOC( 0x40, 0x40, "SW2:7" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) ) PORT_DIPLOCATION("SW2:8")
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START("DSW2")	/* audio board DSW A */
	PORT_DIPUNUSED_DIPLOC( 0x01, 0x01, "SW3:1" )
	PORT_DIPUNUSED_DIPLOC( 0x02, 0x02, "SW3:2" )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW3:3" )
	PORT_DIPUNUSED_DIPLOC( 0x08, 0x08, "SW3:4" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW3:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW3:6" )
	PORT_DIPUNUSED_DIPLOC( 0x40, 0x40, "SW3:7" )
	PORT_DIPUNUSED_DIPLOC( 0x80, 0x80, "SW3:8" )

	PORT_START("DSW3")	/* audio board DSW B */
	PORT_DIPUNUSED_DIPLOC( 0x01, 0x01, "SW4:1" )
	PORT_DIPUNUSED_DIPLOC( 0x02, 0x02, "SW4:2" )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW4:3" )
	PORT_DIPUNUSED_DIPLOC( 0x08, 0x08, "SW4:4" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW4:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW4:6" )
	PORT_DIPUNUSED_DIPLOC( 0x40, 0x40, "SW4:7" )
	PORT_DIPUNUSED_DIPLOC( 0x80, 0x80, "SW4:8" )
INPUT_PORTS_END


static INPUT_PORTS_START( leprechn )
	PORT_START("IN0")	/* COL. A - from "TEST NO.7 - status locator - coin-door" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME("Do Tests") PORT_CODE(KEYCODE_F1)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME("Select Test") PORT_CODE(KEYCODE_F2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN1 )

	PORT_START("IN1")	/* COL. B - from "TEST NO.7 - status locator - start sws." */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START("IN2")	/* COL. C - from "TEST NO.8 - status locator - player no.1" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )

	PORT_START("IN3")	/* COL. D - from "TEST NO.8 - status locator - player no.2" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_COCKTAIL
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_COCKTAIL
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_COCKTAIL
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_COCKTAIL

	PORT_START("DSW0")	/* DSW A - from "TEST NO.6 - dip switch A" */
	PORT_DIPNAME( 0x09, 0x09, DEF_STR( Coin_B ) ) PORT_DIPLOCATION("SW1:1,4")
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_7C ) )
	PORT_DIPNAME( 0x22, 0x22, "Max Credits" ) PORT_DIPLOCATION("SW1:2,6")
	PORT_DIPSETTING(    0x22, "10" )
	PORT_DIPSETTING(    0x20, "20" )
	PORT_DIPSETTING(    0x02, "30" )
	PORT_DIPSETTING(    0x00, "40" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Cabinet ) ) PORT_DIPLOCATION("SW1:3")
	PORT_DIPSETTING(    0x04, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Free_Play ) ) PORT_DIPLOCATION("SW1:5")
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_A ) ) PORT_DIPLOCATION("SW1:7,8")
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_4C ) )

	PORT_START("DSW1")	/* DSW B - from "TEST NO.6 - dip switch B" */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) ) PORT_DIPLOCATION("SW2:1")
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPUNUSED_DIPLOC( 0x02, 0x02, "SW2:2" )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW2:3" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Lives ) ) PORT_DIPLOCATION("SW2:4")
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW2:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW2:6" )
	PORT_DIPNAME( 0xc0, 0x40, DEF_STR( Bonus_Life ) ) PORT_DIPLOCATION("SW2:7,8")
	PORT_DIPSETTING(    0x40, "30000" )
	PORT_DIPSETTING(    0x80, "60000" )
	PORT_DIPSETTING(    0x00, "90000" )
	PORT_DIPSETTING(    0xc0, DEF_STR( None ) )

	PORT_START("DSW2")	/* audio board DSW A */
	PORT_DIPUNUSED_DIPLOC( 0x01, 0x01, "SW3:1" )
	PORT_DIPUNUSED_DIPLOC( 0x02, 0x02, "SW3:2" )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW3:3" )
	PORT_DIPUNUSED_DIPLOC( 0x08, 0x08, "SW3:4" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW3:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW3:6" )
	PORT_DIPUNUSED_DIPLOC( 0x40, 0x40, "SW3:7" )
	PORT_DIPUNUSED_DIPLOC( 0x80, 0x80, "SW3:8" )

	PORT_START("DSW3")	/* audio board DSW B */
	PORT_DIPUNUSED_DIPLOC( 0x01, 0x01, "SW4:1" )
	PORT_DIPUNUSED_DIPLOC( 0x02, 0x02, "SW4:2" )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW4:3" )
	PORT_DIPUNUSED_DIPLOC( 0x08, 0x08, "SW4:4" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW4:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW4:6" )
	PORT_DIPUNUSED_DIPLOC( 0x40, 0x40, "SW4:7" )
	PORT_DIPUNUSED_DIPLOC( 0x80, 0x80, "SW4:8" )
INPUT_PORTS_END

#if 0
static INPUT_PORTS_START( potogold )
        PORT_INCLUDE(leprechn)
        PORT_MODIFY("DSW1")
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Lives ) ) PORT_DIPLOCATION("SW2:4")
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x00, "4" )
INPUT_PORTS_END
#endif

static INPUT_PORTS_START( piratetr )
	PORT_START("IN0")	/* COL. A - from "TEST NO.7 - status locator - coin-door" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME("Do Tests") PORT_CODE(KEYCODE_F1)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME("Select Test") PORT_CODE(KEYCODE_F2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN1 )

	PORT_START("IN1")	/* COL. B - from "TEST NO.7 - status locator - start sws." */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START("IN2")	/* COL. C - from "TEST NO.8 - status locator - player no.1" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )

	PORT_START("IN3")	/* COL. D - from "TEST NO.8 - status locator - player no.2" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_COCKTAIL
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_COCKTAIL
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_COCKTAIL
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_COCKTAIL

	PORT_START("DSW0")	/* DSW A - from "TEST NO.6 - dip switch A" */
	PORT_DIPNAME( 0x09, 0x09, DEF_STR( Coin_B ) ) PORT_DIPLOCATION("SW1:1,4")
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_7C ) )
	PORT_DIPNAME( 0x22, 0x22, "Max Credits" ) PORT_DIPLOCATION("SW1:2,6")
	PORT_DIPSETTING(    0x22, "10" )
	PORT_DIPSETTING(    0x20, "20" )
	PORT_DIPSETTING(    0x02, "30" )
	PORT_DIPSETTING(    0x00, "40" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Cabinet ) ) PORT_DIPLOCATION("SW1:3")
	PORT_DIPSETTING(    0x04, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Free_Play ) ) PORT_DIPLOCATION("SW1:5")
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_A ) ) PORT_DIPLOCATION("SW1:7,8")
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_4C ) )

	PORT_START("DSW1")	/* DSW B - from "TEST NO.6 - dip switch B" */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) ) PORT_DIPLOCATION("SW2:1")
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Stringing Check" ) PORT_DIPLOCATION("SW2:2")
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW2:3" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Lives ) ) PORT_DIPLOCATION("SW2:4")
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW2:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW2:6" )
	PORT_DIPNAME( 0xc0, 0x40, DEF_STR( Bonus_Life ) ) PORT_DIPLOCATION("SW2:7,8")
	PORT_DIPSETTING(    0x40, "30000" )
	PORT_DIPSETTING(    0x80, "60000" )
	PORT_DIPSETTING(    0x00, "90000" )
	PORT_DIPSETTING(    0xc0, DEF_STR( None ) )

	PORT_START("DSW2")	/* audio board DSW A */
	PORT_DIPUNUSED_DIPLOC( 0x01, 0x01, "SW3:1" )
	PORT_DIPUNUSED_DIPLOC( 0x02, 0x02, "SW3:2" )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW3:3" )
	PORT_DIPUNUSED_DIPLOC( 0x08, 0x08, "SW3:4" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW3:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW3:6" )
	PORT_DIPUNUSED_DIPLOC( 0x40, 0x40, "SW3:7" )
	PORT_DIPUNUSED_DIPLOC( 0x80, 0x80, "SW3:8" )

	PORT_START("DSW3")	/* audio board DSW B */
	PORT_DIPUNUSED_DIPLOC( 0x01, 0x01, "SW4:1" )
	PORT_DIPUNUSED_DIPLOC( 0x02, 0x02, "SW4:2" )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW4:3" )
	PORT_DIPUNUSED_DIPLOC( 0x08, 0x08, "SW4:4" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW4:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW4:6" )
	PORT_DIPUNUSED_DIPLOC( 0x40, 0x40, "SW4:7" )
	PORT_DIPUNUSED_DIPLOC( 0x80, 0x80, "SW4:8" )
INPUT_PORTS_END



/*************************************
 *
 *  Machine drivers
 *
 *************************************/

static const ay8910_interface ay8910_config =
{
	AY8910_LEGACY_OUTPUT,
	AY8910_DEFAULT_LOADS,
	DEVCB_INPUT_PORT("DSW2"),
	DEVCB_INPUT_PORT("DSW3")
};


static MACHINE_DRIVER_START( gameplan )

	MDRV_DRIVER_DATA(gameplan_state)

	/* basic machine hardware */
	MDRV_CPU_ADD("maincpu", M6502, GAMEPLAN_MAIN_CPU_CLOCK)
	MDRV_CPU_PROGRAM_MAP(gameplan_main_map)

	MDRV_CPU_ADD("audiocpu", M6502, GAMEPLAN_AUDIO_CPU_CLOCK)
	MDRV_CPU_PROGRAM_MAP(gameplan_audio_map)

	MDRV_RIOT6532_ADD("riot", GAMEPLAN_AUDIO_CPU_CLOCK, r6532_interface)

	MDRV_MACHINE_START(gameplan)
	MDRV_MACHINE_RESET(gameplan)

	/* video hardware */
	MDRV_IMPORT_FROM(gameplan_video)

	/* audio hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")

	MDRV_SOUND_ADD("ay", AY8910, GAMEPLAN_AY8910_CLOCK)
	MDRV_SOUND_CONFIG(ay8910_config)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.33)

	/* via */
	MDRV_VIA6522_ADD("via6522_0", 0, gameplan_via_0_interface)
	MDRV_VIA6522_ADD("via6522_1", 0, via_1_interface)
	MDRV_VIA6522_ADD("via6522_2", 0, via_2_interface)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( leprechn )

	MDRV_IMPORT_FROM(gameplan)
	MDRV_CPU_REPLACE("maincpu", M6502, LEPRECHAUN_MAIN_CPU_CLOCK)

	/* basic machine hardware */
	MDRV_CPU_MODIFY("audiocpu")
	MDRV_CPU_PROGRAM_MAP(leprechn_audio_map)

	/* video hardware */
	MDRV_IMPORT_FROM(leprechn_video)

	/* via */
	MDRV_DEVICE_REMOVE("via6522_0")
	MDRV_VIA6522_ADD("via6522_0", 0, leprechn_via_0_interface)
MACHINE_DRIVER_END



/*************************************
 *
 *  ROM defintions
 *
 *************************************/

ROM_START( killcom )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "killcom.e2",   0xc000, 0x0800, CRC(a01cbb9a) SHA1(a8769243adbdddedfda5f3c8f054e9281a0eca46) )
	ROM_LOAD( "killcom.f2",   0xc800, 0x0800, CRC(bb3b4a93) SHA1(a0ea61ac30a4d191db619b7bfb697914e1500036) )
	ROM_LOAD( "killcom.g2",   0xd000, 0x0800, CRC(86ec68b2) SHA1(a09238190d61684d943ce0acda25eb901d2580ac) )
	ROM_LOAD( "killcom.j2",   0xd800, 0x0800, CRC(28d8c6a1) SHA1(d9003410a651221e608c0dd20d4c9c60c3b0febc) )
	ROM_LOAD( "killcom.j1",   0xe000, 0x0800, CRC(33ef5ac5) SHA1(42f839ad295d3df457ced7886a0003eff7e6c4ae) )
	ROM_LOAD( "killcom.g1",   0xe800, 0x0800, CRC(49cb13e2) SHA1(635e4665042ddd9b8c0b9f275d4bcc6830dc6a98) )
	ROM_LOAD( "killcom.f1",   0xf000, 0x0800, CRC(ef652762) SHA1(414714e5a3f83916bd3ae54afe2cb2271ee9008b) )
	ROM_LOAD( "killcom.e1",   0xf800, 0x0800, CRC(bc19dcb7) SHA1(eb983d2df010c12cb3ffb584fceafa54a4e956b3) )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "killsnd.e1",   0xe000, 0x0800, CRC(77d4890d) SHA1(a3ed7e11dec5d404f022c521256ff50aa6940d3c) )
ROM_END

ROM_START( megatack )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "megattac.e2",  0xc000, 0x0800, CRC(33fa5104) SHA1(15693eb540563e03502b53ed8a83366e395ca529) )
	ROM_LOAD( "megattac.f2",  0xc800, 0x0800, CRC(af5e96b1) SHA1(5f6ab47c12d051f6af446b08f3cd459fbd2c13bf) )
	ROM_LOAD( "megattac.g2",  0xd000, 0x0800, CRC(670103ea) SHA1(e11f01e8843ed918c6ea5dda75319dc95105d345) )
	ROM_LOAD( "megattac.j2",  0xd800, 0x0800, CRC(4573b798) SHA1(388db11ab114b3575fe26ed65bbf49174021939a) )
	ROM_LOAD( "megattac.j1",  0xe000, 0x0800, CRC(3b1d01a1) SHA1(30bbf51885b1e510b8d21cdd82244a455c5dada0) )
	ROM_LOAD( "megattac.g1",  0xe800, 0x0800, CRC(eed75ef4) SHA1(7c02337344f2716d2f2771229f7dee7b651eeb95) )
	ROM_LOAD( "megattac.f1",  0xf000, 0x0800, CRC(c93a8ed4) SHA1(c87e2f13f2cc00055f4941c272a3126b165a6252) )
	ROM_LOAD( "megattac.e1",  0xf800, 0x0800, CRC(d9996b9f) SHA1(e71d65b695000fdfd5fd1ce9ae39c0cb0b61669e) )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "megatsnd.e1",  0xe000, 0x0800, CRC(0c186bdb) SHA1(233af9481a3979971f2d5aa75ec8df4333aa5e0d) )
ROM_END

ROM_START( challeng )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "chall.6",      0xa000, 0x1000, CRC(b30fe7f5) SHA1(ce93a57d626f90d31ddedbc35135f70758949dfa) )
	ROM_LOAD( "chall.5",      0xb000, 0x1000, CRC(34c6a88e) SHA1(250577e2c8eb1d3a78cac679310ec38924ac1fe0) )
	ROM_LOAD( "chall.4",      0xc000, 0x1000, CRC(0ddc18ef) SHA1(9f1aa27c71d7e7533bddf7de420c06fb0058cf60) )
	ROM_LOAD( "chall.3",      0xd000, 0x1000, CRC(6ce03312) SHA1(69c047f501f327f568fe4ad1274168f9dda1ca70) )
	ROM_LOAD( "chall.2",      0xe000, 0x1000, CRC(948912ad) SHA1(e79738ab94501f858f4d5f218787267523611e92) )
	ROM_LOAD( "chall.1",      0xf000, 0x1000, CRC(7c71a9dc) SHA1(2530ada6390fb46c44bf7bf2636910ee54786493) )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "chall.snd",    0xe000, 0x0800, CRC(1b2bffd2) SHA1(36ceb5abbc92a17576c375019f1c5900320398f9) )
ROM_END

ROM_START( kaos )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "kaosab.g2",    0x9000, 0x0800, CRC(b23d858f) SHA1(e31fa657ace34130211a0b9fc0d115fd89bb20dd) )
	ROM_CONTINUE(		   	  0xd000, 0x0800 )
	ROM_LOAD( "kaosab.j2",    0x9800, 0x0800, CRC(4861e5dc) SHA1(96ca0b8625af3897bd4a50a45ea964715f9e4973) )
	ROM_CONTINUE(		   	  0xd800, 0x0800 )
	ROM_LOAD( "kaosab.j1",    0xa000, 0x0800, CRC(e055db3f) SHA1(099176629723c1a9bdc59f440339b2e8c38c3261) )
	ROM_CONTINUE(		   	  0xe000, 0x0800 )
	ROM_LOAD( "kaosab.g1",    0xa800, 0x0800, CRC(35d7c467) SHA1(6d5bfd29ff7b96fed4b24c899ddd380e47e52bc5) )
	ROM_CONTINUE(		   	  0xe800, 0x0800 )
	ROM_LOAD( "kaosab.f1",    0xb000, 0x0800, CRC(995b9260) SHA1(580896aa8b6f0618dc532a12d0795b0d03f7cadd) )
	ROM_CONTINUE(		   	  0xf000, 0x0800 )
	ROM_LOAD( "kaosab.e1",    0xb800, 0x0800, CRC(3da5202a) SHA1(6b5aaf44377415763aa0895c64765a4b82086f25) )
	ROM_CONTINUE(		   	  0xf800, 0x0800 )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "kaossnd.e1",   0xe000, 0x0800, CRC(ab23d52a) SHA1(505f3e4a56e78a3913010f5484891f01c9831480) )
ROM_END

ROM_START( leprechn )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "lep1",         0x8000, 0x1000, CRC(2c4a46ca) SHA1(28a157c1514bc9f27cc27baddb83cf1a1887f3d1) )
	ROM_LOAD( "lep2",         0x9000, 0x1000, CRC(6ed26b3e) SHA1(4ee5d09200d9e8f94ae29751c8ee838faa268f15) )
	ROM_LOAD( "lep3",         0xa000, 0x1000, CRC(a2eaa016) SHA1(be992ee787766137fd800ec59529c98ef2e6991e) )
	ROM_LOAD( "lep4",         0xb000, 0x1000, CRC(6c12a065) SHA1(2acae6a5b94cbdcc550cee88a7be9254fdae908c) )
	ROM_LOAD( "lep5",         0xc000, 0x1000, CRC(21ddb539) SHA1(b4dd0a1916adc076fa6084c315459fcb2522161e) )
	ROM_LOAD( "lep6",         0xd000, 0x1000, CRC(03c34dce) SHA1(6dff202e1a3d0643050f3287f6b5906613d56511) )
	ROM_LOAD( "lep7",         0xe000, 0x1000, CRC(7e06d56d) SHA1(5f68f2047969d803b752a4cd02e0e0af916c8358) )
	ROM_LOAD( "lep8",         0xf000, 0x1000, CRC(097ede60) SHA1(5509c41167c066fa4e7f4f4bd1ce9cd00773a82c) )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "lepsound",     0xe000, 0x1000, CRC(6651e294) SHA1(ce2875fc4df61a30d51d3bf2153864b562601151) )
ROM_END

#if 0
ROM_START( potogold )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "pog.pg1",      0x8000, 0x1000, CRC(9f1dbda6) SHA1(baf20e9a0793c0f1529396f95a820bd1f9431465) )
	ROM_LOAD( "pog.pg2",      0x9000, 0x1000, CRC(a70e3811) SHA1(7ee306dc7d75a7d3fd497870ec92bef9d86535e9) )
	ROM_LOAD( "pog.pg3",      0xa000, 0x1000, CRC(81cfb516) SHA1(12732707e2a51ec39563f2d1e898cc567ab688f0) )
	ROM_LOAD( "pog.pg4",      0xb000, 0x1000, CRC(d61b1f33) SHA1(da024c0776214b8b5a3e49401c4110e86a1bead1) )
	ROM_LOAD( "pog.pg5",      0xc000, 0x1000, CRC(eee7597e) SHA1(9b5cd293580c5d212f8bf39286070280d55e4cb3) )
	ROM_LOAD( "pog.pg6",      0xd000, 0x1000, CRC(25e682bc) SHA1(085d2d553ec10f2f830918df3a7fb8e8c1e5d18c) )
	ROM_LOAD( "pog.pg7",      0xe000, 0x1000, CRC(84399f54) SHA1(c90ba3e3120adda2785ab5abd309e0a703d39f8b) )
	ROM_LOAD( "pog.pg8",      0xf000, 0x1000, CRC(9e995a1a) SHA1(5c525e6c161d9d7d646857b27cecfbf8e0943480) )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "pog.snd",      0xe000, 0x1000, CRC(ec61f0a4) SHA1(26944ecc3e7413259928c8b0a74b2260e67d2c4e) )
ROM_END
#endif
#if 0
ROM_START( leprechp )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "lep1",         0x8000, 0x1000, CRC(2c4a46ca) SHA1(28a157c1514bc9f27cc27baddb83cf1a1887f3d1) )
	ROM_LOAD( "lep2",         0x9000, 0x1000, CRC(6ed26b3e) SHA1(4ee5d09200d9e8f94ae29751c8ee838faa268f15) )
	ROM_LOAD( "3u15.bin",     0xa000, 0x1000, CRC(b5f79fd8) SHA1(271f7b55ecda5bb99f40687264256b82649e2141) )
	ROM_LOAD( "lep4",         0xb000, 0x1000, CRC(6c12a065) SHA1(2acae6a5b94cbdcc550cee88a7be9254fdae908c) )
	ROM_LOAD( "lep5",         0xc000, 0x1000, CRC(21ddb539) SHA1(b4dd0a1916adc076fa6084c315459fcb2522161e) )
	ROM_LOAD( "lep6",         0xd000, 0x1000, CRC(03c34dce) SHA1(6dff202e1a3d0643050f3287f6b5906613d56511) )
	ROM_LOAD( "lep7",         0xe000, 0x1000, CRC(7e06d56d) SHA1(5f68f2047969d803b752a4cd02e0e0af916c8358) )
	ROM_LOAD( "lep8",         0xf000, 0x1000, CRC(097ede60) SHA1(5509c41167c066fa4e7f4f4bd1ce9cd00773a82c) )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "lepsound",     0xe000, 0x1000, CRC(6651e294) SHA1(ce2875fc4df61a30d51d3bf2153864b562601151) )
ROM_END
#endif

ROM_START( piratetr )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "1u13.bin",     0x8000, 0x1000, CRC(4433bb61) SHA1(eee0d7f356118f8595dd7533541db744a63a8176) )
	ROM_LOAD( "2u14.bin",     0x9000, 0x1000, CRC(9bdc4b77) SHA1(ebaab8b3024efd3d0b76647085d441ca204ad5d5) )
	ROM_LOAD( "3u15.bin",     0xa000, 0x1000, CRC(ebced718) SHA1(3a2f4385347f14093360cfa595922254c9badf1a) )
	ROM_LOAD( "4u16.bin",     0xb000, 0x1000, CRC(f494e657) SHA1(83a31849de8f4f70d7547199f229079f491ddc61) )
	ROM_LOAD( "5u17.bin",     0xc000, 0x1000, CRC(2789d68e) SHA1(af8f334ce4938cd75143b729c97cfbefd68c9e13) )
	ROM_LOAD( "6u18.bin",     0xd000, 0x1000, CRC(d91abb3a) SHA1(11170e69686c2a1f2dc31d41516f44b612f99bad) )
	ROM_LOAD( "7u19.bin",     0xe000, 0x1000, CRC(6e8808c4) SHA1(d1f76fd37d8f78552a9d53467073cc9a571d96ce) )
	ROM_LOAD( "8u20.bin",     0xf000, 0x1000, CRC(2802d626) SHA1(b0db688500076ee73e0001c00089a8d552c6f607) )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "su31.bin",     0xe000, 0x1000, CRC(2fe86a11) SHA1(aaafe411b9cb3d0221cc2af73d34ad8bb74f8327) )
ROM_END



/*************************************
 *
 *  Game drivers
 *
 *************************************/

GAME( 1980, killcom,  0,        gameplan, killcom,  0, ROT0,   "GamePlan (Centuri license)", "Killer Comet", GAME_SUPPORTS_SAVE )
GAME( 1980, megatack, 0,        gameplan, megatack, 0, ROT0,   "GamePlan (Centuri license)", "Megatack", GAME_SUPPORTS_SAVE )
GAME( 1981, challeng, 0,        gameplan, challeng, 0, ROT0,   "GamePlan (Centuri license)", "Challenger", GAME_SUPPORTS_SAVE )
GAME( 1981, kaos,     0,        gameplan, kaos,     0, ROT270, "GamePlan", "Kaos", GAME_SUPPORTS_SAVE )
GAME( 1982, leprechn, 0,        leprechn, leprechn, 0, ROT0,   "Tong Electronic", "Leprechaun", GAME_SUPPORTS_SAVE )
//GAME( 1982, potogold, leprechn, leprechn, potogold, 0, ROT0,   "GamePlan", "Pot of Gold", GAME_SUPPORTS_SAVE )
//GAME( 1982, leprechp, leprechn, leprechn, potogold, 0, ROT0,   "Tong Electronic", "Leprechaun (Pacific Polytechnical license)", GAME_SUPPORTS_SAVE )
GAME( 1982, piratetr, 0,        leprechn, piratetr, 0, ROT0,   "Tong Electronic", "Pirate Treasure", GAME_SUPPORTS_SAVE )

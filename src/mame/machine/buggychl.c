#include "driver.h"
#include "cpu/z80/z80.h"
#include "includes/buggychl.h"


static UINT8 from_main,from_mcu;
static int mcu_sent,main_sent;

MACHINE_RESET( buggychl )
{
	mcu_sent = 0;
	main_sent = 0;
	cputag_set_input_line(machine, "mcu", 0, CLEAR_LINE);
}


/***************************************************************************

 Buggy CHallenge 68705 protection interface

 This is accurate. FairyLand Story seems to be identical.

***************************************************************************/

static UINT8 portA_in,portA_out,ddrA;

READ8_HANDLER( buggychl_68705_portA_r )
{
//logerror("%04x: 68705 port A read %02x\n",cpu_get_pc(space->cpu),portA_in);
	return (portA_out & ddrA) | (portA_in & ~ddrA);
}

WRITE8_HANDLER( buggychl_68705_portA_w )
{
//logerror("%04x: 68705 port A write %02x\n",cpu_get_pc(space->cpu),data);
	portA_out = data;
}

WRITE8_HANDLER( buggychl_68705_ddrA_w )
{
	ddrA = data;
}



/*
 *  Port B connections:
 *  parts in [ ] are optional (not used by buggychl)
 *
 *  all bits are logical 1 when read (+5V pullup)
 *
 *  0   n.c.
 *  1   W  IRQ ack and enable latch which holds data from main Z80 memory
 *  2   W  loads latch to Z80
 *  3   W  to Z80 BUSRQ (put it on hold?)
 *  4   W  n.c.
 *  5   W  [selects Z80 memory access direction (0 = write 1 = read)]
 *  6   W  [loads the latch which holds the low 8 bits of the address of
 *               the main Z80 memory location to access]
 *  7   W  [loads the latch which holds the high 8 bits of the address of
 *               the main Z80 memory location to access]
 */

static UINT8 portB_in,portB_out,ddrB;

READ8_HANDLER( buggychl_68705_portB_r )
{
	return (portB_out & ddrB) | (portB_in & ~ddrB);
}

WRITE8_HANDLER( buggychl_68705_portB_w )
{
logerror("%04x: 68705 port B write %02x\n", cpu_get_pc(space->cpu), data);

	if ((ddrB & 0x02) && (~data & 0x02) && (portB_out & 0x02))
	{
		portA_in = from_main;
		if (main_sent) cputag_set_input_line(space->machine, "mcu", 0, CLEAR_LINE);
		main_sent = 0;
logerror("read command %02x from main cpu\n", portA_in);
	}
	if ((ddrB & 0x04) && (data & 0x04) && (~portB_out & 0x04))
	{
logerror("send command %02x to main cpu\n", portA_out);
		from_mcu = portA_out;
		mcu_sent = 1;
	}

	portB_out = data;
}

WRITE8_HANDLER( buggychl_68705_ddrB_w )
{
	ddrB = data;
}


/*
 *  Port C connections:
 *
 *  all bits are logical 1 when read (+5V pullup)
 *
 *  0   R  1 when pending command Z80->68705
 *  1   R  0 when pending command 68705->Z80
 */

static UINT8 portC_in,portC_out,ddrC;

READ8_HANDLER( buggychl_68705_portC_r )
{
	portC_in = 0;
	if (main_sent) portC_in |= 0x01;
	if (!mcu_sent) portC_in |= 0x02;
logerror("%04x: 68705 port C read %02x\n", cpu_get_pc(space->cpu), portC_in);
	return (portC_out & ddrC) | (portC_in & ~ddrC);
}

WRITE8_HANDLER( buggychl_68705_portC_w )
{
logerror("%04x: 68705 port C write %02x\n", cpu_get_pc(space->cpu), data);
	portC_out = data;
}

WRITE8_HANDLER( buggychl_68705_ddrC_w )
{
	ddrC = data;
}


WRITE8_HANDLER( buggychl_mcu_w )
{
logerror("%04x: mcu_w %02x\n", cpu_get_pc(space->cpu), data);
	from_main = data;
	main_sent = 1;
	cputag_set_input_line(space->machine, "mcu", 0, ASSERT_LINE);
}

READ8_HANDLER( buggychl_mcu_r )
{
logerror("%04x: mcu_r %02x\n", cpu_get_pc(space->cpu), from_mcu);
	mcu_sent = 0;
	return from_mcu;
}

READ8_HANDLER( buggychl_mcu_status_r )
{
	int res = 0;

	/* bit 0 = when 1, mcu is ready to receive data from main cpu */
	/* bit 1 = when 1, mcu has sent data to the main cpu */
//logerror("%04x: mcu_status_r\n",cpu_get_pc(space->cpu));
	if (!main_sent) res |= 0x01;
	if (mcu_sent) res |= 0x02;

	return res;
}

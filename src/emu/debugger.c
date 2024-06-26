/*********************************************************************

    debugger.c

    Front-end debugger interfaces.

    Copyright Nicola Salmoria and the MAME Team.
    Visit http://mamedev.org for licensing and usage restrictions.

*********************************************************************/

#include "driver.h"
#include "debugger.h"
#include "debug/debugcpu.h"
#include "debug/debugcmd.h"
#include "debug/debugcmt.h"
#include "debug/debugcon.h"
#include "debug/express.h"
#include "debug/debugvw.h"
#include <ctype.h>



/***************************************************************************
    TYPE DEFINITIONS
***************************************************************************/

typedef struct _machine_entry machine_entry;
struct _machine_entry
{
	machine_entry *		next;
	running_machine *	machine;
};



/***************************************************************************
    GLOBAL VARIABLES
***************************************************************************/
#ifndef __WII__
static machine_entry *machine_list;
static int atexit_registered;
#endif


/***************************************************************************
    FUNCTION PROTOTYPES
***************************************************************************/
#ifndef __WII__
static void debugger_exit(running_machine *machine);
#endif


/***************************************************************************
    CENTRAL INITIALIZATION POINT
***************************************************************************/

/*-------------------------------------------------
    debugger_init - start up all subsections
-------------------------------------------------*/

void debugger_init(running_machine *machine)
{
#ifndef __WII__
	/* only if debugging is enabled */
	if (machine->debug_flags & DEBUG_FLAG_ENABLED)
	{
		machine_entry *entry;

		/* initialize the submodules */
		debug_cpu_init(machine);
		debug_command_init(machine);
		debug_console_init(machine);
		debug_view_init(machine);
		debug_comment_init(machine);

		/* allocate a new entry for our global list */
		add_exit_callback(machine, debugger_exit);
		entry = alloc_or_die(machine_entry);
		entry->next = machine_list;
		entry->machine = machine;
		machine_list = entry;

		/* register an atexit handler if we haven't yet */
		if (!atexit_registered)
			atexit(debugger_flush_all_traces_on_abnormal_exit);
		atexit_registered = TRUE;

		/* listen in on the errorlog */
		add_logerror_callback(machine, debug_errorlog_write_line);
	}
#endif
}


/*-------------------------------------------------
    debugger_refresh_display - redraw the current
    video display
-------------------------------------------------*/

void debugger_refresh_display(running_machine *machine)
{
	video_frame_update(machine, TRUE);
}


/*-------------------------------------------------
    debugger_exit - remove ourself from the
    global list of active machines for cleanup
-------------------------------------------------*/
#ifndef __WII__
static void debugger_exit(running_machine *machine)
{
	machine_entry **entryptr;

	/* remove this machine from the list; it came down cleanly */
	for (entryptr = &machine_list; *entryptr != NULL; entryptr = &(*entryptr)->next)
		if ((*entryptr)->machine == machine)
		{
			machine_entry *deleteme = *entryptr;
			*entryptr = deleteme->next;
			free(deleteme);
			break;
		}
}
#endif

/*-------------------------------------------------
    debugger_flush_all_traces_on_abnormal_exit -
    flush any traces in the event of an aborted
    execution
-------------------------------------------------*/

void debugger_flush_all_traces_on_abnormal_exit(void)
{
#ifndef __WII__
	/* clear out the machine list and flush traces on each one */
	while (machine_list != NULL)
	{
		machine_entry *deleteme = machine_list;
		debug_cpu_flush_traces(deleteme->machine);
		machine_list = deleteme->next;
		free(deleteme);
	}
#endif
}

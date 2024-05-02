/***************************************************************************

    state.h

    Save state management functions.

    Copyright Nicola Salmoria and the MAME Team.
    Visit http://mamedev.org for licensing and usage restrictions.

***************************************************************************/

#pragma once

#ifndef __STATE_H__
#define __STATE_H__

#include "mamecore.h"



/***************************************************************************
    TYPE DEFINTIONS
***************************************************************************/

typedef void (*state_presave_func)(running_machine *machine, void *param);
typedef void (*state_postload_func)(running_machine *machine, void *param);



/***************************************************************************
    CONSTANTS
***************************************************************************/

enum _state_save_error
{
	STATERR_NONE,
	STATERR_ILLEGAL_REGISTRATIONS,
	STATERR_INVALID_HEADER,
	STATERR_READ_ERROR,
	STATERR_WRITE_ERROR
};
typedef enum _state_save_error state_save_error;



/***************************************************************************
    MACROS
***************************************************************************/

#define STATE_PRESAVE(name) void name(running_machine *machine, void *param)
#define STATE_POSTLOAD(name) void name(running_machine *machine, void *param)


#define IS_COMPATIBLE_TYPE(_valtype, _checktype)										\
	(sizeof(_valtype) == sizeof(_checktype) && TYPES_COMPATIBLE(_valtype, _checktype))

#define IS_VALID_SAVE_TYPE(_valtype)													\
	(IS_COMPATIBLE_TYPE(_valtype, double) || IS_COMPATIBLE_TYPE(_valtype, float)  ||	\
	 IS_COMPATIBLE_TYPE(_valtype, INT64)  || IS_COMPATIBLE_TYPE(_valtype, UINT64) ||	\
	 IS_COMPATIBLE_TYPE(_valtype, INT32)  || IS_COMPATIBLE_TYPE(_valtype, UINT32) ||	\
	 IS_COMPATIBLE_TYPE(_valtype, INT16)  || IS_COMPATIBLE_TYPE(_valtype, UINT16) ||	\
	 IS_COMPATIBLE_TYPE(_valtype, INT8)   || IS_COMPATIBLE_TYPE(_valtype, UINT8) ||		\
	 IS_COMPATIBLE_TYPE(_valtype, PAIR)   || IS_COMPATIBLE_TYPE(_valtype, PAIR64))


/* generic registration; all further registrations are based on this */
#define state_save_register_generic(_mach, _mod, _tag, _index, _name, _val, _valsize, _count) 		\
do {																								\
	assert_always(IS_VALID_SAVE_TYPE(_valsize), "Invalid data type supplied for state saving.");	\
	state_save_register_memory(_mach, _mod, _tag, _index, _name, _val, sizeof(_valsize), _count);	\
} while (0)


/* register items with explicit tags */
#define state_save_register_item(_mach, _mod, _tag, _index, _val) \
	state_save_register_generic(_mach, _mod, _tag, _index, #_val, &_val, _val, 1)

#define state_save_register_item_pointer(_mach, _mod, _tag, _index, _val, _count) \
	state_save_register_generic(_mach, _mod, _tag, _index, #_val, &_val[0], _val[0], _count)

#define state_save_register_item_array(_mach, _mod, _tag, _index, _val) \
	state_save_register_item_pointer(_mach, _mod, _tag, _index, _val, sizeof(_val)/sizeof(_val[0]))

#define state_save_register_item_2d_array(_mach, _mod, _tag, _index, _val) \
	state_save_register_item_pointer(_mach, _mod, _tag, _index, _val[0], sizeof(_val)/sizeof(_val[0][0]))

#define state_save_register_item_bitmap(_mach, _mod, _tag, _index, _val)	\
	state_save_register_bitmap(_mach, _mod, _tag, _index, #_val, _val)



/* register device items */
#define state_save_register_device_item(_dev, _index, _val) \
	state_save_register_generic((_dev)->machine, device_get_name(_dev), (_dev)->tag, _index, #_val, &_val, _val, 1)

#define state_save_register_device_item_pointer(_dev, _index, _val, _count) \
	state_save_register_generic((_dev)->machine, device_get_name(_dev), (_dev)->tag, _index, #_val, &_val[0], _val[0], _count)

#define state_save_register_device_item_array(_dev, _index, _val) \
	state_save_register_item_pointer((_dev)->machine, device_get_name(_dev), (_dev)->tag, _index, _val, sizeof(_val)/sizeof(_val[0]))

#define state_save_register_device_item_2d_array(_dev, _index, _val) \
	state_save_register_item_pointer((_dev)->machine, device_get_name(_dev), (_dev)->tag, _index, _val[0], sizeof(_val)/sizeof(_val[0][0]))

#define state_save_register_device_item_bitmap(_dev, _index, _val)	\
	state_save_register_bitmap((_dev)->machine, device_get_name(_dev), (_dev)->tag, _index, #_val, _val)



/* register global items */
#define state_save_register_global(_mach, _val) \
	state_save_register_item(_mach, "globals", NULL, 0, _val)

#define state_save_register_global_pointer(_mach, _val, _count) \
	state_save_register_item_pointer(_mach, "globals", NULL, 0, _val, _count)

#define state_save_register_global_array(_mach, _val) \
	state_save_register_item_array(_mach, "globals", NULL, 0, _val)

#define state_save_register_global_2d_array(_mach, _val) \
	state_save_register_item_2d_array(_mach, "globals", NULL, 0, _val)

#define state_save_register_global_bitmap(_mach, _val) \
	state_save_register_bitmap(_mach, "globals", NULL, 0, #_val, _val)



/***************************************************************************
    FUNCTION PROTOTYPES
***************************************************************************/


/* ----- core system operations ----- */

/* initialize the system and reset all registrations */
void state_init(running_machine *machine);

/* return the number of total registrations so far */
int state_save_get_reg_count(running_machine *machine);



/* ----- registration handling ----- */

/* allow/disallow registrations to happen (called by the core) */
void state_save_allow_registration(running_machine *machine, int allowed);

/* query whether or not registrations are allowed */
int state_save_registration_allowed(running_machine *machine);

/* register an array of data in memory */
void state_save_register_memory(running_machine *machine, const char *module, const char *tag, UINT32 index, const char *name, void *val, UINT32 valsize, UINT32 valcount);

/* register a bitmap to be saved */
void state_save_register_bitmap(running_machine *machine, const char *module, const char *tag, UINT32 index, const char *name, bitmap_t *val);



/* ----- callback function registraton ----- */

/* register a pre-save function callback */
void state_save_register_presave(running_machine *machine, state_presave_func func, void *param);

/* register a post-load function callback */
void state_save_register_postload(running_machine *machine, state_postload_func func, void *param);



/* ----- registration freeing ----- */

/* free all registrations that have been tagged with the current resource tag */
void state_destructor(void *ptr, size_t size);



/* ----- save state file processing ----- */

/* check if a file is a valid save state */
state_save_error state_save_check_file(running_machine *machine, mame_file *file, const char *gamename, void (CLIB_DECL *errormsg)(const char *fmt, ...));

/* write out the save state file */
state_save_error state_save_write_file(running_machine *machine, mame_file *file);

/* read in a save state file */
state_save_error state_save_read_file(running_machine *machine, mame_file *file);



/* ----- debugging ----- */

/* return an item with the given index */
const char *state_save_get_indexed_item(running_machine *machine, int index, void **base, UINT32 *valsize, UINT32 *valcount);

/* dump the registry to the logfile */
void state_save_dump_registry(running_machine *machine);


#endif	/* __STATE_H__ */

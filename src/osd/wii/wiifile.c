//============================================================
//
//  Copyright (c) 1996-2009, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//  MAME Wii by Toad King
//
//============================================================
#include <errno.h>
#include "osdcore.h"
#include "wiimame.h"

//============================================================
//  osd_open
//============================================================
file_error osd_open(const char *path, UINT32 openflags, osd_file **file, UINT64 *filesize)
{
	const char *mode;
	FILE *fileptr;

#ifdef WIIFILE_DEBUG
	wii_debug("%s: osd_open %s\n", __FUNCTION__, path);
#endif
	
	// based on the flags, choose a mode
	if (openflags & OPEN_FLAG_WRITE)
	{
		if (openflags & OPEN_FLAG_READ)
			mode = (openflags & OPEN_FLAG_CREATE) ? "w+b" : "r+b";
		else
			mode = "wb";
	}
	else if (openflags & OPEN_FLAG_READ)
		mode = "rb";
	else 
	{
#ifdef WIIFILE_DEBUG		
		wii_debug("%s: failed FILERR_INVALID_ACCESS\n",__FUNCTION__);
#endif
		return FILERR_INVALID_ACCESS;
	}
	// open the file
	fileptr = fopen(path, mode);
	if (fileptr == NULL) 
	{
#ifdef WIIFILE_DEBUG
		wii_debug("%s: failed FILERR_NOT_FOUND - %d\n",__FUNCTION__, errno);
#endif
		return FILERR_NOT_FOUND;
	}
	// store the file pointer directly as an osd_file
	*file = (osd_file *)fileptr;

	// get the size -- note that most fseek/ftell implementations are limited to 32 bits
	fseek(fileptr, 0, SEEK_END);
	*filesize = (UINT64)ftell(fileptr);
	fseek(fileptr, 0, SEEK_SET);
#ifdef WIIFILE_DEBUG
	wii_debug("%s: OK\n",__FUNCTION__);
#endif
	return FILERR_NONE;
}

//============================================================
//  osd_close
//============================================================
file_error osd_close(osd_file *file)
{
#ifdef WIIFILE_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif	
	// close the file handle
	fclose((FILE *)file);
	return FILERR_NONE;
}

//============================================================
//  osd_read
//============================================================
file_error osd_read(osd_file *file, void *buffer, UINT64 offset, UINT32 length, UINT32 *actual)
{
	size_t count;
#ifdef WIIFILE_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif
	// seek to the new location; note that most fseek implementations are limited to 32 bits
	fseek((FILE *)file, offset, SEEK_SET);

	// perform the read
	count = fread(buffer, 1, length, (FILE *)file);
	if (actual != NULL)
		*actual = (UINT32)count;

	return FILERR_NONE;
}

//============================================================
//  osd_write
//============================================================
file_error osd_write(osd_file *file, const void *buffer, UINT64 offset, UINT32 length, UINT32 *actual)
{
	size_t count;
#ifdef WIIFILE_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif
	// seek to the new location; note that most fseek implementations are limited to 32 bits
	fseek((FILE *)file, offset, SEEK_SET);

	// perform the write
	count = fwrite(buffer, 1, length, (FILE *)file);
	if (actual != NULL)
		*actual = (UINT32)count;

	return FILERR_NONE;
}

//============================================================
//  osd_rmfile
//============================================================
file_error osd_rmfile(const char *filename)
{
#ifdef WIIFILE_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif
	return remove(filename) ? FILERR_FAILURE : FILERR_NONE;
}

//============================================================
//  osd_get_physical_drive_geometry
//============================================================
int osd_get_physical_drive_geometry(const char *filename, UINT32 *cylinders, UINT32 *heads, UINT32 *sectors, UINT32 *bps)
{
	// there is no standard way of doing this, so we always return FALSE, indicating
	// that a given path is not a physical drive
#ifdef WIIFILE_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif
	return FALSE;
}

//============================================================
//  osd_uchar_from_osdchar
//============================================================
int osd_uchar_from_osdchar(UINT32 /* unicode_char */ *uchar, const char *osdchar, size_t count)
{
#ifdef WIIFILE_DEBUG
	wii_debug("%s: Start\n",__FUNCTION__);
#endif
	// we assume a standard 1:1 mapping of characters to the first 256 unicode characters
	*uchar = (UINT8)*osdchar;
	return 1;
}

//============================================================
//
//  Copyright (c) 1996-2009, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//  MAME Wii by Toad King
//
//============================================================
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "osdcore.h"
#include "wiimame.h"

#define PATHSEPCH '/'

struct _osd_directory
{
	osd_directory_entry ent;
	struct dirent *data;
	DIR *fd;
};

static osd_dir_entry_type get_attributes_stat(const char *file)
{
	struct stat st;
#ifdef WIIDIR_DEBUG	
	wii_debug("%s: Start\n",__FUNCTION__);
#endif
	if(stat(file, &st))
		return 0;

	if (S_ISDIR(st.st_mode)) 
		return ENTTYPE_DIR;

	return ENTTYPE_FILE;
}

static UINT64 osd_get_file_size(const char *file)
{
	struct stat st;
#ifdef WIIDIR_DEBUG	
	wii_debug("%s: Start\n",__FUNCTION__);
#endif
	if(stat(file, &st))
		return 0;

	return st.st_size;
}

//============================================================
//  osd_opendir
//============================================================
osd_directory *osd_opendir(const char *dirname)
{
	osd_directory *dir = NULL;
#ifdef WIIDIR_DEBUG	
	wii_debug("%s: Start\n",__FUNCTION__);
#endif
	dir = malloc(sizeof(osd_directory));
	if (dir != NULL) {
		memset(dir, 0, sizeof(osd_directory));
		
		if(dirname[0] != '$')
			dir->fd = opendir(dirname);
		else {
			char *envstr, *envval;
			int i, j, ll;
			
			ll = strlen(dirname)+1;
			envstr = malloc(ll);
			strcpy(envstr, dirname);
			
			i = 0;
			while (envstr[i] != PATHSEPCH && envstr[i] != 0 && envstr[i] != '.') {
				i++;
			}
			envstr[i] = '\0';

			envval = getenv(&envstr[1]);
			if (envval != NULL) {
				char *tmpstr;
				
				j = strlen(envval) + ll;
				tmpstr = malloc(j);
		
				// start with the value of $HOME
				strcpy(tmpstr, envval);
				// replace the null with a path separator again
				envstr[i] = PATHSEPCH;
				// append it
				strcat(tmpstr, &envstr[i]);
				
				dir->fd = opendir(tmpstr);
				free(tmpstr);
			}
			else {
				wii_debug("%s: Warning: osd_opendir environment variable %s not found.\n", __FUNCTION__, envstr);
				dir->fd = opendir(dirname);
			}
			free(envstr);
		}
		
		if (dir->fd == NULL) {
			free(dir);
			dir = NULL;
		}
	}
	return dir;
}

//============================================================
//  osd_readdir
//============================================================
const osd_directory_entry *osd_readdir(osd_directory *dir)
{
#ifdef WIIDIR_DEBUG	
	wii_debug("%s: Start\n",__FUNCTION__);
#endif
	dir->data = readdir(dir->fd);

	if (dir->data == NULL)
		return NULL;

	dir->ent.name = dir->data->d_name;
	dir->ent.type = get_attributes_stat(dir->data->d_name);
	dir->ent.size = osd_get_file_size(dir->data->d_name);
	return &dir->ent;
}

//============================================================
//  osd_closedir
//============================================================
void osd_closedir(osd_directory *dir)
{
#ifdef WIIDIR_DEBUG	
	wii_debug("%s: Start\n",__FUNCTION__);
#endif
	if(dir != NULL) {
		if (dir->fd != NULL)
			closedir(dir->fd);
		free(dir);
	}
}

//============================================================
//  osd_is_absolute_path
//============================================================
int osd_is_absolute_path(const char *path)
{
        int result;
#ifdef WIIDIR_DEBUG	
	wii_debug("%s: Start\n",__FUNCTION__);
#endif
        
        if ((path[0] == '/') || (path[0] == '\\'))
                result = TRUE;
        else if (path[0] == '.')
                result = TRUE;
        else
                result = FALSE;
        return result;
}

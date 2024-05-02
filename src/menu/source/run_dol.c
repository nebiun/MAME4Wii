#include <string.h>
#include <gccore.h>
#include <ogc/lwp_threads.h>

#include <sys/stat.h>
#include <unistd.h>
#include <fat.h>

#include <stdio.h>
#include <limits.h>


typedef struct _dol_header_t {
	u32 textFileAddress[7];
	u32 dataFileAddress[11];
	u32 textMemAddress[7];
	u32 dataMemAddress[11];
	u32	textSize[7];
	u32	dataSize[11];
	u32	bssMemAddress;
	u32	bssSize;
	u32	entry;
} _dol_header;

static _dol_header dolHeader;
typedef void (*entrypoint) (void);

static char argvBuffer[4096];
extern char _start[];

bool checkAddress(u32 start, u32 size ) {
	
	return true;
}


bool loadSections( int number, u32 fileAddress[], u32 memAddress[], u32 size[], FILE* dolFile ) {

	int i;

	for ( i = 0; i < number; i++ ) {

		if ( fileAddress[i] != 0 ) {
			if ( !checkAddress( memAddress[i], size[i] ) ) return false;
			
			fseek( dolFile, fileAddress[i], SEEK_SET );
			
			if ( size[i] != fread( (void *)memAddress[i], 1, size[i], dolFile) ) return false;
			
			DCFlushRange ((void *) memAddress[i], size[i]);
		}
	}
	return true;
} 

void setArgv (int argc, const char **argv, char *argStart, struct __argv *dol_argv) {
	char* argData;
	int argSize;
	const char* argChar;

	// Give arguments to dol
	argData = (char*)argStart;
	argSize = 0;
	
	for (; argc > 0 && *argv; ++argv, --argc) {
		for (argChar = *argv; *argChar != 0; ++argChar, ++argSize) {
			*(argData++) = *argChar;
		}
		*(argData++) = 0;
		++argSize;
	}
	*(argData++) = 0;
	++argSize;
		
	dol_argv->argvMagic = ARGV_MAGIC;
	dol_argv->commandLine = argStart;
	dol_argv->length = argSize;
	
	DCFlushRange ((void *) argStart, argSize);
	DCFlushRange ((void *) dol_argv, sizeof(struct __argv));
}

bool runDOL (const char* filename, int argc, const char** argv) {
	struct stat st;
	char filePath[PATH_MAX * 2];
	int pathLen;
	const char* args[1];
	
	if (stat (filename, &st) < 0) {
		return false;
	}

	if (argc <= 0 || !argv) {
		// Construct a command line if we weren't supplied with one
		if (!getcwd (filePath, PATH_MAX)) {
			return false;
		}
		pathLen = strlen (filePath);
		strcpy (filePath + pathLen, filename);
		args[0] = filePath;
		argv = args;
	}
	
	FILE *dolFile = fopen(filename,"rb");
	if ( dolFile == NULL ) return false;

	u32 *entryPoint;

	if ( sizeof(dolHeader) == fread(&dolHeader,1, sizeof(dolHeader), dolFile) ) {

		if ( loadSections( 7, dolHeader.textFileAddress, dolHeader.textMemAddress, dolHeader.textSize, dolFile ) ) {

			if ( loadSections( 11, dolHeader.dataFileAddress, dolHeader.dataMemAddress, dolHeader.dataSize, dolFile ) ) {

				entryPoint = (u32*)dolHeader.entry;

				if ( entryPoint[1] == ARGV_MAGIC ) {
					setArgv( argc, argv, argvBuffer, (struct __argv*)&entryPoint[2]);
				}

				fclose(dolFile);

				SYS_ResetSystem(SYS_SHUTDOWN,0,0);

				__lwp_thread_stopmultitasking((entrypoint)entryPoint);

			}
		}
	} 

	fclose(dolFile);
	return false;
}

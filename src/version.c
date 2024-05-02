/***************************************************************************

    version.c

    Version string source file for MAME.

    Copyright Nicola Salmoria and the MAME Team.
    Visit http://mamedev.org for licensing and usage restrictions.

***************************************************************************/
#if 0
#if ( __WII_BUILD__ == 1 )
#define VERSION		"MAME4WII_1"
#elif ( __WII_BUILD__ == 2 )
#define VERSION		"MAME4WII_2"
#elif ( __WII_BUILD__ == 3 )
#define VERSION		"MAME4WII_3"	
#elif ( __WII_BUILD__ == 4 )
#define VERSION		"MAME4WII_4"	
#elif ( __WII_BUILD__ == 1234 )
#define VERSION		"MAME4WII_ALL"
#elif ( __WII_BUILD__ == 0 )
#define VERSION		"MAME4WII_SINGLE"	
#else
#define VERSION		"MAME4WII_Nebiun"
#endif
const char build_version[] = "0.135.2 "VERSION" ("__DATE__")";
#else
const char build_version[] = WII_VERSION" "WII_BUILD" ("__DATE__")";
#endif



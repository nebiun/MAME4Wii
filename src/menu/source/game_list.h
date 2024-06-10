/* MAME4WII
 *
 */
#include <grrlib.h>

#define M4W_HOME			"sd:/mame4wii"
#define M4W_HOMEUSB			"usb:/mame4wii"
#define	M4W_LIBS			"libs"
#define M4W_LOGDIR			"logs"
#define M4W_CRASHDIR		"crash"
#define	M4W_ROMS			"roms"
#define	M4W_SNAP			"snap"
#define M4W_BYDESC	1
#define M4W_BYROM	2

#define M4W_TEXT_HEIGHT		20
#define M4W_TEXT_COLOR		0xFFFFFFFF
#define M4W_GAMES_COLOR		0x00FF00FF
#define M4W_NOTWORK_COLOR	0x0000FFFF
#define M4W_CRASH_COLOR		0xFF0000FF
#define M4W_CURRENT_COLOR	0xFFFFFFFF
#define M4W_MISSING_COLOR	0xFFA500FF
#define M4W_NOLIB_COLOR		0xFFFF00FF
#define M4W_SELECT_COLOR	0x00FFFFFF

#define M4W_FLAGS_WORKING		0x0
#define M4W_FLAGS_ROMMISSING	0x1
#define M4W_FLAGS_NOTWORKING	0x2
#define M4W_FLAGS_DOLMISSING	0x4
#define M4W_FLAGS_TOOBIG		0x8
#define M4W_FLAGS_CHECKED		0x10
#define M4W_FLAGS_CRASHED		0x20
#define M4W_FLAGS_JOYSTICK		0x100
#define M4W_FLAGS_ROMONUSB		0x1000
#define M4W_FLAGS_DOLONUSB		0x2000
#define M4W_FLAGS_PNGONUSB		0x4000
#define M4W_FLAGS_PNGONSD		0x8000

#define M4W_RUNNABLE(x)		(((x) & (M4W_FLAGS_ROMMISSING|M4W_FLAGS_DOLMISSING)) == 0)  
#define M4W_HASSNAP(x)		(((x) & (M4W_FLAGS_PNGONUSB|M4W_FLAGS_PNGONUSB)) == 0) 

typedef struct {
	const char *rom;
	const char *desc;
	const char *exe;
	u16 flags;
} game_entry_t;

extern void m4w_debug(const char *format, ...);
extern game_entry_t *initGamelist(int *n_games);
extern game_entry_t *searchPattern(const char *pattern, int type);
extern int check_game(game_entry_t *p);
extern void print_ttf(int x, int y, const u32 color, const char *format, ...);
extern void show_logo(void);

/* MAME4WII
 *
 */
#include <grrlib.h>

#define M4W_HOME 			"sd:/mame" 
#define	M4W_LIBS			M4W_HOME"/libs"
#define M4W_LOGDIR			M4W_HOME"/logs"
#define M4W_CRASHDIR		M4W_HOME"/crash"
#define	M4W_ROMS			M4W_HOME"/roms"
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

#define M4W_FLAGS_WORKING		0x00
#define M4W_FLAGS_ROMMISSING	0x01
#define M4W_FLAGS_NOTWORKING	0x02
#define M4W_FLAGS_DOLMISSING	0x04
#define M4W_FLAGS_TOOBIG		0x08

#define M4W_RUNNABLE(x)		(((x) & (M4W_FLAGS_ROMMISSING|M4W_FLAGS_DOLMISSING)) == 0)  

typedef enum { UNCHECKED, OK, CRASHED } game_status_t;

typedef struct {
	const char *rom;
	const char *desc;
	const char *exe;
	game_status_t status;
	u16 flags;
} game_entry_t;

extern void m4w_debug(const char *format, ...);
extern game_entry_t *initGamelist(int *n_games);
extern game_entry_t *searchPattern(const char *pattern, int type);
extern int check_game(game_entry_t *p);
extern void print_ttf(int x, int y, const u32 color, const char *format, ...);
extern void show_logo(void);

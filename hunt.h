/*
 *  Hunt
 *  Copyright (c) 1985 Conrad C. Huang, Gregory S. Couch, Kenneth C.R.C. Arnold
 *  San Francisco, California
 *
 *  Copyright (c) 1985 Regents of the University of California.
 *  All rights reserved.  The Berkeley software License Agreement
 *  specifies the terms and conditions for redistribution.
 */

#include	<stdio.h>
#include	<string.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<curses.h>
#include	<term.h>
#ifndef OLDIPC
#include	<sgtty.h>
#include	<sys/types.h>
#include	<sys/uio.h>
#else /* OLDIPC */
#include	<sys/localopts.h>
#include	<sys/types.h>
#include	<sys/netltoshort.h>
#endif
#include	<sys/socket.h>
#ifdef	INTERNET
#include	<netinet/in.h>
#include	<netdb.h>
#ifndef OLDIPC
#include	<arpa/inet.h>
#endif
#ifdef BROADCAST
#include	<net/if.h>
#endif
#else /* INTERNET */
#include	<sys/un.h>
#endif

#ifdef	INTERNET
#define	SOCK_FAMILY	AF_INET
#else /* INTERNET */
#define	SOCK_FAMILY	AF_UNIX
#define	AF_UNIX_HACK		/* 4.2 hack; leaves files around */
#endif

#define	ADDCH		('a' | 0200)
#define	MOVE		('m' | 0200)
#define	REFRESH		('r' | 0200)
#define	CLRTOEOL	('c' | 0200)
#define	ENDWIN		('e' | 0200)
#define	CLEAR		('C' | 0200)
#define	REDRAW		('R' | 0200)
#define	LAST_PLAYER	('l' | 0200)
#define	BELL		('b' | 0200)
#define	READY		('g' | 0200)

/*
 * Choose MAXPL and MAXMON carefully.  The screen is assumed to be
 * 23 lines high and will only tolerate (MAXPL == 12 && MAXMON == 0)
 * or (MAXPL + MAXMON <= 10).
 */
#define	MAXPL		9
#define	PROXMIN		150
#ifdef MONITOR
#define	MAXMON		1
#endif
#define	NAMELEN		20
#define	MSGLEN		80
#define	DECAY		50.0
#define	MINPROXDAM	(PROXMIN*12+(1<<3)-2)
#define	MAXPROXDAM	(PROXMIN*12+(1<<3)+1)

#define	NASCII		128

#ifndef REFLECT
#ifndef RANDOM
#define RANDOM
#endif
#endif

#define	WIDTH	59
#define	WIDTH2	64	/* Next power of 2 >= WIDTH (for fast access) */
#define	HEIGHT	23
#define	UBOUND	1
#define	DBOUND	22
#define	LBOUND	1
#define	RBOUND	(WIDTH - 1)

#define	STAT_LABEL_COL	60
#define	STAT_VALUE_COL	72
#define	STAT_NAME_COL	61
#define	STAT_SCAN_COL	(STAT_NAME_COL + 5)
#define	STAT_NAME_ROW	0
#define	STAT_AMMO_ROW	2
#define	STAT_SCAN_ROW	3
#define	STAT_CLOAK_ROW	4
#define	STAT_GUN_ROW	5
#define	STAT_DAM_ROW	7
#define	STAT_KILL_ROW	8
#define	STAT_PLAY_ROW	10
#ifdef MONITOR
#define	STAT_MON_ROW	(STAT_PLAY_ROW + MAXPL + 1)
#endif
#define	STAT_NAME_LEN	16

#define	DOOR	'#'
#define	WALL1	'-'
#define	WALL2	'|'
#define	WALL3	'+'
#ifdef REFLECT
#define	WALL4	'/'
#define	WALL5	'\\'
#endif
#define	KNIFE	'K'
#define	SHOT	':'
#define	GRENADE	'o'
#define	SATCHEL	'O'
#define	BOMB	'@'
#define        ABOMB   '.'
#define	MINE	';'
#define	GMINE	'g'
#ifdef	OOZE
#define	SLIME	'$'
#endif
#ifdef	VOLCANO
#define	LAVA	'~'
#define        BLOB    'b'
#endif
#ifdef FLY
#define	FALL	'F'
#endif
#define	SPACE	' '

#define	ABOVE	'i'
#define	BELOW	'!'
#define	RIGHT	'}'
#define	LEFTS	'{'
#ifdef FLY
#define	FLYER	'&'
#endif

#define	NORTH	01
#define	SOUTH	02
#define	EAST	010
#define	WEST	020

#ifndef TRUE
#define	TRUE	1
#define	FALSE	0
#endif
#ifndef CTRL
#define	CTRL(x)		('x' & 037)
#endif
#define	func(y)		(xtst(y->i_uid))

#define	BULSPD		5		/* bullets movement speed */
#define	ISHOTS		1500
#define	NSHOTS		50
#define	MAXNCSHOT	2
#define	MAXDAM		10
#define	MINDAM		5
#define	STABDAM		2

#define	BULREQ		1
#define	GRENREQ		5
#define	SATREQ		15
#define	BOMBREQ		25
#define        ABOMBREQ        500
#ifdef	OOZE
#define	SLIMEREQ	30
#define	SSLIMEREQ	50
#define        SSSLIMEREQ      120
#define	SLIMESPEED	5
#endif
#ifdef	VOLCANO
#define        LAVAREQ       300
#define	LAVASPEED	2
#define        BLOBREQ		10
#define        BBLOBREQ	30
#define        BLOBSPEED	 1
#endif

#define	CLOAKLEN	20
#define	SCANLEN		(Nplayer * 20)
#define	EXPLEN		4

#ifdef FLY
#define	proxdam(x, y)	(func(x->p_ident) ? (y>>1) : y)
#define	_cloak_char(pp)	(((pp)->p_cloak < 0) ? ' ' : '+')
#define	_scan_char(pp)	(((pp)->p_scan < 0) ? _cloak_char(pp) : '*')
#define	stat_char(pp)	(((pp)->p_flying < 0) ? _scan_char(pp) : FLYER)
#define	xtst(pp)	(pp == MINPROXDAM || pp == MAXPROXDAM)
#else /* FLY */
#define	proxdam(x, y)	(func(x->p_ident) ? (y>>1) : y)
#define	_cloak_char(pp)	(((pp)->p_cloak < 0) ? ' ' : '+')
#define	stat_char(pp)	(((pp)->p_scan < 0) ? _cloak_char(pp) : '*')
#define	xtst(pp)	(pp == MINPROXDAM || pp == MAXPROXDAM)
#endif


typedef int			FLAG;
typedef struct bullet_def	BULLET;
typedef struct expl_def		EXPL;
typedef struct player_def	PLAYER;
typedef struct ident_def	IDENT;
typedef struct regen_def	REGEN;
#ifdef	INTERNET
typedef struct sockaddr_in	SOCKET;
#else /* INTERNET */
typedef struct sockaddr_un	SOCKET;
#endif
typedef struct sgttyb		TTYB;

struct ident_def {
	char	i_name[NAMELEN];
	long	i_machine;
	long	i_uid;
	int	i_kills;
	int	i_entries;
	float	i_score;
	IDENT	*i_next;
};

struct player_def {
	IDENT	*p_ident;
	int	p_face;
	char	p_over;
	int	p_undershot;
#ifdef	FLY
	int	p_flying;
	int	p_flyx, p_flyy;
#endif
	FILE	*p_output;
	int	p_fd;
	int	p_mask;
	int	p_damage;
	int	p_damcap;
	int	p_ammo;
	int	p_ncshot;
	int	p_scan;
	int	p_cloak;
	int	p_x, p_y;
	int	p_ncount;
	int	p_nexec;
	long	p_nchar;
	char	p_death[MSGLEN];
	char	p_maze[HEIGHT][WIDTH2];
	int	p_curx, p_cury;
	int	p_lastx, p_lasty;
	int	p_changed;
	char	p_cbuf[BUFSIZ];
};

struct bullet_def {
	int	b_x, b_y;
	int	b_face;
	int	b_charge;
	char	b_type;
	char	b_over;
	PLAYER	*b_owner;
	IDENT	*b_score;
	FLAG	b_expl;
	BULLET	*b_next;
};

struct expl_def {
	int	e_x, e_y;
	char	e_char;
	EXPL	*e_next;
};

struct regen_def {
	int	r_x, r_y;
	REGEN	*r_next;
};

/*
 * external variables
 */

extern FLAG	Last_player;

extern char	Buf[BUFSIZ], Maze[HEIGHT][WIDTH2], Orig_maze[HEIGHT][WIDTH2];

extern char	*Sock_name, *Driver;

extern int	errno, Have_inp, Nplayer, Num_fds, Socket;
extern long	Fds_mask, Sock_mask;

#ifdef INTERNET
extern int	Test_port;
extern int	Sock_port;
#else
extern char	*Sock_name;
#endif

#ifdef VOLCANO
extern int	volcano;
#endif

extern int	See_over[NASCII];

extern BULLET	*Bullets;

extern EXPL	*Expl[EXPLEN];

extern IDENT	*Scores;

extern PLAYER	Player[MAXPL], *End_player;

#ifdef MONITOR
extern FLAG	Am_monitor;
extern PLAYER	Monitor[MAXMON], *End_monitor;
#endif

/*
 * function prototypes
 */

/* System function prototypes */
/* char *getenv(const char *name);
   void *malloc(size_t size);
   int sprintf(char *str, const char *format, ...);
   char *strcpy(char *dest, const char *src);
   char *strncpy(char *dest, const char *src, size_t n); */

/* hunt.c */
int find_driver(FLAG do_startup);
void env_init(void);
void start_driver(void);
void bad_con(void);
void dumpit(int sig);
void sigterm(int sig);
void sigemt(int sig);
#ifdef INTERNET
void sigalrm(int sig);
#endif
void rmnl(char *s);
void intr(int sig);
void leave(int eval, char *mesg);
void tstp(int sig);

/* shots.c */
void moveshots(void);
void save_bullet(BULLET *bp);
void chkshot(BULLET *bp);
#ifdef OOZE
void chkslime(BULLET *bp);
void moveslime(BULLET *bp, int speed);
#endif
int iswall(int y, int x);
void zapshot(BULLET *blist, BULLET *obp);
void explshot(BULLET *blist, int y, int x);
PLAYER *play_at(int y, int x);
int opposite(int face, char dir);
BULLET *is_bullet(int y, int x);
void fixshots(int y, int x, char over);
void find_under(BULLET *blist, BULLET *bp);
void mark_player(BULLET *bp);

/* playit.c */
void playit(void);
int getchr(FILE *fd);
void send_stuff(void);
int quit(void);
void put_ch(char ch);
void put_str(char *s);
void hunt_clear_screen(void);
void clear_eol(void);
void redraw_screen(void);

/* makemaze.c */
void makemaze(void);
void dig(int y, int x);
int candig(int y, int x);
void remap(void);

/* expl.c */
void showexpl(int y, int x, char type);
void rollexpl(void);
void remove_wall(int y, int x);

/* driver.c */
void init(void);
void bul_alarm(int val);
void checkdam(PLAYER *ouch, PLAYER *gotcha, IDENT *credit, int amt, char shot_type);
void zap(PLAYER *pp, FLAG was_player);
int rand_num(int range);
int havechar(PLAYER *pp);
void cleanup(int eval);

/* answer.c */
void answer(void);
void stmonitor(PLAYER *pp);
void stplayer(PLAYER *newpp);
void rand_face(PLAYER *pp);
int reached_limit(u_long machine);

/* terminal.c */
void cgoto(PLAYER *pp, int y, int x);
void outch(PLAYER *pp, char ch);
void outstr(PLAYER *pp, char *str, int len);
void clrscr(PLAYER *pp);
void ce(PLAYER *pp);
void ref(PLAYER *pp);
void sendcom(PLAYER *pp, int command, int arg1, int arg2);

/* execute.c */
void mon_execute(PLAYER *pp);
void execute(PLAYER *pp);
void player_move(PLAYER *pp, int dir);
void face(PLAYER *pp, int dir);
void fire(PLAYER *pp, char type);
void fire_slime(PLAYER *pp, int type, int req, int expl);
void add_shot(int type, int y, int x, char face, int charge, PLAYER *owner, int expl, char over);
BULLET *create_shot(int type, int y, int x, char face, int charge, PLAYER *owner, IDENT *score, int expl, char over);
void cloak(PLAYER *pp);
void scan(PLAYER *pp);
void pickup(PLAYER *pp, int y, int x, int prob, int obj);

/* draw.c */
void drawmaze(PLAYER *pp);
void drawstatus(PLAYER *pp);
void look(PLAYER *pp);
void see(PLAYER *pp, int face);
void showstat(PLAYER *pp);
void drawplayer(PLAYER *pp, FLAG draw);
void message(PLAYER *pp, char *s);
char translate(char ch);

/* connect.c */
void do_connect(char *name);

/* other file prototypes */
IDENT *get_ident(void);

#define check(_pp,_y,_x)  \
{ \
	register int	xxindex; \
	register int	xxch; \
	xxindex = ((_y) << 6) + (_x); \
	xxch = ((char *) Maze)[xxindex]; \
	if (xxch != ((char *) ((_pp)->p_maze))[xxindex]) { \
		cgoto((_pp), (_y), (_x)); \
		if ((_x) == (_pp)->p_x && (_y) == (_pp)->p_y) \
			outch((_pp), translate(xxch)); \
		else \
			outch((_pp), xxch); \
		((char *) ((_pp)->p_maze))[xxindex] = xxch; \
	} \
}

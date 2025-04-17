/*
 *  Hunt
 *  Copyright (c) 1985 Conrad C. Huang, Gregory S. Couch, Kenneth C.R.C. Arnold
 *  San Francisco, California
 *
 *  Copyright (c) 1985 Regents of the University of California.
 *  All rights reserved.  The Berkeley software License Agreement
 *  specifies the terms and conditions for redistribution.
 */

#include	<curses.h>
#include	<ctype.h>
#include	<signal.h>
#include	<errno.h>
#include	"hunt.h"
#include	<sys/file.h>

#undef  CTRL
#define CTRL(x)	('x' & 037)

int		input(void);
static int	nchar_send;
static int	in	= FREAD;
char		screen[24][80], blanks[80];
int		cur_row, cur_col;
#ifdef OTTO
int		Otto_count;
int		Otto_mode;
static int	otto_y, otto_x;
static char	otto_face;
#endif

#define	MAX_SEND	5

/*
 * ibuf is the input buffer used for the stream from the driver.
 * It is small because we do not check for user input when there
 * are characters in the input buffer.
 */
static char	ibuf[20];

/* Modified GETCHR macro to use standard getc instead of direct struct access */
#define	GETCHR(fd)	(getc(fd))

/*
 * playit:
 *	Play a given game, handling all the curses commands from
 *	the driver.
 */
void playit(void)
{
	register FILE		*inf;
	register int		ch;
	register unsigned int	y, x;
	extern int		Master_pid;
	extern int		errno;
	extern int		putchar(int);

	errno = 0;
	while ((inf = fdopen(Socket, "r")) == NULL)
		if (errno == EINTR)
			errno = 0;
		else {
			perror("fdopen of socket");
			exit(1);
		}
	setbuffer(inf, ibuf, sizeof ibuf);
	Master_pid = getw(inf);
	if (Master_pid == 0 || Master_pid == EOF) {
		bad_con();
		/* NOTREACHED */
	}
#ifdef OTTO
	Otto_count = 0;
#endif
	nchar_send = MAX_SEND;
	while ((ch = GETCHR(inf)) != EOF) {
#ifdef DEBUG
		fputc(ch, stderr);
#endif
		switch (ch & 0377) {
		  case MOVE:
			y = GETCHR(inf);
			x = GETCHR(inf);
			mvcur(cur_row, cur_col, y, x);
			cur_row = y;
			cur_col = x;
			break;
		  case ADDCH:
			ch = GETCHR(inf);
#ifdef OTTO
			switch (ch) {

			case '<':
			case '>':
			case '^':
			case 'v':
				otto_face = ch;
				getyx(stdscr, otto_y, otto_x);
				break;
			}
#endif
			put_ch(ch);
			break;
		  case CLRTOEOL:
			clear_eol();
			break;
		  case CLEAR:
			hunt_clear_screen();
			break;
		  case REFRESH:
			fflush(stdout);
			break;
		  case REDRAW:
			redraw_screen();
			fflush(stdout);
			break;
		  case ENDWIN:
			fflush(stdout);
			if ((ch = GETCHR(inf)) == LAST_PLAYER)
				Last_player = TRUE;
			ch = EOF;
			goto out;
		  case BELL:
			putchar(CTRL(G));
			break;
		  case READY:
			(void) fflush(stdout);
			if (nchar_send < 0)
				(void) ioctl(fileno(stdin), TIOCFLUSH, &in);
			nchar_send = MAX_SEND;
#ifndef OTTO
			(void) GETCHR(inf);
#else /* OTTO */
			Otto_count -= (GETCHR(inf) & 255);
			if (!Am_monitor) {
#ifdef DEBUG
				fputc('0' + Otto_count, stderr);
#endif
				if (Otto_count == 0 && Otto_mode)
					otto(otto_y, otto_x, otto_face);
			}
#endif
			break;
		  default:
#ifdef OTTO
			switch (ch) {

			case '<':
			case '>':
			case '^':
			case 'v':
				otto_face = ch;
				getyx(stdscr, otto_y, otto_x);
				break;
			}
#endif
			put_ch(ch);
			break;
		}
	}
out:
	(void) fclose(inf);
}

/*
 * getchr:
 *	Grab input and pass it along to the driver
 *	Return any characters from the driver
 *	When this routine is called by GETCHR, we already know there are
 *	no characters in the input buffer.
 */
int getchr(register FILE *fd)
{
	long	nchar;
	fd_set  readfds, s_readfds;
	int	driver_mask, stdin_mask;
	int	nfds, s_nfds;

	FD_ZERO(&readfds);
	FD_ZERO(&s_readfds);
	
	driver_mask = fileno(fd);
	stdin_mask = fileno(stdin);
	FD_SET(driver_mask, &s_readfds);
	FD_SET(stdin_mask, &s_readfds);
	s_nfds = (driver_mask > stdin_mask) ? driver_mask : stdin_mask;
	s_nfds++;

one_more_time:
	do {
		errno = 0;
		readfds = s_readfds;
		nfds = s_nfds;
#ifndef OLDIPC
		nfds = select(nfds, &readfds, NULL, NULL, NULL);
#else /* OLDIPC */
		nfds = select(nfds, &readfds, (int *) NULL, 32767);
#endif
	} while (nfds <= 0 && errno == EINTR);

	if (FD_ISSET(stdin_mask, &readfds))
		send_stuff();
	if (!FD_ISSET(driver_mask, &readfds))
		goto one_more_time;
	return getc(fd);
}

/*
 * send_stuff:
 *	Send standard input characters to the driver
 */
void send_stuff(void)
{
	register int	count;
	register char	*sp, *nsp;
	static char	inp[sizeof Buf];
	extern char	map_key[256];

	count = read(fileno(stdin), Buf, sizeof Buf);
	if (count <= 0)
		return;
	if (nchar_send <= 0) {
		(void) write(1, "\7", 1);
		return;
	}

	/*
	 * look for 'q'uit commands; if we find one,
	 * confirm it.  If it is not confirmed, strip
	 * it out of the input
	 */
	Buf[count] = '\0';
	nsp = inp;
	for (sp = Buf; *sp != '\0'; sp++)
		if ((*nsp = map_key[*sp]) == 'q')
			intr(SIGINT);
#ifdef OTTO
		else if (*nsp == CTRL(O))
			Otto_mode = !Otto_mode;
#endif
		else
			nsp++;
	count = nsp - inp;
	if (count) {
#ifdef OTTO
		Otto_count += count;
#endif
		nchar_send -= count;
		if (nchar_send < 0)
			count += nchar_send;
		(void) write(Socket, inp, count);
	}
}

/*
 * quit:
 *	Handle the end of the game when the player dies
 */
int quit(void)
{
	register int	explain, ch;

	if (Last_player)
		return TRUE;
#ifdef OTTO
	if (Otto_mode)
		return FALSE;
#endif
	mvcur(cur_row, cur_col, HEIGHT, 0);
	cur_row = HEIGHT;
	cur_col = 0;
	put_str("Re-enter game? ");
	clear_eol();
	fflush(stdout);
	explain = FALSE;
	for (;;) {
		if (isupper(ch = getchar()))
			ch = tolower(ch);
		if (ch == 'y') {
			sleep(2);
			return FALSE;
		}
		else if (ch == 'n')
			return TRUE;
		(void) putchar(CTRL(G));
		if (!explain) {
			put_str("(Y or N) ");
			explain = TRUE;
		}
		fflush(stdout);
	}
}

void put_ch(char ch)
{
	extern int LINES, COLS;
	/* Define terminal capability variables */
	int AM = 0;  /* Auto margins */
	int XN = 0;  /* Newline ignored after 80 cols */
	
	if (!isprint(ch)) {
		fprintf(stderr, "r,c,ch: %d,%d,%d", cur_row, cur_col, ch);
		return;
	}
	screen[cur_row][cur_col] = ch;
	putchar(ch);
	if (++cur_col >= COLS) {
		if (!AM || XN)
			putchar('\n');
		cur_col = 0;
		if (++cur_row >= LINES)
			cur_row = LINES;
	}
}

void put_str(char *s)
{
	while (*s)
		put_ch(*s++);
}

void hunt_clear_screen(void)
{
	register int	i;
	char *CL = NULL;
	
	if (blanks[0] == '\0')
		for (i = 0; i < 80; i++)
			blanks[i] = ' ';

	if (CL != NULL) {
		tputs(CL, LINES, putchar);
		for (i = 0; i < 24; i++)
			bcopy(blanks, screen[i], 80);
	} else {
		for (i = 0; i < 24; i++) {
			mvcur(cur_row, cur_col, i, 0);
			cur_row = i;
			cur_col = 0;
			clear_eol();
		}
		mvcur(cur_row, cur_col, 0, 0);
	}
	cur_row = cur_col = 0;
}

void clear_eol(void)
{
	char *CE = NULL;
	extern int COLS;
	/* Define terminal capability variable */
	int AM = 0;
	
	if (CE != NULL)
		tputs(CE, 1, putchar);
	else {
		fwrite(blanks, sizeof (char), 80 - cur_col, stdout);
		if (COLS != 80)
			mvcur(cur_row, 80, cur_row, cur_col);
		else if (AM)
			mvcur(cur_row + 1, 0, cur_row, cur_col);
		else
			mvcur(cur_row, 79, cur_row, cur_col);
	}
	bcopy(blanks, &screen[cur_row][cur_col], 80 - cur_col);
}

void redraw_screen(void)
{
	register int	i;
	static int	first = 1;

	if (first) {
		/* Initialize curscr - modern curses doesn't expose struct fields */
		if ((curscr = newwin(24, 80, 0, 0)) == NULL) {
			fprintf(stderr, "Can't create curscr\n");
			exit(1);
		}
		
		/* Copy screen contents into curses window */
		for (i = 0; i < 24; i++) {
			mvwaddnstr(curscr, i, 0, screen[i], 80);
		}
		first = 0;
	}
	
	/* Update cursor position */
	wmove(curscr, cur_row, cur_col);
	wrefresh(curscr);
	
#ifdef	NOCURSES
	mvcur(cur_row, cur_col, 0, 0);
	for (i = 0; i < 23; i++) {
		fwrite(screen[i], sizeof (char), 80, stdout);
		if (COLS > 80 || (COLS == 80 && !AM))
			putchar('\n');
	}
	fwrite(screen[23], sizeof (char), 79, stdout);
	mvcur(23, 79, cur_row, cur_col);
#endif
}
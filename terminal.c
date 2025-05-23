/*
 *  Hunt
 *  Copyright (c) 1985 Conrad C. Huang, Gregory S. Couch, Kenneth C.R.C. Arnold
 *  San Francisco, California
 *
 *  Copyright (c) 1985 Regents of the University of California.
 *  All rights reserved.  The Berkeley software License Agreement
 *  specifies the terms and conditions for redistribution.
 */

#include	"hunt.h"
#define	TERM_WIDTH	80	/* Assume terminals are 80-char wide */

/*
 * cgoto:
 *	Move the cursor to the given position on the given player's
 *	terminal.
 */
void
cgoto(PLAYER *pp, int y, int x)
{
	if (x == pp->p_curx && y == pp->p_cury)
		return;
	sendcom(pp, MOVE, y, x);
	pp->p_cury = y;
	pp->p_curx = x;
}

/*
 * outch:
 *	Put out a single character.
 */
void
outch(PLAYER *pp, char ch)
{
	if (++pp->p_curx >= TERM_WIDTH) {
		pp->p_curx = 0;
		pp->p_cury++;
	}
	(void) putc(ch, pp->p_output);
}

/*
 * outstr:
 *	Put out a string of the given length.
 */
void
outstr(PLAYER *pp, char *str, int len)
{
	pp->p_curx += len;
	pp->p_cury += (pp->p_curx / TERM_WIDTH);
	pp->p_curx %= TERM_WIDTH;
	while (len--)
		(void) putc(*str++, pp->p_output);
}

/*
 * clrscr:
 *	Clear the screen, and reset the current position on the screen.
 */
void
clrscr(PLAYER *pp)
{
	sendcom(pp, CLEAR, 0, 0);
	pp->p_cury = 0;
	pp->p_curx = 0;
}

/*
 * ce:
 *	Clear to the end of the line
 */
void
ce(PLAYER *pp)
{
	sendcom(pp, CLRTOEOL, 0, 0);
}

/*
 * ref;
 *	Refresh the screen
 */
void
ref(PLAYER *pp)
{
	sendcom(pp, REFRESH, 0, 0);
}

/*
 * sendcom:
 *	Send a command to the given user
 */
/* VARARGS2 */
void
sendcom(PLAYER *pp, int command, int arg1, int arg2)
{
	(void) putc(command, pp->p_output);
	switch (command & 0377) {
	  case MOVE:
		(void) putc(arg1, pp->p_output);
		(void) putc(arg2, pp->p_output);
		break;
	  case ADDCH:
	  case READY:
		(void) putc(arg1, pp->p_output);
		break;
	}
	/* Debug: Force flush after critical commands */
	if ((command & 0377) == CLEAR || (command & 0377) == REFRESH) {
		(void) fflush(pp->p_output);
	}
}

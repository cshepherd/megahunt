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

#undef CTRL
#define	CTRL(x)	('x' & 037)

#ifdef MONITOR
/*
 * mon_execute:
 *	Execute a single monitor command
 */
void
mon_execute(PLAYER *pp)
{
	register char	ch;

	ch = pp->p_cbuf[pp->p_ncount++];
	switch (ch) {
	  case CTRL(L):
		sendcom(pp, REDRAW, 0, 0);
		break;
	  case 'q':
		(void) strcpy(pp->p_death, "| Quit |");
		break;
	}
}
#endif

/*
 * execute:
 *	Execute a single command
 */
void
execute(PLAYER *pp)
{
	register char	ch;

	ch = pp->p_cbuf[pp->p_ncount++];

#ifdef	FLY
	if (pp->p_flying >= 0) {
		switch (ch) {
		  case CTRL(L):
			sendcom(pp, REDRAW, 0, 0);
			break;
		  case 'q':
			(void) strcpy(pp->p_death, "| Quit |");
			break;
		}
		return;
	}
#endif

	switch (ch) {
	  case CTRL(L):
		sendcom(pp, REDRAW, 0, 0);
		break;
	  case 'h':
		player_move(pp, LEFTS);
		break;
	  case 'H':
		face(pp, LEFTS);
		break;
	  case 'j':
		player_move(pp, BELOW);
		break;
	  case 'J':
		face(pp, BELOW);
		break;
	  case 'k':
		player_move(pp, ABOVE);
		break;
	  case 'K':
		face(pp, ABOVE);
		break;
	  case 'l':
		player_move(pp, RIGHT);
		break;
	  case 'L':
		face(pp, RIGHT);
		break;
	  case 'f':
		fire(pp, SHOT);
		break;
	  case 'g':
		fire(pp, GRENADE);
		break;
	  case 'F':
		fire(pp, SATCHEL);
		break;
	  case 'G':
		fire(pp, BOMB);
		break;
	  case 'A':
		fire(pp, ABOMB);
		break;
#ifdef	OOZE
	  case 'o':
		fire_slime(pp, SLIME, SLIMEREQ, FALSE);
		break;
	  case 'O':
		fire_slime(pp, SLIME, SSLIMEREQ, FALSE);
		break;
	  case '0':
		fire_slime(pp, SLIME, SSSLIMEREQ, FALSE);
		break;
#  ifdef VOLCANO
	  case 'b':
	  case ' ':
		fire_slime(pp, BLOB, BLOBREQ, FALSE);
		break;
	  case 'B':
		fire_slime(pp, BLOB, BBLOBREQ, FALSE);
		break;
	  case 'V':
		fire_slime(pp, LAVA, LAVAREQ, TRUE);
		break;
#  endif
#endif
	  case 's':
		scan(pp);
		break;
	  case 'c':
		cloak(pp);
		break;
	  case 'q':
		(void) strcpy(pp->p_death, "| Quit |");
		break;
	}
}

/*
 * move:
 *	Execute a move in the given direction
 */
void
player_move(PLAYER *pp, int dir)
{
	register PLAYER	*newp;
	register int	x, y;
	register FLAG	moved;
	register BULLET	*bp;

	y = pp->p_y;
	x = pp->p_x;

	switch (dir) {
	  case LEFTS:
		x--;
		break;
	  case RIGHT:
		x++;
		break;
	  case ABOVE:
		y--;
		break;
	  case BELOW:
		y++;
		break;
	}

	moved = FALSE;
	switch (Maze[y][x]) {
	  case SPACE:
#ifdef RANDOM
	  case DOOR:
#endif
		moved = TRUE;
		break;
	  case WALL1:
	  case WALL2:
	  case WALL3:
#ifdef REFLECT
	  case WALL4:
	  case WALL5:
#endif
		break;
	  case MINE:
	  case GMINE:
		if (dir == pp->p_face)
			pickup(pp, y, x, 5, Maze[y][x]);
		else if (opposite(dir, pp->p_face))
			pickup(pp, y, x, 95, Maze[y][x]);
		else
			pickup(pp, y, x, 50, Maze[y][x]);
		Maze[y][x] = SPACE;
		moved = TRUE;
		break;
#ifdef VOLCANO
	  case BLOB:
		moved = TRUE;
		Maze[y][x] = SPACE;
		add_shot(BLOB, y, x, LEFTS, 3, (PLAYER *) NULL,
			TRUE, pp->p_face);
		break;
#endif
	  case SHOT:
	  case GRENADE:
	  case SATCHEL:
	  case BOMB:
	  case ABOMB:
		bp = is_bullet(y, x);
		if (bp != NULL)
			bp->b_expl = TRUE;
		Maze[y][x] = SPACE;
		moved = TRUE;
		break;
	  case LEFTS:
	  case RIGHT:
	  case ABOVE:
	  case BELOW:
#ifdef FLY
	  case FLYER:
#endif
		if (dir != pp->p_face)
			sendcom(pp, BELL, 0, 0);
		else {
			newp = play_at(y, x);
			checkdam(newp, pp, pp->p_ident, STABDAM, KNIFE);
		}
		break;
	}
	if (moved) {
		if (pp->p_ncshot > 0)
			if (--pp->p_ncshot == MAXNCSHOT) {
				cgoto(pp, STAT_GUN_ROW, STAT_VALUE_COL);
				outstr(pp, " ok", 3);
			}
		if (pp->p_undershot) {
			fixshots(pp->p_y, pp->p_x, pp->p_over);
			pp->p_undershot = FALSE;
		}
		drawplayer(pp, FALSE);
		pp->p_over = Maze[y][x];
		pp->p_y = y;
		pp->p_x = x;
		drawplayer(pp, TRUE);
	}
}

/*
 * face:
 *	Change the direction the player is facing
 */
void
face(PLAYER *pp, int dir)
{
	if (pp->p_face != dir) {
		pp->p_face = dir;
		drawplayer(pp, TRUE);
	}
}

/*
 * fire:
 *	Fire a shot of the given type in the given direction
 */
void
fire(PLAYER *pp, char type)
{
	register int	req_index;
	static int	req[5] = { BULREQ, GRENREQ, SATREQ, BOMBREQ, ABOMBREQ };
	static int	shot_type[5] = { SHOT, GRENADE, SATCHEL, BOMB, ABOMB };

	if (pp == NULL)
		return;
	if (pp->p_ammo == 0) {
		message(pp, "No more charges.");
		return;
	}
	if (pp->p_ncshot > MAXNCSHOT)
		return;
	if (pp->p_ncshot++ == MAXNCSHOT) {
		cgoto(pp, STAT_GUN_ROW, STAT_VALUE_COL);
		outstr(pp, "   ", 3);
	}
	switch (type) {
	  case SHOT:
		req_index = 0;
		break;
	  case GRENADE:
		req_index = 1;
		break;
	  case SATCHEL:
		req_index = 2;
		break;
	  case BOMB:
		req_index = 3;
		break;
	  case ABOMB:
		req_index = 4;
		break;
#ifdef DEBUG
	  default:
		message(pp, "What you do!!!");
		return;
#endif
	}
	while (pp->p_ammo < req[req_index])
		req_index--;
	pp->p_ammo -= req[req_index];
	(void) sprintf(Buf, "%4d", pp->p_ammo);
	cgoto(pp, STAT_AMMO_ROW, STAT_VALUE_COL);
	outstr(pp, Buf, 4);

	add_shot(shot_type[req_index], pp->p_y, pp->p_x, pp->p_face,
		req[req_index], pp, FALSE, pp->p_face);
	pp->p_undershot = TRUE;

	/*
	 * Show the object to everyone
	 */
	showexpl(pp->p_y, pp->p_x, shot_type[req_index]);
	for (pp = Player; pp < End_player; pp++)
		sendcom(pp, REFRESH, 0, 0);
#ifdef MONITOR
	for (pp = Monitor; pp < End_monitor; pp++)
		sendcom(pp, REFRESH, 0, 0);
#endif
}

#ifdef	OOZE
/*
 * fire_slime:
 *	Fire a slime shot in the given direction
 */
void
fire_slime(PLAYER *pp, int type, int req, int expl)
{
	if (pp == NULL)
		return;
	if (pp->p_ammo < req) {
		message(pp, "Not enough charges.");
		return;
	}
	if (pp->p_ncshot > MAXNCSHOT)
		return;
	if (pp->p_ncshot++ == MAXNCSHOT) {
		cgoto(pp, STAT_GUN_ROW, STAT_VALUE_COL);
		outstr(pp, "   ", 3);
	}
	pp->p_ammo -= req;
	(void) sprintf(Buf, "%4d", pp->p_ammo);
	cgoto(pp, STAT_AMMO_ROW, STAT_VALUE_COL);
	outstr(pp, Buf, 4);

	add_shot(type, pp->p_y, pp->p_x, pp->p_face, req,
		pp, expl, pp->p_face);
	/*
	 * Show the object to everyone
	 */
	showexpl(pp->p_y, pp->p_x, type);
	for (pp = Player; pp < End_player; pp++)
		sendcom(pp, REFRESH, 0, 0);
#ifdef MONITOR
	for (pp = Monitor; pp < End_monitor; pp++)
		sendcom(pp, REFRESH, 0, 0);
#endif
}
#endif

/*
 * create_shot:
 *	Create a shot with the given properties
 */
void
add_shot(int type, int y, int x, char face, int charge, PLAYER *owner, int expl, char over)
{
	register BULLET	*bp;

#ifdef CONSTANT_MOVE
	/*
	 * if there are no bullets in flight, set up the alarm
	 */

	if (Bullets == NULL)
		bul_alarm(1);
#endif

	bp = create_shot(type, y, x, face, charge, owner,
		(owner == NULL) ? NULL : owner->p_ident, expl, over);
	bp->b_next = Bullets;
	Bullets = bp;
}

BULLET *
create_shot(int type, int y, int x, char face, int charge, PLAYER *owner, IDENT *score, int expl, char over)
{
	register BULLET	*bp;

	bp = (BULLET *) malloc(sizeof (BULLET));	/* NOSTRICT */
	if (bp == NULL) {
		if (owner != NULL)
			message(owner, "Out of memory");
		return NULL;
	}

	bp->b_face = face;
	bp->b_x = x;
	bp->b_y = y;
	bp->b_charge = charge;
	bp->b_owner = owner;
	bp->b_score = score;
	bp->b_type = type;
	bp->b_expl = expl;
	bp->b_over = over;
	bp->b_next = NULL;

	return bp;
}

/*
 * cloak:
 *	Turn on or increase length of a cloak
 */
void
cloak(PLAYER *pp)
{
	if (pp->p_ammo <= 0) {
		message(pp, "No more charges");
		return;
	}
	(void) sprintf(Buf, "%4d", --pp->p_ammo);
	cgoto(pp, STAT_AMMO_ROW, STAT_VALUE_COL);
	outstr(pp, Buf, 4);

	pp->p_cloak += CLOAKLEN;
	cgoto(pp, STAT_CLOAK_ROW, STAT_VALUE_COL);
	outstr(pp, " on", 3);

	if (pp->p_scan >= 0) {
		pp->p_scan = -1;
		cgoto(pp, STAT_SCAN_ROW, STAT_VALUE_COL);
		outstr(pp, "   ", 3);
	}

	showstat(pp);
}

/*
 * scan:
 *	Turn on or increase length of a scan
 */
void
scan(PLAYER *pp)
{
	if (pp->p_ammo <= 0) {
		message(pp, "No more charges");
		return;
	}
	(void) sprintf(Buf, "%4d", --pp->p_ammo);
	cgoto(pp, STAT_AMMO_ROW, STAT_VALUE_COL);
	outstr(pp, Buf, 4);

	pp->p_scan += SCANLEN;
	cgoto(pp, STAT_SCAN_ROW, STAT_VALUE_COL);
	outstr(pp, " on", 3);

	if (pp->p_cloak >= 0) {
		pp->p_cloak = -1;
		cgoto(pp, STAT_CLOAK_ROW, STAT_VALUE_COL);
		outstr(pp, "   ", 3);
	}

	showstat(pp);
}

/*
 * pickup:
 *	check whether the object blew up or whether he picked it up
 */
void
pickup(PLAYER *pp, int y, int x, int prob, int obj)
{
	register int	req;

	switch (obj) {
	  case MINE:
		req = BULREQ;
		break;
	  case GMINE:
		req = GRENREQ;
		break;
	  default:
		abort();
	}
	if (rand_num(100) < prob)
		add_shot(obj, y, x, LEFTS, req, (PLAYER *) NULL,
			TRUE, pp->p_face);
	else {
		if (obj == MINE || obj == GMINE) {
			pp->p_damage -= 10;
			(void) sprintf(Buf, "%3d/%-2d", pp->p_damage, pp->p_damcap);
			cgoto(pp, STAT_DAM_ROW, STAT_VALUE_COL);
			outstr(pp, Buf, 6);
		}
		pp->p_ammo += req*40;
		(void) sprintf(Buf, "%4d", pp->p_ammo);
		cgoto(pp, STAT_AMMO_ROW, STAT_VALUE_COL);
		outstr(pp, Buf, 4);
	}
}

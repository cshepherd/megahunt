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
#include	<signal.h>
#include	<errno.h>
#include	<sys/ioctl.h>
#include	<sys/time.h>
#include	<time.h>

#ifndef pdp11
#define	RN	(((Seed = Seed * 11109 + 13849) >> 16) & 0xffff)
#else /* pdp11 */
#define	RN	((Seed = Seed * 11109 + 13849) & 0x7fff)
#endif

int	Seed = 0;

#ifdef CONSTANT_MOVE
static struct itimerval	Timing;
#endif

SOCKET	Daemon;
#ifdef	INTERNET
int	Test_socket;		/* test socket to answer datagrams */
#define	DAEMON_SIZE	(sizeof Daemon)
#else /* INTERNET */
#define	DAEMON_SIZE	(sizeof Daemon - 1)
#endif
#define	dodam(x, y)	x->p_damage += proxdam(x, y)

/*
 * main:
 *	The main program.
 */
int main(void)
{
	register PLAYER	*pp;
	register int	had_char;
#ifdef INTERNET
	register long	test_mask;
	int		msg;
	int		namelen;
	SOCKET		test;
#endif
#ifdef CONSTANT_MOVE
	register int	enable_alarm, disable_alarm;
#endif
	static long	read_fds;

	init();
	Sock_mask = (1 << Socket);
#ifdef INTERNET
	test_mask = (1 << Test_socket);
#endif

#ifdef CONSTANT_MOVE
	enable_alarm = sigblock(0);
	disable_alarm = enable_alarm | (1 << (SIGALRM - 1));
	(void) sigsetmask(disable_alarm);
	(void) signal(SIGALRM, moveshots);
#endif

	while (Nplayer > 0) {
#ifdef CONSTANT_MOVE
		(void) sigsetmask(enable_alarm);
#endif
		read_fds = Fds_mask;
		errno = 0;
#ifndef OLDIPC
		while (select(Num_fds, (fd_set *)&read_fds, (fd_set *) NULL,
		    (fd_set *) NULL, (struct timeval *) NULL) < 0)
#else /* OLDIPC */
		while (select(20, &read_fds, NULL, 32767) < 0)
#endif
		{
			if (errno != EINTR)
				perror("select");
			if (Nplayer == 0)
				goto out;
			errno = 0;
		}
		Have_inp = read_fds;
#ifdef CONSTANT_MOVE
		(void) sigsetmask(disable_alarm);
#endif
#ifdef INTERNET
		if (read_fds & test_mask) {
			namelen = DAEMON_SIZE;
#ifndef OLDIPC
			(void) recvfrom(Test_socket, (char *) &msg, sizeof msg,
				0, (struct sockaddr *) &test, (socklen_t *)&namelen);
			(void) sendto(Test_socket, (char *) &msg, sizeof msg,
				0, (struct sockaddr *) &test, DAEMON_SIZE);
#else /* OLDIPC */
			(void) receive(Test_socket, (struct sockaddr *) &test,
				(char *) &msg, sizeof msg);
			(void) send(Test_socket, (struct sockaddr *) &test,
				(char *) &msg, sizeof msg);
#endif
		}
#endif
		for (;;) {
			had_char = FALSE;
			for (pp = Player; pp < End_player; pp++)
				if (havechar(pp)) {
					execute(pp);
					pp->p_nexec++;
					had_char++;
				}
#ifdef MONITOR
			for (pp = Monitor; pp < End_monitor; pp++)
				if (havechar(pp)) {
					mon_execute(pp);
					pp->p_nexec++;
					had_char++;
				}
#endif
			if (!had_char)
				break;
#ifdef CONSTANT_MOVE
			for (pp = Player; pp < End_player; pp++) {
				look(pp);
				sendcom(pp, REFRESH, 0, 0);
			}
#ifdef MONITOR
			for (pp = Monitor; pp < End_monitor; pp++)
				sendcom(pp, REFRESH, 0, 0);
#endif
#else /* CONSTANT_MOVE */
			moveshots();
#endif
			for (pp = Player; pp < End_player; )
				if (pp->p_death[0] != '\0')
					zap(pp, TRUE);
				else
					pp++;
#ifdef MONITOR
			for (pp = Monitor; pp < End_monitor; )
				if (pp->p_death[0] != '\0')
					zap(pp, FALSE);
				else
					pp++;
#endif
		}
		if (read_fds & Sock_mask)
			answer();
		for (pp = Player; pp < End_player; pp++) {
			if (read_fds & pp->p_mask)
				sendcom(pp, READY, pp->p_nexec, 0);
			pp->p_nexec = 0;
			(void) fflush(pp->p_output);
		}
#ifdef MONITOR
		for (pp = Monitor; pp < End_monitor; pp++) {
			if (read_fds & pp->p_mask)
				sendcom(pp, READY, pp->p_nexec, 0);
			pp->p_nexec = 0;
			(void) fflush(pp->p_output);
		}
#endif
	}
out:
#ifdef	CONSTANT_MOVE
	bul_alarm(0);
#endif

#ifdef MONITOR
	for (pp = Monitor; pp < End_monitor; )
		zap(pp, FALSE);
#endif
	cleanup(0);
}

/*
 * init:
 *	Initialize the global parameters.
 */
void
init(void)
{
	register int	i;
#ifdef	INTERNET
	SOCKET		test_port;
	auto int	msg;
#endif

#ifndef DEBUG
	(void) ioctl(fileno(stdout), TIOCNOTTY, NULL);
	(void) setpgrp();
	(void) signal(SIGHUP, SIG_IGN);
	(void) signal(SIGINT, SIG_IGN);
	(void) signal(SIGQUIT, SIG_IGN);
	(void) signal(SIGTERM, cleanup);
#endif

	(void) chdir("/usr/tmp");	/* just in case it core dumps */
	(void) signal(SIGPIPE, SIG_IGN);

#ifdef	INTERNET
	Daemon.sin_family = SOCK_FAMILY;
#ifdef OLD
	if (gethostname(local_name, sizeof local_name) < 0) {
		perror("gethostname");
		exit(1);
	}
	if ((hp = gethostbyname(local_name)) == NULL) {
		fprintf(stderr, "Unknown host %s\n", local_name);
		exit(1);
	}
	bcopy(hp->h_addr, &(Daemon.sin_addr.s_addr), hp->h_length);
#else
	Daemon.sin_addr.s_addr = INADDR_ANY;
#endif
	Daemon.sin_port = htons(Sock_port);
#else /* INTERNET */
	Daemon.sun_family = SOCK_FAMILY;
	(void) strcpy(Daemon.sun_path, Sock_name);
#endif

#ifndef OLDIPC
	Socket = socket(SOCK_FAMILY, SOCK_STREAM, 0);
#else /* OLDIPC */
	Socket = socket(SOCK_STREAM, 0, (struct sockaddr *) &Daemon,
		SO_ACCEPTCONN);
#endif
#if defined(INTERNET) && !defined(OLDIPC)
#ifdef SO_USELOOPBACK
	if (setsockopt(Socket, SOL_SOCKET, SO_USELOOPBACK, &msg, sizeof msg)<0)
		perror("setsockopt loopback");
#endif
#endif
#ifndef OLDIPC
	if (bind(Socket, (struct sockaddr *) &Daemon, DAEMON_SIZE) < 0) {
		if (errno == EADDRINUSE)
			exit(0);
		else {
			perror("bind");
			cleanup(1);
		}
	}
	(void) listen(Socket, 5);
#endif
	Fds_mask = (1 << Socket);
	Num_fds = Socket + 1;

#ifdef INTERNET
	test_port = Daemon;
	test_port.sin_port = htons(Test_port);

#ifndef OLDIPC
	Test_socket = socket(SOCK_FAMILY, SOCK_DGRAM, 0);
	if (bind(Test_socket, (struct sockaddr *) &test_port,
	    DAEMON_SIZE) < 0) {
		perror("bind");
		exit(1);
	}
	(void) listen(Test_socket, 5);
#else /* OLDIPC */
	Test_socket = socket(SOCK_DGRAM, 0, (struct sockaddr *) &test_port, 0);
#endif
	Fds_mask |= (1 << Test_socket);
	if (Test_socket > Socket)
		Num_fds = Test_socket + 1;
#endif

	Seed = getpid() + time((time_t *) NULL);
	makemaze();

	for (i = 0; i < NASCII; i++)
		See_over[i] = TRUE;
	See_over[DOOR] = FALSE;
	See_over[WALL1] = FALSE;
	See_over[WALL2] = FALSE;
	See_over[WALL3] = FALSE;
#ifdef REFLECT
	See_over[WALL4] = FALSE;
	See_over[WALL5] = FALSE;
#endif

#ifdef CONSTANT_MOVE
	getitimer(ITIMER_REAL, &Timing);
	Timing.it_interval.tv_sec = 0;
	Timing.it_interval.tv_usec = 500;
	Timing.it_value.tv_sec = 0;
	Timing.it_value.tv_usec = 0;
	setitimer(ITIMER_REAL, &Timing, NULL);
#endif

	answer();
}

#ifdef CONSTANT_MOVE
/*
 * bul_alarm:
 *	Set up the alarm for the bullets
 */
void
bul_alarm(int val)
{
	Timing.it_value.tv_usec = val * Timing.it_interval.tv_usec;
	setitimer(ITIMER_REAL, &Timing, NULL);
}
#endif

/*
 * checkdam:
 *	Check the damage to the given player, and see if s/he is killed
 */
void
checkdam(PLAYER *ouch, PLAYER *gotcha, IDENT *credit, int amt, char shot_type)
{
	register char	*cp;

	if (ouch->p_death[0] != '\0')
		return;
	if (rand_num(100) < 5) {
		message(ouch, "Missed you by a hair");
		if (gotcha != NULL)
			message(gotcha, "Missed him");
		return;
	}
	dodam(ouch, amt);
	if (ouch->p_damage <= ouch->p_damcap) {
		(void) sprintf(Buf, "%3d", ouch->p_damage);
		cgoto(ouch, STAT_DAM_ROW, STAT_VALUE_COL);
		outstr(ouch, Buf, 3);
		return;
	}

	/* Someone DIED */
	switch (shot_type) {
	  default:
		cp = "Killed";
		break;
#ifdef FLY
	  case FALL:
		cp = "Killed on impact";
		break;
#endif
	  case KNIFE:
		cp = "Stabbed to death";
		break;
	  case SHOT:
		cp = "Shot to death";
		break;
	  case GRENADE:
	  case SATCHEL:
	  case BOMB:
		cp = "Bombed";
		break;
	  case ABOMB:
		cp = "Nuked";
		break;
	  case MINE:
	  case GMINE:
		cp = "Blown apart";
		break;
#ifdef	OOZE
	  case SLIME:
		cp = "Slimed";
		break;
#endif
#ifdef	VOLCANO
	  case BLOB:
		sprintf(ouch->p_death, "| Eaten by the Blob |");
		return;
	  case LAVA:
		cp = "Baked";
		break;
#endif
	}
	if (credit == NULL) {
		(void) sprintf(ouch->p_death, "| %s by %s |", cp,
			(shot_type == MINE || shot_type == GMINE) ?
			"a mine" : "act of God");
		return;
	}

	(void) sprintf(ouch->p_death, "| %s by %s |", cp, credit->i_name);

	credit->i_kills++;
	credit->i_score = credit->i_kills / (double) credit->i_entries;
	if (gotcha == NULL)
		return;
	gotcha->p_damcap += STABDAM;
	gotcha->p_damage -= STABDAM;
/*	if (gotcha->p_damage < 0)
		gotcha->p_damage = 0;
Not wanted 'cause -damage is possible now */
	(void) sprintf(Buf, "%3d/%2d", gotcha->p_damage, gotcha->p_damcap);
	cgoto(gotcha, STAT_DAM_ROW, STAT_VALUE_COL);
	outstr(gotcha, Buf, 6);
	(void) sprintf(Buf, "%3d", (gotcha->p_damcap - MAXDAM) / 2);
	cgoto(gotcha, STAT_KILL_ROW, STAT_VALUE_COL);
	outstr(gotcha, Buf, 3);
	(void) sprintf(Buf, "%5.2f", gotcha->p_ident->i_score);
	for (ouch = Player; ouch < End_player; ouch++) {
		cgoto(ouch, STAT_PLAY_ROW + 1 + (gotcha - Player),
			STAT_NAME_COL);
		outstr(ouch, Buf, 5);
	}
}

/*
 * zap:
 *	Kill off a player and take him out of the game.
 */
void
zap(PLAYER *pp, FLAG was_player)
{
	register int	i, len;
	register BULLET	*bp;
	register PLAYER	*np;
	register int	x, y;
	int		savefd, savemask;

	if (was_player) {
		drawplayer(pp, FALSE);
		Nplayer--;
	}

	len = strlen(pp->p_death);	/* Display the cause of death */
	x = (WIDTH - len) / 2;
	cgoto(pp, HEIGHT / 2, x);
	outstr(pp, pp->p_death, len);
	for (i = 1; i < len; i++)
		pp->p_death[i] = '-';
	pp->p_death[0] = '+';
	pp->p_death[len - 1] = '+';
	cgoto(pp, HEIGHT / 2 - 1, x);
	outstr(pp, pp->p_death, len);
	cgoto(pp, HEIGHT / 2 + 1, x);
	outstr(pp, pp->p_death, len);
	cgoto(pp, HEIGHT, 0);

	if (Nplayer == 0) {
#ifdef CONSTANT_MOVE
		bul_alarm(0);
#endif
		cleanup(0);
		/* NOTREACHED */
	}

	savefd = pp->p_fd;
	savemask = pp->p_mask;

#ifdef MONITOR
	if (was_player) {
#endif
		for (bp = Bullets; bp != NULL; bp = bp->b_next) {
			if (bp->b_owner == pp)
				bp->b_owner = NULL;
			if (bp->b_x == pp->p_x && bp->b_y == pp->p_y)
				bp->b_over = SPACE;
		}

		i = rand_num(pp->p_ammo);
		if (i == pp->p_ammo - 1) {
			x = pp->p_ammo;
			len = SLIME;
		}
		else if (i >= BOMBREQ) {
			x = BOMBREQ;
			len = BOMB;
		}
		else if (i >= SSLIMEREQ) {
			x = SSLIMEREQ;
			len = SLIME;
		}
		else if (i >= SATREQ) {
			x = SATREQ;
			len = SATCHEL;
		}
		else if (i >= SLIMEREQ) {
			x = SLIMEREQ;
			len = SLIME;
		}
		else if (i >= GRENREQ) {
			x = GRENREQ;
			len = GRENADE;
		}
		else
			x = 0;
		if (x > 0) {
			add_shot(len, pp->p_y, pp->p_x, pp->p_face, x,
				(PLAYER *) NULL, TRUE, SPACE);
			(void) sprintf(Buf, "%s detonated.",
				pp->p_ident->i_name);
			for (np = Player; np < End_player; np++)
				message(np, Buf);
#ifdef MONITOR
			for (np = Monitor; np < End_monitor; np++)
				message(np, Buf);
#endif
		}

#ifdef VOLCANO
		volcano += pp->p_ammo - x;
#ifdef UNDEFINED
		if (rand_num(100) < volcano / 50) {
#endif
		if (rand_num(300) < 2) {
			do {
				x = rand_num(WIDTH / 2) + WIDTH / 4;
				y = rand_num(HEIGHT / 2) + HEIGHT / 4;
			} while (Maze[y][x] != SPACE);
			add_shot(LAVA, y, x, LEFTS, 300,
				(PLAYER *) NULL, TRUE, SPACE);
			for (np = Player; np < End_player; np++)
				message(np, "Volcano eruption.");
			volcano = 0;
		}
#endif

		sendcom(pp, ENDWIN, 0, 0);
		(void) fclose(pp->p_output);

		End_player--;
		if (pp != End_player) {
			bcopy((char *) End_player, (char *) pp,
				sizeof (PLAYER));
			(void) sprintf(Buf, "%5.2f%c%-10.10s",
				pp->p_ident->i_score, stat_char(pp),
				pp->p_ident->i_name);
			i = STAT_PLAY_ROW + 1 + (pp - Player);
			for (np = Player; np < End_player; np++) {
				cgoto(np, i, STAT_NAME_COL);
				outstr(np, Buf, STAT_NAME_LEN);
			}
#ifdef MONITOR
			for (np = Monitor; np < End_monitor; np++) {
				cgoto(np, i, STAT_NAME_COL);
				outstr(np, Buf, STAT_NAME_LEN);
			}
#endif
		}

		/* Erase the last player */
		i = STAT_PLAY_ROW + 1 + Nplayer;
		for (np = Player; np < End_player; np++) {
			cgoto(np, i, STAT_NAME_COL);
			ce(np);
		}
#ifdef MONITOR
		for (np = Monitor; np < End_monitor; np++) {
			cgoto(np, i, STAT_NAME_COL);
			ce(np);
		}
	}
	else {
		sendcom(pp, ENDWIN, 0, 0);
		(void) putc(LAST_PLAYER, pp->p_output);
		(void) fclose(pp->p_output);

		End_monitor--;
		if (pp != End_monitor) {
			bcopy((char *) End_monitor, (char *) pp,
				sizeof (PLAYER));
			(void) sprintf(Buf, "%5.5s %-10.10s", " ",
				pp->p_ident->i_name);
			i = STAT_MON_ROW + 1 + (pp - Player);
			for (np = Player; np < End_player; np++) {
				cgoto(np, i, STAT_NAME_COL);
				outstr(np, Buf, STAT_NAME_LEN);
			}
			for (np = Monitor; np < End_monitor; np++) {
				cgoto(np, i, STAT_NAME_COL);
				outstr(np, Buf, STAT_NAME_LEN);
			}
		}

		/* Erase the last monitor */
		i = STAT_MON_ROW + 1 + (End_monitor - Monitor);
		for (np = Player; np < End_player; np++) {
			cgoto(np, i, STAT_NAME_COL);
			ce(np);
		}
		for (np = Monitor; np < End_monitor; np++) {
			cgoto(np, i, STAT_NAME_COL);
			ce(np);
		}

	}
#endif

	Fds_mask &= ~savemask;
	if (Num_fds == savefd + 1) {
		Num_fds = Socket;
#ifdef INTERNET
		if (Test_socket > Socket)
			Num_fds = Test_socket;
#endif
		for (np = Player; np < End_player; np++)
			if (np->p_fd > Num_fds)
				Num_fds = np->p_fd;
#ifdef MONITOR
		for (np = Monitor; np < End_monitor; np++)
			if (np->p_fd > Num_fds)
				Num_fds = np->p_fd;
#endif
		Num_fds++;
	}
}

/*
 * rand_num:
 *	Return a random number in a given range.
 */
int
rand_num(int range)
{
	return (range == 0 ? 0 : RN % range);
}

/*
 * havechar:
 *	Check to see if we have any characters in the input queue; if
 *	we do, read them, stash them away, and return TRUE; else return
 *	FALSE.
 */
int
havechar(PLAYER *pp)
{
	extern int	errno;

	if (pp->p_ncount < pp->p_nchar)
		return TRUE;
	if (!(Have_inp & pp->p_mask))
		return FALSE;
	Have_inp &= ~pp->p_mask;
check_again:
	errno = 0;
	if ((pp->p_nchar = read(pp->p_fd, pp->p_cbuf, sizeof pp->p_cbuf)) <= 0)
	{
		if (errno == EINTR)
			goto check_again;
		pp->p_cbuf[0] = 'q';
	}
	pp->p_ncount = 0;
	return TRUE;
}

/*
 * cleanup:
 *	Exit with the given value, cleaning up any droppings lying around
 */
void
cleanup(int eval)
{
	register PLAYER	*pp;

	for (pp = Player; pp < End_player; pp++) {
		cgoto(pp, HEIGHT, 0);
		sendcom(pp, ENDWIN, 0, 0);
		(void) putc(LAST_PLAYER, pp->p_output);
		(void) fclose(pp->p_output);
	}
#ifdef MONITOR
	for (pp = Monitor; pp < End_monitor; pp++) {
		cgoto(pp, HEIGHT, 0);
		sendcom(pp, ENDWIN, 0, 0);
		(void) putc(LAST_PLAYER, pp->p_output);
		(void) fclose(pp->p_output);
	}
#endif
	(void) close(Socket);
#ifdef AF_UNIX_HACK
	(void) unlink(Sock_name);
#endif
	exit(eval);
}

/*
 *  Hunt
 *  Copyright (c) 1985 Conrad C. Huang, Gregory S. Couch, Kenneth C.R.C. Arnold
 *  San Francisco, California
 *
 *  Copyright (c) 1985 Regents of the University of California.
 *  All rights reserved.  The Berkeley software License Agreement
 *  specifies the terms and conditions for redistribution.
 */

#include	<errno.h>
#include	<curses.h>
#include	"hunt.h"
#include	<signal.h>
#include	<ctype.h>
#include	<sys/stat.h>

FLAG	Last_player = FALSE;
#ifdef MONITOR
FLAG	Am_monitor = FALSE;
#endif
FLAG	Query_driver = FALSE;

char	Buf[BUFSIZ];

int	Master_pid;
int	Socket;
#ifdef INTERNET
char	*Sock_host;
#endif

SOCKET	Daemon;
#ifdef	INTERNET
#define	DAEMON_SIZE	(sizeof Daemon)
#else
#define	DAEMON_SIZE	(sizeof Daemon - 1)
#endif

char	map_key[256];			/* what to map keys to */

static char	name[NAMELEN];

extern int	cur_row, cur_col, _putchar();
extern char	*tgoto();

int find_driver(FLAG do_startup);
void env_init(void);

/*
 * main:
 *	Main program for local process
 */
int main(int ac, char **av)
{
	char		*term;
	extern int	errno;
	extern int	Otto_mode;
	/* signal handlers are defined as void functions in prototypes */

	for (ac--, av++; ac > 0 && av[0][0] == '-'; ac--, av++) {
		switch (av[0][1]) {

		case 'l':	/* rsh compatibility */
		case 'n':
			if (ac <= 1)
				goto usage;
			ac--, av++;
			(void) strcpy(name, av[0]);
			break;
		case 'o':
#ifndef OTTO
			fputs("The -o flag is reserved for future use.\n",
				stderr);
			goto usage;
#else /* OTTO */
			Otto_mode = TRUE;
			break;
#endif
#ifdef MONITOR
		case 'm':
			Am_monitor = TRUE;
			break;
#endif
#ifdef INTERNET
		case 'q':	/* query whether hunt is running */
			Query_driver = TRUE;
			break;
		case 'h':
			if (ac <= 1)
				goto usage;
			ac--, av++;
			Sock_host = av[0];
			break;
#endif
		default:
		usage:
#ifdef INTERNET
#  ifdef MONITOR
#   define	USAGE	"usage: hunt [-q] [-n name] [-h host] [-m]\n"
#  else
#   define	USAGE	"usage: hunt [-q] [-n name] [-h host]\n"
#  endif
#else /* INTERNET */
#  ifdef MONITOR
#   define	USAGE	"usage: hunt [-n name] [-m]\n"
#  else
#   define	USAGE	"usage: hunt [-n name]\n"
#  endif
#endif
			fputs(USAGE, stderr);
#undef USAGE
			exit(1);
		}
	}
#ifdef INTERNET
	if (ac > 1)
		goto usage;
	else if (ac > 0)
		Sock_host = av[0];
#else /* INTERNET */
	if (ac > 0)
		goto usage;
#endif	

#ifdef INTERNET
	if (Query_driver) {
		find_driver(FALSE);
		if (Daemon.sin_port != 0) {
			struct	hostent	*hp;

			hp = gethostbyaddr(&Daemon.sin_addr,
				sizeof Daemon.sin_addr, AF_INET);
			fprintf(stderr, "HUNT!! found on %s\n", hp != NULL
				? hp->h_name : inet_ntoa(Daemon.sin_addr));
		}
		exit(Daemon.sin_port == 0);
	}
#endif
#ifdef OTTO
	if (Otto_mode)
		(void) strcpy(name, "otto");
	else
#endif
	env_init();

	(void) fflush(stdout);
	if (!isatty(0) || (term = getenv("TERM")) == NULL) {
		fprintf(stderr, "no terminal type\n");
		exit(1);
	}
	int _tty_ch = 0;
	gettmode();
	setterm(term);
	noecho();
	crmode();
	_puts(TI);
	_puts(VS);
	hunt_clear_screen();
	(void) signal(SIGINT, intr);
	(void) signal(SIGTERM, sigterm);
	(void) signal(SIGEMT, sigemt);
	(void) signal(SIGQUIT, dumpit);
	(void) signal(SIGPIPE, SIG_IGN);
	(void) signal(SIGTSTP, tstp);

	do {
#ifdef	INTERNET
		find_driver(TRUE);

		do {
			int	msg;

#ifndef OLDIPC
			Socket = socket(SOCK_FAMILY, SOCK_STREAM, 0);
#else /* OLDIPC */
			Socket = socket(SOCK_STREAM, 0, 0, 0);
#endif
			if (Socket < 0) {
				perror("socket");
				exit(1);
			}
#ifndef OLDIPC
			msg = 1;
			if (setsockopt(Socket, SOL_SOCKET, SO_USELOOPBACK,
			    &msg, sizeof msg) < 0)
				perror("setsockopt loopback");
#endif
			errno = 0;
			if (connect(Socket, (struct sockaddr *) &Daemon,
			    DAEMON_SIZE) < 0) {
				if (errno != ECONNREFUSED) {
					perror("connect");
					leave(1, "connect");
				}
			}
			else
				break;
			sleep(1);
		} while (close(Socket) == 0);
#else
		/*
		 * set up a socket
		 */

		if ((Socket = socket(SOCK_FAMILY, SOCK_STREAM, 0)) < 0) {
			perror("socket");
			exit(1);
		}

		/*
		 * attempt to connect the socket to a name; if it fails that
		 * usually means that the driver isn't running, so we start
		 * up the driver.
		 */

		Daemon.sun_family = SOCK_FAMILY;
		(void) strcpy(Daemon.sun_path, Sock_name);
		if (connect(Socket, &Daemon, DAEMON_SIZE) < 0) {
			if (errno != ENOENT) {
				perror("connect");
				leave(1, "connect2");
			}
			start_driver();

			do {
				(void) close(Socket);
				if ((Socket = socket(SOCK_FAMILY, SOCK_STREAM, 0)) < 0) {
					perror("socket");
					exit(1);
				}
				sleep(2);
			} while (connect(Socket, &Daemon, DAEMON_SIZE) < 0);
		}
#endif

		do_connect(name);
		playit();
	} while (!quit());
	leave(0, NULL);
	/* NOTREACHED */
}

#ifdef INTERNET
#ifdef BROADCAST
int broadcast_vec(int s, struct sockaddr **vector)
{

	char			if_buf[BUFSIZ];
	struct	ifconf		ifc;
	struct	ifreq		*ifr;
	int			n;
	int			vec_cnt;

	*vector = NULL;
	ifc.ifc_len = sizeof if_buf;
	ifc.ifc_buf = if_buf;
	if (ioctl(s, SIOCGIFCONF, (char *) &ifc) < 0)
		return 0;
	vec_cnt = 0;
	n = ifc.ifc_len / sizeof (struct ifreq);
	*vector = (struct sockaddr *) malloc(n * sizeof (struct sockaddr));
	for (ifr = ifc.ifc_req; n > 0; n--, ifr++)
		if (ioctl(s, SIOCGIFBRDADDR, ifr) >= 0)
			bcopy(&ifr->ifr_addr, &(*vector)[vec_cnt++],
						sizeof (struct sockaddr));
	return vec_cnt;
}
#endif

int find_driver(FLAG do_startup)
{
	/* Return 1 to indicate success */

	int			msg;
	static SOCKET		test;
	int			test_socket;
	int			namelen;
	char			local_name[80];
	static			initial = TRUE;
	static struct in_addr	local_address;
	register struct hostent	*hp;
	void			(*oldsigalrm)(int);
	extern int		errno;
#ifdef BROADCAST
	static	int		brdc;
	static	SOCKET		*brdv;
	int			i;
#endif

	if (Sock_host != NULL) {
		if (!initial)
			return 1;		/* Daemon address already valid */
		initial = FALSE;
		if ((hp = gethostbyname(Sock_host)) == NULL) {
			leave(1, "Unknown host");
			/* NOTREACHED */
		}
		Daemon.sin_family = SOCK_FAMILY;
		Daemon.sin_port = htons(Sock_port);
		Daemon.sin_addr = *((struct in_addr *) hp->h_addr);
		if (!Query_driver)
			return;
	}


	if (initial) {			/* do one time initialization */
#ifndef BROADCAST
		sethostent(1);		/* don't bother to close host file */
#endif
		if (gethostname(local_name, sizeof local_name) < 0) {
			leave(1, "Sorry, I have no name.");
			/* NOTREACHED */
		}
		if ((hp = gethostbyname(local_name)) == NULL) {
			leave(1, "Can't find myself.");
			/* NOTREACHED */
		}
		local_address = * ((struct in_addr *) hp->h_addr);

		test.sin_family = SOCK_FAMILY;
		test.sin_addr = local_address;
		test.sin_port = htons(Test_port);
	}

#ifndef OLDIPC
	test_socket = socket(SOCK_FAMILY, SOCK_DGRAM, 0);
#else /* OLDIPC */
	test_socket = socket(SOCK_DGRAM, 0, 0, 0);
#endif
	if (test_socket < 0) {
		perror("socket");
		leave(1, "socket system call failed");
		/* NOTREACHED */
	}

	msg = 1;
	if (Query_driver && Sock_host != NULL) {
		test.sin_family = SOCK_FAMILY;
		test.sin_addr = Daemon.sin_addr;
		test.sin_port = htons(Test_port);
#ifndef OLDIPC
		(void) sendto(test_socket, (char *) &msg, sizeof msg, 0,
		    (struct sockaddr *) &test, DAEMON_SIZE);
#else /* OLDIPC */
		(void) send(test_socket, (struct sockaddr *) &test,
			(char *) &msg, sizeof msg);
#endif
		goto get_response;
	}

	if (!initial) {
		/* favor host of previous session by broadcasting to it first */
		test.sin_addr = Daemon.sin_addr;
		test.sin_port = htons(Test_port);
		(void) sendto(test_socket, (char *) &msg, sizeof msg, 0,
		    (struct sockaddr *) &test, DAEMON_SIZE);
	}


#ifdef BROADCAST
	if (initial)
		brdc = broadcast_vec(test_socket, &brdv);

	if (brdc <= 0) {
		Daemon.sin_family = SOCK_FAMILY;
		Daemon.sin_addr = local_address;
		Daemon.sin_port = htons(Sock_port);
		initial = FALSE;
		return;
	}

	if (setsockopt(test_socket, SOL_SOCKET, SO_BROADCAST,
	    (int) &msg, sizeof msg) < 0) {
		perror("setsockopt broadcast");
		leave(1, "setsockopt broadcast");
		/* NOTREACHED */
	}

	/* send broadcast packets on all interfaces */
	for (i = 0; i < brdc; i++) {
		bcopy(&brdv[i], &test, sizeof (SOCKET));
		test.sin_port = htons(Test_port);
		if (sendto(test_socket, (char *) &msg, sizeof msg, 0,
		    (struct sockaddr *) &test, DAEMON_SIZE) < 0) {
			perror("sendto");
			leave(1, "sendto");
			/* NOTREACHED */
		}
	}
#else BROADCAST
	/* loop thru all hosts on local net and send msg to them. */
	sethostent(0);		/* rewind host file */
	while (hp = gethostent()) {
		if (inet_netof(test.sin_addr)
		== inet_netof(* ((struct in_addr *) hp->h_addr))) {
			test.sin_addr = * ((struct in_addr *) hp->h_addr);
#ifndef OLDIPC
			(void) sendto(test_socket, (char *) &msg, sizeof msg, 0,
			    (struct sockaddr *) &test, DAEMON_SIZE);
#else /* OLDIPC */
			(void) send(test_socket, (struct sockaddr *) &test,
				(char *) &msg, sizeof msg);
#endif
		}
	}
#endif

get_response:
	namelen = DAEMON_SIZE;
	oldsigalrm = signal(SIGALRM, sigalrm);
	errno = 0;
	(void) alarm(1);
#ifndef OLDIPC
	if (recvfrom(test_socket, (char *) &msg, sizeof msg, 0,
	    (struct sockaddr *) &Daemon, &namelen) < 0)
#else /* OLDIPC */
	if (receive(test_socket, (struct sockaddr *) &Daemon, &msg,
	    sizeof msg) < 0)
#endif
	{
		if (errno != EINTR) {
			perror("recvfrom");
			leave(1, "recvfrom");
			/* NOTREACHED */
		}
		(void) alarm(0);
		(void) signal(SIGALRM, oldsigalrm);
		Daemon.sin_family = SOCK_FAMILY;
		Daemon.sin_port = htons(Sock_port);
		Daemon.sin_addr = local_address;
		if (!do_startup)
			Daemon.sin_port = 0;
		else
			start_driver();
	}
	else {
		(void) alarm(0);
		(void) signal(SIGALRM, oldsigalrm);
		Daemon.sin_port = htons(Sock_port);
	}
	(void) close(test_socket);
	initial = FALSE;
}
#endif

void start_driver(void)
{

	register int	procid;

#ifdef MONITOR
	if (Am_monitor) {
		leave(1, "No one playing.");
		/* NOTREACHED */
	}
#endif

#ifdef INTERNET
	if (Sock_host != NULL) {
		sleep(3);
		return 0;
	}
#endif

	mvcur(cur_row, cur_col, 23, 0);
	cur_row = 23;
	cur_col = 0;
	put_str("Starting...");
	fflush(stdout);
	procid = vfork();
	if (procid == -1) {
		perror("fork");
		leave(1, "fork failed.");
	}
	if (procid == 0) {
		(void) signal(SIGINT, SIG_IGN);
		(void) close(Socket);
		execl(Driver, "HUNT", NULL);
		/* only get here if exec failed */
		kill(getppid(), SIGEMT);	/* tell mom */
		_exit(1);
	}
	mvcur(cur_row, cur_col, 23, 0);
	cur_row = 23;
	cur_col = 0;
	put_str("Connecting...");
	fflush(stdout);
	return 0;
}

/*
 * bad_con:
 *	We had a bad connection.  For the moment we assume that this
 *	means the game is full.
 */
void bad_con(void)
{

	leave(1, "The game is full.  Sorry.");
	/* NOTREACHED */
}

/*
 * dumpit:
 *	Handle a core dump signal by not dumping core, just leaving,
 *	so we end up with a core dump from the driver
 */
void dumpit(int sig)
{

	(void) kill(Master_pid, SIGQUIT);
	(void) chdir("coredump");
	abort();
}

/*
 * sigterm:
 *	Handle a terminate signal
 */
void sigterm(int sig)
{

	leave(0, NULL);
	/* NOTREACHED */
}


/*
 * sigemt:
 *	Handle a emt signal - shouldn't happen on vaxes(?)
 */
void sigemt(int sig)
{

	leave(1, "Unable to start driver.  Try again.");
	/* NOTREACHED */
}

#ifdef INTERNET
/*
 * sigalrm:
 *	Handle an alarm signal
 */
void sigalrm(int sig)
{
	return;
}
#endif

/*
 * rmnl:
 *	Remove a '\n' at the end of a string if there is one
 */
void rmnl(char *s)
{

	register char	*cp;
	char		*rindex();

	cp = rindex(s, '\n');
	if (cp != NULL)
		*cp = '\0';
}

/*
 * intr:
 *	Handle a interrupt signal
 */
void intr(int sig)
{

	register int	ch;
	register int	explained;
	register int	y, x;

	(void) signal(SIGINT, SIG_IGN);
	y = cur_row;
	x = cur_col;
	mvcur(cur_row, cur_col, 23, 0);
	cur_row = 23;
	cur_col = 0;
	put_str("Really quit? ");
	clear_eol();
	fflush(stdout);
	explained = FALSE;
	for (;;) {
		ch = getchar();
		if (isupper(ch))
			ch = tolower(ch);
		if (ch == 'y') {
			(void) write(Socket, "q", 1);
			(void) close(Socket);
			leave(0, NULL);
		}
		else if (ch == 'n') {
			(void) signal(SIGINT, intr);
			mvcur(cur_row, cur_col, y, x);
			cur_row = y;
			cur_col = x;
			fflush(stdout);
			return;
		}
		if (!explained) {
			put_str("(Y or N) ");
			fflush(stdout);
			explained = TRUE;
		}
		(void) putchar('\007'); /* control-g bell */
		(void) fflush(stdout);
	}
}

/*
 * leave:
 *	Leave the game somewhat gracefully, restoring all current
 *	tty stats.
 */
void leave(int eval, char *mesg)
{

	mvcur(cur_row, cur_col, 23, 0);
	if (mesg == NULL)
		clear_eol();
	else {
		put_str(mesg);
		clear_eol();
		putchar('\n');
		fflush(stdout);		/* flush in case VE changes pages */
	} 
	resetty();
	_puts(VE);
	_puts(TE);
	exit(eval);
}

/*
 * tstp:
 *	Handle stop and start signals
 */
void tstp(int sig)
{

	static struct sgttyb	tty;
	int	y, x;

	tty = _tty;
	y = cur_row;
	x = cur_col;
	mvcur(cur_row, cur_col, 23, 0);
	cur_row = 23;
	cur_col = 0;
	_puts(VE);
	_puts(TE);
	(void) fflush(stdout);
	resetty();
	(void) kill(getpid(), SIGSTOP);
	(void) signal(SIGTSTP, tstp);
	_tty = tty;
	(void) stty(_tty_ch, &_tty);
	_puts(TI);
	_puts(VS);
	cur_row = y;
	cur_col = x;
	_puts(tgoto(CM, cur_row, cur_col));
	redraw_screen();
	fflush(stdout);
}

void env_init(void)
{

	register int	i;
	char	*envp, *envname, *s, *index();

	for (i = 0; i < 256; i++)
		map_key[i] = (char) i;

	envname = NULL;
	if ((envp = getenv("HUNT")) != NULL) {
		while ((s = index(envp, '=')) != NULL) {
			if (strncmp(envp, "name=", s - envp + 1) == 0) {
				envname = s + 1;
				if ((s = index(envp, ',')) == NULL) {
					*envp = '\0';
					break;
				}
				*s = '\0';
				envp = s + 1;
			}			/* must be last option */
			else if (strncmp(envp, "mapkey=", s - envp + 1) == 0) {
				for (s = s + 1; *s != '\0'; s += 2) {
					map_key[(unsigned int) *s] = *(s + 1);
					if (*(s + 1) == '\0') {
						break;
					}
				}
				*envp = '\0';
				break;
			} else {
				*s = '\0';
				printf("unknown option %s\n", envp);
				if ((s = index(envp, ',')) == NULL) {
					*envp = '\0';
					break;
				}
				envp = s + 1;
			}
		}
		if (*envp != '\0')
			if (envname == NULL)
				envname = envp;
			else
				printf("unknown option %s\n", envp);
	}
	if (envname != NULL) {
		(void) strcpy(name, envname);
		printf("Entering as '%s'\n", envname);
	}
	else if (name[0] == '\0') {
		printf("Enter your code name: ");
		if (fgets(name, sizeof name, stdin) == NULL)
			exit(1);
	}
	rmnl(name);
}

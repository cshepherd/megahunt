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

void do_connect(char *name)
{
	static long	uid;
	extern char	*ttyname(int);

	uid = htonl(getuid());
	if (write(Socket, (char *) &uid, sizeof uid) < 0) {
		perror("write uid");
		return;
	}
	if (write(Socket, name, NAMELEN) < 0) {
		perror("write name");
		return;
	}
	{
		char *tty = ttyname(fileno(stderr));
		if (tty == NULL) {
			strcpy(Buf, "/dev/console");
		} else {
			strcpy(Buf, tty);
		}
	}
	if (write(Socket, Buf, NAMELEN) < 0) {
		perror("write tty");
		return;
	}
#ifdef MONITOR
	if (write(Socket, (char *) &Am_monitor, sizeof Am_monitor) < 0) {
		perror("write monitor flag");
		return;
	}
#endif
}

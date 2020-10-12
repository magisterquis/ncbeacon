/*
 * nccatch.c
 * Lightweight catcher for reverse shells over TCP
 * By J. Stuart McMurray
 * Created 20201009
 * Last Modified 20201010
 */

#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <err.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFLEN 1024 /* Proxy buffer size */

int proxy(int dst, int src, char *name);

int
main(int argc, char **argv)
{
        struct sockaddr_in sa;
        char *ep, *p;
        unsigned long ul;
        int i, l, c, pd, set;
        socklen_t sl;
        struct pollfd pfds[2];
        int dfds[2];
        char *nfds[2];

        /* Usage */
        if (2 > argc || 3 < argc || 0 == strcmp("-h", argv[1]))
                errx(1, "usage: %s [host] port", argv[0]);

        /* Work out the address */
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        switch (argc) {
                default: /* Unpossible */
                        errx(2, "having %d arguments here is unpossible",
                                        argc);
                case 2: /* Just a port */
                        p = argv[1];
                        sa.sin_addr.s_addr = INADDR_ANY;
                        break;
                case 3: /* Port and address */
                        p = argv[2];
                        if (INADDR_NONE == (sa.sin_addr.s_addr =
                                                inet_addr(argv[1])))
                                errx(3, "invalid listen addresss");
                        break;
        }

        /* Work out the port */
        if (0 == (ul = strtoul(p, &ep, 10)) || 0xFFFF < ul)
                errx(4, "invalid port %s", p);
        sa.sin_port = htons(ul);

        /* Get a socket and listen */
        if (-1 == (l = socket(AF_INET, SOCK_STREAM, 0)))
                err(5, "socket");
        set = 1;
        if (-1 == setsockopt(l, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)))
                err(13, "setsockopt (SO_REUSEADDR)");
        if (-1 == bind(l, (struct sockaddr *)&sa, sizeof(sa)))
                err(6, "bind");
        if (-1 == listen(l, 1))
                err(7, "listen");
        memset(&sa, 0, sizeof(sa));
        sl = sizeof(sa);
        if (-1 == getsockname(l, (struct sockaddr *)&sa, &sl))
                err(8, "getsockname");
        fprintf(stderr, "Listening on %s:%hu\n",
                        inet_ntoa(sa.sin_addr), ntohs(sa.sin_port));

        /* Wait for a connection */
        memset(&sa, 0, sizeof(sa));
        if (-1 == (c = accept(l, (struct sockaddr *)&sa, &sl)))
                err(9, "accept");
        fprintf(stderr, "Connection from %s:%hu\n",
                        inet_ntoa(sa.sin_addr), ntohs(sa.sin_port));
        if (-1 == close(l))
                err(14, "close listener");

        /* Set up to wait for input */
        memset(pfds, 0, sizeof(pfds));
        memset(dfds, 0, sizeof(dfds));
        pfds[0].fd = STDIN_FILENO;
        pfds[0].events = POLLIN;
        dfds[0] = c;
        nfds[0] = "stdin";
        pfds[1].fd = c;
        pfds[1].events = POLLIN;
        dfds[1] = STDOUT_FILENO;
        nfds[1] = "client";

        /* Proxy data */
        for (;;) {
                /* Wait for data */
                switch (poll(pfds, 2, INFTIM)) {
                        case -1: /* Error */
                                err(11, "poll");
                        case 0: /* Nothing to read? */
                                warnx("woken up but nothing to read");
                                continue;
                }
                /* Try to proxy */
                pd = 0;
                for (i = 0; i < 2; ++i) {
                        /* Ignore closed files */
                        if (-1 == pfds[i].fd)
                                continue;

                        if (pfds[i].revents & POLLIN) { /* Something to read */
                                if (-1 == proxy(dfds[i], pfds[i].fd, nfds[i]))
                                        pfds[i].fd = -1;
                                else
                                        pd |= 1;
                        } else if (pfds[i].revents & POLLHUP) { /* Closed */
                                warnx("disconnect from %s", nfds[i]);
                                pfds[i].fd = -1;
                        } else if (pfds[i].revents & POLLNVAL) { /* Invalid FD */
                                errx(15, "invalid fd %d for %s", pfds[i].fd,
                                                nfds[i]);
                        } else if (pfds[i].revents & POLLERR) {
                                errx(16, "error waiting on fd %d for %s",
                                                pfds[i].fd, nfds[i]);
                        } else if (0 != pfds[i].revents) {
                                errx(17, "poll on fd %d for %s got unexpected revents %hx",
                                                pfds[i].fd, nfds[i], pfds[i].revents);
                        }

                        /* If this fd is pollable, note it */
                        if (-1 != pfds[i].fd)
                                pd |= 1;
                }

                /* Give up if no descriptors are pollable */
                if (!pd)
                        break;
        }

        return 0;
}

/* proxy reads up to BUFLEN from src and sends to dst.  It returns -1 if the
 * src should no longer be polled. */
int
proxy(int dst, int src, char *name)
{
        unsigned char buf[BUFLEN];
        ssize_t nr, nw, off;

        /* Read from the source */
        if (-1 == (nr = read(src, buf, sizeof(buf)))) {
                warn("read from %s", name);
                return -1;
        } else if (0 == nr) {
                warnx("EOF from %s", name);
                if (-1 == close(src))
                        warn("close %s", name);
                /* Shutdown the receiving end.  This will fail on stdout. */
                shutdown(dst, SHUT_WR);
                return -1;
        }

        /* Write to the destination */
        for (off = 0; off < nr; off += nw)
                if (-1 == (nw = write(dst, buf + off, nr - off))) {
                        warn("write from %s", name);
                        return -1;
                }


        return 0;
}

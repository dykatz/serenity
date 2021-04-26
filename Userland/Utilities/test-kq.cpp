/*
 * Copyright (c) 2021, Dylan Katz <dykatz@uw.edu>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibCore/ArgsParser.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

int check_inheritance();
int do_fdpass();
int do_timer();
int do_invalid_timer();
int do_pipe();
int do_process();
int do_random();
int do_signal();

int main(int argc, char** argv)
{
    int ret { 0 };

    bool check_inheritance_flag { false };
    bool do_fdpass_flag { false };
    bool do_timer_flag { false };
    bool do_invalid_timer_flag { false };
    bool do_pipe_flag { false };
    bool do_process_flag { false };
    bool do_random_flag { false };
    bool do_signal_flag { false };

    Core::ArgsParser args_parser;

    args_parser.add_option(check_inheritance_flag, "Check inheritance", "check-inheritance", 'f');
    args_parser.add_option(do_fdpass_flag, "Perform fdpass test", "fdpass", 'F');
    args_parser.add_option(do_timer_flag, "Perform timer test", "timer", 'i');
    args_parser.add_option(do_invalid_timer_flag, "Perform invalid timer test", "invalid-timer", 'I');
    args_parser.add_option(do_pipe_flag, "Perform pipe test", "pipe", 'p');
    args_parser.add_option(do_process_flag, "Perform process test", "process", 'P');
    args_parser.add_option(do_random_flag, "Perform random test", "random", 'r');
    args_parser.add_option(do_signal_flag, "Perform signal test", "signal", 's');

    args_parser.parse(argc, argv);

    if (check_inheritance_flag)
        ret |= check_inheritance();

    if (do_fdpass_flag)
        ret |= do_fdpass();

    if (do_timer_flag)
        ret |= do_timer();

    if (do_invalid_timer_flag)
        ret |= do_invalid_timer();

    if (do_pipe_flag)
        ret |= do_pipe();

    if (do_process_flag)
        ret |= do_process();

    if (do_random_flag)
        ret |= do_random();

    if (do_signal_flag)
        ret |= do_signal();

    return ret;
}

/*
 * Adapted from OpenBSD regress/sys/kern/kqueue/main.h
 *
 * Written by Alexaner Bluhm <bluhm@openbsd.org> 2016 Public Domain
 */

#define ASS(cond, mess) \
    do {                \
        if (!(cond)) {  \
            mess;       \
            return (1); \
        }               \
    } while (0)

#define ASSX(cond) ASS(cond,                                         \
    fprintf(stderr, "assertion " #cond " failed in %s on line %d\n", \
        __FILE__, __LINE__))

#define MIN(a, b) ((a) < (b) ? (a) : (b))

int check_inheritance()
{
    return 0;
}

/*
 * Adapted from OpenBSD regress/sys/kern/kqueue/kqueue-fdpass.c
 *
 * Written by Philip Guenther <guenther@openbsd.org> 2011 Public Domain
 */

int do_fdpass()
{
    struct msghdr msg;
    union {
        struct cmsghdr hdr;
        char buf[CMSG_SPACE(sizeof(int))];
    } cmsgbuf;
    struct kevent ke;
    struct cmsghdr* cmp;
    pid_t pid;
    int pfd[2], fd, status;

    ASS(socketpair(PF_LOCAL, SOCK_STREAM, 0, pfd) == 0,
        warn("socketpair"));

    pid = fork();

    if (pid == -1) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        close(pfd[0]);

        /* a kqueue with event to pass */
        fd = kqueue();
        EV_SET(&ke, SIGHUP, EVFILT_SIGNAL, EV_ADD | EV_ENABLE, 0, 0, NULL);

        if (kevent(fd, &ke, 1, NULL, 0, NULL) != 0) {
            perror("can't register events on kqueue");
            exit(1);
        }

        memset(&cmsgbuf.buf, 0, sizeof cmsgbuf.buf);
        memset(&msg, 0, sizeof msg);
        msg.msg_control = &cmsgbuf.buf;
        msg.msg_controllen = sizeof(cmsgbuf);

        cmp = CMSG_FIRSTHDR(&msg);
        cmp->cmsg_len = CMSG_LEN(sizeof(int));
        cmp->cmsg_level = SOL_SOCKET;
        cmp->cmsg_type = SCM_RIGHTS;

        *(int*)CMSG_DATA(cmp) = fd;

        if (sendmsg(pfd[1], &msg, 0) == 0) {
            fprintf(stderr, "sendmsg succeeded when it shouldn't");
            exit(1);
        }

        if (errno != EINVAL) {
            perror("child sendmsg");
            exit(1);
        }

        printf("sendmsg failed with EINVAL as expected\n");
        close(pfd[1]);
        exit(0);
    }

    close(pfd[1]);
    wait(&status);
    close(pfd[0]);

    return (0);
}

/*
 * Adapted from OpenBSD regress/sys/kern/kqueue/kqueue-timer.c
 *
 * Copyright (c) 2015 Bret Stephen Lambert <blambert@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

int do_timer()
{
    int kq, n;
    struct kevent ev;
    struct timespec ts;

    ASS((kq = kqueue()) >= 0, perror("kqueue"));

    memset(&ev, 0, sizeof(ev));
    ev.filter = EVFILT_TIMER;
    ev.flags = EV_ADD | EV_ENABLE | EV_ONESHOT;
    ev.data = 500; /* 1/2 second in ms */

    n = kevent(kq, &ev, 1, NULL, 0, NULL);
    ASSX(n != -1);

    ts.tv_sec = 2; /* wait 2s for kqueue timeout */
    ts.tv_nsec = 0;

    n = kevent(kq, NULL, 0, &ev, 1, &ts);
    ASSX(n == 1);

    /* Now retry w/o EV_ONESHOT, as EV_CLEAR is implicit */

    memset(&ev, 0, sizeof(ev));
    ev.filter = EVFILT_TIMER;
    ev.flags = EV_ADD | EV_ENABLE;
    ev.data = 500; /* 1/2 second in ms */

    n = kevent(kq, &ev, 1, NULL, 0, NULL);
    ASSX(n != -1);

    ts.tv_sec = 2; /* wait 2s for kqueue timeout */
    ts.tv_nsec = 0;

    n = kevent(kq, NULL, 0, &ev, 1, &ts);
    ASSX(n == 1);

    return (0);
}

int do_invalid_timer()
{
    int i, kq, n;
    struct kevent ev;
    struct timespec invalid_ts[3] = { { -1, 0 }, { 0, -1 }, { 0, 1000000000L } };

    ASS((kq = kqueue()) >= 0, perror("kqueue"));

    memset(&ev, 0, sizeof(ev));
    ev.filter = EVFILT_TIMER;
    ev.flags = EV_ADD | EV_ENABLE;
    ev.data = 500; /* 1/2 second in ms */

    n = kevent(kq, &ev, 1, NULL, 0, NULL);
    ASSX(n != -1);

    for (i = 0; i < 3; i++) {
        n = kevent(kq, NULL, 0, &ev, 1, &invalid_ts[i]);
        ASS(
            n == -1 && errno == EINVAL,
            do {
                perror("kevent");
                fprintf(stderr, "timeout %lld %ld",
                    (long long)invalid_ts[i].tv_sec, invalid_ts[i].tv_nsec);
            } while (0));
    }

    return (0);
}

/*
 * Adapted from OpenBSD regress/sys/kern/kqueue/kqueue-pipe.c
 *
 * Copyright 2001 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

int do_pipe()
{
    int kq;
    int n;
    int fd[2];
    struct kevent ev;
    struct timespec ts;
    char buf[8000];

    ASS(pipe(fd) == 0,
        warn("pipe"));
    ASS(fcntl(fd[1], F_SETFL, O_NONBLOCK) == 0,
        warn("fcntl"));

    ASS((kq = kqueue()) >= 0,
        warn("fcntl"));

    memset(&ev, 0, sizeof(ev));
    ev.ident = fd[1];
    ev.filter = EVFILT_WRITE;
    ev.flags = EV_ADD | EV_ENABLE;
    n = kevent(kq, &ev, 1, NULL, 0, NULL);
    ASSX(n != -1);

    memset(buf, 0, sizeof(buf));
    while (write(fd[1], buf, sizeof(buf)) == sizeof(buf))
        ;

    ts.tv_sec = 0;
    ts.tv_nsec = 0;
    n = kevent(kq, NULL, 0, &ev, 1, &ts);
    ASSX(n == 0);

    read(fd[0], buf, sizeof(buf));

    ts.tv_sec = 0;
    ts.tv_nsec = 0;
    n = kevent(kq, NULL, 0, &ev, 1, &ts);
    ASSX(n != -1 && n != 0);

    close(fd[0]);
    close(fd[1]);
    return (0);
}

/*
 * Adapted from OpenBSD regress/sys/kern/kqueue/kqueue-process.c
 *
 * Written by Artur Grabowski <art@openbsd.org> 2002 Public Domain
 */

int process_child();

int process_pfd1[2];
int process_pfd2[2];

int do_process()
{
    struct kevent ke;
    struct timespec ts;
    int kq, status;
    pid_t pid, pid2;
    int didfork, didchild;
    int i;
    char ch = 0;

    /*
     * Timeout in case something doesn't work.
     */
    ts.tv_sec = 10;
    ts.tv_nsec = 0;

    ASS((kq = kqueue()) >= 0,
        perror("kqueue"));

    /* Open pipes for synchronizing the children with the parent. */
    if (pipe(process_pfd1) == -1) {
        perror("pipe 1");
        exit(1);
    }
    if (pipe(process_pfd2) == -1) {
        perror("pipe 2");
        exit(1);
    }

    switch ((pid = fork())) {
    case -1:
        perror("fork");
        exit(1);
    case 0:
        _exit(process_child());
    }

    EV_SET(&ke, pid, EVFILT_PROC, EV_ADD | EV_ENABLE | EV_CLEAR,
        NOTE_EXIT | NOTE_FORK | NOTE_EXEC | NOTE_TRACK, 0, NULL);
    ASS(kevent(kq, &ke, 1, NULL, 0, NULL) == 0,
        perror("can't register events on kqueue"));

    /* negative case */
    EV_SET(&ke, pid + (1ULL << 30), EVFILT_PROC, EV_ADD | EV_ENABLE | EV_CLEAR,
        NOTE_EXIT | NOTE_FORK | NOTE_EXEC | NOTE_TRACK, 0, NULL);
    ASS(kevent(kq, &ke, 1, NULL, 0, NULL) != 0,
        fprintf(stderr, "can register bogus pid on kqueue\n"));
    ASS(errno == ESRCH,
        perror("register bogus pid on kqueue returned wrong error"));

    ASS(write(process_pfd1[1], &ch, 1) == 1,
        perror("write sync 1"));

    didfork = didchild = 0;

    pid2 = -1;
    for (i = 0; i < 2; i++) {
        ASS(kevent(kq, NULL, 0, &ke, 1, &ts) == 1,
            perror("didn't receive event"));
        ASSX(ke.filter == EVFILT_PROC);
        switch (ke.fflags) {
        case NOTE_CHILD:
            didchild = 1;
            ASSX((pid_t)ke.data == pid);
            pid2 = ke.ident;
            fprintf(stderr, "child %d (from %d)\n", pid2, pid);
            break;
        case NOTE_FORK:
            didfork = 1;
            ASSX(ke.ident == (uintptr_t)pid);
            fprintf(stderr, "fork\n");
            break;
        case NOTE_TRACKERR:
            fprintf(stderr, "child tracking failed due to resource shortage");
            exit(1);
        default:
            fprintf(stderr, "kevent returned weird event 0x%x pid %d",
                ke.fflags, (pid_t)ke.ident);
            exit(1);
        }
    }

    ASSX(pid2 != -1);

    /* Both children now sleeping. */

    ASSX(didchild == 1);
    ASSX(didfork == 1);

    ASS(write(process_pfd2[1], &ch, 1) == 1,
        warn("write sync 2.1"));
    ASS(write(process_pfd1[1], &ch, 1) == 1,
        warn("write sync 2"));

    /*
     * Wait for child's exit. It also implies child-child has exited.
     * This should ensure that NOTE_EXIT has been posted for both children.
     * Child-child's events should get aggregated.
     */
    if (wait(&status) < 0) {
        perror("wait");
        exit(1);
    }

    for (i = 0; i < 2; i++) {
        ASS(kevent(kq, NULL, 0, &ke, 1, &ts) == 1,
            fprintf(stderr, "didn't receive event\n"));
        ASSX(ke.filter == EVFILT_PROC);
        switch (ke.fflags) {
        case NOTE_EXIT:
            ASSX((pid_t)ke.ident == pid);
            fprintf(stderr, "child exit %d\n", pid);
            break;
        case NOTE_EXEC | NOTE_EXIT:
            ASSX((pid_t)ke.ident == pid2);
            fprintf(stderr, "child-child exec/exit %d\n", pid2);
            break;
        default:
            fprintf(stderr, "kevent returned weird event 0x%x pid %d",
                ke.fflags, (pid_t)ke.ident);
            exit(1);
        }
    }

    if (!WIFEXITED(status)) {
        fprintf(stderr, "child didn't exit?");
        exit(1);
    }

    close(kq);
    return (WEXITSTATUS(status) != 0);
}

int process_child()
{
    int status;
    char ch;

    ASS(read(process_pfd1[0], &ch, 1) == 1,
        perror("read sync 1"));

    /* fork and see if tracking works. */
    switch (fork()) {
    case -1:
        perror("fork");
        exit(1);
    case 0:
        ASS(read(process_pfd2[0], &ch, 1) == 1,
            warn("read sync 2.1"));
        execl("/usr/bin/true", "true", (char*)NULL);
        perror("execl(true)");
        exit(1);
    }

    ASS(read(process_pfd1[0], &ch, 1) == 1,
        warn("read sync 2"));

    if (wait(&status) < 0) {
        perror("wait 2");
        exit(1);
    }
    if (!WIFEXITED(status)) {
        fprintf(stderr, "child-child didn't exit?\n");
        exit(1);
    }

    return 0;
}

/*
 * Adapted from OpenBSD regress/sys/kern/kqueue/kqueue-random.c
 *
 * Written by Michael Shalayeff, 2002, Public Domain
 */

int do_random()
{
    int n, fd, kq;
    struct timespec ts;
    struct kevent ev;
    int32_t buf[BUFSIZ];

    ASS((fd = open("/dev/random", O_RDONLY)) >= 0, perror("open: /dev/random"));
    ASS(fcntl(fd, F_SETFL, O_NONBLOCK) == 0, perror("fcntl"));
    ASS((kq = kqueue()) >= 0, perror("kqueue"));

    memset(&ev, 0, sizeof(ev));
    ev.ident = fd;
    ev.filter = EVFILT_READ;
    ev.flags = EV_ADD | EV_ENABLE;
    n = kevent(kq, &ev, 1, NULL, 0, NULL);
    ASSX(n != -1);

    ts.tv_sec = 0;
    ts.tv_nsec = 0;
    n = kevent(kq, NULL, 0, &ev, 1, &ts);
    ASSX(n >= 0);

    n = MIN(((unsigned long)ev.data + 7) / 8, sizeof(buf));
    ASSX(read(fd, buf, n) > 0);

    close(kq);
    close(fd);
    return (0);
}

/*
 * Adapted from OpenBSD regress/sys/kern/kqueue/kqueue-signal.c
 *
 * Written by Philip Guenther <guenther@openbsd.org> 2011 Public Domain
 */

volatile sig_atomic_t saw_usr1 = 0;
volatile sig_atomic_t result = 0;
int signal_kq;

int signal_sigtest(int, int);
void signal_usr1handler(int);

int do_signal()
{
    struct kevent ke;
    pid_t pid = getpid();
    sigset_t mask;

    signal_kq = kqueue();

    signal(SIGUSR1, signal_usr1handler);
    signal(SIGUSR2, SIG_IGN);

    EV_SET(&ke, SIGUSR1, EVFILT_SIGNAL, EV_ADD | EV_ENABLE, 0, 0, nullptr);
    ASS(kevent(signal_kq, &ke, 1, NULL, 0, NULL) == 0, perror("can't register events on kqueue"));
    EV_SET(&ke, SIGUSR2, EVFILT_SIGNAL, EV_ADD | EV_ENABLE, 0, 0, nullptr);
    ASS(kevent(signal_kq, &ke, 1, nullptr, 0, nullptr) == 0, perror("can't register events on kqueue"));

    EV_SET(&ke, 10000, EVFILT_SIGNAL, EV_ADD | EV_ENABLE, 0, 0, nullptr);
    ASS(kevent(signal_kq, &ke, 1, nullptr, 0, nullptr) != 0, fprintf(stderr, "registered bogus signal on kqueue\n"));
    ASS(errno == EINVAL, perror("registering bogus signal on kqueue returned wrong error"));

    ASSX(saw_usr1 == 0);
    kill(pid, SIGUSR1);
    ASSX(saw_usr1 == 1);

    kill(pid, SIGUSR2);
    ASSX(signal_sigtest(SIGUSR2, 1) == 0);
    kill(pid, SIGUSR2);
    kill(pid, SIGUSR2);
    ASSX(signal_sigtest(SIGUSR2, 2) == 0);

    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    signal(SIGUSR1, SIG_DFL);
    kill(pid, SIGUSR1);
    kill(pid, SIGUSR2);

    close(signal_kq);
    return (0);
}

int signal_sigtest(int signum, int catch_)
{
    struct kevent ke;
    struct timespec ts;

    ts.tv_sec = 10;
    ts.tv_nsec = 0;

    if (kevent(signal_kq, nullptr, 0, &ke, 1, &ts) != 1) {
        perror("can't fetch event on kqueue");
    }

    ASSX(ke.filter == EVFILT_SIGNAL);
    ASSX(ke.ident == (uintptr_t)signum);
    ASSX(ke.data == catch_);
    return (0);
}

void signal_usr1handler(int)
{
    saw_usr1 = 1;
    result = signal_sigtest(SIGUSR1, 1);
}

/*
 * Copyright (c) 2021, Dylan Katz <dykatz@uw.edu>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <sys/cdefs.h>
#include <sys/time.h>
#include <sys/types.h>

/* Options for kevent->filter */
#define EVFILT_READ (-1)
#define EVFILT_WRITE (-2)
#define EVFILT_AIO (-3)
#define EVFILT_PROC (-4)
#define EVFILT_SIGNAL (-5)
#define EVFILT_TIMER (-6)

#define EVFILT_SYSCOUNT 6

/* Action options for kevent->flags */
#define EV_ADD (0x1)
#define EV_DELETE (0x2)
#define EV_ENABLE (0x4)
#define EV_DISABLE (0x8)

/* Flag options for kevent->flags */
#define EV_ONESHOT (0x10)
#define EV_CLEAR (0x20)
#define EV_RECEIPT (0x40)
#define EV_DISPATCH (0x80)

/* Returned flag options from kevent->flags */
#define EV_EOF (0x8000)
#define EV_ERROR (0x4000)

/* Filter flags for kevent->fflags when kevent->filter is EVFILT_{READ,WRITE} */
#define NOTE_LOWAT (0x1)
#define NOTE_EOF (0x2)

/* Filter flags for kevent->fflags when kevent->filter is EVFILT_PROC */
#define NOTE_EXIT (0x80000000)
#define NOTE_FORK (0x40000000)
#define NOTE_EXEC (0x20000000)
#define NOTE_PCTRLMASK (0xF0000000)
#define NOTE_PDATAMASK (0x000FFFFF)
#define NOTE_TRACK (0x1)
#define NOTE_TRACKERR (0x2)
#define NOTE_CHILD (0x4)

struct kevent {
    uintptr_t ident;
    short filter;
    unsigned short flags;
    unsigned int fflags;
    int64_t data;
    void* udata;
};

#define EV_SET(kevp, a, b, c, d, e, f)  \
    do {                                \
        struct kevent* __kevp = (kevp); \
        (__kevp)->ident = (a);          \
        (__kevp)->filter = (b);         \
        (__kevp)->flags = (c);          \
        (__kevp)->fflags = (d);         \
        (__kevp)->data = (e);           \
        (__kevp)->udata = (f);          \
    } while (0)

__BEGIN_DECLS

int kqueue(void);
int kevent(int, const struct kevent*, int, struct kevent*, int, const struct timespec*);

__END_DECLS

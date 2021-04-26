/*
 * Copyright (c) 2021, Dylan Katz <dykatz@uw.edu>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <errno.h>
#include <sys/event.h>
#include <syscall.h>

extern "C" {

int kqueue(void)
{
    int rc = syscall(SC_kqueue);
    __RETURN_WITH_ERRNO(rc, rc, -1);
}

int kevent(int kq, const struct kevent* changes, int nchanges, struct kevent* events, int nevents, const struct timespec* to)
{
    Syscall::SC_kevent_params params { kq, changes, nchanges, events, nevents, to };
    int rc = syscall(SC_kevent, &params);
    __RETURN_WITH_ERRNO(rc, rc, -1);
}
}

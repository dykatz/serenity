/*
 * Copyright (c) 2021, Dylan Katz <dykatz@uw.edu>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/EventQueue.h>

namespace Kernel {

EventQueue::~EventQueue()
{
}

KResult EventQueue::apply_change(kevent* event)
{
    (void)event;
    return ENOTSUP;
}

}

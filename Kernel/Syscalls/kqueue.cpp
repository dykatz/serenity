/*
 * Copyright (c) 2021, Dylan Katz <dykatz@uw.edu>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/EventQueue.h>
#include <Kernel/FileSystem/FileDescription.h>
#include <Kernel/Process.h>

namespace Kernel {

KResultOr<int> Process::sys$kqueue()
{
    REQUIRE_PROMISE(stdio);

    int new_fd = alloc_fd();
    if (new_fd < 0)
        return new_fd;

    auto anon_file = EventQueue::create();
    auto description_or_error = FileDescription::create(*anon_file);
    if (description_or_error.is_error())
        return description_or_error.error();

    auto description = description_or_error.release_value();
    m_fds[new_fd].set(move(description), FD_CLOEXEC);
    return new_fd;
}

KResultOr<int> Process::sys$kevent(Userspace<const Syscall::SC_kevent_params*> user_params)
{
    REQUIRE_PROMISE(stdio);

    Syscall::SC_kevent_params params {};
    if (!copy_from_user(&params, user_params))
        return EFAULT;

    auto kq = params.kq;
    auto changelist = params.changelist;
    auto nchanges = params.nchanges;
    auto eventlist = params.eventlist;
    auto nevents = params.nevents;
    auto to = params.timeout;

    auto description = file_description(kq);
    if (!description)
        return EBADF;

    auto& file = description->file();
    if (!file.is_event_queue())
        return EBADF;

    auto& event_queue = static_cast<EventQueue&>(file);
    auto change_list_had_errors = 0;
    kevent* change_list = nullptr;

    if (nchanges != 0) {
        change_list = static_cast<kevent*>(kmalloc(sizeof(kevent) * nchanges));
        if (!change_list)
            return ENOMEM;
        if (!copy_n_from_user(change_list, changelist, nchanges)) {
            kfree(change_list);
            return EFAULT;
        }

        for (auto change = change_list; change < change_list + nchanges; ++change) {
            auto result = event_queue.apply_change(change);
            if (result.is_error()) {
                change_list_had_errors++;
                change->data = result;
                change->flags |= EV_ERROR;
            }
        }

        if (change_list_had_errors == 0)
            kfree(change_list);
    }

    if (change_list_had_errors != 0 && change_list_had_errors > nevents) {
        for (auto change = change_list; change < change_list + nchanges; ++change) {
            if ((change->flags & EV_ERROR) == 0)
                continue;
            KResult final_result { (ErrnoCode)change->data };
            kfree(change_list);
            return final_result;
        }
        VERIFY_NOT_REACHED();
    }

    if (nevents != 0) {
        kevent* event_list = static_cast<kevent*>(kmalloc(sizeof(kevent) * nevents));
        if (!event_list)
            return ENOMEM;

        if (change_list_had_errors != 0) {
            for (auto change = change_list, event = event_list; change < change_list + nchanges; ++change) {
                if ((change->flags & EV_ERROR) == 0)
                    continue;
                memcpy(event, change, sizeof(kevent));
                event++;
            }
            kfree(change_list);
            if (!copy_n_to_user(eventlist, event_list, nevents)) {
                kfree(event_list);
                return EFAULT;
            }
            kfree(event_list);
        }

        Thread::BlockTimeout block_to;
        if (to) {
            Optional<Time> time_out = copy_time_from_user(to);
            if (!time_out.has_value()) {
                kfree(event_list);
                return EFAULT;
            }
        }

        // TODO

        if (!copy_n_to_user(eventlist, event_list, nevents)) {
            kfree(event_list);
            return EFAULT;
        }

        kfree(event_list);
    }

    return 0;
}

}

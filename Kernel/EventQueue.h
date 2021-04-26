/*
 * Copyright (c) 2021, Dylan Katz <dykatz@uw.edu>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Vector.h>
#include <Kernel/FileSystem/File.h>

namespace Kernel {

class EventNote {
public:
    virtual ~EventNote() = default;

    int flags() const { return m_flags; }
    void set_flags(int flags) { m_flags = flags; }

    virtual int attach() = 0;
    virtual void detach() = 0;
    virtual int event(long hint) = 0;
    virtual int modify() = 0;
    virtual int process() = 0;

protected:
    int m_flags;
};

class EventQueue final : public File {
public:
    static NonnullRefPtr<EventQueue> create()
    {
        return adopt_ref(*new EventQueue());
    }

    virtual ~EventQueue() override;
    virtual bool is_event_queue() const override { return true; }

    KResult apply_change(kevent*);

private:
    virtual const char* class_name() const override { return "EventQueue"; }
    virtual String absolute_path(const FileDescription&) const override { return ":event-queue:"; }
    virtual bool can_read(const FileDescription&, size_t) const override { return false; }
    virtual bool can_write(const FileDescription&, size_t) const override { return false; }
    virtual KResultOr<size_t> read(FileDescription&, u64, UserOrKernelBuffer&, size_t) override { return ENOTSUP; }
    virtual KResultOr<size_t> write(FileDescription&, u64, const UserOrKernelBuffer&, size_t) override { return ENOTSUP; }
};

}

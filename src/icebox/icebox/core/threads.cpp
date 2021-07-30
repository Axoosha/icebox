#include "threads.hpp"

#define PRIVATE_CORE_
#define FDP_MODULE "threads"
#include "core.hpp"
#include "core_private.hpp"
#include "interfaces/if_os.hpp"

bool threads::list(core::Core& core, proc_t proc, threads::on_thread_fn on_thread)
{
    return core.os_->thread_list(proc, std::move(on_thread));
}

opt<thread_t> threads::current(core::Core& core)
{
    return core.os_->thread_current();
}

opt<proc_t> threads::process(core::Core& core, thread_t thread)
{
    return core.os_->thread_proc(thread);
}

opt<uint64_t> threads::program_counter(core::Core& core, proc_t proc, thread_t thread)
{
    return core.os_->thread_pc(proc, thread);
}

uint64_t threads::tid(core::Core& core, proc_t proc, thread_t thread)
{
    return core.os_->thread_id(proc, thread);
}

opt<bpid_t> threads::listen_create(core::Core& core, const on_event_fn& on_thread_event)
{
    return core.os_->listen_thread_create(on_thread_event);
}

opt<bpid_t> threads::listen_delete(core::Core& core, const on_event_fn& on_thread_event)
{
    return core.os_->listen_thread_delete(on_thread_event);
}

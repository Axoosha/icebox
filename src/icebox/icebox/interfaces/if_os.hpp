#pragma once

#include "types.hpp"

#include <functional>
#include <memory>

namespace core { struct Core; }
namespace io { struct Io; }
namespace nt { struct Os; }

namespace os
{
    struct Module
    {
        virtual ~Module() = default;

        virtual bool        setup               () = 0;
        virtual bool        is_kernel_address   (uint64_t ptr) = 0;
        virtual bool        read_page           (void* dst, uint64_t ptr, proc_t* proc, dtb_t dtb) = 0;
        virtual bool        write_page          (uint64_t ptr, const void* src, proc_t* proc, dtb_t dtb) = 0;
        virtual opt<phy_t>  virtual_to_physical (proc_t* proc, dtb_t dtb, uint64_t ptr) = 0;
        virtual dtb_t       kernel_dtb          () = 0;

        virtual bool                proc_list       (process::on_proc_fn on_proc) = 0;
        virtual opt<proc_t>         proc_current    () = 0;
        virtual opt<proc_t>         proc_find       (std::string_view name, flags_t flags) = 0;
        virtual opt<proc_t>         proc_find       (uint64_t pid) = 0;
        virtual opt<std::string>    proc_name       (proc_t proc) = 0;
        virtual bool                proc_is_valid   (proc_t proc) = 0;
        virtual uint64_t            proc_id         (proc_t proc) = 0;
        virtual flags_t             proc_flags      (proc_t proc) = 0;
        virtual opt<proc_t>         proc_parent     (proc_t proc) = 0;

        virtual bool            thread_list     (proc_t proc, threads::on_thread_fn on_thread) = 0;
        virtual opt<thread_t>   thread_current  () = 0;
        virtual opt<proc_t>     thread_proc     (thread_t thread) = 0;
        virtual opt<uint64_t>   thread_pc       (proc_t proc, thread_t thread) = 0;
        virtual uint64_t        thread_id       (proc_t proc, thread_t thread) = 0;

        virtual bool                mod_list(proc_t proc, modules::on_mod_fn on_mod) = 0;
        virtual opt<std::string>    mod_name(proc_t proc, mod_t mod) = 0;
        virtual opt<span_t>         mod_span(proc_t proc, mod_t mod) = 0;
        virtual opt<mod_t>          mod_find(proc_t proc, uint64_t addr) = 0;

        virtual bool                vm_area_list    (proc_t proc, vm_area::on_vm_area_fn on_vm_area) = 0;
        virtual opt<vm_area_t>      vm_area_find    (proc_t proc, uint64_t addr) = 0;
        virtual opt<span_t>         vm_area_span    (proc_t proc, vm_area_t vm_area) = 0;
        virtual vma_access_e        vm_area_access  (proc_t proc, vm_area_t vm_area) = 0;
        virtual vma_type_e          vm_area_type    (proc_t proc, vm_area_t vm_area) = 0;
        virtual opt<std::string>    vm_area_name    (proc_t proc, vm_area_t vm_area) = 0;

        virtual bool                driver_list (drivers::on_driver_fn on_driver) = 0;
        virtual opt<std::string>    driver_name (driver_t drv) = 0;
        virtual opt<span_t>         driver_span (driver_t drv) = 0;

        virtual opt<bpid_t> listen_proc_create  (const process::on_event_fn& on_create) = 0;
        virtual opt<bpid_t> listen_proc_delete  (const process::on_event_fn& on_delete) = 0;
        virtual opt<bpid_t> listen_thread_create(const threads::on_event_fn& on_create) = 0;
        virtual opt<bpid_t> listen_thread_delete(const threads::on_event_fn& on_delete) = 0;
        virtual opt<bpid_t> listen_mod_create   (proc_t proc, flags_t flags, const modules::on_event_fn& on_load) = 0;
        virtual opt<bpid_t> listen_drv_create   (const drivers::on_event_fn& on_load) = 0;

        virtual opt<arg_t>  read_stack  (size_t index) = 0;
        virtual opt<arg_t>  read_arg    (size_t index) = 0;
        virtual bool        write_arg   (size_t index, arg_t arg) = 0;

        virtual void debug_print() = 0;
    };

    std::shared_ptr<nt::Os> make_nt     (core::Core& core);
    void                    attach      (core::Core&, nt::Os&);
    std::unique_ptr<Module> make_linux  (core::Core& core);
    std::unique_ptr<Module> make_none   ();
} // namespace os

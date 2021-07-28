#define FDP_MODULE "tests_win10"
#include <icebox/core.hpp>
#include <icebox/log.hpp>
#include <icebox/tracer/syscalls.gen.hpp>
#include <icebox/tracer/syscalls32.gen.hpp>
#include <icebox/tracer/tracer.hpp>
#include <icebox/utils/path.hpp>

#include <fmt/format.h>

#define GTEST_DONT_DEFINE_FAIL 1
#include <gtest/gtest.h>

#include <map>
#include <thread>
#include <unordered_set>

namespace
{
    struct win10
        : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            ptr_core = core::attach("win10");
            ASSERT_TRUE(ptr_core);
            const auto paused = state::pause(*ptr_core);
            ASSERT_TRUE(paused);
        }

        void TearDown() override
        {
            const auto resumed = state::resume(*ptr_core);
            EXPECT_TRUE(resumed);
        }

        std::shared_ptr<core::Core> ptr_core;
    };
}

TEST(win10_, attach_detach)
{
    for(size_t i = 0; i < 16; ++i)
    {
        const auto core = core::attach("win10");
        EXPECT_TRUE(!!core);
        state::resume(*core);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

TEST_F(win10, attach)
{
}

TEST_F(win10, drivers)
{
    using Driver  = std::tuple<uint64_t, uint64_t, size_t>;
    using Drivers = std::map<std::string, Driver>;

    Drivers drivers;
    auto& core = *ptr_core;
    drivers::list(core, [&](driver_t drv)
    {
        const auto name = drivers::name(core, drv);
        EXPECT_TRUE(!!name);
        const auto span = drivers::span(core, drv);
        EXPECT_TRUE(!!span);
        drivers.emplace(*name, Driver{drv.id, span->addr, span->size});
        return walk_e::next;
    });
    EXPECT_NE(drivers.size(), 0U);
    const auto it = drivers.find(R"(\SystemRoot\system32\ntoskrnl.exe)");
    EXPECT_NE(it, drivers.end());

    const auto [id, addr, size] = it->second;
    EXPECT_NE(id, 0U);
    EXPECT_NE(addr, 0U);
    EXPECT_GT(size, 0U);

    const auto want = addr + (size >> 1);
    const auto drv  = drivers::find(core, want);
    EXPECT_TRUE(!!drv);
    EXPECT_EQ(id, drv->id);

    const auto kernel = drivers::find_name(core, "ntoskrnl.exe");
    EXPECT_TRUE(!!kernel);
    const auto kernel_name = drivers::name(core, *kernel);
    EXPECT_TRUE(!!kernel_name);
    EXPECT_STRCASEEQ(path::filename(*kernel_name).generic_string().data(), "ntoskrnl.exe");

    LOG(INFO, "kernel %" PRIx64, kernel->id);
    const auto hal = drivers::find_name(core, "HaL.dll");
    EXPECT_TRUE(!!hal);
    const auto hal_name = drivers::name(core, *hal);
    EXPECT_TRUE(!!hal_name);
    EXPECT_STRCASEEQ(path::filename(*hal_name).generic_string().data(), "hal.dll");
    LOG(INFO, "hal %" PRIx64, hal->id);
}

TEST_F(win10, processes)
{
    using Process   = std::tuple<uint64_t, uint64_t, uint64_t, flags_t>;
    using Processes = std::multimap<std::string, Process>;

    Processes processes;
    auto& core = *ptr_core;
    process::list(core, [&](proc_t proc)
    {
        EXPECT_NE(proc.kdtb.val, 0U);
        EXPECT_NE(proc.kdtb.val, 1U);
        EXPECT_NE(proc.udtb.val, 0U);
        EXPECT_NE(proc.udtb.val, 1U);
        const auto name = process::name(core, proc);
        EXPECT_TRUE(!!name);
        const auto pid = process::pid(core, proc);
        EXPECT_NE(pid, 0U);
        const auto flags = process::flags(core, proc);
        processes.emplace(*name, Process{proc.id, proc.udtb.val, pid, flags});
        return walk_e::next;
    });
    EXPECT_NE(processes.size(), 0U);
    const auto it = processes.find("services.exe");
    EXPECT_NE(it, processes.end());

    const auto [id, dtb, pid, flags] = it->second;
    EXPECT_NE(id, 0U);
    EXPECT_NE(dtb, 0U);
    EXPECT_NE(pid, 0U);
    UNUSED(flags);

    const auto proc = process::find_pid(core, pid);
    EXPECT_TRUE(!!proc);
    EXPECT_EQ(id, proc->id);
    EXPECT_EQ(dtb, proc->udtb.val);

    const auto valid = process::is_valid(core, *proc);
    EXPECT_TRUE(valid);

    // check parent
    const auto parent = process::parent(core, *proc);
    EXPECT_TRUE(!!parent);
    const auto parent_name = process::name(core, *parent);
    EXPECT_TRUE(!!parent_name);
    EXPECT_EQ(*parent_name, "wininit.exe");

    const auto exproc = process::find_name(core, "explorer.exe", {});
    EXPECT_TRUE(!!exproc);
}

namespace
{
    template <typename T>
    auto wait_for_predicate(core::Core& core, const T& predicate)
    {
        while(true)
        {
            const auto ret = predicate();
            if(ret)
                return ret;

            const auto ok = state::run_to_cr_write(core, reg_e::cr3);
            if(!ok)
                LOG(ERROR, "unable to run to cr3 write");
        }
    }

    void wait_for_process(core::Core& core, proc_t proc)
    {
        wait_for_predicate(core, [&]
        {
            const auto opt_proc = process::current(core);
            return opt_proc && opt_proc->id == proc.id;
        });
    }
}

TEST_F(win10, threads)
{
    using Threads = std::set<uint64_t>;

    auto& core          = *ptr_core;
    const auto explorer = process::find_name(core, "explorer.exe", {});
    EXPECT_TRUE(!!explorer);

    Threads threads;
    threads::list(core, *explorer, [&](thread_t thread)
    {
        const auto proc = threads::process(core, thread);
        EXPECT_TRUE(!!proc);
        EXPECT_EQ(proc->id, explorer->id);
        const auto tid = threads::tid(core, *proc, thread);
        EXPECT_NE(tid, 0U);
        threads.emplace(tid);
        return walk_e::next;
    });
    EXPECT_NE(threads.size(), 0U);

    wait_for_process(core, *explorer);
    const auto current = threads::current(core);
    EXPECT_TRUE(!!current);

    const auto tid = threads::tid(core, *explorer, *current);
    const auto it  = threads.find(tid);
    EXPECT_NE(it, threads.end());
}

TEST_F(win10, modules)
{
    using Module  = std::tuple<uint64_t, uint64_t, size_t, flags_t>;
    using Modules = std::multimap<std::string, Module>;

    auto& core      = *ptr_core;
    const auto proc = process::find_name(core, "explorer.exe", {});
    EXPECT_TRUE(!!proc);

    Modules modules;
    modules::list(core, *proc, [&](mod_t mod)
    {
        const auto name = modules::name(core, *proc, mod);
        if(!name)
            return walk_e::next; // FIXME

        const auto span = modules::span(core, *proc, mod);
        EXPECT_TRUE(!!span);
        modules.emplace(*name, Module{mod.id, span->addr, span->size, mod.flags});
        return walk_e::next;
    });
    EXPECT_NE(modules.size(), 0U);

    const auto it = modules.find(R"(C:\Windows\SYSTEM32\ntdll.dll)");
    EXPECT_NE(it, modules.end());

    const auto [id, addr, size, flags] = it->second;
    EXPECT_NE(id, 0U);
    EXPECT_NE(addr, 0U);
    EXPECT_GT(size, 0U);
    UNUSED(flags);

    const auto want = addr + (size >> 1);
    const auto mod  = modules::find(core, *proc, want);
    EXPECT_TRUE(!!mod);
    EXPECT_EQ(id, mod->id);
}

namespace
{
    template <typename T>
    void run_until(core::Core& core, T predicate)
    {
        const auto now = std::chrono::high_resolution_clock::now();
        const auto end = now + std::chrono::minutes(8);
        while(!predicate() && std::chrono::high_resolution_clock::now() < end)
            state::exec(core);

        EXPECT_TRUE(predicate());
    }
}

TEST_F(win10, unable_to_single_step_query_information_process)
{
    auto& core      = *ptr_core;
    const auto proc = process::wait(core, "Taskmgr.exe", flags::x86);
    EXPECT_TRUE(!!proc);

    const auto ok = symbols::load_module(core, *proc, "wntdll");
    EXPECT_TRUE(!!ok);

    wow64::syscalls32 tracer{core, "wntdll"};
    auto count = size_t{0};
    // ZwQueryInformationProcess in 32-bit has code reading itself
    // we need to ensure we can break this function & resume properly
    // FDP had a bug where this was not possible
    tracer.register_ZwQueryInformationProcess(*proc, [&](wow64::HANDLE /*ProcessHandle*/,
                                                         wow64::PROCESSINFOCLASS /*ProcessInformationClass*/,
                                                         wow64::PVOID /*ProcessInformation*/,
                                                         wow64::ULONG /*ProcessInformationLength*/,
                                                         wow64::PULONG /*ReturnLength*/)
    {
        ++count;
    });
    run_until(core, [&] { return count > 1; });
}

TEST_F(win10, unset_bp_when_two_bps_share_phy_page)
{
    auto& core      = *ptr_core;
    const auto proc = process::wait(core, "Taskmgr.exe", flags::x86);
    EXPECT_TRUE(!!proc);

    const auto ok = symbols::load_module(core, *proc, "wntdll");
    EXPECT_TRUE(!!ok);

    // break on a single function once
    wow64::syscalls32 tracer{core, "wntdll"};
    int func_start = 0;
    tracer.register_ZwWaitForSingleObject(*proc, [&](wow64::HANDLE /*Handle*/,
                                                     wow64::BOOLEAN /*Alertable*/,
                                                     wow64::PLARGE_INTEGER /*Timeout*/)
    {
        ++func_start;
    });
    run_until(core, [&] { return func_start > 0; });

    // set a breakpoint on next instruction
    state::single_step(core);
    const auto addr_a = registers::read(core, reg_e::rip);
    int func_a        = 0;
    auto bp_a         = state::break_on_process(core, "ZwWaitForSingleObject + $1", *proc, addr_a, [&]
    {
        func_a++;
    });

    // set a breakpoint on next instruction again
    // we are sure the previous bp share a physical page with at least one bp
    state::single_step(core);
    const auto addr_b = registers::read(core, reg_e::rip);
    int func_b        = 0;
    const auto bp_b   = state::break_on_process(core, "ZwWaitForSingleObject + $2", *proc, addr_b, [&]
    {
        func_b++;
    });

    // wait to break on third breakpoint
    run_until(core, [&] { return func_b > 0; });

    // remove mid breakpoint
    bp_a.reset();

    // ensure vm is not frozen
    run_until(core, [&] { return func_start > 4; });
}

namespace
{
    void dump_buffer(const std::vector<uint8_t>& data1, const std::vector<uint8_t>& data2)
    {
        for(size_t i = 0; i < data1.size(); i++)
        {
            if(data1[i] != data2[i])
            {
                LOG(INFO, "data differ at offset 0x%zx", i);
                break;
            }
        }
    }

    bool test_memory(core::Core& core, proc_t proc)
    {
        os::debug_print(core);
        auto buffer   = std::vector<uint8_t>{};
        auto got_read = std::vector<uint8_t>{};
        const auto io = memory::make_io(core, proc);
        auto ret      = bool{};
        modules::list(core, proc, [&](mod_t mod)
        {
            ret             = false;
            const auto span = modules::span(core, proc, mod);
            if(!span)
                return FAIL(walk_e::stop, "modules::span failed");

            buffer.resize(span->size);
            auto ok = io.read_all(&buffer[0], span->addr, span->size);
            if(!ok)
                return FAIL(walk_e::stop, "io.read_all");

            if(buffer.size() < 2 || buffer[0] != 'M' || buffer[1] != 'Z')
                return FAIL(walk_e::stop, "invalid buffer");

            got_read.resize(span->size);
            const auto phy = memory::virtual_to_physical(core, proc, span->addr);
            if(!phy)
                return FAIL(walk_e::stop, "memory::virtual_to_physical");

            const auto phyb = memory::virtual_to_physical_with_dtb(core, proc.udtb, span->addr);
            if(!phyb)
                return FAIL(walk_e::stop, "memory::virtual_to_physical_with_dtb");

            if(phy->val != phyb->val)
                return FAIL(walk_e::stop, "phy->val != phyb->val");

            ok = memory::write_virtual(core, proc, span->addr, &buffer[0], span->size);
            if(!ok)
                return FAIL(walk_e::stop, "memory::write_virtual");

            ok = memory::read_virtual(core, proc, &got_read[0], span->addr, span->size);
            if(!ok)
                return FAIL(walk_e::stop, "memory::read_virtual");

            if(!!memcmp(&buffer[0], &got_read[0], span->size))
            {
                dump_buffer(buffer, got_read);
                return FAIL(walk_e::stop, "buffer and got_read are different!");
            }

            ret = true;
            return walk_e::next;
        });
        os::debug_print(core);
        return ret;
    }
}

TEST_F(win10, memory)
{
    auto& core = *ptr_core;
    auto proc  = process::find_name(core, "explorer.exe", {});
    EXPECT_TRUE(!!proc);
    auto ok = test_memory(core, *proc);
    EXPECT_TRUE(!!ok);
}

TEST_F(win10, memory_kernel_passive)
{
    auto& core          = *ptr_core;
    const auto explorer = process::find_name(core, "explorer.exe", {});
    while(true)
    {
        state::run_to_cr_write(core, reg_e::cr8);
        const auto cr8 = registers::read(core, reg_e::cr8);
        if(cr8 != 0)
            continue;

        const auto proc = process::current(core);
        EXPECT_TRUE(!!proc);
        if(proc->id == explorer->id)
            break;
    };
    auto ok = test_memory(core, *explorer);
    EXPECT_TRUE(!!ok);
}

TEST_F(win10, memory_kernel_apc)
{
    auto& core          = *ptr_core;
    const auto explorer = process::find_name(core, "explorer.exe", {});
    while(true)
    {
        state::run_to_cr_write(core, reg_e::cr8);
        const auto cr8 = registers::read(core, reg_e::cr8);
        if(cr8 != 1)
            continue;

        const auto proc = process::current(core);
        EXPECT_TRUE(!!proc);
        if(proc->id == explorer->id)
            break;
    };
    auto ok = test_memory(core, *explorer);
    EXPECT_TRUE(!!ok);
}

TEST_F(win10, vm_area)
{
    auto& core      = *ptr_core;
    const auto proc = process::find_name(core, "explorer.exe", {});
    EXPECT_TRUE(!!proc);
    LOG(INFO, "explorer udtb: 0x%" PRIx64, proc->udtb.val);

    LOG(INFO, "MMVAD               address             size                access      image");
    vm_area::list(core, *proc, [&](vm_area_t area)
    {
        auto span = vm_area::span(core, *proc, area);
        EXPECT_TRUE(!!span);

        const auto access = vm_area::access(core, *proc, area);
        const auto name   = vm_area::name(core, *proc, area);
        EXPECT_TRUE(!!name);
        if(false)
            LOG(INFO, "0x%-16" PRIx64 "  0x%-16" PRIx64 "  0x%-16" PRIx64 "  0x%-8x  %s", area.id, span->addr, span->size, access, name->c_str());
        return walk_e::next;
    });

    const auto invalid_area = vm_area::find(core, *proc, 0x10);
    EXPECT_FALSE(!!invalid_area);

    modules::list(core, *proc, [&](mod_t mod)
    {
        const auto mod_span = modules::span(core, *proc, mod);
        EXPECT_TRUE(!!mod_span);
        const auto mod_name = modules::name(core, *proc, mod);
        EXPECT_TRUE(!!mod_name);

        const auto area = vm_area::find(core, *proc, mod_span->addr + 1);
        EXPECT_TRUE(!!area);

        auto area_span = vm_area::span(core, *proc, *area);
        EXPECT_TRUE(!!area_span);

        EXPECT_TRUE(area_span->addr == mod_span->addr);
        EXPECT_TRUE(area_span->size == mod_span->size);

        const auto area_name = vm_area::name(core, *proc, *area);
        EXPECT_TRUE(!!area_name);

        const auto access = vm_area::access(core, *proc, *area);
        (void) access;
        // EXPECT_TRUE(access & VMA_ACCESS_COPY_ON_WRITE);

        const auto pos = area_name->find(*mod_name);
        EXPECT_TRUE(!!pos);

        return walk_e::next;
    });
}

TEST_F(win10, loader)
{
    auto& core      = *ptr_core;
    const auto proc = process::wait(core, "dwm.exe", flags::x64);
    ASSERT_TRUE(!!proc);

    symbols::load_drivers(core);
    symbols::autoload_modules(core, *proc);
    const auto ok = symbols::load_module(core, *proc, "ntdll");
    EXPECT_TRUE(!!ok);
}

TEST_F(win10, out_of_context_loads)
{
    auto& core      = *ptr_core;
    const auto proc = process::find_name(core, "dwm.exe", flags::x64);
    ASSERT_TRUE(!!proc);

    const auto other = process::find_name(core, "explorer.exe", flags::x64);
    ASSERT_TRUE(!!other);

    // test memory from another process context
    symbols::load_modules(core, *proc);
    ASSERT_TRUE(process::is_valid(core, *other));
}

TEST_F(win10, tracer)
{
    auto& core      = *ptr_core;
    const auto proc = process::wait(core, "dwm.exe", {});
    ASSERT_TRUE(!!proc);

    const auto ok = symbols::load_module(core, *proc, "ntdll");
    EXPECT_TRUE(!!ok);

    using Calls = std::unordered_set<std::string>;
    auto calls  = Calls{};
    auto tracer = nt::syscalls{core, "ntdll"};
    auto count  = 0;
    tracer.register_all(*proc, [&](const auto& cfg)
    {
        calls.insert(cfg.name);
        ++count;
    });
    run_until(core, [&] { return count > 32; });
    for(const auto& call : calls)
        LOG(INFO, "call: %s", call.data());
}

namespace
{
    std::string dump_address(core::Core& core, proc_t proc, uint64_t addr)
    {
        return symbols::string(core, proc, addr);
    }

    bool starts_with(std::string_view token, std::string_view prefix)
    {
        if(token.size() < prefix.size())
            return false;

        return !token.compare(0, prefix.size(), prefix.data());
    }
}

TEST_F(win10, callstacks)
{
    auto& core      = *ptr_core;
    const auto proc = process::wait(core, "dwm.exe", {});
    ASSERT_TRUE(!!proc);

    const auto ok = symbols::load_module(core, *proc, "ntdll");
    EXPECT_TRUE(!!ok);

    drivers::list(core, [&](driver_t drv)
    {
        callstacks::load_driver(core, *proc, drv);
        return walk_e::next;
    });

    symbols::autoload_modules(core, *proc);
    callstacks::autoload_modules(core, *proc);

    // check regular callstacks
    auto tracer = nt::syscalls{core, "ntdll"};
    auto count  = size_t{0};
    auto bpid   = tracer.register_all(*proc, [&](const auto& /* cfg*/)
    {
        LOG(INFO, " ");
        auto callers = std::vector<callstacks::caller_t>(128);
        const auto n = callstacks::read(core, &callers[0], callers.size(), *proc);
        EXPECT_GT(n, 1U);
        for(size_t i = 0; i < n; ++i)
            LOG(INFO, "0x%02" PRIx64 ": %s", i, dump_address(core, *proc, callers[i].addr).data());
        // check last address is thread entry point
        const auto last = dump_address(core, *proc, callers[n - 1].addr);
        EXPECT_TRUE(starts_with(last, "ntdll!RtlUserThreadStart+")) << last;
        count++;
    });
    EXPECT_TRUE(!!bpid);
    run_until(core, [&] { return count > 32; });
    EXPECT_EQ(count, 33U);
    state::drop_breakpoint(core, *bpid);

    // check we can read callstacks with one address only
    const auto opt_addr = symbols::address(core, *proc, "ntdll", "RtlUserThreadStart");
    EXPECT_TRUE(!!opt_addr);

    const auto opt_phy = memory::virtual_to_physical(core, *proc, *opt_addr);
    EXPECT_TRUE(!!opt_phy);

    count         = 0;
    const auto bp = state::break_on_physical(core, "ntdll!RtlUserThreadStart", *opt_phy, [&]
    {
        const auto curr = process::current(core);
        LOG(INFO, " ");
        auto callers = std::vector<callstacks::caller_t>(128);
        const auto n = callstacks::read(core, &callers[0], callers.size(), *curr);
        EXPECT_EQ(n, 1U);
        const auto last = dump_address(core, *curr, callers[0].addr);
        EXPECT_EQ(last, "ntdll!RtlUserThreadStart");
        ++count;
    });
    run_until(core, [&] { return count > 0; });
    EXPECT_EQ(count, 1U);
}

TEST_F(win10, listen_module_wow64)
{
    auto& core      = *ptr_core;
    const auto proc = process::wait(core, "Taskmgr.exe", flags::x86);
    EXPECT_TRUE(!!proc);

    modules::listen_create(core, *proc, flags::x64, [&](mod_t mod)
    {
        const auto name = modules::name(core, *proc, mod);
        if(!name)
            return;

        LOG(INFO, "+ x64 module %s", name->data());
    });

    modules::listen_create(core, *proc, flags::x86, [&](mod_t mod)
    {
        const auto name = modules::name(core, *proc, mod);
        if(!name)
            return;

        LOG(INFO, "+ x86 module %s", name->data());
    });

    const auto nt64 = symbols::load_module(core, *proc, "ntdll");
    EXPECT_TRUE(!!nt64);

    const auto wow64cpu = symbols::load_module(core, *proc, "wow64cpu");
    EXPECT_TRUE(!!wow64cpu);

    const auto ntwow64 = symbols::load_module(core, *proc, "wntdll");
    EXPECT_TRUE(!!ntwow64);

    const auto kbase = symbols::load_module(core, *proc, "wkernelbase");
    EXPECT_TRUE(!!kbase);
}

TEST_F(win10, symbols)
{
    auto& core          = *ptr_core;
    const auto opt_proc = process::find_pid(core, 4);
    EXPECT_TRUE(!!opt_proc);

    const auto opt_addr = symbols::address(core, *opt_proc, "nt", "PspExitProcess");
    EXPECT_TRUE(!!opt_addr);

    const auto strsym = symbols::string(core, *opt_proc, *opt_addr);
    EXPECT_EQ(strsym, "nt!PspExitProcess");

    auto strucs = std::vector<std::string>{};
    symbols::list_strucs(core, *opt_proc, "nt", [&](std::string_view struc)
    {
        strucs.emplace_back(struc);
    });
    const auto it_struc = std::find(strucs.begin(), strucs.end(), "_KPROCESS");
    EXPECT_NE(it_struc, strucs.end());

    const auto opt_struc = symbols::read_struc(core, *opt_proc, "nt", "_KPROCESS");
    EXPECT_TRUE(!!opt_struc);

    auto members = std::vector<std::string>{};
    for(const auto& m : opt_struc->members)
        members.emplace_back(m.name);
    const auto it_member = std::find(members.begin(), members.end(), "DirectoryTableBase");
    EXPECT_NE(it_member, members.end());
}

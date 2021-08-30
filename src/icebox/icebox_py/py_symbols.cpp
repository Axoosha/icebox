#include "bindings.hpp"

PyObject* py::symbols::address(core::Core& core, PyObject* args)
{
    auto*       py_proc = static_cast<PyObject*>(nullptr);
    const auto* module  = static_cast<const char*>(nullptr);
    const auto* symbol  = static_cast<const char*>(nullptr);
    auto        ok      = PyArg_ParseTuple(args, "Sss", &py_proc, &module, &symbol);
    if(!ok)
        return nullptr;

    const auto opt_proc = py::from_bytes<proc_t>(py_proc);
    if(!opt_proc)
        return nullptr;

    module             = module ? module : "";
    symbol             = symbol ? symbol : "";
    const auto opt_ptr = ::symbols::address(core, *opt_proc, module, symbol);
    if(!opt_ptr)
        Py_RETURN_NONE;

    return PyLong_FromUnsignedLongLong(*opt_ptr);
}

PyObject* py::symbols::list_strucs(core::Core& core, PyObject* args)
{
    auto*       py_proc = static_cast<PyObject*>(nullptr);
    const auto* module  = static_cast<const char*>(nullptr);
    auto        ok      = PyArg_ParseTuple(args, "Ss", &py_proc, &module);
    if(!ok)
        return nullptr;

    const auto opt_proc = py::from_bytes<proc_t>(py_proc);
    if(!opt_proc)
        return nullptr;

    module        = module ? module : "";
    auto* py_list = PyList_New(0);
    if(!py_list)
        return nullptr;

    PY_DEFER_DECREF(py_list);
    ::symbols::list_strucs(core, *opt_proc, module, [&](std::string_view name)
    {
        auto* py_name = py_to_string(name.data(), name.size());
        if(!py_name)
            return;

        PY_DEFER_DECREF(py_name);
        PyList_Append(py_list, py_name);
    });
    Py_INCREF(py_list);
    return py_list;
}

PyObject* py::symbols::read_struc(core::Core& core, PyObject* args)
{
    auto*       py_proc = static_cast<PyObject*>(nullptr);
    const auto* module  = static_cast<const char*>(nullptr);
    const auto* struc   = static_cast<const char*>(nullptr);
    auto        ok      = PyArg_ParseTuple(args, "Sss", &py_proc, &module, &struc);
    if(!ok)
        return nullptr;

    const auto opt_proc = py::from_bytes<proc_t>(py_proc);
    if(!opt_proc)
        return nullptr;

    module               = module ? module : "";
    struc                = struc ? struc : "";
    const auto opt_struc = ::symbols::read_struc(core, *opt_proc, module, struc);
    if(!opt_struc)
        return py::fail_with(nullptr, PyExc_RuntimeError, "unable to read struc size");

    auto* py_list = PyList_New(0);
    if(!py_list)
        return nullptr;

    for(const auto& m : opt_struc->members)
    {
        auto* py_item = Py_BuildValue("{s:s,s:K,s:K}",
                                      "name", m.name.data(),
                                      "offset", (uint64_t) m.offset,
                                      "bits", (uint64_t) m.bits);
        PY_DEFER_DECREF(py_item);
        PyList_Append(py_list, py_item);
    }

    return Py_BuildValue("{s:s,s:K,s:O}",
                         "name", opt_struc->name.data(),
                         "bytes", opt_struc->bytes,
                         "members", py_list);
}

PyObject* py::symbols::string(core::Core& core, PyObject* args)
{
    auto* py_proc = static_cast<PyObject*>(nullptr);
    auto  ptr     = uint64_t{};
    auto  ok      = PyArg_ParseTuple(args, "SK", &py_proc, &ptr);
    if(!ok)
        return nullptr;

    const auto opt_proc = py::from_bytes<proc_t>(py_proc);
    if(!opt_proc)
        return nullptr;

    const auto sym = ::symbols::string(core, *opt_proc, ptr);
    return py_to_string(sym.data(), sym.size());
}

PyObject* py::symbols::load_module_memory(core::Core& core, PyObject* args)
{
    auto* py_proc = static_cast<PyObject*>(nullptr);
    auto  addr    = uint64_t{};
    auto  size    = uint64_t{};
    auto  ok      = PyArg_ParseTuple(args, "SKK", &py_proc, &addr, &size);
    if(!ok)
        return nullptr;

    const auto opt_proc = py::from_bytes<proc_t>(py_proc);
    if(!opt_proc)
        return nullptr;

    const auto io = ::memory::make_io(core, *opt_proc);
    ok            = ::symbols::load_module_memory(core, *opt_proc, io, {addr, size});
    if(!ok)
        return py::fail_with(nullptr, PyExc_RuntimeError, "unable to load module memory");

    Py_RETURN_NONE;
}

PyObject* py::symbols::load_module(core::Core& core, PyObject* args)
{
    auto*       py_proc = static_cast<PyObject*>(nullptr);
    const auto* name    = static_cast<const char*>(nullptr);
    auto        ok      = PyArg_ParseTuple(args, "Ss", &py_proc, &name);
    if(!ok)
        return nullptr;

    const auto opt_proc = py::from_bytes<proc_t>(py_proc);
    if(!opt_proc)
        return nullptr;

    name = name ? name : "";
    ok   = ::symbols::load_module(core, *opt_proc, name);
    if(!ok)
        return py::fail_with(nullptr, PyExc_RuntimeError, "unable to load module");

    Py_RETURN_NONE;
}

PyObject* py::symbols::load_modules(core::Core& core, PyObject* args)
{
    auto* py_proc = static_cast<PyObject*>(nullptr);
    auto  ok      = PyArg_ParseTuple(args, "S", &py_proc);
    if(!ok)
        return nullptr;

    const auto opt_proc = py::from_bytes<proc_t>(py_proc);
    if(!opt_proc)
        return nullptr;

    ok = ::symbols::load_modules(core, *opt_proc);
    if(!ok)
        return py::fail_with(nullptr, PyExc_RuntimeError, "unable to load modules");

    Py_RETURN_NONE;
}

PyObject* py::symbols::load_driver_memory(core::Core& core, PyObject* args)
{
    auto addr = uint64_t{};
    auto size = uint64_t{};
    auto ok   = PyArg_ParseTuple(args, "KK", &addr, &size);
    if(!ok)
        return nullptr;

    ok = ::symbols::load_driver_memory(core, {addr, size});
    if(!ok)
        return py::fail_with(nullptr, PyExc_RuntimeError, "unable to load driver memory");

    Py_RETURN_NONE;
}

PyObject* py::symbols::load_driver(core::Core& core, PyObject* args)
{
    const auto* name = static_cast<const char*>(nullptr);
    auto        ok   = PyArg_ParseTuple(args, "s", &name);
    if(!ok)
        return nullptr;

    name = name ? name : "";
    ok   = ::symbols::load_driver(core, name);
    if(!ok)
        return py::fail_with(nullptr, PyExc_RuntimeError, "unable to load driver");

    Py_RETURN_NONE;
}

PyObject* py::symbols::load_drivers(core::Core& core, PyObject* /*args*/)
{
    const auto ok = ::symbols::load_drivers(core);
    if(!ok)
        return py::fail_with(nullptr, PyExc_RuntimeError, "unable to load drivers");

    Py_RETURN_NONE;
}

PyObject* py::symbols::autoload_modules(core::Core& core, PyObject* args)
{
    auto* py_proc = static_cast<PyObject*>(nullptr);
    auto  ok      = PyArg_ParseTuple(args, "S", &py_proc);
    if(!ok)
        return nullptr;

    const auto opt_proc = py::from_bytes<proc_t>(py_proc);
    if(!opt_proc)
        return nullptr;

    const auto opt_bp = ::symbols::autoload_modules(core, *opt_proc);
    if(!opt_bp)
        return py::fail_with(nullptr, PyExc_RuntimeError, "unable to load modules");

    return py::to_bytes(*opt_bp);
}

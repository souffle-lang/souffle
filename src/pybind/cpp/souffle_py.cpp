// FIXME:  According to C++, all names with double underscores
//         are reserved.  Strictly speaking, this is UB.
#define __EMBEDDED_SOUFFLE__
#define SOUFFLE_PYBIND_MODULE
#include "souffle/SouffleInterface.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace py = pybind11;

namespace souffle::pybind {

std::vector<std::function<Own<ProgramFactory>()>> registrar;
VecOwn<ProgramFactory> factories;

auto mkSouffleProgram(std::string const& name) {
    // Register and drop any new factories
    for (auto beg = registrar.begin(); beg != registrar.end();) {
        auto reg = std::move(*beg);
        beg = registrar.erase(beg);
        // register the factory after removing it from the list
        factories.push_back(reg());
    }

    auto res = Own<SouffleProgram>{ProgramFactory::newInstance(name.c_str())};
    if (!res) {
        throw std::runtime_error(
                "Unable to locate program '" + name + "'. Try loading it using Program.load_program");
    }

    return res;
}

template <typename RamType>
py::object addTupleElem(tuple& tup) {
    RamType val;
    tup >> val;
    return py::cast(val);
}

// FIXME: Until we have type_identity in C++20
template <typename T>
using Type = std::remove_const<T>;

template <typename F>
void forEachType(Relation const& relation, F&& fun) {
    for (std::size_t ii = 0; ii < relation.getArity(); ++ii) {
        auto shortType = *relation.getAttrType(ii);
        switch (shortType) {
            case 'u': fun(ii, shortType, Type<RamUnsigned>()); break;
            case 'i': fun(ii, shortType, Type<RamSigned>()); break;
            case 's': fun(ii, shortType, Type<std::string>()); break;
            case 'f': fun(ii, shortType, Type<RamFloat>()); break;
            default: assert(false && "Invalid type found in relation");
        }
    }
}

py::tuple toPyTuple(tuple& tup, Relation const& relation) {
    auto const arity = relation.getArity();

    std::vector<py::object> elems;
    elems.reserve(arity);

    forEachType(relation, [&elems, &tup](std::size_t, char, auto tid) {
        elems.emplace_back(addTupleElem<typename decltype(tid)::type>(tup));
    });

    return py::tuple(py::cast(elems));
}

class pytuple_iterator {
public:
    pytuple_iterator(Relation::iterator base, Relation const& rel) : iter(std::move(base)), relation(rel) {}

    pytuple_iterator& operator++() {
        ++iter;
        return *this;
    }

    py::tuple operator*() const {
        return toPyTuple(*iter, relation);
    }

    bool operator==(pytuple_iterator const& other) {
        return iter == other.iter;
    }

    bool operator!=(pytuple_iterator const& other) {
        return iter != other.iter;
    }

private:
    Relation::iterator iter;
    Relation const& relation;
};

auto mkTupleIterator(Relation const* relation) {
    auto begin = relation->begin();
    return py::make_iterator(
            pytuple_iterator{begin, *relation}, pytuple_iterator{relation->end(), *relation});
}

py::tuple getAttrs(Relation const& relation, char const* (Relation::*fun)(std::size_t) const) {
    auto const arity = relation.getArity();
    std::vector<std::string> vals;
    vals.reserve(arity);

    for (Relation::arity_type ii = 0; ii < arity; ++ii) {
        vals.push_back((relation.*fun)(ii));
    }

    return py::tuple(py::cast(vals));
}

tuple pyTupleToTuple(Relation const& relation, py::tuple const& tup) {
    if (tup.size() != relation.getArity()) {
        std::ostringstream out;
        out << "Attempted to convert tuple of arity " << tup.size() << " but relation " << relation.getName()
            << " has arity " << relation.getArity();
        throw std::runtime_error(out.str());
    }

    tuple res(&relation);

    forEachType(relation, [&tup, &res](std::size_t ii, char, auto tid) {
        try {
            res << tup[ii].cast<typename decltype(tid)::type>();
        } catch (py::cast_error const& err) {
            std::ostringstream out;
            out << "Type conversion for tuple element " << ii << " failed: " << err.what();
            throw std::runtime_error(out.str());
        }
    });

    return res;
}

bool contains(Relation const* relation, py::tuple const& tup) {
    return relation->contains(pyTupleToTuple(*relation, tup));
}

void insert(Relation* relation, py::list const& elems) {
    std::size_t ii = 0;
    for (auto&& elem : elems) {
        try {
            relation->insert(pyTupleToTuple(*relation, elem.cast<py::tuple>()));
        } catch (std::exception const& ex) {
            std::ostringstream out;
            out << "Insertion of element " << ii << " failed: " << ex.what();
            throw std::runtime_error(out.str());
        }

        ++ii;
    }
}
}  // namespace souffle::pybind

PYBIND11_MODULE(libsouffle_py, m) {
    py::class_<souffle::Relation>(m, "Relation")
            .def("insert", &souffle::pybind::insert)
            .def("contains", &souffle::pybind::contains)
            .def("__iter__", &souffle::pybind::mkTupleIterator, py::keep_alive<0, 1>())
            .def("__len__", &souffle::Relation::size)
            .def("get_name", &souffle::Relation::getName)
            .def("get_attr_types",
                    [](souffle::Relation const* rel) {
                        return souffle::pybind::getAttrs(*rel, &souffle::Relation::getAttrType);
                    })
            .def("get_attr_names",
                    [](souffle::Relation const* rel) {
                        return souffle::pybind::getAttrs(*rel, &souffle::Relation::getAttrName);
                    })
            .def("get_arity", &souffle::Relation::getArity)
            // TODO: getAuxiliaryArity - note sure yet what these are for, need to ask
            // TODO: getPrimaryArity - note sure yet what these are for, need to ask
            // TODO: getSymbolTable.  Can we automatically create UDTs in Python?
            .def("get_signature", &souffle::Relation::getSignature)
            .def("purge", &souffle::Relation::purge);

    py::class_<souffle::SouffleProgram>(m, "Program")
            .def(py::init(&souffle::pybind::mkSouffleProgram))
            // drop the GIL for potentially long-running processe
            .def("run", &souffle::SouffleProgram::run, py::call_guard<py::gil_scoped_release>())
            .def("run_all", &souffle::SouffleProgram::runAll, py::call_guard<py::gil_scoped_release>())
            .def("load_all", &souffle::SouffleProgram::loadAll, py::call_guard<py::gil_scoped_release>())
            .def("print_all", &souffle::SouffleProgram::printAll, py::call_guard<py::gil_scoped_release>())
            // Note - ignoring dumpInputs/dumpOutputs, not very pythonic
            // TODO: setNumThreads
            .def("get_num_threads", &souffle::SouffleProgram::getNumThreads)
            .def("get_output_relations", &souffle::SouffleProgram::getOutputRelations,
                    py::return_value_policy::reference_internal)
            .def("get_input_relations", &souffle::SouffleProgram::getInputRelations,
                    py::return_value_policy::reference_internal)
            .def("get_internal_relations", &souffle::SouffleProgram::getInternalRelations,
                    py::return_value_policy::reference_internal)
            .def("get_all_relations", &souffle::SouffleProgram::getAllRelations,
                    py::return_value_policy::reference_internal)
            // TODO: getRecordTable
            .def("purge_input_relations", &souffle::SouffleProgram::purgeInputRelations)
            .def("purge_output_relations", &souffle::SouffleProgram::purgeOutputRelations)
            .def("purge_internal_relations", &souffle::SouffleProgram::purgeInternalRelations);
}

/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "SynthesiserRelation.h"

#include <numeric>
#include <set>

namespace souffle {

std::unique_ptr<SynthesiserRelation> SynthesiserRelation::getSynthesiserRelation(
        const RamRelationReference& ramRel, const IndexSet& indexSet, bool isProvenance) {
    SynthesiserRelation* rel;

    // Handle the qualifier in souffle code
    if (isProvenance) {
        rel = new SynthesiserDirectRelation(ramRel, indexSet, isProvenance);
    } else if (ramRel.getArity() == 0) {
        rel = new SynthesiserNullaryRelation(ramRel, indexSet, isProvenance);
    } else if (ramRel.isBTree()) {
        rel = new SynthesiserDirectRelation(ramRel, indexSet, isProvenance);
    } else if (ramRel.isBrie()) {
        rel = new SynthesiserBrieRelation(ramRel, indexSet, isProvenance);
    } else if (ramRel.isEqRel()) {
        rel = new SynthesiserEqrelRelation(ramRel, indexSet, isProvenance);
    } else {
        // Handle the data structure command line flag
        if (ramRel.getArity() > 6) {
            rel = new SynthesiserIndirectRelation(ramRel, indexSet, isProvenance);
        } else {
            rel = new SynthesiserDirectRelation(ramRel, indexSet, isProvenance);
        }
    }

    assert(rel != nullptr && "relation type not specified");

    // generate index set
    rel->computeIndices();

    return std::unique_ptr<SynthesiserRelation>(rel);
}

// -------- Nullary Relation --------

/** Generate index set for a nullary relation, which should be empty */
void SynthesiserNullaryRelation::computeIndices() {
    computedIndices = {};
}

/** Generate type name of a nullary relation */
std::string SynthesiserNullaryRelation::getTypeName() {
    return "t_nullaries";
}

/** Generate type struct of a nullary relation, which is empty,
 * the actual implementation is in CompiledSouffle.h */
void SynthesiserNullaryRelation::generateTypeStruct(std::ostream& out) {
    return;
}

// -------- Direct Indexed B-Tree Relation --------

/** Generate index set for a direct indexed relation */
void SynthesiserDirectRelation::computeIndices() {
    // Generate and set indices
    std::vector<std::vector<int>> inds = indices.getAllOrders();

    // generate a full index if no indices exist
    if (inds.empty()) {
        std::vector<int> fullInd(getArity());
        std::iota(fullInd.begin(), fullInd.end(), 0);
        inds.push_back(fullInd);
    }

    // expand all search orders to be full
    for (auto& ind : inds) {
        if (ind.size() < getArity()) {
            // use a set as a cache for fast lookup
            std::set<int> curIndexElems(ind.begin(), ind.end());

            // If this relation is used with provenance,
            // we must expand all search orders to be full indices,
            // since weak/strong comparators and updaters need this,
            // and also add provenance annotations to the indices
            if (isProvenance) {
                // expand index to be full
                for (size_t i = 0; i < getArity() - 2; i++) {
                    if (curIndexElems.find(i) == curIndexElems.end()) {
                        ind.push_back(i);
                    }
                }

                // remove any provenance annotations already in the index order
                if (curIndexElems.find(getArity() - 1) != curIndexElems.end()) {
                    ind.erase(std::find(ind.begin(), ind.end(), getArity() - 1));
                }

                if (curIndexElems.find(getArity() - 2) != curIndexElems.end()) {
                    ind.erase(std::find(ind.begin(), ind.end(), getArity() - 2));
                }

                // add provenance annotations to the index, but in reverse order
                ind.push_back(getArity() - 1);
                ind.push_back(getArity() - 2);
            } else {
                // expand index to be full
                for (size_t i = 0; i < getArity(); i++) {
                    if (curIndexElems.find(i) == curIndexElems.end()) {
                        ind.push_back(i);
                    }
                }
            }
        }
    }

    masterIndex = 0;

    computedIndices = inds;
}

/** Generate type name of a direct indexed relation */
std::string SynthesiserDirectRelation::getTypeName() {
    std::stringstream res;
    res << "t_btree_" << getArity();

    for (auto& ind : getIndices()) {
        res << "__" << join(ind, "_");
    }

    for (auto& search : getIndexSet().getSearches()) {
        res << "__" << search;
    }

    return res.str();
}

/** Generate type struct of a direct indexed relation */
void SynthesiserDirectRelation::generateTypeStruct(std::ostream& out) {
    size_t arity = getArity();
    const auto& inds = getIndices();
    size_t numIndexes = inds.size();
    std::map<std::vector<int>, int> indexToNumMap;

    // struct definition
    out << "struct " << getTypeName() << " {\n";

    // stored tuple type
    out << "using t_tuple = Tuple<RamDomain, " << arity << ">;\n";

    // generate an updater class for provenance
    if (isProvenance) {
        out << "struct updater_" << getTypeName() << " {\n";
        out << "void update(t_tuple& old_t, const t_tuple& new_t) {\n";
        out << "old_t[" << arity - 2 << "] = new_t[" << arity - 2 << "];\n";
        out << "old_t[" << arity - 1 << "] = new_t[" << arity - 1 << "];\n";
        out << "}\n";
        out << "};\n";
    }

    // generate the btree type for each relation
    for (size_t i = 0; i < inds.size(); i++) {
        auto& ind = inds[i];

        if (i < getIndexSet().getAllOrders().size()) {
            indexToNumMap[getIndexSet().getAllOrders()[i]] = i;
        }

        // for provenance, all indices must be full so we use btree_set
        // also strong/weak comparators and updater methods
        if (isProvenance) {
            out << "using t_ind_" << i << " = btree_set<t_tuple, index_utils::comparator<" << join(ind);
            out << ">, std::allocator<t_tuple>, 256, typename "
                   "souffle::detail::default_strategy<t_tuple>::type, index_utils::comparator<";
            out << join(ind.begin(), ind.end() - 2) << ">, updater_" << getTypeName() << ">;\n";

            // without provenance, some indices may be not full, so we use btree_multiset for those
        } else {
            if (ind.size() == arity) {
                out << "using t_ind_" << i << " = btree_set<t_tuple, index_utils::comparator<" << join(ind)
                    << ">>;\n";
            } else {
                out << "using t_ind_" << i << " = btree_multiset<t_tuple, index_utils::comparator<"
                    << join(ind) << ">>;\n";
            }
        }
        out << "t_ind_" << i << " ind_" << i << ";\n";
    }

    // typedef master index iterator to be struct iterator
    out << "using iterator = t_ind_" << masterIndex << "::iterator;\n";

    // create a struct storing hints for each btree
    out << "struct context {\n";
    for (size_t i = 0; i < numIndexes; i++) {
        out << "t_ind_" << i << "::operation_hints hints_" << i << ";\n";
    }
    out << "};\n";
    out << "context createContext() { return context(); }\n";

    // insert methods
    out << "bool insert(const t_tuple& t) {\n";
    out << "context h;\n";
    out << "return insert(t, h);\n";
    out << "}\n";  // end of insert(t_tuple&)

    out << "bool insert(const t_tuple& t, context& h) {\n";
    out << "if (ind_" << masterIndex << ".insert(t, h.hints_" << masterIndex << ")) {\n";
    for (size_t i = 0; i < numIndexes; i++) {
        if (i != masterIndex) {
            out << "ind_" << i << ".insert(t, h.hints_" << i << ");\n";
        }
    }
    out << "return true;\n";
    out << "} else return false;\n";
    out << "}\n";  // end of insert(t_tuple&, context&)

    out << "bool insert(const RamDomain* ramDomain) {\n";
    out << "RamDomain data[" << arity << "];\n";
    out << "std::copy(ramDomain, ramDomain + " << arity << ", data);\n";
    out << "const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);\n";
    out << "context h;\n";
    out << "return insert(tuple, h);\n";
    out << "}\n";  // end of insert(RamDomain*)

    std::vector<std::string> decls, params;
    for (size_t i = 0; i < arity; i++) {
        decls.push_back("RamDomain a" + std::to_string(i));
        params.push_back("a" + std::to_string(i));
    }
    out << "bool insert(" << join(decls, ",") << ") {\n";
    out << "RamDomain data[" << arity << "] = {" << join(params, ",") << "};\n";
    out << "return insert(data);\n";
    out << "}\n";  // end of insert(RamDomain x1, RamDomain x2, ...)

    // insertAll methods
    out << "template <typename T>\n";
    out << "void insertAll(T& other) {\n";
    out << "for (auto const& cur : other) {\n";
    out << "insert(cur);\n";
    out << "}\n";
    out << "}\n";  // end of insertAll<T>

    out << "void insertAll(" << getTypeName() << "& other) {\n";
    for (size_t i = 0; i < numIndexes; i++) {
        out << "ind_" << i << ".insertAll(other.ind_" << i << ");\n";
    }
    out << "}\n";  // end of insertAll(relationType& other)

    // contains methods
    out << "bool contains(const t_tuple& t, context& h) const {\n";
    out << "return ind_" << masterIndex << ".contains(t, h.hints_" << masterIndex << ");\n";
    out << "}\n";

    out << "bool contains(const t_tuple& t) const {\n";
    out << "context h;\n";
    out << "return contains(t, h);\n";
    out << "}\n";

    // size method
    out << "std::size_t size() const {\n";
    out << "return ind_" << masterIndex << ".size();\n";
    out << "}\n";

    // find methods
    out << "iterator find(const t_tuple& t, context& h) const {\n";
    out << "return ind_" << masterIndex << ".find(t, h.hints_" << masterIndex << ");\n";
    out << "}\n";

    out << "iterator find(const t_tuple& t) const {\n";
    out << "context h;\n";
    out << "return find(t, h);\n";
    out << "}\n";

    // empty equalRange method
    out << "range<iterator> equalRange_0(const t_tuple& t, context& h) const {\n";
    out << "return range<iterator>(ind_" << masterIndex << ".begin(),ind_" << masterIndex << ".end());\n";
    out << "}\n";

    // equalRange methods for each pattern which is used to search this relation
    for (int64_t search : getIndexSet().getSearches()) {
        auto lexOrder = getIndexSet().getLexOrder(search);
        size_t indNum = indexToNumMap[lexOrder];

        out << "range<t_ind_" << indNum << "::iterator> equalRange_" << search;
        out << "(const t_tuple& t, context& h) const {\n";

        // count size of search pattern
        size_t indSize = 0;
        for (size_t column = 0; column < arity; column++) {
            if ((search >> column) & 1) {
                indSize++;
            }
        }

        // use the more efficient find() method if the search pattern is full
        if (indSize == arity) {
            out << "auto pos = ind_" << indNum << ".find(t, h.hints_" << indNum << ");\n";
            out << "auto fin = ind_" << indNum << ".end();\n";
            out << "if (pos != fin) {fin = pos; ++fin;}\n";
            out << "return make_range(pos, fin);\n";
        } else {
            // generate lower and upper bounds for range search
            out << "t_tuple low(t); t_tuple high(t);\n";
            // check which indices to pad out
            for (size_t column = 0; column < arity; column++) {
                // if bit number column is set
                if (!((search >> column) & 1)) {
                    out << "low[" << column << "] = MIN_RAM_DOMAIN;\n";
                    out << "high[" << column << "] = MAX_RAM_DOMAIN;\n";
                }
            }
            out << "return make_range(ind_" << indNum << ".lower_bound(low, h.hints_" << indNum << "), ind_"
                << indNum << ".upper_bound(high, h.hints_" << indNum << "));\n";
        }
        out << "}\n";

        out << "range<t_ind_" << indNum << "::iterator> equalRange_" << search;
        out << "(const t_tuple& t) const {\n";
        out << "context h;\n";
        out << "return equalRange_" << search << "(t, h);\n";
        out << "}\n";
    }

    // empty method
    out << "bool empty() const {\n";
    out << "return ind_" << masterIndex << ".empty();\n";
    out << "}\n";

    // partition method for parallelism
    out << "std::vector<range<iterator>> partition() const {\n";
    out << "return ind_" << masterIndex << ".getChunks(400);\n";
    out << "}\n";

    // purge method
    out << "void purge() {\n";
    for (size_t i = 0; i < numIndexes; i++) {
        out << "ind_" << i << ".clear();\n";
    }
    out << "}\n";

    // begin and end iterators
    out << "iterator begin() const {\n";
    out << "return ind_" << masterIndex << ".begin();\n";
    out << "}\n";

    out << "iterator end() const {\n";
    out << "return ind_" << masterIndex << ".end();\n";
    out << "}\n";

    // printHintStatistics method
    out << "void printHintStatistics(std::ostream& o, const std::string prefix) const {\n";
    for (size_t i = 0; i < numIndexes; i++) {
        out << "const auto& stats_" << i << " = ind_" << i << ".getHintStatistics();\n";
        out << "o << prefix << \"arity " << getArity() << " direct b-tree index " << inds[i]
            << ": (hits/misses/total)\\n\";\n";
        out << "o << prefix << \"Insert: \" << stats_" << i << ".inserts.getHits() << \"/\" << stats_" << i
            << ".inserts.getMisses() << \"/\" << stats_" << i << ".inserts.getAccesses() << \"\\n\";\n";
        out << "o << prefix << \"Contains: \" << stats_" << i << ".contains.getHits() << \"/\" << stats_" << i
            << ".contains.getMisses() << \"/\" << stats_" << i << ".contains.getAccesses() << \"\\n\";\n";
        out << "o << prefix << \"Lower-bound: \" << stats_" << i
            << ".lower_bound.getHits() << \"/\" << stats_" << i
            << ".lower_bound.getMisses() << \"/\" << stats_" << i
            << ".lower_bound.getAccesses() << \"\\n\";\n";
        out << "o << prefix << \"Upper-bound: \" << stats_" << i
            << ".upper_bound.getHits() << \"/\" << stats_" << i
            << ".upper_bound.getMisses() << \"/\" << stats_" << i
            << ".upper_bound.getAccesses() << \"\\n\";\n";
    }
    out << "}\n";

    // end struct
    out << "};\n";
}

// -------- Indirect Indexed B-Tree Relation --------

/** Generate index set for a indirect indexed relation */
void SynthesiserIndirectRelation::computeIndices() {
    assert(!isProvenance);

    // Generate and set indices
    std::vector<std::vector<int>> inds = indices.getAllOrders();

    // generate a full index if no indices exist
    if (inds.empty()) {
        std::vector<int> fullInd(getArity());
        std::iota(fullInd.begin(), fullInd.end(), 0);
        inds.push_back(fullInd);
        masterIndex = 0;
    }

    // Expand the first index to be a full index if no full inds exist
    bool fullExists = false;
    // check for full index
    for (size_t i = 0; i < inds.size(); i++) {
        auto& ind = inds[i];
        if (ind.size() == getArity()) {
            fullExists = true;
            if (masterIndex == (size_t)-1) {
                masterIndex = i;
            }
        }
    }

    // expand the first ind to be full, it is guaranteed that at least one index exists
    if (!fullExists) {
        std::set<int> curIndexElems(inds[0].begin(), inds[0].end());

        // expand index to be full
        for (size_t i = 0; i < getArity(); i++) {
            if (curIndexElems.find(i) == curIndexElems.end()) {
                inds[0].push_back(i);
            }
        }

        masterIndex = 0;
    }

    computedIndices = inds;
}

/** Generate type name of a indirect indexed relation */
std::string SynthesiserIndirectRelation::getTypeName() {
    std::stringstream res;
    res << "t_btree_" << getArity();

    for (auto& ind : getIndices()) {
        res << "__" << join(ind, "_");
    }

    for (auto& search : getIndexSet().getSearches()) {
        res << "__" << search;
    }

    return res.str();
}

/** Generate type struct of a indirect indexed relation */
void SynthesiserIndirectRelation::generateTypeStruct(std::ostream& out) {
    size_t arity = getArity();
    const auto& inds = getIndices();
    size_t numIndexes = inds.size();
    std::map<std::vector<int>, int> indexToNumMap;

    // struct definition
    out << "struct " << getTypeName() << " {\n";

    // stored tuple type
    out << "using t_tuple = Tuple<RamDomain, " << arity << ">;\n";

    // table and lock required for storing actual data for indirect indices
    out << "Table<t_tuple> dataTable;\n";
    out << "Lock insert_lock;\n";

    // btree types
    for (size_t i = 0; i < inds.size(); i++) {
        auto ind = inds[i];

        if (i < getIndexSet().getAllOrders().size()) {
            indexToNumMap[getIndexSet().getAllOrders()[i]] = i;
        }

        if (ind.size() == arity) {
            out << "using t_ind_" << i
                << " = btree_set<const t_tuple*, index_utils::deref_compare<typename "
                   "index_utils::comparator<"
                << join(ind) << ">>>;\n";
        } else {
            out << "using t_ind_" << i
                << " = btree_multiset<const t_tuple*, index_utils::deref_compare<typename "
                   "index_utils::comparator<"
                << join(ind) << ">>>;\n";
        }

        out << "t_ind_" << i << " ind_" << i << ";\n";
    }

    // typedef deref iterators
    for (size_t i = 0; i < numIndexes; i++) {
        out << "using iterator_" << i << " = IterDerefWrapper<typename t_ind_" << i << "::iterator>;\n";
    }
    out << "using iterator = iterator_" << masterIndex << ";\n";

    // Create a struct storing the context hints for each index
    out << "struct context {\n";
    for (size_t i = 0; i < numIndexes; i++) {
        out << "t_ind_" << i << "::operation_hints hints_" << i << ";\n";
    }
    out << "};\n";
    out << "context createContext() { return context(); }\n";

    // insert methods
    out << "bool insert(const t_tuple& t) {\n";
    out << "context h;\n";
    out << "return insert(t, h);\n";
    out << "}\n";

    out << "bool insert(const t_tuple& t, context& h) {\n";
    out << "const t_tuple* masterCopy = nullptr;\n";
    out << "{\n";
    out << "auto lease = insert_lock.acquire();\n";
    out << "if (contains(t, h)) return false;\n";
    out << "masterCopy = &dataTable.insert(t);\n";
    out << "ind_" << masterIndex << ".insert(masterCopy, h.hints_" << masterIndex << ");\n";
    out << "}\n";
    for (size_t i = 0; i < numIndexes; i++) {
        if (i != masterIndex) {
            out << "ind_" << i << ".insert(masterCopy, h.hints_" << i << ");\n";
        }
    }
    out << "return true;\n";
    out << "}\n";

    out << "bool insert(const RamDomain* ramDomain) {\n";
    out << "RamDomain data[" << arity << "];\n";
    out << "std::copy(ramDomain, ramDomain + " << arity << ", data);\n";
    out << "const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);\n";
    out << "context h;\n";
    out << "return insert(tuple, h);\n";
    out << "}\n";  // end of insert(RamDomain*)

    std::vector<std::string> decls, params;
    for (size_t i = 0; i < arity; i++) {
        decls.push_back("RamDomain a" + std::to_string(i));
        params.push_back("a" + std::to_string(i));
    }
    out << "bool insert(" << join(decls, ",") << ") {\n";
    out << "RamDomain data[" << arity << "] = {" << join(params, ",") << "};\n";
    out << "return insert(data);\n";
    out << "}\n";  // end of insert(RamDomain x1, RamDomain x2, ...)

    // insertAll method
    // do not use the specialized insertAll as it will copy references rather than tuples
    out << "template <typename T>\n";
    out << "void insertAll(T& other) {\n";
    out << "for (auto const& cur : other) {\n";
    out << "insert(cur);\n";
    out << "}\n";
    out << "}\n";

    // contains methods
    out << "bool contains(const t_tuple& t, context& h) const {\n";
    out << "return ind_" << masterIndex << ".contains(&t, h.hints_" << masterIndex << ");\n";
    out << "}\n";

    out << "bool contains(const t_tuple& t) const {\n";
    out << "context h;\n";
    out << "return contains(t, h);\n";
    out << "}\n";

    // size method
    out << "std::size_t size() const {\n";
    out << "return ind_" << masterIndex << ".size();\n";
    out << "}\n";

    // find methods
    out << "iterator find(const t_tuple& t, context& h) const {\n";
    out << "return ind_" << masterIndex << ".find(&t, h.hints_" << masterIndex << ");\n";
    out << "}\n";

    out << "iterator find(const t_tuple& t) const {\n";
    out << "context h;\n";
    out << "return find(t, h);\n";
    out << "}\n";

    // empty equalRange method
    out << "range<iterator> equalRange_0(const t_tuple& t, context& h) const {\n";
    out << "return range<iterator>(ind_" << masterIndex << ".begin(),ind_" << masterIndex << ".end());\n";
    out << "}\n";

    for (int64_t search : getIndexSet().getSearches()) {
        auto lexOrder = getIndexSet().getLexOrder(search);
        size_t indNum = indexToNumMap[lexOrder];

        out << "range<iterator_" << indNum << "> equalRange_" << search;
        out << "(const t_tuple& t, context& h) const {\n";

        // count size of search pattern
        size_t indSize = 0;
        for (size_t column = 0; column < arity; column++) {
            if ((search >> column) & 1) {
                indSize++;
            }
        }

        // use the more efficient find() method for full range search
        if (indSize == arity) {
            out << "auto pos = find(t, h);\n";
            out << "auto fin = end();\n";
            out << "if (pos != fin) {fin = pos; ++fin;}\n";
            out << "return make_range(pos, fin);\n";
        } else {
            out << "t_tuple low(t); t_tuple high(t);\n";
            // check which indices to pad out
            for (size_t column = 0; column < arity; column++) {
                // if bit number column is set
                if (!((search >> column) & 1)) {
                    out << "low[" << column << "] = MIN_RAM_DOMAIN;\n";
                    out << "high[" << column << "] = MAX_RAM_DOMAIN;\n";
                }
            }
            out << "return range<iterator_" << indNum << ">(ind_" << indNum << ".lower_bound(&low, h.hints_"
                << indNum << "), ind_" << indNum << ".upper_bound(&high, h.hints_" << indNum << "));\n";
        }
        out << "}\n";

        out << "range<iterator_" << indNum << "> equalRange_" << search;
        out << "(const t_tuple& t) const {\n";
        out << "context h; return equalRange_" << search << "(t, h);\n";
        out << "}\n";
    }

    // empty method
    out << "bool empty() const {\n";
    out << "return ind_" << masterIndex << ".empty();\n";
    out << "}\n";

    // partition method
    out << "std::vector<range<iterator>> partition() const {\n";
    out << "std::vector<range<iterator>> res;\n";
    out << "for (const auto& cur : ind_" << masterIndex << ".getChunks(400)) {\n";
    out << "    res.push_back(make_range(derefIter(cur.begin()), derefIter(cur.end())));\n";
    out << "}\n";
    out << "return res;\n";
    out << "}\n";

    // purge method
    out << "void purge() {\n";
    for (size_t i = 0; i < numIndexes; i++) {
        out << "ind_" << i << ".clear();\n";
    }
    out << "dataTable.clear();\n";
    out << "}\n";

    // begin and end iterators
    out << "iterator begin() const {\n";
    out << "return ind_" << masterIndex << ".begin();\n";
    out << "}\n";

    out << "iterator end() const {\n";
    out << "return ind_" << masterIndex << ".end();\n";
    out << "}\n";

    // printHintStatistics method
    out << "void printHintStatistics(std::ostream& o, const std::string prefix) const {\n";
    for (size_t i = 0; i < numIndexes; i++) {
        out << "const auto& stats_" << i << " = ind_" << i << ".getHintStatistics();\n";
        out << "o << prefix << \"arity " << arity << " indirect b-tree index " << inds[i]
            << ": (hits/misses/total)\\n\";\n";
        out << "o << prefix << \"Insert: \" << stats_" << i << ".inserts.getHits() << \"/\" << stats_" << i
            << ".inserts.getMisses() << \"/\" << stats_" << i << ".inserts.getAccesses() << \"\\n\";\n";
        out << "o << prefix << \"Contains: \" << stats_" << i << ".contains.getHits() << \"/\" << stats_" << i
            << ".contains.getMisses() << \"/\" << stats_" << i << ".contains.getAccesses() << \"\\n\";\n";
        out << "o << prefix << \"Lower-bound: \" << stats_" << i
            << ".lower_bound.getHits() << \"/\" << stats_" << i
            << ".lower_bound.getMisses() << \"/\" << stats_" << i
            << ".lower_bound.getAccesses() << \"\\n\";\n";
        out << "o << prefix << \"Upper-bound: \" << stats_" << i
            << ".upper_bound.getHits() << \"/\" << stats_" << i
            << ".upper_bound.getMisses() << \"/\" << stats_" << i
            << ".upper_bound.getAccesses() << \"\\n\";\n";
    }
    out << "}\n";

    // end struct
    out << "};\n";
}

// -------- Brie Relation --------

/** Generate index set for a brie relation */
void SynthesiserBrieRelation::computeIndices() {
    assert(!isProvenance && "bries cannot be used with provenance");

    // Generate and set indices
    std::vector<std::vector<int>> inds = indices.getAllOrders();

    // generate a full index if no indices exist
    if (inds.empty()) {
        std::vector<int> fullInd(getArity());
        std::iota(fullInd.begin(), fullInd.end(), 0);
        inds.push_back(fullInd);
    }

    // expand all indexes to be full
    for (auto& ind : inds) {
        if (ind.size() != getArity()) {
            // use a set as a cache for fast lookup
            std::set<int> curIndexElems(ind.begin(), ind.end());

            // expand index to be full
            for (size_t i = 0; i < getArity(); i++) {
                if (curIndexElems.find(i) == curIndexElems.end()) {
                    ind.push_back(i);
                }
            }
        }

        assert(ind.size() == getArity());
    }
    masterIndex = 0;

    computedIndices = inds;
}

/** Generate type name of a brie relation */
std::string SynthesiserBrieRelation::getTypeName() {
    std::stringstream res;
    res << "t_brie_" << getArity();

    for (auto& ind : getIndices()) {
        res << "__" << join(ind, "_");
    }

    for (auto& search : getIndexSet().getSearches()) {
        res << "__" << search;
    }

    return res.str();
}

/** Generate type struct of a brie relation */
void SynthesiserBrieRelation::generateTypeStruct(std::ostream& out) {
    size_t arity = getArity();
    const auto& inds = getIndices();
    size_t numIndexes = inds.size();
    std::map<std::vector<int>, int> indexToNumMap;

    // struct definition
    out << "struct " << getTypeName() << " {\n";

    // define trie structures
    for (size_t i = 0; i < inds.size(); i++) {
        if (i < getIndexSet().getAllOrders().size()) {
            indexToNumMap[getIndexSet().getAllOrders()[i]] = i;
        }
        out << "using t_ind_" << i << " = Trie<" << inds[i].size() << ">;\n";
        out << "t_ind_" << i << " ind_" << i << ";\n";
    }
    out << "using t_tuple = t_ind_" << masterIndex << "::entry_type;\n";

    // generate auxiliary iterators that use orderOut
    for (size_t i = 0; i < numIndexes; i++) {
        // generate auxiliary iterators which orderOut
        out << "class iterator_" << i << " : public std::iterator<std::forward_iterator_tag, t_tuple> {\n";
        out << "    using nested_iterator = typename t_ind_" << i << "::iterator;\n";
        out << "    nested_iterator nested;\n";
        out << "    t_tuple value;\n";

        out << "public:\n";
        out << "    iterator_" << i << "() = default;\n";
        out << "    iterator_" << i << "(const nested_iterator& iter) : nested(iter), value(orderOut_" << i
            << "(*iter)) {}\n";
        out << "    iterator_" << i << "(const iterator_" << i << "& other) = default;\n";
        out << "    iterator_" << i << "& operator=(const iterator_" << i << "& other) = default;\n";

        out << "    bool operator==(const iterator_" << i << "& other) const {\n";
        out << "        return nested == other.nested;\n";
        out << "    }\n";

        out << "    bool operator!=(const iterator_" << i << "& other) const {\n";
        out << "        return !(*this == other);\n";
        out << "    }\n";

        out << "    const t_tuple& operator*() const {\n";
        out << "        return value;\n";
        out << "    }\n";

        out << "    const t_tuple* operator->() const {\n";
        out << "        return &value;\n";
        out << "    }\n";

        out << "    iterator_" << i << "& operator++() {\n";
        out << "        ++nested;\n";
        out << "        value = orderOut_" << i << "(*nested);\n";
        out << "        return *this;\n";
        out << "    }\n";
        out << "};\n";
    }
    out << "using iterator = iterator_" << masterIndex << ";\n";

    // hints struct
    out << "struct context {\n";
    for (size_t i = 0; i < numIndexes; i++) {
        out << "t_ind_" << i << "::op_context hints_" << i << ";\n";
    }
    out << "};\n";
    out << "context createContext() { return context(); }\n";

    // insert methods
    out << "bool insert(const t_tuple& t) {\n";
    out << "context h;\n";
    out << "return insert(t, h);\n";
    out << "}\n";

    out << "bool insert(const t_tuple& t, context& h) {\n";
    out << "if (ind_" << masterIndex << ".insert(orderIn_" << masterIndex << "(t), h.hints_" << masterIndex
        << ")) {\n";
    for (size_t i = 0; i < numIndexes; i++) {
        if (i != masterIndex) {
            out << "ind_" << i << ".insert(orderIn_" << i << "(t), h.hints_" << i << ");\n";
        }
    }
    out << "return true;\n";
    out << "} else return false;\n";
    out << "}\n";

    out << "bool insert(const RamDomain* ramDomain) {\n";
    out << "RamDomain data[" << arity << "];\n";
    out << "std::copy(ramDomain, ramDomain + " << arity << ", data);\n";
    out << "const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);\n";
    out << "context h;\n";
    out << "return insert(tuple, h);\n";
    out << "}\n";

    // insertAll method
    out << "template <typename T>\n";
    out << "void insertAll(T& other) {\n";
    out << "for (auto const& cur : other) {\n";
    out << "insert(cur);\n";
    out << "}\n";
    out << "}\n";

    // insertAll using the index method
    out << "void insertAll(" << getTypeName() << "& other) {\n";
    for (size_t i = 0; i < numIndexes; i++) {
        out << "ind_" << i << ".insertAll(other.ind_" << i << ");\n";
    }
    out << "}\n";

    // insert method
    std::vector<std::string> decls, params;
    for (size_t i = 0; i < arity; i++) {
        decls.push_back("RamDomain a" + std::to_string(i));
        params.push_back("a" + std::to_string(i));
    }
    out << "bool insert(" << join(decls, ",") << ") {\nRamDomain data[";
    out << arity << "] = {" << join(params, ",") << "};\n";
    out << "return insert(data);\n";
    out << "}\n";

    // contains methods
    out << "bool contains(const t_tuple& t, context& h) const {\n";
    out << "return ind_" << masterIndex << ".contains(orderIn_" << masterIndex << "(t), h.hints_"
        << masterIndex << ");\n";
    out << "}\n";

    out << "bool contains(const t_tuple& t) const {\n";
    out << "context h;\n";
    out << "return contains(t, h);\n";
    out << "}\n";

    // size method
    out << "std::size_t size() const {\n";
    out << "return ind_" << masterIndex << ".size();\n";
    out << "}\n";

    // find methods
    if (arity > 1) {
        out << "iterator find(const t_tuple& t, context& h) const {\n";
        out << "return ind_" << masterIndex << ".find(orderIn_" << masterIndex << "(t), h.hints_"
            << masterIndex << ");\n";
        out << "}\n";

        out << "iterator find(const t_tuple& t) const {\n";
        out << "context h;\n";
        out << "return find(t, h);\n";
        out << "}\n";
    }

    // empty equalRange method
    out << "range<iterator> equalRange_0(const t_tuple& t, context& h) const {\n";
    out << "return range<iterator>(ind_" << masterIndex << ".begin(),ind_" << masterIndex << ".end());\n";
    out << "}\n";

    // equalRange methods
    for (int64_t search : getIndexSet().getSearches()) {
        auto lexOrder = getIndexSet().getLexOrder(search);
        size_t indNum = indexToNumMap[lexOrder];

        out << "range<iterator_" << indNum << "> equalRange_" << search;
        out << "(const t_tuple& t, context& h) const {\n";

        // compute size of sub-index
        size_t indSize = 0;
        for (size_t i = 0; i < arity; i++) {
            if ((search >> i) & 1) {
                indSize++;
            }
        }

        out << "auto r = ind_" << indNum << ".template getBoundaries<" << indSize << ">(orderIn_" << indNum
            << "(t), h.hints_" << indNum << ");\n";
        out << "return make_range(iterator_" << indNum << "(r.begin()), iterator_" << indNum
            << "(r.end()));\n";
        out << "}\n";

        out << "range<iterator_" << indNum << "> equalRange_" << search;
        out << "(const t_tuple& t) const {\n";
        out << "context h; return equalRange_" << search << "(t, h);\n";
        out << "}\n";
    }

    // empty method
    out << "bool empty() const {\n";
    out << "return ind_" << masterIndex << ".empty();\n";
    out << "}\n";

    // partition method
    out << "std::vector<range<iterator>> partition() const {\n";
    out << "std::vector<range<iterator>> res;\n";
    out << "for (const auto& cur : ind_" << masterIndex << ".partition(10000)) {\n";
    out << "    res.push_back(make_range(iterator(cur.begin()), iterator(cur.end())));\n";
    out << "}\n";
    out << "return res;\n";
    out << "}\n";

    // purge method
    out << "void purge() {\n";
    for (size_t i = 0; i < numIndexes; i++) {
        out << "ind_" << i << ".clear();\n";
    }
    out << "}\n";

    // begin and end iterators
    out << "iterator begin() const {\n";
    out << "return iterator_" << masterIndex << "(ind_" << masterIndex << ".begin());\n";
    out << "}\n";

    out << "iterator end() const {\n";
    out << "return iterator_" << masterIndex << "(ind_" << masterIndex << ".end());\n";
    out << "}\n";

    // TODO: finish printHintStatistics method
    out << "void printHintStatistics(std::ostream& o, const std::string prefix) const {\n";
    for (size_t i = 0; i < numIndexes; i++) {
        out << "const auto& stats_" << i << " = ind_" << i << ".getHintStatistics();\n";
        out << "o << prefix << \"arity " << arity << " brie index " << inds[i]
            << ": (hits/misses/total)\\n\";\n";
        out << "o << prefix << \"Insert: \" << stats_" << i << ".inserts.getHits() << \"/\" << stats_" << i
            << ".inserts.getMisses() << \"/\" << stats_" << i << ".inserts.getAccesses() << \"\\n\";\n";
        out << "o << prefix << \"Contains: \" << stats_" << i << ".contains.getHits() << \"/\" << stats_" << i
            << ".contains.getMisses() << \"/\" << stats_" << i << ".contains.getAccesses() << \"\\n\";\n";
        out << "o << prefix << \"Range-query: \" << stats_" << i
            << ".get_boundaries.getHits() << \"/\" << stats_" << i
            << ".get_boundaries.getMisses() << \"/\" << stats_" << i
            << ".get_boundaries.getAccesses() << \"\\n\";\n";
    }
    out << "}\n";

    // orderOut and orderIn methods for reordering tuples according to index orders
    for (size_t i = 0; i < numIndexes; i++) {
        auto ind = inds[i];
        out << "static t_tuple orderIn_" << i << "(const t_tuple& t) {\n";
        out << "t_tuple res;\n";
        for (size_t j = 0; j < ind.size(); j++) {
            out << "res[" << j << "] = t[" << ind[j] << "];\n";
        }
        out << "return res;\n";
        out << "}\n";

        out << "static t_tuple orderOut_" << i << "(const t_tuple& t) {\n";
        out << "t_tuple res;\n";
        for (size_t j = 0; j < ind.size(); j++) {
            out << "res[" << ind[j] << "] = t[" << j << "];\n";
        }
        out << "return res;\n";
        out << "}\n";
    }

    // end class
    out << "};\n";
}

// -------- Eqrel Relation --------

/** Generate index set for a eqrel relation */
void SynthesiserEqrelRelation::computeIndices() {
    assert(!isProvenance && "eqrel cannot be used with provenance");

    // Generate and set indices
    std::vector<std::vector<int>> inds = indices.getAllOrders();

    // generate a full index if no indices exist
    if (inds.empty()) {
        std::vector<int> fullInd(getArity());
        std::iota(fullInd.begin(), fullInd.end(), 0);
        inds.push_back(fullInd);
    }

    // expand all indexes to be full
    for (auto& ind : inds) {
        if (ind.size() != getArity()) {
            // use a set as a cache for fast lookup
            std::set<int> curIndexElems(ind.begin(), ind.end());

            // expand index to be full
            for (size_t i = 0; i < getArity(); i++) {
                if (curIndexElems.find(i) == curIndexElems.end()) {
                    ind.push_back(i);
                }
            }
        }

        assert(ind.size() == getArity());
    }

    masterIndex = 0;
    computedIndices = inds;
}

/** Generate type name of a eqrel relation */
std::string SynthesiserEqrelRelation::getTypeName() {
    return "t_eqrel";
}

/** Generate type struct of a eqrel relation */
void SynthesiserEqrelRelation::generateTypeStruct(std::ostream& out) {
    const auto& inds = getIndices();
    size_t numIndexes = inds.size();
    std::map<std::vector<int>, int> indexToNumMap;

    // struct definition
    out << "struct " << getTypeName() << " {\n";

    // eqrel is only for binary relations
    out << "using t_tuple = ram::Tuple<RamDomain, 2>;\n";
    out << "using t_ind_" << masterIndex << " = BinaryRelation<t_tuple>;\n";
    out << "t_ind_" << masterIndex << " ind_" << masterIndex << ";\n";

    // generate auxiliary iterators that reorder tuples according to index orders
    for (size_t i = 0; i < numIndexes; i++) {
        // generate auxiliary iterators which orderOut
        out << "class iterator_" << i << " : public std::iterator<std::forward_iterator_tag, t_tuple> {\n";
        out << "    using nested_iterator = typename t_ind_0::iterator;\n";
        out << "    nested_iterator nested;\n";
        out << "    t_tuple value;\n";

        out << "public:\n";
        out << "    iterator_" << i << "() = default;\n";
        out << "    iterator_" << i << "(const nested_iterator& iter) : nested(iter), value(orderOut_" << i
            << "(*iter)) {}\n";
        out << "    iterator_" << i << "(const iterator_" << i << "& other) = default;\n";
        out << "    iterator_" << i << "& operator=(const iterator_" << i << "& other) = default;\n";

        out << "    bool operator==(const iterator_" << i << "& other) const {\n";
        out << "        return nested == other.nested;\n";
        out << "    }\n";

        out << "    bool operator!=(const iterator_" << i << "& other) const {\n";
        out << "        return !(*this == other);\n";
        out << "    }\n";

        out << "    const t_tuple& operator*() const {\n";
        out << "        return value;\n";
        out << "    }\n";

        out << "    const t_tuple* operator->() const {\n";
        out << "        return &value;\n";
        out << "    }\n";

        out << "    iterator_" << i << "& operator++() {\n";
        out << "        ++nested;\n";
        out << "        value = orderOut_" << i << "(*nested);\n";
        out << "        return *this;\n";
        out << "    }\n";
        out << "};\n";
    }

    out << "using iterator = iterator_" << masterIndex << ";\n";

    // Create a struct storing the context hints for each index
    out << "struct context {\n";
    out << "t_ind_" << masterIndex << "::operation_hints hints_" << masterIndex << ";\n";
    out << "};\n";
    out << "context createContext() { return context(); }\n";

    // insert methods
    out << "bool insert(const t_tuple& t) {\n";
    out << "return ind_" << masterIndex << ".insert(t[0], t[1]);\n";
    out << "}\n";

    out << "bool insert(const t_tuple& t, context& h) {\n";
    out << "return ind_" << masterIndex << ".insert(t[0], t[1], h.hints_" << masterIndex << ");\n";
    out << "}\n";

    out << "bool insert(const RamDomain* ramDomain) {\n";
    out << "RamDomain data[2];\n";
    out << "std::copy(ramDomain, ramDomain + 2, data);\n";
    out << "const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);\n";
    out << "context h;\n";
    out << "return insert(tuple, h);\n";
    out << "}\n";

    out << "bool insert(RamDomain a1, RamDomain a2) {\n";
    out << "RamDomain data[2] = {a1, a2};\n";
    out << "return insert(data);\n";
    out << "}\n";

    // extends method for eqrel
    // performs a delta extension, where we union the sets that share elements between this and other.
    //      i.e. if a in this, and a in other, union(set(this->a), set(other->a))
    out << "void extend(const " << getTypeName() << "& other) {\n";
    out << "ind_" << masterIndex << ".extend(other.ind_" << masterIndex << ");\n";
    out << "}\n";

    // insertAll method
    out << "template <typename T>\n";
    out << "void insertAll(T& other) {\n";
    out << "for (auto const& cur : other) {\n";
    out << "insert(cur);\n";
    out << "}\n";
    out << "}\n";

    // insertAll using the index method
    out << "void insertAll(" << getTypeName() << "& other) {\n";
    out << "ind_" << masterIndex << ".insertAll(other.ind_" << masterIndex << ");\n";
    out << "}\n";

    // contains methods
    out << "bool contains(const t_tuple& t) const {\n";
    out << "return ind_" << masterIndex << ".contains(t[0], t[1]);\n";
    out << "}\n";

    out << "bool contains(const t_tuple& t, context& h) const {\n";
    out << "return ind_" << masterIndex << ".contains(t[0], t[1]);\n";
    out << "}\n";

    // size method
    out << "std::size_t size() const {\n";
    out << "return ind_" << masterIndex << ".size();\n";
    out << "}\n";

    // find methods
    out << "iterator find(const t_tuple& t) const {\n";
    out << "return ind_" << masterIndex << ".find(orderIn_" << masterIndex << "(t));\n";
    out << "}\n";

    out << "iterator find(const t_tuple& t, context& h) const {\n";
    out << "return ind_" << masterIndex << ".find(orderIn_" << masterIndex << "(t));\n";
    out << "}\n";

    // equalRange methods, one for each of the 4 possible search patterns
    for (int i = 1; i < 4; i++) {
        out << "range<iterator> equalRange_" << i;
        out << "(const t_tuple& t, context& h) const {\n";
        // compute size of sub-index
        size_t indSize = 0;
        for (size_t column = 0; column < 2; column++) {
            if ((i >> column) & 1) {
                indSize++;
            }
        }
        out << "auto r = ind_" << masterIndex << ".template getBoundaries<" << indSize << ">(orderIn_"
            << masterIndex << "(t), h.hints_" << masterIndex << ");\n";
        out << "return make_range(iterator(r.begin()), iterator(r.end()));\n";
        out << "}\n";

        out << "range<iterator> equalRange_" << i;
        out << "(const t_tuple& t) const {\n";
        out << "context h; return equalRange_" << i << "(t, h);\n";
        out << "}\n";
    }

    // empty method
    out << "bool empty() const {\n";
    out << "return ind_" << masterIndex << ".size() == 0;\n";
    out << "}\n";

    // partition method
    out << "std::vector<range<iterator>> partition() const {\n";
    out << "std::vector<range<iterator>> res;\n";
    out << "for (const auto& cur : ind_" << masterIndex << ".partition(10000)) {\n";
    out << "    res.push_back(make_range(iterator(cur.begin()), iterator(cur.end())));\n";
    out << "}\n";
    out << "return res;\n";
    out << "}\n";

    // purge method
    out << "void purge() {\n";
    for (size_t i = 0; i < numIndexes; i++) {
        out << "ind_" << i << ".clear();\n";
    }
    out << "}\n";

    // begin and end iterators
    out << "iterator begin() const {\n";
    out << "return iterator_" << masterIndex << "(ind_" << masterIndex << ".begin());\n";
    out << "}\n";

    out << "iterator end() const {\n";
    out << "return iterator_" << masterIndex << "(ind_" << masterIndex << ".end());\n";
    out << "}\n";

    // printHintStatistics method
    out << "void printHintStatistics(std::ostream& o, const std::string prefix) const {\n";
    out << "o << \"eqrel index: no hint statistics supported\\n\";\n";
    out << "}\n";

    // generate orderIn and orderOut methods which reorder tuples
    // according to index orders
    for (size_t i = 0; i < numIndexes; i++) {
        auto ind = inds[i];
        out << "static t_tuple orderIn_" << i << "(const t_tuple& t) {\n";
        out << "t_tuple res;\n";
        for (size_t j = 0; j < ind.size(); j++) {
            out << "res[" << j << "] = t[" << ind[j] << "];\n";
        }
        out << "return res;\n";
        out << "}\n";

        out << "static t_tuple orderOut_" << i << "(const t_tuple& t) {\n";
        out << "t_tuple res;\n";
        for (size_t j = 0; j < ind.size(); j++) {
            out << "res[" << ind[j] << "] = t[" << j << "];\n";
        }
        out << "return res;\n";
        out << "}\n";
    }

    // end class
    out << "};\n";
}

// -------- Rbtset Relation --------

}  // end of namespace souffle

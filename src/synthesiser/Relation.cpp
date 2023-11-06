/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "synthesiser/Relation.h"
#include "RelationTag.h"
#include "ram/analysis/Index.h"
#include "souffle/SouffleInterface.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include "synthesiser/Utils.h"
#include <algorithm>
#include <cassert>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace souffle::synthesiser {

using namespace stream_write_qualified_char_as_number;
using namespace ram;
using ram::analysis::LexOrder;
using ram::analysis::SearchSignature;

std::string Relation::getTypeAttributeString(const std::vector<std::string>& attributeTypes,
        const std::unordered_set<std::size_t>& attributesUsed) const {
    std::stringstream type;
    for (std::size_t i = 0; i < attributeTypes.size(); ++i) {
        // consider only attributes used in a lex-order
        if (attributesUsed.find(i) != attributesUsed.end()) {
            switch (attributeTypes[i][0]) {
                case 'f': type << 'f'; break;
                case 'u': type << 'u'; break;
                default: type << 'i';  // consider all non-float/unsigned types (i.e. records) as RamSigned
            }
        }
    }
    return type.str();
}

Own<Relation> Relation::getSynthesiserRelation(
        const ram::Relation& ramRel, const ram::analysis::IndexCluster& indexSelection) {
    Relation* rel;

    bool hasProvenance = ramRel.getAttributeNames().back() == "@level_number";
    // Handle the qualifier in souffle code
    if (ramRel.getAuxiliaryArity() > 0) {
        rel = new DirectRelation(ramRel, indexSelection, true, hasProvenance, false);
    } else if (ramRel.isNullary()) {
        rel = new NullaryRelation(ramRel, indexSelection);
    } else if (ramRel.getRepresentation() == RelationRepresentation::BTREE) {
        rel = new DirectRelation(ramRel, indexSelection, false, false, false);
    } else if (ramRel.getRepresentation() == RelationRepresentation::BTREE_DELETE) {
        rel = new DirectRelation(ramRel, indexSelection, false, false, true);
    } else if (ramRel.getRepresentation() == RelationRepresentation::BRIE) {
        rel = new BrieRelation(ramRel, indexSelection);
    } else if (ramRel.getRepresentation() == RelationRepresentation::EQREL) {
        rel = new EqrelRelation(ramRel, indexSelection);
    } else if (ramRel.getRepresentation() == RelationRepresentation::INFO) {
        rel = new InfoRelation(ramRel, indexSelection);
    } else {
        // Handle the data structure command line flag
        if (ramRel.getArity() > 6) {
            rel = new IndirectRelation(ramRel, indexSelection);
        } else {
            rel = new DirectRelation(ramRel, indexSelection, false, false, false);
        }
    }

    assert(rel != nullptr && "relation type not specified");
    // generate index set
    rel->computeIndices();

    return Own<Relation>(rel);
}

// -------- Info Relation --------

/** Generate index set for a info relation, which should be empty */
void InfoRelation::computeIndices() {
    computedIndices = {};
}

/** Generate type name of a info relation */
std::string InfoRelation::getTypeName() {
    return "t_info<" + std::to_string(getArity()) + ">";
}

/** Generate type struct of a info relation, which is empty,
 * the actual implementation is in Info.h */
void InfoRelation::generateTypeStruct(GenDb& db) {
    db.datastructureIncludes(getTypeName(), "\"souffle/datastructure/Info.h\"");
    return;
}

// -------- Nullary Relation --------

/** Generate index set for a nullary relation, which should be empty */
void NullaryRelation::computeIndices() {
    computedIndices = {};
}

/** Generate type name of a nullary relation */
std::string NullaryRelation::getTypeName() {
    return "t_nullaries";
}

/** Generate type struct of a nullary relation, which is empty,
 * the actual implementation is in Nullaries.h */
void NullaryRelation::generateTypeStruct(GenDb& db) {
    db.datastructureIncludes(getTypeName(), "\"souffle/datastructure/Nullaries.h\"");
    return;
}

// -------- Direct Indexed B-Tree Relation --------

/** Generate index set for a direct indexed relation */
void DirectRelation::computeIndices() {
    // Generate and set indices
    auto inds = indexSelection.getAllOrders();

    // generate a full index if no indices exist
    assert(!inds.empty() && "no full index in relation");

    std::size_t index_nr = 0;
    // expand all search orders to be full
    for (auto& ind : inds) {
        // use a set as a cache for fast lookup
        std::set<std::size_t> curIndexElems(ind.begin(), ind.end());

        // If this relation is used with provenance,
        // we must expand all search orders to be full indices,
        // since weak/strong comparators and updaters need this,
        // and also add provenance annotations to the indices
        if (hasAuxiliary || hasErase) {
            // expand index to be full
            for (std::size_t i = 0; i < getArity() - relation.getAuxiliaryArity(); i++) {
                if (curIndexElems.find(i) == curIndexElems.end()) {
                    ind.push_back(i);
                }
            }

            if (hasAuxiliary) {
                // remove any provenance annotations already in the index order
                std::size_t n = getArity() - relation.getAuxiliaryArity();
                for (std::size_t i = n; i < getArity(); i++) {
                    if (curIndexElems.find(i) != curIndexElems.end()) {
                        ind.erase(std::find(ind.begin(), ind.end(), i));
                    }
                }
                // add provenance annotations to the index, but in reverse order
                for (std::size_t i = getArity(); i > n; i--) {
                    ind.push_back(i-1);
                }
            }
            masterIndex = 0;
        } else if (ind.size() == getArity()) {
            masterIndex = index_nr;
        }
        index_nr++;
    }
    assert(masterIndex < inds.size() && "no full index in relation");
    computedIndices = inds;
}

/** Generate type name of a direct indexed relation */
std::string DirectRelation::getTypeNamespace() {
    // collect all attributes used in the lex-order
    std::unordered_set<std::size_t> attributesUsed;
    for (auto& ind : getIndices()) {
        for (auto& attr : ind) {
            attributesUsed.insert(attr);
        }
    }

    std::stringstream res;
    res << "t_btree_";
    res << hasErase << hasAuxiliary << hasProvenance << "_";
    res << getTypeAttributeString(relation.getAttributeTypes(), attributesUsed);

    for (auto& ind : getIndices()) {
        res << "__" << join(ind, "_");
    }

    for (auto& search : indexSelection.getSearches()) {
        res << "__" << search;
    }

    return res.str();
}

std::string DirectRelation::getTypeName() {
    return getTypeNamespace() + "::Type";
}

/** Generate type struct of a direct indexed relation */
void DirectRelation::generateTypeStruct(GenDb& db) {
    std::size_t arity = getArity();
    std::size_t auxiliaryArity = relation.getAuxiliaryArity();
    auto types = relation.getAttributeTypes();
    const auto& inds = getIndices();
    std::size_t numIndexes = inds.size();
    std::map<LexOrder, std::size_t> indexToNumMap;

    fs::path basename(uniqueCppIdent(getTypeNamespace(), 20));
    GenDatastructure& cl = db.getDatastructure("Type", basename, std::make_optional(getTypeNamespace()));
    std::ostream& decl = cl.decl();
    std::ostream& def = cl.def();

    cl.addInclude("\"souffle/SouffleInterface.h\"");
    if (hasErase) {
        cl.addInclude("\"souffle/datastructure/BTreeDelete.h\"");
    } else {
        cl.addInclude("\"souffle/datastructure/BTree.h\"");
    }

    // struct definition
    decl << "struct Type {\n";
    decl << "static constexpr Relation::arity_type Arity = " << arity << ";\n";

    // stored tuple type
    decl << "using t_tuple = Tuple<RamDomain, " << arity << ">;\n";

    // generate an updater class for provenance
    if (hasAuxiliary) {
        decl << "struct updater {\n";
        decl << "bool update(t_tuple& old_t, const t_tuple& new_t) {\n";
        decl << "bool changed = false;\n";
        if (hasProvenance) {
            assert(auxiliaryArity == 2);
            auto rule = arity-2;
            auto level = arity-1;
            decl << "if (ramBitCast<RamSigned>(new_t[" << level << "]) < ramBitCast<RamSigned>(old_t[" << level << "])"
                 << " || (ramBitCast<RamSigned>(new_t[" << level << "]) == ramBitCast<RamSigned>(old_t[" << level << "])"
                 << " && ramBitCast<RamSigned>(new_t[" << rule << "]) < ramBitCast<RamSigned>(old_t[" << rule << "]))) {\n";
            decl << "    old_t[" << rule << "] = new_t[" << rule << "];\n";
            decl << "    old_t[" << level << "] = new_t[" << level << "];\n";
            decl << "    changed = true;\n";
            decl << "}\n";
        } else {
            for (std::size_t i = arity - auxiliaryArity; i < arity; i++) {
                decl << "if (old_t[" << i << "] != new_t[" << i << "]) {\n";
                decl << "    changed = true;\n";
                decl << "    old_t[" << i << "] = new_t[" << i << "];\n";
                decl << "}\n";
            }
        }
        decl << "return changed;\n";

        decl << "}\n";
        decl << "};\n";
    }

    // generate the btree type for each relation
    for (std::size_t i = 0; i < inds.size(); i++) {
        auto& ind = inds[i];

        if (i < indexSelection.getAllOrders().size()) {
            indexToNumMap[indexSelection.getAllOrders()[i]] = i;
        }

        std::vector<std::string> typecasts;
        typecasts.reserve(types.size());

        for (auto type : types) {
            switch (type[0]) {
                case 'f': typecasts.push_back("ramBitCast<RamFloat>"); break;
                case 'u': typecasts.push_back("ramBitCast<RamUnsigned>"); break;
                default: typecasts.push_back("ramBitCast<RamSigned>");
            }
        }

        auto genstruct = [&](std::string name, std::size_t bound) {
            decl << "struct " << name << "{\n";
            decl << " int operator()(const t_tuple& a, const t_tuple& b) const {\n";
            decl << "  return ";
            std::function<void(std::size_t)> gencmp = [&](std::size_t i) {
                std::size_t attrib = ind[i];
                const auto& typecast = typecasts[attrib];

                decl << "(" << typecast << "(a[" << attrib << "]) < " << typecast << "(b[" << attrib
                     << "])) ? -1 : (" << typecast << "(a[" << attrib << "]) > " << typecast << "(b["
                     << attrib << "])) ? 1 :(";
                if (i + 1 < bound) {
                    gencmp(i + 1);
                } else {
                    decl << "0";
                }
                decl << ")";
            };
            gencmp(0);
            decl << ";\n }\n";
            decl << "bool less(const t_tuple& a, const t_tuple& b) const {\n";
            decl << "  return ";
            std::function<void(std::size_t)> genless = [&](std::size_t i) {
                std::size_t attrib = ind[i];
                const auto& typecast = typecasts[attrib];

                decl << "(" << typecast << "(a[" << attrib << "]) < " << typecast << "(b[" << attrib << "]))";
                if (i + 1 < bound) {
                    decl << "|| ((" << typecast << "(a[" << attrib << "]) == " << typecast << "(b[" << attrib
                         << "])) && (";
                    genless(i + 1);
                    decl << "))";
                }
            };
            genless(0);
            decl << ";\n }\n";
            decl << "bool equal(const t_tuple& a, const t_tuple& b) const {\n";
            decl << "return ";
            std::function<void(std::size_t)> geneq = [&](std::size_t i) {
                std::size_t attrib = ind[i];
                const auto& typecast = typecasts[attrib];

                decl << "(" << typecast << "(a[" << attrib << "]) == " << typecast << "(b[" << attrib
                     << "]))";
                if (i + 1 < bound) {
                    decl << "&&";
                    geneq(i + 1);
                }
            };
            geneq(0);
            decl << ";\n }\n";
            decl << "};\n";
        };

        std::string comparator = "t_comparator_" + std::to_string(i);
        genstruct(comparator, ind.size());

        // for provenance, all indices must be full so we use btree_set
        // also strong/weak comparators and updater methods

        if (hasAuxiliary) {
            std::string comparator_aux;
            if (provenanceIndexNumbers.find(i) == provenanceIndexNumbers.end()) {
                // index for bottom up phase
                comparator_aux = "t_comparator_" + std::to_string(i) + "_aux";
                genstruct(comparator_aux, ind.size() - auxiliaryArity);
            } else {
                // index for top down phase
                comparator_aux = comparator;
            }
            decl << "using t_ind_" << i << " = btree_set<t_tuple," << comparator
                 << ",std::allocator<t_tuple>,256,typename "
                    "souffle::detail::default_strategy<t_tuple>::type,"
                 << comparator_aux << ",updater>;\n";
        } else {
            std::string btree_name = "btree";
            if (hasErase) {
                btree_name = "btree_delete";
            }
            if (ind.size() == arity) {
                decl << "using t_ind_" << i << " = " << btree_name << "_set<t_tuple," << comparator << ">;\n";
            } else {
                // without provenance, some indices may be not full, so we use btree_multiset for those
                decl << "using t_ind_" << i << " = " << btree_name << "_multiset<t_tuple," << comparator
                     << ">;\n";
            }
        }
        decl << "t_ind_" << i << " ind_" << i << ";\n";
        def << "using t_ind_" << i << " = Type::t_ind_" << i << ";\n";
    }

    // typedef master index iterator to be struct iterator
    decl << "using iterator = t_ind_" << masterIndex << "::iterator;\n";
    def << "using iterator = Type::iterator;\n";

    // create a struct storing hints for each btree
    decl << "struct context {\n";
    for (std::size_t i = 0; i < numIndexes; i++) {
        decl << "t_ind_" << i << "::operation_hints hints_" << i << "_lower"
             << ";\n";
        decl << "t_ind_" << i << "::operation_hints hints_" << i << "_upper"
             << ";\n";
    }
    decl << "};\n";
    def << "using context = Type::context;\n";
    decl << "context createContext() { return context(); }\n";

    // erase method
    if (hasErase) {
        decl << "bool erase(const t_tuple& t);\n";

        def << "bool Type::erase(const t_tuple& t) {\n";

        def << "if (ind_" << masterIndex << ".erase(t) > 0) {\n";
        for (std::size_t i = 0; i < numIndexes; i++) {
            if (i != masterIndex && provenanceIndexNumbers.find(i) == provenanceIndexNumbers.end()) {
                def << "ind_" << i << ".erase(t);\n";
            }
        }
        def << "return true;\n";
        def << "} else return false;\n";
        def << "}\n";  // end of erase(t_tuple&)
    }

    // insert methods
    decl << "bool insert(const t_tuple& t);\n";

    def << "bool Type::insert(const t_tuple& t) {\n";
    def << "context h;\n";
    def << "return insert(t, h);\n";
    def << "}\n";  // end of insert(t_tuple&)

    decl << "bool insert(const t_tuple& t, context& h);\n";
    def << "bool Type::insert(const t_tuple& t, context& h) {\n";
    def << "if (ind_" << masterIndex << ".insert(t, h.hints_" << masterIndex << "_lower"
        << ")) {\n";
    for (std::size_t i = 0; i < numIndexes; i++) {
        if (i != masterIndex && provenanceIndexNumbers.find(i) == provenanceIndexNumbers.end()) {
            def << "ind_" << i << ".insert(t, h.hints_" << i << "_lower"
                << ");\n";
        }
    }
    def << "return true;\n";
    def << "} else return false;\n";
    def << "}\n";  // end of insert(t_tuple&, context&)

    decl << "bool insert(const RamDomain* ramDomain);\n";
    def << "bool Type::insert(const RamDomain* ramDomain) {\n";
    def << "RamDomain data[" << arity << "];\n";
    def << "std::copy(ramDomain, ramDomain + " << arity << ", data);\n";
    def << "const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);\n";
    def << "context h;\n";
    def << "return insert(tuple, h);\n";
    def << "}\n";  // end of insert(RamDomain*)

    std::vector<std::string> decls;
    std::vector<std::string> params;
    for (std::size_t i = 0; i < arity; i++) {
        decls.push_back("RamDomain a" + std::to_string(i));
        params.push_back("a" + std::to_string(i));
    }
    decl << "bool insert(" << join(decls, ",") << ");\n";

    def << "bool Type::insert(" << join(decls, ",") << ") {\n";
    def << "RamDomain data[" << arity << "] = {" << join(params, ",") << "};\n";
    def << "return insert(data);\n";
    def << "}\n";  // end of insert(RamDomain x1, RamDomain x2, ...)

    // contains methods
    decl << "bool contains(const t_tuple& t, context& h) const;\n";
    def << "bool Type::contains(const t_tuple& t, context& h) const {\n";
    def << "return ind_" << masterIndex << ".contains(t, h.hints_" << masterIndex << "_lower"
        << ");\n";
    def << "}\n";

    decl << "bool contains(const t_tuple& t) const;\n";
    def << "bool Type::contains(const t_tuple& t) const {\n";
    def << "context h;\n";
    def << "return contains(t, h);\n";
    def << "}\n";

    // size method
    decl << "std::size_t size() const;\n";
    def << "std::size_t Type::size() const {\n";
    def << "return ind_" << masterIndex << ".size();\n";
    def << "}\n";

    // find methods
    decl << "iterator find(const t_tuple& t, context& h) const;\n";
    def << "iterator Type::find(const t_tuple& t, context& h) const {\n";
    def << "return ind_" << masterIndex << ".find(t, h.hints_" << masterIndex << "_lower"
        << ");\n";
    def << "}\n";

    decl << "iterator find(const t_tuple& t) const;\n";
    def << "iterator Type::find(const t_tuple& t) const {\n";
    def << "context h;\n";
    def << "return find(t, h);\n";
    def << "}\n";

    // empty lowerUpperRange method
    decl << "range<iterator> lowerUpperRange_" << SearchSignature(arity)
         << "(const t_tuple& /* lower */, const t_tuple& /* upper */, context& /* h */) const;\n";
    def << "range<iterator> Type::lowerUpperRange_" << SearchSignature(arity)
        << "(const t_tuple& /* lower */, const t_tuple& /* upper */, context& /* h */) const "
           "{\n";

    def << "return range<iterator>(ind_" << masterIndex << ".begin(),ind_" << masterIndex << ".end());\n";
    def << "}\n";

    decl << "range<iterator> lowerUpperRange_" << SearchSignature(arity)
         << "(const t_tuple& /* lower */, const t_tuple& /* upper */) const;\n";
    def << "range<iterator> Type::lowerUpperRange_" << SearchSignature(arity)
        << "(const t_tuple& /* lower */, const t_tuple& /* upper */) const {\n";

    def << "return range<iterator>(ind_" << masterIndex << ".begin(),ind_" << masterIndex << ".end());\n";
    def << "}\n";

    // lowerUpperRange methods for each pattern which is used to search this relation
    for (auto search : indexSelection.getSearches()) {
        auto& lexOrder = indexSelection.getLexOrder(search);
        std::size_t indNum = indexToNumMap[lexOrder];

        decl << "range<t_ind_" << indNum << "::iterator> lowerUpperRange_" << search;
        decl << "(const t_tuple& lower, const t_tuple& upper, context& h) const;\n";
        def << "range<t_ind_" << indNum << "::iterator> Type::lowerUpperRange_" << search;
        def << "(const t_tuple& lower, const t_tuple& upper, context& h) const {\n";

        // count size of search pattern
        std::size_t eqSize = 0;
        for (std::size_t column = 0; column < arity; column++) {
            if (search[column] == analysis::AttributeConstraint::Equal) {
                eqSize++;
            }
        }

        def << "t_comparator_" << indNum << " comparator;\n";
        def << "int cmp = comparator(lower, upper);\n";

        // if search signature is full we can apply this specialization
        if (eqSize == arity) {
            // use the more efficient find() method if lower == upper
            def << "if (cmp == 0) {\n";
            def << "    auto pos = ind_" << indNum << ".find(lower, h.hints_" << indNum << "_lower);\n";
            def << "    auto fin = ind_" << indNum << ".end();\n";
            def << "    if (pos != fin) {fin = pos; ++fin;}\n";
            def << "    return make_range(pos, fin);\n";
            def << "}\n";
        }
        // if lower_bound > upper_bound then we return an empty range
        def << "if (cmp > 0) {\n";
        def << "    return make_range(ind_" << indNum << ".end(), ind_" << indNum << ".end());\n";
        def << "}\n";
        // otherwise use the general method
        def << "return make_range(ind_" << indNum << ".lower_bound(lower, h.hints_" << indNum << "_lower"
            << "), ind_" << indNum << ".upper_bound(upper, h.hints_" << indNum << "_upper"
            << "));\n";

        def << "}\n";

        decl << "range<t_ind_" << indNum << "::iterator> lowerUpperRange_" << search;
        decl << "(const t_tuple& lower, const t_tuple& upper) const;\n";
        def << "range<t_ind_" << indNum << "::iterator> Type::lowerUpperRange_" << search;
        def << "(const t_tuple& lower, const t_tuple& upper) const {\n";

        def << "context h;\n";
        def << "return lowerUpperRange_" << search << "(lower,upper,h);\n";
        def << "}\n";
    }

    // empty method
    decl << "bool empty() const;\n";
    def << "bool Type::empty() const {\n";
    def << "return ind_" << masterIndex << ".empty();\n";
    def << "}\n";

    // partition method for parallelism
    decl << "std::vector<range<iterator>> partition() const;\n";
    def << "std::vector<range<iterator>> Type::partition() const {\n";
    def << "return ind_" << masterIndex << ".getChunks(400);\n";
    def << "}\n";

    // purge method
    decl << "void purge();\n";
    def << "void Type::purge() {\n";
    for (std::size_t i = 0; i < numIndexes; i++) {
        def << "ind_" << i << ".clear();\n";
    }
    def << "}\n";

    // begin and end iterators
    decl << "iterator begin() const;\n";
    def << "iterator Type::begin() const {\n";
    def << "return ind_" << masterIndex << ".begin();\n";
    def << "}\n";

    decl << "iterator end() const;\n";
    def << "iterator Type::end() const {\n";
    def << "return ind_" << masterIndex << ".end();\n";
    def << "}\n";

    // copyIndex method
    if (!provenanceIndexNumbers.empty()) {
        decl << "void copyIndex();\n";
        def << "void Type::copyIndex() {\n";
        def << "for (auto const &cur : ind_" << masterIndex << ") {\n";
        for (auto const i : provenanceIndexNumbers) {
            def << "ind_" << i << ".insert(cur);\n";
        }
        def << "}\n";
        def << "}\n";
    }

    // printStatistics method
    decl << "void printStatistics(std::ostream& o) const;\n";
    def << "void Type::printStatistics(std::ostream& o) const {\n";
    for (std::size_t i = 0; i < numIndexes; i++) {
        def << "o << \" arity " << arity << " direct b-tree index " << i << " lex-order " << inds[i]
            << "\\n\";\n";
        def << "ind_" << i << ".printStats(o);\n";
    }
    def << "}\n";

    // end struct
    decl << "};\n";

}  // namespace souffle

// -------- Indirect Indexed B-Tree Relation --------

/** Generate index set for a indirect indexed relation */
void IndirectRelation::computeIndices() {
    // Generate and set indices
    auto inds = indexSelection.getAllOrders();

    // generate a full index if no indices exist
    assert(!inds.empty() && "no full index in relation");

    // check for full index
    for (std::size_t i = 0; i < inds.size(); i++) {
        auto& ind = inds[i];
        if (ind.size() == getArity()) {
            masterIndex = i;
            break;
        }
    }
    assert(masterIndex < inds.size() && "no full index in relation");
    computedIndices = inds;
}

/** Generate type name of a indirect indexed relation */
std::string IndirectRelation::getTypeNamespace() {
    // collect all attributes used in the lex-order
    std::unordered_set<std::size_t> attributesUsed;
    for (auto& ind : getIndices()) {
        for (auto& attr : ind) {
            attributesUsed.insert(attr);
        }
    }

    std::stringstream res;
    res << "t_btree_" << getTypeAttributeString(relation.getAttributeTypes(), attributesUsed);

    for (auto& ind : getIndices()) {
        res << "__" << join(ind, "_");
    }

    for (auto& search : indexSelection.getSearches()) {
        res << "__" << search;
    }

    return res.str();
}

std::string IndirectRelation::getTypeName() {
    return getTypeNamespace() + "::Type";
}

/** Generate type struct of a indirect indexed relation */
void IndirectRelation::generateTypeStruct(GenDb& db) {
    std::size_t arity = getArity();
    const auto& inds = getIndices();
    auto types = relation.getAttributeTypes();
    std::size_t numIndexes = inds.size();
    std::map<LexOrder, std::size_t> indexToNumMap;

    fs::path basename(uniqueCppIdent(getTypeNamespace(), 20));
    GenDatastructure& cl = db.getDatastructure("Type", basename, std::make_optional(getTypeNamespace()));

    std::ostream& decl = cl.decl();
    std::ostream& def = cl.def();

    cl.addInclude("\"souffle/SouffleInterface.h\"");
    cl.addInclude("\"souffle/datastructure/Table.h\"");
    cl.addInclude("\"souffle/datastructure/BTree.h\"");

    // struct definition
    decl << "struct Type {\n";
    decl << "static constexpr Relation::arity_type Arity = " << arity << ";\n";

    // stored tuple type
    decl << "using t_tuple = Tuple<RamDomain, " << arity << ">;\n";

    // table and lock required for storing actual data for indirect indices
    decl << "Table<t_tuple> dataTable;\n";
    decl << "Lock insert_lock;\n";

    // btree types
    for (std::size_t i = 0; i < inds.size(); i++) {
        auto ind = inds[i];

        if (i < indexSelection.getAllOrders().size()) {
            indexToNumMap[indexSelection.getAllOrders()[i]] = i;
        }

        std::vector<std::string> typecasts;
        typecasts.reserve(types.size());

        for (auto type : types) {
            switch (type[0]) {
                case 'f': typecasts.push_back("ramBitCast<RamFloat>"); break;
                case 'u': typecasts.push_back("ramBitCast<RamUnsigned>"); break;
                default: typecasts.push_back("ramBitCast<RamSigned>");
            }
        }

        std::string comparator = "t_comparator_" + std::to_string(i);

        decl << "struct " << comparator << "{\n";
        decl << " int operator()(const t_tuple *a, const t_tuple *b) const {\n";
        decl << "  return ";
        std::function<void(std::size_t)> gencmp = [&](std::size_t i) {
            std::size_t attrib = ind[i];
            const auto& typecast = typecasts[attrib];
            decl << "(" << typecast << "((*a)[" << attrib << "]) <" << typecast << " ((*b)[" << attrib
                 << "])) ? -1 : ((" << typecast << "((*a)[" << attrib << "]) > " << typecast << "((*b)["
                 << attrib << "])) ? 1 :(";
            if (i + 1 < ind.size()) {
                gencmp(i + 1);
            } else {
                decl << "0";
            }
            decl << "))";
        };
        gencmp(0);
        decl << ";\n }\n";
        decl << "bool less(const t_tuple *a, const t_tuple *b) const {\n";
        decl << "  return ";
        std::function<void(std::size_t)> genless = [&](std::size_t i) {
            std::size_t attrib = ind[i];
            const auto& typecast = typecasts[attrib];
            decl << typecast << " ((*a)[" << attrib << "]) < " << typecast << "((*b)[" << attrib << "])";
            if (i + 1 < ind.size()) {
                decl << "|| ((" << typecast << "((*a)[" << attrib << "]) == " << typecast << "((*b)["
                     << attrib << "]) && (";
                genless(i + 1);
                decl << ")))";
            }
        };
        genless(0);
        decl << ";\n }\n";
        decl << "bool equal(const t_tuple *a, const t_tuple *b) const {\n";
        decl << "return ";
        std::function<void(std::size_t)> geneq = [&](std::size_t i) {
            std::size_t attrib = ind[i];
            const auto& typecast = typecasts[attrib];
            decl << typecast << "((*a)[" << attrib << "]) == " << typecast << "((*b)[" << attrib << "])";
            if (i + 1 < ind.size()) {
                decl << "&&";
                geneq(i + 1);
            }
        };
        geneq(0);
        decl << ";\n }\n";
        decl << "};\n";

        if (ind.size() == arity) {
            decl << "using t_ind_" << i << " = btree_set<const t_tuple*," << comparator << ">;\n";
        } else {
            decl << "using t_ind_" << i << " = btree_multiset<const t_tuple*," << comparator << ">;\n";
        }

        decl << "t_ind_" << i << " ind_" << i << ";\n";
    }

    // typedef deref iterators
    for (std::size_t i = 0; i < numIndexes; i++) {
        decl << "using iterator_" << i << " = IterDerefWrapper<typename t_ind_" << i << "::iterator>;\n";
        def << "using iterator_" << i << " = Type::iterator_" << i << ";\n";
    }
    decl << "using iterator = iterator_" << masterIndex << ";\n";
    def << "using iterator = Type::iterator;\n";

    // Create a struct storing the context hints for each index
    decl << "struct context {\n";
    for (std::size_t i = 0; i < numIndexes; i++) {
        decl << "t_ind_" << i << "::operation_hints hints_" << i << "_lower;\n";
        decl << "t_ind_" << i << "::operation_hints hints_" << i << "_upper;\n";
    }
    decl << "};\n";

    decl << "context createContext();\n";
    def << "using context = Type::context;\n";
    def << "context Type::createContext() { return context(); }\n";

    // insert methods
    decl << "bool insert(const t_tuple& t);\n";
    def << "bool Type::insert(const t_tuple& t) {\n";
    def << "context h;\n";
    def << "return insert(t, h);\n";
    def << "}\n";

    decl << "bool insert(const t_tuple& t, context& h);\n";

    def << "bool Type::insert(const t_tuple& t, context& h) {\n";
    def << "const t_tuple* masterCopy = nullptr;\n";
    def << "{\n";
    def << "auto lease = insert_lock.acquire();\n";
    def << "if (contains(t, h)) return false;\n";
    def << "masterCopy = &dataTable.insert(t);\n";
    def << "ind_" << masterIndex << ".insert(masterCopy, h.hints_" << masterIndex << "_lower);\n";
    def << "}\n";
    for (std::size_t i = 0; i < numIndexes; i++) {
        if (i != masterIndex) {
            def << "ind_" << i << ".insert(masterCopy, h.hints_" << i << "_lower"
                << ");\n";
        }
    }
    def << "return true;\n";
    def << "}\n";

    decl << "bool insert(const RamDomain* ramDomain);\n";
    def << "bool Type::insert(const RamDomain* ramDomain) {\n";
    def << "RamDomain data[" << arity << "];\n";
    def << "std::copy(ramDomain, ramDomain + " << arity << ", data);\n";
    def << "const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);\n";
    def << "context h;\n";
    def << "return insert(tuple, h);\n";
    def << "}\n";  // end of insert(RamDomain*)

    std::vector<std::string> decls;
    std::vector<std::string> params;
    for (std::size_t i = 0; i < arity; i++) {
        decls.push_back("RamDomain a" + std::to_string(i));
        params.push_back("a" + std::to_string(i));
    }
    decl << "bool insert(" << join(decls, ",") << ");\n";
    def << "bool Type::insert(" << join(decls, ",") << ") {\n";
    def << "RamDomain data[" << arity << "] = {" << join(params, ",") << "};\n";
    def << "return insert(data);\n";
    def << "}\n";  // end of insert(RamDomain x1, RamDomain x2, ...)

    // contains methods
    decl << "bool contains(const t_tuple& t, context& h) const;\n";
    def << "bool Type::contains(const t_tuple& t, context& h) const {\n";
    def << "return ind_" << masterIndex << ".contains(&t, h.hints_" << masterIndex << "_lower"
        << ");\n";
    def << "}\n";

    decl << "bool contains(const t_tuple& t) const;\n";
    def << "bool Type::contains(const t_tuple& t) const {\n";
    def << "context h;\n";
    def << "return contains(t, h);\n";
    def << "}\n";

    // size method
    decl << "std::size_t size() const;\n";
    def << "std::size_t Type::size() const {\n";
    def << "return ind_" << masterIndex << ".size();\n";
    def << "}\n";

    // find methods
    decl << "iterator find(const t_tuple& t, context& h) const;\n";
    def << "iterator Type::find(const t_tuple& t, context& h) const {\n";
    def << "return ind_" << masterIndex << ".find(&t, h.hints_" << masterIndex << "_lower"
        << ");\n";
    def << "}\n";

    decl << "iterator find(const t_tuple& t) const;\n";
    def << "iterator Type::find(const t_tuple& t) const {\n";
    def << "context h;\n";
    def << "return find(t, h);\n";
    def << "}\n";

    // empty lowerUpperRange method
    decl << "range<iterator> lowerUpperRange_0(const t_tuple& lower, const t_tuple& upper, context& h) "
            "const;\n";
    def << "range<iterator> Type::lowerUpperRange_0(const t_tuple& lower, const t_tuple& upper, context& h) "
           "const "
           "{\n";
    def << "return range<iterator>(ind_" << masterIndex << ".begin(),ind_" << masterIndex << ".end());\n";
    def << "}\n";

    decl << "range<iterator> lowerUpperRange_0(const t_tuple& lower, const t_tuple& upper) const;\n";
    def << "range<iterator> Type::lowerUpperRange_0(const t_tuple& lower, const t_tuple& upper) const {\n";
    def << "return range<iterator>(ind_" << masterIndex << ".begin(),ind_" << masterIndex << ".end());\n";
    def << "}\n";

    // lowerUpperRange methods for each pattern which is used to search this relation
    for (auto search : indexSelection.getSearches()) {
        auto& lexOrder = indexSelection.getLexOrder(search);
        std::size_t indNum = indexToNumMap[lexOrder];

        decl << "range<iterator_" << indNum << "> lowerUpperRange_" << search;
        decl << "(const t_tuple& lower, const t_tuple& upper, context& h) const;\n";
        def << "range<iterator_" << indNum << "> Type::lowerUpperRange_" << search;
        def << "(const t_tuple& lower, const t_tuple& upper, context& h) const {\n";

        // count size of search pattern
        std::size_t eqSize = 0;
        for (std::size_t column = 0; column < arity; column++) {
            if (search[column] == analysis::AttributeConstraint::Equal) {
                eqSize++;
            }
        }

        def << "t_comparator_" << indNum << " comparator;\n";
        def << "int cmp = comparator(&lower, &upper);\n";

        // use the more efficient find() method if the search pattern is full
        if (eqSize == arity) {
            // if lower == upper we can just do a find
            def << "if (cmp == 0) {\n";
            def << "    auto pos = ind_" << indNum << ".find(&lower, h.hints_" << indNum << "_lower);\n";
            def << "    auto fin = ind_" << indNum << ".end();\n";
            def << "    if (pos != fin) {fin = pos; ++fin;}\n";
            def << "    return range<iterator_" << indNum << ">(pos, fin);\n";
            def << "}\n";
        }
        // if lower > upper then we have an empty range
        def << "if (cmp > 0) {\n";
        def << "    return range<iterator_" << indNum << ">(ind_" << indNum << ".end(), ind_" << indNum
            << ".end());\n";
        def << "}\n";

        // otherwise do the default method
        def << "return range<iterator_" << indNum << ">(ind_" << indNum << ".lower_bound(&lower, h.hints_"
            << indNum << "_lower"
            << "), ind_" << indNum << ".upper_bound(&upper, h.hints_" << indNum << "_upper"
            << "));\n";

        def << "}\n";

        decl << "range<iterator_" << indNum << "> lowerUpperRange_" << search;
        decl << "(const t_tuple& lower, const t_tuple& upper) const;\n";
        def << "range<iterator_" << indNum << "> Type::lowerUpperRange_" << search;
        def << "(const t_tuple& lower, const t_tuple& upper) const {\n";

        def << "context h;\n";
        def << "return lowerUpperRange_" << search << "(lower, upper, h);\n";
        def << "}\n";
    }

    // empty method
    decl << "bool empty() const;\n";
    def << "bool Type::empty() const {\n";
    def << "return ind_" << masterIndex << ".empty();\n";
    def << "}\n";

    // partition method
    decl << "std::vector<range<iterator>> partition() const;\n";
    def << "std::vector<range<iterator>> Type::partition() const {\n";
    def << "std::vector<range<iterator>> res;\n";
    def << "for (const auto& cur : ind_" << masterIndex << ".getChunks(400)) {\n";
    def << "    res.push_back(make_range(derefIter(cur.begin()), derefIter(cur.end())));\n";
    def << "}\n";
    def << "return res;\n";
    def << "}\n";

    // purge method
    decl << "void purge();\n";
    def << "void Type::purge() {\n";
    for (std::size_t i = 0; i < numIndexes; i++) {
        def << "ind_" << i << ".clear();\n";
    }
    def << "dataTable.clear();\n";
    def << "}\n";

    // begin and end iterators
    decl << "iterator begin() const;\n";
    def << "iterator Type::begin() const {\n";
    def << "return ind_" << masterIndex << ".begin();\n";
    def << "}\n";

    decl << "iterator end() const;\n";
    def << "iterator Type::end() const {\n";
    def << "return ind_" << masterIndex << ".end();\n";
    def << "}\n";

    // printStatistics method
    decl << "void printStatistics(std::ostream& o) const;\n";
    def << "void Type::printStatistics(std::ostream& o) const {\n";
    for (std::size_t i = 0; i < numIndexes; i++) {
        def << "o << \" arity " << arity << " indirect b-tree index " << i << " lex-order " << inds[i]
            << "\\n\";\n";
        def << "ind_" << i << ".printStats(o);\n";
    }
    def << "}\n";

    // end struct
    decl << "};\n";
}

// -------- Brie Relation --------

/** Generate index set for a brie relation */
void BrieRelation::computeIndices() {
    // Generate and set indices
    auto inds = indexSelection.getAllOrders();

    // generate a full index if no indices exist
    assert(!inds.empty() && "No full index in relation");

    // expand all indexes to be full
    for (auto& ind : inds) {
        if (ind.size() != getArity()) {
            // use a set as a cache for fast lookup
            std::set<std::size_t> curIndexElems(ind.begin(), ind.end());

            // expand index to be full
            for (std::size_t i = 0; i < getArity(); i++) {
                if (curIndexElems.find(i) == curIndexElems.end()) {
                    ind.push_back(i);
                }
            }
        }

        assert(ind.size() == getArity() && "index is not a full");
    }
    masterIndex = 0;

    computedIndices = inds;
}

/** Generate type name of a brie relation */
std::string BrieRelation::getTypeNamespace() {
    // collect all attributes used in the lex-order
    std::unordered_set<std::size_t> attributesUsed;
    for (auto& ind : getIndices()) {
        for (auto& attr : ind) {
            attributesUsed.insert(attr);
        }
    }

    std::stringstream res;
    res << "t_brie_" << getTypeAttributeString(relation.getAttributeTypes(), attributesUsed);

    for (auto& ind : getIndices()) {
        res << "__" << join(ind, "_");
    }

    for (auto& search : indexSelection.getSearches()) {
        res << "__" << search;
    }

    return res.str();
}

std::string BrieRelation::getTypeName() {
    return getTypeNamespace() + "::Type";
}

/** Generate type struct of a brie relation */
void BrieRelation::generateTypeStruct(GenDb& db) {
    std::size_t arity = getArity();
    const auto& inds = getIndices();
    std::size_t numIndexes = inds.size();
    std::map<LexOrder, std::size_t> indexToNumMap;

    fs::path basename(uniqueCppIdent(getTypeNamespace(), 20));
    GenDatastructure& cl = db.getDatastructure("Type", basename, std::make_optional(getTypeNamespace()));
    std::ostream& decl = cl.decl();
    std::ostream& def = cl.def();
    cl.addInclude("\"souffle/SouffleInterface.h\"");
    cl.addInclude("\"souffle/datastructure/Brie.h\"");

    // struct definition
    decl << "struct Type {\n";
    decl << "static constexpr Relation::arity_type Arity = " << arity << ";\n";

    // define trie structures
    for (std::size_t i = 0; i < inds.size(); i++) {
        if (i < indexSelection.getAllOrders().size()) {
            indexToNumMap[indexSelection.getAllOrders()[i]] = i;
        }
        decl << "using t_ind_" << i << " = Trie<" << inds[i].size() << ">;\n";
        decl << "t_ind_" << i << " ind_" << i << ";\n";
    }
    decl << "using t_tuple = t_ind_" << masterIndex << "::entry_type;\n";

    // generate auxiliary iterators that use orderOut
    for (std::size_t i = 0; i < numIndexes; i++) {
        // generate auxiliary iterators which orderOut
        decl << "class iterator_" << i << " : public std::iterator<std::forward_iterator_tag, t_tuple> {\n";
        decl << "    using nested_iterator = typename t_ind_" << i << "::iterator;\n";
        decl << "    nested_iterator nested;\n";
        decl << "    t_tuple value;\n";

        decl << "public:\n";
        decl << "    iterator_" << i << "() = default;\n";
        decl << "    iterator_" << i << "(const nested_iterator& iter) : nested(iter), value(orderOut_" << i
             << "(*iter)) {}\n";
        decl << "    iterator_" << i << "(const iterator_" << i << "& other) = default;\n";
        decl << "    iterator_" << i << "& operator=(const iterator_" << i << "& other) = default;\n";

        decl << "    bool operator==(const iterator_" << i << "& other) const {\n";
        decl << "        return nested == other.nested;\n";
        decl << "    }\n";

        decl << "    bool operator!=(const iterator_" << i << "& other) const {\n";
        decl << "        return !(*this == other);\n";
        decl << "    }\n";

        decl << "    const t_tuple& operator*() const {\n";
        decl << "        return value;\n";
        decl << "    }\n";

        decl << "    const t_tuple* operator->() const {\n";
        decl << "        return &value;\n";
        decl << "    }\n";

        decl << "    iterator_" << i << "& operator++() {\n";
        decl << "        ++nested;\n";
        decl << "        value = orderOut_" << i << "(*nested);\n";
        decl << "        return *this;\n";
        decl << "    }\n";
        decl << "};\n";

        def << "using iterator_" << i << " = Type::iterator_" << i << ";\n";
    }
    decl << "using iterator = iterator_" << masterIndex << ";\n";
    def << "using iterator = Type::iterator;\n";

    // hints struct
    decl << "struct context {\n";
    for (std::size_t i = 0; i < numIndexes; i++) {
        decl << "t_ind_" << i << "::op_context hints_" << i << ";\n";
    }
    decl << "};\n";
    def << "using context = Type::context;\n";

    decl << "context createContext();\n";
    def << "context Type::createContext() { return context(); }\n";

    // insert methods
    decl << "bool insert(const t_tuple& t);\n";
    def << "bool Type::insert(const t_tuple& t) {\n";
    def << "context h;\n";
    def << "return insert(t, h);\n";
    def << "}\n";

    decl << "bool insert(const t_tuple& t, context& h);\n";
    def << "bool Type::insert(const t_tuple& t, context& h) {\n";
    def << "if (ind_" << masterIndex << ".insert(orderIn_" << masterIndex << "(t), h.hints_" << masterIndex
        << ")) {\n";
    for (std::size_t i = 0; i < numIndexes; i++) {
        if (i != masterIndex) {
            def << "ind_" << i << ".insert(orderIn_" << i << "(t), h.hints_" << i << ");\n";
        }
    }
    def << "return true;\n";
    def << "} else return false;\n";
    def << "}\n";

    decl << "bool insert(const RamDomain* ramDomain);\n";
    def << "bool Type::insert(const RamDomain* ramDomain) {\n";
    def << "RamDomain data[" << arity << "];\n";
    def << "std::copy(ramDomain, ramDomain + " << arity << ", data);\n";
    def << "const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);\n";
    def << "context h;\n";
    def << "return insert(tuple, h);\n";
    def << "}\n";

    // insert method
    std::vector<std::string> decls;
    std::vector<std::string> params;
    for (std::size_t i = 0; i < arity; i++) {
        decls.push_back("RamDomain a" + std::to_string(i));
        params.push_back("a" + std::to_string(i));
    }
    decl << "bool insert(" << join(decls, ",") << ");\n";
    def << "bool Type::insert(" << join(decls, ",") << ") {\nRamDomain data[";
    def << arity << "] = {" << join(params, ",") << "};\n";
    def << "return insert(data);\n";
    def << "}\n";

    // contains methods
    decl << "bool contains(const t_tuple& t, context& h) const;\n";
    def << "bool Type::contains(const t_tuple& t, context& h) const {\n";
    def << "return ind_" << masterIndex << ".contains(orderIn_" << masterIndex << "(t), h.hints_"
        << masterIndex << ");\n";
    def << "}\n";

    decl << "bool contains(const t_tuple& t) const;\n";
    def << "bool Type::contains(const t_tuple& t) const {\n";
    def << "context h;\n";
    def << "return contains(t, h);\n";
    def << "}\n";

    // size method
    decl << "std::size_t size() const;\n";
    def << "std::size_t Type::size() const {\n";
    def << "return ind_" << masterIndex << ".size();\n";
    def << "}\n";

    // find methods
    if (arity > 1) {
        decl << "iterator find(const t_tuple& t, context& h) const;\n";
        def << "iterator Type::find(const t_tuple& t, context& h) const {\n";
        def << "return ind_" << masterIndex << ".find(orderIn_" << masterIndex << "(t), h.hints_"
            << masterIndex << ");\n";
        def << "}\n";

        decl << "iterator find(const t_tuple& t) const;\n";
        def << "iterator Type::find(const t_tuple& t) const {\n";
        def << "context h;\n";
        def << "return find(t, h);\n";
        def << "}\n";
    }

    // empty lowerUpperRange method
    decl << "range<iterator> lowerUpperRange_0(const t_tuple& lower, const t_tuple& upper, context& h) "
            "const;\n";
    def << "range<iterator> Type::lowerUpperRange_0(const t_tuple& lower, const t_tuple& upper, context& h)"
        << " const {\n";
    def << "return range<iterator>(ind_" << masterIndex << ".begin(),ind_" << masterIndex << ".end());\n";
    def << "}\n";

    decl << "range<iterator> lowerUpperRange_0(const t_tuple& lower, const t_tuple& upper) const;\n";
    def << "range<iterator> Type::lowerUpperRange_0(const t_tuple& lower, const t_tuple& upper) const {\n";
    def << "return range<iterator>(ind_" << masterIndex << ".begin(),ind_" << masterIndex << ".end());\n";
    def << "}\n";

    // loweUpperRange methods
    for (auto search : indexSelection.getSearches()) {
        auto& lexOrder = indexSelection.getLexOrder(search);
        std::size_t indNum = indexToNumMap[lexOrder];

        decl << "range<iterator_" << indNum << "> lowerUpperRange_" << search;
        decl << "(const t_tuple& lower, const t_tuple& upper, context& h) const;\n";
        def << "range<iterator_" << indNum << "> Type::lowerUpperRange_" << search;
        def << "(const t_tuple& lower, const t_tuple& upper, context& h) const {\n";

        // compute size of sub-index
        std::size_t indSize = 0;
        for (std::size_t i = 0; i < arity; i++) {
            if (search[i] != analysis::AttributeConstraint::None) {
                indSize++;
            }
        }

        def << "auto r = ind_" << indNum << ".template getBoundaries<" << indSize << ">(orderIn_" << indNum
            << "(lower), h.hints_" << indNum << ");\n";
        def << "return make_range(iterator_" << indNum << "(r.begin()), iterator_" << indNum
            << "(r.end()));\n";
        def << "}\n";

        decl << "range<iterator_" << indNum << "> lowerUpperRange_" << search;
        decl << "(const t_tuple& lower, const t_tuple& upper) const;\n";
        def << "range<iterator_" << indNum << "> Type::lowerUpperRange_" << search;
        def << "(const t_tuple& lower, const t_tuple& upper) const {\n";
        def << "context h; return lowerUpperRange_" << search << "(lower,upper, h);\n";
        def << "}\n";
    }

    // empty method
    decl << "bool empty() const;\n";
    def << "bool Type::empty() const {\n";
    def << "return ind_" << masterIndex << ".empty();\n";
    def << "}\n";

    // partition method
    decl << "std::vector<range<iterator>> partition() const;\n";
    def << "std::vector<range<iterator>> Type::partition() const {\n";
    def << "std::vector<range<iterator>> res;\n";
    def << "for (const auto& cur : ind_" << masterIndex << ".partition(10000)) {\n";
    def << "    res.push_back(make_range(iterator(cur.begin()), iterator(cur.end())));\n";
    def << "}\n";
    def << "return res;\n";
    def << "}\n";

    // purge method
    decl << "void purge();\n";
    def << "void Type::purge() {\n";
    for (std::size_t i = 0; i < numIndexes; i++) {
        def << "ind_" << i << ".clear();\n";
    }
    def << "}\n";

    // begin and end iterators
    decl << "iterator begin() const;\n";
    def << "iterator Type::begin() const {\n";
    def << "return iterator_" << masterIndex << "(ind_" << masterIndex << ".begin());\n";
    def << "}\n";

    decl << "iterator end() const;\n";
    def << "iterator Type::end() const {\n";
    def << "return iterator_" << masterIndex << "(ind_" << masterIndex << ".end());\n";
    def << "}\n";

    // TODO: finish printStatistics method
    decl << "void printStatistics(std::ostream& o) const;\n";
    def << "void Type::printStatistics(std::ostream& o) const {\n";
    for (std::size_t i = 0; i < numIndexes; i++) {
        def << "o << \" arity " << arity << " brie index " << i << " lex-order " << inds[i] << "\\n\";\n";
        def << "ind_" << i << ".printStats(o);\n";
    }
    def << "}\n";

    // orderOut and orderIn methods for reordering tuples according to index orders
    for (std::size_t i = 0; i < numIndexes; i++) {
        auto ind = inds[i];
        decl << "static t_tuple orderIn_" << i << "(const t_tuple& t) {\n";
        decl << "t_tuple res;\n";
        for (std::size_t j = 0; j < ind.size(); j++) {
            decl << "res[" << j << "] = t[" << ind[j] << "];\n";
        }
        decl << "return res;\n";
        decl << "}\n";

        decl << "static t_tuple orderOut_" << i << "(const t_tuple& t) {\n";
        decl << "t_tuple res;\n";
        for (std::size_t j = 0; j < ind.size(); j++) {
            decl << "res[" << ind[j] << "] = t[" << j << "];\n";
        }
        decl << "return res;\n";
        decl << "}\n";
    }

    // end class
    decl << "};\n";
}

// -------- Eqrel Relation --------

/** Generate index set for a eqrel relation, which should be empty */
void EqrelRelation::computeIndices() {
    computedIndices = {};
}

/** Generate type name of a eqrel relation */
std::string EqrelRelation::getTypeName() {
    return "t_eqrel";
}

/** Generate type struct of a eqrel relation, which is empty,
 * the actual implementation is in EqRel.h */
void EqrelRelation::generateTypeStruct(GenDb& db) {
    db.datastructureIncludes(getTypeName(), "\"souffle/datastructure/EqRel.h\"");
    return;
}

}  // namespace souffle::synthesiser

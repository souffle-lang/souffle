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

    // Handle the qualifier in souffle code
    if (ramRel.getRepresentation() == RelationRepresentation::PROVENANCE) {
        rel = new DirectRelation(ramRel, indexSelection, true, false, false);
    } else if (ramRel.getRepresentation() == RelationRepresentation::INCREMENTAL) {
        rel = new DirectRelation(ramRel, indexSelection, false, true, false);
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
 * the actual implementation is in CompiledSouffle.h */
void InfoRelation::generateTypeStruct(std::ostream&) {
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
 * the actual implementation is in CompiledSouffle.h */
void NullaryRelation::generateTypeStruct(std::ostream&) {
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
        if (isProvenance || hasErase) {
            // expand index to be full
            for (std::size_t i = 0; i < getArity() - relation.getAuxiliaryArity(); i++) {
                if (curIndexElems.find(i) == curIndexElems.end()) {
                    ind.push_back(i);
                }
            }

            if (isProvenance) {
                // remove any provenance annotations already in the index order
                if (curIndexElems.find(getArity() - relation.getAuxiliaryArity() + 1) !=
                        curIndexElems.end()) {
                    ind.erase(
                            std::find(ind.begin(), ind.end(), getArity() - relation.getAuxiliaryArity() + 1));
                }

                if (curIndexElems.find(getArity() - relation.getAuxiliaryArity()) != curIndexElems.end()) {
                    ind.erase(std::find(ind.begin(), ind.end(), getArity() - relation.getAuxiliaryArity()));
                }

                // add provenance annotations to the index, but in reverse order
                ind.push_back(getArity() - relation.getAuxiliaryArity() + 1);
                ind.push_back(getArity() - relation.getAuxiliaryArity());
            }
            masterIndex = 0;
        } else if (isIncremental) {
            // expand index to be full
            for (std::size_t i = 0; i < getArity() - relation.getAuxiliaryArity(); i++) {
                if (curIndexElems.find(i) == curIndexElems.end()) {
                    ind.push_back(i);
                }
            }
            /*
            // remove any provenance annotations already in the index order
            if (curIndexElems.find(getArity() - relation.getAuxiliaryArity() + 1) != curIndexElems.end()) {
                ind.erase(std::find(ind.begin(), ind.end(), getArity() - relation.getAuxiliaryArity() + 1));
            }
            */

            if (curIndexElems.find(getArity() - relation.getAuxiliaryArity()) != curIndexElems.end()) {
                ind.erase(std::find(ind.begin(), ind.end(), getArity() - relation.getAuxiliaryArity()));
            }

            // add provenance annotations to the index, but in reverse order
            ind.push_back(getArity() - relation.getAuxiliaryArity());
            // ind.push_back(getArity() - relation.getAuxiliaryArity() + 1);
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
std::string DirectRelation::getTypeName() {
    // collect all attributes used in the lex-order
    std::unordered_set<std::size_t> attributesUsed;
    for (auto& ind : getIndices()) {
        for (auto& attr : ind) {
            attributesUsed.insert(attr);
        }
    }

    std::stringstream res;
    if (hasErase) {
        res << "t_btree_delete_";
    } else {
        res << "t_btree_";
    }
    res << getTypeAttributeString(relation.getAttributeTypes(), attributesUsed);

    for (auto& ind : getIndices()) {
        res << "__" << join(ind, "_");
    }

    for (auto& search : indexSelection.getSearches()) {
        res << "__" << search;
    }

    return res.str();
}

/** Generate type struct of a direct indexed relation */
void DirectRelation::generateTypeStruct(std::ostream& out) {
    std::size_t arity = getArity();
    std::size_t auxiliaryArity = relation.getAuxiliaryArity();
    auto types = relation.getAttributeTypes();
    const auto& inds = getIndices();
    std::size_t numIndexes = inds.size();
    std::map<LexOrder, std::size_t> indexToNumMap;

    // struct definition
    out << "struct " << getTypeName() << " {\n";
    out << "static constexpr Relation::arity_type Arity = " << arity << ";\n";

    // stored tuple type
    out << "using t_tuple = Tuple<RamDomain, " << arity << ">;\n";

    // generate an updater class for provenance
    if (isProvenance) {
        out << "struct updater_" << getTypeName() << " {\n";
        out << "void update(t_tuple& old_t, const t_tuple& new_t) {\n";

        for (std::size_t i = arity - auxiliaryArity; i < arity; i++) {
            out << "old_t[" << i << "] = new_t[" << i << "];\n";
        }

        out << "}\n";
        out << "};\n";
    }

    if (isIncremental) {
        out << "struct updater_" << getTypeName() << " {\n";
        out << "void update(t_tuple& old_t, const t_tuple& new_t) {\n";

        /*
        if (relation.getName().find("applied@") != std::string::npos || relation.getName().find("@") == std::string::npos) {
            // out << "if (new_t[" << arity - 1 << "] == 0 && new_t[" << arity - 2 << "] == 0) {\n";
            out << "if (new_t[" << arity - 1 << "] == 0) {\n";
            // out << "old_t[" << arity - 2 << "] = 0;\n";
            out << "old_t[" << arity - 1 << "] = 0;\n";
            // out << "return true;\n";
            out << "}\n";
        }
        */

        out << "old_t[" << arity - 1 << "] += new_t[" << arity - 1 << "];\n";
        // out << "return true;\n";

        out << "}\n";
        out << "};\n";
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
            out << "struct " << name << "{\n";
            out << " int operator()(const t_tuple& a, const t_tuple& b) const {\n";
            out << "  return ";
            std::function<void(std::size_t)> gencmp = [&](std::size_t i) {
                std::size_t attrib = ind[i];
                const auto& typecast = typecasts[attrib];

                out << "(" << typecast << "(a[" << attrib << "]) < " << typecast << "(b[" << attrib
                    << "])) ? -1 : (" << typecast << "(a[" << attrib << "]) > " << typecast << "(b[" << attrib
                    << "])) ? 1 :(";
                if (i + 1 < bound) {
                    gencmp(i + 1);
                } else {
                    out << "0";
                }
                out << ")";
            };
            gencmp(0);
            out << ";\n }\n";
            out << "bool less(const t_tuple& a, const t_tuple& b) const {\n";
            out << "  return ";
            std::function<void(std::size_t)> genless = [&](std::size_t i) {
                std::size_t attrib = ind[i];
                const auto& typecast = typecasts[attrib];

                out << "(" << typecast << "(a[" << attrib << "]) < " << typecast << "(b[" << attrib << "]))";
                if (i + 1 < bound) {
                    out << "|| ((" << typecast << "(a[" << attrib << "]) == " << typecast << "(b[" << attrib
                        << "])) && (";
                    genless(i + 1);
                    out << "))";
                }
            };
            genless(0);
            out << ";\n }\n";
            out << "bool equal(const t_tuple& a, const t_tuple& b) const {\n";
            out << "return ";
            std::function<void(std::size_t)> geneq = [&](std::size_t i) {
                std::size_t attrib = ind[i];
                const auto& typecast = typecasts[attrib];

                out << "(" << typecast << "(a[" << attrib << "]) == " << typecast << "(b[" << attrib << "]))";
                if (i + 1 < bound) {
                    out << "&&";
                    geneq(i + 1);
                }
            };
            geneq(0);
            out << ";\n }\n";
            out << "};\n";
        };

        std::string comparator = "t_comparator_" + std::to_string(i);
        genstruct(comparator, ind.size());

        // for provenance, all indices must be full so we use btree_set
        // also strong/weak comparators and updater methods

        if (isProvenance) {
            std::string comparator_aux;
            if (provenanceIndexNumbers.find(i) == provenanceIndexNumbers.end()) {
                // index for bottom up phase
                comparator_aux = "t_comparator_" + std::to_string(i) + "_aux";
                genstruct(comparator_aux, ind.size() - auxiliaryArity);
            } else {
                // index for top down phase
                comparator_aux = comparator;
            }
            out << "using t_ind_" << i << " = btree_set<t_tuple," << comparator
                << ",std::allocator<t_tuple>,256,typename "
                   "souffle::detail::default_strategy<t_tuple>::type,"
                << comparator_aux << ",updater_" << getTypeName() << ">;\n";
        } else if (isIncremental) {
            std::string comparator_aux;
            if (provenanceIndexNumbers.find(i) == provenanceIndexNumbers.end()) {
                // index for bottom up phase
                comparator_aux = "t_comparator_" + std::to_string(i) + "_aux";
                genstruct(comparator_aux, ind.size() - auxiliaryArity);
            } else {
                // index for top down phase
                comparator_aux = comparator;
            }
            out << "using t_ind_" << i << " = btree_set<t_tuple," << comparator
                << ",std::allocator<t_tuple>,256,typename "
                   "souffle::detail::default_strategy<t_tuple>::type,"
                << comparator_aux << ",updater_" << getTypeName() << ">;\n";
        } else {
            std::string btree_name = "btree";
            if (hasErase) {
                btree_name = "btree_delete";
            }
            if (ind.size() == arity) {
                out << "using t_ind_" << i << " = " << btree_name << "_set<t_tuple," << comparator << ">;\n";
            } else {
                // without provenance, some indices may be not full, so we use btree_multiset for those
                out << "using t_ind_" << i << " = " << btree_name << "_multiset<t_tuple," << comparator
                    << ">;\n";
            }
        }
        out << "t_ind_" << i << " ind_" << i << ";\n";
    }

    // typedef master index iterator to be struct iterator
    out << "using iterator = t_ind_" << masterIndex << "::iterator;\n";

    // create a struct storing hints for each btree
    out << "struct context {\n";
    for (std::size_t i = 0; i < numIndexes; i++) {
        out << "t_ind_" << i << "::operation_hints hints_" << i << "_lower"
            << ";\n";
        out << "t_ind_" << i << "::operation_hints hints_" << i << "_upper"
            << ";\n";
    }
    out << "};\n";
    out << "context createContext() { return context(); }\n";

    // erase method
    if (hasErase) {
        out << "bool erase(const t_tuple& t) {\n";

        out << "if (ind_" << masterIndex << ".erase(t) > 0) {\n";
        for (std::size_t i = 0; i < numIndexes; i++) {
            if (i != masterIndex && provenanceIndexNumbers.find(i) == provenanceIndexNumbers.end()) {
                out << "ind_" << i << ".erase(t);\n";
            }
        }
        out << "return true;\n";
        out << "} else return false;\n";
        out << "}\n";  // end of erase(t_tuple&)
    }

    // insert methods
    out << "bool insert(const t_tuple& t) {\n";
    out << "context h;\n";
    out << "return insert(t, h);\n";
    out << "}\n";  // end of insert(t_tuple&)

    out << "bool insert(const t_tuple& t, context& h) {\n";
    out << "if (ind_" << masterIndex << ".insert(t, h.hints_" << masterIndex << "_lower"
        << ")) {\n";
    for (std::size_t i = 0; i < numIndexes; i++) {
        if (i != masterIndex && provenanceIndexNumbers.find(i) == provenanceIndexNumbers.end()) {
            out << "ind_" << i << ".insert(t, h.hints_" << i << "_lower"
                << ");\n";
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

    std::vector<std::string> decls;
    std::vector<std::string> params;
    for (std::size_t i = 0; i < arity; i++) {
        decls.push_back("RamDomain a" + std::to_string(i));
        params.push_back("a" + std::to_string(i));
    }
    out << "bool insert(" << join(decls, ",") << ") {\n";
    out << "RamDomain data[" << arity << "] = {" << join(params, ",") << "};\n";
    out << "return insert(data);\n";
    out << "}\n";  // end of insert(RamDomain x1, RamDomain x2, ...)

    // contains methods
    out << "bool contains(const t_tuple& t, context& h) const {\n";
    out << "return ind_" << masterIndex << ".contains(t, h.hints_" << masterIndex << "_lower"
        << ");\n";
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
    out << "return ind_" << masterIndex << ".find(t, h.hints_" << masterIndex << "_lower"
        << ");\n";
    out << "}\n";

    out << "iterator find(const t_tuple& t) const {\n";
    out << "context h;\n";
    out << "return find(t, h);\n";
    out << "}\n";

    // empty lowerUpperRange method
    out << "range<iterator> lowerUpperRange_" << SearchSignature(arity)
        << "(const t_tuple& /* lower */, const t_tuple& /* upper */, context& /* h */) const "
           "{\n";

    out << "return range<iterator>(ind_" << masterIndex << ".begin(),ind_" << masterIndex << ".end());\n";
    out << "}\n";

    out << "range<iterator> lowerUpperRange_" << SearchSignature(arity)
        << "(const t_tuple& /* lower */, const t_tuple& /* upper */) const {\n";

    out << "return range<iterator>(ind_" << masterIndex << ".begin(),ind_" << masterIndex << ".end());\n";
    out << "}\n";

    // lowerUpperRange methods for each pattern which is used to search this relation
    for (auto search : indexSelection.getSearches()) {
        auto& lexOrder = indexSelection.getLexOrder(search);
        std::size_t indNum = indexToNumMap[lexOrder];

        out << "range<t_ind_" << indNum << "::iterator> lowerUpperRange_" << search;
        out << "(const t_tuple& lower, const t_tuple& upper, context& h) const {\n";

        // count size of search pattern
        std::size_t eqSize = 0;
        for (std::size_t column = 0; column < arity; column++) {
            if (search[column] == analysis::AttributeConstraint::Equal) {
                eqSize++;
            }
        }

        out << "t_comparator_" << indNum << " comparator;\n";
        out << "int cmp = comparator(lower, upper);\n";

        // if search signature is full we can apply this specialization
        if (eqSize == arity) {
            // use the more efficient find() method if lower == upper
            out << "if (cmp == 0) {\n";
            out << "    auto pos = ind_" << indNum << ".find(lower, h.hints_" << indNum << "_lower);\n";
            out << "    auto fin = ind_" << indNum << ".end();\n";
            out << "    if (pos != fin) {fin = pos; ++fin;}\n";
            out << "    return make_range(pos, fin);\n";
            out << "}\n";
        }
        // if lower_bound > upper_bound then we return an empty range
        out << "if (cmp > 0) {\n";
        out << "    return make_range(ind_" << indNum << ".end(), ind_" << indNum << ".end());\n";
        out << "}\n";
        // otherwise use the general method
        out << "return make_range(ind_" << indNum << ".lower_bound(lower, h.hints_" << indNum << "_lower"
            << "), ind_" << indNum << ".upper_bound(upper, h.hints_" << indNum << "_upper"
            << "));\n";

        out << "}\n";

        out << "range<t_ind_" << indNum << "::iterator> lowerUpperRange_" << search;
        out << "(const t_tuple& lower, const t_tuple& upper) const {\n";

        out << "context h;\n";
        out << "return lowerUpperRange_" << search << "(lower,upper,h);\n";
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
    for (std::size_t i = 0; i < numIndexes; i++) {
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

    // copyIndex method
    if (!provenanceIndexNumbers.empty()) {
        out << "void copyIndex() {\n";
        out << "for (auto const &cur : ind_" << masterIndex << ") {\n";
        for (auto const i : provenanceIndexNumbers) {
            out << "ind_" << i << ".insert(cur);\n";
        }
        out << "}\n";
        out << "}\n";
    }

    // printStatistics method
    out << "void printStatistics(std::ostream& o) const {\n";
    for (std::size_t i = 0; i < numIndexes; i++) {
        out << "o << \" arity " << arity << " direct b-tree index " << i << " lex-order " << inds[i]
            << "\\n\";\n";
        out << "ind_" << i << ".printStats(o);\n";
    }
    out << "}\n";

    // printRel method
    out << "void printRel(std::string relName) const {\n";
    out << "std::cout << relName << \":\" << std::endl;\n";
    out << "for (auto const &t : ind_" << masterIndex << ") {\n";
    out << "std::cout << t << std::endl;\n";
    out << "}\n";
    out << "}\n";

    // end struct
    out << "};\n";
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
std::string IndirectRelation::getTypeName() {
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

/** Generate type struct of a indirect indexed relation */
void IndirectRelation::generateTypeStruct(std::ostream& out) {
    std::size_t arity = getArity();
    const auto& inds = getIndices();
    auto types = relation.getAttributeTypes();
    std::size_t numIndexes = inds.size();
    std::map<LexOrder, std::size_t> indexToNumMap;

    // struct definition
    out << "struct " << getTypeName() << " {\n";
    out << "static constexpr Relation::arity_type Arity = " << arity << ";\n";

    // stored tuple type
    out << "using t_tuple = Tuple<RamDomain, " << arity << ">;\n";

    // table and lock required for storing actual data for indirect indices
    out << "Table<t_tuple> dataTable;\n";
    out << "Lock insert_lock;\n";

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

        out << "struct " << comparator << "{\n";
        out << " int operator()(const t_tuple *a, const t_tuple *b) const {\n";
        out << "  return ";
        std::function<void(std::size_t)> gencmp = [&](std::size_t i) {
            std::size_t attrib = ind[i];
            const auto& typecast = typecasts[attrib];
            out << "(" << typecast << "((*a)[" << attrib << "]) <" << typecast << " ((*b)[" << attrib
                << "])) ? -1 : ((" << typecast << "((*a)[" << attrib << "]) > " << typecast << "((*b)["
                << attrib << "])) ? 1 :(";
            if (i + 1 < ind.size()) {
                gencmp(i + 1);
            } else {
                out << "0";
            }
            out << "))";
        };
        gencmp(0);
        out << ";\n }\n";
        out << "bool less(const t_tuple *a, const t_tuple *b) const {\n";
        out << "  return ";
        std::function<void(std::size_t)> genless = [&](std::size_t i) {
            std::size_t attrib = ind[i];
            const auto& typecast = typecasts[attrib];
            out << typecast << " ((*a)[" << attrib << "]) < " << typecast << "((*b)[" << attrib << "])";
            if (i + 1 < ind.size()) {
                out << "|| ((" << typecast << "((*a)[" << attrib << "]) == " << typecast << "((*b)[" << attrib
                    << "]) && (";
                genless(i + 1);
                out << ")))";
            }
        };
        genless(0);
        out << ";\n }\n";
        out << "bool equal(const t_tuple *a, const t_tuple *b) const {\n";
        out << "return ";
        std::function<void(std::size_t)> geneq = [&](std::size_t i) {
            std::size_t attrib = ind[i];
            const auto& typecast = typecasts[attrib];
            out << typecast << "((*a)[" << attrib << "]) == " << typecast << "((*b)[" << attrib << "])";
            if (i + 1 < ind.size()) {
                out << "&&";
                geneq(i + 1);
            }
        };
        geneq(0);
        out << ";\n }\n";
        out << "};\n";

        if (ind.size() == arity) {
            out << "using t_ind_" << i << " = btree_set<const t_tuple*," << comparator << ">;\n";
        } else {
            out << "using t_ind_" << i << " = btree_multiset<const t_tuple*," << comparator << ">;\n";
        }

        out << "t_ind_" << i << " ind_" << i << ";\n";
    }

    // typedef deref iterators
    for (std::size_t i = 0; i < numIndexes; i++) {
        out << "using iterator_" << i << " = IterDerefWrapper<typename t_ind_" << i << "::iterator>;\n";
    }
    out << "using iterator = iterator_" << masterIndex << ";\n";

    // Create a struct storing the context hints for each index
    out << "struct context {\n";
    for (std::size_t i = 0; i < numIndexes; i++) {
        out << "t_ind_" << i << "::operation_hints hints_" << i << "_lower;\n";
        out << "t_ind_" << i << "::operation_hints hints_" << i << "_upper;\n";
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
    out << "ind_" << masterIndex << ".insert(masterCopy, h.hints_" << masterIndex << "_lower);\n";
    out << "}\n";
    for (std::size_t i = 0; i < numIndexes; i++) {
        if (i != masterIndex) {
            out << "ind_" << i << ".insert(masterCopy, h.hints_" << i << "_lower"
                << ");\n";
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

    std::vector<std::string> decls;
    std::vector<std::string> params;
    for (std::size_t i = 0; i < arity; i++) {
        decls.push_back("RamDomain a" + std::to_string(i));
        params.push_back("a" + std::to_string(i));
    }
    out << "bool insert(" << join(decls, ",") << ") {\n";
    out << "RamDomain data[" << arity << "] = {" << join(params, ",") << "};\n";
    out << "return insert(data);\n";
    out << "}\n";  // end of insert(RamDomain x1, RamDomain x2, ...)

    // contains methods
    out << "bool contains(const t_tuple& t, context& h) const {\n";
    out << "return ind_" << masterIndex << ".contains(&t, h.hints_" << masterIndex << "_lower"
        << ");\n";
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
    out << "return ind_" << masterIndex << ".find(&t, h.hints_" << masterIndex << "_lower"
        << ");\n";
    out << "}\n";

    out << "iterator find(const t_tuple& t) const {\n";
    out << "context h;\n";
    out << "return find(t, h);\n";
    out << "}\n";

    // empty lowerUpperRange method
    out << "range<iterator> lowerUpperRange_0(const t_tuple& lower, const t_tuple& upper, context& h) const "
           "{\n";

    out << "return range<iterator>(ind_" << masterIndex << ".begin(),ind_" << masterIndex << ".end());\n";
    out << "}\n";

    out << "range<iterator> lowerUpperRange_0(const t_tuple& lower, const t_tuple& upper) const {\n";

    out << "return range<iterator>(ind_" << masterIndex << ".begin(),ind_" << masterIndex << ".end());\n";
    out << "}\n";

    // lowerUpperRange methods for each pattern which is used to search this relation
    for (auto search : indexSelection.getSearches()) {
        auto& lexOrder = indexSelection.getLexOrder(search);
        std::size_t indNum = indexToNumMap[lexOrder];

        out << "range<iterator_" << indNum << "> lowerUpperRange_" << search;
        out << "(const t_tuple& lower, const t_tuple& upper, context& h) const {\n";

        // count size of search pattern
        std::size_t eqSize = 0;
        for (std::size_t column = 0; column < arity; column++) {
            if (search[column] == analysis::AttributeConstraint::Equal) {
                eqSize++;
            }
        }

        out << "t_comparator_" << indNum << " comparator;\n";
        out << "int cmp = comparator(&lower, &upper);\n";

        // use the more efficient find() method if the search pattern is full
        if (eqSize == arity) {
            // if lower == upper we can just do a find
            out << "if (cmp == 0) {\n";
            out << "    auto pos = ind_" << indNum << ".find(&lower, h.hints_" << indNum << "_lower);\n";
            out << "    auto fin = ind_" << indNum << ".end();\n";
            out << "    if (pos != fin) {fin = pos; ++fin;}\n";
            out << "    return range<iterator_" << indNum << ">(pos, fin);\n";
            out << "}\n";
        }
        // if lower > upper then we have an empty range
        out << "if (cmp > 0) {\n";
        out << "    return range<iterator_" << indNum << ">(ind_" << indNum << ".end(), ind_" << indNum
            << ".end());\n";
        out << "}\n";

        // otherwise do the default method
        out << "return range<iterator_" << indNum << ">(ind_" << indNum << ".lower_bound(&lower, h.hints_"
            << indNum << "_lower"
            << "), ind_" << indNum << ".upper_bound(&upper, h.hints_" << indNum << "_upper"
            << "));\n";

        out << "}\n";

        out << "range<iterator_" << indNum << "> lowerUpperRange_" << search;
        out << "(const t_tuple& lower, const t_tuple& upper) const {\n";

        out << "context h;\n";
        out << "return lowerUpperRange_" << search << "(lower, upper, h);\n";
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
    for (std::size_t i = 0; i < numIndexes; i++) {
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

    // printStatistics method
    out << "void printStatistics(std::ostream& o) const {\n";
    for (std::size_t i = 0; i < numIndexes; i++) {
        out << "o << \" arity " << arity << " indirect b-tree index " << i << " lex-order " << inds[i]
            << "\\n\";\n";
        out << "ind_" << i << ".printStats(o);\n";
    }
    out << "}\n";

    // end struct
    out << "};\n";
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
std::string BrieRelation::getTypeName() {
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

/** Generate type struct of a brie relation */
void BrieRelation::generateTypeStruct(std::ostream& out) {
    std::size_t arity = getArity();
    const auto& inds = getIndices();
    std::size_t numIndexes = inds.size();
    std::map<LexOrder, std::size_t> indexToNumMap;

    // struct definition
    out << "struct " << getTypeName() << " {\n";
    out << "static constexpr Relation::arity_type Arity = " << arity << ";\n";

    // define trie structures
    for (std::size_t i = 0; i < inds.size(); i++) {
        if (i < indexSelection.getAllOrders().size()) {
            indexToNumMap[indexSelection.getAllOrders()[i]] = i;
        }
        out << "using t_ind_" << i << " = Trie<" << inds[i].size() << ">;\n";
        out << "t_ind_" << i << " ind_" << i << ";\n";
    }
    out << "using t_tuple = t_ind_" << masterIndex << "::entry_type;\n";

    // generate auxiliary iterators that use orderOut
    for (std::size_t i = 0; i < numIndexes; i++) {
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
    for (std::size_t i = 0; i < numIndexes; i++) {
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
    for (std::size_t i = 0; i < numIndexes; i++) {
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

    // insert method
    std::vector<std::string> decls;
    std::vector<std::string> params;
    for (std::size_t i = 0; i < arity; i++) {
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

    // empty lowerUpperRange method
    out << "range<iterator> lowerUpperRange_0(const t_tuple& lower, const t_tuple& upper, context& h) const "
           "{\n";
    out << "return range<iterator>(ind_" << masterIndex << ".begin(),ind_" << masterIndex << ".end());\n";
    out << "}\n";

    out << "range<iterator> lowerUpperRange_0(const t_tuple& lower, const t_tuple& upper) const {\n";
    out << "return range<iterator>(ind_" << masterIndex << ".begin(),ind_" << masterIndex << ".end());\n";
    out << "}\n";

    // loweUpperRange methods
    for (auto search : indexSelection.getSearches()) {
        auto& lexOrder = indexSelection.getLexOrder(search);
        std::size_t indNum = indexToNumMap[lexOrder];

        out << "range<iterator_" << indNum << "> lowerUpperRange_" << search;
        out << "(const t_tuple& lower, const t_tuple& upper, context& h) const {\n";

        // compute size of sub-index
        std::size_t indSize = 0;
        for (std::size_t i = 0; i < arity; i++) {
            if (search[i] != analysis::AttributeConstraint::None) {
                indSize++;
            }
        }

        out << "auto r = ind_" << indNum << ".template getBoundaries<" << indSize << ">(orderIn_" << indNum
            << "(lower), h.hints_" << indNum << ");\n";
        out << "return make_range(iterator_" << indNum << "(r.begin()), iterator_" << indNum
            << "(r.end()));\n";
        out << "}\n";

        out << "range<iterator_" << indNum << "> lowerUpperRange_" << search;
        out << "(const t_tuple& lower, const t_tuple& upper) const {\n";
        out << "context h; return lowerUpperRange_" << search << "(lower,upper, h);\n";
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
    for (std::size_t i = 0; i < numIndexes; i++) {
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

    // TODO: finish printStatistics method
    out << "void printStatistics(std::ostream& o) const {\n";
    for (std::size_t i = 0; i < numIndexes; i++) {
        out << "o << \" arity " << arity << " brie index " << i << " lex-order " << inds[i] << "\\n\";\n";
        ;
        out << "ind_" << i << ".printStats(o);\n";
    }
    out << "}\n";

    // orderOut and orderIn methods for reordering tuples according to index orders
    for (std::size_t i = 0; i < numIndexes; i++) {
        auto ind = inds[i];
        out << "static t_tuple orderIn_" << i << "(const t_tuple& t) {\n";
        out << "t_tuple res;\n";
        for (std::size_t j = 0; j < ind.size(); j++) {
            out << "res[" << j << "] = t[" << ind[j] << "];\n";
        }
        out << "return res;\n";
        out << "}\n";

        out << "static t_tuple orderOut_" << i << "(const t_tuple& t) {\n";
        out << "t_tuple res;\n";
        for (std::size_t j = 0; j < ind.size(); j++) {
            out << "res[" << ind[j] << "] = t[" << j << "];\n";
        }
        out << "return res;\n";
        out << "}\n";
    }

    // end class
    out << "};\n";
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
 * the actual implementation is in CompiledSouffle.h */
void EqrelRelation::generateTypeStruct(std::ostream&) {
    return;
}

}  // namespace souffle::synthesiser

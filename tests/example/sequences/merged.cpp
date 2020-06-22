
#include "souffle/CompiledSouffle.h"

extern "C" {
}

namespace souffle {
static const RamDomain RAM_BIT_SHIFT_MASK = RAM_DOMAIN_SIZE - 1;
struct t_btree_1__0__1__2 {
using t_tuple = Tuple<RamDomain, 1>;
using t_comparator_0 = index_utils::comparator<0>;
using t_ind_0 = btree_set<t_tuple, t_comparator_0>;
t_ind_0 ind_0;
using iterator = t_ind_0::iterator;
struct context {
t_ind_0::operation_hints hints_0_lower;
t_ind_0::operation_hints hints_0_upper;
};
context createContext() { return context(); }
bool insert(const t_tuple& t) {
context h;
return insert(t, h);
}
bool insert(const t_tuple& t, context& h) {
if (ind_0.insert(t, h.hints_0_lower)) {
return true;
} else return false;
}
bool insert(const RamDomain* ramDomain) {
RamDomain data[1];
std::copy(ramDomain, ramDomain + 1, data);
const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
context h;
return insert(tuple, h);
}
bool insert(RamDomain a0) {
RamDomain data[1] = {a0};
return insert(data);
}
bool contains(const t_tuple& t, context& h) const {
return ind_0.contains(t, h.hints_0_lower);
}
bool contains(const t_tuple& t) const {
context h;
return contains(t, h);
}
std::size_t size() const {
return ind_0.size();
}
iterator find(const t_tuple& t, context& h) const {
return ind_0.find(t, h.hints_0_lower);
}
iterator find(const t_tuple& t) const {
context h;
return find(t, h);
}
range<iterator> lowerUpperRange_0(const t_tuple& lower, const t_tuple& upper, context& h) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<iterator> lowerUpperRange_0(const t_tuple& lower, const t_tuple& upper) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<t_ind_0::iterator> lowerUpperRange_1(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp == 0) {
    auto pos = ind_0.find(lower, h.hints_0_lower);
    auto fin = ind_0.end();
    if (pos != fin) {fin = pos; ++fin;}
    return make_range(pos, fin);
}
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
t_tuple low(lower); t_tuple high(upper);
return make_range(ind_0.lower_bound(low, h.hints_0_lower), ind_0.upper_bound(high, h.hints_0_upper));
}
range<t_ind_0::iterator> lowerUpperRange_1(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_1(lower,upper,h);
}
range<t_ind_0::iterator> lowerUpperRange_2(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
t_tuple low(lower); t_tuple high(upper);
return make_range(ind_0.lower_bound(low, h.hints_0_lower), ind_0.upper_bound(high, h.hints_0_upper));
}
range<t_ind_0::iterator> lowerUpperRange_2(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_2(lower,upper,h);
}
bool empty() const {
return ind_0.empty();
}
std::vector<range<iterator>> partition() const {
return ind_0.getChunks(400);
}
void purge() {
ind_0.clear();
}
iterator begin() const {
return ind_0.begin();
}
iterator end() const {
return ind_0.end();
}
void printStatistics(std::ostream& o) const {
o << " arity 1 direct b-tree index 0 lex-order [0]\n";
ind_0.printStats(o);
}
};
struct t_btree_3__1_0_2__010__111 {
using t_tuple = Tuple<RamDomain, 3>;
using t_comparator_0 = index_utils::comparator<1,0,2>;
using t_ind_0 = btree_set<t_tuple, t_comparator_0>;
t_ind_0 ind_0;
using iterator = t_ind_0::iterator;
struct context {
t_ind_0::operation_hints hints_0_lower;
t_ind_0::operation_hints hints_0_upper;
};
context createContext() { return context(); }
bool insert(const t_tuple& t) {
context h;
return insert(t, h);
}
bool insert(const t_tuple& t, context& h) {
if (ind_0.insert(t, h.hints_0_lower)) {
return true;
} else return false;
}
bool insert(const RamDomain* ramDomain) {
RamDomain data[3];
std::copy(ramDomain, ramDomain + 3, data);
const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
context h;
return insert(tuple, h);
}
bool insert(RamDomain a0,RamDomain a1,RamDomain a2) {
RamDomain data[3] = {a0,a1,a2};
return insert(data);
}
bool contains(const t_tuple& t, context& h) const {
return ind_0.contains(t, h.hints_0_lower);
}
bool contains(const t_tuple& t) const {
context h;
return contains(t, h);
}
std::size_t size() const {
return ind_0.size();
}
iterator find(const t_tuple& t, context& h) const {
return ind_0.find(t, h.hints_0_lower);
}
iterator find(const t_tuple& t) const {
context h;
return find(t, h);
}
range<iterator> lowerUpperRange_000(const t_tuple& lower, const t_tuple& upper, context& h) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<iterator> lowerUpperRange_000(const t_tuple& lower, const t_tuple& upper) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<t_ind_0::iterator> lowerUpperRange_010(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
t_tuple low(lower); t_tuple high(upper);
low[0] = MIN_RAM_SIGNED;
high[0] = MAX_RAM_SIGNED;
low[2] = MIN_RAM_SIGNED;
high[2] = MAX_RAM_SIGNED;
return make_range(ind_0.lower_bound(low, h.hints_0_lower), ind_0.upper_bound(high, h.hints_0_upper));
}
range<t_ind_0::iterator> lowerUpperRange_010(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_010(lower,upper,h);
}
range<t_ind_0::iterator> lowerUpperRange_111(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp == 0) {
    auto pos = ind_0.find(lower, h.hints_0_lower);
    auto fin = ind_0.end();
    if (pos != fin) {fin = pos; ++fin;}
    return make_range(pos, fin);
}
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
t_tuple low(lower); t_tuple high(upper);
return make_range(ind_0.lower_bound(low, h.hints_0_lower), ind_0.upper_bound(high, h.hints_0_upper));
}
range<t_ind_0::iterator> lowerUpperRange_111(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_111(lower,upper,h);
}
bool empty() const {
return ind_0.empty();
}
std::vector<range<iterator>> partition() const {
return ind_0.getChunks(400);
}
void purge() {
ind_0.clear();
}
iterator begin() const {
return ind_0.begin();
}
iterator end() const {
return ind_0.end();
}
void printStatistics(std::ostream& o) const {
o << " arity 3 direct b-tree index 0 lex-order [1,0,2]\n";
ind_0.printStats(o);
}
};
struct t_btree_3__0_1_2__110__111 {
using t_tuple = Tuple<RamDomain, 3>;
using t_comparator_0 = index_utils::comparator<0,1,2>;
using t_ind_0 = btree_set<t_tuple, t_comparator_0>;
t_ind_0 ind_0;
using iterator = t_ind_0::iterator;
struct context {
t_ind_0::operation_hints hints_0_lower;
t_ind_0::operation_hints hints_0_upper;
};
context createContext() { return context(); }
bool insert(const t_tuple& t) {
context h;
return insert(t, h);
}
bool insert(const t_tuple& t, context& h) {
if (ind_0.insert(t, h.hints_0_lower)) {
return true;
} else return false;
}
bool insert(const RamDomain* ramDomain) {
RamDomain data[3];
std::copy(ramDomain, ramDomain + 3, data);
const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
context h;
return insert(tuple, h);
}
bool insert(RamDomain a0,RamDomain a1,RamDomain a2) {
RamDomain data[3] = {a0,a1,a2};
return insert(data);
}
bool contains(const t_tuple& t, context& h) const {
return ind_0.contains(t, h.hints_0_lower);
}
bool contains(const t_tuple& t) const {
context h;
return contains(t, h);
}
std::size_t size() const {
return ind_0.size();
}
iterator find(const t_tuple& t, context& h) const {
return ind_0.find(t, h.hints_0_lower);
}
iterator find(const t_tuple& t) const {
context h;
return find(t, h);
}
range<iterator> lowerUpperRange_000(const t_tuple& lower, const t_tuple& upper, context& h) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<iterator> lowerUpperRange_000(const t_tuple& lower, const t_tuple& upper) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<t_ind_0::iterator> lowerUpperRange_110(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
t_tuple low(lower); t_tuple high(upper);
low[2] = MIN_RAM_SIGNED;
high[2] = MAX_RAM_SIGNED;
return make_range(ind_0.lower_bound(low, h.hints_0_lower), ind_0.upper_bound(high, h.hints_0_upper));
}
range<t_ind_0::iterator> lowerUpperRange_110(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_110(lower,upper,h);
}
range<t_ind_0::iterator> lowerUpperRange_111(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp == 0) {
    auto pos = ind_0.find(lower, h.hints_0_lower);
    auto fin = ind_0.end();
    if (pos != fin) {fin = pos; ++fin;}
    return make_range(pos, fin);
}
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
t_tuple low(lower); t_tuple high(upper);
return make_range(ind_0.lower_bound(low, h.hints_0_lower), ind_0.upper_bound(high, h.hints_0_upper));
}
range<t_ind_0::iterator> lowerUpperRange_111(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_111(lower,upper,h);
}
bool empty() const {
return ind_0.empty();
}
std::vector<range<iterator>> partition() const {
return ind_0.getChunks(400);
}
void purge() {
ind_0.clear();
}
iterator begin() const {
return ind_0.begin();
}
iterator end() const {
return ind_0.end();
}
void printStatistics(std::ostream& o) const {
o << " arity 3 direct b-tree index 0 lex-order [0,1,2]\n";
ind_0.printStats(o);
}
};
struct t_btree_2__0_1__11 {
using t_tuple = Tuple<RamDomain, 2>;
using t_comparator_0 = index_utils::comparator<0,1>;
using t_ind_0 = btree_set<t_tuple, t_comparator_0>;
t_ind_0 ind_0;
using iterator = t_ind_0::iterator;
struct context {
t_ind_0::operation_hints hints_0_lower;
t_ind_0::operation_hints hints_0_upper;
};
context createContext() { return context(); }
bool insert(const t_tuple& t) {
context h;
return insert(t, h);
}
bool insert(const t_tuple& t, context& h) {
if (ind_0.insert(t, h.hints_0_lower)) {
return true;
} else return false;
}
bool insert(const RamDomain* ramDomain) {
RamDomain data[2];
std::copy(ramDomain, ramDomain + 2, data);
const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
context h;
return insert(tuple, h);
}
bool insert(RamDomain a0,RamDomain a1) {
RamDomain data[2] = {a0,a1};
return insert(data);
}
bool contains(const t_tuple& t, context& h) const {
return ind_0.contains(t, h.hints_0_lower);
}
bool contains(const t_tuple& t) const {
context h;
return contains(t, h);
}
std::size_t size() const {
return ind_0.size();
}
iterator find(const t_tuple& t, context& h) const {
return ind_0.find(t, h.hints_0_lower);
}
iterator find(const t_tuple& t) const {
context h;
return find(t, h);
}
range<iterator> lowerUpperRange_00(const t_tuple& lower, const t_tuple& upper, context& h) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<iterator> lowerUpperRange_00(const t_tuple& lower, const t_tuple& upper) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<t_ind_0::iterator> lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp == 0) {
    auto pos = ind_0.find(lower, h.hints_0_lower);
    auto fin = ind_0.end();
    if (pos != fin) {fin = pos; ++fin;}
    return make_range(pos, fin);
}
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
t_tuple low(lower); t_tuple high(upper);
return make_range(ind_0.lower_bound(low, h.hints_0_lower), ind_0.upper_bound(high, h.hints_0_upper));
}
range<t_ind_0::iterator> lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_11(lower,upper,h);
}
bool empty() const {
return ind_0.empty();
}
std::vector<range<iterator>> partition() const {
return ind_0.getChunks(400);
}
void purge() {
ind_0.clear();
}
iterator begin() const {
return ind_0.begin();
}
iterator end() const {
return ind_0.end();
}
void printStatistics(std::ostream& o) const {
o << " arity 2 direct b-tree index 0 lex-order [0,1]\n";
ind_0.printStats(o);
}
};
struct t_btree_1__0__1 {
using t_tuple = Tuple<RamDomain, 1>;
using t_comparator_0 = index_utils::comparator<0>;
using t_ind_0 = btree_set<t_tuple, t_comparator_0>;
t_ind_0 ind_0;
using iterator = t_ind_0::iterator;
struct context {
t_ind_0::operation_hints hints_0_lower;
t_ind_0::operation_hints hints_0_upper;
};
context createContext() { return context(); }
bool insert(const t_tuple& t) {
context h;
return insert(t, h);
}
bool insert(const t_tuple& t, context& h) {
if (ind_0.insert(t, h.hints_0_lower)) {
return true;
} else return false;
}
bool insert(const RamDomain* ramDomain) {
RamDomain data[1];
std::copy(ramDomain, ramDomain + 1, data);
const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
context h;
return insert(tuple, h);
}
bool insert(RamDomain a0) {
RamDomain data[1] = {a0};
return insert(data);
}
bool contains(const t_tuple& t, context& h) const {
return ind_0.contains(t, h.hints_0_lower);
}
bool contains(const t_tuple& t) const {
context h;
return contains(t, h);
}
std::size_t size() const {
return ind_0.size();
}
iterator find(const t_tuple& t, context& h) const {
return ind_0.find(t, h.hints_0_lower);
}
iterator find(const t_tuple& t) const {
context h;
return find(t, h);
}
range<iterator> lowerUpperRange_0(const t_tuple& lower, const t_tuple& upper, context& h) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<iterator> lowerUpperRange_0(const t_tuple& lower, const t_tuple& upper) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<t_ind_0::iterator> lowerUpperRange_1(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp == 0) {
    auto pos = ind_0.find(lower, h.hints_0_lower);
    auto fin = ind_0.end();
    if (pos != fin) {fin = pos; ++fin;}
    return make_range(pos, fin);
}
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
t_tuple low(lower); t_tuple high(upper);
return make_range(ind_0.lower_bound(low, h.hints_0_lower), ind_0.upper_bound(high, h.hints_0_upper));
}
range<t_ind_0::iterator> lowerUpperRange_1(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_1(lower,upper,h);
}
bool empty() const {
return ind_0.empty();
}
std::vector<range<iterator>> partition() const {
return ind_0.getChunks(400);
}
void purge() {
ind_0.clear();
}
iterator begin() const {
return ind_0.begin();
}
iterator end() const {
return ind_0.end();
}
void printStatistics(std::ostream& o) const {
o << " arity 1 direct b-tree index 0 lex-order [0]\n";
ind_0.printStats(o);
}
};
struct t_btree_2__0_1__10__11 {
using t_tuple = Tuple<RamDomain, 2>;
using t_comparator_0 = index_utils::comparator<0,1>;
using t_ind_0 = btree_set<t_tuple, t_comparator_0>;
t_ind_0 ind_0;
using iterator = t_ind_0::iterator;
struct context {
t_ind_0::operation_hints hints_0_lower;
t_ind_0::operation_hints hints_0_upper;
};
context createContext() { return context(); }
bool insert(const t_tuple& t) {
context h;
return insert(t, h);
}
bool insert(const t_tuple& t, context& h) {
if (ind_0.insert(t, h.hints_0_lower)) {
return true;
} else return false;
}
bool insert(const RamDomain* ramDomain) {
RamDomain data[2];
std::copy(ramDomain, ramDomain + 2, data);
const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
context h;
return insert(tuple, h);
}
bool insert(RamDomain a0,RamDomain a1) {
RamDomain data[2] = {a0,a1};
return insert(data);
}
bool contains(const t_tuple& t, context& h) const {
return ind_0.contains(t, h.hints_0_lower);
}
bool contains(const t_tuple& t) const {
context h;
return contains(t, h);
}
std::size_t size() const {
return ind_0.size();
}
iterator find(const t_tuple& t, context& h) const {
return ind_0.find(t, h.hints_0_lower);
}
iterator find(const t_tuple& t) const {
context h;
return find(t, h);
}
range<iterator> lowerUpperRange_00(const t_tuple& lower, const t_tuple& upper, context& h) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<iterator> lowerUpperRange_00(const t_tuple& lower, const t_tuple& upper) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<t_ind_0::iterator> lowerUpperRange_10(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
t_tuple low(lower); t_tuple high(upper);
low[1] = MIN_RAM_SIGNED;
high[1] = MAX_RAM_SIGNED;
return make_range(ind_0.lower_bound(low, h.hints_0_lower), ind_0.upper_bound(high, h.hints_0_upper));
}
range<t_ind_0::iterator> lowerUpperRange_10(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_10(lower,upper,h);
}
range<t_ind_0::iterator> lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp == 0) {
    auto pos = ind_0.find(lower, h.hints_0_lower);
    auto fin = ind_0.end();
    if (pos != fin) {fin = pos; ++fin;}
    return make_range(pos, fin);
}
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
t_tuple low(lower); t_tuple high(upper);
return make_range(ind_0.lower_bound(low, h.hints_0_lower), ind_0.upper_bound(high, h.hints_0_upper));
}
range<t_ind_0::iterator> lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_11(lower,upper,h);
}
bool empty() const {
return ind_0.empty();
}
std::vector<range<iterator>> partition() const {
return ind_0.getChunks(400);
}
void purge() {
ind_0.clear();
}
iterator begin() const {
return ind_0.begin();
}
iterator end() const {
return ind_0.end();
}
void printStatistics(std::ostream& o) const {
o << " arity 2 direct b-tree index 0 lex-order [0,1]\n";
ind_0.printStats(o);
}
};
struct t_btree_3__0_1_2__0_2_1__101__110__120__111 {
using t_tuple = Tuple<RamDomain, 3>;
using t_comparator_0 = index_utils::comparator<0,1,2>;
using t_ind_0 = btree_set<t_tuple, t_comparator_0>;
t_ind_0 ind_0;
using t_comparator_1 = index_utils::comparator<0,2,1>;
using t_ind_1 = btree_set<t_tuple, t_comparator_1>;
t_ind_1 ind_1;
using iterator = t_ind_0::iterator;
struct context {
t_ind_0::operation_hints hints_0_lower;
t_ind_0::operation_hints hints_0_upper;
t_ind_1::operation_hints hints_1_lower;
t_ind_1::operation_hints hints_1_upper;
};
context createContext() { return context(); }
bool insert(const t_tuple& t) {
context h;
return insert(t, h);
}
bool insert(const t_tuple& t, context& h) {
if (ind_0.insert(t, h.hints_0_lower)) {
ind_1.insert(t, h.hints_1_lower);
return true;
} else return false;
}
bool insert(const RamDomain* ramDomain) {
RamDomain data[3];
std::copy(ramDomain, ramDomain + 3, data);
const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
context h;
return insert(tuple, h);
}
bool insert(RamDomain a0,RamDomain a1,RamDomain a2) {
RamDomain data[3] = {a0,a1,a2};
return insert(data);
}
bool contains(const t_tuple& t, context& h) const {
return ind_0.contains(t, h.hints_0_lower);
}
bool contains(const t_tuple& t) const {
context h;
return contains(t, h);
}
std::size_t size() const {
return ind_0.size();
}
iterator find(const t_tuple& t, context& h) const {
return ind_0.find(t, h.hints_0_lower);
}
iterator find(const t_tuple& t) const {
context h;
return find(t, h);
}
range<iterator> lowerUpperRange_000(const t_tuple& lower, const t_tuple& upper, context& h) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<iterator> lowerUpperRange_000(const t_tuple& lower, const t_tuple& upper) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<t_ind_1::iterator> lowerUpperRange_101(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_1 comparator;
int cmp = comparator(lower, upper);
if (cmp > 0) {
    return make_range(ind_1.end(), ind_1.end());
}
t_tuple low(lower); t_tuple high(upper);
low[1] = MIN_RAM_SIGNED;
high[1] = MAX_RAM_SIGNED;
return make_range(ind_1.lower_bound(low, h.hints_1_lower), ind_1.upper_bound(high, h.hints_1_upper));
}
range<t_ind_1::iterator> lowerUpperRange_101(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_101(lower,upper,h);
}
range<t_ind_0::iterator> lowerUpperRange_110(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
t_tuple low(lower); t_tuple high(upper);
low[2] = MIN_RAM_SIGNED;
high[2] = MAX_RAM_SIGNED;
return make_range(ind_0.lower_bound(low, h.hints_0_lower), ind_0.upper_bound(high, h.hints_0_upper));
}
range<t_ind_0::iterator> lowerUpperRange_110(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_110(lower,upper,h);
}
range<t_ind_0::iterator> lowerUpperRange_120(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
t_tuple low(lower); t_tuple high(upper);
low[2] = MIN_RAM_SIGNED;
high[2] = MAX_RAM_SIGNED;
return make_range(ind_0.lower_bound(low, h.hints_0_lower), ind_0.upper_bound(high, h.hints_0_upper));
}
range<t_ind_0::iterator> lowerUpperRange_120(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_120(lower,upper,h);
}
range<t_ind_1::iterator> lowerUpperRange_111(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_1 comparator;
int cmp = comparator(lower, upper);
if (cmp == 0) {
    auto pos = ind_1.find(lower, h.hints_1_lower);
    auto fin = ind_1.end();
    if (pos != fin) {fin = pos; ++fin;}
    return make_range(pos, fin);
}
if (cmp > 0) {
    return make_range(ind_1.end(), ind_1.end());
}
t_tuple low(lower); t_tuple high(upper);
return make_range(ind_1.lower_bound(low, h.hints_1_lower), ind_1.upper_bound(high, h.hints_1_upper));
}
range<t_ind_1::iterator> lowerUpperRange_111(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_111(lower,upper,h);
}
bool empty() const {
return ind_0.empty();
}
std::vector<range<iterator>> partition() const {
return ind_0.getChunks(400);
}
void purge() {
ind_0.clear();
ind_1.clear();
}
iterator begin() const {
return ind_0.begin();
}
iterator end() const {
return ind_0.end();
}
void printStatistics(std::ostream& o) const {
o << " arity 3 direct b-tree index 0 lex-order [0,1,2]\n";
ind_0.printStats(o);
o << " arity 3 direct b-tree index 1 lex-order [0,2,1]\n";
ind_1.printStats(o);
}
};
struct t_btree_2__1_0__0_1__01__10__11 {
using t_tuple = Tuple<RamDomain, 2>;
using t_comparator_0 = index_utils::comparator<1,0>;
using t_ind_0 = btree_set<t_tuple, t_comparator_0>;
t_ind_0 ind_0;
using t_comparator_1 = index_utils::comparator<0,1>;
using t_ind_1 = btree_set<t_tuple, t_comparator_1>;
t_ind_1 ind_1;
using iterator = t_ind_0::iterator;
struct context {
t_ind_0::operation_hints hints_0_lower;
t_ind_0::operation_hints hints_0_upper;
t_ind_1::operation_hints hints_1_lower;
t_ind_1::operation_hints hints_1_upper;
};
context createContext() { return context(); }
bool insert(const t_tuple& t) {
context h;
return insert(t, h);
}
bool insert(const t_tuple& t, context& h) {
if (ind_0.insert(t, h.hints_0_lower)) {
ind_1.insert(t, h.hints_1_lower);
return true;
} else return false;
}
bool insert(const RamDomain* ramDomain) {
RamDomain data[2];
std::copy(ramDomain, ramDomain + 2, data);
const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
context h;
return insert(tuple, h);
}
bool insert(RamDomain a0,RamDomain a1) {
RamDomain data[2] = {a0,a1};
return insert(data);
}
bool contains(const t_tuple& t, context& h) const {
return ind_0.contains(t, h.hints_0_lower);
}
bool contains(const t_tuple& t) const {
context h;
return contains(t, h);
}
std::size_t size() const {
return ind_0.size();
}
iterator find(const t_tuple& t, context& h) const {
return ind_0.find(t, h.hints_0_lower);
}
iterator find(const t_tuple& t) const {
context h;
return find(t, h);
}
range<iterator> lowerUpperRange_00(const t_tuple& lower, const t_tuple& upper, context& h) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<iterator> lowerUpperRange_00(const t_tuple& lower, const t_tuple& upper) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<t_ind_0::iterator> lowerUpperRange_01(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
t_tuple low(lower); t_tuple high(upper);
low[0] = MIN_RAM_SIGNED;
high[0] = MAX_RAM_SIGNED;
return make_range(ind_0.lower_bound(low, h.hints_0_lower), ind_0.upper_bound(high, h.hints_0_upper));
}
range<t_ind_0::iterator> lowerUpperRange_01(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_01(lower,upper,h);
}
range<t_ind_1::iterator> lowerUpperRange_10(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_1 comparator;
int cmp = comparator(lower, upper);
if (cmp > 0) {
    return make_range(ind_1.end(), ind_1.end());
}
t_tuple low(lower); t_tuple high(upper);
low[1] = MIN_RAM_SIGNED;
high[1] = MAX_RAM_SIGNED;
return make_range(ind_1.lower_bound(low, h.hints_1_lower), ind_1.upper_bound(high, h.hints_1_upper));
}
range<t_ind_1::iterator> lowerUpperRange_10(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_10(lower,upper,h);
}
range<t_ind_1::iterator> lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_1 comparator;
int cmp = comparator(lower, upper);
if (cmp == 0) {
    auto pos = ind_1.find(lower, h.hints_1_lower);
    auto fin = ind_1.end();
    if (pos != fin) {fin = pos; ++fin;}
    return make_range(pos, fin);
}
if (cmp > 0) {
    return make_range(ind_1.end(), ind_1.end());
}
t_tuple low(lower); t_tuple high(upper);
return make_range(ind_1.lower_bound(low, h.hints_1_lower), ind_1.upper_bound(high, h.hints_1_upper));
}
range<t_ind_1::iterator> lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_11(lower,upper,h);
}
bool empty() const {
return ind_0.empty();
}
std::vector<range<iterator>> partition() const {
return ind_0.getChunks(400);
}
void purge() {
ind_0.clear();
ind_1.clear();
}
iterator begin() const {
return ind_0.begin();
}
iterator end() const {
return ind_0.end();
}
void printStatistics(std::ostream& o) const {
o << " arity 2 direct b-tree index 0 lex-order [1,0]\n";
ind_0.printStats(o);
o << " arity 2 direct b-tree index 1 lex-order [0,1]\n";
ind_1.printStats(o);
}
};
struct t_btree_3__0_1_2__100__111 {
using t_tuple = Tuple<RamDomain, 3>;
using t_comparator_0 = index_utils::comparator<0,1,2>;
using t_ind_0 = btree_set<t_tuple, t_comparator_0>;
t_ind_0 ind_0;
using iterator = t_ind_0::iterator;
struct context {
t_ind_0::operation_hints hints_0_lower;
t_ind_0::operation_hints hints_0_upper;
};
context createContext() { return context(); }
bool insert(const t_tuple& t) {
context h;
return insert(t, h);
}
bool insert(const t_tuple& t, context& h) {
if (ind_0.insert(t, h.hints_0_lower)) {
return true;
} else return false;
}
bool insert(const RamDomain* ramDomain) {
RamDomain data[3];
std::copy(ramDomain, ramDomain + 3, data);
const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
context h;
return insert(tuple, h);
}
bool insert(RamDomain a0,RamDomain a1,RamDomain a2) {
RamDomain data[3] = {a0,a1,a2};
return insert(data);
}
bool contains(const t_tuple& t, context& h) const {
return ind_0.contains(t, h.hints_0_lower);
}
bool contains(const t_tuple& t) const {
context h;
return contains(t, h);
}
std::size_t size() const {
return ind_0.size();
}
iterator find(const t_tuple& t, context& h) const {
return ind_0.find(t, h.hints_0_lower);
}
iterator find(const t_tuple& t) const {
context h;
return find(t, h);
}
range<iterator> lowerUpperRange_000(const t_tuple& lower, const t_tuple& upper, context& h) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<iterator> lowerUpperRange_000(const t_tuple& lower, const t_tuple& upper) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<t_ind_0::iterator> lowerUpperRange_100(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
t_tuple low(lower); t_tuple high(upper);
low[1] = MIN_RAM_SIGNED;
high[1] = MAX_RAM_SIGNED;
low[2] = MIN_RAM_SIGNED;
high[2] = MAX_RAM_SIGNED;
return make_range(ind_0.lower_bound(low, h.hints_0_lower), ind_0.upper_bound(high, h.hints_0_upper));
}
range<t_ind_0::iterator> lowerUpperRange_100(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_100(lower,upper,h);
}
range<t_ind_0::iterator> lowerUpperRange_111(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp == 0) {
    auto pos = ind_0.find(lower, h.hints_0_lower);
    auto fin = ind_0.end();
    if (pos != fin) {fin = pos; ++fin;}
    return make_range(pos, fin);
}
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
t_tuple low(lower); t_tuple high(upper);
return make_range(ind_0.lower_bound(low, h.hints_0_lower), ind_0.upper_bound(high, h.hints_0_upper));
}
range<t_ind_0::iterator> lowerUpperRange_111(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_111(lower,upper,h);
}
bool empty() const {
return ind_0.empty();
}
std::vector<range<iterator>> partition() const {
return ind_0.getChunks(400);
}
void purge() {
ind_0.clear();
}
iterator begin() const {
return ind_0.begin();
}
iterator end() const {
return ind_0.end();
}
void printStatistics(std::ostream& o) const {
o << " arity 3 direct b-tree index 0 lex-order [0,1,2]\n";
ind_0.printStats(o);
}
};
struct t_btree_2__1_0__0_1__01__10__11__12 {
using t_tuple = Tuple<RamDomain, 2>;
using t_comparator_0 = index_utils::comparator<1,0>;
using t_ind_0 = btree_set<t_tuple, t_comparator_0>;
t_ind_0 ind_0;
using t_comparator_1 = index_utils::comparator<0,1>;
using t_ind_1 = btree_set<t_tuple, t_comparator_1>;
t_ind_1 ind_1;
using iterator = t_ind_0::iterator;
struct context {
t_ind_0::operation_hints hints_0_lower;
t_ind_0::operation_hints hints_0_upper;
t_ind_1::operation_hints hints_1_lower;
t_ind_1::operation_hints hints_1_upper;
};
context createContext() { return context(); }
bool insert(const t_tuple& t) {
context h;
return insert(t, h);
}
bool insert(const t_tuple& t, context& h) {
if (ind_0.insert(t, h.hints_0_lower)) {
ind_1.insert(t, h.hints_1_lower);
return true;
} else return false;
}
bool insert(const RamDomain* ramDomain) {
RamDomain data[2];
std::copy(ramDomain, ramDomain + 2, data);
const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
context h;
return insert(tuple, h);
}
bool insert(RamDomain a0,RamDomain a1) {
RamDomain data[2] = {a0,a1};
return insert(data);
}
bool contains(const t_tuple& t, context& h) const {
return ind_0.contains(t, h.hints_0_lower);
}
bool contains(const t_tuple& t) const {
context h;
return contains(t, h);
}
std::size_t size() const {
return ind_0.size();
}
iterator find(const t_tuple& t, context& h) const {
return ind_0.find(t, h.hints_0_lower);
}
iterator find(const t_tuple& t) const {
context h;
return find(t, h);
}
range<iterator> lowerUpperRange_00(const t_tuple& lower, const t_tuple& upper, context& h) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<iterator> lowerUpperRange_00(const t_tuple& lower, const t_tuple& upper) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<t_ind_0::iterator> lowerUpperRange_01(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
t_tuple low(lower); t_tuple high(upper);
low[0] = MIN_RAM_SIGNED;
high[0] = MAX_RAM_SIGNED;
return make_range(ind_0.lower_bound(low, h.hints_0_lower), ind_0.upper_bound(high, h.hints_0_upper));
}
range<t_ind_0::iterator> lowerUpperRange_01(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_01(lower,upper,h);
}
range<t_ind_1::iterator> lowerUpperRange_10(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_1 comparator;
int cmp = comparator(lower, upper);
if (cmp > 0) {
    return make_range(ind_1.end(), ind_1.end());
}
t_tuple low(lower); t_tuple high(upper);
low[1] = MIN_RAM_SIGNED;
high[1] = MAX_RAM_SIGNED;
return make_range(ind_1.lower_bound(low, h.hints_1_lower), ind_1.upper_bound(high, h.hints_1_upper));
}
range<t_ind_1::iterator> lowerUpperRange_10(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_10(lower,upper,h);
}
range<t_ind_1::iterator> lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_1 comparator;
int cmp = comparator(lower, upper);
if (cmp == 0) {
    auto pos = ind_1.find(lower, h.hints_1_lower);
    auto fin = ind_1.end();
    if (pos != fin) {fin = pos; ++fin;}
    return make_range(pos, fin);
}
if (cmp > 0) {
    return make_range(ind_1.end(), ind_1.end());
}
t_tuple low(lower); t_tuple high(upper);
return make_range(ind_1.lower_bound(low, h.hints_1_lower), ind_1.upper_bound(high, h.hints_1_upper));
}
range<t_ind_1::iterator> lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_11(lower,upper,h);
}
range<t_ind_1::iterator> lowerUpperRange_12(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_1 comparator;
int cmp = comparator(lower, upper);
if (cmp > 0) {
    return make_range(ind_1.end(), ind_1.end());
}
t_tuple low(lower); t_tuple high(upper);
return make_range(ind_1.lower_bound(low, h.hints_1_lower), ind_1.upper_bound(high, h.hints_1_upper));
}
range<t_ind_1::iterator> lowerUpperRange_12(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_12(lower,upper,h);
}
bool empty() const {
return ind_0.empty();
}
std::vector<range<iterator>> partition() const {
return ind_0.getChunks(400);
}
void purge() {
ind_0.clear();
ind_1.clear();
}
iterator begin() const {
return ind_0.begin();
}
iterator end() const {
return ind_0.end();
}
void printStatistics(std::ostream& o) const {
o << " arity 2 direct b-tree index 0 lex-order [1,0]\n";
ind_0.printStats(o);
o << " arity 2 direct b-tree index 1 lex-order [0,1]\n";
ind_1.printStats(o);
}
};
struct t_btree_2__0_1__10__11__12 {
using t_tuple = Tuple<RamDomain, 2>;
using t_comparator_0 = index_utils::comparator<0,1>;
using t_ind_0 = btree_set<t_tuple, t_comparator_0>;
t_ind_0 ind_0;
using iterator = t_ind_0::iterator;
struct context {
t_ind_0::operation_hints hints_0_lower;
t_ind_0::operation_hints hints_0_upper;
};
context createContext() { return context(); }
bool insert(const t_tuple& t) {
context h;
return insert(t, h);
}
bool insert(const t_tuple& t, context& h) {
if (ind_0.insert(t, h.hints_0_lower)) {
return true;
} else return false;
}
bool insert(const RamDomain* ramDomain) {
RamDomain data[2];
std::copy(ramDomain, ramDomain + 2, data);
const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
context h;
return insert(tuple, h);
}
bool insert(RamDomain a0,RamDomain a1) {
RamDomain data[2] = {a0,a1};
return insert(data);
}
bool contains(const t_tuple& t, context& h) const {
return ind_0.contains(t, h.hints_0_lower);
}
bool contains(const t_tuple& t) const {
context h;
return contains(t, h);
}
std::size_t size() const {
return ind_0.size();
}
iterator find(const t_tuple& t, context& h) const {
return ind_0.find(t, h.hints_0_lower);
}
iterator find(const t_tuple& t) const {
context h;
return find(t, h);
}
range<iterator> lowerUpperRange_00(const t_tuple& lower, const t_tuple& upper, context& h) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<iterator> lowerUpperRange_00(const t_tuple& lower, const t_tuple& upper) const {
return range<iterator>(ind_0.begin(),ind_0.end());
}
range<t_ind_0::iterator> lowerUpperRange_10(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
t_tuple low(lower); t_tuple high(upper);
low[1] = MIN_RAM_SIGNED;
high[1] = MAX_RAM_SIGNED;
return make_range(ind_0.lower_bound(low, h.hints_0_lower), ind_0.upper_bound(high, h.hints_0_upper));
}
range<t_ind_0::iterator> lowerUpperRange_10(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_10(lower,upper,h);
}
range<t_ind_0::iterator> lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp == 0) {
    auto pos = ind_0.find(lower, h.hints_0_lower);
    auto fin = ind_0.end();
    if (pos != fin) {fin = pos; ++fin;}
    return make_range(pos, fin);
}
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
t_tuple low(lower); t_tuple high(upper);
return make_range(ind_0.lower_bound(low, h.hints_0_lower), ind_0.upper_bound(high, h.hints_0_upper));
}
range<t_ind_0::iterator> lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_11(lower,upper,h);
}
range<t_ind_0::iterator> lowerUpperRange_12(const t_tuple& lower, const t_tuple& upper, context& h) const {
t_comparator_0 comparator;
int cmp = comparator(lower, upper);
if (cmp > 0) {
    return make_range(ind_0.end(), ind_0.end());
}
t_tuple low(lower); t_tuple high(upper);
return make_range(ind_0.lower_bound(low, h.hints_0_lower), ind_0.upper_bound(high, h.hints_0_upper));
}
range<t_ind_0::iterator> lowerUpperRange_12(const t_tuple& lower, const t_tuple& upper) const {
context h;
return lowerUpperRange_12(lower,upper,h);
}
bool empty() const {
return ind_0.empty();
}
std::vector<range<iterator>> partition() const {
return ind_0.getChunks(400);
}
void purge() {
ind_0.clear();
}
iterator begin() const {
return ind_0.begin();
}
iterator end() const {
return ind_0.end();
}
void printStatistics(std::ostream& o) const {
o << " arity 2 direct b-tree index 0 lex-order [0,1]\n";
ind_0.printStats(o);
}
};

class Sf_merged : public SouffleProgram {
private:
static inline bool regex_wrapper(const std::string& pattern, const std::string& text) {
   bool result = false; 
   try { result = std::regex_match(text, std::regex(pattern)); } catch(...) { 
     std::cerr << "warning: wrong pattern provided for match(\"" << pattern << "\",\"" << text << "\").\n";
}
   return result;
}
private:
static inline std::string substr_wrapper(const std::string& str, size_t idx, size_t len) {
   std::string result; 
   try { result = str.substr(idx,len); } catch(...) { 
     std::cerr << "warning: wrong index position provided by substr(\"";
     std::cerr << str << "\"," << (int32_t)idx << "," << (int32_t)len << ") functor.\n";
   } return result;
}
private:
static inline RamDomain wrapper_tonumber(const std::string& str) {
   RamDomain result=0; 
   try { result = RamSignedFromString(str); } catch(...) { 
     std::cerr << "error: wrong string provided by to_number(\"";
     std::cerr << str << "\") functor.\n";
     raise(SIGFPE);
   } return result;
}
public:
// -- initialize symbol table --
SymbolTable symTable{
	R"_(a)_",
	R"_(b)_",
	R"_(c)_",
};// -- initialize record table --
RecordTable recordTable;
// -- Table: @delta_n
std::unique_ptr<t_btree_1__0__1__2> rel_1_delta_n = std::make_unique<t_btree_1__0__1__2>();
// -- Table: @delta_op_add
std::unique_ptr<t_btree_3__1_0_2__010__111> rel_2_delta_op_add = std::make_unique<t_btree_3__1_0_2__010__111>();
// -- Table: @delta_op_exp
std::unique_ptr<t_btree_3__1_0_2__010__111> rel_3_delta_op_exp = std::make_unique<t_btree_3__1_0_2__010__111>();
// -- Table: @delta_op_mul
std::unique_ptr<t_btree_3__1_0_2__010__111> rel_4_delta_op_mul = std::make_unique<t_btree_3__1_0_2__010__111>();
// -- Table: @delta_palin_aux
std::unique_ptr<t_btree_3__0_1_2__110__111> rel_5_delta_palin_aux = std::make_unique<t_btree_3__0_1_2__110__111>();
// -- Table: @delta_str_chain
std::unique_ptr<t_btree_2__0_1__11> rel_6_delta_str_chain = std::make_unique<t_btree_2__0_1__11>();
// -- Table: @delta_trie
std::unique_ptr<t_btree_1__0__1> rel_7_delta_trie = std::make_unique<t_btree_1__0__1>();
// -- Table: @delta_trie_level
std::unique_ptr<t_btree_2__0_1__11> rel_8_delta_trie_level = std::make_unique<t_btree_2__0_1__11>();
// -- Table: @delta_trie_level_end
std::unique_ptr<t_btree_2__0_1__10__11> rel_9_delta_trie_level_end = std::make_unique<t_btree_2__0_1__10__11>();
// -- Table: @new_n
std::unique_ptr<t_btree_1__0__1__2> rel_10_new_n = std::make_unique<t_btree_1__0__1__2>();
// -- Table: @new_op_add
std::unique_ptr<t_btree_3__1_0_2__010__111> rel_11_new_op_add = std::make_unique<t_btree_3__1_0_2__010__111>();
// -- Table: @new_op_exp
std::unique_ptr<t_btree_3__1_0_2__010__111> rel_12_new_op_exp = std::make_unique<t_btree_3__1_0_2__010__111>();
// -- Table: @new_op_mul
std::unique_ptr<t_btree_3__1_0_2__010__111> rel_13_new_op_mul = std::make_unique<t_btree_3__1_0_2__010__111>();
// -- Table: @new_palin_aux
std::unique_ptr<t_btree_3__0_1_2__110__111> rel_14_new_palin_aux = std::make_unique<t_btree_3__0_1_2__110__111>();
// -- Table: @new_str_chain
std::unique_ptr<t_btree_2__0_1__11> rel_15_new_str_chain = std::make_unique<t_btree_2__0_1__11>();
// -- Table: @new_trie
std::unique_ptr<t_btree_1__0__1> rel_16_new_trie = std::make_unique<t_btree_1__0__1>();
// -- Table: @new_trie_level
std::unique_ptr<t_btree_2__0_1__11> rel_17_new_trie_level = std::make_unique<t_btree_2__0_1__11>();
// -- Table: @new_trie_level_end
std::unique_ptr<t_btree_2__0_1__10__11> rel_18_new_trie_level_end = std::make_unique<t_btree_2__0_1__10__11>();
// -- Table: debug_str
std::unique_ptr<t_btree_1__0__1> rel_19_debug_str = std::make_unique<t_btree_1__0__1>();
souffle::RelationWrapper<0,t_btree_1__0__1,Tuple<RamDomain,1>,1,0> wrapper_rel_19_debug_str;
// -- Table: idx
std::unique_ptr<t_btree_2__0_1__10__11> rel_20_idx = std::make_unique<t_btree_2__0_1__10__11>();
souffle::RelationWrapper<1,t_btree_2__0_1__10__11,Tuple<RamDomain,2>,2,0> wrapper_rel_20_idx;
// -- Table: n
std::unique_ptr<t_btree_1__0__1> rel_21_n = std::make_unique<t_btree_1__0__1>();
souffle::RelationWrapper<2,t_btree_1__0__1,Tuple<RamDomain,1>,1,0> wrapper_rel_21_n;
// -- Table: num_letters
std::unique_ptr<t_btree_1__0__1> rel_22_num_letters = std::make_unique<t_btree_1__0__1>();
souffle::RelationWrapper<3,t_btree_1__0__1,Tuple<RamDomain,1>,1,0> wrapper_rel_22_num_letters;
// -- Table: op_add
std::unique_ptr<t_btree_3__0_1_2__0_2_1__101__110__120__111> rel_23_op_add = std::make_unique<t_btree_3__0_1_2__0_2_1__101__110__120__111>();
souffle::RelationWrapper<4,t_btree_3__0_1_2__0_2_1__101__110__120__111,Tuple<RamDomain,3>,3,0> wrapper_rel_23_op_add;
// -- Table: op_div
std::unique_ptr<t_btree_3__0_1_2__110__111> rel_24_op_div = std::make_unique<t_btree_3__0_1_2__110__111>();
souffle::RelationWrapper<5,t_btree_3__0_1_2__110__111,Tuple<RamDomain,3>,3,0> wrapper_rel_24_op_div;
// -- Table: op_exp
std::unique_ptr<t_btree_3__0_1_2__110__111> rel_25_op_exp = std::make_unique<t_btree_3__0_1_2__110__111>();
souffle::RelationWrapper<6,t_btree_3__0_1_2__110__111,Tuple<RamDomain,3>,3,0> wrapper_rel_25_op_exp;
// -- Table: op_mod
std::unique_ptr<t_btree_3__0_1_2__110__111> rel_26_op_mod = std::make_unique<t_btree_3__0_1_2__110__111>();
souffle::RelationWrapper<7,t_btree_3__0_1_2__110__111,Tuple<RamDomain,3>,3,0> wrapper_rel_26_op_mod;
// -- Table: op_mul
std::unique_ptr<t_btree_3__0_1_2__110__111> rel_27_op_mul = std::make_unique<t_btree_3__0_1_2__110__111>();
souffle::RelationWrapper<8,t_btree_3__0_1_2__110__111,Tuple<RamDomain,3>,3,0> wrapper_rel_27_op_mul;
// -- Table: palin_aux
std::unique_ptr<t_btree_3__1_0_2__010__111> rel_28_palin_aux = std::make_unique<t_btree_3__1_0_2__010__111>();
souffle::RelationWrapper<9,t_btree_3__1_0_2__010__111,Tuple<RamDomain,3>,3,0> wrapper_rel_28_palin_aux;
// -- Table: palindrome
std::unique_ptr<t_btree_1__0__1> rel_29_palindrome = std::make_unique<t_btree_1__0__1>();
souffle::RelationWrapper<10,t_btree_1__0__1,Tuple<RamDomain,1>,1,0> wrapper_rel_29_palindrome;
// -- Table: read
std::unique_ptr<t_btree_2__0_1__11> rel_30_read = std::make_unique<t_btree_2__0_1__11>();
souffle::RelationWrapper<11,t_btree_2__0_1__11,Tuple<RamDomain,2>,2,0> wrapper_rel_30_read;
// -- Table: s
std::unique_ptr<t_btree_2__1_0__0_1__01__10__11> rel_31_s = std::make_unique<t_btree_2__1_0__0_1__01__10__11>();
souffle::RelationWrapper<12,t_btree_2__1_0__0_1__01__10__11,Tuple<RamDomain,2>,2,0> wrapper_rel_31_s;
// -- Table: str_chain
std::unique_ptr<t_btree_2__0_1__11> rel_32_str_chain = std::make_unique<t_btree_2__0_1__11>();
souffle::RelationWrapper<13,t_btree_2__0_1__11,Tuple<RamDomain,2>,2,0> wrapper_rel_32_str_chain;
// -- Table: str_letter_at
std::unique_ptr<t_btree_3__0_1_2__100__111> rel_33_str_letter_at = std::make_unique<t_btree_3__0_1_2__100__111>();
souffle::RelationWrapper<14,t_btree_3__0_1_2__100__111,Tuple<RamDomain,3>,3,0> wrapper_rel_33_str_letter_at;
// -- Table: trie
std::unique_ptr<t_btree_1__0__1> rel_34_trie = std::make_unique<t_btree_1__0__1>();
souffle::RelationWrapper<15,t_btree_1__0__1,Tuple<RamDomain,1>,1,0> wrapper_rel_34_trie;
// -- Table: trie_letter
std::unique_ptr<t_btree_2__0_1__10__11> rel_35_trie_letter = std::make_unique<t_btree_2__0_1__10__11>();
souffle::RelationWrapper<16,t_btree_2__0_1__10__11,Tuple<RamDomain,2>,2,0> wrapper_rel_35_trie_letter;
// -- Table: trie_level
std::unique_ptr<t_btree_2__1_0__0_1__01__10__11__12> rel_36_trie_level = std::make_unique<t_btree_2__1_0__0_1__01__10__11__12>();
souffle::RelationWrapper<17,t_btree_2__1_0__0_1__01__10__11__12,Tuple<RamDomain,2>,2,0> wrapper_rel_36_trie_level;
// -- Table: trie_level_end
std::unique_ptr<t_btree_2__0_1__10__11__12> rel_37_trie_level_end = std::make_unique<t_btree_2__0_1__10__11__12>();
souffle::RelationWrapper<18,t_btree_2__0_1__10__11__12,Tuple<RamDomain,2>,2,0> wrapper_rel_37_trie_level_end;
// -- Table: trie_level_start
std::unique_ptr<t_btree_2__0_1__10__11> rel_38_trie_level_start = std::make_unique<t_btree_2__0_1__10__11>();
souffle::RelationWrapper<19,t_btree_2__0_1__10__11,Tuple<RamDomain,2>,2,0> wrapper_rel_38_trie_level_start;
// -- Table: trie_parent
std::unique_ptr<t_btree_2__0_1__10__11> rel_39_trie_parent = std::make_unique<t_btree_2__0_1__10__11>();
souffle::RelationWrapper<20,t_btree_2__0_1__10__11,Tuple<RamDomain,2>,2,0> wrapper_rel_39_trie_parent;
public:
Sf_merged() : 
wrapper_rel_19_debug_str(*rel_19_debug_str,symTable,"debug_str",std::array<const char *,1>{{"i:number"}},std::array<const char *,1>{{"x"}}),

wrapper_rel_20_idx(*rel_20_idx,symTable,"idx",std::array<const char *,2>{{"i:number","s:Letter"}},std::array<const char *,2>{{"i","a"}}),

wrapper_rel_21_n(*rel_21_n,symTable,"n",std::array<const char *,1>{{"i:number"}},std::array<const char *,1>{{"x"}}),

wrapper_rel_22_num_letters(*rel_22_num_letters,symTable,"num_letters",std::array<const char *,1>{{"i:number"}},std::array<const char *,1>{{"l"}}),

wrapper_rel_23_op_add(*rel_23_op_add,symTable,"op_add",std::array<const char *,3>{{"i:number","i:number","i:number"}},std::array<const char *,3>{{"x","y","r"}}),

wrapper_rel_24_op_div(*rel_24_op_div,symTable,"op_div",std::array<const char *,3>{{"i:number","i:number","i:number"}},std::array<const char *,3>{{"x","y","r"}}),

wrapper_rel_25_op_exp(*rel_25_op_exp,symTable,"op_exp",std::array<const char *,3>{{"i:number","i:number","i:number"}},std::array<const char *,3>{{"x","y","r"}}),

wrapper_rel_26_op_mod(*rel_26_op_mod,symTable,"op_mod",std::array<const char *,3>{{"i:number","i:number","i:number"}},std::array<const char *,3>{{"x","y","r"}}),

wrapper_rel_27_op_mul(*rel_27_op_mul,symTable,"op_mul",std::array<const char *,3>{{"i:number","i:number","i:number"}},std::array<const char *,3>{{"x","y","r"}}),

wrapper_rel_28_palin_aux(*rel_28_palin_aux,symTable,"palin_aux",std::array<const char *,3>{{"i:number","i:number","i:number"}},std::array<const char *,3>{{"s","a","b"}}),

wrapper_rel_29_palindrome(*rel_29_palindrome,symTable,"palindrome",std::array<const char *,1>{{"i:number"}},std::array<const char *,1>{{"s"}}),

wrapper_rel_30_read(*rel_30_read,symTable,"read",std::array<const char *,2>{{"i:number","s:Letter"}},std::array<const char *,2>{{"x","y"}}),

wrapper_rel_31_s(*rel_31_s,symTable,"s",std::array<const char *,2>{{"i:number","i:number"}},std::array<const char *,2>{{"x","y"}}),

wrapper_rel_32_str_chain(*rel_32_str_chain,symTable,"str_chain",std::array<const char *,2>{{"i:number","i:number"}},std::array<const char *,2>{{"id","parents"}}),

wrapper_rel_33_str_letter_at(*rel_33_str_letter_at,symTable,"str_letter_at",std::array<const char *,3>{{"i:number","i:number","s:Letter"}},std::array<const char *,3>{{"id","pos","l"}}),

wrapper_rel_34_trie(*rel_34_trie,symTable,"trie",std::array<const char *,1>{{"i:number"}},std::array<const char *,1>{{"x"}}),

wrapper_rel_35_trie_letter(*rel_35_trie_letter,symTable,"trie_letter",std::array<const char *,2>{{"i:number","s:Letter"}},std::array<const char *,2>{{"i","a"}}),

wrapper_rel_36_trie_level(*rel_36_trie_level,symTable,"trie_level",std::array<const char *,2>{{"i:number","i:number"}},std::array<const char *,2>{{"i","l"}}),

wrapper_rel_37_trie_level_end(*rel_37_trie_level_end,symTable,"trie_level_end",std::array<const char *,2>{{"i:number","i:number"}},std::array<const char *,2>{{"l","i"}}),

wrapper_rel_38_trie_level_start(*rel_38_trie_level_start,symTable,"trie_level_start",std::array<const char *,2>{{"i:number","i:number"}},std::array<const char *,2>{{"l","i"}}),

wrapper_rel_39_trie_parent(*rel_39_trie_parent,symTable,"trie_parent",std::array<const char *,2>{{"i:number","i:number"}},std::array<const char *,2>{{"i","p"}}){
addRelation("debug_str",&wrapper_rel_19_debug_str,false,false);
addRelation("idx",&wrapper_rel_20_idx,false,false);
addRelation("n",&wrapper_rel_21_n,false,false);
addRelation("num_letters",&wrapper_rel_22_num_letters,false,false);
addRelation("op_add",&wrapper_rel_23_op_add,false,false);
addRelation("op_div",&wrapper_rel_24_op_div,false,false);
addRelation("op_exp",&wrapper_rel_25_op_exp,false,false);
addRelation("op_mod",&wrapper_rel_26_op_mod,false,false);
addRelation("op_mul",&wrapper_rel_27_op_mul,false,false);
addRelation("palin_aux",&wrapper_rel_28_palin_aux,false,false);
addRelation("palindrome",&wrapper_rel_29_palindrome,false,true);
addRelation("read",&wrapper_rel_30_read,false,true);
addRelation("s",&wrapper_rel_31_s,false,false);
addRelation("str_chain",&wrapper_rel_32_str_chain,false,false);
addRelation("str_letter_at",&wrapper_rel_33_str_letter_at,false,false);
addRelation("trie",&wrapper_rel_34_trie,false,false);
addRelation("trie_letter",&wrapper_rel_35_trie_letter,false,false);
addRelation("trie_level",&wrapper_rel_36_trie_level,false,false);
addRelation("trie_level_end",&wrapper_rel_37_trie_level_end,false,false);
addRelation("trie_level_start",&wrapper_rel_38_trie_level_start,false,false);
addRelation("trie_parent",&wrapper_rel_39_trie_parent,false,false);
}
~Sf_merged() {
}
private:
void runFunction(std::string inputDirectory = ".", std::string outputDirectory = ".", bool performIO = false) {
SignalHandler::instance()->set();
std::atomic<size_t> iter(0);

#if defined(_OPENMP)
if (getNumThreads() > 0) {omp_set_num_threads(getNumThreads());}
#endif

// -- query evaluation --
SignalHandler::instance()->setMsg(R"_(num_letters(3).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [11:1-11:16])_");
[&](){
CREATE_OP_CONTEXT(rel_22_num_letters_op_ctxt,rel_22_num_letters->createContext());
Tuple<RamDomain,1> tuple{{ramBitCast(RamSigned(3))}};
rel_22_num_letters->insert(tuple,READ_OP_CONTEXT(rel_22_num_letters_op_ctxt));
}
();SignalHandler::instance()->setMsg(R"_(n(0).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [24:1-24:6])_");
[&](){
CREATE_OP_CONTEXT(rel_21_n_op_ctxt,rel_21_n->createContext());
Tuple<RamDomain,1> tuple{{ramBitCast(RamSigned(0))}};
rel_21_n->insert(tuple,READ_OP_CONTEXT(rel_21_n_op_ctxt));
}
();[&](){
CREATE_OP_CONTEXT(rel_1_delta_n_op_ctxt,rel_1_delta_n->createContext());
CREATE_OP_CONTEXT(rel_21_n_op_ctxt,rel_21_n->createContext());
for(const auto& env0 : *rel_21_n) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_1_delta_n->insert(tuple,READ_OP_CONTEXT(rel_1_delta_n_op_ctxt));
}
}
();iter = 0;
for(;;) {
SignalHandler::instance()->setMsg(R"_(n((x+1)) :- 
   n(x),
   x < 120.
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [25:1-25:25])_");
if(!(rel_1_delta_n->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_1_delta_n_op_ctxt,rel_1_delta_n->createContext());
CREATE_OP_CONTEXT(rel_10_new_n_op_ctxt,rel_10_new_n->createContext());
CREATE_OP_CONTEXT(rel_21_n_op_ctxt,rel_21_n->createContext());
Tuple<RamDomain,1> lower{{0}};
Tuple<RamDomain,1> upper{{ramBitCast((ramBitCast<RamSigned>(RamSigned(120)) - ramBitCast<RamSigned>(RamSigned(1))))}};
lower[0] = MIN_RAM_SIGNED;
auto range = rel_1_delta_n->lowerUpperRange_2(lower, upper,READ_OP_CONTEXT(rel_1_delta_n_op_ctxt));
for(const auto& env0 : range) {
if( !(rel_21_n->contains(Tuple<RamDomain,1>{{ramBitCast((ramBitCast<RamSigned>(env0[0]) + ramBitCast<RamSigned>(RamSigned(1))))}},READ_OP_CONTEXT(rel_21_n_op_ctxt)))) {
Tuple<RamDomain,1> tuple{{ramBitCast((ramBitCast<RamSigned>(env0[0]) + ramBitCast<RamSigned>(RamSigned(1))))}};
rel_10_new_n->insert(tuple,READ_OP_CONTEXT(rel_10_new_n_op_ctxt));
}
}
}
();}
if(rel_10_new_n->empty()) break;
[&](){
CREATE_OP_CONTEXT(rel_10_new_n_op_ctxt,rel_10_new_n->createContext());
CREATE_OP_CONTEXT(rel_21_n_op_ctxt,rel_21_n->createContext());
for(const auto& env0 : *rel_10_new_n) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_21_n->insert(tuple,READ_OP_CONTEXT(rel_21_n_op_ctxt));
}
}
();std::swap(rel_1_delta_n, rel_10_new_n);
rel_10_new_n->purge();
iter++;
}
iter = 0;
rel_1_delta_n->purge();
rel_10_new_n->purge();
SignalHandler::instance()->setMsg(R"_(s(x,(x+1)) :- 
   n(x).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [29:1-29:18])_");
if(!(rel_21_n->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_31_s_op_ctxt,rel_31_s->createContext());
CREATE_OP_CONTEXT(rel_21_n_op_ctxt,rel_21_n->createContext());
for(const auto& env0 : *rel_21_n) {
Tuple<RamDomain,2> tuple{{ramBitCast(env0[0]),ramBitCast((ramBitCast<RamSigned>(env0[0]) + ramBitCast<RamSigned>(RamSigned(1))))}};
rel_31_s->insert(tuple,READ_OP_CONTEXT(rel_31_s_op_ctxt));
}
}
();}
SignalHandler::instance()->setMsg(R"_(op_add(x,0,x) :- 
   n(x).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [33:1-33:23])_");
if(!(rel_21_n->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_23_op_add_op_ctxt,rel_23_op_add->createContext());
CREATE_OP_CONTEXT(rel_21_n_op_ctxt,rel_21_n->createContext());
for(const auto& env0 : *rel_21_n) {
Tuple<RamDomain,3> tuple{{ramBitCast(env0[0]),ramBitCast(RamSigned(0)),ramBitCast(env0[0])}};
rel_23_op_add->insert(tuple,READ_OP_CONTEXT(rel_23_op_add_op_ctxt));
}
}
();}
[&](){
CREATE_OP_CONTEXT(rel_23_op_add_op_ctxt,rel_23_op_add->createContext());
CREATE_OP_CONTEXT(rel_2_delta_op_add_op_ctxt,rel_2_delta_op_add->createContext());
for(const auto& env0 : *rel_23_op_add) {
Tuple<RamDomain,3> tuple{{ramBitCast(env0[0]),ramBitCast(env0[1]),ramBitCast(env0[2])}};
rel_2_delta_op_add->insert(tuple,READ_OP_CONTEXT(rel_2_delta_op_add_op_ctxt));
}
}
();iter = 0;
for(;;) {
SignalHandler::instance()->setMsg(R"_(op_add(x,y,r) :- 
   s(py,y),
   op_add(x,py,pr),
   s(pr,r).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [34:1-34:52])_");
if(!(rel_31_s->empty()) && !(rel_2_delta_op_add->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_31_s_op_ctxt,rel_31_s->createContext());
CREATE_OP_CONTEXT(rel_23_op_add_op_ctxt,rel_23_op_add->createContext());
CREATE_OP_CONTEXT(rel_2_delta_op_add_op_ctxt,rel_2_delta_op_add->createContext());
CREATE_OP_CONTEXT(rel_11_new_op_add_op_ctxt,rel_11_new_op_add->createContext());
for(const auto& env0 : *rel_31_s) {
Tuple<RamDomain,3> lower{{0,ramBitCast(env0[0]),0}};
Tuple<RamDomain,3> upper{{0,ramBitCast(env0[0]),0}};
lower[0] = MIN_RAM_SIGNED;
upper[0] = MAX_RAM_SIGNED;
lower[2] = MIN_RAM_SIGNED;
upper[2] = MAX_RAM_SIGNED;
auto range = rel_2_delta_op_add->lowerUpperRange_010(lower, upper,READ_OP_CONTEXT(rel_2_delta_op_add_op_ctxt));
for(const auto& env1 : range) {
Tuple<RamDomain,2> lower{{ramBitCast(env1[2]),0}};
Tuple<RamDomain,2> upper{{ramBitCast(env1[2]),0}};
lower[1] = MIN_RAM_SIGNED;
upper[1] = MAX_RAM_SIGNED;
auto range = rel_31_s->lowerUpperRange_10(lower, upper,READ_OP_CONTEXT(rel_31_s_op_ctxt));
for(const auto& env2 : range) {
if( !(rel_23_op_add->contains(Tuple<RamDomain,3>{{ramBitCast(env1[0]),ramBitCast(env0[1]),ramBitCast(env2[1])}},READ_OP_CONTEXT(rel_23_op_add_op_ctxt)))) {
Tuple<RamDomain,3> tuple{{ramBitCast(env1[0]),ramBitCast(env0[1]),ramBitCast(env2[1])}};
rel_11_new_op_add->insert(tuple,READ_OP_CONTEXT(rel_11_new_op_add_op_ctxt));
}
}
}
}
}
();}
if(rel_11_new_op_add->empty()) break;
[&](){
CREATE_OP_CONTEXT(rel_23_op_add_op_ctxt,rel_23_op_add->createContext());
CREATE_OP_CONTEXT(rel_11_new_op_add_op_ctxt,rel_11_new_op_add->createContext());
for(const auto& env0 : *rel_11_new_op_add) {
Tuple<RamDomain,3> tuple{{ramBitCast(env0[0]),ramBitCast(env0[1]),ramBitCast(env0[2])}};
rel_23_op_add->insert(tuple,READ_OP_CONTEXT(rel_23_op_add_op_ctxt));
}
}
();std::swap(rel_2_delta_op_add, rel_11_new_op_add);
rel_11_new_op_add->purge();
iter++;
}
iter = 0;
rel_2_delta_op_add->purge();
rel_11_new_op_add->purge();
SignalHandler::instance()->setMsg(R"_(op_mul(x,0,0) :- 
   n(x).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [38:1-38:23])_");
if(!(rel_21_n->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_27_op_mul_op_ctxt,rel_27_op_mul->createContext());
CREATE_OP_CONTEXT(rel_21_n_op_ctxt,rel_21_n->createContext());
for(const auto& env0 : *rel_21_n) {
Tuple<RamDomain,3> tuple{{ramBitCast(env0[0]),ramBitCast(RamSigned(0)),ramBitCast(RamSigned(0))}};
rel_27_op_mul->insert(tuple,READ_OP_CONTEXT(rel_27_op_mul_op_ctxt));
}
}
();}
[&](){
CREATE_OP_CONTEXT(rel_27_op_mul_op_ctxt,rel_27_op_mul->createContext());
CREATE_OP_CONTEXT(rel_4_delta_op_mul_op_ctxt,rel_4_delta_op_mul->createContext());
for(const auto& env0 : *rel_27_op_mul) {
Tuple<RamDomain,3> tuple{{ramBitCast(env0[0]),ramBitCast(env0[1]),ramBitCast(env0[2])}};
rel_4_delta_op_mul->insert(tuple,READ_OP_CONTEXT(rel_4_delta_op_mul_op_ctxt));
}
}
();iter = 0;
for(;;) {
SignalHandler::instance()->setMsg(R"_(op_mul(x,y,r) :- 
   s(py,y),
   op_mul(x,py,pr),
   op_add(pr,x,r).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [39:1-39:59])_");
if(!(rel_23_op_add->empty()) && !(rel_31_s->empty()) && !(rel_4_delta_op_mul->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_31_s_op_ctxt,rel_31_s->createContext());
CREATE_OP_CONTEXT(rel_27_op_mul_op_ctxt,rel_27_op_mul->createContext());
CREATE_OP_CONTEXT(rel_4_delta_op_mul_op_ctxt,rel_4_delta_op_mul->createContext());
CREATE_OP_CONTEXT(rel_13_new_op_mul_op_ctxt,rel_13_new_op_mul->createContext());
CREATE_OP_CONTEXT(rel_23_op_add_op_ctxt,rel_23_op_add->createContext());
for(const auto& env0 : *rel_31_s) {
Tuple<RamDomain,3> lower{{0,ramBitCast(env0[0]),0}};
Tuple<RamDomain,3> upper{{0,ramBitCast(env0[0]),0}};
lower[0] = MIN_RAM_SIGNED;
upper[0] = MAX_RAM_SIGNED;
lower[2] = MIN_RAM_SIGNED;
upper[2] = MAX_RAM_SIGNED;
auto range = rel_4_delta_op_mul->lowerUpperRange_010(lower, upper,READ_OP_CONTEXT(rel_4_delta_op_mul_op_ctxt));
for(const auto& env1 : range) {
Tuple<RamDomain,3> lower{{ramBitCast(env1[2]),ramBitCast(env1[0]),0}};
Tuple<RamDomain,3> upper{{ramBitCast(env1[2]),ramBitCast(env1[0]),0}};
lower[2] = MIN_RAM_SIGNED;
upper[2] = MAX_RAM_SIGNED;
auto range = rel_23_op_add->lowerUpperRange_110(lower, upper,READ_OP_CONTEXT(rel_23_op_add_op_ctxt));
for(const auto& env2 : range) {
if( !(rel_27_op_mul->contains(Tuple<RamDomain,3>{{ramBitCast(env1[0]),ramBitCast(env0[1]),ramBitCast(env2[2])}},READ_OP_CONTEXT(rel_27_op_mul_op_ctxt)))) {
Tuple<RamDomain,3> tuple{{ramBitCast(env1[0]),ramBitCast(env0[1]),ramBitCast(env2[2])}};
rel_13_new_op_mul->insert(tuple,READ_OP_CONTEXT(rel_13_new_op_mul_op_ctxt));
}
}
}
}
}
();}
if(rel_13_new_op_mul->empty()) break;
[&](){
CREATE_OP_CONTEXT(rel_27_op_mul_op_ctxt,rel_27_op_mul->createContext());
CREATE_OP_CONTEXT(rel_13_new_op_mul_op_ctxt,rel_13_new_op_mul->createContext());
for(const auto& env0 : *rel_13_new_op_mul) {
Tuple<RamDomain,3> tuple{{ramBitCast(env0[0]),ramBitCast(env0[1]),ramBitCast(env0[2])}};
rel_27_op_mul->insert(tuple,READ_OP_CONTEXT(rel_27_op_mul_op_ctxt));
}
}
();std::swap(rel_4_delta_op_mul, rel_13_new_op_mul);
rel_13_new_op_mul->purge();
iter++;
}
iter = 0;
rel_4_delta_op_mul->purge();
rel_13_new_op_mul->purge();
SignalHandler::instance()->setMsg(R"_(op_exp(x,0,1) :- 
   n(x).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [44:1-44:23])_");
if(!(rel_21_n->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_25_op_exp_op_ctxt,rel_25_op_exp->createContext());
CREATE_OP_CONTEXT(rel_21_n_op_ctxt,rel_21_n->createContext());
for(const auto& env0 : *rel_21_n) {
Tuple<RamDomain,3> tuple{{ramBitCast(env0[0]),ramBitCast(RamSigned(0)),ramBitCast(RamSigned(1))}};
rel_25_op_exp->insert(tuple,READ_OP_CONTEXT(rel_25_op_exp_op_ctxt));
}
}
();}
[&](){
CREATE_OP_CONTEXT(rel_25_op_exp_op_ctxt,rel_25_op_exp->createContext());
CREATE_OP_CONTEXT(rel_3_delta_op_exp_op_ctxt,rel_3_delta_op_exp->createContext());
for(const auto& env0 : *rel_25_op_exp) {
Tuple<RamDomain,3> tuple{{ramBitCast(env0[0]),ramBitCast(env0[1]),ramBitCast(env0[2])}};
rel_3_delta_op_exp->insert(tuple,READ_OP_CONTEXT(rel_3_delta_op_exp_op_ctxt));
}
}
();iter = 0;
for(;;) {
SignalHandler::instance()->setMsg(R"_(op_exp(x,y,r) :- 
   s(py,y),
   op_exp(x,py,pr),
   op_mul(pr,x,r).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [45:1-45:59])_");
if(!(rel_27_op_mul->empty()) && !(rel_31_s->empty()) && !(rel_3_delta_op_exp->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_31_s_op_ctxt,rel_31_s->createContext());
CREATE_OP_CONTEXT(rel_27_op_mul_op_ctxt,rel_27_op_mul->createContext());
CREATE_OP_CONTEXT(rel_25_op_exp_op_ctxt,rel_25_op_exp->createContext());
CREATE_OP_CONTEXT(rel_3_delta_op_exp_op_ctxt,rel_3_delta_op_exp->createContext());
CREATE_OP_CONTEXT(rel_12_new_op_exp_op_ctxt,rel_12_new_op_exp->createContext());
for(const auto& env0 : *rel_31_s) {
Tuple<RamDomain,3> lower{{0,ramBitCast(env0[0]),0}};
Tuple<RamDomain,3> upper{{0,ramBitCast(env0[0]),0}};
lower[0] = MIN_RAM_SIGNED;
upper[0] = MAX_RAM_SIGNED;
lower[2] = MIN_RAM_SIGNED;
upper[2] = MAX_RAM_SIGNED;
auto range = rel_3_delta_op_exp->lowerUpperRange_010(lower, upper,READ_OP_CONTEXT(rel_3_delta_op_exp_op_ctxt));
for(const auto& env1 : range) {
Tuple<RamDomain,3> lower{{ramBitCast(env1[2]),ramBitCast(env1[0]),0}};
Tuple<RamDomain,3> upper{{ramBitCast(env1[2]),ramBitCast(env1[0]),0}};
lower[2] = MIN_RAM_SIGNED;
upper[2] = MAX_RAM_SIGNED;
auto range = rel_27_op_mul->lowerUpperRange_110(lower, upper,READ_OP_CONTEXT(rel_27_op_mul_op_ctxt));
for(const auto& env2 : range) {
if( !(rel_25_op_exp->contains(Tuple<RamDomain,3>{{ramBitCast(env1[0]),ramBitCast(env0[1]),ramBitCast(env2[2])}},READ_OP_CONTEXT(rel_25_op_exp_op_ctxt)))) {
Tuple<RamDomain,3> tuple{{ramBitCast(env1[0]),ramBitCast(env0[1]),ramBitCast(env2[2])}};
rel_12_new_op_exp->insert(tuple,READ_OP_CONTEXT(rel_12_new_op_exp_op_ctxt));
}
}
}
}
}
();}
if(rel_12_new_op_exp->empty()) break;
[&](){
CREATE_OP_CONTEXT(rel_25_op_exp_op_ctxt,rel_25_op_exp->createContext());
CREATE_OP_CONTEXT(rel_12_new_op_exp_op_ctxt,rel_12_new_op_exp->createContext());
for(const auto& env0 : *rel_12_new_op_exp) {
Tuple<RamDomain,3> tuple{{ramBitCast(env0[0]),ramBitCast(env0[1]),ramBitCast(env0[2])}};
rel_25_op_exp->insert(tuple,READ_OP_CONTEXT(rel_25_op_exp_op_ctxt));
}
}
();std::swap(rel_3_delta_op_exp, rel_12_new_op_exp);
rel_12_new_op_exp->purge();
iter++;
}
iter = 0;
rel_3_delta_op_exp->purge();
rel_12_new_op_exp->purge();
SignalHandler::instance()->setMsg(R"_(trie_level_end(0,0).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [76:1-76:21])_");
[&](){
CREATE_OP_CONTEXT(rel_37_trie_level_end_op_ctxt,rel_37_trie_level_end->createContext());
Tuple<RamDomain,2> tuple{{ramBitCast(RamSigned(0)),ramBitCast(RamSigned(0))}};
rel_37_trie_level_end->insert(tuple,READ_OP_CONTEXT(rel_37_trie_level_end_op_ctxt));
}
();[&](){
CREATE_OP_CONTEXT(rel_9_delta_trie_level_end_op_ctxt,rel_9_delta_trie_level_end->createContext());
CREATE_OP_CONTEXT(rel_37_trie_level_end_op_ctxt,rel_37_trie_level_end->createContext());
for(const auto& env0 : *rel_37_trie_level_end) {
Tuple<RamDomain,2> tuple{{ramBitCast(env0[0]),ramBitCast(env0[1])}};
rel_9_delta_trie_level_end->insert(tuple,READ_OP_CONTEXT(rel_9_delta_trie_level_end_op_ctxt));
}
}
();iter = 0;
for(;;) {
SignalHandler::instance()->setMsg(R"_(trie_level_end(l,i) :- 
   num_letters(n),
   s(pl,l),
   trie_level_end(pl,b),
   op_exp(n,l,p),
   op_add(b,p,i).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [77:1-77:100])_");
if(!(rel_23_op_add->empty()) && !(rel_25_op_exp->empty()) && !(rel_9_delta_trie_level_end->empty()) && !(rel_22_num_letters->empty()) && !(rel_31_s->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_22_num_letters_op_ctxt,rel_22_num_letters->createContext());
CREATE_OP_CONTEXT(rel_31_s_op_ctxt,rel_31_s->createContext());
CREATE_OP_CONTEXT(rel_25_op_exp_op_ctxt,rel_25_op_exp->createContext());
CREATE_OP_CONTEXT(rel_23_op_add_op_ctxt,rel_23_op_add->createContext());
CREATE_OP_CONTEXT(rel_18_new_trie_level_end_op_ctxt,rel_18_new_trie_level_end->createContext());
CREATE_OP_CONTEXT(rel_9_delta_trie_level_end_op_ctxt,rel_9_delta_trie_level_end->createContext());
CREATE_OP_CONTEXT(rel_37_trie_level_end_op_ctxt,rel_37_trie_level_end->createContext());
for(const auto& env0 : *rel_22_num_letters) {
for(const auto& env1 : *rel_31_s) {
Tuple<RamDomain,2> lower{{ramBitCast(env1[0]),0}};
Tuple<RamDomain,2> upper{{ramBitCast(env1[0]),0}};
lower[1] = MIN_RAM_SIGNED;
upper[1] = MAX_RAM_SIGNED;
auto range = rel_9_delta_trie_level_end->lowerUpperRange_10(lower, upper,READ_OP_CONTEXT(rel_9_delta_trie_level_end_op_ctxt));
for(const auto& env2 : range) {
Tuple<RamDomain,3> lower{{ramBitCast(env0[0]),ramBitCast(env1[1]),0}};
Tuple<RamDomain,3> upper{{ramBitCast(env0[0]),ramBitCast(env1[1]),0}};
lower[2] = MIN_RAM_SIGNED;
upper[2] = MAX_RAM_SIGNED;
auto range = rel_25_op_exp->lowerUpperRange_110(lower, upper,READ_OP_CONTEXT(rel_25_op_exp_op_ctxt));
for(const auto& env3 : range) {
Tuple<RamDomain,3> lower{{ramBitCast(env2[1]),ramBitCast(env3[2]),0}};
Tuple<RamDomain,3> upper{{ramBitCast(env2[1]),ramBitCast(env3[2]),0}};
lower[2] = MIN_RAM_SIGNED;
upper[2] = MAX_RAM_SIGNED;
auto range = rel_23_op_add->lowerUpperRange_110(lower, upper,READ_OP_CONTEXT(rel_23_op_add_op_ctxt));
for(const auto& env4 : range) {
if( !(rel_37_trie_level_end->contains(Tuple<RamDomain,2>{{ramBitCast(env1[1]),ramBitCast(env4[2])}},READ_OP_CONTEXT(rel_37_trie_level_end_op_ctxt)))) {
Tuple<RamDomain,2> tuple{{ramBitCast(env1[1]),ramBitCast(env4[2])}};
rel_18_new_trie_level_end->insert(tuple,READ_OP_CONTEXT(rel_18_new_trie_level_end_op_ctxt));
}
}
}
}
}
}
}
();}
if(rel_18_new_trie_level_end->empty()) break;
[&](){
CREATE_OP_CONTEXT(rel_18_new_trie_level_end_op_ctxt,rel_18_new_trie_level_end->createContext());
CREATE_OP_CONTEXT(rel_37_trie_level_end_op_ctxt,rel_37_trie_level_end->createContext());
for(const auto& env0 : *rel_18_new_trie_level_end) {
Tuple<RamDomain,2> tuple{{ramBitCast(env0[0]),ramBitCast(env0[1])}};
rel_37_trie_level_end->insert(tuple,READ_OP_CONTEXT(rel_37_trie_level_end_op_ctxt));
}
}
();std::swap(rel_9_delta_trie_level_end, rel_18_new_trie_level_end);
rel_18_new_trie_level_end->purge();
iter++;
}
iter = 0;
rel_9_delta_trie_level_end->purge();
rel_18_new_trie_level_end->purge();
if (performIO) rel_25_op_exp->purge();
SignalHandler::instance()->setMsg(R"_(trie_level_start(0,0).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [80:1-80:23])_");
[&](){
CREATE_OP_CONTEXT(rel_38_trie_level_start_op_ctxt,rel_38_trie_level_start->createContext());
Tuple<RamDomain,2> tuple{{ramBitCast(RamSigned(0)),ramBitCast(RamSigned(0))}};
rel_38_trie_level_start->insert(tuple,READ_OP_CONTEXT(rel_38_trie_level_start_op_ctxt));
}
();SignalHandler::instance()->setMsg(R"_(trie_level_start(l,i) :- 
   s(pl,l),
   trie_level_end(pl,b),
   op_add(b,1,i).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [81:1-81:71])_");
if(!(rel_23_op_add->empty()) && !(rel_31_s->empty()) && !(rel_37_trie_level_end->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_31_s_op_ctxt,rel_31_s->createContext());
CREATE_OP_CONTEXT(rel_23_op_add_op_ctxt,rel_23_op_add->createContext());
CREATE_OP_CONTEXT(rel_38_trie_level_start_op_ctxt,rel_38_trie_level_start->createContext());
CREATE_OP_CONTEXT(rel_37_trie_level_end_op_ctxt,rel_37_trie_level_end->createContext());
for(const auto& env0 : *rel_31_s) {
Tuple<RamDomain,2> lower{{ramBitCast(env0[0]),0}};
Tuple<RamDomain,2> upper{{ramBitCast(env0[0]),0}};
lower[1] = MIN_RAM_SIGNED;
upper[1] = MAX_RAM_SIGNED;
auto range = rel_37_trie_level_end->lowerUpperRange_10(lower, upper,READ_OP_CONTEXT(rel_37_trie_level_end_op_ctxt));
for(const auto& env1 : range) {
Tuple<RamDomain,3> lower{{ramBitCast(env1[1]),ramBitCast(RamSigned(1)),0}};
Tuple<RamDomain,3> upper{{ramBitCast(env1[1]),ramBitCast(RamSigned(1)),0}};
lower[2] = MIN_RAM_SIGNED;
upper[2] = MAX_RAM_SIGNED;
auto range = rel_23_op_add->lowerUpperRange_110(lower, upper,READ_OP_CONTEXT(rel_23_op_add_op_ctxt));
for(const auto& env2 : range) {
Tuple<RamDomain,2> tuple{{ramBitCast(env0[1]),ramBitCast(env2[2])}};
rel_38_trie_level_start->insert(tuple,READ_OP_CONTEXT(rel_38_trie_level_start_op_ctxt));
}
}
}
}
();}
SignalHandler::instance()->setMsg(R"_(trie_level(0,0).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [85:1-85:17])_");
[&](){
CREATE_OP_CONTEXT(rel_36_trie_level_op_ctxt,rel_36_trie_level->createContext());
Tuple<RamDomain,2> tuple{{ramBitCast(RamSigned(0)),ramBitCast(RamSigned(0))}};
rel_36_trie_level->insert(tuple,READ_OP_CONTEXT(rel_36_trie_level_op_ctxt));
}
();SignalHandler::instance()->setMsg(R"_(trie_level(i,b) :- 
   n(i),
   s(a,b),
   trie_level_end(a,low),
   trie_level_end(b,high),
   low < i,
   i <= high.
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [86:1-86:96])_");
if(!(rel_31_s->empty()) && !(rel_37_trie_level_end->empty()) && !(rel_21_n->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_31_s_op_ctxt,rel_31_s->createContext());
CREATE_OP_CONTEXT(rel_36_trie_level_op_ctxt,rel_36_trie_level->createContext());
CREATE_OP_CONTEXT(rel_21_n_op_ctxt,rel_21_n->createContext());
CREATE_OP_CONTEXT(rel_37_trie_level_end_op_ctxt,rel_37_trie_level_end->createContext());
for(const auto& env0 : *rel_21_n) {
for(const auto& env1 : *rel_31_s) {
Tuple<RamDomain,2> lower{{ramBitCast(env1[0]),0}};
Tuple<RamDomain,2> upper{{ramBitCast(env1[0]),ramBitCast((ramBitCast<RamSigned>(env0[0]) - ramBitCast<RamSigned>(RamSigned(1))))}};
lower[1] = MIN_RAM_SIGNED;
auto range = rel_37_trie_level_end->lowerUpperRange_12(lower, upper,READ_OP_CONTEXT(rel_37_trie_level_end_op_ctxt));
for(const auto& env2 : range) {
Tuple<RamDomain,2> lower{{ramBitCast(env1[1]),ramBitCast(env0[0])}};
Tuple<RamDomain,2> upper{{ramBitCast(env1[1]),0}};
upper[1] = MAX_RAM_SIGNED;
auto range = rel_37_trie_level_end->lowerUpperRange_12(lower, upper,READ_OP_CONTEXT(rel_37_trie_level_end_op_ctxt));
for(const auto& env3 : range) {
Tuple<RamDomain,2> tuple{{ramBitCast(env0[0]),ramBitCast(env1[1])}};
rel_36_trie_level->insert(tuple,READ_OP_CONTEXT(rel_36_trie_level_op_ctxt));
}
}
}
}
}
();}
[&](){
CREATE_OP_CONTEXT(rel_36_trie_level_op_ctxt,rel_36_trie_level->createContext());
CREATE_OP_CONTEXT(rel_8_delta_trie_level_op_ctxt,rel_8_delta_trie_level->createContext());
for(const auto& env0 : *rel_36_trie_level) {
Tuple<RamDomain,2> tuple{{ramBitCast(env0[0]),ramBitCast(env0[1])}};
rel_8_delta_trie_level->insert(tuple,READ_OP_CONTEXT(rel_8_delta_trie_level_op_ctxt));
}
}
();iter = 0;
for(;;) {
SignalHandler::instance()->setMsg(R"_(trie_level(id,l) :- 
   trie_level(id,l).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [104:1-104:36])_");
if(!(rel_8_delta_trie_level->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_17_new_trie_level_op_ctxt,rel_17_new_trie_level->createContext());
CREATE_OP_CONTEXT(rel_36_trie_level_op_ctxt,rel_36_trie_level->createContext());
CREATE_OP_CONTEXT(rel_8_delta_trie_level_op_ctxt,rel_8_delta_trie_level->createContext());
for(const auto& env0 : *rel_8_delta_trie_level) {
if( !(rel_36_trie_level->contains(Tuple<RamDomain,2>{{ramBitCast(env0[0]),ramBitCast(env0[1])}},READ_OP_CONTEXT(rel_36_trie_level_op_ctxt)))) {
Tuple<RamDomain,2> tuple{{ramBitCast(env0[0]),ramBitCast(env0[1])}};
rel_17_new_trie_level->insert(tuple,READ_OP_CONTEXT(rel_17_new_trie_level_op_ctxt));
}
}
}
();}
if(rel_17_new_trie_level->empty()) break;
[&](){
CREATE_OP_CONTEXT(rel_17_new_trie_level_op_ctxt,rel_17_new_trie_level->createContext());
CREATE_OP_CONTEXT(rel_36_trie_level_op_ctxt,rel_36_trie_level->createContext());
for(const auto& env0 : *rel_17_new_trie_level) {
Tuple<RamDomain,2> tuple{{ramBitCast(env0[0]),ramBitCast(env0[1])}};
rel_36_trie_level->insert(tuple,READ_OP_CONTEXT(rel_36_trie_level_op_ctxt));
}
}
();std::swap(rel_8_delta_trie_level, rel_17_new_trie_level);
rel_17_new_trie_level->purge();
iter++;
}
iter = 0;
rel_8_delta_trie_level->purge();
rel_17_new_trie_level->purge();
if (performIO) rel_37_trie_level_end->purge();
SignalHandler::instance()->setMsg(R"_(op_div(x,y,r) :- 
   op_mul(r,y,a),
   op_add(a,z,x),
   0 <= z,
   z < y.
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [55:1-55:58])_");
if(!(rel_27_op_mul->empty()) && !(rel_23_op_add->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_27_op_mul_op_ctxt,rel_27_op_mul->createContext());
CREATE_OP_CONTEXT(rel_24_op_div_op_ctxt,rel_24_op_div->createContext());
CREATE_OP_CONTEXT(rel_23_op_add_op_ctxt,rel_23_op_add->createContext());
for(const auto& env0 : *rel_27_op_mul) {
Tuple<RamDomain,3> lower{{ramBitCast(env0[2]),ramBitCast(RamSigned(0)),0}};
Tuple<RamDomain,3> upper{{ramBitCast(env0[2]),ramBitCast((ramBitCast<RamSigned>(env0[1]) - ramBitCast<RamSigned>(RamSigned(1)))),0}};
lower[2] = MIN_RAM_SIGNED;
upper[2] = MAX_RAM_SIGNED;
auto range = rel_23_op_add->lowerUpperRange_120(lower, upper,READ_OP_CONTEXT(rel_23_op_add_op_ctxt));
for(const auto& env1 : range) {
Tuple<RamDomain,3> tuple{{ramBitCast(env1[2]),ramBitCast(env0[1]),ramBitCast(env0[0])}};
rel_24_op_div->insert(tuple,READ_OP_CONTEXT(rel_24_op_div_op_ctxt));
}
}
}
();}
SignalHandler::instance()->setMsg(R"_(trie_parent(i,p) :- 
   num_letters(n),
   trie_level(i,l),
   s(pl,l),
   trie_level_start(l,b),
   op_add(b,x,i),
   op_div(x,n,o),
   trie_level_start(pl,c),
   op_add(c,o,p).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [89:1-89:154])_");
if(!(rel_36_trie_level->empty()) && !(rel_22_num_letters->empty()) && !(rel_31_s->empty()) && !(rel_24_op_div->empty()) && !(rel_23_op_add->empty()) && !(rel_38_trie_level_start->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_22_num_letters_op_ctxt,rel_22_num_letters->createContext());
CREATE_OP_CONTEXT(rel_31_s_op_ctxt,rel_31_s->createContext());
CREATE_OP_CONTEXT(rel_39_trie_parent_op_ctxt,rel_39_trie_parent->createContext());
CREATE_OP_CONTEXT(rel_36_trie_level_op_ctxt,rel_36_trie_level->createContext());
CREATE_OP_CONTEXT(rel_24_op_div_op_ctxt,rel_24_op_div->createContext());
CREATE_OP_CONTEXT(rel_23_op_add_op_ctxt,rel_23_op_add->createContext());
CREATE_OP_CONTEXT(rel_38_trie_level_start_op_ctxt,rel_38_trie_level_start->createContext());
for(const auto& env0 : *rel_22_num_letters) {
for(const auto& env1 : *rel_36_trie_level) {
Tuple<RamDomain,2> lower{{0,ramBitCast(env1[1])}};
Tuple<RamDomain,2> upper{{0,ramBitCast(env1[1])}};
lower[0] = MIN_RAM_SIGNED;
upper[0] = MAX_RAM_SIGNED;
auto range = rel_31_s->lowerUpperRange_01(lower, upper,READ_OP_CONTEXT(rel_31_s_op_ctxt));
for(const auto& env2 : range) {
Tuple<RamDomain,2> lower{{ramBitCast(env1[1]),0}};
Tuple<RamDomain,2> upper{{ramBitCast(env1[1]),0}};
lower[1] = MIN_RAM_SIGNED;
upper[1] = MAX_RAM_SIGNED;
auto range = rel_38_trie_level_start->lowerUpperRange_10(lower, upper,READ_OP_CONTEXT(rel_38_trie_level_start_op_ctxt));
for(const auto& env3 : range) {
Tuple<RamDomain,3> lower{{ramBitCast(env3[1]),0,ramBitCast(env1[0])}};
Tuple<RamDomain,3> upper{{ramBitCast(env3[1]),0,ramBitCast(env1[0])}};
lower[1] = MIN_RAM_SIGNED;
upper[1] = MAX_RAM_SIGNED;
auto range = rel_23_op_add->lowerUpperRange_101(lower, upper,READ_OP_CONTEXT(rel_23_op_add_op_ctxt));
for(const auto& env4 : range) {
Tuple<RamDomain,3> lower{{ramBitCast(env4[1]),ramBitCast(env0[0]),0}};
Tuple<RamDomain,3> upper{{ramBitCast(env4[1]),ramBitCast(env0[0]),0}};
lower[2] = MIN_RAM_SIGNED;
upper[2] = MAX_RAM_SIGNED;
auto range = rel_24_op_div->lowerUpperRange_110(lower, upper,READ_OP_CONTEXT(rel_24_op_div_op_ctxt));
for(const auto& env5 : range) {
Tuple<RamDomain,2> lower{{ramBitCast(env2[0]),0}};
Tuple<RamDomain,2> upper{{ramBitCast(env2[0]),0}};
lower[1] = MIN_RAM_SIGNED;
upper[1] = MAX_RAM_SIGNED;
auto range = rel_38_trie_level_start->lowerUpperRange_10(lower, upper,READ_OP_CONTEXT(rel_38_trie_level_start_op_ctxt));
for(const auto& env6 : range) {
Tuple<RamDomain,3> lower{{ramBitCast(env6[1]),ramBitCast(env5[2]),0}};
Tuple<RamDomain,3> upper{{ramBitCast(env6[1]),ramBitCast(env5[2]),0}};
lower[2] = MIN_RAM_SIGNED;
upper[2] = MAX_RAM_SIGNED;
auto range = rel_23_op_add->lowerUpperRange_110(lower, upper,READ_OP_CONTEXT(rel_23_op_add_op_ctxt));
for(const auto& env7 : range) {
Tuple<RamDomain,2> tuple{{ramBitCast(env1[0]),ramBitCast(env7[2])}};
rel_39_trie_parent->insert(tuple,READ_OP_CONTEXT(rel_39_trie_parent_op_ctxt));
}
}
}
}
}
}
}
}
}
();}
if (performIO) rel_24_op_div->purge();
if (performIO) rel_38_trie_level_start->purge();
SignalHandler::instance()->setMsg(R"_(op_mod(x,y,r) :- 
   op_mul(y,_,z),
   op_add(z,r,x),
   0 <= r,
   r < y.
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [59:1-59:58])_");
if(!(rel_27_op_mul->empty()) && !(rel_23_op_add->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_27_op_mul_op_ctxt,rel_27_op_mul->createContext());
CREATE_OP_CONTEXT(rel_23_op_add_op_ctxt,rel_23_op_add->createContext());
CREATE_OP_CONTEXT(rel_26_op_mod_op_ctxt,rel_26_op_mod->createContext());
for(const auto& env0 : *rel_27_op_mul) {
Tuple<RamDomain,3> lower{{ramBitCast(env0[2]),ramBitCast(RamSigned(0)),0}};
Tuple<RamDomain,3> upper{{ramBitCast(env0[2]),ramBitCast((ramBitCast<RamSigned>(env0[0]) - ramBitCast<RamSigned>(RamSigned(1)))),0}};
lower[2] = MIN_RAM_SIGNED;
upper[2] = MAX_RAM_SIGNED;
auto range = rel_23_op_add->lowerUpperRange_120(lower, upper,READ_OP_CONTEXT(rel_23_op_add_op_ctxt));
for(const auto& env1 : range) {
Tuple<RamDomain,3> tuple{{ramBitCast(env1[2]),ramBitCast(env0[0]),ramBitCast(env1[1])}};
rel_26_op_mod->insert(tuple,READ_OP_CONTEXT(rel_26_op_mod_op_ctxt));
}
}
}
();}
if (performIO) rel_23_op_add->purge();
if (performIO) rel_27_op_mul->purge();
SignalHandler::instance()->setMsg(R"_(idx(0,"a").
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [65:1-65:12])_");
[&](){
CREATE_OP_CONTEXT(rel_20_idx_op_ctxt,rel_20_idx->createContext());
Tuple<RamDomain,2> tuple{{ramBitCast(RamSigned(0)),ramBitCast(RamSigned(0))}};
rel_20_idx->insert(tuple,READ_OP_CONTEXT(rel_20_idx_op_ctxt));
}
();SignalHandler::instance()->setMsg(R"_(idx(1,"b").
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [66:1-66:12])_");
[&](){
CREATE_OP_CONTEXT(rel_20_idx_op_ctxt,rel_20_idx->createContext());
Tuple<RamDomain,2> tuple{{ramBitCast(RamSigned(1)),ramBitCast(RamSigned(1))}};
rel_20_idx->insert(tuple,READ_OP_CONTEXT(rel_20_idx_op_ctxt));
}
();SignalHandler::instance()->setMsg(R"_(idx(2,"c").
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [67:1-67:12])_");
[&](){
CREATE_OP_CONTEXT(rel_20_idx_op_ctxt,rel_20_idx->createContext());
Tuple<RamDomain,2> tuple{{ramBitCast(RamSigned(2)),ramBitCast(RamSigned(2))}};
rel_20_idx->insert(tuple,READ_OP_CONTEXT(rel_20_idx_op_ctxt));
}
();SignalHandler::instance()->setMsg(R"_(trie_letter(i,a) :- 
   s(x,i),
   num_letters(n),
   op_mod(x,n,r),
   idx(r,a).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [73:1-73:69])_");
if(!(rel_20_idx->empty()) && !(rel_26_op_mod->empty()) && !(rel_31_s->empty()) && !(rel_22_num_letters->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_22_num_letters_op_ctxt,rel_22_num_letters->createContext());
CREATE_OP_CONTEXT(rel_31_s_op_ctxt,rel_31_s->createContext());
CREATE_OP_CONTEXT(rel_26_op_mod_op_ctxt,rel_26_op_mod->createContext());
CREATE_OP_CONTEXT(rel_35_trie_letter_op_ctxt,rel_35_trie_letter->createContext());
CREATE_OP_CONTEXT(rel_20_idx_op_ctxt,rel_20_idx->createContext());
for(const auto& env0 : *rel_31_s) {
for(const auto& env1 : *rel_22_num_letters) {
Tuple<RamDomain,3> lower{{ramBitCast(env0[0]),ramBitCast(env1[0]),0}};
Tuple<RamDomain,3> upper{{ramBitCast(env0[0]),ramBitCast(env1[0]),0}};
lower[2] = MIN_RAM_SIGNED;
upper[2] = MAX_RAM_SIGNED;
auto range = rel_26_op_mod->lowerUpperRange_110(lower, upper,READ_OP_CONTEXT(rel_26_op_mod_op_ctxt));
for(const auto& env2 : range) {
Tuple<RamDomain,2> lower{{ramBitCast(env2[2]),0}};
Tuple<RamDomain,2> upper{{ramBitCast(env2[2]),0}};
lower[1] = MIN_RAM_SIGNED;
upper[1] = MAX_RAM_SIGNED;
auto range = rel_20_idx->lowerUpperRange_10(lower, upper,READ_OP_CONTEXT(rel_20_idx_op_ctxt));
for(const auto& env3 : range) {
Tuple<RamDomain,2> tuple{{ramBitCast(env0[1]),ramBitCast(env3[1])}};
rel_35_trie_letter->insert(tuple,READ_OP_CONTEXT(rel_35_trie_letter_op_ctxt));
}
}
}
}
}
();}
if (performIO) rel_22_num_letters->purge();
if (performIO) rel_26_op_mod->purge();
if (performIO) rel_20_idx->purge();
SignalHandler::instance()->setMsg(R"_(trie(x) :- 
   trie_letter(x,_).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [95:1-95:29])_");
if(!(rel_35_trie_letter->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_34_trie_op_ctxt,rel_34_trie->createContext());
CREATE_OP_CONTEXT(rel_35_trie_letter_op_ctxt,rel_35_trie_letter->createContext());
for(const auto& env0 : *rel_35_trie_letter) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_34_trie->insert(tuple,READ_OP_CONTEXT(rel_34_trie_op_ctxt));
}
}
();}
[&](){
CREATE_OP_CONTEXT(rel_34_trie_op_ctxt,rel_34_trie->createContext());
CREATE_OP_CONTEXT(rel_7_delta_trie_op_ctxt,rel_7_delta_trie->createContext());
for(const auto& env0 : *rel_34_trie) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_7_delta_trie->insert(tuple,READ_OP_CONTEXT(rel_7_delta_trie_op_ctxt));
}
}
();iter = 0;
for(;;) {
SignalHandler::instance()->setMsg(R"_(trie(x) :- 
   trie(x).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [101:1-101:19])_");
if(!(rel_7_delta_trie->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_34_trie_op_ctxt,rel_34_trie->createContext());
CREATE_OP_CONTEXT(rel_7_delta_trie_op_ctxt,rel_7_delta_trie->createContext());
CREATE_OP_CONTEXT(rel_16_new_trie_op_ctxt,rel_16_new_trie->createContext());
for(const auto& env0 : *rel_7_delta_trie) {
if( !(rel_34_trie->contains(Tuple<RamDomain,1>{{ramBitCast(env0[0])}},READ_OP_CONTEXT(rel_34_trie_op_ctxt)))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_16_new_trie->insert(tuple,READ_OP_CONTEXT(rel_16_new_trie_op_ctxt));
}
}
}
();}
if(rel_16_new_trie->empty()) break;
[&](){
CREATE_OP_CONTEXT(rel_34_trie_op_ctxt,rel_34_trie->createContext());
CREATE_OP_CONTEXT(rel_16_new_trie_op_ctxt,rel_16_new_trie->createContext());
for(const auto& env0 : *rel_16_new_trie) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_34_trie->insert(tuple,READ_OP_CONTEXT(rel_34_trie_op_ctxt));
}
}
();std::swap(rel_7_delta_trie, rel_16_new_trie);
rel_16_new_trie->purge();
iter++;
}
iter = 0;
rel_7_delta_trie->purge();
rel_16_new_trie->purge();
SignalHandler::instance()->setMsg(R"_(str_chain(id,id) :- 
   trie(id).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [107:1-107:30])_");
if(!(rel_34_trie->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_32_str_chain_op_ctxt,rel_32_str_chain->createContext());
CREATE_OP_CONTEXT(rel_34_trie_op_ctxt,rel_34_trie->createContext());
for(const auto& env0 : *rel_34_trie) {
Tuple<RamDomain,2> tuple{{ramBitCast(env0[0]),ramBitCast(env0[0])}};
rel_32_str_chain->insert(tuple,READ_OP_CONTEXT(rel_32_str_chain_op_ctxt));
}
}
();}
[&](){
CREATE_OP_CONTEXT(rel_32_str_chain_op_ctxt,rel_32_str_chain->createContext());
CREATE_OP_CONTEXT(rel_6_delta_str_chain_op_ctxt,rel_6_delta_str_chain->createContext());
for(const auto& env0 : *rel_32_str_chain) {
Tuple<RamDomain,2> tuple{{ramBitCast(env0[0]),ramBitCast(env0[1])}};
rel_6_delta_str_chain->insert(tuple,READ_OP_CONTEXT(rel_6_delta_str_chain_op_ctxt));
}
}
();iter = 0;
for(;;) {
SignalHandler::instance()->setMsg(R"_(str_chain(id,p) :- 
   str_chain(id,x),
   trie_parent(x,p).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [108:1-108:54])_");
if(!(rel_6_delta_str_chain->empty()) && !(rel_39_trie_parent->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_32_str_chain_op_ctxt,rel_32_str_chain->createContext());
CREATE_OP_CONTEXT(rel_39_trie_parent_op_ctxt,rel_39_trie_parent->createContext());
CREATE_OP_CONTEXT(rel_6_delta_str_chain_op_ctxt,rel_6_delta_str_chain->createContext());
CREATE_OP_CONTEXT(rel_15_new_str_chain_op_ctxt,rel_15_new_str_chain->createContext());
for(const auto& env0 : *rel_6_delta_str_chain) {
Tuple<RamDomain,2> lower{{ramBitCast(env0[1]),0}};
Tuple<RamDomain,2> upper{{ramBitCast(env0[1]),0}};
lower[1] = MIN_RAM_SIGNED;
upper[1] = MAX_RAM_SIGNED;
auto range = rel_39_trie_parent->lowerUpperRange_10(lower, upper,READ_OP_CONTEXT(rel_39_trie_parent_op_ctxt));
for(const auto& env1 : range) {
if( !(rel_32_str_chain->contains(Tuple<RamDomain,2>{{ramBitCast(env0[0]),ramBitCast(env1[1])}},READ_OP_CONTEXT(rel_32_str_chain_op_ctxt)))) {
Tuple<RamDomain,2> tuple{{ramBitCast(env0[0]),ramBitCast(env1[1])}};
rel_15_new_str_chain->insert(tuple,READ_OP_CONTEXT(rel_15_new_str_chain_op_ctxt));
}
}
}
}
();}
if(rel_15_new_str_chain->empty()) break;
[&](){
CREATE_OP_CONTEXT(rel_32_str_chain_op_ctxt,rel_32_str_chain->createContext());
CREATE_OP_CONTEXT(rel_15_new_str_chain_op_ctxt,rel_15_new_str_chain->createContext());
for(const auto& env0 : *rel_15_new_str_chain) {
Tuple<RamDomain,2> tuple{{ramBitCast(env0[0]),ramBitCast(env0[1])}};
rel_32_str_chain->insert(tuple,READ_OP_CONTEXT(rel_32_str_chain_op_ctxt));
}
}
();std::swap(rel_6_delta_str_chain, rel_15_new_str_chain);
rel_15_new_str_chain->purge();
iter++;
}
iter = 0;
rel_6_delta_str_chain->purge();
rel_15_new_str_chain->purge();
if (performIO) rel_39_trie_parent->purge();
SignalHandler::instance()->setMsg(R"_(str_letter_at(id,pos,l) :- 
   str_chain(id,p),
   trie_level(p,pos),
   trie_letter(p,l).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [111:1-111:81])_");
if(!(rel_35_trie_letter->empty()) && !(rel_32_str_chain->empty()) && !(rel_36_trie_level->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_32_str_chain_op_ctxt,rel_32_str_chain->createContext());
CREATE_OP_CONTEXT(rel_33_str_letter_at_op_ctxt,rel_33_str_letter_at->createContext());
CREATE_OP_CONTEXT(rel_36_trie_level_op_ctxt,rel_36_trie_level->createContext());
CREATE_OP_CONTEXT(rel_35_trie_letter_op_ctxt,rel_35_trie_letter->createContext());
for(const auto& env0 : *rel_32_str_chain) {
Tuple<RamDomain,2> lower{{ramBitCast(env0[1]),0}};
Tuple<RamDomain,2> upper{{ramBitCast(env0[1]),0}};
lower[1] = MIN_RAM_SIGNED;
upper[1] = MAX_RAM_SIGNED;
auto range = rel_36_trie_level->lowerUpperRange_10(lower, upper,READ_OP_CONTEXT(rel_36_trie_level_op_ctxt));
for(const auto& env1 : range) {
Tuple<RamDomain,2> lower{{ramBitCast(env0[1]),0}};
Tuple<RamDomain,2> upper{{ramBitCast(env0[1]),0}};
lower[1] = MIN_RAM_SIGNED;
upper[1] = MAX_RAM_SIGNED;
auto range = rel_35_trie_letter->lowerUpperRange_10(lower, upper,READ_OP_CONTEXT(rel_35_trie_letter_op_ctxt));
for(const auto& env2 : range) {
Tuple<RamDomain,3> tuple{{ramBitCast(env0[0]),ramBitCast(env1[1]),ramBitCast(env2[1])}};
rel_33_str_letter_at->insert(tuple,READ_OP_CONTEXT(rel_33_str_letter_at_op_ctxt));
}
}
}
}
();}
if (performIO) rel_35_trie_letter->purge();
if (performIO) rel_32_str_chain->purge();
SignalHandler::instance()->setMsg(R"_(palin_aux(s,x,x) :- 
   trie(s),
   n(x),
   trie_level(s,l),
   x <= l.
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [122:1-122:54])_");
if(!(rel_36_trie_level->empty()) && !(rel_34_trie->empty()) && !(rel_21_n->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_34_trie_op_ctxt,rel_34_trie->createContext());
CREATE_OP_CONTEXT(rel_36_trie_level_op_ctxt,rel_36_trie_level->createContext());
CREATE_OP_CONTEXT(rel_28_palin_aux_op_ctxt,rel_28_palin_aux->createContext());
CREATE_OP_CONTEXT(rel_21_n_op_ctxt,rel_21_n->createContext());
for(const auto& env0 : *rel_34_trie) {
for(const auto& env1 : *rel_21_n) {
Tuple<RamDomain,2> lower{{ramBitCast(env0[0]),ramBitCast(env1[0])}};
Tuple<RamDomain,2> upper{{ramBitCast(env0[0]),0}};
upper[1] = MAX_RAM_SIGNED;
auto range = rel_36_trie_level->lowerUpperRange_12(lower, upper,READ_OP_CONTEXT(rel_36_trie_level_op_ctxt));
for(const auto& env2 : range) {
Tuple<RamDomain,3> tuple{{ramBitCast(env0[0]),ramBitCast(env1[0]),ramBitCast(env1[0])}};
rel_28_palin_aux->insert(tuple,READ_OP_CONTEXT(rel_28_palin_aux_op_ctxt));
}
}
}
}
();}
SignalHandler::instance()->setMsg(R"_(palin_aux(s,x,(x+1)) :- 
   str_letter_at(s,x,_).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [123:1-123:44])_");
if(!(rel_33_str_letter_at->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_33_str_letter_at_op_ctxt,rel_33_str_letter_at->createContext());
CREATE_OP_CONTEXT(rel_28_palin_aux_op_ctxt,rel_28_palin_aux->createContext());
for(const auto& env0 : *rel_33_str_letter_at) {
Tuple<RamDomain,3> tuple{{ramBitCast(env0[0]),ramBitCast(env0[1]),ramBitCast((ramBitCast<RamSigned>(env0[1]) + ramBitCast<RamSigned>(RamSigned(1))))}};
rel_28_palin_aux->insert(tuple,READ_OP_CONTEXT(rel_28_palin_aux_op_ctxt));
}
}
();}
[&](){
CREATE_OP_CONTEXT(rel_5_delta_palin_aux_op_ctxt,rel_5_delta_palin_aux->createContext());
CREATE_OP_CONTEXT(rel_28_palin_aux_op_ctxt,rel_28_palin_aux->createContext());
for(const auto& env0 : *rel_28_palin_aux) {
Tuple<RamDomain,3> tuple{{ramBitCast(env0[0]),ramBitCast(env0[1]),ramBitCast(env0[2])}};
rel_5_delta_palin_aux->insert(tuple,READ_OP_CONTEXT(rel_5_delta_palin_aux_op_ctxt));
}
}
();iter = 0;
for(;;) {
SignalHandler::instance()->setMsg(R"_(palin_aux(s,x,sy) :- 
   str_letter_at(s,x,a),
   s(x,sx),
   palin_aux(s,sx,y),
   str_letter_at(s,y,a),
   s(y,sy).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [124:1-124:102])_");
if(!(rel_5_delta_palin_aux->empty()) && !(rel_31_s->empty()) && !(rel_33_str_letter_at->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_31_s_op_ctxt,rel_31_s->createContext());
CREATE_OP_CONTEXT(rel_33_str_letter_at_op_ctxt,rel_33_str_letter_at->createContext());
CREATE_OP_CONTEXT(rel_14_new_palin_aux_op_ctxt,rel_14_new_palin_aux->createContext());
CREATE_OP_CONTEXT(rel_5_delta_palin_aux_op_ctxt,rel_5_delta_palin_aux->createContext());
CREATE_OP_CONTEXT(rel_28_palin_aux_op_ctxt,rel_28_palin_aux->createContext());
for(const auto& env0 : *rel_33_str_letter_at) {
Tuple<RamDomain,2> lower{{ramBitCast(env0[1]),0}};
Tuple<RamDomain,2> upper{{ramBitCast(env0[1]),0}};
lower[1] = MIN_RAM_SIGNED;
upper[1] = MAX_RAM_SIGNED;
auto range = rel_31_s->lowerUpperRange_10(lower, upper,READ_OP_CONTEXT(rel_31_s_op_ctxt));
for(const auto& env1 : range) {
Tuple<RamDomain,3> lower{{ramBitCast(env0[0]),ramBitCast(env1[1]),0}};
Tuple<RamDomain,3> upper{{ramBitCast(env0[0]),ramBitCast(env1[1]),0}};
lower[2] = MIN_RAM_SIGNED;
upper[2] = MAX_RAM_SIGNED;
auto range = rel_5_delta_palin_aux->lowerUpperRange_110(lower, upper,READ_OP_CONTEXT(rel_5_delta_palin_aux_op_ctxt));
for(const auto& env2 : range) {
if( rel_33_str_letter_at->contains(Tuple<RamDomain,3>{{ramBitCast(env0[0]),ramBitCast(env2[2]),ramBitCast(env0[2])}},READ_OP_CONTEXT(rel_33_str_letter_at_op_ctxt))) {
Tuple<RamDomain,2> lower{{ramBitCast(env2[2]),0}};
Tuple<RamDomain,2> upper{{ramBitCast(env2[2]),0}};
lower[1] = MIN_RAM_SIGNED;
upper[1] = MAX_RAM_SIGNED;
auto range = rel_31_s->lowerUpperRange_10(lower, upper,READ_OP_CONTEXT(rel_31_s_op_ctxt));
for(const auto& env3 : range) {
if( !(rel_28_palin_aux->contains(Tuple<RamDomain,3>{{ramBitCast(env0[0]),ramBitCast(env0[1]),ramBitCast(env3[1])}},READ_OP_CONTEXT(rel_28_palin_aux_op_ctxt)))) {
Tuple<RamDomain,3> tuple{{ramBitCast(env0[0]),ramBitCast(env0[1]),ramBitCast(env3[1])}};
rel_14_new_palin_aux->insert(tuple,READ_OP_CONTEXT(rel_14_new_palin_aux_op_ctxt));
}
}
}
}
}
}
}
();}
if(rel_14_new_palin_aux->empty()) break;
[&](){
CREATE_OP_CONTEXT(rel_14_new_palin_aux_op_ctxt,rel_14_new_palin_aux->createContext());
CREATE_OP_CONTEXT(rel_28_palin_aux_op_ctxt,rel_28_palin_aux->createContext());
for(const auto& env0 : *rel_14_new_palin_aux) {
Tuple<RamDomain,3> tuple{{ramBitCast(env0[0]),ramBitCast(env0[1]),ramBitCast(env0[2])}};
rel_28_palin_aux->insert(tuple,READ_OP_CONTEXT(rel_28_palin_aux_op_ctxt));
}
}
();std::swap(rel_5_delta_palin_aux, rel_14_new_palin_aux);
rel_14_new_palin_aux->purge();
iter++;
}
iter = 0;
rel_5_delta_palin_aux->purge();
rel_14_new_palin_aux->purge();
if (performIO) rel_21_n->purge();
if (performIO) rel_34_trie->purge();
SignalHandler::instance()->setMsg(R"_(palindrome(s) :- 
   trie_level(s,0).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [128:1-128:31])_");
if(!(rel_36_trie_level->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_29_palindrome_op_ctxt,rel_29_palindrome->createContext());
CREATE_OP_CONTEXT(rel_36_trie_level_op_ctxt,rel_36_trie_level->createContext());
Tuple<RamDomain,2> lower{{0,ramBitCast(RamSigned(0))}};
Tuple<RamDomain,2> upper{{0,ramBitCast(RamSigned(0))}};
lower[0] = MIN_RAM_SIGNED;
upper[0] = MAX_RAM_SIGNED;
auto range = rel_36_trie_level->lowerUpperRange_01(lower, upper,READ_OP_CONTEXT(rel_36_trie_level_op_ctxt));
for(const auto& env0 : range) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_29_palindrome->insert(tuple,READ_OP_CONTEXT(rel_29_palindrome_op_ctxt));
}
}
();}
SignalHandler::instance()->setMsg(R"_(palindrome(s) :- 
   palin_aux(s,1,sl),
   trie_level(s,l),
   s(l,sl).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [129:1-129:59])_");
if(!(rel_31_s->empty()) && !(rel_28_palin_aux->empty()) && !(rel_36_trie_level->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_31_s_op_ctxt,rel_31_s->createContext());
CREATE_OP_CONTEXT(rel_29_palindrome_op_ctxt,rel_29_palindrome->createContext());
CREATE_OP_CONTEXT(rel_36_trie_level_op_ctxt,rel_36_trie_level->createContext());
CREATE_OP_CONTEXT(rel_28_palin_aux_op_ctxt,rel_28_palin_aux->createContext());
Tuple<RamDomain,3> lower{{0,ramBitCast(RamSigned(1)),0}};
Tuple<RamDomain,3> upper{{0,ramBitCast(RamSigned(1)),0}};
lower[0] = MIN_RAM_SIGNED;
upper[0] = MAX_RAM_SIGNED;
lower[2] = MIN_RAM_SIGNED;
upper[2] = MAX_RAM_SIGNED;
auto range = rel_28_palin_aux->lowerUpperRange_010(lower, upper,READ_OP_CONTEXT(rel_28_palin_aux_op_ctxt));
for(const auto& env0 : range) {
Tuple<RamDomain,2> lower{{ramBitCast(env0[0]),0}};
Tuple<RamDomain,2> upper{{ramBitCast(env0[0]),0}};
lower[1] = MIN_RAM_SIGNED;
upper[1] = MAX_RAM_SIGNED;
auto range = rel_36_trie_level->lowerUpperRange_10(lower,upper,READ_OP_CONTEXT(rel_36_trie_level_op_ctxt));
for(const auto& env1 : range) {
if( rel_31_s->contains(Tuple<RamDomain,2>{{ramBitCast(env1[1]),ramBitCast(env0[2])}},READ_OP_CONTEXT(rel_31_s_op_ctxt))) {
Tuple<RamDomain,1> tuple{{ramBitCast(env0[0])}};
rel_29_palindrome->insert(tuple,READ_OP_CONTEXT(rel_29_palindrome_op_ctxt));
break;
}
}
}
}
();}
if (performIO) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","s"},{"filename","./palindrome.csv"},{"name","palindrome"},{"operation","output"},{"types","{\"palindrome\": {\"arity\": 1, \"auxArity\": 0, \"types\": [\"i:number\"]}, \"records\": {}}"}});
if (!outputDirectory.empty() && directiveMap["filename"].front() != '/') {directiveMap["filename"] = outputDirectory + "/" + directiveMap["filename"];}
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_29_palindrome);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}
if (performIO) rel_31_s->purge();
if (performIO) rel_36_trie_level->purge();
if (performIO) rel_28_palin_aux->purge();
SignalHandler::instance()->setMsg(R"_(debug_str(96).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [133:1-133:15])_");
[&](){
CREATE_OP_CONTEXT(rel_19_debug_str_op_ctxt,rel_19_debug_str->createContext());
Tuple<RamDomain,1> tuple{{ramBitCast(RamSigned(96))}};
rel_19_debug_str->insert(tuple,READ_OP_CONTEXT(rel_19_debug_str_op_ctxt));
}
();SignalHandler::instance()->setMsg(R"_(read(x,y) :- 
   debug_str(s),
   str_letter_at(s,x,y).
in file /home/sarc9328/souffle/tests/example/sequences/sequences.dl [137:1-137:49])_");
if(!(rel_19_debug_str->empty()) && !(rel_33_str_letter_at->empty())) {
[&](){
CREATE_OP_CONTEXT(rel_33_str_letter_at_op_ctxt,rel_33_str_letter_at->createContext());
CREATE_OP_CONTEXT(rel_19_debug_str_op_ctxt,rel_19_debug_str->createContext());
CREATE_OP_CONTEXT(rel_30_read_op_ctxt,rel_30_read->createContext());
for(const auto& env0 : *rel_19_debug_str) {
Tuple<RamDomain,3> lower{{ramBitCast(env0[0]),0,0}};
Tuple<RamDomain,3> upper{{ramBitCast(env0[0]),0,0}};
lower[1] = MIN_RAM_SIGNED;
upper[1] = MAX_RAM_SIGNED;
lower[2] = MIN_RAM_SIGNED;
upper[2] = MAX_RAM_SIGNED;
auto range = rel_33_str_letter_at->lowerUpperRange_100(lower, upper,READ_OP_CONTEXT(rel_33_str_letter_at_op_ctxt));
for(const auto& env1 : range) {
Tuple<RamDomain,2> tuple{{ramBitCast(env1[1]),ramBitCast(env1[2])}};
rel_30_read->insert(tuple,READ_OP_CONTEXT(rel_30_read_op_ctxt));
}
}
}
();}
if (performIO) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x\ty"},{"filename","./read.csv"},{"name","read"},{"operation","output"},{"types","{\"read\": {\"arity\": 2, \"auxArity\": 0, \"types\": [\"i:number\", \"s:Letter\"]}, \"records\": {}}"}});
if (!outputDirectory.empty() && directiveMap["filename"].front() != '/') {directiveMap["filename"] = outputDirectory + "/" + directiveMap["filename"];}
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_30_read);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}
if (performIO) rel_33_str_letter_at->purge();
if (performIO) rel_19_debug_str->purge();

// -- relation hint statistics --
SignalHandler::instance()->reset();
}
public:
void run() override { runFunction(".", ".", false); }
public:
void runAll(std::string inputDirectory = ".", std::string outputDirectory = ".") override { runFunction(inputDirectory, outputDirectory, true);
}
public:
void printAll(std::string outputDirectory = ".") override {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","s"},{"filename","./palindrome.csv"},{"name","palindrome"},{"operation","output"},{"types","{\"palindrome\": {\"arity\": 1, \"auxArity\": 0, \"types\": [\"i:number\"]}, \"records\": {}}"}});
if (!outputDirectory.empty() && directiveMap["IO"] == "file" && directiveMap["filename"].front() != '/') {directiveMap["filename"] = outputDirectory + "/" + directiveMap["filename"];}
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_29_palindrome);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"attributeNames","x\ty"},{"filename","./read.csv"},{"name","read"},{"operation","output"},{"types","{\"read\": {\"arity\": 2, \"auxArity\": 0, \"types\": [\"i:number\", \"s:Letter\"]}, \"records\": {}}"}});
if (!outputDirectory.empty() && directiveMap["IO"] == "file" && directiveMap["filename"].front() != '/') {directiveMap["filename"] = outputDirectory + "/" + directiveMap["filename"];}
IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_30_read);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}
public:
void loadAll(std::string inputDirectory = ".") override {
}
public:
void dumpInputs(std::ostream& out = std::cout) override {
}
public:
void dumpOutputs(std::ostream& out = std::cout) override {
try {std::map<std::string, std::string> rwOperation;
rwOperation["IO"] = "stdout";
rwOperation["name"] = "palindrome";
rwOperation["types"] = "{\"palindrome\": {\"arity\": 1, \"auxArity\": 0, \"types\": [\"i:number\"]}}";
IOSystem::getInstance().getWriter(rwOperation, symTable, recordTable)->writeAll(*rel_29_palindrome);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> rwOperation;
rwOperation["IO"] = "stdout";
rwOperation["name"] = "read";
rwOperation["types"] = "{\"read\": {\"arity\": 2, \"auxArity\": 0, \"types\": [\"i:number\", \"s:Letter\"]}}";
IOSystem::getInstance().getWriter(rwOperation, symTable, recordTable)->writeAll(*rel_30_read);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}
public:
SymbolTable& getSymbolTable() override {
return symTable;
}
};
SouffleProgram *newInstance_merged(){return new Sf_merged;}
SymbolTable *getST_merged(SouffleProgram *p){return &reinterpret_cast<Sf_merged*>(p)->symTable;}

#ifdef __EMBEDDED_SOUFFLE__
class factory_Sf_merged: public souffle::ProgramFactory {
SouffleProgram *newInstance() {
return new Sf_merged();
};
public:
factory_Sf_merged() : ProgramFactory("merged"){}
};
static factory_Sf_merged __factory_Sf_merged_instance;
}
#else
}
int main(int argc, char** argv)
{
try{
souffle::CmdOptions opt(R"(sequences.dl)",
R"(.)",
R"(.)",
false,
R"()",
1);
if (!opt.parse(argc,argv)) return 1;
souffle::Sf_merged obj;
#if defined(_OPENMP) 
obj.setNumThreads(opt.getNumJobs());

#endif
obj.runAll(opt.getInputFileDir(), opt.getOutputFileDir());
return 0;
} catch(std::exception &e) { souffle::SignalHandler::instance()->error(e.what());}
}

#endif


#include "souffle/CompiledSouffle.h"

extern "C" {}

namespace souffle {
static const RamDomain RAM_BIT_SHIFT_MASK = RAM_DOMAIN_SIZE - 1;
struct t_btree_2__0_1__11 {
    using t_tuple = Tuple<RamDomain, 2>;
    using t_ind_0 = btree_set<t_tuple, index_utils::comparator<0, 1>>;
    t_ind_0 ind_0;
    using iterator = t_ind_0::iterator;
    struct context {
        t_ind_0::operation_hints hints_0;
    };
    context createContext() {
        return context();
    }
    bool insert(const t_tuple& t) {
        context h;
        return insert(t, h);
    }
    bool insert(const t_tuple& t, context& h) {
        if (ind_0.insert(t, h.hints_0)) {
            return true;
        } else
            return false;
    }
    bool insert(const RamDomain* ramDomain) {
        RamDomain data[2];
        std::copy(ramDomain, ramDomain + 2, data);
        const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
        context h;
        return insert(tuple, h);
    }
    bool insert(RamDomain a0, RamDomain a1) {
        RamDomain data[2] = {a0, a1};
        return insert(data);
    }
    bool contains(const t_tuple& t, context& h) const {
        return ind_0.contains(t, h.hints_0);
    }
    bool contains(const t_tuple& t) const {
        context h;
        return contains(t, h);
    }
    std::size_t size() const {
        return ind_0.size();
    }
    iterator find(const t_tuple& t, context& h) const {
        return ind_0.find(t, h.hints_0);
    }
    iterator find(const t_tuple& t) const {
        context h;
        return find(t, h);
    }
    range<iterator> lowerUpperRange_00(const t_tuple& lower, const t_tuple& upper, context& h) const {
        return range<iterator>(ind_0.begin(), ind_0.end());
    }
    range<iterator> lowerUpperRange_00(const t_tuple& lower, const t_tuple& upper) const {
        return range<iterator>(ind_0.begin(), ind_0.end());
    }
    range<t_ind_0::iterator> lowerUpperRange_11(
            const t_tuple& lower, const t_tuple& upper, context& h) const {
        auto pos = ind_0.find(lower, h.hints_0);
        auto fin = ind_0.end();
        if (pos != fin) {
            fin = pos;
            ++fin;
        }
        return make_range(pos, fin);
    }
    range<t_ind_0::iterator> lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper) const {
        context h;
        return lowerUpperRange_11(lower, upper, h);
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
struct t_btree_2__0_1__1_0__11__20 {
    using t_tuple = Tuple<RamDomain, 2>;
    using t_ind_0 = btree_set<t_tuple, index_utils::comparator<0, 1>>;
    t_ind_0 ind_0;
    using t_ind_1 = btree_set<t_tuple, index_utils::comparator<1, 0>>;
    t_ind_1 ind_1;
    using iterator = t_ind_0::iterator;
    struct context {
        t_ind_0::operation_hints hints_0;
        t_ind_1::operation_hints hints_1;
    };
    context createContext() {
        return context();
    }
    bool insert(const t_tuple& t) {
        context h;
        return insert(t, h);
    }
    bool insert(const t_tuple& t, context& h) {
        if (ind_0.insert(t, h.hints_0)) {
            ind_1.insert(t, h.hints_1);
            return true;
        } else
            return false;
    }
    bool insert(const RamDomain* ramDomain) {
        RamDomain data[2];
        std::copy(ramDomain, ramDomain + 2, data);
        const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
        context h;
        return insert(tuple, h);
    }
    bool insert(RamDomain a0, RamDomain a1) {
        RamDomain data[2] = {a0, a1};
        return insert(data);
    }
    bool contains(const t_tuple& t, context& h) const {
        return ind_0.contains(t, h.hints_0);
    }
    bool contains(const t_tuple& t) const {
        context h;
        return contains(t, h);
    }
    std::size_t size() const {
        return ind_0.size();
    }
    iterator find(const t_tuple& t, context& h) const {
        return ind_0.find(t, h.hints_0);
    }
    iterator find(const t_tuple& t) const {
        context h;
        return find(t, h);
    }
    range<iterator> lowerUpperRange_00(const t_tuple& lower, const t_tuple& upper, context& h) const {
        return range<iterator>(ind_0.begin(), ind_0.end());
    }
    range<iterator> lowerUpperRange_00(const t_tuple& lower, const t_tuple& upper) const {
        return range<iterator>(ind_0.begin(), ind_0.end());
    }
    range<t_ind_0::iterator> lowerUpperRange_11(
            const t_tuple& lower, const t_tuple& upper, context& h) const {
        auto pos = ind_0.find(lower, h.hints_0);
        auto fin = ind_0.end();
        if (pos != fin) {
            fin = pos;
            ++fin;
        }
        return make_range(pos, fin);
    }
    range<t_ind_0::iterator> lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper) const {
        context h;
        return lowerUpperRange_11(lower, upper, h);
    }
    range<t_ind_1::iterator> lowerUpperRange_20(
            const t_tuple& lower, const t_tuple& upper, context& h) const {
        t_tuple low(lower);
        t_tuple high(upper);
        low[0] = MIN_RAM_SIGNED;
        high[0] = MAX_RAM_SIGNED;
        return make_range(ind_1.lower_bound(low, h.hints_1), ind_1.upper_bound(high, h.hints_1));
    }
    range<t_ind_1::iterator> lowerUpperRange_20(const t_tuple& lower, const t_tuple& upper) const {
        context h;
        return lowerUpperRange_20(lower, upper, h);
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
        o << " arity 2 direct b-tree index 0 lex-order [0,1]\n";
        ind_0.printStats(o);
        o << " arity 2 direct b-tree index 1 lex-order [1,0]\n";
        ind_1.printStats(o);
    }
};
struct t_btree_2__1_0__10__11 {
    using t_tuple = Tuple<RamDomain, 2>;
    using t_ind_0 = btree_set<t_tuple, index_utils::comparator<1, 0>>;
    t_ind_0 ind_0;
    using iterator = t_ind_0::iterator;
    struct context {
        t_ind_0::operation_hints hints_0;
    };
    context createContext() {
        return context();
    }
    bool insert(const t_tuple& t) {
        context h;
        return insert(t, h);
    }
    bool insert(const t_tuple& t, context& h) {
        if (ind_0.insert(t, h.hints_0)) {
            return true;
        } else
            return false;
    }
    bool insert(const RamDomain* ramDomain) {
        RamDomain data[2];
        std::copy(ramDomain, ramDomain + 2, data);
        const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
        context h;
        return insert(tuple, h);
    }
    bool insert(RamDomain a0, RamDomain a1) {
        RamDomain data[2] = {a0, a1};
        return insert(data);
    }
    bool contains(const t_tuple& t, context& h) const {
        return ind_0.contains(t, h.hints_0);
    }
    bool contains(const t_tuple& t) const {
        context h;
        return contains(t, h);
    }
    std::size_t size() const {
        return ind_0.size();
    }
    iterator find(const t_tuple& t, context& h) const {
        return ind_0.find(t, h.hints_0);
    }
    iterator find(const t_tuple& t) const {
        context h;
        return find(t, h);
    }
    range<iterator> lowerUpperRange_00(const t_tuple& lower, const t_tuple& upper, context& h) const {
        return range<iterator>(ind_0.begin(), ind_0.end());
    }
    range<iterator> lowerUpperRange_00(const t_tuple& lower, const t_tuple& upper) const {
        return range<iterator>(ind_0.begin(), ind_0.end());
    }
    range<t_ind_0::iterator> lowerUpperRange_10(
            const t_tuple& lower, const t_tuple& upper, context& h) const {
        t_tuple low(lower);
        t_tuple high(upper);
        low[0] = MIN_RAM_SIGNED;
        high[0] = MAX_RAM_SIGNED;
        return make_range(ind_0.lower_bound(low, h.hints_0), ind_0.upper_bound(high, h.hints_0));
    }
    range<t_ind_0::iterator> lowerUpperRange_10(const t_tuple& lower, const t_tuple& upper) const {
        context h;
        return lowerUpperRange_10(lower, upper, h);
    }
    range<t_ind_0::iterator> lowerUpperRange_11(
            const t_tuple& lower, const t_tuple& upper, context& h) const {
        auto pos = ind_0.find(lower, h.hints_0);
        auto fin = ind_0.end();
        if (pos != fin) {
            fin = pos;
            ++fin;
        }
        return make_range(pos, fin);
    }
    range<t_ind_0::iterator> lowerUpperRange_11(const t_tuple& lower, const t_tuple& upper) const {
        context h;
        return lowerUpperRange_11(lower, upper, h);
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
        o << " arity 2 direct b-tree index 0 lex-order [1,0]\n";
        ind_0.printStats(o);
    }
};
struct t_btree_5__0_1_2_3_4__11111 {
    using t_tuple = Tuple<RamDomain, 5>;
    using t_ind_0 = btree_set<t_tuple, index_utils::comparator<0, 1, 2, 3, 4>>;
    t_ind_0 ind_0;
    using iterator = t_ind_0::iterator;
    struct context {
        t_ind_0::operation_hints hints_0;
    };
    context createContext() {
        return context();
    }
    bool insert(const t_tuple& t) {
        context h;
        return insert(t, h);
    }
    bool insert(const t_tuple& t, context& h) {
        if (ind_0.insert(t, h.hints_0)) {
            return true;
        } else
            return false;
    }
    bool insert(const RamDomain* ramDomain) {
        RamDomain data[5];
        std::copy(ramDomain, ramDomain + 5, data);
        const t_tuple& tuple = reinterpret_cast<const t_tuple&>(data);
        context h;
        return insert(tuple, h);
    }
    bool insert(RamDomain a0, RamDomain a1, RamDomain a2, RamDomain a3, RamDomain a4) {
        RamDomain data[5] = {a0, a1, a2, a3, a4};
        return insert(data);
    }
    bool contains(const t_tuple& t, context& h) const {
        return ind_0.contains(t, h.hints_0);
    }
    bool contains(const t_tuple& t) const {
        context h;
        return contains(t, h);
    }
    std::size_t size() const {
        return ind_0.size();
    }
    iterator find(const t_tuple& t, context& h) const {
        return ind_0.find(t, h.hints_0);
    }
    iterator find(const t_tuple& t) const {
        context h;
        return find(t, h);
    }
    range<iterator> lowerUpperRange_00000(const t_tuple& lower, const t_tuple& upper, context& h) const {
        return range<iterator>(ind_0.begin(), ind_0.end());
    }
    range<iterator> lowerUpperRange_00000(const t_tuple& lower, const t_tuple& upper) const {
        return range<iterator>(ind_0.begin(), ind_0.end());
    }
    range<t_ind_0::iterator> lowerUpperRange_11111(
            const t_tuple& lower, const t_tuple& upper, context& h) const {
        auto pos = ind_0.find(lower, h.hints_0);
        auto fin = ind_0.end();
        if (pos != fin) {
            fin = pos;
            ++fin;
        }
        return make_range(pos, fin);
    }
    range<t_ind_0::iterator> lowerUpperRange_11111(const t_tuple& lower, const t_tuple& upper) const {
        context h;
        return lowerUpperRange_11111(lower, upper, h);
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
        o << " arity 5 direct b-tree index 0 lex-order [0,1,2,3,4]\n";
        ind_0.printStats(o);
    }
};

class Sf_test : public SouffleProgram {
private:
    static inline bool regex_wrapper(const std::string& pattern, const std::string& text) {
        bool result = false;
        try {
            result = std::regex_match(text, std::regex(pattern));
        } catch (...) {
            std::cerr << "warning: wrong pattern provided for match(\"" << pattern << "\",\"" << text
                      << "\").\n";
        }
        return result;
    }

private:
    static inline std::string substr_wrapper(const std::string& str, size_t idx, size_t len) {
        std::string result;
        try {
            result = str.substr(idx, len);
        } catch (...) {
            std::cerr << "warning: wrong index position provided by substr(\"";
            std::cerr << str << "\"," << (int32_t)idx << "," << (int32_t)len << ") functor.\n";
        }
        return result;
    }

private:
    static inline RamDomain wrapper_tonumber(const std::string& str) {
        RamDomain result = 0;
        try {
            result = RamSignedFromString(str);
        } catch (...) {
            std::cerr << "error: wrong string provided by to_number(\"";
            std::cerr << str << "\") functor.\n";
            raise(SIGFPE);
        }
        return result;
    }

public:
    // -- initialize symbol table --
    SymbolTable symTable;  // -- initialize record table --
    RecordTable recordTable;
    // -- Table: @delta_L
    std::unique_ptr<t_btree_2__0_1__11> rel_1_delta_L = std::make_unique<t_btree_2__0_1__11>();
    // -- Table: @delta_T
    std::unique_ptr<t_btree_2__0_1__1_0__11__20> rel_2_delta_T =
            std::make_unique<t_btree_2__0_1__1_0__11__20>();
    // -- Table: @new_L
    std::unique_ptr<t_btree_2__0_1__11> rel_3_new_L = std::make_unique<t_btree_2__0_1__11>();
    // -- Table: @new_T
    std::unique_ptr<t_btree_2__0_1__1_0__11__20> rel_4_new_T =
            std::make_unique<t_btree_2__0_1__1_0__11__20>();
    // -- Table: L
    std::unique_ptr<t_btree_2__1_0__10__11> rel_5_L = std::make_unique<t_btree_2__1_0__10__11>();
    souffle::RelationWrapper<0, t_btree_2__1_0__10__11, Tuple<RamDomain, 2>, 2, 0> wrapper_rel_5_L;
    // -- Table: O
    std::unique_ptr<t_btree_5__0_1_2_3_4__11111> rel_6_O = std::make_unique<t_btree_5__0_1_2_3_4__11111>();
    souffle::RelationWrapper<1, t_btree_5__0_1_2_3_4__11111, Tuple<RamDomain, 5>, 5, 0> wrapper_rel_6_O;
    // -- Table: T
    std::unique_ptr<t_btree_2__0_1__1_0__11__20> rel_7_T = std::make_unique<t_btree_2__0_1__1_0__11__20>();
    souffle::RelationWrapper<2, t_btree_2__0_1__1_0__11__20, Tuple<RamDomain, 2>, 2, 0> wrapper_rel_7_T;

public:
    Sf_test()
            : wrapper_rel_5_L(*rel_5_L, symTable, "L",
                      std::array<const char*, 2>{
                              {"r:List{head#r:Tree{left#r:Tree,val#i:number,right#r:Tree},tail#r:List}",
                                      "i:number"}},
                      std::array<const char*, 2>{{"l", "m"}}),

              wrapper_rel_6_O(*rel_6_O, symTable, "O",
                      std::array<const char*,
                              5>{{"r:List{head#r:Tree{left#r:Tree,val#i:number,right#r:Tree},tail#r:List}",
                              "r:List{head#r:Tree{left#r:Tree,val#i:number,right#r:Tree},tail#r:List}",
                              "r:List{head#r:Tree{left#r:Tree,val#i:number,right#r:Tree},tail#r:List}",
                              "r:List{head#r:Tree{left#r:Tree,val#i:number,right#r:Tree},tail#r:List}",
                              "r:List{head#r:Tree{left#r:Tree,val#i:number,right#r:Tree},tail#r:List}"}},
                      std::array<const char*, 5>{{"l5", "l6", "l7", "l8", "l9"}}),

              wrapper_rel_7_T(*rel_7_T, symTable, "T",
                      std::array<const char*, 2>{
                              {"r:Tree{left#r:Tree,val#i:number,right#r:Tree}", "i:number"}},
                      std::array<const char*, 2>{{"t", "height"}}) {
        addRelation("L", &wrapper_rel_5_L, false, false);
        addRelation("O", &wrapper_rel_6_O, false, true);
        addRelation("T", &wrapper_rel_7_T, false, false);
    }
    ~Sf_test() {}

private:
    void runFunction(
            std::string inputDirectory = ".", std::string outputDirectory = ".", bool performIO = false) {
        SignalHandler::instance()->set();
        std::atomic<size_t> iter(0);

#if defined(_OPENMP)
        if (getNumThreads() > 0) {
            omp_set_num_threads(getNumThreads());
        }
#endif

        // -- query evaluation --
        SignalHandler::instance()->setMsg(R"_(T(nil,0).
in file /home/sarc9328/souffle/tests/semantic/store_record_large/store_record_large.dl [17:1-17:11])_");
        [&]() {
            CREATE_OP_CONTEXT(rel_7_T_op_ctxt, rel_7_T->createContext());
            Tuple<RamDomain, 2> tuple{{ramBitCast(RamSigned(0)), ramBitCast(RamSigned(0))}};
            rel_7_T->insert(tuple, READ_OP_CONTEXT(rel_7_T_op_ctxt));
        }();
        [&]() {
            CREATE_OP_CONTEXT(rel_7_T_op_ctxt, rel_7_T->createContext());
            CREATE_OP_CONTEXT(rel_2_delta_T_op_ctxt, rel_2_delta_T->createContext());
            for (const auto& env0 : *rel_7_T) {
                Tuple<RamDomain, 2> tuple{{ramBitCast(env0[0]), ramBitCast(env0[1])}};
                rel_2_delta_T->insert(tuple, READ_OP_CONTEXT(rel_2_delta_T_op_ctxt));
            }
        }();
        iter = 0;
        for (;;) {
            SignalHandler::instance()->setMsg(R"_(T([left,(m+1),right],(m+1)) :- 
   T(left,m),
   T(right,m),
   m < 10.
in file /home/sarc9328/souffle/tests/semantic/store_record_large/store_record_large.dl [18:1-19:39])_");
            if (!(rel_2_delta_T->empty()) && !(rel_7_T->empty())) {
                [&]() {
                    CREATE_OP_CONTEXT(rel_7_T_op_ctxt, rel_7_T->createContext());
                    CREATE_OP_CONTEXT(rel_2_delta_T_op_ctxt, rel_2_delta_T->createContext());
                    CREATE_OP_CONTEXT(rel_4_new_T_op_ctxt, rel_4_new_T->createContext());
                    Tuple<RamDomain, 2> lower{{0, 0}};
                    Tuple<RamDomain, 2> upper{{0, ramBitCast((ramBitCast<RamSigned>(RamSigned(10)) -
                                                              ramBitCast<RamSigned>(RamSigned(1))))}};
                    lower[0] = MIN_RAM_SIGNED;
                    upper[0] = MAX_RAM_SIGNED;
                    lower[1] = MIN_RAM_SIGNED;
                    auto range = rel_2_delta_T->lowerUpperRange_20(
                            lower, upper, READ_OP_CONTEXT(rel_2_delta_T_op_ctxt));
                    for (const auto& env0 : range) {
                        Tuple<RamDomain, 2> lower{{0, ramBitCast(env0[1])}};
                        Tuple<RamDomain, 2> upper{{0, ramBitCast(env0[1])}};
                        lower[0] = MIN_RAM_SIGNED;
                        upper[0] = MAX_RAM_SIGNED;
                        auto range =
                                rel_7_T->lowerUpperRange_10(lower, upper, READ_OP_CONTEXT(rel_7_T_op_ctxt));
                        for (const auto& env1 : range) {
                            if (!(rel_2_delta_T->contains(
                                        Tuple<RamDomain, 2>{{ramBitCast(env1[0]), ramBitCast(env0[1])}},
                                        READ_OP_CONTEXT(rel_2_delta_T_op_ctxt))) &&
                                    !(rel_7_T->contains(
                                            Tuple<RamDomain, 2>{
                                                    {ramBitCast(pack(recordTable,
                                                             Tuple<RamDomain, 3>{{ramBitCast(ramBitCast(
                                                                                          env0[0])),
                                                                     ramBitCast(ramBitCast(
                                                                             (ramBitCast<RamSigned>(env0[1]) +
                                                                                     ramBitCast<RamSigned>(
                                                                                             RamSigned(1))))),
                                                                     ramBitCast(ramBitCast(env1[0]))}})),
                                                            ramBitCast((
                                                                    ramBitCast<RamSigned>(env0[1]) +
                                                                    ramBitCast<RamSigned>(RamSigned(1))))}},
                                            READ_OP_CONTEXT(rel_7_T_op_ctxt)))) {
                                Tuple<RamDomain, 2> tuple{
                                        {ramBitCast(pack(recordTable,
                                                 Tuple<RamDomain, 3>{{ramBitCast(ramBitCast(env0[0])),
                                                         ramBitCast(ramBitCast((
                                                                 ramBitCast<RamSigned>(env0[1]) +
                                                                 ramBitCast<RamSigned>(RamSigned(1))))),
                                                         ramBitCast(ramBitCast(env1[0]))}})),
                                                ramBitCast((ramBitCast<RamSigned>(env0[1]) +
                                                            ramBitCast<RamSigned>(RamSigned(1))))}};
                                rel_4_new_T->insert(tuple, READ_OP_CONTEXT(rel_4_new_T_op_ctxt));
                            }
                        }
                    }
                }();
            }
            SignalHandler::instance()->setMsg(R"_(T([left,(m+1),right],(m+1)) :- 
   T(left,m),
   T(right,m),
   m < 10.
in file /home/sarc9328/souffle/tests/semantic/store_record_large/store_record_large.dl [18:1-19:39])_");
            if (!(rel_7_T->empty()) && !(rel_2_delta_T->empty())) {
                [&]() {
                    CREATE_OP_CONTEXT(rel_7_T_op_ctxt, rel_7_T->createContext());
                    CREATE_OP_CONTEXT(rel_2_delta_T_op_ctxt, rel_2_delta_T->createContext());
                    CREATE_OP_CONTEXT(rel_4_new_T_op_ctxt, rel_4_new_T->createContext());
                    Tuple<RamDomain, 2> lower{{0, 0}};
                    Tuple<RamDomain, 2> upper{{0, ramBitCast((ramBitCast<RamSigned>(RamSigned(10)) -
                                                              ramBitCast<RamSigned>(RamSigned(1))))}};
                    lower[0] = MIN_RAM_SIGNED;
                    upper[0] = MAX_RAM_SIGNED;
                    lower[1] = MIN_RAM_SIGNED;
                    auto range = rel_7_T->lowerUpperRange_20(lower, upper, READ_OP_CONTEXT(rel_7_T_op_ctxt));
                    for (const auto& env0 : range) {
                        Tuple<RamDomain, 2> lower{{0, ramBitCast(env0[1])}};
                        Tuple<RamDomain, 2> upper{{0, ramBitCast(env0[1])}};
                        lower[0] = MIN_RAM_SIGNED;
                        upper[0] = MAX_RAM_SIGNED;
                        auto range = rel_2_delta_T->lowerUpperRange_10(
                                lower, upper, READ_OP_CONTEXT(rel_2_delta_T_op_ctxt));
                        for (const auto& env1 : range) {
                            if (!(rel_7_T->contains(
                                        Tuple<RamDomain, 2>{
                                                {ramBitCast(pack(recordTable,
                                                         Tuple<RamDomain, 3>{{ramBitCast(ramBitCast(env0[0])),
                                                                 ramBitCast(ramBitCast(
                                                                         (ramBitCast<RamSigned>(env0[1]) +
                                                                                 ramBitCast<RamSigned>(
                                                                                         RamSigned(1))))),
                                                                 ramBitCast(ramBitCast(env1[0]))}})),
                                                        ramBitCast((ramBitCast<RamSigned>(env0[1]) +
                                                                    ramBitCast<RamSigned>(RamSigned(1))))}},
                                        READ_OP_CONTEXT(rel_7_T_op_ctxt)))) {
                                Tuple<RamDomain, 2> tuple{
                                        {ramBitCast(pack(recordTable,
                                                 Tuple<RamDomain, 3>{{ramBitCast(ramBitCast(env0[0])),
                                                         ramBitCast(ramBitCast((
                                                                 ramBitCast<RamSigned>(env0[1]) +
                                                                 ramBitCast<RamSigned>(RamSigned(1))))),
                                                         ramBitCast(ramBitCast(env1[0]))}})),
                                                ramBitCast((ramBitCast<RamSigned>(env0[1]) +
                                                            ramBitCast<RamSigned>(RamSigned(1))))}};
                                rel_4_new_T->insert(tuple, READ_OP_CONTEXT(rel_4_new_T_op_ctxt));
                            }
                        }
                    }
                }();
            }
            if (rel_4_new_T->empty()) break;
            [&]() {
                CREATE_OP_CONTEXT(rel_7_T_op_ctxt, rel_7_T->createContext());
                CREATE_OP_CONTEXT(rel_4_new_T_op_ctxt, rel_4_new_T->createContext());
                for (const auto& env0 : *rel_4_new_T) {
                    Tuple<RamDomain, 2> tuple{{ramBitCast(env0[0]), ramBitCast(env0[1])}};
                    rel_7_T->insert(tuple, READ_OP_CONTEXT(rel_7_T_op_ctxt));
                }
            }();
            std::swap(rel_2_delta_T, rel_4_new_T);
            rel_4_new_T->purge();
            iter++;
        }
        iter = 0;
        rel_2_delta_T->purge();
        rel_4_new_T->purge();
        SignalHandler::instance()->setMsg(R"_(L([nil,nil],0).
in file /home/sarc9328/souffle/tests/semantic/store_record_large/store_record_large.dl [22:1-22:18])_");
        [&]() {
            CREATE_OP_CONTEXT(rel_5_L_op_ctxt, rel_5_L->createContext());
            Tuple<RamDomain, 2> tuple{
                    {ramBitCast(pack(recordTable, Tuple<RamDomain, 2>{{ramBitCast(ramBitCast(RamSigned(0))),
                                                          ramBitCast(ramBitCast(RamSigned(0)))}})),
                            ramBitCast(RamSigned(0))}};
            rel_5_L->insert(tuple, READ_OP_CONTEXT(rel_5_L_op_ctxt));
        }();
        [&]() {
            CREATE_OP_CONTEXT(rel_1_delta_L_op_ctxt, rel_1_delta_L->createContext());
            CREATE_OP_CONTEXT(rel_5_L_op_ctxt, rel_5_L->createContext());
            for (const auto& env0 : *rel_5_L) {
                Tuple<RamDomain, 2> tuple{{ramBitCast(env0[0]), ramBitCast(env0[1])}};
                rel_1_delta_L->insert(tuple, READ_OP_CONTEXT(rel_1_delta_L_op_ctxt));
            }
        }();
        iter = 0;
        for (;;) {
            SignalHandler::instance()->setMsg(R"_(L([h,t],l) :- 
   L(t, _tmp_0),
   T([h,l,+underscore_0],_),
    _tmp_0 = (l-1).
in file /home/sarc9328/souffle/tests/semantic/store_record_large/store_record_large.dl [23:1-23:46])_");
            if (!(rel_1_delta_L->empty()) && !(rel_7_T->empty())) {
                [&]() {
                    CREATE_OP_CONTEXT(rel_7_T_op_ctxt, rel_7_T->createContext());
                    CREATE_OP_CONTEXT(rel_3_new_L_op_ctxt, rel_3_new_L->createContext());
                    CREATE_OP_CONTEXT(rel_1_delta_L_op_ctxt, rel_1_delta_L->createContext());
                    CREATE_OP_CONTEXT(rel_5_L_op_ctxt, rel_5_L->createContext());
                    for (const auto& env0 : *rel_1_delta_L) {
                        for (const auto& env1 : *rel_7_T) {
                            RamDomain const ref = env1[0];
                            if (ref == 0) continue;
                            const RamDomain* env2 = recordTable.unpack(ref, 3);
                            {
                                if ((ramBitCast<RamDomain>(env0[1]) ==
                                            ramBitCast<RamDomain>((ramBitCast<RamSigned>(env2[1]) -
                                                                   ramBitCast<RamSigned>(RamSigned(1))))) &&
                                        !(rel_5_L->contains(
                                                Tuple<RamDomain, 2>{
                                                        {ramBitCast(pack(recordTable,
                                                                 Tuple<RamDomain, 2>{{ramBitCast(ramBitCast(
                                                                                              env2[0])),
                                                                         ramBitCast(ramBitCast(env0[0]))}})),
                                                                ramBitCast(env2[1])}},
                                                READ_OP_CONTEXT(rel_5_L_op_ctxt)))) {
                                    Tuple<RamDomain, 2> tuple{
                                            {ramBitCast(pack(recordTable,
                                                     Tuple<RamDomain, 2>{{ramBitCast(ramBitCast(env2[0])),
                                                             ramBitCast(ramBitCast(env0[0]))}})),
                                                    ramBitCast(env2[1])}};
                                    rel_3_new_L->insert(tuple, READ_OP_CONTEXT(rel_3_new_L_op_ctxt));
                                }
                            }
                        }
                    }
                }();
            }
            if (rel_3_new_L->empty()) break;
            [&]() {
                CREATE_OP_CONTEXT(rel_3_new_L_op_ctxt, rel_3_new_L->createContext());
                CREATE_OP_CONTEXT(rel_5_L_op_ctxt, rel_5_L->createContext());
                for (const auto& env0 : *rel_3_new_L) {
                    Tuple<RamDomain, 2> tuple{{ramBitCast(env0[0]), ramBitCast(env0[1])}};
                    rel_5_L->insert(tuple, READ_OP_CONTEXT(rel_5_L_op_ctxt));
                }
            }();
            std::swap(rel_1_delta_L, rel_3_new_L);
            rel_3_new_L->purge();
            iter++;
        }
        iter = 0;
        rel_1_delta_L->purge();
        rel_3_new_L->purge();
        if (performIO) rel_7_T->purge();
        SignalHandler::instance()->setMsg(R"_(O(l5,l6,l7,l8,l9) :- 
   L(l5,5),
   L(l6,6),
   L(l7,7),
   L(l8,8),
   L(l9,9).
in file /home/sarc9328/souffle/tests/semantic/store_record_large/store_record_large.dl [27:1-27:75])_");
        if (!(rel_5_L->empty())) {
            [&]() {
                CREATE_OP_CONTEXT(rel_6_O_op_ctxt, rel_6_O->createContext());
                CREATE_OP_CONTEXT(rel_5_L_op_ctxt, rel_5_L->createContext());
                Tuple<RamDomain, 2> lower{{0, ramBitCast(RamSigned(5))}};
                Tuple<RamDomain, 2> upper{{0, ramBitCast(RamSigned(5))}};
                lower[0] = MIN_RAM_SIGNED;
                upper[0] = MAX_RAM_SIGNED;
                auto range = rel_5_L->lowerUpperRange_10(lower, upper, READ_OP_CONTEXT(rel_5_L_op_ctxt));
                for (const auto& env0 : range) {
                    Tuple<RamDomain, 2> lower{{0, ramBitCast(RamSigned(6))}};
                    Tuple<RamDomain, 2> upper{{0, ramBitCast(RamSigned(6))}};
                    lower[0] = MIN_RAM_SIGNED;
                    upper[0] = MAX_RAM_SIGNED;
                    auto range = rel_5_L->lowerUpperRange_10(lower, upper, READ_OP_CONTEXT(rel_5_L_op_ctxt));
                    for (const auto& env1 : range) {
                        Tuple<RamDomain, 2> lower{{0, ramBitCast(RamSigned(7))}};
                        Tuple<RamDomain, 2> upper{{0, ramBitCast(RamSigned(7))}};
                        lower[0] = MIN_RAM_SIGNED;
                        upper[0] = MAX_RAM_SIGNED;
                        auto range =
                                rel_5_L->lowerUpperRange_10(lower, upper, READ_OP_CONTEXT(rel_5_L_op_ctxt));
                        for (const auto& env2 : range) {
                            Tuple<RamDomain, 2> lower{{0, ramBitCast(RamSigned(8))}};
                            Tuple<RamDomain, 2> upper{{0, ramBitCast(RamSigned(8))}};
                            lower[0] = MIN_RAM_SIGNED;
                            upper[0] = MAX_RAM_SIGNED;
                            auto range = rel_5_L->lowerUpperRange_10(
                                    lower, upper, READ_OP_CONTEXT(rel_5_L_op_ctxt));
                            for (const auto& env3 : range) {
                                Tuple<RamDomain, 2> lower{{0, ramBitCast(RamSigned(9))}};
                                Tuple<RamDomain, 2> upper{{0, ramBitCast(RamSigned(9))}};
                                lower[0] = MIN_RAM_SIGNED;
                                upper[0] = MAX_RAM_SIGNED;
                                auto range = rel_5_L->lowerUpperRange_10(
                                        lower, upper, READ_OP_CONTEXT(rel_5_L_op_ctxt));
                                for (const auto& env4 : range) {
                                    Tuple<RamDomain, 5> tuple{{ramBitCast(env0[0]), ramBitCast(env1[0]),
                                            ramBitCast(env2[0]), ramBitCast(env3[0]), ramBitCast(env4[0])}};
                                    rel_6_O->insert(tuple, READ_OP_CONTEXT(rel_6_O_op_ctxt));
                                }
                            }
                        }
                    }
                }
            }();
        }
        if (performIO) {
            try {
                std::map<std::string, std::string> directiveMap({{"IO", "file"},
                        {"attributeNames", "l5\tl6\tl7\tl8\tl9"}, {"filename", "./O.csv"}, {"name", "O"},
                        {"operation", "output"},
                        {"types",
                                "{\"O\": {\"arity\": 5, \"auxArity\": 0, \"types\": "
                                "[\"r:List{head#r:Tree{left#r:Tree,val#i:number,right#r:Tree},tail#r:List}\","
                                " \"r:List{head#r:Tree{left#r:Tree,val#i:number,right#r:Tree},tail#r:List}\","
                                " \"r:List{head#r:Tree{left#r:Tree,val#i:number,right#r:Tree},tail#r:List}\","
                                " \"r:List{head#r:Tree{left#r:Tree,val#i:number,right#r:Tree},tail#r:List}\","
                                " \"r:List{head#r:Tree{left#r:Tree,val#i:number,right#r:Tree},tail#r:List}\"]"
                                "}, \"records\": "
                                "{\"r:List{head#r:Tree{left#r:Tree,val#i:number,right#r:Tree},tail#r:List}\":"
                                " {\"arity\": 2, \"types\": "
                                "[\"r:Tree{left#r:Tree,val#i:number,right#r:Tree}\", "
                                "\"r:List{head#r:Tree{left#r:Tree,val#i:number,right#r:Tree},tail#r:List}\"]}"
                                ", \"r:Tree{left#r:Tree,val#i:number,right#r:Tree}\": {\"arity\": 3, "
                                "\"types\": [\"r:Tree{left#r:Tree,val#i:number,right#r:Tree}\", "
                                "\"i:number\", \"r:Tree{left#r:Tree,val#i:number,right#r:Tree}\"]}}}"}});
                if (!outputDirectory.empty() && directiveMap["filename"].front() != '/') {
                    directiveMap["filename"] = outputDirectory + "/" + directiveMap["filename"];
                }
                IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_6_O);
            } catch (std::exception& e) {
                std::cerr << e.what();
                exit(1);
            }
        }
        if (performIO) rel_5_L->purge();

        // -- relation hint statistics --
        SignalHandler::instance()->reset();
    }

public:
    void run() override {
        runFunction(".", ".", false);
    }

public:
    void runAll(std::string inputDirectory = ".", std::string outputDirectory = ".") override {
        runFunction(inputDirectory, outputDirectory, true);
    }

public:
    void printAll(std::string outputDirectory = ".") override {
        try {
            std::map<std::string, std::string> directiveMap({{"IO", "file"},
                    {"attributeNames", "l5\tl6\tl7\tl8\tl9"}, {"filename", "./O.csv"}, {"name", "O"},
                    {"operation", "output"},
                    {"types",
                            "{\"O\": {\"arity\": 5, \"auxArity\": 0, \"types\": "
                            "[\"r:List{head#r:Tree{left#r:Tree,val#i:number,right#r:Tree},tail#r:List}\", "
                            "\"r:List{head#r:Tree{left#r:Tree,val#i:number,right#r:Tree},tail#r:List}\", "
                            "\"r:List{head#r:Tree{left#r:Tree,val#i:number,right#r:Tree},tail#r:List}\", "
                            "\"r:List{head#r:Tree{left#r:Tree,val#i:number,right#r:Tree},tail#r:List}\", "
                            "\"r:List{head#r:Tree{left#r:Tree,val#i:number,right#r:Tree},tail#r:List}\"]}, "
                            "\"records\": "
                            "{\"r:List{head#r:Tree{left#r:Tree,val#i:number,right#r:Tree},tail#r:List}\": "
                            "{\"arity\": 2, \"types\": [\"r:Tree{left#r:Tree,val#i:number,right#r:Tree}\", "
                            "\"r:List{head#r:Tree{left#r:Tree,val#i:number,right#r:Tree},tail#r:List}\"]}, "
                            "\"r:Tree{left#r:Tree,val#i:number,right#r:Tree}\": {\"arity\": 3, \"types\": "
                            "[\"r:Tree{left#r:Tree,val#i:number,right#r:Tree}\", \"i:number\", "
                            "\"r:Tree{left#r:Tree,val#i:number,right#r:Tree}\"]}}}"}});
            if (!outputDirectory.empty() && directiveMap["IO"] == "file" &&
                    directiveMap["filename"].front() != '/') {
                directiveMap["filename"] = outputDirectory + "/" + directiveMap["filename"];
            }
            IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(*rel_6_O);
        } catch (std::exception& e) {
            std::cerr << e.what();
            exit(1);
        }
    }

public:
    void loadAll(std::string inputDirectory = ".") override {}

public:
    void dumpInputs(std::ostream& out = std::cout) override {}

public:
    void dumpOutputs(std::ostream& out = std::cout) override {
        try {
            std::map<std::string, std::string> rwOperation;
            rwOperation["IO"] = "stdout";
            rwOperation["name"] = "O";
            rwOperation["types"] =
                    "{\"O\": {\"arity\": 5, \"auxArity\": 0, \"types\": "
                    "[\"r:List{head#r:Tree{left#r:Tree,val#i:number,right#r:Tree},tail#r:List}\", "
                    "\"r:List{head#r:Tree{left#r:Tree,val#i:number,right#r:Tree},tail#r:List}\", "
                    "\"r:List{head#r:Tree{left#r:Tree,val#i:number,right#r:Tree},tail#r:List}\", "
                    "\"r:List{head#r:Tree{left#r:Tree,val#i:number,right#r:Tree},tail#r:List}\", "
                    "\"r:List{head#r:Tree{left#r:Tree,val#i:number,right#r:Tree},tail#r:List}\"]}}";
            IOSystem::getInstance().getWriter(rwOperation, symTable, recordTable)->writeAll(*rel_6_O);
        } catch (std::exception& e) {
            std::cerr << e.what();
            exit(1);
        }
    }

public:
    SymbolTable& getSymbolTable() override {
        return symTable;
    }
};
SouffleProgram* newInstance_test() {
    return new Sf_test;
}
SymbolTable* getST_test(SouffleProgram* p) {
    return &reinterpret_cast<Sf_test*>(p)->symTable;
}

#ifdef __EMBEDDED_SOUFFLE__
class factory_Sf_test : public souffle::ProgramFactory {
    SouffleProgram* newInstance() {
        return new Sf_test();
    };

public:
    factory_Sf_test() : ProgramFactory("test") {}
};
static factory_Sf_test __factory_Sf_test_instance;
}
#else
}
int main(int argc, char** argv) {
    try {
        souffle::CmdOptions opt(R"(store_record_large.dl)", R"(.)", R"(.)", false, R"()", 1);
        if (!opt.parse(argc, argv)) return 1;
        souffle::Sf_test obj;
#if defined(_OPENMP)
        obj.setNumThreads(opt.getNumJobs());

#endif
        obj.runAll(opt.getInputFileDir(), opt.getOutputFileDir());
        return 0;
    } catch (std::exception& e) {
        souffle::SignalHandler::instance()->error(e.what());
    }
}

#endif

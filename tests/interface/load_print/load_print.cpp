#include "souffle/CompiledSouffle.h"

namespace souffle {
using namespace ram;
class Sf_load_print : public SouffleProgram {
private:
static inline bool regex_wrapper(const char *pattern, const char *text) {
   bool result = false; 
   try { result = std::regex_match(text, std::regex(pattern)); } catch(...) { 
     std::cerr << "warning: wrong pattern provided for match(\"" << pattern << "\",\"" << text << "\")\n";
}
   return result;
}
static inline std::string substr_wrapper(const char *str, size_t idx, size_t len) {
   std::string sub_str, result; 
   try { result = std::string(str).substr(idx,len); } catch(...) { 
     std::cerr << "warning: wrong index position provided by substr(\"";
     std::cerr << str << "\"," << idx << "," << len << ") functor.\n";
   } return result;
}
public:
SymbolTable symTable;
// -- Table: edge
ram::Relation<Auto,2, ram::index<0>>* rel_1_edge;
souffle::RelationWrapper<0,ram::Relation<Auto,2, ram::index<0>>,Tuple<RamDomain,2>,2,true,false> wrapper_rel_1_edge;
// -- Table: path
ram::Relation<Auto,2, ram::index<0,1>>* rel_2_path;
souffle::RelationWrapper<1,ram::Relation<Auto,2, ram::index<0,1>>,Tuple<RamDomain,2>,2,false,true> wrapper_rel_2_path;
// -- Table: @delta_path
ram::Relation<Auto,2>* rel_3_delta_path;
// -- Table: @new_path
ram::Relation<Auto,2>* rel_4_new_path;
public:
Sf_load_print() : 
rel_1_edge(new ram::Relation<Auto,2, ram::index<0>>()),
wrapper_rel_1_edge(*rel_1_edge,symTable,"edge",std::array<const char *,2>{"s:Node","s:Node"},std::array<const char *,2>{"node1","node2"}),
rel_2_path(new ram::Relation<Auto,2, ram::index<0,1>>()),
wrapper_rel_2_path(*rel_2_path,symTable,"path",std::array<const char *,2>{"s:Node","s:Node"},std::array<const char *,2>{"node1","node2"}),
rel_3_delta_path(new ram::Relation<Auto,2>()),
rel_4_new_path(new ram::Relation<Auto,2>()){
addRelation("edge",&wrapper_rel_1_edge,1,0);
addRelation("path",&wrapper_rel_2_path,0,1);
}
~Sf_load_print() {
delete rel_1_edge;
delete rel_2_path;
delete rel_3_delta_path;
delete rel_4_new_path;
}
void run() {
// -- initialize counter --
std::atomic<RamDomain> ctr(0);

#if defined(__EMBEDDED_SOUFFLE__) && defined(_OPENMP)
omp_set_num_threads(1);
#endif

// -- query evaluation --
if (!rel_1_edge->empty()) {
auto part = rel_1_edge->partition();
PARALLEL_START;
CREATE_OP_CONTEXT(rel_1_edge_op_ctxt,rel_1_edge->createContext());
CREATE_OP_CONTEXT(rel_2_path_op_ctxt,rel_2_path->createContext());
pfor(auto it = part.begin(); it<part.end(); ++it) 
for(const auto& env0 : *it) {
Tuple<RamDomain,2> tuple({(RamDomain)(env0[0]),(RamDomain)(env0[1])});
rel_2_path->insert(tuple,READ_OP_CONTEXT(rel_2_path_op_ctxt));
}
PARALLEL_END;
}
rel_3_delta_path->insertAll(*rel_2_path);
for(;;) {
if (!rel_3_delta_path->empty()&&!rel_1_edge->empty()) {
auto part = rel_3_delta_path->partition();
PARALLEL_START;
CREATE_OP_CONTEXT(rel_3_delta_path_op_ctxt,rel_3_delta_path->createContext());
CREATE_OP_CONTEXT(rel_4_new_path_op_ctxt,rel_4_new_path->createContext());
CREATE_OP_CONTEXT(rel_1_edge_op_ctxt,rel_1_edge->createContext());
CREATE_OP_CONTEXT(rel_2_path_op_ctxt,rel_2_path->createContext());
pfor(auto it = part.begin(); it<part.end(); ++it) 
for(const auto& env0 : *it) {
const Tuple<RamDomain,2> key({env0[1],0});
auto range = rel_1_edge->equalRange<0>(key,READ_OP_CONTEXT(rel_1_edge_op_ctxt));
for(const auto& env1 : range) {
if( !rel_2_path->contains(Tuple<RamDomain,2>({env0[0],env1[1]}),READ_OP_CONTEXT(rel_2_path_op_ctxt))) {
Tuple<RamDomain,2> tuple({(RamDomain)(env0[0]),(RamDomain)(env1[1])});
rel_4_new_path->insert(tuple,READ_OP_CONTEXT(rel_4_new_path_op_ctxt));
}
}
}
PARALLEL_END;
}
if(rel_4_new_path->empty()) break;
rel_2_path->insertAll(*rel_4_new_path);
{
auto rel_0 = rel_3_delta_path;
rel_3_delta_path = rel_4_new_path;
rel_4_new_path = rel_0;
}
rel_4_new_path->purge();
}
rel_3_delta_path->purge();
rel_4_new_path->purge();
}
public:
void printAll(std::string dirname) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"filename","./path.csv"},{"name","path"}});
if (!dirname.empty() && directiveMap["IO"] == "file" && directiveMap["filename"].front() != '/') {directiveMap["filename"] = dirname + "/" + directiveMap["filename"];}IODirectives ioDirectives(directiveMap);
IOSystem::getInstance().getWriter(SymbolMask({1, 1}), symTable, ioDirectives)->writeAll(*rel_2_path);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}
public:
void loadAll(std::string dirname) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"filename","./edge.facts"},{"name","edge"}});
if (!dirname.empty() && directiveMap["IO"] == "file" && directiveMap["filename"].front() != '/') {directiveMap["filename"] = dirname + "/" + directiveMap["filename"];}IODirectives ioDirectives(directiveMap);
IOSystem::getInstance().getReader(SymbolMask({1, 1}), symTable, ioDirectives)->readAll(*rel_1_edge);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}
public:
void dumpInputs(std::ostream& out = std::cout) {
try {IODirectives ioDirectives;
ioDirectives.setIOType("stdout");
ioDirectives.setRelationName("rel_1_edge");
IOSystem::getInstance().getWriter(SymbolMask({1, 1}), symTable, ioDirectives)->writeAll(*rel_1_edge);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}
public:
void dumpOutputs(std::ostream& out = std::cout) {
try {IODirectives ioDirectives;
ioDirectives.setIOType("stdout");
ioDirectives.setRelationName("rel_2_path");
IOSystem::getInstance().getWriter(SymbolMask({1, 1}), symTable, ioDirectives)->writeAll(*rel_2_path);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}
public:
const SymbolTable &getSymbolTable() const {
return symTable;
}
};
SouffleProgram *newInstance_load_print(){return new Sf_load_print;}
SymbolTable *getST_load_print(SouffleProgram *p){return &reinterpret_cast<Sf_load_print*>(p)->symTable;}
#ifdef __EMBEDDED_SOUFFLE__
class factory_Sf_load_print: public souffle::ProgramFactory {
SouffleProgram *newInstance() {
return new Sf_load_print();
};
public:
factory_Sf_load_print() : ProgramFactory("load_print"){}
};
static factory_Sf_load_print __factory_Sf_load_print_instance;
}
#else
}
int main(int argc, char** argv)
{
souffle::CmdOptions opt(R"(load_print.dl)",
R"(.)",
R"(.)",
false,
R"()",
1
);
if (!opt.parse(argc,argv)) return 1;
#if defined(_OPENMP) 
omp_set_nested(true);
#endif
souffle::Sf_load_print obj;
obj.loadAll(opt.getInputFileDir());
obj.run();
obj.printAll(opt.getOutputFileDir());
return 0;
}
#endif

#include "souffle/CompiledSouffle.h"
#include "souffle/Explain.h"

namespace souffle {
using namespace ram;
class Sf_path : public SouffleProgram {
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
ram::Relation<Auto,2>* rel_1_edge;
souffle::RelationWrapper<0,ram::Relation<Auto,2>,Tuple<RamDomain,2>,2,true,false> wrapper_rel_1_edge;
// -- Table: edge_new
ram::Relation<Auto,1>* rel_2_edge_new;
// -- Table: edge_output
ram::Relation<Auto,3>* rel_3_edge_output;
souffle::RelationWrapper<1,ram::Relation<Auto,3>,Tuple<RamDomain,3>,3,false,true> wrapper_rel_3_edge_output;
// -- Table: path_new
ram::Relation<Auto,1, ram::index<0>>* rel_4_path_new;
// -- Table: @delta_path_new
ram::Relation<Auto,1>* rel_5_delta_path_new;
// -- Table: @new_path_new
ram::Relation<Auto,1>* rel_6_new_path_new;
// -- Table: path_new_0
ram::Relation<Auto,2>* rel_7_path_new_0;
souffle::RelationWrapper<2,ram::Relation<Auto,2>,Tuple<RamDomain,2>,2,false,true> wrapper_rel_7_path_new_0;
// -- Table: path_new_0_info
ram::Relation<Auto,3>* rel_8_path_new_0_info;
souffle::RelationWrapper<3,ram::Relation<Auto,3>,Tuple<RamDomain,3>,3,false,true> wrapper_rel_8_path_new_0_info;
// -- Table: path_new_1
ram::Relation<Auto,3, ram::index<0,1,2>>* rel_9_path_new_1;
souffle::RelationWrapper<4,ram::Relation<Auto,3, ram::index<0,1,2>>,Tuple<RamDomain,3>,3,false,true> wrapper_rel_9_path_new_1;
// -- Table: @delta_path_new_1
ram::Relation<Auto,3>* rel_10_delta_path_new_1;
// -- Table: @new_path_new_1
ram::Relation<Auto,3>* rel_11_new_path_new_1;
// -- Table: path_new_1_info
ram::Relation<Auto,4>* rel_12_path_new_1_info;
souffle::RelationWrapper<5,ram::Relation<Auto,4>,Tuple<RamDomain,4>,4,false,true> wrapper_rel_12_path_new_1_info;
// -- Table: path_output
ram::Relation<Auto,3>* rel_13_path_output;
souffle::RelationWrapper<6,ram::Relation<Auto,3>,Tuple<RamDomain,3>,3,false,true> wrapper_rel_13_path_output;
public:
Sf_path() : 
rel_1_edge(new ram::Relation<Auto,2>()),
wrapper_rel_1_edge(*rel_1_edge,symTable,"edge",std::array<const char *,2>{"s:symbol","s:symbol"},std::array<const char *,2>{"x","y"}),
rel_2_edge_new(new ram::Relation<Auto,1>()),
rel_3_edge_output(new ram::Relation<Auto,3>()),
wrapper_rel_3_edge_output(*rel_3_edge_output,symTable,"edge_output",std::array<const char *,3>{"r:edge_type{x#s:symbol,y#s:symbol}","s:symbol","s:symbol"},std::array<const char *,3>{"x","x_0","x_1"}),
rel_4_path_new(new ram::Relation<Auto,1, ram::index<0>>()),
rel_5_delta_path_new(new ram::Relation<Auto,1>()),
rel_6_new_path_new(new ram::Relation<Auto,1>()),
rel_7_path_new_0(new ram::Relation<Auto,2>()),
wrapper_rel_7_path_new_0(*rel_7_path_new_0,symTable,"path_new_0",std::array<const char *,2>{"r:path_type{x#s:symbol,y#s:symbol}","r:edge_type{x#s:symbol,y#s:symbol}"},std::array<const char *,2>{"0","1"}),
rel_8_path_new_0_info(new ram::Relation<Auto,3>()),
wrapper_rel_8_path_new_0_info(*rel_8_path_new_0_info,symTable,"path_new_0_info",std::array<const char *,3>{"s:symbol","s:symbol","s:symbol"},std::array<const char *,3>{"1_rel","orig_name","clause_repr"}),
rel_9_path_new_1(new ram::Relation<Auto,3, ram::index<0,1,2>>()),
wrapper_rel_9_path_new_1(*rel_9_path_new_1,symTable,"path_new_1",std::array<const char *,3>{"r:path_type{x#s:symbol,y#s:symbol}","r:edge_type{x#s:symbol,y#s:symbol}","r:path_type{x#s:symbol,y#s:symbol}"},std::array<const char *,3>{"0","1","2"}),
rel_10_delta_path_new_1(new ram::Relation<Auto,3>()),
rel_11_new_path_new_1(new ram::Relation<Auto,3>()),
rel_12_path_new_1_info(new ram::Relation<Auto,4>()),
wrapper_rel_12_path_new_1_info(*rel_12_path_new_1_info,symTable,"path_new_1_info",std::array<const char *,4>{"s:symbol","s:symbol","s:symbol","s:symbol"},std::array<const char *,4>{"1_rel","2_rel","orig_name","clause_repr"}),
rel_13_path_output(new ram::Relation<Auto,3>()),
wrapper_rel_13_path_output(*rel_13_path_output,symTable,"path_output",std::array<const char *,3>{"r:path_type{x#s:symbol,y#s:symbol}","s:symbol","s:symbol"},std::array<const char *,3>{"x","x_0","x_1"}){
addRelation("edge",&wrapper_rel_1_edge,1,0);
addRelation("edge_output",&wrapper_rel_3_edge_output,0,1);
addRelation("path_new_0",&wrapper_rel_7_path_new_0,0,1);
addRelation("path_new_0_info",&wrapper_rel_8_path_new_0_info,0,1);
addRelation("path_new_1",&wrapper_rel_9_path_new_1,0,1);
addRelation("path_new_1_info",&wrapper_rel_12_path_new_1_info,0,1);
addRelation("path_output",&wrapper_rel_13_path_output,0,1);
// -- initialize symbol table --
static const char *symbols[]={
	R"(edge)",
	R"(path)",
	R"(path(x,y) :- 
   edge(x,y).)",
	R"(path(x,z) :- 
   edge(x,y),
   path(y,z).)",
};
symTable.insert(symbols,4);

}
~Sf_path() {
delete rel_1_edge;
delete rel_2_edge_new;
delete rel_3_edge_output;
delete rel_4_path_new;
delete rel_5_delta_path_new;
delete rel_6_new_path_new;
delete rel_7_path_new_0;
delete rel_8_path_new_0_info;
delete rel_9_path_new_1;
delete rel_10_delta_path_new_1;
delete rel_11_new_path_new_1;
delete rel_12_path_new_1_info;
delete rel_13_path_output;
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
CREATE_OP_CONTEXT(rel_2_edge_new_op_ctxt,rel_2_edge_new->createContext());
pfor(auto it = part.begin(); it<part.end(); ++it) 
for(const auto& env0 : *it) {
Tuple<RamDomain,1> tuple({(RamDomain)(pack(ram::Tuple<RamDomain,2>({env0[0],env0[1]})))});
rel_2_edge_new->insert(tuple,READ_OP_CONTEXT(rel_2_edge_new_op_ctxt));
}
PARALLEL_END;
}
if (!rel_2_edge_new->empty()) {
auto part = rel_2_edge_new->partition();
PARALLEL_START;
CREATE_OP_CONTEXT(rel_2_edge_new_op_ctxt,rel_2_edge_new->createContext());
CREATE_OP_CONTEXT(rel_3_edge_output_op_ctxt,rel_3_edge_output->createContext());
pfor(auto it = part.begin(); it<part.end(); ++it) 
for(const auto& env0 : *it) {
auto ref = env0[0];
if (isNull<ram::Tuple<RamDomain,2>>(ref)) continue;
ram::Tuple<RamDomain,2> env1 = unpack<ram::Tuple<RamDomain,2>>(ref);
{
Tuple<RamDomain,3> tuple({(RamDomain)(pack(ram::Tuple<RamDomain,2>({env1[0],env1[1]}))),(RamDomain)(env1[0]),(RamDomain)(env1[1])});
rel_3_edge_output->insert(tuple,READ_OP_CONTEXT(rel_3_edge_output_op_ctxt));
}
}
PARALLEL_END;
}
if (!rel_2_edge_new->empty()) {
auto part = rel_2_edge_new->partition();
PARALLEL_START;
CREATE_OP_CONTEXT(rel_2_edge_new_op_ctxt,rel_2_edge_new->createContext());
CREATE_OP_CONTEXT(rel_7_path_new_0_op_ctxt,rel_7_path_new_0->createContext());
pfor(auto it = part.begin(); it<part.end(); ++it) 
for(const auto& env0 : *it) {
auto ref = env0[0];
if (isNull<ram::Tuple<RamDomain,2>>(ref)) continue;
ram::Tuple<RamDomain,2> env1 = unpack<ram::Tuple<RamDomain,2>>(ref);
{
Tuple<RamDomain,2> tuple({(RamDomain)(pack(ram::Tuple<RamDomain,2>({env1[0],env1[1]}))),(RamDomain)(pack(ram::Tuple<RamDomain,2>({env1[0],env1[1]})))});
rel_7_path_new_0->insert(tuple,READ_OP_CONTEXT(rel_7_path_new_0_op_ctxt));
}
}
PARALLEL_END;
}
if (!rel_7_path_new_0->empty()) {
auto part = rel_7_path_new_0->partition();
PARALLEL_START;
CREATE_OP_CONTEXT(rel_4_path_new_op_ctxt,rel_4_path_new->createContext());
CREATE_OP_CONTEXT(rel_7_path_new_0_op_ctxt,rel_7_path_new_0->createContext());
pfor(auto it = part.begin(); it<part.end(); ++it) 
for(const auto& env0 : *it) {
Tuple<RamDomain,1> tuple({(RamDomain)(env0[0])});
rel_4_path_new->insert(tuple,READ_OP_CONTEXT(rel_4_path_new_op_ctxt));
}
PARALLEL_END;
}
rel_5_delta_path_new->insertAll(*rel_4_path_new);
rel_10_delta_path_new_1->insertAll(*rel_9_path_new_1);
for(;;) {
SECTIONS_START;
SECTION_START;
if (!rel_10_delta_path_new_1->empty()) {
auto part = rel_10_delta_path_new_1->partition();
PARALLEL_START;
CREATE_OP_CONTEXT(rel_10_delta_path_new_1_op_ctxt,rel_10_delta_path_new_1->createContext());
CREATE_OP_CONTEXT(rel_6_new_path_new_op_ctxt,rel_6_new_path_new->createContext());
CREATE_OP_CONTEXT(rel_4_path_new_op_ctxt,rel_4_path_new->createContext());
pfor(auto it = part.begin(); it<part.end(); ++it) 
for(const auto& env0 : *it) {
if( !rel_4_path_new->contains(Tuple<RamDomain,1>({env0[0]}),READ_OP_CONTEXT(rel_4_path_new_op_ctxt))) {
Tuple<RamDomain,1> tuple({(RamDomain)(env0[0])});
rel_6_new_path_new->insert(tuple,READ_OP_CONTEXT(rel_6_new_path_new_op_ctxt));
}
}
PARALLEL_END;
}
SECTION_END
SECTION_START;
if (!rel_5_delta_path_new->empty()&&!rel_2_edge_new->empty()) {
auto part = rel_2_edge_new->partition();
PARALLEL_START;
CREATE_OP_CONTEXT(rel_5_delta_path_new_op_ctxt,rel_5_delta_path_new->createContext());
CREATE_OP_CONTEXT(rel_11_new_path_new_1_op_ctxt,rel_11_new_path_new_1->createContext());
CREATE_OP_CONTEXT(rel_2_edge_new_op_ctxt,rel_2_edge_new->createContext());
CREATE_OP_CONTEXT(rel_9_path_new_1_op_ctxt,rel_9_path_new_1->createContext());
pfor(auto it = part.begin(); it<part.end(); ++it) 
for(const auto& env0 : *it) {
auto ref = env0[0];
if (isNull<ram::Tuple<RamDomain,2>>(ref)) continue;
ram::Tuple<RamDomain,2> env1 = unpack<ram::Tuple<RamDomain,2>>(ref);
{
for(const auto& env2 : *rel_5_delta_path_new) {
auto ref = env2[0];
if (isNull<ram::Tuple<RamDomain,2>>(ref)) continue;
ram::Tuple<RamDomain,2> env3 = unpack<ram::Tuple<RamDomain,2>>(ref);
{
if( ((((env1[1]) == (env3[0]))) && (!rel_9_path_new_1->contains(Tuple<RamDomain,3>({pack(ram::Tuple<RamDomain,2>({env1[0],env3[1]})),pack(ram::Tuple<RamDomain,2>({env1[0],env1[1]})),pack(ram::Tuple<RamDomain,2>({env1[1],env3[1]}))}),READ_OP_CONTEXT(rel_9_path_new_1_op_ctxt))))) {
Tuple<RamDomain,3> tuple({(RamDomain)(pack(ram::Tuple<RamDomain,2>({env1[0],env3[1]}))),(RamDomain)(pack(ram::Tuple<RamDomain,2>({env1[0],env1[1]}))),(RamDomain)(pack(ram::Tuple<RamDomain,2>({env1[1],env3[1]})))});
rel_11_new_path_new_1->insert(tuple,READ_OP_CONTEXT(rel_11_new_path_new_1_op_ctxt));
}
}
}
}
}
PARALLEL_END;
}
SECTION_END
SECTIONS_END;
if(((rel_6_new_path_new->empty()) && (rel_11_new_path_new_1->empty()))) break;
rel_4_path_new->insertAll(*rel_6_new_path_new);
{
auto rel_0 = rel_5_delta_path_new;
rel_5_delta_path_new = rel_6_new_path_new;
rel_6_new_path_new = rel_0;
}
rel_6_new_path_new->purge();
rel_9_path_new_1->insertAll(*rel_11_new_path_new_1);
{
auto rel_0 = rel_10_delta_path_new_1;
rel_10_delta_path_new_1 = rel_11_new_path_new_1;
rel_11_new_path_new_1 = rel_0;
}
rel_11_new_path_new_1->purge();
}
rel_5_delta_path_new->purge();
rel_6_new_path_new->purge();
rel_10_delta_path_new_1->purge();
rel_11_new_path_new_1->purge();
if (!rel_4_path_new->empty()) {
auto part = rel_4_path_new->partition();
PARALLEL_START;
CREATE_OP_CONTEXT(rel_4_path_new_op_ctxt,rel_4_path_new->createContext());
CREATE_OP_CONTEXT(rel_13_path_output_op_ctxt,rel_13_path_output->createContext());
pfor(auto it = part.begin(); it<part.end(); ++it) 
for(const auto& env0 : *it) {
auto ref = env0[0];
if (isNull<ram::Tuple<RamDomain,2>>(ref)) continue;
ram::Tuple<RamDomain,2> env1 = unpack<ram::Tuple<RamDomain,2>>(ref);
{
Tuple<RamDomain,3> tuple({(RamDomain)(pack(ram::Tuple<RamDomain,2>({env1[0],env1[1]}))),(RamDomain)(env1[0]),(RamDomain)(env1[1])});
rel_13_path_output->insert(tuple,READ_OP_CONTEXT(rel_13_path_output_op_ctxt));
}
}
PARALLEL_END;
}
rel_8_path_new_0_info->insert(0,1,2);
rel_12_path_new_1_info->insert(0,1,1,3);
}
public:
void printAll(std::string dirname) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"filename","./edge_output.csv"},{"name","edge_output"}});
if (!dirname.empty() && directiveMap["IO"] == "file" && directiveMap["filename"].front() != '/') {directiveMap["filename"] = dirname + "/" + directiveMap["filename"];}IODirectives ioDirectives(directiveMap);
IOSystem::getInstance().getWriter(SymbolMask({0, 1, 1}), symTable, ioDirectives)->writeAll(*rel_3_edge_output);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"filename","./path_new_0.csv"},{"name","path_new_0"}});
if (!dirname.empty() && directiveMap["IO"] == "file" && directiveMap["filename"].front() != '/') {directiveMap["filename"] = dirname + "/" + directiveMap["filename"];}IODirectives ioDirectives(directiveMap);
IOSystem::getInstance().getWriter(SymbolMask({0, 0}), symTable, ioDirectives)->writeAll(*rel_7_path_new_0);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"filename","./path_new_0_info.csv"},{"name","path_new_0_info"}});
if (!dirname.empty() && directiveMap["IO"] == "file" && directiveMap["filename"].front() != '/') {directiveMap["filename"] = dirname + "/" + directiveMap["filename"];}IODirectives ioDirectives(directiveMap);
IOSystem::getInstance().getWriter(SymbolMask({1, 1, 1}), symTable, ioDirectives)->writeAll(*rel_8_path_new_0_info);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"filename","./path_new_1.csv"},{"name","path_new_1"}});
if (!dirname.empty() && directiveMap["IO"] == "file" && directiveMap["filename"].front() != '/') {directiveMap["filename"] = dirname + "/" + directiveMap["filename"];}IODirectives ioDirectives(directiveMap);
IOSystem::getInstance().getWriter(SymbolMask({0, 0, 0}), symTable, ioDirectives)->writeAll(*rel_9_path_new_1);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"filename","./path_new_1_info.csv"},{"name","path_new_1_info"}});
if (!dirname.empty() && directiveMap["IO"] == "file" && directiveMap["filename"].front() != '/') {directiveMap["filename"] = dirname + "/" + directiveMap["filename"];}IODirectives ioDirectives(directiveMap);
IOSystem::getInstance().getWriter(SymbolMask({1, 1, 1, 1}), symTable, ioDirectives)->writeAll(*rel_12_path_new_1_info);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"filename","./path_output.csv"},{"name","path_output"}});
if (!dirname.empty() && directiveMap["IO"] == "file" && directiveMap["filename"].front() != '/') {directiveMap["filename"] = dirname + "/" + directiveMap["filename"];}IODirectives ioDirectives(directiveMap);
IOSystem::getInstance().getWriter(SymbolMask({0, 1, 1}), symTable, ioDirectives)->writeAll(*rel_13_path_output);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}
public:
void loadAll(std::string dirname) {
try {std::map<std::string, std::string> directiveMap({{"IO","file"},{"filename","facts/edge.facts"},{"name","edge"}});
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
ioDirectives.setRelationName("rel_3_edge_output");
IOSystem::getInstance().getWriter(SymbolMask({0, 1, 1}), symTable, ioDirectives)->writeAll(*rel_3_edge_output);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {IODirectives ioDirectives;
ioDirectives.setIOType("stdout");
ioDirectives.setRelationName("rel_7_path_new_0");
IOSystem::getInstance().getWriter(SymbolMask({0, 0}), symTable, ioDirectives)->writeAll(*rel_7_path_new_0);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {IODirectives ioDirectives;
ioDirectives.setIOType("stdout");
ioDirectives.setRelationName("rel_8_path_new_0_info");
IOSystem::getInstance().getWriter(SymbolMask({1, 1, 1}), symTable, ioDirectives)->writeAll(*rel_8_path_new_0_info);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {IODirectives ioDirectives;
ioDirectives.setIOType("stdout");
ioDirectives.setRelationName("rel_9_path_new_1");
IOSystem::getInstance().getWriter(SymbolMask({0, 0, 0}), symTable, ioDirectives)->writeAll(*rel_9_path_new_1);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {IODirectives ioDirectives;
ioDirectives.setIOType("stdout");
ioDirectives.setRelationName("rel_12_path_new_1_info");
IOSystem::getInstance().getWriter(SymbolMask({1, 1, 1, 1}), symTable, ioDirectives)->writeAll(*rel_12_path_new_1_info);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
try {IODirectives ioDirectives;
ioDirectives.setIOType("stdout");
ioDirectives.setRelationName("rel_13_path_output");
IOSystem::getInstance().getWriter(SymbolMask({0, 1, 1}), symTable, ioDirectives)->writeAll(*rel_13_path_output);
} catch (std::exception& e) {std::cerr << e.what();exit(1);}
}
public:
const SymbolTable &getSymbolTable() const {
return symTable;
}
};
SouffleProgram *newInstance_path(){return new Sf_path;}
SymbolTable *getST_path(SouffleProgram *p){return &reinterpret_cast<Sf_path*>(p)->symTable;}
#ifdef __EMBEDDED_SOUFFLE__
class factory_Sf_path: public souffle::ProgramFactory {
SouffleProgram *newInstance() {
return new Sf_path();
};
public:
factory_Sf_path() : ProgramFactory("path"){}
};
static factory_Sf_path __factory_Sf_path_instance;
}
#else
}
int main(int argc, char** argv)
{
souffle::CmdOptions opt(R"(path.dl)",
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
souffle::Sf_path obj;
obj.loadAll(opt.getInputFileDir());
obj.run();
obj.printAll(opt.getOutputFileDir());
explain(obj);
return 0;
}
#endif

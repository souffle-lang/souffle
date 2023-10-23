// functors.cpp
#include "souffle/RecordTable.h"
#include "souffle/SymbolTable.h"

extern "C" {

souffle::RamDomain id(souffle::SymbolTable*, souffle::RecordTable*, souffle::RamDomain x) {
    return x;
}

souffle::RamDomain blubb(
        souffle::SymbolTable* symbolTable, souffle::RecordTable*, souffle::RamDomain, souffle::RamDomain y) {
    symbolTable->decode(y);
    return 1;
}
}

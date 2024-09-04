#include "souffle/RecordTable.h"
#include "souffle/SymbolTable.h"

extern "C" {

souffle::RamDomain id(
        souffle::SymbolTable* symbolTable, souffle::RecordTable* recordTable, souffle::RamDomain x) {
    return x;
}

souffle::RamDomain decode(
        souffle::SymbolTable* symbolTable, souffle::RecordTable* recordTable, souffle::RamDomain x) {
    symbolTable->decode(x);
    return x;
}
}

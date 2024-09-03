#include "souffle/SymbolTable.h"
#include "souffle/RecordTable.h"

extern "C" {

souffle::RamDomain id(souffle::SymbolTable* symbolTable, souffle::RecordTable* recordTable, souffle::RamDomain x) {
  return x;
}

souffle::RamDomain decode(souffle::SymbolTable* symbolTable, souffle::RecordTable* recordTable, souffle::RamDomain x) {
  symbolTable->decode(x);
  return x;
}

}

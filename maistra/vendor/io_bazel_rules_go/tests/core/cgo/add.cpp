#include "add.h"

#if !defined(RULES_GO_CPP) || !defined(RULES_GO_CXX) || defined(RULES_GO_C)
#error This is a C++ file, only RULES_GO_CXX and RULES_GO_CPP should be defined.
#endif

int add_cpp(int a, int b) {
    return a + b;
}

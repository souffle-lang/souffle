#pragma once
#include <iostream>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HASH_SIZE (1048583)

namespace souffle {

#define SLOOKUP(s) StringPool::instance()->lookup(s)

class StringPool {
    /* Hash table */
    struct hashentry {
        char* str;
        hashentry* next;
        hashentry(char* s = nullptr, struct hashentry* n = NULL) : str(s), next(n) {}
    };
    static hashentry* hashtab[HASH_SIZE];

    /* Hash function */
    inline size_t hash(const char* str) {
        size_t hash = 5381;
        int c;
        while ((c = *str++) != 0) {
            hash = ((hash << 5) + hash) + c;
        }
        return hash % HASH_SIZE;
    }

    ~StringPool() {
        for (size_t i = 0; i < HASH_SIZE; i++) {
            hashentry* q;
            for (hashentry* p = hashtab[i]; p != nullptr; p = q) {
                q = p->next;
                free(p->str);
                delete p;
            }
        }
    }

public:
    static StringPool* instance() {
        static StringPool singleton;
        return &singleton;
    }

    /* lookup a string */
    inline const char* lookup(const char* str) {
        size_t i = hash(str);

        const char* res = nullptr;

        {
            if (hashtab[i] == nullptr) {
                char* nstr = strdup(str);
                hashtab[i] = new hashentry(nstr);
                res = nstr;
            } else {
                for (hashentry* p = hashtab[i]; p != nullptr && res == NULL; p = p->next) {
                    if (!strcmp(p->str, str)) {
                        res = p->str;
                    }
                }
                char* nstr = strdup(str);
                hashtab[i] = new hashentry(nstr, hashtab[i]);
                res = nstr;
            }
        }

        return res;
    }
};

}  // end namespace souffle

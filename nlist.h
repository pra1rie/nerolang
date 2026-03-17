#ifndef __NERO_LIST_H
#define __NERO_LIST_H

#include <stdlib.h>

#define _NERO_LIST_ALLOC_SIZE 1024
#define LENGTH(x) (sizeof(x) / sizeof((x)[0]))

#define LIST(T) struct { int alloc, sz; T *ptr; }
#define LIST_EMPTY() { .alloc = 0, .sz = 0, .ptr = NULL }
#define LIST_ALLOCN(T, N) { .alloc = N, .sz = 0, .ptr = malloc(N*sizeof(T)) }
#define LIST_ALLOC(T) LIST_ALLOCN(T, _NERO_LIST_ALLOC_SIZE)
#define LIST_FREE_OP(l, op) do { free(l op ptr); l op ptr = NULL; l op alloc = l op sz = 0; } while(0)
#define LIST_PUSH_OP(l, v, op) do { if (((l) op sz)+1 >= (l) op alloc) (l) op ptr = realloc((l) op ptr, \
                        ((l) op alloc += _NERO_LIST_ALLOC_SIZE)*sizeof(((l) op ptr)[0])); \
                        (l) op ptr[(l) op sz++] = v; } while(0)
#define LIST_POP_OP(l, op) do { (--(l) op sz); } while(0)

#define LIST_FREE(l) LIST_FREE_OP(l, .)
#define LIST_FREEP(l) LIST_FREE_OP(l, ->)
#define LIST_PUSH(l, v) LIST_PUSH_OP(l, v, .)
#define LIST_PUSHP(l, v) LIST_PUSH_OP(l, v, ->)
#define LIST_POP(l) LIST_POP_OP(l, .)
#define LIST_POPP(l) LIST_POP_OP(l, ->)

#endif


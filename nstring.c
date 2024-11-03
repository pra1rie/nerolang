#include <stdlib.h>
#include <string.h>
#include "nstring.h"

void strcats(String *a, String *b) {
    if (a->sz+b->sz >= a->alloc)
        a->ptr = realloc(a->ptr, a->alloc += (b->sz+_NERO_STRING_ALLOC_SIZE));
    memmove(a->ptr+a->sz, b->ptr, b->sz);
    a->sz += b->sz;
}

void strcatp(String *a, char *b) {
    strcats(a, &(String) {0, strlen(b), b});
}


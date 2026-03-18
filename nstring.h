#ifndef __NERO_STRING_H
#define __NERO_STRING_H

#include <string.h>
#include "nlist.h"

#define _NERO_STRING_ALLOC_SIZE 1024

typedef LIST(char) String;

#define STRALLOC() (String) STRALLOCN(_NERO_STRING_ALLOC_SIZE)
#define STRALLOCN(N) (String) LIST_ALLOCN(char, N)
#define STRCMPP_OP(a, b, op) ((a) op sz == strlen(b) && !strncmp((a) op ptr, b, (a) op sz))
#define STRCMPS_OP(a, b, op) ((a) op sz == (b) op sz && !strncmp((a) op ptr, (b) op ptr, (a) op sz))
#define STRCMPP(a, b) STRCMPP_OP(a, b, .)
#define STRCMPPP(a, b) STRCMPP_OP(a, b, ->)
#define STRCMPS(a, b) STRCMPS_OP(a, b, .)
#define STRCMPSP(a, b) STRCMPS_OP(a, b, ->)

#endif

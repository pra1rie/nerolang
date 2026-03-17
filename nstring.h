#ifndef __NERO_STRING_H
#define __NERO_STRING_H

#include <string.h>
#include "nlist.h"

#define _NERO_STRING_ALLOC_SIZE 512

typedef LIST(char) String;

void strcats(String *a, String *b);
void strcatp(String *a, char *b);

#define STRALLOC() (String) LIST_ALLOC(char)
#define STRALLOCN(N) (String) LIST_ALLOCN(char, N)
#define STRFREE(a) LIST_FREE(a)
#define STRFREEP(a) LIST_FREEP(a)
#define STRCMPP_OP(a, b, op) ((a) op sz == strlen(b) && !strncmp((a) op ptr, b, (a) op sz))
#define STRCMPS_OP(a, b, op) ((a) op sz == (b) op sz && !strncmp((a) op ptr, (b) op ptr, (a) op sz))
#define STRCMPP(a, b) STRCMPP_OP(a, b, .)
#define STRCMPPP(a, b) STRCMPP_OP(a, b, ->)
#define STRCMPS(a, b) STRCMPS_OP(a, b, .)
#define STRCMPSP(a, b) STRCMPS_OP(a, b, ->)

#endif

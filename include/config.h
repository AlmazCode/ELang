/* ELang shared configuration */
#ifndef ELANG_CONFIG_H
#define ELANG_CONFIG_H

#include <stdio.h>
#include <stdlib.h>

/* --- Shared constants --- */
#define MAX_IDENT_LEN       128
#define MAX_PATH_LEN        1024
#define MAX_MODULES         64
#define MAX_PARSE_DEPTH     2000

/* --- Version --- */
#define ELANG_VERSION       "0.45.0"

/* --- Safe allocation macros --- */
#define CHECK_ALLOC(ptr) do { \
    if (!(ptr)) { \
        fprintf(stderr, "Out of memory at %s:%d\n", __FILE__, __LINE__); \
        abort(); \
    } \
} while(0)

#define SAFE_MALLOC(size) ({ \
    void *_p = malloc(size); \
    CHECK_ALLOC(_p); \
    _p; \
})

#define SAFE_CALLOC(count, size) ({ \
    void *_p = calloc(count, size); \
    CHECK_ALLOC(_p); \
    _p; \
})

#define SAFE_REALLOC(ptr, size) ({ \
    void *_p = realloc(ptr, size); \
    CHECK_ALLOC(_p); \
    _p; \
})

#define SAFE_STRDUP(s) ({ \
    char *_p = strdup(s); \
    CHECK_ALLOC(_p); \
    _p; \
})

#define SAFE_STRNDUP(s, n) ({ \
    char *_p = strndup(s, n); \
    CHECK_ALLOC(_p); \
    _p; \
})

#endif

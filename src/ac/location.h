#ifndef AC_LOCATION_H
#define AC_LOCATION_H

#include <stdbool.h>

#include "re_lib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ac_location ac_location;
struct ac_location
{
    const char* filepath;
    int row, col, pos;
    
    strv content; /* view to the whole content of the file for convenience */
};

static ac_location ac_location_empty() {
    ac_location l;
    l.filepath = 0;
    l.row = -1;
    l.col = -1;
    l.pos = -1;
    l.content = strv_make();
    return l;
}

static void ac_location_init_with_file(ac_location* l, const char* filepath, strv content) {
    l->filepath = filepath;
    l->row = 1;
    l->col = 0;
    l->pos = 0;
    l->content = content;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_LOCATION_H */
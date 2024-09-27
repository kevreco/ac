#include "re_lib.h"

/* re/dstr.h is a single file and header only library, we have to define the implementation at least in one file */
#define DSTR_IMPLEMENTATION
#include <re/dstr.h>

#define DARR_IMPLEMENTATION
#include <re/darr.h>

#define RE_FILE_IMPLEMENTATION
#include <re/file.h>

#define RE_PATH_IMPLEMENTATION
#include <re/path.h>

#define RE_AA_IMPLEMENTATION
#include <re/arena_alloc.h>
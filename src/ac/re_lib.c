
#include <re/dstr.h>
#include <re/file.h>
#include <re/path.h>

/* re/dstr.h is a single file and header only library, we have to define the implementation at least in one file */
#define DSTR_IMPLEMENTATION
#include <re/dstr.h>

#define RE_FILE_IMPLEMENTATION
#include <re/file.h>

#define RE_PATH_IMPLEMENTATION
#include <re/path.h>
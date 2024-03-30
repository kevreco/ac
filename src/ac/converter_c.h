#ifndef AC_CONVERTER_C_H
#define AC_CONVERTER_C_H

#include "manager.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ac_converter_c
{
    struct ac_manager* mgr;
    int indentation_level;
    const char* indent_pattern;
    dstr string_buffer;
};

void ac_converter_c_init(struct ac_converter_c* c, struct ac_manager* mgr);
void ac_converter_c_destroy(struct ac_converter_c* c);

void ac_converter_c_convert(struct ac_converter_c* c, const char* filepath);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_CONVERTER_C_H */
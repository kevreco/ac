#include "manager.h"

#include "stdbool.h"

#include "re/file.h"
#include "re/path.h"

#include "global.h"

bool try_get_file_content(const char* filepath, dstr* content);

void ac_manager_init(ac_manager* m)
{
    memset(m, 0, sizeof(ac_manager));
}

void ac_manager_destroy(ac_manager* m)
{
    if (!dstr_empty(&m->source_file.content))
    {
        dstr_destroy(&m->source_file.content);
    }
}

ac_source_file* ac_manager_load_content(ac_manager* m, const char* filepath)
{
    if (m->source_file.content.data != 0)
    {
        ac_report_error("Internal error: @TEMP we can only load a single file per manager instance.");
        return 0;
    }

    if (!re_file_exists_str(filepath))
    {
        ac_report_error("w_manager_load_content: file '%s' does not exist.", filepath);
        return 0;
    }

    dstr content;
    dstr_init(&content);

    if (try_get_file_content(filepath, &content))
    {
        m->source_file.filepath = filepath;
        m->source_file.content = content;

        return &m->source_file;
    }
    else
    {
        ac_report_error("Could not load file into memory.");
        return 0;
    }
}

bool try_get_file_content(const char* filepath, dstr* content)
{
    if (!re_file_exists_str(filepath)) {
        ac_report_error("File does not exist '%s'.\n", filepath);
        return false;
    }

    if (!re_file_open_and_read(content, filepath))
    {
        ac_report_error("Could not open '%s'\n", filepath);
        return false;
    }
   
    if (content->size == 0) {
        ac_report_warning("Empty file '%s'.\n", filepath);
        return true;
    }

    return true;
}
#ifndef AC_INTERNAL_H
#define AC_INTERNAL_H

static bool is_directory_separator(int c)
{
    return c == '\\' || c == '/';
}

static char* path_normalize_slashes(char* path)
{
    for (char* p = path; *p; p += 1)
    {
        if (*p == '\\')
        {
            *p = '/';
        }
    }
    return path;
}

/* Get last part of a path. */
static char* path_basename(const char* path)
{
    char* p = strchr(path, 0);

    while (p > path && !is_directory_separator(p[-1]))
    {
        p -= 1;
    }
       
    return p;
}
/* Get path minus the last part. */
static char* path_directory(char* path)
{
    char* p = path_basename(path);

    while (*p)
    {
        *p = '\0';
        p += 1;
    }
    return path;
}

#endif /* AC_INTERNAL_H */
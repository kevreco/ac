#ifndef CB_ADD_FILES_H
#define CB_ADD_FILES_H

#include "cb_file_it.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 
	Add files from directory that matches the pattern.
	Pattern can contains stars (*), and is not "slash sensitive".
	Multiple stars in a row does not have special effect.
*/
CB_API void
cb_add_files(const char* directory, const char* pattern);

CB_API void
cb_add_files_recursive(const char* directory, const char* pattern);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CB_ADD_FILES_H */

#ifdef CB_IMPLEMENTATION

/* ================================================================ */
/* WILDMATCH_H */
/* Taken from https://github.com/ujr/wildmatch - UNLICENSED
*  - 'cb_decode_utf8' was named 'decode'
*  - 'cb_wildmatch' was created from 'match1'
/* ================================================================ */

/** return nbytes, 0 on end, -1 on error */
CB_INTERNAL int
cb_decode_utf8(const void* p, int* pc)
{
	const int replacement = 0xFFFD;
	const unsigned char* s = (const unsigned char*)p;
	if (s[0] < 0x80) {
		*pc = s[0];
		return *pc ? 1 : 0;
	}
	if ((s[0] & 0xE0) == 0xC0) {
		*pc = (int)(s[0] & 0x1F) << 6
			| (int)(s[1] & 0x3F);
		return 2;
	}
	if ((s[0] & 0xF0) == 0xE0) {
		*pc = (int)(s[0] & 0x0F) << 12
			| (int)(s[1] & 0x3F) << 6
			| (int)(s[2] & 0x3F);
		/* surrogate pairs not allowed in UTF8 */
		if (0xD800 <= *pc && *pc <= 0xDFFF)
			*pc = replacement;
		return 3;
	}
	if ((s[0] & 0xF8) == 0xF0 && (s[0] <= 0xF4)) {
		/* 2nd cond: not greater than 0x10FFFF */
		*pc = (int)(s[0] & 0x07) << 18
			| (int)(s[1] & 0x3F) << 12
			| (int)(s[2] & 0x3F) << 6
			| (int)(s[3] & 0x3F);
		return 4;
	}
	*pc = replacement;
	/*errno = EILSEQ;*/
	return -1;
}

/* backslash and slash are assumed to be the same */
CB_INTERNAL cb_bool
cb_path_char_is_different(int left, int right)
{
	return cb_is_directory_separator((char)left)
		? !cb_is_directory_separator((char)right)
		: left != right;
}

CB_INTERNAL cb_bool
cb_wildmatch(const char* pat, const char* str)
{
	const char* p, * s;
	int pc, sc;
	int len = 0;
	p = s = 0;           /* anchor initially not set */

	if (!pat || !str) { return cb_false; }

	for (;;) {
		len = cb_decode_utf8(pat, &pc);
		if (len < 0)
			return cb_false;
		pat += len;
		if (pc == '*') {
			while (*pat == '*') pat++; /* multiple wildcards have not special effect compared to a single wildcard */
			p = pat;         /* set anchor just after wild star */
			s = str;
			continue;
		}
		len = cb_decode_utf8(str, &sc);
		if (len < 0)
			return cb_false;
		str += len;
		if (sc == '\0')
			return pc == '\0';
		if (pc != '?' && cb_path_char_is_different(pc, sc)) {
			if (!p)
				return cb_false;
			pat = p;         /* resume at anchor in pattern */
			str = s += cb_decode_utf8(s, &pc); /* but one later in string */
			continue;
		}
	}
}

CB_INTERNAL cb_bool
cb_file_it_get_next_glob(cb_file_it* it, const char* pattern)
{
	while (cb_file_it_get_next(it))
	{
		if (cb_wildmatch(pattern, cb_file_it_current_file(it)))
		{
			return cb_true;
		}
	}
	return cb_false;
}

CB_API void
cb_add_files(const char* directory, const char* pattern)
{
	cb_file_it it;
	cb_file_it_init(&it, directory);

	while (cb_file_it_get_next_glob(&it, pattern))
	{
		const char* filepath = cb_file_it_current_file(&it);

		cb_add(cb_FILES, filepath);
	}
}

CB_API void
cb_add_files_recursive(const char* directory, const char* pattern)
{
	cb_file_it it;
	cb_file_it_init_recursive(&it, directory);

	while (cb_file_it_get_next_glob(&it, pattern))
	{
		const char* filepath = cb_file_it_current_file(&it);

		cb_add(cb_FILES, filepath);
	}
}

#endif /* CB_IMPLEMENTATION */
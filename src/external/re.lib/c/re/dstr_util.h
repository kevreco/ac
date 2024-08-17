#ifndef RE_DSTR_UTIL_H
#define RE_DSTR_UTIL_H

#include "dstr.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline int dstr_view_atoi(dstr_view sv)
{
	const dstr_char_t* str = sv.data;
	dstr_size_t size = sv.size;
	
	const dstr_char_t* end = str + size;

	int result = 0;

	int bool_is_negative = 0;

	if (*str == '-') {
		bool_is_negative = 1;
		++str;
	}

	while (*str >= '0' && *str <= '9' && str < end) {
		result = (result * 10) + (*str - '0');
		++str;
	}

	if (bool_is_negative) {
		result = -result;
	}
	
	return result;
}

static inline dstr_bool_t dstr_char_is_alphanum(char c)
{
	return
		(c >= 'a' && c <= 'z')
		|| (c >= 'A' && c <= 'Z')
		|| (c >= '0' && c <= '1')
		;
}

static inline dstr_bool_t dstr_view_contains_char(dstr_view sv, char c)
{
	return dstr_view_find_char(sv, 0, c) != DSTR_NPOS;
}

static inline dstr_bool_t dstr_view_contains_chars(dstr_view sv, dstr_view chars)
{
	const dstr_char_t* end = chars.data + chars.size;
	const dstr_char_t* cursor = chars.data;
	while (cursor < end)
	{
		if (dstr_view_contains_char(sv, *cursor))
		{
			return (dstr_bool_t)(1);
		}

		++cursor;
	}

	return (dstr_bool_t)(0);
}

static inline dstr_bool_t dstr_view_begins_with(dstr_view sv, dstr_view rhs)
{
	if (sv.size < rhs.size)
	{
		return (dstr_bool_t)(0);
	}

	dstr_view sub = dstr_view_make_from(sv.data, rhs.size);
	return dstr_view_equals(sub, rhs);
}

static inline dstr_bool_t dstr_view_begins_with_str(dstr_view sv, const dstr_char_t* str)
{
	dstr_view str_view = dstr_view_make_from_str(str);
	return dstr_view_begins_with(sv, str_view);
}

static inline dstr_bool_t dstr_view_ends_with(dstr_view sv, dstr_view rhs)
{
	if (sv.size < rhs.size)
	{
		return (dstr_bool_t)(0);
	}

	dstr_view sub = dstr_view_make_from(sv.data + (sv.size - rhs.size), rhs.size);
	return dstr_view_equals(sub, rhs);
}

static inline dstr_bool_t dstr_view_ends_with_str(dstr_view sv, const dstr_char_t* str)
{
	dstr_view str_view = dstr_view_make_from_str(str);
	return dstr_view_ends_with(sv, str_view);
}

// Will start at the end of the string if start_pos == DSTR_NPOS
static inline dstr_size_t dstr_view_find_last_of_char(dstr_view sv, char c, dstr_size_t start_pos)
{
	dstr_size_t index = DSTR_NPOS;

	if (!sv.size)
	{
		return index;
	}

	const char* begin = dstr_view_begin(sv);
	const char* cursor = dstr_view_end(sv) - 1;

	if (start_pos != DSTR_NPOS)
	{
		cursor = begin + (start_pos < sv.size ? start_pos : sv.size); // min
	}

	dstr_size_t i = cursor - begin;
	for (; cursor >= begin; --cursor, --i) {
		if (*cursor == c) {
			index = i;
			break;
		}
	}

	return index;
}

static inline dstr_size_t dstr_view_find_first_of_char(dstr_view str, char c);
static inline dstr_size_t dstr_view_find_last_of_chars(dstr_view str, dstr_view chars)
{
	dstr_size_t index = DSTR_NPOS;

	if (str.size <= 0)
	{
		return index;
	}

	const char* begin = str.data;
	const char* data = str.data;
	const char* end = str.data + str.size;

	while (data < end)
	{
		if (dstr_view_find_first_of_char(chars, (*data)) != DSTR_NPOS)
		{
			index = data - begin;
			break;
		}
		data++;
	}

	return index;
}

static inline dstr_view dstr_view_line_at(dstr_view sv, dstr_size_t pos)
{
	DSTR_ASSERT(pos >= 0 && pos < sv.size);

	dstr_size_t start_line_index = 0;
	dstr_size_t end_line_index = 0;

	if (pos != 0) // if pos already equal to 0 no need to search for a previous char
	{
		dstr_size_t index = dstr_view_find_last_of_char(sv, '\n', pos - 1);

		if (index == DSTR_NPOS) // if not found start is the first char of the string
		{
			start_line_index = 0;
		}
		else
		{
			start_line_index = index + 1; // get char after the line ending.
		}
	}
	

	dstr_size_t index = dstr_view_find_char(sv, pos, '\n');

	if (index == DSTR_NPOS) // if not found end is the last char of the string
	{
		end_line_index = sv.size - 1;
	}
	else
	{
		end_line_index = index -1; // get char before the line ending
	}

	if (start_line_index == end_line_index)
		return dstr_view_make();


	dstr_size_t new_size = (end_line_index - start_line_index) + 1;
	return dstr_view_substr(sv, dstr_view_begin(sv) + start_line_index, new_size);
}

// return first line "line" and remove the line from the source "sv"
static inline dstr_bool_t dstr_view_pop_line(dstr_view* sv, dstr_view* line)
{
	if (sv->size == 0)
	{
		*line = dstr_view_make();
		return (dstr_bool_t)(0);
	}
	else
	{
		dstr_size_t index = dstr_view_find_char(*sv, 0, '\n');
		if (index == DSTR_NPOS)
		{
			*line = dstr_view_make();
			dstr_view_swap(sv, line);
		}
		else
		{
			dstr_size_t line_size = index + 1;
			*line = dstr_view_make_from(sv->data, line_size);
			sv->data += line_size;
			sv->size -= line_size;
		}
		return (dstr_bool_t)(1);
	}
} // dstr_view_pop_line


// given the source at position 'pos' this function returns the number of lines before et the number of line after.
static inline void dstr_view_count_surrounding_lines(dstr_view sv, const char* pos, size_t* previous_line_count, size_t* next_line_count)
{
	size_t before_count = 0;
	size_t after_count = 0;
	*previous_line_count = 0;
	*next_line_count = 0;

	if (sv.size == 0)
		return;

	assert(pos >= dstr_view_begin(sv) && pos < dstr_view_end(sv));

	if (sv.size == 1)
		return;

	const char* cursor = dstr_view_begin(sv);
	const char* end = dstr_view_end(sv);

	// skip last line ending of the source its existence or not does not chance the line count
	// maybe we should do this for any
	if (dstr_view_ends_with_str(sv, "\n"))  
	{
		--end;
	}

	while (cursor < end)
	{
		if (*cursor == '\n')
		{
			cursor < pos ? ++before_count : ++after_count;
		}
		++cursor;
	}

	*previous_line_count = before_count;
	*next_line_count = after_count;
} // dstr_view_count_surrounding_lines

  // find the n previous chars
  // return bound_left - 1 if the count has not been found.
static inline const char* dstr__mem_rfind_n(const char* data, const char* bound_left, char ch, size_t count, size_t* result)
{
	size_t current_count = 0;
	const char* res_ptr = data;
	while (data >= bound_left && current_count != count)
	{
		if (*data == ch)
		{
			res_ptr = data;
			++current_count;
		}

		--data;
	}

	res_ptr = current_count == count ? res_ptr : data;

	*result = current_count;

	return res_ptr;

}

// find the n next chars
// returns end if the total has not been reached.
static inline const char* dstr__mem_find_n(const char* data, const char* end, char ch, size_t count, size_t* result)
{
	size_t current_count = 0;

	while (data < end && (current_count != count))
	{
		if (*data == ch)
		{
			++current_count;
		}

		++data;
	}

	*result = current_count;

	return data;
}

static inline dstr_view dstr_view_get_surrounding_lines(dstr_view source, const dstr_char_t* pos, size_t extra_lines_required, size_t* previous_line_count, size_t* next_line_count)
{
	assert(extra_lines_required >= 0);

	*previous_line_count = 0;
	*next_line_count = 0;

	if (source.size == 0)
		return source;

	assert(pos >= dstr_view_begin(source) && pos < dstr_view_end(source));

	const dstr_char_t* initial_cursor = pos;

	// handle special case where source is only one char
	if (source.size == 1)
	{
		return dstr_view_make_from(initial_cursor, 1);
	}

	const dstr_char_t* begin_line = initial_cursor;
	const dstr_char_t* end_line = initial_cursor;

	const dstr_char_t* begin_source = dstr_view_begin(source);
	const dstr_char_t* end_source = dstr_view_end(source);

	size_t required = extra_lines_required + 1;

	// find start of line
	// we want the next lines, but also the current line from the selected char.
	size_t rfound;
	const char* res = dstr__mem_rfind_n(initial_cursor - 1, begin_source, '\n', required, &rfound);

	if (res != (begin_source - 1))
	{
		begin_line = res + 1; // remove last \n
		rfound--;
	}
	else // beyond limit, set it at the source start.
	{
		begin_line = begin_source;
	}
	*previous_line_count = rfound;

	end_line = initial_cursor;

	required = extra_lines_required + 1; // we want the next lines, but also the current line from the selected char.
	size_t found = 0;
	end_line = dstr__mem_find_n(end_line, end_source, '\n', required, &found);

	if (*(end_line - 1) == '\n') // last char is line endings, we should not take it into account
	{
		found--;
	}

	*next_line_count = found;

	return dstr_view_substr(
		source,
		(const dstr_it)begin_line,
		end_line - begin_line
	);

}

/*
// quick and dirty
static inline void dstr_get_timestamp_YYYY_MM_DD_HH_MM_SS(dstr* str)
{
	static char buffer[] = "YYYY-MM-DD__HH-MM-SS\0";

	dstr_resize(str, sizeof(buffer));

	time_t now = time(NULL);

	strftime(buffer, sizeof(buffer), "%Y-%m-%d__%H-%M-%S", gmtime(&now));
	dstr_assign_str(str, buffer);
}
*/

static inline size_t dstr_view_tok(dstr_view sv, dstr_view delims, dstr_view* tok)
{
	DSTR_ASSERT(tok);

	//size_t result = false;
	size_t result_size = 0;

	if (!sv.size) {
		return result_size;
	}

	const char* start = sv.data;

	size_t i = 0;

	while (dstr_view_contains_char(delims, start[i]) && i < sv.size)
	{
		++start;
		++i;
	}

	if (i == sv.size) {
		return result_size;
	}

	tok->data = start;
	tok->size = 0;
	const char* end = sv.data + sv.size;
	while (start < end && !dstr_view_contains_char(delims, *start))
	{
		start++;
		tok->size += 1;

	}

	return tok->size;
}

static inline dstr_bool_t dstr_view_is_identifier(dstr_view sv)
{
	for (dstr_size_t i = 0; i < sv.size; ++i)
	{
		dstr_char_t c = sv.data[i];
		dstr_bool_t valid_char = (c >= 'a' && c <= 'z')
			|| (c >= 'A' && c <= 'Z')
			|| (c >= '0' && c <= '9')
			|| c == '_';

		if (!valid_char)
			return 0;
	}

	return 1;
}

static inline dstr_size_t dstr_view_find_first_of_char(dstr_view str, char c)
{
	dstr_size_t index = DSTR_NPOS;

	if (str.size <= 0)
	{
		return index;
	}

	const char* begin = str.data;
	const char* data = str.data;
	const char* end = str.data + str.size;

	while (data < end)
	{
		if (*data == c)
		{
			index = data - begin;
			break;
		}
		data++;
	}

	return index;
}

static inline dstr_bool_t dstr__char_is_alphanum(char c)
{
	return
		(c >= 'a' && c <= 'z')
		|| (c >= 'A' && c <= 'Z')
		|| (c >= '0' && c <= '1')
		;
}

static inline size_t dstr_view_find_first_of_chars(dstr_view str, dstr_view chars)
{
	size_t index = DSTR_NPOS;

	if (str.size <= 0)
	{
		return index;
	}

	const dstr_char_t* begin = str.data;
	const dstr_char_t* data = str.data;
	const dstr_char_t* end = str.data + str.size;

	while (data < end)
	{
		if (dstr_view_find_first_of_char(chars, (*data)) != DSTR_NPOS)
		{
			index = data - begin;
			break;
		}
		data++;
	}

	return index;
}

static inline void eat_email_suffix(const char** _cursor, const char* _end)
{
	const char* cursor = *_cursor;
	while (cursor < _end)
	{
		dstr_bool_t valid = !!(dstr__char_is_alphanum(*cursor)
			|| dstr_view_contains_char(dstr_view_make_from_str(".!#$%&’*+/=?^_`{|}~-"), *cursor))
			;

		if (!valid) {
			break;
		}

		++cursor;
	}

	*_cursor = cursor;
}


static inline dstr_view* dstr_view_trim(dstr_view* str, dstr_view chars)
{
	// String is empty
	if (str->size == 0) {
		return str;
	}

	// No char to trim
	if (chars.size == 0) {
		return str;
	}

	const char* end_data = str->data + str->size;

	// Skip chars at left.
	while (str->data < end_data) {
		if (!dstr_view_contains_char(chars, (*str->data))) {
			break;
		}
		str->data++;
		str->size--;
	}

	// Skip chars at right.
	while (str->size > 0) {
		if (!dstr_view_contains_char(chars, str->data[str->size - 1])) {
			break;
		}
		str->size--;
	}

	return str;
}


// return true if match regex [a-zA-Z0-9_]+
static dstr_bool_t dstr__parse_email_subdomain(const char** _cursor, const char* _end)
{
	const char* cursor = *_cursor;

	dstr_bool_t match = 0;
	while (cursor < _end)
	{
		dstr_bool_t valid = !!(dstr__char_is_alphanum(*cursor)
			|| dstr_view_contains_char(dstr_view_make_from_str("-"), *cursor))
			;

		if (!valid) {
			break;
		}
		match = 1;
		++cursor;
	}

	*_cursor = cursor;

	return match;
}

/* return true if match regex (.[a-zA-Z0-9_])+ */
static dstr_bool_t dstr__parse_email_ending(const char** _cursor, const char* _end)
{
	const char* cursor = *_cursor;

	dstr_bool_t match = 0;
	while (cursor < _end)
	{
		if (*cursor != '.') return 0; /* must contain '.' 
		++cursor;  /* skip '.' */

		dstr_bool_t found_char = 0;
		while (cursor < _end)
		{
			dstr_bool_t valid = !!(dstr__char_is_alphanum(*cursor)
				|| dstr_view_contains_char(dstr_view_make_from_str("-"), *cursor))
				;

			if (!valid) {
				break;
			}
			found_char = 1;
			match = 1;
			++cursor;
		}


		if (!found_char)
		{
			break;
		}
	}

	*_cursor = cursor;

	return match;
}

// Should match W3C email regex:
// /^[a-zA-Z0-9.!#$%&’*+/=?^_`{|}~-]+@[a-zA-Z0-9-]+(?:\.[a-zA-Z0-9-]+)*$/
static inline dstr_bool_t dstr_view_is_email(dstr_view _str)
{
	const dstr_char_t* cursor = _str.data;
	const dstr_char_t* end = _str.data + _str.size;

	/* advance to '@' if every character are valid */
	eat_email_suffix(&cursor, end);

	if (cursor >= end) return 0;

	if (*cursor != '@') return 0; /* must contain '@' */

	++cursor; /* skip '@' */

	dstr_view view = dstr_view_make_from(cursor, end - cursor);
	size_t first_dot = dstr_view_find_first_of_char(view, '.');

	if (first_dot == DSTR_NPOS) return 0; /* does not contains any dots */

	const dstr_char_t* first_dot_ptr = cursor + first_dot;

	if (!dstr__parse_email_subdomain(&cursor, first_dot_ptr)) return 0;

	if (cursor == end) return 0; /* must not reach the end already */

	if (!dstr__parse_email_ending(&cursor, end)) return 0;

	if (cursor < end) return 0; /* must reach the end */

	return 1;
}


static inline void dstr_replace(dstr* s, size_t index, size_t count, dstr_view replacing) {

	assert(index <= s->size);
	assert(count <= s->size);
	assert(index + count <= s->size);

	// Just an alias to make it shorter
	dstr_view r = replacing;

	if (r.size < count) { // mem replacing <  mem to replace

		dstr_char_t* first = s->data + index;
		dstr_char_t* last = (s->data + index + count);

		size_t count_to_move = s->size - (index + count);
		size_t count_removed = count - r.size;

		if (count_to_move) {
			memmove(last - count_removed, last, count_to_move * sizeof(dstr_char_t));
		}
		if (s->size) {
			memcpy(first, r.data, r.size * sizeof(dstr_char_t));
		}

		dstr_resize(s, s->size - count_removed);
		s->data[s->size] = '\0';

	}
	else if (r.size > count) { // mem replacing >  mem to replace

		size_t extra_count = r.size - count;
		size_t needed_capacity = s->size + extra_count + 1; // +1 for '\0'
		size_t count_to_move = s->size - index - count;

		//_DSTR_GROW_IF_NEEDED(s, needed_capacity);
		dstr_reserve(s, needed_capacity);
		// Need to set this after "grow" because of potential allocation
		char* first = s->data + index;
		char* last = s->data + index + count;

		if (count_to_move) {
			memmove(last + extra_count, last, count_to_move * sizeof(dstr_char_t));
		}

		memcpy(first, r.data, r.size * sizeof(dstr_char_t));

		//s->size += extra_count;
		//s->data[s->size] = '\0';
		dstr_resize(s, s->size + extra_count);
		s->data[s->size] = '\0';

	}
	else
	{ // mem replacing == mem to replace
		char* first = s->data + index;
		memcpy(first, r.data, r.size * sizeof(dstr_char_t));
	}
}

/*
-------------------------------------------------------------------------
dstr_spliter
-------------------------------------------------------------------------
*/

typedef struct dstr_spliter_t
{
	dstr_view str;
	dstr_view delims;
} dstr_spliter;

static inline void dstr_spliter_init(dstr_spliter* s, dstr_view sv, dstr_view delims)
{
	s->str = sv;
	s->delims = delims;
}

static inline void dstr_spliter_init_str(dstr_spliter* s, dstr_view sv, const char* delims)
{
	s->str = sv;
	s->delims = dstr_view_make_from_str(delims);
}

static inline dstr_spliter dstr_spliter_make(dstr_view sv, dstr_view delims)
{
	dstr_spliter s;
	dstr_spliter_init(&s, sv, delims);
	return s;
}

static inline dstr_spliter dstr_spliter_make_str(dstr_view sv, const char* delims)
{
	dstr_view delims_v = dstr_view_make_from_str(delims);
	return dstr_spliter_make(sv, delims_v);
}

static inline dstr_bool_t dstr_spliter_get_next(dstr_spliter* s, dstr_view* res) {

	if (dstr_view_tok(s->str, s->delims, res))
	{
		s->str.size -= (res->data - s->str.data) + res->size;
		s->str.data = res->data + res->size;
		return 1;
	}
	return 0;
}

#ifdef __cplusplus
} /* extern "C" */
#endif


#endif /* RE_DSTR_UTIL_H */
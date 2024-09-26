#ifndef CB_ASSERT_H
#define CB_ASSERT_H

#ifdef __cplusplus
extern "C" {
#endif

#define cb_assert_int_equals(expected, expression) \
	do { \
		int result = (expression); \
		if (result != expected) { \
			cb_log_error("Result '%d' is not equal to the expected value '%d' : %s %d", result, expected, __FILE__, __LINE__); \
			exit(1); \
		} \
	} while (0);

#define cb_assert_int_not_equals(expected, expression) \
	do { \
		int result = (expression); \
		if (result == expected) { \
			cb_log_error("Result '%d' is equal to the expected value '%d' : %s %d", result, expected, __FILE__, __LINE__); \
			exit(1); \
		} \
	} while (0);

#define cb_assert_run(command) \
	cb_assert_int_equals(0, cb_run(command))

#define cb_assert_true(expression) \
	do { \
		int result = (expression); \
		if (result == 0) { \
			cb_log_error("Expression is not true : %s %d", __FILE__, __LINE__); \
			exit(1); \
		} \
	} while (0);

#define cb_assert_false(expression) \
	do { \
		int result = (expression); \
		if (result != 0) { \
			cb_log_error("Expression is not false : %s %d", __FILE__, __LINE__); \
			exit(1); \
		} \
	} while (0);

CB_API void cb_assert_file_exists(const char* filepath);
CB_API void cb_assert_file_exists_f(const char* format, ...);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CB_ASSERT_H */

#ifdef CB_IMPLEMENTATION

CB_API void
cb_assert_file_exists(const char* filepath)
{
	if (!cb_path_exists(filepath))
	{
		cb_log_error("File does not exist: %s", filepath);
		exit(1);
	}
}

CB_API void
cb_assert_file_exists_f(const char* format, ...)
{
	va_list args;
	const char* str = NULL;
	va_start(args, format);
	
	str = cb_tmp_vsprintf(format, args);
	cb_assert_file_exists(str);

	va_end(args);
}

#endif /* CB_IMPLEMENTATION */
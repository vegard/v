#ifndef V_FORMAT_HH
#define V_FORMAT_HH

#include <cstdarg>
#include <cstdio>
#include <stdexcept>

// This is a wrapper around sprintf() that returns std::string
static std::string format(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	char *str;
	if (vasprintf(&str, format, ap) == -1)
		throw std::runtime_error("vasprintf() failed");
	va_end(ap);

	std::string ret(str);
	free(str);
	return ret;
}

#endif

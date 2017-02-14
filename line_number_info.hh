#ifndef V_LINE_NUMBER_INFO_HH
#define V_LINE_NUMBER_INFO_HH

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <algorithm>
#include <sstream>

struct line_number_info {
	std::map<unsigned int, unsigned int> byte_offset_to_line_number_map;

	line_number_info(const char *buf, size_t len)
	{
		unsigned int current_line = 1;

		for (unsigned int i = 0; i < len; ++i) {
			byte_offset_to_line_number_map.insert(std::make_pair(i, current_line++));

			while (i < len && buf[i] != '\n')
				++i;
		}

		byte_offset_to_line_number_map.insert(std::make_pair(len, current_line));
	}

	struct lookup_result {
		unsigned int line_start;
		unsigned int line_length;

		unsigned int line;
		unsigned int column;
	};

	struct lookup_result lookup(unsigned int byte_offset)
	{
		auto it = byte_offset_to_line_number_map.upper_bound(byte_offset);
		if (it == byte_offset_to_line_number_map.begin())
			return lookup_result{0, 0, 0, 0};

		auto it2 = it;
		--it;

		assert(byte_offset >= it->first);
		return lookup_result{
			it->first,
			it2->first - it->first,
			it->second,
			byte_offset - it->first,
		};
	}
};

#if 0
int main(int argc, char *argv[])
{
	const char *doc = "hello\n\n\nworld!\n";
	line_number_info line_numbers("<buf>", doc, strlen(doc));
	for (unsigned int i = 0; i < strlen(doc); ++i)
		printf("%s: %s\n", line_numbers.tag(i).c_str(), &doc[i]);

	return 0;
}
#endif

#endif

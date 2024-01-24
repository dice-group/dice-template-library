#include <dice/template-library/defer.hpp>

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <random>
#include <string>

/**
 * Some C Api File handling
 */
void write_to_file(std::filesystem::path const &p) {
	FILE *f = fopen(p.c_str(), "w");
	if (f == nullptr) {
		return;
	}

	DEFER {
		fclose(f);
	};

	std::string const s = "Spherical Cow";
	fwrite(s.data(), 1, s.size(), f);
}

/**
 * Copies a file from src to dst, will not overwrite dst
 * if copying does not succeed
 *
 * @note Inspired by Andrei Alexandrescu's “Declarative Control Flow" presentation
 */
void copy_file_transact(std::filesystem::path const &src, std::filesystem::path const &dst) {
	std::filesystem::path const dst2 = dst.string() + ".deleteme";
	DEFER_TO_FAIL {
		std::error_code ec;
		std::filesystem::remove(dst2, ec);
	};

	std::filesystem::copy_file(src, dst2);
	std::filesystem::rename(dst2, dst);
}

/**
 * Converts a string to and int with a postcondition check
 *
 * @note Inspired by Andrei Alexandrescu's “Declarative Control Flow" presentation
 */
int string_to_int(std::string const &integer) {
	int value = std::stoi(integer);

	DEFER_TO_SUCCESS {
		// check postcondition
		assert(std::to_string(value) == integer);
	};

	return value;
}

int main() {
	std::filesystem::path const p = "/tmp/dice-template-lib-defer-example1-" + std::to_string(std::random_device{}());
	std::filesystem::path const p2 = "/tmp/dice-template-lib-defer-example2-" + std::to_string(std::random_device{}());

	DEFER {
		std::error_code ec;
		std::filesystem::remove(p, ec);
		std::filesystem::remove(p2, ec);
	};

	write_to_file(p);
	copy_file_transact(p, p2);

	std::string const i = "42";
	int const j = 10 + string_to_int(i);
	assert(j == 52);
}

/**
 * dice::template_library::generator is **DEPRECATED**.
 * It will be removed in next major release.
 *
 * Use std::generator instead.
 */

#include <iostream>
#include <string>

// disable deprecation warnings because we run CI with -Werror
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <dice/template-library/generator.hpp>

namespace dtl = dice::template_library;

template<typename T>
struct Tree {
	T value;
	Tree *left = nullptr;
	Tree *right = nullptr;

	[[nodiscard]] dtl::generator<T const &> traverse_inorder() const {
		if (left != nullptr) {
			co_yield dtl::ranges::elements_of(left->traverse_inorder());
		}

		co_yield value;

		if (right != nullptr) {
			co_yield dtl::ranges::elements_of(right->traverse_inorder());
		}
	}
};

int main() {
	//    D
	//  B    F
	// A C  E G
	Tree<char> leaf1{'A'};
	Tree<char> leaf2{'C'};
	Tree<char> leaf3{'E'};
	Tree<char> leaf4{'G'};
	Tree<char> branch1{'B', &leaf1, &leaf2};
	Tree<char> branch2{'F', &leaf3, &leaf4};
	Tree<char> root{'D', &branch1, &branch2};

	std::string output;
	for (char const x : root.traverse_inorder()) {
		output.push_back(x);
	}

	assert(output == "ABCDEFG");
	std::cout << output << '\n';
}

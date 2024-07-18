#include <iostream>
#include <string>

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
	Tree leaf1{'A'};
	Tree leaf2{'C'};
	Tree leaf3{'E'};
	Tree leaf4{'G'};
	Tree branch1{'B', &leaf1, &leaf2};
	Tree branch2{'F', &leaf3, &leaf4};
	Tree root{'D', &branch1, &branch2};

	std::string output;
	for (char const x : root.traverse_inorder()) {
		output.push_back(x);
	}

	assert(output == "ABCDEFG");
	std::cout << output << '\n';
}

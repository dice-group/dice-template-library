#include <dice/template-library/dbg.hpp>

#include <cassert>

int main() {
	int const a = 2;
	int const b = DICE_DBG(a * 2) + 1;
	//            ^ prints: [example_dbg.cpp:7] a * 2 = 4
	assert(b == 5);
}

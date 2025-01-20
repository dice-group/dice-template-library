#include <dice/template-library/pool_allocator.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

struct list {
	uint64_t elem;
	list *next;
};

int main() {
	dice::template_library::pool_allocator<std::byte, sizeof(list)> alloc;

	{ // efficient pool allocations for elements of known size
		dice::template_library::pool_allocator<list, sizeof(list)> list_alloc = alloc;

		auto *head = list_alloc.allocate(1); // efficient pool allocation
		new (head) list{.elem = 0, .next = nullptr};

		head->next = list_alloc.allocate(1); // efficient pool allocation
		new (head->next) list{.elem = 1, .next = nullptr};

		auto const *cur = head;
		while (cur != nullptr) {
			std::cout << cur->elem << " ";
			cur = cur->next;
		}

		list_alloc.deallocate(head->next, 1);
		list_alloc.deallocate(head, 1);
	}

	{ // fallback allocation with new & support as container allocator
		std::vector<uint64_t, dice::template_library::pool_allocator<uint64_t, sizeof(list)>> vec(alloc);
		vec.resize(1024);
	}
}

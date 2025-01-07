#include <dice/template-library/pool_allocator.hpp>

#include <cstdint>
#include <iostream>
#include <vector>

struct list {
	uint64_t elem;
	list *next;
};

int main() {
	dice::template_library::pool<sizeof(list)> pool;

	{ // efficient pool allocations for elements of known size
		auto list_alloc = pool.get_allocator<list>();

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
		std::vector<uint64_t, dice::template_library::pool_allocator<uint64_t, sizeof(list)>> vec(pool.get_allocator());
		vec.resize(1024);
	}
}

#include <dice/template-library/polymorphic_allocator.hpp>

#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>

#include <array>
#include <iostream>
#include <vector>

template<typename T>
using shm_allocator_type = boost::interprocess::allocator<T, boost::interprocess::managed_shared_memory::segment_manager>;

template<typename T>
using stl_allocator_type = dice::template_library::offset_ptr_stl_allocator<T>;

template<typename T>
using allocator_type = dice::template_library::polymorphic_allocator<T, stl_allocator_type, shm_allocator_type>;

using vector_type = boost::interprocess::vector<int, allocator_type<int>>;

inline constexpr auto shm_name = "dice_template_library_polymorhic_allocator_example_shm";
inline constexpr auto shm_vec_name = "example_vec";


vector_type vector_sum(std::vector<vector_type const *> const &operands) {
	vector_type sum;

	for (auto const &opr : operands) {
		if (opr->size() > sum.size()) {
			sum.resize(opr->size());
		}

		for (size_t ix = 0; ix < opr->size(); ++ix) {
			sum[ix] += (*opr)[ix];
		}
	}

	return sum;
}

int main() {
	std::vector<vector_type const *> operands;

	{ // populate shared memory
		boost::interprocess::managed_shared_memory shm{boost::interprocess::create_only, shm_name, 4096};

		// operand 1: allocated fully in shared memory
		auto *shm_vec = shm.construct<vector_type>(shm_vec_name)(allocator_type<int>{std::in_place_type<shm_allocator_type<int>>, shm.get_segment_manager()});
		shm_vec->push_back(1);
		shm_vec->push_back(5);
		operands.push_back(shm_vec);
	}

	boost::interprocess::managed_shared_memory shm{boost::interprocess::open_only, shm_name};

	// operand 1: reloaded from shared memory
	// this would segfault with std::pmr::polymorphic_allocator
	auto [shm_vec, _] = shm.find<vector_type>(shm_vec_name);

	// operand 2: allocated fully on heap
	auto *normal_vec = new vector_type{allocator_type<int>{}};
	normal_vec->push_back(2);
	normal_vec->push_back(10);
	normal_vec->push_back(20);
	operands.push_back(normal_vec);

	auto const sum = vector_sum(operands);
	assert(sum.size() == 3);
	assert(sum[0] == 3);
	assert(sum[1] == 15);
	assert(sum[2] == 20);

	std::cout << "sum: ";
	for (auto const val : sum) {
		std::cout << val << " ";
	}
	std::cout << '\n';


	shm.destroy_ptr(shm_vec);
	delete normal_vec;

	boost::interprocess::shared_memory_object::remove(shm_name);
}

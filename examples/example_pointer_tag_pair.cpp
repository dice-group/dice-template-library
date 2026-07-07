#include <cassert>
#include <tuple>

import dice.template_library;

namespace dtl = dice::template_library;

// alignof(node) == 8, so the lowest 3 bits of any node* are zero
struct alignas(8) node {
    int value;
};

enum class letters : unsigned {
    A = 0,
    B = 1,
    C = 2,
};

int main() {
    node n{42};

    // By default, the number of tag bits is deduced from alignof(T).
    // For `node` that gives 3 bits (values 0..7).
    using tagged_node = dtl::pointer_tag_pair<node, unsigned>;
    static_assert(tagged_node::bits_requested == 3);

    tagged_node p{&n, 5};
    assert(p.pointer() == &n);
    assert(p.pointer()->value == 42);
    assert(p.tag() == 5);

    auto const [ptr, tag] = p;
    assert(ptr == &n);
    assert(tag == 5);

    // Enums with an unsigned underlying type are valid tags too.
    dtl::pointer_tag_pair<node, letters> q{&n, letters::C};
    assert(q.tag() == letters::C);
}

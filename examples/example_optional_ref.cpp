#include <dice/template-library/optional_ref.hpp>

#include <iostream>
#include <map>
#include <optional>
#include <string>

namespace dtl = dice::template_library;

// Returns a reference to the score of `name`, or nothing if there is no such entry.
dtl::optional_ref<int> find_score(std::map<std::string, int> &scores, std::string const &name) {
    auto it = scores.find(name);
    if (it == scores.end()) {
        return std::nullopt;
    }
    return it->second;
}

int main() {
    std::map<std::string, int> scores{{"alice", 3}, {"bob", 5}};

    // Change the value through the reference
    if (auto score = find_score(scores, "alice")) {
        *score += 10;
    }
    std::cout << "alice: " << scores.at("alice") << "\n";  // 13

    // value_or returns a copy of the referenced value, or the default if there is none
    std::cout << "carol: " << find_score(scores, "carol").value_or(0) << "\n";  // 0

    // transform applies a function to the referenced value, if there is one
    std::optional<int> const doubled = find_score(scores, "bob").transform([](int score) {
        return 2 * score;
    });
    std::cout << "bob doubled: " << *doubled << "\n";  // 10

    // or_else gives another reference if there is none
    int default_score = -1;
    auto const score = find_score(scores, "dave").or_else([&default_score] {
        return dtl::optional_ref<int>{default_score};
    });
    std::cout << "dave: " << *score << "\n";  // -1

    // optional_ref<int> converts to optional_ref<int const>
    dtl::optional_ref<int const> const read_only = find_score(scores, "bob");
    std::cout << "bob: " << *read_only << "\n";  // 5

    // optional_ref is a range of zero or one elements
    auto const bob_score = find_score(scores, "bob");
    for (int &value : bob_score) {
        value = 0;
    }
    std::cout << "bob set to 0: " << scores.at("bob") << "\n";  // 0

    // Like std::optional<T &>, optional_ref never binds a temporary. This does not compile:
    // dtl::optional_ref<int const> dangling{42};

    // value throws std::bad_optional_access if there is no value
    try {
        std::cout << find_score(scores, "erin").value() << "\n";
    } catch (std::bad_optional_access const &e) {
        std::cout << "erin: " << e.what() << "\n";
    }
}

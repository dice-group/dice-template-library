#include <dice/template-library/opt_minmax.hpp>

#include <functional>
#include <iostream>
#include <optional>
#include <vector>

int main()
{
    using namespace dice::template_library;

    // Basic opt_min / opt_max with plain values
    auto min_val = opt_min<int>({5, 3, 8, 1, 4});
    auto max_val = opt_max<int>({5, 3, 8, 1, 4});
    std::cout << "opt_min(5,3,8,1,4) = " << *min_val << "\n";
    std::cout << "opt_max(5,3,8,1,4) = " << *max_val << "\n";

    // Mixing plain values and optionals
    std::optional<int> maybe_small{2};
    std::optional<int> missing;
    auto mixed_min = opt_min<int>({10, maybe_small, missing, 7});
    std::cout << "opt_min(10, optional{2}, nullopt, 7) = " << *mixed_min << "\n";

    // opt_minmax variadic
    auto [lo, hi] = *opt_minmax<int>({5, 3, 8, 1, 4});
    std::cout << "opt_minmax(5,3,8,1,4) = {" << lo << ", " << hi << "}\n";

    // Range-based variants
    std::vector<int> v{10, 20, 5, 15};
    auto rmin = opt_min_element(v);
    auto rmax = opt_max_element(v);
    auto [rlo, rhi] = *opt_minmax_element(v);
    std::cout << "opt_min_element({10,20,5,15}) = " << *rmin << "\n";
    std::cout << "opt_max_element({10,20,5,15}) = " << *rmax << "\n";
    std::cout << "opt_minmax_element({10,20,5,15}) = {" << rlo << ", " << rhi << "}\n";

    // Range of optionals
    std::vector<std::optional<int>> opts{std::nullopt, std::optional{42}, std::optional{7}, std::nullopt};
    auto opt_rmin = opt_min_element(opts);
    std::cout << "opt_min_range({nullopt, 42, 7, nullopt}) = " << *opt_rmin << "\n";

    // Explicit result type: deduced by default, or specify with a template argument
    auto deduced = opt_min<int>({5, 3, 8});                // deduced as std::optional<int>
    auto explicit_type = opt_min<double>({5, 3, 8});  // forced to std::optional<double>
    std::cout << "opt_min(5,3,8) [deduced]  = " << *deduced << "\n";
    std::cout << "opt_min<double>(5,3,8)    = " << *explicit_type << "\n";

    // Custom comparator: pass as the first argument (replaces operator<)
    auto cmin = opt_min<int>({5, 3, 8}, std::greater{});  // "min" under greater = largest value
    auto cmax = opt_max<int>({5, 3, 8}, std::greater{});  // "max" under greater = smallest value
    std::cout << "opt_min(greater, 5,3,8)   = " << *cmin << "\n";
    std::cout << "opt_max(greater, 5,3,8)   = " << *cmax << "\n";

    // Custom comparator with ranges
    auto crmin = opt_min_element(v, std::greater{});
    std::cout << "opt_min_range({10,20,5,15}, greater) = " << *crmin << "\n";

    // Custom comparator with opt_minmax
    auto [clo, chi] = *opt_minmax<int>({5, 3, 8, 1, 4}, std::greater{});
    std::cout << "opt_minmax(greater, 5,3,8,1,4) = {" << clo << ", " << chi << "}\n";
}

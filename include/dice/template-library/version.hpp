#ifndef DICE_TEMPLATELIBRARY_VERSION_HPP
#define DICE_TEMPLATELIBRARY_VERSION_HPP

#include <array>

namespace dice::template_library {
    inline constexpr char name[] = "dice-template-library";
    inline constexpr char version[] = "1.5.1";
    inline constexpr std::array<int, 3> version_tuple = {1, 5, 1};
    inline constexpr int pobr_version = 1; ///< persisted object binary representation version
} // namespace dice::template_library

#endif // DICE_TEMPLATELIBRARY_VERSION_HPP

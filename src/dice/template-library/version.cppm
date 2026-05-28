module;
#include <array>
export module dice.template_library:version;

export namespace dice::template_library {
    inline constexpr char name[] = "dice-template-library";
    inline constexpr char version[] = "2.6.0";
    inline constexpr std::array<int, 3> version_tuple = {2, 6, 0};
    inline constexpr int pobr_version = 2; ///< persisted object binary representation version
} // namespace dice::template_library

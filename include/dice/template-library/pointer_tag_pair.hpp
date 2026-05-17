#ifndef DICE_TEMPLATE_LIBRARY_POINTERTAGPAIR_HPP
#define DICE_TEMPLATE_LIBRARY_POINTERTAGPAIR_HPP

#include <bit>
#include <cassert>
#include <tuple>
#include <utility>

#include <dice/template-library/type_traits.hpp>

namespace dice::template_library {
namespace detail_pointer_tag_pair {

template <typename T>
concept taggable_pointee = !std::is_function_v<T>;

template <typename Pointer, typename T>
concept taggable_pointer =
    std::is_pointer_v<Pointer> && std::convertible_to<Pointer, T *>;

template <typename Tag>
concept taggable_pointer_tag =
    std::unsigned_integral<Tag> ||
    std::is_enum_v<Tag> && std::unsigned_integral<std::underlying_type_t<Tag>>;

template <typename T, unsigned Alignment = 0>
consteval unsigned taggable_bits_available() {
  if constexpr (Alignment != 0) {
    return std::countr_zero(Alignment);
  } else {
    if constexpr (std::is_void_v<T>) {
      return 0;
    } else {
      return std::countr_zero(alignof(T));
    }
  }
}

} // namespace detail_pointer_tag_pair

template <detail_pointer_tag_pair::taggable_pointee T,
          detail_pointer_tag_pair::taggable_pointer_tag Tag,
          unsigned BitsRequested =
              detail_pointer_tag_pair::taggable_bits_available<T>()>
class pointer_tag_pair {
  static constexpr uintptr_t ptr_mask = static_cast<uintptr_t>(-1)
                                        << BitsRequested;

  static constexpr uintptr_t tag_mask =
      (static_cast<uintptr_t>(1) << BitsRequested) - 1;

  static constexpr size_t alignment_required = static_cast<size_t>(1)
                                               << BitsRequested;

  uintptr_t value_;

public:
  using pointer_type = T *;
  using tagged_pointer_type = copy_cv_t<T, void> *;
  using tag_type = Tag;

  static constexpr unsigned bits_requested = BitsRequested;

  constexpr pointer_tag_pair() : value_(0) {}

  /*constexpr*/ pointer_tag_pair(decltype(nullptr), Tag const tag)
      : pointer_tag_pair(static_cast<uintptr_t>(tag)) {
    assert(static_cast<uintptr_t>(tag) <= tag_mask);
  }

  template <detail_pointer_tag_pair::taggable_pointer<T> P>
    requires(BitsRequested <= detail_pointer_tag_pair::taggable_bits_available<
                                  std::remove_pointer_t<P>>())
  /*constexpr*/ pointer_tag_pair(P const pointer, Tag const tag)
      : pointer_tag_pair(reinterpret_cast<uintptr_t>(pointer) |
                         static_cast<uintptr_t>(tag)) {
    assert((reinterpret_cast<uintptr_t>(pointer) & tag_mask) == 0);
    assert(static_cast<uintptr_t>(tag) <= tag_mask);
  }

  template <unsigned PromisedAlignment,
            detail_pointer_tag_pair::taggable_pointer<T> P>
    requires(BitsRequested <=
             detail_pointer_tag_pair::taggable_bits_available<
                 std::remove_pointer_t<P>, PromisedAlignment>())
  static /*constexpr*/ pointer_tag_pair from_overaligned(P const pointer,
                                                         Tag const tag) {
    assert((reinterpret_cast<uintptr_t>(pointer) & tag_mask) == 0);
    assert(static_cast<uintptr_t>(tag) <= tag_mask);

    return pointer_tag_pair(reinterpret_cast<uintptr_t>(pointer) |
                            static_cast<uintptr_t>(tag));
  }

  [[nodiscard]] static pointer_tag_pair
  from_tagged(tagged_pointer_type const pointer) noexcept {
    return pointer_tag_pair(reinterpret_cast<uintptr_t>(pointer));
  }

  [[nodiscard]] tagged_pointer_type tagged_pointer() const noexcept {
    return reinterpret_cast<tagged_pointer_type>(value_);
  }

  [[nodiscard]] /*constexpr*/ T *pointer() const noexcept {
    return reinterpret_cast<T *>(value_ & ptr_mask);
  }

  [[nodiscard]] constexpr Tag tag() const noexcept {
    return static_cast<Tag>(value_ & tag_mask);
  }

  friend constexpr void swap(pointer_tag_pair &lhs,
                             pointer_tag_pair &rhs) noexcept {
    auto const lhs_value = lhs.value_;
    lhs.value_ = rhs.value_;
    rhs.value_ = lhs_value;
  }

  friend constexpr auto operator<=>(pointer_tag_pair const &,
                                    pointer_tag_pair const &) = default;

private:
  explicit pointer_tag_pair(uintptr_t const value) : value_(value) {}
};

template <size_t I, typename T, typename Tag, unsigned BitsRequested>
constexpr std::tuple_element_t<I, pointer_tag_pair<T, Tag, BitsRequested>>
get(pointer_tag_pair<T, Tag, BitsRequested> const pointer) {
  if constexpr (I == 0) {
    return pointer.pointer();
  } else {
    return pointer.tag();
  }
}

} // namespace dice::template_library

template <typename T, typename Tag, unsigned BitsRequested>
struct std::tuple_size<
    dice::template_library::pointer_tag_pair<T, Tag, BitsRequested>> {
  static constexpr size_t value = 2;
};

template <typename T, typename Tag, unsigned BitsRequested>
struct std::tuple_size<
    dice::template_library::pointer_tag_pair<T, Tag, BitsRequested> const> {
  static constexpr size_t value = 2;
};

template <typename T, typename Tag, unsigned BitsRequested>
struct std::tuple_element<
    0, dice::template_library::pointer_tag_pair<T, Tag, BitsRequested>> {
  using type = T *;
};

template <typename T, typename Tag, unsigned BitsRequested>
struct std::tuple_element<
    1, dice::template_library::pointer_tag_pair<T, Tag, BitsRequested>> {
  using type = Tag;
};

template <typename T, typename Tag, unsigned BitsRequested>
struct std::tuple_element<
    0, dice::template_library::pointer_tag_pair<T, Tag, BitsRequested> const> {
  using type = T *;
};

template <typename T, typename Tag, unsigned BitsRequested>
struct std::tuple_element<
    1, dice::template_library::pointer_tag_pair<T, Tag, BitsRequested> const> {
  using type = Tag;
};

#endif // DICE_TEMPLATE_LIBRARY_POINTERTAGPAIR_HPP

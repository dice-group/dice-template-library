#ifndef DICE_TEMPLATELIBRARY_STATICVECTOR_HPP
#define DICE_TEMPLATELIBRARY_STATICVECTOR_HPP

#include <algorithm>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <utility>

namespace dice::template_library {

    /// Marker to indicate that elements can be left default initialized (like in std::make_unique_for_overwrite).
    struct for_overwrite_t {};
    inline constexpr for_overwrite_t for_overwrite{};

    /**
     * A constant-size (i.e. non-growing), heap-allocated vector type
     * Behaves like std::vector except that it cannot grow and therefore occupies 1 less word in memory (it has no capacity field).
     * Useful e.g. for I/O buffers where you would typically use the less ergonomic `std::unique_ptr<T[]>`.
     *
     * @note the elements are constructed and destroyed directly, A::construct and A::destroy are not used
     */
    template<typename T, typename A = std::allocator<T>>
    struct static_vector {
        using allocator_type = A;
        using value_type = T;
        using size_type = size_t;
        using difference_type = std::ptrdiff_t;
        using reference = value_type &;
        using const_reference = value_type const &;
        using pointer = value_type *;
        using const_pointer = value_type const *;
        using iterator = pointer;
        using const_iterator = const_pointer;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    private:
        using alloc_traits = std::allocator_traits<A>;

        alloc_traits::pointer data_ = nullptr;
        size_t size_ = 0;
        [[no_unique_address]] allocator_type alloc_;

        /// Allocates the storage, construct must then make size elements in it
        constexpr static_vector(size_type const size, allocator_type const &alloc, std::invocable<pointer> auto &&construct) :
            size_{size},
            alloc_{alloc} {
            if (size_ == 0) {
                return;
            }

            data_ = alloc_traits::allocate(alloc_, size_);
            try {
                construct(data());
            } catch (...) {
                alloc_traits::deallocate(alloc_, data_, size_);
                throw;
            }
        }

    public:
        constexpr static_vector(allocator_type const &alloc = allocator_type{}) noexcept : alloc_{alloc} {}

        /// Makes a vector of size value-initialized elements
        explicit constexpr static_vector(size_type const size, allocator_type const &alloc = allocator_type{}) :
            static_vector(size, alloc, [&](pointer const p) { std::uninitialized_value_construct_n(p, size); }) {}

        /// Makes a vector of size default-initialized elements
        constexpr static_vector(for_overwrite_t, size_type const size, allocator_type const &alloc = allocator_type{}) :
            static_vector(size, alloc, [&](pointer const p) { std::uninitialized_default_construct_n(p, size); }) {}

        constexpr static_vector(size_type const size, const_reference value, allocator_type const &alloc = allocator_type{}) :
            static_vector(size, alloc, [&](pointer const p) { std::uninitialized_fill_n(p, size, value); }) {}

        template<std::forward_iterator I, std::sentinel_for<I> S>
        constexpr static_vector(I first, S last, allocator_type const &alloc = allocator_type{}) :
            static_vector(static_cast<size_type>(std::ranges::distance(first, last)), alloc, [&](pointer const p) {
                std::ranges::uninitialized_copy(first, last, p, p + size_);
            }) {}

        constexpr static_vector(std::initializer_list<T> const init, allocator_type const &alloc = allocator_type{}) :
            static_vector(init.begin(), init.end(), alloc) {}

        constexpr static_vector(static_vector const &other) :
            static_vector(other.begin(), other.end(), alloc_traits::select_on_container_copy_construction(other.alloc_)) {}

        constexpr static_vector(static_vector &&other) noexcept :
            data_{std::exchange(other.data_, nullptr)},
            size_{std::exchange(other.size_, 0)},
            alloc_{std::move(other.alloc_)} {}

        constexpr ~static_vector() {
            if (data_ != nullptr) {
                std::destroy_n(data(), size_);
                alloc_traits::deallocate(alloc_, data_, size_);
            }
        }

        constexpr static_vector &operator=(static_vector const &other) {
            static_vector tmp(
                other.begin(),
                other.end(),
                alloc_traits::propagate_on_container_copy_assignment::value ? other.alloc_ : alloc_
            );
            swap(*this, tmp);
            return *this;
        }

        constexpr static_vector &operator=(static_vector &&other) noexcept(
            alloc_traits::propagate_on_container_move_assignment::value || alloc_traits::is_always_equal::value
        ) {
            if constexpr (!alloc_traits::propagate_on_container_move_assignment::value && !alloc_traits::is_always_equal::value) {
                if (alloc_ != other.alloc_) {
                    // not allowed to take the storage of other, need to move the elements
                    static_vector tmp(other.size_, alloc_, [&](pointer const p) {
                        std::uninitialized_move_n(other.data(), other.size_, p);
                    });
                    swap(*this, tmp);
                    return *this;
                }
            }

            static_vector tmp(std::move(other));
            swap(*this, tmp);
            return *this;
        }

        [[nodiscard]] constexpr pointer data() noexcept {
            return std::to_address(data_);
        }
        [[nodiscard]] constexpr const_pointer data() const noexcept {
            return std::to_address(data_);
        }

        [[nodiscard]] constexpr bool empty() const noexcept {
            return size_ == 0;
        }

        [[nodiscard]] constexpr size_type size() const noexcept {
            return size_;
        }

        [[nodiscard]] constexpr auto &operator[](this auto &&self, size_type const ix) noexcept {
            assert(ix < self.size());
            return self.data()[ix];
        }

        [[nodiscard]] constexpr auto &front(this auto &&self) noexcept {
            return self[0];
        }

        [[nodiscard]] constexpr auto &back(this auto &&self) noexcept {
            return self[self.size() - 1];
        }

        [[nodiscard]] constexpr auto begin(this auto &&self) noexcept {
            return self.data();
        }
        [[nodiscard]] constexpr auto end(this auto &&self) noexcept {
            return self.data() + self.size();
        }

        [[nodiscard]] constexpr const_iterator cbegin() const noexcept {
            return begin();
        }
        [[nodiscard]] constexpr const_iterator cend() const noexcept {
            return end();
        }

        [[nodiscard]] constexpr auto rbegin(this auto &&self) noexcept {
            return std::reverse_iterator{self.end()};
        }
        [[nodiscard]] constexpr auto rend(this auto &&self) noexcept {
            return std::reverse_iterator{self.begin()};
        }

        [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept {
            return rbegin();
        }
        [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept {
            return rend();
        }

        friend constexpr void swap(static_vector &a, static_vector &b) noexcept {
            using std::swap;
            swap(a.data_, b.data_);
            swap(a.size_, b.size_);
            swap(a.alloc_, b.alloc_);
        }

        constexpr bool operator==(static_vector const &other) const
        requires std::equality_comparable<T> {
            return std::ranges::equal(*this, other);
        }

        constexpr auto operator<=>(static_vector const &other) const
        requires std::three_way_comparable<T> {
            return std::lexicographical_compare_three_way(begin(), end(), other.begin(), other.end());
        }
    };

    template<std::forward_iterator I, std::sentinel_for<I> S, typename A = std::allocator<std::iter_value_t<I>>>
    static_vector(I, S, A = A{}) -> static_vector<std::iter_value_t<I>, A>;

} // namespace dice::template_library

#endif // DICE_TEMPLATELIBRARY_STATICVECTOR_HPP

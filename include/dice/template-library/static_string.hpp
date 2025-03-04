#ifndef DICE_TEMPLATELIBRARY_CONSTSTRING_HPP
#define DICE_TEMPLATELIBRARY_CONSTSTRING_HPP

#include <cassert>
#include <cstring>
#include <memory>
#include <ostream>
#include <string_view>
#include <utility>

namespace dice::template_library {

    /**
     * A constant-size (i.e. non-growing), heap-allocated string type
     * Behaves like std::string except that it cannot grow and therefore occupies 1 less word in memory (it has no capacity field).
     */
    template<typename Char, typename Traits = std::char_traits<Char>, typename Allocator = std::allocator<Char>>
    struct basic_static_string {
        using value_type = Char;
        using traits_type = Traits;
        using allocator_type = Allocator;
        using size_type = size_t;
        using different_type = std::ptrdiff_t;
        using pointer = typename std::allocator_traits<allocator_type>::pointer;
        using const_pointer = typename std::allocator_traits<allocator_type>::const_pointer;
        using view_type = std::basic_string_view<Char, Traits>;
        using iterator = value_type *;
        using const_iterator = value_type const *;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    private:
        pointer data_;
        size_type size_;
        [[no_unique_address]] allocator_type alloc_;

        static constexpr void swap_data(basic_static_string &a, basic_static_string &b) noexcept {
            using std::swap;
            swap(a.data_, b.data_);
            swap(a.size_, b.size_);
        }

        void copy_assign_from_other(basic_static_string const &other) {
            if (size_ != 0) {
                std::allocator_traits<allocator_type>::deallocate(alloc_, data_, size_);
            }

            size_ = other.size_;

            if (size_ != 0) {
                data_ = std::allocator_traits<allocator_type>::allocate(alloc_, size_);
                memcpy(std::to_address(data_), std::to_address(other.data_), size_);
            } else {
                data_ = nullptr;
            }
        }

    public:
        constexpr basic_static_string(allocator_type const &alloc = allocator_type{}) noexcept
            : data_{nullptr}, size_{0}, alloc_{alloc} {
        }

        explicit basic_static_string(view_type const sv, allocator_type const &alloc = allocator_type{})
            : size_{sv.size()}, alloc_{alloc} {

            if (sv.empty()) {
                data_ = nullptr;
                return;
            }

            data_ = std::allocator_traits<allocator_type>::allocate(alloc_, size_);
            memcpy(std::to_address(data_), sv.data(), size_);
        }

        basic_static_string(basic_static_string const &other) : basic_static_string{static_cast<view_type>(other), other.alloc_} {
        }

        basic_static_string &operator=(basic_static_string const &other) {
            if (this == &other) {
                return *this;
            }

            if constexpr (std::allocator_traits<allocator_type>::propagate_on_container_copy_assignment::value) {
                alloc_ = other.alloc_;
            }

            copy_assign_from_other(other);
            return *this;
        }

        constexpr basic_static_string(basic_static_string &&other) noexcept : data_{std::exchange(other.data_, nullptr)},
                                                                              size_{std::exchange(other.size_, 0)},
                                                                              alloc_{std::move(other.alloc_)} {
        }

        basic_static_string &operator=(basic_static_string &&other) noexcept(std::allocator_traits<allocator_type>::propagate_on_container_move_assignment::value
                                                                                || std::allocator_traits<allocator_type>::is_always_equal::value) {
            assert(this != &other);

            if constexpr (std::allocator_traits<allocator_type>::propagate_on_container_move_assignment::value) {
                swap(*this, other);
                return *this;
            } else if constexpr (std::allocator_traits<allocator_type>::is_always_equal::value) {
                swap_data(*this, other);
                return *this;
            } else {
                if (alloc_ == other.alloc_) [[likely]] {
                    swap_data(*this, other);
                    return *this;
                }

                // alloc_ != other.alloc_ and not allowed to propagate, need to copy
                copy_assign_from_other(other);
                return *this;
            }
        }

        ~basic_static_string() {
            if (data_ != nullptr) {
                std::allocator_traits<allocator_type>::deallocate(alloc_, data_, size_);
            }
        }

        operator view_type() const noexcept {
            return {std::to_address(data_), size_};
        }

        [[nodiscard]] const_pointer data() const noexcept {
            return data_;
        }
        [[nodiscard]] pointer data() noexcept {
            return data_;
        }

        [[nodiscard]] bool empty() const noexcept {
            return size_ == 0;
        }

        [[nodiscard]] size_type size() const noexcept {
            return size_;
        }

        [[nodiscard]] value_type operator[](size_type const ix) const noexcept {
            assert(ix < size());
            return std::to_address(data_)[ix];
        }

        [[nodiscard]] value_type &operator[](size_type const ix) noexcept {
            assert(ix < size());
            return std::to_address(data_)[ix];
        }

        [[nodiscard]] value_type front() const noexcept {
            assert(size() > 0);
            return (*this)[0];
        }

        [[nodiscard]] value_type &front() noexcept {
            assert(size() > 0);
            return (*this)[0];
        }

        [[nodiscard]] value_type back() const noexcept {
            assert(size() > 0);
            return (*this)[size() - 1];
        }

        [[nodiscard]] value_type &back() noexcept {
            assert(size() > 0);
            return (*this)[size() - 1];
        }

        [[nodiscard]] const_iterator begin() const noexcept {
            return std::to_address(data_);
        }
        [[nodiscard]] const_iterator end() const noexcept {
            return std::to_address(data_) + size_;
        }

        [[nodiscard]] const_iterator cbegin() const noexcept {
            return begin();
        }
        [[nodiscard]] const_iterator cend() const noexcept {
            return end();
        }

        [[nodiscard]] iterator begin() noexcept {
            return std::to_address(data_);
        }
        [[nodiscard]] iterator end() noexcept {
            return std::to_address(data_) + size_;
        }

        [[nodiscard]] const_reverse_iterator rbegin() const noexcept {
            return const_reverse_iterator{end()};
        }
        [[nodiscard]] const_reverse_iterator rend() const noexcept {
            return const_reverse_iterator{begin()};
        }

        [[nodiscard]] const_reverse_iterator crbegin() const noexcept {
            return rbegin();
        }
        [[nodiscard]] const_reverse_iterator crend() const noexcept {
            return rend();
        }

        [[nodiscard]] reverse_iterator rbegin() noexcept {
            return reverse_iterator{end()};
        }
        [[nodiscard]] reverse_iterator rend() noexcept {
            return reverse_iterator{begin()};
        }

        friend void swap(basic_static_string &a, basic_static_string &b) noexcept {
            using std::swap;
            swap(a.data_, b.data_);
            swap(a.size_, b.size_);
            swap(a.alloc_, b.alloc_);
        }

        bool operator==(basic_static_string const &other) const noexcept {
            return static_cast<view_type>(*this) == static_cast<view_type>(other);
        }

        auto operator<=>(basic_static_string const &other) const noexcept {
            return static_cast<view_type>(*this) <=> static_cast<view_type>(other);
        }

        friend bool operator==(basic_static_string const &self, view_type const other) noexcept {
            return static_cast<view_type>(self) == other;
        }

        friend auto operator<=>(basic_static_string const &self, view_type const other) noexcept {
            return static_cast<view_type>(self) <=> other;
        }
    };

    template<typename Char, typename CharTraits, typename Allocator>
    std::basic_ostream<Char, CharTraits> &operator<<(std::basic_ostream<Char, CharTraits> &os, basic_static_string<Char, CharTraits, Allocator> const &str) {
        os << static_cast<std::basic_string_view<Char, CharTraits>>(str);
        return os;
    }

    using static_string = basic_static_string<char>;
    using static_wstring = basic_static_string<wchar_t>;
    using static_u8string = basic_static_string<char8_t>;
    using static_u16string = basic_static_string<char16_t>;
    using static_u32string = basic_static_string<char32_t>;

} // namespace dice::template_library

#endif // DICE_TEMPLATELIBRARY_CONSTSTRING_HPP
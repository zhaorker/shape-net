#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <initializer_list>

namespace exstring{
class Explicit_string : public std::string {
public:
    Explicit_string() = default;

    explicit Explicit_string(const Explicit_string&) = default;
    Explicit_string(Explicit_string&& other) noexcept
        : std::string(std::move(other)) {}

    explicit Explicit_string(const std::string& s) : std::string(s) {}
    explicit Explicit_string(std::string&& s) noexcept
        : std::string(std::move(s)) {}

    explicit Explicit_string(const char* s) : std::string(s) {}
    explicit Explicit_string(const char* s, size_t n) : std::string(s, n) {}

    explicit Explicit_string(size_t n, char c) : std::string(n, c) {}

    explicit Explicit_string(std::string_view sv) : std::string(sv) {}

    template <class InputIterator>
    explicit Explicit_string(InputIterator first, InputIterator last)
        : std::string(first, last) {}

    explicit Explicit_string(std::initializer_list<char> il)
        : std::string(il) {}

    explicit Explicit_string(const Explicit_string& str, size_t pos,
                             size_t len = npos)
        : std::string(str, pos, len) {}
    explicit Explicit_string(const std::string& str, size_t pos,
                             size_t len = npos)
        : std::string(str, pos, len) {}

    Explicit_string& operator=(const Explicit_string& str) {
        std::string::operator=(str);
        return *this;
    }
    Explicit_string& operator=(Explicit_string&& str) noexcept {
        std::string::operator=(std::move(str));
        return *this;
    }

    std::string_view substr(size_t pos = 0,
                            size_t len = std::string::npos) const {
        return std::string_view(*this).substr(pos, len);
    }

    using std::string::npos;
};

inline Explicit_string operator+(const Explicit_string& lhs,
                                 const Explicit_string& rhs) {
    return Explicit_string(static_cast<const std::string&>(lhs) +
                           static_cast<const std::string&>(rhs));
}
inline Explicit_string operator+(const Explicit_string& lhs,
                                 Explicit_string&& rhs) {
    return Explicit_string(static_cast<const std::string&>(lhs) +
                           static_cast<std::string&&>(rhs));
}
inline Explicit_string operator+(Explicit_string&& lhs,
                                 const Explicit_string& rhs) {
    return Explicit_string(static_cast<std::string&&>(lhs) +
                           static_cast<const std::string&>(rhs));
}
inline Explicit_string operator+(Explicit_string&& lhs,
                                 Explicit_string&& rhs) {
    return Explicit_string(static_cast<std::string&&>(lhs) +
                           static_cast<std::string&&>(rhs));
}
inline Explicit_string operator+(const Explicit_string& lhs, const char* rhs) {
    return Explicit_string(static_cast<const std::string&>(lhs) + rhs);
}
inline Explicit_string operator+(Explicit_string&& lhs, const char* rhs) {
    return Explicit_string(std::move(static_cast<std::string&>(lhs)) + rhs);
}
inline Explicit_string operator+(const char* lhs, const Explicit_string& rhs) {
    return Explicit_string(lhs + static_cast<const std::string&>(rhs));
}
inline Explicit_string operator+(const char* lhs, Explicit_string&& rhs) {
    return Explicit_string(lhs + std::move(static_cast<std::string&>(rhs)));
}
inline Explicit_string operator+(const Explicit_string& lhs, char rhs) {
    return Explicit_string(static_cast<const std::string&>(lhs) + rhs);
}
inline Explicit_string operator+(Explicit_string&& lhs, char rhs) {
    return Explicit_string(std::move(static_cast<std::string&>(lhs)) + rhs);
}
inline Explicit_string operator+(char lhs, const Explicit_string& rhs) {
    return Explicit_string(std::string(1, lhs) + static_cast<const std::string&>(rhs));
}
inline Explicit_string operator+(char lhs, Explicit_string&& rhs) {
    return Explicit_string(std::string(1, lhs) + std::move(static_cast<std::string&>(rhs)));
}
}
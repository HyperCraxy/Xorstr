#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#if (defined(_MSVC_LANG) ? _MSVC_LANG : __cplusplus) < 201703L
#  error "xorstr.hpp requires C++17 or later"
#endif
#if defined(__cpp_consteval) && __cpp_consteval >= 201811L
#  define XORSTR_HAS_CONSTEVAL 1
#  define XORSTR_CX_ consteval
#else
#  define XORSTR_HAS_CONSTEVAL 0
#  define XORSTR_CX_ constexpr
#endif
#if defined(XORSTR_PER_TU_KEYS)
#  if defined(_MSC_VER)
#    pragma message("xorstr: XORSTR_PER_TU_KEYS - per-TU keys: keep XorStr out of headers, inline functions and templates")
#  else
#    warning "xorstr: XORSTR_PER_TU_KEYS - per-TU keys: keep XorStr out of headers, inline functions and templates"
#  endif
#endif

namespace xorstr {

template <class CharT>
constexpr bool is_char_type =
    std::is_same<CharT, char>::value || std::is_same<CharT, wchar_t>::value ||
    std::is_same<CharT, char16_t>::value || std::is_same<CharT, char32_t>::value
#if defined(__cpp_char8_t)
    || std::is_same<CharT, char8_t>::value
#endif
    ;
constexpr std::uint64_t mix64(std::uint64_t z) noexcept
{
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
constexpr std::uint64_t fnv1a(const char* s) noexcept
{
    std::uint64_t h = 0xCBF29CE484222325ull;
    while (*s) { h ^= static_cast<unsigned char>(*s++); h *= 0x00000100000001B3ull; }
    return h;
}
template <class CharT, std::size_t N>
constexpr std::uint64_t literal_hash(const CharT (&lit)[N]) noexcept
{
    std::uint64_t h = 0x84222325CBF29CE4ull;
    for (std::size_t i = 0; i < N; ++i)
        for (std::size_t b = 0; b < sizeof(CharT); ++b)
            h = (h ^ ((static_cast<std::uint64_t>(lit[i]) >> (8u * b)) & 0xFFu)) *
                0x00000100000001B3ull;
    return h;
}
#if defined(XORSTR_PER_TU_KEYS)
#  define XORSTR_COUNTER_ARG_ __COUNTER__
#else
#  define XORSTR_COUNTER_ARG_ 0
#endif
constexpr std::uint64_t make_seed(const char* file, std::uint64_t line,
                                  std::uint64_t counter, std::uint64_t content) noexcept
{
    std::uint64_t s = 0x243F6A8885A308D3ull;
    s ^= mix64(fnv1a(file) ^ 0xA5A5A5A5A5A5A5A5ull);
    s ^= mix64(content ^ 0x0123456789ABCDEFull);
#if defined(XORSTR_PER_TU_KEYS)
    s ^= mix64(fnv1a(__DATE__) ^ 0xFEDCBA9876543210ull);
    s ^= mix64(fnv1a(__TIME__) ^ 0x9E3779B97F4A7C15ull);
    counter *= 0xC2B2AE3D27D4EB4Full;
#else
    counter = 0;
# if defined(__TIMESTAMP__)
    s ^= mix64(fnv1a(__TIMESTAMP__) ^ 0xFEDCBA9876543210ull);
# endif
# if defined(XORSTR_BUILD_SEED)
    s ^= mix64(static_cast<std::uint64_t>(XORSTR_BUILD_SEED) ^ 0xC2B2AE3D27D4EB4Full);
# endif
#endif
    return mix64(s + (line + 1u) * 0x9E3779B97F4A7C15ull + counter);
}
constexpr std::uint64_t keystream_word(std::uint64_t seed, std::size_t word) noexcept
{ return mix64(seed + (word + 1u) * 0x9E3779B97F4A7C15ull); }
constexpr unsigned char keystream_byte(std::uint64_t seed, std::size_t k) noexcept
{ return static_cast<unsigned char>((keystream_word(seed, k / 8u) >> (8u * (k % 8u))) & 0xFFu); }
template <class CharT>
constexpr CharT keystream_element(std::uint64_t seed, std::size_t i) noexcept
{   const std::size_t o = i * sizeof(CharT);
    return static_cast<CharT>(keystream_word(seed, o / 8u) >> (8u * (o % 8u))); }

[[noreturn]] inline void not_a_constant_expression()
{
#if defined(_CPPUNWIND) || defined(__cpp_exceptions)
    throw "XorStr: argument must be a null-terminated string literal";
#else
    for (;;) { }
#endif
}
template <class CharT, std::size_t N, std::uint64_t Seed>
XORSTR_CX_ std::array<CharT, N> encrypt(const CharT (&lit)[N]) noexcept
{
    static_assert(is_char_type<CharT>,
                  "XorStr supports char, wchar_t, char8_t, char16_t and char32_t");
    if (lit[N - 1u] != CharT(0)) not_a_constant_expression();
    std::array<CharT, N> out{};
    for (std::size_t i = 0; i < N; ++i)
        out[i] = static_cast<CharT>(lit[i] ^ keystream_element<CharT>(Seed, i));
    return out;
}
template <class CharT, std::size_t N, std::uint64_t Seed>
class guard
{
public:
    constexpr static std::size_t size() noexcept { return N - 1u; }
    const CharT* data() const noexcept { return buf_; }
    const CharT* c_str() const noexcept { return buf_; }
    operator const CharT*() const noexcept { return buf_; }
    const CharT& operator[](std::size_t i) const noexcept { return buf_[i]; }
    guard(const guard&) = delete;
    guard& operator=(const guard&) = delete;
    guard(guard&&) = delete;
    guard& operator=(guard&&) = delete;
    ~guard()
    {
        volatile CharT* p = buf_;
        for (std::size_t i = 0; i < N; ++i) p[i] = CharT(0);
        std::atomic_signal_fence(std::memory_order_seq_cst);
    }
    explicit guard(const std::array<CharT, N>& cipher) noexcept
    {
        const std::uint64_t seed = opaque(Seed);
        for (std::size_t i = 0; i < N; ++i)
            buf_[i] = static_cast<CharT>(cipher[i] ^ keystream_element<CharT>(seed, i));
    }
private:
    static std::uint64_t opaque(std::uint64_t v) noexcept
    {
        volatile std::uint64_t slot = v;
        std::atomic_signal_fence(std::memory_order_seq_cst);
        return slot;
    }
    CharT buf_[N];
};

template <class CharT, std::size_t N, std::uint64_t Seed>
struct string
{
    std::array<CharT, N> cipher;
    [[nodiscard]] guard<CharT, N, Seed> get() const noexcept
    { return guard<CharT, N, Seed>(cipher); }
};

template <std::uint64_t Seed, class CharT, std::size_t N>
[[nodiscard]] XORSTR_CX_ string<CharT, N, Seed> make(const CharT (&lit)[N]) noexcept
{ return string<CharT, N, Seed>{ encrypt<CharT, N, Seed>(lit) }; }

}

#if defined(XorStr) || defined(XORSTR_MAKE_) || defined(XORSTR_SEED_)
#  error "XorStr is already defined - xorstr.hpp will not silently ignore another macro of that name"
#endif
#define XORSTR_SEED_(lit)                                                     \
    (::xorstr::make_seed(__FILE__, __LINE__, XORSTR_COUNTER_ARG_,             \
                         ::xorstr::literal_hash(lit)))
#if XORSTR_HAS_CONSTEVAL
#  define XORSTR_MAKE_(lit) (::xorstr::make<XORSTR_SEED_(lit)>(lit))
#else
#  define XORSTR_MAKE_(lit)                                                   \
     ([]() {                                                                  \
         constexpr auto xorstr_ = ::xorstr::make<XORSTR_SEED_(lit)>(lit);     \
         return xorstr_;                                                      \
     }())
#endif

#define XorStr(str) XORSTR_MAKE_(str)

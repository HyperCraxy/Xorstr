# Xorstr

Compile-time string obfuscation for C++17 and C++20. One header, one macro, no dependencies, an old version of my private Xorstr.

```cpp
#include "xorstr\_min.hpp"

int main() {
    auto s = XorStr("secret");     // encrypted at compile time
    {
        auto d = s.get();          // decrypted into a stack buffer here
        std::puts(d.c\_str());      // "secret"
    }                              // wiped here
}
```

The literal never appears in the binary, and the plaintext only exists while the guard is
alive. `strings yourapp | grep secret` prints nothing.

## Features

* Encrypted entirely at compile time — `consteval` on C++20, forced `constexpr` on C++17
* A different key per literal, and per build when you pass `XORSTR\_BUILD\_SEED`
* RAII guard: decrypts on construction, volatile-wipes on destruction
* One macro for every character type: `char`, `wchar\_t`, `char8\_t`, `char16\_t`, `char32\_t`
* Header-only, no heap, no exceptions, no intrinsics, scalar XOR — auditable in one sitting
* Safe in headers, `inline` functions and templates (see *Keys* below)
* No SIMD, no CPUID, no self-test block, no build system integration required

## Usage

Copy `xorstr\_min.hpp` into your project and include it. (Rename it to `xorstr.hpp` if you
prefer — nothing depends on the file name.)

```cpp
#include "xorstr\_min.hpp"
#include <cstdio>

int main() {
    auto s = XorStr("hello");

    auto d = s.get();              // guard: plaintext is valid until d goes out of scope
    std::printf("%s\\n", d.c\_str());
}
```

The character type is deduced from the literal, so there is nothing else to remember:

```cpp
auto a = XorStr("narrow");     // char
auto b = XorStr(L"wide");      // wchar\_t
auto c = XorStr(u8"utf-8");    // char8\_t on C++20, char on C++17
auto d = XorStr(u"utf-16");    // char16\_t
auto e = XorStr(U"utf-32");    // char32\_t
```

### The guard

`s.get()` returns a small object that owns a decrypted stack buffer:

|member|meaning|
|-|-|
|`c\_str()` / `data()`|`const CharT\*` to the plaintext|
|`size()`|length, excluding the terminator|
|`operator\[]`|element access|
|`operator const CharT\*`|implicit conversion, e.g. `std::puts(g)`|

Keep it alive for as long as you need the text. It is wiped when it goes out of scope, and
it cannot be copied or moved — copying would double-wipe.

### There is no owning copy

No `.str()`, no conversion to `std::string`. Those would hand you plaintext that the header
cannot wipe, which defeats the whole point. If you need one, make it yourself and wipe it
yourself:

```cpp
std::string copy;
{
    auto g = s.get();
    copy = g.c\_str();          // now yours: the header will not wipe it for you
}
```

## Keys

The key comes from a constexpr seed built from the literal, its source location and an
optional build salt.

**Default — stable and ODR-safe.** The seed is identical in every translation unit that sees
the same source, so `XorStr` is safe inside a shared header, an `inline` function or a
template. That is not a detail: with a per-TU seed those cases violate the One Definition
Rule, and the linker resolves it silently — the string just decrypts to garbage. Different
literals still get different keys (the literal's hash is in the seed), even on one line.

**Rotation.** Define `XORSTR\_BUILD\_SEED` to rotate keys on every build:

```bash
g++ -std=c++20 -O2 -DXORSTR\_BUILD\_SEED=$(od -An -N4 -tu4 /dev/urandom | tr -d ' ') main.cpp
```

```cmake
string(RANDOM LENGTH 18 ALPHABET 0123456789 XORSTR\_BUILD\_SEED)
target\_compile\_definitions(app PRIVATE XORSTR\_BUILD\_SEED=${XORSTR\_BUILD\_SEED})
```

Without it, the ciphertext is stable for a given source location: fine for hiding strings,
weak for rotating them. Set it in your release build.

**`-DXORSTR\_PER\_TU\_KEYS` — opt-in, `.cpp` files only.** Adds `\_\_COUNTER\_\_`, `\_\_DATE\_\_` and
`\_\_TIME\_\_` to the seed: a new key on every single compile, with no build-system work at all.
The cost is ODR safety, because those macros differ per translation unit. Use it only in
`.cpp` files, never in a header, an `inline` function or a template. The header warns you at
compile time when this mode is on.

## What it does and does not do

It defeats `strings`, YARA-style static rules, IDA auto-comments and casual `.rdata`
scanning. It does **not** defeat someone with a debugger: at the point of use, the plaintext
is in memory, and any copy you make is yours. It is obfuscation, not encryption.

The "nothing in `.rodata`" property relies on a volatile round-trip that stops the optimiser
from folding the decryption, which is a best-effort heuristic rather than a language
guarantee. Check it yourself once, on your toolchain and optimisation level — including
`-flto`:

```bash
grep -a "your-literal-here" yourapp        # expect: no output
```

## Requirements

* C++17 or later. C++20 is recommended: there `make()`/`encrypt()` are `consteval`, so
compile-time evaluation is mandatory rather than merely forced by the macro's idiom.
* GCC, Clang or MSVC. Any target, any architecture (the decrypt loop is plain scalar C++).
* Nothing else. The only includes are `<array>`, `<atomic>`, `<cstddef>`, `<cstdint>` and
`<type\_traits>`.

## License

MIT — License.

Usage of AI was used to make this Readme


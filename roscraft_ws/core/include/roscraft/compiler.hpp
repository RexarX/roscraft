#pragma once

#if defined(__GNUC__) || defined(__clang__)
#define ROSCRAFT_EXPECT_TRUE(x) __builtin_expect(!!(x), 1)
#define ROSCRAFT_EXPECT_FALSE(x) __builtin_expect(!!(x), 0)
#else
#define ROSCRAFT_EXPECT_TRUE(x) (x)
#define ROSCRAFT_EXPECT_FALSE(x) (x)
#endif

#ifdef _MSC_VER
#define ROSCRAFT_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define ROSCRAFT_FORCE_INLINE __attribute__((always_inline)) inline
#else
#define ROSCRAFT_FORCE_INLINE inline
#endif

#if defined(__clang__)
#define ROSCRAFT_ALWAYS_INLINE [[clang::always_inline]]
#elif defined(__GNUC__)
#define ROSCRAFT_ALWAYS_INLINE [[gnu::always_inline]]
#elif defined(_MSC_VER)
#define ROSCRAFT_ALWAYS_INLINE [[msvc::forceinline]]
#else
#define ROSCRAFT_ALWAYS_INLINE
#endif

#ifdef _MSC_VER
#define ROSCRAFT_NO_INLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define ROSCRAFT_NO_INLINE __attribute__((noinline))
#else
#define ROSCRAFT_NO_INLINE
#endif

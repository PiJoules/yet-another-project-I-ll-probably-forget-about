#ifndef LIBC_INCLUDE__INTERNALS_H_
#define LIBC_INCLUDE__INTERNALS_H_

// C++ header guards.
#ifdef __cplusplus
#define __BEGIN_CDECLS extern "C" {
#define __END_CDECLS }
#else
#define __BEGIN_CDECLS
#define __END_CDECLS
#endif

#ifndef NULL
#ifdef __cplusplus
#define NULL nullptr
#else
#define NULL (char * (0))
#endif
#endif

#define __EXPORT __attribute__((visibility("default")))

#ifdef NDEBUG
#define DEBUG_PRINT(...)
#define DEBUG_ASSERT(x) (void)(x)
#else

// Users can define `LOCAL_DEBUG_LVL` to control printing. Note that this must
// be defined in source files since headers can be included in any order.
#if defined(LOCAL_DEBUG_LVL) && LOCAL_DEBUG_LVL < 1
// This is a function that is only used for accepting variadic macro args
// while still being able to evaluate them. Ideally, the compiler will optimize
// this out.
inline void __dummy_printf(...) {}
#define DEBUG_PRINT(...) (__dummy_printf(__VA_ARGS__))
#else
#define DEBUG_PRINT(...) printf("DEBUG_PRINT> " __VA_ARGS__)
#endif

#define DEBUG_ASSERT(x) assert(x)
#endif

#define DEBUG_OK(x) DEBUG_ASSERT((x) == K_OK)

#endif  // LIBC_INCLUDE__INTERNALS_H_

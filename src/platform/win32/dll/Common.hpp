#pragma once

#include <cstddef>

#ifdef TWASE_STATIC_LIB
#undef TWASE_HEADER_ONLY
#define TWASE_INLINE
#else
#define TWASE_HEADER_ONLY
#define TWASE_INLINE inline
#endif

#ifndef TWASE_ASSERT_ESCAPE
#define TWASE_ASSERT_ESCAPE(...) __VA_ARGS__
#endif

#ifndef TWASE_ASSERT_SIZE
#define TWASE_ASSERT_SIZE(cls, size)                                                                                 \
    static_assert(sizeof(cls) == size, #cls " size does not match the expected size (" #size ") ")
#endif

#ifndef TWASE_ASSERT_OFFSET
// TODO: find a better way to handle this (clang does not allow offsetof in static_assert)
#ifdef __clang__
#define TWASE_ASSERT_OFFSET(cls, mbr, offset)
#else
#define TWASE_ASSERT_OFFSET(cls, mbr, offset)                                                                        \
    static_assert(offsetof(cls, mbr) == offset, #cls "::" #mbr " is not on the expected offset (" #offset ")")
#endif
#endif

/**
 * @brief This macro is used to avoid compiler warnings about unreferenced / used parameter.
 */
#ifndef TWASE_UNUSED_PARAMETER
#define TWASE_UNUSED_PARAMETER(param) (param)
#endif

#ifndef TWASE_DECLARE_TYPE
#define TWASE_DECLARE_TYPE(type, name)                                                                               \
    const type* const_##name;                                                                                          \
    type* name;
#endif

#ifndef TWASE_C_EXPORT
#define TWASE_C_EXPORT extern "C" __declspec(dllexport)
#endif

#ifndef TWASE_CALL
#define TWASE_CALL __fastcall
#endif

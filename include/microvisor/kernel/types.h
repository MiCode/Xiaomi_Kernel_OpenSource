/*
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* Auto generated - do not modify */

/** @addtogroup lib_microvisor
 * @{
 */
/** @addtogroup lib_microvisor_types Microvisor Types
 * @{
 */
#ifndef __AUTO__MICROVISOR_TYPES_H__
#define __AUTO__MICROVISOR_TYPES_H__

#if !defined(ASSEMBLY)

#define OKL4_DEFAULT_PERMS OKL4_PAGE_PERMS_RWX
#define OKL4_DEFAULT_CACHE_ATTRIBUTES OKL4_PAGE_CACHE_DEFAULT

#if __SIZEOF_POINTER__ != 8
#define __ptr64(type, name) union { type name; uint64_t _x_##name; }
#define __ptr64_array(type, name) union { type val; uint64_t _x; } name
#else
#define __ptr64(type, name) type name
#define __ptr64_array(type, name) type name
#endif

/**
    The `okl4_bool_t` type represents a standard boolean value.  Valid values are
    restricted to @ref OKL4_TRUE and @ref OKL4_FALSE.
*/

typedef _Bool okl4_bool_t;








/**
    - BITS 7..0 -   @ref OKL4_MASK_AFF0_ARM_MPIDR
    - BITS 15..8 -   @ref OKL4_MASK_AFF1_ARM_MPIDR
    - BITS 23..16 -   @ref OKL4_MASK_AFF2_ARM_MPIDR
    - BIT 24 -   @ref OKL4_MASK_MT_ARM_MPIDR
    - BIT 30 -   @ref OKL4_MASK_U_ARM_MPIDR
    - BIT 31 -   @ref OKL4_MASK_MP_ARM_MPIDR
    - BITS 39..32 -   @ref OKL4_MASK_AFF3_ARM_MPIDR
*/

/*lint -esym(621, okl4_arm_mpidr_t) */
typedef uint64_t okl4_arm_mpidr_t;

/*lint -esym(621, okl4_arm_mpidr_getaff0) */
/*lint -esym(714, okl4_arm_mpidr_getaff0) */
OKL4_FORCE_INLINE uint64_t
okl4_arm_mpidr_getaff0(const okl4_arm_mpidr_t *x);

/*lint -esym(621, okl4_arm_mpidr_setaff0) */
OKL4_FORCE_INLINE void
okl4_arm_mpidr_setaff0(okl4_arm_mpidr_t *x, uint64_t _aff0);

/*lint -esym(621, okl4_arm_mpidr_getaff1) */
/*lint -esym(714, okl4_arm_mpidr_getaff1) */
OKL4_FORCE_INLINE uint64_t
okl4_arm_mpidr_getaff1(const okl4_arm_mpidr_t *x);

/*lint -esym(621, okl4_arm_mpidr_setaff1) */
OKL4_FORCE_INLINE void
okl4_arm_mpidr_setaff1(okl4_arm_mpidr_t *x, uint64_t _aff1);

/*lint -esym(621, okl4_arm_mpidr_getaff2) */
/*lint -esym(714, okl4_arm_mpidr_getaff2) */
OKL4_FORCE_INLINE uint64_t
okl4_arm_mpidr_getaff2(const okl4_arm_mpidr_t *x);

/*lint -esym(621, okl4_arm_mpidr_setaff2) */
OKL4_FORCE_INLINE void
okl4_arm_mpidr_setaff2(okl4_arm_mpidr_t *x, uint64_t _aff2);

/*lint -esym(621, okl4_arm_mpidr_getaff3) */
/*lint -esym(714, okl4_arm_mpidr_getaff3) */
OKL4_FORCE_INLINE uint64_t
okl4_arm_mpidr_getaff3(const okl4_arm_mpidr_t *x);

/*lint -esym(621, okl4_arm_mpidr_setaff3) */
OKL4_FORCE_INLINE void
okl4_arm_mpidr_setaff3(okl4_arm_mpidr_t *x, uint64_t _aff3);

/*lint -esym(621, okl4_arm_mpidr_getmt) */
/*lint -esym(714, okl4_arm_mpidr_getmt) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_mpidr_getmt(const okl4_arm_mpidr_t *x);

/*lint -esym(621, okl4_arm_mpidr_setmt) */
OKL4_FORCE_INLINE void
okl4_arm_mpidr_setmt(okl4_arm_mpidr_t *x, okl4_bool_t _mt);

/*lint -esym(621, okl4_arm_mpidr_getu) */
/*lint -esym(714, okl4_arm_mpidr_getu) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_mpidr_getu(const okl4_arm_mpidr_t *x);

/*lint -esym(621, okl4_arm_mpidr_setu) */
OKL4_FORCE_INLINE void
okl4_arm_mpidr_setu(okl4_arm_mpidr_t *x, okl4_bool_t _u);

/*lint -esym(621, okl4_arm_mpidr_getmp) */
/*lint -esym(714, okl4_arm_mpidr_getmp) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_mpidr_getmp(const okl4_arm_mpidr_t *x);

/*lint -esym(714, okl4_arm_mpidr_init) */
OKL4_FORCE_INLINE void
okl4_arm_mpidr_init(okl4_arm_mpidr_t *x);

/*lint -esym(714, okl4_arm_mpidr_cast) */
OKL4_FORCE_INLINE okl4_arm_mpidr_t
okl4_arm_mpidr_cast(uint64_t p, okl4_bool_t force);




/*lint -esym(621, OKL4_ARM_MPIDR_AFF0_MASK) */
#define OKL4_ARM_MPIDR_AFF0_MASK ((okl4_arm_mpidr_t)255U) /* Deprecated */
/*lint -esym(621, OKL4_MASK_AFF0_ARM_MPIDR) */
#define OKL4_MASK_AFF0_ARM_MPIDR ((okl4_arm_mpidr_t)255U)
/*lint -esym(621, OKL4_SHIFT_AFF0_ARM_MPIDR) */
#define OKL4_SHIFT_AFF0_ARM_MPIDR (0)
/*lint -esym(621, OKL4_WIDTH_AFF0_ARM_MPIDR) */
#define OKL4_WIDTH_AFF0_ARM_MPIDR (8)
/*lint -esym(621, OKL4_ARM_MPIDR_AFF1_MASK) */
#define OKL4_ARM_MPIDR_AFF1_MASK ((okl4_arm_mpidr_t)255U << 8) /* Deprecated */
/*lint -esym(621, OKL4_MASK_AFF1_ARM_MPIDR) */
#define OKL4_MASK_AFF1_ARM_MPIDR ((okl4_arm_mpidr_t)255U << 8)
/*lint -esym(621, OKL4_SHIFT_AFF1_ARM_MPIDR) */
#define OKL4_SHIFT_AFF1_ARM_MPIDR (8)
/*lint -esym(621, OKL4_WIDTH_AFF1_ARM_MPIDR) */
#define OKL4_WIDTH_AFF1_ARM_MPIDR (8)
/*lint -esym(621, OKL4_ARM_MPIDR_AFF2_MASK) */
#define OKL4_ARM_MPIDR_AFF2_MASK ((okl4_arm_mpidr_t)255U << 16) /* Deprecated */
/*lint -esym(621, OKL4_MASK_AFF2_ARM_MPIDR) */
#define OKL4_MASK_AFF2_ARM_MPIDR ((okl4_arm_mpidr_t)255U << 16)
/*lint -esym(621, OKL4_SHIFT_AFF2_ARM_MPIDR) */
#define OKL4_SHIFT_AFF2_ARM_MPIDR (16)
/*lint -esym(621, OKL4_WIDTH_AFF2_ARM_MPIDR) */
#define OKL4_WIDTH_AFF2_ARM_MPIDR (8)
/*lint -esym(621, OKL4_ARM_MPIDR_MT_MASK) */
#define OKL4_ARM_MPIDR_MT_MASK ((okl4_arm_mpidr_t)1U << 24) /* Deprecated */
/*lint -esym(621, OKL4_MASK_MT_ARM_MPIDR) */
#define OKL4_MASK_MT_ARM_MPIDR ((okl4_arm_mpidr_t)1U << 24)
/*lint -esym(621, OKL4_SHIFT_MT_ARM_MPIDR) */
#define OKL4_SHIFT_MT_ARM_MPIDR (24)
/*lint -esym(621, OKL4_WIDTH_MT_ARM_MPIDR) */
#define OKL4_WIDTH_MT_ARM_MPIDR (1)
/*lint -esym(621, OKL4_ARM_MPIDR_U_MASK) */
#define OKL4_ARM_MPIDR_U_MASK ((okl4_arm_mpidr_t)1U << 30) /* Deprecated */
/*lint -esym(621, OKL4_MASK_U_ARM_MPIDR) */
#define OKL4_MASK_U_ARM_MPIDR ((okl4_arm_mpidr_t)1U << 30)
/*lint -esym(621, OKL4_SHIFT_U_ARM_MPIDR) */
#define OKL4_SHIFT_U_ARM_MPIDR (30)
/*lint -esym(621, OKL4_WIDTH_U_ARM_MPIDR) */
#define OKL4_WIDTH_U_ARM_MPIDR (1)
/*lint -esym(621, OKL4_ARM_MPIDR_MP_MASK) */
#define OKL4_ARM_MPIDR_MP_MASK ((okl4_arm_mpidr_t)1U << 31) /* Deprecated */
/*lint -esym(621, OKL4_MASK_MP_ARM_MPIDR) */
#define OKL4_MASK_MP_ARM_MPIDR ((okl4_arm_mpidr_t)1U << 31)
/*lint -esym(621, OKL4_SHIFT_MP_ARM_MPIDR) */
#define OKL4_SHIFT_MP_ARM_MPIDR (31)
/*lint -esym(621, OKL4_WIDTH_MP_ARM_MPIDR) */
#define OKL4_WIDTH_MP_ARM_MPIDR (1)
/*lint -esym(621, OKL4_ARM_MPIDR_AFF3_MASK) */
#define OKL4_ARM_MPIDR_AFF3_MASK ((okl4_arm_mpidr_t)255U << 32) /* Deprecated */
/*lint -esym(621, OKL4_MASK_AFF3_ARM_MPIDR) */
#define OKL4_MASK_AFF3_ARM_MPIDR ((okl4_arm_mpidr_t)255U << 32)
/*lint -esym(621, OKL4_SHIFT_AFF3_ARM_MPIDR) */
#define OKL4_SHIFT_AFF3_ARM_MPIDR (32)
/*lint -esym(621, OKL4_WIDTH_AFF3_ARM_MPIDR) */
#define OKL4_WIDTH_AFF3_ARM_MPIDR (8)


/*lint -sem(okl4_arm_mpidr_getaff0, 1p, @n >= 0 && @n <= 255) */
/*lint -esym(621, okl4_arm_mpidr_getaff0) */
/*lint -esym(714, okl4_arm_mpidr_getaff0) */
OKL4_FORCE_INLINE uint64_t
okl4_arm_mpidr_getaff0(const okl4_arm_mpidr_t *x)
{
    uint64_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint64_t field : 8;
        } bits;
        okl4_arm_mpidr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (uint64_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_mpidr_setaff0, 2n >= 0 && 2n <= 255) */
/*lint -esym(714, okl4_arm_mpidr_setaff0) */

/*lint -esym(621, okl4_arm_mpidr_setaff0) */
OKL4_FORCE_INLINE void
okl4_arm_mpidr_setaff0(okl4_arm_mpidr_t *x, uint64_t _aff0)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint64_t field : 8;
        } bits;
        okl4_arm_mpidr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint64_t)_aff0;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_mpidr_getaff1, 1p, @n >= 0 && @n <= 255) */
/*lint -esym(621, okl4_arm_mpidr_getaff1) */
/*lint -esym(714, okl4_arm_mpidr_getaff1) */
OKL4_FORCE_INLINE uint64_t
okl4_arm_mpidr_getaff1(const okl4_arm_mpidr_t *x)
{
    uint64_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint64_t _skip : 8;
            uint64_t field : 8;
        } bits;
        okl4_arm_mpidr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (uint64_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_mpidr_setaff1, 2n >= 0 && 2n <= 255) */
/*lint -esym(714, okl4_arm_mpidr_setaff1) */

/*lint -esym(621, okl4_arm_mpidr_setaff1) */
OKL4_FORCE_INLINE void
okl4_arm_mpidr_setaff1(okl4_arm_mpidr_t *x, uint64_t _aff1)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint64_t _skip : 8;
            uint64_t field : 8;
        } bits;
        okl4_arm_mpidr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint64_t)_aff1;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_mpidr_getaff2, 1p, @n >= 0 && @n <= 255) */
/*lint -esym(621, okl4_arm_mpidr_getaff2) */
/*lint -esym(714, okl4_arm_mpidr_getaff2) */
OKL4_FORCE_INLINE uint64_t
okl4_arm_mpidr_getaff2(const okl4_arm_mpidr_t *x)
{
    uint64_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint64_t _skip : 16;
            uint64_t field : 8;
        } bits;
        okl4_arm_mpidr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (uint64_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_mpidr_setaff2, 2n >= 0 && 2n <= 255) */
/*lint -esym(714, okl4_arm_mpidr_setaff2) */

/*lint -esym(621, okl4_arm_mpidr_setaff2) */
OKL4_FORCE_INLINE void
okl4_arm_mpidr_setaff2(okl4_arm_mpidr_t *x, uint64_t _aff2)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint64_t _skip : 16;
            uint64_t field : 8;
        } bits;
        okl4_arm_mpidr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint64_t)_aff2;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_mpidr_getmt, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_mpidr_getmt) */
/*lint -esym(714, okl4_arm_mpidr_getmt) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_mpidr_getmt(const okl4_arm_mpidr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint64_t _skip : 24;
            _Bool field : 1;
        } bits;
        okl4_arm_mpidr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_mpidr_setmt, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_mpidr_setmt) */

/*lint -esym(621, okl4_arm_mpidr_setmt) */
OKL4_FORCE_INLINE void
okl4_arm_mpidr_setmt(okl4_arm_mpidr_t *x, okl4_bool_t _mt)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint64_t _skip : 24;
            _Bool field : 1;
        } bits;
        okl4_arm_mpidr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_mt;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_mpidr_getu, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_mpidr_getu) */
/*lint -esym(714, okl4_arm_mpidr_getu) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_mpidr_getu(const okl4_arm_mpidr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint64_t _skip : 30;
            _Bool field : 1;
        } bits;
        okl4_arm_mpidr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_mpidr_setu, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_mpidr_setu) */

/*lint -esym(621, okl4_arm_mpidr_setu) */
OKL4_FORCE_INLINE void
okl4_arm_mpidr_setu(okl4_arm_mpidr_t *x, okl4_bool_t _u)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint64_t _skip : 30;
            _Bool field : 1;
        } bits;
        okl4_arm_mpidr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_u;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_mpidr_getmp, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_mpidr_getmp) */
/*lint -esym(714, okl4_arm_mpidr_getmp) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_mpidr_getmp(const okl4_arm_mpidr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint64_t _skip : 31;
            _Bool field : 1;
        } bits;
        okl4_arm_mpidr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_mpidr_getaff3, 1p, @n >= 0 && @n <= 255) */
/*lint -esym(621, okl4_arm_mpidr_getaff3) */
/*lint -esym(714, okl4_arm_mpidr_getaff3) */
OKL4_FORCE_INLINE uint64_t
okl4_arm_mpidr_getaff3(const okl4_arm_mpidr_t *x)
{
    uint64_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint64_t _skip : 32;
            uint64_t field : 8;
        } bits;
        okl4_arm_mpidr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (uint64_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_mpidr_setaff3, 2n >= 0 && 2n <= 255) */
/*lint -esym(714, okl4_arm_mpidr_setaff3) */

/*lint -esym(621, okl4_arm_mpidr_setaff3) */
OKL4_FORCE_INLINE void
okl4_arm_mpidr_setaff3(okl4_arm_mpidr_t *x, uint64_t _aff3)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint64_t _skip : 32;
            uint64_t field : 8;
        } bits;
        okl4_arm_mpidr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint64_t)_aff3;
    *x = _conv.raw;
}
/*lint -esym(714, okl4_arm_mpidr_init) */
OKL4_FORCE_INLINE void
okl4_arm_mpidr_init(okl4_arm_mpidr_t *x)
{
    *x = (okl4_arm_mpidr_t)2147483648U;
}

/*lint -esym(714, okl4_arm_mpidr_cast) */
OKL4_FORCE_INLINE okl4_arm_mpidr_t
okl4_arm_mpidr_cast(uint64_t p, okl4_bool_t force)
{
    okl4_arm_mpidr_t x = (okl4_arm_mpidr_t)p;
    if (force) {
        x &= ~(okl4_arm_mpidr_t)0x80000000U;
        x |= (okl4_arm_mpidr_t)0x80000000U; /* x.mp */
    }
    return x;
}




/*lint -esym(621, OKL4_AXON_NUM_RECEIVE_QUEUES) */
#define OKL4_AXON_NUM_RECEIVE_QUEUES ((uint32_t)(4U))

/*lint -esym(621, OKL4_AXON_NUM_SEND_QUEUES) */
#define OKL4_AXON_NUM_SEND_QUEUES ((uint32_t)(4U))

/*lint -esym(621, _OKL4_POISON) */
#define _OKL4_POISON ((uint32_t)(3735928559U))

/*lint -esym(621, OKL4_TRACEBUFFER_INVALID_REF) */
#define OKL4_TRACEBUFFER_INVALID_REF ((uint32_t)(0xffffffffU))




typedef uint32_t okl4_arm_psci_function_t;

/*lint -esym(621, OKL4_ARM_PSCI_FUNCTION_PSCI_VERSION) */
#define OKL4_ARM_PSCI_FUNCTION_PSCI_VERSION ((okl4_arm_psci_function_t)0x0U)
/*lint -esym(621, OKL4_ARM_PSCI_FUNCTION_CPU_SUSPEND) */
#define OKL4_ARM_PSCI_FUNCTION_CPU_SUSPEND ((okl4_arm_psci_function_t)0x1U)
/*lint -esym(621, OKL4_ARM_PSCI_FUNCTION_CPU_OFF) */
#define OKL4_ARM_PSCI_FUNCTION_CPU_OFF ((okl4_arm_psci_function_t)0x2U)
/*lint -esym(621, OKL4_ARM_PSCI_FUNCTION_CPU_ON) */
#define OKL4_ARM_PSCI_FUNCTION_CPU_ON ((okl4_arm_psci_function_t)0x3U)
/*lint -esym(621, OKL4_ARM_PSCI_FUNCTION_AFFINITY_INFO) */
#define OKL4_ARM_PSCI_FUNCTION_AFFINITY_INFO ((okl4_arm_psci_function_t)0x4U)
/*lint -esym(621, OKL4_ARM_PSCI_FUNCTION_MIGRATE) */
#define OKL4_ARM_PSCI_FUNCTION_MIGRATE ((okl4_arm_psci_function_t)0x5U)
/*lint -esym(621, OKL4_ARM_PSCI_FUNCTION_MIGRATE_INFO_TYPE) */
#define OKL4_ARM_PSCI_FUNCTION_MIGRATE_INFO_TYPE ((okl4_arm_psci_function_t)0x6U)
/*lint -esym(621, OKL4_ARM_PSCI_FUNCTION_MIGRATE_INFO_UP_CPU) */
#define OKL4_ARM_PSCI_FUNCTION_MIGRATE_INFO_UP_CPU ((okl4_arm_psci_function_t)0x7U)
/*lint -esym(621, OKL4_ARM_PSCI_FUNCTION_SYSTEM_OFF) */
#define OKL4_ARM_PSCI_FUNCTION_SYSTEM_OFF ((okl4_arm_psci_function_t)0x8U)
/*lint -esym(621, OKL4_ARM_PSCI_FUNCTION_SYSTEM_RESET) */
#define OKL4_ARM_PSCI_FUNCTION_SYSTEM_RESET ((okl4_arm_psci_function_t)0x9U)
/*lint -esym(621, OKL4_ARM_PSCI_FUNCTION_PSCI_FEATURES) */
#define OKL4_ARM_PSCI_FUNCTION_PSCI_FEATURES ((okl4_arm_psci_function_t)0xaU)
/*lint -esym(621, OKL4_ARM_PSCI_FUNCTION_CPU_FREEZE) */
#define OKL4_ARM_PSCI_FUNCTION_CPU_FREEZE ((okl4_arm_psci_function_t)0xbU)
/*lint -esym(621, OKL4_ARM_PSCI_FUNCTION_CPU_DEFAULT_SUSPEND) */
#define OKL4_ARM_PSCI_FUNCTION_CPU_DEFAULT_SUSPEND ((okl4_arm_psci_function_t)0xcU)
/*lint -esym(621, OKL4_ARM_PSCI_FUNCTION_NODE_HW_STATE) */
#define OKL4_ARM_PSCI_FUNCTION_NODE_HW_STATE ((okl4_arm_psci_function_t)0xdU)
/*lint -esym(621, OKL4_ARM_PSCI_FUNCTION_SYSTEM_SUSPEND) */
#define OKL4_ARM_PSCI_FUNCTION_SYSTEM_SUSPEND ((okl4_arm_psci_function_t)0xeU)
/*lint -esym(621, OKL4_ARM_PSCI_FUNCTION_PSCI_SET_SUSPEND_MODE) */
#define OKL4_ARM_PSCI_FUNCTION_PSCI_SET_SUSPEND_MODE ((okl4_arm_psci_function_t)0xfU)
/*lint -esym(621, OKL4_ARM_PSCI_FUNCTION_PSCI_STAT_RESIDENCY) */
#define OKL4_ARM_PSCI_FUNCTION_PSCI_STAT_RESIDENCY ((okl4_arm_psci_function_t)0x10U)
/*lint -esym(621, OKL4_ARM_PSCI_FUNCTION_PSCI_STAT_COUNT) */
#define OKL4_ARM_PSCI_FUNCTION_PSCI_STAT_COUNT ((okl4_arm_psci_function_t)0x11U)

/*lint -esym(714, okl4_arm_psci_function_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_psci_function_is_element_of(okl4_arm_psci_function_t var);


/*lint -esym(714, okl4_arm_psci_function_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_psci_function_is_element_of(okl4_arm_psci_function_t var)
{
    /*lint --e{944} Disable dead expression detection */
    /*lint --e{948} --e{845} Disable constant always zero */
    return ((var == OKL4_ARM_PSCI_FUNCTION_PSCI_VERSION) ||
            (var == OKL4_ARM_PSCI_FUNCTION_CPU_SUSPEND) ||
            (var == OKL4_ARM_PSCI_FUNCTION_CPU_OFF) ||
            (var == OKL4_ARM_PSCI_FUNCTION_CPU_ON) ||
            (var == OKL4_ARM_PSCI_FUNCTION_AFFINITY_INFO) ||
            (var == OKL4_ARM_PSCI_FUNCTION_MIGRATE) ||
            (var == OKL4_ARM_PSCI_FUNCTION_MIGRATE_INFO_TYPE) ||
            (var == OKL4_ARM_PSCI_FUNCTION_MIGRATE_INFO_UP_CPU) ||
            (var == OKL4_ARM_PSCI_FUNCTION_SYSTEM_OFF) ||
            (var == OKL4_ARM_PSCI_FUNCTION_SYSTEM_RESET) ||
            (var == OKL4_ARM_PSCI_FUNCTION_PSCI_FEATURES) ||
            (var == OKL4_ARM_PSCI_FUNCTION_CPU_FREEZE) ||
            (var == OKL4_ARM_PSCI_FUNCTION_CPU_DEFAULT_SUSPEND) ||
            (var == OKL4_ARM_PSCI_FUNCTION_NODE_HW_STATE) ||
            (var == OKL4_ARM_PSCI_FUNCTION_SYSTEM_SUSPEND) ||
            (var == OKL4_ARM_PSCI_FUNCTION_PSCI_SET_SUSPEND_MODE) ||
            (var == OKL4_ARM_PSCI_FUNCTION_PSCI_STAT_RESIDENCY) ||
            (var == OKL4_ARM_PSCI_FUNCTION_PSCI_STAT_COUNT));
}



typedef uint32_t okl4_arm_psci_result_t;

/*lint -esym(621, OKL4_ARM_PSCI_RESULT_SUCCESS) */
#define OKL4_ARM_PSCI_RESULT_SUCCESS ((okl4_arm_psci_result_t)0x0U)
/*lint -esym(621, OKL4_ARM_PSCI_RESULT_INVALID_ADDRESS) */
#define OKL4_ARM_PSCI_RESULT_INVALID_ADDRESS ((okl4_arm_psci_result_t)0xfffffff7U)
/*lint -esym(621, OKL4_ARM_PSCI_RESULT_DISABLED) */
#define OKL4_ARM_PSCI_RESULT_DISABLED ((okl4_arm_psci_result_t)0xfffffff8U)
/*lint -esym(621, OKL4_ARM_PSCI_RESULT_NOT_PRESENT) */
#define OKL4_ARM_PSCI_RESULT_NOT_PRESENT ((okl4_arm_psci_result_t)0xfffffff9U)
/*lint -esym(621, OKL4_ARM_PSCI_RESULT_INTERNAL_FAILURE) */
#define OKL4_ARM_PSCI_RESULT_INTERNAL_FAILURE ((okl4_arm_psci_result_t)0xfffffffaU)
/*lint -esym(621, OKL4_ARM_PSCI_RESULT_ON_PENDING) */
#define OKL4_ARM_PSCI_RESULT_ON_PENDING ((okl4_arm_psci_result_t)0xfffffffbU)
/*lint -esym(621, OKL4_ARM_PSCI_RESULT_ALREADY_ON) */
#define OKL4_ARM_PSCI_RESULT_ALREADY_ON ((okl4_arm_psci_result_t)0xfffffffcU)
/*lint -esym(621, OKL4_ARM_PSCI_RESULT_DENIED) */
#define OKL4_ARM_PSCI_RESULT_DENIED ((okl4_arm_psci_result_t)0xfffffffdU)
/*lint -esym(621, OKL4_ARM_PSCI_RESULT_INVALID_PARAMETERS) */
#define OKL4_ARM_PSCI_RESULT_INVALID_PARAMETERS ((okl4_arm_psci_result_t)0xfffffffeU)
/*lint -esym(621, OKL4_ARM_PSCI_RESULT_NOT_SUPPORTED) */
#define OKL4_ARM_PSCI_RESULT_NOT_SUPPORTED ((okl4_arm_psci_result_t)0xffffffffU)

/*lint -esym(714, okl4_arm_psci_result_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_psci_result_is_element_of(okl4_arm_psci_result_t var);


/*lint -esym(714, okl4_arm_psci_result_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_psci_result_is_element_of(okl4_arm_psci_result_t var)
{
    /*lint --e{944} Disable dead expression detection */
    /*lint --e{948} --e{845} Disable constant always zero */
    return ((var == OKL4_ARM_PSCI_RESULT_SUCCESS) ||
            (var == OKL4_ARM_PSCI_RESULT_NOT_SUPPORTED) ||
            (var == OKL4_ARM_PSCI_RESULT_INVALID_PARAMETERS) ||
            (var == OKL4_ARM_PSCI_RESULT_DENIED) ||
            (var == OKL4_ARM_PSCI_RESULT_ALREADY_ON) ||
            (var == OKL4_ARM_PSCI_RESULT_ON_PENDING) ||
            (var == OKL4_ARM_PSCI_RESULT_INTERNAL_FAILURE) ||
            (var == OKL4_ARM_PSCI_RESULT_NOT_PRESENT) ||
            (var == OKL4_ARM_PSCI_RESULT_DISABLED) ||
            (var == OKL4_ARM_PSCI_RESULT_INVALID_ADDRESS));
}


/**
    - BITS 15..0 -   @ref OKL4_MASK_STATE_ID_ARM_PSCI_SUSPEND_STATE
    - BIT 16 -   @ref OKL4_MASK_POWER_DOWN_ARM_PSCI_SUSPEND_STATE
    - BITS 25..24 -   @ref OKL4_MASK_POWER_LEVEL_ARM_PSCI_SUSPEND_STATE
*/

/*lint -esym(621, okl4_arm_psci_suspend_state_t) */
typedef uint32_t okl4_arm_psci_suspend_state_t;

/*lint -esym(621, okl4_arm_psci_suspend_state_getstateid) */
/*lint -esym(714, okl4_arm_psci_suspend_state_getstateid) */
OKL4_FORCE_INLINE uint32_t
okl4_arm_psci_suspend_state_getstateid(const okl4_arm_psci_suspend_state_t *x);

/*lint -esym(621, okl4_arm_psci_suspend_state_setstateid) */
OKL4_FORCE_INLINE void
okl4_arm_psci_suspend_state_setstateid(okl4_arm_psci_suspend_state_t *x, uint32_t _state_id);

/*lint -esym(621, okl4_arm_psci_suspend_state_getpowerdown) */
/*lint -esym(714, okl4_arm_psci_suspend_state_getpowerdown) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_psci_suspend_state_getpowerdown(const okl4_arm_psci_suspend_state_t *x);

/*lint -esym(621, okl4_arm_psci_suspend_state_setpowerdown) */
OKL4_FORCE_INLINE void
okl4_arm_psci_suspend_state_setpowerdown(okl4_arm_psci_suspend_state_t *x, okl4_bool_t _power_down);

/*lint -esym(621, okl4_arm_psci_suspend_state_getpowerlevel) */
/*lint -esym(714, okl4_arm_psci_suspend_state_getpowerlevel) */
OKL4_FORCE_INLINE uint32_t
okl4_arm_psci_suspend_state_getpowerlevel(const okl4_arm_psci_suspend_state_t *x);

/*lint -esym(621, okl4_arm_psci_suspend_state_setpowerlevel) */
OKL4_FORCE_INLINE void
okl4_arm_psci_suspend_state_setpowerlevel(okl4_arm_psci_suspend_state_t *x, uint32_t _power_level);

/*lint -esym(714, okl4_arm_psci_suspend_state_init) */
OKL4_FORCE_INLINE void
okl4_arm_psci_suspend_state_init(okl4_arm_psci_suspend_state_t *x);

/*lint -esym(714, okl4_arm_psci_suspend_state_cast) */
OKL4_FORCE_INLINE okl4_arm_psci_suspend_state_t
okl4_arm_psci_suspend_state_cast(uint32_t p, okl4_bool_t force);



/*lint -esym(621, OKL4_ARM_PSCI_POWER_LEVEL_CPU) */
#define OKL4_ARM_PSCI_POWER_LEVEL_CPU ((okl4_arm_psci_suspend_state_t)(0U))

/*lint -esym(621, OKL4_ARM_PSCI_SUSPEND_STATE_STATE_ID_MASK) */
#define OKL4_ARM_PSCI_SUSPEND_STATE_STATE_ID_MASK ((okl4_arm_psci_suspend_state_t)65535U) /* Deprecated */
/*lint -esym(621, OKL4_MASK_STATE_ID_ARM_PSCI_SUSPEND_STATE) */
#define OKL4_MASK_STATE_ID_ARM_PSCI_SUSPEND_STATE ((okl4_arm_psci_suspend_state_t)65535U)
/*lint -esym(621, OKL4_SHIFT_STATE_ID_ARM_PSCI_SUSPEND_STATE) */
#define OKL4_SHIFT_STATE_ID_ARM_PSCI_SUSPEND_STATE (0)
/*lint -esym(621, OKL4_WIDTH_STATE_ID_ARM_PSCI_SUSPEND_STATE) */
#define OKL4_WIDTH_STATE_ID_ARM_PSCI_SUSPEND_STATE (16)
/*lint -esym(621, OKL4_ARM_PSCI_SUSPEND_STATE_POWER_DOWN_MASK) */
#define OKL4_ARM_PSCI_SUSPEND_STATE_POWER_DOWN_MASK ((okl4_arm_psci_suspend_state_t)1U << 16) /* Deprecated */
/*lint -esym(621, OKL4_MASK_POWER_DOWN_ARM_PSCI_SUSPEND_STATE) */
#define OKL4_MASK_POWER_DOWN_ARM_PSCI_SUSPEND_STATE ((okl4_arm_psci_suspend_state_t)1U << 16)
/*lint -esym(621, OKL4_SHIFT_POWER_DOWN_ARM_PSCI_SUSPEND_STATE) */
#define OKL4_SHIFT_POWER_DOWN_ARM_PSCI_SUSPEND_STATE (16)
/*lint -esym(621, OKL4_WIDTH_POWER_DOWN_ARM_PSCI_SUSPEND_STATE) */
#define OKL4_WIDTH_POWER_DOWN_ARM_PSCI_SUSPEND_STATE (1)
/*lint -esym(621, OKL4_ARM_PSCI_SUSPEND_STATE_POWER_LEVEL_MASK) */
#define OKL4_ARM_PSCI_SUSPEND_STATE_POWER_LEVEL_MASK ((okl4_arm_psci_suspend_state_t)3U << 24) /* Deprecated */
/*lint -esym(621, OKL4_MASK_POWER_LEVEL_ARM_PSCI_SUSPEND_STATE) */
#define OKL4_MASK_POWER_LEVEL_ARM_PSCI_SUSPEND_STATE ((okl4_arm_psci_suspend_state_t)3U << 24)
/*lint -esym(621, OKL4_SHIFT_POWER_LEVEL_ARM_PSCI_SUSPEND_STATE) */
#define OKL4_SHIFT_POWER_LEVEL_ARM_PSCI_SUSPEND_STATE (24)
/*lint -esym(621, OKL4_WIDTH_POWER_LEVEL_ARM_PSCI_SUSPEND_STATE) */
#define OKL4_WIDTH_POWER_LEVEL_ARM_PSCI_SUSPEND_STATE (2)


/*lint -sem(okl4_arm_psci_suspend_state_getstateid, 1p, @n >= 0 && @n <= 65535) */
/*lint -esym(621, okl4_arm_psci_suspend_state_getstateid) */
/*lint -esym(714, okl4_arm_psci_suspend_state_getstateid) */
OKL4_FORCE_INLINE uint32_t
okl4_arm_psci_suspend_state_getstateid(const okl4_arm_psci_suspend_state_t *x)
{
    uint32_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t field : 16;
        } bits;
        okl4_arm_psci_suspend_state_t raw;
    } _conv;

    _conv.raw = *x;
    field = (uint32_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_psci_suspend_state_setstateid, 2n >= 0 && 2n <= 65535) */
/*lint -esym(714, okl4_arm_psci_suspend_state_setstateid) */

/*lint -esym(621, okl4_arm_psci_suspend_state_setstateid) */
OKL4_FORCE_INLINE void
okl4_arm_psci_suspend_state_setstateid(okl4_arm_psci_suspend_state_t *x, uint32_t _state_id)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t field : 16;
        } bits;
        okl4_arm_psci_suspend_state_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_state_id;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_psci_suspend_state_getpowerdown, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_psci_suspend_state_getpowerdown) */
/*lint -esym(714, okl4_arm_psci_suspend_state_getpowerdown) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_psci_suspend_state_getpowerdown(const okl4_arm_psci_suspend_state_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 16;
            _Bool field : 1;
        } bits;
        okl4_arm_psci_suspend_state_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_psci_suspend_state_setpowerdown, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_psci_suspend_state_setpowerdown) */

/*lint -esym(621, okl4_arm_psci_suspend_state_setpowerdown) */
OKL4_FORCE_INLINE void
okl4_arm_psci_suspend_state_setpowerdown(okl4_arm_psci_suspend_state_t *x, okl4_bool_t _power_down)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 16;
            _Bool field : 1;
        } bits;
        okl4_arm_psci_suspend_state_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_power_down;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_psci_suspend_state_getpowerlevel, 1p, @n >= 0 && @n <= 3) */
/*lint -esym(621, okl4_arm_psci_suspend_state_getpowerlevel) */
/*lint -esym(714, okl4_arm_psci_suspend_state_getpowerlevel) */
OKL4_FORCE_INLINE uint32_t
okl4_arm_psci_suspend_state_getpowerlevel(const okl4_arm_psci_suspend_state_t *x)
{
    uint32_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 24;
            uint32_t field : 2;
        } bits;
        okl4_arm_psci_suspend_state_t raw;
    } _conv;

    _conv.raw = *x;
    field = (uint32_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_psci_suspend_state_setpowerlevel, 2n >= 0 && 2n <= 3) */
/*lint -esym(714, okl4_arm_psci_suspend_state_setpowerlevel) */

/*lint -esym(621, okl4_arm_psci_suspend_state_setpowerlevel) */
OKL4_FORCE_INLINE void
okl4_arm_psci_suspend_state_setpowerlevel(okl4_arm_psci_suspend_state_t *x, uint32_t _power_level)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 24;
            uint32_t field : 2;
        } bits;
        okl4_arm_psci_suspend_state_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_power_level;
    *x = _conv.raw;
}
/*lint -esym(714, okl4_arm_psci_suspend_state_init) */
OKL4_FORCE_INLINE void
okl4_arm_psci_suspend_state_init(okl4_arm_psci_suspend_state_t *x)
{
    *x = (okl4_arm_psci_suspend_state_t)0U;
}

/*lint -esym(714, okl4_arm_psci_suspend_state_cast) */
OKL4_FORCE_INLINE okl4_arm_psci_suspend_state_t
okl4_arm_psci_suspend_state_cast(uint32_t p, okl4_bool_t force)
{
    okl4_arm_psci_suspend_state_t x = (okl4_arm_psci_suspend_state_t)p;
    (void)force;
    return x;
}



/**
    - BIT 0 -   @ref OKL4_MASK_MMU_ENABLE_ARM_SCTLR
    - BIT 1 -   @ref OKL4_MASK_ALIGNMENT_CHECK_ENABLE_ARM_SCTLR
    - BIT 2 -   @ref OKL4_MASK_DATA_CACHE_ENABLE_ARM_SCTLR
    - BIT 3 -   @ref OKL4_MASK_STACK_ALIGN_ARM_SCTLR
    - BIT 4 -   @ref OKL4_MASK_STACK_ALIGN_EL0_ARM_SCTLR
    - BIT 5 -   @ref OKL4_MASK_CP15_BARRIER_ENABLE_ARM_SCTLR
    - BIT 6 -   @ref OKL4_MASK_OKL_HCR_EL2_DC_ARM_SCTLR
    - BIT 7 -   @ref OKL4_MASK_IT_DISABLE_ARM_SCTLR
    - BIT 8 -   @ref OKL4_MASK_SETEND_DISABLE_ARM_SCTLR
    - BIT 9 -   @ref OKL4_MASK_USER_MASK_ACCESS_ARM_SCTLR
    - BIT 11 -   @ref OKL4_MASK_RESERVED11_ARM_SCTLR
    - BIT 12 -   @ref OKL4_MASK_INSTRUCTION_CACHE_ENABLE_ARM_SCTLR
    - BIT 13 -   @ref OKL4_MASK_VECTORS_BIT_ARM_SCTLR
    - BIT 14 -   @ref OKL4_MASK_DCACHE_ZERO_ARM_SCTLR
    - BIT 15 -   @ref OKL4_MASK_USER_CACHE_TYPE_ARM_SCTLR
    - BIT 16 -   @ref OKL4_MASK_NO_TRAP_WFI_ARM_SCTLR
    - BIT 18 -   @ref OKL4_MASK_NO_TRAP_WFE_ARM_SCTLR
    - BIT 19 -   @ref OKL4_MASK_WRITE_EXEC_NEVER_ARM_SCTLR
    - BIT 20 -   @ref OKL4_MASK_USER_WRITE_EXEC_NEVER_ARM_SCTLR
    - BIT 22 -   @ref OKL4_MASK_RESERVED22_ARM_SCTLR
    - BIT 23 -   @ref OKL4_MASK_RESERVED23_ARM_SCTLR
    - BIT 24 -   @ref OKL4_MASK_EL0_ENDIANNESS_ARM_SCTLR
    - BIT 25 -   @ref OKL4_MASK_EXCEPTION_ENDIANNESS_ARM_SCTLR
    - BIT 28 -   @ref OKL4_MASK_TEX_REMAP_ENABLE_ARM_SCTLR
    - BIT 29 -   @ref OKL4_MASK_ACCESS_FLAG_ENABLE_ARM_SCTLR
    - BIT 30 -   @ref OKL4_MASK_THUMB_EXCEPTION_ENABLE_ARM_SCTLR
*/

/*lint -esym(621, okl4_arm_sctlr_t) */
typedef uint32_t okl4_arm_sctlr_t;

/*lint -esym(621, okl4_arm_sctlr_getmmuenable) */
/*lint -esym(714, okl4_arm_sctlr_getmmuenable) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getmmuenable(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_setmmuenable) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setmmuenable(okl4_arm_sctlr_t *x, okl4_bool_t _mmu_enable);

/*lint -esym(621, okl4_arm_sctlr_getalignmentcheckenable) */
/*lint -esym(714, okl4_arm_sctlr_getalignmentcheckenable) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getalignmentcheckenable(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_setalignmentcheckenable) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setalignmentcheckenable(okl4_arm_sctlr_t *x, okl4_bool_t _alignment_check_enable);

/*lint -esym(621, okl4_arm_sctlr_getdatacacheenable) */
/*lint -esym(714, okl4_arm_sctlr_getdatacacheenable) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getdatacacheenable(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_setdatacacheenable) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setdatacacheenable(okl4_arm_sctlr_t *x, okl4_bool_t _data_cache_enable);

/*lint -esym(621, okl4_arm_sctlr_getinstructioncacheenable) */
/*lint -esym(714, okl4_arm_sctlr_getinstructioncacheenable) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getinstructioncacheenable(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_setinstructioncacheenable) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setinstructioncacheenable(okl4_arm_sctlr_t *x, okl4_bool_t _instruction_cache_enable);

/*lint -esym(621, okl4_arm_sctlr_getcp15barrierenable) */
/*lint -esym(714, okl4_arm_sctlr_getcp15barrierenable) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getcp15barrierenable(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_setcp15barrierenable) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setcp15barrierenable(okl4_arm_sctlr_t *x, okl4_bool_t _cp15_barrier_enable);

/*lint -esym(621, okl4_arm_sctlr_getitdisable) */
/*lint -esym(714, okl4_arm_sctlr_getitdisable) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getitdisable(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_setitdisable) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setitdisable(okl4_arm_sctlr_t *x, okl4_bool_t _it_disable);

/*lint -esym(621, okl4_arm_sctlr_getsetenddisable) */
/*lint -esym(714, okl4_arm_sctlr_getsetenddisable) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getsetenddisable(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_setsetenddisable) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setsetenddisable(okl4_arm_sctlr_t *x, okl4_bool_t _setend_disable);

/*lint -esym(621, okl4_arm_sctlr_getreserved11) */
/*lint -esym(714, okl4_arm_sctlr_getreserved11) */
OKL4_FORCE_INLINE uint32_t
okl4_arm_sctlr_getreserved11(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_getnotrapwfi) */
/*lint -esym(714, okl4_arm_sctlr_getnotrapwfi) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getnotrapwfi(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_setnotrapwfi) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setnotrapwfi(okl4_arm_sctlr_t *x, okl4_bool_t _no_trap_wfi);

/*lint -esym(621, okl4_arm_sctlr_getnotrapwfe) */
/*lint -esym(714, okl4_arm_sctlr_getnotrapwfe) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getnotrapwfe(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_setnotrapwfe) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setnotrapwfe(okl4_arm_sctlr_t *x, okl4_bool_t _no_trap_wfe);

/*lint -esym(621, okl4_arm_sctlr_getwriteexecnever) */
/*lint -esym(714, okl4_arm_sctlr_getwriteexecnever) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getwriteexecnever(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_setwriteexecnever) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setwriteexecnever(okl4_arm_sctlr_t *x, okl4_bool_t _write_exec_never);

/*lint -esym(621, okl4_arm_sctlr_getreserved22) */
/*lint -esym(714, okl4_arm_sctlr_getreserved22) */
OKL4_FORCE_INLINE uint32_t
okl4_arm_sctlr_getreserved22(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_getreserved23) */
/*lint -esym(714, okl4_arm_sctlr_getreserved23) */
OKL4_FORCE_INLINE uint32_t
okl4_arm_sctlr_getreserved23(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_getel0endianness) */
/*lint -esym(714, okl4_arm_sctlr_getel0endianness) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getel0endianness(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_setel0endianness) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setel0endianness(okl4_arm_sctlr_t *x, okl4_bool_t _el0_endianness);

/*lint -esym(621, okl4_arm_sctlr_getexceptionendianness) */
/*lint -esym(714, okl4_arm_sctlr_getexceptionendianness) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getexceptionendianness(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_setexceptionendianness) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setexceptionendianness(okl4_arm_sctlr_t *x, okl4_bool_t _exception_endianness);

/*lint -esym(621, okl4_arm_sctlr_getvectorsbit) */
/*lint -esym(714, okl4_arm_sctlr_getvectorsbit) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getvectorsbit(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_setvectorsbit) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setvectorsbit(okl4_arm_sctlr_t *x, okl4_bool_t _vectors_bit);

/*lint -esym(621, okl4_arm_sctlr_getuserwriteexecnever) */
/*lint -esym(714, okl4_arm_sctlr_getuserwriteexecnever) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getuserwriteexecnever(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_setuserwriteexecnever) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setuserwriteexecnever(okl4_arm_sctlr_t *x, okl4_bool_t _user_write_exec_never);

/*lint -esym(621, okl4_arm_sctlr_gettexremapenable) */
/*lint -esym(714, okl4_arm_sctlr_gettexremapenable) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_gettexremapenable(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_settexremapenable) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_settexremapenable(okl4_arm_sctlr_t *x, okl4_bool_t _tex_remap_enable);

/*lint -esym(621, okl4_arm_sctlr_getaccessflagenable) */
/*lint -esym(714, okl4_arm_sctlr_getaccessflagenable) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getaccessflagenable(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_setaccessflagenable) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setaccessflagenable(okl4_arm_sctlr_t *x, okl4_bool_t _access_flag_enable);

/*lint -esym(621, okl4_arm_sctlr_getthumbexceptionenable) */
/*lint -esym(714, okl4_arm_sctlr_getthumbexceptionenable) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getthumbexceptionenable(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_setthumbexceptionenable) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setthumbexceptionenable(okl4_arm_sctlr_t *x, okl4_bool_t _thumb_exception_enable);

/*lint -esym(621, okl4_arm_sctlr_getstackalign) */
/*lint -esym(714, okl4_arm_sctlr_getstackalign) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getstackalign(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_setstackalign) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setstackalign(okl4_arm_sctlr_t *x, okl4_bool_t _stack_align);

/*lint -esym(621, okl4_arm_sctlr_getstackalignel0) */
/*lint -esym(714, okl4_arm_sctlr_getstackalignel0) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getstackalignel0(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_setstackalignel0) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setstackalignel0(okl4_arm_sctlr_t *x, okl4_bool_t _stack_align_el0);

/*lint -esym(621, okl4_arm_sctlr_getusermaskaccess) */
/*lint -esym(714, okl4_arm_sctlr_getusermaskaccess) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getusermaskaccess(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_setusermaskaccess) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setusermaskaccess(okl4_arm_sctlr_t *x, okl4_bool_t _user_mask_access);

/*lint -esym(621, okl4_arm_sctlr_getdcachezero) */
/*lint -esym(714, okl4_arm_sctlr_getdcachezero) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getdcachezero(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_setdcachezero) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setdcachezero(okl4_arm_sctlr_t *x, okl4_bool_t _dcache_zero);

/*lint -esym(621, okl4_arm_sctlr_getusercachetype) */
/*lint -esym(714, okl4_arm_sctlr_getusercachetype) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getusercachetype(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_setusercachetype) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setusercachetype(okl4_arm_sctlr_t *x, okl4_bool_t _user_cache_type);

/*lint -esym(621, okl4_arm_sctlr_getoklhcrel2dc) */
/*lint -esym(714, okl4_arm_sctlr_getoklhcrel2dc) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getoklhcrel2dc(const okl4_arm_sctlr_t *x);

/*lint -esym(621, okl4_arm_sctlr_setoklhcrel2dc) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setoklhcrel2dc(okl4_arm_sctlr_t *x, okl4_bool_t _okl_hcr_el2_dc);

/*lint -esym(714, okl4_arm_sctlr_init) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_init(okl4_arm_sctlr_t *x);

/*lint -esym(714, okl4_arm_sctlr_cast) */
OKL4_FORCE_INLINE okl4_arm_sctlr_t
okl4_arm_sctlr_cast(uint32_t p, okl4_bool_t force);




/*lint -esym(621, OKL4_ARM_SCTLR_MMU_ENABLE_MASK) */
#define OKL4_ARM_SCTLR_MMU_ENABLE_MASK ((okl4_arm_sctlr_t)1U) /* Deprecated */
/*lint -esym(621, OKL4_MASK_MMU_ENABLE_ARM_SCTLR) */
#define OKL4_MASK_MMU_ENABLE_ARM_SCTLR ((okl4_arm_sctlr_t)1U)
/*lint -esym(621, OKL4_SHIFT_MMU_ENABLE_ARM_SCTLR) */
#define OKL4_SHIFT_MMU_ENABLE_ARM_SCTLR (0)
/*lint -esym(621, OKL4_WIDTH_MMU_ENABLE_ARM_SCTLR) */
#define OKL4_WIDTH_MMU_ENABLE_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ARM_SCTLR_ALIGNMENT_CHECK_ENABLE_MASK) */
#define OKL4_ARM_SCTLR_ALIGNMENT_CHECK_ENABLE_MASK ((okl4_arm_sctlr_t)1U << 1) /* Deprecated */
/*lint -esym(621, OKL4_MASK_ALIGNMENT_CHECK_ENABLE_ARM_SCTLR) */
#define OKL4_MASK_ALIGNMENT_CHECK_ENABLE_ARM_SCTLR ((okl4_arm_sctlr_t)1U << 1)
/*lint -esym(621, OKL4_SHIFT_ALIGNMENT_CHECK_ENABLE_ARM_SCTLR) */
#define OKL4_SHIFT_ALIGNMENT_CHECK_ENABLE_ARM_SCTLR (1)
/*lint -esym(621, OKL4_WIDTH_ALIGNMENT_CHECK_ENABLE_ARM_SCTLR) */
#define OKL4_WIDTH_ALIGNMENT_CHECK_ENABLE_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ARM_SCTLR_DATA_CACHE_ENABLE_MASK) */
#define OKL4_ARM_SCTLR_DATA_CACHE_ENABLE_MASK ((okl4_arm_sctlr_t)1U << 2) /* Deprecated */
/*lint -esym(621, OKL4_MASK_DATA_CACHE_ENABLE_ARM_SCTLR) */
#define OKL4_MASK_DATA_CACHE_ENABLE_ARM_SCTLR ((okl4_arm_sctlr_t)1U << 2)
/*lint -esym(621, OKL4_SHIFT_DATA_CACHE_ENABLE_ARM_SCTLR) */
#define OKL4_SHIFT_DATA_CACHE_ENABLE_ARM_SCTLR (2)
/*lint -esym(621, OKL4_WIDTH_DATA_CACHE_ENABLE_ARM_SCTLR) */
#define OKL4_WIDTH_DATA_CACHE_ENABLE_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ARM_SCTLR_STACK_ALIGN_MASK) */
#define OKL4_ARM_SCTLR_STACK_ALIGN_MASK ((okl4_arm_sctlr_t)1U << 3) /* Deprecated */
/*lint -esym(621, OKL4_MASK_STACK_ALIGN_ARM_SCTLR) */
#define OKL4_MASK_STACK_ALIGN_ARM_SCTLR ((okl4_arm_sctlr_t)1U << 3)
/*lint -esym(621, OKL4_SHIFT_STACK_ALIGN_ARM_SCTLR) */
#define OKL4_SHIFT_STACK_ALIGN_ARM_SCTLR (3)
/*lint -esym(621, OKL4_WIDTH_STACK_ALIGN_ARM_SCTLR) */
#define OKL4_WIDTH_STACK_ALIGN_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ARM_SCTLR_STACK_ALIGN_EL0_MASK) */
#define OKL4_ARM_SCTLR_STACK_ALIGN_EL0_MASK ((okl4_arm_sctlr_t)1U << 4) /* Deprecated */
/*lint -esym(621, OKL4_MASK_STACK_ALIGN_EL0_ARM_SCTLR) */
#define OKL4_MASK_STACK_ALIGN_EL0_ARM_SCTLR ((okl4_arm_sctlr_t)1U << 4)
/*lint -esym(621, OKL4_SHIFT_STACK_ALIGN_EL0_ARM_SCTLR) */
#define OKL4_SHIFT_STACK_ALIGN_EL0_ARM_SCTLR (4)
/*lint -esym(621, OKL4_WIDTH_STACK_ALIGN_EL0_ARM_SCTLR) */
#define OKL4_WIDTH_STACK_ALIGN_EL0_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ARM_SCTLR_CP15_BARRIER_ENABLE_MASK) */
#define OKL4_ARM_SCTLR_CP15_BARRIER_ENABLE_MASK ((okl4_arm_sctlr_t)1U << 5) /* Deprecated */
/*lint -esym(621, OKL4_MASK_CP15_BARRIER_ENABLE_ARM_SCTLR) */
#define OKL4_MASK_CP15_BARRIER_ENABLE_ARM_SCTLR ((okl4_arm_sctlr_t)1U << 5)
/*lint -esym(621, OKL4_SHIFT_CP15_BARRIER_ENABLE_ARM_SCTLR) */
#define OKL4_SHIFT_CP15_BARRIER_ENABLE_ARM_SCTLR (5)
/*lint -esym(621, OKL4_WIDTH_CP15_BARRIER_ENABLE_ARM_SCTLR) */
#define OKL4_WIDTH_CP15_BARRIER_ENABLE_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ARM_SCTLR_OKL_HCR_EL2_DC_MASK) */
#define OKL4_ARM_SCTLR_OKL_HCR_EL2_DC_MASK ((okl4_arm_sctlr_t)1U << 6) /* Deprecated */
/*lint -esym(621, OKL4_MASK_OKL_HCR_EL2_DC_ARM_SCTLR) */
#define OKL4_MASK_OKL_HCR_EL2_DC_ARM_SCTLR ((okl4_arm_sctlr_t)1U << 6)
/*lint -esym(621, OKL4_SHIFT_OKL_HCR_EL2_DC_ARM_SCTLR) */
#define OKL4_SHIFT_OKL_HCR_EL2_DC_ARM_SCTLR (6)
/*lint -esym(621, OKL4_WIDTH_OKL_HCR_EL2_DC_ARM_SCTLR) */
#define OKL4_WIDTH_OKL_HCR_EL2_DC_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ARM_SCTLR_IT_DISABLE_MASK) */
#define OKL4_ARM_SCTLR_IT_DISABLE_MASK ((okl4_arm_sctlr_t)1U << 7) /* Deprecated */
/*lint -esym(621, OKL4_MASK_IT_DISABLE_ARM_SCTLR) */
#define OKL4_MASK_IT_DISABLE_ARM_SCTLR ((okl4_arm_sctlr_t)1U << 7)
/*lint -esym(621, OKL4_SHIFT_IT_DISABLE_ARM_SCTLR) */
#define OKL4_SHIFT_IT_DISABLE_ARM_SCTLR (7)
/*lint -esym(621, OKL4_WIDTH_IT_DISABLE_ARM_SCTLR) */
#define OKL4_WIDTH_IT_DISABLE_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ARM_SCTLR_SETEND_DISABLE_MASK) */
#define OKL4_ARM_SCTLR_SETEND_DISABLE_MASK ((okl4_arm_sctlr_t)1U << 8) /* Deprecated */
/*lint -esym(621, OKL4_MASK_SETEND_DISABLE_ARM_SCTLR) */
#define OKL4_MASK_SETEND_DISABLE_ARM_SCTLR ((okl4_arm_sctlr_t)1U << 8)
/*lint -esym(621, OKL4_SHIFT_SETEND_DISABLE_ARM_SCTLR) */
#define OKL4_SHIFT_SETEND_DISABLE_ARM_SCTLR (8)
/*lint -esym(621, OKL4_WIDTH_SETEND_DISABLE_ARM_SCTLR) */
#define OKL4_WIDTH_SETEND_DISABLE_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ARM_SCTLR_USER_MASK_ACCESS_MASK) */
#define OKL4_ARM_SCTLR_USER_MASK_ACCESS_MASK ((okl4_arm_sctlr_t)1U << 9) /* Deprecated */
/*lint -esym(621, OKL4_MASK_USER_MASK_ACCESS_ARM_SCTLR) */
#define OKL4_MASK_USER_MASK_ACCESS_ARM_SCTLR ((okl4_arm_sctlr_t)1U << 9)
/*lint -esym(621, OKL4_SHIFT_USER_MASK_ACCESS_ARM_SCTLR) */
#define OKL4_SHIFT_USER_MASK_ACCESS_ARM_SCTLR (9)
/*lint -esym(621, OKL4_WIDTH_USER_MASK_ACCESS_ARM_SCTLR) */
#define OKL4_WIDTH_USER_MASK_ACCESS_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ARM_SCTLR_RESERVED11_MASK) */
#define OKL4_ARM_SCTLR_RESERVED11_MASK ((okl4_arm_sctlr_t)1U << 11) /* Deprecated */
/*lint -esym(621, OKL4_MASK_RESERVED11_ARM_SCTLR) */
#define OKL4_MASK_RESERVED11_ARM_SCTLR ((okl4_arm_sctlr_t)1U << 11)
/*lint -esym(621, OKL4_SHIFT_RESERVED11_ARM_SCTLR) */
#define OKL4_SHIFT_RESERVED11_ARM_SCTLR (11)
/*lint -esym(621, OKL4_WIDTH_RESERVED11_ARM_SCTLR) */
#define OKL4_WIDTH_RESERVED11_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ARM_SCTLR_INSTRUCTION_CACHE_ENABLE_MASK) */
#define OKL4_ARM_SCTLR_INSTRUCTION_CACHE_ENABLE_MASK ((okl4_arm_sctlr_t)1U << 12) /* Deprecated */
/*lint -esym(621, OKL4_MASK_INSTRUCTION_CACHE_ENABLE_ARM_SCTLR) */
#define OKL4_MASK_INSTRUCTION_CACHE_ENABLE_ARM_SCTLR ((okl4_arm_sctlr_t)1U << 12)
/*lint -esym(621, OKL4_SHIFT_INSTRUCTION_CACHE_ENABLE_ARM_SCTLR) */
#define OKL4_SHIFT_INSTRUCTION_CACHE_ENABLE_ARM_SCTLR (12)
/*lint -esym(621, OKL4_WIDTH_INSTRUCTION_CACHE_ENABLE_ARM_SCTLR) */
#define OKL4_WIDTH_INSTRUCTION_CACHE_ENABLE_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ARM_SCTLR_VECTORS_BIT_MASK) */
#define OKL4_ARM_SCTLR_VECTORS_BIT_MASK ((okl4_arm_sctlr_t)1U << 13) /* Deprecated */
/*lint -esym(621, OKL4_MASK_VECTORS_BIT_ARM_SCTLR) */
#define OKL4_MASK_VECTORS_BIT_ARM_SCTLR ((okl4_arm_sctlr_t)1U << 13)
/*lint -esym(621, OKL4_SHIFT_VECTORS_BIT_ARM_SCTLR) */
#define OKL4_SHIFT_VECTORS_BIT_ARM_SCTLR (13)
/*lint -esym(621, OKL4_WIDTH_VECTORS_BIT_ARM_SCTLR) */
#define OKL4_WIDTH_VECTORS_BIT_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ARM_SCTLR_DCACHE_ZERO_MASK) */
#define OKL4_ARM_SCTLR_DCACHE_ZERO_MASK ((okl4_arm_sctlr_t)1U << 14) /* Deprecated */
/*lint -esym(621, OKL4_MASK_DCACHE_ZERO_ARM_SCTLR) */
#define OKL4_MASK_DCACHE_ZERO_ARM_SCTLR ((okl4_arm_sctlr_t)1U << 14)
/*lint -esym(621, OKL4_SHIFT_DCACHE_ZERO_ARM_SCTLR) */
#define OKL4_SHIFT_DCACHE_ZERO_ARM_SCTLR (14)
/*lint -esym(621, OKL4_WIDTH_DCACHE_ZERO_ARM_SCTLR) */
#define OKL4_WIDTH_DCACHE_ZERO_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ARM_SCTLR_USER_CACHE_TYPE_MASK) */
#define OKL4_ARM_SCTLR_USER_CACHE_TYPE_MASK ((okl4_arm_sctlr_t)1U << 15) /* Deprecated */
/*lint -esym(621, OKL4_MASK_USER_CACHE_TYPE_ARM_SCTLR) */
#define OKL4_MASK_USER_CACHE_TYPE_ARM_SCTLR ((okl4_arm_sctlr_t)1U << 15)
/*lint -esym(621, OKL4_SHIFT_USER_CACHE_TYPE_ARM_SCTLR) */
#define OKL4_SHIFT_USER_CACHE_TYPE_ARM_SCTLR (15)
/*lint -esym(621, OKL4_WIDTH_USER_CACHE_TYPE_ARM_SCTLR) */
#define OKL4_WIDTH_USER_CACHE_TYPE_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ARM_SCTLR_NO_TRAP_WFI_MASK) */
#define OKL4_ARM_SCTLR_NO_TRAP_WFI_MASK ((okl4_arm_sctlr_t)1U << 16) /* Deprecated */
/*lint -esym(621, OKL4_MASK_NO_TRAP_WFI_ARM_SCTLR) */
#define OKL4_MASK_NO_TRAP_WFI_ARM_SCTLR ((okl4_arm_sctlr_t)1U << 16)
/*lint -esym(621, OKL4_SHIFT_NO_TRAP_WFI_ARM_SCTLR) */
#define OKL4_SHIFT_NO_TRAP_WFI_ARM_SCTLR (16)
/*lint -esym(621, OKL4_WIDTH_NO_TRAP_WFI_ARM_SCTLR) */
#define OKL4_WIDTH_NO_TRAP_WFI_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ARM_SCTLR_NO_TRAP_WFE_MASK) */
#define OKL4_ARM_SCTLR_NO_TRAP_WFE_MASK ((okl4_arm_sctlr_t)1U << 18) /* Deprecated */
/*lint -esym(621, OKL4_MASK_NO_TRAP_WFE_ARM_SCTLR) */
#define OKL4_MASK_NO_TRAP_WFE_ARM_SCTLR ((okl4_arm_sctlr_t)1U << 18)
/*lint -esym(621, OKL4_SHIFT_NO_TRAP_WFE_ARM_SCTLR) */
#define OKL4_SHIFT_NO_TRAP_WFE_ARM_SCTLR (18)
/*lint -esym(621, OKL4_WIDTH_NO_TRAP_WFE_ARM_SCTLR) */
#define OKL4_WIDTH_NO_TRAP_WFE_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ARM_SCTLR_WRITE_EXEC_NEVER_MASK) */
#define OKL4_ARM_SCTLR_WRITE_EXEC_NEVER_MASK ((okl4_arm_sctlr_t)1U << 19) /* Deprecated */
/*lint -esym(621, OKL4_MASK_WRITE_EXEC_NEVER_ARM_SCTLR) */
#define OKL4_MASK_WRITE_EXEC_NEVER_ARM_SCTLR ((okl4_arm_sctlr_t)1U << 19)
/*lint -esym(621, OKL4_SHIFT_WRITE_EXEC_NEVER_ARM_SCTLR) */
#define OKL4_SHIFT_WRITE_EXEC_NEVER_ARM_SCTLR (19)
/*lint -esym(621, OKL4_WIDTH_WRITE_EXEC_NEVER_ARM_SCTLR) */
#define OKL4_WIDTH_WRITE_EXEC_NEVER_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ARM_SCTLR_USER_WRITE_EXEC_NEVER_MASK) */
#define OKL4_ARM_SCTLR_USER_WRITE_EXEC_NEVER_MASK ((okl4_arm_sctlr_t)1U << 20) /* Deprecated */
/*lint -esym(621, OKL4_MASK_USER_WRITE_EXEC_NEVER_ARM_SCTLR) */
#define OKL4_MASK_USER_WRITE_EXEC_NEVER_ARM_SCTLR ((okl4_arm_sctlr_t)1U << 20)
/*lint -esym(621, OKL4_SHIFT_USER_WRITE_EXEC_NEVER_ARM_SCTLR) */
#define OKL4_SHIFT_USER_WRITE_EXEC_NEVER_ARM_SCTLR (20)
/*lint -esym(621, OKL4_WIDTH_USER_WRITE_EXEC_NEVER_ARM_SCTLR) */
#define OKL4_WIDTH_USER_WRITE_EXEC_NEVER_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ARM_SCTLR_RESERVED22_MASK) */
#define OKL4_ARM_SCTLR_RESERVED22_MASK ((okl4_arm_sctlr_t)1U << 22) /* Deprecated */
/*lint -esym(621, OKL4_MASK_RESERVED22_ARM_SCTLR) */
#define OKL4_MASK_RESERVED22_ARM_SCTLR ((okl4_arm_sctlr_t)1U << 22)
/*lint -esym(621, OKL4_SHIFT_RESERVED22_ARM_SCTLR) */
#define OKL4_SHIFT_RESERVED22_ARM_SCTLR (22)
/*lint -esym(621, OKL4_WIDTH_RESERVED22_ARM_SCTLR) */
#define OKL4_WIDTH_RESERVED22_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ARM_SCTLR_RESERVED23_MASK) */
#define OKL4_ARM_SCTLR_RESERVED23_MASK ((okl4_arm_sctlr_t)1U << 23) /* Deprecated */
/*lint -esym(621, OKL4_MASK_RESERVED23_ARM_SCTLR) */
#define OKL4_MASK_RESERVED23_ARM_SCTLR ((okl4_arm_sctlr_t)1U << 23)
/*lint -esym(621, OKL4_SHIFT_RESERVED23_ARM_SCTLR) */
#define OKL4_SHIFT_RESERVED23_ARM_SCTLR (23)
/*lint -esym(621, OKL4_WIDTH_RESERVED23_ARM_SCTLR) */
#define OKL4_WIDTH_RESERVED23_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ARM_SCTLR_EL0_ENDIANNESS_MASK) */
#define OKL4_ARM_SCTLR_EL0_ENDIANNESS_MASK ((okl4_arm_sctlr_t)1U << 24) /* Deprecated */
/*lint -esym(621, OKL4_MASK_EL0_ENDIANNESS_ARM_SCTLR) */
#define OKL4_MASK_EL0_ENDIANNESS_ARM_SCTLR ((okl4_arm_sctlr_t)1U << 24)
/*lint -esym(621, OKL4_SHIFT_EL0_ENDIANNESS_ARM_SCTLR) */
#define OKL4_SHIFT_EL0_ENDIANNESS_ARM_SCTLR (24)
/*lint -esym(621, OKL4_WIDTH_EL0_ENDIANNESS_ARM_SCTLR) */
#define OKL4_WIDTH_EL0_ENDIANNESS_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ARM_SCTLR_EXCEPTION_ENDIANNESS_MASK) */
#define OKL4_ARM_SCTLR_EXCEPTION_ENDIANNESS_MASK ((okl4_arm_sctlr_t)1U << 25) /* Deprecated */
/*lint -esym(621, OKL4_MASK_EXCEPTION_ENDIANNESS_ARM_SCTLR) */
#define OKL4_MASK_EXCEPTION_ENDIANNESS_ARM_SCTLR ((okl4_arm_sctlr_t)1U << 25)
/*lint -esym(621, OKL4_SHIFT_EXCEPTION_ENDIANNESS_ARM_SCTLR) */
#define OKL4_SHIFT_EXCEPTION_ENDIANNESS_ARM_SCTLR (25)
/*lint -esym(621, OKL4_WIDTH_EXCEPTION_ENDIANNESS_ARM_SCTLR) */
#define OKL4_WIDTH_EXCEPTION_ENDIANNESS_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ARM_SCTLR_TEX_REMAP_ENABLE_MASK) */
#define OKL4_ARM_SCTLR_TEX_REMAP_ENABLE_MASK ((okl4_arm_sctlr_t)1U << 28) /* Deprecated */
/*lint -esym(621, OKL4_MASK_TEX_REMAP_ENABLE_ARM_SCTLR) */
#define OKL4_MASK_TEX_REMAP_ENABLE_ARM_SCTLR ((okl4_arm_sctlr_t)1U << 28)
/*lint -esym(621, OKL4_SHIFT_TEX_REMAP_ENABLE_ARM_SCTLR) */
#define OKL4_SHIFT_TEX_REMAP_ENABLE_ARM_SCTLR (28)
/*lint -esym(621, OKL4_WIDTH_TEX_REMAP_ENABLE_ARM_SCTLR) */
#define OKL4_WIDTH_TEX_REMAP_ENABLE_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ARM_SCTLR_ACCESS_FLAG_ENABLE_MASK) */
#define OKL4_ARM_SCTLR_ACCESS_FLAG_ENABLE_MASK ((okl4_arm_sctlr_t)1U << 29) /* Deprecated */
/*lint -esym(621, OKL4_MASK_ACCESS_FLAG_ENABLE_ARM_SCTLR) */
#define OKL4_MASK_ACCESS_FLAG_ENABLE_ARM_SCTLR ((okl4_arm_sctlr_t)1U << 29)
/*lint -esym(621, OKL4_SHIFT_ACCESS_FLAG_ENABLE_ARM_SCTLR) */
#define OKL4_SHIFT_ACCESS_FLAG_ENABLE_ARM_SCTLR (29)
/*lint -esym(621, OKL4_WIDTH_ACCESS_FLAG_ENABLE_ARM_SCTLR) */
#define OKL4_WIDTH_ACCESS_FLAG_ENABLE_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ARM_SCTLR_THUMB_EXCEPTION_ENABLE_MASK) */
#define OKL4_ARM_SCTLR_THUMB_EXCEPTION_ENABLE_MASK ((okl4_arm_sctlr_t)1U << 30) /* Deprecated */
/*lint -esym(621, OKL4_MASK_THUMB_EXCEPTION_ENABLE_ARM_SCTLR) */
#define OKL4_MASK_THUMB_EXCEPTION_ENABLE_ARM_SCTLR ((okl4_arm_sctlr_t)1U << 30)
/*lint -esym(621, OKL4_SHIFT_THUMB_EXCEPTION_ENABLE_ARM_SCTLR) */
#define OKL4_SHIFT_THUMB_EXCEPTION_ENABLE_ARM_SCTLR (30)
/*lint -esym(621, OKL4_WIDTH_THUMB_EXCEPTION_ENABLE_ARM_SCTLR) */
#define OKL4_WIDTH_THUMB_EXCEPTION_ENABLE_ARM_SCTLR (1)


/*lint -sem(okl4_arm_sctlr_getmmuenable, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_getmmuenable) */
/*lint -esym(714, okl4_arm_sctlr_getmmuenable) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getmmuenable(const okl4_arm_sctlr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_setmmuenable, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_sctlr_setmmuenable) */

/*lint -esym(621, okl4_arm_sctlr_setmmuenable) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setmmuenable(okl4_arm_sctlr_t *x, okl4_bool_t _mmu_enable)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_mmu_enable;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_sctlr_getalignmentcheckenable, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_getalignmentcheckenable) */
/*lint -esym(714, okl4_arm_sctlr_getalignmentcheckenable) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getalignmentcheckenable(const okl4_arm_sctlr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 1;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_setalignmentcheckenable, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_sctlr_setalignmentcheckenable) */

/*lint -esym(621, okl4_arm_sctlr_setalignmentcheckenable) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setalignmentcheckenable(okl4_arm_sctlr_t *x, okl4_bool_t _alignment_check_enable)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 1;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_alignment_check_enable;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_sctlr_getdatacacheenable, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_getdatacacheenable) */
/*lint -esym(714, okl4_arm_sctlr_getdatacacheenable) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getdatacacheenable(const okl4_arm_sctlr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 2;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_setdatacacheenable, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_sctlr_setdatacacheenable) */

/*lint -esym(621, okl4_arm_sctlr_setdatacacheenable) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setdatacacheenable(okl4_arm_sctlr_t *x, okl4_bool_t _data_cache_enable)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 2;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_data_cache_enable;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_sctlr_getstackalign, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_getstackalign) */
/*lint -esym(714, okl4_arm_sctlr_getstackalign) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getstackalign(const okl4_arm_sctlr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 3;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_setstackalign, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_sctlr_setstackalign) */

/*lint -esym(621, okl4_arm_sctlr_setstackalign) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setstackalign(okl4_arm_sctlr_t *x, okl4_bool_t _stack_align)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 3;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_stack_align;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_sctlr_getstackalignel0, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_getstackalignel0) */
/*lint -esym(714, okl4_arm_sctlr_getstackalignel0) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getstackalignel0(const okl4_arm_sctlr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 4;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_setstackalignel0, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_sctlr_setstackalignel0) */

/*lint -esym(621, okl4_arm_sctlr_setstackalignel0) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setstackalignel0(okl4_arm_sctlr_t *x, okl4_bool_t _stack_align_el0)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 4;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_stack_align_el0;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_sctlr_getcp15barrierenable, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_getcp15barrierenable) */
/*lint -esym(714, okl4_arm_sctlr_getcp15barrierenable) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getcp15barrierenable(const okl4_arm_sctlr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 5;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_setcp15barrierenable, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_sctlr_setcp15barrierenable) */

/*lint -esym(621, okl4_arm_sctlr_setcp15barrierenable) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setcp15barrierenable(okl4_arm_sctlr_t *x, okl4_bool_t _cp15_barrier_enable)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 5;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_cp15_barrier_enable;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_sctlr_getoklhcrel2dc, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_getoklhcrel2dc) */
/*lint -esym(714, okl4_arm_sctlr_getoklhcrel2dc) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getoklhcrel2dc(const okl4_arm_sctlr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 6;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_setoklhcrel2dc, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_sctlr_setoklhcrel2dc) */

/*lint -esym(621, okl4_arm_sctlr_setoklhcrel2dc) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setoklhcrel2dc(okl4_arm_sctlr_t *x, okl4_bool_t _okl_hcr_el2_dc)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 6;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_okl_hcr_el2_dc;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_sctlr_getitdisable, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_getitdisable) */
/*lint -esym(714, okl4_arm_sctlr_getitdisable) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getitdisable(const okl4_arm_sctlr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 7;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_setitdisable, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_sctlr_setitdisable) */

/*lint -esym(621, okl4_arm_sctlr_setitdisable) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setitdisable(okl4_arm_sctlr_t *x, okl4_bool_t _it_disable)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 7;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_it_disable;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_sctlr_getsetenddisable, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_getsetenddisable) */
/*lint -esym(714, okl4_arm_sctlr_getsetenddisable) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getsetenddisable(const okl4_arm_sctlr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 8;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_setsetenddisable, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_sctlr_setsetenddisable) */

/*lint -esym(621, okl4_arm_sctlr_setsetenddisable) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setsetenddisable(okl4_arm_sctlr_t *x, okl4_bool_t _setend_disable)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 8;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_setend_disable;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_sctlr_getusermaskaccess, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_getusermaskaccess) */
/*lint -esym(714, okl4_arm_sctlr_getusermaskaccess) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getusermaskaccess(const okl4_arm_sctlr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 9;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_setusermaskaccess, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_sctlr_setusermaskaccess) */

/*lint -esym(621, okl4_arm_sctlr_setusermaskaccess) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setusermaskaccess(okl4_arm_sctlr_t *x, okl4_bool_t _user_mask_access)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 9;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_user_mask_access;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_sctlr_getreserved11, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_getreserved11) */
/*lint -esym(714, okl4_arm_sctlr_getreserved11) */
OKL4_FORCE_INLINE uint32_t
okl4_arm_sctlr_getreserved11(const okl4_arm_sctlr_t *x)
{
    uint32_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 11;
            uint32_t field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (uint32_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_getinstructioncacheenable, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_getinstructioncacheenable) */
/*lint -esym(714, okl4_arm_sctlr_getinstructioncacheenable) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getinstructioncacheenable(const okl4_arm_sctlr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 12;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_setinstructioncacheenable, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_sctlr_setinstructioncacheenable) */

/*lint -esym(621, okl4_arm_sctlr_setinstructioncacheenable) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setinstructioncacheenable(okl4_arm_sctlr_t *x, okl4_bool_t _instruction_cache_enable)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 12;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_instruction_cache_enable;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_sctlr_getvectorsbit, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_getvectorsbit) */
/*lint -esym(714, okl4_arm_sctlr_getvectorsbit) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getvectorsbit(const okl4_arm_sctlr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 13;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_setvectorsbit, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_sctlr_setvectorsbit) */

/*lint -esym(621, okl4_arm_sctlr_setvectorsbit) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setvectorsbit(okl4_arm_sctlr_t *x, okl4_bool_t _vectors_bit)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 13;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_vectors_bit;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_sctlr_getdcachezero, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_getdcachezero) */
/*lint -esym(714, okl4_arm_sctlr_getdcachezero) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getdcachezero(const okl4_arm_sctlr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 14;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_setdcachezero, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_sctlr_setdcachezero) */

/*lint -esym(621, okl4_arm_sctlr_setdcachezero) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setdcachezero(okl4_arm_sctlr_t *x, okl4_bool_t _dcache_zero)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 14;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_dcache_zero;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_sctlr_getusercachetype, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_getusercachetype) */
/*lint -esym(714, okl4_arm_sctlr_getusercachetype) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getusercachetype(const okl4_arm_sctlr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 15;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_setusercachetype, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_sctlr_setusercachetype) */

/*lint -esym(621, okl4_arm_sctlr_setusercachetype) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setusercachetype(okl4_arm_sctlr_t *x, okl4_bool_t _user_cache_type)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 15;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_user_cache_type;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_sctlr_getnotrapwfi, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_getnotrapwfi) */
/*lint -esym(714, okl4_arm_sctlr_getnotrapwfi) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getnotrapwfi(const okl4_arm_sctlr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 16;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_setnotrapwfi, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_sctlr_setnotrapwfi) */

/*lint -esym(621, okl4_arm_sctlr_setnotrapwfi) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setnotrapwfi(okl4_arm_sctlr_t *x, okl4_bool_t _no_trap_wfi)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 16;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_no_trap_wfi;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_sctlr_getnotrapwfe, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_getnotrapwfe) */
/*lint -esym(714, okl4_arm_sctlr_getnotrapwfe) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getnotrapwfe(const okl4_arm_sctlr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 18;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_setnotrapwfe, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_sctlr_setnotrapwfe) */

/*lint -esym(621, okl4_arm_sctlr_setnotrapwfe) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setnotrapwfe(okl4_arm_sctlr_t *x, okl4_bool_t _no_trap_wfe)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 18;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_no_trap_wfe;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_sctlr_getwriteexecnever, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_getwriteexecnever) */
/*lint -esym(714, okl4_arm_sctlr_getwriteexecnever) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getwriteexecnever(const okl4_arm_sctlr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 19;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_setwriteexecnever, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_sctlr_setwriteexecnever) */

/*lint -esym(621, okl4_arm_sctlr_setwriteexecnever) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setwriteexecnever(okl4_arm_sctlr_t *x, okl4_bool_t _write_exec_never)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 19;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_write_exec_never;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_sctlr_getuserwriteexecnever, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_getuserwriteexecnever) */
/*lint -esym(714, okl4_arm_sctlr_getuserwriteexecnever) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getuserwriteexecnever(const okl4_arm_sctlr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 20;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_setuserwriteexecnever, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_sctlr_setuserwriteexecnever) */

/*lint -esym(621, okl4_arm_sctlr_setuserwriteexecnever) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setuserwriteexecnever(okl4_arm_sctlr_t *x, okl4_bool_t _user_write_exec_never)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 20;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_user_write_exec_never;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_sctlr_getreserved22, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_getreserved22) */
/*lint -esym(714, okl4_arm_sctlr_getreserved22) */
OKL4_FORCE_INLINE uint32_t
okl4_arm_sctlr_getreserved22(const okl4_arm_sctlr_t *x)
{
    uint32_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 22;
            uint32_t field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (uint32_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_getreserved23, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_getreserved23) */
/*lint -esym(714, okl4_arm_sctlr_getreserved23) */
OKL4_FORCE_INLINE uint32_t
okl4_arm_sctlr_getreserved23(const okl4_arm_sctlr_t *x)
{
    uint32_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 23;
            uint32_t field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (uint32_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_getel0endianness, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_getel0endianness) */
/*lint -esym(714, okl4_arm_sctlr_getel0endianness) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getel0endianness(const okl4_arm_sctlr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 24;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_setel0endianness, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_sctlr_setel0endianness) */

/*lint -esym(621, okl4_arm_sctlr_setel0endianness) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setel0endianness(okl4_arm_sctlr_t *x, okl4_bool_t _el0_endianness)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 24;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_el0_endianness;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_sctlr_getexceptionendianness, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_getexceptionendianness) */
/*lint -esym(714, okl4_arm_sctlr_getexceptionendianness) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getexceptionendianness(const okl4_arm_sctlr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 25;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_setexceptionendianness, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_sctlr_setexceptionendianness) */

/*lint -esym(621, okl4_arm_sctlr_setexceptionendianness) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setexceptionendianness(okl4_arm_sctlr_t *x, okl4_bool_t _exception_endianness)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 25;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_exception_endianness;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_sctlr_gettexremapenable, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_gettexremapenable) */
/*lint -esym(714, okl4_arm_sctlr_gettexremapenable) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_gettexremapenable(const okl4_arm_sctlr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 28;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_settexremapenable, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_sctlr_settexremapenable) */

/*lint -esym(621, okl4_arm_sctlr_settexremapenable) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_settexremapenable(okl4_arm_sctlr_t *x, okl4_bool_t _tex_remap_enable)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 28;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_tex_remap_enable;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_sctlr_getaccessflagenable, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_getaccessflagenable) */
/*lint -esym(714, okl4_arm_sctlr_getaccessflagenable) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getaccessflagenable(const okl4_arm_sctlr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 29;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_setaccessflagenable, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_sctlr_setaccessflagenable) */

/*lint -esym(621, okl4_arm_sctlr_setaccessflagenable) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setaccessflagenable(okl4_arm_sctlr_t *x, okl4_bool_t _access_flag_enable)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 29;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_access_flag_enable;
    *x = _conv.raw;
}
/*lint -sem(okl4_arm_sctlr_getthumbexceptionenable, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_arm_sctlr_getthumbexceptionenable) */
/*lint -esym(714, okl4_arm_sctlr_getthumbexceptionenable) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_sctlr_getthumbexceptionenable(const okl4_arm_sctlr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 30;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_arm_sctlr_setthumbexceptionenable, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_arm_sctlr_setthumbexceptionenable) */

/*lint -esym(621, okl4_arm_sctlr_setthumbexceptionenable) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_setthumbexceptionenable(okl4_arm_sctlr_t *x, okl4_bool_t _thumb_exception_enable)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 30;
            _Bool field : 1;
        } bits;
        okl4_arm_sctlr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_thumb_exception_enable;
    *x = _conv.raw;
}
/*lint -esym(714, okl4_arm_sctlr_init) */
OKL4_FORCE_INLINE void
okl4_arm_sctlr_init(okl4_arm_sctlr_t *x)
{
    *x = (okl4_arm_sctlr_t)12912928U;
}

/*lint -esym(714, okl4_arm_sctlr_cast) */
OKL4_FORCE_INLINE okl4_arm_sctlr_t
okl4_arm_sctlr_cast(uint32_t p, okl4_bool_t force)
{
    okl4_arm_sctlr_t x = (okl4_arm_sctlr_t)p;
    if (force) {
        x &= ~(okl4_arm_sctlr_t)0x800U;
        x |= (okl4_arm_sctlr_t)0x800U; /* x.reserved11 */
        x &= ~(okl4_arm_sctlr_t)0x400000U;
        x |= (okl4_arm_sctlr_t)0x400000U; /* x.reserved22 */
        x &= ~(okl4_arm_sctlr_t)0x800000U;
        x |= (okl4_arm_sctlr_t)0x800000U; /* x.reserved23 */
    }
    return x;
}




typedef uint32_t okl4_arm_smccc_arch_function_t;

/*lint -esym(621, OKL4_ARM_SMCCC_ARCH_FUNCTION_SMCCC_VERSION) */
#define OKL4_ARM_SMCCC_ARCH_FUNCTION_SMCCC_VERSION ((okl4_arm_smccc_arch_function_t)0x0U)
/*lint -esym(621, OKL4_ARM_SMCCC_ARCH_FUNCTION_ARCH_FEATURES) */
#define OKL4_ARM_SMCCC_ARCH_FUNCTION_ARCH_FEATURES ((okl4_arm_smccc_arch_function_t)0x1U)
/*lint -esym(621, OKL4_ARM_SMCCC_ARCH_FUNCTION_ARCH_WORKAROUND_1) */
#define OKL4_ARM_SMCCC_ARCH_FUNCTION_ARCH_WORKAROUND_1 ((okl4_arm_smccc_arch_function_t)0x8000U)

/*lint -esym(714, okl4_arm_smccc_arch_function_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_smccc_arch_function_is_element_of(okl4_arm_smccc_arch_function_t var);


/*lint -esym(714, okl4_arm_smccc_arch_function_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_smccc_arch_function_is_element_of(okl4_arm_smccc_arch_function_t var)
{
    /*lint --e{944} Disable dead expression detection */
    /*lint --e{948} --e{845} Disable constant always zero */
    return ((var == OKL4_ARM_SMCCC_ARCH_FUNCTION_SMCCC_VERSION) ||
            (var == OKL4_ARM_SMCCC_ARCH_FUNCTION_ARCH_FEATURES) ||
            (var == OKL4_ARM_SMCCC_ARCH_FUNCTION_ARCH_WORKAROUND_1));
}



typedef uint32_t okl4_arm_smccc_result_t;

/*lint -esym(621, OKL4_ARM_SMCCC_RESULT_SUCCESS) */
#define OKL4_ARM_SMCCC_RESULT_SUCCESS ((okl4_arm_smccc_result_t)0x0U)
/*lint -esym(621, OKL4_ARM_SMCCC_RESULT_NOT_SUPPORTED) */
#define OKL4_ARM_SMCCC_RESULT_NOT_SUPPORTED ((okl4_arm_smccc_result_t)0xffffffffU)

/*lint -esym(714, okl4_arm_smccc_result_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_smccc_result_is_element_of(okl4_arm_smccc_result_t var);


/*lint -esym(714, okl4_arm_smccc_result_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_arm_smccc_result_is_element_of(okl4_arm_smccc_result_t var)
{
    /*lint --e{944} Disable dead expression detection */
    /*lint --e{948} --e{845} Disable constant always zero */
    return ((var == OKL4_ARM_SMCCC_RESULT_SUCCESS) ||
            (var == OKL4_ARM_SMCCC_RESULT_NOT_SUPPORTED));
}


/**
    The `okl4_register_t` type represents an unsigned, machine-native
    register-sized integer value.
*/

typedef uint64_t okl4_register_t;





typedef okl4_register_t okl4_atomic_raw_register_t;









typedef uint16_t okl4_atomic_raw_uint16_t;





typedef uint32_t okl4_atomic_raw_uint32_t;





typedef uint64_t okl4_atomic_raw_uint64_t;









typedef uint8_t okl4_atomic_raw_uint8_t;




/**
    The okl4_atomic_register_t type implements a machine-word-sized value
    that can be operated on using atomic operations.  This can be used
    to implement thread-safe synchronisation primitives.
*/

struct okl4_atomic_register {
    volatile okl4_atomic_raw_register_t value;
};






/**
    The okl4_atomic_register_t type implements a machine-word-sized value
    that can be operated on using atomic operations.  This can be used
    to implement thread-safe synchronisation primitives.
*/

typedef struct okl4_atomic_register okl4_atomic_register_t;




/**
    The okl4_atomic_uint16_t type implements a 16-bit value
    that can be operated on using atomic operations.  This can be used
    to implement thread-safe synchronisation primitives.
*/

struct okl4_atomic_uint16 {
    volatile okl4_atomic_raw_uint16_t value;
};






/**
    The okl4_atomic_uint16_t type implements a 16-bit value
    that can be operated on using atomic operations.  This can be used
    to implement thread-safe synchronisation primitives.
*/

typedef struct okl4_atomic_uint16 okl4_atomic_uint16_t;




/**
    The okl4_atomic_uint32_t type implements a 32-bit value
    that can be operated on using atomic operations.  This can be used
    to implement thread-safe synchronisation primitives.
*/

struct okl4_atomic_uint32 {
    volatile okl4_atomic_raw_uint32_t value;
};






/**
    The okl4_atomic_uint32_t type implements a 32-bit value
    that can be operated on using atomic operations.  This can be used
    to implement thread-safe synchronisation primitives.
*/

typedef struct okl4_atomic_uint32 okl4_atomic_uint32_t;




/**
    The okl4_atomic_uint64_t type implements a 64-bit value
    that can be operated on using atomic operations.  This can be used
    to implement thread-safe synchronisation primitives.
*/

struct okl4_atomic_uint64 {
    volatile okl4_atomic_raw_uint64_t value;
};






/**
    The okl4_atomic_uint64_t type implements a 64-bit value
    that can be operated on using atomic operations.  This can be used
    to implement thread-safe synchronisation primitives.
*/

typedef struct okl4_atomic_uint64 okl4_atomic_uint64_t;




/**
    The okl4_atomic_uint8_t type implements an 8-bit value
    that can be operated on using atomic operations.  This can be used
    to implement thread-safe synchronisation primitives.
*/

struct okl4_atomic_uint8 {
    volatile okl4_atomic_raw_uint8_t value;
};






/**
    The okl4_atomic_uint8_t type implements an 8-bit value
    that can be operated on using atomic operations.  This can be used
    to implement thread-safe synchronisation primitives.
*/

typedef struct okl4_atomic_uint8 okl4_atomic_uint8_t;




/**
    The `okl4_count_t` type represents a natural number of items or
    iterations. This type is unsigned and cannot represent error values; use
    `okl4_scount_t` if an error representation is required.
*/

typedef uint32_t okl4_count_t;

/*lint -esym(621, OKL4_DEFAULT_PAGEBITS) */
#define OKL4_DEFAULT_PAGEBITS ((okl4_count_t)(12U))

/** The maximum limit for segment index retured in mmu_lookup_segment. */
/*lint -esym(621, OKL4_KMMU_LOOKUP_PAGE_SEGMENT_MASK) */
#define OKL4_KMMU_LOOKUP_PAGE_SEGMENT_MASK ((okl4_count_t)(1023U))

/** The maximum limit for segment attachments to a KMMU. */
/*lint -esym(621, OKL4_KMMU_MAX_SEGMENTS) */
#define OKL4_KMMU_MAX_SEGMENTS ((okl4_count_t)(256U))

/*lint -esym(621, OKL4_PROFILE_NO_PCPUS) */
#define OKL4_PROFILE_NO_PCPUS ((okl4_count_t)(0xffffffffU))



/**
    The `okl4_kcap_t` type represents a kernel object capability identifier
    (otherwise known as *designator* or *cap*) that addresses a kernel
    capability. A capability encodes rights to perform particular operations on
    a kernel object.
*/

typedef okl4_count_t okl4_kcap_t;

/*lint -esym(621, OKL4_KCAP_INVALID) */
#define OKL4_KCAP_INVALID ((okl4_kcap_t)(0xffffffffU))



/**
    The `okl4_interrupt_number_t` type is an index into the interrupt ID
    space. For platforms with a single simple interrupt controller, this is
    the physical interrupt number. When there are multiple interrupt
    controllers, or a large and sparse interrupt ID space, the mapping from
    this type to the physical interrupt is defined by the KSP.
*/

typedef okl4_count_t okl4_interrupt_number_t;

/*lint -esym(621, OKL4_INTERRUPT_INVALID_IRQ) */
#define OKL4_INTERRUPT_INVALID_IRQ ((okl4_interrupt_number_t)(1023U))

/*lint -esym(621, OKL4_INVALID_VIRQ) */
#define OKL4_INVALID_VIRQ ((okl4_interrupt_number_t)(1023U))




typedef okl4_interrupt_number_t okl4_irq_t;




/**

*/

struct okl4_axon_data {
    okl4_kcap_t kcap;
    okl4_kcap_t segment;
    okl4_irq_t virq;
};




/**
    The `okl4_psize_t` type represents an unsigned integer value which is large
    enough to represent the size of any physical memory object.
*/

typedef okl4_register_t okl4_psize_t;




/**
    The `okl4_lsize_t` type represents an unsigned integer value which is large
    enough to represent the size of any guest logical memory object.
*/

typedef okl4_psize_t okl4_lsize_t;

/*lint -esym(621, OKL4_DEFAULT_PAGESIZE) */
#define OKL4_DEFAULT_PAGESIZE ((okl4_lsize_t)(4096U))



/**
    The `okl4_laddr_t` type represents an unsigned integer value which is large
    enough to contain a guest logical address; that is, an address in the
    input address space of the guest's virtual MMU. This may be larger than
    the machine's pointer type.
*/

typedef okl4_lsize_t okl4_laddr_t;

/*lint -esym(621, OKL4_USER_AREA_END) */
#define OKL4_USER_AREA_END ((okl4_laddr_t)(17592186044416U))



/**
    - BIT 0 -   @ref OKL4_MASK_PENDING_AXON_DATA_INFO
    - BIT 1 -   @ref OKL4_MASK_FAILURE_AXON_DATA_INFO
    - BIT 2 -   @ref OKL4_MASK_USR_AXON_DATA_INFO
    - BITS 63..3 -   @ref OKL4_MASK_LADDR_AXON_DATA_INFO
*/

/*lint -esym(621, okl4_axon_data_info_t) */
typedef okl4_laddr_t okl4_axon_data_info_t;

/*lint -esym(621, okl4_axon_data_info_getpending) */
/*lint -esym(714, okl4_axon_data_info_getpending) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_axon_data_info_getpending(const okl4_axon_data_info_t *x);

/*lint -esym(621, okl4_axon_data_info_setpending) */
OKL4_FORCE_INLINE void
okl4_axon_data_info_setpending(okl4_axon_data_info_t *x, okl4_bool_t _pending);

/*lint -esym(621, okl4_axon_data_info_getfailure) */
/*lint -esym(714, okl4_axon_data_info_getfailure) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_axon_data_info_getfailure(const okl4_axon_data_info_t *x);

/*lint -esym(621, okl4_axon_data_info_setfailure) */
OKL4_FORCE_INLINE void
okl4_axon_data_info_setfailure(okl4_axon_data_info_t *x, okl4_bool_t _failure);

/*lint -esym(621, okl4_axon_data_info_getusr) */
/*lint -esym(714, okl4_axon_data_info_getusr) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_axon_data_info_getusr(const okl4_axon_data_info_t *x);

/*lint -esym(621, okl4_axon_data_info_setusr) */
OKL4_FORCE_INLINE void
okl4_axon_data_info_setusr(okl4_axon_data_info_t *x, okl4_bool_t _usr);

/*lint -esym(621, okl4_axon_data_info_getladdr) */
/*lint -esym(714, okl4_axon_data_info_getladdr) */
OKL4_FORCE_INLINE okl4_laddr_t
okl4_axon_data_info_getladdr(const okl4_axon_data_info_t *x);

/*lint -esym(621, okl4_axon_data_info_setladdr) */
OKL4_FORCE_INLINE void
okl4_axon_data_info_setladdr(okl4_axon_data_info_t *x, okl4_laddr_t _laddr);

/*lint -esym(714, okl4_axon_data_info_init) */
OKL4_FORCE_INLINE void
okl4_axon_data_info_init(okl4_axon_data_info_t *x);

/*lint -esym(714, okl4_axon_data_info_cast) */
OKL4_FORCE_INLINE okl4_axon_data_info_t
okl4_axon_data_info_cast(uint64_t p, okl4_bool_t force);




/*lint -esym(621, OKL4_AXON_DATA_INFO_PENDING_MASK) */
#define OKL4_AXON_DATA_INFO_PENDING_MASK ((okl4_axon_data_info_t)1U) /* Deprecated */
/*lint -esym(621, OKL4_MASK_PENDING_AXON_DATA_INFO) */
#define OKL4_MASK_PENDING_AXON_DATA_INFO ((okl4_axon_data_info_t)1U)
/*lint -esym(621, OKL4_SHIFT_PENDING_AXON_DATA_INFO) */
#define OKL4_SHIFT_PENDING_AXON_DATA_INFO (0)
/*lint -esym(621, OKL4_WIDTH_PENDING_AXON_DATA_INFO) */
#define OKL4_WIDTH_PENDING_AXON_DATA_INFO (1)
/*lint -esym(621, OKL4_AXON_DATA_INFO_FAILURE_MASK) */
#define OKL4_AXON_DATA_INFO_FAILURE_MASK ((okl4_axon_data_info_t)1U << 1) /* Deprecated */
/*lint -esym(621, OKL4_MASK_FAILURE_AXON_DATA_INFO) */
#define OKL4_MASK_FAILURE_AXON_DATA_INFO ((okl4_axon_data_info_t)1U << 1)
/*lint -esym(621, OKL4_SHIFT_FAILURE_AXON_DATA_INFO) */
#define OKL4_SHIFT_FAILURE_AXON_DATA_INFO (1)
/*lint -esym(621, OKL4_WIDTH_FAILURE_AXON_DATA_INFO) */
#define OKL4_WIDTH_FAILURE_AXON_DATA_INFO (1)
/*lint -esym(621, OKL4_AXON_DATA_INFO_USR_MASK) */
#define OKL4_AXON_DATA_INFO_USR_MASK ((okl4_axon_data_info_t)1U << 2) /* Deprecated */
/*lint -esym(621, OKL4_MASK_USR_AXON_DATA_INFO) */
#define OKL4_MASK_USR_AXON_DATA_INFO ((okl4_axon_data_info_t)1U << 2)
/*lint -esym(621, OKL4_SHIFT_USR_AXON_DATA_INFO) */
#define OKL4_SHIFT_USR_AXON_DATA_INFO (2)
/*lint -esym(621, OKL4_WIDTH_USR_AXON_DATA_INFO) */
#define OKL4_WIDTH_USR_AXON_DATA_INFO (1)
/*lint -esym(621, OKL4_AXON_DATA_INFO_LADDR_MASK) */
#define OKL4_AXON_DATA_INFO_LADDR_MASK ((okl4_axon_data_info_t)2305843009213693951U << 3) /* Deprecated */
/*lint -esym(621, OKL4_MASK_LADDR_AXON_DATA_INFO) */
#define OKL4_MASK_LADDR_AXON_DATA_INFO ((okl4_axon_data_info_t)2305843009213693951U << 3)
/*lint -esym(621, OKL4_SHIFT_LADDR_AXON_DATA_INFO) */
#define OKL4_SHIFT_LADDR_AXON_DATA_INFO (3)
/*lint -esym(621, OKL4_PRESHIFT_LADDR_AXON_DATA_INFO) */
#define OKL4_PRESHIFT_LADDR_AXON_DATA_INFO (3)
/*lint -esym(621, OKL4_WIDTH_LADDR_AXON_DATA_INFO) */
#define OKL4_WIDTH_LADDR_AXON_DATA_INFO (61)


/*lint -sem(okl4_axon_data_info_getpending, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_axon_data_info_getpending) */
/*lint -esym(714, okl4_axon_data_info_getpending) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_axon_data_info_getpending(const okl4_axon_data_info_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            _Bool field : 1;
        } bits;
        okl4_axon_data_info_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_axon_data_info_setpending, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_axon_data_info_setpending) */

/*lint -esym(621, okl4_axon_data_info_setpending) */
OKL4_FORCE_INLINE void
okl4_axon_data_info_setpending(okl4_axon_data_info_t *x, okl4_bool_t _pending)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            _Bool field : 1;
        } bits;
        okl4_axon_data_info_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_pending;
    *x = _conv.raw;
}
/*lint -sem(okl4_axon_data_info_getfailure, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_axon_data_info_getfailure) */
/*lint -esym(714, okl4_axon_data_info_getfailure) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_axon_data_info_getfailure(const okl4_axon_data_info_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint64_t _skip : 1;
            _Bool field : 1;
        } bits;
        okl4_axon_data_info_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_axon_data_info_setfailure, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_axon_data_info_setfailure) */

/*lint -esym(621, okl4_axon_data_info_setfailure) */
OKL4_FORCE_INLINE void
okl4_axon_data_info_setfailure(okl4_axon_data_info_t *x, okl4_bool_t _failure)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint64_t _skip : 1;
            _Bool field : 1;
        } bits;
        okl4_axon_data_info_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_failure;
    *x = _conv.raw;
}
/*lint -sem(okl4_axon_data_info_getusr, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_axon_data_info_getusr) */
/*lint -esym(714, okl4_axon_data_info_getusr) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_axon_data_info_getusr(const okl4_axon_data_info_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint64_t _skip : 2;
            _Bool field : 1;
        } bits;
        okl4_axon_data_info_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_axon_data_info_setusr, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_axon_data_info_setusr) */

/*lint -esym(621, okl4_axon_data_info_setusr) */
OKL4_FORCE_INLINE void
okl4_axon_data_info_setusr(okl4_axon_data_info_t *x, okl4_bool_t _usr)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint64_t _skip : 2;
            _Bool field : 1;
        } bits;
        okl4_axon_data_info_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_usr;
    *x = _conv.raw;
}
/*lint -sem(okl4_axon_data_info_getladdr, 1p) */
/*lint -esym(621, okl4_axon_data_info_getladdr) */
/*lint -esym(714, okl4_axon_data_info_getladdr) */
OKL4_FORCE_INLINE okl4_laddr_t
okl4_axon_data_info_getladdr(const okl4_axon_data_info_t *x)
{
    okl4_laddr_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint64_t _skip : 3;
            uint64_t field : 61;
        } bits;
        okl4_axon_data_info_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_laddr_t)_conv.bits.field;
    return (okl4_laddr_t)(field << 3);
}

/*lint -esym(714, okl4_axon_data_info_setladdr) */

/*lint -esym(621, okl4_axon_data_info_setladdr) */
OKL4_FORCE_INLINE void
okl4_axon_data_info_setladdr(okl4_axon_data_info_t *x, okl4_laddr_t _laddr)
{
    okl4_laddr_t val = _laddr >> 3;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint64_t _skip : 3;
            uint64_t field : 61;
        } bits;
        okl4_axon_data_info_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint64_t)val;
    *x = _conv.raw;
}
/*lint -esym(714, okl4_axon_data_info_init) */
OKL4_FORCE_INLINE void
okl4_axon_data_info_init(okl4_axon_data_info_t *x)
{
    *x = (okl4_axon_data_info_t)0U;
}

/*lint -esym(714, okl4_axon_data_info_cast) */
OKL4_FORCE_INLINE okl4_axon_data_info_t
okl4_axon_data_info_cast(uint64_t p, okl4_bool_t force)
{
    okl4_axon_data_info_t x = (okl4_axon_data_info_t)p;
    (void)force;
    return x;
}



/**

*/

struct okl4_axon_ep_data {
    struct okl4_axon_data rx;
    struct okl4_axon_data tx;
};









typedef char _okl4_padding_t;





struct okl4_axon_queue {
    uint32_t queue_offset;
    uint16_t entries;
    volatile uint16_t kptr;
    volatile uint16_t uptr;
    _okl4_padding_t __padding0_2; /**< Padding 4 */
    _okl4_padding_t __padding1_3; /**< Padding 4 */
};






/**
    The `okl4_ksize_t` type represents an unsigned integer value which is large
    enough to represent the size of any kernel-accessible memory object.
*/

typedef okl4_lsize_t okl4_ksize_t;





struct okl4_axon_queue_entry {
    okl4_axon_data_info_t info;
    okl4_ksize_t data_size;
    uint32_t recv_sequence;
    _okl4_padding_t __padding0_4; /**< Padding 8 */
    _okl4_padding_t __padding1_5; /**< Padding 8 */
    _okl4_padding_t __padding2_6; /**< Padding 8 */
    _okl4_padding_t __padding3_7; /**< Padding 8 */
};






/**
    - BITS 4..0 -   @ref OKL4_MASK_ALLOC_ORDER_AXON_QUEUE_SIZE
    - BITS 12..8 -   @ref OKL4_MASK_MIN_ORDER_AXON_QUEUE_SIZE
*/

/*lint -esym(621, okl4_axon_queue_size_t) */
typedef uint16_t okl4_axon_queue_size_t;

/*lint -esym(621, okl4_axon_queue_size_getallocorder) */
/*lint -esym(714, okl4_axon_queue_size_getallocorder) */
OKL4_FORCE_INLINE okl4_count_t
okl4_axon_queue_size_getallocorder(const okl4_axon_queue_size_t *x);

/*lint -esym(621, okl4_axon_queue_size_setallocorder) */
OKL4_FORCE_INLINE void
okl4_axon_queue_size_setallocorder(okl4_axon_queue_size_t *x, okl4_count_t _alloc_order);

/*lint -esym(621, okl4_axon_queue_size_getminorder) */
/*lint -esym(714, okl4_axon_queue_size_getminorder) */
OKL4_FORCE_INLINE okl4_count_t
okl4_axon_queue_size_getminorder(const okl4_axon_queue_size_t *x);

/*lint -esym(621, okl4_axon_queue_size_setminorder) */
OKL4_FORCE_INLINE void
okl4_axon_queue_size_setminorder(okl4_axon_queue_size_t *x, okl4_count_t _min_order);

/*lint -esym(714, okl4_axon_queue_size_init) */
OKL4_FORCE_INLINE void
okl4_axon_queue_size_init(okl4_axon_queue_size_t *x);

/*lint -esym(714, okl4_axon_queue_size_cast) */
OKL4_FORCE_INLINE okl4_axon_queue_size_t
okl4_axon_queue_size_cast(uint16_t p, okl4_bool_t force);




/*lint -esym(621, OKL4_AXON_QUEUE_SIZE_ALLOC_ORDER_MASK) */
#define OKL4_AXON_QUEUE_SIZE_ALLOC_ORDER_MASK (okl4_axon_queue_size_t)(31U) /* Deprecated */
/*lint -esym(621, OKL4_MASK_ALLOC_ORDER_AXON_QUEUE_SIZE) */
#define OKL4_MASK_ALLOC_ORDER_AXON_QUEUE_SIZE (okl4_axon_queue_size_t)(31U)
/*lint -esym(621, OKL4_SHIFT_ALLOC_ORDER_AXON_QUEUE_SIZE) */
#define OKL4_SHIFT_ALLOC_ORDER_AXON_QUEUE_SIZE (0)
/*lint -esym(621, OKL4_WIDTH_ALLOC_ORDER_AXON_QUEUE_SIZE) */
#define OKL4_WIDTH_ALLOC_ORDER_AXON_QUEUE_SIZE (5)
/*lint -esym(621, OKL4_AXON_QUEUE_SIZE_MIN_ORDER_MASK) */
#define OKL4_AXON_QUEUE_SIZE_MIN_ORDER_MASK (okl4_axon_queue_size_t)(31U << 8) /* Deprecated */
/*lint -esym(621, OKL4_MASK_MIN_ORDER_AXON_QUEUE_SIZE) */
#define OKL4_MASK_MIN_ORDER_AXON_QUEUE_SIZE (okl4_axon_queue_size_t)(31U << 8)
/*lint -esym(621, OKL4_SHIFT_MIN_ORDER_AXON_QUEUE_SIZE) */
#define OKL4_SHIFT_MIN_ORDER_AXON_QUEUE_SIZE (8)
/*lint -esym(621, OKL4_WIDTH_MIN_ORDER_AXON_QUEUE_SIZE) */
#define OKL4_WIDTH_MIN_ORDER_AXON_QUEUE_SIZE (5)


/*lint -sem(okl4_axon_queue_size_getallocorder, 1p, @n >= 0 && @n <= 31) */
/*lint -esym(621, okl4_axon_queue_size_getallocorder) */
/*lint -esym(714, okl4_axon_queue_size_getallocorder) */
OKL4_FORCE_INLINE okl4_count_t
okl4_axon_queue_size_getallocorder(const okl4_axon_queue_size_t *x)
{
    okl4_count_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t field : 5;
        } bits;
        okl4_axon_queue_size_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_count_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_axon_queue_size_setallocorder, 2n >= 0 && 2n <= 31) */
/*lint -esym(714, okl4_axon_queue_size_setallocorder) */

/*lint -esym(621, okl4_axon_queue_size_setallocorder) */
OKL4_FORCE_INLINE void
okl4_axon_queue_size_setallocorder(okl4_axon_queue_size_t *x, okl4_count_t _alloc_order)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t field : 5;
        } bits;
        okl4_axon_queue_size_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_alloc_order;
    *x = _conv.raw;
}
/*lint -sem(okl4_axon_queue_size_getminorder, 1p, @n >= 0 && @n <= 31) */
/*lint -esym(621, okl4_axon_queue_size_getminorder) */
/*lint -esym(714, okl4_axon_queue_size_getminorder) */
OKL4_FORCE_INLINE okl4_count_t
okl4_axon_queue_size_getminorder(const okl4_axon_queue_size_t *x)
{
    okl4_count_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 8;
            uint32_t field : 5;
        } bits;
        okl4_axon_queue_size_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_count_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_axon_queue_size_setminorder, 2n >= 0 && 2n <= 31) */
/*lint -esym(714, okl4_axon_queue_size_setminorder) */

/*lint -esym(621, okl4_axon_queue_size_setminorder) */
OKL4_FORCE_INLINE void
okl4_axon_queue_size_setminorder(okl4_axon_queue_size_t *x, okl4_count_t _min_order)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 8;
            uint32_t field : 5;
        } bits;
        okl4_axon_queue_size_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_min_order;
    *x = _conv.raw;
}
/*lint -esym(714, okl4_axon_queue_size_init) */
OKL4_FORCE_INLINE void
okl4_axon_queue_size_init(okl4_axon_queue_size_t *x)
{
    *x = (okl4_axon_queue_size_t)0U;
}

/*lint -esym(714, okl4_axon_queue_size_cast) */
OKL4_FORCE_INLINE okl4_axon_queue_size_t
okl4_axon_queue_size_cast(uint16_t p, okl4_bool_t force)
{
    okl4_axon_queue_size_t x = (okl4_axon_queue_size_t)p;
    (void)force;
    return x;
}




struct okl4_axon_rx {
    struct okl4_axon_queue queues[4];
    okl4_axon_queue_size_t queue_sizes[4];
};







struct okl4_axon_tx {
    struct okl4_axon_queue queues[4];
};







typedef okl4_register_t okl4_virq_flags_t;




/**
    - BIT 0 -   @ref OKL4_MASK_READY_AXON_VIRQ_FLAGS
    - BIT 1 -   @ref OKL4_MASK_FAULT_AXON_VIRQ_FLAGS
*/

/*lint -esym(621, okl4_axon_virq_flags_t) */
typedef okl4_virq_flags_t okl4_axon_virq_flags_t;

/*lint -esym(621, okl4_axon_virq_flags_getready) */
/*lint -esym(714, okl4_axon_virq_flags_getready) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_axon_virq_flags_getready(const okl4_axon_virq_flags_t *x);

/*lint -esym(621, okl4_axon_virq_flags_setready) */
OKL4_FORCE_INLINE void
okl4_axon_virq_flags_setready(okl4_axon_virq_flags_t *x, okl4_bool_t _ready);

/*lint -esym(621, okl4_axon_virq_flags_getfault) */
/*lint -esym(714, okl4_axon_virq_flags_getfault) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_axon_virq_flags_getfault(const okl4_axon_virq_flags_t *x);

/*lint -esym(621, okl4_axon_virq_flags_setfault) */
OKL4_FORCE_INLINE void
okl4_axon_virq_flags_setfault(okl4_axon_virq_flags_t *x, okl4_bool_t _fault);

/*lint -esym(714, okl4_axon_virq_flags_init) */
OKL4_FORCE_INLINE void
okl4_axon_virq_flags_init(okl4_axon_virq_flags_t *x);

/*lint -esym(714, okl4_axon_virq_flags_cast) */
OKL4_FORCE_INLINE okl4_axon_virq_flags_t
okl4_axon_virq_flags_cast(uint64_t p, okl4_bool_t force);




/*lint -esym(621, OKL4_AXON_VIRQ_FLAGS_READY_MASK) */
#define OKL4_AXON_VIRQ_FLAGS_READY_MASK ((okl4_axon_virq_flags_t)1U) /* Deprecated */
/*lint -esym(621, OKL4_MASK_READY_AXON_VIRQ_FLAGS) */
#define OKL4_MASK_READY_AXON_VIRQ_FLAGS ((okl4_axon_virq_flags_t)1U)
/*lint -esym(621, OKL4_SHIFT_READY_AXON_VIRQ_FLAGS) */
#define OKL4_SHIFT_READY_AXON_VIRQ_FLAGS (0)
/*lint -esym(621, OKL4_WIDTH_READY_AXON_VIRQ_FLAGS) */
#define OKL4_WIDTH_READY_AXON_VIRQ_FLAGS (1)
/*lint -esym(621, OKL4_AXON_VIRQ_FLAGS_FAULT_MASK) */
#define OKL4_AXON_VIRQ_FLAGS_FAULT_MASK ((okl4_axon_virq_flags_t)1U << 1) /* Deprecated */
/*lint -esym(621, OKL4_MASK_FAULT_AXON_VIRQ_FLAGS) */
#define OKL4_MASK_FAULT_AXON_VIRQ_FLAGS ((okl4_axon_virq_flags_t)1U << 1)
/*lint -esym(621, OKL4_SHIFT_FAULT_AXON_VIRQ_FLAGS) */
#define OKL4_SHIFT_FAULT_AXON_VIRQ_FLAGS (1)
/*lint -esym(621, OKL4_WIDTH_FAULT_AXON_VIRQ_FLAGS) */
#define OKL4_WIDTH_FAULT_AXON_VIRQ_FLAGS (1)


/*lint -sem(okl4_axon_virq_flags_getready, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_axon_virq_flags_getready) */
/*lint -esym(714, okl4_axon_virq_flags_getready) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_axon_virq_flags_getready(const okl4_axon_virq_flags_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            _Bool field : 1;
        } bits;
        okl4_axon_virq_flags_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_axon_virq_flags_setready, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_axon_virq_flags_setready) */

/*lint -esym(621, okl4_axon_virq_flags_setready) */
OKL4_FORCE_INLINE void
okl4_axon_virq_flags_setready(okl4_axon_virq_flags_t *x, okl4_bool_t _ready)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            _Bool field : 1;
        } bits;
        okl4_axon_virq_flags_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_ready;
    *x = _conv.raw;
}
/*lint -sem(okl4_axon_virq_flags_getfault, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_axon_virq_flags_getfault) */
/*lint -esym(714, okl4_axon_virq_flags_getfault) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_axon_virq_flags_getfault(const okl4_axon_virq_flags_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint64_t _skip : 1;
            _Bool field : 1;
        } bits;
        okl4_axon_virq_flags_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_axon_virq_flags_setfault, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_axon_virq_flags_setfault) */

/*lint -esym(621, okl4_axon_virq_flags_setfault) */
OKL4_FORCE_INLINE void
okl4_axon_virq_flags_setfault(okl4_axon_virq_flags_t *x, okl4_bool_t _fault)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint64_t _skip : 1;
            _Bool field : 1;
        } bits;
        okl4_axon_virq_flags_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_fault;
    *x = _conv.raw;
}
/*lint -esym(714, okl4_axon_virq_flags_init) */
OKL4_FORCE_INLINE void
okl4_axon_virq_flags_init(okl4_axon_virq_flags_t *x)
{
    *x = (okl4_axon_virq_flags_t)0U;
}

/*lint -esym(714, okl4_axon_virq_flags_cast) */
OKL4_FORCE_INLINE okl4_axon_virq_flags_t
okl4_axon_virq_flags_cast(uint64_t p, okl4_bool_t force)
{
    okl4_axon_virq_flags_t x = (okl4_axon_virq_flags_t)p;
    (void)force;
    return x;
}



/**
    The `okl4_page_cache_t` object represents a set of attributes that
    controls the caching behaviour of memory page mappings.

    - @ref OKL4_PAGE_CACHE_WRITECOMBINE
    - @ref OKL4_PAGE_CACHE_DEFAULT
    - @ref OKL4_PAGE_CACHE_IPC_RX
    - @ref OKL4_PAGE_CACHE_IPC_TX
    - @ref OKL4_PAGE_CACHE_TRACEBUFFER
    - @ref OKL4_PAGE_CACHE_WRITEBACK
    - @ref OKL4_PAGE_CACHE_IWB_RWA_ONC
    - @ref OKL4_PAGE_CACHE_WRITETHROUGH
    - @ref OKL4_PAGE_CACHE_DEVICE_GRE
    - @ref OKL4_PAGE_CACHE_DEVICE_NGRE
    - @ref OKL4_PAGE_CACHE_DEVICE
    - @ref OKL4_PAGE_CACHE_STRONG
    - @ref OKL4_PAGE_CACHE_HW_DEVICE_NGNRNE
    - @ref OKL4_PAGE_CACHE_HW_MASK
    - @ref OKL4_PAGE_CACHE_HW_DEVICE_NGNRE
    - @ref OKL4_PAGE_CACHE_HW_DEVICE_NGRE
    - @ref OKL4_PAGE_CACHE_HW_DEVICE_GRE
    - @ref OKL4_PAGE_CACHE_HW_TWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OTWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OTWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OTWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_TWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OTWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OTWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OTWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_TWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OTWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_ONC_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_ONC_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_ONC_NSH
    - @ref OKL4_PAGE_CACHE_HW_NC_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_ONC_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_ONC_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_ONC_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_ONC_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_ONC_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_ONC_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_ONC_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_ONC_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_ONC_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_ONC_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_ONC_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OTWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_TWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OTWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OTWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OTWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_TWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OTWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OTWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OTWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_TWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OWT_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_WT_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OWT_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OWT_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OWT_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OWT_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OWT_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_WT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_WT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_WT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OWB_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OWB_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OWB_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OWB_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_WB_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OWB_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OWB_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_NA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_WB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_WA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_WB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_RA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OWB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_WB_RWA_NSH
    - @ref OKL4_PAGE_CACHE_HW_TWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OTWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OTWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OTWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_TWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OTWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OTWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OTWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_TWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OTWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_ONC_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_ONC_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_ONC_OSH
    - @ref OKL4_PAGE_CACHE_HW_NC_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_ONC_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_ONC_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_ONC_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_ONC_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_ONC_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_ONC_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_ONC_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_ONC_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_ONC_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_ONC_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_ONC_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OTWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_TWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OTWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OTWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OTWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_TWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OTWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OTWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OTWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_TWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OWT_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_WT_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OWT_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OWT_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OWT_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OWT_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OWT_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_WT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_WT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_WT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OWB_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OWB_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OWB_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OWB_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_WB_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OWB_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OWB_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_NA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_WB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_WA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_WB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_RA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_INC_OWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OWB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_WB_RWA_OSH
    - @ref OKL4_PAGE_CACHE_HW_TWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OTWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_INC_OTWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OTWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_TWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_INC_OTWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OTWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OTWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_TWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_INC_OTWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_ONC_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_ONC_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_ONC_ISH
    - @ref OKL4_PAGE_CACHE_HW_NC_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_ONC_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_ONC_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_ONC_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_ONC_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_ONC_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_ONC_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_ONC_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_ONC_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_ONC_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_ONC_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_ONC_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_INC_OTWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_TWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OTWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_INC_OTWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OTWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_TWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_INC_OTWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OTWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OTWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_TWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_INC_OWT_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_WT_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OWT_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OWT_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OWT_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OWT_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OWT_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_INC_OWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_WT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_INC_OWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_WT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_INC_OWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_WT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_INC_OWB_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OWB_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OWB_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OWB_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_WB_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OWB_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OWB_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_NA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_INC_OWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_WB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_WA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_INC_OWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_WB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_RA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_INC_OWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_NA_OWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_WA_OWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RA_OWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_NA_OWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_WA_OWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_IWB_RA_OWB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_HW_WB_RWA_ISH
    - @ref OKL4_PAGE_CACHE_MAX
    - @ref OKL4_PAGE_CACHE_INVALID
*/

typedef okl4_count_t okl4_page_cache_t;

/*lint -esym(621, OKL4_PAGE_CACHE_WRITECOMBINE) */
#define OKL4_PAGE_CACHE_WRITECOMBINE ((okl4_page_cache_t)0x0U)
/*lint -esym(621, OKL4_PAGE_CACHE_DEFAULT) */
#define OKL4_PAGE_CACHE_DEFAULT ((okl4_page_cache_t)0x1U)
/*lint -esym(621, OKL4_PAGE_CACHE_IPC_RX) */
#define OKL4_PAGE_CACHE_IPC_RX ((okl4_page_cache_t)0x1U)
/*lint -esym(621, OKL4_PAGE_CACHE_IPC_TX) */
#define OKL4_PAGE_CACHE_IPC_TX ((okl4_page_cache_t)0x1U)
/*lint -esym(621, OKL4_PAGE_CACHE_TRACEBUFFER) */
#define OKL4_PAGE_CACHE_TRACEBUFFER ((okl4_page_cache_t)0x1U)
/*lint -esym(621, OKL4_PAGE_CACHE_WRITEBACK) */
#define OKL4_PAGE_CACHE_WRITEBACK ((okl4_page_cache_t)0x1U)
/*lint -esym(621, OKL4_PAGE_CACHE_IWB_RWA_ONC) */
#define OKL4_PAGE_CACHE_IWB_RWA_ONC ((okl4_page_cache_t)0x2U)
/*lint -esym(621, OKL4_PAGE_CACHE_WRITETHROUGH) */
#define OKL4_PAGE_CACHE_WRITETHROUGH ((okl4_page_cache_t)0x3U)
/*lint -esym(621, OKL4_PAGE_CACHE_DEVICE_GRE) */
#define OKL4_PAGE_CACHE_DEVICE_GRE ((okl4_page_cache_t)0x4U)
/*lint -esym(621, OKL4_PAGE_CACHE_DEVICE_NGRE) */
#define OKL4_PAGE_CACHE_DEVICE_NGRE ((okl4_page_cache_t)0x5U)
/*lint -esym(621, OKL4_PAGE_CACHE_DEVICE) */
#define OKL4_PAGE_CACHE_DEVICE ((okl4_page_cache_t)0x6U)
/*lint -esym(621, OKL4_PAGE_CACHE_STRONG) */
#define OKL4_PAGE_CACHE_STRONG ((okl4_page_cache_t)0x7U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_DEVICE_NGNRNE) */
#define OKL4_PAGE_CACHE_HW_DEVICE_NGNRNE ((okl4_page_cache_t)0x8000000U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_MASK) */
#define OKL4_PAGE_CACHE_HW_MASK ((okl4_page_cache_t)0x8000000U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_DEVICE_NGNRE) */
#define OKL4_PAGE_CACHE_HW_DEVICE_NGNRE ((okl4_page_cache_t)0x8000004U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_DEVICE_NGRE) */
#define OKL4_PAGE_CACHE_HW_DEVICE_NGRE ((okl4_page_cache_t)0x8000008U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_DEVICE_GRE) */
#define OKL4_PAGE_CACHE_HW_DEVICE_GRE ((okl4_page_cache_t)0x800000cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_TWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_TWT_WA_NSH ((okl4_page_cache_t)0x8000011U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OTWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OTWT_WA_NSH ((okl4_page_cache_t)0x8000012U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWT_WA_NSH ((okl4_page_cache_t)0x8000013U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OTWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_INC_OTWT_WA_NSH ((okl4_page_cache_t)0x8000014U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_WA_NSH ((okl4_page_cache_t)0x8000015U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_WA_NSH ((okl4_page_cache_t)0x8000016U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_WA_NSH ((okl4_page_cache_t)0x8000017U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_WA_NSH ((okl4_page_cache_t)0x8000018U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_WA_NSH ((okl4_page_cache_t)0x8000019U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_WA_NSH ((okl4_page_cache_t)0x800001aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_WA_NSH ((okl4_page_cache_t)0x800001bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_WA_NSH ((okl4_page_cache_t)0x800001cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_WA_NSH ((okl4_page_cache_t)0x800001dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_WA_NSH ((okl4_page_cache_t)0x800001eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_WA_NSH ((okl4_page_cache_t)0x800001fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OTWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OTWT_RA_NSH ((okl4_page_cache_t)0x8000021U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_TWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_TWT_RA_NSH ((okl4_page_cache_t)0x8000022U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWT_RA_NSH ((okl4_page_cache_t)0x8000023U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OTWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_INC_OTWT_RA_NSH ((okl4_page_cache_t)0x8000024U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_RA_NSH ((okl4_page_cache_t)0x8000025U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_RA_NSH ((okl4_page_cache_t)0x8000026U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_RA_NSH ((okl4_page_cache_t)0x8000027U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_RA_NSH ((okl4_page_cache_t)0x8000028U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_RA_NSH ((okl4_page_cache_t)0x8000029U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_RA_NSH ((okl4_page_cache_t)0x800002aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_RA_NSH ((okl4_page_cache_t)0x800002bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_RA_NSH ((okl4_page_cache_t)0x800002cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_RA_NSH ((okl4_page_cache_t)0x800002dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_RA_NSH ((okl4_page_cache_t)0x800002eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_RA_NSH ((okl4_page_cache_t)0x800002fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OTWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OTWT_RWA_NSH ((okl4_page_cache_t)0x8000031U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OTWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OTWT_RWA_NSH ((okl4_page_cache_t)0x8000032U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_TWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_TWT_RWA_NSH ((okl4_page_cache_t)0x8000033U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OTWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_INC_OTWT_RWA_NSH ((okl4_page_cache_t)0x8000034U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_RWA_NSH ((okl4_page_cache_t)0x8000035U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_RWA_NSH ((okl4_page_cache_t)0x8000036U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_RWA_NSH ((okl4_page_cache_t)0x8000037U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_RWA_NSH ((okl4_page_cache_t)0x8000038U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_RWA_NSH ((okl4_page_cache_t)0x8000039U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_RWA_NSH ((okl4_page_cache_t)0x800003aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_RWA_NSH ((okl4_page_cache_t)0x800003bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_RWA_NSH ((okl4_page_cache_t)0x800003cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_RWA_NSH ((okl4_page_cache_t)0x800003dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_RWA_NSH ((okl4_page_cache_t)0x800003eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_RWA_NSH ((okl4_page_cache_t)0x800003fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_ONC_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_ONC_NSH ((okl4_page_cache_t)0x8000041U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_ONC_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_ONC_NSH ((okl4_page_cache_t)0x8000042U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_ONC_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_ONC_NSH ((okl4_page_cache_t)0x8000043U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_NC_NSH) */
#define OKL4_PAGE_CACHE_HW_NC_NSH ((okl4_page_cache_t)0x8000044U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_ONC_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_ONC_NSH ((okl4_page_cache_t)0x8000045U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_ONC_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_ONC_NSH ((okl4_page_cache_t)0x8000046U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_ONC_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_ONC_NSH ((okl4_page_cache_t)0x8000047U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_ONC_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_ONC_NSH ((okl4_page_cache_t)0x8000048U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_ONC_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_ONC_NSH ((okl4_page_cache_t)0x8000049U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_ONC_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_ONC_NSH ((okl4_page_cache_t)0x800004aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_ONC_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_ONC_NSH ((okl4_page_cache_t)0x800004bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_ONC_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_ONC_NSH ((okl4_page_cache_t)0x800004cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_ONC_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_ONC_NSH ((okl4_page_cache_t)0x800004dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_ONC_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_ONC_NSH ((okl4_page_cache_t)0x800004eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_ONC_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_ONC_NSH ((okl4_page_cache_t)0x800004fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_WA_NSH ((okl4_page_cache_t)0x8000051U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_WA_NSH ((okl4_page_cache_t)0x8000052U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_WA_NSH ((okl4_page_cache_t)0x8000053U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OTWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_INC_OTWB_WA_NSH ((okl4_page_cache_t)0x8000054U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_TWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_TWB_WA_NSH ((okl4_page_cache_t)0x8000055U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OTWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OTWB_WA_NSH ((okl4_page_cache_t)0x8000056U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWB_WA_NSH ((okl4_page_cache_t)0x8000057U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_WA_NSH ((okl4_page_cache_t)0x8000058U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_WA_NSH ((okl4_page_cache_t)0x8000059U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_WA_NSH ((okl4_page_cache_t)0x800005aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_WA_NSH ((okl4_page_cache_t)0x800005bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_WA_NSH ((okl4_page_cache_t)0x800005cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_WA_NSH ((okl4_page_cache_t)0x800005dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_WA_NSH ((okl4_page_cache_t)0x800005eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_WA_NSH ((okl4_page_cache_t)0x800005fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_RA_NSH ((okl4_page_cache_t)0x8000061U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_RA_NSH ((okl4_page_cache_t)0x8000062U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_RA_NSH ((okl4_page_cache_t)0x8000063U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OTWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_INC_OTWB_RA_NSH ((okl4_page_cache_t)0x8000064U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OTWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OTWB_RA_NSH ((okl4_page_cache_t)0x8000065U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_TWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_TWB_RA_NSH ((okl4_page_cache_t)0x8000066U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWB_RA_NSH ((okl4_page_cache_t)0x8000067U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_RA_NSH ((okl4_page_cache_t)0x8000068U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_RA_NSH ((okl4_page_cache_t)0x8000069U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_RA_NSH ((okl4_page_cache_t)0x800006aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_RA_NSH ((okl4_page_cache_t)0x800006bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_RA_NSH ((okl4_page_cache_t)0x800006cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_RA_NSH ((okl4_page_cache_t)0x800006dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_RA_NSH ((okl4_page_cache_t)0x800006eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_RA_NSH ((okl4_page_cache_t)0x800006fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_RWA_NSH ((okl4_page_cache_t)0x8000071U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_RWA_NSH ((okl4_page_cache_t)0x8000072U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_RWA_NSH ((okl4_page_cache_t)0x8000073U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OTWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_INC_OTWB_RWA_NSH ((okl4_page_cache_t)0x8000074U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OTWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OTWB_RWA_NSH ((okl4_page_cache_t)0x8000075U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OTWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OTWB_RWA_NSH ((okl4_page_cache_t)0x8000076U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_TWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_TWB_RWA_NSH ((okl4_page_cache_t)0x8000077U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_RWA_NSH ((okl4_page_cache_t)0x8000078U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_RWA_NSH ((okl4_page_cache_t)0x8000079U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_RWA_NSH ((okl4_page_cache_t)0x800007aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_RWA_NSH ((okl4_page_cache_t)0x800007bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_RWA_NSH ((okl4_page_cache_t)0x800007cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_RWA_NSH ((okl4_page_cache_t)0x800007dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_RWA_NSH ((okl4_page_cache_t)0x800007eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_RWA_NSH ((okl4_page_cache_t)0x800007fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_NA_NSH ((okl4_page_cache_t)0x8000081U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_NA_NSH ((okl4_page_cache_t)0x8000082U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_NA_NSH ((okl4_page_cache_t)0x8000083U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OWT_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_INC_OWT_NA_NSH ((okl4_page_cache_t)0x8000084U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_NA_NSH ((okl4_page_cache_t)0x8000085U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_NA_NSH ((okl4_page_cache_t)0x8000086U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_NA_NSH ((okl4_page_cache_t)0x8000087U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_WT_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_WT_NA_NSH ((okl4_page_cache_t)0x8000088U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OWT_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OWT_NA_NSH ((okl4_page_cache_t)0x8000089U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OWT_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OWT_NA_NSH ((okl4_page_cache_t)0x800008aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_NA_NSH ((okl4_page_cache_t)0x800008bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OWT_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OWT_NA_NSH ((okl4_page_cache_t)0x800008cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OWT_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OWT_NA_NSH ((okl4_page_cache_t)0x800008dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OWT_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OWT_NA_NSH ((okl4_page_cache_t)0x800008eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_NA_NSH ((okl4_page_cache_t)0x800008fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_WA_NSH ((okl4_page_cache_t)0x8000091U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_WA_NSH ((okl4_page_cache_t)0x8000092U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_WA_NSH ((okl4_page_cache_t)0x8000093U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_INC_OWT_WA_NSH ((okl4_page_cache_t)0x8000094U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_WA_NSH ((okl4_page_cache_t)0x8000095U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_WA_NSH ((okl4_page_cache_t)0x8000096U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_WA_NSH ((okl4_page_cache_t)0x8000097U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OWT_WA_NSH ((okl4_page_cache_t)0x8000098U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_WT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_WT_WA_NSH ((okl4_page_cache_t)0x8000099U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OWT_WA_NSH ((okl4_page_cache_t)0x800009aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_WA_NSH ((okl4_page_cache_t)0x800009bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OWT_WA_NSH ((okl4_page_cache_t)0x800009cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OWT_WA_NSH ((okl4_page_cache_t)0x800009dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OWT_WA_NSH ((okl4_page_cache_t)0x800009eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_WA_NSH ((okl4_page_cache_t)0x800009fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_RA_NSH ((okl4_page_cache_t)0x80000a1U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_RA_NSH ((okl4_page_cache_t)0x80000a2U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_RA_NSH ((okl4_page_cache_t)0x80000a3U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_INC_OWT_RA_NSH ((okl4_page_cache_t)0x80000a4U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_RA_NSH ((okl4_page_cache_t)0x80000a5U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_RA_NSH ((okl4_page_cache_t)0x80000a6U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_RA_NSH ((okl4_page_cache_t)0x80000a7U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OWT_RA_NSH ((okl4_page_cache_t)0x80000a8U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OWT_RA_NSH ((okl4_page_cache_t)0x80000a9U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_WT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_WT_RA_NSH ((okl4_page_cache_t)0x80000aaU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_RA_NSH ((okl4_page_cache_t)0x80000abU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OWT_RA_NSH ((okl4_page_cache_t)0x80000acU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OWT_RA_NSH ((okl4_page_cache_t)0x80000adU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OWT_RA_NSH ((okl4_page_cache_t)0x80000aeU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_RA_NSH ((okl4_page_cache_t)0x80000afU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_RWA_NSH ((okl4_page_cache_t)0x80000b1U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_RWA_NSH ((okl4_page_cache_t)0x80000b2U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_RWA_NSH ((okl4_page_cache_t)0x80000b3U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_INC_OWT_RWA_NSH ((okl4_page_cache_t)0x80000b4U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_RWA_NSH ((okl4_page_cache_t)0x80000b5U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_RWA_NSH ((okl4_page_cache_t)0x80000b6U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_RWA_NSH ((okl4_page_cache_t)0x80000b7U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OWT_RWA_NSH ((okl4_page_cache_t)0x80000b8U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OWT_RWA_NSH ((okl4_page_cache_t)0x80000b9U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OWT_RWA_NSH ((okl4_page_cache_t)0x80000baU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_WT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_WT_RWA_NSH ((okl4_page_cache_t)0x80000bbU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OWT_RWA_NSH ((okl4_page_cache_t)0x80000bcU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OWT_RWA_NSH ((okl4_page_cache_t)0x80000bdU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OWT_RWA_NSH ((okl4_page_cache_t)0x80000beU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_RWA_NSH ((okl4_page_cache_t)0x80000bfU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_NA_NSH ((okl4_page_cache_t)0x80000c1U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_NA_NSH ((okl4_page_cache_t)0x80000c2U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_NA_NSH ((okl4_page_cache_t)0x80000c3U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OWB_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_INC_OWB_NA_NSH ((okl4_page_cache_t)0x80000c4U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_NA_NSH ((okl4_page_cache_t)0x80000c5U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_NA_NSH ((okl4_page_cache_t)0x80000c6U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_NA_NSH ((okl4_page_cache_t)0x80000c7U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OWB_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OWB_NA_NSH ((okl4_page_cache_t)0x80000c8U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OWB_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OWB_NA_NSH ((okl4_page_cache_t)0x80000c9U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OWB_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OWB_NA_NSH ((okl4_page_cache_t)0x80000caU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_NA_NSH ((okl4_page_cache_t)0x80000cbU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_WB_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_WB_NA_NSH ((okl4_page_cache_t)0x80000ccU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OWB_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OWB_NA_NSH ((okl4_page_cache_t)0x80000cdU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OWB_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OWB_NA_NSH ((okl4_page_cache_t)0x80000ceU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_NA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_NA_NSH ((okl4_page_cache_t)0x80000cfU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_WA_NSH ((okl4_page_cache_t)0x80000d1U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_WA_NSH ((okl4_page_cache_t)0x80000d2U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_WA_NSH ((okl4_page_cache_t)0x80000d3U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_INC_OWB_WA_NSH ((okl4_page_cache_t)0x80000d4U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_WA_NSH ((okl4_page_cache_t)0x80000d5U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_WA_NSH ((okl4_page_cache_t)0x80000d6U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_WA_NSH ((okl4_page_cache_t)0x80000d7U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OWB_WA_NSH ((okl4_page_cache_t)0x80000d8U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OWB_WA_NSH ((okl4_page_cache_t)0x80000d9U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OWB_WA_NSH ((okl4_page_cache_t)0x80000daU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_WA_NSH ((okl4_page_cache_t)0x80000dbU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OWB_WA_NSH ((okl4_page_cache_t)0x80000dcU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_WB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_WB_WA_NSH ((okl4_page_cache_t)0x80000ddU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OWB_WA_NSH ((okl4_page_cache_t)0x80000deU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_WA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_WA_NSH ((okl4_page_cache_t)0x80000dfU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_RA_NSH ((okl4_page_cache_t)0x80000e1U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_RA_NSH ((okl4_page_cache_t)0x80000e2U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_RA_NSH ((okl4_page_cache_t)0x80000e3U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_INC_OWB_RA_NSH ((okl4_page_cache_t)0x80000e4U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_RA_NSH ((okl4_page_cache_t)0x80000e5U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_RA_NSH ((okl4_page_cache_t)0x80000e6U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_RA_NSH ((okl4_page_cache_t)0x80000e7U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OWB_RA_NSH ((okl4_page_cache_t)0x80000e8U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OWB_RA_NSH ((okl4_page_cache_t)0x80000e9U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OWB_RA_NSH ((okl4_page_cache_t)0x80000eaU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_RA_NSH ((okl4_page_cache_t)0x80000ebU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OWB_RA_NSH ((okl4_page_cache_t)0x80000ecU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OWB_RA_NSH ((okl4_page_cache_t)0x80000edU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_WB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_WB_RA_NSH ((okl4_page_cache_t)0x80000eeU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_RA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_RA_NSH ((okl4_page_cache_t)0x80000efU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_RWA_NSH ((okl4_page_cache_t)0x80000f1U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_RWA_NSH ((okl4_page_cache_t)0x80000f2U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_RWA_NSH ((okl4_page_cache_t)0x80000f3U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_INC_OWB_RWA_NSH ((okl4_page_cache_t)0x80000f4U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_RWA_NSH ((okl4_page_cache_t)0x80000f5U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_RWA_NSH ((okl4_page_cache_t)0x80000f6U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_RWA_NSH ((okl4_page_cache_t)0x80000f7U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OWB_RWA_NSH ((okl4_page_cache_t)0x80000f8U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OWB_RWA_NSH ((okl4_page_cache_t)0x80000f9U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OWB_RWA_NSH ((okl4_page_cache_t)0x80000faU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_RWA_NSH ((okl4_page_cache_t)0x80000fbU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OWB_RWA_NSH ((okl4_page_cache_t)0x80000fcU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OWB_RWA_NSH ((okl4_page_cache_t)0x80000fdU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OWB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OWB_RWA_NSH ((okl4_page_cache_t)0x80000feU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_WB_RWA_NSH) */
#define OKL4_PAGE_CACHE_HW_WB_RWA_NSH ((okl4_page_cache_t)0x80000ffU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_TWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_TWT_WA_OSH ((okl4_page_cache_t)0x8000211U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OTWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OTWT_WA_OSH ((okl4_page_cache_t)0x8000212U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWT_WA_OSH ((okl4_page_cache_t)0x8000213U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OTWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_INC_OTWT_WA_OSH ((okl4_page_cache_t)0x8000214U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_WA_OSH ((okl4_page_cache_t)0x8000215U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_WA_OSH ((okl4_page_cache_t)0x8000216U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_WA_OSH ((okl4_page_cache_t)0x8000217U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_WA_OSH ((okl4_page_cache_t)0x8000218U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_WA_OSH ((okl4_page_cache_t)0x8000219U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_WA_OSH ((okl4_page_cache_t)0x800021aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_WA_OSH ((okl4_page_cache_t)0x800021bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_WA_OSH ((okl4_page_cache_t)0x800021cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_WA_OSH ((okl4_page_cache_t)0x800021dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_WA_OSH ((okl4_page_cache_t)0x800021eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_WA_OSH ((okl4_page_cache_t)0x800021fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OTWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OTWT_RA_OSH ((okl4_page_cache_t)0x8000221U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_TWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_TWT_RA_OSH ((okl4_page_cache_t)0x8000222U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWT_RA_OSH ((okl4_page_cache_t)0x8000223U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OTWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_INC_OTWT_RA_OSH ((okl4_page_cache_t)0x8000224U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_RA_OSH ((okl4_page_cache_t)0x8000225U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_RA_OSH ((okl4_page_cache_t)0x8000226U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_RA_OSH ((okl4_page_cache_t)0x8000227U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_RA_OSH ((okl4_page_cache_t)0x8000228U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_RA_OSH ((okl4_page_cache_t)0x8000229U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_RA_OSH ((okl4_page_cache_t)0x800022aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_RA_OSH ((okl4_page_cache_t)0x800022bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_RA_OSH ((okl4_page_cache_t)0x800022cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_RA_OSH ((okl4_page_cache_t)0x800022dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_RA_OSH ((okl4_page_cache_t)0x800022eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_RA_OSH ((okl4_page_cache_t)0x800022fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OTWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OTWT_RWA_OSH ((okl4_page_cache_t)0x8000231U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OTWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OTWT_RWA_OSH ((okl4_page_cache_t)0x8000232U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_TWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_TWT_RWA_OSH ((okl4_page_cache_t)0x8000233U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OTWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_INC_OTWT_RWA_OSH ((okl4_page_cache_t)0x8000234U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_RWA_OSH ((okl4_page_cache_t)0x8000235U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_RWA_OSH ((okl4_page_cache_t)0x8000236U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_RWA_OSH ((okl4_page_cache_t)0x8000237U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_RWA_OSH ((okl4_page_cache_t)0x8000238U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_RWA_OSH ((okl4_page_cache_t)0x8000239U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_RWA_OSH ((okl4_page_cache_t)0x800023aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_RWA_OSH ((okl4_page_cache_t)0x800023bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_RWA_OSH ((okl4_page_cache_t)0x800023cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_RWA_OSH ((okl4_page_cache_t)0x800023dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_RWA_OSH ((okl4_page_cache_t)0x800023eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_RWA_OSH ((okl4_page_cache_t)0x800023fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_ONC_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_ONC_OSH ((okl4_page_cache_t)0x8000241U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_ONC_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_ONC_OSH ((okl4_page_cache_t)0x8000242U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_ONC_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_ONC_OSH ((okl4_page_cache_t)0x8000243U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_NC_OSH) */
#define OKL4_PAGE_CACHE_HW_NC_OSH ((okl4_page_cache_t)0x8000244U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_ONC_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_ONC_OSH ((okl4_page_cache_t)0x8000245U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_ONC_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_ONC_OSH ((okl4_page_cache_t)0x8000246U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_ONC_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_ONC_OSH ((okl4_page_cache_t)0x8000247U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_ONC_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_ONC_OSH ((okl4_page_cache_t)0x8000248U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_ONC_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_ONC_OSH ((okl4_page_cache_t)0x8000249U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_ONC_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_ONC_OSH ((okl4_page_cache_t)0x800024aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_ONC_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_ONC_OSH ((okl4_page_cache_t)0x800024bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_ONC_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_ONC_OSH ((okl4_page_cache_t)0x800024cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_ONC_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_ONC_OSH ((okl4_page_cache_t)0x800024dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_ONC_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_ONC_OSH ((okl4_page_cache_t)0x800024eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_ONC_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_ONC_OSH ((okl4_page_cache_t)0x800024fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_WA_OSH ((okl4_page_cache_t)0x8000251U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_WA_OSH ((okl4_page_cache_t)0x8000252U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_WA_OSH ((okl4_page_cache_t)0x8000253U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OTWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_INC_OTWB_WA_OSH ((okl4_page_cache_t)0x8000254U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_TWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_TWB_WA_OSH ((okl4_page_cache_t)0x8000255U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OTWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OTWB_WA_OSH ((okl4_page_cache_t)0x8000256U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWB_WA_OSH ((okl4_page_cache_t)0x8000257U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_WA_OSH ((okl4_page_cache_t)0x8000258U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_WA_OSH ((okl4_page_cache_t)0x8000259U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_WA_OSH ((okl4_page_cache_t)0x800025aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_WA_OSH ((okl4_page_cache_t)0x800025bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_WA_OSH ((okl4_page_cache_t)0x800025cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_WA_OSH ((okl4_page_cache_t)0x800025dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_WA_OSH ((okl4_page_cache_t)0x800025eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_WA_OSH ((okl4_page_cache_t)0x800025fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_RA_OSH ((okl4_page_cache_t)0x8000261U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_RA_OSH ((okl4_page_cache_t)0x8000262U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_RA_OSH ((okl4_page_cache_t)0x8000263U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OTWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_INC_OTWB_RA_OSH ((okl4_page_cache_t)0x8000264U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OTWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OTWB_RA_OSH ((okl4_page_cache_t)0x8000265U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_TWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_TWB_RA_OSH ((okl4_page_cache_t)0x8000266U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWB_RA_OSH ((okl4_page_cache_t)0x8000267U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_RA_OSH ((okl4_page_cache_t)0x8000268U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_RA_OSH ((okl4_page_cache_t)0x8000269U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_RA_OSH ((okl4_page_cache_t)0x800026aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_RA_OSH ((okl4_page_cache_t)0x800026bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_RA_OSH ((okl4_page_cache_t)0x800026cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_RA_OSH ((okl4_page_cache_t)0x800026dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_RA_OSH ((okl4_page_cache_t)0x800026eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_RA_OSH ((okl4_page_cache_t)0x800026fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_RWA_OSH ((okl4_page_cache_t)0x8000271U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_RWA_OSH ((okl4_page_cache_t)0x8000272U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_RWA_OSH ((okl4_page_cache_t)0x8000273U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OTWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_INC_OTWB_RWA_OSH ((okl4_page_cache_t)0x8000274U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OTWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OTWB_RWA_OSH ((okl4_page_cache_t)0x8000275U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OTWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OTWB_RWA_OSH ((okl4_page_cache_t)0x8000276U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_TWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_TWB_RWA_OSH ((okl4_page_cache_t)0x8000277U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_RWA_OSH ((okl4_page_cache_t)0x8000278U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_RWA_OSH ((okl4_page_cache_t)0x8000279U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_RWA_OSH ((okl4_page_cache_t)0x800027aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_RWA_OSH ((okl4_page_cache_t)0x800027bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_RWA_OSH ((okl4_page_cache_t)0x800027cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_RWA_OSH ((okl4_page_cache_t)0x800027dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_RWA_OSH ((okl4_page_cache_t)0x800027eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_RWA_OSH ((okl4_page_cache_t)0x800027fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_NA_OSH ((okl4_page_cache_t)0x8000281U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_NA_OSH ((okl4_page_cache_t)0x8000282U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_NA_OSH ((okl4_page_cache_t)0x8000283U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OWT_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_INC_OWT_NA_OSH ((okl4_page_cache_t)0x8000284U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_NA_OSH ((okl4_page_cache_t)0x8000285U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_NA_OSH ((okl4_page_cache_t)0x8000286U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_NA_OSH ((okl4_page_cache_t)0x8000287U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_WT_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_WT_NA_OSH ((okl4_page_cache_t)0x8000288U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OWT_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OWT_NA_OSH ((okl4_page_cache_t)0x8000289U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OWT_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OWT_NA_OSH ((okl4_page_cache_t)0x800028aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_NA_OSH ((okl4_page_cache_t)0x800028bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OWT_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OWT_NA_OSH ((okl4_page_cache_t)0x800028cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OWT_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OWT_NA_OSH ((okl4_page_cache_t)0x800028dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OWT_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OWT_NA_OSH ((okl4_page_cache_t)0x800028eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_NA_OSH ((okl4_page_cache_t)0x800028fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_WA_OSH ((okl4_page_cache_t)0x8000291U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_WA_OSH ((okl4_page_cache_t)0x8000292U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_WA_OSH ((okl4_page_cache_t)0x8000293U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_INC_OWT_WA_OSH ((okl4_page_cache_t)0x8000294U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_WA_OSH ((okl4_page_cache_t)0x8000295U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_WA_OSH ((okl4_page_cache_t)0x8000296U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_WA_OSH ((okl4_page_cache_t)0x8000297U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OWT_WA_OSH ((okl4_page_cache_t)0x8000298U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_WT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_WT_WA_OSH ((okl4_page_cache_t)0x8000299U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OWT_WA_OSH ((okl4_page_cache_t)0x800029aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_WA_OSH ((okl4_page_cache_t)0x800029bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OWT_WA_OSH ((okl4_page_cache_t)0x800029cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OWT_WA_OSH ((okl4_page_cache_t)0x800029dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OWT_WA_OSH ((okl4_page_cache_t)0x800029eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_WA_OSH ((okl4_page_cache_t)0x800029fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_RA_OSH ((okl4_page_cache_t)0x80002a1U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_RA_OSH ((okl4_page_cache_t)0x80002a2U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_RA_OSH ((okl4_page_cache_t)0x80002a3U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_INC_OWT_RA_OSH ((okl4_page_cache_t)0x80002a4U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_RA_OSH ((okl4_page_cache_t)0x80002a5U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_RA_OSH ((okl4_page_cache_t)0x80002a6U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_RA_OSH ((okl4_page_cache_t)0x80002a7U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OWT_RA_OSH ((okl4_page_cache_t)0x80002a8U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OWT_RA_OSH ((okl4_page_cache_t)0x80002a9U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_WT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_WT_RA_OSH ((okl4_page_cache_t)0x80002aaU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_RA_OSH ((okl4_page_cache_t)0x80002abU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OWT_RA_OSH ((okl4_page_cache_t)0x80002acU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OWT_RA_OSH ((okl4_page_cache_t)0x80002adU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OWT_RA_OSH ((okl4_page_cache_t)0x80002aeU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_RA_OSH ((okl4_page_cache_t)0x80002afU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_RWA_OSH ((okl4_page_cache_t)0x80002b1U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_RWA_OSH ((okl4_page_cache_t)0x80002b2U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_RWA_OSH ((okl4_page_cache_t)0x80002b3U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_INC_OWT_RWA_OSH ((okl4_page_cache_t)0x80002b4U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_RWA_OSH ((okl4_page_cache_t)0x80002b5U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_RWA_OSH ((okl4_page_cache_t)0x80002b6U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_RWA_OSH ((okl4_page_cache_t)0x80002b7U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OWT_RWA_OSH ((okl4_page_cache_t)0x80002b8U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OWT_RWA_OSH ((okl4_page_cache_t)0x80002b9U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OWT_RWA_OSH ((okl4_page_cache_t)0x80002baU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_WT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_WT_RWA_OSH ((okl4_page_cache_t)0x80002bbU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OWT_RWA_OSH ((okl4_page_cache_t)0x80002bcU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OWT_RWA_OSH ((okl4_page_cache_t)0x80002bdU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OWT_RWA_OSH ((okl4_page_cache_t)0x80002beU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_RWA_OSH ((okl4_page_cache_t)0x80002bfU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_NA_OSH ((okl4_page_cache_t)0x80002c1U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_NA_OSH ((okl4_page_cache_t)0x80002c2U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_NA_OSH ((okl4_page_cache_t)0x80002c3U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OWB_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_INC_OWB_NA_OSH ((okl4_page_cache_t)0x80002c4U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_NA_OSH ((okl4_page_cache_t)0x80002c5U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_NA_OSH ((okl4_page_cache_t)0x80002c6U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_NA_OSH ((okl4_page_cache_t)0x80002c7U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OWB_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OWB_NA_OSH ((okl4_page_cache_t)0x80002c8U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OWB_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OWB_NA_OSH ((okl4_page_cache_t)0x80002c9U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OWB_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OWB_NA_OSH ((okl4_page_cache_t)0x80002caU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_NA_OSH ((okl4_page_cache_t)0x80002cbU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_WB_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_WB_NA_OSH ((okl4_page_cache_t)0x80002ccU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OWB_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OWB_NA_OSH ((okl4_page_cache_t)0x80002cdU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OWB_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OWB_NA_OSH ((okl4_page_cache_t)0x80002ceU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_NA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_NA_OSH ((okl4_page_cache_t)0x80002cfU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_WA_OSH ((okl4_page_cache_t)0x80002d1U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_WA_OSH ((okl4_page_cache_t)0x80002d2U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_WA_OSH ((okl4_page_cache_t)0x80002d3U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_INC_OWB_WA_OSH ((okl4_page_cache_t)0x80002d4U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_WA_OSH ((okl4_page_cache_t)0x80002d5U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_WA_OSH ((okl4_page_cache_t)0x80002d6U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_WA_OSH ((okl4_page_cache_t)0x80002d7U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OWB_WA_OSH ((okl4_page_cache_t)0x80002d8U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OWB_WA_OSH ((okl4_page_cache_t)0x80002d9U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OWB_WA_OSH ((okl4_page_cache_t)0x80002daU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_WA_OSH ((okl4_page_cache_t)0x80002dbU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OWB_WA_OSH ((okl4_page_cache_t)0x80002dcU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_WB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_WB_WA_OSH ((okl4_page_cache_t)0x80002ddU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OWB_WA_OSH ((okl4_page_cache_t)0x80002deU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_WA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_WA_OSH ((okl4_page_cache_t)0x80002dfU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_RA_OSH ((okl4_page_cache_t)0x80002e1U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_RA_OSH ((okl4_page_cache_t)0x80002e2U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_RA_OSH ((okl4_page_cache_t)0x80002e3U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_INC_OWB_RA_OSH ((okl4_page_cache_t)0x80002e4U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_RA_OSH ((okl4_page_cache_t)0x80002e5U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_RA_OSH ((okl4_page_cache_t)0x80002e6U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_RA_OSH ((okl4_page_cache_t)0x80002e7U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OWB_RA_OSH ((okl4_page_cache_t)0x80002e8U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OWB_RA_OSH ((okl4_page_cache_t)0x80002e9U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OWB_RA_OSH ((okl4_page_cache_t)0x80002eaU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_RA_OSH ((okl4_page_cache_t)0x80002ebU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OWB_RA_OSH ((okl4_page_cache_t)0x80002ecU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OWB_RA_OSH ((okl4_page_cache_t)0x80002edU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_WB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_WB_RA_OSH ((okl4_page_cache_t)0x80002eeU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_RA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_RA_OSH ((okl4_page_cache_t)0x80002efU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_RWA_OSH ((okl4_page_cache_t)0x80002f1U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_RWA_OSH ((okl4_page_cache_t)0x80002f2U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_RWA_OSH ((okl4_page_cache_t)0x80002f3U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_INC_OWB_RWA_OSH ((okl4_page_cache_t)0x80002f4U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_RWA_OSH ((okl4_page_cache_t)0x80002f5U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_RWA_OSH ((okl4_page_cache_t)0x80002f6U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_RWA_OSH ((okl4_page_cache_t)0x80002f7U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OWB_RWA_OSH ((okl4_page_cache_t)0x80002f8U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OWB_RWA_OSH ((okl4_page_cache_t)0x80002f9U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OWB_RWA_OSH ((okl4_page_cache_t)0x80002faU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_RWA_OSH ((okl4_page_cache_t)0x80002fbU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OWB_RWA_OSH ((okl4_page_cache_t)0x80002fcU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OWB_RWA_OSH ((okl4_page_cache_t)0x80002fdU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OWB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OWB_RWA_OSH ((okl4_page_cache_t)0x80002feU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_WB_RWA_OSH) */
#define OKL4_PAGE_CACHE_HW_WB_RWA_OSH ((okl4_page_cache_t)0x80002ffU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_TWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_TWT_WA_ISH ((okl4_page_cache_t)0x8000311U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OTWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OTWT_WA_ISH ((okl4_page_cache_t)0x8000312U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWT_WA_ISH ((okl4_page_cache_t)0x8000313U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OTWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_INC_OTWT_WA_ISH ((okl4_page_cache_t)0x8000314U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_WA_ISH ((okl4_page_cache_t)0x8000315U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_WA_ISH ((okl4_page_cache_t)0x8000316U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_WA_ISH ((okl4_page_cache_t)0x8000317U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_WA_ISH ((okl4_page_cache_t)0x8000318U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_WA_ISH ((okl4_page_cache_t)0x8000319U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_WA_ISH ((okl4_page_cache_t)0x800031aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_WA_ISH ((okl4_page_cache_t)0x800031bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_WA_ISH ((okl4_page_cache_t)0x800031cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_WA_ISH ((okl4_page_cache_t)0x800031dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_WA_ISH ((okl4_page_cache_t)0x800031eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_WA_ISH ((okl4_page_cache_t)0x800031fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OTWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OTWT_RA_ISH ((okl4_page_cache_t)0x8000321U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_TWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_TWT_RA_ISH ((okl4_page_cache_t)0x8000322U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWT_RA_ISH ((okl4_page_cache_t)0x8000323U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OTWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_INC_OTWT_RA_ISH ((okl4_page_cache_t)0x8000324U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_RA_ISH ((okl4_page_cache_t)0x8000325U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_RA_ISH ((okl4_page_cache_t)0x8000326U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_RA_ISH ((okl4_page_cache_t)0x8000327U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_RA_ISH ((okl4_page_cache_t)0x8000328U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_RA_ISH ((okl4_page_cache_t)0x8000329U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_RA_ISH ((okl4_page_cache_t)0x800032aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_RA_ISH ((okl4_page_cache_t)0x800032bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_RA_ISH ((okl4_page_cache_t)0x800032cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_RA_ISH ((okl4_page_cache_t)0x800032dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_RA_ISH ((okl4_page_cache_t)0x800032eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_RA_ISH ((okl4_page_cache_t)0x800032fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OTWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OTWT_RWA_ISH ((okl4_page_cache_t)0x8000331U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OTWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OTWT_RWA_ISH ((okl4_page_cache_t)0x8000332U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_TWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_TWT_RWA_ISH ((okl4_page_cache_t)0x8000333U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OTWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_INC_OTWT_RWA_ISH ((okl4_page_cache_t)0x8000334U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_RWA_ISH ((okl4_page_cache_t)0x8000335U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_RWA_ISH ((okl4_page_cache_t)0x8000336U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_RWA_ISH ((okl4_page_cache_t)0x8000337U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_RWA_ISH ((okl4_page_cache_t)0x8000338U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_RWA_ISH ((okl4_page_cache_t)0x8000339U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_RWA_ISH ((okl4_page_cache_t)0x800033aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_RWA_ISH ((okl4_page_cache_t)0x800033bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_RWA_ISH ((okl4_page_cache_t)0x800033cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_RWA_ISH ((okl4_page_cache_t)0x800033dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_RWA_ISH ((okl4_page_cache_t)0x800033eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_RWA_ISH ((okl4_page_cache_t)0x800033fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_ONC_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_ONC_ISH ((okl4_page_cache_t)0x8000341U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_ONC_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_ONC_ISH ((okl4_page_cache_t)0x8000342U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_ONC_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_ONC_ISH ((okl4_page_cache_t)0x8000343U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_NC_ISH) */
#define OKL4_PAGE_CACHE_HW_NC_ISH ((okl4_page_cache_t)0x8000344U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_ONC_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_ONC_ISH ((okl4_page_cache_t)0x8000345U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_ONC_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_ONC_ISH ((okl4_page_cache_t)0x8000346U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_ONC_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_ONC_ISH ((okl4_page_cache_t)0x8000347U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_ONC_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_ONC_ISH ((okl4_page_cache_t)0x8000348U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_ONC_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_ONC_ISH ((okl4_page_cache_t)0x8000349U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_ONC_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_ONC_ISH ((okl4_page_cache_t)0x800034aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_ONC_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_ONC_ISH ((okl4_page_cache_t)0x800034bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_ONC_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_ONC_ISH ((okl4_page_cache_t)0x800034cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_ONC_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_ONC_ISH ((okl4_page_cache_t)0x800034dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_ONC_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_ONC_ISH ((okl4_page_cache_t)0x800034eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_ONC_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_ONC_ISH ((okl4_page_cache_t)0x800034fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_WA_ISH ((okl4_page_cache_t)0x8000351U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_WA_ISH ((okl4_page_cache_t)0x8000352U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_WA_ISH ((okl4_page_cache_t)0x8000353U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OTWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_INC_OTWB_WA_ISH ((okl4_page_cache_t)0x8000354U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_TWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_TWB_WA_ISH ((okl4_page_cache_t)0x8000355U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OTWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OTWB_WA_ISH ((okl4_page_cache_t)0x8000356U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWB_WA_ISH ((okl4_page_cache_t)0x8000357U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_WA_ISH ((okl4_page_cache_t)0x8000358U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_WA_ISH ((okl4_page_cache_t)0x8000359U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_WA_ISH ((okl4_page_cache_t)0x800035aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_WA_ISH ((okl4_page_cache_t)0x800035bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_WA_ISH ((okl4_page_cache_t)0x800035cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_WA_ISH ((okl4_page_cache_t)0x800035dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_WA_ISH ((okl4_page_cache_t)0x800035eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_WA_ISH ((okl4_page_cache_t)0x800035fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_RA_ISH ((okl4_page_cache_t)0x8000361U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_RA_ISH ((okl4_page_cache_t)0x8000362U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_RA_ISH ((okl4_page_cache_t)0x8000363U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OTWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_INC_OTWB_RA_ISH ((okl4_page_cache_t)0x8000364U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OTWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OTWB_RA_ISH ((okl4_page_cache_t)0x8000365U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_TWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_TWB_RA_ISH ((okl4_page_cache_t)0x8000366U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWB_RA_ISH ((okl4_page_cache_t)0x8000367U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_RA_ISH ((okl4_page_cache_t)0x8000368U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_RA_ISH ((okl4_page_cache_t)0x8000369U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_RA_ISH ((okl4_page_cache_t)0x800036aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_RA_ISH ((okl4_page_cache_t)0x800036bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_RA_ISH ((okl4_page_cache_t)0x800036cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_RA_ISH ((okl4_page_cache_t)0x800036dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_RA_ISH ((okl4_page_cache_t)0x800036eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_RA_ISH ((okl4_page_cache_t)0x800036fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_RWA_ISH ((okl4_page_cache_t)0x8000371U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_RWA_ISH ((okl4_page_cache_t)0x8000372U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_RWA_ISH ((okl4_page_cache_t)0x8000373U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OTWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_INC_OTWB_RWA_ISH ((okl4_page_cache_t)0x8000374U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OTWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OTWB_RWA_ISH ((okl4_page_cache_t)0x8000375U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OTWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OTWB_RWA_ISH ((okl4_page_cache_t)0x8000376U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_TWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_TWB_RWA_ISH ((okl4_page_cache_t)0x8000377U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_RWA_ISH ((okl4_page_cache_t)0x8000378U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_RWA_ISH ((okl4_page_cache_t)0x8000379U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_RWA_ISH ((okl4_page_cache_t)0x800037aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_RWA_ISH ((okl4_page_cache_t)0x800037bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_RWA_ISH ((okl4_page_cache_t)0x800037cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_RWA_ISH ((okl4_page_cache_t)0x800037dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_RWA_ISH ((okl4_page_cache_t)0x800037eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_RWA_ISH ((okl4_page_cache_t)0x800037fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_NA_ISH ((okl4_page_cache_t)0x8000381U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_NA_ISH ((okl4_page_cache_t)0x8000382U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_NA_ISH ((okl4_page_cache_t)0x8000383U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OWT_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_INC_OWT_NA_ISH ((okl4_page_cache_t)0x8000384U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_NA_ISH ((okl4_page_cache_t)0x8000385U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_NA_ISH ((okl4_page_cache_t)0x8000386U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_NA_ISH ((okl4_page_cache_t)0x8000387U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_WT_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_WT_NA_ISH ((okl4_page_cache_t)0x8000388U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OWT_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OWT_NA_ISH ((okl4_page_cache_t)0x8000389U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OWT_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OWT_NA_ISH ((okl4_page_cache_t)0x800038aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_NA_ISH ((okl4_page_cache_t)0x800038bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OWT_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OWT_NA_ISH ((okl4_page_cache_t)0x800038cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OWT_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OWT_NA_ISH ((okl4_page_cache_t)0x800038dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OWT_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OWT_NA_ISH ((okl4_page_cache_t)0x800038eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_NA_ISH ((okl4_page_cache_t)0x800038fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_WA_ISH ((okl4_page_cache_t)0x8000391U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_WA_ISH ((okl4_page_cache_t)0x8000392U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_WA_ISH ((okl4_page_cache_t)0x8000393U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_INC_OWT_WA_ISH ((okl4_page_cache_t)0x8000394U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_WA_ISH ((okl4_page_cache_t)0x8000395U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_WA_ISH ((okl4_page_cache_t)0x8000396U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_WA_ISH ((okl4_page_cache_t)0x8000397U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OWT_WA_ISH ((okl4_page_cache_t)0x8000398U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_WT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_WT_WA_ISH ((okl4_page_cache_t)0x8000399U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OWT_WA_ISH ((okl4_page_cache_t)0x800039aU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_WA_ISH ((okl4_page_cache_t)0x800039bU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OWT_WA_ISH ((okl4_page_cache_t)0x800039cU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OWT_WA_ISH ((okl4_page_cache_t)0x800039dU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OWT_WA_ISH ((okl4_page_cache_t)0x800039eU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_WA_ISH ((okl4_page_cache_t)0x800039fU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_RA_ISH ((okl4_page_cache_t)0x80003a1U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_RA_ISH ((okl4_page_cache_t)0x80003a2U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_RA_ISH ((okl4_page_cache_t)0x80003a3U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_INC_OWT_RA_ISH ((okl4_page_cache_t)0x80003a4U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_RA_ISH ((okl4_page_cache_t)0x80003a5U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_RA_ISH ((okl4_page_cache_t)0x80003a6U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_RA_ISH ((okl4_page_cache_t)0x80003a7U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OWT_RA_ISH ((okl4_page_cache_t)0x80003a8U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OWT_RA_ISH ((okl4_page_cache_t)0x80003a9U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_WT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_WT_RA_ISH ((okl4_page_cache_t)0x80003aaU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_RA_ISH ((okl4_page_cache_t)0x80003abU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OWT_RA_ISH ((okl4_page_cache_t)0x80003acU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OWT_RA_ISH ((okl4_page_cache_t)0x80003adU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OWT_RA_ISH ((okl4_page_cache_t)0x80003aeU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_RA_ISH ((okl4_page_cache_t)0x80003afU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_RWA_ISH ((okl4_page_cache_t)0x80003b1U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_RWA_ISH ((okl4_page_cache_t)0x80003b2U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_RWA_ISH ((okl4_page_cache_t)0x80003b3U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_INC_OWT_RWA_ISH ((okl4_page_cache_t)0x80003b4U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_RWA_ISH ((okl4_page_cache_t)0x80003b5U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_RWA_ISH ((okl4_page_cache_t)0x80003b6U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_RWA_ISH ((okl4_page_cache_t)0x80003b7U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OWT_RWA_ISH ((okl4_page_cache_t)0x80003b8U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OWT_RWA_ISH ((okl4_page_cache_t)0x80003b9U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OWT_RWA_ISH ((okl4_page_cache_t)0x80003baU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_WT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_WT_RWA_ISH ((okl4_page_cache_t)0x80003bbU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OWT_RWA_ISH ((okl4_page_cache_t)0x80003bcU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OWT_RWA_ISH ((okl4_page_cache_t)0x80003bdU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OWT_RWA_ISH ((okl4_page_cache_t)0x80003beU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_RWA_ISH ((okl4_page_cache_t)0x80003bfU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_NA_ISH ((okl4_page_cache_t)0x80003c1U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_NA_ISH ((okl4_page_cache_t)0x80003c2U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_NA_ISH ((okl4_page_cache_t)0x80003c3U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OWB_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_INC_OWB_NA_ISH ((okl4_page_cache_t)0x80003c4U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_NA_ISH ((okl4_page_cache_t)0x80003c5U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_NA_ISH ((okl4_page_cache_t)0x80003c6U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_NA_ISH ((okl4_page_cache_t)0x80003c7U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OWB_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OWB_NA_ISH ((okl4_page_cache_t)0x80003c8U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OWB_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OWB_NA_ISH ((okl4_page_cache_t)0x80003c9U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OWB_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OWB_NA_ISH ((okl4_page_cache_t)0x80003caU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_NA_ISH ((okl4_page_cache_t)0x80003cbU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_WB_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_WB_NA_ISH ((okl4_page_cache_t)0x80003ccU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OWB_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OWB_NA_ISH ((okl4_page_cache_t)0x80003cdU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OWB_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OWB_NA_ISH ((okl4_page_cache_t)0x80003ceU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_NA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_NA_ISH ((okl4_page_cache_t)0x80003cfU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_WA_ISH ((okl4_page_cache_t)0x80003d1U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_WA_ISH ((okl4_page_cache_t)0x80003d2U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_WA_ISH ((okl4_page_cache_t)0x80003d3U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_INC_OWB_WA_ISH ((okl4_page_cache_t)0x80003d4U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_WA_ISH ((okl4_page_cache_t)0x80003d5U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_WA_ISH ((okl4_page_cache_t)0x80003d6U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_WA_ISH ((okl4_page_cache_t)0x80003d7U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OWB_WA_ISH ((okl4_page_cache_t)0x80003d8U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OWB_WA_ISH ((okl4_page_cache_t)0x80003d9U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OWB_WA_ISH ((okl4_page_cache_t)0x80003daU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_WA_ISH ((okl4_page_cache_t)0x80003dbU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OWB_WA_ISH ((okl4_page_cache_t)0x80003dcU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_WB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_WB_WA_ISH ((okl4_page_cache_t)0x80003ddU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OWB_WA_ISH ((okl4_page_cache_t)0x80003deU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_WA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_WA_ISH ((okl4_page_cache_t)0x80003dfU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_RA_ISH ((okl4_page_cache_t)0x80003e1U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_RA_ISH ((okl4_page_cache_t)0x80003e2U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_RA_ISH ((okl4_page_cache_t)0x80003e3U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_INC_OWB_RA_ISH ((okl4_page_cache_t)0x80003e4U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_RA_ISH ((okl4_page_cache_t)0x80003e5U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_RA_ISH ((okl4_page_cache_t)0x80003e6U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_RA_ISH ((okl4_page_cache_t)0x80003e7U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OWB_RA_ISH ((okl4_page_cache_t)0x80003e8U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OWB_RA_ISH ((okl4_page_cache_t)0x80003e9U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OWB_RA_ISH ((okl4_page_cache_t)0x80003eaU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_RA_ISH ((okl4_page_cache_t)0x80003ebU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OWB_RA_ISH ((okl4_page_cache_t)0x80003ecU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OWB_RA_ISH ((okl4_page_cache_t)0x80003edU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_WB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_WB_RA_ISH ((okl4_page_cache_t)0x80003eeU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_RA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_RA_ISH ((okl4_page_cache_t)0x80003efU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_RWA_ISH ((okl4_page_cache_t)0x80003f1U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_RWA_ISH ((okl4_page_cache_t)0x80003f2U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_RWA_ISH ((okl4_page_cache_t)0x80003f3U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_INC_OWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_INC_OWB_RWA_ISH ((okl4_page_cache_t)0x80003f4U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_RWA_ISH ((okl4_page_cache_t)0x80003f5U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_RWA_ISH ((okl4_page_cache_t)0x80003f6U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_RWA_ISH ((okl4_page_cache_t)0x80003f7U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_NA_OWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_NA_OWB_RWA_ISH ((okl4_page_cache_t)0x80003f8U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_WA_OWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_WA_OWB_RWA_ISH ((okl4_page_cache_t)0x80003f9U)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RA_OWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RA_OWB_RWA_ISH ((okl4_page_cache_t)0x80003faU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_RWA_ISH ((okl4_page_cache_t)0x80003fbU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_NA_OWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_NA_OWB_RWA_ISH ((okl4_page_cache_t)0x80003fcU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_WA_OWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_WA_OWB_RWA_ISH ((okl4_page_cache_t)0x80003fdU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_IWB_RA_OWB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_IWB_RA_OWB_RWA_ISH ((okl4_page_cache_t)0x80003feU)
/*lint -esym(621, OKL4_PAGE_CACHE_HW_WB_RWA_ISH) */
#define OKL4_PAGE_CACHE_HW_WB_RWA_ISH ((okl4_page_cache_t)0x80003ffU)
/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_PAGE_CACHE_MAX) */
#define OKL4_PAGE_CACHE_MAX ((okl4_page_cache_t)0x80003ffU)
/**
    Invalid enumeration value
*/
/*lint -esym(621, OKL4_PAGE_CACHE_INVALID) */
#define OKL4_PAGE_CACHE_INVALID ((okl4_page_cache_t)0xffffffffU)

/*lint -esym(714, okl4_page_cache_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_page_cache_is_element_of(okl4_page_cache_t var);


/*lint -esym(714, okl4_page_cache_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_page_cache_is_element_of(okl4_page_cache_t var)
{
    /*lint --e{944} Disable dead expression detection */
    /*lint --e{948} --e{845} Disable constant always zero */
    return ((var == OKL4_PAGE_CACHE_WRITECOMBINE) ||
            (var == OKL4_PAGE_CACHE_DEFAULT) ||
            (var == OKL4_PAGE_CACHE_IPC_RX) ||
            (var == OKL4_PAGE_CACHE_IPC_TX) ||
            (var == OKL4_PAGE_CACHE_TRACEBUFFER) ||
            (var == OKL4_PAGE_CACHE_WRITEBACK) ||
            (var == OKL4_PAGE_CACHE_IWB_RWA_ONC) ||
            (var == OKL4_PAGE_CACHE_WRITETHROUGH) ||
            (var == OKL4_PAGE_CACHE_DEVICE_GRE) ||
            (var == OKL4_PAGE_CACHE_DEVICE_NGRE) ||
            (var == OKL4_PAGE_CACHE_DEVICE) ||
            (var == OKL4_PAGE_CACHE_STRONG) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_WB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_TWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_ONC_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_ONC_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OTWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_ONC_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OWT_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OTWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_ONC_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OTWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_ONC_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_TWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OTWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_WT_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_TWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OTWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_ONC_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OTWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OTWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_ONC_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OWT_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_ONC_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_ONC_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OTWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_TWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_DEVICE_NGNRE) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OWT_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_DEVICE_GRE) ||
            (var == OKL4_PAGE_CACHE_HW_TWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_WT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OTWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_WB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OWB_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OTWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OWB_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OWB_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OTWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OWT_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_WT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OTWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_ONC_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_ONC_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OTWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_NC_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OWT_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_ONC_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_ONC_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_ONC_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_ONC_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_ONC_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OWB_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_WT_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_ONC_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OWB_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_ONC_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_WB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_ONC_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OTWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OTWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OWB_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OWT_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OTWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_ONC_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_WB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_WT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OWB_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_ONC_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_TWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OTWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_ONC_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OWT_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OWB_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_ONC_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OWT_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_ONC_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OTWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OTWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_TWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OWT_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OWB_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_TWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OWB_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OTWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_WB_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_DEVICE_NGNRNE) ||
            (var == OKL4_PAGE_CACHE_HW_WB_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OTWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OTWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_WB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OTWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OWT_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OTWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_ONC_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_WT_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_ONC_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_ONC_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OTWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OTWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OTWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OTWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OTWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OTWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_ONC_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_TWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_TWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OTWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_WB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_ONC_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_ONC_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_TWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OTWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_ONC_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OTWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_ONC_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_TWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OWT_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OWT_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OTWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OTWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OWT_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_WB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OTWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OWB_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_ONC_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OWT_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_TWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OWB_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OTWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OWB_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_WB_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_ONC_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OTWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_DEVICE_NGRE) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_TWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OTWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_ONC_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_WB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OWB_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_ONC_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OWT_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_WT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_TWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_NC_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OWB_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OTWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OWB_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_WT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_ONC_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OTWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OTWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OTWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OTWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OTWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_TWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_WB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OTWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_ONC_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_TWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_NC_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_WT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OTWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_ONC_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OTWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_WT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_ONC_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_TWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_INC_OWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OWT_NA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OTWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OTWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_NA_OTWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OTWB_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_WT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OTWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OWT_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RA_OTWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OWT_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_OWT_RWA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OWT_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OWT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OTWB_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OTWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RWA_ONC_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_ONC_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RWA_OTWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_NA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_WA_OWB_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RA_OWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OTWT_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OWT_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_WT_RWA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OTWB_WA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWT_RA_OTWB_RA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_RWA_OWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OTWT_RA_ISH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_NA_OTWT_WA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWB_WA_OTWB_WA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_WA_OTWT_RA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_WA_OWB_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_IWT_RA_OWB_NA_OSH) ||
            (var == OKL4_PAGE_CACHE_HW_ITWB_RWA_OWB_RWA_NSH) ||
            (var == OKL4_PAGE_CACHE_HW_MASK));
}



typedef uint32_t okl4_cell_id_t;





typedef char okl4_char_t;








/**
    The `okl4_string_t` type represents a constant C string of type
    'const char *'.
*/

typedef const okl4_char_t *okl4_string_t;












/**

*/

struct okl4_range_item {
    okl4_laddr_t base;
    okl4_lsize_t size;
};




/**

*/

struct okl4_virtmem_item {
    struct okl4_range_item range;
};




/**

*/

struct okl4_cell_management_item {
    okl4_laddr_t entry;
    struct okl4_virtmem_item mapping_range;
    __ptr64(void *, data);
    __ptr64(okl4_string_t, image);
    okl4_kcap_t mmu;
    _okl4_padding_t __padding0_4;
    _okl4_padding_t __padding1_5;
    _okl4_padding_t __padding2_6;
    _okl4_padding_t __padding3_7;
    __ptr64(okl4_string_t, name);
    okl4_kcap_t registers_cap;
    okl4_kcap_t reset_virq;
    okl4_count_t segment_index;
    _okl4_padding_t __padding4_4;
    _okl4_padding_t __padding5_5;
    _okl4_padding_t __padding6_6;
    _okl4_padding_t __padding7_7;
    __ptr64(struct okl4_cell_management_segments *, segments);
    __ptr64(struct okl4_cell_management_vcpus *, vcpus);
    okl4_bool_t boot_once;
    okl4_bool_t can_stop;
    okl4_bool_t deferred;
    okl4_bool_t detached;
    okl4_bool_t erase;
    _okl4_padding_t __padding8_5;
    _okl4_padding_t __padding9_6;
    _okl4_padding_t __padding10_7;
    okl4_laddr_t dtb_address;
};




/**

*/

struct okl4_cell_management {
    okl4_count_t num_items;
    _okl4_padding_t __padding0_4;
    _okl4_padding_t __padding1_5;
    _okl4_padding_t __padding2_6;
    _okl4_padding_t __padding3_7;
    struct okl4_cell_management_item items[]; /*lint --e{9038} flex array */
};




/**
    The `okl4_paddr_t` type represents an unsigned integer value which is large
    enough to contain a machine-native physical address.
*/

typedef okl4_psize_t okl4_paddr_t;




/**

*/

struct okl4_segment_mapping {
    okl4_paddr_t phys_addr;
    okl4_psize_t size;
    okl4_laddr_t virt_addr;
    okl4_kcap_t cap;
    okl4_bool_t device;
    okl4_bool_t owned;
    _okl4_padding_t __padding0_6;
    _okl4_padding_t __padding1_7;
};




/**

*/

struct okl4_cell_management_segments {
    okl4_count_t free_segments;
    okl4_count_t num_segments;
    struct okl4_segment_mapping segment_mappings[]; /*lint --e{9038} flex array */
};




/**

*/

struct okl4_cell_management_vcpus {
    okl4_count_t num_vcpus;
    okl4_kcap_t vcpu_caps[]; /*lint --e{9038} flex array */
};




/**
    CPU instruction set
*/

typedef uint32_t okl4_cpu_exec_mode;

/*lint -esym(621, OKL4_ARM_MODE) */
#define OKL4_ARM_MODE ((okl4_cpu_exec_mode)(0U))

/*lint -esym(621, OKL4_DEFAULT_MODE) */
#define OKL4_DEFAULT_MODE ((okl4_cpu_exec_mode)(4U))

/*lint -esym(621, OKL4_JAZELLE_MODE) */
#define OKL4_JAZELLE_MODE ((okl4_cpu_exec_mode)(2U))

/*lint -esym(621, OKL4_THUMBEE_MODE) */
#define OKL4_THUMBEE_MODE ((okl4_cpu_exec_mode)(3U))

/*lint -esym(621, OKL4_THUMB_MODE) */
#define OKL4_THUMB_MODE ((okl4_cpu_exec_mode)(1U))



/**
    CPU mode specifier

    - BITS 2..0 -   @ref OKL4_MASK_EXEC_MODE_CPU_MODE
    - BIT 7 -   @ref OKL4_MASK_ENDIAN_CPU_MODE
*/

/*lint -esym(621, okl4_cpu_mode_t) */
typedef uint32_t okl4_cpu_mode_t;

/*lint -esym(621, okl4_cpu_mode_getexecmode) */
/*lint -esym(714, okl4_cpu_mode_getexecmode) */
OKL4_FORCE_INLINE okl4_cpu_exec_mode
okl4_cpu_mode_getexecmode(const okl4_cpu_mode_t *x);

/*lint -esym(621, okl4_cpu_mode_setexecmode) */
OKL4_FORCE_INLINE void
okl4_cpu_mode_setexecmode(okl4_cpu_mode_t *x, okl4_cpu_exec_mode _exec_mode);

/*lint -esym(621, okl4_cpu_mode_getendian) */
/*lint -esym(714, okl4_cpu_mode_getendian) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_cpu_mode_getendian(const okl4_cpu_mode_t *x);

/*lint -esym(621, okl4_cpu_mode_setendian) */
OKL4_FORCE_INLINE void
okl4_cpu_mode_setendian(okl4_cpu_mode_t *x, okl4_bool_t _endian);

/*lint -esym(714, okl4_cpu_mode_init) */
OKL4_FORCE_INLINE void
okl4_cpu_mode_init(okl4_cpu_mode_t *x);

/*lint -esym(714, okl4_cpu_mode_cast) */
OKL4_FORCE_INLINE okl4_cpu_mode_t
okl4_cpu_mode_cast(uint32_t p, okl4_bool_t force);




/*lint -esym(621, OKL4_CPU_MODE_EXEC_MODE_MASK) */
#define OKL4_CPU_MODE_EXEC_MODE_MASK ((okl4_cpu_mode_t)7U) /* Deprecated */
/*lint -esym(621, OKL4_MASK_EXEC_MODE_CPU_MODE) */
#define OKL4_MASK_EXEC_MODE_CPU_MODE ((okl4_cpu_mode_t)7U)
/*lint -esym(621, OKL4_SHIFT_EXEC_MODE_CPU_MODE) */
#define OKL4_SHIFT_EXEC_MODE_CPU_MODE (0)
/*lint -esym(621, OKL4_WIDTH_EXEC_MODE_CPU_MODE) */
#define OKL4_WIDTH_EXEC_MODE_CPU_MODE (3)
/*lint -esym(621, OKL4_CPU_MODE_ENDIAN_MASK) */
#define OKL4_CPU_MODE_ENDIAN_MASK ((okl4_cpu_mode_t)1U << 7) /* Deprecated */
/*lint -esym(621, OKL4_MASK_ENDIAN_CPU_MODE) */
#define OKL4_MASK_ENDIAN_CPU_MODE ((okl4_cpu_mode_t)1U << 7)
/*lint -esym(621, OKL4_SHIFT_ENDIAN_CPU_MODE) */
#define OKL4_SHIFT_ENDIAN_CPU_MODE (7)
/*lint -esym(621, OKL4_WIDTH_ENDIAN_CPU_MODE) */
#define OKL4_WIDTH_ENDIAN_CPU_MODE (1)


/*lint -sem(okl4_cpu_mode_getexecmode, 1p, @n >= 0 && @n <= 7) */
/*lint -esym(621, okl4_cpu_mode_getexecmode) */
/*lint -esym(714, okl4_cpu_mode_getexecmode) */
OKL4_FORCE_INLINE okl4_cpu_exec_mode
okl4_cpu_mode_getexecmode(const okl4_cpu_mode_t *x)
{
    okl4_cpu_exec_mode field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t field : 3;
        } bits;
        okl4_cpu_mode_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_cpu_exec_mode)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_cpu_mode_setexecmode, 2n >= 0 && 2n <= 7) */
/*lint -esym(714, okl4_cpu_mode_setexecmode) */

/*lint -esym(621, okl4_cpu_mode_setexecmode) */
OKL4_FORCE_INLINE void
okl4_cpu_mode_setexecmode(okl4_cpu_mode_t *x, okl4_cpu_exec_mode _exec_mode)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t field : 3;
        } bits;
        okl4_cpu_mode_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_exec_mode;
    *x = _conv.raw;
}
/*lint -sem(okl4_cpu_mode_getendian, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_cpu_mode_getendian) */
/*lint -esym(714, okl4_cpu_mode_getendian) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_cpu_mode_getendian(const okl4_cpu_mode_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 7;
            _Bool field : 1;
        } bits;
        okl4_cpu_mode_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_cpu_mode_setendian, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_cpu_mode_setendian) */

/*lint -esym(621, okl4_cpu_mode_setendian) */
OKL4_FORCE_INLINE void
okl4_cpu_mode_setendian(okl4_cpu_mode_t *x, okl4_bool_t _endian)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 7;
            _Bool field : 1;
        } bits;
        okl4_cpu_mode_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_endian;
    *x = _conv.raw;
}
/*lint -esym(714, okl4_cpu_mode_init) */
OKL4_FORCE_INLINE void
okl4_cpu_mode_init(okl4_cpu_mode_t *x)
{
    *x = (okl4_cpu_mode_t)0U;
}

/*lint -esym(714, okl4_cpu_mode_cast) */
OKL4_FORCE_INLINE okl4_cpu_mode_t
okl4_cpu_mode_cast(uint32_t p, okl4_bool_t force)
{
    okl4_cpu_mode_t x = (okl4_cpu_mode_t)p;
    (void)force;
    return x;
}




struct _okl4_env_hdr {
    uint16_t magic;
    uint16_t count;
};







struct _okl4_env_item {
    __ptr64(okl4_string_t, name);
    __ptr64(void *, item);
};






/**
    The OKL4 environment.  It is a dictionary that maps strings to
    arbitary objects.  The content of the environment is defined
    during system construction time, and is read-only during run
    time.
*/

struct _okl4_env {
    struct _okl4_env_hdr env_hdr;
    _okl4_padding_t __padding0_4;
    _okl4_padding_t __padding1_5;
    _okl4_padding_t __padding2_6;
    _okl4_padding_t __padding3_7;
    struct _okl4_env_item env_item[]; /*lint --e{9038} flex array */
};




/**

*/

struct okl4_env_access_cell {
    __ptr64(okl4_string_t, name);
    okl4_count_t num_entries;
    okl4_count_t start_entry;
};




/**
    The okl4_page_perms_t object represents a set of access permissions for
    page mappings.

    - @ref OKL4_PAGE_PERMS_NONE
    - @ref OKL4_PAGE_PERMS_X
    - @ref OKL4_PAGE_PERMS_W
    - @ref OKL4_PAGE_PERMS_WX
    - @ref OKL4_PAGE_PERMS_R
    - @ref OKL4_PAGE_PERMS_RX
    - @ref OKL4_PAGE_PERMS_RW
    - @ref OKL4_PAGE_PERMS_RWX
    - @ref OKL4_PAGE_PERMS_MAX
    - @ref OKL4_PAGE_PERMS_INVALID
*/

typedef uint32_t okl4_page_perms_t;

/*lint -esym(621, OKL4_PAGE_PERMS_NONE) */
#define OKL4_PAGE_PERMS_NONE ((okl4_page_perms_t)0x0U)
/*lint -esym(621, OKL4_PAGE_PERMS_X) */
#define OKL4_PAGE_PERMS_X ((okl4_page_perms_t)0x1U)
/*lint -esym(621, OKL4_PAGE_PERMS_W) */
#define OKL4_PAGE_PERMS_W ((okl4_page_perms_t)0x2U)
/*lint -esym(621, OKL4_PAGE_PERMS_WX) */
#define OKL4_PAGE_PERMS_WX ((okl4_page_perms_t)0x3U)
/*lint -esym(621, OKL4_PAGE_PERMS_R) */
#define OKL4_PAGE_PERMS_R ((okl4_page_perms_t)0x4U)
/*lint -esym(621, OKL4_PAGE_PERMS_RX) */
#define OKL4_PAGE_PERMS_RX ((okl4_page_perms_t)0x5U)
/*lint -esym(621, OKL4_PAGE_PERMS_RW) */
#define OKL4_PAGE_PERMS_RW ((okl4_page_perms_t)0x6U)
/*lint -esym(621, OKL4_PAGE_PERMS_RWX) */
#define OKL4_PAGE_PERMS_RWX ((okl4_page_perms_t)0x7U)
/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_PAGE_PERMS_MAX) */
#define OKL4_PAGE_PERMS_MAX ((okl4_page_perms_t)0x7U)
/**
    Invalid enumeration value
*/
/*lint -esym(621, OKL4_PAGE_PERMS_INVALID) */
#define OKL4_PAGE_PERMS_INVALID ((okl4_page_perms_t)0xffffffffU)

/*lint -esym(714, okl4_page_perms_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_page_perms_is_element_of(okl4_page_perms_t var);


/*lint -esym(714, okl4_page_perms_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_page_perms_is_element_of(okl4_page_perms_t var)
{
    /*lint --e{944} Disable dead expression detection */
    /*lint --e{948} --e{845} Disable constant always zero */
    return ((var == OKL4_PAGE_PERMS_NONE) ||
            (var == OKL4_PAGE_PERMS_X) ||
            (var == OKL4_PAGE_PERMS_W) ||
            (var == OKL4_PAGE_PERMS_WX) ||
            (var == OKL4_PAGE_PERMS_R) ||
            (var == OKL4_PAGE_PERMS_RX) ||
            (var == OKL4_PAGE_PERMS_RW) ||
            (var == OKL4_PAGE_PERMS_RWX));
}


/**

*/

struct okl4_env_access_entry {
    okl4_laddr_t virtual_address;
    okl4_psize_t offset;
    okl4_psize_t size;
    okl4_count_t num_segs;
    okl4_count_t segment_index;
    okl4_page_cache_t cache_attrs;
    okl4_page_perms_t permissions;
    __ptr64(okl4_string_t, object_name);
};




/**

*/

struct okl4_env_access_table {
    okl4_count_t num_cells;
    _okl4_padding_t __padding0_4;
    _okl4_padding_t __padding1_5;
    _okl4_padding_t __padding2_6;
    _okl4_padding_t __padding3_7;
    __ptr64(struct okl4_env_access_cell *, cells);
    __ptr64(struct okl4_env_access_entry *, entries);
};




/**
    This object contains command-line arguments passed to
    user-level programs.
*/

struct okl4_env_args {
    okl4_count_t argc;
    _okl4_padding_t __padding0_4;
    _okl4_padding_t __padding1_5;
    _okl4_padding_t __padding2_6;
    _okl4_padding_t __padding3_7;
    __ptr64_array(okl4_string_t, argv)[]; /*lint --e{9038} flex array */
};




/**
    The okl4_env_interrupt_device_map_t type represents a list of interrupt
    numbers (IRQs) that are connected to a given peripheral
    device.  Objects of this type are typically obtained from
    the OKL4 environment.
*/

struct okl4_env_interrupt_device_map {
    okl4_count_t num_entries;
    okl4_interrupt_number_t entries[]; /*lint --e{9038} flex array */
};




/**
    The okl4_interrupt_t structure is used to represent a kernel interrupt
    object.
*/

struct okl4_interrupt {
    okl4_kcap_t kcap;
};




/**
    The okl4_env_interrupt_handle_t type stores the information required to
    perform operations on a interrupt.
*/

struct okl4_env_interrupt_handle {
    okl4_interrupt_number_t descriptor;
    struct okl4_interrupt interrupt;
};




/**
    The okl4_env_interrupt_list_t type stores a list of interrupt handle objects
    which represent all the interrupts that are available to the cell.
    Objects of this type are typically obtained from
    the OKL4 environment.
*/

struct okl4_env_interrupt_list {
    okl4_count_t num_entries;
    _okl4_padding_t __padding0_4;
    _okl4_padding_t __padding1_5;
    _okl4_padding_t __padding2_6;
    _okl4_padding_t __padding3_7;
    __ptr64(okl4_interrupt_number_t *, descriptor);
    __ptr64(struct okl4_interrupt *, interrupt);
};




/**

*/

struct okl4_env_profile_cell {
    okl4_char_t name[32];
    okl4_count_t num_cores;
    _okl4_padding_t __padding0_4;
    _okl4_padding_t __padding1_5;
    _okl4_padding_t __padding2_6;
    _okl4_padding_t __padding3_7;
    __ptr64(struct okl4_env_profile_cpu *, core);
};




/**

*/

struct okl4_env_profile_cpu {
    okl4_kcap_t cap;
};




/**

*/

struct okl4_env_profile_table {
    okl4_count_t num_cell_entries;
    okl4_count_t pcpu_cell_entry;
    __ptr64(struct okl4_env_profile_cell *, cells);
};




/**

*/

struct okl4_env_segment {
    okl4_paddr_t base;
    okl4_psize_t size;
    okl4_kcap_t cap_id;
    okl4_page_perms_t rwx;
};




/**

*/

struct okl4_env_segment_table {
    okl4_count_t num_segments;
    _okl4_padding_t __padding0_4;
    _okl4_padding_t __padding1_5;
    _okl4_padding_t __padding2_6;
    _okl4_padding_t __padding3_7;
    struct okl4_env_segment segments[]; /*lint --e{9038} flex array */
};




/**
    The `okl4_error_t` type represents an error condition returned by the
    OKL4 API.

    See OKL4_ERROR_*

    - @ref OKL4_ERROR_KSP_OK
    - @ref OKL4_ERROR_OK
    - @ref OKL4_ERROR_ALREADY_STARTED
    - @ref OKL4_ERROR_ALREADY_STOPPED
    - @ref OKL4_ERROR_AXON_AREA_TOO_BIG
    - @ref OKL4_ERROR_AXON_BAD_MESSAGE_SIZE
    - @ref OKL4_ERROR_AXON_INVALID_OFFSET
    - @ref OKL4_ERROR_AXON_QUEUE_NOT_MAPPED
    - @ref OKL4_ERROR_AXON_QUEUE_NOT_READY
    - @ref OKL4_ERROR_AXON_TRANSFER_LIMIT_EXCEEDED
    - @ref OKL4_ERROR_CANCELLED
    - @ref OKL4_ERROR_EXISTING_MAPPING
    - @ref OKL4_ERROR_INSUFFICIENT_SEGMENT_RIGHTS
    - @ref OKL4_ERROR_INTERRUPTED
    - @ref OKL4_ERROR_INTERRUPT_ALREADY_ATTACHED
    - @ref OKL4_ERROR_INTERRUPT_INVALID_IRQ
    - @ref OKL4_ERROR_INTERRUPT_NOT_ATTACHED
    - @ref OKL4_ERROR_INVALID_ARGUMENT
    - @ref OKL4_ERROR_INVALID_DESIGNATOR
    - @ref OKL4_ERROR_INVALID_POWER_STATE
    - @ref OKL4_ERROR_INVALID_SEGMENT_INDEX
    - @ref OKL4_ERROR_MEMORY_FAULT
    - @ref OKL4_ERROR_MISSING_MAPPING
    - @ref OKL4_ERROR_NON_EMPTY_MMU_CONTEXT
    - @ref OKL4_ERROR_NOT_IN_SEGMENT
    - @ref OKL4_ERROR_NOT_LAST_CPU
    - @ref OKL4_ERROR_NO_RESOURCES
    - @ref OKL4_ERROR_PIPE_BAD_STATE
    - @ref OKL4_ERROR_PIPE_EMPTY
    - @ref OKL4_ERROR_PIPE_FULL
    - @ref OKL4_ERROR_PIPE_NOT_READY
    - @ref OKL4_ERROR_PIPE_RECV_OVERFLOW
    - @ref OKL4_ERROR_POWER_VCPU_RESUMED
    - @ref OKL4_ERROR_SEGMENT_USED
    - @ref OKL4_ERROR_THREAD_ALREADY_WATCHING_SUSPENDED
    - @ref OKL4_ERROR_TIMER_ACTIVE
    - @ref OKL4_ERROR_TIMER_CANCELLED
    - @ref OKL4_ERROR_TRY_AGAIN
    - @ref OKL4_ERROR_WOULD_BLOCK
    - @ref OKL4_ERROR_ALLOC_EXHAUSTED
    - @ref OKL4_ERROR_KSP_ERROR_0
    - @ref OKL4_ERROR_KSP_ERROR_1
    - @ref OKL4_ERROR_KSP_ERROR_2
    - @ref OKL4_ERROR_KSP_ERROR_3
    - @ref OKL4_ERROR_KSP_ERROR_4
    - @ref OKL4_ERROR_KSP_ERROR_5
    - @ref OKL4_ERROR_KSP_ERROR_6
    - @ref OKL4_ERROR_KSP_ERROR_7
    - @ref OKL4_ERROR_KSP_INVALID_ARG
    - @ref OKL4_ERROR_KSP_NOT_IMPLEMENTED
    - @ref OKL4_ERROR_KSP_INSUFFICIENT_RIGHTS
    - @ref OKL4_ERROR_KSP_INTERRUPT_REGISTERED
    - @ref OKL4_ERROR_NOT_IMPLEMENTED
    - @ref OKL4_ERROR_MAX
*/

typedef uint32_t okl4_error_t;

/**
    KSP returned OK
*/
/*lint -esym(621, OKL4_ERROR_KSP_OK) */
#define OKL4_ERROR_KSP_OK ((okl4_error_t)0x0U)
/**
    The operation succeeded
*/
/*lint -esym(621, OKL4_ERROR_OK) */
#define OKL4_ERROR_OK ((okl4_error_t)0x0U)
/**
    The target vCPU was already running.
*/
/*lint -esym(621, OKL4_ERROR_ALREADY_STARTED) */
#define OKL4_ERROR_ALREADY_STARTED ((okl4_error_t)0x1U)
/**
    The target vCPU was not running.
*/
/*lint -esym(621, OKL4_ERROR_ALREADY_STOPPED) */
#define OKL4_ERROR_ALREADY_STOPPED ((okl4_error_t)0x2U)
/*lint -esym(621, OKL4_ERROR_AXON_AREA_TOO_BIG) */
#define OKL4_ERROR_AXON_AREA_TOO_BIG ((okl4_error_t)0x3U)
/*lint -esym(621, OKL4_ERROR_AXON_BAD_MESSAGE_SIZE) */
#define OKL4_ERROR_AXON_BAD_MESSAGE_SIZE ((okl4_error_t)0x4U)
/*lint -esym(621, OKL4_ERROR_AXON_INVALID_OFFSET) */
#define OKL4_ERROR_AXON_INVALID_OFFSET ((okl4_error_t)0x5U)
/*lint -esym(621, OKL4_ERROR_AXON_QUEUE_NOT_MAPPED) */
#define OKL4_ERROR_AXON_QUEUE_NOT_MAPPED ((okl4_error_t)0x6U)
/*lint -esym(621, OKL4_ERROR_AXON_QUEUE_NOT_READY) */
#define OKL4_ERROR_AXON_QUEUE_NOT_READY ((okl4_error_t)0x7U)
/*lint -esym(621, OKL4_ERROR_AXON_TRANSFER_LIMIT_EXCEEDED) */
#define OKL4_ERROR_AXON_TRANSFER_LIMIT_EXCEEDED ((okl4_error_t)0x8U)
/**
    A blocking operation was cancelled due to an abort of the operation.
*/
/*lint -esym(621, OKL4_ERROR_CANCELLED) */
#define OKL4_ERROR_CANCELLED ((okl4_error_t)0x9U)
/**
    The operation failed due to an existing mapping.  Mapping
    operations must not overlap an existing mapping.  Unmapping
    must be performed at the same size as the original mapping.
*/
/*lint -esym(621, OKL4_ERROR_EXISTING_MAPPING) */
#define OKL4_ERROR_EXISTING_MAPPING ((okl4_error_t)0xaU)
/**
    The operation requested with a segment failed due to
    insufficient rights in the segment.
*/
/*lint -esym(621, OKL4_ERROR_INSUFFICIENT_SEGMENT_RIGHTS) */
#define OKL4_ERROR_INSUFFICIENT_SEGMENT_RIGHTS ((okl4_error_t)0xbU)
/**
    The operation did not complete because it was interrupted by a
    preemption.  This error value is only used internally.
*/
/*lint -esym(621, OKL4_ERROR_INTERRUPTED) */
#define OKL4_ERROR_INTERRUPTED ((okl4_error_t)0xcU)
/**
    Attempt to attach an interrupt to an IRQ number, when the
    interrupt is already attached to an IRQ number
*/
/*lint -esym(621, OKL4_ERROR_INTERRUPT_ALREADY_ATTACHED) */
#define OKL4_ERROR_INTERRUPT_ALREADY_ATTACHED ((okl4_error_t)0xdU)
/**
    Attempt to use an IRQ number that is out of range, of
    the wrong type, or not in the correct state
*/
/*lint -esym(621, OKL4_ERROR_INTERRUPT_INVALID_IRQ) */
#define OKL4_ERROR_INTERRUPT_INVALID_IRQ ((okl4_error_t)0xeU)
/**
    Attempt to operate on an unknown IRQ number
*/
/*lint -esym(621, OKL4_ERROR_INTERRUPT_NOT_ATTACHED) */
#define OKL4_ERROR_INTERRUPT_NOT_ATTACHED ((okl4_error_t)0xfU)
/**
    An invalid argument was provided.
*/
/*lint -esym(621, OKL4_ERROR_INVALID_ARGUMENT) */
#define OKL4_ERROR_INVALID_ARGUMENT ((okl4_error_t)0x10U)
/**
    The operation failed because one of the arguments does not refer to a
    valid object.
*/
/*lint -esym(621, OKL4_ERROR_INVALID_DESIGNATOR) */
#define OKL4_ERROR_INVALID_DESIGNATOR ((okl4_error_t)0x11U)
/**
    The operation failed because the power_state
    argument is invalid.
*/
/*lint -esym(621, OKL4_ERROR_INVALID_POWER_STATE) */
#define OKL4_ERROR_INVALID_POWER_STATE ((okl4_error_t)0x12U)
/**
    The operation failed because the given segment index does
    not correspond to an attached physical segment.
*/
/*lint -esym(621, OKL4_ERROR_INVALID_SEGMENT_INDEX) */
#define OKL4_ERROR_INVALID_SEGMENT_INDEX ((okl4_error_t)0x13U)
/**
    A user provided address produced a read or write fault in the operation.
*/
/*lint -esym(621, OKL4_ERROR_MEMORY_FAULT) */
#define OKL4_ERROR_MEMORY_FAULT ((okl4_error_t)0x14U)
/**
    The operation failed because there is no mapping at the
    specified location.
*/
/*lint -esym(621, OKL4_ERROR_MISSING_MAPPING) */
#define OKL4_ERROR_MISSING_MAPPING ((okl4_error_t)0x15U)
/**
    The delete operation failed because the KMMU context is not
    empty.
*/
/*lint -esym(621, OKL4_ERROR_NON_EMPTY_MMU_CONTEXT) */
#define OKL4_ERROR_NON_EMPTY_MMU_CONTEXT ((okl4_error_t)0x16U)
/**
    The lookup operation failed because the given virtual address
    of the given KMMU context is not mapped at the given physical
    segment.
*/
/*lint -esym(621, OKL4_ERROR_NOT_IN_SEGMENT) */
#define OKL4_ERROR_NOT_IN_SEGMENT ((okl4_error_t)0x17U)
/**
    The operation failed because the caller is not on the last
    online cpu.
*/
/*lint -esym(621, OKL4_ERROR_NOT_LAST_CPU) */
#define OKL4_ERROR_NOT_LAST_CPU ((okl4_error_t)0x18U)
/**
    Insufficient resources are available to perform the operation.
*/
/*lint -esym(621, OKL4_ERROR_NO_RESOURCES) */
#define OKL4_ERROR_NO_RESOURCES ((okl4_error_t)0x19U)
/**
    Operation failed because pipe was not in the required state.
*/
/*lint -esym(621, OKL4_ERROR_PIPE_BAD_STATE) */
#define OKL4_ERROR_PIPE_BAD_STATE ((okl4_error_t)0x1aU)
/**
    Operation failed because no messages are in the queue.
*/
/*lint -esym(621, OKL4_ERROR_PIPE_EMPTY) */
#define OKL4_ERROR_PIPE_EMPTY ((okl4_error_t)0x1bU)
/**
    Operation failed because no memory is available in the queue.
*/
/*lint -esym(621, OKL4_ERROR_PIPE_FULL) */
#define OKL4_ERROR_PIPE_FULL ((okl4_error_t)0x1cU)
/**
    Operation failed because the pipe is in reset or not ready.
*/
/*lint -esym(621, OKL4_ERROR_PIPE_NOT_READY) */
#define OKL4_ERROR_PIPE_NOT_READY ((okl4_error_t)0x1dU)
/**
    Message was truncated because receive buffer size is too small.
*/
/*lint -esym(621, OKL4_ERROR_PIPE_RECV_OVERFLOW) */
#define OKL4_ERROR_PIPE_RECV_OVERFLOW ((okl4_error_t)0x1eU)
/**
    The operation failed because at least one VCPU has a monitored
    power state and is not currently suspended.
*/
/*lint -esym(621, OKL4_ERROR_POWER_VCPU_RESUMED) */
#define OKL4_ERROR_POWER_VCPU_RESUMED ((okl4_error_t)0x1fU)
/**
    The operation requires a segment to be unused, or not attached
    to an MMU context.
*/
/*lint -esym(621, OKL4_ERROR_SEGMENT_USED) */
#define OKL4_ERROR_SEGMENT_USED ((okl4_error_t)0x20U)
/*lint -esym(621, OKL4_ERROR_THREAD_ALREADY_WATCHING_SUSPENDED) */
#define OKL4_ERROR_THREAD_ALREADY_WATCHING_SUSPENDED ((okl4_error_t)0x21U)
/**
    The timer is already active, and was not reprogrammed.
*/
/*lint -esym(621, OKL4_ERROR_TIMER_ACTIVE) */
#define OKL4_ERROR_TIMER_ACTIVE ((okl4_error_t)0x22U)
/**
    The timer has already been cancelled or expired.
*/
/*lint -esym(621, OKL4_ERROR_TIMER_CANCELLED) */
#define OKL4_ERROR_TIMER_CANCELLED ((okl4_error_t)0x23U)
/**
    Operation failed due to a temporary condition, and may be retried.
*/
/*lint -esym(621, OKL4_ERROR_TRY_AGAIN) */
#define OKL4_ERROR_TRY_AGAIN ((okl4_error_t)0x24U)
/**
    The non-blocking operation failed because it would
    block on a resource.
*/
/*lint -esym(621, OKL4_ERROR_WOULD_BLOCK) */
#define OKL4_ERROR_WOULD_BLOCK ((okl4_error_t)0x25U)
/**
    Insufficient resources
*/
/*lint -esym(621, OKL4_ERROR_ALLOC_EXHAUSTED) */
#define OKL4_ERROR_ALLOC_EXHAUSTED ((okl4_error_t)0x26U)
/**
    KSP specific error 0
*/
/*lint -esym(621, OKL4_ERROR_KSP_ERROR_0) */
#define OKL4_ERROR_KSP_ERROR_0 ((okl4_error_t)0x10000010U)
/**
    KSP specific error 1
*/
/*lint -esym(621, OKL4_ERROR_KSP_ERROR_1) */
#define OKL4_ERROR_KSP_ERROR_1 ((okl4_error_t)0x10000011U)
/**
    KSP specific error 2
*/
/*lint -esym(621, OKL4_ERROR_KSP_ERROR_2) */
#define OKL4_ERROR_KSP_ERROR_2 ((okl4_error_t)0x10000012U)
/**
    KSP specific error 3
*/
/*lint -esym(621, OKL4_ERROR_KSP_ERROR_3) */
#define OKL4_ERROR_KSP_ERROR_3 ((okl4_error_t)0x10000013U)
/**
    KSP specific error 4
*/
/*lint -esym(621, OKL4_ERROR_KSP_ERROR_4) */
#define OKL4_ERROR_KSP_ERROR_4 ((okl4_error_t)0x10000014U)
/**
    KSP specific error 5
*/
/*lint -esym(621, OKL4_ERROR_KSP_ERROR_5) */
#define OKL4_ERROR_KSP_ERROR_5 ((okl4_error_t)0x10000015U)
/**
    KSP specific error 6
*/
/*lint -esym(621, OKL4_ERROR_KSP_ERROR_6) */
#define OKL4_ERROR_KSP_ERROR_6 ((okl4_error_t)0x10000016U)
/**
    KSP specific error 7
*/
/*lint -esym(621, OKL4_ERROR_KSP_ERROR_7) */
#define OKL4_ERROR_KSP_ERROR_7 ((okl4_error_t)0x10000017U)
/**
    Invalid argument to KSP
*/
/*lint -esym(621, OKL4_ERROR_KSP_INVALID_ARG) */
#define OKL4_ERROR_KSP_INVALID_ARG ((okl4_error_t)0x80000001U)
/**
    KSP doesn't implement requested feature
*/
/*lint -esym(621, OKL4_ERROR_KSP_NOT_IMPLEMENTED) */
#define OKL4_ERROR_KSP_NOT_IMPLEMENTED ((okl4_error_t)0x80000002U)
/**
    User didn't supply rights for requested feature
*/
/*lint -esym(621, OKL4_ERROR_KSP_INSUFFICIENT_RIGHTS) */
#define OKL4_ERROR_KSP_INSUFFICIENT_RIGHTS ((okl4_error_t)0x80000003U)
/**
    Interrupt already registered
*/
/*lint -esym(621, OKL4_ERROR_KSP_INTERRUPT_REGISTERED) */
#define OKL4_ERROR_KSP_INTERRUPT_REGISTERED ((okl4_error_t)0x80000004U)
/**
    Requested operation is not implemented.
*/
/*lint -esym(621, OKL4_ERROR_NOT_IMPLEMENTED) */
#define OKL4_ERROR_NOT_IMPLEMENTED ((okl4_error_t)0xffffffffU)
/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_ERROR_MAX) */
#define OKL4_ERROR_MAX ((okl4_error_t)0xffffffffU)

/*lint -esym(714, okl4_error_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_error_is_element_of(okl4_error_t var);


/*lint -esym(714, okl4_error_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_error_is_element_of(okl4_error_t var)
{
    /*lint --e{944} Disable dead expression detection */
    /*lint --e{948} --e{845} Disable constant always zero */
    return ((var == OKL4_ERROR_ALREADY_STARTED) ||
            (var == OKL4_ERROR_ALREADY_STOPPED) ||
            (var == OKL4_ERROR_AXON_AREA_TOO_BIG) ||
            (var == OKL4_ERROR_AXON_BAD_MESSAGE_SIZE) ||
            (var == OKL4_ERROR_AXON_INVALID_OFFSET) ||
            (var == OKL4_ERROR_AXON_QUEUE_NOT_MAPPED) ||
            (var == OKL4_ERROR_AXON_QUEUE_NOT_READY) ||
            (var == OKL4_ERROR_AXON_TRANSFER_LIMIT_EXCEEDED) ||
            (var == OKL4_ERROR_CANCELLED) ||
            (var == OKL4_ERROR_EXISTING_MAPPING) ||
            (var == OKL4_ERROR_INSUFFICIENT_SEGMENT_RIGHTS) ||
            (var == OKL4_ERROR_INTERRUPTED) ||
            (var == OKL4_ERROR_INTERRUPT_ALREADY_ATTACHED) ||
            (var == OKL4_ERROR_INTERRUPT_INVALID_IRQ) ||
            (var == OKL4_ERROR_INTERRUPT_NOT_ATTACHED) ||
            (var == OKL4_ERROR_INVALID_ARGUMENT) ||
            (var == OKL4_ERROR_INVALID_DESIGNATOR) ||
            (var == OKL4_ERROR_INVALID_POWER_STATE) ||
            (var == OKL4_ERROR_INVALID_SEGMENT_INDEX) ||
            (var == OKL4_ERROR_KSP_ERROR_0) ||
            (var == OKL4_ERROR_KSP_ERROR_1) ||
            (var == OKL4_ERROR_KSP_ERROR_2) ||
            (var == OKL4_ERROR_KSP_ERROR_3) ||
            (var == OKL4_ERROR_KSP_ERROR_4) ||
            (var == OKL4_ERROR_KSP_ERROR_5) ||
            (var == OKL4_ERROR_KSP_ERROR_6) ||
            (var == OKL4_ERROR_KSP_ERROR_7) ||
            (var == OKL4_ERROR_KSP_INSUFFICIENT_RIGHTS) ||
            (var == OKL4_ERROR_KSP_INTERRUPT_REGISTERED) ||
            (var == OKL4_ERROR_KSP_INVALID_ARG) ||
            (var == OKL4_ERROR_KSP_NOT_IMPLEMENTED) ||
            (var == OKL4_ERROR_KSP_OK) ||
            (var == OKL4_ERROR_MEMORY_FAULT) ||
            (var == OKL4_ERROR_MISSING_MAPPING) ||
            (var == OKL4_ERROR_NON_EMPTY_MMU_CONTEXT) ||
            (var == OKL4_ERROR_NOT_IMPLEMENTED) ||
            (var == OKL4_ERROR_NOT_IN_SEGMENT) ||
            (var == OKL4_ERROR_NOT_LAST_CPU) ||
            (var == OKL4_ERROR_NO_RESOURCES) ||
            (var == OKL4_ERROR_OK) ||
            (var == OKL4_ERROR_PIPE_BAD_STATE) ||
            (var == OKL4_ERROR_PIPE_EMPTY) ||
            (var == OKL4_ERROR_PIPE_FULL) ||
            (var == OKL4_ERROR_PIPE_NOT_READY) ||
            (var == OKL4_ERROR_PIPE_RECV_OVERFLOW) ||
            (var == OKL4_ERROR_POWER_VCPU_RESUMED) ||
            (var == OKL4_ERROR_SEGMENT_USED) ||
            (var == OKL4_ERROR_THREAD_ALREADY_WATCHING_SUSPENDED) ||
            (var == OKL4_ERROR_TIMER_ACTIVE) ||
            (var == OKL4_ERROR_TIMER_CANCELLED) ||
            (var == OKL4_ERROR_TRY_AGAIN) ||
            (var == OKL4_ERROR_WOULD_BLOCK) ||
            (var == OKL4_ERROR_ALLOC_EXHAUSTED));
}


/**

*/

struct okl4_firmware_segment {
    okl4_laddr_t copy_addr;
    okl4_laddr_t exec_addr;
    okl4_lsize_t filesz;
    okl4_lsize_t memsz_diff;
};




/**

*/

struct okl4_firmware_segments_info {
    okl4_count_t num_segments;
    _okl4_padding_t __padding0_4;
    _okl4_padding_t __padding1_5;
    _okl4_padding_t __padding2_6;
    _okl4_padding_t __padding3_7;
    struct okl4_firmware_segment segments[]; /*lint --e{9038} flex array */
};




/**
    - BIT 1 -   @ref OKL4_MASK_EDGE_GICD_ICFGR
*/

/*lint -esym(621, okl4_gicd_icfgr_t) */
typedef uint32_t okl4_gicd_icfgr_t;

/*lint -esym(621, okl4_gicd_icfgr_getedge) */
/*lint -esym(714, okl4_gicd_icfgr_getedge) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_gicd_icfgr_getedge(const okl4_gicd_icfgr_t *x);

/*lint -esym(621, okl4_gicd_icfgr_setedge) */
OKL4_FORCE_INLINE void
okl4_gicd_icfgr_setedge(okl4_gicd_icfgr_t *x, okl4_bool_t _edge);

/*lint -esym(714, okl4_gicd_icfgr_init) */
OKL4_FORCE_INLINE void
okl4_gicd_icfgr_init(okl4_gicd_icfgr_t *x);

/*lint -esym(714, okl4_gicd_icfgr_cast) */
OKL4_FORCE_INLINE okl4_gicd_icfgr_t
okl4_gicd_icfgr_cast(uint32_t p, okl4_bool_t force);




/*lint -esym(621, OKL4_GICD_ICFGR_EDGE_MASK) */
#define OKL4_GICD_ICFGR_EDGE_MASK ((okl4_gicd_icfgr_t)1U << 1) /* Deprecated */
/*lint -esym(621, OKL4_MASK_EDGE_GICD_ICFGR) */
#define OKL4_MASK_EDGE_GICD_ICFGR ((okl4_gicd_icfgr_t)1U << 1)
/*lint -esym(621, OKL4_SHIFT_EDGE_GICD_ICFGR) */
#define OKL4_SHIFT_EDGE_GICD_ICFGR (1)
/*lint -esym(621, OKL4_WIDTH_EDGE_GICD_ICFGR) */
#define OKL4_WIDTH_EDGE_GICD_ICFGR (1)


/*lint -sem(okl4_gicd_icfgr_getedge, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_gicd_icfgr_getedge) */
/*lint -esym(714, okl4_gicd_icfgr_getedge) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_gicd_icfgr_getedge(const okl4_gicd_icfgr_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 1;
            _Bool field : 1;
        } bits;
        okl4_gicd_icfgr_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_gicd_icfgr_setedge, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_gicd_icfgr_setedge) */

/*lint -esym(621, okl4_gicd_icfgr_setedge) */
OKL4_FORCE_INLINE void
okl4_gicd_icfgr_setedge(okl4_gicd_icfgr_t *x, okl4_bool_t _edge)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 1;
            _Bool field : 1;
        } bits;
        okl4_gicd_icfgr_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_edge;
    *x = _conv.raw;
}
/*lint -esym(714, okl4_gicd_icfgr_init) */
OKL4_FORCE_INLINE void
okl4_gicd_icfgr_init(okl4_gicd_icfgr_t *x)
{
    *x = (okl4_gicd_icfgr_t)0U;
}

/*lint -esym(714, okl4_gicd_icfgr_cast) */
OKL4_FORCE_INLINE okl4_gicd_icfgr_t
okl4_gicd_icfgr_cast(uint32_t p, okl4_bool_t force)
{
    okl4_gicd_icfgr_t x = (okl4_gicd_icfgr_t)p;
    (void)force;
    return x;
}




typedef uint32_t okl4_sgi_target_t;

/*lint -esym(621, OKL4_SGI_TARGET_LISTED) */
#define OKL4_SGI_TARGET_LISTED ((okl4_sgi_target_t)0x0U)
/*lint -esym(621, OKL4_SGI_TARGET_ALL_OTHERS) */
#define OKL4_SGI_TARGET_ALL_OTHERS ((okl4_sgi_target_t)0x1U)
/*lint -esym(621, OKL4_SGI_TARGET_SELF) */
#define OKL4_SGI_TARGET_SELF ((okl4_sgi_target_t)0x2U)
/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_SGI_TARGET_MAX) */
#define OKL4_SGI_TARGET_MAX ((okl4_sgi_target_t)0x2U)
/**
    Invalid enumeration value
*/
/*lint -esym(621, OKL4_SGI_TARGET_INVALID) */
#define OKL4_SGI_TARGET_INVALID ((okl4_sgi_target_t)0xffffffffU)

/*lint -esym(714, okl4_sgi_target_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_sgi_target_is_element_of(okl4_sgi_target_t var);


/*lint -esym(714, okl4_sgi_target_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_sgi_target_is_element_of(okl4_sgi_target_t var)
{
    /*lint --e{944} Disable dead expression detection */
    /*lint --e{948} --e{845} Disable constant always zero */
    return ((var == OKL4_SGI_TARGET_LISTED) ||
            (var == OKL4_SGI_TARGET_ALL_OTHERS) ||
            (var == OKL4_SGI_TARGET_SELF));
}


/**
    - BITS 3..0 -   @ref OKL4_MASK_SGIINTID_GICD_SGIR
    - BIT 15 -   @ref OKL4_MASK_NSATT_GICD_SGIR
    - BITS 23..16 -   @ref OKL4_MASK_CPUTARGETLIST_GICD_SGIR
    - BITS 25..24 -   @ref OKL4_MASK_TARGETLISTFILTER_GICD_SGIR
*/

/*lint -esym(621, okl4_gicd_sgir_t) */
typedef uint32_t okl4_gicd_sgir_t;

/*lint -esym(621, okl4_gicd_sgir_getsgiintid) */
/*lint -esym(714, okl4_gicd_sgir_getsgiintid) */
OKL4_FORCE_INLINE okl4_interrupt_number_t
okl4_gicd_sgir_getsgiintid(const okl4_gicd_sgir_t *x);

/*lint -esym(621, okl4_gicd_sgir_setsgiintid) */
OKL4_FORCE_INLINE void
okl4_gicd_sgir_setsgiintid(okl4_gicd_sgir_t *x, okl4_interrupt_number_t _sgiintid);

/*lint -esym(621, okl4_gicd_sgir_getnsatt) */
/*lint -esym(714, okl4_gicd_sgir_getnsatt) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_gicd_sgir_getnsatt(const okl4_gicd_sgir_t *x);

/*lint -esym(621, okl4_gicd_sgir_setnsatt) */
OKL4_FORCE_INLINE void
okl4_gicd_sgir_setnsatt(okl4_gicd_sgir_t *x, okl4_bool_t _nsatt);

/*lint -esym(621, okl4_gicd_sgir_getcputargetlist) */
/*lint -esym(714, okl4_gicd_sgir_getcputargetlist) */
OKL4_FORCE_INLINE uint8_t
okl4_gicd_sgir_getcputargetlist(const okl4_gicd_sgir_t *x);

/*lint -esym(621, okl4_gicd_sgir_setcputargetlist) */
OKL4_FORCE_INLINE void
okl4_gicd_sgir_setcputargetlist(okl4_gicd_sgir_t *x, uint8_t _cputargetlist);

/*lint -esym(621, okl4_gicd_sgir_gettargetlistfilter) */
/*lint -esym(714, okl4_gicd_sgir_gettargetlistfilter) */
OKL4_FORCE_INLINE okl4_sgi_target_t
okl4_gicd_sgir_gettargetlistfilter(const okl4_gicd_sgir_t *x);

/*lint -esym(621, okl4_gicd_sgir_settargetlistfilter) */
OKL4_FORCE_INLINE void
okl4_gicd_sgir_settargetlistfilter(okl4_gicd_sgir_t *x, okl4_sgi_target_t _targetlistfilter);

/*lint -esym(714, okl4_gicd_sgir_init) */
OKL4_FORCE_INLINE void
okl4_gicd_sgir_init(okl4_gicd_sgir_t *x);

/*lint -esym(714, okl4_gicd_sgir_cast) */
OKL4_FORCE_INLINE okl4_gicd_sgir_t
okl4_gicd_sgir_cast(uint32_t p, okl4_bool_t force);




/*lint -esym(621, OKL4_GICD_SGIR_SGIINTID_MASK) */
#define OKL4_GICD_SGIR_SGIINTID_MASK ((okl4_gicd_sgir_t)15U) /* Deprecated */
/*lint -esym(621, OKL4_MASK_SGIINTID_GICD_SGIR) */
#define OKL4_MASK_SGIINTID_GICD_SGIR ((okl4_gicd_sgir_t)15U)
/*lint -esym(621, OKL4_SHIFT_SGIINTID_GICD_SGIR) */
#define OKL4_SHIFT_SGIINTID_GICD_SGIR (0)
/*lint -esym(621, OKL4_WIDTH_SGIINTID_GICD_SGIR) */
#define OKL4_WIDTH_SGIINTID_GICD_SGIR (4)
/*lint -esym(621, OKL4_GICD_SGIR_NSATT_MASK) */
#define OKL4_GICD_SGIR_NSATT_MASK ((okl4_gicd_sgir_t)1U << 15) /* Deprecated */
/*lint -esym(621, OKL4_MASK_NSATT_GICD_SGIR) */
#define OKL4_MASK_NSATT_GICD_SGIR ((okl4_gicd_sgir_t)1U << 15)
/*lint -esym(621, OKL4_SHIFT_NSATT_GICD_SGIR) */
#define OKL4_SHIFT_NSATT_GICD_SGIR (15)
/*lint -esym(621, OKL4_WIDTH_NSATT_GICD_SGIR) */
#define OKL4_WIDTH_NSATT_GICD_SGIR (1)
/*lint -esym(621, OKL4_GICD_SGIR_CPUTARGETLIST_MASK) */
#define OKL4_GICD_SGIR_CPUTARGETLIST_MASK ((okl4_gicd_sgir_t)255U << 16) /* Deprecated */
/*lint -esym(621, OKL4_MASK_CPUTARGETLIST_GICD_SGIR) */
#define OKL4_MASK_CPUTARGETLIST_GICD_SGIR ((okl4_gicd_sgir_t)255U << 16)
/*lint -esym(621, OKL4_SHIFT_CPUTARGETLIST_GICD_SGIR) */
#define OKL4_SHIFT_CPUTARGETLIST_GICD_SGIR (16)
/*lint -esym(621, OKL4_WIDTH_CPUTARGETLIST_GICD_SGIR) */
#define OKL4_WIDTH_CPUTARGETLIST_GICD_SGIR (8)
/*lint -esym(621, OKL4_GICD_SGIR_TARGETLISTFILTER_MASK) */
#define OKL4_GICD_SGIR_TARGETLISTFILTER_MASK ((okl4_gicd_sgir_t)3U << 24) /* Deprecated */
/*lint -esym(621, OKL4_MASK_TARGETLISTFILTER_GICD_SGIR) */
#define OKL4_MASK_TARGETLISTFILTER_GICD_SGIR ((okl4_gicd_sgir_t)3U << 24)
/*lint -esym(621, OKL4_SHIFT_TARGETLISTFILTER_GICD_SGIR) */
#define OKL4_SHIFT_TARGETLISTFILTER_GICD_SGIR (24)
/*lint -esym(621, OKL4_WIDTH_TARGETLISTFILTER_GICD_SGIR) */
#define OKL4_WIDTH_TARGETLISTFILTER_GICD_SGIR (2)


/*lint -sem(okl4_gicd_sgir_getsgiintid, 1p, @n >= 0 && @n <= 15) */
/*lint -esym(621, okl4_gicd_sgir_getsgiintid) */
/*lint -esym(714, okl4_gicd_sgir_getsgiintid) */
OKL4_FORCE_INLINE okl4_interrupt_number_t
okl4_gicd_sgir_getsgiintid(const okl4_gicd_sgir_t *x)
{
    okl4_interrupt_number_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t field : 4;
        } bits;
        okl4_gicd_sgir_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_interrupt_number_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_gicd_sgir_setsgiintid, 2n >= 0 && 2n <= 15) */
/*lint -esym(714, okl4_gicd_sgir_setsgiintid) */

/*lint -esym(621, okl4_gicd_sgir_setsgiintid) */
OKL4_FORCE_INLINE void
okl4_gicd_sgir_setsgiintid(okl4_gicd_sgir_t *x, okl4_interrupt_number_t _sgiintid)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t field : 4;
        } bits;
        okl4_gicd_sgir_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_sgiintid;
    *x = _conv.raw;
}
/*lint -sem(okl4_gicd_sgir_getnsatt, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_gicd_sgir_getnsatt) */
/*lint -esym(714, okl4_gicd_sgir_getnsatt) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_gicd_sgir_getnsatt(const okl4_gicd_sgir_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 15;
            _Bool field : 1;
        } bits;
        okl4_gicd_sgir_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_gicd_sgir_setnsatt, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_gicd_sgir_setnsatt) */

/*lint -esym(621, okl4_gicd_sgir_setnsatt) */
OKL4_FORCE_INLINE void
okl4_gicd_sgir_setnsatt(okl4_gicd_sgir_t *x, okl4_bool_t _nsatt)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 15;
            _Bool field : 1;
        } bits;
        okl4_gicd_sgir_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_nsatt;
    *x = _conv.raw;
}
/*lint -sem(okl4_gicd_sgir_getcputargetlist, 1p, @n >= 0 && @n <= 255) */
/*lint -esym(621, okl4_gicd_sgir_getcputargetlist) */
/*lint -esym(714, okl4_gicd_sgir_getcputargetlist) */
OKL4_FORCE_INLINE uint8_t
okl4_gicd_sgir_getcputargetlist(const okl4_gicd_sgir_t *x)
{
    uint8_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 16;
            uint32_t field : 8;
        } bits;
        okl4_gicd_sgir_t raw;
    } _conv;

    _conv.raw = *x;
    field = (uint8_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_gicd_sgir_setcputargetlist, 2n >= 0 && 2n <= 255) */
/*lint -esym(714, okl4_gicd_sgir_setcputargetlist) */

/*lint -esym(621, okl4_gicd_sgir_setcputargetlist) */
OKL4_FORCE_INLINE void
okl4_gicd_sgir_setcputargetlist(okl4_gicd_sgir_t *x, uint8_t _cputargetlist)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 16;
            uint32_t field : 8;
        } bits;
        okl4_gicd_sgir_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_cputargetlist;
    *x = _conv.raw;
}
/*lint -sem(okl4_gicd_sgir_gettargetlistfilter, 1p, @n >= 0 && @n <= 3) */
/*lint -esym(621, okl4_gicd_sgir_gettargetlistfilter) */
/*lint -esym(714, okl4_gicd_sgir_gettargetlistfilter) */
OKL4_FORCE_INLINE okl4_sgi_target_t
okl4_gicd_sgir_gettargetlistfilter(const okl4_gicd_sgir_t *x)
{
    okl4_sgi_target_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 24;
            uint32_t field : 2;
        } bits;
        okl4_gicd_sgir_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_sgi_target_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_gicd_sgir_settargetlistfilter, 2n >= 0 && 2n <= 3) */
/*lint -esym(714, okl4_gicd_sgir_settargetlistfilter) */

/*lint -esym(621, okl4_gicd_sgir_settargetlistfilter) */
OKL4_FORCE_INLINE void
okl4_gicd_sgir_settargetlistfilter(okl4_gicd_sgir_t *x, okl4_sgi_target_t _targetlistfilter)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 24;
            uint32_t field : 2;
        } bits;
        okl4_gicd_sgir_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_targetlistfilter;
    *x = _conv.raw;
}
/*lint -esym(714, okl4_gicd_sgir_init) */
OKL4_FORCE_INLINE void
okl4_gicd_sgir_init(okl4_gicd_sgir_t *x)
{
    *x = (okl4_gicd_sgir_t)32768U;
}

/*lint -esym(714, okl4_gicd_sgir_cast) */
OKL4_FORCE_INLINE okl4_gicd_sgir_t
okl4_gicd_sgir_cast(uint32_t p, okl4_bool_t force)
{
    okl4_gicd_sgir_t x = (okl4_gicd_sgir_t)p;
    (void)force;
    return x;
}























/**
    The okl4_kmmu_t structure is used to represent a kernel MMU
    context.
*/

struct okl4_kmmu {
    okl4_kcap_t kcap;
};




/**
    The `okl4_ksp_arg_t` type represents an unsigned, machine-native
    register-sized integer value used for KSP call arguments. Important: it is
    truncated to guest register-size when guest register-size is smaller than
    kernel register-size.
*/

typedef okl4_register_t okl4_ksp_arg_t;




/**

*/

struct okl4_ksp_user_agent {
    okl4_kcap_t kcap;
    okl4_interrupt_number_t virq;
};





typedef uint32_t okl4_ksp_vdevice_class_t;





typedef okl4_register_t okl4_laddr_pn_t;





typedef okl4_register_t okl4_laddr_tr_t;




/**

*/

struct okl4_pipe_data {
    okl4_kcap_t kcap;
    okl4_irq_t virq;
};




/**

*/

struct okl4_pipe_ep_data {
    struct okl4_pipe_data rx;
    struct okl4_pipe_data tx;
};





typedef uint32_t okl4_link_role_t;

/*lint -esym(621, OKL4_LINK_ROLE_SYMMETRIC) */
#define OKL4_LINK_ROLE_SYMMETRIC ((okl4_link_role_t)0x0U)
/*lint -esym(621, OKL4_LINK_ROLE_SERVER) */
#define OKL4_LINK_ROLE_SERVER ((okl4_link_role_t)0x1U)
/*lint -esym(621, OKL4_LINK_ROLE_CLIENT) */
#define OKL4_LINK_ROLE_CLIENT ((okl4_link_role_t)0x2U)
/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_LINK_ROLE_MAX) */
#define OKL4_LINK_ROLE_MAX ((okl4_link_role_t)0x2U)
/**
    Invalid enumeration value
*/
/*lint -esym(621, OKL4_LINK_ROLE_INVALID) */
#define OKL4_LINK_ROLE_INVALID ((okl4_link_role_t)0xffffffffU)

/*lint -esym(714, okl4_link_role_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_link_role_is_element_of(okl4_link_role_t var);


/*lint -esym(714, okl4_link_role_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_link_role_is_element_of(okl4_link_role_t var)
{
    /*lint --e{944} Disable dead expression detection */
    /*lint --e{948} --e{845} Disable constant always zero */
    return ((var == OKL4_LINK_ROLE_SYMMETRIC) ||
            (var == OKL4_LINK_ROLE_SERVER) ||
            (var == OKL4_LINK_ROLE_CLIENT));
}



typedef uint32_t okl4_link_transport_type_t;

/*lint -esym(621, OKL4_LINK_TRANSPORT_TYPE_SHARED_BUFFER) */
#define OKL4_LINK_TRANSPORT_TYPE_SHARED_BUFFER ((okl4_link_transport_type_t)0x0U)
/*lint -esym(621, OKL4_LINK_TRANSPORT_TYPE_AXONS) */
#define OKL4_LINK_TRANSPORT_TYPE_AXONS ((okl4_link_transport_type_t)0x1U)
/*lint -esym(621, OKL4_LINK_TRANSPORT_TYPE_PIPES) */
#define OKL4_LINK_TRANSPORT_TYPE_PIPES ((okl4_link_transport_type_t)0x2U)
/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_LINK_TRANSPORT_TYPE_MAX) */
#define OKL4_LINK_TRANSPORT_TYPE_MAX ((okl4_link_transport_type_t)0x2U)
/**
    Invalid enumeration value
*/
/*lint -esym(621, OKL4_LINK_TRANSPORT_TYPE_INVALID) */
#define OKL4_LINK_TRANSPORT_TYPE_INVALID ((okl4_link_transport_type_t)0xffffffffU)

/*lint -esym(714, okl4_link_transport_type_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_link_transport_type_is_element_of(okl4_link_transport_type_t var);


/*lint -esym(714, okl4_link_transport_type_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_link_transport_type_is_element_of(okl4_link_transport_type_t var)
{
    /*lint --e{944} Disable dead expression detection */
    /*lint --e{948} --e{845} Disable constant always zero */
    return ((var == OKL4_LINK_TRANSPORT_TYPE_SHARED_BUFFER) ||
            (var == OKL4_LINK_TRANSPORT_TYPE_AXONS) ||
            (var == OKL4_LINK_TRANSPORT_TYPE_PIPES));
}


/**

*/

struct okl4_link {
    __ptr64(okl4_string_t, name);
    __ptr64(void *, opaque);
    __ptr64(okl4_string_t, partner_name);
    okl4_link_role_t role;
    _okl4_padding_t __padding0_4;
    _okl4_padding_t __padding1_5;
    _okl4_padding_t __padding2_6;
    _okl4_padding_t __padding3_7;
    union {
        struct {
            struct okl4_virtmem_item buffer;
            okl4_irq_t virq_in;
            okl4_kcap_t virq_out;
        } shared_buffer;

        struct {
            struct okl4_axon_ep_data axon_ep;
            okl4_ksize_t message_size;
            okl4_count_t queue_length;
            _okl4_padding_t __padding0_4; /**< Padding 8 */
            _okl4_padding_t __padding1_5; /**< Padding 8 */
            _okl4_padding_t __padding2_6; /**< Padding 8 */
            _okl4_padding_t __padding3_7; /**< Padding 8 */
        } axons;

        struct {
            okl4_ksize_t message_size;
            struct okl4_pipe_ep_data pipe_ep;
            okl4_count_t queue_length;
            _okl4_padding_t __padding0_4; /**< Padding 8 */
            _okl4_padding_t __padding1_5; /**< Padding 8 */
            _okl4_padding_t __padding2_6; /**< Padding 8 */
            _okl4_padding_t __padding3_7; /**< Padding 8 */
        } pipes;

    } transport;

    okl4_link_transport_type_t transport_type;
    _okl4_padding_t __padding4_4;
    _okl4_padding_t __padding5_5;
    _okl4_padding_t __padding6_6;
    _okl4_padding_t __padding7_7;
};




/**

*/

struct okl4_links {
    okl4_count_t num_links;
    _okl4_padding_t __padding0_4;
    _okl4_padding_t __padding1_5;
    _okl4_padding_t __padding2_6;
    _okl4_padding_t __padding3_7;
    __ptr64_array(struct okl4_link *, links)[]; /*lint --e{9038} flex array */
};





typedef okl4_register_t okl4_lsize_pn_t;





typedef okl4_register_t okl4_lsize_tr_t;




/**
    The okl4_machine_info_t structure holds machine-specific
    constants that are only known at weave-time. Objects of this
    type are typically obtained from the OKL4 environment.
*/

struct okl4_machine_info {
    okl4_ksize_t l1_cache_line_size;
    okl4_ksize_t l2_cache_line_size;
    okl4_count_t num_cpus;
    _okl4_padding_t __padding0_4;
    _okl4_padding_t __padding1_5;
    _okl4_padding_t __padding2_6;
    _okl4_padding_t __padding3_7;
};




/**

*/

struct okl4_merged_physpool {
    okl4_paddr_t phys_addr;
    okl4_count_t num_segments;
    _okl4_padding_t __padding0_4;
    _okl4_padding_t __padding1_5;
    _okl4_padding_t __padding2_6;
    _okl4_padding_t __padding3_7;
    struct okl4_virtmem_item segments[]; /*lint --e{9038} flex array */
};





typedef uint32_t okl4_microseconds_t;




/**

*/

struct okl4_microvisor_timer {
    okl4_kcap_t kcap;
    okl4_irq_t virq;
};




/**
    - BITS 15..0 -   @ref OKL4_MASK_ERROR_MMU_LOOKUP_INDEX
    - BITS 31..16 -   @ref OKL4_MASK_INDEX_MMU_LOOKUP_INDEX
*/

/*lint -esym(621, okl4_mmu_lookup_index_t) */
typedef uint32_t okl4_mmu_lookup_index_t;

/*lint -esym(621, okl4_mmu_lookup_index_geterror) */
/*lint -esym(714, okl4_mmu_lookup_index_geterror) */
OKL4_FORCE_INLINE okl4_error_t
okl4_mmu_lookup_index_geterror(const okl4_mmu_lookup_index_t *x);

/*lint -esym(621, okl4_mmu_lookup_index_seterror) */
OKL4_FORCE_INLINE void
okl4_mmu_lookup_index_seterror(okl4_mmu_lookup_index_t *x, okl4_error_t _error);

/*lint -esym(621, okl4_mmu_lookup_index_getindex) */
/*lint -esym(714, okl4_mmu_lookup_index_getindex) */
OKL4_FORCE_INLINE okl4_count_t
okl4_mmu_lookup_index_getindex(const okl4_mmu_lookup_index_t *x);

/*lint -esym(621, okl4_mmu_lookup_index_setindex) */
OKL4_FORCE_INLINE void
okl4_mmu_lookup_index_setindex(okl4_mmu_lookup_index_t *x, okl4_count_t _index);

/*lint -esym(714, okl4_mmu_lookup_index_init) */
OKL4_FORCE_INLINE void
okl4_mmu_lookup_index_init(okl4_mmu_lookup_index_t *x);

/*lint -esym(714, okl4_mmu_lookup_index_cast) */
OKL4_FORCE_INLINE okl4_mmu_lookup_index_t
okl4_mmu_lookup_index_cast(uint32_t p, okl4_bool_t force);




/*lint -esym(621, OKL4_MMU_LOOKUP_INDEX_ERROR_MASK) */
#define OKL4_MMU_LOOKUP_INDEX_ERROR_MASK ((okl4_mmu_lookup_index_t)65535U) /* Deprecated */
/*lint -esym(621, OKL4_MASK_ERROR_MMU_LOOKUP_INDEX) */
#define OKL4_MASK_ERROR_MMU_LOOKUP_INDEX ((okl4_mmu_lookup_index_t)65535U)
/*lint -esym(621, OKL4_SHIFT_ERROR_MMU_LOOKUP_INDEX) */
#define OKL4_SHIFT_ERROR_MMU_LOOKUP_INDEX (0)
/*lint -esym(621, OKL4_WIDTH_ERROR_MMU_LOOKUP_INDEX) */
#define OKL4_WIDTH_ERROR_MMU_LOOKUP_INDEX (16)
/*lint -esym(621, OKL4_MMU_LOOKUP_INDEX_INDEX_MASK) */
#define OKL4_MMU_LOOKUP_INDEX_INDEX_MASK ((okl4_mmu_lookup_index_t)65535U << 16) /* Deprecated */
/*lint -esym(621, OKL4_MASK_INDEX_MMU_LOOKUP_INDEX) */
#define OKL4_MASK_INDEX_MMU_LOOKUP_INDEX ((okl4_mmu_lookup_index_t)65535U << 16)
/*lint -esym(621, OKL4_SHIFT_INDEX_MMU_LOOKUP_INDEX) */
#define OKL4_SHIFT_INDEX_MMU_LOOKUP_INDEX (16)
/*lint -esym(621, OKL4_WIDTH_INDEX_MMU_LOOKUP_INDEX) */
#define OKL4_WIDTH_INDEX_MMU_LOOKUP_INDEX (16)


/*lint -sem(okl4_mmu_lookup_index_geterror, 1p, @n >= 0 && @n <= 65535) */
/*lint -esym(621, okl4_mmu_lookup_index_geterror) */
/*lint -esym(714, okl4_mmu_lookup_index_geterror) */
OKL4_FORCE_INLINE okl4_error_t
okl4_mmu_lookup_index_geterror(const okl4_mmu_lookup_index_t *x)
{
    okl4_error_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t field : 16;
        } bits;
        okl4_mmu_lookup_index_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_error_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_mmu_lookup_index_seterror, 2n >= 0 && 2n <= 65535) */
/*lint -esym(714, okl4_mmu_lookup_index_seterror) */

/*lint -esym(621, okl4_mmu_lookup_index_seterror) */
OKL4_FORCE_INLINE void
okl4_mmu_lookup_index_seterror(okl4_mmu_lookup_index_t *x, okl4_error_t _error)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t field : 16;
        } bits;
        okl4_mmu_lookup_index_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_error;
    *x = _conv.raw;
}
/*lint -sem(okl4_mmu_lookup_index_getindex, 1p, @n >= 0 && @n <= 65535) */
/*lint -esym(621, okl4_mmu_lookup_index_getindex) */
/*lint -esym(714, okl4_mmu_lookup_index_getindex) */
OKL4_FORCE_INLINE okl4_count_t
okl4_mmu_lookup_index_getindex(const okl4_mmu_lookup_index_t *x)
{
    okl4_count_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 16;
            uint32_t field : 16;
        } bits;
        okl4_mmu_lookup_index_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_count_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_mmu_lookup_index_setindex, 2n >= 0 && 2n <= 65535) */
/*lint -esym(714, okl4_mmu_lookup_index_setindex) */

/*lint -esym(621, okl4_mmu_lookup_index_setindex) */
OKL4_FORCE_INLINE void
okl4_mmu_lookup_index_setindex(okl4_mmu_lookup_index_t *x, okl4_count_t _index)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 16;
            uint32_t field : 16;
        } bits;
        okl4_mmu_lookup_index_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_index;
    *x = _conv.raw;
}
/*lint -esym(714, okl4_mmu_lookup_index_init) */
OKL4_FORCE_INLINE void
okl4_mmu_lookup_index_init(okl4_mmu_lookup_index_t *x)
{
    *x = (okl4_mmu_lookup_index_t)0U;
}

/*lint -esym(714, okl4_mmu_lookup_index_cast) */
OKL4_FORCE_INLINE okl4_mmu_lookup_index_t
okl4_mmu_lookup_index_cast(uint32_t p, okl4_bool_t force)
{
    okl4_mmu_lookup_index_t x = (okl4_mmu_lookup_index_t)p;
    (void)force;
    return x;
}



/**
    - BITS 9..0 -   @ref OKL4_MASK_SEG_INDEX_MMU_LOOKUP_SIZE
    - BITS 63..10 -   @ref OKL4_MASK_SIZE_10_MMU_LOOKUP_SIZE
*/

/*lint -esym(621, okl4_mmu_lookup_size_t) */
typedef okl4_register_t okl4_mmu_lookup_size_t;

/*lint -esym(621, okl4_mmu_lookup_size_getsegindex) */
/*lint -esym(714, okl4_mmu_lookup_size_getsegindex) */
OKL4_FORCE_INLINE okl4_count_t
okl4_mmu_lookup_size_getsegindex(const okl4_mmu_lookup_size_t *x);

/*lint -esym(621, okl4_mmu_lookup_size_setsegindex) */
OKL4_FORCE_INLINE void
okl4_mmu_lookup_size_setsegindex(okl4_mmu_lookup_size_t *x, okl4_count_t _seg_index);

/*lint -esym(621, okl4_mmu_lookup_size_getsize10) */
/*lint -esym(714, okl4_mmu_lookup_size_getsize10) */
OKL4_FORCE_INLINE okl4_register_t
okl4_mmu_lookup_size_getsize10(const okl4_mmu_lookup_size_t *x);

/*lint -esym(621, okl4_mmu_lookup_size_setsize10) */
OKL4_FORCE_INLINE void
okl4_mmu_lookup_size_setsize10(okl4_mmu_lookup_size_t *x, okl4_register_t _size_10);

/*lint -esym(714, okl4_mmu_lookup_size_init) */
OKL4_FORCE_INLINE void
okl4_mmu_lookup_size_init(okl4_mmu_lookup_size_t *x);

/*lint -esym(714, okl4_mmu_lookup_size_cast) */
OKL4_FORCE_INLINE okl4_mmu_lookup_size_t
okl4_mmu_lookup_size_cast(uint64_t p, okl4_bool_t force);




/*lint -esym(621, OKL4_MMU_LOOKUP_SIZE_SEG_INDEX_MASK) */
#define OKL4_MMU_LOOKUP_SIZE_SEG_INDEX_MASK ((okl4_mmu_lookup_size_t)1023U) /* Deprecated */
/*lint -esym(621, OKL4_MASK_SEG_INDEX_MMU_LOOKUP_SIZE) */
#define OKL4_MASK_SEG_INDEX_MMU_LOOKUP_SIZE ((okl4_mmu_lookup_size_t)1023U)
/*lint -esym(621, OKL4_SHIFT_SEG_INDEX_MMU_LOOKUP_SIZE) */
#define OKL4_SHIFT_SEG_INDEX_MMU_LOOKUP_SIZE (0)
/*lint -esym(621, OKL4_WIDTH_SEG_INDEX_MMU_LOOKUP_SIZE) */
#define OKL4_WIDTH_SEG_INDEX_MMU_LOOKUP_SIZE (10)
/*lint -esym(621, OKL4_MMU_LOOKUP_SIZE_SIZE_10_MASK) */
#define OKL4_MMU_LOOKUP_SIZE_SIZE_10_MASK ((okl4_mmu_lookup_size_t)18014398509481983U << 10) /* Deprecated */
/*lint -esym(621, OKL4_MASK_SIZE_10_MMU_LOOKUP_SIZE) */
#define OKL4_MASK_SIZE_10_MMU_LOOKUP_SIZE ((okl4_mmu_lookup_size_t)18014398509481983U << 10)
/*lint -esym(621, OKL4_SHIFT_SIZE_10_MMU_LOOKUP_SIZE) */
#define OKL4_SHIFT_SIZE_10_MMU_LOOKUP_SIZE (10)
/*lint -esym(621, OKL4_WIDTH_SIZE_10_MMU_LOOKUP_SIZE) */
#define OKL4_WIDTH_SIZE_10_MMU_LOOKUP_SIZE (54)


/*lint -sem(okl4_mmu_lookup_size_getsegindex, 1p, @n >= 0 && @n <= 1023) */
/*lint -esym(621, okl4_mmu_lookup_size_getsegindex) */
/*lint -esym(714, okl4_mmu_lookup_size_getsegindex) */
OKL4_FORCE_INLINE okl4_count_t
okl4_mmu_lookup_size_getsegindex(const okl4_mmu_lookup_size_t *x)
{
    okl4_count_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint64_t field : 10;
        } bits;
        okl4_mmu_lookup_size_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_count_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_mmu_lookup_size_setsegindex, 2n >= 0 && 2n <= 1023) */
/*lint -esym(714, okl4_mmu_lookup_size_setsegindex) */

/*lint -esym(621, okl4_mmu_lookup_size_setsegindex) */
OKL4_FORCE_INLINE void
okl4_mmu_lookup_size_setsegindex(okl4_mmu_lookup_size_t *x, okl4_count_t _seg_index)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint64_t field : 10;
        } bits;
        okl4_mmu_lookup_size_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint64_t)_seg_index;
    *x = _conv.raw;
}
/*lint -sem(okl4_mmu_lookup_size_getsize10, 1p, @n >= 0 && @n <= 18014398509481983) */
/*lint -esym(621, okl4_mmu_lookup_size_getsize10) */
/*lint -esym(714, okl4_mmu_lookup_size_getsize10) */
OKL4_FORCE_INLINE okl4_register_t
okl4_mmu_lookup_size_getsize10(const okl4_mmu_lookup_size_t *x)
{
    okl4_register_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint64_t _skip : 10;
            uint64_t field : 54;
        } bits;
        okl4_mmu_lookup_size_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_register_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_mmu_lookup_size_setsize10, 2n >= 0 && 2n <= 18014398509481983) */
/*lint -esym(714, okl4_mmu_lookup_size_setsize10) */

/*lint -esym(621, okl4_mmu_lookup_size_setsize10) */
OKL4_FORCE_INLINE void
okl4_mmu_lookup_size_setsize10(okl4_mmu_lookup_size_t *x, okl4_register_t _size_10)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint64_t _skip : 10;
            uint64_t field : 54;
        } bits;
        okl4_mmu_lookup_size_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint64_t)_size_10;
    *x = _conv.raw;
}
/*lint -esym(714, okl4_mmu_lookup_size_init) */
OKL4_FORCE_INLINE void
okl4_mmu_lookup_size_init(okl4_mmu_lookup_size_t *x)
{
    *x = (okl4_mmu_lookup_size_t)0U;
}

/*lint -esym(714, okl4_mmu_lookup_size_cast) */
OKL4_FORCE_INLINE okl4_mmu_lookup_size_t
okl4_mmu_lookup_size_cast(uint64_t p, okl4_bool_t force)
{
    okl4_mmu_lookup_size_t x = (okl4_mmu_lookup_size_t)p;
    (void)force;
    return x;
}




typedef uint64_t okl4_nanoseconds_t;

/** Timer period upper bound is (1 << 55) ns */
/*lint -esym(621, OKL4_TIMER_MAX_PERIOD_NS) */
#define OKL4_TIMER_MAX_PERIOD_NS ((okl4_nanoseconds_t)(36028797018963968U))

/** Timer period lower bound is 1000000 ns */
/*lint -esym(621, OKL4_TIMER_MIN_PERIOD_NS) */
#define OKL4_TIMER_MIN_PERIOD_NS ((okl4_nanoseconds_t)(1000000U))



/**
    - BITS 2..0 -   @ref _OKL4_MASK_RWX_PAGE_ATTRIBUTE
    - BITS 31..4 -   @ref _OKL4_MASK_ATTRIB_PAGE_ATTRIBUTE
*/

/*lint -esym(621, _okl4_page_attribute_t) */
typedef uint32_t _okl4_page_attribute_t;

/*lint -esym(621, _okl4_page_attribute_getrwx) */
/*lint -esym(714, _okl4_page_attribute_getrwx) */
OKL4_FORCE_INLINE okl4_page_perms_t
_okl4_page_attribute_getrwx(const _okl4_page_attribute_t *x);

/*lint -esym(621, _okl4_page_attribute_setrwx) */
OKL4_FORCE_INLINE void
_okl4_page_attribute_setrwx(_okl4_page_attribute_t *x, okl4_page_perms_t _rwx);

/*lint -esym(621, _okl4_page_attribute_getattrib) */
/*lint -esym(714, _okl4_page_attribute_getattrib) */
OKL4_FORCE_INLINE okl4_page_cache_t
_okl4_page_attribute_getattrib(const _okl4_page_attribute_t *x);

/*lint -esym(621, _okl4_page_attribute_setattrib) */
OKL4_FORCE_INLINE void
_okl4_page_attribute_setattrib(_okl4_page_attribute_t *x, okl4_page_cache_t _attrib);

/*lint -esym(714, _okl4_page_attribute_init) */
OKL4_FORCE_INLINE void
_okl4_page_attribute_init(_okl4_page_attribute_t *x);

/*lint -esym(714, _okl4_page_attribute_cast) */
OKL4_FORCE_INLINE _okl4_page_attribute_t
_okl4_page_attribute_cast(uint32_t p, okl4_bool_t force);




/*lint -esym(621, _OKL4_PAGE_ATTRIBUTE_RWX_MASK) */
#define _OKL4_PAGE_ATTRIBUTE_RWX_MASK ((_okl4_page_attribute_t)7U) /* Deprecated */
/*lint -esym(621, _OKL4_MASK_RWX_PAGE_ATTRIBUTE) */
#define _OKL4_MASK_RWX_PAGE_ATTRIBUTE ((_okl4_page_attribute_t)7U)
/*lint -esym(621, _OKL4_SHIFT_RWX_PAGE_ATTRIBUTE) */
#define _OKL4_SHIFT_RWX_PAGE_ATTRIBUTE (0)
/*lint -esym(621, _OKL4_WIDTH_RWX_PAGE_ATTRIBUTE) */
#define _OKL4_WIDTH_RWX_PAGE_ATTRIBUTE (3)
/*lint -esym(621, _OKL4_PAGE_ATTRIBUTE_ATTRIB_MASK) */
#define _OKL4_PAGE_ATTRIBUTE_ATTRIB_MASK ((_okl4_page_attribute_t)268435455U << 4) /* Deprecated */
/*lint -esym(621, _OKL4_MASK_ATTRIB_PAGE_ATTRIBUTE) */
#define _OKL4_MASK_ATTRIB_PAGE_ATTRIBUTE ((_okl4_page_attribute_t)268435455U << 4)
/*lint -esym(621, _OKL4_SHIFT_ATTRIB_PAGE_ATTRIBUTE) */
#define _OKL4_SHIFT_ATTRIB_PAGE_ATTRIBUTE (4)
/*lint -esym(621, _OKL4_WIDTH_ATTRIB_PAGE_ATTRIBUTE) */
#define _OKL4_WIDTH_ATTRIB_PAGE_ATTRIBUTE (28)


/*lint -sem(_okl4_page_attribute_getrwx, 1p, @n >= 0 && @n <= 7) */
/*lint -esym(621, _okl4_page_attribute_getrwx) */
/*lint -esym(714, _okl4_page_attribute_getrwx) */
OKL4_FORCE_INLINE okl4_page_perms_t
_okl4_page_attribute_getrwx(const _okl4_page_attribute_t *x)
{
    okl4_page_perms_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t field : 3;
        } bits;
        _okl4_page_attribute_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_page_perms_t)_conv.bits.field;
    return field;
}

/*lint -sem(_okl4_page_attribute_setrwx, 2n >= 0 && 2n <= 7) */
/*lint -esym(714, _okl4_page_attribute_setrwx) */

/*lint -esym(621, _okl4_page_attribute_setrwx) */
OKL4_FORCE_INLINE void
_okl4_page_attribute_setrwx(_okl4_page_attribute_t *x, okl4_page_perms_t _rwx)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t field : 3;
        } bits;
        _okl4_page_attribute_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_rwx;
    *x = _conv.raw;
}
/*lint -sem(_okl4_page_attribute_getattrib, 1p, @n >= 0 && @n <= 268435455) */
/*lint -esym(621, _okl4_page_attribute_getattrib) */
/*lint -esym(714, _okl4_page_attribute_getattrib) */
OKL4_FORCE_INLINE okl4_page_cache_t
_okl4_page_attribute_getattrib(const _okl4_page_attribute_t *x)
{
    okl4_page_cache_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 4;
            uint32_t field : 28;
        } bits;
        _okl4_page_attribute_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_page_cache_t)_conv.bits.field;
    return field;
}

/*lint -sem(_okl4_page_attribute_setattrib, 2n >= 0 && 2n <= 268435455) */
/*lint -esym(714, _okl4_page_attribute_setattrib) */

/*lint -esym(621, _okl4_page_attribute_setattrib) */
OKL4_FORCE_INLINE void
_okl4_page_attribute_setattrib(_okl4_page_attribute_t *x, okl4_page_cache_t _attrib)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 4;
            uint32_t field : 28;
        } bits;
        _okl4_page_attribute_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_attrib;
    *x = _conv.raw;
}
/*lint -esym(714, _okl4_page_attribute_init) */
OKL4_FORCE_INLINE void
_okl4_page_attribute_init(_okl4_page_attribute_t *x)
{
    *x = (_okl4_page_attribute_t)0U;
}

/*lint -esym(714, _okl4_page_attribute_cast) */
OKL4_FORCE_INLINE _okl4_page_attribute_t
_okl4_page_attribute_cast(uint32_t p, okl4_bool_t force)
{
    _okl4_page_attribute_t x = (_okl4_page_attribute_t)p;
    (void)force;
    return x;
}



/**
    - BIT 0 -   @ref OKL4_MASK_DO_OP_PIPE_CONTROL
    - BITS 3..1 -   @ref OKL4_MASK_OPERATION_PIPE_CONTROL
*/

/*lint -esym(621, okl4_pipe_control_t) */
typedef uint8_t okl4_pipe_control_t;

/*lint -esym(621, okl4_pipe_control_getdoop) */
/*lint -esym(714, okl4_pipe_control_getdoop) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_pipe_control_getdoop(const okl4_pipe_control_t *x);

/*lint -esym(621, okl4_pipe_control_setdoop) */
OKL4_FORCE_INLINE void
okl4_pipe_control_setdoop(okl4_pipe_control_t *x, okl4_bool_t _do_op);

/*lint -esym(621, okl4_pipe_control_getoperation) */
/*lint -esym(714, okl4_pipe_control_getoperation) */
OKL4_FORCE_INLINE uint8_t
okl4_pipe_control_getoperation(const okl4_pipe_control_t *x);

/*lint -esym(621, okl4_pipe_control_setoperation) */
OKL4_FORCE_INLINE void
okl4_pipe_control_setoperation(okl4_pipe_control_t *x, uint8_t _operation);

/*lint -esym(714, okl4_pipe_control_init) */
OKL4_FORCE_INLINE void
okl4_pipe_control_init(okl4_pipe_control_t *x);

/*lint -esym(714, okl4_pipe_control_cast) */
OKL4_FORCE_INLINE okl4_pipe_control_t
okl4_pipe_control_cast(uint8_t p, okl4_bool_t force);



/*lint -esym(621, OKL4_PIPE_CONTROL_OP_CLR_HALTED) */
#define OKL4_PIPE_CONTROL_OP_CLR_HALTED ((okl4_pipe_control_t)(4U))
/*lint -esym(621, OKL4_PIPE_CONTROL_OP_RESET) */
#define OKL4_PIPE_CONTROL_OP_RESET ((okl4_pipe_control_t)(0U))
/*lint -esym(621, OKL4_PIPE_CONTROL_OP_SET_HALTED) */
#define OKL4_PIPE_CONTROL_OP_SET_HALTED ((okl4_pipe_control_t)(3U))
/*lint -esym(621, OKL4_PIPE_CONTROL_OP_SET_RX_READY) */
#define OKL4_PIPE_CONTROL_OP_SET_RX_READY ((okl4_pipe_control_t)(2U))
/*lint -esym(621, OKL4_PIPE_CONTROL_OP_SET_TX_READY) */
#define OKL4_PIPE_CONTROL_OP_SET_TX_READY ((okl4_pipe_control_t)(1U))

/*lint -esym(621, OKL4_PIPE_CONTROL_DO_OP_MASK) */
#define OKL4_PIPE_CONTROL_DO_OP_MASK (okl4_pipe_control_t)(1U) /* Deprecated */
/*lint -esym(621, OKL4_MASK_DO_OP_PIPE_CONTROL) */
#define OKL4_MASK_DO_OP_PIPE_CONTROL (okl4_pipe_control_t)(1U)
/*lint -esym(621, OKL4_SHIFT_DO_OP_PIPE_CONTROL) */
#define OKL4_SHIFT_DO_OP_PIPE_CONTROL (0)
/*lint -esym(621, OKL4_WIDTH_DO_OP_PIPE_CONTROL) */
#define OKL4_WIDTH_DO_OP_PIPE_CONTROL (1)
/*lint -esym(621, OKL4_PIPE_CONTROL_OPERATION_MASK) */
#define OKL4_PIPE_CONTROL_OPERATION_MASK (okl4_pipe_control_t)(7U << 1) /* Deprecated */
/*lint -esym(621, OKL4_MASK_OPERATION_PIPE_CONTROL) */
#define OKL4_MASK_OPERATION_PIPE_CONTROL (okl4_pipe_control_t)(7U << 1)
/*lint -esym(621, OKL4_SHIFT_OPERATION_PIPE_CONTROL) */
#define OKL4_SHIFT_OPERATION_PIPE_CONTROL (1)
/*lint -esym(621, OKL4_WIDTH_OPERATION_PIPE_CONTROL) */
#define OKL4_WIDTH_OPERATION_PIPE_CONTROL (3)


/*lint -sem(okl4_pipe_control_getdoop, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_pipe_control_getdoop) */
/*lint -esym(714, okl4_pipe_control_getdoop) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_pipe_control_getdoop(const okl4_pipe_control_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            _Bool field : 1;
        } bits;
        okl4_pipe_control_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_pipe_control_setdoop, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_pipe_control_setdoop) */

/*lint -esym(621, okl4_pipe_control_setdoop) */
OKL4_FORCE_INLINE void
okl4_pipe_control_setdoop(okl4_pipe_control_t *x, okl4_bool_t _do_op)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            _Bool field : 1;
        } bits;
        okl4_pipe_control_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_do_op;
    *x = _conv.raw;
}
/*lint -sem(okl4_pipe_control_getoperation, 1p, @n >= 0 && @n <= 7) */
/*lint -esym(621, okl4_pipe_control_getoperation) */
/*lint -esym(714, okl4_pipe_control_getoperation) */
OKL4_FORCE_INLINE uint8_t
okl4_pipe_control_getoperation(const okl4_pipe_control_t *x)
{
    uint8_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 1;
            uint32_t field : 3;
        } bits;
        okl4_pipe_control_t raw;
    } _conv;

    _conv.raw = *x;
    field = (uint8_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_pipe_control_setoperation, 2n >= 0 && 2n <= 7) */
/*lint -esym(714, okl4_pipe_control_setoperation) */

/*lint -esym(621, okl4_pipe_control_setoperation) */
OKL4_FORCE_INLINE void
okl4_pipe_control_setoperation(okl4_pipe_control_t *x, uint8_t _operation)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 1;
            uint32_t field : 3;
        } bits;
        okl4_pipe_control_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_operation;
    *x = _conv.raw;
}
/*lint -esym(714, okl4_pipe_control_init) */
OKL4_FORCE_INLINE void
okl4_pipe_control_init(okl4_pipe_control_t *x)
{
    *x = (okl4_pipe_control_t)0U;
}

/*lint -esym(714, okl4_pipe_control_cast) */
OKL4_FORCE_INLINE okl4_pipe_control_t
okl4_pipe_control_cast(uint8_t p, okl4_bool_t force)
{
    okl4_pipe_control_t x = (okl4_pipe_control_t)p;
    (void)force;
    return x;
}



/**
    - BIT 0 -   @ref OKL4_MASK_RESET_PIPE_STATE
    - BIT 1 -   @ref OKL4_MASK_HALTED_PIPE_STATE
    - BIT 2 -   @ref OKL4_MASK_RX_READY_PIPE_STATE
    - BIT 3 -   @ref OKL4_MASK_TX_READY_PIPE_STATE
    - BIT 4 -   @ref OKL4_MASK_RX_AVAILABLE_PIPE_STATE
    - BIT 5 -   @ref OKL4_MASK_TX_AVAILABLE_PIPE_STATE
    - BIT 6 -   @ref OKL4_MASK_WAITING_PIPE_STATE
    - BIT 7 -   @ref OKL4_MASK_OVERQUOTA_PIPE_STATE
*/

/*lint -esym(621, okl4_pipe_state_t) */
typedef uint8_t okl4_pipe_state_t;

/*lint -esym(621, okl4_pipe_state_getreset) */
/*lint -esym(714, okl4_pipe_state_getreset) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_pipe_state_getreset(const okl4_pipe_state_t *x);

/*lint -esym(621, okl4_pipe_state_setreset) */
OKL4_FORCE_INLINE void
okl4_pipe_state_setreset(okl4_pipe_state_t *x, okl4_bool_t _reset);

/*lint -esym(621, okl4_pipe_state_gethalted) */
/*lint -esym(714, okl4_pipe_state_gethalted) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_pipe_state_gethalted(const okl4_pipe_state_t *x);

/*lint -esym(621, okl4_pipe_state_sethalted) */
OKL4_FORCE_INLINE void
okl4_pipe_state_sethalted(okl4_pipe_state_t *x, okl4_bool_t _halted);

/*lint -esym(621, okl4_pipe_state_getrxready) */
/*lint -esym(714, okl4_pipe_state_getrxready) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_pipe_state_getrxready(const okl4_pipe_state_t *x);

/*lint -esym(621, okl4_pipe_state_setrxready) */
OKL4_FORCE_INLINE void
okl4_pipe_state_setrxready(okl4_pipe_state_t *x, okl4_bool_t _rx_ready);

/*lint -esym(621, okl4_pipe_state_gettxready) */
/*lint -esym(714, okl4_pipe_state_gettxready) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_pipe_state_gettxready(const okl4_pipe_state_t *x);

/*lint -esym(621, okl4_pipe_state_settxready) */
OKL4_FORCE_INLINE void
okl4_pipe_state_settxready(okl4_pipe_state_t *x, okl4_bool_t _tx_ready);

/*lint -esym(621, okl4_pipe_state_getrxavailable) */
/*lint -esym(714, okl4_pipe_state_getrxavailable) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_pipe_state_getrxavailable(const okl4_pipe_state_t *x);

/*lint -esym(621, okl4_pipe_state_setrxavailable) */
OKL4_FORCE_INLINE void
okl4_pipe_state_setrxavailable(okl4_pipe_state_t *x, okl4_bool_t _rx_available);

/*lint -esym(621, okl4_pipe_state_gettxavailable) */
/*lint -esym(714, okl4_pipe_state_gettxavailable) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_pipe_state_gettxavailable(const okl4_pipe_state_t *x);

/*lint -esym(621, okl4_pipe_state_settxavailable) */
OKL4_FORCE_INLINE void
okl4_pipe_state_settxavailable(okl4_pipe_state_t *x, okl4_bool_t _tx_available);

/*lint -esym(621, okl4_pipe_state_getwaiting) */
/*lint -esym(714, okl4_pipe_state_getwaiting) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_pipe_state_getwaiting(const okl4_pipe_state_t *x);

/*lint -esym(621, okl4_pipe_state_setwaiting) */
OKL4_FORCE_INLINE void
okl4_pipe_state_setwaiting(okl4_pipe_state_t *x, okl4_bool_t _waiting);

/*lint -esym(621, okl4_pipe_state_getoverquota) */
/*lint -esym(714, okl4_pipe_state_getoverquota) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_pipe_state_getoverquota(const okl4_pipe_state_t *x);

/*lint -esym(621, okl4_pipe_state_setoverquota) */
OKL4_FORCE_INLINE void
okl4_pipe_state_setoverquota(okl4_pipe_state_t *x, okl4_bool_t _overquota);

/*lint -esym(714, okl4_pipe_state_init) */
OKL4_FORCE_INLINE void
okl4_pipe_state_init(okl4_pipe_state_t *x);

/*lint -esym(714, okl4_pipe_state_cast) */
OKL4_FORCE_INLINE okl4_pipe_state_t
okl4_pipe_state_cast(uint8_t p, okl4_bool_t force);




/*lint -esym(621, OKL4_PIPE_STATE_RESET_MASK) */
#define OKL4_PIPE_STATE_RESET_MASK (okl4_pipe_state_t)(1U) /* Deprecated */
/*lint -esym(621, OKL4_MASK_RESET_PIPE_STATE) */
#define OKL4_MASK_RESET_PIPE_STATE (okl4_pipe_state_t)(1U)
/*lint -esym(621, OKL4_SHIFT_RESET_PIPE_STATE) */
#define OKL4_SHIFT_RESET_PIPE_STATE (0)
/*lint -esym(621, OKL4_WIDTH_RESET_PIPE_STATE) */
#define OKL4_WIDTH_RESET_PIPE_STATE (1)
/*lint -esym(621, OKL4_PIPE_STATE_HALTED_MASK) */
#define OKL4_PIPE_STATE_HALTED_MASK (okl4_pipe_state_t)(1U << 1) /* Deprecated */
/*lint -esym(621, OKL4_MASK_HALTED_PIPE_STATE) */
#define OKL4_MASK_HALTED_PIPE_STATE (okl4_pipe_state_t)(1U << 1)
/*lint -esym(621, OKL4_SHIFT_HALTED_PIPE_STATE) */
#define OKL4_SHIFT_HALTED_PIPE_STATE (1)
/*lint -esym(621, OKL4_WIDTH_HALTED_PIPE_STATE) */
#define OKL4_WIDTH_HALTED_PIPE_STATE (1)
/*lint -esym(621, OKL4_PIPE_STATE_RX_READY_MASK) */
#define OKL4_PIPE_STATE_RX_READY_MASK (okl4_pipe_state_t)(1U << 2) /* Deprecated */
/*lint -esym(621, OKL4_MASK_RX_READY_PIPE_STATE) */
#define OKL4_MASK_RX_READY_PIPE_STATE (okl4_pipe_state_t)(1U << 2)
/*lint -esym(621, OKL4_SHIFT_RX_READY_PIPE_STATE) */
#define OKL4_SHIFT_RX_READY_PIPE_STATE (2)
/*lint -esym(621, OKL4_WIDTH_RX_READY_PIPE_STATE) */
#define OKL4_WIDTH_RX_READY_PIPE_STATE (1)
/*lint -esym(621, OKL4_PIPE_STATE_TX_READY_MASK) */
#define OKL4_PIPE_STATE_TX_READY_MASK (okl4_pipe_state_t)(1U << 3) /* Deprecated */
/*lint -esym(621, OKL4_MASK_TX_READY_PIPE_STATE) */
#define OKL4_MASK_TX_READY_PIPE_STATE (okl4_pipe_state_t)(1U << 3)
/*lint -esym(621, OKL4_SHIFT_TX_READY_PIPE_STATE) */
#define OKL4_SHIFT_TX_READY_PIPE_STATE (3)
/*lint -esym(621, OKL4_WIDTH_TX_READY_PIPE_STATE) */
#define OKL4_WIDTH_TX_READY_PIPE_STATE (1)
/*lint -esym(621, OKL4_PIPE_STATE_RX_AVAILABLE_MASK) */
#define OKL4_PIPE_STATE_RX_AVAILABLE_MASK (okl4_pipe_state_t)(1U << 4) /* Deprecated */
/*lint -esym(621, OKL4_MASK_RX_AVAILABLE_PIPE_STATE) */
#define OKL4_MASK_RX_AVAILABLE_PIPE_STATE (okl4_pipe_state_t)(1U << 4)
/*lint -esym(621, OKL4_SHIFT_RX_AVAILABLE_PIPE_STATE) */
#define OKL4_SHIFT_RX_AVAILABLE_PIPE_STATE (4)
/*lint -esym(621, OKL4_WIDTH_RX_AVAILABLE_PIPE_STATE) */
#define OKL4_WIDTH_RX_AVAILABLE_PIPE_STATE (1)
/*lint -esym(621, OKL4_PIPE_STATE_TX_AVAILABLE_MASK) */
#define OKL4_PIPE_STATE_TX_AVAILABLE_MASK (okl4_pipe_state_t)(1U << 5) /* Deprecated */
/*lint -esym(621, OKL4_MASK_TX_AVAILABLE_PIPE_STATE) */
#define OKL4_MASK_TX_AVAILABLE_PIPE_STATE (okl4_pipe_state_t)(1U << 5)
/*lint -esym(621, OKL4_SHIFT_TX_AVAILABLE_PIPE_STATE) */
#define OKL4_SHIFT_TX_AVAILABLE_PIPE_STATE (5)
/*lint -esym(621, OKL4_WIDTH_TX_AVAILABLE_PIPE_STATE) */
#define OKL4_WIDTH_TX_AVAILABLE_PIPE_STATE (1)
/*lint -esym(621, OKL4_PIPE_STATE_WAITING_MASK) */
#define OKL4_PIPE_STATE_WAITING_MASK (okl4_pipe_state_t)(1U << 6) /* Deprecated */
/*lint -esym(621, OKL4_MASK_WAITING_PIPE_STATE) */
#define OKL4_MASK_WAITING_PIPE_STATE (okl4_pipe_state_t)(1U << 6)
/*lint -esym(621, OKL4_SHIFT_WAITING_PIPE_STATE) */
#define OKL4_SHIFT_WAITING_PIPE_STATE (6)
/*lint -esym(621, OKL4_WIDTH_WAITING_PIPE_STATE) */
#define OKL4_WIDTH_WAITING_PIPE_STATE (1)
/*lint -esym(621, OKL4_PIPE_STATE_OVERQUOTA_MASK) */
#define OKL4_PIPE_STATE_OVERQUOTA_MASK (okl4_pipe_state_t)(1U << 7) /* Deprecated */
/*lint -esym(621, OKL4_MASK_OVERQUOTA_PIPE_STATE) */
#define OKL4_MASK_OVERQUOTA_PIPE_STATE (okl4_pipe_state_t)(1U << 7)
/*lint -esym(621, OKL4_SHIFT_OVERQUOTA_PIPE_STATE) */
#define OKL4_SHIFT_OVERQUOTA_PIPE_STATE (7)
/*lint -esym(621, OKL4_WIDTH_OVERQUOTA_PIPE_STATE) */
#define OKL4_WIDTH_OVERQUOTA_PIPE_STATE (1)


/*lint -sem(okl4_pipe_state_getreset, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_pipe_state_getreset) */
/*lint -esym(714, okl4_pipe_state_getreset) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_pipe_state_getreset(const okl4_pipe_state_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            _Bool field : 1;
        } bits;
        okl4_pipe_state_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_pipe_state_setreset, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_pipe_state_setreset) */

/*lint -esym(621, okl4_pipe_state_setreset) */
OKL4_FORCE_INLINE void
okl4_pipe_state_setreset(okl4_pipe_state_t *x, okl4_bool_t _reset)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            _Bool field : 1;
        } bits;
        okl4_pipe_state_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_reset;
    *x = _conv.raw;
}
/*lint -sem(okl4_pipe_state_gethalted, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_pipe_state_gethalted) */
/*lint -esym(714, okl4_pipe_state_gethalted) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_pipe_state_gethalted(const okl4_pipe_state_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 1;
            _Bool field : 1;
        } bits;
        okl4_pipe_state_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_pipe_state_sethalted, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_pipe_state_sethalted) */

/*lint -esym(621, okl4_pipe_state_sethalted) */
OKL4_FORCE_INLINE void
okl4_pipe_state_sethalted(okl4_pipe_state_t *x, okl4_bool_t _halted)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 1;
            _Bool field : 1;
        } bits;
        okl4_pipe_state_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_halted;
    *x = _conv.raw;
}
/*lint -sem(okl4_pipe_state_getrxready, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_pipe_state_getrxready) */
/*lint -esym(714, okl4_pipe_state_getrxready) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_pipe_state_getrxready(const okl4_pipe_state_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 2;
            _Bool field : 1;
        } bits;
        okl4_pipe_state_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_pipe_state_setrxready, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_pipe_state_setrxready) */

/*lint -esym(621, okl4_pipe_state_setrxready) */
OKL4_FORCE_INLINE void
okl4_pipe_state_setrxready(okl4_pipe_state_t *x, okl4_bool_t _rx_ready)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 2;
            _Bool field : 1;
        } bits;
        okl4_pipe_state_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_rx_ready;
    *x = _conv.raw;
}
/*lint -sem(okl4_pipe_state_gettxready, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_pipe_state_gettxready) */
/*lint -esym(714, okl4_pipe_state_gettxready) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_pipe_state_gettxready(const okl4_pipe_state_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 3;
            _Bool field : 1;
        } bits;
        okl4_pipe_state_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_pipe_state_settxready, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_pipe_state_settxready) */

/*lint -esym(621, okl4_pipe_state_settxready) */
OKL4_FORCE_INLINE void
okl4_pipe_state_settxready(okl4_pipe_state_t *x, okl4_bool_t _tx_ready)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 3;
            _Bool field : 1;
        } bits;
        okl4_pipe_state_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_tx_ready;
    *x = _conv.raw;
}
/*lint -sem(okl4_pipe_state_getrxavailable, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_pipe_state_getrxavailable) */
/*lint -esym(714, okl4_pipe_state_getrxavailable) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_pipe_state_getrxavailable(const okl4_pipe_state_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 4;
            _Bool field : 1;
        } bits;
        okl4_pipe_state_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_pipe_state_setrxavailable, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_pipe_state_setrxavailable) */

/*lint -esym(621, okl4_pipe_state_setrxavailable) */
OKL4_FORCE_INLINE void
okl4_pipe_state_setrxavailable(okl4_pipe_state_t *x, okl4_bool_t _rx_available)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 4;
            _Bool field : 1;
        } bits;
        okl4_pipe_state_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_rx_available;
    *x = _conv.raw;
}
/*lint -sem(okl4_pipe_state_gettxavailable, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_pipe_state_gettxavailable) */
/*lint -esym(714, okl4_pipe_state_gettxavailable) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_pipe_state_gettxavailable(const okl4_pipe_state_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 5;
            _Bool field : 1;
        } bits;
        okl4_pipe_state_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_pipe_state_settxavailable, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_pipe_state_settxavailable) */

/*lint -esym(621, okl4_pipe_state_settxavailable) */
OKL4_FORCE_INLINE void
okl4_pipe_state_settxavailable(okl4_pipe_state_t *x, okl4_bool_t _tx_available)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 5;
            _Bool field : 1;
        } bits;
        okl4_pipe_state_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_tx_available;
    *x = _conv.raw;
}
/*lint -sem(okl4_pipe_state_getwaiting, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_pipe_state_getwaiting) */
/*lint -esym(714, okl4_pipe_state_getwaiting) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_pipe_state_getwaiting(const okl4_pipe_state_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 6;
            _Bool field : 1;
        } bits;
        okl4_pipe_state_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_pipe_state_setwaiting, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_pipe_state_setwaiting) */

/*lint -esym(621, okl4_pipe_state_setwaiting) */
OKL4_FORCE_INLINE void
okl4_pipe_state_setwaiting(okl4_pipe_state_t *x, okl4_bool_t _waiting)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 6;
            _Bool field : 1;
        } bits;
        okl4_pipe_state_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_waiting;
    *x = _conv.raw;
}
/*lint -sem(okl4_pipe_state_getoverquota, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_pipe_state_getoverquota) */
/*lint -esym(714, okl4_pipe_state_getoverquota) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_pipe_state_getoverquota(const okl4_pipe_state_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 7;
            _Bool field : 1;
        } bits;
        okl4_pipe_state_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_pipe_state_setoverquota, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_pipe_state_setoverquota) */

/*lint -esym(621, okl4_pipe_state_setoverquota) */
OKL4_FORCE_INLINE void
okl4_pipe_state_setoverquota(okl4_pipe_state_t *x, okl4_bool_t _overquota)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 7;
            _Bool field : 1;
        } bits;
        okl4_pipe_state_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_overquota;
    *x = _conv.raw;
}
/*lint -esym(714, okl4_pipe_state_init) */
OKL4_FORCE_INLINE void
okl4_pipe_state_init(okl4_pipe_state_t *x)
{
    *x = (okl4_pipe_state_t)1U;
}

/*lint -esym(714, okl4_pipe_state_cast) */
OKL4_FORCE_INLINE okl4_pipe_state_t
okl4_pipe_state_cast(uint8_t p, okl4_bool_t force)
{
    okl4_pipe_state_t x = (okl4_pipe_state_t)p;
    (void)force;
    return x;
}




typedef uint32_t okl4_power_state_t;

/*lint -esym(621, OKL4_POWER_STATE_IDLE) */
#define OKL4_POWER_STATE_IDLE ((okl4_power_state_t)(0U))

/*lint -esym(621, OKL4_POWER_STATE_PLATFORM_BASE) */
#define OKL4_POWER_STATE_PLATFORM_BASE ((okl4_power_state_t)(256U))

/*lint -esym(621, OKL4_POWER_STATE_POWEROFF) */
#define OKL4_POWER_STATE_POWEROFF ((okl4_power_state_t)(1U))



/**
    The okl4_priority_t type represents a thread scheduling priority.
    Valid prioritys range from [0, CONFIG\_SCHEDULER\_NUM\_PRIOS).
*/

typedef int8_t okl4_priority_t;





typedef okl4_register_t okl4_psize_pn_t;





typedef okl4_register_t okl4_psize_tr_t;




/**
    The okl4_register_set_t type is an enumeration identifying one of
    the register sets supported by the host machine. This includes the
    general-purpose registers, along with other CPU-specific register
    sets such as floating point or vector registers.

    - @ref OKL4_REGISTER_SET_CPU_REGS
    - @ref OKL4_REGISTER_SET_VFP_REGS
    - @ref OKL4_REGISTER_SET_VFP_CTRL_REGS
    - @ref OKL4_REGISTER_SET_VFP64_REGS
    - @ref OKL4_REGISTER_SET_VFP128_REGS
    - @ref OKL4_REGISTER_SET_MAX
    - @ref OKL4_REGISTER_SET_INVALID
*/

typedef uint32_t okl4_register_set_t;

/*lint -esym(621, OKL4_REGISTER_SET_CPU_REGS) */
#define OKL4_REGISTER_SET_CPU_REGS ((okl4_register_set_t)0x0U)
/*lint -esym(621, OKL4_REGISTER_SET_VFP_REGS) */
#define OKL4_REGISTER_SET_VFP_REGS ((okl4_register_set_t)0x1U)
/*lint -esym(621, OKL4_REGISTER_SET_VFP_CTRL_REGS) */
#define OKL4_REGISTER_SET_VFP_CTRL_REGS ((okl4_register_set_t)0x2U)
/*lint -esym(621, OKL4_REGISTER_SET_VFP64_REGS) */
#define OKL4_REGISTER_SET_VFP64_REGS ((okl4_register_set_t)0x3U)
/*lint -esym(621, OKL4_REGISTER_SET_VFP128_REGS) */
#define OKL4_REGISTER_SET_VFP128_REGS ((okl4_register_set_t)0x4U)
/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_REGISTER_SET_MAX) */
#define OKL4_REGISTER_SET_MAX ((okl4_register_set_t)0x4U)
/**
    Invalid enumeration value
*/
/*lint -esym(621, OKL4_REGISTER_SET_INVALID) */
#define OKL4_REGISTER_SET_INVALID ((okl4_register_set_t)0xffffffffU)

/*lint -esym(714, okl4_register_set_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_register_set_is_element_of(okl4_register_set_t var);


/*lint -esym(714, okl4_register_set_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_register_set_is_element_of(okl4_register_set_t var)
{
    /*lint --e{944} Disable dead expression detection */
    /*lint --e{948} --e{845} Disable constant always zero */
    return ((var == OKL4_REGISTER_SET_CPU_REGS) ||
            (var == OKL4_REGISTER_SET_VFP_REGS) ||
            (var == OKL4_REGISTER_SET_VFP_CTRL_REGS) ||
            (var == OKL4_REGISTER_SET_VFP64_REGS) ||
            (var == OKL4_REGISTER_SET_VFP128_REGS));
}



typedef okl4_psize_t okl4_vsize_t;




/**
    The okl4_register_and_set_t type is a bitfield containing a register
    set identifier of type okl4_register_set_t, and an index into that
    register set.

    - BITS 15..0 -   @ref OKL4_MASK_OFFSET_REGISTER_AND_SET
    - BITS 31..16 -   @ref OKL4_MASK_SET_REGISTER_AND_SET
*/

/*lint -esym(621, okl4_register_and_set_t) */
typedef uint32_t okl4_register_and_set_t;

/*lint -esym(621, okl4_register_and_set_getoffset) */
/*lint -esym(714, okl4_register_and_set_getoffset) */
OKL4_FORCE_INLINE okl4_vsize_t
okl4_register_and_set_getoffset(const okl4_register_and_set_t *x);

/*lint -esym(621, okl4_register_and_set_setoffset) */
OKL4_FORCE_INLINE void
okl4_register_and_set_setoffset(okl4_register_and_set_t *x, okl4_vsize_t _offset);

/*lint -esym(621, okl4_register_and_set_getset) */
/*lint -esym(714, okl4_register_and_set_getset) */
OKL4_FORCE_INLINE okl4_register_set_t
okl4_register_and_set_getset(const okl4_register_and_set_t *x);

/*lint -esym(621, okl4_register_and_set_setset) */
OKL4_FORCE_INLINE void
okl4_register_and_set_setset(okl4_register_and_set_t *x, okl4_register_set_t _set);

/*lint -esym(714, okl4_register_and_set_init) */
OKL4_FORCE_INLINE void
okl4_register_and_set_init(okl4_register_and_set_t *x);

/*lint -esym(714, okl4_register_and_set_cast) */
OKL4_FORCE_INLINE okl4_register_and_set_t
okl4_register_and_set_cast(uint32_t p, okl4_bool_t force);




/*lint -esym(621, OKL4_REGISTER_AND_SET_OFFSET_MASK) */
#define OKL4_REGISTER_AND_SET_OFFSET_MASK ((okl4_register_and_set_t)65535U) /* Deprecated */
/*lint -esym(621, OKL4_MASK_OFFSET_REGISTER_AND_SET) */
#define OKL4_MASK_OFFSET_REGISTER_AND_SET ((okl4_register_and_set_t)65535U)
/*lint -esym(621, OKL4_SHIFT_OFFSET_REGISTER_AND_SET) */
#define OKL4_SHIFT_OFFSET_REGISTER_AND_SET (0)
/*lint -esym(621, OKL4_WIDTH_OFFSET_REGISTER_AND_SET) */
#define OKL4_WIDTH_OFFSET_REGISTER_AND_SET (16)
/*lint -esym(621, OKL4_REGISTER_AND_SET_SET_MASK) */
#define OKL4_REGISTER_AND_SET_SET_MASK ((okl4_register_and_set_t)65535U << 16) /* Deprecated */
/*lint -esym(621, OKL4_MASK_SET_REGISTER_AND_SET) */
#define OKL4_MASK_SET_REGISTER_AND_SET ((okl4_register_and_set_t)65535U << 16)
/*lint -esym(621, OKL4_SHIFT_SET_REGISTER_AND_SET) */
#define OKL4_SHIFT_SET_REGISTER_AND_SET (16)
/*lint -esym(621, OKL4_WIDTH_SET_REGISTER_AND_SET) */
#define OKL4_WIDTH_SET_REGISTER_AND_SET (16)


/*lint -sem(okl4_register_and_set_getoffset, 1p, @n >= 0 && @n <= 65535) */
/*lint -esym(621, okl4_register_and_set_getoffset) */
/*lint -esym(714, okl4_register_and_set_getoffset) */
OKL4_FORCE_INLINE okl4_vsize_t
okl4_register_and_set_getoffset(const okl4_register_and_set_t *x)
{
    okl4_vsize_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t field : 16;
        } bits;
        okl4_register_and_set_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_vsize_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_register_and_set_setoffset, 2n >= 0 && 2n <= 65535) */
/*lint -esym(714, okl4_register_and_set_setoffset) */

/*lint -esym(621, okl4_register_and_set_setoffset) */
OKL4_FORCE_INLINE void
okl4_register_and_set_setoffset(okl4_register_and_set_t *x, okl4_vsize_t _offset)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t field : 16;
        } bits;
        okl4_register_and_set_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_offset;
    *x = _conv.raw;
}
/*lint -sem(okl4_register_and_set_getset, 1p, @n >= 0 && @n <= 65535) */
/*lint -esym(621, okl4_register_and_set_getset) */
/*lint -esym(714, okl4_register_and_set_getset) */
OKL4_FORCE_INLINE okl4_register_set_t
okl4_register_and_set_getset(const okl4_register_and_set_t *x)
{
    okl4_register_set_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 16;
            uint32_t field : 16;
        } bits;
        okl4_register_and_set_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_register_set_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_register_and_set_setset, 2n >= 0 && 2n <= 65535) */
/*lint -esym(714, okl4_register_and_set_setset) */

/*lint -esym(621, okl4_register_and_set_setset) */
OKL4_FORCE_INLINE void
okl4_register_and_set_setset(okl4_register_and_set_t *x, okl4_register_set_t _set)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 16;
            uint32_t field : 16;
        } bits;
        okl4_register_and_set_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_set;
    *x = _conv.raw;
}
/*lint -esym(714, okl4_register_and_set_init) */
OKL4_FORCE_INLINE void
okl4_register_and_set_init(okl4_register_and_set_t *x)
{
    *x = (okl4_register_and_set_t)0U;
}

/*lint -esym(714, okl4_register_and_set_cast) */
OKL4_FORCE_INLINE okl4_register_and_set_t
okl4_register_and_set_cast(uint32_t p, okl4_bool_t force)
{
    okl4_register_and_set_t x = (okl4_register_and_set_t)p;
    (void)force;
    return x;
}




struct okl4_cpu_registers {
    okl4_register_t x[31];
    okl4_register_t sp_el0;
    okl4_register_t ip;
    uint32_t cpsr;
    _okl4_padding_t __padding0_4; /**< Padding 8 */
    _okl4_padding_t __padding1_5; /**< Padding 8 */
    _okl4_padding_t __padding2_6; /**< Padding 8 */
    _okl4_padding_t __padding3_7; /**< Padding 8 */
    okl4_register_t sp_EL1;
    okl4_register_t elr_EL1;
    uint32_t spsr_EL1;
    uint32_t spsr_abt;
    uint32_t spsr_und;
    uint32_t spsr_irq;
    uint32_t spsr_fiq;
    uint32_t csselr_EL1;
    okl4_arm_sctlr_t sctlr_EL1;
    uint32_t cpacr_EL1;
    uint64_t ttbr0_EL1;
    uint64_t ttbr1_EL1;
    uint64_t tcr_EL1;
    uint32_t dacr32_EL2;
    uint32_t ifsr32_EL2;
    uint32_t esr_EL1;
    _okl4_padding_t __padding4_4; /**< Padding 8 */
    _okl4_padding_t __padding5_5; /**< Padding 8 */
    _okl4_padding_t __padding6_6; /**< Padding 8 */
    _okl4_padding_t __padding7_7; /**< Padding 8 */
    uint64_t far_EL1;
    uint64_t par_EL1;
    uint64_t mair_EL1;
    uint64_t vbar_EL1;
    uint32_t contextidr_EL1;
    _okl4_padding_t __padding8_4; /**< Padding 8 */
    _okl4_padding_t __padding9_5; /**< Padding 8 */
    _okl4_padding_t __padding10_6; /**< Padding 8 */
    _okl4_padding_t __padding11_7; /**< Padding 8 */
    uint64_t tpidr_EL1;
    uint64_t tpidrro_EL0;
    uint64_t tpidr_EL0;
    uint32_t pmcr_EL0;
    _okl4_padding_t __padding12_4; /**< Padding 8 */
    _okl4_padding_t __padding13_5; /**< Padding 8 */
    _okl4_padding_t __padding14_6; /**< Padding 8 */
    _okl4_padding_t __padding15_7; /**< Padding 8 */
    uint64_t pmccntr_EL0;
    uint32_t fpexc32_EL2;
    uint32_t cntkctl_EL1;
};






/**
    The okl4_cpu_registers_t type represents a set of CPU general-purpose
    registers on the native machine.
*/

typedef struct okl4_cpu_registers okl4_cpu_registers_t;




/**
    The `okl4_rights_t` type represents a set of operations that are allowed to
    be performed using a given cap.
*/

typedef uint32_t okl4_rights_t;





typedef uint64_t okl4_soc_time_t;




/**

*/

struct okl4_schedule_profile_data {
    okl4_soc_time_t timestamp;
    okl4_soc_time_t cpu_time;
    okl4_count_t context_switches;
    okl4_count_t cpu_migrations;
    okl4_count_t cpu_hwirqs;
    okl4_count_t cpu_virqs;
};




/**
    - BIT 0 -   @ref OKL4_MASK_POWER_SUSPENDED_SCHEDULER_VIRQ_FLAGS
*/

/*lint -esym(621, okl4_scheduler_virq_flags_t) */
typedef okl4_virq_flags_t okl4_scheduler_virq_flags_t;

/*lint -esym(621, okl4_scheduler_virq_flags_getpowersuspended) */
/*lint -esym(714, okl4_scheduler_virq_flags_getpowersuspended) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_scheduler_virq_flags_getpowersuspended(const okl4_scheduler_virq_flags_t *x);

/*lint -esym(621, okl4_scheduler_virq_flags_setpowersuspended) */
OKL4_FORCE_INLINE void
okl4_scheduler_virq_flags_setpowersuspended(okl4_scheduler_virq_flags_t *x, okl4_bool_t _power_suspended);

/*lint -esym(714, okl4_scheduler_virq_flags_init) */
OKL4_FORCE_INLINE void
okl4_scheduler_virq_flags_init(okl4_scheduler_virq_flags_t *x);

/*lint -esym(714, okl4_scheduler_virq_flags_cast) */
OKL4_FORCE_INLINE okl4_scheduler_virq_flags_t
okl4_scheduler_virq_flags_cast(uint64_t p, okl4_bool_t force);




/*lint -esym(621, OKL4_SCHEDULER_VIRQ_FLAGS_POWER_SUSPENDED_MASK) */
#define OKL4_SCHEDULER_VIRQ_FLAGS_POWER_SUSPENDED_MASK ((okl4_scheduler_virq_flags_t)1U) /* Deprecated */
/*lint -esym(621, OKL4_MASK_POWER_SUSPENDED_SCHEDULER_VIRQ_FLAGS) */
#define OKL4_MASK_POWER_SUSPENDED_SCHEDULER_VIRQ_FLAGS ((okl4_scheduler_virq_flags_t)1U)
/*lint -esym(621, OKL4_SHIFT_POWER_SUSPENDED_SCHEDULER_VIRQ_FLAGS) */
#define OKL4_SHIFT_POWER_SUSPENDED_SCHEDULER_VIRQ_FLAGS (0)
/*lint -esym(621, OKL4_WIDTH_POWER_SUSPENDED_SCHEDULER_VIRQ_FLAGS) */
#define OKL4_WIDTH_POWER_SUSPENDED_SCHEDULER_VIRQ_FLAGS (1)


/*lint -sem(okl4_scheduler_virq_flags_getpowersuspended, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_scheduler_virq_flags_getpowersuspended) */
/*lint -esym(714, okl4_scheduler_virq_flags_getpowersuspended) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_scheduler_virq_flags_getpowersuspended(const okl4_scheduler_virq_flags_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            _Bool field : 1;
        } bits;
        okl4_scheduler_virq_flags_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_scheduler_virq_flags_setpowersuspended, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_scheduler_virq_flags_setpowersuspended) */

/*lint -esym(621, okl4_scheduler_virq_flags_setpowersuspended) */
OKL4_FORCE_INLINE void
okl4_scheduler_virq_flags_setpowersuspended(okl4_scheduler_virq_flags_t *x, okl4_bool_t _power_suspended)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            _Bool field : 1;
        } bits;
        okl4_scheduler_virq_flags_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_power_suspended;
    *x = _conv.raw;
}
/*lint -esym(714, okl4_scheduler_virq_flags_init) */
OKL4_FORCE_INLINE void
okl4_scheduler_virq_flags_init(okl4_scheduler_virq_flags_t *x)
{
    *x = (okl4_scheduler_virq_flags_t)0U;
}

/*lint -esym(714, okl4_scheduler_virq_flags_cast) */
OKL4_FORCE_INLINE okl4_scheduler_virq_flags_t
okl4_scheduler_virq_flags_cast(uint64_t p, okl4_bool_t force)
{
    okl4_scheduler_virq_flags_t x = (okl4_scheduler_virq_flags_t)p;
    (void)force;
    return x;
}



/**
    The `okl4_scount_t` type represents a natural number of items or
    iterations. Negative values represent errors. Use `okl4_count_t` if error
    values are not required.
*/

typedef int32_t okl4_scount_t;




/**
    The SDK_VERSION contains a global SDK wide versioning of software.

    - BITS 5..0 -   @ref OKL4_MASK_MAINTENANCE_SDK_VERSION
    - BITS 15..8 -   @ref OKL4_MASK_RELEASE_SDK_VERSION
    - BITS 21..16 -   @ref OKL4_MASK_MINOR_SDK_VERSION
    - BITS 27..24 -   @ref OKL4_MASK_MAJOR_SDK_VERSION
    - BIT 28 -   @ref OKL4_MASK_RES0_FLAG_SDK_VERSION
    - BIT 30 -   @ref OKL4_MASK_DEV_FLAG_SDK_VERSION
    - BIT 31 -   @ref OKL4_MASK_FORMAT_FLAG_SDK_VERSION
*/

/*lint -esym(621, okl4_sdk_version_t) */
typedef uint32_t okl4_sdk_version_t;

/*lint -esym(621, okl4_sdk_version_getformatflag) */
/*lint -esym(714, okl4_sdk_version_getformatflag) */
OKL4_FORCE_INLINE uint32_t
okl4_sdk_version_getformatflag(const okl4_sdk_version_t *x);

/*lint -esym(621, okl4_sdk_version_setformatflag) */
OKL4_FORCE_INLINE void
okl4_sdk_version_setformatflag(okl4_sdk_version_t *x, uint32_t _format_flag);

/*lint -esym(621, okl4_sdk_version_getdevflag) */
/*lint -esym(714, okl4_sdk_version_getdevflag) */
OKL4_FORCE_INLINE uint32_t
okl4_sdk_version_getdevflag(const okl4_sdk_version_t *x);

/*lint -esym(621, okl4_sdk_version_setdevflag) */
OKL4_FORCE_INLINE void
okl4_sdk_version_setdevflag(okl4_sdk_version_t *x, uint32_t _dev_flag);

/*lint -esym(621, okl4_sdk_version_getres0flag) */
/*lint -esym(714, okl4_sdk_version_getres0flag) */
OKL4_FORCE_INLINE uint32_t
okl4_sdk_version_getres0flag(const okl4_sdk_version_t *x);

/*lint -esym(621, okl4_sdk_version_setres0flag) */
OKL4_FORCE_INLINE void
okl4_sdk_version_setres0flag(okl4_sdk_version_t *x, uint32_t _res0_flag);

/*lint -esym(621, okl4_sdk_version_getmajor) */
/*lint -esym(714, okl4_sdk_version_getmajor) */
OKL4_FORCE_INLINE uint32_t
okl4_sdk_version_getmajor(const okl4_sdk_version_t *x);

/*lint -esym(621, okl4_sdk_version_setmajor) */
OKL4_FORCE_INLINE void
okl4_sdk_version_setmajor(okl4_sdk_version_t *x, uint32_t _major);

/*lint -esym(621, okl4_sdk_version_getminor) */
/*lint -esym(714, okl4_sdk_version_getminor) */
OKL4_FORCE_INLINE uint32_t
okl4_sdk_version_getminor(const okl4_sdk_version_t *x);

/*lint -esym(621, okl4_sdk_version_setminor) */
OKL4_FORCE_INLINE void
okl4_sdk_version_setminor(okl4_sdk_version_t *x, uint32_t _minor);

/*lint -esym(621, okl4_sdk_version_getrelease) */
/*lint -esym(714, okl4_sdk_version_getrelease) */
OKL4_FORCE_INLINE uint32_t
okl4_sdk_version_getrelease(const okl4_sdk_version_t *x);

/*lint -esym(621, okl4_sdk_version_setrelease) */
OKL4_FORCE_INLINE void
okl4_sdk_version_setrelease(okl4_sdk_version_t *x, uint32_t _release);

/*lint -esym(621, okl4_sdk_version_getmaintenance) */
/*lint -esym(714, okl4_sdk_version_getmaintenance) */
OKL4_FORCE_INLINE uint32_t
okl4_sdk_version_getmaintenance(const okl4_sdk_version_t *x);

/*lint -esym(621, okl4_sdk_version_setmaintenance) */
OKL4_FORCE_INLINE void
okl4_sdk_version_setmaintenance(okl4_sdk_version_t *x, uint32_t _maintenance);

/*lint -esym(714, okl4_sdk_version_init) */
OKL4_FORCE_INLINE void
okl4_sdk_version_init(okl4_sdk_version_t *x);

/*lint -esym(714, okl4_sdk_version_cast) */
OKL4_FORCE_INLINE okl4_sdk_version_t
okl4_sdk_version_cast(uint32_t p, okl4_bool_t force);




/*lint -esym(621, OKL4_SDK_VERSION_MAINTENANCE_MASK) */
#define OKL4_SDK_VERSION_MAINTENANCE_MASK ((okl4_sdk_version_t)63U) /* Deprecated */
/** Maintenance number */
/*lint -esym(621, OKL4_MASK_MAINTENANCE_SDK_VERSION) */
#define OKL4_MASK_MAINTENANCE_SDK_VERSION ((okl4_sdk_version_t)63U)
/*lint -esym(621, OKL4_SHIFT_MAINTENANCE_SDK_VERSION) */
#define OKL4_SHIFT_MAINTENANCE_SDK_VERSION (0)
/*lint -esym(621, OKL4_WIDTH_MAINTENANCE_SDK_VERSION) */
#define OKL4_WIDTH_MAINTENANCE_SDK_VERSION (6)
/*lint -esym(621, OKL4_SDK_VERSION_RELEASE_MASK) */
#define OKL4_SDK_VERSION_RELEASE_MASK ((okl4_sdk_version_t)255U << 8) /* Deprecated */
/** SDK Release Number */
/*lint -esym(621, OKL4_MASK_RELEASE_SDK_VERSION) */
#define OKL4_MASK_RELEASE_SDK_VERSION ((okl4_sdk_version_t)255U << 8)
/*lint -esym(621, OKL4_SHIFT_RELEASE_SDK_VERSION) */
#define OKL4_SHIFT_RELEASE_SDK_VERSION (8)
/*lint -esym(621, OKL4_WIDTH_RELEASE_SDK_VERSION) */
#define OKL4_WIDTH_RELEASE_SDK_VERSION (8)
/*lint -esym(621, OKL4_SDK_VERSION_MINOR_MASK) */
#define OKL4_SDK_VERSION_MINOR_MASK ((okl4_sdk_version_t)63U << 16) /* Deprecated */
/** SDK Minor Number */
/*lint -esym(621, OKL4_MASK_MINOR_SDK_VERSION) */
#define OKL4_MASK_MINOR_SDK_VERSION ((okl4_sdk_version_t)63U << 16)
/*lint -esym(621, OKL4_SHIFT_MINOR_SDK_VERSION) */
#define OKL4_SHIFT_MINOR_SDK_VERSION (16)
/*lint -esym(621, OKL4_WIDTH_MINOR_SDK_VERSION) */
#define OKL4_WIDTH_MINOR_SDK_VERSION (6)
/*lint -esym(621, OKL4_SDK_VERSION_MAJOR_MASK) */
#define OKL4_SDK_VERSION_MAJOR_MASK ((okl4_sdk_version_t)15U << 24) /* Deprecated */
/** SDK Major Number */
/*lint -esym(621, OKL4_MASK_MAJOR_SDK_VERSION) */
#define OKL4_MASK_MAJOR_SDK_VERSION ((okl4_sdk_version_t)15U << 24)
/*lint -esym(621, OKL4_SHIFT_MAJOR_SDK_VERSION) */
#define OKL4_SHIFT_MAJOR_SDK_VERSION (24)
/*lint -esym(621, OKL4_WIDTH_MAJOR_SDK_VERSION) */
#define OKL4_WIDTH_MAJOR_SDK_VERSION (4)
/*lint -esym(621, OKL4_SDK_VERSION_RES0_FLAG_MASK) */
#define OKL4_SDK_VERSION_RES0_FLAG_MASK ((okl4_sdk_version_t)1U << 28) /* Deprecated */
/** Reserved */
/*lint -esym(621, OKL4_MASK_RES0_FLAG_SDK_VERSION) */
#define OKL4_MASK_RES0_FLAG_SDK_VERSION ((okl4_sdk_version_t)1U << 28)
/*lint -esym(621, OKL4_SHIFT_RES0_FLAG_SDK_VERSION) */
#define OKL4_SHIFT_RES0_FLAG_SDK_VERSION (28)
/*lint -esym(621, OKL4_WIDTH_RES0_FLAG_SDK_VERSION) */
#define OKL4_WIDTH_RES0_FLAG_SDK_VERSION (1)
/*lint -esym(621, OKL4_SDK_VERSION_DEV_FLAG_MASK) */
#define OKL4_SDK_VERSION_DEV_FLAG_MASK ((okl4_sdk_version_t)1U << 30) /* Deprecated */
/** Unreleased internal development version */
/*lint -esym(621, OKL4_MASK_DEV_FLAG_SDK_VERSION) */
#define OKL4_MASK_DEV_FLAG_SDK_VERSION ((okl4_sdk_version_t)1U << 30)
/*lint -esym(621, OKL4_SHIFT_DEV_FLAG_SDK_VERSION) */
#define OKL4_SHIFT_DEV_FLAG_SDK_VERSION (30)
/*lint -esym(621, OKL4_WIDTH_DEV_FLAG_SDK_VERSION) */
#define OKL4_WIDTH_DEV_FLAG_SDK_VERSION (1)
/*lint -esym(621, OKL4_SDK_VERSION_FORMAT_FLAG_MASK) */
#define OKL4_SDK_VERSION_FORMAT_FLAG_MASK ((okl4_sdk_version_t)1U << 31) /* Deprecated */
/** Format: 0 = Version format 1, 1 = Reserved */
/*lint -esym(621, OKL4_MASK_FORMAT_FLAG_SDK_VERSION) */
#define OKL4_MASK_FORMAT_FLAG_SDK_VERSION ((okl4_sdk_version_t)1U << 31)
/*lint -esym(621, OKL4_SHIFT_FORMAT_FLAG_SDK_VERSION) */
#define OKL4_SHIFT_FORMAT_FLAG_SDK_VERSION (31)
/*lint -esym(621, OKL4_WIDTH_FORMAT_FLAG_SDK_VERSION) */
#define OKL4_WIDTH_FORMAT_FLAG_SDK_VERSION (1)


/*lint -sem(okl4_sdk_version_getmaintenance, 1p, @n >= 0 && @n <= 63) */
/*lint -esym(621, okl4_sdk_version_getmaintenance) */
/*lint -esym(714, okl4_sdk_version_getmaintenance) */
OKL4_FORCE_INLINE uint32_t
okl4_sdk_version_getmaintenance(const okl4_sdk_version_t *x)
{
    uint32_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t field : 6;
        } bits;
        okl4_sdk_version_t raw;
    } _conv;

    _conv.raw = *x;
    field = (uint32_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_sdk_version_setmaintenance, 2n >= 0 && 2n <= 63) */
/*lint -esym(714, okl4_sdk_version_setmaintenance) */

/*lint -esym(621, okl4_sdk_version_setmaintenance) */
OKL4_FORCE_INLINE void
okl4_sdk_version_setmaintenance(okl4_sdk_version_t *x, uint32_t _maintenance)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t field : 6;
        } bits;
        okl4_sdk_version_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_maintenance;
    *x = _conv.raw;
}
/*lint -sem(okl4_sdk_version_getrelease, 1p, @n >= 0 && @n <= 255) */
/*lint -esym(621, okl4_sdk_version_getrelease) */
/*lint -esym(714, okl4_sdk_version_getrelease) */
OKL4_FORCE_INLINE uint32_t
okl4_sdk_version_getrelease(const okl4_sdk_version_t *x)
{
    uint32_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 8;
            uint32_t field : 8;
        } bits;
        okl4_sdk_version_t raw;
    } _conv;

    _conv.raw = *x;
    field = (uint32_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_sdk_version_setrelease, 2n >= 0 && 2n <= 255) */
/*lint -esym(714, okl4_sdk_version_setrelease) */

/*lint -esym(621, okl4_sdk_version_setrelease) */
OKL4_FORCE_INLINE void
okl4_sdk_version_setrelease(okl4_sdk_version_t *x, uint32_t _release)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 8;
            uint32_t field : 8;
        } bits;
        okl4_sdk_version_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_release;
    *x = _conv.raw;
}
/*lint -sem(okl4_sdk_version_getminor, 1p, @n >= 0 && @n <= 63) */
/*lint -esym(621, okl4_sdk_version_getminor) */
/*lint -esym(714, okl4_sdk_version_getminor) */
OKL4_FORCE_INLINE uint32_t
okl4_sdk_version_getminor(const okl4_sdk_version_t *x)
{
    uint32_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 16;
            uint32_t field : 6;
        } bits;
        okl4_sdk_version_t raw;
    } _conv;

    _conv.raw = *x;
    field = (uint32_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_sdk_version_setminor, 2n >= 0 && 2n <= 63) */
/*lint -esym(714, okl4_sdk_version_setminor) */

/*lint -esym(621, okl4_sdk_version_setminor) */
OKL4_FORCE_INLINE void
okl4_sdk_version_setminor(okl4_sdk_version_t *x, uint32_t _minor)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 16;
            uint32_t field : 6;
        } bits;
        okl4_sdk_version_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_minor;
    *x = _conv.raw;
}
/*lint -sem(okl4_sdk_version_getmajor, 1p, @n >= 0 && @n <= 15) */
/*lint -esym(621, okl4_sdk_version_getmajor) */
/*lint -esym(714, okl4_sdk_version_getmajor) */
OKL4_FORCE_INLINE uint32_t
okl4_sdk_version_getmajor(const okl4_sdk_version_t *x)
{
    uint32_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 24;
            uint32_t field : 4;
        } bits;
        okl4_sdk_version_t raw;
    } _conv;

    _conv.raw = *x;
    field = (uint32_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_sdk_version_setmajor, 2n >= 0 && 2n <= 15) */
/*lint -esym(714, okl4_sdk_version_setmajor) */

/*lint -esym(621, okl4_sdk_version_setmajor) */
OKL4_FORCE_INLINE void
okl4_sdk_version_setmajor(okl4_sdk_version_t *x, uint32_t _major)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 24;
            uint32_t field : 4;
        } bits;
        okl4_sdk_version_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_major;
    *x = _conv.raw;
}
/*lint -sem(okl4_sdk_version_getres0flag, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_sdk_version_getres0flag) */
/*lint -esym(714, okl4_sdk_version_getres0flag) */
OKL4_FORCE_INLINE uint32_t
okl4_sdk_version_getres0flag(const okl4_sdk_version_t *x)
{
    uint32_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 28;
            uint32_t field : 1;
        } bits;
        okl4_sdk_version_t raw;
    } _conv;

    _conv.raw = *x;
    field = (uint32_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_sdk_version_setres0flag, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_sdk_version_setres0flag) */

/*lint -esym(621, okl4_sdk_version_setres0flag) */
OKL4_FORCE_INLINE void
okl4_sdk_version_setres0flag(okl4_sdk_version_t *x, uint32_t _res0_flag)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 28;
            uint32_t field : 1;
        } bits;
        okl4_sdk_version_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_res0_flag;
    *x = _conv.raw;
}
/*lint -sem(okl4_sdk_version_getdevflag, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_sdk_version_getdevflag) */
/*lint -esym(714, okl4_sdk_version_getdevflag) */
OKL4_FORCE_INLINE uint32_t
okl4_sdk_version_getdevflag(const okl4_sdk_version_t *x)
{
    uint32_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 30;
            uint32_t field : 1;
        } bits;
        okl4_sdk_version_t raw;
    } _conv;

    _conv.raw = *x;
    field = (uint32_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_sdk_version_setdevflag, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_sdk_version_setdevflag) */

/*lint -esym(621, okl4_sdk_version_setdevflag) */
OKL4_FORCE_INLINE void
okl4_sdk_version_setdevflag(okl4_sdk_version_t *x, uint32_t _dev_flag)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 30;
            uint32_t field : 1;
        } bits;
        okl4_sdk_version_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_dev_flag;
    *x = _conv.raw;
}
/*lint -sem(okl4_sdk_version_getformatflag, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_sdk_version_getformatflag) */
/*lint -esym(714, okl4_sdk_version_getformatflag) */
OKL4_FORCE_INLINE uint32_t
okl4_sdk_version_getformatflag(const okl4_sdk_version_t *x)
{
    uint32_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 31;
            uint32_t field : 1;
        } bits;
        okl4_sdk_version_t raw;
    } _conv;

    _conv.raw = *x;
    field = (uint32_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_sdk_version_setformatflag, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_sdk_version_setformatflag) */

/*lint -esym(621, okl4_sdk_version_setformatflag) */
OKL4_FORCE_INLINE void
okl4_sdk_version_setformatflag(okl4_sdk_version_t *x, uint32_t _format_flag)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 31;
            uint32_t field : 1;
        } bits;
        okl4_sdk_version_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_format_flag;
    *x = _conv.raw;
}
/*lint -esym(714, okl4_sdk_version_init) */
OKL4_FORCE_INLINE void
okl4_sdk_version_init(okl4_sdk_version_t *x)
{
    *x = (okl4_sdk_version_t)0U;
}

/*lint -esym(714, okl4_sdk_version_cast) */
OKL4_FORCE_INLINE okl4_sdk_version_t
okl4_sdk_version_cast(uint32_t p, okl4_bool_t force)
{
    okl4_sdk_version_t x = (okl4_sdk_version_t)p;
    (void)force;
    return x;
}



/**

*/

struct okl4_shared_buffer {
    okl4_paddr_t physical_base;
    struct okl4_virtmem_item virtmem_item;
    okl4_kcap_t cap;
    _okl4_padding_t __padding0_4;
    _okl4_padding_t __padding1_5;
    _okl4_padding_t __padding2_6;
    _okl4_padding_t __padding3_7;
};




/**

*/

struct okl4_shared_buffers_array {
    __ptr64(struct okl4_shared_buffer *, buffers);
    okl4_count_t num_buffers;
    _okl4_padding_t __padding0_4;
    _okl4_padding_t __padding1_5;
    _okl4_padding_t __padding2_6;
    _okl4_padding_t __padding3_7;
};





typedef okl4_kcap_t okl4_signal_t;








/**
    The `okl4_sregister_t` type represents a signed, machine-native
    register-sized integer value.
*/

typedef int64_t okl4_sregister_t;





typedef uint64_t okl4_ticks_t;




/**
    - BIT 0 -   @ref OKL4_MASK_ACTIVE_TIMER_FLAGS
    - BIT 1 -   @ref OKL4_MASK_PERIODIC_TIMER_FLAGS
    - BIT 2 -   @ref OKL4_MASK_ABSOLUTE_TIMER_FLAGS
    - BIT 3 -   @ref OKL4_MASK_UNITS_TIMER_FLAGS
    - BIT 4 -   @ref OKL4_MASK_ALIGN_TIMER_FLAGS
    - BIT 5 -   @ref OKL4_MASK_WATCHDOG_TIMER_FLAGS
    - BIT 30 -   @ref OKL4_MASK_RELOAD_TIMER_FLAGS
    - BIT 31 -   @ref OKL4_MASK_TIMESLICE_TIMER_FLAGS
*/

/*lint -esym(621, okl4_timer_flags_t) */
typedef uint32_t okl4_timer_flags_t;

/*lint -esym(621, okl4_timer_flags_getactive) */
/*lint -esym(714, okl4_timer_flags_getactive) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_timer_flags_getactive(const okl4_timer_flags_t *x);

/*lint -esym(621, okl4_timer_flags_setactive) */
OKL4_FORCE_INLINE void
okl4_timer_flags_setactive(okl4_timer_flags_t *x, okl4_bool_t _active);

/*lint -esym(621, okl4_timer_flags_getperiodic) */
/*lint -esym(714, okl4_timer_flags_getperiodic) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_timer_flags_getperiodic(const okl4_timer_flags_t *x);

/*lint -esym(621, okl4_timer_flags_setperiodic) */
OKL4_FORCE_INLINE void
okl4_timer_flags_setperiodic(okl4_timer_flags_t *x, okl4_bool_t _periodic);

/*lint -esym(621, okl4_timer_flags_getabsolute) */
/*lint -esym(714, okl4_timer_flags_getabsolute) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_timer_flags_getabsolute(const okl4_timer_flags_t *x);

/*lint -esym(621, okl4_timer_flags_setabsolute) */
OKL4_FORCE_INLINE void
okl4_timer_flags_setabsolute(okl4_timer_flags_t *x, okl4_bool_t _absolute);

/*lint -esym(621, okl4_timer_flags_getunits) */
/*lint -esym(714, okl4_timer_flags_getunits) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_timer_flags_getunits(const okl4_timer_flags_t *x);

/*lint -esym(621, okl4_timer_flags_setunits) */
OKL4_FORCE_INLINE void
okl4_timer_flags_setunits(okl4_timer_flags_t *x, okl4_bool_t _units);

/*lint -esym(621, okl4_timer_flags_getalign) */
/*lint -esym(714, okl4_timer_flags_getalign) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_timer_flags_getalign(const okl4_timer_flags_t *x);

/*lint -esym(621, okl4_timer_flags_setalign) */
OKL4_FORCE_INLINE void
okl4_timer_flags_setalign(okl4_timer_flags_t *x, okl4_bool_t _align);

/*lint -esym(621, okl4_timer_flags_getwatchdog) */
/*lint -esym(714, okl4_timer_flags_getwatchdog) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_timer_flags_getwatchdog(const okl4_timer_flags_t *x);

/*lint -esym(621, okl4_timer_flags_setwatchdog) */
OKL4_FORCE_INLINE void
okl4_timer_flags_setwatchdog(okl4_timer_flags_t *x, okl4_bool_t _watchdog);

/*lint -esym(621, okl4_timer_flags_getreload) */
/*lint -esym(714, okl4_timer_flags_getreload) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_timer_flags_getreload(const okl4_timer_flags_t *x);

/*lint -esym(621, okl4_timer_flags_setreload) */
OKL4_FORCE_INLINE void
okl4_timer_flags_setreload(okl4_timer_flags_t *x, okl4_bool_t _reload);

/*lint -esym(621, okl4_timer_flags_gettimeslice) */
/*lint -esym(714, okl4_timer_flags_gettimeslice) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_timer_flags_gettimeslice(const okl4_timer_flags_t *x);

/*lint -esym(621, okl4_timer_flags_settimeslice) */
OKL4_FORCE_INLINE void
okl4_timer_flags_settimeslice(okl4_timer_flags_t *x, okl4_bool_t _timeslice);

/*lint -esym(714, okl4_timer_flags_init) */
OKL4_FORCE_INLINE void
okl4_timer_flags_init(okl4_timer_flags_t *x);

/*lint -esym(714, okl4_timer_flags_cast) */
OKL4_FORCE_INLINE okl4_timer_flags_t
okl4_timer_flags_cast(uint32_t p, okl4_bool_t force);




/*lint -esym(621, OKL4_TIMER_FLAGS_ACTIVE_MASK) */
#define OKL4_TIMER_FLAGS_ACTIVE_MASK ((okl4_timer_flags_t)1U) /* Deprecated */
/** Indicates that the timer has a timeout set */
/*lint -esym(621, OKL4_MASK_ACTIVE_TIMER_FLAGS) */
#define OKL4_MASK_ACTIVE_TIMER_FLAGS ((okl4_timer_flags_t)1U)
/*lint -esym(621, OKL4_SHIFT_ACTIVE_TIMER_FLAGS) */
#define OKL4_SHIFT_ACTIVE_TIMER_FLAGS (0)
/*lint -esym(621, OKL4_WIDTH_ACTIVE_TIMER_FLAGS) */
#define OKL4_WIDTH_ACTIVE_TIMER_FLAGS (1)
/*lint -esym(621, OKL4_TIMER_FLAGS_PERIODIC_MASK) */
#define OKL4_TIMER_FLAGS_PERIODIC_MASK ((okl4_timer_flags_t)1U << 1) /* Deprecated */
/** Indicates that the timer is periodic, otherwise it is one-shot */
/*lint -esym(621, OKL4_MASK_PERIODIC_TIMER_FLAGS) */
#define OKL4_MASK_PERIODIC_TIMER_FLAGS ((okl4_timer_flags_t)1U << 1)
/*lint -esym(621, OKL4_SHIFT_PERIODIC_TIMER_FLAGS) */
#define OKL4_SHIFT_PERIODIC_TIMER_FLAGS (1)
/*lint -esym(621, OKL4_WIDTH_PERIODIC_TIMER_FLAGS) */
#define OKL4_WIDTH_PERIODIC_TIMER_FLAGS (1)
/*lint -esym(621, OKL4_TIMER_FLAGS_ABSOLUTE_MASK) */
#define OKL4_TIMER_FLAGS_ABSOLUTE_MASK ((okl4_timer_flags_t)1U << 2) /* Deprecated */
/** Indicates that the timeout value is absolute, otherwise it is relative */
/*lint -esym(621, OKL4_MASK_ABSOLUTE_TIMER_FLAGS) */
#define OKL4_MASK_ABSOLUTE_TIMER_FLAGS ((okl4_timer_flags_t)1U << 2)
/*lint -esym(621, OKL4_SHIFT_ABSOLUTE_TIMER_FLAGS) */
#define OKL4_SHIFT_ABSOLUTE_TIMER_FLAGS (2)
/*lint -esym(621, OKL4_WIDTH_ABSOLUTE_TIMER_FLAGS) */
#define OKL4_WIDTH_ABSOLUTE_TIMER_FLAGS (1)
/*lint -esym(621, OKL4_TIMER_FLAGS_UNITS_MASK) */
#define OKL4_TIMER_FLAGS_UNITS_MASK ((okl4_timer_flags_t)1U << 3) /* Deprecated */
/** Select time in UNITS of raw ticks */
/*lint -esym(621, OKL4_MASK_UNITS_TIMER_FLAGS) */
#define OKL4_MASK_UNITS_TIMER_FLAGS ((okl4_timer_flags_t)1U << 3)
/*lint -esym(621, OKL4_SHIFT_UNITS_TIMER_FLAGS) */
#define OKL4_SHIFT_UNITS_TIMER_FLAGS (3)
/*lint -esym(621, OKL4_WIDTH_UNITS_TIMER_FLAGS) */
#define OKL4_WIDTH_UNITS_TIMER_FLAGS (1)
/*lint -esym(621, OKL4_TIMER_FLAGS_ALIGN_MASK) */
#define OKL4_TIMER_FLAGS_ALIGN_MASK ((okl4_timer_flags_t)1U << 4) /* Deprecated */
/** Align first timeout of a periodic timer to a multiple of the timeout length */
/*lint -esym(621, OKL4_MASK_ALIGN_TIMER_FLAGS) */
#define OKL4_MASK_ALIGN_TIMER_FLAGS ((okl4_timer_flags_t)1U << 4)
/*lint -esym(621, OKL4_SHIFT_ALIGN_TIMER_FLAGS) */
#define OKL4_SHIFT_ALIGN_TIMER_FLAGS (4)
/*lint -esym(621, OKL4_WIDTH_ALIGN_TIMER_FLAGS) */
#define OKL4_WIDTH_ALIGN_TIMER_FLAGS (1)
/*lint -esym(621, OKL4_TIMER_FLAGS_WATCHDOG_MASK) */
#define OKL4_TIMER_FLAGS_WATCHDOG_MASK ((okl4_timer_flags_t)1U << 5) /* Deprecated */
/** Enter the kernel interactive debugger on timer expiry (no effect for production builds of the kernel) */
/*lint -esym(621, OKL4_MASK_WATCHDOG_TIMER_FLAGS) */
#define OKL4_MASK_WATCHDOG_TIMER_FLAGS ((okl4_timer_flags_t)1U << 5)
/*lint -esym(621, OKL4_SHIFT_WATCHDOG_TIMER_FLAGS) */
#define OKL4_SHIFT_WATCHDOG_TIMER_FLAGS (5)
/*lint -esym(621, OKL4_WIDTH_WATCHDOG_TIMER_FLAGS) */
#define OKL4_WIDTH_WATCHDOG_TIMER_FLAGS (1)
/*lint -esym(621, OKL4_TIMER_FLAGS_RELOAD_MASK) */
#define OKL4_TIMER_FLAGS_RELOAD_MASK ((okl4_timer_flags_t)1U << 30) /* Deprecated */
/*lint -esym(621, OKL4_MASK_RELOAD_TIMER_FLAGS) */
#define OKL4_MASK_RELOAD_TIMER_FLAGS ((okl4_timer_flags_t)1U << 30)
/*lint -esym(621, OKL4_SHIFT_RELOAD_TIMER_FLAGS) */
#define OKL4_SHIFT_RELOAD_TIMER_FLAGS (30)
/*lint -esym(621, OKL4_WIDTH_RELOAD_TIMER_FLAGS) */
#define OKL4_WIDTH_RELOAD_TIMER_FLAGS (1)
/*lint -esym(621, OKL4_TIMER_FLAGS_TIMESLICE_MASK) */
#define OKL4_TIMER_FLAGS_TIMESLICE_MASK ((okl4_timer_flags_t)1U << 31) /* Deprecated */
/*lint -esym(621, OKL4_MASK_TIMESLICE_TIMER_FLAGS) */
#define OKL4_MASK_TIMESLICE_TIMER_FLAGS ((okl4_timer_flags_t)1U << 31)
/*lint -esym(621, OKL4_SHIFT_TIMESLICE_TIMER_FLAGS) */
#define OKL4_SHIFT_TIMESLICE_TIMER_FLAGS (31)
/*lint -esym(621, OKL4_WIDTH_TIMESLICE_TIMER_FLAGS) */
#define OKL4_WIDTH_TIMESLICE_TIMER_FLAGS (1)


/*lint -sem(okl4_timer_flags_getactive, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_timer_flags_getactive) */
/*lint -esym(714, okl4_timer_flags_getactive) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_timer_flags_getactive(const okl4_timer_flags_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            _Bool field : 1;
        } bits;
        okl4_timer_flags_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_timer_flags_setactive, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_timer_flags_setactive) */

/*lint -esym(621, okl4_timer_flags_setactive) */
OKL4_FORCE_INLINE void
okl4_timer_flags_setactive(okl4_timer_flags_t *x, okl4_bool_t _active)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            _Bool field : 1;
        } bits;
        okl4_timer_flags_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_active;
    *x = _conv.raw;
}
/*lint -sem(okl4_timer_flags_getperiodic, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_timer_flags_getperiodic) */
/*lint -esym(714, okl4_timer_flags_getperiodic) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_timer_flags_getperiodic(const okl4_timer_flags_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 1;
            _Bool field : 1;
        } bits;
        okl4_timer_flags_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_timer_flags_setperiodic, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_timer_flags_setperiodic) */

/*lint -esym(621, okl4_timer_flags_setperiodic) */
OKL4_FORCE_INLINE void
okl4_timer_flags_setperiodic(okl4_timer_flags_t *x, okl4_bool_t _periodic)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 1;
            _Bool field : 1;
        } bits;
        okl4_timer_flags_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_periodic;
    *x = _conv.raw;
}
/*lint -sem(okl4_timer_flags_getabsolute, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_timer_flags_getabsolute) */
/*lint -esym(714, okl4_timer_flags_getabsolute) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_timer_flags_getabsolute(const okl4_timer_flags_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 2;
            _Bool field : 1;
        } bits;
        okl4_timer_flags_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_timer_flags_setabsolute, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_timer_flags_setabsolute) */

/*lint -esym(621, okl4_timer_flags_setabsolute) */
OKL4_FORCE_INLINE void
okl4_timer_flags_setabsolute(okl4_timer_flags_t *x, okl4_bool_t _absolute)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 2;
            _Bool field : 1;
        } bits;
        okl4_timer_flags_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_absolute;
    *x = _conv.raw;
}
/*lint -sem(okl4_timer_flags_getunits, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_timer_flags_getunits) */
/*lint -esym(714, okl4_timer_flags_getunits) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_timer_flags_getunits(const okl4_timer_flags_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 3;
            _Bool field : 1;
        } bits;
        okl4_timer_flags_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_timer_flags_setunits, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_timer_flags_setunits) */

/*lint -esym(621, okl4_timer_flags_setunits) */
OKL4_FORCE_INLINE void
okl4_timer_flags_setunits(okl4_timer_flags_t *x, okl4_bool_t _units)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 3;
            _Bool field : 1;
        } bits;
        okl4_timer_flags_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_units;
    *x = _conv.raw;
}
/*lint -sem(okl4_timer_flags_getalign, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_timer_flags_getalign) */
/*lint -esym(714, okl4_timer_flags_getalign) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_timer_flags_getalign(const okl4_timer_flags_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 4;
            _Bool field : 1;
        } bits;
        okl4_timer_flags_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_timer_flags_setalign, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_timer_flags_setalign) */

/*lint -esym(621, okl4_timer_flags_setalign) */
OKL4_FORCE_INLINE void
okl4_timer_flags_setalign(okl4_timer_flags_t *x, okl4_bool_t _align)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 4;
            _Bool field : 1;
        } bits;
        okl4_timer_flags_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_align;
    *x = _conv.raw;
}
/*lint -sem(okl4_timer_flags_getwatchdog, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_timer_flags_getwatchdog) */
/*lint -esym(714, okl4_timer_flags_getwatchdog) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_timer_flags_getwatchdog(const okl4_timer_flags_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 5;
            _Bool field : 1;
        } bits;
        okl4_timer_flags_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_timer_flags_setwatchdog, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_timer_flags_setwatchdog) */

/*lint -esym(621, okl4_timer_flags_setwatchdog) */
OKL4_FORCE_INLINE void
okl4_timer_flags_setwatchdog(okl4_timer_flags_t *x, okl4_bool_t _watchdog)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 5;
            _Bool field : 1;
        } bits;
        okl4_timer_flags_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_watchdog;
    *x = _conv.raw;
}
/*lint -sem(okl4_timer_flags_getreload, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_timer_flags_getreload) */
/*lint -esym(714, okl4_timer_flags_getreload) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_timer_flags_getreload(const okl4_timer_flags_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 30;
            _Bool field : 1;
        } bits;
        okl4_timer_flags_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_timer_flags_setreload, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_timer_flags_setreload) */

/*lint -esym(621, okl4_timer_flags_setreload) */
OKL4_FORCE_INLINE void
okl4_timer_flags_setreload(okl4_timer_flags_t *x, okl4_bool_t _reload)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 30;
            _Bool field : 1;
        } bits;
        okl4_timer_flags_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_reload;
    *x = _conv.raw;
}
/*lint -sem(okl4_timer_flags_gettimeslice, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, okl4_timer_flags_gettimeslice) */
/*lint -esym(714, okl4_timer_flags_gettimeslice) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_timer_flags_gettimeslice(const okl4_timer_flags_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 31;
            _Bool field : 1;
        } bits;
        okl4_timer_flags_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(okl4_timer_flags_settimeslice, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, okl4_timer_flags_settimeslice) */

/*lint -esym(621, okl4_timer_flags_settimeslice) */
OKL4_FORCE_INLINE void
okl4_timer_flags_settimeslice(okl4_timer_flags_t *x, okl4_bool_t _timeslice)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 31;
            _Bool field : 1;
        } bits;
        okl4_timer_flags_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_timeslice;
    *x = _conv.raw;
}
/*lint -esym(714, okl4_timer_flags_init) */
OKL4_FORCE_INLINE void
okl4_timer_flags_init(okl4_timer_flags_t *x)
{
    *x = (okl4_timer_flags_t)0U;
}

/*lint -esym(714, okl4_timer_flags_cast) */
OKL4_FORCE_INLINE okl4_timer_flags_t
okl4_timer_flags_cast(uint32_t p, okl4_bool_t force)
{
    okl4_timer_flags_t x = (okl4_timer_flags_t)p;
    (void)force;
    return x;
}




struct _okl4_tracebuffer_buffer_header {
    okl4_soc_time_t timestamp;
    okl4_count_t wrap;
    _okl4_padding_t __padding0_4; /**< Padding 8 */
    _okl4_padding_t __padding1_5; /**< Padding 8 */
    _okl4_padding_t __padding2_6; /**< Padding 8 */
    _okl4_padding_t __padding3_7; /**< Padding 8 */
    okl4_ksize_t size;
    okl4_ksize_t head;
    okl4_ksize_t offset;
};






/**

*/

struct okl4_tracebuffer_env {
    struct okl4_virtmem_item virt;
    okl4_interrupt_number_t virq;
    _okl4_padding_t __padding0_4;
    _okl4_padding_t __padding1_5;
    _okl4_padding_t __padding2_6;
    _okl4_padding_t __padding3_7;
};





struct _okl4_tracebuffer_header {
    uint32_t magic;
    uint32_t version;
    uint32_t id;
    okl4_count_t num_buffers;
    okl4_ksize_t buffer_size;
    okl4_atomic_uint32_t log_mask;
    okl4_atomic_uint32_t active_buffer;
    okl4_atomic_uint32_t grabbed_buffer;
    okl4_atomic_uint32_t empty_buffers;
    struct _okl4_tracebuffer_buffer_header buffers[]; /*lint --e{9038} flex array */
};







typedef uint32_t okl4_tracepoint_class_t;

/*lint -esym(621, OKL4_TRACEPOINT_CLASS_THREAD_STATE) */
#define OKL4_TRACEPOINT_CLASS_THREAD_STATE ((okl4_tracepoint_class_t)0x0U)
/*lint -esym(621, OKL4_TRACEPOINT_CLASS_SYSCALLS) */
#define OKL4_TRACEPOINT_CLASS_SYSCALLS ((okl4_tracepoint_class_t)0x1U)
/*lint -esym(621, OKL4_TRACEPOINT_CLASS_PRIMARY) */
#define OKL4_TRACEPOINT_CLASS_PRIMARY ((okl4_tracepoint_class_t)0x2U)
/*lint -esym(621, OKL4_TRACEPOINT_CLASS_SECONDARY) */
#define OKL4_TRACEPOINT_CLASS_SECONDARY ((okl4_tracepoint_class_t)0x3U)
/*lint -esym(621, OKL4_TRACEPOINT_CLASS_TERTIARY) */
#define OKL4_TRACEPOINT_CLASS_TERTIARY ((okl4_tracepoint_class_t)0x4U)
/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_TRACEPOINT_CLASS_MAX) */
#define OKL4_TRACEPOINT_CLASS_MAX ((okl4_tracepoint_class_t)0x4U)
/**
    Invalid enumeration value
*/
/*lint -esym(621, OKL4_TRACEPOINT_CLASS_INVALID) */
#define OKL4_TRACEPOINT_CLASS_INVALID ((okl4_tracepoint_class_t)0xffffffffU)

/*lint -esym(714, okl4_tracepoint_class_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_tracepoint_class_is_element_of(okl4_tracepoint_class_t var);


/*lint -esym(714, okl4_tracepoint_class_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_tracepoint_class_is_element_of(okl4_tracepoint_class_t var)
{
    /*lint --e{944} Disable dead expression detection */
    /*lint --e{948} --e{845} Disable constant always zero */
    return ((var == OKL4_TRACEPOINT_CLASS_THREAD_STATE) ||
            (var == OKL4_TRACEPOINT_CLASS_SYSCALLS) ||
            (var == OKL4_TRACEPOINT_CLASS_PRIMARY) ||
            (var == OKL4_TRACEPOINT_CLASS_SECONDARY) ||
            (var == OKL4_TRACEPOINT_CLASS_TERTIARY));
}


/**
    - BITS 7..0 -   @ref _OKL4_MASK_ID_TRACEPOINT_DESC
    - BIT 8 -   @ref _OKL4_MASK_USER_TRACEPOINT_DESC
    - BIT 9 -   @ref _OKL4_MASK_BIN_TRACEPOINT_DESC
    - BITS 15..10 -   @ref _OKL4_MASK_RECLEN_TRACEPOINT_DESC
    - BITS 21..16 -   @ref _OKL4_MASK_CPUID_TRACEPOINT_DESC
    - BITS 27..22 -   @ref _OKL4_MASK_THREADID_TRACEPOINT_DESC
    - BITS 31..28 -   @ref _OKL4_MASK__R1_TRACEPOINT_DESC
*/

/*lint -esym(621, _okl4_tracepoint_desc_t) */
typedef uint32_t _okl4_tracepoint_desc_t;

/*lint -esym(621, _okl4_tracepoint_desc_getid) */
/*lint -esym(714, _okl4_tracepoint_desc_getid) */
OKL4_FORCE_INLINE uint32_t
_okl4_tracepoint_desc_getid(const _okl4_tracepoint_desc_t *x);

/*lint -esym(621, _okl4_tracepoint_desc_setid) */
OKL4_FORCE_INLINE void
_okl4_tracepoint_desc_setid(_okl4_tracepoint_desc_t *x, uint32_t _id);

/*lint -esym(621, _okl4_tracepoint_desc_getuser) */
/*lint -esym(714, _okl4_tracepoint_desc_getuser) */
OKL4_FORCE_INLINE okl4_bool_t
_okl4_tracepoint_desc_getuser(const _okl4_tracepoint_desc_t *x);

/*lint -esym(621, _okl4_tracepoint_desc_setuser) */
OKL4_FORCE_INLINE void
_okl4_tracepoint_desc_setuser(_okl4_tracepoint_desc_t *x, okl4_bool_t _user);

/*lint -esym(621, _okl4_tracepoint_desc_getbin) */
/*lint -esym(714, _okl4_tracepoint_desc_getbin) */
OKL4_FORCE_INLINE okl4_bool_t
_okl4_tracepoint_desc_getbin(const _okl4_tracepoint_desc_t *x);

/*lint -esym(621, _okl4_tracepoint_desc_setbin) */
OKL4_FORCE_INLINE void
_okl4_tracepoint_desc_setbin(_okl4_tracepoint_desc_t *x, okl4_bool_t _bin);

/*lint -esym(621, _okl4_tracepoint_desc_getreclen) */
/*lint -esym(714, _okl4_tracepoint_desc_getreclen) */
OKL4_FORCE_INLINE uint32_t
_okl4_tracepoint_desc_getreclen(const _okl4_tracepoint_desc_t *x);

/*lint -esym(621, _okl4_tracepoint_desc_setreclen) */
OKL4_FORCE_INLINE void
_okl4_tracepoint_desc_setreclen(_okl4_tracepoint_desc_t *x, uint32_t _reclen);

/*lint -esym(621, _okl4_tracepoint_desc_getcpuid) */
/*lint -esym(714, _okl4_tracepoint_desc_getcpuid) */
OKL4_FORCE_INLINE okl4_count_t
_okl4_tracepoint_desc_getcpuid(const _okl4_tracepoint_desc_t *x);

/*lint -esym(621, _okl4_tracepoint_desc_setcpuid) */
OKL4_FORCE_INLINE void
_okl4_tracepoint_desc_setcpuid(_okl4_tracepoint_desc_t *x, okl4_count_t _cpuid);

/*lint -esym(621, _okl4_tracepoint_desc_getthreadid) */
/*lint -esym(714, _okl4_tracepoint_desc_getthreadid) */
OKL4_FORCE_INLINE uint32_t
_okl4_tracepoint_desc_getthreadid(const _okl4_tracepoint_desc_t *x);

/*lint -esym(621, _okl4_tracepoint_desc_setthreadid) */
OKL4_FORCE_INLINE void
_okl4_tracepoint_desc_setthreadid(_okl4_tracepoint_desc_t *x, uint32_t _threadid);

/*lint -esym(621, _okl4_tracepoint_desc_getr1) */
/*lint -esym(714, _okl4_tracepoint_desc_getr1) */
OKL4_FORCE_INLINE uint32_t
_okl4_tracepoint_desc_getr1(const _okl4_tracepoint_desc_t *x);

/*lint -esym(621, _okl4_tracepoint_desc_setr1) */
OKL4_FORCE_INLINE void
_okl4_tracepoint_desc_setr1(_okl4_tracepoint_desc_t *x, uint32_t __r1);

/*lint -esym(714, _okl4_tracepoint_desc_init) */
OKL4_FORCE_INLINE void
_okl4_tracepoint_desc_init(_okl4_tracepoint_desc_t *x);

/*lint -esym(714, _okl4_tracepoint_desc_cast) */
OKL4_FORCE_INLINE _okl4_tracepoint_desc_t
_okl4_tracepoint_desc_cast(uint32_t p, okl4_bool_t force);




/*lint -esym(621, _OKL4_TRACEPOINT_DESC_ID_MASK) */
#define _OKL4_TRACEPOINT_DESC_ID_MASK ((_okl4_tracepoint_desc_t)255U) /* Deprecated */
/*lint -esym(621, _OKL4_MASK_ID_TRACEPOINT_DESC) */
#define _OKL4_MASK_ID_TRACEPOINT_DESC ((_okl4_tracepoint_desc_t)255U)
/*lint -esym(621, _OKL4_SHIFT_ID_TRACEPOINT_DESC) */
#define _OKL4_SHIFT_ID_TRACEPOINT_DESC (0)
/*lint -esym(621, _OKL4_WIDTH_ID_TRACEPOINT_DESC) */
#define _OKL4_WIDTH_ID_TRACEPOINT_DESC (8)
/*lint -esym(621, _OKL4_TRACEPOINT_DESC_USER_MASK) */
#define _OKL4_TRACEPOINT_DESC_USER_MASK ((_okl4_tracepoint_desc_t)1U << 8) /* Deprecated */
/*lint -esym(621, _OKL4_MASK_USER_TRACEPOINT_DESC) */
#define _OKL4_MASK_USER_TRACEPOINT_DESC ((_okl4_tracepoint_desc_t)1U << 8)
/*lint -esym(621, _OKL4_SHIFT_USER_TRACEPOINT_DESC) */
#define _OKL4_SHIFT_USER_TRACEPOINT_DESC (8)
/*lint -esym(621, _OKL4_WIDTH_USER_TRACEPOINT_DESC) */
#define _OKL4_WIDTH_USER_TRACEPOINT_DESC (1)
/*lint -esym(621, _OKL4_TRACEPOINT_DESC_BIN_MASK) */
#define _OKL4_TRACEPOINT_DESC_BIN_MASK ((_okl4_tracepoint_desc_t)1U << 9) /* Deprecated */
/*lint -esym(621, _OKL4_MASK_BIN_TRACEPOINT_DESC) */
#define _OKL4_MASK_BIN_TRACEPOINT_DESC ((_okl4_tracepoint_desc_t)1U << 9)
/*lint -esym(621, _OKL4_SHIFT_BIN_TRACEPOINT_DESC) */
#define _OKL4_SHIFT_BIN_TRACEPOINT_DESC (9)
/*lint -esym(621, _OKL4_WIDTH_BIN_TRACEPOINT_DESC) */
#define _OKL4_WIDTH_BIN_TRACEPOINT_DESC (1)
/*lint -esym(621, _OKL4_TRACEPOINT_DESC_RECLEN_MASK) */
#define _OKL4_TRACEPOINT_DESC_RECLEN_MASK ((_okl4_tracepoint_desc_t)63U << 10) /* Deprecated */
/*lint -esym(621, _OKL4_MASK_RECLEN_TRACEPOINT_DESC) */
#define _OKL4_MASK_RECLEN_TRACEPOINT_DESC ((_okl4_tracepoint_desc_t)63U << 10)
/*lint -esym(621, _OKL4_SHIFT_RECLEN_TRACEPOINT_DESC) */
#define _OKL4_SHIFT_RECLEN_TRACEPOINT_DESC (10)
/*lint -esym(621, _OKL4_WIDTH_RECLEN_TRACEPOINT_DESC) */
#define _OKL4_WIDTH_RECLEN_TRACEPOINT_DESC (6)
/*lint -esym(621, _OKL4_TRACEPOINT_DESC_CPUID_MASK) */
#define _OKL4_TRACEPOINT_DESC_CPUID_MASK ((_okl4_tracepoint_desc_t)63U << 16) /* Deprecated */
/*lint -esym(621, _OKL4_MASK_CPUID_TRACEPOINT_DESC) */
#define _OKL4_MASK_CPUID_TRACEPOINT_DESC ((_okl4_tracepoint_desc_t)63U << 16)
/*lint -esym(621, _OKL4_SHIFT_CPUID_TRACEPOINT_DESC) */
#define _OKL4_SHIFT_CPUID_TRACEPOINT_DESC (16)
/*lint -esym(621, _OKL4_WIDTH_CPUID_TRACEPOINT_DESC) */
#define _OKL4_WIDTH_CPUID_TRACEPOINT_DESC (6)
/*lint -esym(621, _OKL4_TRACEPOINT_DESC_THREADID_MASK) */
#define _OKL4_TRACEPOINT_DESC_THREADID_MASK ((_okl4_tracepoint_desc_t)63U << 22) /* Deprecated */
/*lint -esym(621, _OKL4_MASK_THREADID_TRACEPOINT_DESC) */
#define _OKL4_MASK_THREADID_TRACEPOINT_DESC ((_okl4_tracepoint_desc_t)63U << 22)
/*lint -esym(621, _OKL4_SHIFT_THREADID_TRACEPOINT_DESC) */
#define _OKL4_SHIFT_THREADID_TRACEPOINT_DESC (22)
/*lint -esym(621, _OKL4_WIDTH_THREADID_TRACEPOINT_DESC) */
#define _OKL4_WIDTH_THREADID_TRACEPOINT_DESC (6)
/*lint -esym(621, _OKL4_TRACEPOINT_DESC__R1_MASK) */
#define _OKL4_TRACEPOINT_DESC__R1_MASK ((_okl4_tracepoint_desc_t)15U << 28) /* Deprecated */
/*lint -esym(621, _OKL4_MASK__R1_TRACEPOINT_DESC) */
#define _OKL4_MASK__R1_TRACEPOINT_DESC ((_okl4_tracepoint_desc_t)15U << 28)
/*lint -esym(621, _OKL4_SHIFT__R1_TRACEPOINT_DESC) */
#define _OKL4_SHIFT__R1_TRACEPOINT_DESC (28)
/*lint -esym(621, _OKL4_WIDTH__R1_TRACEPOINT_DESC) */
#define _OKL4_WIDTH__R1_TRACEPOINT_DESC (4)


/*lint -sem(_okl4_tracepoint_desc_getid, 1p, @n >= 0 && @n <= 255) */
/*lint -esym(621, _okl4_tracepoint_desc_getid) */
/*lint -esym(714, _okl4_tracepoint_desc_getid) */
OKL4_FORCE_INLINE uint32_t
_okl4_tracepoint_desc_getid(const _okl4_tracepoint_desc_t *x)
{
    uint32_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t field : 8;
        } bits;
        _okl4_tracepoint_desc_t raw;
    } _conv;

    _conv.raw = *x;
    field = (uint32_t)_conv.bits.field;
    return field;
}

/*lint -sem(_okl4_tracepoint_desc_setid, 2n >= 0 && 2n <= 255) */
/*lint -esym(714, _okl4_tracepoint_desc_setid) */

/*lint -esym(621, _okl4_tracepoint_desc_setid) */
OKL4_FORCE_INLINE void
_okl4_tracepoint_desc_setid(_okl4_tracepoint_desc_t *x, uint32_t _id)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t field : 8;
        } bits;
        _okl4_tracepoint_desc_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_id;
    *x = _conv.raw;
}
/*lint -sem(_okl4_tracepoint_desc_getuser, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, _okl4_tracepoint_desc_getuser) */
/*lint -esym(714, _okl4_tracepoint_desc_getuser) */
OKL4_FORCE_INLINE okl4_bool_t
_okl4_tracepoint_desc_getuser(const _okl4_tracepoint_desc_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 8;
            _Bool field : 1;
        } bits;
        _okl4_tracepoint_desc_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(_okl4_tracepoint_desc_setuser, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, _okl4_tracepoint_desc_setuser) */

/*lint -esym(621, _okl4_tracepoint_desc_setuser) */
OKL4_FORCE_INLINE void
_okl4_tracepoint_desc_setuser(_okl4_tracepoint_desc_t *x, okl4_bool_t _user)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 8;
            _Bool field : 1;
        } bits;
        _okl4_tracepoint_desc_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_user;
    *x = _conv.raw;
}
/*lint -sem(_okl4_tracepoint_desc_getbin, 1p, @n >= 0 && @n <= 1) */
/*lint -esym(621, _okl4_tracepoint_desc_getbin) */
/*lint -esym(714, _okl4_tracepoint_desc_getbin) */
OKL4_FORCE_INLINE okl4_bool_t
_okl4_tracepoint_desc_getbin(const _okl4_tracepoint_desc_t *x)
{
    okl4_bool_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 9;
            _Bool field : 1;
        } bits;
        _okl4_tracepoint_desc_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_bool_t)_conv.bits.field;
    return field;
}

/*lint -sem(_okl4_tracepoint_desc_setbin, 2n >= 0 && 2n <= 1) */
/*lint -esym(714, _okl4_tracepoint_desc_setbin) */

/*lint -esym(621, _okl4_tracepoint_desc_setbin) */
OKL4_FORCE_INLINE void
_okl4_tracepoint_desc_setbin(_okl4_tracepoint_desc_t *x, okl4_bool_t _bin)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 9;
            _Bool field : 1;
        } bits;
        _okl4_tracepoint_desc_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (_Bool)_bin;
    *x = _conv.raw;
}
/*lint -sem(_okl4_tracepoint_desc_getreclen, 1p, @n >= 0 && @n <= 63) */
/*lint -esym(621, _okl4_tracepoint_desc_getreclen) */
/*lint -esym(714, _okl4_tracepoint_desc_getreclen) */
OKL4_FORCE_INLINE uint32_t
_okl4_tracepoint_desc_getreclen(const _okl4_tracepoint_desc_t *x)
{
    uint32_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 10;
            uint32_t field : 6;
        } bits;
        _okl4_tracepoint_desc_t raw;
    } _conv;

    _conv.raw = *x;
    field = (uint32_t)_conv.bits.field;
    return field;
}

/*lint -sem(_okl4_tracepoint_desc_setreclen, 2n >= 0 && 2n <= 63) */
/*lint -esym(714, _okl4_tracepoint_desc_setreclen) */

/*lint -esym(621, _okl4_tracepoint_desc_setreclen) */
OKL4_FORCE_INLINE void
_okl4_tracepoint_desc_setreclen(_okl4_tracepoint_desc_t *x, uint32_t _reclen)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 10;
            uint32_t field : 6;
        } bits;
        _okl4_tracepoint_desc_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_reclen;
    *x = _conv.raw;
}
/*lint -sem(_okl4_tracepoint_desc_getcpuid, 1p, @n >= 0 && @n <= 63) */
/*lint -esym(621, _okl4_tracepoint_desc_getcpuid) */
/*lint -esym(714, _okl4_tracepoint_desc_getcpuid) */
OKL4_FORCE_INLINE okl4_count_t
_okl4_tracepoint_desc_getcpuid(const _okl4_tracepoint_desc_t *x)
{
    okl4_count_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 16;
            uint32_t field : 6;
        } bits;
        _okl4_tracepoint_desc_t raw;
    } _conv;

    _conv.raw = *x;
    field = (okl4_count_t)_conv.bits.field;
    return field;
}

/*lint -sem(_okl4_tracepoint_desc_setcpuid, 2n >= 0 && 2n <= 63) */
/*lint -esym(714, _okl4_tracepoint_desc_setcpuid) */

/*lint -esym(621, _okl4_tracepoint_desc_setcpuid) */
OKL4_FORCE_INLINE void
_okl4_tracepoint_desc_setcpuid(_okl4_tracepoint_desc_t *x, okl4_count_t _cpuid)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 16;
            uint32_t field : 6;
        } bits;
        _okl4_tracepoint_desc_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_cpuid;
    *x = _conv.raw;
}
/*lint -sem(_okl4_tracepoint_desc_getthreadid, 1p, @n >= 0 && @n <= 63) */
/*lint -esym(621, _okl4_tracepoint_desc_getthreadid) */
/*lint -esym(714, _okl4_tracepoint_desc_getthreadid) */
OKL4_FORCE_INLINE uint32_t
_okl4_tracepoint_desc_getthreadid(const _okl4_tracepoint_desc_t *x)
{
    uint32_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 22;
            uint32_t field : 6;
        } bits;
        _okl4_tracepoint_desc_t raw;
    } _conv;

    _conv.raw = *x;
    field = (uint32_t)_conv.bits.field;
    return field;
}

/*lint -sem(_okl4_tracepoint_desc_setthreadid, 2n >= 0 && 2n <= 63) */
/*lint -esym(714, _okl4_tracepoint_desc_setthreadid) */

/*lint -esym(621, _okl4_tracepoint_desc_setthreadid) */
OKL4_FORCE_INLINE void
_okl4_tracepoint_desc_setthreadid(_okl4_tracepoint_desc_t *x, uint32_t _threadid)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 22;
            uint32_t field : 6;
        } bits;
        _okl4_tracepoint_desc_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_threadid;
    *x = _conv.raw;
}
/*lint -sem(_okl4_tracepoint_desc_getr1, 1p, @n >= 0 && @n <= 15) */
/*lint -esym(621, _okl4_tracepoint_desc_getr1) */
/*lint -esym(714, _okl4_tracepoint_desc_getr1) */
OKL4_FORCE_INLINE uint32_t
_okl4_tracepoint_desc_getr1(const _okl4_tracepoint_desc_t *x)
{
    uint32_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 28;
            uint32_t field : 4;
        } bits;
        _okl4_tracepoint_desc_t raw;
    } _conv;

    _conv.raw = *x;
    field = (uint32_t)_conv.bits.field;
    return field;
}

/*lint -sem(_okl4_tracepoint_desc_setr1, 2n >= 0 && 2n <= 15) */
/*lint -esym(714, _okl4_tracepoint_desc_setr1) */

/*lint -esym(621, _okl4_tracepoint_desc_setr1) */
OKL4_FORCE_INLINE void
_okl4_tracepoint_desc_setr1(_okl4_tracepoint_desc_t *x, uint32_t __r1)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 28;
            uint32_t field : 4;
        } bits;
        _okl4_tracepoint_desc_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)__r1;
    *x = _conv.raw;
}
/*lint -esym(714, _okl4_tracepoint_desc_init) */
OKL4_FORCE_INLINE void
_okl4_tracepoint_desc_init(_okl4_tracepoint_desc_t *x)
{
    *x = (_okl4_tracepoint_desc_t)0U;
}

/*lint -esym(714, _okl4_tracepoint_desc_cast) */
OKL4_FORCE_INLINE _okl4_tracepoint_desc_t
_okl4_tracepoint_desc_cast(uint32_t p, okl4_bool_t force)
{
    _okl4_tracepoint_desc_t x = (_okl4_tracepoint_desc_t)p;
    (void)force;
    return x;
}



/**
    - BITS 15..0 -   @ref _OKL4_MASK_CLASS_TRACEPOINT_MASKS
    - BITS 31..16 -   @ref _OKL4_MASK_SUBSYSTEM_TRACEPOINT_MASKS
*/

/*lint -esym(621, _okl4_tracepoint_masks_t) */
typedef uint32_t _okl4_tracepoint_masks_t;

/*lint -esym(621, _okl4_tracepoint_masks_getclass) */
/*lint -esym(714, _okl4_tracepoint_masks_getclass) */
OKL4_FORCE_INLINE uint32_t
_okl4_tracepoint_masks_getclass(const _okl4_tracepoint_masks_t *x);

/*lint -esym(621, _okl4_tracepoint_masks_setclass) */
OKL4_FORCE_INLINE void
_okl4_tracepoint_masks_setclass(_okl4_tracepoint_masks_t *x, uint32_t _class);

/*lint -esym(621, _okl4_tracepoint_masks_getsubsystem) */
/*lint -esym(714, _okl4_tracepoint_masks_getsubsystem) */
OKL4_FORCE_INLINE uint32_t
_okl4_tracepoint_masks_getsubsystem(const _okl4_tracepoint_masks_t *x);

/*lint -esym(621, _okl4_tracepoint_masks_setsubsystem) */
OKL4_FORCE_INLINE void
_okl4_tracepoint_masks_setsubsystem(_okl4_tracepoint_masks_t *x, uint32_t _subsystem);

/*lint -esym(714, _okl4_tracepoint_masks_init) */
OKL4_FORCE_INLINE void
_okl4_tracepoint_masks_init(_okl4_tracepoint_masks_t *x);

/*lint -esym(714, _okl4_tracepoint_masks_cast) */
OKL4_FORCE_INLINE _okl4_tracepoint_masks_t
_okl4_tracepoint_masks_cast(uint32_t p, okl4_bool_t force);




/*lint -esym(621, _OKL4_TRACEPOINT_MASKS_CLASS_MASK) */
#define _OKL4_TRACEPOINT_MASKS_CLASS_MASK ((_okl4_tracepoint_masks_t)65535U) /* Deprecated */
/*lint -esym(621, _OKL4_MASK_CLASS_TRACEPOINT_MASKS) */
#define _OKL4_MASK_CLASS_TRACEPOINT_MASKS ((_okl4_tracepoint_masks_t)65535U)
/*lint -esym(621, _OKL4_SHIFT_CLASS_TRACEPOINT_MASKS) */
#define _OKL4_SHIFT_CLASS_TRACEPOINT_MASKS (0)
/*lint -esym(621, _OKL4_WIDTH_CLASS_TRACEPOINT_MASKS) */
#define _OKL4_WIDTH_CLASS_TRACEPOINT_MASKS (16)
/*lint -esym(621, _OKL4_TRACEPOINT_MASKS_SUBSYSTEM_MASK) */
#define _OKL4_TRACEPOINT_MASKS_SUBSYSTEM_MASK ((_okl4_tracepoint_masks_t)65535U << 16) /* Deprecated */
/*lint -esym(621, _OKL4_MASK_SUBSYSTEM_TRACEPOINT_MASKS) */
#define _OKL4_MASK_SUBSYSTEM_TRACEPOINT_MASKS ((_okl4_tracepoint_masks_t)65535U << 16)
/*lint -esym(621, _OKL4_SHIFT_SUBSYSTEM_TRACEPOINT_MASKS) */
#define _OKL4_SHIFT_SUBSYSTEM_TRACEPOINT_MASKS (16)
/*lint -esym(621, _OKL4_WIDTH_SUBSYSTEM_TRACEPOINT_MASKS) */
#define _OKL4_WIDTH_SUBSYSTEM_TRACEPOINT_MASKS (16)


/*lint -sem(_okl4_tracepoint_masks_getclass, 1p, @n >= 0 && @n <= 65535) */
/*lint -esym(621, _okl4_tracepoint_masks_getclass) */
/*lint -esym(714, _okl4_tracepoint_masks_getclass) */
OKL4_FORCE_INLINE uint32_t
_okl4_tracepoint_masks_getclass(const _okl4_tracepoint_masks_t *x)
{
    uint32_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t field : 16;
        } bits;
        _okl4_tracepoint_masks_t raw;
    } _conv;

    _conv.raw = *x;
    field = (uint32_t)_conv.bits.field;
    return field;
}

/*lint -sem(_okl4_tracepoint_masks_setclass, 2n >= 0 && 2n <= 65535) */
/*lint -esym(714, _okl4_tracepoint_masks_setclass) */

/*lint -esym(621, _okl4_tracepoint_masks_setclass) */
OKL4_FORCE_INLINE void
_okl4_tracepoint_masks_setclass(_okl4_tracepoint_masks_t *x, uint32_t _class)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t field : 16;
        } bits;
        _okl4_tracepoint_masks_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_class;
    *x = _conv.raw;
}
/*lint -sem(_okl4_tracepoint_masks_getsubsystem, 1p, @n >= 0 && @n <= 65535) */
/*lint -esym(621, _okl4_tracepoint_masks_getsubsystem) */
/*lint -esym(714, _okl4_tracepoint_masks_getsubsystem) */
OKL4_FORCE_INLINE uint32_t
_okl4_tracepoint_masks_getsubsystem(const _okl4_tracepoint_masks_t *x)
{
    uint32_t field;
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 16;
            uint32_t field : 16;
        } bits;
        _okl4_tracepoint_masks_t raw;
    } _conv;

    _conv.raw = *x;
    field = (uint32_t)_conv.bits.field;
    return field;
}

/*lint -sem(_okl4_tracepoint_masks_setsubsystem, 2n >= 0 && 2n <= 65535) */
/*lint -esym(714, _okl4_tracepoint_masks_setsubsystem) */

/*lint -esym(621, _okl4_tracepoint_masks_setsubsystem) */
OKL4_FORCE_INLINE void
_okl4_tracepoint_masks_setsubsystem(_okl4_tracepoint_masks_t *x, uint32_t _subsystem)
{
    union {
        /*lint -e{806} -e{958} -e{959} */
        struct {
            uint32_t _skip : 16;
            uint32_t field : 16;
        } bits;
        _okl4_tracepoint_masks_t raw;
    } _conv;

    _conv.raw = *x;
    _conv.bits.field = (uint32_t)_subsystem;
    *x = _conv.raw;
}
/*lint -esym(714, _okl4_tracepoint_masks_init) */
OKL4_FORCE_INLINE void
_okl4_tracepoint_masks_init(_okl4_tracepoint_masks_t *x)
{
    *x = (_okl4_tracepoint_masks_t)0U;
}

/*lint -esym(714, _okl4_tracepoint_masks_cast) */
OKL4_FORCE_INLINE _okl4_tracepoint_masks_t
_okl4_tracepoint_masks_cast(uint32_t p, okl4_bool_t force)
{
    _okl4_tracepoint_masks_t x = (_okl4_tracepoint_masks_t)p;
    (void)force;
    return x;
}




struct okl4_tracepoint_entry_base {
    uint32_t time_offset;
    _okl4_tracepoint_masks_t masks;
    _okl4_tracepoint_desc_t description;
};







typedef uint32_t okl4_tracepoint_evt_t;

/*lint -esym(621, OKL4_TRACEPOINT_EVT_SCH_SCHEDULER_FLAG_SET_RUNNABLE) */
#define OKL4_TRACEPOINT_EVT_SCH_SCHEDULER_FLAG_SET_RUNNABLE ((okl4_tracepoint_evt_t)0x0U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SCH_SCHEDULER_FLAG_CLEAR_RUNNABLE) */
#define OKL4_TRACEPOINT_EVT_SCH_SCHEDULER_FLAG_CLEAR_RUNNABLE ((okl4_tracepoint_evt_t)0x1U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SCH_CONTEXT_SWITCH) */
#define OKL4_TRACEPOINT_EVT_SCH_CONTEXT_SWITCH ((okl4_tracepoint_evt_t)0x2U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_KDB_SET_OBJECT_NAME) */
#define OKL4_TRACEPOINT_EVT_KDB_SET_OBJECT_NAME ((okl4_tracepoint_evt_t)0x3U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_AXON_PROCESS_RECV) */
#define OKL4_TRACEPOINT_EVT_SWI_AXON_PROCESS_RECV ((okl4_tracepoint_evt_t)0x4U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_AXON_SET_HALTED) */
#define OKL4_TRACEPOINT_EVT_SWI_AXON_SET_HALTED ((okl4_tracepoint_evt_t)0x5U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_AXON_SET_RECV_AREA) */
#define OKL4_TRACEPOINT_EVT_SWI_AXON_SET_RECV_AREA ((okl4_tracepoint_evt_t)0x6U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_AXON_SET_RECV_QUEUE) */
#define OKL4_TRACEPOINT_EVT_SWI_AXON_SET_RECV_QUEUE ((okl4_tracepoint_evt_t)0x7U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_AXON_SET_RECV_SEGMENT) */
#define OKL4_TRACEPOINT_EVT_SWI_AXON_SET_RECV_SEGMENT ((okl4_tracepoint_evt_t)0x8U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_AXON_SET_SEND_AREA) */
#define OKL4_TRACEPOINT_EVT_SWI_AXON_SET_SEND_AREA ((okl4_tracepoint_evt_t)0x9U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_AXON_SET_SEND_QUEUE) */
#define OKL4_TRACEPOINT_EVT_SWI_AXON_SET_SEND_QUEUE ((okl4_tracepoint_evt_t)0xaU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_AXON_SET_SEND_SEGMENT) */
#define OKL4_TRACEPOINT_EVT_SWI_AXON_SET_SEND_SEGMENT ((okl4_tracepoint_evt_t)0xbU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_AXON_TRIGGER_SEND) */
#define OKL4_TRACEPOINT_EVT_SWI_AXON_TRIGGER_SEND ((okl4_tracepoint_evt_t)0xcU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_ACK) */
#define OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_ACK ((okl4_tracepoint_evt_t)0xdU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_ATTACH_PRIVATE) */
#define OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_ATTACH_PRIVATE ((okl4_tracepoint_evt_t)0xeU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_ATTACH_SHARED) */
#define OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_ATTACH_SHARED ((okl4_tracepoint_evt_t)0xfU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_DETACH) */
#define OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_DETACH ((okl4_tracepoint_evt_t)0x10U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_DIST_ENABLE) */
#define OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_DIST_ENABLE ((okl4_tracepoint_evt_t)0x11U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_EOI) */
#define OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_EOI ((okl4_tracepoint_evt_t)0x12U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_GET_HIGHEST_PRIORITY_PENDING) */
#define OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_GET_HIGHEST_PRIORITY_PENDING ((okl4_tracepoint_evt_t)0x13U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_GET_PAYLOAD) */
#define OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_GET_PAYLOAD ((okl4_tracepoint_evt_t)0x14U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_LIMITS) */
#define OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_LIMITS ((okl4_tracepoint_evt_t)0x15U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_MASK) */
#define OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_MASK ((okl4_tracepoint_evt_t)0x16U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_RAISE) */
#define OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_RAISE ((okl4_tracepoint_evt_t)0x17U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_SET_BINARY_POINT) */
#define OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_SET_BINARY_POINT ((okl4_tracepoint_evt_t)0x18U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_SET_CONFIG) */
#define OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_SET_CONFIG ((okl4_tracepoint_evt_t)0x19U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_SET_CONTROL) */
#define OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_SET_CONTROL ((okl4_tracepoint_evt_t)0x1aU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_SET_PRIORITY) */
#define OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_SET_PRIORITY ((okl4_tracepoint_evt_t)0x1bU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_SET_PRIORITY_MASK) */
#define OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_SET_PRIORITY_MASK ((okl4_tracepoint_evt_t)0x1cU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_SET_TARGETS) */
#define OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_SET_TARGETS ((okl4_tracepoint_evt_t)0x1dU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_UNMASK) */
#define OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_UNMASK ((okl4_tracepoint_evt_t)0x1eU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_KDB_INTERACT) */
#define OKL4_TRACEPOINT_EVT_SWI_KDB_INTERACT ((okl4_tracepoint_evt_t)0x1fU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_KDB_SET_OBJECT_NAME) */
#define OKL4_TRACEPOINT_EVT_SWI_KDB_SET_OBJECT_NAME ((okl4_tracepoint_evt_t)0x20U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_KSP_PROCEDURE_CALL) */
#define OKL4_TRACEPOINT_EVT_SWI_KSP_PROCEDURE_CALL ((okl4_tracepoint_evt_t)0x21U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_MMU_ATTACH_SEGMENT) */
#define OKL4_TRACEPOINT_EVT_SWI_MMU_ATTACH_SEGMENT ((okl4_tracepoint_evt_t)0x22U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_MMU_DETACH_SEGMENT) */
#define OKL4_TRACEPOINT_EVT_SWI_MMU_DETACH_SEGMENT ((okl4_tracepoint_evt_t)0x23U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_MMU_FLUSH_RANGE) */
#define OKL4_TRACEPOINT_EVT_SWI_MMU_FLUSH_RANGE ((okl4_tracepoint_evt_t)0x24U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_MMU_FLUSH_RANGE_PN) */
#define OKL4_TRACEPOINT_EVT_SWI_MMU_FLUSH_RANGE_PN ((okl4_tracepoint_evt_t)0x25U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_MMU_LOOKUP_PAGE) */
#define OKL4_TRACEPOINT_EVT_SWI_MMU_LOOKUP_PAGE ((okl4_tracepoint_evt_t)0x26U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_MMU_LOOKUP_PN) */
#define OKL4_TRACEPOINT_EVT_SWI_MMU_LOOKUP_PN ((okl4_tracepoint_evt_t)0x27U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_MMU_MAP_PAGE) */
#define OKL4_TRACEPOINT_EVT_SWI_MMU_MAP_PAGE ((okl4_tracepoint_evt_t)0x28U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_MMU_MAP_PN) */
#define OKL4_TRACEPOINT_EVT_SWI_MMU_MAP_PN ((okl4_tracepoint_evt_t)0x29U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_MMU_UNMAP_PAGE) */
#define OKL4_TRACEPOINT_EVT_SWI_MMU_UNMAP_PAGE ((okl4_tracepoint_evt_t)0x2aU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_MMU_UNMAP_PN) */
#define OKL4_TRACEPOINT_EVT_SWI_MMU_UNMAP_PN ((okl4_tracepoint_evt_t)0x2bU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_MMU_UPDATE_PAGE_ATTRS) */
#define OKL4_TRACEPOINT_EVT_SWI_MMU_UPDATE_PAGE_ATTRS ((okl4_tracepoint_evt_t)0x2cU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_MMU_UPDATE_PAGE_PERMS) */
#define OKL4_TRACEPOINT_EVT_SWI_MMU_UPDATE_PAGE_PERMS ((okl4_tracepoint_evt_t)0x2dU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_MMU_UPDATE_PN_ATTRS) */
#define OKL4_TRACEPOINT_EVT_SWI_MMU_UPDATE_PN_ATTRS ((okl4_tracepoint_evt_t)0x2eU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_MMU_UPDATE_PN_PERMS) */
#define OKL4_TRACEPOINT_EVT_SWI_MMU_UPDATE_PN_PERMS ((okl4_tracepoint_evt_t)0x2fU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_PERFORMANCE_NULL_SYSCALL) */
#define OKL4_TRACEPOINT_EVT_SWI_PERFORMANCE_NULL_SYSCALL ((okl4_tracepoint_evt_t)0x30U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_PIPE_CONTROL) */
#define OKL4_TRACEPOINT_EVT_SWI_PIPE_CONTROL ((okl4_tracepoint_evt_t)0x31U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_PIPE_RECV) */
#define OKL4_TRACEPOINT_EVT_SWI_PIPE_RECV ((okl4_tracepoint_evt_t)0x32U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_PIPE_SEND) */
#define OKL4_TRACEPOINT_EVT_SWI_PIPE_SEND ((okl4_tracepoint_evt_t)0x33U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_PRIORITY_WAIVE) */
#define OKL4_TRACEPOINT_EVT_SWI_PRIORITY_WAIVE ((okl4_tracepoint_evt_t)0x34U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_REMOTE_GET_REGISTER) */
#define OKL4_TRACEPOINT_EVT_SWI_REMOTE_GET_REGISTER ((okl4_tracepoint_evt_t)0x35U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_REMOTE_GET_REGISTERS) */
#define OKL4_TRACEPOINT_EVT_SWI_REMOTE_GET_REGISTERS ((okl4_tracepoint_evt_t)0x36U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_REMOTE_READ_MEMORY32) */
#define OKL4_TRACEPOINT_EVT_SWI_REMOTE_READ_MEMORY32 ((okl4_tracepoint_evt_t)0x37U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_REMOTE_SET_REGISTER) */
#define OKL4_TRACEPOINT_EVT_SWI_REMOTE_SET_REGISTER ((okl4_tracepoint_evt_t)0x38U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_REMOTE_SET_REGISTERS) */
#define OKL4_TRACEPOINT_EVT_SWI_REMOTE_SET_REGISTERS ((okl4_tracepoint_evt_t)0x39U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_REMOTE_WRITE_MEMORY32) */
#define OKL4_TRACEPOINT_EVT_SWI_REMOTE_WRITE_MEMORY32 ((okl4_tracepoint_evt_t)0x3aU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_SCHEDULE_METRICS_STATUS_SUSPENDED) */
#define OKL4_TRACEPOINT_EVT_SWI_SCHEDULE_METRICS_STATUS_SUSPENDED ((okl4_tracepoint_evt_t)0x3bU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_SCHEDULE_METRICS_WATCH_SUSPENDED) */
#define OKL4_TRACEPOINT_EVT_SWI_SCHEDULE_METRICS_WATCH_SUSPENDED ((okl4_tracepoint_evt_t)0x3cU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_CPU_DISABLE) */
#define OKL4_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_CPU_DISABLE ((okl4_tracepoint_evt_t)0x3dU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_CPU_ENABLE) */
#define OKL4_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_CPU_ENABLE ((okl4_tracepoint_evt_t)0x3eU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_CPU_GET_DATA) */
#define OKL4_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_CPU_GET_DATA ((okl4_tracepoint_evt_t)0x3fU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_VCPU_DISABLE) */
#define OKL4_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_VCPU_DISABLE ((okl4_tracepoint_evt_t)0x40U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_VCPU_ENABLE) */
#define OKL4_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_VCPU_ENABLE ((okl4_tracepoint_evt_t)0x41U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_VCPU_GET_DATA) */
#define OKL4_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_VCPU_GET_DATA ((okl4_tracepoint_evt_t)0x42U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_SCHEDULER_SUSPEND) */
#define OKL4_TRACEPOINT_EVT_SWI_SCHEDULER_SUSPEND ((okl4_tracepoint_evt_t)0x43U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_TIMER_CANCEL) */
#define OKL4_TRACEPOINT_EVT_SWI_TIMER_CANCEL ((okl4_tracepoint_evt_t)0x44U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_TIMER_GET_RESOLUTION) */
#define OKL4_TRACEPOINT_EVT_SWI_TIMER_GET_RESOLUTION ((okl4_tracepoint_evt_t)0x45U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_TIMER_GET_TIME) */
#define OKL4_TRACEPOINT_EVT_SWI_TIMER_GET_TIME ((okl4_tracepoint_evt_t)0x46U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_TIMER_QUERY) */
#define OKL4_TRACEPOINT_EVT_SWI_TIMER_QUERY ((okl4_tracepoint_evt_t)0x47U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_TIMER_START) */
#define OKL4_TRACEPOINT_EVT_SWI_TIMER_START ((okl4_tracepoint_evt_t)0x48U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_TRACEBUFFER_SYNC) */
#define OKL4_TRACEPOINT_EVT_SWI_TRACEBUFFER_SYNC ((okl4_tracepoint_evt_t)0x49U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_VCPU_RESET) */
#define OKL4_TRACEPOINT_EVT_SWI_VCPU_RESET ((okl4_tracepoint_evt_t)0x4aU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_VCPU_START) */
#define OKL4_TRACEPOINT_EVT_SWI_VCPU_START ((okl4_tracepoint_evt_t)0x4bU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_VCPU_STOP) */
#define OKL4_TRACEPOINT_EVT_SWI_VCPU_STOP ((okl4_tracepoint_evt_t)0x4cU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_VCPU_SWITCH_MODE) */
#define OKL4_TRACEPOINT_EVT_SWI_VCPU_SWITCH_MODE ((okl4_tracepoint_evt_t)0x4dU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_VCPU_SYNC_SEV) */
#define OKL4_TRACEPOINT_EVT_SWI_VCPU_SYNC_SEV ((okl4_tracepoint_evt_t)0x4eU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_VCPU_SYNC_WFE) */
#define OKL4_TRACEPOINT_EVT_SWI_VCPU_SYNC_WFE ((okl4_tracepoint_evt_t)0x4fU)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_VINTERRUPT_CLEAR_AND_RAISE) */
#define OKL4_TRACEPOINT_EVT_SWI_VINTERRUPT_CLEAR_AND_RAISE ((okl4_tracepoint_evt_t)0x50U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_VINTERRUPT_MODIFY) */
#define OKL4_TRACEPOINT_EVT_SWI_VINTERRUPT_MODIFY ((okl4_tracepoint_evt_t)0x51U)
/*lint -esym(621, OKL4_TRACEPOINT_EVT_SWI_VINTERRUPT_RAISE) */
#define OKL4_TRACEPOINT_EVT_SWI_VINTERRUPT_RAISE ((okl4_tracepoint_evt_t)0x52U)
/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_TRACEPOINT_EVT_MAX) */
#define OKL4_TRACEPOINT_EVT_MAX ((okl4_tracepoint_evt_t)0x52U)
/**
    Invalid enumeration value
*/
/*lint -esym(621, OKL4_TRACEPOINT_EVT_INVALID) */
#define OKL4_TRACEPOINT_EVT_INVALID ((okl4_tracepoint_evt_t)0xffffffffU)

/*lint -esym(714, okl4_tracepoint_evt_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_tracepoint_evt_is_element_of(okl4_tracepoint_evt_t var);


/*lint -esym(714, okl4_tracepoint_evt_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_tracepoint_evt_is_element_of(okl4_tracepoint_evt_t var)
{
    /*lint --e{944} Disable dead expression detection */
    /*lint --e{948} --e{845} Disable constant always zero */
    return ((var == OKL4_TRACEPOINT_EVT_SCH_SCHEDULER_FLAG_SET_RUNNABLE) ||
            (var == OKL4_TRACEPOINT_EVT_SCH_SCHEDULER_FLAG_CLEAR_RUNNABLE) ||
            (var == OKL4_TRACEPOINT_EVT_SCH_CONTEXT_SWITCH) ||
            (var == OKL4_TRACEPOINT_EVT_KDB_SET_OBJECT_NAME) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_AXON_PROCESS_RECV) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_AXON_SET_HALTED) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_AXON_SET_RECV_AREA) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_AXON_SET_RECV_QUEUE) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_AXON_SET_RECV_SEGMENT) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_AXON_SET_SEND_AREA) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_AXON_SET_SEND_QUEUE) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_AXON_SET_SEND_SEGMENT) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_AXON_TRIGGER_SEND) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_ACK) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_ATTACH_PRIVATE) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_ATTACH_SHARED) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_DETACH) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_DIST_ENABLE) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_EOI) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_GET_HIGHEST_PRIORITY_PENDING) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_GET_PAYLOAD) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_LIMITS) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_MASK) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_RAISE) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_SET_BINARY_POINT) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_SET_CONFIG) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_SET_CONTROL) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_SET_PRIORITY) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_SET_PRIORITY_MASK) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_SET_TARGETS) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_INTERRUPT_UNMASK) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_KDB_INTERACT) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_KDB_SET_OBJECT_NAME) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_KSP_PROCEDURE_CALL) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_MMU_ATTACH_SEGMENT) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_MMU_DETACH_SEGMENT) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_MMU_FLUSH_RANGE) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_MMU_FLUSH_RANGE_PN) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_MMU_LOOKUP_PAGE) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_MMU_LOOKUP_PN) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_MMU_MAP_PAGE) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_MMU_MAP_PN) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_MMU_UNMAP_PAGE) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_MMU_UNMAP_PN) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_MMU_UPDATE_PAGE_ATTRS) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_MMU_UPDATE_PAGE_PERMS) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_MMU_UPDATE_PN_ATTRS) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_MMU_UPDATE_PN_PERMS) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_PERFORMANCE_NULL_SYSCALL) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_PIPE_CONTROL) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_PIPE_RECV) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_PIPE_SEND) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_PRIORITY_WAIVE) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_REMOTE_GET_REGISTER) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_REMOTE_GET_REGISTERS) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_REMOTE_READ_MEMORY32) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_REMOTE_SET_REGISTER) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_REMOTE_SET_REGISTERS) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_REMOTE_WRITE_MEMORY32) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_SCHEDULE_METRICS_STATUS_SUSPENDED) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_SCHEDULE_METRICS_WATCH_SUSPENDED) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_CPU_DISABLE) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_CPU_ENABLE) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_CPU_GET_DATA) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_VCPU_DISABLE) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_VCPU_ENABLE) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_VCPU_GET_DATA) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_SCHEDULER_SUSPEND) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_TIMER_CANCEL) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_TIMER_GET_RESOLUTION) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_TIMER_GET_TIME) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_TIMER_QUERY) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_TIMER_START) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_TRACEBUFFER_SYNC) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_VCPU_RESET) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_VCPU_START) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_VCPU_STOP) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_VCPU_SWITCH_MODE) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_VCPU_SYNC_SEV) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_VCPU_SYNC_WFE) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_VINTERRUPT_CLEAR_AND_RAISE) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_VINTERRUPT_MODIFY) ||
            (var == OKL4_TRACEPOINT_EVT_SWI_VINTERRUPT_RAISE));
}



typedef uint32_t okl4_tracepoint_level_t;

/*lint -esym(621, OKL4_TRACEPOINT_LEVEL_DEBUG) */
#define OKL4_TRACEPOINT_LEVEL_DEBUG ((okl4_tracepoint_level_t)0x0U)
/*lint -esym(621, OKL4_TRACEPOINT_LEVEL_INFO) */
#define OKL4_TRACEPOINT_LEVEL_INFO ((okl4_tracepoint_level_t)0x1U)
/*lint -esym(621, OKL4_TRACEPOINT_LEVEL_WARN) */
#define OKL4_TRACEPOINT_LEVEL_WARN ((okl4_tracepoint_level_t)0x2U)
/*lint -esym(621, OKL4_TRACEPOINT_LEVEL_CRITICAL) */
#define OKL4_TRACEPOINT_LEVEL_CRITICAL ((okl4_tracepoint_level_t)0x3U)
/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_TRACEPOINT_LEVEL_MAX) */
#define OKL4_TRACEPOINT_LEVEL_MAX ((okl4_tracepoint_level_t)0x3U)
/**
    Invalid enumeration value
*/
/*lint -esym(621, OKL4_TRACEPOINT_LEVEL_INVALID) */
#define OKL4_TRACEPOINT_LEVEL_INVALID ((okl4_tracepoint_level_t)0xffffffffU)

/*lint -esym(714, okl4_tracepoint_level_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_tracepoint_level_is_element_of(okl4_tracepoint_level_t var);


/*lint -esym(714, okl4_tracepoint_level_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_tracepoint_level_is_element_of(okl4_tracepoint_level_t var)
{
    /*lint --e{944} Disable dead expression detection */
    /*lint --e{948} --e{845} Disable constant always zero */
    return ((var == OKL4_TRACEPOINT_LEVEL_DEBUG) ||
            (var == OKL4_TRACEPOINT_LEVEL_INFO) ||
            (var == OKL4_TRACEPOINT_LEVEL_WARN) ||
            (var == OKL4_TRACEPOINT_LEVEL_CRITICAL));
}



typedef uint32_t okl4_tracepoint_mask_t;





typedef uint32_t okl4_tracepoint_subsystem_t;

/*lint -esym(621, OKL4_TRACEPOINT_SUBSYSTEM_SCHEDULER) */
#define OKL4_TRACEPOINT_SUBSYSTEM_SCHEDULER ((okl4_tracepoint_subsystem_t)0x0U)
/*lint -esym(621, OKL4_TRACEPOINT_SUBSYSTEM_TRACE) */
#define OKL4_TRACEPOINT_SUBSYSTEM_TRACE ((okl4_tracepoint_subsystem_t)0x1U)
/*lint -esym(621, OKL4_TRACEPOINT_SUBSYSTEM_CORE) */
#define OKL4_TRACEPOINT_SUBSYSTEM_CORE ((okl4_tracepoint_subsystem_t)0x2U)
/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_TRACEPOINT_SUBSYSTEM_MAX) */
#define OKL4_TRACEPOINT_SUBSYSTEM_MAX ((okl4_tracepoint_subsystem_t)0x2U)
/**
    Invalid enumeration value
*/
/*lint -esym(621, OKL4_TRACEPOINT_SUBSYSTEM_INVALID) */
#define OKL4_TRACEPOINT_SUBSYSTEM_INVALID ((okl4_tracepoint_subsystem_t)0xffffffffU)

/*lint -esym(714, okl4_tracepoint_subsystem_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_tracepoint_subsystem_is_element_of(okl4_tracepoint_subsystem_t var);


/*lint -esym(714, okl4_tracepoint_subsystem_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_tracepoint_subsystem_is_element_of(okl4_tracepoint_subsystem_t var)
{
    /*lint --e{944} Disable dead expression detection */
    /*lint --e{948} --e{845} Disable constant always zero */
    return ((var == OKL4_TRACEPOINT_SUBSYSTEM_SCHEDULER) ||
            (var == OKL4_TRACEPOINT_SUBSYSTEM_TRACE) ||
            (var == OKL4_TRACEPOINT_SUBSYSTEM_CORE));
}



struct okl4_tracepoint_unpacked_entry {
    struct okl4_tracepoint_entry_base entry;
    uint32_t data[]; /*lint --e{9038} flex array */
};


















/**

*/

struct okl4_vclient_info {
    struct okl4_axon_ep_data axon_ep;
    __ptr64(void *, opaque);
};




/**

*/

struct okl4_vcpu_entry {
    okl4_kcap_t vcpu;
    okl4_kcap_t ipi;
    okl4_interrupt_number_t irq;
    _okl4_padding_t __padding0_4;
    _okl4_padding_t __padding1_5;
    _okl4_padding_t __padding2_6;
    _okl4_padding_t __padding3_7;
    okl4_register_t stack_pointer;
};





typedef okl4_arm_mpidr_t okl4_vcpu_id_t;




/**

*/

struct okl4_vcpu_table {
    okl4_count_t num_vcpus;
    _okl4_padding_t __padding0_4;
    _okl4_padding_t __padding1_5;
    _okl4_padding_t __padding2_6;
    _okl4_padding_t __padding3_7;
    __ptr64(struct okl4_vcpu_entry *, vcpu);
};




/**
    The okl4_vfp_ctrl_registers object represents the set of control
    registers in the ARM VFP unit.
*/

struct okl4_vfp_ctrl_registers {
    uint32_t fpsr;
    uint32_t fpcr;
};






/**
    The okl4_vfp_registers_t type represents a set of VFP registers on
    the native machine.
*/

typedef struct okl4_vfp_ctrl_registers okl4_vfp_ctrl_registers_t;




/**
    The okl4_vfp_ops_t object represents the set of operations that may be
    performed on the ARM VFP unit.

    - @ref OKL4_VFP_OPS_MAX
    - @ref OKL4_VFP_OPS_INVALID
*/

typedef uint32_t okl4_vfp_ops_t;

/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_VFP_OPS_MAX) */
#define OKL4_VFP_OPS_MAX ((okl4_vfp_ops_t)0x0U)
/**
    Invalid enumeration value
*/
/*lint -esym(621, OKL4_VFP_OPS_INVALID) */
#define OKL4_VFP_OPS_INVALID ((okl4_vfp_ops_t)0xffffffffU)

/*lint -esym(714, okl4_vfp_ops_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_vfp_ops_is_element_of(okl4_vfp_ops_t var);


/*lint -esym(714, okl4_vfp_ops_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_vfp_ops_is_element_of(okl4_vfp_ops_t var)
{
    /*lint --e{944} Disable dead expression detection */
    /*lint --e{948} --e{845} Disable constant always zero */
    return ((okl4_bool_t)0);
}



struct okl4_vfp_register {
    __attribute__((aligned(16))) uint8_t __bytes[16];
};







typedef struct okl4_vfp_register okl4_vfp_register_t;




/**
    The okl4_vfp_registers object represents the set of registers in the
    ARM VFP unit, including the control registers.
*/

struct okl4_vfp_registers {
    okl4_vfp_register_t v0;
    okl4_vfp_register_t v1;
    okl4_vfp_register_t v2;
    okl4_vfp_register_t v3;
    okl4_vfp_register_t v4;
    okl4_vfp_register_t v5;
    okl4_vfp_register_t v6;
    okl4_vfp_register_t v7;
    okl4_vfp_register_t v8;
    okl4_vfp_register_t v9;
    okl4_vfp_register_t v10;
    okl4_vfp_register_t v11;
    okl4_vfp_register_t v12;
    okl4_vfp_register_t v13;
    okl4_vfp_register_t v14;
    okl4_vfp_register_t v15;
    okl4_vfp_register_t v16;
    okl4_vfp_register_t v17;
    okl4_vfp_register_t v18;
    okl4_vfp_register_t v19;
    okl4_vfp_register_t v20;
    okl4_vfp_register_t v21;
    okl4_vfp_register_t v22;
    okl4_vfp_register_t v23;
    okl4_vfp_register_t v24;
    okl4_vfp_register_t v25;
    okl4_vfp_register_t v26;
    okl4_vfp_register_t v27;
    okl4_vfp_register_t v28;
    okl4_vfp_register_t v29;
    okl4_vfp_register_t v30;
    okl4_vfp_register_t v31;
    struct okl4_vfp_ctrl_registers control;
    _okl4_padding_t __padding0_8; /**< Padding 16 */
    _okl4_padding_t __padding1_9; /**< Padding 16 */
    _okl4_padding_t __padding2_10; /**< Padding 16 */
    _okl4_padding_t __padding3_11; /**< Padding 16 */
    _okl4_padding_t __padding4_12; /**< Padding 16 */
    _okl4_padding_t __padding5_13; /**< Padding 16 */
    _okl4_padding_t __padding6_14; /**< Padding 16 */
    _okl4_padding_t __padding7_15; /**< Padding 16 */
};






/**
    The okl4_vfp_registers_t type represents a set of VFP registers on
    the native machine.
*/

typedef struct okl4_vfp_registers okl4_vfp_registers_t;




/**

*/

struct okl4_virtmem_pool {
    struct okl4_virtmem_item pool;
};




/**

*/

struct okl4_virtual_interrupt_lines {
    okl4_count_t num_lines;
    _okl4_padding_t __padding0_4;
    _okl4_padding_t __padding1_5;
    _okl4_padding_t __padding2_6;
    _okl4_padding_t __padding3_7;
    __ptr64(okl4_kcap_t *, lines);
};




/**

*/

struct okl4_vserver_info {
    struct {
        __ptr64(struct okl4_axon_ep_data *, data);
        okl4_count_t max_messages;
        _okl4_padding_t __padding0_4; /**< Padding 8 */
        _okl4_padding_t __padding1_5; /**< Padding 8 */
        _okl4_padding_t __padding2_6; /**< Padding 8 */
        _okl4_padding_t __padding3_7; /**< Padding 8 */
        okl4_ksize_t message_size;
    } channels;

    okl4_count_t num_clients;
    _okl4_padding_t __padding0_4;
    _okl4_padding_t __padding1_5;
    _okl4_padding_t __padding2_6;
    _okl4_padding_t __padding3_7;
};




/**

*/

struct okl4_vservices_service_descriptor {
    __ptr64(okl4_string_t, name);
    __ptr64(okl4_string_t, protocol);
    __ptr64(void *, RESERVED);
};





typedef uint32_t okl4_vservices_transport_type_t;

/*lint -esym(621, OKL4_VSERVICES_TRANSPORT_TYPE_AXON) */
#define OKL4_VSERVICES_TRANSPORT_TYPE_AXON ((okl4_vservices_transport_type_t)0x0U)
/*lint -esym(621, OKL4_VSERVICES_TRANSPORT_TYPE_SHARED_BUFFER) */
#define OKL4_VSERVICES_TRANSPORT_TYPE_SHARED_BUFFER ((okl4_vservices_transport_type_t)0x1U)
/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_VSERVICES_TRANSPORT_TYPE_MAX) */
#define OKL4_VSERVICES_TRANSPORT_TYPE_MAX ((okl4_vservices_transport_type_t)0x1U)
/**
    Invalid enumeration value
*/
/*lint -esym(621, OKL4_VSERVICES_TRANSPORT_TYPE_INVALID) */
#define OKL4_VSERVICES_TRANSPORT_TYPE_INVALID ((okl4_vservices_transport_type_t)0xffffffffU)

/*lint -esym(714, okl4_vservices_transport_type_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_vservices_transport_type_is_element_of(okl4_vservices_transport_type_t var);


/*lint -esym(714, okl4_vservices_transport_type_is_element_of) */
OKL4_FORCE_INLINE okl4_bool_t
okl4_vservices_transport_type_is_element_of(okl4_vservices_transport_type_t var)
{
    /*lint --e{944} Disable dead expression detection */
    /*lint --e{948} --e{845} Disable constant always zero */
    return ((var == OKL4_VSERVICES_TRANSPORT_TYPE_AXON) ||
            (var == OKL4_VSERVICES_TRANSPORT_TYPE_SHARED_BUFFER));
}


/**

*/

struct okl4_vservices_transport_microvisor {
    okl4_bool_t is_server;
    _okl4_padding_t __padding0_1;
    _okl4_padding_t __padding1_2;
    _okl4_padding_t __padding2_3;
    okl4_vservices_transport_type_t type;
    union {
        struct {
            struct okl4_axon_ep_data ep;
            okl4_ksize_t message_size;
            okl4_count_t queue_length;
            _okl4_padding_t __padding0_4; /**< Padding 8 */
            _okl4_padding_t __padding1_5; /**< Padding 8 */
            _okl4_padding_t __padding2_6; /**< Padding 8 */
            _okl4_padding_t __padding3_7; /**< Padding 8 */
        } axon;

        struct {
            okl4_ksize_t message_size;
            okl4_count_t queue_length;
            _okl4_padding_t __padding0_4; /**< Padding 8 */
            _okl4_padding_t __padding1_5; /**< Padding 8 */
            _okl4_padding_t __padding2_6; /**< Padding 8 */
            _okl4_padding_t __padding3_7; /**< Padding 8 */
            struct okl4_virtmem_item rx;
            okl4_count_t rx_batch_size;
            okl4_count_t rx_notify_bits;
            struct okl4_virtmem_item tx;
            okl4_count_t tx_batch_size;
            okl4_count_t tx_notify_bits;
        } shared_buffer;

    } u;

    struct okl4_virtual_interrupt_lines virqs_in;
    struct okl4_virtual_interrupt_lines virqs_out;
    okl4_count_t num_services;
    _okl4_padding_t __padding3_4;
    _okl4_padding_t __padding4_5;
    _okl4_padding_t __padding5_6;
    _okl4_padding_t __padding6_7;
    __ptr64(struct okl4_vservices_service_descriptor *, services);
};




/**

*/

struct okl4_vservices_transports {
    okl4_count_t num_transports;
    _okl4_padding_t __padding0_4;
    _okl4_padding_t __padding1_5;
    _okl4_padding_t __padding2_6;
    _okl4_padding_t __padding3_7;
    __ptr64(struct okl4_vservices_transport_microvisor *, transports);
};





typedef struct okl4_axon_data okl4_axon_data_t;
typedef struct okl4_axon_ep_data okl4_axon_ep_data_t;
typedef struct okl4_range_item okl4_range_item_t;
typedef struct okl4_virtmem_item okl4_virtmem_item_t;
typedef struct okl4_cell_management_item okl4_cell_management_item_t;
typedef struct okl4_cell_management okl4_cell_management_t;
typedef struct okl4_segment_mapping okl4_segment_mapping_t;
typedef struct okl4_cell_management_segments okl4_cell_management_segments_t;
typedef struct okl4_cell_management_vcpus okl4_cell_management_vcpus_t;
typedef struct _okl4_env okl4_env_t;
typedef struct okl4_env_access_cell okl4_env_access_cell_t;
typedef struct okl4_env_access_entry okl4_env_access_entry_t;
typedef struct okl4_env_access_table okl4_env_access_table_t;
typedef struct okl4_env_args okl4_env_args_t;
typedef struct okl4_env_interrupt_device_map okl4_env_interrupt_device_map_t;
typedef struct okl4_interrupt okl4_interrupt_t;
typedef struct okl4_env_interrupt_handle okl4_env_interrupt_handle_t;
typedef struct okl4_env_interrupt_list okl4_env_interrupt_list_t;
typedef struct okl4_env_profile_cell okl4_env_profile_cell_t;
typedef struct okl4_env_profile_cpu okl4_env_profile_cpu_t;
typedef struct okl4_env_profile_table okl4_env_profile_table_t;
typedef struct okl4_env_segment okl4_env_segment_t;
typedef struct okl4_env_segment_table okl4_env_segment_table_t;
typedef struct okl4_firmware_segment okl4_firmware_segment_t;
typedef struct okl4_firmware_segments_info okl4_firmware_segments_info_t;
typedef void (*okl4_irq_callback_t)(okl4_interrupt_number_t irq, void *opaque);
typedef struct okl4_kmmu okl4_kmmu_t;
typedef struct okl4_ksp_user_agent okl4_ksp_user_agent_t;
typedef struct okl4_pipe_data okl4_pipe_data_t;
typedef struct okl4_pipe_ep_data okl4_pipe_ep_data_t;
typedef struct okl4_link okl4_link_t;
typedef struct okl4_links okl4_links_t;
typedef struct okl4_machine_info okl4_machine_info_t;
typedef struct okl4_merged_physpool okl4_merged_physpool_t;
typedef struct okl4_microvisor_timer okl4_microvisor_timer_t;
typedef struct okl4_schedule_profile_data okl4_schedule_profile_data_t;
typedef struct okl4_shared_buffer okl4_shared_buffer_t;
typedef struct okl4_shared_buffers_array okl4_shared_buffers_array_t;
typedef struct okl4_tracebuffer_env okl4_tracebuffer_env_t;
typedef struct okl4_vclient_info okl4_vclient_info_t;
typedef struct okl4_vcpu_entry okl4_vcpu_entry_t;
typedef struct okl4_vcpu_table okl4_vcpu_table_t;
typedef struct okl4_virtmem_pool okl4_virtmem_pool_t;
typedef struct okl4_virtual_interrupt_lines okl4_virtual_interrupt_lines_t;
typedef struct okl4_vserver_info okl4_vserver_info_t;
typedef struct okl4_vservices_service_descriptor okl4_vservices_service_descriptor_t;
typedef struct okl4_vservices_transport_microvisor okl4_vservices_transport_microvisor_t;
typedef struct okl4_vservices_transports okl4_vservices_transports_t;

/*
 * Return structures from system calls.
 */
/*lint -save -e958 -e959 implicit padding */
struct _okl4_sys_axon_process_recv_return {
    okl4_error_t error;
    okl4_bool_t send_empty;
};

struct _okl4_sys_axon_set_halted_return {
    okl4_error_t error;
};

struct _okl4_sys_axon_set_recv_area_return {
    okl4_error_t error;
};

struct _okl4_sys_axon_set_recv_queue_return {
    okl4_error_t error;
};

struct _okl4_sys_axon_set_recv_segment_return {
    okl4_error_t error;
};

struct _okl4_sys_axon_set_send_area_return {
    okl4_error_t error;
};

struct _okl4_sys_axon_set_send_queue_return {
    okl4_error_t error;
};

struct _okl4_sys_axon_set_send_segment_return {
    okl4_error_t error;
};

struct _okl4_sys_axon_trigger_send_return {
    okl4_error_t error;
};

struct _okl4_sys_interrupt_ack_return {
    okl4_interrupt_number_t irq;
    uint8_t source;
};

struct _okl4_sys_interrupt_attach_private_return {
    okl4_error_t error;
};

struct _okl4_sys_interrupt_attach_shared_return {
    okl4_error_t error;
};

struct _okl4_sys_interrupt_detach_return {
    okl4_error_t error;
};

struct _okl4_sys_interrupt_dist_enable_return {
    okl4_error_t error;
};

struct _okl4_sys_interrupt_eoi_return {
    okl4_error_t error;
};

struct _okl4_sys_interrupt_get_highest_priority_pending_return {
    okl4_interrupt_number_t irq;
    uint8_t source;
};

struct _okl4_sys_interrupt_get_payload_return {
    okl4_error_t error;
    okl4_virq_flags_t payload;
};

struct _okl4_sys_interrupt_limits_return {
    okl4_count_t cpunumber;
    okl4_count_t itnumber;
};

struct _okl4_sys_interrupt_mask_return {
    okl4_error_t error;
};

struct _okl4_sys_interrupt_raise_return {
    okl4_error_t error;
};

struct _okl4_sys_interrupt_set_binary_point_return {
    okl4_error_t error;
};

struct _okl4_sys_interrupt_set_config_return {
    okl4_error_t error;
};

struct _okl4_sys_interrupt_set_control_return {
    okl4_error_t error;
};

struct _okl4_sys_interrupt_set_priority_return {
    okl4_error_t error;
};

struct _okl4_sys_interrupt_set_priority_mask_return {
    okl4_error_t error;
};

struct _okl4_sys_interrupt_set_targets_return {
    okl4_error_t error;
};

struct _okl4_sys_interrupt_unmask_return {
    okl4_error_t error;
};

struct _okl4_sys_kdb_set_object_name_return {
    okl4_error_t error;
};

struct _okl4_sys_ksp_procedure_call_return {
    okl4_error_t error;
    okl4_ksp_arg_t ret0;
    okl4_ksp_arg_t ret1;
    okl4_ksp_arg_t ret2;
};

struct _okl4_sys_mmu_attach_segment_return {
    okl4_error_t error;
};

struct _okl4_sys_mmu_detach_segment_return {
    okl4_error_t error;
};

struct _okl4_sys_mmu_flush_range_return {
    okl4_error_t error;
};

struct _okl4_sys_mmu_flush_range_pn_return {
    okl4_error_t error;
};

struct _okl4_sys_mmu_lookup_page_return {
    okl4_error_t error;
    okl4_psize_tr_t offset;
    okl4_mmu_lookup_size_t size;
    _okl4_page_attribute_t page_attr;
};

struct _okl4_sys_mmu_lookup_pn_return {
    okl4_mmu_lookup_index_t segment_index;
    okl4_psize_pn_t offset_pn;
    okl4_lsize_pn_t count_pn;
    _okl4_page_attribute_t page_attr;
};

struct _okl4_sys_mmu_map_page_return {
    okl4_error_t error;
};

struct _okl4_sys_mmu_map_pn_return {
    okl4_error_t error;
};

struct _okl4_sys_mmu_unmap_page_return {
    okl4_error_t error;
};

struct _okl4_sys_mmu_unmap_pn_return {
    okl4_error_t error;
};

struct _okl4_sys_mmu_update_page_attrs_return {
    okl4_error_t error;
};

struct _okl4_sys_mmu_update_page_perms_return {
    okl4_error_t error;
};

struct _okl4_sys_mmu_update_pn_attrs_return {
    okl4_error_t error;
};

struct _okl4_sys_mmu_update_pn_perms_return {
    okl4_error_t error;
};

struct _okl4_sys_performance_null_syscall_return {
    okl4_error_t error;
};

struct _okl4_sys_pipe_control_return {
    okl4_error_t error;
};

struct _okl4_sys_pipe_recv_return {
    okl4_error_t error;
    okl4_ksize_t size;
};

struct _okl4_sys_pipe_send_return {
    okl4_error_t error;
};

struct _okl4_sys_priority_waive_return {
    okl4_error_t error;
};

struct _okl4_sys_remote_get_register_return {
    uint32_t reg_w0;
    uint32_t reg_w1;
    uint32_t reg_w2;
    uint32_t reg_w3;
    okl4_error_t error;
};

struct _okl4_sys_remote_get_registers_return {
    okl4_error_t error;
};

struct _okl4_sys_remote_read_memory32_return {
    uint32_t data;
    okl4_error_t error;
};

struct _okl4_sys_remote_set_register_return {
    okl4_error_t error;
};

struct _okl4_sys_remote_set_registers_return {
    okl4_error_t error;
};

struct _okl4_sys_remote_write_memory32_return {
    okl4_error_t error;
};

struct _okl4_sys_schedule_metrics_status_suspended_return {
    okl4_error_t error;
    uint32_t power_suspend_version;
    uint32_t power_suspend_running_count;
};

struct _okl4_sys_schedule_metrics_watch_suspended_return {
    okl4_error_t error;
};

struct _okl4_sys_schedule_profile_cpu_disable_return {
    okl4_error_t error;
};

struct _okl4_sys_schedule_profile_cpu_enable_return {
    okl4_error_t error;
    uint64_t timestamp;
};

struct _okl4_sys_schedule_profile_cpu_get_data_return {
    okl4_error_t error;
};

struct _okl4_sys_schedule_profile_vcpu_disable_return {
    okl4_error_t error;
};

struct _okl4_sys_schedule_profile_vcpu_enable_return {
    okl4_error_t error;
    uint64_t timestamp;
};

struct _okl4_sys_schedule_profile_vcpu_get_data_return {
    okl4_error_t error;
};

struct _okl4_sys_scheduler_suspend_return {
    okl4_error_t error;
};

struct _okl4_sys_timer_cancel_return {
    uint64_t remaining;
    okl4_timer_flags_t old_flags;
    okl4_error_t error;
};

struct _okl4_sys_timer_get_resolution_return {
    uint64_t tick_freq;
    uint32_t a;
    uint32_t b;
    okl4_error_t error;
};

struct _okl4_sys_timer_get_time_return {
    uint64_t time;
    okl4_error_t error;
};

struct _okl4_sys_timer_query_return {
    uint64_t remaining;
    okl4_timer_flags_t active_flags;
    okl4_error_t error;
};

struct _okl4_sys_timer_start_return {
    okl4_error_t error;
};

struct _okl4_sys_vcpu_reset_return {
    okl4_error_t error;
};

struct _okl4_sys_vcpu_start_return {
    okl4_error_t error;
};

struct _okl4_sys_vcpu_stop_return {
    okl4_error_t error;
};

struct _okl4_sys_vcpu_switch_mode_return {
    okl4_error_t error;
};

struct _okl4_sys_vinterrupt_clear_and_raise_return {
    okl4_error_t error;
    okl4_virq_flags_t payload;
};

struct _okl4_sys_vinterrupt_modify_return {
    okl4_error_t error;
};

struct _okl4_sys_vinterrupt_raise_return {
    okl4_error_t error;
};

/*lint -restore */

/*
 * Ensure type sizes have been correctly calculated by the
 * code generator.  We test to see if the C compiler agrees
 * with us about the size of the type.
 */

#if !defined(GLOBAL_STATIC_ASSERT)
#if defined(__cplusplus)
/* FIX: we should be able to use static_assert, but it doesn't compile */
#define GLOBAL_STATIC_ASSERT(expr, msg)
#else
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#define GLOBAL_STATIC_ASSERT(expr, msg) \
        _Static_assert(expr, #msg);
#else
#define GLOBAL_STATIC_ASSERT(expr, msg)
#endif
#endif
#endif


GLOBAL_STATIC_ASSERT(sizeof(okl4_arm_mpidr_t) == 8U,
        __autogen_confused_about_sizeof_arm_mpidr)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_arm_mpidr_t) == 8U,
        __autogen_confused_about_alignof_arm_mpidr)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_arm_psci_function_t) == 4U,
        __autogen_confused_about_sizeof_arm_psci_function)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_arm_psci_function_t) == 4U,
        __autogen_confused_about_alignof_arm_psci_function)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_arm_psci_result_t) == 4U,
        __autogen_confused_about_sizeof_arm_psci_result)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_arm_psci_result_t) == 4U,
        __autogen_confused_about_alignof_arm_psci_result)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_arm_psci_suspend_state_t) == 4U,
        __autogen_confused_about_sizeof_arm_psci_suspend_state)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_arm_psci_suspend_state_t) == 4U,
        __autogen_confused_about_alignof_arm_psci_suspend_state)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_arm_sctlr_t) == 4U,
        __autogen_confused_about_sizeof_arm_sctlr)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_arm_sctlr_t) == 4U,
        __autogen_confused_about_alignof_arm_sctlr)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_arm_smccc_arch_function_t) == 4U,
        __autogen_confused_about_sizeof_arm_smccc_arch_function)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_arm_smccc_arch_function_t) == 4U,
        __autogen_confused_about_alignof_arm_smccc_arch_function)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_arm_smccc_result_t) == 4U,
        __autogen_confused_about_sizeof_arm_smccc_result)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_arm_smccc_result_t) == 4U,
        __autogen_confused_about_alignof_arm_smccc_result)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_atomic_register) == 8U,
        __autogen_confused_about_sizeof_atomic_register)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_atomic_register) == 8U,
        __autogen_confused_about_alignof_atomic_register)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_atomic_register_t) == 8U,
        __autogen_confused_about_sizeof_atomic_register_t)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_atomic_register_t) == 8U,
        __autogen_confused_about_alignof_atomic_register_t)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_atomic_uint16) == 2U,
        __autogen_confused_about_sizeof_atomic_uint16)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_atomic_uint16) == 2U,
        __autogen_confused_about_alignof_atomic_uint16)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_atomic_uint16_t) == 2U,
        __autogen_confused_about_sizeof_atomic_uint16_t)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_atomic_uint16_t) == 2U,
        __autogen_confused_about_alignof_atomic_uint16_t)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_atomic_uint32) == 4U,
        __autogen_confused_about_sizeof_atomic_uint32)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_atomic_uint32) == 4U,
        __autogen_confused_about_alignof_atomic_uint32)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_atomic_uint32_t) == 4U,
        __autogen_confused_about_sizeof_atomic_uint32_t)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_atomic_uint32_t) == 4U,
        __autogen_confused_about_alignof_atomic_uint32_t)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_atomic_uint64) == 8U,
        __autogen_confused_about_sizeof_atomic_uint64)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_atomic_uint64) == 8U,
        __autogen_confused_about_alignof_atomic_uint64)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_atomic_uint64_t) == 8U,
        __autogen_confused_about_sizeof_atomic_uint64_t)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_atomic_uint64_t) == 8U,
        __autogen_confused_about_alignof_atomic_uint64_t)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_atomic_uint8) == 1U,
        __autogen_confused_about_sizeof_atomic_uint8)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_atomic_uint8) == 1U,
        __autogen_confused_about_alignof_atomic_uint8)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_atomic_uint8_t) == 1U,
        __autogen_confused_about_sizeof_atomic_uint8_t)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_atomic_uint8_t) == 1U,
        __autogen_confused_about_alignof_atomic_uint8_t)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_axon_data) == 12U,
        __autogen_confused_about_sizeof_axon_data)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_axon_data) == 4U,
        __autogen_confused_about_alignof_axon_data)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_axon_data_info_t) == 8U,
        __autogen_confused_about_sizeof_axon_data_info)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_axon_data_info_t) == 8U,
        __autogen_confused_about_alignof_axon_data_info)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_axon_ep_data) == 24U,
        __autogen_confused_about_sizeof_axon_ep_data)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_axon_ep_data) == 4U,
        __autogen_confused_about_alignof_axon_ep_data)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_axon_queue) == 12U,
        __autogen_confused_about_sizeof_axon_queue)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_axon_queue) == 4U,
        __autogen_confused_about_alignof_axon_queue)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_axon_queue_entry) == 24U,
        __autogen_confused_about_sizeof_axon_queue_entry)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_axon_queue_entry) == 8U,
        __autogen_confused_about_alignof_axon_queue_entry)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_axon_queue_size_t) == 2U,
        __autogen_confused_about_sizeof_axon_queue_size)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_axon_queue_size_t) == 2U,
        __autogen_confused_about_alignof_axon_queue_size)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_axon_rx) == 56U,
        __autogen_confused_about_sizeof_axon_rx)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_axon_rx) == 4U,
        __autogen_confused_about_alignof_axon_rx)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_axon_tx) == 48U,
        __autogen_confused_about_sizeof_axon_tx)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_axon_tx) == 4U,
        __autogen_confused_about_alignof_axon_tx)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_axon_virq_flags_t) == 8U,
        __autogen_confused_about_sizeof_axon_virq_flags)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_axon_virq_flags_t) == 8U,
        __autogen_confused_about_alignof_axon_virq_flags)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_page_cache_t) == 4U,
        __autogen_confused_about_sizeof_cache_attr)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_page_cache_t) == 4U,
        __autogen_confused_about_alignof_cache_attr)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_range_item) == 16U,
        __autogen_confused_about_sizeof_range_item)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_range_item) == 8U,
        __autogen_confused_about_alignof_range_item)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_virtmem_item) == 16U,
        __autogen_confused_about_sizeof_virtmem_item)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_virtmem_item) == 8U,
        __autogen_confused_about_alignof_virtmem_item)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_cell_management_item) == 104U,
        __autogen_confused_about_sizeof_cell_management_item)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_cell_management_item) == 8U,
        __autogen_confused_about_alignof_cell_management_item)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_cell_management) == 8U,
        __autogen_confused_about_sizeof_cell_management)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_cell_management) == 8U,
        __autogen_confused_about_alignof_cell_management)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_segment_mapping) == 32U,
        __autogen_confused_about_sizeof_segment_mapping)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_segment_mapping) == 8U,
        __autogen_confused_about_alignof_segment_mapping)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_cell_management_segments) == 8U,
        __autogen_confused_about_sizeof_cell_management_segments)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_cell_management_segments) == 8U,
        __autogen_confused_about_alignof_cell_management_segments)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_cell_management_vcpus) == 4U,
        __autogen_confused_about_sizeof_cell_management_vcpus)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_cell_management_vcpus) == 4U,
        __autogen_confused_about_alignof_cell_management_vcpus)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_cpu_mode_t) == 4U,
        __autogen_confused_about_sizeof_cpu_mode)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_cpu_mode_t) == 4U,
        __autogen_confused_about_alignof_cpu_mode)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct _okl4_env_hdr) == 4U,
        __autogen_confused_about_sizeof_env_hdr)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct _okl4_env_hdr) == 2U,
        __autogen_confused_about_alignof_env_hdr)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct _okl4_env_item) == 16U,
        __autogen_confused_about_sizeof_env_item)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct _okl4_env_item) == 8U,
        __autogen_confused_about_alignof_env_item)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct _okl4_env) == 8U,
        __autogen_confused_about_sizeof_env)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct _okl4_env) == 8U,
        __autogen_confused_about_alignof_env)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_env_access_cell) == 16U,
        __autogen_confused_about_sizeof_env_access_cell)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_env_access_cell) == 8U,
        __autogen_confused_about_alignof_env_access_cell)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_page_perms_t) == 4U,
        __autogen_confused_about_sizeof_page_perms)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_page_perms_t) == 4U,
        __autogen_confused_about_alignof_page_perms)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_env_access_entry) == 48U,
        __autogen_confused_about_sizeof_env_access_entry)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_env_access_entry) == 8U,
        __autogen_confused_about_alignof_env_access_entry)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_env_access_table) == 24U,
        __autogen_confused_about_sizeof_env_access_table)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_env_access_table) == 8U,
        __autogen_confused_about_alignof_env_access_table)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_env_args) == 8U,
        __autogen_confused_about_sizeof_env_args)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_env_args) == 8U,
        __autogen_confused_about_alignof_env_args)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_env_interrupt_device_map) == 4U,
        __autogen_confused_about_sizeof_env_interrupt_device_map)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_env_interrupt_device_map) == 4U,
        __autogen_confused_about_alignof_env_interrupt_device_map)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_interrupt) == 4U,
        __autogen_confused_about_sizeof_okl4_interrupt)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_interrupt) == 4U,
        __autogen_confused_about_alignof_okl4_interrupt)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_env_interrupt_handle) == 8U,
        __autogen_confused_about_sizeof_env_interrupt_handle)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_env_interrupt_handle) == 4U,
        __autogen_confused_about_alignof_env_interrupt_handle)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_env_interrupt_list) == 24U,
        __autogen_confused_about_sizeof_env_interrupt_list)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_env_interrupt_list) == 8U,
        __autogen_confused_about_alignof_env_interrupt_list)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_env_profile_cell) == 48U,
        __autogen_confused_about_sizeof_env_profile_cell)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_env_profile_cell) == 8U,
        __autogen_confused_about_alignof_env_profile_cell)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_env_profile_cpu) == 4U,
        __autogen_confused_about_sizeof_env_profile_cpu)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_env_profile_cpu) == 4U,
        __autogen_confused_about_alignof_env_profile_cpu)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_env_profile_table) == 16U,
        __autogen_confused_about_sizeof_env_profile_table)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_env_profile_table) == 8U,
        __autogen_confused_about_alignof_env_profile_table)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_env_segment) == 24U,
        __autogen_confused_about_sizeof_env_segment)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_env_segment) == 8U,
        __autogen_confused_about_alignof_env_segment)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_env_segment_table) == 8U,
        __autogen_confused_about_sizeof_env_segment_table)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_env_segment_table) == 8U,
        __autogen_confused_about_alignof_env_segment_table)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_error_t) == 4U,
        __autogen_confused_about_sizeof_error_t)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_error_t) == 4U,
        __autogen_confused_about_alignof_error_t)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_firmware_segment) == 32U,
        __autogen_confused_about_sizeof_firmware_segment)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_firmware_segment) == 8U,
        __autogen_confused_about_alignof_firmware_segment)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_firmware_segments_info) == 8U,
        __autogen_confused_about_sizeof_firmware_segments_info)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_firmware_segments_info) == 8U,
        __autogen_confused_about_alignof_firmware_segments_info)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_gicd_icfgr_t) == 4U,
        __autogen_confused_about_sizeof_gicd_icfgr)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_gicd_icfgr_t) == 4U,
        __autogen_confused_about_alignof_gicd_icfgr)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_sgi_target_t) == 4U,
        __autogen_confused_about_sizeof_sgi_target)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_sgi_target_t) == 4U,
        __autogen_confused_about_alignof_sgi_target)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_gicd_sgir_t) == 4U,
        __autogen_confused_about_sizeof_gicd_sgir)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_gicd_sgir_t) == 4U,
        __autogen_confused_about_alignof_gicd_sgir)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_kmmu) == 4U,
        __autogen_confused_about_sizeof_kmmu)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_kmmu) == 4U,
        __autogen_confused_about_alignof_kmmu)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_ksp_user_agent) == 8U,
        __autogen_confused_about_sizeof_ksp_user_agent)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_ksp_user_agent) == 4U,
        __autogen_confused_about_alignof_ksp_user_agent)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_pipe_data) == 8U,
        __autogen_confused_about_sizeof_pipe_data)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_pipe_data) == 4U,
        __autogen_confused_about_alignof_pipe_data)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_pipe_ep_data) == 16U,
        __autogen_confused_about_sizeof_pipe_ep_data)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_pipe_ep_data) == 4U,
        __autogen_confused_about_alignof_pipe_ep_data)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_link_role_t) == 4U,
        __autogen_confused_about_sizeof_link_role)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_link_role_t) == 4U,
        __autogen_confused_about_alignof_link_role)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_link_transport_type_t) == 4U,
        __autogen_confused_about_sizeof_link_transport_type)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_link_transport_type_t) == 4U,
        __autogen_confused_about_alignof_link_transport_type)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_link) == 80U,
        __autogen_confused_about_sizeof_link)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_link) == 8U,
        __autogen_confused_about_alignof_link)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_links) == 8U,
        __autogen_confused_about_sizeof_links)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_links) == 8U,
        __autogen_confused_about_alignof_links)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_machine_info) == 24U,
        __autogen_confused_about_sizeof_machine_info)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_machine_info) == 8U,
        __autogen_confused_about_alignof_machine_info)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_merged_physpool) == 16U,
        __autogen_confused_about_sizeof_merged_physpool)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_merged_physpool) == 8U,
        __autogen_confused_about_alignof_merged_physpool)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_microvisor_timer) == 8U,
        __autogen_confused_about_sizeof_microvisor_timer)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_microvisor_timer) == 4U,
        __autogen_confused_about_alignof_microvisor_timer)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_mmu_lookup_index_t) == 4U,
        __autogen_confused_about_sizeof_mmu_lookup_index)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_mmu_lookup_index_t) == 4U,
        __autogen_confused_about_alignof_mmu_lookup_index)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_mmu_lookup_size_t) == 8U,
        __autogen_confused_about_sizeof_mmu_lookup_size)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_mmu_lookup_size_t) == 8U,
        __autogen_confused_about_alignof_mmu_lookup_size)
#endif
GLOBAL_STATIC_ASSERT(sizeof(_okl4_page_attribute_t) == 4U,
        __autogen_confused_about_sizeof_page_attribute)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(_okl4_page_attribute_t) == 4U,
        __autogen_confused_about_alignof_page_attribute)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_pipe_control_t) == 1U,
        __autogen_confused_about_sizeof_pipe_control)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_pipe_control_t) == 1U,
        __autogen_confused_about_alignof_pipe_control)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_pipe_state_t) == 1U,
        __autogen_confused_about_sizeof_pipe_state)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_pipe_state_t) == 1U,
        __autogen_confused_about_alignof_pipe_state)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_register_set_t) == 4U,
        __autogen_confused_about_sizeof_register_set)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_register_set_t) == 4U,
        __autogen_confused_about_alignof_register_set)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_register_and_set_t) == 4U,
        __autogen_confused_about_sizeof_register_and_set)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_register_and_set_t) == 4U,
        __autogen_confused_about_alignof_register_and_set)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_cpu_registers) == 448U,
        __autogen_confused_about_sizeof_registers)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_cpu_registers) == 8U,
        __autogen_confused_about_alignof_registers)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_cpu_registers_t) == 448U,
        __autogen_confused_about_sizeof_registers_t)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_cpu_registers_t) == 8U,
        __autogen_confused_about_alignof_registers_t)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_schedule_profile_data) == 32U,
        __autogen_confused_about_sizeof_schedule_profile_data)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_schedule_profile_data) == 8U,
        __autogen_confused_about_alignof_schedule_profile_data)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_scheduler_virq_flags_t) == 8U,
        __autogen_confused_about_sizeof_scheduler_virq_flags)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_scheduler_virq_flags_t) == 8U,
        __autogen_confused_about_alignof_scheduler_virq_flags)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_sdk_version_t) == 4U,
        __autogen_confused_about_sizeof_sdk_version)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_sdk_version_t) == 4U,
        __autogen_confused_about_alignof_sdk_version)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_shared_buffer) == 32U,
        __autogen_confused_about_sizeof_shared_buffer)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_shared_buffer) == 8U,
        __autogen_confused_about_alignof_shared_buffer)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_shared_buffers_array) == 16U,
        __autogen_confused_about_sizeof_shared_buffers_array)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_shared_buffers_array) == 8U,
        __autogen_confused_about_alignof_shared_buffers_array)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_timer_flags_t) == 4U,
        __autogen_confused_about_sizeof_timer_flags)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_timer_flags_t) == 4U,
        __autogen_confused_about_alignof_timer_flags)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct _okl4_tracebuffer_buffer_header) == 40U,
        __autogen_confused_about_sizeof_tracebuffer_buffer_header)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct _okl4_tracebuffer_buffer_header) == 8U,
        __autogen_confused_about_alignof_tracebuffer_buffer_header)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_tracebuffer_env) == 24U,
        __autogen_confused_about_sizeof_tracebuffer_env)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_tracebuffer_env) == 8U,
        __autogen_confused_about_alignof_tracebuffer_env)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct _okl4_tracebuffer_header) == 40U,
        __autogen_confused_about_sizeof_tracebuffer_header)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct _okl4_tracebuffer_header) == 8U,
        __autogen_confused_about_alignof_tracebuffer_header)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_tracepoint_class_t) == 4U,
        __autogen_confused_about_sizeof_tracepoint_class)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_tracepoint_class_t) == 4U,
        __autogen_confused_about_alignof_tracepoint_class)
#endif
GLOBAL_STATIC_ASSERT(sizeof(_okl4_tracepoint_desc_t) == 4U,
        __autogen_confused_about_sizeof_tracepoint_desc)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(_okl4_tracepoint_desc_t) == 4U,
        __autogen_confused_about_alignof_tracepoint_desc)
#endif
GLOBAL_STATIC_ASSERT(sizeof(_okl4_tracepoint_masks_t) == 4U,
        __autogen_confused_about_sizeof_tracepoint_masks)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(_okl4_tracepoint_masks_t) == 4U,
        __autogen_confused_about_alignof_tracepoint_masks)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_tracepoint_entry_base) == 12U,
        __autogen_confused_about_sizeof_tracepoint_entry_base)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_tracepoint_entry_base) == 4U,
        __autogen_confused_about_alignof_tracepoint_entry_base)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_tracepoint_evt_t) == 4U,
        __autogen_confused_about_sizeof_tracepoint_evt)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_tracepoint_evt_t) == 4U,
        __autogen_confused_about_alignof_tracepoint_evt)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_tracepoint_level_t) == 4U,
        __autogen_confused_about_sizeof_tracepoint_level)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_tracepoint_level_t) == 4U,
        __autogen_confused_about_alignof_tracepoint_level)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_tracepoint_subsystem_t) == 4U,
        __autogen_confused_about_sizeof_tracepoint_subsystem)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_tracepoint_subsystem_t) == 4U,
        __autogen_confused_about_alignof_tracepoint_subsystem)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_tracepoint_unpacked_entry) == 12U,
        __autogen_confused_about_sizeof_tracepoint_unpacked_entry)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_tracepoint_unpacked_entry) == 4U,
        __autogen_confused_about_alignof_tracepoint_unpacked_entry)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_vclient_info) == 32U,
        __autogen_confused_about_sizeof_vclient_info)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_vclient_info) == 8U,
        __autogen_confused_about_alignof_vclient_info)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_vcpu_entry) == 24U,
        __autogen_confused_about_sizeof_vcpu_entry)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_vcpu_entry) == 8U,
        __autogen_confused_about_alignof_vcpu_entry)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_vcpu_table) == 16U,
        __autogen_confused_about_sizeof_vcpu_table)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_vcpu_table) == 8U,
        __autogen_confused_about_alignof_vcpu_table)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_vfp_ctrl_registers) == 8U,
        __autogen_confused_about_sizeof_vfp_ctrl_registers)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_vfp_ctrl_registers) == 4U,
        __autogen_confused_about_alignof_vfp_ctrl_registers)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_vfp_ctrl_registers_t) == 8U,
        __autogen_confused_about_sizeof_vfp_ctrl_registers_t)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_vfp_ctrl_registers_t) == 4U,
        __autogen_confused_about_alignof_vfp_ctrl_registers_t)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_vfp_ops_t) == 4U,
        __autogen_confused_about_sizeof_vfp_ops)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_vfp_ops_t) == 4U,
        __autogen_confused_about_alignof_vfp_ops)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_vfp_register) == 16U,
        __autogen_confused_about_sizeof_vfp_register)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_vfp_register) == 16U,
        __autogen_confused_about_alignof_vfp_register)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_vfp_register_t) == 16U,
        __autogen_confused_about_sizeof_vfp_register_t)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_vfp_register_t) == 16U,
        __autogen_confused_about_alignof_vfp_register_t)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_vfp_registers) == 528U,
        __autogen_confused_about_sizeof_vfp_registers)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_vfp_registers) == 16U,
        __autogen_confused_about_alignof_vfp_registers)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_vfp_registers_t) == 528U,
        __autogen_confused_about_sizeof_vfp_registers_t)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_vfp_registers_t) == 16U,
        __autogen_confused_about_alignof_vfp_registers_t)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_virtmem_pool) == 16U,
        __autogen_confused_about_sizeof_virtmem_pool)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_virtmem_pool) == 8U,
        __autogen_confused_about_alignof_virtmem_pool)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_virtual_interrupt_lines) == 16U,
        __autogen_confused_about_sizeof_virtual_interrupt_lines)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_virtual_interrupt_lines) == 8U,
        __autogen_confused_about_alignof_virtual_interrupt_lines)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_vserver_info) == 32U,
        __autogen_confused_about_sizeof_vserver_info)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_vserver_info) == 8U,
        __autogen_confused_about_alignof_vserver_info)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_vservices_service_descriptor) == 24U,
        __autogen_confused_about_sizeof_vservices_service_descriptor)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_vservices_service_descriptor) == 8U,
        __autogen_confused_about_alignof_vservices_service_descriptor)
#endif
GLOBAL_STATIC_ASSERT(sizeof(okl4_vservices_transport_type_t) == 4U,
        __autogen_confused_about_sizeof_vservices_transport_type)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(okl4_vservices_transport_type_t) == 4U,
        __autogen_confused_about_alignof_vservices_transport_type)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_vservices_transport_microvisor) == 120U,
        __autogen_confused_about_sizeof_vservices_transport_microvisor)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_vservices_transport_microvisor) == 8U,
        __autogen_confused_about_alignof_vservices_transport_microvisor)
#endif
GLOBAL_STATIC_ASSERT(sizeof(struct okl4_vservices_transports) == 16U,
        __autogen_confused_about_sizeof_vservices_transports)
#if !defined(LINTER)
GLOBAL_STATIC_ASSERT(_Alignof(struct okl4_vservices_transports) == 8U,
        __autogen_confused_about_alignof_vservices_transports)
#endif

#else

/**
 *  okl4_arm_mpidr_t
 **/


/*lint -esym(621, OKL4_ASM_MASK_AFF0_ARM_MPIDR) */
#define OKL4_ASM_MASK_AFF0_ARM_MPIDR (255)
/*lint -esym(621, OKL4_ASM_SHIFT_AFF0_ARM_MPIDR) */
#define OKL4_ASM_SHIFT_AFF0_ARM_MPIDR (0)
/*lint -esym(621, OKL4_ASM_WIDTH_AFF0_ARM_MPIDR) */
#define OKL4_ASM_WIDTH_AFF0_ARM_MPIDR (8)
/*lint -esym(621, OKL4_ASM_MASK_AFF1_ARM_MPIDR) */
#define OKL4_ASM_MASK_AFF1_ARM_MPIDR (255 << 8)
/*lint -esym(621, OKL4_ASM_SHIFT_AFF1_ARM_MPIDR) */
#define OKL4_ASM_SHIFT_AFF1_ARM_MPIDR (8)
/*lint -esym(621, OKL4_ASM_WIDTH_AFF1_ARM_MPIDR) */
#define OKL4_ASM_WIDTH_AFF1_ARM_MPIDR (8)
/*lint -esym(621, OKL4_ASM_MASK_AFF2_ARM_MPIDR) */
#define OKL4_ASM_MASK_AFF2_ARM_MPIDR (255 << 16)
/*lint -esym(621, OKL4_ASM_SHIFT_AFF2_ARM_MPIDR) */
#define OKL4_ASM_SHIFT_AFF2_ARM_MPIDR (16)
/*lint -esym(621, OKL4_ASM_WIDTH_AFF2_ARM_MPIDR) */
#define OKL4_ASM_WIDTH_AFF2_ARM_MPIDR (8)
/*lint -esym(621, OKL4_ASM_MASK_MT_ARM_MPIDR) */
#define OKL4_ASM_MASK_MT_ARM_MPIDR (1 << 24)
/*lint -esym(621, OKL4_ASM_SHIFT_MT_ARM_MPIDR) */
#define OKL4_ASM_SHIFT_MT_ARM_MPIDR (24)
/*lint -esym(621, OKL4_ASM_WIDTH_MT_ARM_MPIDR) */
#define OKL4_ASM_WIDTH_MT_ARM_MPIDR (1)
/*lint -esym(621, OKL4_ASM_MASK_U_ARM_MPIDR) */
#define OKL4_ASM_MASK_U_ARM_MPIDR (1 << 30)
/*lint -esym(621, OKL4_ASM_SHIFT_U_ARM_MPIDR) */
#define OKL4_ASM_SHIFT_U_ARM_MPIDR (30)
/*lint -esym(621, OKL4_ASM_WIDTH_U_ARM_MPIDR) */
#define OKL4_ASM_WIDTH_U_ARM_MPIDR (1)
/*lint -esym(621, OKL4_ASM_MASK_MP_ARM_MPIDR) */
#define OKL4_ASM_MASK_MP_ARM_MPIDR (1 << 31)
/*lint -esym(621, OKL4_ASM_SHIFT_MP_ARM_MPIDR) */
#define OKL4_ASM_SHIFT_MP_ARM_MPIDR (31)
/*lint -esym(621, OKL4_ASM_WIDTH_MP_ARM_MPIDR) */
#define OKL4_ASM_WIDTH_MP_ARM_MPIDR (1)
/*lint -esym(621, OKL4_ASM_MASK_AFF3_ARM_MPIDR) */
#define OKL4_ASM_MASK_AFF3_ARM_MPIDR (255 << 32)
/*lint -esym(621, OKL4_ASM_SHIFT_AFF3_ARM_MPIDR) */
#define OKL4_ASM_SHIFT_AFF3_ARM_MPIDR (32)
/*lint -esym(621, OKL4_ASM_WIDTH_AFF3_ARM_MPIDR) */
#define OKL4_ASM_WIDTH_AFF3_ARM_MPIDR (8)


/**
 *  uint32_t
 **/
/*lint -esym(621, OKL4_AXON_NUM_RECEIVE_QUEUES) */
#define OKL4_AXON_NUM_RECEIVE_QUEUES (4)

/*lint -esym(621, OKL4_AXON_NUM_SEND_QUEUES) */
#define OKL4_AXON_NUM_SEND_QUEUES (4)

/*lint -esym(621, _OKL4_POISON) */
#define _OKL4_POISON (3735928559)

/*lint -esym(621, OKL4_TRACEBUFFER_INVALID_REF) */
#define OKL4_TRACEBUFFER_INVALID_REF (-1)

/**
 *  okl4_arm_psci_function_t
 **/
/*lint -esym(621, OKL4_ASM_ARM_PSCI_FUNCTION_PSCI_VERSION) */
#define OKL4_ASM_ARM_PSCI_FUNCTION_PSCI_VERSION (0x0)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_FUNCTION_CPU_SUSPEND) */
#define OKL4_ASM_ARM_PSCI_FUNCTION_CPU_SUSPEND (0x1)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_FUNCTION_CPU_OFF) */
#define OKL4_ASM_ARM_PSCI_FUNCTION_CPU_OFF (0x2)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_FUNCTION_CPU_ON) */
#define OKL4_ASM_ARM_PSCI_FUNCTION_CPU_ON (0x3)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_FUNCTION_AFFINITY_INFO) */
#define OKL4_ASM_ARM_PSCI_FUNCTION_AFFINITY_INFO (0x4)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_FUNCTION_MIGRATE) */
#define OKL4_ASM_ARM_PSCI_FUNCTION_MIGRATE (0x5)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_FUNCTION_MIGRATE_INFO_TYPE) */
#define OKL4_ASM_ARM_PSCI_FUNCTION_MIGRATE_INFO_TYPE (0x6)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_FUNCTION_MIGRATE_INFO_UP_CPU) */
#define OKL4_ASM_ARM_PSCI_FUNCTION_MIGRATE_INFO_UP_CPU (0x7)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_FUNCTION_SYSTEM_OFF) */
#define OKL4_ASM_ARM_PSCI_FUNCTION_SYSTEM_OFF (0x8)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_FUNCTION_SYSTEM_RESET) */
#define OKL4_ASM_ARM_PSCI_FUNCTION_SYSTEM_RESET (0x9)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_FUNCTION_PSCI_FEATURES) */
#define OKL4_ASM_ARM_PSCI_FUNCTION_PSCI_FEATURES (0xa)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_FUNCTION_CPU_FREEZE) */
#define OKL4_ASM_ARM_PSCI_FUNCTION_CPU_FREEZE (0xb)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_FUNCTION_CPU_DEFAULT_SUSPEND) */
#define OKL4_ASM_ARM_PSCI_FUNCTION_CPU_DEFAULT_SUSPEND (0xc)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_FUNCTION_NODE_HW_STATE) */
#define OKL4_ASM_ARM_PSCI_FUNCTION_NODE_HW_STATE (0xd)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_FUNCTION_SYSTEM_SUSPEND) */
#define OKL4_ASM_ARM_PSCI_FUNCTION_SYSTEM_SUSPEND (0xe)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_FUNCTION_PSCI_SET_SUSPEND_MODE) */
#define OKL4_ASM_ARM_PSCI_FUNCTION_PSCI_SET_SUSPEND_MODE (0xf)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_FUNCTION_PSCI_STAT_RESIDENCY) */
#define OKL4_ASM_ARM_PSCI_FUNCTION_PSCI_STAT_RESIDENCY (0x10)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_FUNCTION_PSCI_STAT_COUNT) */
#define OKL4_ASM_ARM_PSCI_FUNCTION_PSCI_STAT_COUNT (0x11)

/**
 *  okl4_arm_psci_result_t
 **/
/*lint -esym(621, OKL4_ASM_ARM_PSCI_RESULT_SUCCESS) */
#define OKL4_ASM_ARM_PSCI_RESULT_SUCCESS (0x0)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_RESULT_INVALID_ADDRESS) */
#define OKL4_ASM_ARM_PSCI_RESULT_INVALID_ADDRESS (0xfffffff7)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_RESULT_DISABLED) */
#define OKL4_ASM_ARM_PSCI_RESULT_DISABLED (0xfffffff8)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_RESULT_NOT_PRESENT) */
#define OKL4_ASM_ARM_PSCI_RESULT_NOT_PRESENT (0xfffffff9)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_RESULT_INTERNAL_FAILURE) */
#define OKL4_ASM_ARM_PSCI_RESULT_INTERNAL_FAILURE (0xfffffffa)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_RESULT_ON_PENDING) */
#define OKL4_ASM_ARM_PSCI_RESULT_ON_PENDING (0xfffffffb)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_RESULT_ALREADY_ON) */
#define OKL4_ASM_ARM_PSCI_RESULT_ALREADY_ON (0xfffffffc)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_RESULT_DENIED) */
#define OKL4_ASM_ARM_PSCI_RESULT_DENIED (0xfffffffd)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_RESULT_INVALID_PARAMETERS) */
#define OKL4_ASM_ARM_PSCI_RESULT_INVALID_PARAMETERS (0xfffffffe)
/*lint -esym(621, OKL4_ASM_ARM_PSCI_RESULT_NOT_SUPPORTED) */
#define OKL4_ASM_ARM_PSCI_RESULT_NOT_SUPPORTED (0xffffffff)

/**
 *  okl4_arm_psci_suspend_state_t
 **/

/*lint -esym(621, OKL4_ARM_PSCI_POWER_LEVEL_CPU) */
#define OKL4_ARM_PSCI_POWER_LEVEL_CPU (0)

/*lint -esym(621, OKL4_ASM_MASK_STATE_ID_ARM_PSCI_SUSPEND_STATE) */
#define OKL4_ASM_MASK_STATE_ID_ARM_PSCI_SUSPEND_STATE (65535)
/*lint -esym(621, OKL4_ASM_SHIFT_STATE_ID_ARM_PSCI_SUSPEND_STATE) */
#define OKL4_ASM_SHIFT_STATE_ID_ARM_PSCI_SUSPEND_STATE (0)
/*lint -esym(621, OKL4_ASM_WIDTH_STATE_ID_ARM_PSCI_SUSPEND_STATE) */
#define OKL4_ASM_WIDTH_STATE_ID_ARM_PSCI_SUSPEND_STATE (16)
/*lint -esym(621, OKL4_ASM_MASK_POWER_DOWN_ARM_PSCI_SUSPEND_STATE) */
#define OKL4_ASM_MASK_POWER_DOWN_ARM_PSCI_SUSPEND_STATE (1 << 16)
/*lint -esym(621, OKL4_ASM_SHIFT_POWER_DOWN_ARM_PSCI_SUSPEND_STATE) */
#define OKL4_ASM_SHIFT_POWER_DOWN_ARM_PSCI_SUSPEND_STATE (16)
/*lint -esym(621, OKL4_ASM_WIDTH_POWER_DOWN_ARM_PSCI_SUSPEND_STATE) */
#define OKL4_ASM_WIDTH_POWER_DOWN_ARM_PSCI_SUSPEND_STATE (1)
/*lint -esym(621, OKL4_ASM_MASK_POWER_LEVEL_ARM_PSCI_SUSPEND_STATE) */
#define OKL4_ASM_MASK_POWER_LEVEL_ARM_PSCI_SUSPEND_STATE (3 << 24)
/*lint -esym(621, OKL4_ASM_SHIFT_POWER_LEVEL_ARM_PSCI_SUSPEND_STATE) */
#define OKL4_ASM_SHIFT_POWER_LEVEL_ARM_PSCI_SUSPEND_STATE (24)
/*lint -esym(621, OKL4_ASM_WIDTH_POWER_LEVEL_ARM_PSCI_SUSPEND_STATE) */
#define OKL4_ASM_WIDTH_POWER_LEVEL_ARM_PSCI_SUSPEND_STATE (2)


/**
 *  okl4_arm_sctlr_t
 **/


/*lint -esym(621, OKL4_ASM_MASK_MMU_ENABLE_ARM_SCTLR) */
#define OKL4_ASM_MASK_MMU_ENABLE_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_SHIFT_MMU_ENABLE_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_MMU_ENABLE_ARM_SCTLR (0)
/*lint -esym(621, OKL4_ASM_WIDTH_MMU_ENABLE_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_MMU_ENABLE_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_MASK_ALIGNMENT_CHECK_ENABLE_ARM_SCTLR) */
#define OKL4_ASM_MASK_ALIGNMENT_CHECK_ENABLE_ARM_SCTLR (1 << 1)
/*lint -esym(621, OKL4_ASM_SHIFT_ALIGNMENT_CHECK_ENABLE_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_ALIGNMENT_CHECK_ENABLE_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_WIDTH_ALIGNMENT_CHECK_ENABLE_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_ALIGNMENT_CHECK_ENABLE_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_MASK_DATA_CACHE_ENABLE_ARM_SCTLR) */
#define OKL4_ASM_MASK_DATA_CACHE_ENABLE_ARM_SCTLR (1 << 2)
/*lint -esym(621, OKL4_ASM_SHIFT_DATA_CACHE_ENABLE_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_DATA_CACHE_ENABLE_ARM_SCTLR (2)
/*lint -esym(621, OKL4_ASM_WIDTH_DATA_CACHE_ENABLE_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_DATA_CACHE_ENABLE_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_MASK_STACK_ALIGN_ARM_SCTLR) */
#define OKL4_ASM_MASK_STACK_ALIGN_ARM_SCTLR (1 << 3)
/*lint -esym(621, OKL4_ASM_SHIFT_STACK_ALIGN_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_STACK_ALIGN_ARM_SCTLR (3)
/*lint -esym(621, OKL4_ASM_WIDTH_STACK_ALIGN_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_STACK_ALIGN_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_MASK_STACK_ALIGN_EL0_ARM_SCTLR) */
#define OKL4_ASM_MASK_STACK_ALIGN_EL0_ARM_SCTLR (1 << 4)
/*lint -esym(621, OKL4_ASM_SHIFT_STACK_ALIGN_EL0_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_STACK_ALIGN_EL0_ARM_SCTLR (4)
/*lint -esym(621, OKL4_ASM_WIDTH_STACK_ALIGN_EL0_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_STACK_ALIGN_EL0_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_MASK_CP15_BARRIER_ENABLE_ARM_SCTLR) */
#define OKL4_ASM_MASK_CP15_BARRIER_ENABLE_ARM_SCTLR (1 << 5)
/*lint -esym(621, OKL4_ASM_SHIFT_CP15_BARRIER_ENABLE_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_CP15_BARRIER_ENABLE_ARM_SCTLR (5)
/*lint -esym(621, OKL4_ASM_WIDTH_CP15_BARRIER_ENABLE_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_CP15_BARRIER_ENABLE_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_MASK_OKL_HCR_EL2_DC_ARM_SCTLR) */
#define OKL4_ASM_MASK_OKL_HCR_EL2_DC_ARM_SCTLR (1 << 6)
/*lint -esym(621, OKL4_ASM_SHIFT_OKL_HCR_EL2_DC_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_OKL_HCR_EL2_DC_ARM_SCTLR (6)
/*lint -esym(621, OKL4_ASM_WIDTH_OKL_HCR_EL2_DC_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_OKL_HCR_EL2_DC_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_MASK_IT_DISABLE_ARM_SCTLR) */
#define OKL4_ASM_MASK_IT_DISABLE_ARM_SCTLR (1 << 7)
/*lint -esym(621, OKL4_ASM_SHIFT_IT_DISABLE_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_IT_DISABLE_ARM_SCTLR (7)
/*lint -esym(621, OKL4_ASM_WIDTH_IT_DISABLE_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_IT_DISABLE_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_MASK_SETEND_DISABLE_ARM_SCTLR) */
#define OKL4_ASM_MASK_SETEND_DISABLE_ARM_SCTLR (1 << 8)
/*lint -esym(621, OKL4_ASM_SHIFT_SETEND_DISABLE_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_SETEND_DISABLE_ARM_SCTLR (8)
/*lint -esym(621, OKL4_ASM_WIDTH_SETEND_DISABLE_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_SETEND_DISABLE_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_MASK_USER_MASK_ACCESS_ARM_SCTLR) */
#define OKL4_ASM_MASK_USER_MASK_ACCESS_ARM_SCTLR (1 << 9)
/*lint -esym(621, OKL4_ASM_SHIFT_USER_MASK_ACCESS_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_USER_MASK_ACCESS_ARM_SCTLR (9)
/*lint -esym(621, OKL4_ASM_WIDTH_USER_MASK_ACCESS_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_USER_MASK_ACCESS_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_MASK_RESERVED11_ARM_SCTLR) */
#define OKL4_ASM_MASK_RESERVED11_ARM_SCTLR (1 << 11)
/*lint -esym(621, OKL4_ASM_SHIFT_RESERVED11_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_RESERVED11_ARM_SCTLR (11)
/*lint -esym(621, OKL4_ASM_WIDTH_RESERVED11_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_RESERVED11_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_MASK_INSTRUCTION_CACHE_ENABLE_ARM_SCTLR) */
#define OKL4_ASM_MASK_INSTRUCTION_CACHE_ENABLE_ARM_SCTLR (1 << 12)
/*lint -esym(621, OKL4_ASM_SHIFT_INSTRUCTION_CACHE_ENABLE_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_INSTRUCTION_CACHE_ENABLE_ARM_SCTLR (12)
/*lint -esym(621, OKL4_ASM_WIDTH_INSTRUCTION_CACHE_ENABLE_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_INSTRUCTION_CACHE_ENABLE_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_MASK_VECTORS_BIT_ARM_SCTLR) */
#define OKL4_ASM_MASK_VECTORS_BIT_ARM_SCTLR (1 << 13)
/*lint -esym(621, OKL4_ASM_SHIFT_VECTORS_BIT_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_VECTORS_BIT_ARM_SCTLR (13)
/*lint -esym(621, OKL4_ASM_WIDTH_VECTORS_BIT_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_VECTORS_BIT_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_MASK_DCACHE_ZERO_ARM_SCTLR) */
#define OKL4_ASM_MASK_DCACHE_ZERO_ARM_SCTLR (1 << 14)
/*lint -esym(621, OKL4_ASM_SHIFT_DCACHE_ZERO_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_DCACHE_ZERO_ARM_SCTLR (14)
/*lint -esym(621, OKL4_ASM_WIDTH_DCACHE_ZERO_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_DCACHE_ZERO_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_MASK_USER_CACHE_TYPE_ARM_SCTLR) */
#define OKL4_ASM_MASK_USER_CACHE_TYPE_ARM_SCTLR (1 << 15)
/*lint -esym(621, OKL4_ASM_SHIFT_USER_CACHE_TYPE_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_USER_CACHE_TYPE_ARM_SCTLR (15)
/*lint -esym(621, OKL4_ASM_WIDTH_USER_CACHE_TYPE_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_USER_CACHE_TYPE_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_MASK_NO_TRAP_WFI_ARM_SCTLR) */
#define OKL4_ASM_MASK_NO_TRAP_WFI_ARM_SCTLR (1 << 16)
/*lint -esym(621, OKL4_ASM_SHIFT_NO_TRAP_WFI_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_NO_TRAP_WFI_ARM_SCTLR (16)
/*lint -esym(621, OKL4_ASM_WIDTH_NO_TRAP_WFI_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_NO_TRAP_WFI_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_MASK_NO_TRAP_WFE_ARM_SCTLR) */
#define OKL4_ASM_MASK_NO_TRAP_WFE_ARM_SCTLR (1 << 18)
/*lint -esym(621, OKL4_ASM_SHIFT_NO_TRAP_WFE_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_NO_TRAP_WFE_ARM_SCTLR (18)
/*lint -esym(621, OKL4_ASM_WIDTH_NO_TRAP_WFE_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_NO_TRAP_WFE_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_MASK_WRITE_EXEC_NEVER_ARM_SCTLR) */
#define OKL4_ASM_MASK_WRITE_EXEC_NEVER_ARM_SCTLR (1 << 19)
/*lint -esym(621, OKL4_ASM_SHIFT_WRITE_EXEC_NEVER_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_WRITE_EXEC_NEVER_ARM_SCTLR (19)
/*lint -esym(621, OKL4_ASM_WIDTH_WRITE_EXEC_NEVER_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_WRITE_EXEC_NEVER_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_MASK_USER_WRITE_EXEC_NEVER_ARM_SCTLR) */
#define OKL4_ASM_MASK_USER_WRITE_EXEC_NEVER_ARM_SCTLR (1 << 20)
/*lint -esym(621, OKL4_ASM_SHIFT_USER_WRITE_EXEC_NEVER_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_USER_WRITE_EXEC_NEVER_ARM_SCTLR (20)
/*lint -esym(621, OKL4_ASM_WIDTH_USER_WRITE_EXEC_NEVER_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_USER_WRITE_EXEC_NEVER_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_MASK_RESERVED22_ARM_SCTLR) */
#define OKL4_ASM_MASK_RESERVED22_ARM_SCTLR (1 << 22)
/*lint -esym(621, OKL4_ASM_SHIFT_RESERVED22_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_RESERVED22_ARM_SCTLR (22)
/*lint -esym(621, OKL4_ASM_WIDTH_RESERVED22_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_RESERVED22_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_MASK_RESERVED23_ARM_SCTLR) */
#define OKL4_ASM_MASK_RESERVED23_ARM_SCTLR (1 << 23)
/*lint -esym(621, OKL4_ASM_SHIFT_RESERVED23_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_RESERVED23_ARM_SCTLR (23)
/*lint -esym(621, OKL4_ASM_WIDTH_RESERVED23_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_RESERVED23_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_MASK_EL0_ENDIANNESS_ARM_SCTLR) */
#define OKL4_ASM_MASK_EL0_ENDIANNESS_ARM_SCTLR (1 << 24)
/*lint -esym(621, OKL4_ASM_SHIFT_EL0_ENDIANNESS_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_EL0_ENDIANNESS_ARM_SCTLR (24)
/*lint -esym(621, OKL4_ASM_WIDTH_EL0_ENDIANNESS_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_EL0_ENDIANNESS_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_MASK_EXCEPTION_ENDIANNESS_ARM_SCTLR) */
#define OKL4_ASM_MASK_EXCEPTION_ENDIANNESS_ARM_SCTLR (1 << 25)
/*lint -esym(621, OKL4_ASM_SHIFT_EXCEPTION_ENDIANNESS_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_EXCEPTION_ENDIANNESS_ARM_SCTLR (25)
/*lint -esym(621, OKL4_ASM_WIDTH_EXCEPTION_ENDIANNESS_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_EXCEPTION_ENDIANNESS_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_MASK_TEX_REMAP_ENABLE_ARM_SCTLR) */
#define OKL4_ASM_MASK_TEX_REMAP_ENABLE_ARM_SCTLR (1 << 28)
/*lint -esym(621, OKL4_ASM_SHIFT_TEX_REMAP_ENABLE_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_TEX_REMAP_ENABLE_ARM_SCTLR (28)
/*lint -esym(621, OKL4_ASM_WIDTH_TEX_REMAP_ENABLE_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_TEX_REMAP_ENABLE_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_MASK_ACCESS_FLAG_ENABLE_ARM_SCTLR) */
#define OKL4_ASM_MASK_ACCESS_FLAG_ENABLE_ARM_SCTLR (1 << 29)
/*lint -esym(621, OKL4_ASM_SHIFT_ACCESS_FLAG_ENABLE_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_ACCESS_FLAG_ENABLE_ARM_SCTLR (29)
/*lint -esym(621, OKL4_ASM_WIDTH_ACCESS_FLAG_ENABLE_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_ACCESS_FLAG_ENABLE_ARM_SCTLR (1)
/*lint -esym(621, OKL4_ASM_MASK_THUMB_EXCEPTION_ENABLE_ARM_SCTLR) */
#define OKL4_ASM_MASK_THUMB_EXCEPTION_ENABLE_ARM_SCTLR (1 << 30)
/*lint -esym(621, OKL4_ASM_SHIFT_THUMB_EXCEPTION_ENABLE_ARM_SCTLR) */
#define OKL4_ASM_SHIFT_THUMB_EXCEPTION_ENABLE_ARM_SCTLR (30)
/*lint -esym(621, OKL4_ASM_WIDTH_THUMB_EXCEPTION_ENABLE_ARM_SCTLR) */
#define OKL4_ASM_WIDTH_THUMB_EXCEPTION_ENABLE_ARM_SCTLR (1)


/**
 *  okl4_arm_smccc_arch_function_t
 **/
/*lint -esym(621, OKL4_ASM_ARM_SMCCC_ARCH_FUNCTION_SMCCC_VERSION) */
#define OKL4_ASM_ARM_SMCCC_ARCH_FUNCTION_SMCCC_VERSION (0x0)
/*lint -esym(621, OKL4_ASM_ARM_SMCCC_ARCH_FUNCTION_ARCH_FEATURES) */
#define OKL4_ASM_ARM_SMCCC_ARCH_FUNCTION_ARCH_FEATURES (0x1)
/*lint -esym(621, OKL4_ASM_ARM_SMCCC_ARCH_FUNCTION_ARCH_WORKAROUND_1) */
#define OKL4_ASM_ARM_SMCCC_ARCH_FUNCTION_ARCH_WORKAROUND_1 (0x8000)

/**
 *  okl4_arm_smccc_result_t
 **/
/*lint -esym(621, OKL4_ASM_ARM_SMCCC_RESULT_SUCCESS) */
#define OKL4_ASM_ARM_SMCCC_RESULT_SUCCESS (0x0)
/*lint -esym(621, OKL4_ASM_ARM_SMCCC_RESULT_NOT_SUPPORTED) */
#define OKL4_ASM_ARM_SMCCC_RESULT_NOT_SUPPORTED (0xffffffff)

/**
 *  okl4_count_t
 **/
/*lint -esym(621, OKL4_DEFAULT_PAGEBITS) */
#define OKL4_DEFAULT_PAGEBITS (12)

/** The maximum limit for segment index retured in mmu_lookup_segment. */
/*lint -esym(621, OKL4_KMMU_LOOKUP_PAGE_SEGMENT_MASK) */
#define OKL4_KMMU_LOOKUP_PAGE_SEGMENT_MASK (1023)

/** The maximum limit for segment attachments to a KMMU. */
/*lint -esym(621, OKL4_KMMU_MAX_SEGMENTS) */
#define OKL4_KMMU_MAX_SEGMENTS (256)

/*lint -esym(621, OKL4_PROFILE_NO_PCPUS) */
#define OKL4_PROFILE_NO_PCPUS (-1)

/**
 *  okl4_kcap_t
 **/
/*lint -esym(621, OKL4_KCAP_INVALID) */
#define OKL4_KCAP_INVALID (-1)

/**
 *  okl4_interrupt_number_t
 **/
/*lint -esym(621, OKL4_INTERRUPT_INVALID_IRQ) */
#define OKL4_INTERRUPT_INVALID_IRQ (1023)

/*lint -esym(621, OKL4_INVALID_VIRQ) */
#define OKL4_INVALID_VIRQ (1023)

/**
 *  okl4_lsize_t
 **/
/*lint -esym(621, OKL4_DEFAULT_PAGESIZE) */
#define OKL4_DEFAULT_PAGESIZE (4096)

/**
 *  okl4_laddr_t
 **/
/*lint -esym(621, OKL4_USER_AREA_END) */
#define OKL4_USER_AREA_END (17592186044416)

/**
 *  okl4_axon_data_info_t
 **/


/*lint -esym(621, OKL4_ASM_MASK_PENDING_AXON_DATA_INFO) */
#define OKL4_ASM_MASK_PENDING_AXON_DATA_INFO (1)
/*lint -esym(621, OKL4_ASM_SHIFT_PENDING_AXON_DATA_INFO) */
#define OKL4_ASM_SHIFT_PENDING_AXON_DATA_INFO (0)
/*lint -esym(621, OKL4_ASM_WIDTH_PENDING_AXON_DATA_INFO) */
#define OKL4_ASM_WIDTH_PENDING_AXON_DATA_INFO (1)
/*lint -esym(621, OKL4_ASM_MASK_FAILURE_AXON_DATA_INFO) */
#define OKL4_ASM_MASK_FAILURE_AXON_DATA_INFO (1 << 1)
/*lint -esym(621, OKL4_ASM_SHIFT_FAILURE_AXON_DATA_INFO) */
#define OKL4_ASM_SHIFT_FAILURE_AXON_DATA_INFO (1)
/*lint -esym(621, OKL4_ASM_WIDTH_FAILURE_AXON_DATA_INFO) */
#define OKL4_ASM_WIDTH_FAILURE_AXON_DATA_INFO (1)
/*lint -esym(621, OKL4_ASM_MASK_USR_AXON_DATA_INFO) */
#define OKL4_ASM_MASK_USR_AXON_DATA_INFO (1 << 2)
/*lint -esym(621, OKL4_ASM_SHIFT_USR_AXON_DATA_INFO) */
#define OKL4_ASM_SHIFT_USR_AXON_DATA_INFO (2)
/*lint -esym(621, OKL4_ASM_WIDTH_USR_AXON_DATA_INFO) */
#define OKL4_ASM_WIDTH_USR_AXON_DATA_INFO (1)
/*lint -esym(621, OKL4_ASM_MASK_LADDR_AXON_DATA_INFO) */
#define OKL4_ASM_MASK_LADDR_AXON_DATA_INFO (2305843009213693951 << 3)
/*lint -esym(621, OKL4_ASM_SHIFT_LADDR_AXON_DATA_INFO) */
#define OKL4_ASM_SHIFT_LADDR_AXON_DATA_INFO (3)
/*lint -esym(621, OKL4_ASM_PRESHIFT_LADDR_AXON_DATA_INFO) */
#define OKL4_ASM_PRESHIFT_LADDR_AXON_DATA_INFO (3)
/*lint -esym(621, OKL4_ASM_WIDTH_LADDR_AXON_DATA_INFO) */
#define OKL4_ASM_WIDTH_LADDR_AXON_DATA_INFO (61)


/**
 *  okl4_axon_queue_size_t
 **/


/*lint -esym(621, OKL4_ASM_MASK_ALLOC_ORDER_AXON_QUEUE_SIZE) */
#define OKL4_ASM_MASK_ALLOC_ORDER_AXON_QUEUE_SIZE (31)
/*lint -esym(621, OKL4_ASM_SHIFT_ALLOC_ORDER_AXON_QUEUE_SIZE) */
#define OKL4_ASM_SHIFT_ALLOC_ORDER_AXON_QUEUE_SIZE (0)
/*lint -esym(621, OKL4_ASM_WIDTH_ALLOC_ORDER_AXON_QUEUE_SIZE) */
#define OKL4_ASM_WIDTH_ALLOC_ORDER_AXON_QUEUE_SIZE (5)
/*lint -esym(621, OKL4_ASM_MASK_MIN_ORDER_AXON_QUEUE_SIZE) */
#define OKL4_ASM_MASK_MIN_ORDER_AXON_QUEUE_SIZE (31 << 8)
/*lint -esym(621, OKL4_ASM_SHIFT_MIN_ORDER_AXON_QUEUE_SIZE) */
#define OKL4_ASM_SHIFT_MIN_ORDER_AXON_QUEUE_SIZE (8)
/*lint -esym(621, OKL4_ASM_WIDTH_MIN_ORDER_AXON_QUEUE_SIZE) */
#define OKL4_ASM_WIDTH_MIN_ORDER_AXON_QUEUE_SIZE (5)


/**
 *  okl4_axon_virq_flags_t
 **/


/*lint -esym(621, OKL4_ASM_MASK_READY_AXON_VIRQ_FLAGS) */
#define OKL4_ASM_MASK_READY_AXON_VIRQ_FLAGS (1)
/*lint -esym(621, OKL4_ASM_SHIFT_READY_AXON_VIRQ_FLAGS) */
#define OKL4_ASM_SHIFT_READY_AXON_VIRQ_FLAGS (0)
/*lint -esym(621, OKL4_ASM_WIDTH_READY_AXON_VIRQ_FLAGS) */
#define OKL4_ASM_WIDTH_READY_AXON_VIRQ_FLAGS (1)
/*lint -esym(621, OKL4_ASM_MASK_FAULT_AXON_VIRQ_FLAGS) */
#define OKL4_ASM_MASK_FAULT_AXON_VIRQ_FLAGS (1 << 1)
/*lint -esym(621, OKL4_ASM_SHIFT_FAULT_AXON_VIRQ_FLAGS) */
#define OKL4_ASM_SHIFT_FAULT_AXON_VIRQ_FLAGS (1)
/*lint -esym(621, OKL4_ASM_WIDTH_FAULT_AXON_VIRQ_FLAGS) */
#define OKL4_ASM_WIDTH_FAULT_AXON_VIRQ_FLAGS (1)


/**
 *  okl4_page_cache_t
 **/
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_WRITECOMBINE) */
#define OKL4_ASM_PAGE_CACHE_WRITECOMBINE (0x0)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_DEFAULT) */
#define OKL4_ASM_PAGE_CACHE_DEFAULT (0x1)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_IPC_RX) */
#define OKL4_ASM_PAGE_CACHE_IPC_RX (0x1)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_IPC_TX) */
#define OKL4_ASM_PAGE_CACHE_IPC_TX (0x1)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_TRACEBUFFER) */
#define OKL4_ASM_PAGE_CACHE_TRACEBUFFER (0x1)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_WRITEBACK) */
#define OKL4_ASM_PAGE_CACHE_WRITEBACK (0x1)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_IWB_RWA_ONC) */
#define OKL4_ASM_PAGE_CACHE_IWB_RWA_ONC (0x2)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_WRITETHROUGH) */
#define OKL4_ASM_PAGE_CACHE_WRITETHROUGH (0x3)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_DEVICE_GRE) */
#define OKL4_ASM_PAGE_CACHE_DEVICE_GRE (0x4)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_DEVICE_NGRE) */
#define OKL4_ASM_PAGE_CACHE_DEVICE_NGRE (0x5)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_DEVICE) */
#define OKL4_ASM_PAGE_CACHE_DEVICE (0x6)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_STRONG) */
#define OKL4_ASM_PAGE_CACHE_STRONG (0x7)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_DEVICE_NGNRNE) */
#define OKL4_ASM_PAGE_CACHE_HW_DEVICE_NGNRNE (0x8000000)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_MASK) */
#define OKL4_ASM_PAGE_CACHE_HW_MASK (0x8000000)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_DEVICE_NGNRE) */
#define OKL4_ASM_PAGE_CACHE_HW_DEVICE_NGNRE (0x8000004)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_DEVICE_NGRE) */
#define OKL4_ASM_PAGE_CACHE_HW_DEVICE_NGRE (0x8000008)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_DEVICE_GRE) */
#define OKL4_ASM_PAGE_CACHE_HW_DEVICE_GRE (0x800000c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_TWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_TWT_WA_NSH (0x8000011)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWT_WA_NSH (0x8000012)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWT_WA_NSH (0x8000013)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OTWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OTWT_WA_NSH (0x8000014)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWT_WA_NSH (0x8000015)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWT_WA_NSH (0x8000016)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWT_WA_NSH (0x8000017)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWT_WA_NSH (0x8000018)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWT_WA_NSH (0x8000019)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWT_WA_NSH (0x800001a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWT_WA_NSH (0x800001b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWT_WA_NSH (0x800001c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWT_WA_NSH (0x800001d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWT_WA_NSH (0x800001e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWT_WA_NSH (0x800001f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWT_RA_NSH (0x8000021)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_TWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_TWT_RA_NSH (0x8000022)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWT_RA_NSH (0x8000023)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OTWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OTWT_RA_NSH (0x8000024)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWT_RA_NSH (0x8000025)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWT_RA_NSH (0x8000026)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWT_RA_NSH (0x8000027)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWT_RA_NSH (0x8000028)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWT_RA_NSH (0x8000029)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWT_RA_NSH (0x800002a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWT_RA_NSH (0x800002b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWT_RA_NSH (0x800002c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWT_RA_NSH (0x800002d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWT_RA_NSH (0x800002e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWT_RA_NSH (0x800002f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWT_RWA_NSH (0x8000031)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWT_RWA_NSH (0x8000032)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_TWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_TWT_RWA_NSH (0x8000033)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OTWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OTWT_RWA_NSH (0x8000034)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWT_RWA_NSH (0x8000035)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWT_RWA_NSH (0x8000036)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWT_RWA_NSH (0x8000037)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWT_RWA_NSH (0x8000038)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWT_RWA_NSH (0x8000039)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWT_RWA_NSH (0x800003a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWT_RWA_NSH (0x800003b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWT_RWA_NSH (0x800003c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWT_RWA_NSH (0x800003d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWT_RWA_NSH (0x800003e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWT_RWA_NSH (0x800003f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_ONC_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_ONC_NSH (0x8000041)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_ONC_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_ONC_NSH (0x8000042)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_ONC_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_ONC_NSH (0x8000043)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_NC_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_NC_NSH (0x8000044)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_ONC_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_ONC_NSH (0x8000045)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_ONC_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_ONC_NSH (0x8000046)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_ONC_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_ONC_NSH (0x8000047)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_ONC_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_ONC_NSH (0x8000048)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_ONC_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_ONC_NSH (0x8000049)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_ONC_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_ONC_NSH (0x800004a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_ONC_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_ONC_NSH (0x800004b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_ONC_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_ONC_NSH (0x800004c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_ONC_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_ONC_NSH (0x800004d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_ONC_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_ONC_NSH (0x800004e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_ONC_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_ONC_NSH (0x800004f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWB_WA_NSH (0x8000051)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWB_WA_NSH (0x8000052)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWB_WA_NSH (0x8000053)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OTWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OTWB_WA_NSH (0x8000054)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_TWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_TWB_WA_NSH (0x8000055)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWB_WA_NSH (0x8000056)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWB_WA_NSH (0x8000057)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWB_WA_NSH (0x8000058)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWB_WA_NSH (0x8000059)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWB_WA_NSH (0x800005a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWB_WA_NSH (0x800005b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWB_WA_NSH (0x800005c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWB_WA_NSH (0x800005d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWB_WA_NSH (0x800005e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWB_WA_NSH (0x800005f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWB_RA_NSH (0x8000061)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWB_RA_NSH (0x8000062)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWB_RA_NSH (0x8000063)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OTWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OTWB_RA_NSH (0x8000064)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWB_RA_NSH (0x8000065)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_TWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_TWB_RA_NSH (0x8000066)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWB_RA_NSH (0x8000067)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWB_RA_NSH (0x8000068)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWB_RA_NSH (0x8000069)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWB_RA_NSH (0x800006a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWB_RA_NSH (0x800006b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWB_RA_NSH (0x800006c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWB_RA_NSH (0x800006d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWB_RA_NSH (0x800006e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWB_RA_NSH (0x800006f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWB_RWA_NSH (0x8000071)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWB_RWA_NSH (0x8000072)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWB_RWA_NSH (0x8000073)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OTWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OTWB_RWA_NSH (0x8000074)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWB_RWA_NSH (0x8000075)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWB_RWA_NSH (0x8000076)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_TWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_TWB_RWA_NSH (0x8000077)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWB_RWA_NSH (0x8000078)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWB_RWA_NSH (0x8000079)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWB_RWA_NSH (0x800007a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWB_RWA_NSH (0x800007b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWB_RWA_NSH (0x800007c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWB_RWA_NSH (0x800007d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWB_RWA_NSH (0x800007e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWB_RWA_NSH (0x800007f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWT_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWT_NA_NSH (0x8000081)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWT_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWT_NA_NSH (0x8000082)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWT_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWT_NA_NSH (0x8000083)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OWT_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OWT_NA_NSH (0x8000084)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWT_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWT_NA_NSH (0x8000085)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWT_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWT_NA_NSH (0x8000086)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWT_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWT_NA_NSH (0x8000087)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_WT_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_WT_NA_NSH (0x8000088)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWT_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWT_NA_NSH (0x8000089)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWT_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWT_NA_NSH (0x800008a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWT_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWT_NA_NSH (0x800008b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWT_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWT_NA_NSH (0x800008c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWT_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWT_NA_NSH (0x800008d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWT_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWT_NA_NSH (0x800008e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWT_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWT_NA_NSH (0x800008f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWT_WA_NSH (0x8000091)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWT_WA_NSH (0x8000092)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWT_WA_NSH (0x8000093)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OWT_WA_NSH (0x8000094)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWT_WA_NSH (0x8000095)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWT_WA_NSH (0x8000096)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWT_WA_NSH (0x8000097)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWT_WA_NSH (0x8000098)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_WT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_WT_WA_NSH (0x8000099)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWT_WA_NSH (0x800009a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWT_WA_NSH (0x800009b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWT_WA_NSH (0x800009c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWT_WA_NSH (0x800009d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWT_WA_NSH (0x800009e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWT_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWT_WA_NSH (0x800009f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWT_RA_NSH (0x80000a1)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWT_RA_NSH (0x80000a2)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWT_RA_NSH (0x80000a3)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OWT_RA_NSH (0x80000a4)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWT_RA_NSH (0x80000a5)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWT_RA_NSH (0x80000a6)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWT_RA_NSH (0x80000a7)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWT_RA_NSH (0x80000a8)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWT_RA_NSH (0x80000a9)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_WT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_WT_RA_NSH (0x80000aa)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWT_RA_NSH (0x80000ab)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWT_RA_NSH (0x80000ac)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWT_RA_NSH (0x80000ad)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWT_RA_NSH (0x80000ae)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWT_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWT_RA_NSH (0x80000af)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWT_RWA_NSH (0x80000b1)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWT_RWA_NSH (0x80000b2)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWT_RWA_NSH (0x80000b3)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OWT_RWA_NSH (0x80000b4)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWT_RWA_NSH (0x80000b5)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWT_RWA_NSH (0x80000b6)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWT_RWA_NSH (0x80000b7)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWT_RWA_NSH (0x80000b8)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWT_RWA_NSH (0x80000b9)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWT_RWA_NSH (0x80000ba)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_WT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_WT_RWA_NSH (0x80000bb)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWT_RWA_NSH (0x80000bc)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWT_RWA_NSH (0x80000bd)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWT_RWA_NSH (0x80000be)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWT_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWT_RWA_NSH (0x80000bf)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWB_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWB_NA_NSH (0x80000c1)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWB_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWB_NA_NSH (0x80000c2)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWB_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWB_NA_NSH (0x80000c3)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OWB_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OWB_NA_NSH (0x80000c4)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWB_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWB_NA_NSH (0x80000c5)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWB_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWB_NA_NSH (0x80000c6)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWB_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWB_NA_NSH (0x80000c7)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWB_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWB_NA_NSH (0x80000c8)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWB_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWB_NA_NSH (0x80000c9)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWB_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWB_NA_NSH (0x80000ca)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWB_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWB_NA_NSH (0x80000cb)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_WB_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_WB_NA_NSH (0x80000cc)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWB_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWB_NA_NSH (0x80000cd)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWB_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWB_NA_NSH (0x80000ce)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWB_NA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWB_NA_NSH (0x80000cf)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWB_WA_NSH (0x80000d1)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWB_WA_NSH (0x80000d2)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWB_WA_NSH (0x80000d3)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OWB_WA_NSH (0x80000d4)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWB_WA_NSH (0x80000d5)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWB_WA_NSH (0x80000d6)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWB_WA_NSH (0x80000d7)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWB_WA_NSH (0x80000d8)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWB_WA_NSH (0x80000d9)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWB_WA_NSH (0x80000da)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWB_WA_NSH (0x80000db)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWB_WA_NSH (0x80000dc)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_WB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_WB_WA_NSH (0x80000dd)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWB_WA_NSH (0x80000de)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWB_WA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWB_WA_NSH (0x80000df)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWB_RA_NSH (0x80000e1)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWB_RA_NSH (0x80000e2)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWB_RA_NSH (0x80000e3)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OWB_RA_NSH (0x80000e4)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWB_RA_NSH (0x80000e5)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWB_RA_NSH (0x80000e6)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWB_RA_NSH (0x80000e7)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWB_RA_NSH (0x80000e8)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWB_RA_NSH (0x80000e9)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWB_RA_NSH (0x80000ea)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWB_RA_NSH (0x80000eb)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWB_RA_NSH (0x80000ec)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWB_RA_NSH (0x80000ed)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_WB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_WB_RA_NSH (0x80000ee)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWB_RA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWB_RA_NSH (0x80000ef)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWB_RWA_NSH (0x80000f1)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWB_RWA_NSH (0x80000f2)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWB_RWA_NSH (0x80000f3)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OWB_RWA_NSH (0x80000f4)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWB_RWA_NSH (0x80000f5)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWB_RWA_NSH (0x80000f6)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWB_RWA_NSH (0x80000f7)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWB_RWA_NSH (0x80000f8)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWB_RWA_NSH (0x80000f9)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWB_RWA_NSH (0x80000fa)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWB_RWA_NSH (0x80000fb)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWB_RWA_NSH (0x80000fc)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWB_RWA_NSH (0x80000fd)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWB_RWA_NSH (0x80000fe)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_WB_RWA_NSH) */
#define OKL4_ASM_PAGE_CACHE_HW_WB_RWA_NSH (0x80000ff)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_TWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_TWT_WA_OSH (0x8000211)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWT_WA_OSH (0x8000212)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWT_WA_OSH (0x8000213)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OTWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OTWT_WA_OSH (0x8000214)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWT_WA_OSH (0x8000215)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWT_WA_OSH (0x8000216)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWT_WA_OSH (0x8000217)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWT_WA_OSH (0x8000218)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWT_WA_OSH (0x8000219)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWT_WA_OSH (0x800021a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWT_WA_OSH (0x800021b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWT_WA_OSH (0x800021c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWT_WA_OSH (0x800021d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWT_WA_OSH (0x800021e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWT_WA_OSH (0x800021f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWT_RA_OSH (0x8000221)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_TWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_TWT_RA_OSH (0x8000222)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWT_RA_OSH (0x8000223)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OTWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OTWT_RA_OSH (0x8000224)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWT_RA_OSH (0x8000225)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWT_RA_OSH (0x8000226)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWT_RA_OSH (0x8000227)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWT_RA_OSH (0x8000228)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWT_RA_OSH (0x8000229)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWT_RA_OSH (0x800022a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWT_RA_OSH (0x800022b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWT_RA_OSH (0x800022c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWT_RA_OSH (0x800022d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWT_RA_OSH (0x800022e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWT_RA_OSH (0x800022f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWT_RWA_OSH (0x8000231)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWT_RWA_OSH (0x8000232)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_TWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_TWT_RWA_OSH (0x8000233)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OTWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OTWT_RWA_OSH (0x8000234)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWT_RWA_OSH (0x8000235)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWT_RWA_OSH (0x8000236)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWT_RWA_OSH (0x8000237)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWT_RWA_OSH (0x8000238)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWT_RWA_OSH (0x8000239)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWT_RWA_OSH (0x800023a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWT_RWA_OSH (0x800023b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWT_RWA_OSH (0x800023c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWT_RWA_OSH (0x800023d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWT_RWA_OSH (0x800023e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWT_RWA_OSH (0x800023f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_ONC_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_ONC_OSH (0x8000241)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_ONC_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_ONC_OSH (0x8000242)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_ONC_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_ONC_OSH (0x8000243)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_NC_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_NC_OSH (0x8000244)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_ONC_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_ONC_OSH (0x8000245)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_ONC_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_ONC_OSH (0x8000246)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_ONC_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_ONC_OSH (0x8000247)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_ONC_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_ONC_OSH (0x8000248)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_ONC_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_ONC_OSH (0x8000249)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_ONC_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_ONC_OSH (0x800024a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_ONC_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_ONC_OSH (0x800024b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_ONC_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_ONC_OSH (0x800024c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_ONC_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_ONC_OSH (0x800024d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_ONC_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_ONC_OSH (0x800024e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_ONC_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_ONC_OSH (0x800024f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWB_WA_OSH (0x8000251)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWB_WA_OSH (0x8000252)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWB_WA_OSH (0x8000253)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OTWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OTWB_WA_OSH (0x8000254)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_TWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_TWB_WA_OSH (0x8000255)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWB_WA_OSH (0x8000256)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWB_WA_OSH (0x8000257)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWB_WA_OSH (0x8000258)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWB_WA_OSH (0x8000259)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWB_WA_OSH (0x800025a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWB_WA_OSH (0x800025b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWB_WA_OSH (0x800025c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWB_WA_OSH (0x800025d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWB_WA_OSH (0x800025e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWB_WA_OSH (0x800025f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWB_RA_OSH (0x8000261)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWB_RA_OSH (0x8000262)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWB_RA_OSH (0x8000263)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OTWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OTWB_RA_OSH (0x8000264)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWB_RA_OSH (0x8000265)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_TWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_TWB_RA_OSH (0x8000266)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWB_RA_OSH (0x8000267)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWB_RA_OSH (0x8000268)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWB_RA_OSH (0x8000269)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWB_RA_OSH (0x800026a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWB_RA_OSH (0x800026b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWB_RA_OSH (0x800026c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWB_RA_OSH (0x800026d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWB_RA_OSH (0x800026e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWB_RA_OSH (0x800026f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWB_RWA_OSH (0x8000271)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWB_RWA_OSH (0x8000272)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWB_RWA_OSH (0x8000273)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OTWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OTWB_RWA_OSH (0x8000274)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWB_RWA_OSH (0x8000275)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWB_RWA_OSH (0x8000276)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_TWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_TWB_RWA_OSH (0x8000277)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWB_RWA_OSH (0x8000278)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWB_RWA_OSH (0x8000279)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWB_RWA_OSH (0x800027a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWB_RWA_OSH (0x800027b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWB_RWA_OSH (0x800027c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWB_RWA_OSH (0x800027d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWB_RWA_OSH (0x800027e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWB_RWA_OSH (0x800027f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWT_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWT_NA_OSH (0x8000281)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWT_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWT_NA_OSH (0x8000282)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWT_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWT_NA_OSH (0x8000283)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OWT_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OWT_NA_OSH (0x8000284)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWT_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWT_NA_OSH (0x8000285)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWT_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWT_NA_OSH (0x8000286)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWT_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWT_NA_OSH (0x8000287)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_WT_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_WT_NA_OSH (0x8000288)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWT_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWT_NA_OSH (0x8000289)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWT_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWT_NA_OSH (0x800028a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWT_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWT_NA_OSH (0x800028b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWT_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWT_NA_OSH (0x800028c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWT_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWT_NA_OSH (0x800028d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWT_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWT_NA_OSH (0x800028e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWT_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWT_NA_OSH (0x800028f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWT_WA_OSH (0x8000291)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWT_WA_OSH (0x8000292)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWT_WA_OSH (0x8000293)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OWT_WA_OSH (0x8000294)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWT_WA_OSH (0x8000295)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWT_WA_OSH (0x8000296)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWT_WA_OSH (0x8000297)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWT_WA_OSH (0x8000298)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_WT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_WT_WA_OSH (0x8000299)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWT_WA_OSH (0x800029a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWT_WA_OSH (0x800029b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWT_WA_OSH (0x800029c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWT_WA_OSH (0x800029d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWT_WA_OSH (0x800029e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWT_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWT_WA_OSH (0x800029f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWT_RA_OSH (0x80002a1)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWT_RA_OSH (0x80002a2)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWT_RA_OSH (0x80002a3)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OWT_RA_OSH (0x80002a4)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWT_RA_OSH (0x80002a5)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWT_RA_OSH (0x80002a6)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWT_RA_OSH (0x80002a7)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWT_RA_OSH (0x80002a8)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWT_RA_OSH (0x80002a9)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_WT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_WT_RA_OSH (0x80002aa)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWT_RA_OSH (0x80002ab)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWT_RA_OSH (0x80002ac)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWT_RA_OSH (0x80002ad)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWT_RA_OSH (0x80002ae)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWT_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWT_RA_OSH (0x80002af)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWT_RWA_OSH (0x80002b1)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWT_RWA_OSH (0x80002b2)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWT_RWA_OSH (0x80002b3)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OWT_RWA_OSH (0x80002b4)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWT_RWA_OSH (0x80002b5)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWT_RWA_OSH (0x80002b6)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWT_RWA_OSH (0x80002b7)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWT_RWA_OSH (0x80002b8)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWT_RWA_OSH (0x80002b9)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWT_RWA_OSH (0x80002ba)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_WT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_WT_RWA_OSH (0x80002bb)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWT_RWA_OSH (0x80002bc)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWT_RWA_OSH (0x80002bd)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWT_RWA_OSH (0x80002be)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWT_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWT_RWA_OSH (0x80002bf)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWB_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWB_NA_OSH (0x80002c1)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWB_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWB_NA_OSH (0x80002c2)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWB_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWB_NA_OSH (0x80002c3)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OWB_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OWB_NA_OSH (0x80002c4)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWB_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWB_NA_OSH (0x80002c5)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWB_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWB_NA_OSH (0x80002c6)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWB_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWB_NA_OSH (0x80002c7)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWB_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWB_NA_OSH (0x80002c8)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWB_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWB_NA_OSH (0x80002c9)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWB_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWB_NA_OSH (0x80002ca)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWB_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWB_NA_OSH (0x80002cb)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_WB_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_WB_NA_OSH (0x80002cc)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWB_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWB_NA_OSH (0x80002cd)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWB_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWB_NA_OSH (0x80002ce)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWB_NA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWB_NA_OSH (0x80002cf)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWB_WA_OSH (0x80002d1)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWB_WA_OSH (0x80002d2)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWB_WA_OSH (0x80002d3)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OWB_WA_OSH (0x80002d4)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWB_WA_OSH (0x80002d5)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWB_WA_OSH (0x80002d6)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWB_WA_OSH (0x80002d7)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWB_WA_OSH (0x80002d8)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWB_WA_OSH (0x80002d9)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWB_WA_OSH (0x80002da)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWB_WA_OSH (0x80002db)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWB_WA_OSH (0x80002dc)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_WB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_WB_WA_OSH (0x80002dd)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWB_WA_OSH (0x80002de)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWB_WA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWB_WA_OSH (0x80002df)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWB_RA_OSH (0x80002e1)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWB_RA_OSH (0x80002e2)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWB_RA_OSH (0x80002e3)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OWB_RA_OSH (0x80002e4)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWB_RA_OSH (0x80002e5)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWB_RA_OSH (0x80002e6)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWB_RA_OSH (0x80002e7)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWB_RA_OSH (0x80002e8)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWB_RA_OSH (0x80002e9)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWB_RA_OSH (0x80002ea)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWB_RA_OSH (0x80002eb)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWB_RA_OSH (0x80002ec)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWB_RA_OSH (0x80002ed)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_WB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_WB_RA_OSH (0x80002ee)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWB_RA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWB_RA_OSH (0x80002ef)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWB_RWA_OSH (0x80002f1)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWB_RWA_OSH (0x80002f2)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWB_RWA_OSH (0x80002f3)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OWB_RWA_OSH (0x80002f4)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWB_RWA_OSH (0x80002f5)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWB_RWA_OSH (0x80002f6)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWB_RWA_OSH (0x80002f7)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWB_RWA_OSH (0x80002f8)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWB_RWA_OSH (0x80002f9)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWB_RWA_OSH (0x80002fa)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWB_RWA_OSH (0x80002fb)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWB_RWA_OSH (0x80002fc)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWB_RWA_OSH (0x80002fd)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWB_RWA_OSH (0x80002fe)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_WB_RWA_OSH) */
#define OKL4_ASM_PAGE_CACHE_HW_WB_RWA_OSH (0x80002ff)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_TWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_TWT_WA_ISH (0x8000311)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWT_WA_ISH (0x8000312)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWT_WA_ISH (0x8000313)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OTWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OTWT_WA_ISH (0x8000314)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWT_WA_ISH (0x8000315)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWT_WA_ISH (0x8000316)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWT_WA_ISH (0x8000317)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWT_WA_ISH (0x8000318)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWT_WA_ISH (0x8000319)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWT_WA_ISH (0x800031a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWT_WA_ISH (0x800031b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWT_WA_ISH (0x800031c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWT_WA_ISH (0x800031d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWT_WA_ISH (0x800031e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWT_WA_ISH (0x800031f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWT_RA_ISH (0x8000321)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_TWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_TWT_RA_ISH (0x8000322)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWT_RA_ISH (0x8000323)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OTWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OTWT_RA_ISH (0x8000324)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWT_RA_ISH (0x8000325)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWT_RA_ISH (0x8000326)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWT_RA_ISH (0x8000327)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWT_RA_ISH (0x8000328)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWT_RA_ISH (0x8000329)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWT_RA_ISH (0x800032a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWT_RA_ISH (0x800032b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWT_RA_ISH (0x800032c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWT_RA_ISH (0x800032d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWT_RA_ISH (0x800032e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWT_RA_ISH (0x800032f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWT_RWA_ISH (0x8000331)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWT_RWA_ISH (0x8000332)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_TWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_TWT_RWA_ISH (0x8000333)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OTWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OTWT_RWA_ISH (0x8000334)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWT_RWA_ISH (0x8000335)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWT_RWA_ISH (0x8000336)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWT_RWA_ISH (0x8000337)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWT_RWA_ISH (0x8000338)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWT_RWA_ISH (0x8000339)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWT_RWA_ISH (0x800033a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWT_RWA_ISH (0x800033b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWT_RWA_ISH (0x800033c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWT_RWA_ISH (0x800033d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWT_RWA_ISH (0x800033e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWT_RWA_ISH (0x800033f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_ONC_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_ONC_ISH (0x8000341)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_ONC_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_ONC_ISH (0x8000342)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_ONC_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_ONC_ISH (0x8000343)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_NC_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_NC_ISH (0x8000344)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_ONC_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_ONC_ISH (0x8000345)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_ONC_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_ONC_ISH (0x8000346)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_ONC_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_ONC_ISH (0x8000347)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_ONC_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_ONC_ISH (0x8000348)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_ONC_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_ONC_ISH (0x8000349)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_ONC_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_ONC_ISH (0x800034a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_ONC_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_ONC_ISH (0x800034b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_ONC_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_ONC_ISH (0x800034c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_ONC_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_ONC_ISH (0x800034d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_ONC_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_ONC_ISH (0x800034e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_ONC_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_ONC_ISH (0x800034f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWB_WA_ISH (0x8000351)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWB_WA_ISH (0x8000352)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWB_WA_ISH (0x8000353)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OTWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OTWB_WA_ISH (0x8000354)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_TWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_TWB_WA_ISH (0x8000355)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWB_WA_ISH (0x8000356)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWB_WA_ISH (0x8000357)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWB_WA_ISH (0x8000358)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWB_WA_ISH (0x8000359)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWB_WA_ISH (0x800035a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWB_WA_ISH (0x800035b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWB_WA_ISH (0x800035c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWB_WA_ISH (0x800035d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWB_WA_ISH (0x800035e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWB_WA_ISH (0x800035f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWB_RA_ISH (0x8000361)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWB_RA_ISH (0x8000362)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWB_RA_ISH (0x8000363)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OTWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OTWB_RA_ISH (0x8000364)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWB_RA_ISH (0x8000365)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_TWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_TWB_RA_ISH (0x8000366)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OTWB_RA_ISH (0x8000367)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWB_RA_ISH (0x8000368)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWB_RA_ISH (0x8000369)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWB_RA_ISH (0x800036a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWB_RA_ISH (0x800036b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWB_RA_ISH (0x800036c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWB_RA_ISH (0x800036d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWB_RA_ISH (0x800036e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWB_RA_ISH (0x800036f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OTWB_RWA_ISH (0x8000371)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OTWB_RWA_ISH (0x8000372)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OTWB_RWA_ISH (0x8000373)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OTWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OTWB_RWA_ISH (0x8000374)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OTWB_RWA_ISH (0x8000375)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OTWB_RWA_ISH (0x8000376)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_TWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_TWB_RWA_ISH (0x8000377)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OTWB_RWA_ISH (0x8000378)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OTWB_RWA_ISH (0x8000379)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OTWB_RWA_ISH (0x800037a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OTWB_RWA_ISH (0x800037b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OTWB_RWA_ISH (0x800037c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OTWB_RWA_ISH (0x800037d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OTWB_RWA_ISH (0x800037e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OTWB_RWA_ISH (0x800037f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWT_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWT_NA_ISH (0x8000381)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWT_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWT_NA_ISH (0x8000382)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWT_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWT_NA_ISH (0x8000383)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OWT_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OWT_NA_ISH (0x8000384)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWT_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWT_NA_ISH (0x8000385)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWT_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWT_NA_ISH (0x8000386)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWT_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWT_NA_ISH (0x8000387)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_WT_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_WT_NA_ISH (0x8000388)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWT_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWT_NA_ISH (0x8000389)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWT_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWT_NA_ISH (0x800038a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWT_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWT_NA_ISH (0x800038b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWT_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWT_NA_ISH (0x800038c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWT_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWT_NA_ISH (0x800038d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWT_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWT_NA_ISH (0x800038e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWT_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWT_NA_ISH (0x800038f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWT_WA_ISH (0x8000391)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWT_WA_ISH (0x8000392)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWT_WA_ISH (0x8000393)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OWT_WA_ISH (0x8000394)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWT_WA_ISH (0x8000395)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWT_WA_ISH (0x8000396)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWT_WA_ISH (0x8000397)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWT_WA_ISH (0x8000398)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_WT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_WT_WA_ISH (0x8000399)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWT_WA_ISH (0x800039a)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWT_WA_ISH (0x800039b)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWT_WA_ISH (0x800039c)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWT_WA_ISH (0x800039d)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWT_WA_ISH (0x800039e)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWT_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWT_WA_ISH (0x800039f)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWT_RA_ISH (0x80003a1)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWT_RA_ISH (0x80003a2)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWT_RA_ISH (0x80003a3)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OWT_RA_ISH (0x80003a4)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWT_RA_ISH (0x80003a5)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWT_RA_ISH (0x80003a6)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWT_RA_ISH (0x80003a7)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWT_RA_ISH (0x80003a8)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWT_RA_ISH (0x80003a9)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_WT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_WT_RA_ISH (0x80003aa)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWT_RA_ISH (0x80003ab)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWT_RA_ISH (0x80003ac)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWT_RA_ISH (0x80003ad)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWT_RA_ISH (0x80003ae)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWT_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWT_RA_ISH (0x80003af)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWT_RWA_ISH (0x80003b1)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWT_RWA_ISH (0x80003b2)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWT_RWA_ISH (0x80003b3)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OWT_RWA_ISH (0x80003b4)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWT_RWA_ISH (0x80003b5)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWT_RWA_ISH (0x80003b6)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWT_RWA_ISH (0x80003b7)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWT_RWA_ISH (0x80003b8)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWT_RWA_ISH (0x80003b9)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWT_RWA_ISH (0x80003ba)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_WT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_WT_RWA_ISH (0x80003bb)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWT_RWA_ISH (0x80003bc)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWT_RWA_ISH (0x80003bd)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWT_RWA_ISH (0x80003be)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWT_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWT_RWA_ISH (0x80003bf)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWB_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWB_NA_ISH (0x80003c1)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWB_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWB_NA_ISH (0x80003c2)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWB_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWB_NA_ISH (0x80003c3)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OWB_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OWB_NA_ISH (0x80003c4)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWB_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWB_NA_ISH (0x80003c5)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWB_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWB_NA_ISH (0x80003c6)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWB_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWB_NA_ISH (0x80003c7)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWB_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWB_NA_ISH (0x80003c8)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWB_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWB_NA_ISH (0x80003c9)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWB_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWB_NA_ISH (0x80003ca)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWB_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWB_NA_ISH (0x80003cb)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_WB_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_WB_NA_ISH (0x80003cc)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWB_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWB_NA_ISH (0x80003cd)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWB_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWB_NA_ISH (0x80003ce)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWB_NA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWB_NA_ISH (0x80003cf)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWB_WA_ISH (0x80003d1)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWB_WA_ISH (0x80003d2)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWB_WA_ISH (0x80003d3)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OWB_WA_ISH (0x80003d4)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWB_WA_ISH (0x80003d5)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWB_WA_ISH (0x80003d6)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWB_WA_ISH (0x80003d7)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWB_WA_ISH (0x80003d8)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWB_WA_ISH (0x80003d9)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWB_WA_ISH (0x80003da)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWB_WA_ISH (0x80003db)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWB_WA_ISH (0x80003dc)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_WB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_WB_WA_ISH (0x80003dd)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWB_WA_ISH (0x80003de)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWB_WA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWB_WA_ISH (0x80003df)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWB_RA_ISH (0x80003e1)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWB_RA_ISH (0x80003e2)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWB_RA_ISH (0x80003e3)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OWB_RA_ISH (0x80003e4)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWB_RA_ISH (0x80003e5)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWB_RA_ISH (0x80003e6)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWB_RA_ISH (0x80003e7)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWB_RA_ISH (0x80003e8)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWB_RA_ISH (0x80003e9)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWB_RA_ISH (0x80003ea)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWB_RA_ISH (0x80003eb)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWB_RA_ISH (0x80003ec)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWB_RA_ISH (0x80003ed)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_WB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_WB_RA_ISH (0x80003ee)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWB_RA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RWA_OWB_RA_ISH (0x80003ef)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_WA_OWB_RWA_ISH (0x80003f1)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RA_OWB_RWA_ISH (0x80003f2)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWT_RWA_OWB_RWA_ISH (0x80003f3)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_INC_OWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_INC_OWB_RWA_ISH (0x80003f4)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_WA_OWB_RWA_ISH (0x80003f5)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RA_OWB_RWA_ISH (0x80003f6)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_ITWB_RWA_OWB_RWA_ISH (0x80003f7)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_NA_OWB_RWA_ISH (0x80003f8)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_WA_OWB_RWA_ISH (0x80003f9)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RA_OWB_RWA_ISH (0x80003fa)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWT_RWA_OWB_RWA_ISH (0x80003fb)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_NA_OWB_RWA_ISH (0x80003fc)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_WA_OWB_RWA_ISH (0x80003fd)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_IWB_RA_OWB_RWA_ISH (0x80003fe)
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_HW_WB_RWA_ISH) */
#define OKL4_ASM_PAGE_CACHE_HW_WB_RWA_ISH (0x80003ff)
/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_MAX) */
#define OKL4_ASM_PAGE_CACHE_MAX (0x80003ff)
/**
    Invalid enumeration value
*/
/*lint -esym(621, OKL4_ASM_PAGE_CACHE_INVALID) */
#define OKL4_ASM_PAGE_CACHE_INVALID (0xffffffff)

/**
 *  okl4_cpu_exec_mode
 **/
/*lint -esym(621, OKL4_ARM_MODE) */
#define OKL4_ARM_MODE (0)

/*lint -esym(621, OKL4_DEFAULT_MODE) */
#define OKL4_DEFAULT_MODE (4)

/*lint -esym(621, OKL4_JAZELLE_MODE) */
#define OKL4_JAZELLE_MODE (2)

/*lint -esym(621, OKL4_THUMBEE_MODE) */
#define OKL4_THUMBEE_MODE (3)

/*lint -esym(621, OKL4_THUMB_MODE) */
#define OKL4_THUMB_MODE (1)

/**
 *  okl4_cpu_mode_t
 **/


/*lint -esym(621, OKL4_ASM_MASK_EXEC_MODE_CPU_MODE) */
#define OKL4_ASM_MASK_EXEC_MODE_CPU_MODE (7)
/*lint -esym(621, OKL4_ASM_SHIFT_EXEC_MODE_CPU_MODE) */
#define OKL4_ASM_SHIFT_EXEC_MODE_CPU_MODE (0)
/*lint -esym(621, OKL4_ASM_WIDTH_EXEC_MODE_CPU_MODE) */
#define OKL4_ASM_WIDTH_EXEC_MODE_CPU_MODE (3)
/*lint -esym(621, OKL4_ASM_MASK_ENDIAN_CPU_MODE) */
#define OKL4_ASM_MASK_ENDIAN_CPU_MODE (1 << 7)
/*lint -esym(621, OKL4_ASM_SHIFT_ENDIAN_CPU_MODE) */
#define OKL4_ASM_SHIFT_ENDIAN_CPU_MODE (7)
/*lint -esym(621, OKL4_ASM_WIDTH_ENDIAN_CPU_MODE) */
#define OKL4_ASM_WIDTH_ENDIAN_CPU_MODE (1)


/**
 *  okl4_page_perms_t
 **/
/*lint -esym(621, OKL4_ASM_PAGE_PERMS_NONE) */
#define OKL4_ASM_PAGE_PERMS_NONE (0x0)
/*lint -esym(621, OKL4_ASM_PAGE_PERMS_X) */
#define OKL4_ASM_PAGE_PERMS_X (0x1)
/*lint -esym(621, OKL4_ASM_PAGE_PERMS_W) */
#define OKL4_ASM_PAGE_PERMS_W (0x2)
/*lint -esym(621, OKL4_ASM_PAGE_PERMS_WX) */
#define OKL4_ASM_PAGE_PERMS_WX (0x3)
/*lint -esym(621, OKL4_ASM_PAGE_PERMS_R) */
#define OKL4_ASM_PAGE_PERMS_R (0x4)
/*lint -esym(621, OKL4_ASM_PAGE_PERMS_RX) */
#define OKL4_ASM_PAGE_PERMS_RX (0x5)
/*lint -esym(621, OKL4_ASM_PAGE_PERMS_RW) */
#define OKL4_ASM_PAGE_PERMS_RW (0x6)
/*lint -esym(621, OKL4_ASM_PAGE_PERMS_RWX) */
#define OKL4_ASM_PAGE_PERMS_RWX (0x7)
/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_ASM_PAGE_PERMS_MAX) */
#define OKL4_ASM_PAGE_PERMS_MAX (0x7)
/**
    Invalid enumeration value
*/
/*lint -esym(621, OKL4_ASM_PAGE_PERMS_INVALID) */
#define OKL4_ASM_PAGE_PERMS_INVALID (0xffffffff)

/**
 *  okl4_error_t
 **/
/**
    KSP returned OK
*/
/*lint -esym(621, OKL4_ASM_ERROR_KSP_OK) */
#define OKL4_ASM_ERROR_KSP_OK (0x0)
/**
    The operation succeeded
*/
/*lint -esym(621, OKL4_ASM_ERROR_OK) */
#define OKL4_ASM_ERROR_OK (0x0)
/**
    The target vCPU was already running.
*/
/*lint -esym(621, OKL4_ASM_ERROR_ALREADY_STARTED) */
#define OKL4_ASM_ERROR_ALREADY_STARTED (0x1)
/**
    The target vCPU was not running.
*/
/*lint -esym(621, OKL4_ASM_ERROR_ALREADY_STOPPED) */
#define OKL4_ASM_ERROR_ALREADY_STOPPED (0x2)
/*lint -esym(621, OKL4_ASM_ERROR_AXON_AREA_TOO_BIG) */
#define OKL4_ASM_ERROR_AXON_AREA_TOO_BIG (0x3)
/*lint -esym(621, OKL4_ASM_ERROR_AXON_BAD_MESSAGE_SIZE) */
#define OKL4_ASM_ERROR_AXON_BAD_MESSAGE_SIZE (0x4)
/*lint -esym(621, OKL4_ASM_ERROR_AXON_INVALID_OFFSET) */
#define OKL4_ASM_ERROR_AXON_INVALID_OFFSET (0x5)
/*lint -esym(621, OKL4_ASM_ERROR_AXON_QUEUE_NOT_MAPPED) */
#define OKL4_ASM_ERROR_AXON_QUEUE_NOT_MAPPED (0x6)
/*lint -esym(621, OKL4_ASM_ERROR_AXON_QUEUE_NOT_READY) */
#define OKL4_ASM_ERROR_AXON_QUEUE_NOT_READY (0x7)
/*lint -esym(621, OKL4_ASM_ERROR_AXON_TRANSFER_LIMIT_EXCEEDED) */
#define OKL4_ASM_ERROR_AXON_TRANSFER_LIMIT_EXCEEDED (0x8)
/**
    A blocking operation was cancelled due to an abort of the operation.
*/
/*lint -esym(621, OKL4_ASM_ERROR_CANCELLED) */
#define OKL4_ASM_ERROR_CANCELLED (0x9)
/**
    The operation failed due to an existing mapping.  Mapping
    operations must not overlap an existing mapping.  Unmapping
    must be performed at the same size as the original mapping.
*/
/*lint -esym(621, OKL4_ASM_ERROR_EXISTING_MAPPING) */
#define OKL4_ASM_ERROR_EXISTING_MAPPING (0xa)
/**
    The operation requested with a segment failed due to
    insufficient rights in the segment.
*/
/*lint -esym(621, OKL4_ASM_ERROR_INSUFFICIENT_SEGMENT_RIGHTS) */
#define OKL4_ASM_ERROR_INSUFFICIENT_SEGMENT_RIGHTS (0xb)
/**
    The operation did not complete because it was interrupted by a
    preemption.  This error value is only used internally.
*/
/*lint -esym(621, OKL4_ASM_ERROR_INTERRUPTED) */
#define OKL4_ASM_ERROR_INTERRUPTED (0xc)
/**
    Attempt to attach an interrupt to an IRQ number, when the
    interrupt is already attached to an IRQ number
*/
/*lint -esym(621, OKL4_ASM_ERROR_INTERRUPT_ALREADY_ATTACHED) */
#define OKL4_ASM_ERROR_INTERRUPT_ALREADY_ATTACHED (0xd)
/**
    Attempt to use an IRQ number that is out of range, of
    the wrong type, or not in the correct state
*/
/*lint -esym(621, OKL4_ASM_ERROR_INTERRUPT_INVALID_IRQ) */
#define OKL4_ASM_ERROR_INTERRUPT_INVALID_IRQ (0xe)
/**
    Attempt to operate on an unknown IRQ number
*/
/*lint -esym(621, OKL4_ASM_ERROR_INTERRUPT_NOT_ATTACHED) */
#define OKL4_ASM_ERROR_INTERRUPT_NOT_ATTACHED (0xf)
/**
    An invalid argument was provided.
*/
/*lint -esym(621, OKL4_ASM_ERROR_INVALID_ARGUMENT) */
#define OKL4_ASM_ERROR_INVALID_ARGUMENT (0x10)
/**
    The operation failed because one of the arguments does not refer to a
    valid object.
*/
/*lint -esym(621, OKL4_ASM_ERROR_INVALID_DESIGNATOR) */
#define OKL4_ASM_ERROR_INVALID_DESIGNATOR (0x11)
/**
    The operation failed because the power_state
    argument is invalid.
*/
/*lint -esym(621, OKL4_ASM_ERROR_INVALID_POWER_STATE) */
#define OKL4_ASM_ERROR_INVALID_POWER_STATE (0x12)
/**
    The operation failed because the given segment index does
    not correspond to an attached physical segment.
*/
/*lint -esym(621, OKL4_ASM_ERROR_INVALID_SEGMENT_INDEX) */
#define OKL4_ASM_ERROR_INVALID_SEGMENT_INDEX (0x13)
/**
    A user provided address produced a read or write fault in the operation.
*/
/*lint -esym(621, OKL4_ASM_ERROR_MEMORY_FAULT) */
#define OKL4_ASM_ERROR_MEMORY_FAULT (0x14)
/**
    The operation failed because there is no mapping at the
    specified location.
*/
/*lint -esym(621, OKL4_ASM_ERROR_MISSING_MAPPING) */
#define OKL4_ASM_ERROR_MISSING_MAPPING (0x15)
/**
    The delete operation failed because the KMMU context is not
    empty.
*/
/*lint -esym(621, OKL4_ASM_ERROR_NON_EMPTY_MMU_CONTEXT) */
#define OKL4_ASM_ERROR_NON_EMPTY_MMU_CONTEXT (0x16)
/**
    The lookup operation failed because the given virtual address
    of the given KMMU context is not mapped at the given physical
    segment.
*/
/*lint -esym(621, OKL4_ASM_ERROR_NOT_IN_SEGMENT) */
#define OKL4_ASM_ERROR_NOT_IN_SEGMENT (0x17)
/**
    The operation failed because the caller is not on the last
    online cpu.
*/
/*lint -esym(621, OKL4_ASM_ERROR_NOT_LAST_CPU) */
#define OKL4_ASM_ERROR_NOT_LAST_CPU (0x18)
/**
    Insufficient resources are available to perform the operation.
*/
/*lint -esym(621, OKL4_ASM_ERROR_NO_RESOURCES) */
#define OKL4_ASM_ERROR_NO_RESOURCES (0x19)
/**
    Operation failed because pipe was not in the required state.
*/
/*lint -esym(621, OKL4_ASM_ERROR_PIPE_BAD_STATE) */
#define OKL4_ASM_ERROR_PIPE_BAD_STATE (0x1a)
/**
    Operation failed because no messages are in the queue.
*/
/*lint -esym(621, OKL4_ASM_ERROR_PIPE_EMPTY) */
#define OKL4_ASM_ERROR_PIPE_EMPTY (0x1b)
/**
    Operation failed because no memory is available in the queue.
*/
/*lint -esym(621, OKL4_ASM_ERROR_PIPE_FULL) */
#define OKL4_ASM_ERROR_PIPE_FULL (0x1c)
/**
    Operation failed because the pipe is in reset or not ready.
*/
/*lint -esym(621, OKL4_ASM_ERROR_PIPE_NOT_READY) */
#define OKL4_ASM_ERROR_PIPE_NOT_READY (0x1d)
/**
    Message was truncated because receive buffer size is too small.
*/
/*lint -esym(621, OKL4_ASM_ERROR_PIPE_RECV_OVERFLOW) */
#define OKL4_ASM_ERROR_PIPE_RECV_OVERFLOW (0x1e)
/**
    The operation failed because at least one VCPU has a monitored
    power state and is not currently suspended.
*/
/*lint -esym(621, OKL4_ASM_ERROR_POWER_VCPU_RESUMED) */
#define OKL4_ASM_ERROR_POWER_VCPU_RESUMED (0x1f)
/**
    The operation requires a segment to be unused, or not attached
    to an MMU context.
*/
/*lint -esym(621, OKL4_ASM_ERROR_SEGMENT_USED) */
#define OKL4_ASM_ERROR_SEGMENT_USED (0x20)
/*lint -esym(621, OKL4_ASM_ERROR_THREAD_ALREADY_WATCHING_SUSPENDED) */
#define OKL4_ASM_ERROR_THREAD_ALREADY_WATCHING_SUSPENDED (0x21)
/**
    The timer is already active, and was not reprogrammed.
*/
/*lint -esym(621, OKL4_ASM_ERROR_TIMER_ACTIVE) */
#define OKL4_ASM_ERROR_TIMER_ACTIVE (0x22)
/**
    The timer has already been cancelled or expired.
*/
/*lint -esym(621, OKL4_ASM_ERROR_TIMER_CANCELLED) */
#define OKL4_ASM_ERROR_TIMER_CANCELLED (0x23)
/**
    Operation failed due to a temporary condition, and may be retried.
*/
/*lint -esym(621, OKL4_ASM_ERROR_TRY_AGAIN) */
#define OKL4_ASM_ERROR_TRY_AGAIN (0x24)
/**
    The non-blocking operation failed because it would
    block on a resource.
*/
/*lint -esym(621, OKL4_ASM_ERROR_WOULD_BLOCK) */
#define OKL4_ASM_ERROR_WOULD_BLOCK (0x25)
/**
    Insufficient resources
*/
/*lint -esym(621, OKL4_ASM_ERROR_ALLOC_EXHAUSTED) */
#define OKL4_ASM_ERROR_ALLOC_EXHAUSTED (0x26)
/**
    KSP specific error 0
*/
/*lint -esym(621, OKL4_ASM_ERROR_KSP_ERROR_0) */
#define OKL4_ASM_ERROR_KSP_ERROR_0 (0x10000010)
/**
    KSP specific error 1
*/
/*lint -esym(621, OKL4_ASM_ERROR_KSP_ERROR_1) */
#define OKL4_ASM_ERROR_KSP_ERROR_1 (0x10000011)
/**
    KSP specific error 2
*/
/*lint -esym(621, OKL4_ASM_ERROR_KSP_ERROR_2) */
#define OKL4_ASM_ERROR_KSP_ERROR_2 (0x10000012)
/**
    KSP specific error 3
*/
/*lint -esym(621, OKL4_ASM_ERROR_KSP_ERROR_3) */
#define OKL4_ASM_ERROR_KSP_ERROR_3 (0x10000013)
/**
    KSP specific error 4
*/
/*lint -esym(621, OKL4_ASM_ERROR_KSP_ERROR_4) */
#define OKL4_ASM_ERROR_KSP_ERROR_4 (0x10000014)
/**
    KSP specific error 5
*/
/*lint -esym(621, OKL4_ASM_ERROR_KSP_ERROR_5) */
#define OKL4_ASM_ERROR_KSP_ERROR_5 (0x10000015)
/**
    KSP specific error 6
*/
/*lint -esym(621, OKL4_ASM_ERROR_KSP_ERROR_6) */
#define OKL4_ASM_ERROR_KSP_ERROR_6 (0x10000016)
/**
    KSP specific error 7
*/
/*lint -esym(621, OKL4_ASM_ERROR_KSP_ERROR_7) */
#define OKL4_ASM_ERROR_KSP_ERROR_7 (0x10000017)
/**
    Invalid argument to KSP
*/
/*lint -esym(621, OKL4_ASM_ERROR_KSP_INVALID_ARG) */
#define OKL4_ASM_ERROR_KSP_INVALID_ARG (0x80000001)
/**
    KSP doesn't implement requested feature
*/
/*lint -esym(621, OKL4_ASM_ERROR_KSP_NOT_IMPLEMENTED) */
#define OKL4_ASM_ERROR_KSP_NOT_IMPLEMENTED (0x80000002)
/**
    User didn't supply rights for requested feature
*/
/*lint -esym(621, OKL4_ASM_ERROR_KSP_INSUFFICIENT_RIGHTS) */
#define OKL4_ASM_ERROR_KSP_INSUFFICIENT_RIGHTS (0x80000003)
/**
    Interrupt already registered
*/
/*lint -esym(621, OKL4_ASM_ERROR_KSP_INTERRUPT_REGISTERED) */
#define OKL4_ASM_ERROR_KSP_INTERRUPT_REGISTERED (0x80000004)
/**
    Requested operation is not implemented.
*/
/*lint -esym(621, OKL4_ASM_ERROR_NOT_IMPLEMENTED) */
#define OKL4_ASM_ERROR_NOT_IMPLEMENTED (0xffffffff)
/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_ASM_ERROR_MAX) */
#define OKL4_ASM_ERROR_MAX (0xffffffff)

/**
 *  okl4_gicd_icfgr_t
 **/


/*lint -esym(621, OKL4_ASM_MASK_EDGE_GICD_ICFGR) */
#define OKL4_ASM_MASK_EDGE_GICD_ICFGR (1 << 1)
/*lint -esym(621, OKL4_ASM_SHIFT_EDGE_GICD_ICFGR) */
#define OKL4_ASM_SHIFT_EDGE_GICD_ICFGR (1)
/*lint -esym(621, OKL4_ASM_WIDTH_EDGE_GICD_ICFGR) */
#define OKL4_ASM_WIDTH_EDGE_GICD_ICFGR (1)


/**
 *  okl4_sgi_target_t
 **/
/*lint -esym(621, OKL4_ASM_SGI_TARGET_LISTED) */
#define OKL4_ASM_SGI_TARGET_LISTED (0x0)
/*lint -esym(621, OKL4_ASM_SGI_TARGET_ALL_OTHERS) */
#define OKL4_ASM_SGI_TARGET_ALL_OTHERS (0x1)
/*lint -esym(621, OKL4_ASM_SGI_TARGET_SELF) */
#define OKL4_ASM_SGI_TARGET_SELF (0x2)
/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_ASM_SGI_TARGET_MAX) */
#define OKL4_ASM_SGI_TARGET_MAX (0x2)
/**
    Invalid enumeration value
*/
/*lint -esym(621, OKL4_ASM_SGI_TARGET_INVALID) */
#define OKL4_ASM_SGI_TARGET_INVALID (0xffffffff)

/**
 *  okl4_gicd_sgir_t
 **/


/*lint -esym(621, OKL4_ASM_MASK_SGIINTID_GICD_SGIR) */
#define OKL4_ASM_MASK_SGIINTID_GICD_SGIR (15)
/*lint -esym(621, OKL4_ASM_SHIFT_SGIINTID_GICD_SGIR) */
#define OKL4_ASM_SHIFT_SGIINTID_GICD_SGIR (0)
/*lint -esym(621, OKL4_ASM_WIDTH_SGIINTID_GICD_SGIR) */
#define OKL4_ASM_WIDTH_SGIINTID_GICD_SGIR (4)
/*lint -esym(621, OKL4_ASM_MASK_NSATT_GICD_SGIR) */
#define OKL4_ASM_MASK_NSATT_GICD_SGIR (1 << 15)
/*lint -esym(621, OKL4_ASM_SHIFT_NSATT_GICD_SGIR) */
#define OKL4_ASM_SHIFT_NSATT_GICD_SGIR (15)
/*lint -esym(621, OKL4_ASM_WIDTH_NSATT_GICD_SGIR) */
#define OKL4_ASM_WIDTH_NSATT_GICD_SGIR (1)
/*lint -esym(621, OKL4_ASM_MASK_CPUTARGETLIST_GICD_SGIR) */
#define OKL4_ASM_MASK_CPUTARGETLIST_GICD_SGIR (255 << 16)
/*lint -esym(621, OKL4_ASM_SHIFT_CPUTARGETLIST_GICD_SGIR) */
#define OKL4_ASM_SHIFT_CPUTARGETLIST_GICD_SGIR (16)
/*lint -esym(621, OKL4_ASM_WIDTH_CPUTARGETLIST_GICD_SGIR) */
#define OKL4_ASM_WIDTH_CPUTARGETLIST_GICD_SGIR (8)
/*lint -esym(621, OKL4_ASM_MASK_TARGETLISTFILTER_GICD_SGIR) */
#define OKL4_ASM_MASK_TARGETLISTFILTER_GICD_SGIR (3 << 24)
/*lint -esym(621, OKL4_ASM_SHIFT_TARGETLISTFILTER_GICD_SGIR) */
#define OKL4_ASM_SHIFT_TARGETLISTFILTER_GICD_SGIR (24)
/*lint -esym(621, OKL4_ASM_WIDTH_TARGETLISTFILTER_GICD_SGIR) */
#define OKL4_ASM_WIDTH_TARGETLISTFILTER_GICD_SGIR (2)


/**
 *  okl4_link_role_t
 **/
/*lint -esym(621, OKL4_ASM_LINK_ROLE_SYMMETRIC) */
#define OKL4_ASM_LINK_ROLE_SYMMETRIC (0x0)
/*lint -esym(621, OKL4_ASM_LINK_ROLE_SERVER) */
#define OKL4_ASM_LINK_ROLE_SERVER (0x1)
/*lint -esym(621, OKL4_ASM_LINK_ROLE_CLIENT) */
#define OKL4_ASM_LINK_ROLE_CLIENT (0x2)
/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_ASM_LINK_ROLE_MAX) */
#define OKL4_ASM_LINK_ROLE_MAX (0x2)
/**
    Invalid enumeration value
*/
/*lint -esym(621, OKL4_ASM_LINK_ROLE_INVALID) */
#define OKL4_ASM_LINK_ROLE_INVALID (0xffffffff)

/**
 *  okl4_link_transport_type_t
 **/
/*lint -esym(621, OKL4_ASM_LINK_TRANSPORT_TYPE_SHARED_BUFFER) */
#define OKL4_ASM_LINK_TRANSPORT_TYPE_SHARED_BUFFER (0x0)
/*lint -esym(621, OKL4_ASM_LINK_TRANSPORT_TYPE_AXONS) */
#define OKL4_ASM_LINK_TRANSPORT_TYPE_AXONS (0x1)
/*lint -esym(621, OKL4_ASM_LINK_TRANSPORT_TYPE_PIPES) */
#define OKL4_ASM_LINK_TRANSPORT_TYPE_PIPES (0x2)
/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_ASM_LINK_TRANSPORT_TYPE_MAX) */
#define OKL4_ASM_LINK_TRANSPORT_TYPE_MAX (0x2)
/**
    Invalid enumeration value
*/
/*lint -esym(621, OKL4_ASM_LINK_TRANSPORT_TYPE_INVALID) */
#define OKL4_ASM_LINK_TRANSPORT_TYPE_INVALID (0xffffffff)

/**
 *  okl4_mmu_lookup_index_t
 **/


/*lint -esym(621, OKL4_ASM_MASK_ERROR_MMU_LOOKUP_INDEX) */
#define OKL4_ASM_MASK_ERROR_MMU_LOOKUP_INDEX (65535)
/*lint -esym(621, OKL4_ASM_SHIFT_ERROR_MMU_LOOKUP_INDEX) */
#define OKL4_ASM_SHIFT_ERROR_MMU_LOOKUP_INDEX (0)
/*lint -esym(621, OKL4_ASM_WIDTH_ERROR_MMU_LOOKUP_INDEX) */
#define OKL4_ASM_WIDTH_ERROR_MMU_LOOKUP_INDEX (16)
/*lint -esym(621, OKL4_ASM_MASK_INDEX_MMU_LOOKUP_INDEX) */
#define OKL4_ASM_MASK_INDEX_MMU_LOOKUP_INDEX (65535 << 16)
/*lint -esym(621, OKL4_ASM_SHIFT_INDEX_MMU_LOOKUP_INDEX) */
#define OKL4_ASM_SHIFT_INDEX_MMU_LOOKUP_INDEX (16)
/*lint -esym(621, OKL4_ASM_WIDTH_INDEX_MMU_LOOKUP_INDEX) */
#define OKL4_ASM_WIDTH_INDEX_MMU_LOOKUP_INDEX (16)


/**
 *  okl4_mmu_lookup_size_t
 **/


/*lint -esym(621, OKL4_ASM_MASK_SEG_INDEX_MMU_LOOKUP_SIZE) */
#define OKL4_ASM_MASK_SEG_INDEX_MMU_LOOKUP_SIZE (1023)
/*lint -esym(621, OKL4_ASM_SHIFT_SEG_INDEX_MMU_LOOKUP_SIZE) */
#define OKL4_ASM_SHIFT_SEG_INDEX_MMU_LOOKUP_SIZE (0)
/*lint -esym(621, OKL4_ASM_WIDTH_SEG_INDEX_MMU_LOOKUP_SIZE) */
#define OKL4_ASM_WIDTH_SEG_INDEX_MMU_LOOKUP_SIZE (10)
/*lint -esym(621, OKL4_ASM_MASK_SIZE_10_MMU_LOOKUP_SIZE) */
#define OKL4_ASM_MASK_SIZE_10_MMU_LOOKUP_SIZE (18014398509481983 << 10)
/*lint -esym(621, OKL4_ASM_SHIFT_SIZE_10_MMU_LOOKUP_SIZE) */
#define OKL4_ASM_SHIFT_SIZE_10_MMU_LOOKUP_SIZE (10)
/*lint -esym(621, OKL4_ASM_WIDTH_SIZE_10_MMU_LOOKUP_SIZE) */
#define OKL4_ASM_WIDTH_SIZE_10_MMU_LOOKUP_SIZE (54)


/**
 *  okl4_nanoseconds_t
 **/
/** Timer period upper bound is (1 << 55) ns */
/*lint -esym(621, OKL4_TIMER_MAX_PERIOD_NS) */
#define OKL4_TIMER_MAX_PERIOD_NS (36028797018963968)

/** Timer period lower bound is 1000000 ns */
/*lint -esym(621, OKL4_TIMER_MIN_PERIOD_NS) */
#define OKL4_TIMER_MIN_PERIOD_NS (1000000)

/**
 *  _okl4_page_attribute_t
 **/


/*lint -esym(621, _OKL4_ASM_MASK_RWX_PAGE_ATTRIBUTE) */
#define _OKL4_ASM_MASK_RWX_PAGE_ATTRIBUTE (7)
/*lint -esym(621, _OKL4_ASM_SHIFT_RWX_PAGE_ATTRIBUTE) */
#define _OKL4_ASM_SHIFT_RWX_PAGE_ATTRIBUTE (0)
/*lint -esym(621, _OKL4_ASM_WIDTH_RWX_PAGE_ATTRIBUTE) */
#define _OKL4_ASM_WIDTH_RWX_PAGE_ATTRIBUTE (3)
/*lint -esym(621, _OKL4_ASM_MASK_ATTRIB_PAGE_ATTRIBUTE) */
#define _OKL4_ASM_MASK_ATTRIB_PAGE_ATTRIBUTE (268435455 << 4)
/*lint -esym(621, _OKL4_ASM_SHIFT_ATTRIB_PAGE_ATTRIBUTE) */
#define _OKL4_ASM_SHIFT_ATTRIB_PAGE_ATTRIBUTE (4)
/*lint -esym(621, _OKL4_ASM_WIDTH_ATTRIB_PAGE_ATTRIBUTE) */
#define _OKL4_ASM_WIDTH_ATTRIB_PAGE_ATTRIBUTE (28)


/**
 *  okl4_pipe_control_t
 **/

/*lint -esym(621, OKL4_PIPE_CONTROL_OP_CLR_HALTED) */
#define OKL4_PIPE_CONTROL_OP_CLR_HALTED (4)
/*lint -esym(621, OKL4_PIPE_CONTROL_OP_RESET) */
#define OKL4_PIPE_CONTROL_OP_RESET (0)
/*lint -esym(621, OKL4_PIPE_CONTROL_OP_SET_HALTED) */
#define OKL4_PIPE_CONTROL_OP_SET_HALTED (3)
/*lint -esym(621, OKL4_PIPE_CONTROL_OP_SET_RX_READY) */
#define OKL4_PIPE_CONTROL_OP_SET_RX_READY (2)
/*lint -esym(621, OKL4_PIPE_CONTROL_OP_SET_TX_READY) */
#define OKL4_PIPE_CONTROL_OP_SET_TX_READY (1)

/*lint -esym(621, OKL4_ASM_MASK_DO_OP_PIPE_CONTROL) */
#define OKL4_ASM_MASK_DO_OP_PIPE_CONTROL (1)
/*lint -esym(621, OKL4_ASM_SHIFT_DO_OP_PIPE_CONTROL) */
#define OKL4_ASM_SHIFT_DO_OP_PIPE_CONTROL (0)
/*lint -esym(621, OKL4_ASM_WIDTH_DO_OP_PIPE_CONTROL) */
#define OKL4_ASM_WIDTH_DO_OP_PIPE_CONTROL (1)
/*lint -esym(621, OKL4_ASM_MASK_OPERATION_PIPE_CONTROL) */
#define OKL4_ASM_MASK_OPERATION_PIPE_CONTROL (7 << 1)
/*lint -esym(621, OKL4_ASM_SHIFT_OPERATION_PIPE_CONTROL) */
#define OKL4_ASM_SHIFT_OPERATION_PIPE_CONTROL (1)
/*lint -esym(621, OKL4_ASM_WIDTH_OPERATION_PIPE_CONTROL) */
#define OKL4_ASM_WIDTH_OPERATION_PIPE_CONTROL (3)


/**
 *  okl4_pipe_state_t
 **/


/*lint -esym(621, OKL4_ASM_MASK_RESET_PIPE_STATE) */
#define OKL4_ASM_MASK_RESET_PIPE_STATE (1)
/*lint -esym(621, OKL4_ASM_SHIFT_RESET_PIPE_STATE) */
#define OKL4_ASM_SHIFT_RESET_PIPE_STATE (0)
/*lint -esym(621, OKL4_ASM_WIDTH_RESET_PIPE_STATE) */
#define OKL4_ASM_WIDTH_RESET_PIPE_STATE (1)
/*lint -esym(621, OKL4_ASM_MASK_HALTED_PIPE_STATE) */
#define OKL4_ASM_MASK_HALTED_PIPE_STATE (1 << 1)
/*lint -esym(621, OKL4_ASM_SHIFT_HALTED_PIPE_STATE) */
#define OKL4_ASM_SHIFT_HALTED_PIPE_STATE (1)
/*lint -esym(621, OKL4_ASM_WIDTH_HALTED_PIPE_STATE) */
#define OKL4_ASM_WIDTH_HALTED_PIPE_STATE (1)
/*lint -esym(621, OKL4_ASM_MASK_RX_READY_PIPE_STATE) */
#define OKL4_ASM_MASK_RX_READY_PIPE_STATE (1 << 2)
/*lint -esym(621, OKL4_ASM_SHIFT_RX_READY_PIPE_STATE) */
#define OKL4_ASM_SHIFT_RX_READY_PIPE_STATE (2)
/*lint -esym(621, OKL4_ASM_WIDTH_RX_READY_PIPE_STATE) */
#define OKL4_ASM_WIDTH_RX_READY_PIPE_STATE (1)
/*lint -esym(621, OKL4_ASM_MASK_TX_READY_PIPE_STATE) */
#define OKL4_ASM_MASK_TX_READY_PIPE_STATE (1 << 3)
/*lint -esym(621, OKL4_ASM_SHIFT_TX_READY_PIPE_STATE) */
#define OKL4_ASM_SHIFT_TX_READY_PIPE_STATE (3)
/*lint -esym(621, OKL4_ASM_WIDTH_TX_READY_PIPE_STATE) */
#define OKL4_ASM_WIDTH_TX_READY_PIPE_STATE (1)
/*lint -esym(621, OKL4_ASM_MASK_RX_AVAILABLE_PIPE_STATE) */
#define OKL4_ASM_MASK_RX_AVAILABLE_PIPE_STATE (1 << 4)
/*lint -esym(621, OKL4_ASM_SHIFT_RX_AVAILABLE_PIPE_STATE) */
#define OKL4_ASM_SHIFT_RX_AVAILABLE_PIPE_STATE (4)
/*lint -esym(621, OKL4_ASM_WIDTH_RX_AVAILABLE_PIPE_STATE) */
#define OKL4_ASM_WIDTH_RX_AVAILABLE_PIPE_STATE (1)
/*lint -esym(621, OKL4_ASM_MASK_TX_AVAILABLE_PIPE_STATE) */
#define OKL4_ASM_MASK_TX_AVAILABLE_PIPE_STATE (1 << 5)
/*lint -esym(621, OKL4_ASM_SHIFT_TX_AVAILABLE_PIPE_STATE) */
#define OKL4_ASM_SHIFT_TX_AVAILABLE_PIPE_STATE (5)
/*lint -esym(621, OKL4_ASM_WIDTH_TX_AVAILABLE_PIPE_STATE) */
#define OKL4_ASM_WIDTH_TX_AVAILABLE_PIPE_STATE (1)
/*lint -esym(621, OKL4_ASM_MASK_WAITING_PIPE_STATE) */
#define OKL4_ASM_MASK_WAITING_PIPE_STATE (1 << 6)
/*lint -esym(621, OKL4_ASM_SHIFT_WAITING_PIPE_STATE) */
#define OKL4_ASM_SHIFT_WAITING_PIPE_STATE (6)
/*lint -esym(621, OKL4_ASM_WIDTH_WAITING_PIPE_STATE) */
#define OKL4_ASM_WIDTH_WAITING_PIPE_STATE (1)
/*lint -esym(621, OKL4_ASM_MASK_OVERQUOTA_PIPE_STATE) */
#define OKL4_ASM_MASK_OVERQUOTA_PIPE_STATE (1 << 7)
/*lint -esym(621, OKL4_ASM_SHIFT_OVERQUOTA_PIPE_STATE) */
#define OKL4_ASM_SHIFT_OVERQUOTA_PIPE_STATE (7)
/*lint -esym(621, OKL4_ASM_WIDTH_OVERQUOTA_PIPE_STATE) */
#define OKL4_ASM_WIDTH_OVERQUOTA_PIPE_STATE (1)


/**
 *  okl4_power_state_t
 **/
/*lint -esym(621, OKL4_POWER_STATE_IDLE) */
#define OKL4_POWER_STATE_IDLE (0)

/*lint -esym(621, OKL4_POWER_STATE_PLATFORM_BASE) */
#define OKL4_POWER_STATE_PLATFORM_BASE (256)

/*lint -esym(621, OKL4_POWER_STATE_POWEROFF) */
#define OKL4_POWER_STATE_POWEROFF (1)

/**
 *  okl4_register_set_t
 **/
/*lint -esym(621, OKL4_ASM_REGISTER_SET_CPU_REGS) */
#define OKL4_ASM_REGISTER_SET_CPU_REGS (0x0)
/*lint -esym(621, OKL4_ASM_REGISTER_SET_VFP_REGS) */
#define OKL4_ASM_REGISTER_SET_VFP_REGS (0x1)
/*lint -esym(621, OKL4_ASM_REGISTER_SET_VFP_CTRL_REGS) */
#define OKL4_ASM_REGISTER_SET_VFP_CTRL_REGS (0x2)
/*lint -esym(621, OKL4_ASM_REGISTER_SET_VFP64_REGS) */
#define OKL4_ASM_REGISTER_SET_VFP64_REGS (0x3)
/*lint -esym(621, OKL4_ASM_REGISTER_SET_VFP128_REGS) */
#define OKL4_ASM_REGISTER_SET_VFP128_REGS (0x4)
/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_ASM_REGISTER_SET_MAX) */
#define OKL4_ASM_REGISTER_SET_MAX (0x4)
/**
    Invalid enumeration value
*/
/*lint -esym(621, OKL4_ASM_REGISTER_SET_INVALID) */
#define OKL4_ASM_REGISTER_SET_INVALID (0xffffffff)

/**
 *  okl4_register_and_set_t
 **/


/*lint -esym(621, OKL4_ASM_MASK_OFFSET_REGISTER_AND_SET) */
#define OKL4_ASM_MASK_OFFSET_REGISTER_AND_SET (65535)
/*lint -esym(621, OKL4_ASM_SHIFT_OFFSET_REGISTER_AND_SET) */
#define OKL4_ASM_SHIFT_OFFSET_REGISTER_AND_SET (0)
/*lint -esym(621, OKL4_ASM_WIDTH_OFFSET_REGISTER_AND_SET) */
#define OKL4_ASM_WIDTH_OFFSET_REGISTER_AND_SET (16)
/*lint -esym(621, OKL4_ASM_MASK_SET_REGISTER_AND_SET) */
#define OKL4_ASM_MASK_SET_REGISTER_AND_SET (65535 << 16)
/*lint -esym(621, OKL4_ASM_SHIFT_SET_REGISTER_AND_SET) */
#define OKL4_ASM_SHIFT_SET_REGISTER_AND_SET (16)
/*lint -esym(621, OKL4_ASM_WIDTH_SET_REGISTER_AND_SET) */
#define OKL4_ASM_WIDTH_SET_REGISTER_AND_SET (16)


/**
 *  okl4_scheduler_virq_flags_t
 **/


/*lint -esym(621, OKL4_ASM_MASK_POWER_SUSPENDED_SCHEDULER_VIRQ_FLAGS) */
#define OKL4_ASM_MASK_POWER_SUSPENDED_SCHEDULER_VIRQ_FLAGS (1)
/*lint -esym(621, OKL4_ASM_SHIFT_POWER_SUSPENDED_SCHEDULER_VIRQ_FLAGS) */
#define OKL4_ASM_SHIFT_POWER_SUSPENDED_SCHEDULER_VIRQ_FLAGS (0)
/*lint -esym(621, OKL4_ASM_WIDTH_POWER_SUSPENDED_SCHEDULER_VIRQ_FLAGS) */
#define OKL4_ASM_WIDTH_POWER_SUSPENDED_SCHEDULER_VIRQ_FLAGS (1)


/**
 *  okl4_sdk_version_t
 **/


/*lint -esym(621, OKL4_ASM_MASK_MAINTENANCE_SDK_VERSION) */
#define OKL4_ASM_MASK_MAINTENANCE_SDK_VERSION (63)
/*lint -esym(621, OKL4_ASM_SHIFT_MAINTENANCE_SDK_VERSION) */
#define OKL4_ASM_SHIFT_MAINTENANCE_SDK_VERSION (0)
/*lint -esym(621, OKL4_ASM_WIDTH_MAINTENANCE_SDK_VERSION) */
#define OKL4_ASM_WIDTH_MAINTENANCE_SDK_VERSION (6)
/*lint -esym(621, OKL4_ASM_MASK_RELEASE_SDK_VERSION) */
#define OKL4_ASM_MASK_RELEASE_SDK_VERSION (255 << 8)
/*lint -esym(621, OKL4_ASM_SHIFT_RELEASE_SDK_VERSION) */
#define OKL4_ASM_SHIFT_RELEASE_SDK_VERSION (8)
/*lint -esym(621, OKL4_ASM_WIDTH_RELEASE_SDK_VERSION) */
#define OKL4_ASM_WIDTH_RELEASE_SDK_VERSION (8)
/*lint -esym(621, OKL4_ASM_MASK_MINOR_SDK_VERSION) */
#define OKL4_ASM_MASK_MINOR_SDK_VERSION (63 << 16)
/*lint -esym(621, OKL4_ASM_SHIFT_MINOR_SDK_VERSION) */
#define OKL4_ASM_SHIFT_MINOR_SDK_VERSION (16)
/*lint -esym(621, OKL4_ASM_WIDTH_MINOR_SDK_VERSION) */
#define OKL4_ASM_WIDTH_MINOR_SDK_VERSION (6)
/*lint -esym(621, OKL4_ASM_MASK_MAJOR_SDK_VERSION) */
#define OKL4_ASM_MASK_MAJOR_SDK_VERSION (15 << 24)
/*lint -esym(621, OKL4_ASM_SHIFT_MAJOR_SDK_VERSION) */
#define OKL4_ASM_SHIFT_MAJOR_SDK_VERSION (24)
/*lint -esym(621, OKL4_ASM_WIDTH_MAJOR_SDK_VERSION) */
#define OKL4_ASM_WIDTH_MAJOR_SDK_VERSION (4)
/*lint -esym(621, OKL4_ASM_MASK_RES0_FLAG_SDK_VERSION) */
#define OKL4_ASM_MASK_RES0_FLAG_SDK_VERSION (1 << 28)
/*lint -esym(621, OKL4_ASM_SHIFT_RES0_FLAG_SDK_VERSION) */
#define OKL4_ASM_SHIFT_RES0_FLAG_SDK_VERSION (28)
/*lint -esym(621, OKL4_ASM_WIDTH_RES0_FLAG_SDK_VERSION) */
#define OKL4_ASM_WIDTH_RES0_FLAG_SDK_VERSION (1)
/*lint -esym(621, OKL4_ASM_MASK_DEV_FLAG_SDK_VERSION) */
#define OKL4_ASM_MASK_DEV_FLAG_SDK_VERSION (1 << 30)
/*lint -esym(621, OKL4_ASM_SHIFT_DEV_FLAG_SDK_VERSION) */
#define OKL4_ASM_SHIFT_DEV_FLAG_SDK_VERSION (30)
/*lint -esym(621, OKL4_ASM_WIDTH_DEV_FLAG_SDK_VERSION) */
#define OKL4_ASM_WIDTH_DEV_FLAG_SDK_VERSION (1)
/*lint -esym(621, OKL4_ASM_MASK_FORMAT_FLAG_SDK_VERSION) */
#define OKL4_ASM_MASK_FORMAT_FLAG_SDK_VERSION (1 << 31)
/*lint -esym(621, OKL4_ASM_SHIFT_FORMAT_FLAG_SDK_VERSION) */
#define OKL4_ASM_SHIFT_FORMAT_FLAG_SDK_VERSION (31)
/*lint -esym(621, OKL4_ASM_WIDTH_FORMAT_FLAG_SDK_VERSION) */
#define OKL4_ASM_WIDTH_FORMAT_FLAG_SDK_VERSION (1)


/**
 *  okl4_timer_flags_t
 **/


/*lint -esym(621, OKL4_ASM_MASK_ACTIVE_TIMER_FLAGS) */
#define OKL4_ASM_MASK_ACTIVE_TIMER_FLAGS (1)
/*lint -esym(621, OKL4_ASM_SHIFT_ACTIVE_TIMER_FLAGS) */
#define OKL4_ASM_SHIFT_ACTIVE_TIMER_FLAGS (0)
/*lint -esym(621, OKL4_ASM_WIDTH_ACTIVE_TIMER_FLAGS) */
#define OKL4_ASM_WIDTH_ACTIVE_TIMER_FLAGS (1)
/*lint -esym(621, OKL4_ASM_MASK_PERIODIC_TIMER_FLAGS) */
#define OKL4_ASM_MASK_PERIODIC_TIMER_FLAGS (1 << 1)
/*lint -esym(621, OKL4_ASM_SHIFT_PERIODIC_TIMER_FLAGS) */
#define OKL4_ASM_SHIFT_PERIODIC_TIMER_FLAGS (1)
/*lint -esym(621, OKL4_ASM_WIDTH_PERIODIC_TIMER_FLAGS) */
#define OKL4_ASM_WIDTH_PERIODIC_TIMER_FLAGS (1)
/*lint -esym(621, OKL4_ASM_MASK_ABSOLUTE_TIMER_FLAGS) */
#define OKL4_ASM_MASK_ABSOLUTE_TIMER_FLAGS (1 << 2)
/*lint -esym(621, OKL4_ASM_SHIFT_ABSOLUTE_TIMER_FLAGS) */
#define OKL4_ASM_SHIFT_ABSOLUTE_TIMER_FLAGS (2)
/*lint -esym(621, OKL4_ASM_WIDTH_ABSOLUTE_TIMER_FLAGS) */
#define OKL4_ASM_WIDTH_ABSOLUTE_TIMER_FLAGS (1)
/*lint -esym(621, OKL4_ASM_MASK_UNITS_TIMER_FLAGS) */
#define OKL4_ASM_MASK_UNITS_TIMER_FLAGS (1 << 3)
/*lint -esym(621, OKL4_ASM_SHIFT_UNITS_TIMER_FLAGS) */
#define OKL4_ASM_SHIFT_UNITS_TIMER_FLAGS (3)
/*lint -esym(621, OKL4_ASM_WIDTH_UNITS_TIMER_FLAGS) */
#define OKL4_ASM_WIDTH_UNITS_TIMER_FLAGS (1)
/*lint -esym(621, OKL4_ASM_MASK_ALIGN_TIMER_FLAGS) */
#define OKL4_ASM_MASK_ALIGN_TIMER_FLAGS (1 << 4)
/*lint -esym(621, OKL4_ASM_SHIFT_ALIGN_TIMER_FLAGS) */
#define OKL4_ASM_SHIFT_ALIGN_TIMER_FLAGS (4)
/*lint -esym(621, OKL4_ASM_WIDTH_ALIGN_TIMER_FLAGS) */
#define OKL4_ASM_WIDTH_ALIGN_TIMER_FLAGS (1)
/*lint -esym(621, OKL4_ASM_MASK_WATCHDOG_TIMER_FLAGS) */
#define OKL4_ASM_MASK_WATCHDOG_TIMER_FLAGS (1 << 5)
/*lint -esym(621, OKL4_ASM_SHIFT_WATCHDOG_TIMER_FLAGS) */
#define OKL4_ASM_SHIFT_WATCHDOG_TIMER_FLAGS (5)
/*lint -esym(621, OKL4_ASM_WIDTH_WATCHDOG_TIMER_FLAGS) */
#define OKL4_ASM_WIDTH_WATCHDOG_TIMER_FLAGS (1)
/*lint -esym(621, OKL4_ASM_MASK_RELOAD_TIMER_FLAGS) */
#define OKL4_ASM_MASK_RELOAD_TIMER_FLAGS (1 << 30)
/*lint -esym(621, OKL4_ASM_SHIFT_RELOAD_TIMER_FLAGS) */
#define OKL4_ASM_SHIFT_RELOAD_TIMER_FLAGS (30)
/*lint -esym(621, OKL4_ASM_WIDTH_RELOAD_TIMER_FLAGS) */
#define OKL4_ASM_WIDTH_RELOAD_TIMER_FLAGS (1)
/*lint -esym(621, OKL4_ASM_MASK_TIMESLICE_TIMER_FLAGS) */
#define OKL4_ASM_MASK_TIMESLICE_TIMER_FLAGS (1 << 31)
/*lint -esym(621, OKL4_ASM_SHIFT_TIMESLICE_TIMER_FLAGS) */
#define OKL4_ASM_SHIFT_TIMESLICE_TIMER_FLAGS (31)
/*lint -esym(621, OKL4_ASM_WIDTH_TIMESLICE_TIMER_FLAGS) */
#define OKL4_ASM_WIDTH_TIMESLICE_TIMER_FLAGS (1)


/**
 *  okl4_tracepoint_class_t
 **/
/*lint -esym(621, OKL4_ASM_TRACEPOINT_CLASS_THREAD_STATE) */
#define OKL4_ASM_TRACEPOINT_CLASS_THREAD_STATE (0x0)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_CLASS_SYSCALLS) */
#define OKL4_ASM_TRACEPOINT_CLASS_SYSCALLS (0x1)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_CLASS_PRIMARY) */
#define OKL4_ASM_TRACEPOINT_CLASS_PRIMARY (0x2)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_CLASS_SECONDARY) */
#define OKL4_ASM_TRACEPOINT_CLASS_SECONDARY (0x3)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_CLASS_TERTIARY) */
#define OKL4_ASM_TRACEPOINT_CLASS_TERTIARY (0x4)
/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_ASM_TRACEPOINT_CLASS_MAX) */
#define OKL4_ASM_TRACEPOINT_CLASS_MAX (0x4)
/**
    Invalid enumeration value
*/
/*lint -esym(621, OKL4_ASM_TRACEPOINT_CLASS_INVALID) */
#define OKL4_ASM_TRACEPOINT_CLASS_INVALID (0xffffffff)

/**
 *  _okl4_tracepoint_desc_t
 **/


/*lint -esym(621, _OKL4_ASM_MASK_ID_TRACEPOINT_DESC) */
#define _OKL4_ASM_MASK_ID_TRACEPOINT_DESC (255)
/*lint -esym(621, _OKL4_ASM_SHIFT_ID_TRACEPOINT_DESC) */
#define _OKL4_ASM_SHIFT_ID_TRACEPOINT_DESC (0)
/*lint -esym(621, _OKL4_ASM_WIDTH_ID_TRACEPOINT_DESC) */
#define _OKL4_ASM_WIDTH_ID_TRACEPOINT_DESC (8)
/*lint -esym(621, _OKL4_ASM_MASK_USER_TRACEPOINT_DESC) */
#define _OKL4_ASM_MASK_USER_TRACEPOINT_DESC (1 << 8)
/*lint -esym(621, _OKL4_ASM_SHIFT_USER_TRACEPOINT_DESC) */
#define _OKL4_ASM_SHIFT_USER_TRACEPOINT_DESC (8)
/*lint -esym(621, _OKL4_ASM_WIDTH_USER_TRACEPOINT_DESC) */
#define _OKL4_ASM_WIDTH_USER_TRACEPOINT_DESC (1)
/*lint -esym(621, _OKL4_ASM_MASK_BIN_TRACEPOINT_DESC) */
#define _OKL4_ASM_MASK_BIN_TRACEPOINT_DESC (1 << 9)
/*lint -esym(621, _OKL4_ASM_SHIFT_BIN_TRACEPOINT_DESC) */
#define _OKL4_ASM_SHIFT_BIN_TRACEPOINT_DESC (9)
/*lint -esym(621, _OKL4_ASM_WIDTH_BIN_TRACEPOINT_DESC) */
#define _OKL4_ASM_WIDTH_BIN_TRACEPOINT_DESC (1)
/*lint -esym(621, _OKL4_ASM_MASK_RECLEN_TRACEPOINT_DESC) */
#define _OKL4_ASM_MASK_RECLEN_TRACEPOINT_DESC (63 << 10)
/*lint -esym(621, _OKL4_ASM_SHIFT_RECLEN_TRACEPOINT_DESC) */
#define _OKL4_ASM_SHIFT_RECLEN_TRACEPOINT_DESC (10)
/*lint -esym(621, _OKL4_ASM_WIDTH_RECLEN_TRACEPOINT_DESC) */
#define _OKL4_ASM_WIDTH_RECLEN_TRACEPOINT_DESC (6)
/*lint -esym(621, _OKL4_ASM_MASK_CPUID_TRACEPOINT_DESC) */
#define _OKL4_ASM_MASK_CPUID_TRACEPOINT_DESC (63 << 16)
/*lint -esym(621, _OKL4_ASM_SHIFT_CPUID_TRACEPOINT_DESC) */
#define _OKL4_ASM_SHIFT_CPUID_TRACEPOINT_DESC (16)
/*lint -esym(621, _OKL4_ASM_WIDTH_CPUID_TRACEPOINT_DESC) */
#define _OKL4_ASM_WIDTH_CPUID_TRACEPOINT_DESC (6)
/*lint -esym(621, _OKL4_ASM_MASK_THREADID_TRACEPOINT_DESC) */
#define _OKL4_ASM_MASK_THREADID_TRACEPOINT_DESC (63 << 22)
/*lint -esym(621, _OKL4_ASM_SHIFT_THREADID_TRACEPOINT_DESC) */
#define _OKL4_ASM_SHIFT_THREADID_TRACEPOINT_DESC (22)
/*lint -esym(621, _OKL4_ASM_WIDTH_THREADID_TRACEPOINT_DESC) */
#define _OKL4_ASM_WIDTH_THREADID_TRACEPOINT_DESC (6)
/*lint -esym(621, _OKL4_ASM_MASK__R1_TRACEPOINT_DESC) */
#define _OKL4_ASM_MASK__R1_TRACEPOINT_DESC (15 << 28)
/*lint -esym(621, _OKL4_ASM_SHIFT__R1_TRACEPOINT_DESC) */
#define _OKL4_ASM_SHIFT__R1_TRACEPOINT_DESC (28)
/*lint -esym(621, _OKL4_ASM_WIDTH__R1_TRACEPOINT_DESC) */
#define _OKL4_ASM_WIDTH__R1_TRACEPOINT_DESC (4)


/**
 *  _okl4_tracepoint_masks_t
 **/


/*lint -esym(621, _OKL4_ASM_MASK_CLASS_TRACEPOINT_MASKS) */
#define _OKL4_ASM_MASK_CLASS_TRACEPOINT_MASKS (65535)
/*lint -esym(621, _OKL4_ASM_SHIFT_CLASS_TRACEPOINT_MASKS) */
#define _OKL4_ASM_SHIFT_CLASS_TRACEPOINT_MASKS (0)
/*lint -esym(621, _OKL4_ASM_WIDTH_CLASS_TRACEPOINT_MASKS) */
#define _OKL4_ASM_WIDTH_CLASS_TRACEPOINT_MASKS (16)
/*lint -esym(621, _OKL4_ASM_MASK_SUBSYSTEM_TRACEPOINT_MASKS) */
#define _OKL4_ASM_MASK_SUBSYSTEM_TRACEPOINT_MASKS (65535 << 16)
/*lint -esym(621, _OKL4_ASM_SHIFT_SUBSYSTEM_TRACEPOINT_MASKS) */
#define _OKL4_ASM_SHIFT_SUBSYSTEM_TRACEPOINT_MASKS (16)
/*lint -esym(621, _OKL4_ASM_WIDTH_SUBSYSTEM_TRACEPOINT_MASKS) */
#define _OKL4_ASM_WIDTH_SUBSYSTEM_TRACEPOINT_MASKS (16)


/**
 *  okl4_tracepoint_evt_t
 **/
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SCH_SCHEDULER_FLAG_SET_RUNNABLE) */
#define OKL4_ASM_TRACEPOINT_EVT_SCH_SCHEDULER_FLAG_SET_RUNNABLE (0x0)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SCH_SCHEDULER_FLAG_CLEAR_RUNNABLE) */
#define OKL4_ASM_TRACEPOINT_EVT_SCH_SCHEDULER_FLAG_CLEAR_RUNNABLE (0x1)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SCH_CONTEXT_SWITCH) */
#define OKL4_ASM_TRACEPOINT_EVT_SCH_CONTEXT_SWITCH (0x2)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_KDB_SET_OBJECT_NAME) */
#define OKL4_ASM_TRACEPOINT_EVT_KDB_SET_OBJECT_NAME (0x3)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_AXON_PROCESS_RECV) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_AXON_PROCESS_RECV (0x4)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_AXON_SET_HALTED) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_AXON_SET_HALTED (0x5)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_AXON_SET_RECV_AREA) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_AXON_SET_RECV_AREA (0x6)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_AXON_SET_RECV_QUEUE) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_AXON_SET_RECV_QUEUE (0x7)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_AXON_SET_RECV_SEGMENT) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_AXON_SET_RECV_SEGMENT (0x8)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_AXON_SET_SEND_AREA) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_AXON_SET_SEND_AREA (0x9)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_AXON_SET_SEND_QUEUE) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_AXON_SET_SEND_QUEUE (0xa)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_AXON_SET_SEND_SEGMENT) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_AXON_SET_SEND_SEGMENT (0xb)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_AXON_TRIGGER_SEND) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_AXON_TRIGGER_SEND (0xc)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_ACK) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_ACK (0xd)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_ATTACH_PRIVATE) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_ATTACH_PRIVATE (0xe)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_ATTACH_SHARED) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_ATTACH_SHARED (0xf)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_DETACH) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_DETACH (0x10)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_DIST_ENABLE) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_DIST_ENABLE (0x11)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_EOI) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_EOI (0x12)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_GET_HIGHEST_PRIORITY_PENDING) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_GET_HIGHEST_PRIORITY_PENDING (0x13)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_GET_PAYLOAD) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_GET_PAYLOAD (0x14)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_LIMITS) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_LIMITS (0x15)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_MASK) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_MASK (0x16)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_RAISE) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_RAISE (0x17)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_SET_BINARY_POINT) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_SET_BINARY_POINT (0x18)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_SET_CONFIG) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_SET_CONFIG (0x19)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_SET_CONTROL) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_SET_CONTROL (0x1a)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_SET_PRIORITY) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_SET_PRIORITY (0x1b)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_SET_PRIORITY_MASK) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_SET_PRIORITY_MASK (0x1c)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_SET_TARGETS) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_SET_TARGETS (0x1d)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_UNMASK) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_INTERRUPT_UNMASK (0x1e)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_KDB_INTERACT) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_KDB_INTERACT (0x1f)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_KDB_SET_OBJECT_NAME) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_KDB_SET_OBJECT_NAME (0x20)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_KSP_PROCEDURE_CALL) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_KSP_PROCEDURE_CALL (0x21)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_ATTACH_SEGMENT) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_ATTACH_SEGMENT (0x22)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_DETACH_SEGMENT) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_DETACH_SEGMENT (0x23)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_FLUSH_RANGE) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_FLUSH_RANGE (0x24)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_FLUSH_RANGE_PN) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_FLUSH_RANGE_PN (0x25)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_LOOKUP_PAGE) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_LOOKUP_PAGE (0x26)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_LOOKUP_PN) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_LOOKUP_PN (0x27)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_MAP_PAGE) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_MAP_PAGE (0x28)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_MAP_PN) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_MAP_PN (0x29)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_UNMAP_PAGE) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_UNMAP_PAGE (0x2a)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_UNMAP_PN) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_UNMAP_PN (0x2b)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_UPDATE_PAGE_ATTRS) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_UPDATE_PAGE_ATTRS (0x2c)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_UPDATE_PAGE_PERMS) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_UPDATE_PAGE_PERMS (0x2d)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_UPDATE_PN_ATTRS) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_UPDATE_PN_ATTRS (0x2e)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_UPDATE_PN_PERMS) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_MMU_UPDATE_PN_PERMS (0x2f)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_PERFORMANCE_NULL_SYSCALL) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_PERFORMANCE_NULL_SYSCALL (0x30)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_PIPE_CONTROL) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_PIPE_CONTROL (0x31)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_PIPE_RECV) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_PIPE_RECV (0x32)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_PIPE_SEND) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_PIPE_SEND (0x33)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_PRIORITY_WAIVE) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_PRIORITY_WAIVE (0x34)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_REMOTE_GET_REGISTER) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_REMOTE_GET_REGISTER (0x35)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_REMOTE_GET_REGISTERS) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_REMOTE_GET_REGISTERS (0x36)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_REMOTE_READ_MEMORY32) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_REMOTE_READ_MEMORY32 (0x37)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_REMOTE_SET_REGISTER) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_REMOTE_SET_REGISTER (0x38)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_REMOTE_SET_REGISTERS) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_REMOTE_SET_REGISTERS (0x39)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_REMOTE_WRITE_MEMORY32) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_REMOTE_WRITE_MEMORY32 (0x3a)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_SCHEDULE_METRICS_STATUS_SUSPENDED) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_SCHEDULE_METRICS_STATUS_SUSPENDED (0x3b)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_SCHEDULE_METRICS_WATCH_SUSPENDED) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_SCHEDULE_METRICS_WATCH_SUSPENDED (0x3c)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_CPU_DISABLE) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_CPU_DISABLE (0x3d)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_CPU_ENABLE) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_CPU_ENABLE (0x3e)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_CPU_GET_DATA) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_CPU_GET_DATA (0x3f)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_VCPU_DISABLE) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_VCPU_DISABLE (0x40)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_VCPU_ENABLE) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_VCPU_ENABLE (0x41)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_VCPU_GET_DATA) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_SCHEDULE_PROFILE_VCPU_GET_DATA (0x42)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_SCHEDULER_SUSPEND) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_SCHEDULER_SUSPEND (0x43)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_TIMER_CANCEL) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_TIMER_CANCEL (0x44)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_TIMER_GET_RESOLUTION) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_TIMER_GET_RESOLUTION (0x45)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_TIMER_GET_TIME) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_TIMER_GET_TIME (0x46)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_TIMER_QUERY) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_TIMER_QUERY (0x47)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_TIMER_START) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_TIMER_START (0x48)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_TRACEBUFFER_SYNC) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_TRACEBUFFER_SYNC (0x49)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_VCPU_RESET) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_VCPU_RESET (0x4a)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_VCPU_START) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_VCPU_START (0x4b)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_VCPU_STOP) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_VCPU_STOP (0x4c)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_VCPU_SWITCH_MODE) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_VCPU_SWITCH_MODE (0x4d)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_VCPU_SYNC_SEV) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_VCPU_SYNC_SEV (0x4e)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_VCPU_SYNC_WFE) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_VCPU_SYNC_WFE (0x4f)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_VINTERRUPT_CLEAR_AND_RAISE) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_VINTERRUPT_CLEAR_AND_RAISE (0x50)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_VINTERRUPT_MODIFY) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_VINTERRUPT_MODIFY (0x51)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_SWI_VINTERRUPT_RAISE) */
#define OKL4_ASM_TRACEPOINT_EVT_SWI_VINTERRUPT_RAISE (0x52)
/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_MAX) */
#define OKL4_ASM_TRACEPOINT_EVT_MAX (0x52)
/**
    Invalid enumeration value
*/
/*lint -esym(621, OKL4_ASM_TRACEPOINT_EVT_INVALID) */
#define OKL4_ASM_TRACEPOINT_EVT_INVALID (0xffffffff)

/**
 *  okl4_tracepoint_level_t
 **/
/*lint -esym(621, OKL4_ASM_TRACEPOINT_LEVEL_DEBUG) */
#define OKL4_ASM_TRACEPOINT_LEVEL_DEBUG (0x0)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_LEVEL_INFO) */
#define OKL4_ASM_TRACEPOINT_LEVEL_INFO (0x1)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_LEVEL_WARN) */
#define OKL4_ASM_TRACEPOINT_LEVEL_WARN (0x2)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_LEVEL_CRITICAL) */
#define OKL4_ASM_TRACEPOINT_LEVEL_CRITICAL (0x3)
/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_ASM_TRACEPOINT_LEVEL_MAX) */
#define OKL4_ASM_TRACEPOINT_LEVEL_MAX (0x3)
/**
    Invalid enumeration value
*/
/*lint -esym(621, OKL4_ASM_TRACEPOINT_LEVEL_INVALID) */
#define OKL4_ASM_TRACEPOINT_LEVEL_INVALID (0xffffffff)

/**
 *  okl4_tracepoint_subsystem_t
 **/
/*lint -esym(621, OKL4_ASM_TRACEPOINT_SUBSYSTEM_SCHEDULER) */
#define OKL4_ASM_TRACEPOINT_SUBSYSTEM_SCHEDULER (0x0)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_SUBSYSTEM_TRACE) */
#define OKL4_ASM_TRACEPOINT_SUBSYSTEM_TRACE (0x1)
/*lint -esym(621, OKL4_ASM_TRACEPOINT_SUBSYSTEM_CORE) */
#define OKL4_ASM_TRACEPOINT_SUBSYSTEM_CORE (0x2)
/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_ASM_TRACEPOINT_SUBSYSTEM_MAX) */
#define OKL4_ASM_TRACEPOINT_SUBSYSTEM_MAX (0x2)
/**
    Invalid enumeration value
*/
/*lint -esym(621, OKL4_ASM_TRACEPOINT_SUBSYSTEM_INVALID) */
#define OKL4_ASM_TRACEPOINT_SUBSYSTEM_INVALID (0xffffffff)

/**
 *  okl4_vfp_ops_t
 **/
/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_ASM_VFP_OPS_MAX) */
#define OKL4_ASM_VFP_OPS_MAX (0x0)
/**
    Invalid enumeration value
*/
/*lint -esym(621, OKL4_ASM_VFP_OPS_INVALID) */
#define OKL4_ASM_VFP_OPS_INVALID (0xffffffff)

/**
 *  okl4_vservices_transport_type_t
 **/
/*lint -esym(621, OKL4_ASM_VSERVICES_TRANSPORT_TYPE_AXON) */
#define OKL4_ASM_VSERVICES_TRANSPORT_TYPE_AXON (0x0)
/*lint -esym(621, OKL4_ASM_VSERVICES_TRANSPORT_TYPE_SHARED_BUFFER) */
#define OKL4_ASM_VSERVICES_TRANSPORT_TYPE_SHARED_BUFFER (0x1)
/**
    Maximum enumeration value
*/
/*lint -esym(621, OKL4_ASM_VSERVICES_TRANSPORT_TYPE_MAX) */
#define OKL4_ASM_VSERVICES_TRANSPORT_TYPE_MAX (0x1)
/**
    Invalid enumeration value
*/
/*lint -esym(621, OKL4_ASM_VSERVICES_TRANSPORT_TYPE_INVALID) */
#define OKL4_ASM_VSERVICES_TRANSPORT_TYPE_INVALID (0xffffffff)


#endif /* !ASSEMBLY */

#endif /* __AUTO__MICROVISOR_TYPES_H__ */
/** @} */
/** @} */

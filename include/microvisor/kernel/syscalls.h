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

#ifndef __AUTO__USER_SYSCALLS_H__
#define __AUTO__USER_SYSCALLS_H__

/**
 * @cond no_doc
 */
#if defined(ASSEMBLY)
#define __hvc_str(x) x
#else
#define _hvc_str(x) #x
#define __hvc_str(x) _hvc_str(x)
#endif
#if (defined(__GNUC__) && !defined(__clang__)) && \
    (__GNUC__ < 4 || ((__GNUC__ == 4) && (__GNUC_MINOR__ < 5)))
#if defined(__thumb2__)
#define hvc(i) __hvc_str(.hword 0xf7e0 | (i & 0xf); .hword 8000 | (i >> 4) @ HVC)
#else
#define hvc(i) __hvc_str(.word 0xe1400070 | (i & 0xf) | (i >> 4 << 8) @ HVC)
#endif
#else
#if defined(__ARM_EABI__)
#if defined(ASSEMBLY) && !defined(__clang__)
    .arch_extension virt
#elif !defined(__clang__)
__asm__(
    ".arch_extension virt\n"
);
#endif
#endif
#define hvc(i) __hvc_str(hvc i)
#endif
/**
 * @endcond
 */

#if !defined(ASSEMBLY)

#define OKL4_OK OKL4_ERROR_OK

/** @} */

/*
 * Syscall prototypes.
 */

/**
 *
 * OKL4 Microvisor system call: AXON_PROCESS_RECV
 *
 * @param axon_id
 * @param transfer_limit
 *
 * @retval error
 * @retval send_empty
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE struct _okl4_sys_axon_process_recv_return
_okl4_sys_axon_process_recv(okl4_kcap_t axon_id, okl4_lsize_t transfer_limit)
{
    struct _okl4_sys_axon_process_recv_return result;

    register uint32_t r0 asm("r0") = (uint32_t)axon_id;
    register uint32_t r1 asm("r1") = (uint32_t)(transfer_limit        & 0xffffffff);
    register uint32_t r2 asm("r2") = (uint32_t)((transfer_limit >> 32) & 0xffffffff);
    __asm__ __volatile__(
            ""hvc(5184)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2)
            :
            : "cc", "memory", "r3", "r4", "r5"
            );


    result.error = (okl4_error_t)(r0);
    result.send_empty = (okl4_bool_t)(r1);
    return result;
}
#endif

#else

OKL4_FORCE_INLINE struct _okl4_sys_axon_process_recv_return
_okl4_sys_axon_process_recv(okl4_kcap_t axon_id, okl4_lsize_t transfer_limit)
{
    struct _okl4_sys_axon_process_recv_return result;

    register okl4_register_t x0 asm("x0") = (okl4_register_t)axon_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)transfer_limit;
    __asm__ __volatile__(
            "" hvc(5184) "\n\t"
            : "+r"(x0), "+r"(x1)
            :
            : "cc", "memory", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    result.error = (okl4_error_t)(x0);
    result.send_empty = (okl4_bool_t)(x1);
    return result;
}

#endif

/**
 *
 * OKL4 Microvisor system call: AXON_SET_HALTED
 *
 * @param axon_id
 * @param halted
 *
 * @retval error
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_axon_set_halted(okl4_kcap_t axon_id, okl4_bool_t halted)
{
    register uint32_t r0 asm("r0") = (uint32_t)axon_id;
    register uint32_t r1 asm("r1") = (uint32_t)halted;
    __asm__ __volatile__(
            ""hvc(5186)"\n\t"
            : "+r"(r0), "+r"(r1)
            :
            : "cc", "memory", "r2", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_axon_set_halted(okl4_kcap_t axon_id, okl4_bool_t halted)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)axon_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)halted;
    __asm__ __volatile__(
            "" hvc(5186) "\n\t"
            : "+r"(x0), "+r"(x1)
            :
            : "cc", "memory", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * OKL4 Microvisor system call: AXON_SET_RECV_AREA
 *
 * @param axon_id
 * @param base
 * @param size
 *
 * @retval error
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_axon_set_recv_area(okl4_kcap_t axon_id, okl4_laddr_t base,
        okl4_lsize_t size)
{
    register uint32_t r0 asm("r0") = (uint32_t)axon_id;
    register uint32_t r1 asm("r1") = (uint32_t)(base        & 0xffffffff);
    register uint32_t r2 asm("r2") = (uint32_t)((base >> 32) & 0xffffffff);
    register uint32_t r3 asm("r3") = (uint32_t)(size        & 0xffffffff);
    register uint32_t r4 asm("r4") = (uint32_t)((size >> 32) & 0xffffffff);
    __asm__ __volatile__(
            ""hvc(5187)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3), "+r"(r4)
            :
            : "cc", "memory", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_axon_set_recv_area(okl4_kcap_t axon_id, okl4_laddr_t base,
        okl4_lsize_t size)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)axon_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)base;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)size;
    __asm__ __volatile__(
            "" hvc(5187) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2)
            :
            : "cc", "memory", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * OKL4 Microvisor system call: AXON_SET_RECV_QUEUE
 *
 * @param axon_id
 * @param queue
 *
 * @retval error
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_axon_set_recv_queue(okl4_kcap_t axon_id, okl4_laddr_t queue)
{
    register uint32_t r0 asm("r0") = (uint32_t)axon_id;
    register uint32_t r1 asm("r1") = (uint32_t)(queue        & 0xffffffff);
    register uint32_t r2 asm("r2") = (uint32_t)((queue >> 32) & 0xffffffff);
    __asm__ __volatile__(
            ""hvc(5188)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2)
            :
            : "cc", "memory", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_axon_set_recv_queue(okl4_kcap_t axon_id, okl4_laddr_t queue)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)axon_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)queue;
    __asm__ __volatile__(
            "" hvc(5188) "\n\t"
            : "+r"(x0), "+r"(x1)
            :
            : "cc", "memory", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * OKL4 Microvisor system call: AXON_SET_RECV_SEGMENT
 *
 * @param axon_id
 * @param segment_id
 * @param segment_base
 *
 * @retval error
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_axon_set_recv_segment(okl4_kcap_t axon_id, okl4_kcap_t segment_id,
        okl4_laddr_t segment_base)
{
    register uint32_t r0 asm("r0") = (uint32_t)axon_id;
    register uint32_t r1 asm("r1") = (uint32_t)segment_id;
    register uint32_t r2 asm("r2") = (uint32_t)(segment_base        & 0xffffffff);
    register uint32_t r3 asm("r3") = (uint32_t)((segment_base >> 32) & 0xffffffff);
    __asm__ __volatile__(
            ""hvc(5189)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3)
            :
            : "cc", "memory", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_axon_set_recv_segment(okl4_kcap_t axon_id, okl4_kcap_t segment_id,
        okl4_laddr_t segment_base)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)axon_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)segment_id;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)segment_base;
    __asm__ __volatile__(
            "" hvc(5189) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2)
            :
            : "cc", "memory", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * OKL4 Microvisor system call: AXON_SET_SEND_AREA
 *
 * @param axon_id
 * @param base
 * @param size
 *
 * @retval error
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_axon_set_send_area(okl4_kcap_t axon_id, okl4_laddr_t base,
        okl4_lsize_t size)
{
    register uint32_t r0 asm("r0") = (uint32_t)axon_id;
    register uint32_t r1 asm("r1") = (uint32_t)(base        & 0xffffffff);
    register uint32_t r2 asm("r2") = (uint32_t)((base >> 32) & 0xffffffff);
    register uint32_t r3 asm("r3") = (uint32_t)(size        & 0xffffffff);
    register uint32_t r4 asm("r4") = (uint32_t)((size >> 32) & 0xffffffff);
    __asm__ __volatile__(
            ""hvc(5190)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3), "+r"(r4)
            :
            : "cc", "memory", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_axon_set_send_area(okl4_kcap_t axon_id, okl4_laddr_t base,
        okl4_lsize_t size)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)axon_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)base;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)size;
    __asm__ __volatile__(
            "" hvc(5190) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2)
            :
            : "cc", "memory", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * OKL4 Microvisor system call: AXON_SET_SEND_QUEUE
 *
 * @param axon_id
 * @param queue
 *
 * @retval error
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_axon_set_send_queue(okl4_kcap_t axon_id, okl4_laddr_t queue)
{
    register uint32_t r0 asm("r0") = (uint32_t)axon_id;
    register uint32_t r1 asm("r1") = (uint32_t)(queue        & 0xffffffff);
    register uint32_t r2 asm("r2") = (uint32_t)((queue >> 32) & 0xffffffff);
    __asm__ __volatile__(
            ""hvc(5191)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2)
            :
            : "cc", "memory", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_axon_set_send_queue(okl4_kcap_t axon_id, okl4_laddr_t queue)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)axon_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)queue;
    __asm__ __volatile__(
            "" hvc(5191) "\n\t"
            : "+r"(x0), "+r"(x1)
            :
            : "cc", "memory", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * OKL4 Microvisor system call: AXON_SET_SEND_SEGMENT
 *
 * @param axon_id
 * @param segment_id
 * @param segment_base
 *
 * @retval error
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_axon_set_send_segment(okl4_kcap_t axon_id, okl4_kcap_t segment_id,
        okl4_laddr_t segment_base)
{
    register uint32_t r0 asm("r0") = (uint32_t)axon_id;
    register uint32_t r1 asm("r1") = (uint32_t)segment_id;
    register uint32_t r2 asm("r2") = (uint32_t)(segment_base        & 0xffffffff);
    register uint32_t r3 asm("r3") = (uint32_t)((segment_base >> 32) & 0xffffffff);
    __asm__ __volatile__(
            ""hvc(5192)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3)
            :
            : "cc", "memory", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_axon_set_send_segment(okl4_kcap_t axon_id, okl4_kcap_t segment_id,
        okl4_laddr_t segment_base)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)axon_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)segment_id;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)segment_base;
    __asm__ __volatile__(
            "" hvc(5192) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2)
            :
            : "cc", "memory", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * OKL4 Microvisor system call: AXON_TRIGGER_SEND
 *
 * @param axon_id
 *
 * @retval error
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_axon_trigger_send(okl4_kcap_t axon_id)
{
    register uint32_t r0 asm("r0") = (uint32_t)axon_id;
    __asm__ __volatile__(
            ""hvc(5185)"\n\t"
            : "+r"(r0)
            :
            : "cc", "memory", "r1", "r2", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_axon_trigger_send(okl4_kcap_t axon_id)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)axon_id;
    __asm__ __volatile__(
            "" hvc(5185) "\n\t"
            : "+r"(x0)
            :
            : "cc", "memory", "x1", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Acknowledge the delivery of an interrupt.
 *
 *    @details
 *    This API returns the number and source of the highest-priority
 *        enabled,
 *    pending and inactive interrupt that is targeted at the calling vCPU
 *    and has higher priority than the calling vCPU's running group
 *        priority.
 *
 *    The returned interrupt is marked as active, and will not be returned
 *        again
 *    by this function until @ref okl4_sys_interrupt_eoi is invoked
 *        specifying the
 *    same interrupt number and source. The vCPU's running interrupt
 *        priority is
 *    raised to the priority of the returned interrupt. This will typically
 *        result
 *    in the de-assertion of the vCPU's virtual IRQ line.
 *
 *    If no such interrupt exists, interrupt number 1023 is returned. If
 *        the
 *    returned interrupt number is 16 or greater, the source ID is 0;
 *        otherwise it
 *    is the vCPU ID of the vCPU that raised the interrupt (which is always
 *        in the
 *    same Cell as the caller).
 *
 *    @note Invoking this API is equivalent to reading from the GIC CPU
 *    Interface's Interrupt Acknowledge Register (\p GICC_IAR).
 *
 *
 * @retval irq
 *    An interrupt line number for the virtual GIC.
 * @retval source
 *    The ID of the originating vCPU of a Software-Generated Interrupt.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE struct _okl4_sys_interrupt_ack_return
_okl4_sys_interrupt_ack(void)
{
    struct _okl4_sys_interrupt_ack_return result;

    register uint32_t r0 asm("r0");
    register uint32_t r1 asm("r1");
    __asm__ __volatile__(
            ""hvc(5128)"\n\t"
            : "=r"(r0), "=r"(r1)
            :
            : "cc", "memory", "r2", "r3", "r4", "r5"
            );


    result.irq = (okl4_interrupt_number_t)(r0);
    result.source = (uint8_t)(r1);
    return result;
}
#endif

#else

OKL4_FORCE_INLINE struct _okl4_sys_interrupt_ack_return
_okl4_sys_interrupt_ack(void)
{
    struct _okl4_sys_interrupt_ack_return result;

    register okl4_register_t x0 asm("x0");
    register okl4_register_t x1 asm("x1");
    __asm__ __volatile__(
            "" hvc(5128) "\n\t"
            : "=r"(x0), "=r"(x1)
            :
            : "cc", "memory", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    result.irq = (okl4_interrupt_number_t)(x0);
    result.source = (uint8_t)(x1);
    return result;
}

#endif

/**
 *
 * @brief Register a vCPU as the handler of an interrupt.
 *
 *    @details
 *    The Microvisor virtual GIC API permits an interrupt source to be
 *        dynamically
 *    assigned to a specific IRQ number in a Cell or vCPU. An interrupt can
 *        only
 *    be assigned to one IRQ number, and one Cell or vCPU, at a time. This
 *    operation attaches the interrupt to a vCPU as a private interrupt.
 *
 *    Interrupt sources are addressed using capabilities. This operation,
 *        given
 *    a capability for an interrupt that is not currently attached to any
 *        handler,
 *    can attach the interrupt at a given unused IRQ number. If the IRQ
 *        number
 *    is between 16 and 31 (the GIC Private Peripheral Interrupt range), it
 *        will
 *    be attached to the specified vCPU; if it is between 32 and 1019 (the
 *        GIC
 *    Shared Peripheral Interrupt range), it will return an error.
 *
 *    @note The Software Generated Interrupt range, from 0 to 15, is
 *        reserved
 *    and cannot be used to attach interrupt source capabilities.
 *
 *    @note In most cases, interrupt sources are attached at system
 *        construction
 *    time by the OK Tool. It is not normally necessary to attach an
 *        interrupt
 *    source before using it.
 *
 * @param vcpu_cap
 *    A virtual CPU capability.
 * @param irq_cap
 *    A virtual interrupt capability.
 * @param irq_num
 *    An interrupt line number for the virtual GIC.
 *
 * @retval error
 *    The resulting error value.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_attach_private(okl4_kcap_t vcpu_cap, okl4_kcap_t irq_cap,
        okl4_interrupt_number_t irq_num)
{
    register uint32_t r0 asm("r0") = (uint32_t)vcpu_cap;
    register uint32_t r1 asm("r1") = (uint32_t)irq_cap;
    register uint32_t r2 asm("r2") = (uint32_t)irq_num;
    __asm__ __volatile__(
            ""hvc(5134)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2)
            :
            : "cc", "memory", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_attach_private(okl4_kcap_t vcpu_cap, okl4_kcap_t irq_cap,
        okl4_interrupt_number_t irq_num)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)vcpu_cap;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)irq_cap;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)irq_num;
    __asm__ __volatile__(
            "" hvc(5134) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2)
            :
            : "cc", "memory", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Register a Cell (domain) as the handler of an interrupt.
 *
 *    @details
 *    The Microvisor virtual GIC API permits an interrupt source to be
 *        dynamically
 *    assigned to a specific IRQ number in a Cell or vCPU. An interrupt can
 *        only
 *    be assigned to one IRQ number, and one Cell or vCPU, at a time. This
 *    operation attaches the interrupt to a Cell as a shared interrupt.
 *
 *    Interrupt sources are addressed using capabilities. This operation,
 *        given
 *    a capability for an interrupt that is not currently attached to any
 *        handler,
 *    can attach the interrupt at a given unused IRQ number. If the IRQ
 *        number
 *    is between 0 and 31 (the GIC SGI or Private Peripheral Interrupt
 *        range), it
 *    will return an error; if it is between 32 and 1019 (the GIC
 *    Shared Peripheral Interrupt range), it will be attached to the
 *        specified
 *    Cell.
 *
 *    @note In most cases, interrupt sources are attached at system
 *        construction
 *    time by the OK Tool. It is not normally necessary to attach an
 *        interrupt
 *    source before using it.
 *
 * @param domain_cap
 *    A domain capability.
 * @param irq_cap
 *    A virtual interrupt capability.
 * @param irq_num
 *    An interrupt line number for the virtual GIC.
 *
 * @retval error
 *    The resulting error value.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_attach_shared(okl4_kcap_t domain_cap, okl4_kcap_t irq_cap,
        okl4_interrupt_number_t irq_num)
{
    register uint32_t r0 asm("r0") = (uint32_t)domain_cap;
    register uint32_t r1 asm("r1") = (uint32_t)irq_cap;
    register uint32_t r2 asm("r2") = (uint32_t)irq_num;
    __asm__ __volatile__(
            ""hvc(5135)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2)
            :
            : "cc", "memory", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_attach_shared(okl4_kcap_t domain_cap, okl4_kcap_t irq_cap,
        okl4_interrupt_number_t irq_num)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)domain_cap;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)irq_cap;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)irq_num;
    __asm__ __volatile__(
            "" hvc(5135) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2)
            :
            : "cc", "memory", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Unregister an interrupt.
 *
 *    @details
 *    Detach the given interrupt source from its registered handler. The
 *        interrupt
 *    will be deactivated and disabled, and will not be delivered again
 *        until it
 *    is reattached. However, if it is configured in edge triggering mode,
 *        its
 *    pending state will be preserved.
 *
 * @param irq_cap
 *    A virtual interrupt capability.
 *
 * @retval error
 *    The resulting error value.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_detach(okl4_kcap_t irq_cap)
{
    register uint32_t r0 asm("r0") = (uint32_t)irq_cap;
    __asm__ __volatile__(
            ""hvc(5136)"\n\t"
            : "+r"(r0)
            :
            : "cc", "memory", "r1", "r2", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_detach(okl4_kcap_t irq_cap)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)irq_cap;
    __asm__ __volatile__(
            "" hvc(5136) "\n\t"
            : "+r"(x0)
            :
            : "cc", "memory", "x1", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Enable the interrupt distributor.
 *
 *    @details
 *    This API enables the interrupt distributor, in the same form as
 *        writing to
 *    the enable bit in (\p GICD_CTLR).
 *
 * @param enable
 *    A boolean value for GIC distributor enable.
 *
 * @retval error
 *    The resulting error value.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_dist_enable(okl4_bool_t enable)
{
    register uint32_t r0 asm("r0") = (uint32_t)enable;
    __asm__ __volatile__(
            ""hvc(5133)"\n\t"
            : "+r"(r0)
            :
            : "cc", "memory", "r1", "r2", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_dist_enable(okl4_bool_t enable)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)enable;
    __asm__ __volatile__(
            "" hvc(5133) "\n\t"
            : "+r"(x0)
            :
            : "cc", "memory", "x1", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Signal the end of the interrupt handling routine.
 *
 *    @details
 *    This API informs the virtual GIC that handling for a given interrupt
 *        has
 *    completed. It marks the interrupt as inactive, and decreases the
 *        running
 *    interrupt priority of the calling vCPU. This may cause immediate
 *        delivery of
 *    another interrupt, possibly with the same number, if one is enabled
 *        and
 *    pending.
 *
 *    The specified interrupt number and source must match the active
 *        interrupt
 *    that was most recently returned by an @ref okl4_sys_interrupt_ack
 *    invocation. If multiple interrupts have been acknowledged and not yet
 *        ended,
 *    they must be ended in the reversed order of their acknowledgement.
 *
 *    @note Invoking this API is equivalent to writing to the GIC CPU
 *    Interface's End of Interrupt Register (\p GICC_EOIR), with \p EOImode
 *    set to 0 in \p GICC_CTLR.
 *
 * @param irq
 *    An interrupt line number for the virtual GIC.
 * @param source
 *    The ID of the originating vCPU of a Software-Generated Interrupt.
 *
 * @retval error
 *    The resulting error value.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_eoi(okl4_interrupt_number_t irq, uint8_t source)
{
    register uint32_t r0 asm("r0") = (uint32_t)irq;
    register uint32_t r1 asm("r1") = (uint32_t)source;
    __asm__ __volatile__(
            ""hvc(5129)"\n\t"
            : "+r"(r0), "+r"(r1)
            :
            : "cc", "memory", "r2", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_eoi(okl4_interrupt_number_t irq, uint8_t source)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)irq;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)source;
    __asm__ __volatile__(
            "" hvc(5129) "\n\t"
            : "+r"(x0), "+r"(x1)
            :
            : "cc", "memory", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Retrieve the highest-priority pending interrupt.
 *
 *    @details
 *    This API returns the number and source of the highest-priority
 *        enabled,
 *    pending and inactive interrupt that is targeted at the calling vCPU
 *    and has higher priority than the calling vCPU's running group
 *        priority.
 *
 *    If no such interrupt exists, interrupt number 1023 is returned. If
 *        the
 *    returned interrupt number is 16 or greater, the source ID is 0;
 *        otherwise it
 *    is the vCPU ID of the vCPU that raised the interrupt (which is always
 *        in the
 *    same Cell as the caller).
 *
 *    @note Invoking this API is equivalent to reading from the GIC CPU
 *    Interface's Highest Priority Pending Interrupt Register (\p
 *        GICC_HPPIR).
 *
 *
 * @retval irq
 *    An interrupt line number for the virtual GIC.
 * @retval source
 *    The ID of the originating vCPU of a Software-Generated Interrupt.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE struct _okl4_sys_interrupt_get_highest_priority_pending_return
_okl4_sys_interrupt_get_highest_priority_pending(void)
{
    struct _okl4_sys_interrupt_get_highest_priority_pending_return result;

    register uint32_t r0 asm("r0");
    register uint32_t r1 asm("r1");
    __asm__ __volatile__(
            ""hvc(5137)"\n\t"
            : "=r"(r0), "=r"(r1)
            :
            : "cc", "memory", "r2", "r3", "r4", "r5"
            );


    result.irq = (okl4_interrupt_number_t)(r0);
    result.source = (uint8_t)(r1);
    return result;
}
#endif

#else

OKL4_FORCE_INLINE struct _okl4_sys_interrupt_get_highest_priority_pending_return
_okl4_sys_interrupt_get_highest_priority_pending(void)
{
    struct _okl4_sys_interrupt_get_highest_priority_pending_return result;

    register okl4_register_t x0 asm("x0");
    register okl4_register_t x1 asm("x1");
    __asm__ __volatile__(
            "" hvc(5137) "\n\t"
            : "=r"(x0), "=r"(x1)
            :
            : "cc", "memory", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    result.irq = (okl4_interrupt_number_t)(x0);
    result.source = (uint8_t)(x1);
    return result;
}

#endif

/**
 *
 * @brief Fetch the payload flags of a virtual interrupt.
 *
 *    @details
 *    This fetches and clears the accumulated payload flags for a virtual
 *    interrupt that has been raised by the Microvisor, or by a vCPU
 *        invoking
 *    the @ref okl4_sys_vinterrupt_raise API.
 *
 *    If the virtual interrupt is configured for level triggering, clearing
 *        the
 *    accumulated flags by calling this function will also clear the
 *        pending state
 *    of the interrupt.
 *
 * @param irq
 *    An interrupt line number for the virtual GIC.
 *
 * @retval error
 *    The resulting error value.
 * @retval payload
 *    Accumulated virtual interrupt payload flags.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE struct _okl4_sys_interrupt_get_payload_return
_okl4_sys_interrupt_get_payload(okl4_interrupt_number_t irq)
{
    typedef union {
        struct uint64 {
            uint32_t lo;
            uint32_t hi;
        } words;
        uint64_t val;
    } okl4_uint64_tmp;
    okl4_uint64_tmp payload_tmp;
    struct _okl4_sys_interrupt_get_payload_return result;

    register uint32_t r0 asm("r0") = (uint32_t)irq;
    register uint32_t r1 asm("r1");
    register uint32_t r2 asm("r2");
    __asm__ __volatile__(
            ""hvc(5132)"\n\t"
            : "=r"(r1), "=r"(r2), "+r"(r0)
            :
            : "cc", "memory", "r3", "r4", "r5"
            );


    result.error = (okl4_error_t)(r0);
    payload_tmp.words.lo = r1;
    payload_tmp.words.hi = r2;
    result.payload = (okl4_virq_flags_t)(payload_tmp.val);
    return result;
}
#endif

#else

OKL4_FORCE_INLINE struct _okl4_sys_interrupt_get_payload_return
_okl4_sys_interrupt_get_payload(okl4_interrupt_number_t irq)
{
    struct _okl4_sys_interrupt_get_payload_return result;

    register okl4_register_t x0 asm("x0") = (okl4_register_t)irq;
    register okl4_register_t x1 asm("x1");
    __asm__ __volatile__(
            "" hvc(5132) "\n\t"
            : "=r"(x1), "+r"(x0)
            :
            : "cc", "memory", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    result.error = (okl4_error_t)(x0);
    result.payload = (okl4_virq_flags_t)(x1);
    return result;
}

#endif

/**
 *
 * @brief Query the number of supported CPUs and interrupt lines.
 *
 *    @details
 *    This API returns the number of CPUs and interrupt lines supported by
 *        the
 *    virtual interrupt controller, in the same form as is found in the GIC
 *    Distributor's Interrupt Controller Type Register (\p GICD_TYPER), in
 *    the \p CPUNumber and \p ITLinesNumber fields.
 *
 *
 * @retval cpunumber
 *    The number of supported target CPUs, minus 1.
 * @retval itnumber
 *    The number of supported groups of 32 interrupt lines, minus 1.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE struct _okl4_sys_interrupt_limits_return
_okl4_sys_interrupt_limits(void)
{
    struct _okl4_sys_interrupt_limits_return result;

    register uint32_t r0 asm("r0");
    register uint32_t r1 asm("r1");
    __asm__ __volatile__(
            ""hvc(5138)"\n\t"
            : "=r"(r0), "=r"(r1)
            :
            : "cc", "memory", "r2", "r3", "r4", "r5"
            );


    result.cpunumber = (okl4_count_t)(r0);
    result.itnumber = (okl4_count_t)(r1);
    return result;
}
#endif

#else

OKL4_FORCE_INLINE struct _okl4_sys_interrupt_limits_return
_okl4_sys_interrupt_limits(void)
{
    struct _okl4_sys_interrupt_limits_return result;

    register okl4_register_t x0 asm("x0");
    register okl4_register_t x1 asm("x1");
    __asm__ __volatile__(
            "" hvc(5138) "\n\t"
            : "=r"(x0), "=r"(x1)
            :
            : "cc", "memory", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    result.cpunumber = (okl4_count_t)(x0);
    result.itnumber = (okl4_count_t)(x1);
    return result;
}

#endif

/**
 *
 * @brief Disable delivery of an interrupt.
 *
 *    @detail
 *    This prevents future delivery of the specified interrupt. It does not
 *    affect any currently active delivery (that is, end-of-interrupt must
 *    still be called). It also does not affect the pending state, so it
 *        cannot
 *    cause loss of edge-triggered interrupts.
 *
 *    @note Invoking this API is equivalent to writing a single bit to one
 *        of the
 *    GIC Distributor's Interrupt Clear-Enable Registers (\p
 *        GICD_ICENABLERn).
 *
 * @param irq
 *    An interrupt line number for the virtual GIC.
 *
 * @retval error
 *    The resulting error value.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_mask(okl4_interrupt_number_t irq)
{
    register uint32_t r0 asm("r0") = (uint32_t)irq;
    __asm__ __volatile__(
            ""hvc(5130)"\n\t"
            : "+r"(r0)
            :
            : "cc", "memory", "r1", "r2", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_mask(okl4_interrupt_number_t irq)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)irq;
    __asm__ __volatile__(
            "" hvc(5130) "\n\t"
            : "+r"(x0)
            :
            : "cc", "memory", "x1", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Raise a Software-Generated Interrupt.
 *
 *    @detail
 *    This allows a Software-Generated Interrupt (with interrupt number
 *        between
 *    0 and 15) to be raised, targeted at a specified set of vCPUs within
 *        the
 *    same Cell. No capability is required, but interrupts cannot be raised
 *        to
 *    other Cells with this API.
 *
 *    @note Invoking this API is equivalent to writing to the GIC
 *        Distributor's
 *    Software Generated Interrupt Register (\p GICD_SGIR).
 *
 *    @note This API is distinct from the @ref okl4_sys_vinterrupt_raise
 *        API,
 *    which raises a virtual interrupt source which may communicate across
 *    Cell boundaries, and requires an explicit capability.
 *
 * @param sgir
 *    A description of the Software-Generated Interrupt to raise.
 *
 * @retval error
 *    The resulting error value.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_raise(okl4_gicd_sgir_t sgir)
{
    register uint32_t r0 asm("r0") = (uint32_t)sgir;
    __asm__ __volatile__(
            ""hvc(5145)"\n\t"
            : "+r"(r0)
            :
            : "cc", "memory", "r1", "r2", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_raise(okl4_gicd_sgir_t sgir)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)sgir;
    __asm__ __volatile__(
            "" hvc(5145) "\n\t"
            : "+r"(x0)
            :
            : "cc", "memory", "x1", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Set the interrupt priority binary point for the calling vCPU.
 *
 *    @details
 *    The GIC splits IRQ priority values into two subfields: the group
 *        priority
 *    and the subpriority. The binary point is the index of the most
 *        significant
 *    bit of the subpriority (that is, one less than the number of
 *        subpriority
 *    bits).
 *
 *    An interrupt can preempt another active interrupt only if its group
 *        priority
 *    is higher than the running group priority; the subpriority is ignored
 *        for
 *    this comparison. The subpriority is used to determine which of two
 *        equal
 *    priority interrupts will be delivered first.
 *
 *    @note Invoking this API is equivalent to writing to the GIC CPU
 *    Interface's Binary Point Register (\p GICC_BPR).
 *
 * @param binary_point
 *    The number of bits in the subpriority field, minus 1.
 *
 * @retval error
 *    The resulting error value.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_set_binary_point(uint8_t binary_point)
{
    register uint32_t r0 asm("r0") = (uint32_t)binary_point;
    __asm__ __volatile__(
            ""hvc(5139)"\n\t"
            : "+r"(r0)
            :
            : "cc", "memory", "r1", "r2", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_set_binary_point(uint8_t binary_point)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)binary_point;
    __asm__ __volatile__(
            "" hvc(5139) "\n\t"
            : "+r"(x0)
            :
            : "cc", "memory", "x1", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Change the configuration of an interrupt.
 *
 *    @detail
 *    This sets the triggering type of a specified interrupt to either
 *    edge or level triggering.
 *
 *    The specified interrupt must be disabled.
 *
 *    @note Some interrupt sources only support one triggering type. In
 *        this case,
 *    calling this API for the interrupt will have no effect.
 *
 *    @note Invoking this API is equivalent to writing a single two-bit
 *        field of
 *    one of the GIC Distributor's Interrupt Configuration Registers (\p
 *    GICD_ICFGRn).
 *
 * @param irq
 *    An interrupt line number for the virtual GIC.
 * @param icfgr
 *    The configuration bits for the interrupt line.
 *
 * @retval error
 *    The resulting error value.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_set_config(okl4_interrupt_number_t irq,
        okl4_gicd_icfgr_t icfgr)
{
    register uint32_t r0 asm("r0") = (uint32_t)irq;
    register uint32_t r1 asm("r1") = (uint32_t)icfgr;
    __asm__ __volatile__(
            ""hvc(5140)"\n\t"
            : "+r"(r0), "+r"(r1)
            :
            : "cc", "memory", "r2", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_set_config(okl4_interrupt_number_t irq,
        okl4_gicd_icfgr_t icfgr)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)irq;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)icfgr;
    __asm__ __volatile__(
            "" hvc(5140) "\n\t"
            : "+r"(x0), "+r"(x1)
            :
            : "cc", "memory", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Enable or disable the signaling of interrupts to the vCPU.
 *
 *    @details
 *    Enable or disable the signaling of interrupts by the virtual CPU
 *        interface
 *    to the connected vCPU.
 *
 *    @note Interrupt signalling is initially disabled, as required by the
 *        GIC
 *    API specification. This API must therefore be invoked at least once
 *        before
 *    any interrupts will be delivered.
 *
 *    @note Invoking this API is equivalent to writing to the GIC CPU
 *    Interface's Control Register (\p GICC_CTLR) using the "GICv1 without
 *    Security Extensions or Non-Secure" format, which contains only a
 *        single
 *    enable bit.
 *
 * @param enable
 *    A boolean value for GIC distributor enable.
 *
 * @retval error
 *    The resulting error value.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_set_control(okl4_bool_t enable)
{
    register uint32_t r0 asm("r0") = (uint32_t)enable;
    __asm__ __volatile__(
            ""hvc(5141)"\n\t"
            : "+r"(r0)
            :
            : "cc", "memory", "r1", "r2", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_set_control(okl4_bool_t enable)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)enable;
    __asm__ __volatile__(
            "" hvc(5141) "\n\t"
            : "+r"(x0)
            :
            : "cc", "memory", "x1", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Change the delivery priority of an interrupt.
 *
 *    @detail
 *    This changes the delivery priority of an interrupt. It has no
 *        immediate
 *    effect on currently active interrupts, but will take effect once the
 *    interrupt is deactivated.
 *
 *    @note The number of significant bits in this value is
 *    implementation-defined. In this configuration, 4 significant priority
 *    bits are implemented. The most significant bit is always at the high
 *        end
 *    of the priority byte; that is, at bit 7.
 *
 *    @note Smaller values represent higher priority. The highest possible
 *    priority is 0; the lowest possible priority has all implemented bits
 *        set,
 *    and in this implementation is currently 0xf0.
 *
 *    @note Invoking this API is equivalent to writing a single byte of one
 *        of the
 *    GIC Distributor's Interrupt Priority Registers (\p GICD_IPRIORITYn).
 *
 * @param irq
 *    An interrupt line number for the virtual GIC.
 * @param priority
 *    A GIC priority value in the range 0-240.
 *
 * @retval error
 *    The resulting error value.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_set_priority(okl4_interrupt_number_t irq, uint8_t priority)
{
    register uint32_t r0 asm("r0") = (uint32_t)irq;
    register uint32_t r1 asm("r1") = (uint32_t)priority;
    __asm__ __volatile__(
            ""hvc(5142)"\n\t"
            : "+r"(r0), "+r"(r1)
            :
            : "cc", "memory", "r2", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_set_priority(okl4_interrupt_number_t irq, uint8_t priority)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)irq;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)priority;
    __asm__ __volatile__(
            "" hvc(5142) "\n\t"
            : "+r"(x0), "+r"(x1)
            :
            : "cc", "memory", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Set the minimum interrupt priority of the calling vCPU.
 *
 *    @details
 *    This API sets the calling vCPU's minimum running interrupt priority.
 *    Interrupts will only be delivered if they have priority higher than
 *        this
 *    value.
 *
 *    @note Higher priority corresponds to a lower priority value; i.e.,
 *        the
 *    highest priority value is 0.
 *
 *    @note The priority mask is initially set to 0, which prevents all
 *        interrupt
 *    delivery, as required by the GIC API specification. This API must
 *        therefore
 *    be invoked at least once before any interrupts will be delivered.
 *
 *    @note Invoking this API is equivalent to writing to the GIC CPU
 *    Interface's Interrupt Priority Mask Register (\p GICC_PMR).
 *
 * @param priority_mask
 *    A GIC priority value in the range 0-240.
 *
 * @retval error
 *    The resulting error value.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_set_priority_mask(uint8_t priority_mask)
{
    register uint32_t r0 asm("r0") = (uint32_t)priority_mask;
    __asm__ __volatile__(
            ""hvc(5143)"\n\t"
            : "+r"(r0)
            :
            : "cc", "memory", "r1", "r2", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_set_priority_mask(uint8_t priority_mask)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)priority_mask;
    __asm__ __volatile__(
            "" hvc(5143) "\n\t"
            : "+r"(x0)
            :
            : "cc", "memory", "x1", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Change the delivery targets of a shared interrupt.
 *
 *    @detail
 *    This sets the subset of a Cell's vCPUs to which the specified shared
 *    interrupt (with an interrupt number between 32 and 1019) can be
 *        delivered.
 *    The target vCPUs are specified by an 8-bit bitfield. Note that no
 *        more
 *    than 8 targets are supported by the GIC API, so vCPUs with IDs beyond
 *        8
 *    will never receive interrupts.
 *
 *    @note The GIC API does not specify how or when the implementation
 *        selects a
 *    target for interrupt delivery. Most hardware implementations deliver
 *        to
 *    all possible targets simultaneously, and then cancel all but the
 *        first to
 *    be acknowledged. In the interests of efficiency, the OKL4 Microvisor
 *        does
 *    not implement this behaviour; instead, it chooses an arbitrary target
 *        when
 *    the interrupt first becomes deliverable.
 *
 *    @note Invoking this API is equivalent to writing a single byte of one
 *        of the
 *    GIC Distributor's Interrupt Targets Registers (\p GICD_ITARGETSRn).
 *
 * @param irq
 *    An interrupt line number for the virtual GIC.
 * @param cpu_mask
 *    Bitmask of vCPU IDs.
 *
 * @retval error
 *    The resulting error value.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_set_targets(okl4_interrupt_number_t irq, uint8_t cpu_mask)
{
    register uint32_t r0 asm("r0") = (uint32_t)irq;
    register uint32_t r1 asm("r1") = (uint32_t)cpu_mask;
    __asm__ __volatile__(
            ""hvc(5144)"\n\t"
            : "+r"(r0), "+r"(r1)
            :
            : "cc", "memory", "r2", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_set_targets(okl4_interrupt_number_t irq, uint8_t cpu_mask)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)irq;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)cpu_mask;
    __asm__ __volatile__(
            "" hvc(5144) "\n\t"
            : "+r"(x0), "+r"(x1)
            :
            : "cc", "memory", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Enable delivery of an interrupt.
 *
 *    @detail
 *    This permits delivery of the specified interrupt, once it is pending
 *        and
 *    inactive and has sufficiently high priority.
 *
 *    @note Invoking this API is equivalent to writing a single bit to one
 *        of the
 *    GIC Distributor's Interrupt Set-Enable Registers (\p
 *        GICD_ISENABLERn).
 *
 * @param irq
 *    An interrupt line number for the virtual GIC.
 *
 * @retval error
 *    The resulting error value.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_unmask(okl4_interrupt_number_t irq)
{
    register uint32_t r0 asm("r0") = (uint32_t)irq;
    __asm__ __volatile__(
            ""hvc(5131)"\n\t"
            : "+r"(r0)
            :
            : "cc", "memory", "r1", "r2", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_interrupt_unmask(okl4_interrupt_number_t irq)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)irq;
    __asm__ __volatile__(
            "" hvc(5131) "\n\t"
            : "+r"(x0)
            :
            : "cc", "memory", "x1", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Enter the kernel interactive debugger.
 *
 * @details
 * This is available on a debug build of the kernel, otherwise the operation
 *     is a
 * no-op.
 *
 *
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE void
_okl4_sys_kdb_interact(void)
{
    __asm__ __volatile__(
            ""hvc(5120)"\n\t"
            :
            :
            : "cc", "memory", "r0", "r1", "r2", "r3", "r4", "r5"
            );

}
#endif

#else

OKL4_FORCE_INLINE void
_okl4_sys_kdb_interact(void)
{
    __asm__ __volatile__(
            "" hvc(5120) "\n\t"
            :
            :
            : "cc", "memory", "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7"
            );

}

#endif

/**
 *
 * @brief Set the debug name of the addressed kernel object.
 *
 *    @details
 *    The debug version of the Microvisor kernel supports naming of kernel
 *        objects
 *    to aid debugging. The object names are visible to external debuggers
 *        such
 *    as a JTAG tool, as well as the in-built interactive kernel debugger.
 *
 *    The target object may be any Microvisor object for which the caller
 *        has a
 *    capability with the master rights.
 *
 *    Debug names may be up to 16 characters long, with four characters
 *        stored per
 *    \p name[x] argument in little-endian order (on a 32-bit machine).
 *
 * @param object
 *    The target kernel object id.
 * @param name0
 * @param name1
 * @param name2
 * @param name3
 *
 * @retval error
 *    Resulting error
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_kdb_set_object_name(okl4_kcap_t object, uint32_t name0, uint32_t name1
        , uint32_t name2, uint32_t name3)
{
    register uint32_t r0 asm("r0") = (uint32_t)object;
    register uint32_t r1 asm("r1") = (uint32_t)name0;
    register uint32_t r2 asm("r2") = (uint32_t)name1;
    register uint32_t r3 asm("r3") = (uint32_t)name2;
    register uint32_t r4 asm("r4") = (uint32_t)name3;
    __asm__ __volatile__(
            ""hvc(5121)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3), "+r"(r4)
            :
            : "cc", "memory", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_kdb_set_object_name(okl4_kcap_t object, uint32_t name0, uint32_t name1
        , uint32_t name2, uint32_t name3)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)object;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)name0;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)name1;
    register okl4_register_t x3 asm("x3") = (okl4_register_t)name2;
    register okl4_register_t x4 asm("x4") = (okl4_register_t)name3;
    __asm__ __volatile__(
            "" hvc(5121) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3), "+r"(x4)
            :
            : "cc", "memory", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Call a kernel support package (KSP) defined interface.
 *
 *    @details
 *    The KSP procedure call allows the caller to interact with customer
 *    specific functions provided by the kernel support package. The caller
 *    must possess a capability with the appropriate rights to a KSP agent
 *        in
 *    order to call this interface.
 *
 *    The remaining parameters provided are passed directly to the KSP
 *        without
 *    any inspection.
 *
 *    The KSP can return an error code and up to three return words.
 *
 * @param agent
 *    The target KSP agent
 * @param operation
 *    The operation to be performed
 * @param arg0
 *    An argument for the operation
 * @param arg1
 *    An argument for the operation
 * @param arg2
 *    An argument for the operation
 * @param arg3
 *    An argument for the operation
 *
 * @retval error
 *    The resulting error
 * @retval ret0
 *    A return value for the operation
 * @retval ret1
 *    A return value for the operation
 * @retval ret2
 *    A return value for the operation
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE struct _okl4_sys_ksp_procedure_call_return
_okl4_sys_ksp_procedure_call(okl4_kcap_t agent, okl4_ksp_arg_t operation,
        okl4_ksp_arg_t arg0, okl4_ksp_arg_t arg1, okl4_ksp_arg_t arg2,
        okl4_ksp_arg_t arg3)
{
    struct _okl4_sys_ksp_procedure_call_return result;

    register uint32_t r0 asm("r0") = (uint32_t)agent;
    register uint32_t r1 asm("r1") = (uint32_t)operation;
    register uint32_t r2 asm("r2") = (uint32_t)arg0;
    register uint32_t r3 asm("r3") = (uint32_t)arg1;
    register uint32_t r4 asm("r4") = (uint32_t)arg2;
    register uint32_t r5 asm("r5") = (uint32_t)arg3;
    __asm__ __volatile__(
            ""hvc(5197)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3), "+r"(r4), "+r"(r5)
            :
            : "cc", "memory"
            );


    result.error = (okl4_error_t)(r0);
    result.ret0 = (okl4_ksp_arg_t)(r1);
    result.ret1 = (okl4_ksp_arg_t)(r2);
    result.ret2 = (okl4_ksp_arg_t)(r3);
    return result;
}
#endif

#else

OKL4_FORCE_INLINE struct _okl4_sys_ksp_procedure_call_return
_okl4_sys_ksp_procedure_call(okl4_kcap_t agent, okl4_ksp_arg_t operation,
        okl4_ksp_arg_t arg0, okl4_ksp_arg_t arg1, okl4_ksp_arg_t arg2,
        okl4_ksp_arg_t arg3)
{
    struct _okl4_sys_ksp_procedure_call_return result;

    register okl4_register_t x0 asm("x0") = (okl4_register_t)agent;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)operation;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)arg0;
    register okl4_register_t x3 asm("x3") = (okl4_register_t)arg1;
    register okl4_register_t x4 asm("x4") = (okl4_register_t)arg2;
    register okl4_register_t x5 asm("x5") = (okl4_register_t)arg3;
    __asm__ __volatile__(
            "" hvc(5197) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3), "+r"(x4), "+r"(x5)
            :
            : "cc", "memory", "x6", "x7"
            );


    result.error = (okl4_error_t)(x0);
    result.ret0 = (okl4_ksp_arg_t)(x1);
    result.ret1 = (okl4_ksp_arg_t)(x2);
    result.ret2 = (okl4_ksp_arg_t)(x3);
    return result;
}

#endif

/**
 *
 * @brief Attach a segment to an MMU.
 *
 *    @details
 *    Before any mappings based on a segment can be established in the
 *        MMU's
 *    address space, the segment must be attached to the MMU. Attaching a
 *        segment
 *    serves to reference count the segment, preventing modifications to
 *        the
 *    segment being made.
 *
 *    A segment may be attached to an MMU multiple times, at the same or
 *    different index. Each time a segment is attached to an MMU, the
 *        attachment
 *    reference count is incremented.
 *
 *    Attaching segments to an MMU is also important for VMMU objects in
 *        that the
 *    segment attachment index is used as a segment reference in the
 *        virtual page
 *    table format.
 *
 * @param mmu_id
 *    The target MMU id.
 * @param segment_id
 *    The target segment id.
 * @param index
 *    Index into the MMU's segment attachment table.
 * @param perms
 *    Mapping permissions.
 *
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_mmu_attach_segment(okl4_kcap_t mmu_id, okl4_kcap_t segment_id,
        okl4_count_t index, okl4_page_perms_t perms)
{
    register uint32_t r0 asm("r0") = (uint32_t)mmu_id;
    register uint32_t r1 asm("r1") = (uint32_t)segment_id;
    register uint32_t r2 asm("r2") = (uint32_t)index;
    register uint32_t r3 asm("r3") = (uint32_t)perms;
    __asm__ __volatile__(
            ""hvc(5152)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3)
            :
            : "cc", "memory", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_mmu_attach_segment(okl4_kcap_t mmu_id, okl4_kcap_t segment_id,
        okl4_count_t index, okl4_page_perms_t perms)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)mmu_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)segment_id;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)index;
    register okl4_register_t x3 asm("x3") = (okl4_register_t)perms;
    __asm__ __volatile__(
            "" hvc(5152) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3)
            :
            : "cc", "memory", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Detach a segment from an MMU.
 *
 *    @details
 *    A segment can be detached from an MMU or vMMU, causing its reference
 *        count
 *    to decrease. When the reference count reaches zero, the attachment is
 *    removed and all mappings in the MMU object relating to the segment
 *        are
 *    removed.
 *
 *    The detach-segment operation is potentially a long running operation,
 *    especially if invoked on a vMMU.
 *
 * @param mmu_id
 *    The target MMU id.
 * @param index
 *    Index into the MMU's segment attachment table.
 *
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_mmu_detach_segment(okl4_kcap_t mmu_id, okl4_count_t index)
{
    register uint32_t r0 asm("r0") = (uint32_t)mmu_id;
    register uint32_t r1 asm("r1") = (uint32_t)index;
    __asm__ __volatile__(
            ""hvc(5153)"\n\t"
            : "+r"(r0), "+r"(r1)
            :
            : "cc", "memory", "r2", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_mmu_detach_segment(okl4_kcap_t mmu_id, okl4_count_t index)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)mmu_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)index;
    __asm__ __volatile__(
            "" hvc(5153) "\n\t"
            : "+r"(x0), "+r"(x1)
            :
            : "cc", "memory", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Flush a range of virtual addresses from an MMU.
 *
 *    @details
 *    This causes the kernel to remove all mappings covering the specified
 *    virtual address range.
 *
 *    @note The size of the range must be a multiple of 1MB and the
 *    starting virtual address must be 1MB aligned.
 *    There is no support for flushing at a finer granularity.
 *    If a fine grained flush is required, the caller should use the
 *    @ref _okl4_sys_mmu_unmap_page operation.
 *
 *    The flush-range operation is potentially a long running operation.
 *
 * @param mmu_id
 *    The target MMU id.
 * @param vaddr
 *    The starting virtual address of the range.
 *    (Must be 1MB aligned)
 * @param size
 *    Size of the range. (Must be a multiple of 1MB)
 *
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_mmu_flush_range(okl4_kcap_t mmu_id, okl4_laddr_tr_t vaddr,
        okl4_lsize_tr_t size)
{
    register uint32_t r0 asm("r0") = (uint32_t)mmu_id;
    register uint32_t r1 asm("r1") = (uint32_t)vaddr;
    register uint32_t r2 asm("r2") = (uint32_t)size;
    __asm__ __volatile__(
            ""hvc(5154)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2)
            :
            : "cc", "memory", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_mmu_flush_range(okl4_kcap_t mmu_id, okl4_laddr_tr_t vaddr,
        okl4_lsize_tr_t size)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)mmu_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)vaddr;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)size;
    __asm__ __volatile__(
            "" hvc(5154) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2)
            :
            : "cc", "memory", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Flush a range of virtual addresses from an MMU.
 *
 *    @details
 *    This causes the kernel to remove all mappings covering the specified
 *    virtual address range.
 *
 *    @note The size of the range must be a multiple of 1MB and the
 *    starting virtual address must be 1MB aligned.
 *    There is no support for flushing at a finer granularity.
 *    If a fine grained flush is required, the caller should use the
 *    @ref _okl4_sys_mmu_unmap_page operation.
 *
 *    The flush-range operation is potentially a long running operation.
 *
 * @param mmu_id
 *    The target MMU id.
 * @param laddr_pn
 *    Logical address page-number of the mapping.
 * @param count_pn
 *    The number of consecutive pages to map/unmap.
 *
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_mmu_flush_range_pn(okl4_kcap_t mmu_id, okl4_laddr_pn_t laddr_pn,
        okl4_lsize_pn_t count_pn)
{
    register uint32_t r0 asm("r0") = (uint32_t)mmu_id;
    register uint32_t r1 asm("r1") = (uint32_t)laddr_pn;
    register uint32_t r2 asm("r2") = (uint32_t)count_pn;
    __asm__ __volatile__(
            ""hvc(5155)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2)
            :
            : "cc", "memory", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_mmu_flush_range_pn(okl4_kcap_t mmu_id, okl4_laddr_pn_t laddr_pn,
        okl4_lsize_pn_t count_pn)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)mmu_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)laddr_pn;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)count_pn;
    __asm__ __volatile__(
            "" hvc(5155) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2)
            :
            : "cc", "memory", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Lookup a virtual address in the MMU.
 *
 *    @details
 *    This operation performs a lookup in the MMU's pagetable for a mapping
 *    derived from a specified segment.
 *
 *    If a mapping is found that is derived from the specified segment, the
 *    operation will return the segment offset, size and the page
 *        attributes
 *    associated with the mapping.
 *
 *    If a segment_index value of OKL4_KCAP_INVALID is specified, the
 *        operation
 *    will search for a matching segment in the MMU.
 *
 * @param mmu_id
 *    The target MMU id.
 * @param vaddr
 *    Virtual address of the mapping.
 * @param segment_index
 *    Index into the MMU's segment attachment table.
 *
 * @retval error
 *    Resulting error.
 * @retval offset
 *    Offset into the segment.
 * @retval size
 *    Size of the mapping, in bytes. Size will be one of the supported
 *    machine page-sizes. If a segment search was performed, the lower
 *        10-bits of
 *    size contain the returned segment-index.
 * @retval page_attr
 *    Mapping attributes.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE struct _okl4_sys_mmu_lookup_page_return
_okl4_sys_mmu_lookup_page(okl4_kcap_t mmu_id, okl4_laddr_tr_t vaddr,
        okl4_count_t segment_index)
{
    typedef union {
        struct uint64 {
            uint32_t lo;
            uint32_t hi;
        } words;
        uint64_t val;
    } okl4_uint64_tmp;
    okl4_uint64_tmp size_tmp;
    struct _okl4_sys_mmu_lookup_page_return result;

    register uint32_t r0 asm("r0") = (uint32_t)mmu_id;
    register uint32_t r1 asm("r1") = (uint32_t)vaddr;
    register uint32_t r2 asm("r2") = (uint32_t)segment_index;
    register uint32_t r3 asm("r3");
    register uint32_t r4 asm("r4");
    __asm__ __volatile__(
            ""hvc(5156)"\n\t"
            : "=r"(r3), "=r"(r4), "+r"(r0), "+r"(r1), "+r"(r2)
            :
            : "cc", "memory", "r5"
            );


    result.error = (okl4_error_t)(r0);
    result.offset = (okl4_psize_tr_t)(r1);
    size_tmp.words.lo = r2;
    size_tmp.words.hi = r3;
    result.size = (okl4_mmu_lookup_size_t)(size_tmp.val);
    result.page_attr = (_okl4_page_attribute_t)(r4);
    return result;
}
#endif

#else

OKL4_FORCE_INLINE struct _okl4_sys_mmu_lookup_page_return
_okl4_sys_mmu_lookup_page(okl4_kcap_t mmu_id, okl4_laddr_tr_t vaddr,
        okl4_count_t segment_index)
{
    struct _okl4_sys_mmu_lookup_page_return result;

    register okl4_register_t x0 asm("x0") = (okl4_register_t)mmu_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)vaddr;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)segment_index;
    register okl4_register_t x3 asm("x3");
    __asm__ __volatile__(
            "" hvc(5156) "\n\t"
            : "=r"(x3), "+r"(x0), "+r"(x1), "+r"(x2)
            :
            : "cc", "memory", "x4", "x5", "x6", "x7"
            );


    result.error = (okl4_error_t)(x0);
    result.offset = (okl4_psize_tr_t)(x1);
    result.size = (okl4_mmu_lookup_size_t)(x2);
    result.page_attr = (_okl4_page_attribute_t)(x3);
    return result;
}

#endif

/**
 *
 * @brief Lookup a virtual address in the MMU.
 *
 *    @details
 *    This operation performs a lookup in the MMU's pagetable for a mapping
 *    derived from a specified segment.
 *
 *    If a mapping is found that is derived from the specified segment, the
 *    operation will return the segment offset, size and the page
 *        attributes
 *    associated with the mapping.
 *
 *    If a segment_index value of OKL4_KCAP_INVALID is specified, the
 *        operation
 *    will search for a matching segment in the MMU.
 *
 * @param mmu_id
 *    The target MMU id.
 * @param laddr_pn
 *    Logical address page-number of the mapping.
 * @param segment_index
 *    Index into the MMU's segment attachment table.
 *
 * @retval segment_index
 *    Index into the MMU's segment attachment table, or error.
 * @retval offset_pn
 *    Offset into the segment in units of page numbers.
 * @retval count_pn
 *    The number of consecutive pages to map/unmap.
 * @retval page_attr
 *    Mapping attributes.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE struct _okl4_sys_mmu_lookup_pn_return
_okl4_sys_mmu_lookup_pn(okl4_kcap_t mmu_id, okl4_laddr_pn_t laddr_pn,
        okl4_count_t segment_index)
{
    struct _okl4_sys_mmu_lookup_pn_return result;

    register uint32_t r0 asm("r0") = (uint32_t)mmu_id;
    register uint32_t r1 asm("r1") = (uint32_t)laddr_pn;
    register uint32_t r2 asm("r2") = (uint32_t)segment_index;
    register uint32_t r3 asm("r3");
    __asm__ __volatile__(
            ""hvc(5157)"\n\t"
            : "=r"(r3), "+r"(r0), "+r"(r1), "+r"(r2)
            :
            : "cc", "memory", "r4", "r5"
            );


    result.segment_index = (okl4_mmu_lookup_index_t)(r0);
    result.offset_pn = (okl4_psize_pn_t)(r1);
    result.count_pn = (okl4_lsize_pn_t)(r2);
    result.page_attr = (_okl4_page_attribute_t)(r3);
    return result;
}
#endif

#else

OKL4_FORCE_INLINE struct _okl4_sys_mmu_lookup_pn_return
_okl4_sys_mmu_lookup_pn(okl4_kcap_t mmu_id, okl4_laddr_pn_t laddr_pn,
        okl4_count_t segment_index)
{
    struct _okl4_sys_mmu_lookup_pn_return result;

    register okl4_register_t x0 asm("x0") = (okl4_register_t)mmu_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)laddr_pn;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)segment_index;
    register okl4_register_t x3 asm("x3");
    __asm__ __volatile__(
            "" hvc(5157) "\n\t"
            : "=r"(x3), "+r"(x0), "+r"(x1), "+r"(x2)
            :
            : "cc", "memory", "x4", "x5", "x6", "x7"
            );


    result.segment_index = (okl4_mmu_lookup_index_t)(x0);
    result.offset_pn = (okl4_psize_pn_t)(x1);
    result.count_pn = (okl4_lsize_pn_t)(x2);
    result.page_attr = (_okl4_page_attribute_t)(x3);
    return result;
}

#endif

/**
 *
 * @brief Create a mapping at a virtual address in the MMU.
 *
 *    @details
 *    This operation installs a new mapping into the MMU at the specified
 *        virtual
 *    address. The mapping's physical address is determined from the
 *        specified
 *    segment and offset, and the mapping's size and attributes are
 *        provided in
 *    \p size and \p page_attr.
 *
 * @param mmu_id
 *    The target MMU id.
 * @param vaddr
 *    Virtual address of the mapping.
 * @param segment_index
 *    Index into the MMU's segment attachment table.
 * @param offset
 *    Offset into the segment.
 * @param size
 *    Size of the mapping, in bytes.
 * @param page_attr
 *    Mapping attributes.
 *
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_mmu_map_page(okl4_kcap_t mmu_id, okl4_laddr_tr_t vaddr,
        okl4_count_t segment_index, okl4_psize_tr_t offset, okl4_lsize_tr_t size
        , _okl4_page_attribute_t page_attr)
{
    register uint32_t r0 asm("r0") = (uint32_t)mmu_id;
    register uint32_t r1 asm("r1") = (uint32_t)vaddr;
    register uint32_t r2 asm("r2") = (uint32_t)segment_index;
    register uint32_t r3 asm("r3") = (uint32_t)offset;
    register uint32_t r4 asm("r4") = (uint32_t)size;
    register uint32_t r5 asm("r5") = (uint32_t)page_attr;
    __asm__ __volatile__(
            ""hvc(5158)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3), "+r"(r4), "+r"(r5)
            :
            : "cc", "memory"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_mmu_map_page(okl4_kcap_t mmu_id, okl4_laddr_tr_t vaddr,
        okl4_count_t segment_index, okl4_psize_tr_t offset, okl4_lsize_tr_t size
        , _okl4_page_attribute_t page_attr)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)mmu_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)vaddr;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)segment_index;
    register okl4_register_t x3 asm("x3") = (okl4_register_t)offset;
    register okl4_register_t x4 asm("x4") = (okl4_register_t)size;
    register okl4_register_t x5 asm("x5") = (okl4_register_t)page_attr;
    __asm__ __volatile__(
            "" hvc(5158) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3), "+r"(x4), "+r"(x5)
            :
            : "cc", "memory", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Create a mapping at a virtual address in the MMU.
 *
 *    @details
 *    This operation installs a new mapping into the MMU at the specified
 *        virtual
 *    address. The mapping's physical address is determined from the
 *        specified
 *    segment and offset, and the mapping's size and attributes are
 *        provided in
 *    \p size and \p page_attr.
 *
 * @param mmu_id
 *    The target MMU id.
 * @param laddr_pn
 *    Logical address page-number of the mapping.
 * @param segment_index
 *    Index into the MMU's segment attachment table.
 * @param segment_offset_pn
 *    Offset into the segment in units of page numbers.
 * @param count_pn
 *    The number of consecutive pages to map/unmap.
 * @param page_attr
 *    Mapping attributes.
 *
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_mmu_map_pn(okl4_kcap_t mmu_id, okl4_laddr_pn_t laddr_pn,
        okl4_count_t segment_index, okl4_psize_pn_t segment_offset_pn,
        okl4_lsize_pn_t count_pn, _okl4_page_attribute_t page_attr)
{
    register uint32_t r0 asm("r0") = (uint32_t)mmu_id;
    register uint32_t r1 asm("r1") = (uint32_t)laddr_pn;
    register uint32_t r2 asm("r2") = (uint32_t)segment_index;
    register uint32_t r3 asm("r3") = (uint32_t)segment_offset_pn;
    register uint32_t r4 asm("r4") = (uint32_t)count_pn;
    register uint32_t r5 asm("r5") = (uint32_t)page_attr;
    __asm__ __volatile__(
            ""hvc(5159)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3), "+r"(r4), "+r"(r5)
            :
            : "cc", "memory"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_mmu_map_pn(okl4_kcap_t mmu_id, okl4_laddr_pn_t laddr_pn,
        okl4_count_t segment_index, okl4_psize_pn_t segment_offset_pn,
        okl4_lsize_pn_t count_pn, _okl4_page_attribute_t page_attr)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)mmu_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)laddr_pn;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)segment_index;
    register okl4_register_t x3 asm("x3") = (okl4_register_t)segment_offset_pn;
    register okl4_register_t x4 asm("x4") = (okl4_register_t)count_pn;
    register okl4_register_t x5 asm("x5") = (okl4_register_t)page_attr;
    __asm__ __volatile__(
            "" hvc(5159) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3), "+r"(x4), "+r"(x5)
            :
            : "cc", "memory", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Remove a mapping at a virtual address in the MMU.
 *
 *    @details
 *    This operation removes a mapping from the MMU at the specified
 *        virtual
 *    address. The size and address specified must match the size and base
 *    address of the mapping being removed.
 *
 * @param mmu_id
 *    The target MMU id.
 * @param vaddr
 *    Virtual address of the mapping.
 * @param size
 *    Size of the mapping, in bytes.
 *
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_mmu_unmap_page(okl4_kcap_t mmu_id, okl4_laddr_tr_t vaddr,
        okl4_lsize_tr_t size)
{
    register uint32_t r0 asm("r0") = (uint32_t)mmu_id;
    register uint32_t r1 asm("r1") = (uint32_t)vaddr;
    register uint32_t r2 asm("r2") = (uint32_t)size;
    __asm__ __volatile__(
            ""hvc(5160)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2)
            :
            : "cc", "memory", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_mmu_unmap_page(okl4_kcap_t mmu_id, okl4_laddr_tr_t vaddr,
        okl4_lsize_tr_t size)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)mmu_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)vaddr;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)size;
    __asm__ __volatile__(
            "" hvc(5160) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2)
            :
            : "cc", "memory", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Remove a mapping at a virtual address in the MMU.
 *
 *    @details
 *    This operation removes a mapping from the MMU at the specified
 *        virtual
 *    address. The size and address specified must match the size and base
 *    address of the mapping being removed.
 *
 * @param mmu_id
 *    The target MMU id.
 * @param laddr_pn
 *    Logical address page-number of the mapping.
 * @param count_pn
 *    The number of consecutive pages to map/unmap.
 *
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_mmu_unmap_pn(okl4_kcap_t mmu_id, okl4_laddr_pn_t laddr_pn,
        okl4_lsize_pn_t count_pn)
{
    register uint32_t r0 asm("r0") = (uint32_t)mmu_id;
    register uint32_t r1 asm("r1") = (uint32_t)laddr_pn;
    register uint32_t r2 asm("r2") = (uint32_t)count_pn;
    __asm__ __volatile__(
            ""hvc(5161)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2)
            :
            : "cc", "memory", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_mmu_unmap_pn(okl4_kcap_t mmu_id, okl4_laddr_pn_t laddr_pn,
        okl4_lsize_pn_t count_pn)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)mmu_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)laddr_pn;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)count_pn;
    __asm__ __volatile__(
            "" hvc(5161) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2)
            :
            : "cc", "memory", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Update the cache attributes of a mapping in the MMU.
 *
 * @param mmu_id
 *    The target MMU id.
 * @param vaddr
 *    Virtual address of the mapping.
 * @param segment_index
 *    Index into the MMU's segment attachment table.
 * @param size
 *    Size of the mapping, in bytes.
 * @param attrs
 *    Mapping cache attributes.
 *
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_mmu_update_page_attrs(okl4_kcap_t mmu_id, okl4_laddr_tr_t vaddr,
        okl4_count_t segment_index, okl4_lsize_tr_t size,
        okl4_page_cache_t attrs)
{
    register uint32_t r0 asm("r0") = (uint32_t)mmu_id;
    register uint32_t r1 asm("r1") = (uint32_t)vaddr;
    register uint32_t r2 asm("r2") = (uint32_t)segment_index;
    register uint32_t r3 asm("r3") = (uint32_t)size;
    register uint32_t r4 asm("r4") = (uint32_t)attrs;
    __asm__ __volatile__(
            ""hvc(5162)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3), "+r"(r4)
            :
            : "cc", "memory", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_mmu_update_page_attrs(okl4_kcap_t mmu_id, okl4_laddr_tr_t vaddr,
        okl4_count_t segment_index, okl4_lsize_tr_t size,
        okl4_page_cache_t attrs)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)mmu_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)vaddr;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)segment_index;
    register okl4_register_t x3 asm("x3") = (okl4_register_t)size;
    register okl4_register_t x4 asm("x4") = (okl4_register_t)attrs;
    __asm__ __volatile__(
            "" hvc(5162) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3), "+r"(x4)
            :
            : "cc", "memory", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Update the page permissions of a mapping in the MMU.
 *
 * @param mmu_id
 *    The target MMU id.
 * @param vaddr
 *    Virtual address of the mapping.
 * @param segment_index
 *    Index into the MMU's segment attachment table.
 * @param size
 *    Size of the mapping, in bytes.
 * @param perms
 *    Mapping permissions.
 *
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_mmu_update_page_perms(okl4_kcap_t mmu_id, okl4_laddr_tr_t vaddr,
        okl4_count_t segment_index, okl4_lsize_tr_t size,
        okl4_page_perms_t perms)
{
    register uint32_t r0 asm("r0") = (uint32_t)mmu_id;
    register uint32_t r1 asm("r1") = (uint32_t)vaddr;
    register uint32_t r2 asm("r2") = (uint32_t)segment_index;
    register uint32_t r3 asm("r3") = (uint32_t)size;
    register uint32_t r4 asm("r4") = (uint32_t)perms;
    __asm__ __volatile__(
            ""hvc(5163)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3), "+r"(r4)
            :
            : "cc", "memory", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_mmu_update_page_perms(okl4_kcap_t mmu_id, okl4_laddr_tr_t vaddr,
        okl4_count_t segment_index, okl4_lsize_tr_t size,
        okl4_page_perms_t perms)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)mmu_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)vaddr;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)segment_index;
    register okl4_register_t x3 asm("x3") = (okl4_register_t)size;
    register okl4_register_t x4 asm("x4") = (okl4_register_t)perms;
    __asm__ __volatile__(
            "" hvc(5163) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3), "+r"(x4)
            :
            : "cc", "memory", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Update the cache attributes of a mapping in the MMU.
 *
 * @param mmu_id
 *    The target MMU id.
 * @param laddr_pn
 *    Logical address page-number of the mapping.
 * @param segment_index
 *    Index into the MMU's segment attachment table.
 * @param count_pn
 *    The number of consecutive pages to map/unmap.
 * @param attrs
 *    Mapping cache attributes.
 *
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_mmu_update_pn_attrs(okl4_kcap_t mmu_id, okl4_laddr_pn_t laddr_pn,
        okl4_count_t segment_index, okl4_lsize_pn_t count_pn,
        okl4_page_cache_t attrs)
{
    register uint32_t r0 asm("r0") = (uint32_t)mmu_id;
    register uint32_t r1 asm("r1") = (uint32_t)laddr_pn;
    register uint32_t r2 asm("r2") = (uint32_t)segment_index;
    register uint32_t r3 asm("r3") = (uint32_t)count_pn;
    register uint32_t r4 asm("r4") = (uint32_t)attrs;
    __asm__ __volatile__(
            ""hvc(5164)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3), "+r"(r4)
            :
            : "cc", "memory", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_mmu_update_pn_attrs(okl4_kcap_t mmu_id, okl4_laddr_pn_t laddr_pn,
        okl4_count_t segment_index, okl4_lsize_pn_t count_pn,
        okl4_page_cache_t attrs)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)mmu_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)laddr_pn;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)segment_index;
    register okl4_register_t x3 asm("x3") = (okl4_register_t)count_pn;
    register okl4_register_t x4 asm("x4") = (okl4_register_t)attrs;
    __asm__ __volatile__(
            "" hvc(5164) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3), "+r"(x4)
            :
            : "cc", "memory", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Update the page permissions of a mapping in the MMU.
 *
 * @param mmu_id
 *    The target MMU id.
 * @param laddr_pn
 *    Logical address page-number of the mapping.
 * @param segment_index
 *    Index into the MMU's segment attachment table.
 * @param count_pn
 *    The number of consecutive pages to map/unmap.
 * @param perms
 *    Mapping permissions.
 *
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_mmu_update_pn_perms(okl4_kcap_t mmu_id, okl4_laddr_pn_t laddr_pn,
        okl4_count_t segment_index, okl4_lsize_pn_t count_pn,
        okl4_page_perms_t perms)
{
    register uint32_t r0 asm("r0") = (uint32_t)mmu_id;
    register uint32_t r1 asm("r1") = (uint32_t)laddr_pn;
    register uint32_t r2 asm("r2") = (uint32_t)segment_index;
    register uint32_t r3 asm("r3") = (uint32_t)count_pn;
    register uint32_t r4 asm("r4") = (uint32_t)perms;
    __asm__ __volatile__(
            ""hvc(5165)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3), "+r"(r4)
            :
            : "cc", "memory", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_mmu_update_pn_perms(okl4_kcap_t mmu_id, okl4_laddr_pn_t laddr_pn,
        okl4_count_t segment_index, okl4_lsize_pn_t count_pn,
        okl4_page_perms_t perms)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)mmu_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)laddr_pn;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)segment_index;
    register okl4_register_t x3 asm("x3") = (okl4_register_t)count_pn;
    register okl4_register_t x4 asm("x4") = (okl4_register_t)perms;
    __asm__ __volatile__(
            "" hvc(5165) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3), "+r"(x4)
            :
            : "cc", "memory", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * A NULL system-call for latency measurement.
 *
 *
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_performance_null_syscall(void)
{
    register uint32_t r0 asm("r0");
    __asm__ __volatile__(
            ""hvc(5198)"\n\t"
            : "=r"(r0)
            :
            : "cc", "memory", "r1", "r2", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_performance_null_syscall(void)
{
    register okl4_register_t x0 asm("x0");
    __asm__ __volatile__(
            "" hvc(5198) "\n\t"
            : "=r"(x0)
            :
            : "cc", "memory", "x1", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * Control a pipe, including reset, ready and halt functionality.
 *
 * @param pipe_id
 *    The capability identifier of the pipe.
 * @param control
 *    The state control argument.
 *
 * @retval error
 *    The returned error code.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_pipe_control(okl4_kcap_t pipe_id, okl4_pipe_control_t control)
{
    register uint32_t r0 asm("r0") = (uint32_t)pipe_id;
    register uint32_t r1 asm("r1") = (uint32_t)control;
    __asm__ __volatile__(
            ""hvc(5146)"\n\t"
            : "+r"(r0), "+r"(r1)
            :
            : "cc", "memory", "r2", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_pipe_control(okl4_kcap_t pipe_id, okl4_pipe_control_t control)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)pipe_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)control;
    __asm__ __volatile__(
            "" hvc(5146) "\n\t"
            : "+r"(x0), "+r"(x1)
            :
            : "cc", "memory", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * Send a message from a microvisor pipe.
 *
 * @param pipe_id
 *    The capability identifier of the pipe.
 * @param buf_size
 *    Size of the receive buffer.
 * @param data
 *    Pointer to receive buffer.
 *
 * @retval error
 *    The returned error code.
 * @retval size
 *    Size of the received message.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE struct _okl4_sys_pipe_recv_return
_okl4_sys_pipe_recv(okl4_kcap_t pipe_id, okl4_vsize_t buf_size, uint8_t *data)
{
    typedef union {
        struct uint64 {
            uint32_t lo;
            uint32_t hi;
        } words;
        uint64_t val;
    } okl4_uint64_tmp;
    okl4_uint64_tmp size_tmp;
    struct _okl4_sys_pipe_recv_return result;

    register uint32_t r0 asm("r0") = (uint32_t)pipe_id;
    register uint32_t r1 asm("r1") = (uint32_t)buf_size;
    register uint32_t r2 asm("r2") = (uint32_t)(uintptr_t)data;
    __asm__ __volatile__(
            ""hvc(5147)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2)
            :
            : "cc", "memory", "r3", "r4", "r5"
            );


    result.error = (okl4_error_t)(r0);
    size_tmp.words.lo = r1;
    size_tmp.words.hi = r2;
    result.size = (okl4_ksize_t)(size_tmp.val);
    return result;
}
#endif

#else

OKL4_FORCE_INLINE struct _okl4_sys_pipe_recv_return
_okl4_sys_pipe_recv(okl4_kcap_t pipe_id, okl4_vsize_t buf_size, uint8_t *data)
{
    struct _okl4_sys_pipe_recv_return result;

    register okl4_register_t x0 asm("x0") = (okl4_register_t)pipe_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)buf_size;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)(uintptr_t)data;
    __asm__ __volatile__(
            "" hvc(5147) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2)
            :
            : "cc", "memory", "x3", "x4", "x5", "x6", "x7"
            );


    result.error = (okl4_error_t)(x0);
    result.size = (okl4_ksize_t)(x1);
    return result;
}

#endif

/**
 *
 * Send a message to a microvisor pipe.
 *
 * @param pipe_id
 *    The capability identifier of the pipe.
 * @param size
 *    Size of the message to send.
 * @param data
 *    Pointer to the message payload to send.
 *
 * @retval error
 *    The returned error code.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_pipe_send(okl4_kcap_t pipe_id, okl4_vsize_t size, const uint8_t *data)
{
    register uint32_t r0 asm("r0") = (uint32_t)pipe_id;
    register uint32_t r1 asm("r1") = (uint32_t)size;
    register uint32_t r2 asm("r2") = (uint32_t)(uintptr_t)data;
    __asm__ __volatile__(
            ""hvc(5148)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2)
            :
            : "cc", "memory", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_pipe_send(okl4_kcap_t pipe_id, okl4_vsize_t size, const uint8_t *data)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)pipe_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)size;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)(uintptr_t)data;
    __asm__ __volatile__(
            "" hvc(5148) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2)
            :
            : "cc", "memory", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Waive the current vCPU's priority.
 *
 *    @details
 *    This operation allows a vCPU to change its waived priority. A vCPU
 *        has
 *    both a base priority and its current priority.
 *
 *    The base priority is the statically assigned maximum priority that a
 *        vCPU
 *    has been given. The current priority is the priority used for system
 *    scheduling and is limited to the range of zero to the base priority.
 *
 *    The `waive-priority` operation allows a vCPU to set its current
 *        priority
 *    and is normally used to reduce its current priority. This allows a
 *        vCPU to
 *    perform work at a lower system priority, and supports the interleaved
 *    scheduling feature.
 *
 *    A vCPU's priority is restored to its base priority whenever an
 *        interrupt
 *    that has the vCPU registered as its handler is raised. This allows
 *    interrupt handling and guest operating systems to return to the base
 *    priority to potentially do higher priority work.
 *
 *    After calling this interface an immediate reschedule will be
 *        performed.
 *
 * @param priority
 *    New vCPU priority.
 *
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_priority_waive(okl4_priority_t priority)
{
    register uint32_t r0 asm("r0") = (uint32_t)priority;
    __asm__ __volatile__(
            ""hvc(5151)"\n\t"
            : "+r"(r0)
            :
            : "cc", "memory", "r1", "r2", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_priority_waive(okl4_priority_t priority)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)priority;
    __asm__ __volatile__(
            "" hvc(5151) "\n\t"
            : "+r"(x0)
            :
            : "cc", "memory", "x1", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * OKL4 Microvisor system call: REMOTE_GET_REGISTER
 *
 * @param target
 * @param reg_and_set
 *
 * @retval reg_w0
 * @retval reg_w1
 * @retval reg_w2
 * @retval reg_w3
 * @retval error
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE struct _okl4_sys_remote_get_register_return
_okl4_sys_remote_get_register(okl4_kcap_t target,
        okl4_register_and_set_t reg_and_set)
{
    struct _okl4_sys_remote_get_register_return result;

    register uint32_t r0 asm("r0") = (uint32_t)target;
    register uint32_t r1 asm("r1") = (uint32_t)reg_and_set;
    register uint32_t r2 asm("r2");
    register uint32_t r3 asm("r3");
    register uint32_t r4 asm("r4");
    __asm__ __volatile__(
            ""hvc(5200)"\n\t"
            : "=r"(r2), "=r"(r3), "=r"(r4), "+r"(r0), "+r"(r1)
            :
            : "cc", "memory", "r5"
            );


    result.reg_w0 = (uint32_t)(r0);
    result.reg_w1 = (uint32_t)(r1);
    result.reg_w2 = (uint32_t)(r2);
    result.reg_w3 = (uint32_t)(r3);
    result.error = (okl4_error_t)(r4);
    return result;
}
#endif

#else

OKL4_FORCE_INLINE struct _okl4_sys_remote_get_register_return
_okl4_sys_remote_get_register(okl4_kcap_t target,
        okl4_register_and_set_t reg_and_set)
{
    struct _okl4_sys_remote_get_register_return result;

    register okl4_register_t x0 asm("x0") = (okl4_register_t)target;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)reg_and_set;
    register okl4_register_t x2 asm("x2");
    register okl4_register_t x3 asm("x3");
    register okl4_register_t x4 asm("x4");
    __asm__ __volatile__(
            "" hvc(5200) "\n\t"
            : "=r"(x2), "=r"(x3), "=r"(x4), "+r"(x0), "+r"(x1)
            :
            : "cc", "memory", "x5", "x6", "x7"
            );


    result.reg_w0 = (uint32_t)(x0);
    result.reg_w1 = (uint32_t)(x1);
    result.reg_w2 = (uint32_t)(x2);
    result.reg_w3 = (uint32_t)(x3);
    result.error = (okl4_error_t)(x4);
    return result;
}

#endif

/**
 *
 * OKL4 Microvisor system call: REMOTE_GET_REGISTERS
 *
 * @param target
 * @param set
 * @param regs
 *
 * @retval error
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_remote_get_registers(okl4_kcap_t target, okl4_register_set_t set,
        void *regs)
{
    register uint32_t r0 asm("r0") = (uint32_t)target;
    register uint32_t r1 asm("r1") = (uint32_t)set;
    register uint32_t r2 asm("r2") = (uint32_t)(uintptr_t)regs;
    __asm__ __volatile__(
            ""hvc(5201)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2)
            :
            : "cc", "memory", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_remote_get_registers(okl4_kcap_t target, okl4_register_set_t set,
        void *regs)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)target;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)set;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)(uintptr_t)regs;
    __asm__ __volatile__(
            "" hvc(5201) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2)
            :
            : "cc", "memory", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * OKL4 Microvisor system call: REMOTE_READ_MEMORY32
 *
 * @param target
 * @param address
 *
 * @retval data
 * @retval error
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE struct _okl4_sys_remote_read_memory32_return
_okl4_sys_remote_read_memory32(okl4_kcap_t target, okl4_laddr_t address)
{
    struct _okl4_sys_remote_read_memory32_return result;

    register uint32_t r0 asm("r0") = (uint32_t)target;
    register uint32_t r1 asm("r1") = (uint32_t)(address        & 0xffffffff);
    register uint32_t r2 asm("r2") = (uint32_t)((address >> 32) & 0xffffffff);
    __asm__ __volatile__(
            ""hvc(5202)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2)
            :
            : "cc", "memory", "r3", "r4", "r5"
            );


    result.data = (uint32_t)(r0);
    result.error = (okl4_error_t)(r1);
    return result;
}
#endif

#else

OKL4_FORCE_INLINE struct _okl4_sys_remote_read_memory32_return
_okl4_sys_remote_read_memory32(okl4_kcap_t target, okl4_laddr_t address)
{
    struct _okl4_sys_remote_read_memory32_return result;

    register okl4_register_t x0 asm("x0") = (okl4_register_t)target;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)address;
    __asm__ __volatile__(
            "" hvc(5202) "\n\t"
            : "+r"(x0), "+r"(x1)
            :
            : "cc", "memory", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    result.data = (uint32_t)(x0);
    result.error = (okl4_error_t)(x1);
    return result;
}

#endif

/**
 *
 * OKL4 Microvisor system call: REMOTE_SET_REGISTER
 *
 * @param target
 * @param reg_and_set
 * @param reg_w0
 * @param reg_w1
 * @param reg_w2
 * @param reg_w3
 *
 * @retval error
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_remote_set_register(okl4_kcap_t target,
        okl4_register_and_set_t reg_and_set, uint32_t reg_w0, uint32_t reg_w1,
        uint32_t reg_w2, uint32_t reg_w3)
{
    register uint32_t r0 asm("r0") = (uint32_t)target;
    register uint32_t r1 asm("r1") = (uint32_t)reg_and_set;
    register uint32_t r2 asm("r2") = (uint32_t)reg_w0;
    register uint32_t r3 asm("r3") = (uint32_t)reg_w1;
    register uint32_t r4 asm("r4") = (uint32_t)reg_w2;
    register uint32_t r5 asm("r5") = (uint32_t)reg_w3;
    __asm__ __volatile__(
            ""hvc(5203)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3), "+r"(r4), "+r"(r5)
            :
            : "cc", "memory"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_remote_set_register(okl4_kcap_t target,
        okl4_register_and_set_t reg_and_set, uint32_t reg_w0, uint32_t reg_w1,
        uint32_t reg_w2, uint32_t reg_w3)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)target;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)reg_and_set;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)reg_w0;
    register okl4_register_t x3 asm("x3") = (okl4_register_t)reg_w1;
    register okl4_register_t x4 asm("x4") = (okl4_register_t)reg_w2;
    register okl4_register_t x5 asm("x5") = (okl4_register_t)reg_w3;
    __asm__ __volatile__(
            "" hvc(5203) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3), "+r"(x4), "+r"(x5)
            :
            : "cc", "memory", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * OKL4 Microvisor system call: REMOTE_SET_REGISTERS
 *
 * @param target
 * @param set
 * @param regs
 *
 * @retval error
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_remote_set_registers(okl4_kcap_t target, okl4_register_set_t set,
        void *regs)
{
    register uint32_t r0 asm("r0") = (uint32_t)target;
    register uint32_t r1 asm("r1") = (uint32_t)set;
    register uint32_t r2 asm("r2") = (uint32_t)(uintptr_t)regs;
    __asm__ __volatile__(
            ""hvc(5204)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2)
            :
            : "cc", "memory", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_remote_set_registers(okl4_kcap_t target, okl4_register_set_t set,
        void *regs)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)target;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)set;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)(uintptr_t)regs;
    __asm__ __volatile__(
            "" hvc(5204) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2)
            :
            : "cc", "memory", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * OKL4 Microvisor system call: REMOTE_WRITE_MEMORY32
 *
 * @param target
 * @param address
 * @param data
 *
 * @retval error
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_remote_write_memory32(okl4_kcap_t target, okl4_laddr_t address,
        uint32_t data)
{
    register uint32_t r0 asm("r0") = (uint32_t)target;
    register uint32_t r1 asm("r1") = (uint32_t)(address        & 0xffffffff);
    register uint32_t r2 asm("r2") = (uint32_t)((address >> 32) & 0xffffffff);
    register uint32_t r3 asm("r3") = (uint32_t)data;
    __asm__ __volatile__(
            ""hvc(5205)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3)
            :
            : "cc", "memory", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_remote_write_memory32(okl4_kcap_t target, okl4_laddr_t address,
        uint32_t data)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)target;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)address;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)data;
    __asm__ __volatile__(
            "" hvc(5205) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2)
            :
            : "cc", "memory", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * Retrieve suspend status.
 *
 * @param scheduler_id
 *    The scheduler capability identifier.
 *
 * @retval error
 *    Resulting error.
 * @retval power_suspend_version
 *    The power suspend versioning number
 * @retval power_suspend_running_count
 *    The number of running power_suspend watched vCPUs
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE struct _okl4_sys_schedule_metrics_status_suspended_return
_okl4_sys_schedule_metrics_status_suspended(okl4_kcap_t scheduler_id)
{
    struct _okl4_sys_schedule_metrics_status_suspended_return result;

    register uint32_t r0 asm("r0") = (uint32_t)scheduler_id;
    register uint32_t r1 asm("r1");
    register uint32_t r2 asm("r2");
    __asm__ __volatile__(
            ""hvc(5206)"\n\t"
            : "=r"(r1), "=r"(r2), "+r"(r0)
            :
            : "cc", "memory", "r3", "r4", "r5"
            );


    result.error = (okl4_error_t)(r0);
    result.power_suspend_version = (uint32_t)(r1);
    result.power_suspend_running_count = (uint32_t)(r2);
    return result;
}
#endif

#else

OKL4_FORCE_INLINE struct _okl4_sys_schedule_metrics_status_suspended_return
_okl4_sys_schedule_metrics_status_suspended(okl4_kcap_t scheduler_id)
{
    struct _okl4_sys_schedule_metrics_status_suspended_return result;

    register okl4_register_t x0 asm("x0") = (okl4_register_t)scheduler_id;
    register okl4_register_t x1 asm("x1");
    register okl4_register_t x2 asm("x2");
    __asm__ __volatile__(
            "" hvc(5206) "\n\t"
            : "=r"(x1), "=r"(x2), "+r"(x0)
            :
            : "cc", "memory", "x3", "x4", "x5", "x6", "x7"
            );


    result.error = (okl4_error_t)(x0);
    result.power_suspend_version = (uint32_t)(x1);
    result.power_suspend_running_count = (uint32_t)(x2);
    return result;
}

#endif

/**
 *
 * Register a vCPU for suspend count tracking.
 *
 * @param scheduler_id
 *    The scheduler capability identifier.
 * @param vcpu_id
 *    The target vCPU capability identifier.
 * @param watch
 *    Whether to register or unregister
 *
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_schedule_metrics_watch_suspended(okl4_kcap_t scheduler_id,
        okl4_kcap_t vcpu_id, okl4_bool_t watch)
{
    register uint32_t r0 asm("r0") = (uint32_t)scheduler_id;
    register uint32_t r1 asm("r1") = (uint32_t)vcpu_id;
    register uint32_t r2 asm("r2") = (uint32_t)watch;
    __asm__ __volatile__(
            ""hvc(5207)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2)
            :
            : "cc", "memory", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_schedule_metrics_watch_suspended(okl4_kcap_t scheduler_id,
        okl4_kcap_t vcpu_id, okl4_bool_t watch)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)scheduler_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)vcpu_id;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)watch;
    __asm__ __volatile__(
            "" hvc(5207) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2)
            :
            : "cc", "memory", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Disable profiling of a physical CPU.
 *
 * @param phys_cpu
 *    The physical CPU capability id.
 *
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_schedule_profile_cpu_disable(okl4_kcap_t phys_cpu)
{
    register uint32_t r0 asm("r0") = (uint32_t)phys_cpu;
    __asm__ __volatile__(
            ""hvc(5168)"\n\t"
            : "+r"(r0)
            :
            : "cc", "memory", "r1", "r2", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_schedule_profile_cpu_disable(okl4_kcap_t phys_cpu)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)phys_cpu;
    __asm__ __volatile__(
            "" hvc(5168) "\n\t"
            : "+r"(x0)
            :
            : "cc", "memory", "x1", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Enable profiling of a physical CPU.
 *
 *    This operation enables profiling of physical CPU related properties
 *        such as
 *    core usage and context switch count.
 *
 * @param phys_cpu
 *    The physical CPU capability id.
 *
 * @retval error
 *    Resulting error.
 * @retval timestamp
 *    The current timestamp.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE struct _okl4_sys_schedule_profile_cpu_enable_return
_okl4_sys_schedule_profile_cpu_enable(okl4_kcap_t phys_cpu)
{
    typedef union {
        struct uint64 {
            uint32_t lo;
            uint32_t hi;
        } words;
        uint64_t val;
    } okl4_uint64_tmp;
    okl4_uint64_tmp timestamp_tmp;
    struct _okl4_sys_schedule_profile_cpu_enable_return result;

    register uint32_t r0 asm("r0") = (uint32_t)phys_cpu;
    register uint32_t r1 asm("r1");
    register uint32_t r2 asm("r2");
    __asm__ __volatile__(
            ""hvc(5169)"\n\t"
            : "=r"(r1), "=r"(r2), "+r"(r0)
            :
            : "cc", "memory", "r3", "r4", "r5"
            );


    result.error = (okl4_error_t)(r0);
    timestamp_tmp.words.lo = r1;
    timestamp_tmp.words.hi = r2;
    result.timestamp = (uint64_t)(timestamp_tmp.val);
    return result;
}
#endif

#else

OKL4_FORCE_INLINE struct _okl4_sys_schedule_profile_cpu_enable_return
_okl4_sys_schedule_profile_cpu_enable(okl4_kcap_t phys_cpu)
{
    struct _okl4_sys_schedule_profile_cpu_enable_return result;

    register okl4_register_t x0 asm("x0") = (okl4_register_t)phys_cpu;
    register okl4_register_t x1 asm("x1");
    __asm__ __volatile__(
            "" hvc(5169) "\n\t"
            : "=r"(x1), "+r"(x0)
            :
            : "cc", "memory", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    result.error = (okl4_error_t)(x0);
    result.timestamp = (uint64_t)(x1);
    return result;
}

#endif

/**
 *
 * @brief Retrieve profiling data relating to a physical CPU core.
 *
 *    @details
 *    This operation returns a set of profiling data relating to a physical
 *        CPU.
 *    A timestamp of the current system time in units of microseconds is
 *        recorded
 *    during the operation. The remaining data fields indicate runtime and
 *    number of events since the last invocation of this operation.
 *
 *    After the profiling data is retrieved, the kernel resets all metrics
 *        to
 *    zero.
 *
 *    @par profile data
 *    For a physical CPU, the returned data is:
 *    - \p cpu_time: Idle time of the CPU in microseconds.
 *    - \p context_switches: Number of context switches on this core.
 *    - \p enabled: True if profiling is enabled on this CPU.
 *
 * @param phys_cpu
 *    The physical CPU capability id.
 * @param profile
 *    `return by reference`. Profiling data.
 *
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_schedule_profile_cpu_get_data(okl4_kcap_t phys_cpu,
        struct okl4_schedule_profile_data *profile)
{
    register uint32_t r0 asm("r0") = (uint32_t)phys_cpu;
    register uint32_t r1 asm("r1") = (uint32_t)(uintptr_t)profile;
    __asm__ __volatile__(
            ""hvc(5170)"\n\t"
            : "+r"(r0), "+r"(r1)
            :
            : "cc", "memory", "r2", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_schedule_profile_cpu_get_data(okl4_kcap_t phys_cpu,
        struct okl4_schedule_profile_data *profile)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)phys_cpu;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)(uintptr_t)profile;
    __asm__ __volatile__(
            "" hvc(5170) "\n\t"
            : "+r"(x0), "+r"(x1)
            :
            : "cc", "memory", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Disable profiling of a vCPU.
 *
 * @param vcpu
 *    The target vCPU id.
 *
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_schedule_profile_vcpu_disable(okl4_kcap_t vcpu)
{
    register uint32_t r0 asm("r0") = (uint32_t)vcpu;
    __asm__ __volatile__(
            ""hvc(5171)"\n\t"
            : "+r"(r0)
            :
            : "cc", "memory", "r1", "r2", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_schedule_profile_vcpu_disable(okl4_kcap_t vcpu)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)vcpu;
    __asm__ __volatile__(
            "" hvc(5171) "\n\t"
            : "+r"(x0)
            :
            : "cc", "memory", "x1", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Enable profiling of a vCPU.
 *
 *    This operation enables profiling of vCPU related properties such as
 *    execution time and context switch count.
 *
 * @param vcpu
 *    The target vCPU id.
 *
 * @retval error
 *    Resulting error.
 * @retval timestamp
 *    The current timestamp.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE struct _okl4_sys_schedule_profile_vcpu_enable_return
_okl4_sys_schedule_profile_vcpu_enable(okl4_kcap_t vcpu)
{
    typedef union {
        struct uint64 {
            uint32_t lo;
            uint32_t hi;
        } words;
        uint64_t val;
    } okl4_uint64_tmp;
    okl4_uint64_tmp timestamp_tmp;
    struct _okl4_sys_schedule_profile_vcpu_enable_return result;

    register uint32_t r0 asm("r0") = (uint32_t)vcpu;
    register uint32_t r1 asm("r1");
    register uint32_t r2 asm("r2");
    __asm__ __volatile__(
            ""hvc(5172)"\n\t"
            : "=r"(r1), "=r"(r2), "+r"(r0)
            :
            : "cc", "memory", "r3", "r4", "r5"
            );


    result.error = (okl4_error_t)(r0);
    timestamp_tmp.words.lo = r1;
    timestamp_tmp.words.hi = r2;
    result.timestamp = (uint64_t)(timestamp_tmp.val);
    return result;
}
#endif

#else

OKL4_FORCE_INLINE struct _okl4_sys_schedule_profile_vcpu_enable_return
_okl4_sys_schedule_profile_vcpu_enable(okl4_kcap_t vcpu)
{
    struct _okl4_sys_schedule_profile_vcpu_enable_return result;

    register okl4_register_t x0 asm("x0") = (okl4_register_t)vcpu;
    register okl4_register_t x1 asm("x1");
    __asm__ __volatile__(
            "" hvc(5172) "\n\t"
            : "=r"(x1), "+r"(x0)
            :
            : "cc", "memory", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    result.error = (okl4_error_t)(x0);
    result.timestamp = (uint64_t)(x1);
    return result;
}

#endif

/**
 *
 * @brief Retrieve profiling data relating to a vCPU.
 *
 *    @details
 *    This operation returns a set of profiling data relating to a vCPU.
 *    A timestamp of the current system time in units of microseconds is
 *        recorded
 *    during the operation. The remaining data fields indicate runtime and
 *    number of events since the last invocation of this operation.
 *
 *    After the profiling data is retrieved, the kernel resets all metrics
 *        to
 *    zero.
 *
 *    @par profile data
 *    For a vCPU, the returned data is:
 *    - \p cpu_time: Execution time of the vCPU in microseconds.
 *    - \p context_switches: Number of context switches.
 *    - \p cpu_migrations: Number of migrations between physical CPUs.
 *    - \p enabled: True if profiling is enabled on this CPU.
 *
 * @param vcpu
 *    The target vCPU id.
 * @param profile
 *    `return by reference`. Profiling data.
 *
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_schedule_profile_vcpu_get_data(okl4_kcap_t vcpu,
        struct okl4_schedule_profile_data *profile)
{
    register uint32_t r0 asm("r0") = (uint32_t)vcpu;
    register uint32_t r1 asm("r1") = (uint32_t)(uintptr_t)profile;
    __asm__ __volatile__(
            ""hvc(5173)"\n\t"
            : "+r"(r0), "+r"(r1)
            :
            : "cc", "memory", "r2", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_schedule_profile_vcpu_get_data(okl4_kcap_t vcpu,
        struct okl4_schedule_profile_data *profile)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)vcpu;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)(uintptr_t)profile;
    __asm__ __volatile__(
            "" hvc(5173) "\n\t"
            : "+r"(x0), "+r"(x1)
            :
            : "cc", "memory", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * OKL4 Microvisor system call: SCHEDULER_SUSPEND
 *
 * @param scheduler_id
 * @param power_state
 *
 * @retval error
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_scheduler_suspend(okl4_kcap_t scheduler_id,
        okl4_power_state_t power_state)
{
    register uint32_t r0 asm("r0") = (uint32_t)scheduler_id;
    register uint32_t r1 asm("r1") = (uint32_t)power_state;
    __asm__ __volatile__(
            ""hvc(5150)"\n\t"
            : "+r"(r0), "+r"(r1)
            :
            : "cc", "memory", "r2", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_scheduler_suspend(okl4_kcap_t scheduler_id,
        okl4_power_state_t power_state)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)scheduler_id;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)power_state;
    __asm__ __volatile__(
            "" hvc(5150) "\n\t"
            : "+r"(x0), "+r"(x1)
            :
            : "cc", "memory", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Cancel an active timeout on a specified timer.
 *
 *    @details
 *    This operation cancels an active timeout on a specified timer. The
 *    operation returns the time that was remaining on the cancelled
 *        timeout.
 *    If there was not an active timeout, the operation returns an error.
 *
 *    The returned remaining time is formatted in the requested units from
 *        the
 *    \p flags argument.
 *
 *    The operation will also return the \p old_flags field indicating
 *        whether
 *    the canceled timeout was periodic or one-shot and whether it was an
 *    absolute or relative timeout.
 *
 *    @par flags
 *    - If the \p units flag is set, the remaining time is returned in
 *        units
 *    of timer ticks. The length of a timer tick is KSP defined and may be
 *    obtained with the @ref _okl4_sys_timer_get_resolution operation.
 *    - If the \p units flag is not set, the remaining time is returned in
 *    nanoseconds.
 *
 *    @par old_flags
 *    - If the \p periodic flag is set, the cancelled timeout was periodic.
 *    - If the \p periodic flag is not set, the cancelled timeout was
 *    one-shot.
 *    - If the \p absolute flag is set, the cancelled timeout was an
 *    absolute time.
 *    - If the \p absolute flag is not set, the cancelled timeout was a
 *    relative time.
 *
 * @param timer
 *    The target timer capability.
 * @param flags
 *    Flags for the requested operation.
 *
 * @retval remaining
 *    Time that was remaining on the cancelled timeout.
 * @retval old_flags
 *    Flags relating to the cancelled timeout.
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE struct _okl4_sys_timer_cancel_return
_okl4_sys_timer_cancel(okl4_kcap_t timer, okl4_timer_flags_t flags)
{
    typedef union {
        struct uint64 {
            uint32_t lo;
            uint32_t hi;
        } words;
        uint64_t val;
    } okl4_uint64_tmp;
    okl4_uint64_tmp remaining_tmp;
    struct _okl4_sys_timer_cancel_return result;

    register uint32_t r0 asm("r0") = (uint32_t)timer;
    register uint32_t r1 asm("r1") = (uint32_t)flags;
    register uint32_t r2 asm("r2");
    register uint32_t r3 asm("r3");
    __asm__ __volatile__(
            ""hvc(5176)"\n\t"
            : "=r"(r2), "=r"(r3), "+r"(r0), "+r"(r1)
            :
            : "cc", "memory", "r4", "r5"
            );


    remaining_tmp.words.lo = r0;
    remaining_tmp.words.hi = r1;
    result.remaining = (uint64_t)(remaining_tmp.val);
    result.old_flags = (okl4_timer_flags_t)(r2);
    result.error = (okl4_error_t)(r3);
    return result;
}
#endif

#else

OKL4_FORCE_INLINE struct _okl4_sys_timer_cancel_return
_okl4_sys_timer_cancel(okl4_kcap_t timer, okl4_timer_flags_t flags)
{
    struct _okl4_sys_timer_cancel_return result;

    register okl4_register_t x0 asm("x0") = (okl4_register_t)timer;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)flags;
    register okl4_register_t x2 asm("x2");
    __asm__ __volatile__(
            "" hvc(5176) "\n\t"
            : "=r"(x2), "+r"(x0), "+r"(x1)
            :
            : "cc", "memory", "x3", "x4", "x5", "x6", "x7"
            );


    result.remaining = (uint64_t)(x0);
    result.old_flags = (okl4_timer_flags_t)(x1);
    result.error = (okl4_error_t)(x2);
    return result;
}

#endif

/**
 *
 * @brief Query the timer frequency and obtain time conversion constants.
 *
 *    @details
 *    This operation returns the timer frequency and the conversion
 *        constants
 *    that may be used to convert between units of nanoseconds and units of
 *    ticks.
 *
 *    The timer frequency is returned as a 64-bit value in units of
 *        micro-hertz.
 *    (1000000 = 1Hz).
 *    The timer resolution (or period) can be calculated from the
 *        frequency.
 *
 *    The time conversion constants are retuned as values \p a and \p b
 *        which can
 *    be used for unit conversions as follows:
 *    - ns = (ticks) * \p a / \p b
 *    - ticks = (ns * \p b) / \p a
 *
 *    @note
 *    The constants are provided by the KSP module and are designed to be
 *        used
 *    for simple overflow-free computation using 64-bit arithmetic covering
 *        the
 *    time values from 0 to 2 years.
 *
 * @param timer
 *    The target timer capability.
 *
 * @retval tick_freq
 *    The timer frequency [in units of micro-hertz].
 * @retval a
 *    Ticks to nanoseconds conversion multiplier.
 * @retval b
 *    Ticks to nanoseconds conversion divisor.
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE struct _okl4_sys_timer_get_resolution_return
_okl4_sys_timer_get_resolution(okl4_kcap_t timer)
{
    typedef union {
        struct uint64 {
            uint32_t lo;
            uint32_t hi;
        } words;
        uint64_t val;
    } okl4_uint64_tmp;
    okl4_uint64_tmp tick_freq_tmp;
    struct _okl4_sys_timer_get_resolution_return result;

    register uint32_t r0 asm("r0") = (uint32_t)timer;
    register uint32_t r1 asm("r1");
    register uint32_t r2 asm("r2");
    register uint32_t r3 asm("r3");
    register uint32_t r4 asm("r4");
    __asm__ __volatile__(
            ""hvc(5177)"\n\t"
            : "=r"(r1), "=r"(r2), "=r"(r3), "=r"(r4), "+r"(r0)
            :
            : "cc", "memory", "r5"
            );


    tick_freq_tmp.words.lo = r0;
    tick_freq_tmp.words.hi = r1;
    result.tick_freq = (uint64_t)(tick_freq_tmp.val);
    result.a = (uint32_t)(r2);
    result.b = (uint32_t)(r3);
    result.error = (okl4_error_t)(r4);
    return result;
}
#endif

#else

OKL4_FORCE_INLINE struct _okl4_sys_timer_get_resolution_return
_okl4_sys_timer_get_resolution(okl4_kcap_t timer)
{
    struct _okl4_sys_timer_get_resolution_return result;

    register okl4_register_t x0 asm("x0") = (okl4_register_t)timer;
    register okl4_register_t x1 asm("x1");
    register okl4_register_t x2 asm("x2");
    register okl4_register_t x3 asm("x3");
    __asm__ __volatile__(
            "" hvc(5177) "\n\t"
            : "=r"(x1), "=r"(x2), "=r"(x3), "+r"(x0)
            :
            : "cc", "memory", "x4", "x5", "x6", "x7"
            );


    result.tick_freq = (uint64_t)(x0);
    result.a = (uint32_t)(x1);
    result.b = (uint32_t)(x2);
    result.error = (okl4_error_t)(x3);
    return result;
}

#endif

/**
 *
 * @brief Query the current system time.
 *
 *    @details
 *    This operation returns the current absolute system time. The \p flags
 *    argument is used to specify the desired units for the return value.
 *
 *    - Absolute time is based on an arbitrary time zero, defined to be at
 *    or before the time of boot.
 *
 *    @par flags
 *    - If the \p units flag is set, the time is returned in units
 *    of timer ticks. The length of a timer tick is KSP defined and may
 *    be obtained with the @ref _okl4_sys_timer_get_resolution operation.
 *    - If the \p units flag is not set, the time is returned in
 *    terms of nanoseconds.
 *
 * @param timer
 *    The target timer capability.
 * @param flags
 *    Flags for the requested operation.
 *
 * @retval time
 *    The current system time.
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE struct _okl4_sys_timer_get_time_return
_okl4_sys_timer_get_time(okl4_kcap_t timer, okl4_timer_flags_t flags)
{
    typedef union {
        struct uint64 {
            uint32_t lo;
            uint32_t hi;
        } words;
        uint64_t val;
    } okl4_uint64_tmp;
    okl4_uint64_tmp time_tmp;
    struct _okl4_sys_timer_get_time_return result;

    register uint32_t r0 asm("r0") = (uint32_t)timer;
    register uint32_t r1 asm("r1") = (uint32_t)flags;
    register uint32_t r2 asm("r2");
    __asm__ __volatile__(
            ""hvc(5178)"\n\t"
            : "=r"(r2), "+r"(r0), "+r"(r1)
            :
            : "cc", "memory", "r3", "r4", "r5"
            );


    time_tmp.words.lo = r0;
    time_tmp.words.hi = r1;
    result.time = (uint64_t)(time_tmp.val);
    result.error = (okl4_error_t)(r2);
    return result;
}
#endif

#else

OKL4_FORCE_INLINE struct _okl4_sys_timer_get_time_return
_okl4_sys_timer_get_time(okl4_kcap_t timer, okl4_timer_flags_t flags)
{
    struct _okl4_sys_timer_get_time_return result;

    register okl4_register_t x0 asm("x0") = (okl4_register_t)timer;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)flags;
    __asm__ __volatile__(
            "" hvc(5178) "\n\t"
            : "+r"(x0), "+r"(x1)
            :
            : "cc", "memory", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    result.time = (uint64_t)(x0);
    result.error = (okl4_error_t)(x1);
    return result;
}

#endif

/**
 *
 * @brief Query a timer about an active timeout.
 *
 *    @details
 *    The operation queries a timer about an active timeout. If there is no
 *    active timeout, this operation returns an error.
 *
 *    If the timer has an active timeout, this operation returns the
 *        remaining
 *    time and the flags associated with the timeout. The remaining time is
 *    returned in the requested units from the \p flags argument.
 *
 *    The operation also returns the \p active_flags field indicating
 *        whether the
 *    active timeout is periodic or one-shot and whether it was an absolute
 *        or
 *    relative timeout.
 *
 *    @par flags
 *    - If the \p units flag is set, the remaining time is returned in
 *        units
 *    of timer ticks. The length of a timer tick is KSP defined and may
 *    be obtained with the @ref _okl4_sys_timer_get_resolution operation.
 *    - If the \p units flag is not set, the remaining time is returned in
 *    units of nanoseconds.
 *
 *    @par active_flags
 *    - If the \p periodic flag is set, the timeout is periodic.
 *    - If the \p periodic flag is not set, the timeout is one-shot.
 *    - If the \p absolute flag is set, the timeout is an absolute time.
 *    - If the \p absolute flag is not set, the timeout is a relative time.
 *
 * @param timer
 *    The target timer capability.
 * @param flags
 *    Flags for the requested operation.
 *
 * @retval remaining
 *    Time remaining before the next timeout.
 * @retval active_flags
 *    Flags relating to the active timeout.
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE struct _okl4_sys_timer_query_return
_okl4_sys_timer_query(okl4_kcap_t timer, okl4_timer_flags_t flags)
{
    typedef union {
        struct uint64 {
            uint32_t lo;
            uint32_t hi;
        } words;
        uint64_t val;
    } okl4_uint64_tmp;
    okl4_uint64_tmp remaining_tmp;
    struct _okl4_sys_timer_query_return result;

    register uint32_t r0 asm("r0") = (uint32_t)timer;
    register uint32_t r1 asm("r1") = (uint32_t)flags;
    register uint32_t r2 asm("r2");
    register uint32_t r3 asm("r3");
    __asm__ __volatile__(
            ""hvc(5179)"\n\t"
            : "=r"(r2), "=r"(r3), "+r"(r0), "+r"(r1)
            :
            : "cc", "memory", "r4", "r5"
            );


    remaining_tmp.words.lo = r0;
    remaining_tmp.words.hi = r1;
    result.remaining = (uint64_t)(remaining_tmp.val);
    result.active_flags = (okl4_timer_flags_t)(r2);
    result.error = (okl4_error_t)(r3);
    return result;
}
#endif

#else

OKL4_FORCE_INLINE struct _okl4_sys_timer_query_return
_okl4_sys_timer_query(okl4_kcap_t timer, okl4_timer_flags_t flags)
{
    struct _okl4_sys_timer_query_return result;

    register okl4_register_t x0 asm("x0") = (okl4_register_t)timer;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)flags;
    register okl4_register_t x2 asm("x2");
    __asm__ __volatile__(
            "" hvc(5179) "\n\t"
            : "=r"(x2), "+r"(x0), "+r"(x1)
            :
            : "cc", "memory", "x3", "x4", "x5", "x6", "x7"
            );


    result.remaining = (uint64_t)(x0);
    result.active_flags = (okl4_timer_flags_t)(x1);
    result.error = (okl4_error_t)(x2);
    return result;
}

#endif

/**
 *
 * @brief Start a timer with a specified timeout.
 *
 *    @details
 *    This operation optionally resets then starts a timer with a new
 *        timeout.
 *    The specified timeout may be an `absolute` or `relative` time, may be
 *    `one-shot` or `periodic` and may be specified in units of nanoseconds
 *        or
 *    ticks.
 *
 *    @par flags
 *    - If the \p absolute flag is set, the timeout is treated as an
 *    absolute time based on an arbitrary time zero, defined to be at or
 *    before the time of boot.
 *    - If the \p absolute flag is not set, the timeout is treated as a
 *    relative time a specified amount of into the future. E.g. 10ms from
 *    now.
 *    - If the \p periodic flag is set, the timeout is treated as a
 *        periodic
 *    timeout that repeats with a period equal to the specified timeout.
 *    - If the \p periodic flag is not set, the timeout is treated as a
 *    one-shot timeout that expires at the specified time and does not
 *    repeat.
 *    - If the \p units flag is set, the timeout is specified in units of
 *    timer ticks. The length of a timer tick is KSP defined and may be
 *    obtained with the @ref _okl4_sys_timer_get_resolution operation.
 *    - If the \p units flag is not set, the timeout is specified in units
 *    of nanoseconds.
 *    - The \p reload flag allows an active timeout to be cancelled and the
 *    new timeout is programmed into the timer.
 *
 * @param timer
 *    The target timer capability.
 * @param timeout
 *    The timeout value.
 * @param flags
 *    Flags for the requested operation.
 *
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_timer_start(okl4_kcap_t timer, uint64_t timeout,
        okl4_timer_flags_t flags)
{
    register uint32_t r0 asm("r0") = (uint32_t)timer;
    register uint32_t r1 asm("r1") = (uint32_t)(timeout        & 0xffffffff);
    register uint32_t r2 asm("r2") = (uint32_t)((timeout >> 32) & 0xffffffff);
    register uint32_t r3 asm("r3") = (uint32_t)flags;
    __asm__ __volatile__(
            ""hvc(5180)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3)
            :
            : "cc", "memory", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_timer_start(okl4_kcap_t timer, uint64_t timeout,
        okl4_timer_flags_t flags)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)timer;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)timeout;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)flags;
    __asm__ __volatile__(
            "" hvc(5180) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2)
            :
            : "cc", "memory", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * OKL4 Microvisor system call: TRACEBUFFER_SYNC
 *
 *
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE void
_okl4_sys_tracebuffer_sync(void)
{
    __asm__ __volatile__(
            ""hvc(5199)"\n\t"
            :
            :
            : "cc", "memory", "r0", "r1", "r2", "r3", "r4", "r5"
            );

}
#endif

#else

OKL4_FORCE_INLINE void
_okl4_sys_tracebuffer_sync(void)
{
    __asm__ __volatile__(
            "" hvc(5199) "\n\t"
            :
            :
            : "cc", "memory", "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7"
            );

}

#endif

/**
 *
 * @brief Reset a vCPU.
 *
 *    @details
 *    This operation resets a vCPU to its boot state.
 *
 * @param vcpu
 *    The target vCPU capability.
 *
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_vcpu_reset(okl4_kcap_t vcpu)
{
    register uint32_t r0 asm("r0") = (uint32_t)vcpu;
    __asm__ __volatile__(
            ""hvc(5122)"\n\t"
            : "+r"(r0)
            :
            : "cc", "memory", "r1", "r2", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_vcpu_reset(okl4_kcap_t vcpu)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)vcpu;
    __asm__ __volatile__(
            "" hvc(5122) "\n\t"
            : "+r"(x0)
            :
            : "cc", "memory", "x1", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Start a vCPU executing.
 *
 *    @details
 *    This operation starts a stopped vCPU, at an optionally specified
 *    instruction pointer. If instruction pointer is not to be set the
 *    value at the previous stop is preserved.
 *
 * @param vcpu
 *    The target vCPU capability.
 * @param set_ip
 *    Should the instruction pointer be set.
 * @param ip
 *    Instruction pointer to start the vCPU at.
 *
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_vcpu_start(okl4_kcap_t vcpu, okl4_bool_t set_ip, void *ip)
{
    register uint32_t r0 asm("r0") = (uint32_t)vcpu;
    register uint32_t r1 asm("r1") = (uint32_t)set_ip;
    register uint32_t r2 asm("r2") = (uint32_t)(uintptr_t)ip;
    __asm__ __volatile__(
            ""hvc(5123)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2)
            :
            : "cc", "memory", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_vcpu_start(okl4_kcap_t vcpu, okl4_bool_t set_ip, void *ip)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)vcpu;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)set_ip;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)(uintptr_t)ip;
    __asm__ __volatile__(
            "" hvc(5123) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2)
            :
            : "cc", "memory", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Stop a vCPU executing.
 *
 *    @details
 *    This operation stops a vCPU's execution until next restarted.
 *
 * @param vcpu
 *    The target vCPU capability.
 *
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_vcpu_stop(okl4_kcap_t vcpu)
{
    register uint32_t r0 asm("r0") = (uint32_t)vcpu;
    __asm__ __volatile__(
            ""hvc(5124)"\n\t"
            : "+r"(r0)
            :
            : "cc", "memory", "r1", "r2", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_vcpu_stop(okl4_kcap_t vcpu)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)vcpu;
    __asm__ __volatile__(
            "" hvc(5124) "\n\t"
            : "+r"(x0)
            :
            : "cc", "memory", "x1", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Switch a vCPU's execution mode between 32-bit and 64-bit.
 *
 *    @details
 *    This operation resets a vCPU to its boot state, switches between
 *        32-bit
 *    and 64-bit modes, and restarts execution at the specified address.
 *        The
 *    start address must be valid in the vCPU's initial address space,
 *        which may
 *    not be the same as the caller's address space.
 *
 * @param vcpu
 *    The target vCPU capability.
 * @param to_64bit
 *    The vCPU will reset in 64-bit mode if true; otherwise in 32-bit mode
 * @param set_ip
 *    Should the instruction pointer be set.
 * @param ip
 *    Instruction pointer to start the vCPU at.
 *
 * @retval error
 *    Resulting error.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_vcpu_switch_mode(okl4_kcap_t vcpu, okl4_bool_t to_64bit,
        okl4_bool_t set_ip, void *ip)
{
    register uint32_t r0 asm("r0") = (uint32_t)vcpu;
    register uint32_t r1 asm("r1") = (uint32_t)to_64bit;
    register uint32_t r2 asm("r2") = (uint32_t)set_ip;
    register uint32_t r3 asm("r3") = (uint32_t)(uintptr_t)ip;
    __asm__ __volatile__(
            ""hvc(5125)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3)
            :
            : "cc", "memory", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_vcpu_switch_mode(okl4_kcap_t vcpu, okl4_bool_t to_64bit,
        okl4_bool_t set_ip, void *ip)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)vcpu;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)to_64bit;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)set_ip;
    register okl4_register_t x3 asm("x3") = (okl4_register_t)(uintptr_t)ip;
    __asm__ __volatile__(
            "" hvc(5125) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3)
            :
            : "cc", "memory", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Signal a synchronization event.
 *
 *    @details
 *    This operation sets the wakeup flags for all vCPUs in the caller's
 *        domain.
 *    If any vCPUs in the domain are waiting due to a pending `sync_wfe`
 *        operation,
 *    they will be released from the wait. The OKL4 scheduler will then
 *        determine
 *    which vCPUs should execute first based on their priority.
 *
 *    This `sync_sev` operation is non-blocking and is used to signal other
 *        vCPUs
 *    about some user-defined event. A typical use of this operation is to
 *        signal
 *    the release of a spinlock to other waiting vCPUs.
 *
 *    @see _okl4_sys_vcpu_sync_wfe
 *
 *
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE void
_okl4_sys_vcpu_sync_sev(void)
{
    __asm__ __volatile__(
            ""hvc(5126)"\n\t"
            :
            :
            : "cc", "memory", "r0", "r1", "r2", "r3", "r4", "r5"
            );

}
#endif

#else

OKL4_FORCE_INLINE void
_okl4_sys_vcpu_sync_sev(void)
{
    __asm__ __volatile__(
            "" hvc(5126) "\n\t"
            :
            :
            : "cc", "memory", "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7"
            );

}

#endif

/**
 *
 * @brief Wait for a synchronization event.
 *
 *    @details
 *    This operation is used to defer the execution of a vCPU while it is
 *        waiting
 *    for an event. This operation is non-blocking, in that if no other
 *        vCPUs in
 *    the system are runnable, the operation will complete and the vCPU is
 *        not
 *    blocked. The `sync_wfe` operation uses the \p holder argument as a
 *        hint to
 *    the vCPU the caller is waiting on.
 *
 *    This operation first determines whether there is a pending wakeup
 *        flag set
 *    for the calling vCPU. If the flag is set, the operation clears the
 *        flag and
 *    returns immediately. If the caller has provided a valid \p holder id,
 *        and
 *    the holder is currently executing on a different physical core, the
 *    operation again returns immediately.
 *
 *    In all other cases, the Microvisor records that the vCPU is waiting
 *        and
 *    reduces the vCPU's priority temporarily to the lowest priority in
 *    the system. The scheduler is then invoked to rebalance the system.
 *
 *    A waiting vCPU will continue execution and return from the `sync_wfe`
 *    operation as soon as no higher priority vCPUs in the system are
 *        available
 *    for scheduling, or a wake-up event is signalled by another vCPU in
 *        the same
 *    domain.
 *
 *    @par holder
 *    The holder identifier may be a valid capability to another vCPU, or
 *        an
 *    invalid id. If the provided id is valid, it is used as a hint to the
 *    Microvisor that the caller is waiting on the specified vCPU. The
 *    `vcpu_sync` API is optimized for short spinlock type use-cases and
 *        will
 *    therefore allow the caller to continue execution without waiting, if
 *        the
 *    target \p holder vCPU is presently running on another physical core.
 *        This
 *    is done to reduce latency with the expectation that the holder vCPU
 *        will
 *    soon release the lock.
 *
 *    @see _okl4_sys_vcpu_sync_sev
 *
 * @param holder
 *    Capability of the vCPU to wait for, or an invalid designator.
 *
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE void
_okl4_sys_vcpu_sync_wfe(okl4_kcap_t holder)
{
    register uint32_t r0 asm("r0") = (uint32_t)holder;
    __asm__ __volatile__(
            ""hvc(5127)"\n\t"
            : "+r"(r0)
            :
            : "cc", "memory", "r1", "r2", "r3", "r4", "r5"
            );

}
#endif

#else

OKL4_FORCE_INLINE void
_okl4_sys_vcpu_sync_wfe(okl4_kcap_t holder)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)holder;
    __asm__ __volatile__(
            "" hvc(5127) "\n\t"
            : "+r"(x0)
            :
            : "cc", "memory", "x1", "x2", "x3", "x4", "x5", "x6", "x7"
            );

}

#endif

/**
 *
 * @brief Atomically fetch an interrupt payload and raise a virtual interrupt.
 *
 *    @details
 *    This API is equivalent to atomically calling @ref
 *        sys_interrupt_get_payload
 *    and @ref sys_vinterrupt_modify. Typically, the specified virtual
 *        interrupt
 *    will be one that is not attached to the specified virtual interrupt
 *        source,
 *    but this is not enforced. If only one virtual interrupt source is
 *        affected,
 *    then the @ref sys_interrupt_get_payload phase will occur first.
 *
 *    Certain communication protocols must perform this sequence of
 *        operations
 *    atomically in order to maintain consistency. Other than being atomic,
 *        this
 *    is no different to invoking the two component operations separately.
 *
 * @param irq
 *    An interrupt line number for the virtual GIC.
 * @param virqline
 *    A virtual interrupt line capability.
 * @param mask
 *    A machine-word-sized array of payload flags to preserve.
 * @param payload
 *    A machine-word-sized array of payload flags to set.
 *
 * @retval error
 *    The resulting error value.
 * @retval payload
 *    Accumulated virtual interrupt payload flags.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE struct _okl4_sys_vinterrupt_clear_and_raise_return
_okl4_sys_vinterrupt_clear_and_raise(okl4_interrupt_number_t irq,
        okl4_kcap_t virqline, okl4_virq_flags_t mask, okl4_virq_flags_t payload)
{
    typedef union {
        struct uint64 {
            uint32_t lo;
            uint32_t hi;
        } words;
        uint64_t val;
    } okl4_uint64_tmp;
    okl4_uint64_tmp payload_tmp;
    struct _okl4_sys_vinterrupt_clear_and_raise_return result;

    register uint32_t r0 asm("r0") = (uint32_t)irq;
    register uint32_t r1 asm("r1") = (uint32_t)virqline;
    register uint32_t r2 asm("r2") = (uint32_t)(mask        & 0xffffffff);
    register uint32_t r3 asm("r3") = (uint32_t)((mask >> 32) & 0xffffffff);
    register uint32_t r4 asm("r4") = (uint32_t)(payload        & 0xffffffff);
    register uint32_t r5 asm("r5") = (uint32_t)((payload >> 32) & 0xffffffff);
    __asm__ __volatile__(
            ""hvc(5194)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3), "+r"(r4), "+r"(r5)
            :
            : "cc", "memory"
            );


    result.error = (okl4_error_t)(r0);
    payload_tmp.words.lo = r1;
    payload_tmp.words.hi = r2;
    result.payload = (okl4_virq_flags_t)(payload_tmp.val);
    return result;
}
#endif

#else

OKL4_FORCE_INLINE struct _okl4_sys_vinterrupt_clear_and_raise_return
_okl4_sys_vinterrupt_clear_and_raise(okl4_interrupt_number_t irq,
        okl4_kcap_t virqline, okl4_virq_flags_t mask, okl4_virq_flags_t payload)
{
    struct _okl4_sys_vinterrupt_clear_and_raise_return result;

    register okl4_register_t x0 asm("x0") = (okl4_register_t)irq;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)virqline;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)mask;
    register okl4_register_t x3 asm("x3") = (okl4_register_t)payload;
    __asm__ __volatile__(
            "" hvc(5194) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3)
            :
            : "cc", "memory", "x4", "x5", "x6", "x7"
            );


    result.error = (okl4_error_t)(x0);
    result.payload = (okl4_virq_flags_t)(x1);
    return result;
}

#endif

/**
 *
 * @brief Raise a virtual interrupt, and modify the payload flags.
 *
 *    @details
 *    This triggers a virtual interrupt by raising a virtual interrupt
 *        source. A
 *    virtual interrupt source object is distinct from a virtual interrupt.
 *        A
 *    virtual interrupt source is always linked to a virtual interrupt, but
 *        the
 *    reverse is not true.
 *
 *    Each Microvisor virtual interrupt carries a payload of flags which
 *        may be
 *    fetched by the recipient of the interrupt. An interrupt payload is a
 *        @ref
 *    okl4_word_t sized array of flags, packed into a single word. Flags
 *        are
 *    cleared whenever the interrupt recipient fetches the payload with the
 *        @ref
 *    okl4_sys_interrupt_get_payload API.
 *
 *    The interrupt-modify API allows the caller to pass in a new set of
 *        flags in
 *    the \p payload field, and a set of flags to keep from the previous
 *        payload
 *    in the \p mask field. If the interrupt has previously been raised and
 *        not
 *    yet delivered, the flags accumulate with a mask; that is, each flag
 *        is the
 *    boolean OR of the specified value with the boolean AND of its
 *        previous
 *    value and the mask.
 *
 *    When the recipient has configured the interrupt for edge triggering,
 *        an
 *    invocation of this API is counted as a single edge; this triggers
 *        interrupt
 *    delivery if the interrupt is not already pending, irrespective of the
 *    payload. If the interrupt is configured for level triggering, then
 *        its
 *    pending state is the boolean OR of its payload flags after any
 *        specified
 *    flags are cleared or raised; at least one flag must be set in the new
 *    payload to permit delivery of a level-triggered interrupt.
 *
 * @param virqline
 *    A virtual interrupt line capability.
 * @param mask
 *    A machine-word-sized array of payload flags to preserve.
 * @param payload
 *    A machine-word-sized array of payload flags to set.
 *
 * @retval error
 *    The resulting error value.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_vinterrupt_modify(okl4_kcap_t virqline, okl4_virq_flags_t mask,
        okl4_virq_flags_t payload)
{
    register uint32_t r0 asm("r0") = (uint32_t)virqline;
    register uint32_t r1 asm("r1") = (uint32_t)(mask        & 0xffffffff);
    register uint32_t r2 asm("r2") = (uint32_t)((mask >> 32) & 0xffffffff);
    register uint32_t r3 asm("r3") = (uint32_t)(payload        & 0xffffffff);
    register uint32_t r4 asm("r4") = (uint32_t)((payload >> 32) & 0xffffffff);
    __asm__ __volatile__(
            ""hvc(5195)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3), "+r"(r4)
            :
            : "cc", "memory", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_vinterrupt_modify(okl4_kcap_t virqline, okl4_virq_flags_t mask,
        okl4_virq_flags_t payload)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)virqline;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)mask;
    register okl4_register_t x2 asm("x2") = (okl4_register_t)payload;
    __asm__ __volatile__(
            "" hvc(5195) "\n\t"
            : "+r"(x0), "+r"(x1), "+r"(x2)
            :
            : "cc", "memory", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif

/**
 *
 * @brief Raise a virtual interrupt, setting specified payload flags.
 *
 *    @details
 *    This triggers a virtual interrupt by raising a virtual interrupt
 *        source. A
 *    virtual interrupt source object is distinct from a virtual interrupt.
 *        A
 *    virtual interrupt source is always linked to a virtual interrupt, but
 *        the
 *    reverse is not true.
 *
 *    Each Microvisor virtual interrupt carries a payload of flags which
 *        may be
 *    fetched by the recipient of the interrupt. An interrupt payload is a
 *        @ref
 *    okl4_word_t sized array of flags, packed into a single word. Flags
 *        are
 *    cleared whenever the interrupt recipient fetches the payload with the
 *        @ref
 *    okl4_sys_interrupt_get_payload API.
 *
 *    The interrupt-raise API allows the caller to pass in a new set of
 *        flags in
 *    the \p payload field. If the interrupt has previously been raised and
 *        not
 *    yet delivered, the flags accumulate; that is, each flag is the
 *        boolean OR
 *    of its previous value and the specified value.
 *
 *    When the recipient has configured the interrupt for edge triggering,
 *        an
 *    invocation of this API is counted as a single edge; this triggers
 *        interrupt
 *    delivery if the interrupt is not already pending, irrespective of the
 *    payload. If the interrupt is configured for level triggering, then
 *        its
 *    pending state is the boolean OR of its payload flags after any
 *        specified
 *    flags are raised; at least one flag must be set in the new payload to
 *    permit delivery of a level-triggered interrupt.
 *
 *    @note Invoking this API is equivalent to invoking the @ref
 *    okl4_sys_vinterrupt_modify API with all bits set in the \p mask
 *        value.
 *
 *    @note This API is distinct from the @ref okl4_sys_interrupt_raise
 *        API,
 *    which raises a local software-generated interrupt without requiring
 *        an
 *    explicit capability.
 *
 * @param virqline
 *    A virtual interrupt line capability.
 * @param payload
 *    A machine-word-sized array of payload flags to set.
 *
 * @retval error
 *    The resulting error value.
 *
 */

#if defined(__ARM_EABI__)

#if defined(__RVCT__) || defined(__RVCT_GNU__)
#elif defined(__ADS__)
#else
OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_vinterrupt_raise(okl4_kcap_t virqline, okl4_virq_flags_t payload)
{
    register uint32_t r0 asm("r0") = (uint32_t)virqline;
    register uint32_t r1 asm("r1") = (uint32_t)(payload        & 0xffffffff);
    register uint32_t r2 asm("r2") = (uint32_t)((payload >> 32) & 0xffffffff);
    __asm__ __volatile__(
            ""hvc(5196)"\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2)
            :
            : "cc", "memory", "r3", "r4", "r5"
            );


    return (okl4_error_t)r0;
}
#endif

#else

OKL4_FORCE_INLINE okl4_error_t
_okl4_sys_vinterrupt_raise(okl4_kcap_t virqline, okl4_virq_flags_t payload)
{
    register okl4_register_t x0 asm("x0") = (okl4_register_t)virqline;
    register okl4_register_t x1 asm("x1") = (okl4_register_t)payload;
    __asm__ __volatile__(
            "" hvc(5196) "\n\t"
            : "+r"(x0), "+r"(x1)
            :
            : "cc", "memory", "x2", "x3", "x4", "x5", "x6", "x7"
            );


    return (okl4_error_t)x0;
}

#endif


/*lint -restore */

#endif /* !ASSEMBLY */

/*
 * Assembly system call prototypes / numbers.
 */

/** @addtogroup lib_microvisor_syscall_numbers Microvisor System Call Numbers
 * @{
 */
#define OKL4_SYSCALL_AXON_PROCESS_RECV 5184

#define OKL4_SYSCALL_AXON_SET_HALTED 5186

#define OKL4_SYSCALL_AXON_SET_RECV_AREA 5187

#define OKL4_SYSCALL_AXON_SET_RECV_QUEUE 5188

#define OKL4_SYSCALL_AXON_SET_RECV_SEGMENT 5189

#define OKL4_SYSCALL_AXON_SET_SEND_AREA 5190

#define OKL4_SYSCALL_AXON_SET_SEND_QUEUE 5191

#define OKL4_SYSCALL_AXON_SET_SEND_SEGMENT 5192

#define OKL4_SYSCALL_AXON_TRIGGER_SEND 5185

#define OKL4_SYSCALL_INTERRUPT_ACK 5128

#define OKL4_SYSCALL_INTERRUPT_ATTACH_PRIVATE 5134

#define OKL4_SYSCALL_INTERRUPT_ATTACH_SHARED 5135

#define OKL4_SYSCALL_INTERRUPT_DETACH 5136

#define OKL4_SYSCALL_INTERRUPT_DIST_ENABLE 5133

#define OKL4_SYSCALL_INTERRUPT_EOI 5129

#define OKL4_SYSCALL_INTERRUPT_GET_HIGHEST_PRIORITY_PENDING 5137

#define OKL4_SYSCALL_INTERRUPT_GET_PAYLOAD 5132

#define OKL4_SYSCALL_INTERRUPT_LIMITS 5138

#define OKL4_SYSCALL_INTERRUPT_MASK 5130

#define OKL4_SYSCALL_INTERRUPT_RAISE 5145

#define OKL4_SYSCALL_INTERRUPT_SET_BINARY_POINT 5139

#define OKL4_SYSCALL_INTERRUPT_SET_CONFIG 5140

#define OKL4_SYSCALL_INTERRUPT_SET_CONTROL 5141

#define OKL4_SYSCALL_INTERRUPT_SET_PRIORITY 5142

#define OKL4_SYSCALL_INTERRUPT_SET_PRIORITY_MASK 5143

#define OKL4_SYSCALL_INTERRUPT_SET_TARGETS 5144

#define OKL4_SYSCALL_INTERRUPT_UNMASK 5131

#define OKL4_SYSCALL_KDB_INTERACT 5120

#define OKL4_SYSCALL_KDB_SET_OBJECT_NAME 5121

#define OKL4_SYSCALL_KSP_PROCEDURE_CALL 5197

#define OKL4_SYSCALL_MMU_ATTACH_SEGMENT 5152

#define OKL4_SYSCALL_MMU_DETACH_SEGMENT 5153

#define OKL4_SYSCALL_MMU_FLUSH_RANGE 5154

#define OKL4_SYSCALL_MMU_FLUSH_RANGE_PN 5155

#define OKL4_SYSCALL_MMU_LOOKUP_PAGE 5156

#define OKL4_SYSCALL_MMU_LOOKUP_PN 5157

#define OKL4_SYSCALL_MMU_MAP_PAGE 5158

#define OKL4_SYSCALL_MMU_MAP_PN 5159

#define OKL4_SYSCALL_MMU_UNMAP_PAGE 5160

#define OKL4_SYSCALL_MMU_UNMAP_PN 5161

#define OKL4_SYSCALL_MMU_UPDATE_PAGE_ATTRS 5162

#define OKL4_SYSCALL_MMU_UPDATE_PAGE_PERMS 5163

#define OKL4_SYSCALL_MMU_UPDATE_PN_ATTRS 5164

#define OKL4_SYSCALL_MMU_UPDATE_PN_PERMS 5165

#define OKL4_SYSCALL_PERFORMANCE_NULL_SYSCALL 5198

#define OKL4_SYSCALL_PIPE_CONTROL 5146

#define OKL4_SYSCALL_PIPE_RECV 5147

#define OKL4_SYSCALL_PIPE_SEND 5148

#define OKL4_SYSCALL_PRIORITY_WAIVE 5151

#define OKL4_SYSCALL_REMOTE_GET_REGISTER 5200

#define OKL4_SYSCALL_REMOTE_GET_REGISTERS 5201

#define OKL4_SYSCALL_REMOTE_READ_MEMORY32 5202

#define OKL4_SYSCALL_REMOTE_SET_REGISTER 5203

#define OKL4_SYSCALL_REMOTE_SET_REGISTERS 5204

#define OKL4_SYSCALL_REMOTE_WRITE_MEMORY32 5205

#define OKL4_SYSCALL_SCHEDULE_METRICS_STATUS_SUSPENDED 5206

#define OKL4_SYSCALL_SCHEDULE_METRICS_WATCH_SUSPENDED 5207

#define OKL4_SYSCALL_SCHEDULE_PROFILE_CPU_DISABLE 5168

#define OKL4_SYSCALL_SCHEDULE_PROFILE_CPU_ENABLE 5169

#define OKL4_SYSCALL_SCHEDULE_PROFILE_CPU_GET_DATA 5170

#define OKL4_SYSCALL_SCHEDULE_PROFILE_VCPU_DISABLE 5171

#define OKL4_SYSCALL_SCHEDULE_PROFILE_VCPU_ENABLE 5172

#define OKL4_SYSCALL_SCHEDULE_PROFILE_VCPU_GET_DATA 5173

#define OKL4_SYSCALL_SCHEDULER_SUSPEND 5150

#define OKL4_SYSCALL_TIMER_CANCEL 5176

#define OKL4_SYSCALL_TIMER_GET_RESOLUTION 5177

#define OKL4_SYSCALL_TIMER_GET_TIME 5178

#define OKL4_SYSCALL_TIMER_QUERY 5179

#define OKL4_SYSCALL_TIMER_START 5180

#define OKL4_SYSCALL_TRACEBUFFER_SYNC 5199

#define OKL4_SYSCALL_VCPU_RESET 5122

#define OKL4_SYSCALL_VCPU_START 5123

#define OKL4_SYSCALL_VCPU_STOP 5124

#define OKL4_SYSCALL_VCPU_SWITCH_MODE 5125

#define OKL4_SYSCALL_VCPU_SYNC_SEV 5126

#define OKL4_SYSCALL_VCPU_SYNC_WFE 5127

#define OKL4_SYSCALL_VINTERRUPT_CLEAR_AND_RAISE 5194

#define OKL4_SYSCALL_VINTERRUPT_MODIFY 5195

#define OKL4_SYSCALL_VINTERRUPT_RAISE 5196

/** @} */
#undef hvc

#if defined(_definitions_for_linters)
/* Ignore lint identifier clashes for syscall names. */
/*lint -esym(621, _okl4_sys_axon_process_recv) */
/*lint -esym(621, _okl4_sys_axon_set_halted) */
/*lint -esym(621, _okl4_sys_axon_set_recv_area) */
/*lint -esym(621, _okl4_sys_axon_set_recv_queue) */
/*lint -esym(621, _okl4_sys_axon_set_recv_segment) */
/*lint -esym(621, _okl4_sys_axon_set_send_area) */
/*lint -esym(621, _okl4_sys_axon_set_send_queue) */
/*lint -esym(621, _okl4_sys_axon_set_send_segment) */
/*lint -esym(621, _okl4_sys_axon_trigger_send) */
/*lint -esym(621, _okl4_sys_interrupt_ack) */
/*lint -esym(621, _okl4_sys_interrupt_attach_private) */
/*lint -esym(621, _okl4_sys_interrupt_attach_shared) */
/*lint -esym(621, _okl4_sys_interrupt_detach) */
/*lint -esym(621, _okl4_sys_interrupt_dist_enable) */
/*lint -esym(621, _okl4_sys_interrupt_eoi) */
/*lint -esym(621, _okl4_sys_interrupt_get_highest_priority_pending) */
/*lint -esym(621, _okl4_sys_interrupt_get_payload) */
/*lint -esym(621, _okl4_sys_interrupt_limits) */
/*lint -esym(621, _okl4_sys_interrupt_mask) */
/*lint -esym(621, _okl4_sys_interrupt_raise) */
/*lint -esym(621, _okl4_sys_interrupt_set_binary_point) */
/*lint -esym(621, _okl4_sys_interrupt_set_config) */
/*lint -esym(621, _okl4_sys_interrupt_set_control) */
/*lint -esym(621, _okl4_sys_interrupt_set_priority) */
/*lint -esym(621, _okl4_sys_interrupt_set_priority_mask) */
/*lint -esym(621, _okl4_sys_interrupt_set_targets) */
/*lint -esym(621, _okl4_sys_interrupt_unmask) */
/*lint -esym(621, _okl4_sys_kdb_interact) */
/*lint -esym(621, _okl4_sys_kdb_set_object_name) */
/*lint -esym(621, _okl4_sys_ksp_procedure_call) */
/*lint -esym(621, _okl4_sys_mmu_attach_segment) */
/*lint -esym(621, _okl4_sys_mmu_detach_segment) */
/*lint -esym(621, _okl4_sys_mmu_flush_range) */
/*lint -esym(621, _okl4_sys_mmu_flush_range_pn) */
/*lint -esym(621, _okl4_sys_mmu_lookup_page) */
/*lint -esym(621, _okl4_sys_mmu_lookup_pn) */
/*lint -esym(621, _okl4_sys_mmu_map_page) */
/*lint -esym(621, _okl4_sys_mmu_map_pn) */
/*lint -esym(621, _okl4_sys_mmu_unmap_page) */
/*lint -esym(621, _okl4_sys_mmu_unmap_pn) */
/*lint -esym(621, _okl4_sys_mmu_update_page_attrs) */
/*lint -esym(621, _okl4_sys_mmu_update_page_perms) */
/*lint -esym(621, _okl4_sys_mmu_update_pn_attrs) */
/*lint -esym(621, _okl4_sys_mmu_update_pn_perms) */
/*lint -esym(621, _okl4_sys_performance_null_syscall) */
/*lint -esym(621, _okl4_sys_pipe_control) */
/*lint -esym(621, _okl4_sys_pipe_recv) */
/*lint -esym(621, _okl4_sys_pipe_send) */
/*lint -esym(621, _okl4_sys_priority_waive) */
/*lint -esym(621, _okl4_sys_remote_get_register) */
/*lint -esym(621, _okl4_sys_remote_get_registers) */
/*lint -esym(621, _okl4_sys_remote_read_memory32) */
/*lint -esym(621, _okl4_sys_remote_set_register) */
/*lint -esym(621, _okl4_sys_remote_set_registers) */
/*lint -esym(621, _okl4_sys_remote_write_memory32) */
/*lint -esym(621, _okl4_sys_schedule_metrics_status_suspended) */
/*lint -esym(621, _okl4_sys_schedule_metrics_watch_suspended) */
/*lint -esym(621, _okl4_sys_schedule_profile_cpu_disable) */
/*lint -esym(621, _okl4_sys_schedule_profile_cpu_enable) */
/*lint -esym(621, _okl4_sys_schedule_profile_cpu_get_data) */
/*lint -esym(621, _okl4_sys_schedule_profile_vcpu_disable) */
/*lint -esym(621, _okl4_sys_schedule_profile_vcpu_enable) */
/*lint -esym(621, _okl4_sys_schedule_profile_vcpu_get_data) */
/*lint -esym(621, _okl4_sys_scheduler_suspend) */
/*lint -esym(621, _okl4_sys_timer_cancel) */
/*lint -esym(621, _okl4_sys_timer_get_resolution) */
/*lint -esym(621, _okl4_sys_timer_get_time) */
/*lint -esym(621, _okl4_sys_timer_query) */
/*lint -esym(621, _okl4_sys_timer_start) */
/*lint -esym(621, _okl4_sys_tracebuffer_sync) */
/*lint -esym(621, _okl4_sys_vcpu_reset) */
/*lint -esym(621, _okl4_sys_vcpu_start) */
/*lint -esym(621, _okl4_sys_vcpu_stop) */
/*lint -esym(621, _okl4_sys_vcpu_switch_mode) */
/*lint -esym(621, _okl4_sys_vcpu_sync_sev) */
/*lint -esym(621, _okl4_sys_vcpu_sync_wfe) */
/*lint -esym(621, _okl4_sys_vinterrupt_clear_and_raise) */
/*lint -esym(621, _okl4_sys_vinterrupt_modify) */
/*lint -esym(621, _okl4_sys_vinterrupt_raise) */
#endif
#endif /* __AUTO__USER_SYSCALLS_H__ */
/** @} */

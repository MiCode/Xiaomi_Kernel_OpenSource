/*
 *  arch/arm/include/asm/io.h
 *
 *  Copyright (C) 1996-2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Modifications:
 *  16-Sep-1996	RMK	Inlined the inx/outx functions & optimised for both
 *			constant addresses and variable addresses.
 *  04-Dec-1997	RMK	Moved a lot of this stuff to the new architecture
 *			specific IO header files.
 *  27-Mar-1999	PJB	Second parameter of memcpy_toio is const..
 *  04-Apr-1999	PJB	Added check_signature.
 *  12-Dec-1999	RMK	More cleanups
 *  18-Jun-2000 RMK	Removed virt_to_* and friends definitions
 *  05-Oct-2004 BJD     Moved memory string functions to use void __iomem
 */
#ifndef __ASM_ARM_IO_H
#define __ASM_ARM_IO_H

#ifdef __KERNEL__

#include <linux/string.h>
#include <linux/types.h>
#include <asm/byteorder.h>
#include <asm/memory.h>
#include <asm-generic/pci_iomap.h>
#include <linux/msm_rtb.h>
#include <xen/xen.h>

/*
 * ISA I/O bus memory addresses are 1:1 with the physical address.
 */
#define isa_virt_to_bus virt_to_phys
#define isa_page_to_bus page_to_phys
#define isa_bus_to_virt phys_to_virt

/*
 * Atomic MMIO-wide IO modify
 */
extern void atomic_io_modify(void __iomem *reg, u32 mask, u32 set);
extern void atomic_io_modify_relaxed(void __iomem *reg, u32 mask, u32 set);

/*
 * Generic IO read/write.  These perform native-endian accesses.  Note
 * that some architectures will want to re-define __raw_{read,write}w.
 */
void __raw_writesb(volatile void __iomem *addr, const void *data, int bytelen);
void __raw_writesw(volatile void __iomem *addr, const void *data, int wordlen);
void __raw_writesl(volatile void __iomem *addr, const void *data, int longlen);

void __raw_readsb(const volatile void __iomem *addr, void *data, int bytelen);
void __raw_readsw(const volatile void __iomem *addr, void *data, int wordlen);
void __raw_readsl(const volatile void __iomem *addr, void *data, int longlen);

#if __LINUX_ARM_ARCH__ < 6
/*
 * Half-word accesses are problematic with RiscPC due to limitations of
 * the bus. Rather than special-case the machine, just let the compiler
 * generate the access for CPUs prior to ARMv6.
 */
#define __raw_readw_no_log(a)		(__chk_io_ptr(a), \
					*(volatile unsigned short __force *)(a))
#define __raw_writew_no_log(v, a)      ((void)(__chk_io_ptr(a), \
					*(volatile unsigned short __force *)\
					(a) = (v)))
#else
/*
 * When running under a hypervisor, we want to avoid I/O accesses with
 * writeback addressing modes as these incur a significant performance
 * overhead (the address generation must be emulated in software).
 */
static inline void __raw_writew_no_log(u16 val, volatile void __iomem *addr)
{
	asm volatile("strh %1, %0"
		     : : "Q" (*(volatile u16 __force *)addr), "r" (val));
}

static inline u16 __raw_readw_no_log(const volatile void __iomem *addr)
{
	u16 val;
	asm volatile("ldrh %0, %1"
		     : "=r" (val)
		     : "Q" (*(volatile u16 __force *)addr));
	return val;
}
#endif

static inline void __raw_writeb_no_log(u8 val, volatile void __iomem *addr)
{
	asm volatile("strb %1, %0"
		     : : "Qo" (*(volatile u8 __force *)addr), "r" (val));
}

static inline void __raw_writel_no_log(u32 val, volatile void __iomem *addr)
{
	asm volatile("str %1, %0"
		     : : "Qo" (*(volatile u32 __force *)addr), "r" (val));
}

static inline void __raw_writeq_no_log(u64 val, volatile void __iomem *addr)
{
	register u64 v asm ("r2");

	v = val;

	asm volatile("strd %1, %0"
		     : "+Qo" (*(volatile u64 __force *)addr)
		     : "r" (v));
}

static inline u8 __raw_readb_no_log(const volatile void __iomem *addr)
{
	u8 val;
	asm volatile("ldrb %0, %1"
		     : "=r" (val)
		     : "Qo" (*(volatile u8 __force *)addr));
	return val;
}

static inline u32 __raw_readl_no_log(const volatile void __iomem *addr)
{
	u32 val;
	asm volatile("ldr %0, %1"
		     : "=r" (val)
		     : "Qo" (*(volatile u32 __force *)addr));
	return val;
}

static inline u64 __raw_readq_no_log(const volatile void __iomem *addr)
{
	register u64 val asm ("r2");

	asm volatile("ldrd %1, %0"
		     : "+Qo" (*(volatile u64 __force *)addr),
		       "=r" (val));
	return val;
}

/*
 * There may be cases when clients don't want to support or can't support the
 * logging. The appropriate functions can be used but clients should carefully
 * consider why they can't support the logging.
 */

#define __raw_write_logged(v, a, _t)	({ \
	int _ret; \
	volatile void __iomem *_a = (a); \
	void *_addr = (void __force *)(_a); \
	_ret = uncached_logk(LOGK_WRITEL, _addr); \
	ETB_WAYPOINT; \
	__raw_write##_t##_no_log((v), _a); \
	if (_ret) \
		LOG_BARRIER; \
	})


#define __raw_writeb(v, a)	__raw_write_logged((v), (a), b)
#define __raw_writew(v, a)	__raw_write_logged((v), (a), w)
#define __raw_writel(v, a)	__raw_write_logged((v), (a), l)
#define __raw_writeq(v, a)	__raw_write_logged((v), (a), q)

#define __raw_read_logged(a, _l, _t)		({ \
	unsigned _t __a; \
	const volatile void __iomem *_a = (a); \
	void *_addr = (void __force *)(_a); \
	int _ret; \
	_ret = uncached_logk(LOGK_READL, _addr); \
	ETB_WAYPOINT; \
	__a = __raw_read##_l##_no_log(_a);\
	if (_ret) \
		LOG_BARRIER; \
	__a; \
	})


#define __raw_readb(a)		__raw_read_logged((a), b, char)
#define __raw_readw(a)		__raw_read_logged((a), w, short)
#define __raw_readl(a)		__raw_read_logged((a), l, int)
#define __raw_readq(a)		__raw_read_logged((a), q, long long)

/*
 * Architecture ioremap implementation.
 */
#define MT_DEVICE		0
#define MT_DEVICE_NONSHARED	1
#define MT_DEVICE_CACHED	2
#define MT_DEVICE_WC		3
/*
 * types 4 onwards can be found in asm/mach/map.h and are undefined
 * for ioremap
 */

/*
 * __arm_ioremap takes CPU physical address.
 * __arm_ioremap_pfn takes a Page Frame Number and an offset into that page
 * The _caller variety takes a __builtin_return_address(0) value for
 * /proc/vmalloc to use - and should only be used in non-inline functions.
 */
extern void __iomem *__arm_ioremap_caller(phys_addr_t, size_t, unsigned int,
	void *);
extern void __iomem *__arm_ioremap_pfn(unsigned long, unsigned long, size_t, unsigned int);
extern void __iomem *__arm_ioremap_exec(phys_addr_t, size_t, bool cached);
extern void __iounmap(volatile void __iomem *addr);

extern void __iomem * (*arch_ioremap_caller)(phys_addr_t, size_t,
	unsigned int, void *);
extern void (*arch_iounmap)(volatile void __iomem *);

/*
 * Bad read/write accesses...
 */
extern void __readwrite_bug(const char *fn);

/*
 * A typesafe __io() helper
 */
static inline void __iomem *__typesafe_io(unsigned long addr)
{
	return (void __iomem *)addr;
}

#define IOMEM(x)	((void __force __iomem *)(x))

/* IO barriers */
#ifdef CONFIG_ARM_DMA_MEM_BUFFERABLE
#include <asm/barrier.h>
#define __iormb()		rmb()
#define __iowmb()		wmb()
#else
#define __iormb()		do { } while (0)
#define __iowmb()		do { } while (0)
#endif

/* PCI fixed i/o mapping */
#define PCI_IO_VIRT_BASE	0xfee00000
#define PCI_IOBASE		((void __iomem *)PCI_IO_VIRT_BASE)

#if defined(CONFIG_PCI)
void pci_ioremap_set_mem_type(int mem_type);
#else
static inline void pci_ioremap_set_mem_type(int mem_type) {}
#endif

extern int pci_ioremap_io(unsigned int offset, phys_addr_t phys_addr);

/*
 * PCI configuration space mapping function.
 *
 * The PCI specification does not allow configuration write
 * transactions to be posted. Add an arch specific
 * pci_remap_cfgspace() definition that is implemented
 * through strongly ordered memory mappings.
 */
#define pci_remap_cfgspace pci_remap_cfgspace
void __iomem *pci_remap_cfgspace(resource_size_t res_cookie, size_t size);
/*
 * Now, pick up the machine-defined IO definitions
 */
#ifdef CONFIG_NEED_MACH_IO_H
#include <mach/io.h>
#elif defined(CONFIG_PCI)
#define IO_SPACE_LIMIT	((resource_size_t)0xfffff)
#define __io(a)		__typesafe_io(PCI_IO_VIRT_BASE + ((a) & IO_SPACE_LIMIT))
#else
#define __io(a)		__typesafe_io((a) & IO_SPACE_LIMIT)
#endif

/*
 * This is the limit of PC card/PCI/ISA IO space, which is by default
 * 64K if we have PC card, PCI or ISA support.  Otherwise, default to
 * zero to prevent ISA/PCI drivers claiming IO space (and potentially
 * oopsing.)
 *
 * Only set this larger if you really need inb() et.al. to operate over
 * a larger address space.  Note that SOC_COMMON ioremaps each sockets
 * IO space area, and so inb() et.al. must be defined to operate as per
 * readb() et.al. on such platforms.
 */
#ifndef IO_SPACE_LIMIT
#if defined(CONFIG_PCMCIA_SOC_COMMON) || defined(CONFIG_PCMCIA_SOC_COMMON_MODULE)
#define IO_SPACE_LIMIT ((resource_size_t)0xffffffff)
#elif defined(CONFIG_PCI) || defined(CONFIG_ISA) || defined(CONFIG_PCCARD)
#define IO_SPACE_LIMIT ((resource_size_t)0xffff)
#else
#define IO_SPACE_LIMIT ((resource_size_t)0)
#endif
#endif

/*
 *  IO port access primitives
 *  -------------------------
 *
 * The ARM doesn't have special IO access instructions; all IO is memory
 * mapped.  Note that these are defined to perform little endian accesses
 * only.  Their primary purpose is to access PCI and ISA peripherals.
 *
 * Note that for a big endian machine, this implies that the following
 * big endian mode connectivity is in place, as described by numerous
 * ARM documents:
 *
 *    PCI:  D0-D7   D8-D15 D16-D23 D24-D31
 *    ARM: D24-D31 D16-D23  D8-D15  D0-D7
 *
 * The machine specific io.h include defines __io to translate an "IO"
 * address to a memory address.
 *
 * Note that we prevent GCC re-ordering or caching values in expressions
 * by introducing sequence points into the in*() definitions.  Note that
 * __raw_* do not guarantee this behaviour.
 *
 * The {in,out}[bwl] macros are for emulating x86-style PCI/ISA IO space.
 */
#ifdef __io
#define outb(v,p)	({ __iowmb(); __raw_writeb(v,__io(p)); })
#define outw(v,p)	({ __iowmb(); __raw_writew((__force __u16) \
					cpu_to_le16(v),__io(p)); })
#define outl(v,p)	({ __iowmb(); __raw_writel((__force __u32) \
					cpu_to_le32(v),__io(p)); })

#define inb(p)	({ __u8 __v = __raw_readb(__io(p)); __iormb(); __v; })
#define inw(p)	({ __u16 __v = le16_to_cpu((__force __le16) \
			__raw_readw(__io(p))); __iormb(); __v; })
#define inl(p)	({ __u32 __v = le32_to_cpu((__force __le32) \
			__raw_readl(__io(p))); __iormb(); __v; })

#define outsb(p,d,l)		__raw_writesb(__io(p),d,l)
#define outsw(p,d,l)		__raw_writesw(__io(p),d,l)
#define outsl(p,d,l)		__raw_writesl(__io(p),d,l)

#define insb(p,d,l)		__raw_readsb(__io(p),d,l)
#define insw(p,d,l)		__raw_readsw(__io(p),d,l)
#define insl(p,d,l)		__raw_readsl(__io(p),d,l)
#endif

/*
 * String version of IO memory access ops:
 */
extern void _memcpy_fromio(void *, const volatile void __iomem *, size_t);
extern void _memcpy_toio(volatile void __iomem *, const void *, size_t);
extern void _memset_io(volatile void __iomem *, int, size_t);

#define mmiowb()

/*
 *  Memory access primitives
 *  ------------------------
 *
 * These perform PCI memory accesses via an ioremap region.  They don't
 * take an address as such, but a cookie.
 *
 * Again, these are defined to perform little endian accesses.  See the
 * IO port primitives for more information.
 */
#ifndef readl
#define readb_relaxed(c) ({ u8  __r = __raw_readb(c); __r; })
#define readw_relaxed(c) ({ u16 __r = le16_to_cpu((__force __le16) \
					__raw_readw(c)); __r; })
#define readl_relaxed(c) ({ u32 __r = le32_to_cpu((__force __le32) \
					__raw_readl(c)); __r; })
#define readq_relaxed(c) ({ u64 __r = le64_to_cpu((__force __le64) \
					__raw_readq(c)); __r; })
#define readb_relaxed_no_log(c)	({ u8 __r = __raw_readb_no_log(c); __r; })
#define readl_relaxed_no_log(c) ({ u32 __r = le32_to_cpu((__force __le32) \
					__raw_readl_no_log(c)); __r; })
#define readq_relaxed_no_log(c) ({ u64 __r = le64_to_cpu((__force __le64) \
					__raw_readq_no_log(c)); __r; })


#define writeb_relaxed(v, c)	__raw_writeb(v, c)
#define writew_relaxed(v, c)	__raw_writew((__force u16) cpu_to_le16(v), c)
#define writel_relaxed(v, c)	__raw_writel((__force u32) cpu_to_le32(v), c)
#define writeq_relaxed(v, c)	__raw_writeq((__force u64) cpu_to_le64(v), c)
#define writeb_relaxed_no_log(v, c)	((void)__raw_writeb_no_log((v), (c)))
#define writew_relaxed_no_log(v, c) __raw_writew_no_log((__force u16) \
					cpu_to_le16(v), c)
#define writel_relaxed_no_log(v, c) __raw_writel_no_log((__force u32) \
					cpu_to_le32(v), c)
#define writeq_relaxed_no_log(v, c) __raw_writeq_no_log((__force u64) \
					cpu_to_le64(v), c)

#define readb(c)		({ u8  __v = readb_relaxed(c); __iormb(); __v; })
#define readw(c)		({ u16 __v = readw_relaxed(c); __iormb(); __v; })
#define readl(c)		({ u32 __v = readl_relaxed(c); __iormb(); __v; })
#define readq(c)		({ u64 __v = readq_relaxed(c)\
					; __iormb(); __v; })

#define writeb(v,c)		({ __iowmb(); writeb_relaxed(v,c); })
#define writew(v,c)		({ __iowmb(); writew_relaxed(v,c); })
#define writel(v,c)		({ __iowmb(); writel_relaxed(v,c); })
#define writeq(v, c)		({ __iowmb(); writeq_relaxed(v, c); })

#define readsb(p,d,l)		__raw_readsb(p,d,l)
#define readsw(p,d,l)		__raw_readsw(p,d,l)
#define readsl(p,d,l)		__raw_readsl(p,d,l)

#define writesb(p,d,l)		__raw_writesb(p,d,l)
#define writesw(p,d,l)		__raw_writesw(p,d,l)
#define writesl(p,d,l)		__raw_writesl(p,d,l)

#define readb_no_log(c) \
		({ u8  __v = readb_relaxed_no_log(c); __iormb(); __v; })
#define readw_no_log(c) \
		({ u16 __v = readw_relaxed_no_log(c); __iormb(); __v; })
#define readl_no_log(c) \
		({ u32 __v = readl_relaxed_no_log(c); __iormb(); __v; })
#define readq_no_log(c) \
		({ u64 __v = readq_relaxed_no_log(c); __iormb(); __v; })

#define writeb_no_log(v, c) \
		({ __iowmb(); writeb_relaxed_no_log((v), (c)); })
#define writew_no_log(v, c) \
		({ __iowmb(); writew_relaxed_no_log((v), (c)); })
#define writel_no_log(v, c) \
		({ __iowmb(); writel_relaxed_no_log((v), (c)); })
#define writeq_no_log(v, c) \
		({ __iowmb(); writeq_relaxed_no_log((v), (c)); })

#ifndef __ARMBE__
static inline void memset_io(volatile void __iomem *dst, unsigned c,
	size_t count)
{
	extern void mmioset(void *, unsigned int, size_t);
	mmioset((void __force *)dst, c, count);
}
#define memset_io(dst,c,count) memset_io(dst,c,count)

static inline void memcpy_fromio(void *to, const volatile void __iomem *from,
	size_t count)
{
	extern void mmiocpy(void *, const void *, size_t);
	mmiocpy(to, (const void __force *)from, count);
}
#define memcpy_fromio(to,from,count) memcpy_fromio(to,from,count)

static inline void memcpy_toio(volatile void __iomem *to, const void *from,
	size_t count)
{
	extern void mmiocpy(void *, const void *, size_t);
	mmiocpy((void __force *)to, from, count);
}
#define memcpy_toio(to,from,count) memcpy_toio(to,from,count)

#else
#define memset_io(c,v,l)	_memset_io(c,(v),(l))
#define memcpy_fromio(a,c,l)	_memcpy_fromio((a),c,(l))
#define memcpy_toio(c,a,l)	_memcpy_toio(c,(a),(l))
#endif

#endif	/* readl */

/*
 * ioremap() and friends.
 *
 * ioremap() takes a resource address, and size.  Due to the ARM memory
 * types, it is important to use the correct ioremap() function as each
 * mapping has specific properties.
 *
 * Function		Memory type	Cacheability	Cache hint
 * ioremap()		Device		n/a		n/a
 * ioremap_nocache()	Device		n/a		n/a
 * ioremap_cache()	Normal		Writeback	Read allocate
 * ioremap_wc()		Normal		Non-cacheable	n/a
 * ioremap_wt()		Normal		Non-cacheable	n/a
 *
 * All device mappings have the following properties:
 * - no access speculation
 * - no repetition (eg, on return from an exception)
 * - number, order and size of accesses are maintained
 * - unaligned accesses are "unpredictable"
 * - writes may be delayed before they hit the endpoint device
 *
 * ioremap_nocache() is the same as ioremap() as there are too many device
 * drivers using this for device registers, and documentation which tells
 * people to use it for such for this to be any different.  This is not a
 * safe fallback for memory-like mappings, or memory regions where the
 * compiler may generate unaligned accesses - eg, via inlining its own
 * memcpy.
 *
 * All normal memory mappings have the following properties:
 * - reads can be repeated with no side effects
 * - repeated reads return the last value written
 * - reads can fetch additional locations without side effects
 * - writes can be repeated (in certain cases) with no side effects
 * - writes can be merged before accessing the target
 * - unaligned accesses can be supported
 * - ordering is not guaranteed without explicit dependencies or barrier
 *   instructions
 * - writes may be delayed before they hit the endpoint memory
 *
 * The cache hint is only a performance hint: CPUs may alias these hints.
 * Eg, a CPU not implementing read allocate but implementing write allocate
 * will provide a write allocate mapping instead.
 */
void __iomem *ioremap(resource_size_t res_cookie, size_t size);
#define ioremap ioremap
#define ioremap_nocache ioremap

/*
 * Do not use ioremap_cache for mapping memory. Use memremap instead.
 */
void __iomem *ioremap_cache(resource_size_t res_cookie, size_t size);
#define ioremap_cache ioremap_cache

/*
 * Do not use ioremap_cached in new code. Provided for the benefit of
 * the pxa2xx-flash MTD driver only.
 */
void __iomem *ioremap_cached(resource_size_t res_cookie, size_t size);

void __iomem *ioremap_wc(resource_size_t res_cookie, size_t size);
#define ioremap_wc ioremap_wc
#define ioremap_wt ioremap_wc

void iounmap(volatile void __iomem *iomem_cookie);
#define iounmap iounmap
/*
 * io{read,write}{8,16,32,64} macros
 */
#ifndef ioread8
#define ioread8(p)	({ unsigned int __v = __raw_readb(p); __iormb(); __v; })
#define ioread16(p)	({ unsigned int __v = le16_to_cpu((__force __le16)\
				__raw_readw(p)); __iormb(); __v; })
#define ioread32(p)	({ unsigned int __v = le32_to_cpu((__force __le32)\
				__raw_readl(p)); __iormb(); __v; })
#define ioread64(p)	({ unsigned int __v = le64_to_cpu((__force __le64)\
				__raw_readq(p)); __iormb(); __v; })

#define ioread64be(p)	({ unsigned int __v = be64_to_cpu((__force __be64)\
				__raw_readq(p)); __iormb(); __v; })

#define iowrite8(v, p)	({ __iowmb(); __raw_writeb(v, p); })
#define iowrite16(v, p)	({ __iowmb(); __raw_writew((__force __u16)\
				cpu_to_le16(v), p); })
#define iowrite32(v, p)	({ __iowmb(); __raw_writel((__force __u32)\
				cpu_to_le32(v), p); })
#define iowrite64(v, p)	({ __iowmb(); __raw_writeq((__force __u64)\
				cpu_to_le64(v), p); })

#define iowrite64be(v, p) ({ __iowmb(); __raw_writeq((__force __u64)\
				cpu_to_be64(v), p); })

void *arch_memremap_wb(phys_addr_t phys_addr, size_t size);
#define arch_memremap_wb arch_memremap_wb

/*
 * io{read,write}{16,32}be() macros
 */
#define ioread16be(p)		({ __u16 __v = be16_to_cpu((__force __be16)__raw_readw(p)); __iormb(); __v; })
#define ioread32be(p)		({ __u32 __v = be32_to_cpu((__force __be32)__raw_readl(p)); __iormb(); __v; })

#define iowrite16be(v,p)	({ __iowmb(); __raw_writew((__force __u16)cpu_to_be16(v), p); })
#define iowrite32be(v,p)	({ __iowmb(); __raw_writel((__force __u32)cpu_to_be32(v), p); })

#ifndef ioport_map
#define ioport_map ioport_map
extern void __iomem *ioport_map(unsigned long port, unsigned int nr);
#endif
#ifndef ioport_unmap
#define ioport_unmap ioport_unmap
extern void ioport_unmap(void __iomem *addr);
#endif
#endif

struct pci_dev;

#define pci_iounmap pci_iounmap
extern void pci_iounmap(struct pci_dev *dev, void __iomem *addr);

/*
 * Convert a physical pointer to a virtual kernel pointer for /dev/mem
 * access
 */
#define xlate_dev_mem_ptr(p)	__va(p)

/*
 * Convert a virtual cached pointer to an uncached pointer
 */
#define xlate_dev_kmem_ptr(p)	p

#include <asm-generic/io.h>

/*
 * can the hardware map this into one segment or not, given no other
 * constraints.
 */
#define BIOVEC_MERGEABLE(vec1, vec2)	\
	((bvec_to_phys((vec1)) + (vec1)->bv_len) == bvec_to_phys((vec2)))

struct bio_vec;
extern bool xen_biovec_phys_mergeable(const struct bio_vec *vec1,
				      const struct bio_vec *vec2);
#define BIOVEC_PHYS_MERGEABLE(vec1, vec2)				\
	(__BIOVEC_PHYS_MERGEABLE(vec1, vec2) &&				\
	 (!xen_domain() || xen_biovec_phys_mergeable(vec1, vec2)))

#ifdef CONFIG_MMU
#define ARCH_HAS_VALID_PHYS_ADDR_RANGE
extern int valid_phys_addr_range(phys_addr_t addr, size_t size);
extern int valid_mmap_phys_addr_range(unsigned long pfn, size_t size);
extern int devmem_is_allowed(unsigned long pfn);
#endif

/*
 * Register ISA memory and port locations for glibc iopl/inb/outb
 * emulation.
 */
extern void register_isa_ports(unsigned int mmio, unsigned int io,
			       unsigned int io_shift);

#endif	/* __KERNEL__ */
#endif	/* __ASM_ARM_IO_H */

#include <linux/export.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/spinlock.h>

#define IO_CHECK_ALIGN(v, a) ((((unsigned long)(v)) & ((a) - 1)) == 0)

static DEFINE_RAW_SPINLOCK(__io_lock);

/*
 * Generic atomic MMIO modify.
 *
 * Allows thread-safe access to registers shared by unrelated subsystems.
 * The access is protected by a single MMIO-wide lock.
 */
void atomic_io_modify_relaxed(void __iomem *reg, u32 mask, u32 set)
{
	unsigned long flags;
	u32 value;

	raw_spin_lock_irqsave(&__io_lock, flags);
	value = readl_relaxed(reg) & ~mask;
	value |= (set & mask);
	writel_relaxed(value, reg);
	raw_spin_unlock_irqrestore(&__io_lock, flags);
}
EXPORT_SYMBOL(atomic_io_modify_relaxed);

void atomic_io_modify(void __iomem *reg, u32 mask, u32 set)
{
	unsigned long flags;
	u32 value;

	raw_spin_lock_irqsave(&__io_lock, flags);
	value = readl_relaxed(reg) & ~mask;
	value |= (set & mask);
	writel(value, reg);
	raw_spin_unlock_irqrestore(&__io_lock, flags);
}
EXPORT_SYMBOL(atomic_io_modify);

/*
 * Copy data from IO memory space to "real" memory space.
 */
void _memcpy_fromio(void *to, const volatile void __iomem *from, size_t count)
{
	while (count && (!IO_CHECK_ALIGN(from, 8) || !IO_CHECK_ALIGN(to, 8))) {
		*(u8 *)to = readb_relaxed_no_log(from);
		from++;
		to++;
		count--;
	}

	while (count >= 8) {
		*(u64 *)to = readq_relaxed_no_log(from);
		from += 8;
		to += 8;
		count -= 8;
	}

	while (count) {
		*(u8 *)to = readb_relaxed_no_log(from);
		from++;
		to++;
		count--;
	}
}
EXPORT_SYMBOL(_memcpy_fromio);

/*
 * Copy data from "real" memory space to IO memory space.
 */
void _memcpy_toio(volatile void __iomem *to, const void *from, size_t count)
{
	void *p = (void __force *)to;

	while (count && (!IO_CHECK_ALIGN(p, 8) || !IO_CHECK_ALIGN(from, 8))) {
		writeb_relaxed_no_log(*(volatile u8 *)from, p);
		from++;
		p++;
		count--;
	}

	while (count >= 8) {
		writeq_relaxed_no_log(*(volatile u64 *)from, p);
		from += 8;
		p += 8;
		count -= 8;
	}

	while (count) {
		writeb_relaxed_no_log(*(volatile u8 *)from, p);
		from++;
		p++;
		count--;
	}
}
EXPORT_SYMBOL(_memcpy_toio);

/*
 * "memset" on IO memory space.
 */
void _memset_io(volatile void __iomem *dst, int c, size_t count)
{
	void *p = (void __force *)dst;
	u64 qc = c;

	qc |= qc << 8;
	qc |= qc << 16;
	qc |= qc << 32;

	while (count && !IO_CHECK_ALIGN(p, 8)) {
		writeb_relaxed_no_log(c, p);
		p++;
		count--;
	}

	while (count >= 8) {
		writeq_relaxed_no_log(qc, p);
		p += 8;
		count -= 8;
	}

	while (count) {
		writeb_relaxed_no_log(c, p);
		p++;
		count--;
	}
}
EXPORT_SYMBOL(_memset_io);

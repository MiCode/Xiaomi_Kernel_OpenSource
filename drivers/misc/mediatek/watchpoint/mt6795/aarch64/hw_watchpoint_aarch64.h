#ifndef __HW_BREAKPOINT_H
#define __HW_BREAKPOINT_H
#include <mach/sync_write.h>
#include <asm/io.h>
typedef int (*wp_handler)(unsigned long addr);

struct wp_event
{
    unsigned long virt;
    unsigned long phys;
    int type;
    wp_handler handler;
    int in_use;
    int auto_disable;
};

#define WP_EVENT_TYPE_READ 1
#define WP_EVENT_TYPE_WRITE 2
#define WP_EVENT_TYPE_ALL 3

#define init_wp_event(__e, __v, __p, __t, __h)   \
        do {    \
            (__e)->virt = (__v);    \
            (__e)->phys = (__p);    \
            (__e)->type = (__t);    \
            (__e)->handler = (__h);    \
            (__e)->auto_disable = 0;    \
        } while (0)

#define auto_disable_wp(__e)   \
        do {    \
            (__e)->auto_disable = 1;    \
        } while (0)


#define MAX_NR_WATCH_POINT 4  
struct wp_trace_context_t
{
  void __iomem **debug_regs; 
  struct wp_event wp_events[MAX_NR_WATCH_POINT];
  unsigned int wp_nr;
  unsigned int bp_nr;
  unsigned long id_aaa64dfr0_el1;
  unsigned int nr_dbg;
};



struct dbgreg_set {
        unsigned long regs[28];
};
#define SDSR            regs[22]
#define DBGVCR          regs[21]
#define DBGWCR3         regs[20]
#define DBGWCR2         regs[19]
#define DBGWCR1         regs[18]
#define DBGWCR0         regs[17]
#define DBGWVR3         regs[16]
#define DBGWVR2         regs[15]
#define DBGWVR1         regs[14]
#define DBGWVR0         regs[13]
#define DBGBCR5         regs[12]
#define DBGBCR4         regs[11]
#define DBGBCR3         regs[10]
#define DBGBCR2         regs[9]
#define DBGBCR1         regs[8]
#define DBGBCR0         regs[7]
#define DBGBVR5         regs[6]
#define DBGBVR4         regs[5]
#define DBGBVR3		regs[4]
#define DBGBVR2		regs[3]
#define DBGBVR1		regs[2]
#define DBGBVR0		regs[1]
#define MDSCR_EL1	regs[0]

#define DBGWVR 0x800
#define DBGWCR 0x808
#define DBGBVR 0x400
#define DBGBCR 0x408
#define EDSCR  0x088

#define EDLAR 0xFB0
#define EDLSR 0xFB4
#define OSLAR_EL1 0x300

#define UNLOCK_KEY 0xC5ACCE55
#define HDE (1 << 14)
#define MDBGEN (1 << 15)
#define KDE (0x1 <<13)
#define DBGWCR_VAL 0x000001E7
/**/
#define WP_EN (1 << 0)
#define LSC_LDR (1 << 3)
#define LSC_STR (2 << 3)
#define LSC_ALL (3 << 3)

#define WATCHPOINT_TEST_SUIT

#define ARM_DBG_READ(N, M, OP2, VAL) do {\
        asm volatile("mrc p14, 0, %0, " #N "," #M ", " #OP2 : "=r" (VAL));\
} while (0)

#define ARM_DBG_WRITE(N, M, OP2, VAL) do {\
        asm volatile("mcr p14, 0, %0, " #N "," #M ", " #OP2 : : "r" (VAL));\
} while (0)

/*This is bit [29:27] in ESR which indicate watchpoint exception occured */
#define DBG_ESR_EVT_HWWP    0x2

#define dbg_mem_read(addr)          (*(volatile unsigned long *)(addr))

//#define dbg_mem_read_64(addr)          (readq((volatile void __iomem *)addr))
#define dbg_mem_read_64(addr)          cs_cpu_read_64((volatile void __iomem *)addr, 0)

#define dbg_mem_write(addr, val)    mt_reg_sync_writel(val, addr)
#define dbg_reg_copy(offset, base_to, base_from)   \
        dbg_mem_write((base_to) + (offset), dbg_mem_read((base_from) + (offset)))

//#define dbg_mem_write_64(addr, val)    mt_reg_sync_writeq(val, addr)
#define dbg_mem_write_64(addr, val)    cs_cpu_write_64((volatile void __iomem *)addr, 0, val)
#define dbg_reg_copy_64(offset, base_to, base_from)   \
        dbg_mem_write_64((base_to) + (offset), dbg_mem_read_64((base_from) + (offset)))

static inline void cs_cpu_write(void __iomem *addr_base, u32 offset, u32 wdata)
{
        /* TINFO="Write addr %h, with data %h", addr_base+offset, wdata */
        mt_reg_sync_writel(wdata, addr_base + offset);
}

static inline unsigned int cs_cpu_read(const void __iomem *addr_base, u32 offset)
{
        u32 actual;

        /* TINFO="Read addr %h, with data %h", addr_base+offset, actual */
        actual = readl(addr_base + offset);

        return actual;
}

static inline void cs_cpu_write_64(volatile void __iomem *addr_base, u64 offset, u64 wdata)
{
        /* TINFO="Write addr %h, with data %h", addr_base+offset, wdata */
#if 1
	/* since 6795 APB bridge allows 32bit at once, we split 64bit access with 2 steps */
	u32 top = 0;
	u32 bottom = 0;

	bottom = (wdata & 0x00000000ffffffff);
	top = (wdata & 0xffffffff00000000)>>32;
	writel(bottom, (volatile void __iomem *)(addr_base+offset));
	writel(top, (volatile void __iomem *)(addr_base+offset+4));
#else
        mt_reg_sync_writeq(wdata, addr_base + offset);
#endif
}

static inline unsigned long cs_cpu_read_64(volatile void __iomem *addr_base, u64 offset)
{
        u64 actual;

        /* TINFO="Read addr %h, with data %h", addr_base+offset, actual */
#if 1
	/* since 6795 APB bridge allows 32bit at once, we split 64bit access with 2 steps */
	u64 top = 0;
	u64 bottom = 0;

	bottom = readl((volatile void __iomem *)(addr_base+offset));
	top = readl((volatile void __iomem *)(addr_base+offset+4));
	actual = bottom | (top<<32);
#else
        actual = readq(addr_base + offset);
#endif
        return actual;
}


void smp_read_dbgsdsr_callback(void *info);
void smp_write_dbgsdsr_callback(void *info);
void smp_read_MDSCR_EL1_callback(void *info);
void smp_write_MDSCR_EL1_callback(void *info);
void smp_read_OSLSR_EL1_callback(void *info);
int register_wp_context(struct wp_trace_context_t **wp_tracer_context );
extern int add_hw_watchpoint(struct wp_event *wp_event);
extern int del_hw_watchpoint(struct wp_event *wp_event);
void __iomem* get_wp_base(void);


#endif  /* !__HW_BREAKPOINT_H */

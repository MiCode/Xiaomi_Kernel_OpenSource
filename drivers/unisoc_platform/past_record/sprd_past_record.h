/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _SPRD_PAST_DEBUG_H_
#define _SPRD_PAST_DEBUG_H_

#include <linux/types.h>
#include <linux/clk.h>
#include <linux/sched.h>
#include <linux/smp.h>

#define PAST_REG_RECORD_LEN 10000
#define SERROR_PROC_BUF_LEN    6
#define SERROR_START_ADDR 0x80000000UL
#define SERROR_END_ADDR (0x80000000UL + 0x100000000UL)

extern struct sprd_debug_reg_record *sprd_past_reg_record;
extern int serror_debug_status;
extern unsigned long sprd_debug_virt_to_phys(void __iomem *addr);

struct past_reg_info {
	unsigned int index;
	unsigned int state;
	pid_t  pid;
	unsigned int entry_no;
	unsigned int regval;
	unsigned int cpu_id;
	unsigned long time;
	phys_addr_t phys_addr;
	void __iomem *addr;
};


struct sprd_debug_reg_record {
	atomic_t index;
	unsigned int cur_index;
	struct past_reg_info reg_info_array[PAST_REG_RECORD_LEN];

};

unsigned long sprd_debug_virt_to_phys(void __iomem *addr);

#define sprd_write_reg_info(entry_no, val, addr)({\
	unsigned int index;                                                       \
	unsigned int time;                                                       \
	if (sprd_past_reg_record && unlikely(serror_debug_status)	\
			&& (sprd_debug_virt_to_phys((void __iomem *) addr) < SERROR_START_ADDR)) { \
		index = (unsigned int)atomic_inc_return(&sprd_past_reg_record->index);	\
		index %= PAST_REG_RECORD_LEN;	\
		sprd_past_reg_record->cur_index = index;	\
		time = (jiffies - INITIAL_JIFFIES) * (MSEC_PER_SEC / HZ);			\
		sprd_past_reg_record->reg_info_array[index].index = index;            \
		sprd_past_reg_record->reg_info_array[index].pid = current->pid;      \
		sprd_past_reg_record->reg_info_array[index].state = 1;            \
		sprd_past_reg_record->reg_info_array[index].entry_no = entry_no;       \
		sprd_past_reg_record->reg_info_array[index].regval = (unsigned int) val;	\
		sprd_past_reg_record->reg_info_array[index].cpu_id = smp_processor_id();	\
		sprd_past_reg_record->reg_info_array[index].time = time;	\
		sprd_past_reg_record->reg_info_array[index].phys_addr =		\
			sprd_debug_virt_to_phys((void __iomem *) addr);	\
		sprd_past_reg_record->reg_info_array[index].addr = (void __iomem *) addr; }    \
		barrier();	\
})
#define sprd_read_reg_info(entry_no, addr)({\
	unsigned int index;                                                       \
	unsigned long time;                                                       \
	if (sprd_past_reg_record && unlikely(serror_debug_status)	\
			&& (sprd_debug_virt_to_phys((void __iomem *) addr) < SERROR_START_ADDR)) { \
		index = (unsigned int)atomic_inc_return(&sprd_past_reg_record->index);	\
		index %= PAST_REG_RECORD_LEN;	\
		sprd_past_reg_record->cur_index = index;	\
		time = (jiffies - INITIAL_JIFFIES) * (MSEC_PER_SEC / HZ);			\
		sprd_past_reg_record->reg_info_array[index].index = index;            \
		sprd_past_reg_record->reg_info_array[index].pid = current->pid;      \
		sprd_past_reg_record->reg_info_array[index].state = 0;            \
		sprd_past_reg_record->reg_info_array[index].entry_no = entry_no;       \
		sprd_past_reg_record->reg_info_array[index].regval = 0;                \
		sprd_past_reg_record->reg_info_array[index].cpu_id = smp_processor_id();	\
		sprd_past_reg_record->reg_info_array[index].time = time;	\
		sprd_past_reg_record->reg_info_array[index].phys_addr =		\
			sprd_debug_virt_to_phys((void __iomem *) addr);	\
		sprd_past_reg_record->reg_info_array[index].addr = (void __iomem *) addr; }	\
		barrier();	\
})
#define sprd_writeb_reg_info(val, addr)         ({ \
			unsigned int entry_no = 8; \
			sprd_write_reg_info(entry_no, val, addr); \
		})
#define sprd_writew_reg_info(val, addr)         ({ \
			unsigned int entry_no = 16; \
			sprd_write_reg_info(entry_no, val, addr); \
		})
#define sprd_writel_reg_info(val, addr)         ({ \
			unsigned int entry_no = 32; \
			sprd_write_reg_info(entry_no, val, addr); \
		})
#define sprd_writeq_reg_info(val, addr)         ({ \
			unsigned int entry_no = 64; \
			sprd_write_reg_info(entry_no, val, addr); \
		})
#define sprd_readb_reg_info(addr)         ({ \
			unsigned int entry_no = 8; \
			sprd_read_reg_info(entry_no, addr); \
		})
#define sprd_readw_reg_info(addr)         ({ \
			unsigned int entry_no = 16; \
			sprd_read_reg_info(entry_no, addr); \
		})
#define sprd_readl_reg_info(addr)         ({ \
			unsigned int entry_no = 32; \
			sprd_read_reg_info(entry_no, addr); \
		})
#define sprd_readq_reg_info(addr)         ({ \
			unsigned int entry_no = 64; \
			sprd_read_reg_info(entry_no, addr); \
		})

#endif

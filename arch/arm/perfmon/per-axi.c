/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
per-axi
DESCRIPTION
Functions related to AXI bus performance counter manipulations.
*/

#include <linux/io.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include "asm/uaccess.h"
#include "per-axi.h"
#include "perf.h"

/*
Definitions for AXI register addresses, macros to set and get register values
*/
#define AXI_BASE_SIZE						0x00004000
#define AXI_REG_BASE					(AXI_BASE + 0x00000000)
#define AXI_REG_BASE_PHYS					0xa8200000

#define __inpdw(port)	ioread32(port)
#define in_dword_masked(addr, mask) (__inpdw(addr) & (mask))
#define __outpdw(port, val) (iowrite32((uint32_t) (val), port))
#define out_dword(addr, val)        __outpdw(addr, val)

#define HWIO_AXI_MONITOR_CYCLE_COUNT_UPPER_REG_ADDR \
						(AXI_REG_BASE + 0x00003434)
#define HWIO_AXI_MONITOR_CYCLE_COUNT_UPPER_REG_RMSK		0xffff
#define HWIO_AXI_MONITOR_CYCLE_COUNT_UPPER_REG_IN	\
	in_dword_masked(HWIO_AXI_MONITOR_CYCLE_COUNT_UPPER_REG_ADDR, \
				HWIO_AXI_MONITOR_CYCLE_COUNT_UPPER_REG_RMSK)

#define HWIO_AXI_MONITOR_CYCLE_COUNT_LOWER_REG_ADDR (AXI_REG_BASE + 0x00003438)
#define HWIO_AXI_MONITOR_CYCLE_COUNT_LOWER_REG_RMSK		0xffff
#define HWIO_AXI_MONITOR_CYCLE_COUNT_LOWER_REG_IN	\
	in_dword_masked(HWIO_AXI_MONITOR_CYCLE_COUNT_LOWER_REG_ADDR, \
				HWIO_AXI_MONITOR_CYCLE_COUNT_LOWER_REG_RMSK)

#define HWIO_AXI_MONITOR_SELECTION_REG0_ADDR	(AXI_REG_BASE + 0x00003428)
#define HWIO_AXI_MONITOR_SELECTION_REG1_ADDR    (AXI_REG_BASE + 0x0000342c)
#define HWIO_AXI_MONITOR_TENURE_SELECTION_REG_ADDR (AXI_REG_BASE + 0x00003430)
#define HWIO_AXI_MONITOR_SELECTION_REG0_ETC_BMSK		0x4000
#define HWIO_AXI_MONITOR_SELECTION_REG0_ECC_BMSK		0x2000
#define HWIO_AXI_MONITOR_SELECTION_REG0_EEC1_BMSK		0x800
#define HWIO_AXI_MONITOR_SELECTION_REG0_EEC0_BMSK		0x200
#define HWIO_AXI_MONITOR_CYCLE_COUNT_UPPER_REG_OUT(v)                       \
	out_dword(HWIO_AXI_MONITOR_CYCLE_COUNT_UPPER_REG_ADDR, v)
#define HWIO_AXI_MONITOR_CYCLE_COUNT_LOWER_REG_OUT(v)                       \
	out_dword(HWIO_AXI_MONITOR_CYCLE_COUNT_LOWER_REG_ADDR, v)
#define HWIO_AXI_MONITOR_SELECTION_REG0_OUT(v)                              \
	out_dword(HWIO_AXI_MONITOR_SELECTION_REG0_ADDR, v)
#define HWIO_AXI_MONITOR_SELECTION_REG1_OUT(v)                              \
	out_dword(HWIO_AXI_MONITOR_SELECTION_REG1_ADDR, v)
#define HWIO_AXI_MONITOR_TENURE_SELECTION_REG_OUT(v)                        \
	out_dword(HWIO_AXI_MONITOR_TENURE_SELECTION_REG_ADDR, v)
#define HWIO_AXI_MONITOR_SELECTION_REG0_RMSK			0xffff
#define HWIO_AXI_MONITOR_SELECTION_REG0_IN                                  \
	in_dword_masked(HWIO_AXI_MONITOR_SELECTION_REG0_ADDR,               \
	HWIO_AXI_MONITOR_SELECTION_REG0_RMSK)

#define HWIO_AXI_CONFIGURATION_REG_ADDR		(AXI_REG_BASE + 0x00000008)
#define HWIO_AXI_CONFIGURATION_REG_OUT(v)                                   \
	out_dword(HWIO_AXI_CONFIGURATION_REG_ADDR, v)
#define HWIO_AXI_CONFIGURATION_REG_PPDM_BMSK			0x0
#define HWIO_AXI_CONFIGURATION_REG_DISABLE			0x2
#define AXI_EVTSEL_ENABLE_MASK					0x6a00
#define AXI_EVTSEL_DISABLE_MASK					0x95ff
#define AXI_EVTSEL_RESET_MASK					0xfe40

#define HWIO_AXI_MONITOR_EVENT_LOWER_REG0_ADDR	(AXI_REG_BASE + 0x00003450)
#define HWIO_AXI_MONITOR_EVENT_LOWER_REG0_RMSK			0xffff
#define HWIO_AXI_MONITOR_EVENT_LOWER_REG0_SHFT			0
#define HWIO_AXI_MONITOR_EVENT_LOWER_REG0_IN                                \
	in_dword_masked(HWIO_AXI_MONITOR_EVENT_LOWER_REG0_ADDR,		\
	HWIO_AXI_MONITOR_EVENT_LOWER_REG0_RMSK)
#define HWIO_AXI_MONITOR_EVENT_UPPER_REG0_ADDR	(AXI_REG_BASE + 0x00003454)
#define HWIO_AXI_MONITOR_EVENT_UPPER_REG0_RMSK			0xffff
#define HWIO_AXI_MONITOR_EVENT_UPPER_REG0_SHFT			0
#define HWIO_AXI_MONITOR_EVENT_UPPER_REG0_IN                                \
	in_dword_masked(HWIO_AXI_MONITOR_EVENT_UPPER_REG0_ADDR,		\
	HWIO_AXI_MONITOR_EVENT_UPPER_REG0_RMSK)

#define HWIO_AXI_MONITOR_EVENT_LOWER_REG1_ADDR	(AXI_REG_BASE + 0x00003458)
#define HWIO_AXI_MONITOR_EVENT_LOWER_REG1_RMSK			0xffff
#define HWIO_AXI_MONITOR_EVENT_LOWER_REG1_SHFT			0
#define HWIO_AXI_MONITOR_EVENT_LOWER_REG1_IN                                \
	in_dword_masked(HWIO_AXI_MONITOR_EVENT_LOWER_REG1_ADDR,		\
	HWIO_AXI_MONITOR_EVENT_LOWER_REG1_RMSK)
#define HWIO_AXI_MONITOR_EVENT_UPPER_REG1_ADDR	(AXI_REG_BASE + 0x0000345c)
#define HWIO_AXI_MONITOR_EVENT_UPPER_REG1_RMSK			0xffff
#define HWIO_AXI_MONITOR_EVENT_UPPER_REG1_SHFT			0
#define HWIO_AXI_MONITOR_EVENT_UPPER_REG1_IN                                \
	in_dword_masked(HWIO_AXI_MONITOR_EVENT_UPPER_REG1_ADDR,		\
	HWIO_AXI_MONITOR_EVENT_UPPER_REG1_RMSK)

#define HWIO_AXI_MONITOR_TENURE_LOWER_REG_ADDR	(AXI_REG_BASE + 0x00003448)
#define HWIO_AXI_MONITOR_TENURE_LOWER_REG_RMSK			0xffff
#define HWIO_AXI_MONITOR_TENURE_LOWER_REG_SHFT			0
#define HWIO_AXI_MONITOR_TENURE_LOWER_REG_IN                                \
	in_dword_masked(HWIO_AXI_MONITOR_TENURE_LOWER_REG_ADDR,		\
	HWIO_AXI_MONITOR_TENURE_LOWER_REG_RMSK)
#define HWIO_AXI_MONITOR_TENURE_UPPER_REG_ADDR	(AXI_REG_BASE + 0x00003444)
#define HWIO_AXI_MONITOR_TENURE_UPPER_REG_RMSK			0xffff
#define HWIO_AXI_MONITOR_TENURE_UPPER_REG_SHFT			0
#define HWIO_AXI_MONITOR_TENURE_UPPER_REG_IN                                \
	in_dword_masked(HWIO_AXI_MONITOR_TENURE_UPPER_REG_ADDR,		\
	HWIO_AXI_MONITOR_TENURE_UPPER_REG_RMSK)

#define HWIO_AXI_MONITOR_MIN_REG_ADDR		(AXI_REG_BASE + 0x0000343c)
#define HWIO_AXI_MONITOR_MIN_REG_RMSK				0xffff
#define HWIO_AXI_MONITOR_MIN_REG_SHFT				0
#define HWIO_AXI_MONITOR_MIN_REG_IN                                         \
	in_dword_masked(HWIO_AXI_MONITOR_MIN_REG_ADDR,			\
	HWIO_AXI_MONITOR_MIN_REG_RMSK)
#define HWIO_AXI_MONITOR_MAX_REG_ADDR		(AXI_REG_BASE + 0x00003440)
#define HWIO_AXI_MONITOR_MAX_REG_RMSK				0xffff
#define HWIO_AXI_MONITOR_MAX_REG_SHFT				0
#define HWIO_AXI_MONITOR_MAX_REG_IN                                         \
	in_dword_masked(HWIO_AXI_MONITOR_MAX_REG_ADDR,			\
	HWIO_AXI_MONITOR_MAX_REG_RMSK)
#define HWIO_AXI_MONITOR_LAST_TENURE_REG_ADDR	(AXI_REG_BASE + 0x0000344c)
#define HWIO_AXI_MONITOR_LAST_TENURE_REG_RMSK			0xffff
#define HWIO_AXI_MONITOR_LAST_TENURE_REG_SHFT			0
#define HWIO_AXI_MONITOR_LAST_TENURE_REG_IN                                 \
	in_dword_masked(HWIO_AXI_MONITOR_LAST_TENURE_REG_ADDR,		\
	HWIO_AXI_MONITOR_LAST_TENURE_REG_RMSK)
#define HWIO_AXI_MONITOR_TENURE_UPPER_REG_OUT(v)                            \
	out_dword(HWIO_AXI_MONITOR_TENURE_UPPER_REG_ADDR, v)
#define HWIO_AXI_MONITOR_TENURE_LOWER_REG_OUT(v)                            \
	out_dword(HWIO_AXI_MONITOR_TENURE_LOWER_REG_ADDR, v)

#define HWIO_AXI_RESET_ALL		0x9400
#define HWIO_AXI_ENABLE_ALL_NOCYCLES	0x4a00
#define HWIO_AXI_DISABLE_ALL		0xb500
uint32_t AXI_BASE;

unsigned int is_first = 1;
struct perf_mon_axi_data pm_axi_info;
struct perf_mon_axi_cnts axi_cnts;

/*
FUNCTION get_axi_sel_reg0

DESCRIPTION
	Retrieve the value of AXI_SEL_REG0

DEPENDENCIES

RETURN VALUE
	AXI_SEL_REG0
SIDE EFFECTS
*/
unsigned long get_axi_sel_reg0(void)
{
	return pm_axi_info.sel_reg0;
}

/*
FUNCTION get_axi_sel_reg1

DESCRIPTION
	Retrieve the value of AXI_SEL_REG1

DEPENDENCIES

RETURN VALUE
	AXI_SEL_REG1
SIDE EFFECTS
*/
unsigned long get_axi_sel_reg1(void)
{
	return pm_axi_info.sel_reg1;
}

/*
FUNCTION get_axi_ten_sel_reg

DESCRIPTION
	Retrieve the value of AXI_TEN_REG

DEPENDENCIES

RETURN VALUE
	AXI_TEN_REG
SIDE EFFECTS
*/
unsigned long get_axi_ten_sel_reg(void)
{
	return pm_axi_info.ten_sel_reg;
}

/*
FUNCTION get_axi_valid

DESCRIPTION
	Retrieve the value of AXI valid bit

DEPENDENCIES

RETURN VALUE
	AXI Valid bit
SIDE EFFECTS
*/
unsigned long get_axi_valid(void)
{
	return pm_axi_info.valid;
}

/*
FUNCTION get_axi_enable

DESCRIPTION
	Retrieve the value of AXI enable bit

DEPENDENCIES

RETURN VALUE
	AXI enable bit
SIDE EFFECTS
*/
unsigned long get_axi_enable(void)
{
	return pm_axi_info.enable;
}

/*
FUNCTION get_axi_clear

DESCRIPTION
	Retrieve the value of AXI clear bit

DEPENDENCIES

RETURN VALUE
	AXI clear bit
SIDE EFFECTS
*/
unsigned long get_axi_clear(void)
{
	return pm_axi_info.clear;
}

/*
FUNCTION pm_axi_cnts_write

DESCRIPTION
	Write handler for the /proc axi results directory.

DEPENDENCIES

RETURN VALUE
	Number of characters to output.

SIDE EFFECTS
*/
int pm_axi_cnts_write(struct file *file, const char *buff,
    unsigned long cnt, void *data)
{
	char *newbuf;
	struct PerfMonAxiCnts *p =
	(struct PerfMonAxiCnts *)data;

	if (p == 0)
		return cnt;
	/*
	* Alloc the user data in kernel space. and then copy user to kernel
	*/
	newbuf = kmalloc(cnt + 1, GFP_KERNEL);
	if (0 == newbuf)
		return cnt;
	if (copy_from_user(newbuf, buff, cnt) != 0) {
		printk(KERN_INFO "%s copy_from_user failed\n", __func__);
		return cnt;
	}
	return cnt;
}

/*
FUNCTION pm_axi_update_cnts

DESCRIPTION
	Read the current AXI counter values. Check for overflows and
	adjust the values stored accordingly.

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
void pm_axi_update_cnts(void)
{
	if (is_first) {
		pm_axi_start();
	} else {
		if (pm_axi_info.valid == 1) {
			pm_axi_info.valid = 0;
			pm_axi_update();
		} else {
			pm_axi_enable();
		}
	}
	is_first = 0;
	axi_cnts.cycles += pm_get_axi_cycle_count();
	axi_cnts.cnt0 += pm_get_axi_evt0_count();
	axi_cnts.cnt1 += pm_get_axi_evt1_count();
	axi_cnts.tenure_total += pm_get_axi_ten_total_count();

	axi_cnts.tenure_min = pm_get_axi_ten_min_count();
	axi_cnts.tenure_max = pm_get_axi_ten_max_count();
	axi_cnts.tenure_last = pm_get_axi_ten_last_count();

	pm_axi_start();
}

/*
FUNCTION pm_axi_clear_cnts

DESCRIPTION
	Clear the locally stored AXI counter values.
	Also clear the AXI counter registers.

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
void pm_axi_clear_cnts(void)
{
	axi_cnts.cycles = 0;
	axi_cnts.cnt0 = 0;
	axi_cnts.cnt1 = 0;
	axi_cnts.tenure_total = 0;
	axi_cnts.tenure_min = 0;
	axi_cnts.tenure_max = 0;
	axi_cnts.tenure_last = 0;
	pm_axi_start();
}

/*
FUNCTION pm_axi_read_decimal

DESCRIPTION
	Read handler for the /proc axi results directory in decimal format.

DEPENDENCIES

RETURN VALUE
	Number of characters to output.

SIDE EFFECTS
*/
int pm_axi_read_decimal(char *page, char **start, off_t off, int count,
   int *eof, void *data)
{
	struct perf_mon_axi_cnts *p = (struct perf_mon_axi_cnts *)data;

	return sprintf(page, "cnt0:%llu cnt1:%llu tenure:%llu ten_max:%llu \
				ten_min:%llu ten_last:%llu cycles:%llu\n",
				p->cnt0,
				p->cnt1,
				p->tenure_total,
				p->tenure_max,
				p->tenure_min,
				p->tenure_last,
				p->cycles);
}

/*
FUNCTION pm_axi_read_hex

DESCRIPTION
	Read handler for the /proc axi results directory in hex format.

DEPENDENCIES

RETURN VALUE
	Number of characters to output.

SIDE EFFECTS
*/
int pm_axi_read_hex(char *page, char **start, off_t off, int count,
   int *eof, void *data)
{
	struct perf_mon_axi_cnts *p = (struct perf_mon_axi_cnts *)data;

	return sprintf(page, "cnt0:%llx cnt1:%llx tenure:%llx ten_max:%llx \
				ten_min:%llx ten_last:%llx cycles:%llx\n",
				p->cnt0,
				p->cnt1,
				p->tenure_total,
				p->tenure_max,
				p->tenure_min,
				p->tenure_last,
				p->cycles);

}

/*
FUNCTION pm_axi_set_proc_entry

DESCRIPTION
	Create a generic entry for the /proc axi settings directory.

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
void pm_axi_set_proc_entry(char *name, unsigned long *var,
    struct proc_dir_entry *d, int hex)
{
	struct proc_dir_entry *pe;
	pe = create_proc_entry(name, 0777, d);
	if (0 == pe)
		return;
	if (hex) {
		pe->read_proc = per_process_read;
		pe->write_proc = per_process_write_hex;
	} else {
		pe->read_proc = per_process_read_decimal;
		pe->write_proc = per_process_write_dec;
	}
	pe->data = (void *)var;
}

/*
FUNCTION pm_axi_get_cnt_proc_entry

DESCRIPTION
	Create a generic entry for the /proc axi results directory.

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
void pm_axi_get_cnt_proc_entry(char *name, struct perf_mon_axi_cnts *var,
    struct proc_dir_entry *d, int hex)
{
	struct proc_dir_entry *pe;
	pe = create_proc_entry(name, 0777, d);
	if (0 == pe)
		return;
	if (hex) {
		pe->read_proc = pm_axi_read_hex;
		pe->write_proc = pm_axi_cnts_write;
	} else {
		pe->read_proc = pm_axi_read_decimal;
		pe->write_proc = pm_axi_cnts_write;
	}
	pe->data = (void *)var;
}

/*
FUNCTION pm_axi_clear_tenure

DESCRIPTION
	Clear AXI tenure cntr manually. Temporary solution till hardware bug
	is fixed

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
void pm_axi_clear_tenure(void)
{
	HWIO_AXI_MONITOR_TENURE_UPPER_REG_OUT(0x0);
	HWIO_AXI_MONITOR_TENURE_LOWER_REG_OUT(0x0);
}

/*
FUNCTION pm_axi_init

DESCRIPTION
	Map AXI region to virtual memory.

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
void pm_axi_init()
{
	/*Map the AXI regs*/
	#ifdef CONFIG_ARCH_QSD8X50
	{
		/*Map the AXI regs*/
		AXI_BASE = (uint32_t)ioremap(AXI_REG_BASE_PHYS, AXI_BASE_SIZE);
		if (!AXI_BASE)
			printk(KERN_ERR "Mem map failed\n");
	}
	#else
	{
		AXI_BASE = (uint32_t)kmalloc(AXI_BASE_SIZE, GFP_KERNEL);
	}
	#endif

}

/*
FUNCTION pm_axi_start

DESCRIPTION
	Set event0, event1 and tenure registers based on the /proc entries.
	Set cycle cntr to fffffffe to start counters.

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
void
pm_axi_start()
{
	unsigned long sel_reg0, sel_reg1, ten_sel_reg;
	sel_reg0 = get_axi_sel_reg0();
	sel_reg1 = get_axi_sel_reg1();
	ten_sel_reg = get_axi_ten_sel_reg();
	HWIO_AXI_CONFIGURATION_REG_OUT(HWIO_AXI_CONFIGURATION_REG_PPDM_BMSK);
	/*Set AXI Cycle Counter to enable AXI Monitors*/
	HWIO_AXI_MONITOR_CYCLE_COUNT_UPPER_REG_OUT(0xffff);
	HWIO_AXI_MONITOR_CYCLE_COUNT_LOWER_REG_OUT(0xfffe);
	/*Set master/slave*/
	HWIO_AXI_MONITOR_SELECTION_REG1_OUT(sel_reg1);
	HWIO_AXI_MONITOR_SELECTION_REG0_OUT(HWIO_AXI_RESET_ALL);
	HWIO_AXI_MONITOR_SELECTION_REG0_OUT(HWIO_AXI_ENABLE_ALL_NOCYCLES);
	HWIO_AXI_MONITOR_SELECTION_REG0_OUT(HWIO_AXI_MONITOR_SELECTION_REG0_IN
		| sel_reg0);
	HWIO_AXI_MONITOR_SELECTION_REG0_OUT(HWIO_AXI_MONITOR_SELECTION_REG0_IN
		| HWIO_AXI_MONITOR_SELECTION_REG0_ECC_BMSK);
	HWIO_AXI_CONFIGURATION_REG_OUT(HWIO_AXI_CONFIGURATION_REG_PPDM_BMSK);
}

/*
FUNCTION pm_axi_update

DESCRIPTION
	Set event0, event1 and tenure registers based on the /proc entries.

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
void
pm_axi_update()
{
	HWIO_AXI_CONFIGURATION_REG_OUT(HWIO_AXI_CONFIGURATION_REG_PPDM_BMSK);
	HWIO_AXI_MONITOR_SELECTION_REG0_OUT(HWIO_AXI_MONITOR_SELECTION_REG0_IN
		| HWIO_AXI_RESET_ALL);
	HWIO_AXI_MONITOR_SELECTION_REG0_OUT(HWIO_AXI_MONITOR_SELECTION_REG0_IN
	& HWIO_AXI_DISABLE_ALL);
	pm_axi_start();
}

/*
FUNCTION pm_axi_disable

DESCRIPTION
	Disable all cntrs.

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
void
pm_axi_disable(void)
{
	unsigned long sel_reg0;
	/*Disable cntrs*/
	sel_reg0 = get_axi_sel_reg0();
	HWIO_AXI_MONITOR_SELECTION_REG0_OUT(sel_reg0 & AXI_EVTSEL_DISABLE_MASK);
	/*Disable clk*/
	HWIO_AXI_CONFIGURATION_REG_OUT(HWIO_AXI_CONFIGURATION_REG_DISABLE);
}

/*
FUNCTION pm_axi_enable

DESCRIPTION
	Enable all cntrs.

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
void
pm_axi_enable(void)
{
	unsigned long sel_reg0;
	/*Enable cntrs*/
	sel_reg0 = get_axi_sel_reg0();
	HWIO_AXI_MONITOR_SELECTION_REG0_OUT(sel_reg0 | 0x6a00);
	/*Enable clk*/
	HWIO_AXI_CONFIGURATION_REG_OUT(HWIO_AXI_CONFIGURATION_REG_PPDM_BMSK);
}

/*
FUNCTION pm_axi_disable_cnts

DESCRIPTION
	Read cycle cntr value

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
unsigned long
pm_get_axi_cycle_count(void)
{
	if (HWIO_AXI_MONITOR_CYCLE_COUNT_UPPER_REG_IN == 0x0 &&
		HWIO_AXI_MONITOR_CYCLE_COUNT_LOWER_REG_IN == 0x0) {
		/*Set AXI Cycle Counter to enable AXI Monitors*/
		HWIO_AXI_MONITOR_CYCLE_COUNT_UPPER_REG_OUT(0xffff);
		HWIO_AXI_MONITOR_CYCLE_COUNT_LOWER_REG_OUT(0xfffe);
	}
	return 0xfffffffe - ((HWIO_AXI_MONITOR_CYCLE_COUNT_UPPER_REG_IN << 16)
	+ HWIO_AXI_MONITOR_CYCLE_COUNT_LOWER_REG_IN);
}

/*
FUNCTION pm_get_axi_evt0_count

DESCRIPTION
	Read Event0 cntr value

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
unsigned long
pm_get_axi_evt0_count(void)
{
	return (HWIO_AXI_MONITOR_EVENT_UPPER_REG0_IN << 16)
		+ HWIO_AXI_MONITOR_EVENT_LOWER_REG0_IN;
}

/*
FUNCTION pm_get_axi_evt1_count

DESCRIPTION
	Read Event1 cntr value

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
unsigned long
pm_get_axi_evt1_count(void)
{
	return (HWIO_AXI_MONITOR_EVENT_UPPER_REG1_IN << 16)
		+ HWIO_AXI_MONITOR_EVENT_LOWER_REG1_IN;
}

/*
FUNCTION pm_get_axi_ten_min_count

DESCRIPTION
	Read min tenure cntr value

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
unsigned long
pm_get_axi_ten_min_count(void)
{
	return HWIO_AXI_MONITOR_MIN_REG_IN;
}

/*
FUNCTION pm_get_axi_ten_max_count

DESCRIPTION
	Read max tenure cntr value

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
unsigned long
pm_get_axi_ten_max_count(void)
{
	return HWIO_AXI_MONITOR_MAX_REG_IN;
}

/*
FUNCTION pm_get_axi_ten_total_count

DESCRIPTION
	Read total tenure cntr value

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
unsigned long
pm_get_axi_ten_total_count(void)
{
	return (HWIO_AXI_MONITOR_TENURE_UPPER_REG_IN << 16)
		+ HWIO_AXI_MONITOR_TENURE_LOWER_REG_IN;
}

/*
FUNCTION pm_get_axi_ten_last_count

DESCRIPTION
	Read last tenure cntr value

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
unsigned long
pm_get_axi_ten_last_count(void)
{
	return HWIO_AXI_MONITOR_LAST_TENURE_REG_IN;
}

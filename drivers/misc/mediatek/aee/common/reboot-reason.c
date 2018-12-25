/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <asm/stacktrace.h>
#include <asm/memory.h>
#include <asm/traps.h>
#include <linux/elf.h>
#ifdef CONFIG_MTK_WATCHDOG
#include <mtk_wd_api.h>
#endif
#ifdef CONFIG_MTK_RAM_CONSOLE
#include <mt-plat/mtk_ram_console.h>
#endif
#include <mt-plat/aee.h>
#include <mrdump.h>
#include "aee-common.h"
#include <mrdump_private.h>

#define RR_PROC_NAME "reboot-reason"

static struct proc_dir_entry *aee_rr_file;

#define WDT_NORMAL_BOOT 0
#define WDT_HW_REBOOT 1
#define WDT_SW_REBOOT 2

enum boot_reason_t {
	BR_POWER_KEY = 0,
	BR_USB,
	BR_RTC,
	BR_WDT,
	BR_WDT_BY_PASS_PWK,
	BR_TOOL_BY_PASS_PWK,
	BR_2SEC_REBOOT,
	BR_UNKNOWN,
	BR_KERNEL_PANIC,
	BR_WDT_SW,
	BR_WDT_HW
};

#define REBOOT_REASON_LEN	16
char boot_reason[][REBOOT_REASON_LEN] = { "keypad", "usb_chg", "rtc", "wdt",
	"reboot", "tool reboot", "smpl", "others", "kpanic", "wdt_sw",
	"wdt_hw" };

int __weak aee_rr_reboot_reason_show(struct seq_file *m, void *v)
{
	seq_puts(m, "mtk_ram_console not enabled.");
	return 0;
}

static int aee_rr_reboot_reason_proc_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, aee_rr_reboot_reason_show, NULL);
}

static const struct file_operations aee_rr_reboot_reason_proc_fops = {
	.open = aee_rr_reboot_reason_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


void aee_rr_proc_init(struct proc_dir_entry *aed_proc_dir)
{
	aee_rr_file = proc_create(RR_PROC_NAME, 0444, aed_proc_dir,
			&aee_rr_reboot_reason_proc_fops);
	if (aee_rr_file == NULL)
		pr_notice("%s: Can't create rr proc entry\n", __func__);
}
EXPORT_SYMBOL(aee_rr_proc_init);

void aee_rr_proc_done(struct proc_dir_entry *aed_proc_dir)
{
	remove_proc_entry(RR_PROC_NAME, aed_proc_dir);
}
EXPORT_SYMBOL(aee_rr_proc_done);

/* define /sys/bootinfo/powerup_reason */
static ssize_t powerup_reason_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	char boot_reason[64];
	char *br_ptr;
	char *br_ptr_e;

	memset(boot_reason, 0x0, 64);
	br_ptr = strstr(saved_command_line, "androidboot.bootreason=");
	if (br_ptr != 0) {
		br_ptr_e = strstr(br_ptr, " ");
		/* get boot reason */
		if (br_ptr_e != 0) {
			strncpy(boot_reason, br_ptr + 23,
					br_ptr_e - br_ptr - 23);
			boot_reason[br_ptr_e - br_ptr - 23] = '\0';
		}
#ifdef CONFIG_MTK_RAM_CONSOLE
		if (aee_rr_last_fiq_step() != 0)
			strncpy(boot_reason, "kpanic", 7);
#endif
		if (!strncmp(boot_reason, "2sec_reboot",
					strlen("2sec_reboot"))) {
			br_ptr = strstr(saved_command_line,
					"has_battery_removed=1");
			if (br_ptr == NULL)
				return snprintf(buf, sizeof(boot_reason),
						"%s_abnormal\n", boot_reason);
		}
		return snprintf(buf, sizeof(boot_reason), "%s\n", boot_reason);
	} else
		return 0;

}

static struct kobj_attribute powerup_reason_attr = __ATTR_RO(powerup_reason);

struct kobject *bootinfo_kobj;
EXPORT_SYMBOL(bootinfo_kobj);

static struct attribute *bootinfo_attrs[] = {
	&powerup_reason_attr.attr,
	NULL
};

static struct attribute_group bootinfo_attr_group = {
	.attrs = bootinfo_attrs,
};

int ksysfs_bootinfo_init(void)
{
	int error;

	bootinfo_kobj = kobject_create_and_add("bootinfo", NULL);
	if (!bootinfo_kobj)
		return -ENOMEM;

	error = sysfs_create_group(bootinfo_kobj, &bootinfo_attr_group);
	if (error)
		kobject_put(bootinfo_kobj);

	return error;
}

void ksysfs_bootinfo_exit(void)
{
	kobject_put(bootinfo_kobj);
}

/* end sysfs bootinfo */

static inline unsigned long get_linear_memory_size(void)
{
	return (unsigned long)high_memory - PAGE_OFFSET;
}

static char nested_panic_buf[1024];
int aee_nested_printf(const char *fmt, ...)
{
	va_list args;
	static int total_len;

	va_start(args, fmt);
	total_len += vsnprintf(nested_panic_buf, sizeof(nested_panic_buf),
			fmt, args);
	va_end(args);

#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_sram_fiq_log(nested_panic_buf);
#endif

	return total_len;
}

static void print_error_msg(int len)
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	static char error_msg[][50] = { "Bottom unaligned",
		"Bottom out of kernel addr",
		"Top out of kernel addr", "Buf len not enough"
	};
	int tmp = (-len) - 1;

	aee_sram_fiq_log(error_msg[tmp]);
#endif
}

/*save stack as binary into buf,
 *return value

    -1: bottom unaligned
    -2: bottom out of kernel addr space
    -3 top out of kernel addr addr
    -4: buff len not enough
    >0: used length of the buf
 */
int aee_dump_stack_top_binary(char *buf, int buf_len, unsigned long bottom,
		unsigned long top)
{
	/*should check stack address in kernel range */
	if (bottom & 3)
		return -1;
	if (!((bottom >= (PAGE_OFFSET + THREAD_SIZE)) &&
	      (bottom <= (PAGE_OFFSET + get_linear_memory_size())))) {
		return -2;
	}

	if (!((top >= (PAGE_OFFSET + THREAD_SIZE)) &&
	      (top <= (PAGE_OFFSET + get_linear_memory_size())))) {
		return -3;
	}

	if (buf_len < top - bottom)
		return -4;

	memcpy((void *)buf, (void *)bottom, top - bottom);

	return top - bottom;
}

/* extern void mt_fiq_printf(const char *fmt, ...); */
void *aee_excp_regs;
static atomic_t nested_panic_time = ATOMIC_INIT(0);

#ifdef __aarch64__
#define FORMAT_LONG "%016lx "
#else
#define FORMAT_LONG "%08lx "
#endif
inline void aee_print_regs(struct pt_regs *regs)
{
	int i;

	aee_nested_printf("[pt_regs]");
	for (i = 0; i < ELF_NGREG; i++)
		aee_nested_printf(FORMAT_LONG, ((unsigned long *)regs)[i]);
	aee_nested_printf("\n");
}

#define AEE_MAX_EXCP_FRAME	32
inline void aee_print_bt(struct pt_regs *regs)
{
	int i;
	int ret;
	unsigned long high, bottom, fp;
	struct stackframe cur_frame;
	struct pt_regs *excp_regs;

	bottom = regs->reg_sp;
	if (!mrdump_virt_addr_valid(bottom)) {
		aee_nested_printf("invalid sp[%lx]\n", regs);
		return;
	}
	high = ALIGN(bottom, THREAD_SIZE);
	cur_frame.fp = regs->reg_fp;
	cur_frame.pc = regs->reg_pc;
	cur_frame.sp = regs->reg_sp;
	aee_nested_printf("\n[<%p>] %pS\n", (void *)cur_frame.pc,
			(void *)cur_frame.pc);
	aee_nested_printf("[<%p>] %pS\n", (void *)regs->reg_lr,
			(void *)regs->reg_lr);
	for (i = 0; i < AEE_MAX_EXCP_FRAME; i++) {
		fp = cur_frame.fp;
		if ((fp < bottom) || (fp >= (high + THREAD_SIZE))) {
			if (fp != 0)
				aee_nested_printf("fp(%lx)", fp);
			break;
		}
#ifdef __aarch64__
		ret = unwind_frame(current, &cur_frame);
#else
		ret = unwind_frame(&cur_frame);
#endif
		if (ret < 0 || !mrdump_virt_addr_valid(cur_frame.pc))
			break;
		if (in_exception_text(cur_frame.pc)) {
#ifdef __aarch64__
			/* work around for unknown reason
			 * do_mem_abort stack abnormal
			 */
			excp_regs = (void *)(cur_frame.fp + 0x10 + 0xa0);
			/* skip do_mem_abort & el1_da */
			ret = unwind_frame(current, &cur_frame);
			if (ret < 0)
				break;
#else
			excp_regs = (void *)(cur_frame.fp + 4);
#endif
			cur_frame.pc = excp_regs->reg_pc;
		}
		aee_nested_printf("[<%p>] %pS\n", (void *)cur_frame.pc,
				(void *)cur_frame.pc);

	}
	aee_nested_printf("\n");
}

inline int aee_nested_save_stack(struct pt_regs *regs)
{
	int len = 0;

	if (!mrdump_virt_addr_valid(regs->reg_sp))
		return -1;
	aee_nested_printf("[%lx %lx]\n", regs->reg_sp, regs->reg_sp + 256);

	len = aee_dump_stack_top_binary(nested_panic_buf,
		sizeof(nested_panic_buf), regs->reg_sp, regs->reg_sp + 256);
	if (len > 0)
#ifdef CONFIG_MTK_RAM_CONSOLE
		aee_sram_fiq_save_bin(nested_panic_buf, len);
#else
		;
#endif
	else
		print_error_msg(len);
	return len;
}

#ifdef CONFIG_MTK_RAM_CONSOLE
int aee_in_nested_panic(void)
{
	return (atomic_read(&nested_panic_time) &&
		((aee_rr_curr_fiq_step() & ~(AEE_FIQ_STEP_KE_NESTED_PANIC - 1))
		 == AEE_FIQ_STEP_KE_NESTED_PANIC));
}
static inline void aee_rec_step_nested_panic(int step)
{
	if (step < 64)
		aee_rr_rec_fiq_step(AEE_FIQ_STEP_KE_NESTED_PANIC + step);
}
#else
int aee_in_nested_panic(void)
{
	return -1;
}
static inline void aee_rec_step_nested_panic(int step)
{
}
__weak int aee_rr_curr_fiq_step(void)
{
	return -1;
}
#endif

#define TS_MAX_LEN 64
static const char *get_timestamp_string(char *buf, int bufsize)
{
	u64 ts;
	unsigned long rem_nsec;

	ts = local_clock();
	rem_nsec = do_div(ts, 1000000000);
	snprintf(buf, bufsize, "[%5lu.%06lu]",
		       (unsigned long)ts, rem_nsec / 1000);
	return buf;
}

asmlinkage void aee_save_excp_regs(struct pt_regs *regs)
{
	if (!user_mode(regs))
		aee_excp_regs = regs;
}

asmlinkage void aee_stop_nested_panic(struct pt_regs *regs)
{
	struct thread_info *thread = current_thread_info();
	int len = 0;
	int timeout = 1000000;
	int cpu;
#ifdef CONFIG_MTK_WATCHDOG
	int res = 0;
	struct wd_api *wd_api = NULL;
#endif
	struct pt_regs *excp_regs = NULL;
	int prev_fiq_step = aee_rr_curr_fiq_step();
	/* everytime enter nested_panic flow, add 8 */
	static int step_base = -8;
	char tsbuf[TS_MAX_LEN] = {0};

	step_base = step_base < 48 ? step_base + 8 : 56;

	aee_rec_step_nested_panic(step_base);
	local_irq_disable();
	aee_rec_step_nested_panic(step_base + 1);
	cpu = get_HW_cpuid();
	aee_rec_step_nested_panic(step_base + 2);
	/*nested panic may happens more than once on many/single cpus */
	if (atomic_read(&nested_panic_time) < 3)
		aee_nested_printf("\nCPU%dpanic%d@%d-%s\n", cpu,
				nested_panic_time, prev_fiq_step,
				get_timestamp_string(tsbuf, TS_MAX_LEN));
	atomic_inc(&nested_panic_time);

	switch (atomic_read(&nested_panic_time)) {
	case 2:
		aee_print_regs(regs);
		aee_nested_printf("backtrace:");
		aee_print_bt(regs);
		break;

		/* must guarantee Only one cpu can run here */
		/* first check if thread valid */
	case 1:
		if (mrdump_virt_addr_valid(thread)
			&& mrdump_virt_addr_valid(thread->regs_on_excp)) {
			excp_regs = thread->regs_on_excp;
		} else {
			/* if thread invalid, which means wrong sp or
			 * thread_info corrupted,
			 * check global aee_excp_regs instead
			 */
			aee_nested_printf(
				"invalid thread [%lx], excp_regs [%lx]\n",
				thread, aee_excp_regs);
			excp_regs = aee_excp_regs;
		}
		aee_nested_printf("Nested panic\n");
		if (excp_regs) {
			aee_nested_printf("Previous\n");
			aee_print_regs(excp_regs);
		}
		aee_nested_printf("Current\n");
		aee_print_regs(regs);

		/*should not print stack info.
		 * this may overwhelms ram console used by fiq
		 */
		if (in_fiq_handler() != 0) {
			aee_nested_printf("in fiq handler\n");
		} else {
			/*Dump first panic stack */
			aee_nested_printf("Previous\n");
			if (excp_regs) {
				len = aee_nested_save_stack(excp_regs);
				aee_nested_printf("\nbacktrace:");
				aee_print_bt(excp_regs);
			}

			/*Dump second panic stack */
			aee_nested_printf("Current\n");
			if (mrdump_virt_addr_valid(regs)) {
				len = aee_nested_save_stack(regs);
				aee_nested_printf("\nbacktrace:");
				aee_print_bt(regs);
			}
		}
		aee_rec_step_nested_panic(step_base + 5);
		ipanic_recursive_ke(regs, excp_regs, cpu);

		aee_rec_step_nested_panic(step_base + 6);
		break;
	default:
		break;
	}

	/* we donot want a FIQ after this, so disable hwt */
#ifdef CONFIG_MTK_WATCHDOG
	res = get_wd_api(&wd_api);

	if (res)
		aee_nested_printf("get_wd_api error\n");
	else
		wd_api->wd_aee_confirm_hwreboot();
#else
	aee_nested_printf("mtk watchdog not enable.\n");
#endif
	aee_rec_step_nested_panic(step_base + 7);

	/* waiting for the WDT timeout */
	while (1) {
		/* output to UART directly to avoid printk nested panic */
		/* mt_fiq_printf("%s hang here%d\t", __func__, i++); */
		while (timeout--)
			udelay(1);
		timeout = 1000000;
	}
}

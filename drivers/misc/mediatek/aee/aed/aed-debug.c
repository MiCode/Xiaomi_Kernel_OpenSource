
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/kallsyms.h>
#include <linux/notifier.h>
#include <linux/kprobes.h>
#include <linux/cpumask.h>
#include <linux/slab.h>
#include <linux/kdebug.h>
#include <asm/uaccess.h>
#if defined(CONFIG_ARM_PSCI) || (CONFIG_ARM64)
#include <mach/mt_secure_api.h>
#endif
#include <mach/smp.h>
#include "aed.h"

#ifndef PARTIAL_BUILD

#define BUFSIZE 128
static int test_case;
static int test_cpu;
static struct task_struct *wk_tsk[NR_CPUS];
extern struct atomic_notifier_head panic_notifier_list;

#ifdef __aarch64__
#undef BUG
#define BUG() *((unsigned *)0xaed) = 0xDEAD
#endif

static int force_panic_hang(struct notifier_block *this, unsigned long event, void *ptr)
{
	LOGW("\n ==> force panic flow hang\n");
	while (1);
	LOGW("\n You should not see this\n");
	return 0;
}

static struct notifier_block panic_test = {
	.notifier_call = force_panic_hang,
	.priority = INT_MAX,
};

void notrace wdt_atf_hang(void)
{
	int cpu = get_HW_cpuid();
	LOGE(" CPU %d : wdt_atf_hang\n", cpu);
	local_fiq_disable();
	preempt_disable();
	local_irq_disable();
	while (1);
}

static int kwdt_thread_test(void *arg)
{
	struct sched_param param = {.sched_priority = RTPM_PRIO_WDT };
	int cpu = get_HW_cpuid();

	sched_setscheduler(current, SCHED_FIFO, &param);
	set_current_state(TASK_INTERRUPTIBLE);
	LOGW("\n ==> kwdt_thread_test on CPU %d, test_case = %d\n", cpu, test_case);
	msleep(1000);

	if (test_case == 1) {
		if (cpu == test_cpu) {
			LOGW("\n CPU %d : disable preemption and local IRQ forever", cpu);
			preempt_disable();
			local_irq_disable();
			while (1);
			LOGW("\n Error : You should not see this !\n");
		} else {
			LOGW("\n CPU %d : Do nothing and exit\n ", cpu);
		}
	} else if (test_case == 2) {
		if (cpu == test_cpu) {
			msleep(1000);
			LOGW("\n CPU %d : disable preemption and local IRQ forever", cpu);
			preempt_disable();
			local_irq_disable();
			while (1);
			LOGE("\n Error : You should not see this !\n");
		} else {
			LOGW("\n CPU %d : disable irq\n ", cpu);
			local_irq_disable();
			while (1);
			LOGE("\n Error : You should not see this !\n");
		}
	} else if (test_case == 3) {
		if (cpu == test_cpu) {
			LOGW("\n CPU %d : register panic notifier and force hang \n",
			     cpu);
			atomic_notifier_chain_register(&panic_notifier_list, &panic_test);
			preempt_disable();
			local_irq_disable();
			while (1);
			LOGE("\n Error : You should not see this !\n");
		} else {
			LOGW("\n CPU %d : Do nothing and exit\n ", cpu);
		}
	} else if (test_case == 4) {
		LOGW("\n CPU %d : disable preemption and local IRQ forever\n ", cpu);
		preempt_disable();
		local_irq_disable();
		while (1);
		LOGW("\n Error : You should not see this !\n");
	} else if (test_case == 5) {
		LOGW("\n CPU %d : disable preemption and local IRQ/FIQ forever\n ", cpu);
		local_fiq_disable();
		preempt_disable();
		local_irq_disable();
		while (1);
		LOGW("\n Error : You should not see this !\n");
	} else if (test_case == 6) {
		LOGW("\n CPU %d : disable preemption and local IRQ/FIQ forever\n ", cpu);
		local_fiq_disable();
		preempt_disable();
		local_irq_disable();
		while (1);
		LOGW("\n Error : You should not see this !\n");
	}
	return 0;
}

static ssize_t proc_generate_wdt_write(struct file *file,
				       const char __user *buf, size_t size, loff_t *ppos)
{
	unsigned int i = 0;
	char msg[4];
	unsigned char name[20] = { 0 };

	if ((size < 2) || (size > sizeof(msg))) {
		LOGW("\n size = %zx\n", size);
		return -EINVAL;
	}
	if (copy_from_user(msg, buf, size)) {
		LOGW("copy_from_user error");
		return -EFAULT;
	}
	test_case = (unsigned int)msg[0] - '0';
	test_cpu = (unsigned int)msg[2] - '0';
	LOGW("test_case = %d, test_cpu = %d", test_case, test_cpu);
	if ((msg[1] != ':') || (test_case < 1) || (test_case > 6)
	    || (test_cpu < 0) || (test_cpu > nr_cpu_ids)) {
		LOGW("WDT test - Usage: [test case number(1~6):test cpu(0~%d)]\n", nr_cpu_ids);
		return -EINVAL;
	}

	if (test_case == 1) {
		LOGW("Test 1 : One CPU WDT timeout (smp_send_stop succeed)\n");
	} else if (test_case == 2) {
		LOGW("Test 2 : One CPU WDT timeout, other CPU disable irq (smp_send_stop fail in old design)\n");
	} else if (test_case == 3) {
		LOGW("Test 3 : WDT timeout and loop in panic flow\n");
	} else if (test_case == 4) {
		LOGW("Test 4 : All CPU WDT timeout (other CPU stop in the loop)\n");
	} else if (test_case == 5) {
		LOGW("Test 5 : Disable ALL CPU IRQ/FIQ (FIQ : HW_reboot, ATF : HWT \n");
	} else if (test_case == 6) {
		LOGW("Test 6 : (For ATF) HW_REBOOT : change SMC call back function and while loop \n");
#ifdef CONFIG_ARM64
		mt_secure_call(MTK_SIP_KERNEL_WDT, (u64)&wdt_atf_hang, 0, 0);
#endif
#ifdef CONFIG_ARM_PSCI
		mt_secure_call(MTK_SIP_KERNEL_WDT, (u32)&wdt_atf_hang, 0, 0);
#endif
	} else {
		LOGE("\n Unknown test_case %d\n", test_case);
		return -EINVAL;
	}

	/* create kernel threads and bind on every cpu */
	for (i = 0; i < nr_cpu_ids; i++) {
		sprintf(name, "wd-test-%d", i);
		LOGW("[WDK]thread name: %s\n", name);
		wk_tsk[i] = kthread_create(kwdt_thread_test, NULL, name);
		if (IS_ERR(wk_tsk[i])) {
			int ret = PTR_ERR(wk_tsk[i]);
			wk_tsk[i] = NULL;
			return ret;
		}
		kthread_bind(wk_tsk[i], i);
	}

	for (i = 0; i < nr_cpu_ids; i++) {
		LOGW(" wake_up_process(wk_tsk[%d])\n", i);
		wake_up_process(wk_tsk[i]);
	}

	return size;
}

static ssize_t proc_generate_wdt_read(struct file *file,
				      char __user *buf, size_t size, loff_t *ppos)
{
	char buffer[BUFSIZE];
	return sprintf(buffer, "WDT test - Usage: [test case number:test cpu]\n");
}


/*****************************BEGIN OOPS***************************/
/**********BEGIN ISR trigger HWT**********/
/* kprobe pre_handler: called just before the probed instruction is executed */
static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	LOGI("process_name:[%s], pid = %d.\n", current->comm, current->pid);
	return 0;
}


/* kprobe post_handler: called after the probed instruction is executed */
int flag = 1;
void handler_post(struct kprobe *p, struct pt_regs *regs, unsigned long flags)
{
	if (flag) {
		flag = 0;
		mdelay(30 * 1000);
	}
}

static int handler_fault(struct kprobe *p, struct pt_regs *regs, int trapnr);

static struct kprobe kp_kpd_irq_handler = {
	.symbol_name = "kpd_irq_handler",
	.pre_handler = handler_pre,
	.post_handler = handler_post,
	.fault_handler = handler_fault,
};

/*
 * fault_handler: this is called if an exception is generated for any
 * instruction within the pre- or post-handler, or when Kprobes
 * single-steps the probed instruction.
 */
static int handler_fault(struct kprobe *p, struct pt_regs *regs, int trapnr)
{
	LOGE("fault_handler: p->addr = 0x%p, trap #%dn", p->addr, trapnr);
	unregister_kprobe(&kp_kpd_irq_handler);
	LOGI("kprobe at %p unregistered\n", kp_kpd_irq_handler.addr);

	/* Return 0 because we don't handle the fault. */
	return 0;
}

static int register_kprobe_kpd_irq_handler(void)
{
	int ret = 0;

	/* All set to register with Kprobes */
	ret = register_kprobe(&kp_kpd_irq_handler);
	if (ret < 0) {
		LOGI("register_kprobe failed, returned %d\n", ret);
	} else {
		LOGI("Planted kprobe at %p, press Vol+/- to trigger.\n", kp_kpd_irq_handler.addr);
	}
	return ret;
}

/**********END ISR trigger HWT**********/
/**********BEGIN panic case**********/
static int noinline stack_overflow_routine(int x, int y, int z)
{
	char a[4];
	char *p = a;
	int i;
	for (i = 0; i < (x + y + z) * 2; i++) {
		*(p + i) = i;
	}
	/* stack overflow */
	return a[0] + a[3];
}

static void noinline buffer_over_flow(void)
{
	int n;
	LOGI("test case : buffer overflow\n");
	n = stack_overflow_routine(10, 1, 22);
	LOGI("%s: %d\n", __func__, n);
}

static void noinline access_null_pointer(void)
{
	LOGI("test case : derefence Null pointer\n");
	*((unsigned *)0) = 0xDEAD;
}

static void noinline double_free(void)
{
	char *p = kmalloc(32, GFP_KERNEL);
	int i;
	LOGI("test case : double free\n");
	for (i = 0; i < 32; i++) {
		p[i] = (char)i;
	}
	LOGI("aee_ut_ke: call free\n");
	kfree(p);
	LOGI("aee_ut_ke: call free again\n");
	kfree(p);
}

static void noinline devide_by_0(void)
{
	int ZERO = 0;
	int number;
	LOGI("test case: division by %d\n", ZERO);
	number = 100 / ZERO;
	LOGI("%s: %d\n", __func__, number);
}

/**********END panic case**********/

static ssize_t proc_generate_oops_read(struct file *file,
				       char __user *buf, size_t size, loff_t *ppos)
{
	int len;
	char buffer[BUFSIZE];
	len = snprintf(buffer, BUFSIZE, "Oops Generated!\n");
	if (copy_to_user(buf, buffer, len)) {
		LOGE("%s fail to output info.\n", __func__);
	}

	BUG();
	return len;
}

static ssize_t proc_generate_oops_write(struct file *file,
				    const char __user *buf, size_t size, loff_t *ppos)
{
	char msg[6];
	int test_case, test_subcase, test_cpu;

	if ((size < 2) || (size > sizeof(msg))) {
		LOGW("%s: count = %zx\n", __func__, size);
		return -EINVAL;
	}
	if (copy_from_user(msg, buf, size)) {
		LOGW("%s: error\n", __func__);
		return -EFAULT;
	}
	test_case = (unsigned int)msg[0] - '0';
	test_subcase = (unsigned int)msg[2] - '0';
	test_cpu = (unsigned int)msg[4] - '0';
	LOGW("test_case = %d-%d, test_cpu = %d\n", test_case, test_subcase, test_cpu);
	switch (test_case) {
	case 1:
		switch (test_subcase) {
		case 1:
			buffer_over_flow();
			break;
		case 2:
			access_null_pointer();
			break;
		case 3:
			double_free();
			break;
		case 4:
			devide_by_0();
			break;
		default:
			break;
		}
		break;
	case 2:
		register_kprobe_kpd_irq_handler();
		break;
	case 3:
		panic("aee test");
		break;
	default:
		break;
	}
	return size;
}

static int nested_panic(struct notifier_block *this, unsigned long event, void *ptr)
{
	LOGE("\n => force nested panic\n");
	BUG();
	return 0;
}

static struct notifier_block panic_blk = {
	.notifier_call = nested_panic,
	.priority = INT_MAX - 100,
};

static ssize_t proc_generate_nested_ke_read(struct file *file,
					    char __user *buf, size_t size, loff_t *ppos)
{
	int len = 0;
	atomic_notifier_chain_register(&panic_notifier_list, &panic_blk);
	LOGE("\n => panic_notifier_list registered\n");
	BUG();
	/* len = sprintf(page, "Nested panic generated\n"); */

	return len;
}

static ssize_t proc_generate_nested_ke_write(struct file *file,
					     const char __user *buf, size_t size, loff_t *ppos)
{
	char msg[6];
	int test_case, test_subcase, test_cpu;

	if ((size < 2) || (size > sizeof(msg))) {
		LOGW("%s: count = %zx\n", __func__, size);
		return -EINVAL;
	}
	if (copy_from_user(msg, buf, size)) {
		LOGW("%s: error\n", __func__);
		return -EFAULT;
	}
	test_case = (unsigned int)msg[0] - '0';
	test_subcase = (unsigned int)msg[2] - '0';
	test_cpu = (unsigned int)msg[4] - '0';
	LOGW("test_case = %d-%d, test_cpu = %d\n", test_case, test_subcase, test_cpu);
	switch (test_case) {
	case 1:
		register_die_notifier(&panic_blk);
		break;
	}
	BUG();
	return 0;
}


static ssize_t proc_generate_ee_read(struct file *file,
				     char __user *buf, size_t size, loff_t *ppos)
{
#define TEST_EE_LOG_SIZE	2048
#define TEST_EE_PHY_SIZE	65536

	char buffer[BUFSIZE];
	char *ptr, *log;
	int i;

	if ((*ppos)++)
		return 0;
	ptr = kmalloc(TEST_EE_PHY_SIZE, GFP_KERNEL);
	log = kmalloc(TEST_EE_LOG_SIZE, GFP_KERNEL);
	if (ptr == NULL) {
		LOGE("proc_generate_ee_read kmalloc fail\n");
		return sprintf(buffer, "kmalloc fail\n");
	}
	for (i = 0; i < TEST_EE_PHY_SIZE; i++) {
		ptr[i] = (i % 26) + 'A';
	}
	for (i = 0; i < TEST_EE_LOG_SIZE; i++) {
		log[i] = i % 255;
	}
	aed_md_exception_api((int *)log, TEST_EE_LOG_SIZE, (int *)ptr, TEST_EE_PHY_SIZE, __FILE__, DB_OPT_FTRACE);
	kfree(ptr);
	kfree(log);

	return sprintf(buffer, "Modem EE Generated\n");
}

static ssize_t proc_generate_ee_write(struct file *file,
				      const char __user *buf, size_t size, loff_t *ppos)
{
	return 0;
}

static ssize_t proc_generate_combo_read(struct file *file,
					char __user *buf, size_t size, loff_t *ppos)
{
#define TEST_COMBO_PHY_SIZE	65536
	char buffer[BUFSIZE];
	int i;
	char *ptr;
	if ((*ppos)++)
		return 0;
	ptr = kmalloc(TEST_COMBO_PHY_SIZE, GFP_KERNEL);
	if (ptr == NULL) {
		LOGE("proc_generate_combo_read kmalloc fail\n");
		return sprintf(buffer, "kmalloc fail\n");
	}
	for (i = 0; i < TEST_COMBO_PHY_SIZE; i++) {
		ptr[i] = (i % 26) + 'A';
	}

	aee_kernel_dal_show("Oops, MT662X is generating core dump, please wait up to 5 min\n");
	aed_combo_exception(NULL, 0, (int *)ptr, TEST_COMBO_PHY_SIZE, __FILE__);
	kfree(ptr);

	return sprintf(buffer, "Combo EE Generated\n");
}

static ssize_t proc_generate_combo_write(struct file *file,
					 const char __user *buf, size_t size, loff_t *ppos)
{
	return 0;
}

static ssize_t proc_generate_md32_read(struct file *file,
				       char __user *buf, size_t size, loff_t *ppos)
{
#define TEST_MD32_PHY_SIZE	65536
	char buffer[BUFSIZE];
	int i;
	char *ptr;
	if ((*ppos)++)
		return 0;
	ptr = kmalloc(TEST_MD32_PHY_SIZE, GFP_KERNEL);
	if (ptr == NULL) {
		LOGE("proc_generate_md32_read kmalloc fail\n");
		return sprintf(buffer, "kmalloc fail\n");
	}
	for (i = 0; i < TEST_MD32_PHY_SIZE; i++) {
		ptr[i] = (i % 26) + 'a';
	}

	sprintf(buffer, "MD32 EE log here\n");
	aed_md32_exception((int *)buffer, (int)sizeof(buffer), (int *)ptr, TEST_MD32_PHY_SIZE, __FILE__);
	kfree(ptr);

	return sprintf(buffer, "MD32 EE Generated\n");
}

static ssize_t proc_generate_md32_write(struct file *file,
					const char __user *buf, size_t size, loff_t *ppos)
{
	return 0;
}

static ssize_t proc_generate_kernel_notify_read(struct file *file,
						char __user *buf, size_t size, loff_t *ppos)
{
	char buffer[BUFSIZE];
	int len =
	    snprintf(buffer, BUFSIZE,
		     "Usage: write message with format \"R|W|E:Tag:You Message\" into this file to generate kernel warning\n");
	if (*ppos)
		return 0;
	if (copy_to_user(buf, buffer, len)) {
		LOGE("%s fail to output info.\n", __func__);
		return -EFAULT;
	}
	*ppos += len;
	return len;
}


static ssize_t proc_generate_kernel_notify_write(struct file *file,
					     const char __user *buf, size_t size, loff_t *ppos)
{
	char msg[164], *colon_ptr;

	if (size == 0) {
		return -EINVAL;
	}

	if ((size < 5) || (size >= sizeof(msg))) {
		LOGW("aed: %s size sould be >= 5 and <= %zx bytes.\n", __func__, sizeof(msg));
		return -EINVAL;
	}

	if (copy_from_user(msg, buf, size)) {
		LOGW("aed: %s unable to read message\n", __func__);
		return -EFAULT;
	}
	/* Be safe */
	msg[size] = 0;

	if (msg[1] != ':') {
		return -EINVAL;
	}
	colon_ptr = strchr(&msg[2], ':');
	if ((colon_ptr == NULL) || ((colon_ptr - msg) > 32)) {
		LOGW("aed: %s cannot find valid module name\n", __func__);
		return -EINVAL;
	}
	*colon_ptr = 0;

	switch (msg[0]) {
	case 'R':
		aee_kernel_reminding(&msg[2], colon_ptr + 1);
		break;

	case 'W':
		aee_kernel_warning(&msg[2], colon_ptr + 1);
		break;

	case 'E':
		aee_kernel_exception(&msg[2], colon_ptr + 1);
		break;

	default:
		return -EINVAL;
	}

	return size;
}

static ssize_t proc_generate_dal_read(struct file *file,
				      char __user *buf, size_t size, loff_t *ppos)
{
	char buffer[BUFSIZE];
	int len;
	if ((*ppos)++)
		return 0;
	aee_kernel_dal_show("Test for DAL\n");
	len = sprintf(buffer, "DAL Generated\n");

	return len;
}

static ssize_t proc_generate_dal_write(struct file *file,
				       const char __user *buf, size_t size, loff_t *ppos)
{
	return 0;
}

AED_FILE_OPS(generate_oops);
AED_FILE_OPS(generate_nested_ke);
AED_FILE_OPS(generate_kernel_notify);
AED_FILE_OPS(generate_wdt);
AED_FILE_OPS(generate_ee);
AED_FILE_OPS(generate_combo);
AED_FILE_OPS(generate_md32);
AED_FILE_OPS(generate_dal);

int aed_proc_debug_init(struct proc_dir_entry *aed_proc_dir)
{
	AED_PROC_ENTRY(generate-oops, generate_oops, S_IRUSR | S_IWUSR);
	AED_PROC_ENTRY(generate-nested-ke, generate_nested_ke, S_IRUSR);
	AED_PROC_ENTRY(generate-kernel-notify, generate_kernel_notify, S_IRUSR | S_IWUSR);
	AED_PROC_ENTRY(generate-wdt, generate_wdt, S_IRUSR | S_IWUSR);
	AED_PROC_ENTRY(generate-ee, generate_ee, S_IRUSR);
	AED_PROC_ENTRY(generate-combo, generate_combo, S_IRUSR);
	AED_PROC_ENTRY(generate-md32, generate_md32, S_IRUSR);
	AED_PROC_ENTRY(generate-dal, generate_dal, S_IRUSR);

	return 0;
}

int aed_proc_debug_done(struct proc_dir_entry *aed_proc_dir)
{
	remove_proc_entry("generate-oops", aed_proc_dir);
	remove_proc_entry("generate-nested-ke", aed_proc_dir);
	remove_proc_entry("generate-kernel-notify", aed_proc_dir);
	remove_proc_entry("generate-ee", aed_proc_dir);
	remove_proc_entry("generate-combo", aed_proc_dir);
	remove_proc_entry("generate-md32", aed_proc_dir);
	remove_proc_entry("generate-wdt", aed_proc_dir);
	remove_proc_entry("generate-dal", aed_proc_dir);
	return 0;
}

#else

int aed_proc_debug_init(struct proc_dir_entry *aed_proc_dir)
{
	return 0;
}

int aed_proc_debug_done(struct proc_dir_entry *aed_proc_dir)
{
	return 0;
}

#endif

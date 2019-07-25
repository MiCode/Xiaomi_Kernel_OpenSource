#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/notifier.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/kallsyms.h>
#include <wt_sys/wt_boot_reason.h>
#include <wt_sys/wt_bootloader_log_save.h>

#define WT_BOOT_REASON_MSG        "[wt_boot_reason]: "
#define XBL_RAMDUMP_DISPLAY_INFO
static void *wt_reset_reason_addr;
static void *tz_reset_reason_addr;

int wt_panic_oops = 0;
static char *init_ptr;
static DEFINE_SPINLOCK(panic_reason_lock);
struct wt_panic_key_log *key_log = NULL;

int save_panic_key_log(const char *fmt, ...)
{
	char buffer[1024] = {0};
	static char *logbuf_ptr;
	va_list args;
	size_t nbytes;
	unsigned long flags;

	spin_lock_irqsave(&panic_reason_lock, flags);
	if (!init_ptr) {
		spin_unlock_irqrestore(&panic_reason_lock, flags);
		return -EPERM;
	}
	if (!logbuf_ptr) {
		logbuf_ptr = init_ptr + sizeof(struct wt_panic_key_log);
	}

	if (!logbuf_ptr) {
		spin_unlock_irqrestore(&panic_reason_lock, flags);
		return -EPERM;
	}

	va_start(args, fmt);
	nbytes = vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	key_log =(struct wt_panic_key_log *)init_ptr;
	if (key_log->magic != WT_PANIC_KEY_LOG_MAGIC) {
		spin_unlock_irqrestore(&panic_reason_lock, flags);
		return -EPERM;
	}
	if ((key_log->panic_key_log_size + strlen(buffer) + sizeof(struct wt_panic_key_log)) > WT_PANIC_KEY_LOG_SIZE) {
		key_log->panic_key_log_size = 0;
		logbuf_ptr = init_ptr + sizeof(struct wt_panic_key_log);
	}
#ifdef XBL_RAMDUMP_DISPLAY_INFO
	if (buffer[strlen(buffer) - 1] == '\n') {
		buffer[strlen(buffer)] = '\r';
	}
#endif
	key_log->panic_key_log_size += strlen(buffer);
	memcpy(logbuf_ptr, buffer, strlen(buffer));
	key_log->crc = key_log->magic & key_log->panic_key_log_size;
	logbuf_ptr += strlen(buffer);
	spin_unlock_irqrestore(&panic_reason_lock, flags);
	return 0;
}

void save_panic_key_log_symbol(const char *fmt, unsigned long address)
{
        char buffer[KSYM_SYMBOL_LEN];

        sprint_symbol(buffer, address);

        save_panic_key_log(fmt, buffer);
}

void uint32_to_4char(uint32_t a)
{
	int i;
	char c[5] = {0};
	for (i = 0; i < 4; ++i) {
		c[i] = (char)a;
		a = a >> 8;
	}
	printk(WT_BOOT_REASON_MSG"%s\n", c);
}

void print_magic(uint32_t a)
{
	if (a < 128) {
		printk(WT_BOOT_REASON_MSG"tz reset magic: %d\n", a);
	} else {
		uint32_to_4char(a);
	}
}

void raw_writel(uint32_t value, void *p_addr)
{
	printk(WT_BOOT_REASON_MSG"raw_writel value\n");
	print_magic(value);
	if (NULL == p_addr)
		return;
	__raw_writel(value, p_addr);
	return;
}

unsigned long raw_readl(void *p_addr)
{
	uint32_t return_value = 0;
	if (p_addr == NULL)
		return return_value;
	return_value = __raw_readl(p_addr);
	printk(WT_BOOT_REASON_MSG"raw_readl value\n");
	print_magic(return_value);
	return return_value;
}

void set_reset_magic(uint32_t magic_number)
{
	raw_writel(magic_number, wt_reset_reason_addr);
	mb();
}

void clear_reset_magic(void)
{
	raw_writel(RESET_MAGIC_INIT, wt_reset_reason_addr);
	mb();
}

static int wt_panic_handler(struct notifier_block *this, unsigned long events, void *ptr)
{
	uint32_t magic_number = 0;
	printk(WT_BOOT_REASON_MSG"wt_panic_handler enters\n");

#ifdef CONFIG_PREEMPT
	preempt_count_add(NMI_MASK);
#endif
	magic_number = raw_readl(wt_reset_reason_addr);
	switch (magic_number) {
		case RESET_MAGIC_WDT_BARK:
		case RESET_MAGIC_THERMAL:
		case RESET_MAGIC_VMREBOOT:
		case RESET_MAGIC_MODEM:
		case RESET_MAGIC_ADSP:
		case RESET_MAGIC_CDSP:
		case RESET_MAGIC_WCNSS:
		case RESET_MAGIC_SPSS:
		case RESET_MAGIC_VENUS:
		case RESET_MAGIC_AXXX_ZAP:
		case RESET_MAGIC_IPA_FWS:
		case RESET_MAGIC_SLPI:
		case RESET_MAGIC_US_PANIC:
		case RESET_MAGIC_SUBSYSTEM:
			break;
		default:
			set_reset_magic(RESET_MAGIC_PANIC);
	}
#ifdef CONFIG_PREEMPT
	preempt_count_sub(NMI_MASK);
#endif
	return NOTIFY_DONE;
}

static struct notifier_block wt_panic_event_nb = {
	.notifier_call = wt_panic_handler,
	.priority = INT_MAX,
};

static void register_wt_panic_notifier(void)
{
	int n;
	n = atomic_notifier_chain_register(&panic_notifier_list, &wt_panic_event_nb);
	printk(WT_BOOT_REASON_MSG"register_wt_panic_notifier is %d\n", n);
}

static __init int reset_boot_reason(char *cmdline)
{
	printk(KERN_ERR"[wt_boot_reason]:%s\n", cmdline);
	return 0;
}
__setup("androidboot.bootreason=", reset_boot_reason);

static int wt_reset_reason_address_get(void)
{
	struct device_node *np;
	np = of_find_compatible_node(NULL, NULL, "qcom,msm-imem-wt_reset_reason_addr");
	if (!np) {
		printk(KERN_ERR WT_BOOT_REASON_MSG"unable to find DT imem wt_reset_reason_addr node\n");
		return -EPERM;
	}
	wt_reset_reason_addr = of_iomap(np, 0);
	if (!wt_reset_reason_addr) {
		printk(KERN_ERR WT_BOOT_REASON_MSG"unable to map imem wt_reset_reason_addr offset\n");
		return -EPERM;
	}
	return 0;
}

static int tz_reset_reason_address_get(void)
{
	struct device_node *np;
	np = of_find_compatible_node(NULL, NULL, "qcom,msm-imem-tz_reset_reason_addr");
	if (!np) {
		printk(KERN_ERR WT_BOOT_REASON_MSG"unable to find DT imem tz_reset_reason_addr node\n");
		return -EPERM;
	}
	tz_reset_reason_addr = of_iomap(np, 0);
	if (!tz_reset_reason_addr) {
		printk(KERN_ERR WT_BOOT_REASON_MSG"unable to map imem tz_reset_reason_addr offset\n");
		return -EPERM;
	}
	return 0;
}

int  wt_boot_reason_init(void)
{
	int ret = 0;
	char *tmp_ptr = NULL;
	unsigned long flags;
	printk(WT_BOOT_REASON_MSG"wt_boot_reason_init!\n");

#ifdef CONFIG_ARM
	tmp_ptr = (void *)ioremap_nocache(logbuf_head->panic_key_log_addr, WT_PANIC_KEY_LOG_SIZE);
#else
	tmp_ptr = (void *)ioremap_wc(logbuf_head->panic_key_log_addr, WT_PANIC_KEY_LOG_SIZE);
#endif
	spin_lock_irqsave(&panic_reason_lock, flags);
	init_ptr = tmp_ptr;
	spin_unlock_irqrestore(&panic_reason_lock, flags);

	ret = wt_reset_reason_address_get();
	if (ret)
		return -EPERM;
	ret = tz_reset_reason_address_get();
	if (ret)
		return -EPERM;
	register_wt_panic_notifier();
	return ret;
}

void wt_boot_reason_exit(void)
{
	printk(WT_BOOT_REASON_MSG"wt_boot_reason_exit!\n");
}


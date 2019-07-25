#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/highmem.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/of_address.h>
#include <linux/workqueue.h>
#include <linux/kallsyms.h>
#include <wt_sys/wt_bootloader_log_save.h>

#define WT_BOOTLOADER_DELAY 20000
struct workqueue_bootloader_log workqueue_get_log;
struct wt_logbuf_info *logbuf_head = NULL;

struct workqueue_bootloader_log {
	char *start_addr;
	unsigned int size;
	struct delayed_work bootloader_log_handle;
};

static uint32_t cal_checksum(void *buf, int len)
{
	uint32_t *_buf = buf;
	int _len = len / sizeof(uint32_t);
	uint32_t checksum = 0x55aa55aa;
	int i;

	for (i = 0; i < _len; i++) {
		checksum += *_buf++;
	}
	return checksum;
}

void wt_bootloader_log_print(struct work_struct *work)
{
	struct workqueue_bootloader_log *bootloader_log = NULL;
	char *buf = NULL;
	unsigned int len = 0;
	unsigned int i = 0;
	char *p = NULL;
	printk("wt_bootloader_log_print start!");
	if (work == NULL) {
		printk(KERN_ERR"wt_bootloader_log_print work == NULL!");
		return;
	}
	bootloader_log = container_of(( struct delayed_work *)work, struct workqueue_bootloader_log, bootloader_log_handle);
	buf = bootloader_log->start_addr;
	len = bootloader_log->size;
	p = buf;
	if (p == NULL) {
		printk(KERN_ERR"wt_bootloader_log_print p == NULL!");
		return;
	}
	if (len >= WT_BOOTLOADER_LOG_HALF_SIZE) {
		printk(KERN_ERR"len > log store size");
		return;
	}
	printk(KERN_ERR"Bootloader log start:%lx, len:%d\n", (long unsigned int)buf, len);
	for (i = 0; i < len; i++) {
		if (buf[i] == '\0')
			buf[i] = ' ';
		if (buf[i] == '\r')
			buf[i] = ' ';
		if (buf[i] == '\n') {
			buf[i] = '\0';
			printk(KERN_ERR"Bootloader log:%s\n",p);
			buf[i] = '\n';
			p = &buf[i+1];
		}
	}
	printk(KERN_ERR"Bootloader log end!\n");
}

int wt_bootloader_log_init(void)
{
	unsigned long wt_log_addr = 0;
	unsigned long wt_log_size = 0;
	void *head_addr = NULL;
	struct device_node *wt_mem_dts_node = NULL;
	const u32 *wt_mem_dts_basep = NULL;

	printk("wt_bootloader_log_init start!\n");
	wt_mem_dts_node = of_find_compatible_node(NULL, NULL, "wt_share_mem");
	if (wt_mem_dts_node == 0) {
		printk(KERN_ERR"of_find_compatible_node error!\n");
		return -EPERM;
	}
	wt_mem_dts_basep = of_get_address(wt_mem_dts_node, 0, (u64 *)&wt_log_size, NULL);
	wt_log_addr = (unsigned long)of_translate_address(wt_mem_dts_node, wt_mem_dts_basep);
	printk(KERN_ERR"wt_log_addr:0x%lx wt_log_size:0x%lx\n", wt_log_addr, wt_log_size);

	if (wt_log_addr == 0 || wt_log_size > WT_BOOTLOADER_LOG_SIZE) {
		printk(KERN_ERR"wt_log_addr error!\n");
		return -EPERM;
	}

	#ifdef CONFIG_ARM
		head_addr = (void *)ioremap_nocache(wt_log_addr, wt_log_size);
	#else
		head_addr = (void *)ioremap_wc(wt_log_addr, wt_log_size);
	#endif

	logbuf_head = (struct wt_logbuf_info *)head_addr;
	#ifdef CONFIG_KALLSYMS
		logbuf_head->kernel_log_addr = virt_to_phys(*(void **)kallsyms_lookup_name("log_buf"));
	#else
		logbuf_head->kernel_log_addr = 0;
	#endif
	#ifdef CONFIG_LOG_BUF_SHIFT
		logbuf_head->kernel_log_size = *(unsigned long *)kallsyms_lookup_name("log_buf_len");
	#else
		logbuf_head->kernel_log_size = 0;
	#endif
	logbuf_head->checksum = cal_checksum(&logbuf_head->kernel_log_addr, sizeof(uint32_t) + sizeof(uint64_t));
	logbuf_head->kernel_started = 1;
	printk(KERN_ERR"logbuf_head->bootloader_log_addr:0x%llx, logbuf_head->kernel_log_addr:0x%llx\n", logbuf_head->bootloader_log_addr, logbuf_head->kernel_log_addr);
	printk(KERN_ERR"logbuf_head->bootloader_log_size:0x%x, logbuf_head->kernel_log_size:0x%x\n", logbuf_head->bootloader_log_size,  logbuf_head->kernel_log_size);
	return 0;
}

int wt_bootloader_log_handle(void)
{
	unsigned long delay_time = 0;
	workqueue_get_log.start_addr = (char *)((unsigned long)logbuf_head + sizeof(struct wt_logbuf_info));
	workqueue_get_log.size = (unsigned int)(logbuf_head->bootloader_log_size);
	delay_time = msecs_to_jiffies(WT_BOOTLOADER_DELAY);
	INIT_DELAYED_WORK(&(workqueue_get_log.bootloader_log_handle), wt_bootloader_log_print);
	schedule_delayed_work(&(workqueue_get_log.bootloader_log_handle), delay_time);
	return 0;
}

void wt_bootloader_log_exit(void)
{
	printk(KERN_ERR"wt_bootloader_log_exit!\n");
	return;
}


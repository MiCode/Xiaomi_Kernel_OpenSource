#include <linux/slab.h>
#include <linux/aee.h>
#include <linux/atomic.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/kthread.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <asm/io.h>
#include <mach/mt_reg_dump.h>
#include <mach/wd_api.h>

#define RAM_CONSOLE_HEADER_STR_LEN 1024

static int mtk_cpu_num;

#ifdef CONFIG_MTK_EMMC_SUPPORT
#ifdef CONFIG_MTK_AEE_IPANIC
#include <linux/mmc/sd_misc.h>

extern int card_dump_func_write(unsigned char *buf, unsigned int len, unsigned long long offset,
				int dev);
extern int card_dump_func_read(unsigned char *buf, unsigned int len, unsigned long long offset,
			       int dev);
#define EMMC_BLOCK_SIZE 512
#endif
#endif
/*
   This group of API call by sub-driver module to report reboot reasons
   aee_rr_* stand for previous reboot reason
 */
struct last_reboot_reason {
	uint32_t fiq_step;
	uint32_t exp_type; /* 0xaeedeadX: X=1 (HWT), X=2 (KE), X=3 (nested panic) */
	uint32_t reboot_mode;

	uint32_t last_irq_enter[NR_CPUS];
	uint64_t jiffies_last_irq_enter[NR_CPUS];

	uint32_t last_irq_exit[NR_CPUS];
	uint64_t jiffies_last_irq_exit[NR_CPUS];

	uint64_t jiffies_last_sched[NR_CPUS];
	char last_sched_comm[NR_CPUS][TASK_COMM_LEN];

	uint8_t hotplug_data1[NR_CPUS];
  	uint8_t hotplug_data2;
	uint64_t hotplug_data3;

	uint32_t mcdi_wfi;
	uint32_t deepidle_data;
	uint32_t sodi_data;
	uint64_t cpu_dormant[NR_CPUS];
	uint32_t clk_data[8];
	uint32_t suspend_debug_flag;
	
	uint8_t cpu_dvfs_vproc_big;
	uint8_t cpu_dvfs_vproc_little;
	uint8_t cpu_dvfs_oppidx;
	uint8_t cpu_dvfs_status;
	
	uint8_t gpu_dvfs_vgpu;
	uint8_t gpu_dvfs_oppidx;
	uint8_t gpu_dvfs_status; 

	uint64_t ptp_cpu_big_volt;
	uint64_t ptp_cpu_little_volt;
	uint64_t ptp_gpu_volt;
	uint64_t ptp_temp;
	uint8_t ptp_status;
	
	void *kparams;
};

struct reboot_reason_pl {
	u32 wdt_status;
	u32 data[0];
};

struct reboot_reason_lk {
	u32 data[0];
};

struct ram_console_buffer {
	uint32_t sig;
	/* for size comptible */
	uint32_t off_pl;
	uint32_t off_lpl; /* last preloader: struct reboot_reason_pl*/
	uint32_t sz_pl;
	uint32_t off_lk;
	uint32_t off_llk; /* last lk: struct reboot_reason_lk */
	uint32_t sz_lk;
	uint32_t padding[4];
	uint32_t off_linux; /* struct last_reboot_reason */
	uint32_t off_console;

	/* console buffer*/
	uint32_t start;
	uint32_t size;
	uint32_t bin_log_count;
};

//#define RAM_CONSOLE_SIG (0x43474244)	/* DBGC */
#define REBOOT_REASON_SIG (0x43474244)	/* DBRR */
static int FIQ_log_size = sizeof(struct ram_console_buffer);


static char *ram_console_old_log_init_buffer;

static struct ram_console_buffer *ram_console_old_header;
static char *ram_console_old_log;
static size_t ram_console_old_log_size;

static struct ram_console_buffer *ram_console_buffer;
static size_t ram_console_buffer_size;

static DEFINE_SPINLOCK(ram_console_lock);

static atomic_t rc_in_fiq = ATOMIC_INIT(0);

#ifdef __aarch64__
static void *_memcpy(void *dest, const void *src, size_t count)
{
	char *tmp = dest;
	const char *s = src;

	while (count--)
		*tmp++ = *s++;
	return dest;
}

#define memcpy _memcpy
#endif

#define LAST_RR_SEC_VAL(header, sect, type, item) \
	((type*)((void*)header + header->off_##sect))->item

#define LAST_RRR_VAL(rr_item)  LAST_RR_SEC_VAL(ram_console_old_header, linux, struct last_reboot_reason, rr_item)

#define LAST_RRPL_VAL(rr_item) LAST_RR_SEC_VAL(ram_console_old_header, pl, struct reboot_reason_pl, rr_item)

#ifdef CONFIG_MTK_EMMC_SUPPORT
#ifdef CONFIG_MTK_AEE_IPANIC
#define EMMC_ADDR 0X700000
static char *ram_console2_log;
extern int boot_finish;

static int ram_console_check_header(struct ram_console_buffer *buffer)
{
	int i;
    size_t buffer_size = ram_console_buffer_size + sizeof(struct ram_console_buffer);
	if (!buffer ||  buffer->off_pl > buffer_size
	    || buffer->off_lk > buffer_size || buffer->off_linux > buffer_size || buffer->off_console > buffer_size || buffer->off_linux > buffer_size) {
		pr_err("ram_console: ilegal header.");
		for (i = 0; i < 16; i++)
			pr_debug("0x%x ", ((int*)buffer)[i]);
		pr_debug("\n");
		return -1;
	} else
		return 0;
}

void last_kmsg_store_to_emmc(void)
{
	int buff_size;
	struct wd_api*wd_api = NULL;
    get_wd_api(&wd_api);
	
//		if(num_online_cpus() > 1){
	if(wd_api->wd_get_check_bit() > 1){
		printk(KERN_ERR"ram_console: online cpu %d!\n",wd_api->wd_get_check_bit());
		if(boot_finish == 0)
		return;
	}
	
	/* save log to emmc */
	buff_size = ram_console_buffer_size + sizeof(struct ram_console_buffer);
	buff_size = buff_size / EMMC_BLOCK_SIZE;
	buff_size *= EMMC_BLOCK_SIZE;
	card_dump_func_write((unsigned char *)ram_console_buffer, buff_size, EMMC_ADDR,
			     DUMP_INTO_BOOT_CARD_IPANIC);

	pr_err("ram_console: save kernel log to emmc!\n");
}

static int ram_console2_show(struct seq_file *m, void *v)
{
	struct ram_console_buffer *bufp = NULL;
	bufp = (struct ram_console_buffer *)ram_console2_log;
	/* seq_printf(m, ram_console2_log); */
	seq_printf(m, "show last_kmsg2 sig %d, size %d, hw_status %u, reboot_mode %u!\n",
		   bufp->sig, bufp->size, LAST_RR_SEC_VAL(bufp, pl, struct reboot_reason_pl, wdt_status), LAST_RR_SEC_VAL(bufp, linux, struct last_reboot_reason, reboot_mode));
	if (bufp->off_console < ram_console_buffer_size && !ram_console_check_header(bufp))
		seq_write(m, (char*)bufp + bufp->off_console, ram_console_buffer_size);
	return 0;
}


static int ram_console2_file_open(struct inode *inode, struct file *file)
{
	return single_open(file, ram_console2_show, inode->i_private);
}

static const struct file_operations ram_console2_file_ops = {
	.owner = THIS_MODULE,
	.open = ram_console2_file_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#ifdef CONFIG_MTK_GPT_SCHEME_SUPPORT
extern int get_emmc_dump_status(void);

#endif
static int emmc_read_last_kmsg(void *data)
{
	int size;

	struct proc_dir_entry *entry;
	struct ram_console_buffer *bufp = NULL;

#ifdef CONFIG_MTK_GPT_SCHEME_SUPPORT
	int emmc_ready_flag = 0;
	int count = 0;

	emmc_ready_flag = get_emmc_dump_status();
	while (emmc_ready_flag != 1) {
		msleep(2000);
		if (emmc_ready_flag == -1) {
			pr_err("emmc have no expd partition!\n");
			return 1;
		}
		pr_err("emmc expd not ready!\n");
		emmc_ready_flag = get_emmc_dump_status();
		count++;
		if (count > 100) {
			pr_err("emmc mount expd partition error!\n");
			return 1;
		}

	}
#endif

	size = ram_console_buffer_size + sizeof(struct ram_console_buffer);
	size = size / EMMC_BLOCK_SIZE;
	size *= EMMC_BLOCK_SIZE;
	if (size == 0) {
		pr_err("ram_console: ram_console_buffer_size = 0\n");
		return 1;
	}
	ram_console2_log = kzalloc(size, GFP_KERNEL);
	if (ram_console2_log == NULL) {
		pr_err("ram_console: malloc size 2 error!\n");
		return 1;
	}

	if (card_dump_func_read(ram_console2_log, size, EMMC_ADDR, DUMP_INTO_BOOT_CARD_IPANIC) != 0) {
		kfree(ram_console2_log);
		ram_console2_log = NULL;
		pr_err("ram_console: read emmc data 2 error!\n");
		return 1;
	}

	bufp = (struct ram_console_buffer *)ram_console2_log;
	if (bufp->sig != REBOOT_REASON_SIG) {
		kfree(ram_console2_log);
		ram_console2_log = NULL;
		pr_err("ram_console: emmc read data sig is not match!\n");
		return 1;
	}

	entry = proc_create("last_kmsg2", 0444, NULL, &ram_console2_file_ops);
	if (!entry) {
		pr_err("ram_console: failed to create proc entry\n");
		kfree(ram_console2_log);
		ram_console2_log = NULL;
		return 1;
	}
	pr_err("ram_console: create last_kmsg2 ok.\n");
	return 0;

}
#else
void last_kmsg_store_to_emmc(void)
{
}
#endif
#endif



void sram_log_save(const char *msg, int count)
{
	struct ram_console_buffer *buffer;
	char *rc_console;
	int rem;

	if (ram_console_buffer == NULL) {
		pr_err("ram console buffer is NULL!\n");
		return;
	}

	buffer = ram_console_buffer;
	rc_console = (char*)ram_console_buffer + ram_console_buffer->off_console;

	/* count >= buffer_size, full the buffer */
	if (count >= ram_console_buffer_size) {
		memcpy(rc_console, msg + (count - ram_console_buffer_size),
		       ram_console_buffer_size);
		buffer->start = 0;
		buffer->size = ram_console_buffer_size;
	} else if (count > (ram_console_buffer_size - buffer->start))	/* count > last buffer, full them and fill the head buffer */
	{
		rem = ram_console_buffer_size - buffer->start;
		memcpy(rc_console + buffer->start, msg, rem);
		memcpy(rc_console, msg + rem, count - rem);
		buffer->start = count - rem;
		buffer->size = ram_console_buffer_size;
	} else			/* count <=  last buffer, fill in free buffer */
	{
		memcpy(rc_console + buffer->start, msg, count);	/* count <= last buffer, fill them */
		buffer->start += count;
		buffer->size += count;
		if (buffer->start >= ram_console_buffer_size) {
			buffer->start = 0;
		}
		if (buffer->size > ram_console_buffer_size) {
			buffer->size = ram_console_buffer_size;
		}
	}

}

void aee_sram_fiq_save_bin(const char *msg, size_t len)
{
	int delay = 100;
	char bin_buffer[4];
	struct ram_console_buffer *buffer = ram_console_buffer;

	if (FIQ_log_size + len > ram_console_buffer_size) {
		return;
	}

	if (len > 0xffff) {
		return;
	}

	if (len % 4 != 0) {
		len -= len % 4;
	}

	atomic_set(&rc_in_fiq, 1);

	while ((delay > 0) && (spin_is_locked(&ram_console_lock))) {
		udelay(1);
		delay--;
	}

	/* bin buffer flag 00ff */
	bin_buffer[0] = 0x00;
	bin_buffer[1] = 0xff;
	/* bin buffer size */
	bin_buffer[2] = len / 255;
	bin_buffer[3] = len % 255;

	sram_log_save(bin_buffer, 4);
	sram_log_save(msg, len);
	FIQ_log_size = FIQ_log_size + len + 4;
	buffer->bin_log_count += len;
}


void aee_disable_ram_console_write(void)
{
	atomic_set(&rc_in_fiq, 1);
	return;
}

void aee_sram_fiq_log(const char *msg)
{
	unsigned int count = strlen(msg);
	int delay = 100;

	if (FIQ_log_size + count > ram_console_buffer_size) {
		return;
	}

	atomic_set(&rc_in_fiq, 1);

	while ((delay > 0) && (spin_is_locked(&ram_console_lock))) {
		udelay(1);
		delay--;
	}

	sram_log_save(msg, count);
	FIQ_log_size += count;
}

void ram_console_write(struct console *console, const char *s, unsigned int count)
{
	unsigned long flags;

	if (atomic_read(&rc_in_fiq))
		return;

	spin_lock_irqsave(&ram_console_lock, flags);

	sram_log_save(s, count);

	spin_unlock_irqrestore(&ram_console_lock, flags);
}

static struct console ram_console = {
	.name = "ram",
	.write = ram_console_write,
	.flags = CON_PRINTBUFFER | CON_ENABLED | CON_ANYTIME,
	.index = -1,
};

void ram_console_enable_console(int enabled)
{
	if (enabled)
		ram_console.flags |= CON_ENABLED;
	else
		ram_console.flags &= ~CON_ENABLED;
}

static inline void bin_to_asc(char *buff, uint8_t num)
{
	if (num > 9) {
		*buff = num - 10 + 'a';
	} else {
		*buff = num + '0';
	}
	pr_err("buff %c, num %d.\n", *buff, num);
}

static void __init ram_console_save_old(struct ram_console_buffer *buffer)
{
	unsigned int header_size;
	unsigned int wdt_status;
	char *rc_console = (char*)buffer + buffer->off_console;
	size_t old_log_size = buffer->size;
	size_t total_size = old_log_size;
	size_t bin_log_size = 0;

	char *tmp;
	int i, n;
	int length;
	int point = 0;

	pr_err("ram_console: lk[%x], linux[%x], console[%x]\n", buffer->off_lk, buffer->off_linux, buffer->off_console);
	if (buffer->off_console)
		header_size = buffer->off_console;
	else
		header_size = buffer->off_linux + ALIGN(sizeof(struct last_reboot_reason), 64);
	pr_err("ram_console: old_header[%p], buffer[%p], header_size[%x]\n", ram_console_old_header, buffer, header_size);
	memcpy(ram_console_old_header, buffer, header_size);
	if (ram_console_old_header->off_pl == 0 || ram_console_old_header->off_pl + ALIGN(ram_console_old_header->sz_pl, 64) != ram_console_old_header->off_lpl) {
		/* workaround for compatiblity to old preloader & lk (OTA) */
		wdt_status = *((unsigned char*)ram_console_old_header + 12);
		ram_console_old_header->off_pl = 64;
		ram_console_old_header->sz_pl = sizeof(struct reboot_reason_pl);
		LAST_RR_SEC_VAL(ram_console_old_header, pl, struct reboot_reason_pl, wdt_status) = wdt_status;
	}
	pr_err("ram_console:bin_log_count[%x]\n", buffer->bin_log_count);
	if (buffer->bin_log_count == 0) {
		ram_console_old_log_init_buffer = kmalloc(total_size, GFP_KERNEL);
		if (ram_console_old_log_init_buffer == NULL) {
			pr_err("ram_console: failed to allocate old buffer\n");
			return;
		}

		ram_console_old_log = ram_console_old_log_init_buffer;
		ram_console_old_log_size = total_size;

		memcpy(ram_console_old_log_init_buffer,
		       rc_console + buffer->start, buffer->size - buffer->start);
		memcpy(ram_console_old_log_init_buffer + buffer->size - buffer->start,
		       rc_console, buffer->start);
	} else {
		bin_log_size = buffer->bin_log_count * 5 / 4;	/* bin: 12 34 56 78-->ascill: 78654321z */

		ram_console_old_log_init_buffer = kmalloc(total_size + bin_log_size, GFP_KERNEL);
		if (ram_console_old_log_init_buffer == NULL) {
			pr_err("ram_console: failed to allocate buffer\n");
			return;
		}

		tmp = kmalloc(total_size, GFP_KERNEL);
		if (tmp == NULL) {
			pr_err("ram_console: failed to allocate tmp buffer\n");
			return;
		}

		ram_console_old_log = ram_console_old_log_init_buffer;
		memcpy(tmp, rc_console + buffer->start, buffer->size - buffer->start);
		memcpy(tmp + buffer->size - buffer->start, rc_console, buffer->start);

		for (i = 0; i < total_size;) {
			if ((tmp[i] == 0x00) && (tmp[i + 1] == 0xff)) {
				length = tmp[i + 2] * 0xff + tmp[i + 3];
				i = i + 4;
				for (n = 0; n < length / 4; n++) {
					bin_to_asc(&ram_console_old_log_init_buffer[point],
						   (uint8_t) (tmp[i + 3] / 16));
					point++;
					bin_to_asc(&ram_console_old_log_init_buffer[point],
						   (uint8_t) (tmp[i + 3] % 16));
					point++;
					bin_to_asc(&ram_console_old_log_init_buffer[point],
						   (uint8_t) (tmp[i + 2] / 16));
					point++;
					bin_to_asc(&ram_console_old_log_init_buffer[point],
						   (uint8_t) (tmp[i + 2] % 16));
					point++;
					bin_to_asc(&ram_console_old_log_init_buffer[point],
						   (uint8_t) (tmp[i + 1] / 16));
					point++;
					bin_to_asc(&ram_console_old_log_init_buffer[point],
						   (uint8_t) (tmp[i + 1] % 16));
					point++;
					bin_to_asc(&ram_console_old_log_init_buffer[point],
						   (uint8_t) (tmp[i] / 16));
					point++;
					bin_to_asc(&ram_console_old_log_init_buffer[point],
						   (uint8_t) (tmp[i] % 16));
					point++;
					ram_console_old_log_init_buffer[point++] = 32;
					i = i + 4;
				}
			} else {
				ram_console_old_log_init_buffer[point++] = tmp[i++];
			}
		}
		ram_console_old_log_size = point;
		kfree(tmp);
	}
}

static int __init ram_console_init(struct ram_console_buffer *buffer, size_t buffer_size)
{
	ram_console_buffer = buffer;

	if (ram_console_old_header == 0) {
		pr_err("ram_console: failed to kmalloc 0x%zx\n", buffer_size);
	}
	if (buffer->sig != REBOOT_REASON_SIG) {
		pr_err("ram_console: sig mismatch(0x%08x)\n", buffer->sig);
		memset((void*)buffer, 0, buffer_size);
		buffer->sig = REBOOT_REASON_SIG;
	}
	if (buffer->off_console != 0 && buffer->off_linux + ALIGN(sizeof(struct last_reboot_reason), 64) == buffer->off_console
	    && buffer->size <= buffer_size - buffer->off_console && buffer->start <= buffer->size) {
		pr_err("ram_console: log size 0x%x, start 0x%x\n", buffer->size, buffer->start);
		ram_console_save_old(buffer);
	} else {
		pr_err("ram_console: no logs, console off 0x%x, size 0x%x, start 0x%x\n", buffer->off_console, buffer->size, buffer->start);
		if (buffer->sz_lk != 0 && buffer->off_lk + ALIGN(buffer->sz_lk, 64) == buffer->off_llk)
			buffer->off_linux = buffer->off_llk + ALIGN(buffer->sz_lk, 64);
		else
			buffer->off_linux = 512; /* OTA:leave enough space for pl/lk */
		buffer->off_console = buffer->off_linux + ALIGN(sizeof(struct last_reboot_reason), 64);
	}
	ram_console_buffer_size = buffer_size - buffer->off_console;
	buffer->size = 0;
       	buffer->start = 0;
	buffer->bin_log_count = 0;
	memset((void*)buffer + buffer->off_linux, 0, buffer_size - buffer->off_linux);

	register_console(&ram_console);

	return 0;
}

#if defined(CONFIG_MTK_RAM_CONSOLE_USING_DRAM)
static void *remap_lowmem(phys_addr_t start, phys_addr_t size)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int page_count;
	pgprot_t prot;
	unsigned int i;
	void *vaddr;

	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);

	prot = pgprot_noncached(PAGE_KERNEL);

	pages = kmalloc(sizeof(struct page *) * page_count, GFP_KERNEL);
	if (!pages) {
		pr_err("%s: Failed to allocate array for %u pages\n", __func__, page_count);
		return NULL;
	}

	for (i = 0; i < page_count; i++) {
		phys_addr_t addr = page_start + i * PAGE_SIZE;
		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}
	vaddr = vmap(pages, page_count, VM_MAP, prot);
	kfree(pages);
	if (!vaddr) {
		pr_err("%s: Failed to map %u pages\n", __func__, page_count);
		return NULL;
	}

	return vaddr + offset_in_page(start);
}
#endif

typedef struct {
    unsigned int start;
    unsigned int size;
} mem_desc_t;

#if defined(CONFIG_MTK_RAM_CONSOLE_USING_SRAM)
#ifdef CONFIG_OF
static int __init dt_get_ram_console(unsigned long node, const char *uname, int depth, void *data)
{
    mem_desc_t *sram;
    if (depth != 1 ||(strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0))
        return 0;
        
    sram = (mem_desc_t *) of_get_flat_dt_prop(node, "non_secure_sram", NULL);
    if(sram){
	    pr_notice("ram_console:[DT] 0x%x@0x%x\n", sram->size, sram->start);
	    *(mem_desc_t*)data = *sram;
    }

    return 1;
}
#endif
#endif

static int __init ram_console_early_init(void)
{
	struct ram_console_buffer *bufp = NULL;
	size_t buffer_size = 0;
	ram_console_old_header = kzalloc(8192, GFP_KERNEL);
#if defined(CONFIG_MTK_RAM_CONSOLE_USING_SRAM)
#ifdef CONFIG_OF
	mem_desc_t sram = {0};
	if (of_scan_flat_dt(dt_get_ram_console, &sram)) {
		if (sram.start == 0) {
			sram.start = CONFIG_MTK_RAM_CONSOLE_ADDR;
			sram.size = CONFIG_MTK_RAM_CONSOLE_SIZE;
		}
		bufp = ioremap(sram.start, sram.size);
		if (bufp)
			buffer_size = sram.size;
		else {
			pr_err("ram_console: ioremap failed, [0x%x, 0x%x]\n", sram.start, sram.size);
			return 0;
		}
	} else {
		return 0;
	}
#else
	bufp = (struct ram_console_buffer *)CONFIG_MTK_RAM_CONSOLE_ADDR;
	buffer_size = CONFIG_MTK_RAM_CONSOLE_SIZE;
#endif
#elif defined(CONFIG_MTK_RAM_CONSOLE_USING_DRAM)
	bufp = remap_lowmem(CONFIG_MTK_RAM_CONSOLE_DRAM_ADDR, CONFIG_MTK_RAM_CONSOLE_DRAM_SIZE);
	if (bufp == NULL) {
		pr_err("ram_console: ioremap failed\n");
		return 0;
	}
	buffer_size = CONFIG_MTK_RAM_CONSOLE_DRAM_SIZE;
#else
	return 0;
#endif

	pr_err("ram_console: buffer start: 0x%p, size: 0x%zx\n", bufp, buffer_size);
	mtk_cpu_num = num_present_cpus();
	return ram_console_init(bufp, buffer_size);
}

static int ram_console_show(struct seq_file *m, void *v)
{
	seq_write(m, ram_console_old_log, ram_console_old_log_size);
	return 0;
}

static int ram_console_file_open(struct inode *inode, struct file *file)
{
	return single_open(file, ram_console_show, inode->i_private);
}

static const struct file_operations ram_console_file_ops = {
	.owner = THIS_MODULE,
	.open = ram_console_file_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init ram_console_late_init(void)
{
	struct proc_dir_entry *entry;
	char *ram_console_header_buffer;
	int str_real_len = 0;

#ifdef CONFIG_MTK_EMMC_SUPPORT
#ifdef CONFIG_MTK_AEE_IPANIC
#ifdef CONFIG_MTK_GPT_SCHEME_SUPPORT
	int err;
	static struct task_struct *thread;
	thread = kthread_run(emmc_read_last_kmsg, 0, "read_poweroff_log");
	if (IS_ERR(thread)) {
		err = PTR_ERR(thread);
		pr_err("ram_console: failed to create kernel thread: %d\n", err);
	}
#else
	emmc_read_last_kmsg(NULL);
#endif
#endif
#endif
	if (ram_console_old_header == NULL || ram_console_old_log == NULL) {
		pr_err("ram_console: old log is null!\n");
		return 0;
	}

	ram_console_header_buffer = kmalloc(RAM_CONSOLE_HEADER_STR_LEN, GFP_KERNEL);
	if (ram_console_header_buffer == NULL) {
		pr_err("ram_console: failed to allocate buffer for header buffer.\n");
		return 0;
	}


	str_real_len =
	    sprintf(ram_console_header_buffer, "ram console header, hw_status: %u, fiq step %u.\n",
		    LAST_RRPL_VAL(wdt_status), LAST_RRR_VAL(fiq_step));

	str_real_len +=
	    sprintf(ram_console_header_buffer + str_real_len, "bin log %d.\n",
		    ram_console_old_header->bin_log_count);

	ram_console_old_log = kmalloc(ram_console_old_log_size + str_real_len, GFP_KERNEL);
	if (ram_console_old_log == NULL) {
		pr_err("ram_console: failed to allocate buffer for old log\n");
		ram_console_old_log_size = 0;
		kfree(ram_console_header_buffer);
		return 0;
	}
	memcpy(ram_console_old_log, ram_console_header_buffer, str_real_len);
	memcpy(ram_console_old_log + str_real_len,
	       ram_console_old_log_init_buffer, ram_console_old_log_size);

	kfree(ram_console_header_buffer);
	kfree(ram_console_old_log_init_buffer);
	entry = proc_create("last_kmsg", 0444, NULL, &ram_console_file_ops);
	if (!entry) {
		pr_err("ram_console: failed to create proc entry\n");
		kfree(ram_console_old_log);
		ram_console_old_log = NULL;
		return 0;
	}

	ram_console_old_log_size += str_real_len;
	return 0;
}
console_initcall(ram_console_early_init);
late_initcall(ram_console_late_init);

/* aee sram flags save */
#define RR_BASE(stage) ((void*)ram_console_buffer + ram_console_buffer->off_##stage)
#define RR_LINUX ((struct last_reboot_reason*)RR_BASE(linux))

#define LAST_RR_SET(rr_item, value)				\
	if (ram_console_buffer) {				\
		RR_LINUX->rr_item = value;	\
	}

#define LAST_RR_SET_WITH_ID(rr_item, id, value)			\
	if (ram_console_buffer) {				\
		RR_LINUX->rr_item[id] = value;	\
	}

#define LAST_RR_VAL(rr_item)				\
	(ram_console_buffer ? RR_LINUX->rr_item : 0)

#define LAST_RR_GET(rr_item)				\
	return LAST_RR_VAL(rr_item)

#define LAST_RR_MEMCPY(rr_item, str, len)				\
	if (ram_console_buffer) {					\
		strlcpy(RR_LINUX->rr_item, str, len);	\
	}

#define LAST_RR_MEMCPY_WITH_ID(rr_item, id, str, len)			\
	if (ram_console_buffer) {					\
		strlcpy(RR_LINUX->rr_item[id], str, len);	\
	}

void aee_rr_rec_reboot_mode(u8 mode)
{
	LAST_RR_SET(reboot_mode, mode);
}

void aee_rr_rec_kdump_params(void *params)
{
	LAST_RR_SET(kparams, params);
}

void aee_rr_rec_fiq_step(u8 step)
{
	LAST_RR_SET(fiq_step, step);
}

int aee_rr_curr_fiq_step(void)
{
	LAST_RR_GET(fiq_step);
}

void aee_rr_rec_exp_type(unsigned int type)
{
	if (LAST_RR_VAL(exp_type) == 0 && type < 16)
		LAST_RR_SET(exp_type, 0xaeedead0 | type);
}

unsigned int aee_rr_curr_exp_type(void)
{
	unsigned int exp_type = LAST_RR_VAL(exp_type);
	return (exp_type ^ 0xaeedead0) < 16 ? exp_type ^ 0xaeedead0 : exp_type;
}

/* composite api */
void aee_rr_rec_last_irq_enter(int cpu, int irq, u64 jiffies)
{
	if (cpu >= 0 && cpu < NR_CPUS) {
		LAST_RR_SET_WITH_ID(last_irq_enter, cpu, irq);
		LAST_RR_SET_WITH_ID(jiffies_last_irq_enter, cpu, jiffies);
	}
	mb();
}

void aee_rr_rec_last_irq_exit(int cpu, int irq, u64 jiffies)
{
	if (cpu >=0 && cpu < NR_CPUS) {
		LAST_RR_SET_WITH_ID(last_irq_exit, cpu, irq);
		LAST_RR_SET_WITH_ID(jiffies_last_irq_exit, cpu, jiffies);
	}
	mb();
}

void aee_rr_rec_last_sched_jiffies(int cpu, u64 jiffies, const char *comm)
{
	if (cpu >=0 && cpu < NR_CPUS) {
		LAST_RR_SET_WITH_ID(jiffies_last_sched, cpu, jiffies);
		LAST_RR_MEMCPY_WITH_ID(last_sched_comm, cpu, comm, TASK_COMM_LEN);
	}
	mb();
}

void aee_rr_rec_hoplug(int cpu, u8 data1, u8 data2)
{
	if (cpu >=0 && cpu < NR_CPUS) {
		LAST_RR_SET_WITH_ID(hotplug_data1, cpu, data1);
		if (cpu == 0)
			LAST_RR_SET(hotplug_data2, data2);
	}
}

void aee_rr_rec_hotplug(int cpu, u8 data1, u8 data2, unsigned long data3)
{
	if (cpu >=0 && cpu < NR_CPUS) {
		LAST_RR_SET_WITH_ID(hotplug_data1, cpu, data1);
		if (cpu == 0) {
			LAST_RR_SET(hotplug_data2, data2);
			LAST_RR_SET(hotplug_data3, (uint64_t)data3);
		}
	}
}

void aee_rr_rec_clk(int id, u32 val)
{
	LAST_RR_SET_WITH_ID(clk_data, id, val);
}

void aee_rr_rec_deepidle_val(u32 val)
{
	LAST_RR_SET(deepidle_data, val);
}

u32 aee_rr_curr_deepidle_val(void)
{
	LAST_RR_GET(deepidle_data);
}

void aee_rr_rec_sodi_val(u32 val)
{
	LAST_RR_SET(sodi_data, val);
}

u32 aee_rr_curr_sodi_val(void)
{
	LAST_RR_GET(sodi_data);
}

/* special case without MMU, return addr directly, strongly suggest not to use */
unsigned int *aee_rr_rec_mcdi_wfi(void)
{
	if (ram_console_buffer)
		return &RR_LINUX->mcdi_wfi;
	else
		return NULL;
}

unsigned long *aee_rr_rec_cpu_dormant(void)
{
	if (ram_console_buffer)
		return (unsigned long*)&RR_LINUX->cpu_dormant;
	else
		return NULL;
}

void aee_rr_rec_cpu_dvfs_vproc_big(u8 val)
{
	LAST_RR_SET(cpu_dvfs_vproc_big, val);
}

void aee_rr_rec_cpu_dvfs_vproc_little(u8 val)
{
	LAST_RR_SET(cpu_dvfs_vproc_little, val);
}

void aee_rr_rec_cpu_dvfs_oppidx(u8 val)
{
	LAST_RR_SET(cpu_dvfs_oppidx, val);
}

u8 aee_rr_curr_cpu_dvfs_oppidx(void)
{
	LAST_RR_GET(cpu_dvfs_oppidx);
}

void aee_rr_rec_cpu_dvfs_status(u8 val)
{
	LAST_RR_SET(cpu_dvfs_status, val);
}

u8 aee_rr_curr_cpu_dvfs_status(void)
{
	LAST_RR_GET(cpu_dvfs_status);
}

void aee_rr_rec_gpu_dvfs_vgpu(u8 val)
{
	LAST_RR_SET(gpu_dvfs_vgpu, val);
}

void aee_rr_rec_gpu_dvfs_oppidx(u8 val)
{
	LAST_RR_SET(gpu_dvfs_oppidx, val);
}

void aee_rr_rec_gpu_dvfs_status(u8 val)
{
	LAST_RR_SET(gpu_dvfs_status, val);
}

u8 aee_rr_curr_gpu_dvfs_status(void)
{
	LAST_RR_GET(gpu_dvfs_status);
}

void aee_rr_rec_ptp_cpu_big_volt(u64 val)
{
	LAST_RR_SET(ptp_cpu_big_volt, val);
}

void aee_rr_rec_ptp_cpu_little_volt(u64 val)
{
	LAST_RR_SET(ptp_cpu_little_volt, val);
}

void aee_rr_rec_ptp_gpu_volt(u64 val)
{
	LAST_RR_SET(ptp_gpu_volt, val);
}

void aee_rr_rec_ptp_temp(u64 val)
{
	LAST_RR_SET(ptp_temp, val);
}

void aee_rr_rec_ptp_status(u8 val)
{
	LAST_RR_SET(ptp_status, val);
}
	
u64 aee_rr_curr_ptp_cpu_big_volt(void)
{
	LAST_RR_GET(ptp_cpu_big_volt);
}

u64 aee_rr_curr_ptp_cpu_little_volt(void)
{
	LAST_RR_GET(ptp_cpu_little_volt);
}

u64 aee_rr_curr_ptp_gpu_volt(void)
{
	LAST_RR_GET(ptp_gpu_volt);
}

u64 aee_rr_curr_ptp_temp(void)
{
	LAST_RR_GET(ptp_temp);
}

u8 aee_rr_curr_ptp_status(void)
{
	LAST_RR_GET(ptp_status);
}

void aee_rr_rec_suspend_debug_flag(u32 val)
{
	LAST_RR_SET(suspend_debug_flag, val);
}

/* aee sram flags print */
int aee_rr_last_fiq_step(void)
{
	return LAST_RRR_VAL(fiq_step);
}

typedef void (*last_rr_show_t)(struct seq_file *m);
typedef void (*last_rr_show_cpu_t)(struct seq_file *m, int cpu);

void aee_rr_show_wdt_status(struct seq_file *m)
{
	seq_printf(m, "WDT status: %d", LAST_RRPL_VAL(wdt_status));
}

void aee_rr_show_fiq_step(struct seq_file *m)
{
	seq_printf(m, " fiq step: %u ", LAST_RRR_VAL(fiq_step));
}

void aee_rr_show_exp_type(struct seq_file *m)
{
	unsigned int exp_type = LAST_RRR_VAL(exp_type);
	seq_printf(m, " exception type: %u\n", (exp_type ^ 0xaeedead0) < 16 ? exp_type ^ 0xaeedead0 : exp_type);
}

void aee_rr_show_last_irq_enter(struct seq_file *m, int cpu)
{
	seq_printf(m, "  irq: enter(%d, ", LAST_RRR_VAL(last_irq_enter)[cpu]);
}

void aee_rr_show_jiffies_last_irq_enter(struct seq_file *m, int cpu)
{
	seq_printf(m, "%llu) ", LAST_RRR_VAL(jiffies_last_irq_enter)[cpu]);
}

void aee_rr_show_last_irq_exit(struct seq_file *m, int cpu)
{
	seq_printf(m, "quit(%d, ", LAST_RRR_VAL(last_irq_exit)[cpu]);
}

void aee_rr_show_jiffies_last_irq_exit(struct seq_file *m, int cpu)
{
	seq_printf(m, "%llu)\n", LAST_RRR_VAL(jiffies_last_irq_exit)[cpu]);
}

void aee_rr_show_hotplug_data1(struct seq_file *m, int cpu)
{
	seq_printf(m, "  hotplug: %d, ", LAST_RRR_VAL(hotplug_data1)[cpu]);
}

void aee_rr_show_hotplug_data2(struct seq_file *m, int cpu)
{
	if (cpu == 0)
		seq_printf(m, "%d, ", LAST_RRR_VAL(hotplug_data2));
}
void aee_rr_show_hotplug_data3(struct seq_file *m, int cpu)
{
	if (cpu == 0)
		seq_printf(m, "0x%llx", LAST_RRR_VAL(hotplug_data3));
	seq_printf(m, "\n");
}

void aee_rr_show_mcdi(struct seq_file *m)
{
	seq_printf(m, "mcdi_wfi: 0x%x\n", LAST_RRR_VAL(mcdi_wfi));
}


void aee_rr_show_deepidle(struct seq_file *m)
{
	seq_printf(m, "deepidle: 0x%x\n", LAST_RRR_VAL(deepidle_data));
}

void aee_rr_show_sodi(struct seq_file *m)
{
	seq_printf(m, "sodi: 0x%x\n", LAST_RRR_VAL(sodi_data));
}

void aee_rr_show_cpu_dormant(struct seq_file *m, int cpu)
{
	seq_printf(m, "  cpu_dormant: 0x%llx\n", LAST_RRR_VAL(cpu_dormant)[cpu]);
}

void aee_rr_show_clk(struct seq_file *m)
{
	int i=0;
	for(i=0; i<8; i++)
		seq_printf(m, "clk_data: 0x%x\n", LAST_RRR_VAL(clk_data)[i]);
}

void aee_rr_show_cpu_dvfs_vproc_big(struct seq_file *m)
{
	seq_printf(m, "cpu_dvfs_vproc_big: 0x%x\n", LAST_RRR_VAL(cpu_dvfs_vproc_big));
}

void aee_rr_show_cpu_dvfs_vproc_little(struct seq_file *m)
{
	seq_printf(m, "cpu_dvfs_vproc_little: 0x%x\n", LAST_RRR_VAL(cpu_dvfs_vproc_little));
}

void aee_rr_show_cpu_dvfs_oppidx(struct seq_file *m)
{
	seq_printf(m, "cpu_dvfs_oppidx: little = 0x%x\n", LAST_RRR_VAL(cpu_dvfs_oppidx) & 0xF);
	seq_printf(m, "cpu_dvfs_oppidx: big = 0x%x\n", (LAST_RRR_VAL(cpu_dvfs_oppidx) >> 4) & 0xF);
}

void aee_rr_show_cpu_dvfs_status(struct seq_file *m)
{
	seq_printf(m, "cpu_dvfs_status: 0x%x\n", LAST_RRR_VAL(cpu_dvfs_status));
}

void aee_rr_show_gpu_dvfs_vgpu(struct seq_file *m)
{
	seq_printf(m, "gpu_dvfs_vgpu: 0x%x\n", LAST_RRR_VAL(gpu_dvfs_vgpu));
}

void aee_rr_show_gpu_dvfs_oppidx(struct seq_file *m)
{
	seq_printf(m, "gpu_dvfs_oppidx: 0x%x\n", LAST_RRR_VAL(gpu_dvfs_oppidx));
}

void aee_rr_show_gpu_dvfs_status(struct seq_file *m)
{
	seq_printf(m, "gpu_dvfs_status: 0x%x\n", LAST_RRR_VAL(gpu_dvfs_status));
}

void aee_rr_show_ptp_cpu_big_volt(struct seq_file *m)
{
	int i;
	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_cpu_big_volt[%d] = %llx\n", i, (LAST_RRR_VAL(ptp_cpu_big_volt) >> (i*8)) & 0xFF);
}

void aee_rr_show_ptp_cpu_little_volt(struct seq_file *m)
{
	int i;
	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_cpu_little_volt[%d] = %llx\n", i, (LAST_RRR_VAL(ptp_cpu_little_volt) >> (i*8)) & 0xFF);
}

void aee_rr_show_ptp_gpu_volt(struct seq_file *m)
{
	int i;
	for (i = 0; i < 8; i++) 
		seq_printf(m, "ptp_gpu_volt[%d] = %llx\n", i, (LAST_RRR_VAL(ptp_gpu_volt) >> (i*8)) & 0xFF);
}

void aee_rr_show_ptp_temp(struct seq_file *m)
{
	seq_printf(m, "ptp_temp: little = %llx\n", LAST_RRR_VAL(ptp_temp) & 0xFF);
	seq_printf(m, "ptp_temp: big = %llx\n", (LAST_RRR_VAL(ptp_temp) >> 8) & 0xFF);
	seq_printf(m, "ptp_temp: GPU = %llx\n", (LAST_RRR_VAL(ptp_temp) >> 16) & 0xFF);
}

void aee_rr_show_ptp_status(struct seq_file *m)
{
	seq_printf(m, "ptp_status: 0x%x\n", LAST_RRR_VAL(ptp_status));
}

__weak uint32_t get_suspend_debug_flag(void)
{
	LAST_RR_GET(suspend_debug_flag);
}

void aee_rr_show_suspend_debug_flag(struct seq_file *m)
{
    uint32_t flag = get_suspend_debug_flag();
    seq_printf(m, "SPM Suspend debug = 0x%x\n", flag );
}

int __weak mt_reg_dump(char *buf)
{
	return 1;
}

void aee_rr_show_last_pc(struct seq_file *m)
{
	char *reg_buf = kmalloc(4096, GFP_KERNEL);
	if (reg_buf && mt_reg_dump(reg_buf) == 0) {
		seq_printf(m, "%s\n", reg_buf);
		kfree(reg_buf);
	}
}

last_rr_show_t aee_rr_show[] = {
	aee_rr_show_wdt_status,
	aee_rr_show_fiq_step,
	aee_rr_show_exp_type,
	aee_rr_show_last_pc,
	aee_rr_show_mcdi,
	aee_rr_show_suspend_debug_flag,
	aee_rr_show_deepidle,
	aee_rr_show_sodi,
	aee_rr_show_clk,
	aee_rr_show_cpu_dvfs_vproc_big,
	aee_rr_show_cpu_dvfs_vproc_little,
	aee_rr_show_cpu_dvfs_oppidx,
	aee_rr_show_cpu_dvfs_status,
	aee_rr_show_gpu_dvfs_vgpu,
	aee_rr_show_gpu_dvfs_oppidx,
	aee_rr_show_gpu_dvfs_status,
	aee_rr_show_ptp_cpu_big_volt,
	aee_rr_show_ptp_cpu_little_volt,
	aee_rr_show_ptp_gpu_volt,
	aee_rr_show_ptp_temp,
	aee_rr_show_ptp_status,
};

last_rr_show_cpu_t aee_rr_show_cpu[] = {
	aee_rr_show_last_irq_enter,
	aee_rr_show_jiffies_last_irq_enter,
	aee_rr_show_last_irq_exit,
	aee_rr_show_jiffies_last_irq_exit,
	aee_rr_show_hotplug_data1,
	aee_rr_show_hotplug_data2,
	aee_rr_show_hotplug_data3,
	aee_rr_show_cpu_dormant,
};

#define array_size(x) (sizeof(x) / sizeof((x)[0]))
int aee_rr_reboot_reason_show(struct seq_file *m, void *v)
{
	int i, cpu;
	for (i = 0; i < array_size(aee_rr_show); i++)
		aee_rr_show[i](m);

	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		seq_printf(m, "CPU %d\n", cpu);
		for (i = 0; i < array_size(aee_rr_show_cpu); i++)
			aee_rr_show_cpu[i](m, cpu);
	}
	return 0;
}

#define DEBUG
#include <linux/module.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>       //min()
#include <linux/uaccess.h>      //copy_to_user()
#include <linux/sched.h>        //TASK_INTERRUPTIBLE/signal_pending/schedule
#include <linux/poll.h>
#include <linux/io.h>           //ioremap()
#include <linux/of_fdt.h>
#include <linux/seq_file.h>
#include <asm/setup.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <asm/atomic.h>

#define ATF_LOG_CTRL_BUF_SIZE 256
#define ATF_CRASH_MAGIC_NO	0xdead1abf
//#define atf_log_lock()        atomic_inc(&(atf_buf_vir_ctl->info.atf_buf_lock))
//#define atf_log_unlock()      atomic_dec(&(atf_buf_vir_ctl->info.atf_buf_lock))

#define atf_log_lock()        ((void)0)
#define atf_log_unlock()      ((void)0)
static int has_dumped;
static unsigned char *atf_log_buffer;
static unsigned char *atf_crash_log_buf;
static wait_queue_head_t    atf_log_wq;

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

typedef union atf_log_ctl {
    struct {
        unsigned int atf_buf_addr;
        unsigned int atf_buf_size;
        unsigned int atf_write_pos;
        unsigned int atf_read_pos;
        //atf_spinlock_t atf_buf_lock;
        unsigned int atf_buf_lock;
        unsigned int atf_buf_unread_size;
        unsigned int atf_irq_count;
        unsigned int atf_reader_alive;
        unsigned long long atf_write_seq;
        unsigned long long atf_read_seq;
        unsigned int atf_aee_dbg_buf_addr;
        unsigned int atf_aee_dbg_buf_size;
	unsigned int atf_crash_log_addr;
	unsigned int atf_crash_log_size;
	unsigned int atf_crash_flag;
    } info;
    unsigned char data[ATF_LOG_CTRL_BUF_SIZE];
} atf_log_ctl_t;


struct ipanic_atf_log_rec {
    size_t total_size;
    size_t has_read;
    unsigned long start_idx;
};

atf_log_ctl_t *atf_buf_vir_ctl;
unsigned long atf_buf_phy_ctl;
unsigned int atf_buf_len;
unsigned char *atf_log_vir_addr;
unsigned int atf_log_len;
unsigned int write_index;
unsigned int read_index;

static void show_data(unsigned long addr, int nbytes, const char *name);
static unsigned int pos_to_index(unsigned int pos) 
{
    return pos - (atf_buf_phy_ctl + ATF_LOG_CTRL_BUF_SIZE);
}

static unsigned int index_to_pos(unsigned int index) 
{
    return (atf_buf_phy_ctl + ATF_LOG_CTRL_BUF_SIZE) + index;
}
#if 0
static size_t atf_log_dump_nolock(unsigned char *buffer, unsigned int start, size_t size)
{
    unsigned int len;
    unsigned int least;
    size_t skip = 0;
    unsigned char *p = atf_log_vir_addr + start;
    write_index = pos_to_index(atf_buf_vir_ctl->info.atf_write_pos);
    least = (write_index + atf_buf_len - start) % atf_buf_len;
    if (size > least)
        size = least;
    len = min(size, atf_log_len - start);
    if (size == len) {
        memcpy(buffer, atf_log_vir_addr + start, size);
    } else {
        size_t right = atf_log_len - start;
        while (skip < right) {
            if (*p != 0)
                break;
            p++;
            skip++;
        }
        //pr_notice("skip:%d, right:%d, %p\n", skip, right, p);
        memcpy(buffer, p, right - skip);
        memcpy(buffer, atf_log_vir_addr, size - right);
        return size - skip;
    }
    return size;
}
#endif

static size_t atf_log_dump_nolock(unsigned char *buffer, struct ipanic_atf_log_rec *rec, size_t size)
{
    unsigned int len;
    unsigned int least;
    write_index = pos_to_index(atf_buf_vir_ctl->info.atf_write_pos);
    //find the first letter to read
    while ((write_index + atf_log_len - rec->start_idx) % atf_log_len > 0) {
        if (*(atf_log_vir_addr + rec->start_idx) != 0)
            break;
        rec->start_idx++;
        if (rec->start_idx == atf_log_len)
            rec->start_idx = 0;
    }
    least = (write_index + atf_buf_len - rec->start_idx) % atf_buf_len;
    if (size > least)
        size = least;
    len = min(size, atf_log_len - rec->start_idx);
    if (size == len) {
        memcpy(buffer, atf_log_vir_addr + rec->start_idx, size);
    } else {
        size_t right = atf_log_len - rec->start_idx;
        memcpy(buffer, atf_log_vir_addr + rec->start_idx, right);
        memcpy(buffer, atf_log_vir_addr, size - right);
    }
    rec->start_idx += size;
    rec->start_idx %= atf_log_len;
    return size;
}
//static size_t atf_log_dump(unsigned char *buffer, unsigned int start, size_t size)
static size_t atf_log_dump(unsigned char *buffer, struct ipanic_atf_log_rec *rec, size_t size)
{
    size_t ret;
    atf_log_lock();
    //ret = atf_log_dump_nolock(buffer, start, size);
    ret = atf_log_dump_nolock(buffer, rec, size);
    atf_log_unlock();
    //show_data(atf_log_vir_addr, 24*1024, "atf_buf");
    return ret;
}

size_t ipanic_atflog_buffer(void *data, unsigned char *buffer, size_t sz_buffer)
{
		if (atf_buf_len == 0)
			return 0;
    static bool last_read = false;
    size_t count;
    struct ipanic_atf_log_rec *rec = (struct ipanic_atf_log_rec *)data;
    //pr_notice("ipanic_atf_log: need %d, rec:%d, %d, %lu\n", sz_buffer, rec->total_size, rec->has_read, rec->start_idx);
    if (rec->total_size == rec->has_read || last_read) {
        last_read = false;
        return 0;
    }
    if (rec->has_read == 0) {
        if (atf_buf_vir_ctl->info.atf_write_seq < atf_log_len && atf_buf_vir_ctl->info.atf_write_seq < sz_buffer)
            rec->start_idx = 0;
        else {
            //atf_log_lock();
            write_index = pos_to_index(atf_buf_vir_ctl->info.atf_write_pos);
            //atf_log_unlock();
            rec->start_idx = (write_index + atf_log_len - rec->total_size) % atf_log_len;
        }
    }
    //count = atf_log_dump_nolock(buffer, (rec->start_idx + rec->has_read) % atf_log_len, sz_buffer);
    //count = atf_log_dump_nolock(buffer, rec, sz_buffer);
    count = atf_log_dump(buffer, rec, sz_buffer);
    //pr_notice("ipanic_atf_log: dump %d\n", count);
    rec->has_read += count;
    if (count != sz_buffer)
        last_read = true;
    return count;
}

static ssize_t atf_log_write(struct kiocb *iocb, const struct iovec *iov, unsigned long nr_segs, loff_t ppos) 
{
    wake_up_interruptible(&atf_log_wq);
    return 1;
}

static ssize_t do_read_log_to_usr(char __user *buf, size_t count)
{
    size_t len;
    size_t least;
    write_index = pos_to_index(atf_buf_vir_ctl->info.atf_write_pos);
    read_index = pos_to_index(atf_buf_vir_ctl->info.atf_read_pos);
    least = (write_index + atf_buf_len - read_index) % atf_buf_len;
    if (count > least)
        count = least;
    len = min(count, atf_log_len - read_index);
    if (count == len) {
        if (copy_to_user(buf, atf_log_vir_addr + read_index, count))
            return -EFAULT;
    } else {
        size_t right = atf_log_len - read_index;
        if (copy_to_user(buf, atf_log_vir_addr + read_index, right))
            return -EFAULT;
        if (copy_to_user(buf, atf_log_vir_addr, count - right))
            return -EFAULT;
    }
    read_index = (read_index + count) % atf_log_len;
    return count;
}

static int atf_log_fix_reader()
{
    if (atf_buf_vir_ctl->info.atf_write_seq < atf_log_len) {
        atf_buf_vir_ctl->info.atf_read_seq = 0;
        atf_buf_vir_ctl->info.atf_read_pos = index_to_pos(0);
    } else {
        atf_buf_vir_ctl->info.atf_read_seq = atf_buf_vir_ctl->info.atf_write_seq;
        atf_buf_vir_ctl->info.atf_read_pos = atf_buf_vir_ctl->info.atf_write_pos;
    }
    return 0;
}

static int atf_log_open(struct inode *inode, struct file *file)
{
    int ret;
    ret = nonseekable_open(inode, file);
    if (unlikely(ret))
        return ret;
    file->private_data = NULL;
    atf_log_lock();
    if (!atf_buf_vir_ctl->info.atf_reader_alive) {
        atf_log_fix_reader();
    }
    atf_buf_vir_ctl->info.atf_reader_alive++;
    atf_log_unlock();
    return 0;
}

static int atf_log_release(struct inode *ignored, struct file *file)
{
    atf_buf_vir_ctl->info.atf_reader_alive--;
    return 0;
}

static ssize_t atf_log_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    ssize_t ret;
    unsigned int write_pos;
    unsigned int read_pos;
    DEFINE_WAIT(wait);

start:
    while (1) {
        atf_log_lock();
        write_pos = atf_buf_vir_ctl->info.atf_write_pos;
        read_pos = atf_buf_vir_ctl->info.atf_read_pos;
        //pr_notice("atf_log_read: wait in wq\n");
        prepare_to_wait(&atf_log_wq, &wait, TASK_INTERRUPTIBLE);
        ret = (write_pos == read_pos);
        atf_log_unlock();
        if (!ret)
            break;
        if (file->f_flags & O_NONBLOCK) {
            ret = -EAGAIN;
            break;
        }
        if (signal_pending(current)) {
            ret = -EINTR;
            break;
        }
        schedule();
    }
    finish_wait(&atf_log_wq, &wait);
    //pr_notice("atf_log_read: finish wait\n");
    if (ret)
        return ret;
    atf_log_lock();
    if (unlikely(write_pos == read_pos)) {
        atf_log_unlock();
        goto start;
    }
    ret = do_read_log_to_usr(buf, count);
    atf_buf_vir_ctl->info.atf_read_pos = index_to_pos(read_index);
    atf_buf_vir_ctl->info.atf_read_seq += ret;
    atf_log_unlock();
    //pr_notice("atf_log_read: return %d, idx: %lu, readpos: %p, writepos: %p\n", ret, read_index, atf_buf_vir_ctl->info.atf_read_pos, atf_buf_vir_ctl->info.atf_write_pos);
    return ret;
}

static unsigned int atf_log_poll(struct file *file, poll_table *wait)
{
    unsigned int ret = POLLOUT | POLLWRNORM;
    if (!(file->f_mode & FMODE_READ))
        return ret;
    poll_wait(file, &atf_log_wq, wait);
    atf_log_lock();
    if (atf_buf_vir_ctl->info.atf_write_pos != atf_buf_vir_ctl->info.atf_read_pos)
        ret |= POLLIN | POLLRDNORM;
    atf_log_unlock();
    return ret;
}

long atf_log_ioctl(struct file *flip, unsigned int cmd, unsigned long arg)
{
}

static struct file_operations atf_log_fops = {
    .owner      = THIS_MODULE,
    .unlocked_ioctl = atf_log_ioctl,
    .poll       = atf_log_poll,
    .read       = atf_log_read,
    .open       = atf_log_open,
    .release    = atf_log_release,
    .write      = atf_log_write,
};

static struct miscdevice atf_log_dev = {
    .minor      = MISC_DYNAMIC_MINOR,
    .name       = "atf_log",
    .fops       = &atf_log_fops,
    .mode       = 0644,
};

static int dt_scan_memory(unsigned long node, const char *uname, int depth, void *data)
{
    char *type = of_get_flat_dt_prop(node, "device_type", NULL);
    __be32 *reg, *endp;
    unsigned long l;
    
	/* We are scanning "memory" nodes only */
	if (type == NULL) {
		/*
		 * The longtrail doesn't have a device_type on the
		 * /memory node, so look for the node called /memory@0.
		 */
		if (depth != 1 || strcmp(uname, "memory@0") != 0)
			return 0;
	} else if (strcmp(type, "memory") != 0)
		return 0;

		reg = of_get_flat_dt_prop(node, "reg", &l);
	if (reg == NULL)
		return 0;

	endp = reg + (l / sizeof(__be32));

	while ((endp - reg) >= (dt_root_addr_cells + dt_root_size_cells)) {
		u64 base, size;

		base = dt_mem_next_cell(dt_root_addr_cells, &reg);
		size = dt_mem_next_cell(dt_root_size_cells, &reg);

		if (size == 0)
			continue;
        //pr_notice(
		//	"[PHY layout]DRAM size (dt) :  0x%llx - 0x%llx  (0x%llx)\n",
		//		(unsigned long long)base,
		//		(unsigned long long)base + (unsigned long long)size - 1,
		//		(unsigned long long)size);
	}
	*(unsigned long *)data = node;
	return node;
}

unsigned long long atf_get_from_dt(unsigned long *phy_addr, unsigned int *len) {
    unsigned long node = 0;
    mem_desc_t *mem_desc;
    if (of_scan_flat_dt(dt_scan_memory, &node)) {
        mem_desc = (mem_desc_t *)of_get_flat_dt_prop(node, "tee_reserved_mem", NULL);
        if (mem_desc && mem_desc->size) {
            pr_notice("ATF reserved memory: 0x%08llx - 0x%08llx (0x%llx)\n", mem_desc->start, mem_desc->start+mem_desc->size - 1, mem_desc->size); }
    }
    *phy_addr = mem_desc->start;
    *len = mem_desc->size;
    return 0;
}

void show_atf_log_ctl() {
    pr_notice("atf_buf_addr(%p) = 0x%x\n", &(atf_buf_vir_ctl->info.atf_buf_addr), atf_buf_vir_ctl->info.atf_buf_addr);
    pr_notice("atf_buf_size(%p) = 0x%x\n", &(atf_buf_vir_ctl->info.atf_buf_size), atf_buf_vir_ctl->info.atf_buf_size);
    pr_notice("atf_write_pos(%p) = %u\n", &(atf_buf_vir_ctl->info.atf_write_pos), atf_buf_vir_ctl->info.atf_write_pos);
    pr_notice("atf_read_pos(%p) = %u\n", &(atf_buf_vir_ctl->info.atf_read_pos), atf_buf_vir_ctl->info.atf_read_pos);
    pr_notice("atf_buf_lock(%p) = %u\n", &(atf_buf_vir_ctl->info.atf_buf_lock), atf_buf_vir_ctl->info.atf_buf_lock);
    pr_notice("atf_buf_unread_size(%p) = %u\n", &(atf_buf_vir_ctl->info.atf_buf_unread_size), atf_buf_vir_ctl->info.atf_buf_unread_size);
    pr_notice("atf_irq_count(%p) = %u\n", &(atf_buf_vir_ctl->info.atf_irq_count), atf_buf_vir_ctl->info.atf_irq_count);
    pr_notice("atf_reader_alive(%p) = %u\n", &(atf_buf_vir_ctl->info.atf_reader_alive), atf_buf_vir_ctl->info.atf_reader_alive);
    pr_notice("atf_write_seq(%p) = %llu\n", &(atf_buf_vir_ctl->info.atf_write_seq), atf_buf_vir_ctl->info.atf_write_seq);
    pr_notice("atf_read_seq(%p) = %llu\n", &(atf_buf_vir_ctl->info.atf_read_seq), atf_buf_vir_ctl->info.atf_read_seq);
}

static void show_data(unsigned long addr, int nbytes, const char *name)
{
	int	i, j;
	int	nlines;
	u32	*p;

	/*
	 * don't attempt to dump non-kernel addresses or
	 * values that are probably just small negative numbers
	 */
	if (addr < PAGE_OFFSET || addr > -256UL)
		return;

	printk("\n%s: %#lx:\n", name, addr);

	/*
	 * round address down to a 32 bit boundary
	 * and always dump a multiple of 32 bytes
	 */
	p = (u32 *)(addr & ~(sizeof(u32) - 1));
	nbytes += (addr & (sizeof(u32) - 1));
	nlines = (nbytes + 31) / 32;


	for (i = 0; i < nlines; i++) {
		/*
		 * just display low 16 bits of address to keep
		 * each line of the dump < 80 characters
		 */
		printk("%04lx ", (unsigned long)p & 0xffff);
		for (j = 0; j < 8; j++) {
			u32	data;
			if (probe_kernel_address(p, data)) {
				printk(" ********");
			} else {
				printk(" %08x", data);
			}
			++p;
		}
		printk("\n");
	}
}

static irqreturn_t ATF_log_irq_handler(int irq, void *dev_id)
{
    if (!atf_buf_vir_ctl->info.atf_reader_alive) {
        pr_err("No alive reader, but still recieve irq\n");
    } else {
        pr_info("ATF_log_irq triggered!\n");
    }
    wake_up_interruptible(&atf_log_wq);
    return IRQ_HANDLED;
}

static const struct file_operations proc_atf_log_file_operations = {
    .owner  = THIS_MODULE,
    .open   = atf_log_open,
    .read   = atf_log_read,
    .unlocked_ioctl = atf_log_ioctl,
    .release = atf_log_release,
    .poll   = atf_log_poll,
};

static int atf_crash_show(struct seq_file *m, void *v)
{
    seq_write(m, atf_crash_log_buf, atf_buf_vir_ctl->info.atf_crash_log_size);
    return 0;
}

static int atf_crash_file_open(struct inode *inode, struct file *file)
{
    return single_open(file, atf_crash_show, inode->i_private);
}

static const struct file_operations proc_atf_crash_file_operations = {
    .owner = THIS_MODULE,
    .open = atf_crash_file_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = seq_release,
};

static struct proc_dir_entry *atf_log_proc_dir;
static struct proc_dir_entry *atf_log_proc_file;
static struct proc_dir_entry *atf_crash_proc_file;

static int __init atf_log_init(void)
{
    //register module driver
    int err;
    err = misc_register(&atf_log_dev);
    if (unlikely(err)) {
        pr_err("atf_log: failed to register device");
        return -1;
    } else {
        pr_notice("atf_log: inited");
    }
    //get atf reserved memory(atf_buf_phy_ctl) from device
    atf_get_from_dt(&atf_buf_phy_ctl, &atf_buf_len);    //TODO
    if (atf_buf_len == 0) {
    	pr_err("No atf_log_buffer!\n");
    	return -1;
    }
    // map control header
    atf_buf_vir_ctl = ioremap_wc(atf_buf_phy_ctl, ATF_LOG_CTRL_BUF_SIZE);
    atf_log_len = atf_buf_vir_ctl->info.atf_buf_size;
    // map log buffer
    atf_log_vir_addr = ioremap_wc(atf_buf_phy_ctl + ATF_LOG_CTRL_BUF_SIZE, atf_log_len);
    pr_notice("atf_buf_phy_ctl: 0x%lu\n", atf_buf_phy_ctl);
    pr_notice("atf_buf_len: %u\n", atf_buf_len);
    pr_notice("atf_buf_vir_ctl: %p\n", atf_buf_vir_ctl);
    pr_notice("atf_log_vir_addr: %p\n", atf_log_vir_addr);
    pr_notice("atf_log_len: %u\n", atf_log_len);
    //show_atf_log_ctl();
    //show_data(atf_buf_vir_ctl, 512, "atf_buf");
    atf_buf_vir_ctl->info.atf_reader_alive = 0;
    atf_buf_vir_ctl->info.atf_read_seq = 0;
    //initial wait queue
    init_waitqueue_head(&atf_log_wq);
    if (request_irq(281, (irq_handler_t)ATF_log_irq_handler, IRQF_TRIGGER_NONE, "ATF_irq",NULL) != 0) {
        pr_crit("Fail to request ATF_log_irq interrupt!\n");
        return -1;
    }
    // create /proc/atf_log
    atf_log_proc_dir = proc_mkdir("atf_log", NULL);
    if (atf_log_proc_dir == NULL) {
        pr_err("atf_log proc_mkdir failed\n");
        return -ENOMEM;
    }
    // create /proc/atf_log/atf_log
    atf_log_proc_file = proc_create("atf_log", 0444, atf_log_proc_dir, &proc_atf_log_file_operations);
    if (atf_log_proc_file == NULL) {
        pr_err("atf_log proc_create failed at atf_log\n");
        return -ENOMEM;
    }

    if (atf_buf_vir_ctl->info.atf_crash_flag == ATF_CRASH_MAGIC_NO) {
	atf_crash_proc_file = proc_create("atf_crash", 0444, atf_log_proc_dir, &proc_atf_crash_file_operations);
	if (atf_crash_proc_file == NULL) {
	    pr_err("atf_log proc_create failed at atf_crash\n");
	    return -ENOMEM;
	}
	atf_buf_vir_ctl->info.atf_crash_flag = 0;
	atf_crash_log_buf = ioremap_wc(atf_buf_vir_ctl->info.atf_crash_log_addr, atf_buf_vir_ctl->info.atf_crash_log_size);
    }
    
    return 0;
}

static void __exit atf_log_exit(void)
{ 
    //deregister module driver
    int err;
    err = misc_deregister(&atf_log_dev);
    if (unlikely(err)) {
        pr_err("atf_log: failed to unregister device");
    }
    pr_notice("atf_log: exited");
}

module_init(atf_log_init);
module_exit(atf_log_exit);

MODULE_DESCRIPTION("MEDIATEK Module ATF Logging Driver");
MODULE_AUTHOR("Ji Zhang<ji.zhang@mediatek.com>");


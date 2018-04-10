
#define pr_fmt(fmt)  "ktrace : " fmt


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/ktrace.h>

#define BUF_SIZE 64

struct ktrace_print {
	struct ktrace_queue *q;
	struct dentry *dir;

	void *buf;
	int entry_size;
	int max;
};

static struct ktrace_print __print;

void ktrace_print(const char *fmt, ...)
{
	struct ktrace_print *kp = &__print;
	struct ktrace_queue *q = kp->q;
	char buf[BUF_SIZE];
	va_list args;
	int cnt;

	if (unlikely(!kp->buf))
		return;

	va_start(args, fmt);
	cnt = vsnprintf(buf, BUF_SIZE, fmt, args);
	va_end(args);

	ktrace_add_queue_multi(q, buf, cnt);

	return;
}

static int str_show_entry(struct seq_file *m, void *entry, bool debug)
{
	char *b = (char *)entry;
	int ret;

	ret = seq_putc(m, *b);

	return ret;
}

KTRACE_QUEUE_RO_SINGLE(print);

static int print_clean_write(void *data, u64 val)
{
	struct ktrace_queue *q = data;
	int clean = val;

	if (clean) {
		ktrace_reset_queue(q);
	}

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(print_clean, NULL, print_clean_write, "%llu\n");

int __init ktrace_print_init(struct dentry *dir, struct ktrace_queue *q)
{
	struct ktrace_print *kp = &__print;
	void *buf;

	memset(kp, sizeof(struct ktrace_print), 0);

	buf = (void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO, 1);
	if (!buf)
		return -ENOMEM;
	kp->q = q;

	kp->entry_size = 1;
	kp->max = (PAGE_SIZE << 1);
	kp->dir = debugfs_create_dir("print", dir);

	ktrace_init_queue(q, kp->dir, kp, buf, kp->entry_size, kp->max,
			DEFAULT_ITEMS_PER_READ,
			str_show_entry);

	if (kp->dir) {
		debugfs_create_file("trace",
				S_IFREG | S_IRUGO,
				kp->dir,
				q,
				&print_fops);

		debugfs_create_file("clean", S_IWUSR, kp->dir, q,
				&print_clean);
	}

	smp_mb();
	kp->buf = buf;
	pr_info("kp ktrace init OK\n");

	return 0;
}

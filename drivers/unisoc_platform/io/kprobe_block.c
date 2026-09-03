// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023, Unisoc, Inc.
 *
 * trace io insert/issue/complete.
 *
 * For more information on theory of operation of kprobes, see
 * Documentation/kprobes.txt
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/rbtree.h>
#include <linux/workqueue.h>
#include <linux/timekeeping.h>
#include <linux/blkdev.h>
#include <linux/list.h>

#define MAX_STORAGE_NUM        4
#define STAT_PRINT_INTERVAL    10  // print interval in second
#define DISK_NAME_LEN_MAX      32
#define FROM                   3   // 2^FROM ms
#define IO_ARRAY_SIZE          12  // last column: 2^(FROM+IO_ARRAY_SIZE-2) ms

#define OP_TYPE			4

/* R:read W:write F:flush D:discard */
static const char op_char[OP_TYPE] = {'R', 'W', 'F', 'D'};

struct op_info {
	unsigned long comp_count;
	unsigned long comp_sectors;
	unsigned long op_i2i[IO_ARRAY_SIZE];
	unsigned long op_i2c[IO_ARRAY_SIZE];
};

struct io_disk_info {
	char disk_name[DISK_NAME_LEN_MAX];
	spinlock_t lock;

	unsigned long rq_count;
	unsigned long rq_count_bak;
	struct op_info opinfo[OP_TYPE];
	struct op_info opinfo_bak[OP_TYPE];
};

struct io_debug_info {
	struct io_disk_info	disk_info[MAX_STORAGE_NUM];
	spinlock_t lock;
	u64 start;
};

static struct io_debug_info io_debug = {
	.disk_info = {
		{
			.disk_name = "sda",
		},
		{
			.disk_name = "mmcblk0",
		},
		{
			.disk_name = "mmcblk1",
		},
		{
			.disk_name = "loop",
		},
	},
};

/*
 * convert ms to index:
 * [0]     [1]      [2]          [3]          [...] [N]
 * <2^FROM >=2^FROM >=2^(FROM+1) >=2^(FROM+2)  ...  >=2^(FROM+N-1)
 */

static inline unsigned int ms_to_index(unsigned int ms)
{
	ms >>= FROM;
	return ms > 0 ? min((IO_ARRAY_SIZE - 1), ilog2(ms) + 1) : 0;
}

static void _io_backup_log(u64 complete)
{
	struct io_disk_info *info;
	unsigned long lock_flags;
	int size, i, j;

	size = sizeof(struct op_info);

	for (i = 0; i < MAX_STORAGE_NUM; i++) {
		info = &io_debug.disk_info[i];

		spin_lock_irqsave(&info->lock, lock_flags);
		info->rq_count_bak = info->rq_count;
		if (!info->rq_count) {
			spin_unlock_irqrestore(&info->lock, lock_flags);
			continue;
		}

		info->rq_count = 0;
		memset(info->opinfo_bak, 0, OP_TYPE * size);
		for (j = 0; j < OP_TYPE; j++) {
			if (!info->opinfo[j].comp_count)
				continue;
			memcpy(&info->opinfo_bak[j], &info->opinfo[j], size);
			memset(&info->opinfo[j], 0, size);
		}
		spin_unlock_irqrestore(&info->lock, lock_flags);
	}

}

#define array_log(array, fmt, ...) \
	pr_info(fmt "[%3ld-%5ldms]:%5ld %4ld %4ld %4ld %4ld %4ld %4ld %4ld %4ld %4ld %4ld %4ld\n", \
		##__VA_ARGS__, 1UL << FROM, 1UL << (FROM + IO_ARRAY_SIZE - 2), \
		array[0], array[1], array[2], array[3], \
		array[4], array[5], array[6], array[7], \
		array[8], array[9], array[10], array[11])

static void pr_iolog(struct io_disk_info *info, int type)
{
	pr_info("|_%-10s %c complete: %6lu request %8lu sectors\n", info->disk_name, op_char[type],
		info->opinfo_bak[type].comp_count, info->opinfo_bak[type].comp_sectors);
	array_log(info->opinfo_bak[type].op_i2i, "|_%-10s %c i2i", info->disk_name, op_char[type]);
	array_log(info->opinfo_bak[type].op_i2c, "|_%-10s %c i2c", info->disk_name, op_char[type]);

}

static void _io_print_info(void)
{
	struct io_disk_info *info;
	int i, j;

	for (i = 0; i < MAX_STORAGE_NUM; i++) {
		info = &io_debug.disk_info[i];
		if (!info->rq_count_bak)
			continue;
		pr_info("|_%-10s total complete %lu requests\n",
			info->disk_name, info->rq_count_bak);
		for (j = 0; j < OP_TYPE; j++) {
			if (!info->opinfo_bak[j].comp_count)
				continue;
			pr_iolog(info, j);
		}
	}
}

/* For each probe you need to allocate a kprobe structure */
static struct kprobe issue_rq_kp = {
	.symbol_name	= "blk_mq_start_request",
};

static struct kprobe complete_rq_kp = {
	.symbol_name	= "blk_update_request",
};

static void issue_handler_post(struct kprobe *p, struct pt_regs *regs,
				unsigned long flags)
{
#ifdef CONFIG_ARM64
	struct request *rq = (struct request *)(regs->regs[0]);
#endif
#ifdef CONFIG_ARM
	struct request *rq = (struct request *)(regs->uregs[0]);
#endif

	if (rq == NULL)
		return;

	rq->io_start_time_ns = ktime_get_ns();
}

static void complete_handler_post(struct kprobe *p, struct pt_regs *regs,
				unsigned long flags)
{
#ifdef CONFIG_ARM64
	struct request *rq = (struct request *)(regs->regs[0]);
#endif
#ifdef CONFIG_ARM
	struct request *rq = (struct request *)(regs->uregs[0]);
#endif
	struct io_disk_info *info;
	unsigned long lock_flags;
	u64 complete_ns, issue_time_ns, insert_time_ns, msecs;
	unsigned int index;
	int op_type;
	int i;

	if (rq == NULL || rq->rq_disk == NULL)
		return;

	complete_ns = ktime_get_ns();
	insert_time_ns = rq->start_time_ns;
	issue_time_ns = rq->io_start_time_ns;

	if (insert_time_ns == 0 || issue_time_ns == 0 || issue_time_ns < insert_time_ns)
		return;

	for (i = 0; i < MAX_STORAGE_NUM; i++) {
		info = &io_debug.disk_info[i];
		if (!strcmp(info->disk_name, rq->rq_disk->disk_name) ||
			(!strcmp(info->disk_name, "loop") &&
			!strncmp(info->disk_name, rq->rq_disk->disk_name, 4)))
			break;
	}
	if (i >= MAX_STORAGE_NUM)
		return;

	op_type = req_op(rq);
	if (op_type >= OP_TYPE)
		return;

	spin_lock_irqsave(&info->lock, lock_flags);
	/* statistics req number */
	++info->rq_count;
	++info->opinfo[op_type].comp_count;

	/* accumulate total sectors */
	info->opinfo[op_type].comp_sectors += blk_rq_sectors(rq);

	/* start/insert to issue */
	msecs = ktime_to_ms(issue_time_ns - insert_time_ns);
	index = ms_to_index(msecs);
	++info->opinfo[op_type].op_i2i[index];

	/* issue to complete */
	msecs = ktime_to_ms(complete_ns - issue_time_ns);
	index = ms_to_index(msecs);
	++info->opinfo[op_type].op_i2c[index];
	spin_unlock_irqrestore(&info->lock, lock_flags);

	spin_lock_irqsave(&io_debug.lock, lock_flags);
	if (complete_ns > (io_debug.start + (1000000000ULL * STAT_PRINT_INTERVAL))) {
		io_debug.start = complete_ns;
		spin_unlock_irqrestore(&io_debug.lock, lock_flags);
		_io_backup_log(complete_ns);
		_io_print_info();
	} else {
		spin_unlock_irqrestore(&io_debug.lock, lock_flags);
	}

#ifdef DEBUG_TIME
	pr_info("%10s complete: %-16lu %lu\n", info->disk_name,
			ktime_get_ns() - complete_ns, complete_ns - issue_time_ns);
#endif
}

static int __init sprd_iodebug_init(void)
{
	int ret;
	int i;
	struct io_disk_info *info;

	spin_lock_init(&io_debug.lock);
	for (i = 0; i < MAX_STORAGE_NUM; i++) {
		info = &io_debug.disk_info[i];
		spin_lock_init(&info->lock);
	}

	issue_rq_kp.post_handler = issue_handler_post;
	ret = register_kprobe(&issue_rq_kp);
	if (ret < 0) {
		pr_err("register issue kprobe failed, returned %d\n", ret);
		return ret;
	}

	complete_rq_kp.post_handler = complete_handler_post;
	ret = register_kprobe(&complete_rq_kp);
	if (ret < 0) {
		pr_err("register complete kprobe failed, returned %d\n", ret);
		return ret;
	}

	return 0;
}

static void __exit sprd_iodebug_exit(void)
{
	unregister_kprobe(&complete_rq_kp);
	unregister_kprobe(&issue_rq_kp);
}

module_init(sprd_iodebug_init)
module_exit(sprd_iodebug_exit)
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hongyu Jin, Yunlong Xing, Unisoc Inc.");

#include <linux/blkdev.h>

#define IO_WINDOW_LIMIT_MASK            1
#define IO_TOKEN_LIMIT_MASK             2
#define IO_TOKEN_NATIVE_LIMIT_MASK      4
#define IO_TOKEN_FG_LIMIT_MASK          8
#define IO_CFQ_SELECTQ_SPEED_MASK       16
#define IO_DEBUG_MASK                   31

#define KB (1024)
#define MB (1024 * KB)

#define LIMIT_STIME_MIN			16
#define LIMIT_STIME_HIGH		100

enum group_type {
	NATIVE = 0,
	BG,
	SBG,
	GL = 5,
	FG = 9,
	TOP = 11,
	GM,
};

static enum group_type cgroup;

static inline atomic64_t *bdev_get_stime(struct block_device *bdev)
{
	return &(bdev->bd_disk->queue->io_stime);
}

static inline atomic64_t *bdev_get_wtime(struct block_device *bdev)
{
	return &(bdev->bd_disk->queue->io_wtime);
}

static inline struct backing_dev_info *bdev_get_bdi(struct block_device *bdev)
{
	return bdev->bd_disk->queue->backing_dev_info;
}

extern unsigned int sysctl_mi_iolimit;
extern int task_type(struct task_struct *task);
extern bool native_need_limit(void);
extern unsigned long elapsed_jiffies(unsigned long start);
extern unsigned long filemap_range_nr_page(struct address_space *mapping,
		pgoff_t index, pgoff_t end);
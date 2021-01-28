/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MMC_CORE_BLOCK_H
#define _MMC_CORE_BLOCK_H

struct mmc_queue;
struct request;

struct mmc_blk_data {
	spinlock_t	lock;
	struct device	*parent;
	struct gendisk	*disk;
	struct mmc_queue queue;
	struct list_head part;
	struct list_head rpmbs;

	unsigned int	flags;
/* Can do SET_BLOCK_COUNT for multiblock */
#define MMC_BLK_CMD23	(1 << 0)
/* MMC Reliable write support */
#define MMC_BLK_REL_WR	(1 << 1)

	unsigned int	usage;
	unsigned int	read_only;
	unsigned int	part_type;
	unsigned int	reset_done;
#define MMC_BLK_READ		BIT(0)
#define MMC_BLK_WRITE		BIT(1)
#define MMC_BLK_DISCARD		BIT(2)
#define MMC_BLK_SECDISCARD	BIT(3)
#define MMC_BLK_CQE_RECOVERY	BIT(4)

	/*
	 * Only set in main mmc_blk_data associated
	 * with mmc_card with dev_set_drvdata, and keeps
	 * track of the current selected device partition.
	 */
	unsigned int	part_curr;
	struct device_attribute force_ro;
	struct device_attribute power_ro_lock;
	int	area_type;

	/* debugfs files (only in main mmc_blk_data) */
	struct dentry *status_dentry;
	struct dentry *ext_csd_dentry;
};


void mmc_blk_cqe_recovery(struct mmc_queue *mq);

enum mmc_issued;

enum mmc_issued mmc_blk_mq_issue_rq(struct mmc_queue *mq, struct request *req);
void mmc_blk_mq_complete(struct request *req);
void mmc_blk_mq_recovery(struct mmc_queue *mq);

struct work_struct;

void mmc_blk_mq_complete_work(struct work_struct *work);

#endif

#include <linux/init.h>	
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/printk.h>
#include <linux/reboot.h>
#include <linux/efi.h>
#include <linux/rcupdate.h>
#include <linux/of.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_host.h>
#include <linux/completion.h>
#include "charger_partition.h"

/* 读取分区 */
int charger_partition_read(const char *partition_name, void *buf, size_t size, loff_t offset)
{
	struct block_device *bdev;
	struct bio *bio;
	unsigned int block_size;
	void *safe_buf = NULL;
	int ret = 0;
	pr_info("%s partition name:%s size:%zu offset:0x%llx \n", __func__, partition_name, size, offset);
	/* 打开块设备 */
	bdev = blkdev_get_by_path(partition_name, FMODE_READ | FMODE_EXEC, THIS_MODULE); // kernel 5.15
	if (IS_ERR(bdev)) {
		pr_err("%s: Failed to open block device: %s (err=%ld)\n",
				__func__, partition_name, PTR_ERR(bdev));
		return PTR_ERR(bdev);
	}
	/* 获取块大小并校验对齐 */
	block_size = bdev_logical_block_size(bdev);
	if ((offset | size) & (block_size - 1)) {
		pr_err("%s: Unaligned access (offset=0x%llx size=0x%zx block_size=0x%x)\n",
				__func__, offset, size, block_size);
		ret = -EINVAL;
		goto out_blkdev;
	}
	safe_buf = kvmalloc(size, GFP_KERNEL);
	if (!safe_buf) {
		ret = -ENOMEM;
		goto out_blkdev;
	}
	bio = bio_alloc(GFP_KERNEL, 1);  // kernel 5.15
	if (!bio) {
		pr_err("%s: Failed to allocate bio\n", __func__);
		ret = -ENOMEM;
		goto out_buf;
	}
	bio_set_dev(bio, bdev);
	bio->bi_iter.bi_sector = offset >> SECTOR_SHIFT;
	bio->bi_opf = REQ_OP_READ;  // kernel 5.15
	if (bio_add_page(bio, virt_to_page(safe_buf), size, offset_in_page(safe_buf)) != size) {
		pr_err("%s: bio_add_page failed (size=%u/%zu)\n",
				__func__, bio->bi_iter.bi_size, size);
		ret = -EIO;
		goto out_bio;
	}
	submit_bio_wait(bio);
	if (bio->bi_status) {
		pr_err("%s: Read error: (status=0x%x)\n",
				__func__, bio->bi_status);
		ret = blk_status_to_errno(bio->bi_status);
	}
	/* 拷贝数据到用户缓冲区 */
	memcpy(buf, safe_buf, size);
out_bio:
	bio_put(bio);
out_buf:
	kvfree(safe_buf);
out_blkdev:
	blkdev_put(bdev, FMODE_READ); //kernel 5.15
	return ret;
}

/* 写入分区 */
int charger_partition_write(const char *partition_name, const void *buf, size_t size, loff_t offset)
{
	struct block_device *bdev;
	struct bio *bio;
	unsigned int block_size;
	void *safe_buf = NULL;
	int ret = 0;
	pr_info("%s partition name:%s size:%zu offset:0x%llx \n", __func__, partition_name, size, offset);
	if(size > PAGE_SIZE) {
		/* 如果写入的数据长度大于1 page的大小，则需要重新适配，bio_add_page,只能写1 page的数据 */
		pr_err("%s size:%zu > PAGE_SIZE:%lu \n", __func__, size, PAGE_SIZE);
		return -EINVAL;
	}
	/* 打开块设备 */
	bdev = blkdev_get_by_path(partition_name, FMODE_WRITE | FMODE_EXEC, THIS_MODULE); // kernel 5.15
	if (IS_ERR(bdev)) {
		pr_err("%s: Failed to open block device: %s (err=%ld)\n",
				__func__, partition_name, PTR_ERR(bdev));
		return PTR_ERR(bdev);
	}
	/* 获取块大小并校验对齐 */
	block_size = bdev_logical_block_size(bdev);
	if ((offset | size) & (block_size - 1)) {
		pr_err("%s: Unaligned access (offset=0x%llx size=0x%zx block_size=0x%x)\n",
				__func__, offset, size, block_size);
		ret = -EINVAL;
		goto out_blkdev;
	}
	safe_buf = kvmalloc(size, GFP_KERNEL);
	if (!safe_buf) {
		ret = -ENOMEM;
		goto out_blkdev;
	}
	memcpy(safe_buf, buf, size);
	bio = bio_alloc(GFP_KERNEL, 1); // kernel 5.15
	if (!bio) {
		pr_err("%s: Failed to allocate bio\n", __func__);
		ret = -ENOMEM;
		goto out_buf;
	}
	bio_set_dev(bio, bdev);
	bio->bi_iter.bi_sector = offset >> SECTOR_SHIFT;
	bio->bi_opf = REQ_OP_WRITE | REQ_SYNC; // kernel 5.15
	pr_info("%s PAGE_SIZE:%lu SECTOR_SHIFT:%d block_size:0x%x(%d)\n", __func__, PAGE_SIZE, SECTOR_SHIFT, block_size, block_size);
	if (bio_add_page(bio, virt_to_page(safe_buf), size, offset_in_page(safe_buf)) != size) {
		pr_err("%s: bio_add_page failed (size=%u/%zu)\n",
				__func__, bio->bi_iter.bi_size, size);
		ret = -EIO;
		goto out_bio;
	}
	submit_bio_wait(bio);
	if (bio->bi_status) {
		pr_err("%s: Write error:(status=0x%x)\n",
				__func__, bio->bi_status);
		ret = blk_status_to_errno(bio->bi_status);
	}
	if (!ret) {
		ret = blkdev_issue_flush(bdev);
		if (ret)
			pr_err("%s: Flush failed (err=%d)\n", __func__, ret);
	}
out_bio:
	bio_put(bio);
out_buf:
	kvfree(safe_buf);
out_blkdev:
	blkdev_put(bdev, FMODE_WRITE); // kernel 5.15
	return ret;
}

int check_charger_partition_header(void)
{
	int ret;
	uint64_t offset;
	charger_partition_header *header = NULL;
	header = kzalloc(CHARGER_CONFIG_SIZE, GFP_KERNEL);
	if (!header) {
		pr_err("Failed to allocate config buffer\n");
		return -EINVAL;
	}
	offset = CHARGER_PARTITION_HEADER * CHARGER_PARTITION_RWSIZE;
	ret = charger_partition_read(CHARGER_PARTITION_NAME, header, CHARGER_CONFIG_SIZE, 0);
	if (ret < 0) {
		pr_err("%s:Failed to read charger config1\n",__func__);
		ret = -EINVAL;
		goto out_free;
	}
	pr_info("[charger] %s start:magic:0x%0x, version:%d\n", __func__, header->magic, header->version);
	if(header->magic != CHARGER_PARTITION_MAGIC) {
		pr_err("[charger] %s magic error, set to default!\n", __func__);
		header->magic = CHARGER_PARTITION_MAGIC;
	}
	header->initialized = 1;
	header->avaliable = 1;
	pr_info("[charger] %s initiablized ok\n", __func__);
	ret = charger_partition_write(CHARGER_PARTITION_NAME, header, CHARGER_CONFIG_SIZE, 0);
	if (ret < 0) {
		pr_err("%s:Failed to write charger config\n",__func__);
		ret = -EINVAL;
		goto out_free;
	}
	ret = charger_partition_read(CHARGER_PARTITION_NAME, header, CHARGER_CONFIG_SIZE, 0);
	if (ret < 0) {
		pr_err("%s:Failed to read charger config\n",__func__);
		ret = -EINVAL;
		goto out_free;
	}
	pr_info("[charger] %s end:magic:0x%0x, version:%d\n", __func__, header->magic, header->version);
out_free:
	kfree(header);
	return ret;
}

static void charger_partition_work(struct work_struct *work)
{
	check_charger_partition_header();
}

int charger_partition_init(void)
{
	pr_info("charger_partition init start\n");
	charger_partition = (struct ChargerPartition *)kzalloc(sizeof(struct ChargerPartition), GFP_KERNEL);
	// pr_err("[charger] %s ufs_xiaomi_comp wait...\n", __func__);
	// wait_for_completion(&ufs_xiaomi_comp);
	// pr_err("[charger] %s ufs_xiaomi_comp ready...\n", __func__);
	INIT_DELAYED_WORK(&charger_partition->charger_partition_work, charger_partition_work);
	schedule_delayed_work(&charger_partition->charger_partition_work, msecs_to_jiffies(CHARGER_WORK_DELAY_MS));
	pr_info("charger_partition init end\n");
	return 0;
}

void charger_partition_exit(void)
{
	cancel_delayed_work(&charger_partition->charger_partition_work);
	kfree(charger_partition);
}

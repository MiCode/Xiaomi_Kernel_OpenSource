/*
 * Copyright (C) 2019 Xiaomi Ltd.
 *
 * Author:
 *    Shane Gao  <gaoshan3@xiaomi.com>
 *    Venco Du   <duwenchao@xiaomi.com>
 */
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/spinlock.h>
#include <linux/reboot.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/efi.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_host.h>

#include "ufshcd.h"

#include "SKhynix_A002.h"
#include "SKhynix_A043.h"
#include "SAMSUNG_128GB_0200.h"
#include "SAMSUNG_256GB_0200.h"
#include "SAMSUNG_512GB_0200.h"
#include "WDC_128GB_1270.h"
#include "KIOXIA_256GB_1201.h"
#include "KIOXIA_512GB_1201.h"
#include "SKhynix_A043_ufs22.h"
#include "hynix_UFS22_A002.h"
#include "Samsung_UFS22_128G_0700.h"
#include "Samsung_UFS22_256G_0700.h"



typedef unsigned int uint32;
typedef  int int32;
typedef unsigned short CHAR16;

#define INQURIY_VENDOR_ID_SIZE		8
#define INQURIY_PRODUCT_ID_SIZE		16
#define INQURIY_FW_VERSION_SIZE		4
#define QPNP_PON_FFU_MASK			0x40
#define QPNP_PON_FFU_BIT			6
#define PART_SECTOR_SIZE			(0x200)
#define PART_BLOCK_SIZE				(0x1000)
#define PART_NAME 					"sda"
#define PART_NUMBER 				(14)		//sda14 --> bk07
#define UFS_MAX_BLOCK_TRANSFERS		(128)
#define FFU_BIN_HEAD_INFO			(0x100)
#define FFU_FLAG 					"ffu"

bool one_time_entry = false;

#pragma pack(1)
typedef struct part {
	sector_t part_start;
	sector_t part_size;
} partinfo;

typedef struct DEV_ST {
    char ffu_flag[INQURIY_FW_VERSION_SIZE + 1]; //0 or ffu
    char ffu_pn[INQURIY_PRODUCT_ID_SIZE + 1]; //pn
    char ffu_current_fw[INQURIY_FW_VERSION_SIZE + 1]; //current fw version
    char ffu_target_fw[INQURIY_FW_VERSION_SIZE + 1]; //target ffu version
    uint32 ffu_count; //ffu count, also is fw count
} device_struct;

struct ffu_inquiry {
	uint8_t vendor_id[INQURIY_VENDOR_ID_SIZE + 1];
	uint8_t product_id[INQURIY_PRODUCT_ID_SIZE + 1];
	uint8_t product_rev[INQURIY_FW_VERSION_SIZE + 1];
};

struct fw_update_checklist {
	uint8_t vendor_id[INQURIY_VENDOR_ID_SIZE + 1];
	uint8_t product_id[INQURIY_PRODUCT_ID_SIZE + 1];
	uint8_t product_rev_from[INQURIY_FW_VERSION_SIZE + 1];
	uint8_t product_rev_to[INQURIY_FW_VERSION_SIZE + 1];
	uint64_t fw_addr;
	uint32_t fw_size;
};
#pragma pack()

extern int qpnp_pon_uvlo_get(int mask) __attribute__((weak));
extern int qpnp_pon_uvlo_set(int val, int mask) __attribute__((weak));
static partinfo part_info = { 0 };

static bool off_charge_flag;

/*
 * set enable ffu function flag by the pon reg.
 *
 * return 0, succeed. other is fail.
 */
static int ffu_set_flag(int flag)
{
	int val;

	val = (flag == 1) ? (1 << QPNP_PON_FFU_BIT) : 0;

	return qpnp_pon_uvlo_set(val, QPNP_PON_FFU_MASK);
}

/*
 * get flag about ffu function.
 *
 *
 */
static int ffu_get_flag(void)
{
	return qpnp_pon_uvlo_get(QPNP_PON_FFU_MASK) ? 1 : 0;
}

/*
* get info of partition from ufs for ffu bin
*
*/
void get_info_of_partition(char* part_name, int part_number, sector_t from, sector_t size)
{
	if (!strncmp(part_name, PART_NAME, 3) && part_number == PART_NUMBER) {
		part_info.part_start = from * PART_SECTOR_SIZE / PART_BLOCK_SIZE;
		part_info.part_size = size * PART_SECTOR_SIZE / PART_BLOCK_SIZE;
		printk("[ufs_ffu] partion:%s start %d(block) size %d(block)\n", part_name, part_info.part_start, part_info.part_size);
	}
}
/*
*crc funtion
*/
static unsigned int crc32(unsigned char const *p, unsigned int len)
{
	int i;
	unsigned int crc = 0;
	while (len--) {
		crc ^= *p++;
		for (i = 0; i < 8; i++)
			crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
	}
 	return crc;
}

/*
* write data to partition
*
*/
static int ufs_scsi_write_partition(struct scsi_device *sdev, void *buf, uint32_t lba, uint32_t blocks)
{
	uint8_t cdb[16];
	int ret = 0;
	struct ufs_hba *hba = NULL;
	struct scsi_sense_hdr sshdr = {};
	unsigned long flags = 0;

	hba = shost_priv(sdev->host);

	spin_lock_irqsave(hba->host->host_lock, flags);
	ret = scsi_device_get(sdev);
	if (!ret && !scsi_device_online(sdev)) {
		ret = -ENODEV;
		scsi_device_put(sdev);
		pr_info("get device fail\n");
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ret)
		return ret;

	hba->host->eh_noresume = 1;

	// Maximum size of transfer supported by CMD_READ 10 is 16k blocks
	if (blocks > UFS_MAX_BLOCK_TRANSFERS) {
		return -EINVAL;
	}

	// Fill in the CDB with SCSI command structure
	memset (cdb, 0, sizeof(cdb));
	cdb[0] = WRITE_10;				// Command
	cdb[1] = 0;
	cdb[2] = (lba >> 24) & 0xff;	// LBA
	cdb[3] = (lba >> 16) & 0xff;
	cdb[4] = (lba >> 8) & 0xff;
	cdb[5] = (lba) & 0xff;
	cdb[6] = 0;						// Group Number
	cdb[7] = (blocks >> 8) & 0xff;	// Transfer Len
	cdb[8] = (blocks) & 0xff;
	cdb[9] = 0;						// Control

	ret = scsi_execute_req(sdev, cdb, DMA_TO_DEVICE, buf, (blocks * PART_BLOCK_SIZE), &sshdr, msecs_to_jiffies(15000), 3, NULL);
	if (ret) {
		pr_info("[ufs_ffu] write error %d\n", ret);
	}

	sdev_printk(KERN_ERR, sdev, "sense key:0x%x; asc:0x%x; ascq:0x%x\n", (int)sshdr.sense_key, (int)sshdr.asc, (int)sshdr.ascq);
	scsi_device_put(sdev);
	hba->host->eh_noresume = 0;
	return ret;
}

static int ufs_write(struct scsi_device *sdev, void *buf, uint32_t lba, uint32_t buf_size)
{
	uint32_t transfer_size = 0, block_count = 0;
	void *temp_buffer = buf;
	int rc;

	block_count = (buf_size / PART_BLOCK_SIZE);

	/* Check if LBA plus the total sectors trying to access would exceed the */
	/* total size of the partition */
	if ((lba + block_count) > (part_info.part_start + part_info.part_size)) {
		return -EINVAL;
	}

	while (block_count > 0) {
		transfer_size = (block_count > UFS_MAX_BLOCK_TRANSFERS) ? UFS_MAX_BLOCK_TRANSFERS : block_count;

		rc = ufs_scsi_write_partition(sdev, temp_buffer, lba, transfer_size);

		lba = lba + transfer_size;
		block_count = block_count - transfer_size;
		temp_buffer = temp_buffer + (transfer_size * PART_BLOCK_SIZE) / sizeof(void);

		if (rc != 0) {
			pr_info("[ufs_ffu] write partition error %d\n", rc);
			return 1;
		}
	}
	return 0;
}

/*
* get data from partition from ufs for ffu bin
*
*/
static int ufs_scsi_read_partition(struct scsi_device *sdev, void *buf, uint32_t lba, uint32_t blocks)
{
	uint8_t cdb[16];
	int ret = 0;
	struct ufs_hba *hba = NULL;
	struct scsi_sense_hdr sshdr = {};
	unsigned long flags = 0;

	hba = shost_priv(sdev->host);

	spin_lock_irqsave(hba->host->host_lock, flags);
	ret = scsi_device_get(sdev);
	if (!ret && !scsi_device_online(sdev)) {
		ret = -ENODEV;
		scsi_device_put(sdev);
		pr_info("get device fail\n");
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ret)
		return ret;

	hba->host->eh_noresume = 1;

	// Maximum size of transfer supported by CMD_READ 10 is 16k blocks
	if (blocks > UFS_MAX_BLOCK_TRANSFERS) {
		return -EINVAL;
	}

	// Fill in the CDB with SCSI command structure
	memset (cdb, 0, sizeof(cdb));
	cdb[0] = READ_10;				// Command
	cdb[1] = 0;
	cdb[2] = (lba >> 24) & 0xff;	// LBA
	cdb[3] = (lba >> 16) & 0xff;
	cdb[4] = (lba >> 8) & 0xff;
	cdb[5] = (lba) & 0xff;
	cdb[6] = 0;						// Group Number
	cdb[7] = (blocks >> 8) & 0xff;	// Transfer Len
	cdb[8] = (blocks) & 0xff;
	cdb[9] = 0;						// Control

	ret = scsi_execute_req(sdev, cdb, DMA_FROM_DEVICE, buf, (blocks * PART_BLOCK_SIZE), &sshdr, msecs_to_jiffies(15000), 3, NULL);
	if (ret) {
		pr_info("[ufs_ffu] read error %d\n", ret);
	}

	sdev_printk(KERN_ERR, sdev, "sense key:0x%x; asc:0x%x; ascq:0x%x\n", (int)sshdr.sense_key, (int)sshdr.asc, (int)sshdr.ascq);
	scsi_device_put(sdev);
	hba->host->eh_noresume = 0;
	return ret;
}

static int ufs_read(struct scsi_device *sdev, void *buf, uint32_t lba, uint32_t buf_size)
{
	uint32_t transfer_size = 0, block_count = 0;
	void *temp_buffer = buf;
	int rc;

	block_count = (buf_size / PART_BLOCK_SIZE);

	/* Check if LBA plus the total sectors trying to access would exceed the */
	/* total size of the partition */
	if ((lba + block_count) > (part_info.part_start + part_info.part_size)) {
		return -EINVAL;
	}

	while (block_count > 0) {
		transfer_size = (block_count > UFS_MAX_BLOCK_TRANSFERS) ? UFS_MAX_BLOCK_TRANSFERS : block_count;

		rc = ufs_scsi_read_partition(sdev, temp_buffer, lba, transfer_size);

		lba = lba + transfer_size;
		block_count = block_count - transfer_size;
		temp_buffer = temp_buffer + (transfer_size * PART_BLOCK_SIZE) / sizeof(void);

		if (rc != 0) {
			pr_info("[ufs_ffu] read partition error %d\n", rc);
			return 1;
		}
	}
	return 0;
}

static struct fw_update_checklist need_update_k[] = {
	/* vendor_id[8], product_id[16], product_rev_from[4],
	product_rev_to[4], fw_addr, fw_size; */
	{"SAMSUNG\0", "KLUDG4UHDC-B0E1\0", "0100","0200",
	(uint64_t)SS_128GB_P02, sizeof(SS_128GB_P02)},
	{"SAMSUNG\0", "KLUEG8UHDC-B0E1\0", "0100","0200",
	(uint64_t)SS_256GB_P02, sizeof(SS_256GB_P02)},
	{"SAMSUNG\0", "KLUFG8RHDB-B0E1\0", "0100","0200",
	(uint64_t)SS_512GB_P02, sizeof(SS_512GB_P02)},
	{"SKhynix\0", "HN8T05BZGKX015\0", "A001","A002",
	(uint64_t)SKhynix_A002, sizeof(SKhynix_A002)},
	{"SKhynix\0", "HN8T15BZGKX016\0", "A001","A002",
	(uint64_t)SKhynix_A002, sizeof(SKhynix_A002)},
	{"SKhynix\0", "HN8T25BZGKX017\0", "A001","A002",
	(uint64_t)SKhynix_A002, sizeof(SKhynix_A002)},
	{"SKhynix\0", "HN8T05BZGKX015\0", "A002","A043",
	(uint64_t)SKhynix_A043, sizeof(SKhynix_A043)},
	{"SKhynix\0", "HN8T15BZGKX016\0", "A002","A043",
	(uint64_t)SKhynix_A043, sizeof(SKhynix_A043)},
	{"SKhynix\0", "HN8T25BZGKX017\0", "A002","A043",
	(uint64_t)SKhynix_A043, sizeof(SKhynix_A043)},
	{"WDC", "SDINFDK4-128G", "1260", "1270",
	(uint64_t)WDC_128GB_1270, sizeof(WDC_128GB_1270)},
	{"WDC", "SDINFDK4-256G", "1260", "1270",
	(uint64_t)WDC_128GB_1270, sizeof(WDC_128GB_1270)},
	{"KIOXIA", "THGJFAT1T84BAIR", "1001", "1201",
	(uint64_t)KIOXIA_256GB_1201, sizeof(KIOXIA_256GB_1201)},
	{"KIOXIA", "THGJFAT2T84BAIR", "1001", "1201",
	(uint64_t)KIOXIA_512GB_1201, sizeof(KIOXIA_512GB_1201)},
	{"SKhynix\0", "H9HQ15AECMBDAR\0", "A002","A043",
	(uint64_t)SKhynix_A043_ufs22, sizeof(SKhynix_A043_ufs22)},
	{"SKhynix\0", "H9HQ15AFAMBDAR\0", "A002","A043",
	(uint64_t)SKhynix_A043_ufs22, sizeof(SKhynix_A043_ufs22)},
	{"SKhynix\0", "H9HQ21AFAMADAR\0", "A002","A043",
	(uint64_t)SKhynix_A043_ufs22, sizeof(SKhynix_A043_ufs22)},
	{"SKhynix", "H9QT0G6CN6X146", "A001", "A002",
        (uint64_t)hynix_UFS22_A002, sizeof(hynix_UFS22_A002)},
	{"SKhynix", "H9QT0GECN6X145", "A001", "A002",
        (uint64_t)hynix_UFS22_A002, sizeof(hynix_UFS22_A002)},
	{"SKhynix", "H9QT1G6DN6X132", "A001", "A002",
        (uint64_t)hynix_UFS22_A002, sizeof(hynix_UFS22_A002)},
	{"SAMSUNG\0", "KM8L9001JM-B624", "0300","0700",
	(uint64_t)Samsung_UFS22_128G_0700, sizeof(Samsung_UFS22_128G_0700)},
	{"SAMSUNG\0", "KM8L9001JM-B624", "0100","0700",
	(uint64_t)Samsung_UFS22_128G_0700, sizeof(Samsung_UFS22_128G_0700)},
	{"SAMSUNG\0", "KM8F9001JM-B813", "0100","0700",
	(uint64_t)Samsung_UFS22_256G_0700, sizeof(Samsung_UFS22_256G_0700)},
};

static int check_fw_version(struct fw_update_checklist *fw_info, struct ffu_inquiry *stdinq)
{
	if (strncmp((char *)stdinq->vendor_id, (char *)fw_info->vendor_id, strlen(fw_info->vendor_id))) {
		pr_info("[ufs_ffu] check fw vendor_id not matching %s\n", (char *)fw_info->vendor_id);
		return 1;
	}
	if (strncmp((char *)stdinq->product_id, (char *)fw_info->product_id, strlen(fw_info->product_id))) {
		pr_info("[ufs_ffu] check fw product_id not matching %s\n", (char *)fw_info->product_id);
		return 2;
	}
	if (!strncmp((char *)stdinq->product_rev, (char *)fw_info->product_rev_to, strlen(fw_info->product_rev_to))) {
		pr_info("[ufs_ffu] check fw product_rev_to not matching %s\n", (char *)fw_info->product_rev_to);
		return 3;
	}
	return 0;
}

static int get_fw_update_checklist(struct scsi_device *sdev, struct fw_update_checklist **need_update, struct ffu_inquiry *stdinq)
{
	void* buf = NULL;
	char* fw = NULL;
	int ret = sizeof(need_update_k) / sizeof(need_update_k[0]);
	int rc = 0, i = 0, list_count = 0;
	device_struct part_head;
	struct fw_update_checklist fw_info;

	buf = kmalloc(PART_BLOCK_SIZE, GFP_KERNEL);
	memset(buf, 0, PART_BLOCK_SIZE);

	/*get ffu partition for head part info*/
	rc = ufs_read(sdev, buf, part_info.part_start, PART_BLOCK_SIZE);
	if (rc) {
		pr_info("[ufs_ffu] get partion info error %d\n", rc);
		*need_update = need_update_k;
		goto out;
	}

	memcpy(&part_head, buf, sizeof(part_head));
	if (strncmp(part_head.ffu_flag, FFU_FLAG, sizeof(FFU_FLAG))) {
		pr_info("[ufs_ffu] ufs_flag is not FFU %s\n", part_head.ffu_flag);
		*need_update = need_update_k;
		goto out;
	}

	pr_info("[ufs_ffu] ffu_count is %d\n", part_head.ffu_count);
	if (0 == part_head.ffu_count) {
		*need_update = need_update_k;
		goto out;
	}

	list_count = part_head.ffu_count + ret;

	/*malloc a list need_update for ffu*/
	*need_update = kmalloc(list_count*sizeof(struct fw_update_checklist), GFP_KERNEL);
	memset(*need_update, 0, list_count*sizeof(struct fw_update_checklist));

	/*copy need_update_k list to need_update list*/
	memcpy(*need_update, need_update_k, sizeof(need_update_k));

	/*put info from ffu partition for bin*/
	for (i = 0; i < part_head.ffu_count; i++) {
		int32_t size = 0;
		uint64_t offset = 0;
		unsigned int *crc = NULL;

		memcpy(&fw_info, (buf + FFU_BIN_HEAD_INFO + i *  FFU_BIN_HEAD_INFO), sizeof(struct fw_update_checklist));
		crc = (unsigned int*)(buf + FFU_BIN_HEAD_INFO + i *  FFU_BIN_HEAD_INFO + sizeof(struct fw_update_checklist));
		pr_info("[ufs_ffu] get info from partition crc 0x%lx addr 0x%llx size 0x%x\n", *crc, fw_info.fw_addr, fw_info.fw_size);

		if (!fw_info.fw_addr || fw_info.fw_addr % PART_BLOCK_SIZE) {
			pr_info("[ufs_ffu] ffu bin is not 4K align 0x%llx\n", fw_info.fw_addr);
			continue;
		}

		rc = check_fw_version(&fw_info, stdinq);
		if (rc) {
			continue;
		}

		offset = fw_info.fw_addr / PART_BLOCK_SIZE;
		size = fw_info.fw_size % PART_BLOCK_SIZE ?((fw_info.fw_size / PART_BLOCK_SIZE) + 1) * PART_BLOCK_SIZE : fw_info.fw_size;

		fw = kmalloc(size, GFP_KERNEL);
		memset(fw, 0, size);

		rc = ufs_read(sdev, fw, (part_info.part_start + offset), size);
		if (rc) {
			pr_info("[ufs_ffu] get fw bin info error %d\n", rc);
			continue;
		}

		if (*crc != crc32(fw, fw_info.fw_size)) {
			pr_info("[ufs_ffu] crc error 0x%x diff 0x%x\n", *crc ,crc32(fw, fw_info.fw_size));
			kfree(fw);
			continue;
		}

		fw_info.fw_addr = (uint64_t)fw;
		memcpy((*need_update + ret), &fw_info, sizeof(struct fw_update_checklist));
		ret++;
	}

	printk("[ufs_ffu] partion start %d size %d\n", part_info.part_start, part_info.part_size);
out:
	kfree(buf);
	return ret;
}

static int ufs_scsi_write_buf(struct scsi_device *sdev, uint8_t *buf, uint8_t mode, uint8_t buf_id, int32 offset, uint32 len)
{
	struct ufs_hba *hba = NULL;
	unsigned char cdb[10] = {0};
	struct scsi_sense_hdr sshdr = {};
	unsigned long flags = 0;
	int ret = 0;

	hba = shost_priv(sdev->host);

	spin_lock_irqsave(hba->host->host_lock, flags);
	ret = scsi_device_get(sdev);
	if (!ret && !scsi_device_online(sdev)) {
		ret = -ENODEV;
		scsi_device_put(sdev);
		pr_info("get device fail\n");
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ret)
		return ret;

	hba->host->eh_noresume = 1;

	cdb[0] = WRITE_BUFFER;
	cdb[1] = mode;
	cdb[2] = buf_id;
	cdb[3] = (offset >> 16) & 0xff;
	cdb[4] = (offset >> 8) & 0xff;
	cdb[5] = (offset) & 0xff;
	cdb[6] = (len >> 16) & 0xff;
	cdb[7] = (len >> 8) & 0xff;
	cdb[8] = (len) & 0xff;
	cdb[9] = 0;

	ret = scsi_execute(sdev, cdb, DMA_TO_DEVICE, buf, len, NULL, &sshdr, msecs_to_jiffies(15000), 0, 0, RQF_PM, NULL);

	if (ret) {
		sdev_printk(KERN_ERR, sdev, "WRITE BUFFER failed for firmware upgrade %d\n", ret);
	}

	/*check sense key*/
	sdev_printk(KERN_ERR, sdev, "sense key:0x%x; asc:0x%x; ascq:0x%x\n", (int)sshdr.sense_key, (int)sshdr.asc, (int)sshdr.ascq);

	/* for some devices, device can't response to host after write buffer.
	ret = ufshcd_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR,
			QUERY_ATTR_IDN_DEVICE_FFU_STATUS, 0, 0, &attr);

		if (ret) {
			dev_err(hba->dev, "%s: query bDeviceFFUStatus failed, err %d\n",__func__, ret);
			return -2;
		}

		if (attr > UFS_FFU_STATUS_OK) {
			dev_err(hba->dev, "%s: check bDeviceFFUStatus fail:%d\n",	__func__, attr);
			ret = -1;
			return ret;
		} else {
			dev_err(hba->dev, "%s: check bDeviceFFUStatus pass:%d\n",	__func__, attr);
		}
	*/

	scsi_device_put(sdev);
	hba->host->eh_noresume = 0;
	return ret;
}

#define WB_SIZE (512*1024)
static int32 ufs_fw_update_write(struct scsi_device *sdev, int32_t size, uint8_t *buf)
{
	int32 rc = 0;
	int offset = 0;
	char *buf_ptr = kzalloc(size, GFP_KERNEL);

	if (!buf_ptr) {
		pr_info("ffu kzalloc fail.\n");
		rc = -1;
		return rc;
	}

	memcpy(buf_ptr, buf, size);
	pr_info("fw addr: %p, %p. total fw size: %d\n", buf_ptr, buf, size);

	for (offset = 0; offset + WB_SIZE <= size; offset += WB_SIZE) {
		pr_info("ffu offset:%d, size%d\n", offset, WB_SIZE);
		rc = ufs_scsi_write_buf(sdev, buf_ptr+offset, 0x0e, 0x00, offset, WB_SIZE);
		if (rc)
			goto out;
	}

	if (size % WB_SIZE) {
		pr_info("ffu offset:%d, size%d\n", offset, size%WB_SIZE);
		rc = ufs_scsi_write_buf(sdev, buf_ptr+offset, 0x0e, 0x00, offset, size % WB_SIZE);
	}

out:
	kfree(buf_ptr);
	return rc;
}

static int check_ffu_parition_fw_error(struct scsi_device *sdev, int index)
{
	void *buf = NULL;
	int rc = 0;

	int update_k_size = sizeof(need_update_k) / sizeof(need_update_k[0]);

	buf = kmalloc(PART_BLOCK_SIZE, GFP_KERNEL);
	memset(buf, 0, PART_BLOCK_SIZE);

	if (index >= update_k_size) {
		/*clear ffu parition HEAD*/
		rc = ufs_write(sdev, buf, part_info.part_start, PART_BLOCK_SIZE);
		if (rc) {
			pr_info("[ufs_ffu] write partion info error %d\n", rc);
		}
	}

	kfree(buf);
	return 0;

}

int ufs_ffu(struct scsi_device *sdev)
{
	int ret = 0;
	unsigned int i = 0;
	int fw_update_checklist_size = 0;
	struct ffu_inquiry stdinq = {};
	struct ufs_hba *hba = NULL;
	char *parse_inquiry = NULL;
	struct fw_update_checklist *need_update = NULL;

	if (off_charge_flag) {
		pr_info("ufs_ffu enter off charger\n");
		return 0;
	}

	pr_info("start to FFU 0x%x\n", ffu_get_flag());

	if (one_time_entry) {
		pr_info("[ufs_ffu] one time entry is true\n");
		return 0;
	} else {
		one_time_entry = true;
	}

	if (sdev->host->hostt->proc_name == NULL) {
		ret = -1;
		return ret;
	}
	if (strncmp(sdev->host->hostt->proc_name, UFSHCD, strlen(UFSHCD))) {
		/*check if the device is ufs. If not, just return directly.*/
		ret = -1;
		return ret;
	}

	hba = shost_priv(sdev->host);

	parse_inquiry = hba->sdev_ufs_device->inquiry + 8;  /*be careful about the inquiry formate*/

	memcpy(stdinq.vendor_id, parse_inquiry, INQURIY_VENDOR_ID_SIZE);
	parse_inquiry += INQURIY_VENDOR_ID_SIZE;

	memcpy(stdinq.product_id, parse_inquiry, INQURIY_PRODUCT_ID_SIZE);
	parse_inquiry += INQURIY_PRODUCT_ID_SIZE;

	memcpy(stdinq.product_rev, parse_inquiry, INQURIY_FW_VERSION_SIZE);


	pr_info("[ufs_ffu] check ffu_arry, vendor:%s, product:%s, fw_rev:%s\n", stdinq.vendor_id, stdinq.product_id, stdinq.product_rev);

	/*
	 * get flag from pon register and the flag can prevent enter ffu again
	 * when first ffu fail.
	 *
	 */
	if (qpnp_pon_uvlo_get) {
		if (ffu_get_flag() == 1) {
			pr_info("fail flag was set in last boot. skip FFU.\n");
			ret = ffu_set_flag(0);
			ret = -1;
			return ret;
		}
	} else {
		pr_info("fail flag is not supported\n");
	}

	fw_update_checklist_size = get_fw_update_checklist(sdev, &need_update, &stdinq);

	for (i = 0; i < fw_update_checklist_size; i++) {
		/* step 1. match vendor id && product id */
		if (strncmp((char *)stdinq.vendor_id, (char *)need_update[i].vendor_id, strlen(need_update[i].vendor_id)) == 0
				&& strncmp((char *)stdinq.product_id, (char *)need_update[i].product_id, strlen(need_update[i].product_id)) == 0) {
			pr_info("match vendor:%s, product id:%s hw_sectors %d\n", stdinq.vendor_id, stdinq.product_id,
				queue_max_hw_sectors(sdev->request_queue));

			/*step 2.  match product revision from */
			if (strncmp((char *)stdinq.product_rev, (char *)need_update[i].product_rev_from, strlen(need_update[i].product_rev_from)) == 0) {
				if (qpnp_pon_uvlo_set) {
					/*set error before ffu and clear error after succeed.*/
					ffu_set_flag(1);
				}
				pr_info("match product revision from:%s\n", stdinq.product_rev);
				pr_info("start write buffer\n");
				pr_info("fw_size = %d, address = %p\n", need_update[i].fw_size, need_update[i].fw_addr);
				ret = ufs_fw_update_write(sdev, need_update[i].fw_size, (uint8_t *)need_update[i].fw_addr);
				if (ret == 0) {
					pr_info("write buffer success\n");
					pr_info("clear fail flag, check revision after reboot\n");
					if (qpnp_pon_uvlo_set) {
						ffu_set_flag(0);
					}
					/*kernel_restart don't  work sometimes. use machine_restart and sleep 10ms to print above logs*/
					msleep(20);
					machine_restart("ufs ffu reboot");
				} else {
					/*set a flag at here and do not enter ffu next time.*/
					if (qpnp_pon_uvlo_get) {
						pr_info("ufs ffu failed, don't clear fail flag, reboot directly\n");
						check_ffu_parition_fw_error(sdev, i);
						msleep(20);
						machine_restart("ufs ffu reboot");
					}
					pr_info("ufs ffu failed, do not reboot to avoid endless reboot\n");
				}
				break;

			} else if (strncmp((char *)stdinq.product_rev, (char *)need_update[i].product_rev_to, strlen(need_update[i].product_rev_to)) == 0) {
				/*step 3. match the product version to after reboot.do not use loop just for reminder.*/
				pr_info("ufs ffu firmware is already the newest  now\n");
				continue;
			} else {
				pr_info("ufs_ffu do not match the rev\n");
			}
		} else {
			pr_info("ufs ffu do not match %d:%s %s\n", i, need_update[i].vendor_id, need_update[i].product_id);
		}
	}

	if (need_update != need_update_k) {
		kfree(need_update);
	}
	return ret;
}

static int __init early_parse_off_charge_flag(char *p)
{
	if (p) {
		if (!strcmp(p, "charger"))
			off_charge_flag = true;
		else
			off_charge_flag = false;
	}

	return 0;
}
early_param("androidboot.mode", early_parse_off_charge_flag);

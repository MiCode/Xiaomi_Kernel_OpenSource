#include <linux/blkdev.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/spinlock.h>
#include <linux/reboot.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/efi.h>
#include <linux/rcupdate.h>
#include <linux/of.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_host.h>
#include <linux/completion.h>
#include "../../../../scsi/sd.h"
#include "charger_partition.h"

// extern struct completion ufs_xiaomi_comp;

#define DEFAULT_SDA_PART_NO (10)
struct ChargerPartition {
	struct scsi_device *sdev;
        struct delayed_work charger_partition_work;

	int part_info_part_number;
	char *part_info_part_name;

	//bool is_charger_partition_rdy;
};
static struct ChargerPartition *charger_partition;
static void* rw_buf;
static bool chg_parti_is_rdy = false;

typedef struct part {
	sector_t part_start;
	sector_t part_size;
} partinfo;
static partinfo part_info = { 0 };

static int charger_scsi_read_partition(struct scsi_device *sdev, void *buf, uint32_t lba, uint32_t blocks)
{
	uint8_t cdb[16];
	int ret = 0;
	struct scsi_sense_hdr sshdr = {};
#if 0
	const struct scsi_exec_args exec_args = {
		.sshdr = &sshdr,
	};
#endif
	unsigned long flags = 0;

	spin_lock_irqsave(sdev->host->host_lock, flags);
	ret = scsi_device_get(sdev);
	if (!ret && !scsi_device_online(sdev)) {
		ret = -ENODEV;
		scsi_device_put(sdev);
		pr_err("[charger] %s get device fail\n", __func__);
	}
	spin_unlock_irqrestore(sdev->host->host_lock, flags);
	if (ret) {
                pr_err("[charger] %s 2\n", __func__);
		return ret;
        }
	sdev->host->eh_noresume = 1;

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
#if 0
int scsi_execute_cmd(struct scsi_device *sdev, const unsigned char *cmd, blk_opf_t opf, void *buffer, unsigned int bufflen,
		     int timeout, int retries, const struct scsi_exec_args *args);

extern int __scsi_execute(struct scsi_device *sdev, const unsigned char *cmd, int data_direction, void *buffer, unsigned bufflen,
			unsigned char *sense, struct scsi_sense_hdr *sshdr, int timeout, int retries, u64 flags,
			req_flags_t rq_flags, int *resid);
#endif
	//ret = scsi_execute_cmd(sdev, cdb, REQ_OP_DRV_IN, buf, (blocks * PART_BLOCK_SIZE), msecs_to_jiffies(15000), 3, &exec_args);
	ret = scsi_execute(sdev, cdb, DMA_FROM_DEVICE, buf, (blocks * PART_BLOCK_SIZE),\
				NULL, &sshdr, HZ, 2, 0,\
				0, 0);
	if (ret) {
		pr_err("[charger] %s read error %d\n", __func__, ret);
		return ret;
	}
	scsi_device_put(sdev);
	sdev->host->eh_noresume = 0;
	return ret;
}

static int charger_scsi_write_partition(struct scsi_device *sdev, void *buf, uint32_t lba, uint32_t blocks)
{
	uint8_t cdb[16];
	int ret = 0;
	struct scsi_sense_hdr sshdr = {};
#if 0
	const struct scsi_exec_args exec_args = {
		.sshdr = &sshdr,
	};
#endif
	unsigned long flags = 0;

	spin_lock_irqsave(sdev->host->host_lock, flags);
	ret = scsi_device_get(sdev);
	if (!ret && !scsi_device_online(sdev)) {
		ret = -ENODEV;
		scsi_device_put(sdev);
		pr_err("[charger] %s get device fail\n", __func__);
	}
	spin_unlock_irqrestore(sdev->host->host_lock, flags);

	if (ret) {
                pr_err("[charger] %s get scsi fail\n", __func__);
		return ret;
        }
	sdev->host->eh_noresume = 1;

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
	cdb[9] = 0;					// Control
	//ret = scsi_execute_cmd(sdev, cdb, REQ_OP_DRV_OUT, buf, (blocks * PART_BLOCK_SIZE), msecs_to_jiffies(15000), 3, &exec_args);
	ret = scsi_execute(sdev, cdb, DMA_TO_DEVICE, buf, (blocks * PART_BLOCK_SIZE),\
				NULL, &sshdr, HZ, 2, 0,\
				0, 0);
	if (ret) {
		pr_err("[charger] %s write error %d\n", __func__, ret);
		return ret;
	}
	scsi_device_put(sdev);
	sdev->host->eh_noresume = 0;
	return ret;
}

int charger_partition_alloc(u8 charger_partition_host_type, u8 charger_partition_info_type, uint32_t size)
{
	int ret = 0;

	if(!chg_parti_is_rdy) {
		pr_err("[charger] charger partition not rdy, can't do rw!");
		return -1;
	}
	if(charger_partition_host_type >= CHARGER_PARTITION_HOST_LAST) {
		pr_err("[charger] charger_partition_host_type not support!");
		return -1;
	}
	if(charger_partition_info_type >= CHARGER_PARTITION_INFO_LAST) {
		pr_err("[charger] charger_partition_info_type not support!");
		return -1;
	}
	if(size >= CHARGER_PARTITION_RWSIZE) {
		pr_err("[charger] read %u size not support!", size);
		return -1;
	}

	rw_buf = kzalloc(CHARGER_PARTITION_RWSIZE, GFP_KERNEL);
	if(!rw_buf){
		pr_err("[charger] malloc buf error!");
		return -1;
	}

	/* check if avaliable */
	ret = charger_scsi_read_partition(charger_partition->sdev, rw_buf, (part_info.part_start + CHARGER_PARTITION_HEADER), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
	if (ret) {
		pr_err("[charger] %s charger read error %d\n", __func__, ret);
		kfree(rw_buf);
		return -1;
	}
	pr_err("[charger] %s avaliable:%u\n", __func__, ((charger_partition_header *)rw_buf)->avaliable);
	if(0 == ((charger_partition_header *)rw_buf)->avaliable) {
		pr_err("[charger] %s not avaliable, can't do rw now!\n", __func__);
		kfree(rw_buf);
		return -1;
	}
	((charger_partition_header *)rw_buf)->avaliable = 0;
	pr_err("[charger] %s set avaliable:%u\n", __func__, ((charger_partition_header *)rw_buf)->avaliable);
	ret = charger_scsi_write_partition(charger_partition->sdev, (void *)rw_buf, (part_info.part_start + CHARGER_PARTITION_HEADER), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
	if (ret) {
		pr_err("[charger] %s charger write error %d\n", __func__, ret);
		kfree(rw_buf);
		return -1;
	}

	return ret;
}
EXPORT_SYMBOL(charger_partition_alloc);

int charger_partition_dealloc(u8 charger_partition_host_type, u8 charger_partition_info_type, uint32_t size)
{
	int ret = 0;

	if(!chg_parti_is_rdy) {
		pr_err("[charger] charger partition not rdy, can't do rw!");
		return -1;
	}
	if(charger_partition_host_type >= CHARGER_PARTITION_HOST_LAST) {
		pr_err("[charger] charger_partition_host_type not support!");
		return -1;
	}
	if(charger_partition_info_type >= CHARGER_PARTITION_INFO_LAST) {
		pr_err("[charger] charger_partition_info_type not support!");
		return -1;
	}
	if(size >= CHARGER_PARTITION_RWSIZE) {
		pr_err("[charger] read %u size not support!", size);
		return -1;
	}

	memset(rw_buf, 0, CHARGER_PARTITION_RWSIZE);
	ret = charger_scsi_read_partition(charger_partition->sdev, rw_buf, (part_info.part_start + CHARGER_PARTITION_HEADER), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
	if (ret) {
		pr_err("[charger] %s charger read error %d\n", __func__, ret);
		kfree(rw_buf);
		return -1;
	}
	((charger_partition_header *)rw_buf)->avaliable = 1;
	pr_err("[charger] %s set avaliable:%u\n", __func__, ((charger_partition_header *)rw_buf)->avaliable);
	ret = charger_scsi_write_partition(charger_partition->sdev, (void *)rw_buf, (part_info.part_start + CHARGER_PARTITION_HEADER), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
	if (ret) {
		pr_err("[charger] %s charger write error %d\n", __func__, ret);
		kfree(rw_buf);
		return -1;
	}

	kfree(rw_buf);
	return ret;
}
EXPORT_SYMBOL(charger_partition_dealloc);

void *charger_partition_read(u8 charger_partition_host_type, u8 charger_partition_info_type, uint32_t size)
{
	int ret = 0;

	if(!chg_parti_is_rdy) {
		pr_err("[charger] charger partition not rdy, can't do read!");
		return NULL;
	}
	if(!rw_buf) {
		pr_err("[charger] rw_buf null, please alloc first!");
		return NULL;
	}
	if(charger_partition_host_type >= CHARGER_PARTITION_HOST_LAST) {
		pr_err("[charger] charger_partition_host_type not support!");
		return NULL;
	}
	if(charger_partition_info_type >= CHARGER_PARTITION_INFO_LAST) {
		pr_err("[charger] charger_partition_info_type not support!");
		return NULL;
	}
	if(size >= CHARGER_PARTITION_RWSIZE) {
		pr_err("[charger] read %u size not support!", size);
		return NULL;
	}

	memset(rw_buf, 0, CHARGER_PARTITION_RWSIZE);
	ret = charger_scsi_read_partition(charger_partition->sdev, rw_buf, (part_info.part_start + charger_partition_info_type), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
	if (ret) {
		pr_err("[charger] %s charger read error %d\n", __func__, ret);
		return NULL;
	}
	return rw_buf;
}
EXPORT_SYMBOL(charger_partition_read);

int charger_partition_write(u8 charger_partition_host_type, u8 charger_partition_info_type, void *buf, uint32_t size)
{
	int ret = 0;
	uint32_t offset;

	if(!chg_parti_is_rdy) {
		pr_err("[charger] charger partition not rdy, can't do read!");
		return -1;
	}
	if(!rw_buf) {
		pr_err("[charger] rw_buf null, please alloc first!");
		return -1;
	}
	if(charger_partition_host_type >= CHARGER_PARTITION_HOST_LAST) {
		pr_err("[charger] charger_partition_host_type not support!");
		return -1;
	}
	if(charger_partition_info_type >= CHARGER_PARTITION_INFO_LAST) {
		pr_err("[charger] charger_partition_info_type not support!");
		return -1;
	}
	if(size >= CHARGER_PARTITION_RWSIZE) {
		pr_err("[charger] read %u size not support!", size);
		return -1;
	}

	memset(rw_buf, 0, CHARGER_PARTITION_RWSIZE);
	ret = charger_scsi_read_partition(charger_partition->sdev, rw_buf, (part_info.part_start + charger_partition_info_type), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
	if (ret) {
		pr_err("[charger] %s charger read error %d\n", __func__, ret);
		return -1;
	}
	memcpy(rw_buf, buf, size);

	ret = charger_scsi_write_partition(charger_partition->sdev, rw_buf, (part_info.part_start + charger_partition_info_type), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
	if (ret) {
		pr_err("[charger] %s charger write error %d\n", __func__, ret);
		return -1;
	}

	offset = part_info.part_start + charger_partition_info_type;
	pr_err("[charger] %s: start: %llu, info_type:%d, offset:%llu \n", __func__, part_info.part_start, charger_partition_info_type, offset);
	return ret;
}
EXPORT_SYMBOL(charger_partition_write);

static bool look_up_scsi_device(int lun)
{
	struct Scsi_Host *shost;

	shost = scsi_host_lookup(0);
	if (!shost)
		return false;

	charger_partition->sdev = scsi_device_lookup(shost, 0, 0, lun);
	if (!charger_partition->sdev)
		return false;

	scsi_host_put(shost);
	return true;
}

static bool check_device_is_correct(void)
{
	pr_err("[charger] %s proc name is %s", __func__, charger_partition->sdev->host->hostt->proc_name);
	if (strncmp(charger_partition->sdev->host->hostt->proc_name, UFSHCD, strlen(UFSHCD))) {
		/*check if the device is ufs. If not, just return directly.*/
                pr_err("[charger] %s proc name is not ufshcd, name: %s", __func__, charger_partition->sdev->host->hostt->proc_name);
		return false;
	}

	return true;
}

static bool get_charger_partition_info(void)
{
	struct scsi_disk *sdkp = NULL;
	struct scsi_device *sdev = charger_partition->sdev;
	int part_number = charger_partition->part_info_part_number;
	struct block_device *part;

	if (!sdev->sdev_gendev.driver_data) {
		pr_err("[charger] %s scsi disk is null\n", __func__);
		return false;
	}

	sdkp = (struct scsi_disk *)sdev->sdev_gendev.driver_data;
	if (!sdkp->disk) {
		pr_err("[charger] %s gendisk is null\n", __func__);
		return false;
	}

	charger_partition->part_info_part_name = sdkp->disk->disk_name;
	pr_err("[charger] %s partion: %s-%d \n",
			__func__, charger_partition->part_info_part_name, part_number);

	part = xa_load(&sdkp->disk->part_tbl, part_number);
	if (!part) {
		pr_err("[charger] %s device is null\n", __func__);
		return false;
	}

	part_info.part_start = part->bd_start_sect * PART_SECTOR_SIZE / PART_BLOCK_SIZE;
	part_info.part_size = bdev_nr_sectors(part) * PART_SECTOR_SIZE / PART_BLOCK_SIZE;

	pr_err("[charger] %s partion: %s start %llu(block) size %llu(block)\n",
			__func__, charger_partition->part_info_part_name, part_info.part_start, part_info.part_size);
	return true;
}

int get_hwc_from_cmdline(char *hwc)
{
    struct device_node *of_chosen = NULL;
    char *bootargs = NULL;
    char androidboot[24];
    char *phwc = NULL;

	of_chosen = of_find_node_by_path("/chosen");
	if (of_chosen) {
        bootargs = (char *)of_get_property(of_chosen, "bootargs", NULL);
		if (!bootargs) {
			pr_err("%s search bootargs property failed\n", __func__);
		} else {
            phwc = strstr(bootargs, "androidboot.hwc");
            if(phwc){
                memset(androidboot, 0, sizeof(androidboot));
                strncpy(androidboot, phwc, sizeof(androidboot));
                sscanf(androidboot, "androidboot.hwc=%s", hwc);
                pr_err("hwc is %s \n", *hwc);
            }else{
                pr_err("search androidboot.hwc failed\n");
                return -EINVAL;
            }
        }
    }else{
        pr_err("search chosen node failed\n");
        return -EINVAL;
    }

	return 0;
}

int check_charger_partition_header(void)
{
	int ret = 0;
	charger_partition_header *header = NULL;

	header = kzalloc(CHARGER_PARTITION_RWSIZE, GFP_KERNEL);
	if(!header){
		pr_err("[charger] malloc buf error!");
		return -1;
	}

	ret = charger_scsi_read_partition(charger_partition->sdev, (void *)header, (part_info.part_start + CHARGER_PARTITION_HEADER), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
	if (ret) {
		pr_err("[charger] %s charger read error %d\n", __func__, ret);
		kfree(header);
		return -1;
	}
	pr_err("[charger] %s magic:0x%0x, version:%d\n", __func__, header->magic, header->version);

	if(header->magic != CHARGER_PARTITION_MAGIC) {
		pr_err("[charger] %s magic error, set to default!\n", __func__);
		header->magic = CHARGER_PARTITION_MAGIC;
	}
	header->initialized = 1;
	header->avaliable = 1;
	pr_err("[charger] %s initiablized ok\n", __func__);

	ret = charger_scsi_write_partition(charger_partition->sdev, (void *)header, (part_info.part_start + CHARGER_PARTITION_HEADER), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
	if (ret) {
		pr_err("[charger] %s charger write error %d\n", __func__, ret);
		kfree(header);
		return -1;
	}

	ret = charger_scsi_read_partition(charger_partition->sdev, (void *)header, (part_info.part_start + CHARGER_PARTITION_HEADER), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
	if (ret) {
		pr_err("[charger] %s charger read error %d\n", __func__, ret);
		kfree(header);
		return -1;
	}
	pr_err("[charger] %s magic:0x%0x, version:%d\n", __func__, header->magic, header->version);

	kfree(header);
	return ret;
}

int get_charger_partition_info_1(void)
{
	int ret = 0;
	charger_partition_info_1 *info_1 = NULL;

	ret = charger_partition_alloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
	if(ret < 0) {
		pr_err("[charger] %s failed to alloc\n", __func__);
		return -1;
	}

	info_1 = (charger_partition_info_1 *)charger_partition_read(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
	if(!info_1) {
		pr_err("[charger] %s failed to read\n", __func__);
		ret = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
		if(ret < 0) {
			pr_err("[charger] %s failed to dealloc\n", __func__);
			return -1;
		}
		return -1;
	}
	pr_err("[charger] %s ret: %d, info_1->test: 0x%0x, zero_speed_mode: %u, power_off_mode: %u\n", __func__, ret, info_1->test, info_1->zero_speed_mode, info_1->power_off_mode);

	ret = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
	if(ret < 0) {
		pr_err("[charger] %s failed to dealloc\n", __func__);
		return -1;
	}
	return 0;
}

int set_charger_partition_info_1(void)
{
	int ret = 0;
	charger_partition_info_1 info_1 = {.power_off_mode = 2, .zero_speed_mode = 2, .test = 0x23456789, .reserved = 0};

	ret = charger_partition_alloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
	if(ret < 0) {
		pr_err("[charger] %s failed to alloc\n", __func__);
		return -1;
	}

	ret = charger_partition_write(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, (void *)&info_1, sizeof(charger_partition_info_1));
	if(ret < 0) {
		pr_err("[charger] %s failed to write\n", __func__);
		ret = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
		if(ret < 0) {
			pr_err("[charger] %s failed to dealloc\n", __func__);
			return -1;
		}
		return -1;
	}

	ret = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
	if(ret < 0) {
		pr_err("[charger] %s failed to dealloc\n", __func__);
		return -1;
	}
	return 0;
}

int charger_partition_get_prop(int type, int *val);
static int eu_mode = -EINVAL;
static void charger_partition_work(struct work_struct *work)
{
	int lun = 0, ret = 0;
	char HWC[16] = { 0 };

	ret = get_hwc_from_cmdline(HWC);
	if (!ret) {
		pr_err("[charger] %s get HWC success: %s\n", __func__, HWC);
		if(!strncmp(HWC, "CN", 2))
		{
			pr_err("[charger] %s CN machine \n", __func__);
			charger_partition->part_info_part_number = DEFAULT_SDA_PART_NO;
		}else if(!strncmp(HWC, "India", 5)){
			pr_err("[charger] %s Indial machine \n", __func__);
			charger_partition->part_info_part_number = DEFAULT_SDA_PART_NO;
		}else if(!strncmp(HWC, "Global", 5)){
			pr_err("[charger] %s Global machine %d \n", __func__,charger_partition->part_info_part_number);
			charger_partition->part_info_part_number = DEFAULT_SDA_PART_NO;
		}else {
			pr_err("[charger] %s other machine \n", __func__);
			charger_partition->part_info_part_number = DEFAULT_SDA_PART_NO;
		}
	}else{
        pr_err("[charger] %s get HWC failed, use defalut SDA part_no\n", __func__);
        charger_partition->part_info_part_number = DEFAULT_SDA_PART_NO;
    }

    //1. find charger partition
    for (lun = 0; lun < 6; lun++) {
	    /*find charger partition scsi device*/
	    if (!look_up_scsi_device(lun)) {
		    pr_err("[charger] %s not find, continue...\n", __func__);
		    continue;
	    }

	    /*check device is correct*/
	    if (!check_device_is_correct()) {
		    pr_err("[charger] %s not find finally, won't read charger partition!!!\n",
			   __func__);
		    return;
	    }

		if (get_charger_partition_info()) {
			pr_err("[charger] %s get partition info ok\n", __func__);
			chg_parti_is_rdy = true;
			check_charger_partition_header();
			/*reset info_1: power_off_mode and zero_speed_mode*/
			get_charger_partition_info_1();
			set_charger_partition_info_1();
			get_charger_partition_info_1();
			ret = charger_partition_get_prop(CHARGER_PARTITION_PROP_EU_MODE, &eu_mode);
			if(ret < 0){
				pr_err("%s: get prop failed, ret = %d \n", __func__, ret);
				return;
			}
			break;
		}
	}

}

int charger_partition_set_prop(int type, int val)
{
    int rc = 0;
    charger_partition_info_2 info_2 = {.eu_model = 0, .test = 0, .reserved = 0};

	if(!chg_parti_is_rdy) {
		pr_err("[charger] charger partition not rdy, can't set prop!");
		return -EINVAL;
	}

	rc = charger_partition_alloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_2, sizeof(charger_partition_info_2));
	if(rc < 0) {
		pr_err("[charger] %s failed to alloc\n", __func__);
		return -EINVAL;
	}

    pr_err("[charger] %s: set %d to %u \n", __func__, type, val);

    switch (type)
    {
    case CHARGER_PARTITION_PROP_TEST_MODE:
        info_2.test = val;
        break;
    case CHARGER_PARTITION_PROP_EU_MODE:
        info_2.eu_model = val;
        break;
    default:
        pr_err("[charger] %s unsupport handle type!\n", __func__);
        break;
    }

	rc = charger_partition_write(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_2, (void *)&info_2, sizeof(charger_partition_info_2));
	if(rc < 0) {
		pr_err("[charger] %s failed to write\n", __func__);
		rc = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_2, sizeof(charger_partition_info_2));
		if(rc < 0) {
			pr_err("[charger] %s failed to dealloc\n", __func__);
			return -EINVAL;
		}
		return -EINVAL;
	}

	pr_info("[charger] %s last_eu_mode:%d, current_eu_mode:%d \n", __func__, eu_mode, info_2.eu_model);
	if(eu_mode != info_2.eu_model){
		pr_err("[charger] %s eu_mode is changed, update it to charge driver \n", __func__);
		schedule_delayed_work(&charger_partition->charger_partition_work, msecs_to_jiffies(100));//100ms
		eu_mode = info_2.eu_model;
	}else{
		cancel_delayed_work(&charger_partition->charger_partition_work);
	}

	rc = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_2, sizeof(charger_partition_info_2));
	if(rc < 0) {
		pr_err("[charger] %s failed to dealloc\n", __func__);
		return -EINVAL;
	}

    return 0;
}
EXPORT_SYMBOL(charger_partition_set_prop);

int charger_partition_get_prop(int type, int *val)
{
	int rc = 0;
	charger_partition_info_2 *info_2 = NULL;

	if(!chg_parti_is_rdy) {
		pr_err("[charger] charger partition not rdy, can't get prop!");
		return -EINVAL;
	}

	rc = charger_partition_alloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_2, sizeof(charger_partition_info_2));
	if(rc < 0) {
		pr_err("[charger] %s failed to alloc\n", __func__);
		return -EINVAL;
	}

	info_2 = (charger_partition_info_2 *)charger_partition_read(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_2, sizeof(charger_partition_info_2));
	if(!info_2) {
		pr_err("[charger] %s failed to read\n", __func__);
		rc = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_2, sizeof(charger_partition_info_2));
		if(rc < 0) {
			pr_err("[charger] %s failed to dealloc\n", __func__);
			return -EINVAL;
		}
		return -EINVAL;
	}

    switch (type)
    {
    case CHARGER_PARTITION_PROP_TEST_MODE:
        *val = (int)info_2->test;
        break;
    case CHARGER_PARTITION_PROP_EU_MODE:
        *val = (int)info_2->eu_model;
        break;
    default:
        pr_err("[charger] %s unsupport handle type!\n", __func__);
        break;
    }

    pr_err("[charger] %s: %d is %d \n", __func__, type, *val);

	rc = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_2, sizeof(charger_partition_info_2));
	if(rc < 0) {
		pr_err("[charger] %s failed to dealloc\n", __func__);
		return -EINVAL;
	}

    return 0;
}
EXPORT_SYMBOL(charger_partition_get_prop);
int get_eu_mode(void)
{
	pr_debug("%s: eu_mode:%d !", __func__, eu_mode);
	return eu_mode;
}
EXPORT_SYMBOL(get_eu_mode);

int charger_partition_info1_set_prop(int type, int val)
{
    int ret = 0;
	charger_partition_info_1 info_1 = {.power_off_mode = 2, .zero_speed_mode = 2, .test = 0x34567890, .reserved = 0};
	if(!chg_parti_is_rdy) {
		pr_err("[charger] charger partition not rdy, can't set prop!");
		return -EINVAL;
	}

	ret = charger_partition_alloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
	if(ret < 0) {
		pr_err("[charger] %s failed to alloc\n", __func__);
		return -EINVAL;
	}

    pr_err("[charger] %s: set %d to %u \n", __func__, type, val);

    switch (type)
    {
    case CHARGER_PARTITION_PROP_INFO1_TEST_MODE:
        info_1.test = val;
        break;
    case CHARGER_PARTITION_PROP_POWER_OFF_MODE:
        info_1.power_off_mode = val;
        break;
    default:
        pr_err("[charger] %s unsupport handle type!\n", __func__);
        break;
    }

	ret = charger_partition_write(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, (void *)&info_1, sizeof(charger_partition_info_1));
	if(ret < 0) {
		pr_err("[charger] %s failed to write\n", __func__);
		ret = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
		if(ret < 0) {
			pr_err("[charger] %s failed to dealloc\n", __func__);
			return -EINVAL;
		}
		return -EINVAL;
	}

	pr_err("[charger] %s ret: %d, info_1.power_off_mode: %u, info_1.test: 0x%0x\n", __func__, ret, info_1.power_off_mode, info_1.test);
	ret = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
	if(ret < 0) {
		pr_err("[charger] %s failed to dealloc\n", __func__);
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL(charger_partition_info1_set_prop);

int charger_partition_info1_get_prop(int type, int *val)
{
	int ret = 0;
	charger_partition_info_1 *info_1 = NULL;

	if(!chg_parti_is_rdy) {
		pr_err("[charger] charger partition not rdy, can't get prop!");
		return -EINVAL;
	}

	ret = charger_partition_alloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
	if(ret < 0) {
		pr_err("[charger] %s failed to alloc\n", __func__);
		return -EINVAL;
	}

	info_1 = (charger_partition_info_1 *)charger_partition_read(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
	if(!info_1) {
		pr_err("[charger] %s failed to read\n", __func__);
		ret = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
		if(ret < 0) {
			pr_err("[charger] %s failed to dealloc\n", __func__);
			return -EINVAL;
		}
		return -EINVAL;
	}

    switch (type)
    {
    case CHARGER_PARTITION_PROP_INFO1_TEST_MODE:
        *val = (int)info_1->test;
        break;
    case CHARGER_PARTITION_PROP_POWER_OFF_MODE:
        *val = (int)info_1->power_off_mode;
        break;
    default:
        pr_err("[charger] %s unsupport handle type!\n", __func__);
        break;
    }

    pr_err("[charger] %s: %d is %d \n", __func__, type, *val);

	ret = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
	if(ret < 0) {
		pr_err("[charger] %s failed to dealloc\n", __func__);
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL(charger_partition_info1_get_prop);

static int __init charger_partition_init(void)
{
	charger_partition = (struct ChargerPartition *)kzalloc(sizeof(struct ChargerPartition), GFP_KERNEL);
	pr_err("%s:init!", __func__);
	INIT_DELAYED_WORK(&charger_partition->charger_partition_work, charger_partition_work);
	schedule_delayed_work(&charger_partition->charger_partition_work, msecs_to_jiffies(CHARGER_WORK_DELAY_MS));
	return 0;
}
device_initcall(charger_partition_init);

static void __exit charger_partition_exit(void)
{
	chg_parti_is_rdy = false;
	cancel_delayed_work(&charger_partition->charger_partition_work);
	kfree(charger_partition);

	pr_err("%s:exit!", __func__);
}
module_exit(charger_partition_exit);

MODULE_AUTHOR("Binghe Wang");
MODULE_DESCRIPTION("LX charge partition");
MODULE_LICENSE("GPL");
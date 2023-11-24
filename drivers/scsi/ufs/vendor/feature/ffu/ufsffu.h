#ifndef __MI_UFS_FFU__
#define __MI_UFS_FFU__

typedef unsigned int uint32;
typedef  int int32;
typedef unsigned short CHAR16;

#define INQURIY_VENDOR_ID_SIZE		8
#define INQURIY_PRODUCT_ID_SIZE		16
#define INQURIY_FW_VERSION_SIZE		4

#define FFU_WORK_DELAY_MS 			200

#define PART_SECTOR_SIZE			(0x200)
#define PART_BLOCK_SIZE				(0x1000)
#define FFU_PART_IN_LUN				(2)
#define FFU_PART_NAME				"sdc"
#define FFU_PART_NUMBER 			(61)		//sdc61 --> ffu
#define UFS_MAX_BLOCK_TRANSFERS		(128)
#define FFU_BIN_HEAD_INFO			(0x100)
#define FFU_FLAG 					"ffu"
#define UFS_FFU_BOOTREASON			(0x41)

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

struct ffu_data {
	struct delayed_work ffu_delayed_work;
	struct scsi_device *sdev;

	int part_info_part_number;
	char *part_info_part_name;

	u32 bootreason;
	u32 batterstatus;

	bool off_charge_flag;
	bool one_time_entry;
};

static partinfo part_info = { 0 };

extern int ufs_ffu_reboot_reason_reboot(void *ptr);

void ufs_ffu(void);
#endif

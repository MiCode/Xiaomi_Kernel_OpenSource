/*
 *  Copyright (c) 2016 MediaTek Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _BTMTK_SDIO_H_
#define _BTMTK_SDIO_H_
#include "btmtk_config.h"
#include <linux/pm_wakeup.h>

#define VERSION "v0.0.1.13_2020101001"

#define SDIO_HEADER_LEN				4
#define STP_HEADER_LEN				4
#define COREDUMP_HEADER_LEN			5
#define HCI_TYPE_LEN				1
#define HCI_EVENT_CODE_LEN			1
#define COREDUMP_PACKET_HEADER_LEN		13

#define BD_ADDRESS_SIZE 6

#define SET_POWER_NUM 3

#define DUMP_HCI_LOG_FILE_NAME          "/sys/hcilog"
/* SD block size can not bigger than 64 due to buf size limit in firmware */
/* define SD block size for data Tx/Rx */
#define SDIO_BLOCK_SIZE                 256

#define SDIO_PATCH_DOWNLOAD_FIRST    1/*first packet*/
#define SDIO_PATCH_DOWNLOAD_CON        2/*continue*/
#define SDIO_PATCH_DOWNLOAD_END        3/*end*/

/* Number of blocks for firmware transfer */
#define FIRMWARE_TRANSFER_NBLOCK        2

/* This is for firmware specific length */
#define FW_EXTRA_LEN                    36

#define MRVDRV_SIZE_OF_CMD_BUFFER       (2 * 1024)

#define MRVDRV_BT_RX_PACKET_BUFFER_SIZE \
					(HCI_MAX_FRAME_SIZE + FW_EXTRA_LEN)

#define ALLOC_BUF_SIZE  (((max_t (int, MRVDRV_BT_RX_PACKET_BUFFER_SIZE, \
				MRVDRV_SIZE_OF_CMD_BUFFER) + SDIO_HEADER_LEN \
				+ SDIO_BLOCK_SIZE - 1) / SDIO_BLOCK_SIZE) \
				* SDIO_BLOCK_SIZE)

/* The number of times to try when polling for status */
#define MAX_POLL_TRIES                  100

/* Max retry number of CMD53 write */
#define MAX_WRITE_IOMEM_RETRY           2

/* register bitmasks */
#define HOST_POWER_UP                           BIT(1)
#define HOST_CMD53_FIN                          BIT(2)

#define HIM_DISABLE                             0xff
#define HIM_ENABLE                              (BIT(0) | BIT(1))

#define UP_LD_HOST_INT_STATUS                   BIT(0)
#define DN_LD_HOST_INT_STATUS                   BIT(1)

#define DN_LD_CARD_RDY                          BIT(0)
#define CARD_IO_READY                           BIT(3)

#define FIRMWARE_READY                          0xfedc

enum {
	BTMTK_FOPS_STATE_UNKNOWN,	/* deinit in stpbt destroy */
	BTMTK_FOPS_STATE_INIT,		/* init in stpbt created */
	BTMTK_FOPS_STATE_OPENING,	/* during opening */
	BTMTK_FOPS_STATE_OPENED,	/* opened */
	BTMTK_FOPS_STATE_CLOSING,	/* during closing */
	BTMTK_FOPS_STATE_CLOSED,	/* closed */
	BTMTK_FOPS_STATE_MAX
};

struct btmtk_sdio_card_reg {
	u8 cfg;
	u8 host_int_mask;
	u8 host_intstatus;
	u8 card_status;
	u8 sq_read_base_addr_a0;
	u8 sq_read_base_addr_a1;
	u8 card_revision;
	u8 card_fw_status0;
	u8 card_fw_status1;
	u8 card_rx_len;
	u8 card_rx_unit;
	u8 io_port_0;
	u8 io_port_1;
	u8 io_port_2;
	bool int_read_to_clear;
	u8 host_int_rsr;
	u8 card_misc_cfg;
	u8 fw_dump_ctrl;
	u8 fw_dump_start;
	u8 fw_dump_end;
	u8 func_num;
	u32 chip_id;
};
#if SUPPORT_MT7668
#define WOBLE_SETTING_FILE_NAME_7668 "woble_setting_7668.bin"
#endif

#if SUPPORT_MT7663
#define WOBLE_SETTING_FILE_NAME_7663 "woble_setting_7663.bin"
#endif

/* Backward compatibility */
#define WOBLE_SETTING_FILE_NAME "woble_setting.bin"
#define BT_CFG_NAME "bt.cfg"
#define BT_UNIFY_WOBLE "SUPPORT_UNIFY_WOBLE"
#define BT_LEGACY_WOBLE "SUPPORT_LEGACY_WOBLE"
#define BT_WOBLE_BY_EINT "SUPPORT_WOBLE_BY_EINT"
#define BT_DONGLE_RESET_PIN "BT_DONGLE_RESET_GPIO_PIN"
#define BT_SAVE_FW_DUMP_IN_KERNEL "SAVE_FW_DUMP_IN_KERNEL"
#define BT_SYS_LOG_FILE "SYS_LOG_FILE_NAME"
#define BT_FW_DUMP_FILE "FW_DUMP_FILE_NAME"
#define BT_RESET_DONGLE "SUPPORT_DONGLE_RESET"
#define BT_FULL_FW_DUMP "SUPPORT_FULL_FW_DUMP"
#define BT_WOBLE_WAKELOCK "SUPPORT_WOBLE_WAKELOCK"
#define BT_WOBLE_FOR_BT_DISABLE "SUPPORT_WOBLE_FOR_BT_DISABLE"
#define BT_RESET_STACK_AFTER_WOBLE "RESET_STACK_AFTER_WOBLE"
#define BT_AUTO_PICUS "SUPPORT_AUTO_PICUS"
#define BT_AUTO_PICUS_FILTER "PICUS_FILTER_CMD"
#define BT_WMT_CMD "WMT_CMD"
#define BT_VENDOR_CMD "VENDOR_CMD"

#define WMT_CMD_COUNT 255
#define VENDOR_CMD_COUNT 255

#define WOBLE_SETTING_COUNT 10

#define WOBLE_FAIL -10

enum bt_sdio_dongle_state {
	BT_SDIO_DONGLE_STATE_UNKNOWN,
	BT_SDIO_DONGLE_STATE_POWER_ON,
	BT_SDIO_DONGLE_STATE_POWER_ON_FOR_WOBLE,
	BT_SDIO_DONGLE_STATE_POWER_OFF,
	BT_SDIO_DONGLE_STATE_WOBLE,
	BT_SDIO_DONGLE_STATE_FW_DUMP,
	BT_SDIO_DONGLE_STATE_ERROR
};

enum fw_cfg_index_len {
	FW_CFG_INX_LEN_NONE = 0,
	FW_CFG_INX_LEN_2 = 2,
	FW_CFG_INX_LEN_3 = 3,
};

struct fw_cfg_struct {
	char	*content;	/* APCF conecnt or radio off content */
	int	length;		/* APCF conecnt or radio off content of length */
};

struct bt_cfg_struct {
	bool	support_unify_woble;	/* support unify woble or not */
	bool	support_legacy_woble;		/* support legacy woble or not */
	bool	support_woble_by_eint;		/* support woble by eint or not */
	bool	save_fw_dump_in_kernel;		/* save fw dump in kernel or not */
	bool	support_dongle_reset;		/* support chip reset or not */
	bool	support_full_fw_dump;		/* dump full fw coredump or not */
	bool	support_woble_wakelock;		/* support when woble error, do wakelock or not */
	bool	support_woble_for_bt_disable;		/* when bt disable, support enter susend or not */
	bool	reset_stack_after_woble;	/* support reset stack to re-connect IOT after resume */
	bool	support_auto_picus;			/* support enable PICUS automatically */
	struct fw_cfg_struct picus_filter;	/* support on PICUS filter command customization */
	int	dongle_reset_gpio_pin;		/* BT_DONGLE_RESET_GPIO_PIN number */
	char	*sys_log_file_name;
	char	*fw_dump_file_name;
	struct fw_cfg_struct wmt_cmd[WMT_CMD_COUNT];
	struct fw_cfg_struct vendor_cmd[VENDOR_CMD_COUNT];
};

struct btmtk_sdio_card {
	struct sdio_func *func;
	u32 ioport;
	const char *helper;
	const struct btmtk_sdio_card_reg *reg;
	bool support_pscan_win_report;
	bool supports_fw_dump;
	u16 sd_blksz_fw_dl;
	u8 rx_unit;
	bool is_KeepFullPwr;
	struct btmtk_private *priv;

	unsigned char		*woble_setting_file_name;

	unsigned int		chip_id;
	struct fw_cfg_struct		woble_setting_apcf[WOBLE_SETTING_COUNT];
	struct fw_cfg_struct		woble_setting_apcf_fill_mac[WOBLE_SETTING_COUNT];
	struct fw_cfg_struct		woble_setting_apcf_fill_mac_location[WOBLE_SETTING_COUNT];

	struct fw_cfg_struct		woble_setting_radio_off[WOBLE_SETTING_COUNT];
	struct fw_cfg_struct		woble_setting_radio_off_status_event[WOBLE_SETTING_COUNT];
	/* complete event */
	struct fw_cfg_struct		woble_setting_radio_off_comp_event[WOBLE_SETTING_COUNT];

	struct fw_cfg_struct		woble_setting_radio_on[WOBLE_SETTING_COUNT];
	struct fw_cfg_struct		woble_setting_radio_on_status_event[WOBLE_SETTING_COUNT];
	struct fw_cfg_struct		woble_setting_radio_on_comp_event[WOBLE_SETTING_COUNT];

	int		suspend_count;
	/* set apcf after resume(radio on) */
	struct fw_cfg_struct		woble_setting_apcf_resume[WOBLE_SETTING_COUNT];
	struct fw_cfg_struct		woble_setting_apcf_resume_event[WOBLE_SETTING_COUNT];
	unsigned char					bdaddr[BD_ADDRESS_SIZE];
	unsigned int					woble_need_trigger_coredump;
	unsigned char		*bt_cfg_file_name;
	unsigned char		*setting_file;
	struct bt_cfg_struct		bt_cfg;
	struct		wakeup_source	*woble_ws;
	struct		wakeup_source	*eint_ws;

	/* WoBLE */
	unsigned int wobt_irq;
	int wobt_irqlevel;
	atomic_t irq_enable_count;
	struct input_dev *WoBLEInputDev;

	int pa_setting;
	int duplex_setting;
	u8 *bin_file_buffer;
	size_t bin_file_size;
	u8 efuse_mode;

	struct sk_buff_head tx_queue;
	struct sk_buff_head fops_queue;
	struct sk_buff_head fwlog_fops_queue;

	enum bt_sdio_dongle_state dongle_state;
};

struct btmtk_sdio_device {
	const char *helper;
	const struct btmtk_sdio_card_reg *reg;
	const bool support_pscan_win_report;
	u16 sd_blksz_fw_dl;
	bool supports_fw_dump;
};
#pragma pack(1)
struct _PATCH_HEADER {
	u8 ucDateTime[16];
	u8 ucPlatform[4];
	u16 u2HwVer;
	u16 u2SwVer;
	u32 u4PatchVer;
	u16 u2PatchStartAddr;/*Patch ram start address*/
};
#pragma pack()

struct bt_stereo_clk {
	u64 sys_clk;
	u64 fw_clk;
};

struct bt_stereo_para {
	u16 handle;
	u8 method;
	u32 period;
	u16 active_slots;
};

#define HW_VERSION 0x80000000
#define FW_VERSION 0x80000004
#define CHIP_ID 0x80000008

/*common register address*/
#define CCIR 0x0000
#define CHLPCR 0x0004
#define CSDIOCSR 0x0008
#define CHCR   0x000C
#define CHISR  0x0010
#define CHIER  0x0014
#define CTDR   0x0018
#define CRDR   0x001C
#define CTFSR 0x0020
#define CRPLR 0x0024
#define SWPCDBGR   0x0154
/*CHLPCR*/
#define C_FW_INT_EN_SET            0x00000001
#define C_FW_INT_EN_CLEAR        0x00000002
/*CHISR*/
#define RX_PKT_LEN             0xFFFF0000
#define FIRMWARE_INT             0x0000FE00
#define FIRMWARE_INT_BIT15       0x00008000/*FW inform driver don't change to fw own for dore dump*/
#define TX_FIFO_OVERFLOW         0x00000100
#define FW_INT_IND_INDICATOR        0x00000080
#define TX_COMPLETE_COUNT         0x00000070
#define TX_UNDER_THOLD             0x00000008
#define TX_EMPTY             0x00000004
#define RX_DONE                 0x00000002
#define FW_OWN_BACK_INT             0x00000001


#define MTKSTP_HEADER_SIZE 0x0004

#define MTK_SDIO_PACKET_HEADER_SIZE 4
#define MTKDATA_HEADER_SIZE 10
#define MTKWMT_HEADER_SIZE 4

#define PATCH_DOWNLOAD_SIZE 1970

#define DRIVER_OWN 0
#define FW_OWN 1

#define MTK_WMT_HEADER_LEN 4

#define DEFAULE_PATCH_FRAG_SIZE    1000

#define PATCH_IS_DOWNLOAD_BY_OTHER 0
#define PATCH_READY 1
#define PATCH_NEED_DOWNLOAD 2

#define BTMTK_SDIO_RETRY_COUNT 500

#define BTMTK_LOAD_WOBLE_RETRY_COUNT 1

/**
 * stpbtfwlog device node
 */
#define HCI_MAX_COMMAND_SIZE		255
/* Write a char to buffer.
 * ex : echo 01 be > /dev/stpbtfwlog
 * "01 " need three bytes.
 */
#define HCI_MAX_COMMAND_BUF_SIZE	(HCI_MAX_COMMAND_SIZE * 3)

/*
 * data event:
 * return
 * 0:
 * patch download is not complete/get patch semaphore fail
 * 1:
 * patch download is complete by others
 * 2
 * patch download is not coplete
 * 3:(for debug)
 * release patch semaphore success
 */

/* Platform specific DMA alignment */
#define BTSDIO_DMA_ALIGN                8

/* Macros for Data Alignment : size */
#define ALIGN_SZ(p, a)  \
		(((p) + ((a) - 1)) & ~((a) - 1))

/* Macros for Data Alignment : address */
#define ALIGN_ADDR(p, a)        \
		((((unsigned long)(p)) + (((unsigned long)(a)) - 1)) & \
		~(((unsigned long)(a)) - 1))
struct sk_buff *btmtk_create_send_data(struct sk_buff *skb);
int btmtk_print_buffer_conent(u8 *buf, u32 Datalen);
u32 lock_unsleepable_lock(struct _OSAL_UNSLEEPABLE_LOCK_ *pUSL);
u32 unlock_unsleepable_lock(struct _OSAL_UNSLEEPABLE_LOCK_ *pUSL);

extern unsigned char probe_counter;
extern unsigned char *txbuf;
extern u8 probe_ready;

enum {
	BTMTK_WOBLE_STATE_UNKNOWN,
	BTMTK_WOBLE_STATE_SUSPEND,
	BTMTK_WOBLE_STATE_RESUME,
	BTMTK_WOBLE_STATE_DUMPING,
	BTMTK_WOBLE_STATE_DUMPEND,
	BTMTK_WOBLE_STATE_NEEDRESET_STACK,
};

enum {
	BTMTK_SDIO_EVENT_COMPARE_STATE_UNKNOWN,
	BTMTK_SDIO_EVENT_COMPARE_STATE_NOTHING_NEED_COMPARE,
	BTMTK_SDIO_EVENT_COMPARE_STATE_NEED_COMPARE,
	BTMTK_SDIO_EVENT_COMPARE_STATE_COMPARE_SUCCESS,
};


/**
 * Maximum rom patch file name length
 */
#define MAX_BIN_FILE_NAME_LEN	32


#define COMPARE_FAIL				-1
#define COMPARE_SUCCESS				1
#define COMP_EVENT_TIMO				2000
#define WOBLE_COMP_EVENT_TIMO		5000
#define WLAN_STATUS_IS_NOT_LOAD		-1
#define WLAN_STATUS_DEFAULT		0
#define WLAN_STATUS_CALL_REMOVE_START	1 /* WIFI driver is inserted */

/**
 * BTMTK ioctl
 */
#define BTMTK_IOCTL_MAGIC 'k'

#define BTMTK_IOCTL_STEREO_GET_CLK _IOR(BTMTK_IOCTL_MAGIC, 1, void *)
#define BTMTK_IOCTL_STEREO_SET_PARA _IOW(BTMTK_IOCTL_MAGIC, 2, void *)

/**
 * Inline functions
 */
static inline int is_support_unify_woble(struct btmtk_sdio_card *data)
{
	if (data->bt_cfg.support_unify_woble) {
		if (((data->chip_id & 0xffff) == 0x7668) ||
				((data->chip_id & 0xffff) == 0x7663))
			return 1;
		else
			return 0;
	} else {
		return 0;
	}
}

static inline int is_mt7668(struct btmtk_sdio_card *data)
{
#if SUPPORT_MT7668
	return ((data->chip_id & 0xffff) == 0x7668);
#else
	return 0;
#endif
}

static inline int is_mt7663(struct btmtk_sdio_card *data)
{
#if SUPPORT_MT7663
	return ((data->chip_id & 0xffff) == 0x7663);
#else
	return 0;
#endif
}

#define FW_OWN_OFF "fw own off"
#define FW_OWN_ON  "fw own on"

#define WOBLE_OFF "woble off"
#define WOBLE_ON  "woble on"

#define RX_CHECK_OFF "rx check off"
#define RX_CHECK_ON "rx check on"

#define RELOAD_SETTING "reload_setting"

enum BTMTK_SDIO_RX_CHECKPOINT {
	BTMTK_SDIO_RX_CHECKPOINT_INTR,
	BTMTK_SDIO_RX_CHECKPOINT_RX_START,
	BTMTK_SDIO_RX_CHECKPOINT_RX_DONE,
	BTMTK_SDIO_RX_CHECKPOINT_ENABLE_INTR,

	BTMTK_SDIO_RX_CHECKPOINT_NUM
};

#define BTMTK_SDIO_TIMESTAMP_NUM 50

int btmtk_sdio_reset_dongle(void);

/* WOBX attribute type */
#define WOBX_TRIGGER_INFO_ADDR_TYPE         1
#define WOBX_TRIGGER_INFO_ADV_DATA_TYPE     2
#define WOBX_TRIGGER_INFO_TRACE_LOG_TYPE    3
#define WOBX_TRIGGER_INFO_SCAN_LOG_TYPE     4
#define WOBX_TRIGGER_INFO_TRIGGER_CNT_TYPE  5

#endif


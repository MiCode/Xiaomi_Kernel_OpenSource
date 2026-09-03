/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _WCN_BOOT
#define _WCN_BOOT

#include <misc/marlin_platform.h>

#include "rf/rf.h"

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "WCN BOOT: " fmt

extern uint GNSS_CP_START_ADDR;
extern uint GNSS_FIRMWARE_MAX_SIZE;
extern uint GNSS_DUMP_PACKET_SIZE;
extern uint GNSS_SHARE_MEMORY_SIZE;
extern uint GNSS_DUMP_IRAM_START_ADDR;
extern uint GNSS_CP_IRAM_DATA_NUM;
extern uint GNSS_DUMP_REG_NUMBER;
extern unsigned char  flag_download_done;

struct wcn_sync_info_t {
	unsigned int init_status;
	unsigned int mem_pd_bt_start_addr;
	unsigned int mem_pd_bt_end_addr;
	unsigned int mem_pd_wifi_start_addr;
	unsigned int mem_pd_wifi_end_addr;
	unsigned int prj_type;
	unsigned short tsx_dac_data;
	unsigned short rsved;
} __packed;

struct tsx_data {
	u32 flag; /* cali flag ref */
	u16 dac; /* AFC cali data */
	u16 reserved;
};

struct tsx_cali {
	u32 init_flag;
	struct tsx_data tsxdata;
};

#define WCN_BOUND_CONFIG_NUM	4
struct wcn_pmic_config {
	bool enable;
	char name[32];
	/* index [0]:addr [1]:mask [2]:unboudval [3]boundval */
	u32 config[WCN_BOUND_CONFIG_NUM];
};

struct wcn_clock_info {
	enum wcn_clock_type type;
	enum wcn_clock_mode mode;
	/*
	 * xtal-26m-clk-type-gpio config in the dts.
	 * if xtal-26m-clk-type config in the dts,this gpio unvalid.
	 */
	int gpio;
};

struct marlin_device {
	struct wcn_clock_info clk_xtal_26m;
	int wakeup_ap;
	int reset;
	int chip_en;
	int int_ap;
	/* pmic config */
	struct regmap *syscon_pmic;
	/* sharkl5 vddgen1 */
	struct wcn_pmic_config avdd12_parent_bound_chip;
	struct wcn_pmic_config avdd12_bound_wbreq;
	struct wcn_pmic_config avdd33_bound_wbreq;

	bool bound_avdd12;
	bool bound_dcxo18;
	/* power sequence */
	/* VDDIO->DVDD12->chip_en->rst_N->AVDD12->AVDD33 */
	struct regulator *dvdd12;
	struct regulator *avdd12;
	/* for PCIe */
	struct regulator *avdd18;
	/* for wifi PA, BT TX RX */
	struct regulator *avdd33;
	/* for internal 26M clock */
	struct regulator *dcxo18;
	struct clk *clk_32k;

	struct clk *clk_parent;
	struct clk *clk_enable;
	struct device *dev;
	struct device_node *np;
	struct mutex power_lock;
	struct completion carddetect_done;
	struct completion download_done;
	struct completion gnss_download_done;
	unsigned long power_state;
	char *write_buffer;
	struct delayed_work power_wq;
	struct work_struct download_wq;
	struct work_struct gnss_dl_wq;
	bool keep_power_on;
	bool wait_ge2;
	bool is_btwf_in_sysfs;
	bool is_gnss_in_sysfs;
	bool need_to_check_ufs;
	int wifi_need_download_ini_flag;
	int first_power_on_ready;
	atomic_t download_finish_flag;
	unsigned char gnss_dl_finish_flag;
	int loopcheck_status_change;
	struct wcn_sync_info_t sync_f;
	struct tsx_cali tsxcali;
	char *btwf_path;
	char *gnss_path;
	char *emmc_ufs;
	phys_addr_t	base_addr_btwf;
	u32	maxsz_btwf;
	phys_addr_t	base_addr_gnss;
	u32	maxsz_gnss;
};

#endif

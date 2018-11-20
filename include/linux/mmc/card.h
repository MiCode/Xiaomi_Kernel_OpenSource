/*
 *  linux/include/linux/mmc/card.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Card driver specific definitions.
 */
#ifndef LINUX_MMC_CARD_H
#define LINUX_MMC_CARD_H

#include <linux/device.h>
#include <linux/mmc/core.h>
#include <linux/mmc/mmc.h>
#include <linux/mod_devicetable.h>
#include <linux/notifier.h>

#define MMC_CARD_CMDQ_BLK_SIZE 512

struct mmc_cid {
	unsigned int		manfid;
	char			prod_name[8];
	unsigned char		prv;
	unsigned int		serial;
	unsigned short		oemid;
	unsigned short		year;
	unsigned char		hwrev;
	unsigned char		fwrev;
	unsigned char		month;
};

struct mmc_csd {
	unsigned char		structure;
	unsigned char		mmca_vsn;
	unsigned short		cmdclass;
	unsigned short		taac_clks;
	unsigned int		taac_ns;
	unsigned int		c_size;
	unsigned int		r2w_factor;
	unsigned int		max_dtr;
	unsigned int		erase_size;		/* In sectors */
	unsigned int		read_blkbits;
	unsigned int		write_blkbits;
	unsigned int		capacity;
	unsigned int		read_partial:1,
				read_misalign:1,
				write_partial:1,
				write_misalign:1,
				dsr_imp:1;
};

struct mmc_ext_csd {
	u8			rev;
	u8			erase_group_def;
	u8			sec_feature_support;
	u8			rel_sectors;
	u8			rel_param;
	bool			enhanced_rpmb_supported;
	u8			part_config;
	u8			cache_ctrl;
	u8			rst_n_function;
	unsigned int		part_time;		/* Units: ms */
	unsigned int		sa_timeout;		/* Units: 100ns */
	unsigned int		generic_cmd6_time;	/* Units: 10ms */
	unsigned int        power_off_longtime;     /* Units: ms */
	u8			power_off_notification;	/* state */
	unsigned int		hs_max_dtr;
	unsigned int		hs200_max_dtr;
#define MMC_HIGH_26_MAX_DTR	26000000
#define MMC_HIGH_52_MAX_DTR	52000000
#define MMC_HIGH_DDR_MAX_DTR	52000000
#define MMC_HS200_MAX_DTR	200000000
	unsigned int		sectors;
	unsigned int		hc_erase_size;		/* In sectors */
	unsigned int		hc_erase_timeout;	/* In milliseconds */
	unsigned int		sec_trim_mult;	/* Secure trim multiplier  */
	unsigned int		sec_erase_mult;	/* Secure erase multiplier */
	unsigned int		trim_timeout;		/* In milliseconds */
	bool			partition_setting_completed;	/* enable bit */
	unsigned long long	enhanced_area_offset;	/* Units: Byte */
	unsigned int		enhanced_area_size;	/* Units: KB */
	unsigned int		cache_size;		/* Units: KB */
	bool			hpi_en;			/* HPI enablebit */
	bool			hpi;			/* HPI support bit */
	unsigned int		hpi_cmd;		/* cmd used as HPI */
	bool			bkops;		/* background support bit */
	bool			man_bkops_en;	/* manual bkops enable bit */
	bool			auto_bkops_en;	/* auto bkops enable bit */
	unsigned int            data_sector_size;       /* 512 bytes or 4KB */
	unsigned int            data_tag_unit_size;     /* DATA TAG UNIT size */
	unsigned int		boot_ro_lock;		/* ro lock support */
	bool			boot_ro_lockable;
	u8			raw_ext_csd_cmdq;	/* 15 */
	u8			raw_ext_csd_cache_ctrl;	/* 33 */
	bool			ffu_capable;	/* Firmware upgrade support */
	bool			cmdq_en;	/* Command Queue enabled */
	bool			cmdq_support;	/* Command Queue supported */
	unsigned int		cmdq_depth;	/* Command Queue depth */
#define MMC_FIRMWARE_LEN 8
	u8			fwrev[MMC_FIRMWARE_LEN];  /* FW version */
	u8			raw_exception_status;	/* 54 */
	u8			raw_partition_support;	/* 160 */
	u8			raw_rpmb_size_mult;	/* 168 */
	u8			raw_erased_mem_count;	/* 181 */
	u8			raw_ext_csd_bus_width;	/* 183 */
	u8			strobe_support;		/* 184 */
#define MMC_STROBE_SUPPORT	(1 << 0)
	u8			raw_ext_csd_hs_timing;	/* 185 */
	u8			raw_ext_csd_structure;	/* 194 */
	u8			raw_card_type;		/* 196 */
	u8			raw_driver_strength;	/* 197 */
	u8			out_of_int_time;	/* 198 */
	u8			raw_pwr_cl_52_195;	/* 200 */
	u8			raw_pwr_cl_26_195;	/* 201 */
	u8			raw_pwr_cl_52_360;	/* 202 */
	u8			raw_pwr_cl_26_360;	/* 203 */
	u8			raw_s_a_timeout;	/* 217 */
	u8			raw_hc_erase_gap_size;	/* 221 */
	u8			raw_erase_timeout_mult;	/* 223 */
	u8			raw_hc_erase_grp_size;	/* 224 */
	u8			raw_sec_trim_mult;	/* 229 */
	u8			raw_sec_erase_mult;	/* 230 */
	u8			raw_sec_feature_support;/* 231 */
	u8			raw_trim_mult;		/* 232 */
	u8			raw_pwr_cl_200_195;	/* 236 */
	u8			raw_pwr_cl_200_360;	/* 237 */
	u8			raw_pwr_cl_ddr_52_195;	/* 238 */
	u8			raw_pwr_cl_ddr_52_360;	/* 239 */
	u8			cache_flush_policy;	/* 240 */
#define MMC_BKOPS_URGENCY_MASK 0x3
	u8			raw_pwr_cl_ddr_200_360;	/* 253 */
	u8			raw_bkops_status;	/* 246 */
	u8			raw_sectors[4];		/* 212 - 4 bytes */
	u8			pre_eol_info;		/* 267 */
	u8			device_life_time_est_typ_a;	/* 268 */
	u8			device_life_time_est_typ_b;	/* 269 */
	u8			barrier_support;	/* 486 */
	u8			barrier_en;

	u8			fw_version;		/* 254 */
	unsigned int            feature_support;
#define MMC_DISCARD_FEATURE	BIT(0)                  /* CMD38 feature */
};

struct sd_scr {
	unsigned char		sda_vsn;
	unsigned char		sda_spec3;
	unsigned char		bus_widths;
#define SD_SCR_BUS_WIDTH_1	(1<<0)
#define SD_SCR_BUS_WIDTH_4	(1<<2)
	unsigned char		cmds;
#define SD_SCR_CMD20_SUPPORT   (1<<0)
#define SD_SCR_CMD23_SUPPORT   (1<<1)
};

struct sd_ssr {
	unsigned int		au;			/* In sectors */
	unsigned int		erase_timeout;		/* In milliseconds */
	unsigned int		erase_offset;		/* In milliseconds */
};

struct sd_switch_caps {
	unsigned int		hs_max_dtr;
	unsigned int		uhs_max_dtr;
#define HIGH_SPEED_MAX_DTR	50000000
#define UHS_SDR104_MAX_DTR	208000000
#define UHS_SDR50_MAX_DTR	100000000
#define UHS_DDR50_MAX_DTR	50000000
#define UHS_SDR25_MAX_DTR	UHS_DDR50_MAX_DTR
#define UHS_SDR12_MAX_DTR	25000000
	unsigned int		sd3_bus_mode;
#define UHS_SDR12_BUS_SPEED	0
#define HIGH_SPEED_BUS_SPEED	1
#define UHS_SDR25_BUS_SPEED	1
#define UHS_SDR50_BUS_SPEED	2
#define UHS_SDR104_BUS_SPEED	3
#define UHS_DDR50_BUS_SPEED	4

#define SD_MODE_HIGH_SPEED	(1 << HIGH_SPEED_BUS_SPEED)
#define SD_MODE_UHS_SDR12	(1 << UHS_SDR12_BUS_SPEED)
#define SD_MODE_UHS_SDR25	(1 << UHS_SDR25_BUS_SPEED)
#define SD_MODE_UHS_SDR50	(1 << UHS_SDR50_BUS_SPEED)
#define SD_MODE_UHS_SDR104	(1 << UHS_SDR104_BUS_SPEED)
#define SD_MODE_UHS_DDR50	(1 << UHS_DDR50_BUS_SPEED)
	unsigned int		sd3_drv_type;
#define SD_DRIVER_TYPE_B	0x01
#define SD_DRIVER_TYPE_A	0x02
#define SD_DRIVER_TYPE_C	0x04
#define SD_DRIVER_TYPE_D	0x08
	unsigned int		sd3_curr_limit;
#define SD_SET_CURRENT_LIMIT_200	0
#define SD_SET_CURRENT_LIMIT_400	1
#define SD_SET_CURRENT_LIMIT_600	2
#define SD_SET_CURRENT_LIMIT_800	3
#define SD_SET_CURRENT_NO_CHANGE	(-1)

#define SD_MAX_CURRENT_200	(1 << SD_SET_CURRENT_LIMIT_200)
#define SD_MAX_CURRENT_400	(1 << SD_SET_CURRENT_LIMIT_400)
#define SD_MAX_CURRENT_600	(1 << SD_SET_CURRENT_LIMIT_600)
#define SD_MAX_CURRENT_800	(1 << SD_SET_CURRENT_LIMIT_800)
};

struct sdio_cccr {
	unsigned int		sdio_vsn;
	unsigned int		sd_vsn;
	unsigned int		multi_block:1,
				low_speed:1,
				wide_bus:1,
				high_power:1,
				high_speed:1,
				disable_cd:1,
				async_intr_sup:1;
};

struct sdio_cis {
	unsigned short		vendor;
	unsigned short		device;
	unsigned short		blksize;
	unsigned int		max_dtr;
};

struct mmc_host;
struct mmc_ios;
struct sdio_func;
struct sdio_func_tuple;
struct mmc_queue_req;

#define SDIO_MAX_FUNCS		7

/* The number of MMC physical partitions.  These consist of:
 * boot partitions (2), general purpose partitions (4) and
 * RPMB partition (1) in MMC v4.4.
 */
#define MMC_NUM_BOOT_PARTITION	2
#define MMC_NUM_GP_PARTITION	4
#define MMC_NUM_PHY_PARTITION	7
#define MAX_MMC_PART_NAME_LEN	20

/*
 * MMC Physical partitions
 */
struct mmc_part {
	unsigned int	size;	/* partition size (in bytes) */
	unsigned int	part_cfg;	/* partition type */
	char	name[MAX_MMC_PART_NAME_LEN];
	bool	force_ro;	/* to make boot parts RO by default */
	unsigned int	area_type;
#define MMC_BLK_DATA_AREA_MAIN	(1<<0)
#define MMC_BLK_DATA_AREA_BOOT	(1<<1)
#define MMC_BLK_DATA_AREA_GP	(1<<2)
#define MMC_BLK_DATA_AREA_RPMB	(1<<3)
};

enum {
	MMC_BKOPS_NO_OP,
	MMC_BKOPS_NOT_CRITICAL,
	MMC_BKOPS_PERF_IMPACT,
	MMC_BKOPS_CRITICAL,
	MMC_BKOPS_NUM_SEVERITY_LEVELS,
};

/**
 * struct mmc_bkops_stats - BKOPS statistics
 * @lock: spinlock used for synchronizing the debugfs and the runtime accesses
 *	to this structure. No need to call with spin_lock_irq api
 * @manual_start: number of times START_BKOPS was sent to the device
 * @hpi: number of times HPI was sent to the device
 * @auto_start: number of times AUTO_EN was set to 1
 * @auto_stop: number of times AUTO_EN was set to 0
 * @level: number of times the device reported the need for each level of
 *	bkops handling
 * @enabled: control over whether statistics should be gathered
 *
 * This structure is used to collect statistics regarding the bkops
 * configuration and use-patterns. It is collected during runtime and can be
 * shown to the user via a debugfs entry.
 */
struct mmc_bkops_stats {
	spinlock_t	lock;
	unsigned int	manual_start;
	unsigned int	hpi;
	unsigned int	auto_start;
	unsigned int	auto_stop;
	unsigned int	level[MMC_BKOPS_NUM_SEVERITY_LEVELS];
	bool		enabled;
};

/**
 * struct mmc_bkops_info - BKOPS data
 * @stats: statistic information regarding bkops
 * @needs_check: indication whether need to check with the device
 *	whether it requires handling of BKOPS (CMD8)
 * @needs_manual: indication whether have to send START_BKOPS
 *	to the device
 */
struct mmc_bkops_info {
	struct mmc_bkops_stats stats;
	bool needs_check;
	bool needs_bkops;
	u32  retry_counter;
};

enum mmc_pon_type {
	MMC_LONG_PON = 1,
	MMC_SHRT_PON,
};

#define mmc_card_strobe(c) (((c)->ext_csd).strobe_support & MMC_STROBE_SUPPORT)

#define MMC_QUIRK_CMDQ_DELAY_BEFORE_DCMD 6 /* microseconds */

/*
 * MMC device
 */
struct mmc_card {
	struct mmc_host		*host;		/* the host this device belongs to */
	struct device		dev;		/* the device */
	u32			ocr;		/* the current OCR setting */
	unsigned long		clk_scaling_lowest;	/* lowest scaleable*/
							/* frequency */
	unsigned long		clk_scaling_highest;	/* highest scaleable */
							/* frequency */
	unsigned int		rca;		/* relative card address of device */
	unsigned int		type;		/* card type */
#define MMC_TYPE_MMC		0		/* MMC card */
#define MMC_TYPE_SD		1		/* SD card */
#define MMC_TYPE_SDIO		2		/* SDIO card */
#define MMC_TYPE_SD_COMBO	3		/* SD combo (IO+mem) card */
	unsigned int		state;		/* (our) card state */
#define MMC_STATE_CMDQ		(1<<12)         /* card is in cmd queue mode */
	unsigned int		quirks; 	/* card quirks */
#define MMC_QUIRK_LENIENT_FN0	(1<<0)		/* allow SDIO FN0 writes outside of the VS CCCR range */
#define MMC_QUIRK_BLKSZ_FOR_BYTE_MODE (1<<1)	/* use func->cur_blksize */
						/* for byte mode */
#define MMC_QUIRK_NONSTD_SDIO	(1<<2)		/* non-standard SDIO card attached */
						/* (missing CIA registers) */
#define MMC_QUIRK_BROKEN_CLK_GATING (1<<3)	/* clock gating the sdio bus will make card fail */
#define MMC_QUIRK_NONSTD_FUNC_IF (1<<4)		/* SDIO card has nonstd function interfaces */
#define MMC_QUIRK_DISABLE_CD	(1<<5)		/* disconnect CD/DAT[3] resistor */
#define MMC_QUIRK_INAND_CMD38	(1<<6)		/* iNAND devices have broken CMD38 */
#define MMC_QUIRK_BLK_NO_CMD23	(1<<7)		/* Avoid CMD23 for regular multiblock */
#define MMC_QUIRK_BROKEN_BYTE_MODE_512 (1<<8)	/* Avoid sending 512 bytes in */
						/* byte mode */
#define MMC_QUIRK_LONG_READ_TIME (1<<9)		/* Data read time > CSD says */
#define MMC_QUIRK_SEC_ERASE_TRIM_BROKEN (1<<10)	/* Skip secure for erase/trim */
#define MMC_QUIRK_BROKEN_IRQ_POLLING	(1<<11)	/* Polling SDIO_CCCR_INTx could create a fake interrupt */
#define MMC_QUIRK_TRIM_BROKEN	(1<<12)		/* Skip trim */
#define MMC_QUIRK_BROKEN_HPI	(1<<13)		/* Disable broken HPI support */
						/* byte mode */
#define MMC_QUIRK_INAND_DATA_TIMEOUT  (1<<14)   /* For incorrect data timeout */
#define MMC_QUIRK_CACHE_DISABLE (1 << 15)	/* prevent cache enable */
#define MMC_QUIRK_QCA6574_SETTINGS (1 << 16)	/* QCA6574 card settings*/
#define MMC_QUIRK_QCA9377_SETTINGS (1 << 17)	/* QCA9377 card settings*/

/* Make sure CMDQ is empty before queuing DCMD */
#define MMC_QUIRK_CMDQ_EMPTY_BEFORE_DCMD (1 << 18)

	bool			reenable_cmdq;	/* Re-enable Command Queue */

	unsigned int		erase_size;	/* erase size in sectors */
 	unsigned int		erase_shift;	/* if erase unit is power 2 */
 	unsigned int		pref_erase;	/* in sectors */
	unsigned int		eg_boundary;	/* don't cross erase-group boundaries */
 	u8			erased_byte;	/* value of erased bytes */

	u32			raw_cid[4];	/* raw card CID */
	u32			raw_csd[4];	/* raw card CSD */
	u32			raw_scr[2];	/* raw card SCR */
	u32			raw_ssr[16];	/* raw card SSR */
	struct mmc_cid		cid;		/* card identification */
	struct mmc_csd		csd;		/* card specific */
	struct mmc_ext_csd	ext_csd;	/* mmc v4 extended card specific */
	struct sd_scr		scr;		/* extra SD information */
	struct sd_ssr		ssr;		/* yet more SD information */
	struct sd_switch_caps	sw_caps;	/* switch (CMD6) caps */

	unsigned int		sdio_funcs;	/* number of SDIO functions */
	struct sdio_cccr	cccr;		/* common card info */
	struct sdio_cis		cis;		/* common tuple info */
	struct sdio_func	*sdio_func[SDIO_MAX_FUNCS]; /* SDIO functions (devices) */
	struct sdio_func	*sdio_single_irq; /* SDIO function when only one IRQ active */
	unsigned		num_info;	/* number of info strings */
	const char		**info;		/* info strings */
	struct sdio_func_tuple	*tuples;	/* unknown common tuples */

	unsigned int		sd_bus_speed;	/* Bus Speed Mode set for the card */
	unsigned int		mmc_avail_type;	/* supported device type by both host and card */
	unsigned int		drive_strength;	/* for UHS-I, HS200 or HS400 */

	struct dentry		*debugfs_root;
	struct mmc_part	part[MMC_NUM_PHY_PARTITION]; /* physical partitions */
	unsigned int		nr_parts;
	unsigned int		part_curr;

	struct notifier_block   reboot_notify;
	enum mmc_pon_type	pon_type;
	bool cmdq_init;
	struct mmc_bkops_info bkops;
};

static inline bool mmc_large_sector(struct mmc_card *card)
{
	return card->ext_csd.data_sector_size == 4096;
}

/* extended CSD mapping to mmc version */
enum mmc_version_ext_csd_rev {
	MMC_V4_0,
	MMC_V4_1,
	MMC_V4_2,
	MMC_V4_41 = 5,
	MMC_V4_5,
	MMC_V4_51 = MMC_V4_5,
	MMC_V5_0,
	MMC_V5_01 = MMC_V5_0,
	MMC_V5_1
};

bool mmc_card_is_blockaddr(struct mmc_card *card);

#define mmc_card_mmc(c)		((c)->type == MMC_TYPE_MMC)
#define mmc_card_sd(c)		((c)->type == MMC_TYPE_SD)
#define mmc_card_sdio(c)	((c)->type == MMC_TYPE_SDIO)
#define mmc_card_cmdq(c)       ((c)->state & MMC_STATE_CMDQ)

static inline bool mmc_card_support_auto_bkops(const struct mmc_card *c)
{
	return c->ext_csd.rev >= MMC_V5_1;
}

static inline bool mmc_card_configured_manual_bkops(const struct mmc_card *c)
{
	return c->ext_csd.man_bkops_en;
}

static inline bool mmc_card_configured_auto_bkops(const struct mmc_card *c)
{
	return c->ext_csd.auto_bkops_en;
}

static inline bool mmc_enable_qca6574_settings(const struct mmc_card *c)
{
	return c->quirks & MMC_QUIRK_QCA6574_SETTINGS;
}

static inline bool mmc_enable_qca9377_settings(const struct mmc_card *c)
{
	return c->quirks & MMC_QUIRK_QCA9377_SETTINGS;
}

#define mmc_dev_to_card(d)	container_of(d, struct mmc_card, dev)
#define mmc_get_drvdata(c)	dev_get_drvdata(&(c)->dev)
#define mmc_set_drvdata(c, d)	dev_set_drvdata(&(c)->dev, d)

extern int mmc_send_pon(struct mmc_card *card);
extern void mmc_blk_cmdq_req_done(struct mmc_request *mrq);
#endif /* LINUX_MMC_CARD_H */

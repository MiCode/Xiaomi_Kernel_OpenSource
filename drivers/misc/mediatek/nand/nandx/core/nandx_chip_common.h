/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */

#ifndef __NANDX_CHIP_COMMON_H__
#define __NANDX_CHIP_COMMON_H__

#include "nandx_device_info.h"
#include "nandx_chip.h"
#include "nfc_core.h"

#define STATUS_FAIL(x)		((x) & 1)
#define STATUS_SLC_FAIL(x)	(((x) >> 2) & 1)

struct nandx_chip;

typedef void (*rr_entry_t) (struct nandx_chip *chip);
typedef void (*rr_get_parameters_t) (struct nandx_chip *chip, u8 *param);
typedef void (*rr_set_parameters_t) (struct nandx_chip *chip, u8 *param);
typedef void (*rr_enable_t) (struct nandx_chip *chip);
typedef void (*rr_exit_t) (struct nandx_chip *chip);

struct read_retry_ops {
	u16 loop_count;
	u16 slc_loop_count;
	rr_entry_t entry;
	rr_get_parameters_t get_parameters;
	rr_set_parameters_t set_parameters;
	rr_enable_t enable;
	rr_exit_t exit;
};

struct pair_page_ops {
	u32 (*transfer_mapping)(u32 page, bool low);
	u32 (*transfer_offset)(u32 low_page);
};

struct slc_mode_ops {
	void (*entry)(struct nandx_chip *chip);
	void (*exit)(struct nandx_chip *chip);
};

struct program_order {
	u32 wl;
	u32 cycle;
};

struct nandx_chip {
	struct nandx_chip_dev chip_dev;
	struct nandx_device_info *dev_info;
	struct nfc_handler *nfc;
	struct pair_page_ops *pp_ops;
	struct slc_mode_ops *slc_ops;
	struct read_retry_ops *rr_ops;
	struct nand_timing timing;
	struct program_order *orders;
	u8 *cmd;
	u8 *feature;
	u8 *interface_value;
	u8 *interface_mapping;
	u8 *addressing;
	u8 *drive_strength;
	bool slc_mode;
	bool interface_ddr;
	bool calibration;
	bool ddr_enable;
	bool randomize;
	u32 rr_count;
	enum EXTEND_CMD_TYPE pre_cmd;
	enum EXTEND_CMD_TYPE program_order_cmd;
	enum VENDOR_TYPE vendor_type;
};

/*
 ********************************
 * operations
 ********************************
 */
void nandx_chip_set_program_order_cmd(struct nandx_chip *chip, int cycle);
void nandx_chip_reset(struct nandx_chip *chip);
void nandx_chip_reset_lun(struct nandx_chip *chip);
void nandx_chip_synchronous_reset(struct nandx_chip *chip);
void nandx_chip_read_id(struct nandx_chip *chip, u8 *id, int num);
u8 nandx_chip_read_status(struct nandx_chip *chip);
u8 nandx_chip_read_enhance_status(struct nandx_chip *chip, u32 row);
void nandx_chip_set_feature(struct nandx_chip *chip, u8 addr, u8 *param,
			    int num);
void nandx_chip_get_feature(struct nandx_chip *chip, u8 addr, u8 *param,
			    int num);
int nandx_chip_set_feature_with_check(struct nandx_chip *chip, u8 addr,
				      u8 *param, u8 *back, int num);
void nandx_chip_set_lun_feature(struct nandx_chip *chip, u8 lun, u8 addr,
				u8 *param, int num);
void nandx_chip_get_lun_feature(struct nandx_chip *chip, u8 lun, u8 addr,
				u8 *param, int num);
int nandx_chip_set_lun_feature_with_check(struct nandx_chip *chip, u8 lun,
					  u8 addr, u8 *param, u8 *back,
					  int num);
void nandx_chip_erase_block(struct nandx_chip *chip, u32 row);
void nandx_chip_multi_erase_block(struct nandx_chip *chip, u32 *rows);
void nandx_chip_read_parameters_page(struct nandx_chip *chip, u8 *data,
				     int size);
void nandx_chip_enable_randomizer(struct nandx_chip *chip, u32 row,
				  bool encode);
void nandx_chip_disable_randomizer(struct nandx_chip *chip);
int nandx_chip_read_data(struct nandx_chip *chip, int sector_num, void *data,
			 void *fdm);
void nandx_chip_read_page(struct nandx_chip *chip, u32 row);
void nandx_chip_cache_read_page(struct nandx_chip *chip, u32 row);
void nandx_chip_cache_read_last_page(struct nandx_chip *chip);
void nandx_chip_multi_read_page(struct nandx_chip *chip, u32 row);
void nandx_chip_random_output(struct nandx_chip *chip, u32 row, u32 col);
int nandx_chip_program_page(struct nandx_chip *chip, u32 row, void *data,
			    void *fdm);
int nandx_chip_cache_program_page(struct nandx_chip *chip, u32 row,
				  void *data, void *fdm);
int nandx_chip_multi_program_1stpage(struct nandx_chip *chip, u32 row,
				     void *data, void *fdm);
int nandx_chip_multi_program_2ndpage(struct nandx_chip *chip, u32 row,
				     void *data, void *fdm);
int nandx_chip_calibration(struct nandx_chip *chip);

int setup_read_retry(struct nandx_chip *chip, int count);
int nandx_chip_read_retry(struct nandx_chip *chip,
			  struct nandx_ops *ops, int *count);
int nandx_bad_block_check(struct nandx_chip *chip, u32 row,
			  enum BAD_BLOCK_TYPE type);
struct read_retry_ops *get_read_retry_ops(struct nandx_device_info *dev_info);
struct pair_page_ops *get_pair_page_ops(u32 mode_type);
struct slc_mode_ops *get_slc_mode_ops(u32 mode_type);
int nandx_chip_auto_calibration(struct nandx_chip *chip, int count);

static inline void slc_mode_entry(struct nandx_chip *chip, bool start)
{
	if (chip->slc_ops && (start || !chip->slc_ops->exit))
		chip->slc_ops->entry(chip);
}

static inline void slc_mode_exit(struct nandx_chip *chip)
{
	if (chip->slc_ops && chip->slc_ops->exit)
		chip->slc_ops->exit(chip);
}

#ifdef CONFIG_NANDX_EXPRESS
#define NANDX_EXPRESS_CHIP(chip) \
	({\
		extern struct nandx_chip *nandx_express_chip; \
		(chip) = nandx_express_chip; \
	})
#endif

#endif				/* __NANDX_CHIP_H__ */

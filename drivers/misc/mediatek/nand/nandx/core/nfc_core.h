/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */
#ifndef __NFC_CORE_H__
#define __NFC_CORE_H__

#include "nandx_util.h"
#include "nandx_info.h"
#include "nandx_device_info.h"

enum WAIT_TYPE {
	IRQ_WAIT_RB,
	POLL_WAIT_RB,
	POLL_WAIT_TWHR2,
};

/**
 * struct nfc_format - nand info from spec
 * @pagesize: nand page size
 * @oobsize: nand oob size
 * @ecc_strength: spec required ecc strength per 1KB
 */
struct nfc_format {
	u32 page_size;
	u32 oob_size;
	u32 ecc_strength;
};

struct nfc_handler {
	u32 sector_size;
	u32 spare_size;
	u32 fdm_size;
	u32 fdm_ecc_size;
	/* ecc strength per sector_size */
	u32 ecc_strength;
	void (*send_command)(struct nfc_handler *handler, u8 cmd);
	void (*send_address)(struct nfc_handler *handler, u32 col, u32 row,
				u32 col_cycle, u32 row_cycle);
	int (*write_page)(struct nfc_handler *handler, u8 *data, u8 *fdm);
	void (*write_byte)(struct nfc_handler *handler, u8 data);
	int (*read_sectors)(struct nfc_handler *handler, int num, u8 *data,
				u8 *fdm);
	u8 (*read_byte)(struct nfc_handler *handler);
	int (*change_interface)(struct nfc_handler *handler,
				enum INTERFACE_TYPE type,
				struct nand_timing *timing, void *arg);
	int (*change_mode)(struct nfc_handler *handler, enum OPS_MODE_TYPE mode,
				bool enable, void *arg);
	bool (*get_mode)(struct nfc_handler *handler, enum OPS_MODE_TYPE mode);
	void (*select_chip)(struct nfc_handler *handler, int cs);
	void (*set_format)(struct nfc_handler *handler,
				struct nfc_format *format);
	void (*enable_randomizer)(struct nfc_handler *handler, u32 page,
					bool encode);
	void (*disable_randomizer)(struct nfc_handler *handler);
	int (*wait_busy)(struct nfc_handler *handler, int timeout,
				enum WAIT_TYPE type);
	int (*calculate_ecc)(struct nfc_handler *handler, u8 *data, u8 *ecc,
				u32 len, u8 ecc_strength);
	int (*correct_ecc)(struct nfc_handler *handler, u8 *data, u32 len,
				u8 ecc_strength);
	int (*calibration)(struct nfc_handler *handler);
	int (*suspend)(struct nfc_handler *handler);
	int (*resume)(struct nfc_handler *handler);
};

extern struct nfc_handler *nfc_init(struct nfc_resource *res);
extern void nfc_exit(struct nfc_handler *handler);

#endif

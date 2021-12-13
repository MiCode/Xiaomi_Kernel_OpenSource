/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __ESE_COLD_RESET_H
#define __ESE_COLD_RESET_H

#include <linux/nfcinfo.h>

#define MAX_BUFF_SIZE 264

/* ESE_COLD_RESET MACROS */
#define COLD_RESET_CMD_LEN		3
#define COLD_RESET_RSP_LEN		4
#define COLD_RESET_PROT_CMD_LEN		4
#define COLD_RESET_PROT_RSP_LEN		4
#define PROP_NCI_CMD_GID		0x2F
#define COLD_RESET_CMD_PL_LEN		0x00
#define COLD_RESET_PROT_CMD_PL_LEN	0x01
#define PROP_NCI_RSP_GID		0x4F
#define COLD_RESET_OID			0x1E
#define COLD_RESET_PROT_OID		0x1F

#define ESE_COLD_RESET _IOWR(NFCC_MAGIC, 0x08, struct ese_ioctl_arg)

enum ese_ioctl_arg_type {
	ESE_ARG_TYPE_COLD_RESET = 0,
};

/* ESE_COLD_RESET ioctl origin, max 4 are supported */
enum ese_cold_reset_origin {
	ESE_COLD_RESET_ORIGIN_ESE = 0,
	ESE_COLD_RESET_ORIGIN_NFC,
	ESE_COLD_RESET_ORIGIN_OTHER = 0x20,
	ESE_COLD_RESET_ORIGIN_NONE = 0xFF,
};

/* ESE_COLD_RESET ioctl sub commands, max 8 are supported */
enum ese_cold_reset_sub_cmd {
	ESE_COLD_RESET_DO = 0,
	ESE_COLD_RESET_PROTECT_EN,
	ESE_COLD_RESET_PROTECT_DIS,
};

/* Data passed in buf of ese cold reset ioctl */
struct ese_cold_reset_arg {
	__u8 src;
	__u8 sub_cmd;
	__u16 rfu;
};

/* Argument buffer passed to ese ioctl */
struct ese_ioctl_arg {
	__u64 buf;
	__u32 buf_size;
	__u8 type;
};

/* Features specific Parameters */
struct cold_reset {
	wait_queue_head_t read_wq;
	char *cmd_buf;
	uint16_t cmd_len;
	uint16_t rsp_len;
	/* Source of last ese protection command */
	uint8_t last_src_ese_prot;
	uint8_t status;
	/* Is cold reset protection enabled */
	bool is_crp_en;
	bool rsp_pending;
	/* Is NFC enabled from UI */
	bool is_nfc_enabled;
};

struct nfc_dev;
int ese_cold_reset_ioctl(struct nfc_dev *nfc_dev, unsigned long arg);
int read_cold_reset_rsp(struct nfc_dev *nfc_dev, char *header);

#endif

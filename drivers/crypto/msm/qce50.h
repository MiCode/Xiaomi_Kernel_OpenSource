/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _DRIVERS_CRYPTO_MSM_QCE50_H_
#define _DRIVERS_CRYPTO_MSM_QCE50_H_

#include <linux/msm-sps.h>

/* MAX Data xfer block size between BAM and CE */
#define MAX_CE_BAM_BURST_SIZE   0x40
#define QCEBAM_BURST_SIZE	MAX_CE_BAM_BURST_SIZE

#define GET_VIRT_ADDR(x)  \
		((uintptr_t)pce_dev->coh_vmem +			\
		((uintptr_t)x - (uintptr_t)pce_dev->coh_pmem))
#define GET_PHYS_ADDR(x)  \
		(phys_addr_t)(((uintptr_t)pce_dev->coh_pmem +	\
		((uintptr_t)x - (uintptr_t)pce_dev->coh_vmem)))

#define CRYPTO_REG_SIZE 4
#define NUM_OF_CRYPTO_AUTH_IV_REG 16
#define NUM_OF_CRYPTO_CNTR_IV_REG 4
#define NUM_OF_CRYPTO_AUTH_BYTE_COUNT_REG 4
#define CRYPTO_TOTAL_REGISTERS_DUMPED   26
#define CRYPTO_RESULT_DUMP_SIZE   \
	ALIGN((CRYPTO_TOTAL_REGISTERS_DUMPED * CRYPTO_REG_SIZE), \
	QCEBAM_BURST_SIZE)

/* QCE max number of descriptor in a descriptor list */
#define QCE_MAX_NUM_DESC    128
#define SPS_MAX_PKT_SIZE  (32 * 1024  - 64)

/* State of consumer/producer Pipe */
enum qce_pipe_st_enum {
	QCE_PIPE_STATE_IDLE = 0,
	QCE_PIPE_STATE_IN_PROG = 1,
	QCE_PIPE_STATE_COMP = 2,
	QCE_PIPE_STATE_LAST
};

enum qce_xfer_type_enum {
	QCE_XFER_HASHING,
	QCE_XFER_CIPHERING,
	QCE_XFER_AEAD,
	QCE_XFER_F8,
	QCE_XFER_F9,
	QCE_XFER_TYPE_LAST
};

struct qce_sps_ep_conn_data {
	struct sps_pipe			*pipe;
	struct sps_connect		connect;
	struct sps_register_event	event;
};

/* CE Result DUMP format*/
struct ce_result_dump_format {
	uint32_t auth_iv[NUM_OF_CRYPTO_AUTH_IV_REG];
	uint32_t auth_byte_count[NUM_OF_CRYPTO_AUTH_BYTE_COUNT_REG];
	uint32_t encr_cntr_iv[NUM_OF_CRYPTO_CNTR_IV_REG];
	uint32_t status;
	uint32_t status2;
};

struct qce_cmdlist_info {

	unsigned long cmdlist;
	struct sps_command_element *crypto_cfg;
	struct sps_command_element *encr_seg_cfg;
	struct sps_command_element *encr_seg_size;
	struct sps_command_element *encr_seg_start;
	struct sps_command_element *encr_key;
	struct sps_command_element *encr_xts_key;
	struct sps_command_element *encr_cntr_iv;
	struct sps_command_element *encr_ccm_cntr_iv;
	struct sps_command_element *encr_mask;
	struct sps_command_element *encr_xts_du_size;

	struct sps_command_element *auth_seg_cfg;
	struct sps_command_element *auth_seg_size;
	struct sps_command_element *auth_seg_start;
	struct sps_command_element *auth_key;
	struct sps_command_element *auth_iv;
	struct sps_command_element *auth_nonce_info;
	struct sps_command_element *auth_bytecount;
	struct sps_command_element *seg_size;
	struct sps_command_element *go_proc;
	ptrdiff_t size;
};

struct qce_cmdlistptr_ops {
	struct qce_cmdlist_info cipher_aes_128_cbc_ctr;
	struct qce_cmdlist_info cipher_aes_256_cbc_ctr;
	struct qce_cmdlist_info cipher_aes_128_ecb;
	struct qce_cmdlist_info cipher_aes_256_ecb;
	struct qce_cmdlist_info cipher_aes_128_xts;
	struct qce_cmdlist_info cipher_aes_256_xts;
	struct qce_cmdlist_info cipher_des_cbc;
	struct qce_cmdlist_info cipher_des_ecb;
	struct qce_cmdlist_info cipher_3des_cbc;
	struct qce_cmdlist_info cipher_3des_ecb;
	struct qce_cmdlist_info auth_sha1;
	struct qce_cmdlist_info auth_sha256;
	struct qce_cmdlist_info auth_sha1_hmac;
	struct qce_cmdlist_info auth_sha256_hmac;
	struct qce_cmdlist_info auth_aes_128_cmac;
	struct qce_cmdlist_info auth_aes_256_cmac;
	struct qce_cmdlist_info aead_hmac_sha1_cbc_aes_128;
	struct qce_cmdlist_info aead_hmac_sha1_cbc_aes_256;
	struct qce_cmdlist_info aead_hmac_sha1_cbc_des;
	struct qce_cmdlist_info aead_hmac_sha1_cbc_3des;
	struct qce_cmdlist_info aead_hmac_sha256_cbc_aes_128;
	struct qce_cmdlist_info aead_hmac_sha256_cbc_aes_256;
	struct qce_cmdlist_info aead_hmac_sha256_cbc_des;
	struct qce_cmdlist_info aead_hmac_sha256_cbc_3des;
	struct qce_cmdlist_info aead_aes_128_ccm;
	struct qce_cmdlist_info aead_aes_256_ccm;
	struct qce_cmdlist_info cipher_null;
	struct qce_cmdlist_info f8_kasumi;
	struct qce_cmdlist_info f8_snow3g;
	struct qce_cmdlist_info f9_kasumi;
	struct qce_cmdlist_info f9_snow3g;
	struct qce_cmdlist_info unlock_all_pipes;
};

struct qce_ce_cfg_reg_setting {
	uint32_t crypto_cfg_be;
	uint32_t crypto_cfg_le;

	uint32_t encr_cfg_aes_cbc_128;
	uint32_t encr_cfg_aes_cbc_256;

	uint32_t encr_cfg_aes_ecb_128;
	uint32_t encr_cfg_aes_ecb_256;

	uint32_t encr_cfg_aes_xts_128;
	uint32_t encr_cfg_aes_xts_256;

	uint32_t encr_cfg_aes_ctr_128;
	uint32_t encr_cfg_aes_ctr_256;

	uint32_t encr_cfg_aes_ccm_128;
	uint32_t encr_cfg_aes_ccm_256;

	uint32_t encr_cfg_des_cbc;
	uint32_t encr_cfg_des_ecb;

	uint32_t encr_cfg_3des_cbc;
	uint32_t encr_cfg_3des_ecb;
	uint32_t encr_cfg_kasumi;
	uint32_t encr_cfg_snow3g;

	uint32_t auth_cfg_cmac_128;
	uint32_t auth_cfg_cmac_256;

	uint32_t auth_cfg_sha1;
	uint32_t auth_cfg_sha256;

	uint32_t auth_cfg_hmac_sha1;
	uint32_t auth_cfg_hmac_sha256;

	uint32_t auth_cfg_aes_ccm_128;
	uint32_t auth_cfg_aes_ccm_256;
	uint32_t auth_cfg_aead_sha1_hmac;
	uint32_t auth_cfg_aead_sha256_hmac;
	uint32_t auth_cfg_kasumi;
	uint32_t auth_cfg_snow3g;
};

struct ce_bam_info {
	uint32_t			bam_irq;
	uint32_t			bam_mem;
	void __iomem			*bam_iobase;
	uint32_t			ce_device;
	uint32_t			ce_hw_instance;
	uint32_t			bam_ee;
	unsigned int			pipe_pair_index;
	unsigned int			src_pipe_index;
	unsigned int			dest_pipe_index;
	unsigned long			bam_handle;
	int				ce_burst_size;
	uint32_t			minor_version;
	struct qce_sps_ep_conn_data	producer;
	struct qce_sps_ep_conn_data	consumer;
};

/* SPS data structure with buffers, commandlists & commmand pointer lists */
struct ce_sps_data {
	enum qce_pipe_st_enum producer_state;	/* Producer pipe state */
	int consumer_status;		/* consumer pipe status */
	int producer_status;		/* producer pipe status */
	struct sps_transfer in_transfer;
	struct sps_transfer out_transfer;
	struct qce_cmdlistptr_ops cmdlistptr;
	uint32_t result_dump; /* reuslt dump virtual address */
	uint32_t result_dump_null;
	uint32_t result_dump_phy; /* result dump physical address (32 bits) */
	uint32_t result_dump_null_phy;

	uint32_t ignore_buffer; /* ignore buffer virtual address */
	struct ce_result_dump_format *result; /* ponter to result dump */
	struct ce_result_dump_format *result_null;
};

struct ce_request_info {
	bool in_use;
	bool in_prog;
	enum qce_xfer_type_enum	xfer_type;
	struct ce_sps_data ce_sps;
	qce_comp_func_ptr_t qce_cb;	/* qce callback function pointer */
	void *user;
	void *areq;
	int assoc_nents;
	int src_nents;
	int dst_nents;
	dma_addr_t phy_iv_in;
	unsigned char dec_iv[16];
	int dir;
	enum qce_cipher_mode_enum mode;
	dma_addr_t phy_ota_src;
	dma_addr_t phy_ota_dst;
	unsigned int ota_size;
};

struct qce_driver_stats {
	int no_of_timeouts;
	int no_of_dummy_reqs;
	int current_mode;
	int outstanding_reqs;
};

#endif /* _DRIVERS_CRYPTO_MSM_QCE50_H */

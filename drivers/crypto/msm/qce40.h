/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#ifndef _DRIVERS_CRYPTO_MSM_QCE40_H_
#define _DRIVERS_CRYPTO_MSM_QCE40_H_


#define GET_VIRT_ADDR(x)  \
		((uint32_t)pce_dev->coh_vmem +			\
		((uint32_t)x - pce_dev->coh_pmem))
#define GET_PHYS_ADDR(x)  \
		(pce_dev->coh_pmem + ((unsigned char *)x -	\
		pce_dev->coh_vmem))

/* Sets the adddress of a command list in command pointer list */
#define QCE_SET_CMD_PTR(x)  \
		(uint32_t)(DMOV_CMD_ADDR(GET_PHYS_ADDR((unsigned char *)x)))

/* Sets the adddress of the last command list in command pointer list */
#define SET_LAST_CMD_PTR(x) \
		((DMOV_CMD_ADDR(x)) | CMD_PTR_LP)

/* Get the adddress of the last command list in command pointer list */
#define QCE_SET_LAST_CMD_PTR(x) \
		SET_LAST_CMD_PTR((GET_PHYS_ADDR((unsigned char *)x)))


/* MAX Data xfer block size between DM and CE */
#define MAX_ADM_CE_BLOCK_SIZE  64
#define ADM_DESC_LENGTH_MASK 0xffff
#define ADM_DESC_LENGTH(x)  (x & ADM_DESC_LENGTH_MASK)

#define ADM_STATUS_OK 0x80000002

/* QCE max number of descriptor in a descriptor list */
#define QCE_MAX_NUM_DESC    128

#define CRYPTO_REG_SIZE                 0x4

struct dmov_desc {
	uint32_t addr;
	uint32_t len;
};

/* State of DM channel */
enum qce_chan_st_enum {
	QCE_CHAN_STATE_IDLE = 0,
	QCE_CHAN_STATE_IN_PROG = 1,
	QCE_CHAN_STATE_COMP = 2,
	QCE_CHAN_STATE_LAST
};

/* CE buffers */
struct ce_reg_buffer_addr {

	unsigned char *reset_buf_64;
	unsigned char *version;

	unsigned char *encr_seg_cfg_size_start;
	unsigned char *encr_key;
	unsigned char *encr_xts_key;
	unsigned char *encr_cntr_iv;
	unsigned char *encr_mask;
	unsigned char *encr_xts_du_size;

	unsigned char *auth_seg_cfg_size_start;
	unsigned char *auth_key;
	unsigned char *auth_iv;
	unsigned char *auth_result;
	unsigned char *auth_nonce_info;
	unsigned char *auth_byte_count;

	unsigned char *seg_size;
	unsigned char *go_proc;
	unsigned char *status;

	unsigned char *pad;
	unsigned char *ignore_data;
};

/* CE buffers */
struct ce_reg_buffers {

	unsigned char reset_buf_64[64];
	unsigned char version[CRYPTO_REG_SIZE];

	unsigned char encr_seg_cfg_size_start[3 * CRYPTO_REG_SIZE];
	unsigned char encr_key[8 * CRYPTO_REG_SIZE];
	unsigned char encr_xts_key[8 * CRYPTO_REG_SIZE];
	unsigned char encr_cntr_iv[4 * CRYPTO_REG_SIZE];
	unsigned char encr_mask[CRYPTO_REG_SIZE];
	unsigned char encr_xts_du_size[CRYPTO_REG_SIZE];

	unsigned char auth_seg_cfg_size_start[3 * CRYPTO_REG_SIZE];
	unsigned char auth_key[16 * CRYPTO_REG_SIZE];
	unsigned char auth_iv[16 * CRYPTO_REG_SIZE];
	unsigned char auth_result[16 * CRYPTO_REG_SIZE];
	unsigned char auth_nonce_info[4 * CRYPTO_REG_SIZE];
	unsigned char auth_byte_count[4 * CRYPTO_REG_SIZE];

	unsigned char seg_size[CRYPTO_REG_SIZE];
	unsigned char go_proc[CRYPTO_REG_SIZE];
	unsigned char status[CRYPTO_REG_SIZE];

	unsigned char pad[2 * MAX_ADM_CE_BLOCK_SIZE];
};

/* CE Command lists */
struct ce_cmdlists {
	dmov_s *get_hw_version;
	dmov_s *clear_status;
	dmov_s *get_status_ocu;

	dmov_s *set_cipher_cfg;

	dmov_s *set_cipher_aes_128_key;
	dmov_s *set_cipher_aes_256_key;
	dmov_s *set_cipher_des_key;
	dmov_s *set_cipher_3des_key;

	dmov_s *set_cipher_aes_128_xts_key;
	dmov_s *set_cipher_aes_256_xts_key;
	dmov_s *set_cipher_xts_du_size;

	dmov_s *set_cipher_aes_iv;
	dmov_s *set_cipher_aes_xts_iv;
	dmov_s *set_cipher_des_iv;
	dmov_s *get_cipher_iv;

	dmov_s *set_cipher_mask;

	dmov_s *set_auth_cfg;
	dmov_s *set_auth_key_128;
	dmov_s *set_auth_key_256;
	dmov_s *set_auth_key_512;
	dmov_s *set_auth_iv_16;
	dmov_s *get_auth_result_16;
	dmov_s *set_auth_iv_20;
	dmov_s *get_auth_result_20;
	dmov_s *set_auth_iv_32;
	dmov_s *get_auth_result_32;
	dmov_s *set_auth_byte_count;
	dmov_s *get_auth_byte_count;

	dmov_s *set_auth_nonce_info;

	dmov_s *reset_cipher_key;
	dmov_s *reset_cipher_xts_key;
	dmov_s *reset_cipher_iv;
	dmov_s *reset_cipher_cfg;
	dmov_s *reset_auth_key;
	dmov_s *reset_auth_iv;
	dmov_s *reset_auth_cfg;
	dmov_s *reset_auth_byte_count;

	dmov_s *set_seg_size_ocb;
	dmov_s *get_status_wait;
	dmov_s *set_go_proc;

	dmov_sg *ce_data_in;
	dmov_sg *ce_data_out;
};

/* Command pointer lists */
struct ce_cmdptrlists_ops {

	uint32_t probe_ce_hw;
	uint32_t cipher_aes_128_cbc_ctr;
	uint32_t cipher_aes_256_cbc_ctr;
	uint32_t cipher_aes_128_ecb;
	uint32_t cipher_aes_256_ecb;
	uint32_t cipher_aes_128_xts;
	uint32_t cipher_aes_256_xts;
	uint32_t cipher_des_cbc;
	uint32_t cipher_des_ecb;
	uint32_t cipher_3des_cbc;
	uint32_t cipher_3des_ecb;
	uint32_t auth_sha1;
	uint32_t auth_sha256;
	uint32_t auth_sha1_hmac;
	uint32_t auth_sha256_hmac;
	uint32_t auth_aes_128_cmac;
	uint32_t auth_aes_256_cmac;
	uint32_t aead_aes_128_ccm;
	uint32_t aead_aes_256_ccm;

	uint32_t cipher_ce_out;
	uint32_t cipher_ce_out_get_iv;
	uint32_t aead_ce_out;
};

/* DM data structure with buffers, commandlists & commmand pointer lists */
struct ce_dm_data {
	unsigned int chan_ce_in;	/* ADM channel used for CE input
					 * and auth result if authentication
					 * only operation. */
	unsigned int chan_ce_out;	/* ADM channel used for CE output,
					 * and icv for esp */

	unsigned int crci_in;		/* CRCI for CE DM IN Channel   */
	unsigned int crci_out;		/* CRCI for CE DM OUT Channel   */

	enum qce_chan_st_enum chan_ce_in_state;		/* chan ce_in state */
	enum qce_chan_st_enum chan_ce_out_state;	/* chan ce_out state */

	int chan_ce_in_status;				/* chan ce_in status */
	int chan_ce_out_status;				/* chan ce_out status */

	struct dmov_desc *ce_out_src_desc;
	struct dmov_desc *ce_out_dst_desc;
	struct dmov_desc *ce_in_src_desc;
	struct dmov_desc *ce_in_dst_desc;

	int ce_out_src_desc_index;
	int ce_out_dst_desc_index;
	int ce_in_src_desc_index;
	int ce_in_dst_desc_index;

	int ce_block_size;

	dma_addr_t phy_ce_out_ignore;
	dma_addr_t phy_ce_pad;

	struct ce_reg_buffer_addr buffer;
	struct ce_cmdlists cmdlist;
	struct ce_cmdptrlists_ops cmdptrlist;

	struct msm_dmov_cmd  *chan_ce_in_cmd;
	struct msm_dmov_cmd  *chan_ce_out_cmd;
};
#endif /* _DRIVERS_CRYPTO_MSM_QCE40_H */

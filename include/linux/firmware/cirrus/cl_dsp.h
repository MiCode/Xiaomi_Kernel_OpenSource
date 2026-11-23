/* SPDX-License-Identifier: GPL-2.0
 *
 * cl_dsp.h -- DSP control for non-ALSA Cirrus Logic devices
 *
 * Copyright 2021 Cirrus Logic, Inc.
 *
 * Author: Fred Treven <fred.treven@cirrus.com>
 */

#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/regmap.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/debugfs.h>
#include <linux/pm_runtime.h>

#ifndef __CL_DSP_H__
#define __CL_DSP_H__

#define CL_DSP_BYTES_PER_WORD		4
#define CL_DSP_BITS_PER_BYTE		8

#define CL_DSP_BYTE_MASK		GENMASK(7, 0)
#define CL_DSP_NIBBLE_MASK		GENMASK(15, 0)

#define CL_DSP_FW_FILE_HEADER_SIZE	40
#define CL_DSP_COEFF_FILE_HEADER_SIZE	16

#define CL_DSP_MAGIC_ID_SIZE		4

#define CL_DSP_WMFW_MAGIC_ID		"WMFW"
#define CL_DSP_WMDR_MAGIC_ID		"WMDR"

#define CL_DSP_DBLK_HEADER_SIZE	8
#define CL_DSP_COEFF_DBLK_HEADER_SIZE	20

#define CL_DSP_ALIGN			0x00000003

#define CL_DSP_TARGET_CORE_ADSP1	0x01
#define CL_DSP_TARGET_CORE_ADSP2	0x02
#define CL_DSP_TARGET_CORE_HALO		0x04
#define CL_DSP_TARGET_CORE_WARP2	0x12
#define CL_DSP_TARGET_CORE_MCU		0x45

#define CL_DSP_MIN_FORMAT_VERSION	0x03
#define CL_DSP_API_REVISION		0x0300

#define CL_DSP_ALGO_NAME_LEN_SIZE	1
#define CL_DSP_ALGO_DESC_LEN_SIZE	2
#define CL_DSP_ALGO_ID_SIZE		4
#define CL_DSP_COEFF_COUNT_SIZE		4
#define CL_DSP_COEFF_OFFSET_SIZE	2
#define CL_DSP_COEFF_TYPE_SIZE		2
#define CL_DSP_COEFF_NAME_LEN_SIZE	1
#define CL_DSP_COEFF_FULLNAME_LEN_SIZE	1
#define CL_DSP_COEFF_DESC_LEN_SIZE	2
#define CL_DSP_COEFF_LEN_SIZE		4
#define CL_DSP_COEFF_FLAGS_SIZE		4
#define CL_DSP_COEFF_FLAGS_SHIFT	16
#define CL_DSP_COEFF_NAME_LEN_MAX	32
#define CL_DSP_COEFF_MIN_FORMAT_VERSION	0x01
#define CL_DSP_COEFF_API_REV_HALO	0x030000
#define CL_DSP_COEFF_API_REV_ADSP2	0x000500

#define CL_DSP_ALGO_LIST_TERM		0xBEDEAD

#define CL_DSP_REV_OFFSET_SHIFT		8

#define CL_DSP_REV_MAJOR_MASK		GENMASK(23, 16)
#define CL_DSP_REV_MAJOR_SHIFT		16
#define CL_DSP_REV_MINOR_MASK		GENMASK(15, 8)
#define CL_DSP_REV_MINOR_SHIFT		8
#define CL_DSP_REV_PATCH_MASK		GENMASK(7, 0)

#define CL_DSP_NUM_ALGOS_MAX		32

#define CL_DSP_MAX_WLEN			4096

#define CL_DSP_XM_UNPACKED_TYPE		0x0005
#define CL_DSP_YM_UNPACKED_TYPE		0x0006
#define CL_DSP_PM_PACKED_TYPE		0x0010
#define CL_DSP_XM_PACKED_TYPE		0x0011
#define CL_DSP_YM_PACKED_TYPE		0x0012
#define CL_DSP_ALGO_INFO_TYPE		0x00F2
#define CL_DSP_WMFW_INFO_TYPE		0x00FF

#define CL_DSP_MEM_REG_TYPE_MASK	GENMASK(27, 20)
#define CL_DSP_MEM_REG_TYPE_SHIFT	20

#define CL_DSP_PM_NUM_BYTES		5
#define CL_DSP_PACKED_NUM_BYTES	3
#define CL_DSP_UNPACKED_NUM_BYTES	4

#define CL_DSP_WMDR_DBLK_OFFSET_SIZE	2
#define CL_DSP_WMDR_DBLK_TYPE_SIZE	2
#define CL_DSP_WMDR_ALGO_ID_SIZE	4
#define CL_DSP_WMDR_ALGO_REV_SIZE	4
#define CL_DSP_WMDR_SAMPLE_RATE_SIZE	4
#define CL_DSP_WMDR_DBLK_LEN_SIZE	4
#define CL_DSP_WMDR_NAME_LEN		32
#define CL_DSP_WMDR_DATE_LEN		16
#define CL_DSP_WMDR_HEADER_LEN_SIZE	4

#define CL_DSP_WMDR_DATE_PREFIX	"Date: "
#define CL_DSP_WMDR_DATE_PREFIX_LEN	6

#define CL_DSP_WMDR_FILE_NAME_MISSING	"N/A"
#define CL_DSP_WMDR_FILE_DATE_MISSING	"N/A"

#define CL_DSP_WMDR_NAME_TYPE		0xFE00
#define CL_DSP_WMDR_INFO_TYPE		0xFF00

/* HALO core */
#define CL_DSP_HALO_XMEM_PACKED_BASE		0x02000000
#define CL_DSP_HALO_XROM_PACKED_BASE		0x02006000
#define CL_DSP_HALO_XMEM_UNPACKED32_BASE	0x02400000
#define CL_DSP_HALO_XMEM_UNPACKED24_BASE	0x02800000
#define CL_DSP_HALO_XROM_UNPACKED24_BASE	0x02808000
#define CL_DSP_HALO_YMEM_PACKED_BASE		0x02C00000
#define CL_DSP_HALO_YMEM_UNPACKED32_BASE	0x03000000
#define CL_DSP_HALO_YMEM_UNPACKED24_BASE	0x03400000
#define CL_DSP_HALO_PMEM_BASE			0x03800000
#define CL_DSP_HALO_PROM_BASE			0x03C60000

#define CL_DSP_HALO_XM_FW_ID_REG		0x0280000C
#define CL_DSP_HALO_NUM_ALGOS_REG		0x02800024

#define CL_DSP_HALO_ALGO_REV_OFFSET	4
#define CL_DSP_HALO_ALGO_XM_BASE_OFFSET	8
#define CL_DSP_HALO_ALGO_XM_SIZE_OFFSET	12
#define CL_DSP_HALO_ALGO_YM_BASE_OFFSET	16
#define CL_DSP_HALO_ALGO_YM_SIZE_OFFSET	20
#define CL_DSP_ALGO_ENTRY_SIZE		24

#define CL_DSP_HALO_FLAG_READ		BIT(0)
#define CL_DSP_HALO_FLAG_WRITE		BIT(1)
#define CL_DSP_HALO_FLAG_VOLATILE	BIT(2)

/* open wavetable */
#define CL_DSP_OWT_HEADER_MAX_LEN		254
#define CL_DSP_OWT_HEADER_ENTRY_SIZE		12

/* Waveform metadata */
#define CL_DSP_SVC_ID			1
#define CL_DSP_SVC_LEN			1
#define CL_DSP_MD_SIZE_MAX_BYTES	28
#define CL_DSP_MD_PRESENT		BIT(10)
#define CL_DSP_MD_TERMINATOR		0xFFFFFF
#define CL_DSP_MD_TYPE_MASK		GENMASK(23, 16)
#define CL_DSP_MD_LENGTH_MASK		GENMASK(15, 8)

/* macros */
#define CL_DSP_WORD_ALIGN(n)	(CL_DSP_BYTES_PER_WORD +\
				(((n) / CL_DSP_BYTES_PER_WORD) *\
				CL_DSP_BYTES_PER_WORD))

#define CL_DSP_GET_MAJOR(n)	(((n) & CL_DSP_REV_MAJOR_MASK) >>\
				CL_DSP_REV_MAJOR_SHIFT)

#define CL_DSP_GET_MINOR(n)	(((n) & CL_DSP_REV_MINOR_MASK) >>\
				CL_DSP_REV_MINOR_SHIFT)

#define CL_DSP_GET_PATCH(n)	((n) & CL_DSP_REV_PATCH_MASK)

#define CL_DSP_MEM_REGION_PREFIX(n)	(((n) & CL_DSP_MEM_REG_TYPE_MASK) >>\
					CL_DSP_MEM_REG_TYPE_SHIFT)

enum cl_dsp_wt_type {
	WT_TYPE_V4_PCM			= 0,
	WT_TYPE_V4_PWLE			= 1,
	WT_TYPE_V4_PCM_F0_REDC		= 2,
	WT_TYPE_V4_PCM_F0_REDC_VAR	= 3,
	WT_TYPE_V4_COMPOSITE		= 4,
	WT_TYPE_V5_PCM_PCM_F0_REDC_Q	= 5,
	WT_TYPE_V5_PWLE_LONG		= 6,
	WT_TYPE_V5_PWLE_LINEAR		= 7,
	WT_TYPE_V6_PCM_F0_REDC		= 8,
	WT_TYPE_V6_PCM_F0_REDC_VAR	= 9,
	WT_TYPE_V6_COMPOSITE		= 10,
	WT_TYPE_V6_PCM_F0_REDC_Q	= 11,
	WT_TYPE_V6_PWLE			= 12,

	WT_TYPE_TERMINATOR		= 0xFF,
};

union cl_dsp_wmdr_header {
	struct {
		char magic[CL_DSP_BYTES_PER_WORD];
		u32 header_len;
		u32 fw_revision : 24;
		u8 file_format_version;
		u32 api_revision : 24;
		u8 target_core;
	} __attribute__((__packed__));
	u8 data[CL_DSP_COEFF_FILE_HEADER_SIZE];
};

union cl_dsp_wmfw_header {
	struct {
		char magic[CL_DSP_BYTES_PER_WORD];
		u32 header_len;
		u16 api_revision;
		u8 target_core;
		u8 file_format_version;
		u32 xm_size;
		u32 ym_size;
		u32 pm_size;
		u32 zm_size;
		u32 timestamp[2];
		u32 checksum;
	} __attribute__((__packed__));
	u8 data[CL_DSP_FW_FILE_HEADER_SIZE];
};

union cl_dsp_data_block_header {
	struct {
		u32 start_offset : 24;
		u8 block_type;
		u32 data_len;
	} __attribute__((__packed__));
	u8 data[CL_DSP_DBLK_HEADER_SIZE];
};

union cl_dsp_coeff_data_block_header {
	struct {
		u16 start_offset;
		u16 block_type;
		u32 algo_id;
		u32 algo_rev;
		u32 sample_rate;
		u32 data_len;
	} __attribute__((__packed__));
	u8 data[CL_DSP_COEFF_DBLK_HEADER_SIZE];
};

struct cl_dsp_data_block {
	union cl_dsp_data_block_header header;
	u8 *payload;
};

struct cl_dsp_coeff_data_block {
	union cl_dsp_coeff_data_block_header header;
	u8 *payload;
};

struct cl_dsp_coeff_desc {
	u32 parent_id;
	char *parent_name;
	u16 block_offset;
	u16 block_type;
	unsigned char name[CL_DSP_COEFF_NAME_LEN_MAX];
	unsigned int reg;
	unsigned int flags;
	unsigned int length;
	struct list_head list;
};

struct cl_dsp_memchunk {
	u8 *data;
	u8 *max;
	int bytes;
	u32 cache;
	int cachebits;
};

struct cl_dsp_owt_header {
	enum cl_dsp_wt_type type;
	u16 flags;
	u32 offset;
	u32 size;
	void *data;
	u32 braking_time;
};

struct cl_dsp_owt_desc {
	struct cl_dsp_owt_header waves[CL_DSP_OWT_HEADER_MAX_LEN];
	int nwaves;
	int bytes;
	u8 *raw_data;
};

struct cl_dsp_wt_desc {
	unsigned int id;
	char wt_name_xm[CL_DSP_WMDR_NAME_LEN];
	char wt_name_ym[CL_DSP_WMDR_NAME_LEN];
	unsigned int wt_limit_xm;
	unsigned int wt_limit_ym;
	char wt_file[CL_DSP_WMDR_NAME_LEN];
	char wt_date[CL_DSP_WMDR_DATE_LEN];
	struct cl_dsp_owt_desc owt;
	bool is_xm;
};

struct cl_dsp_algo_info {
	unsigned int id;
	unsigned int rev;
	unsigned int xm_base;
	unsigned int xm_size;
	unsigned int ym_base;
	unsigned int ym_size;
};

struct cl_dsp {
	struct device *dev;
	struct regmap *regmap;
	struct list_head coeff_desc_head;
	unsigned int num_algos;
	struct cl_dsp_algo_info algo_info[CL_DSP_NUM_ALGOS_MAX + 1];
	const struct cl_dsp_fw_desc *fw_desc;
	const struct cl_dsp_mem_reg_desc *mem_reg_desc;
	const struct cl_dsp_algo_params *algo_params;
	struct cl_dsp_wt_desc *wt_desc;
};

#ifdef CONFIG_DEBUG_FS
/* Debug Logger */
struct cl_dsp_host_buffer {
	__be32 buf1_base;
	__be32 buf1_size;
	__be32 buf2_base;
	__be32 buf1_buf2_size;
	__be32 buf3_base;
	__be32 buf_total_size;
	__be32 high_water_mark;
	__be32 irq_count;
	__be32 irq_ack;
	__be32 next_write_index;
	__be32 next_read_index;
	__be32 error;
	__be32 oldest_block_index;
	__be32 requested_rewind;
	__be32 reserved_space;
	__be32 min_free;
	__be32 blocks_written[2];
	__be32 words_written[2];
} __packed;

struct cl_dsp_logger {
	u32 *buf_data;
	u32 buf_data_size;
	u32 algo_id;
	u32 host_buf_ptr;
	u32 host_buf_base;
	int host_buf_size_words;
	u32 high_watermark;
};

struct cl_dsp_debugfs {
	struct cl_dsp *core;
	struct dentry *debugfs_root;
	struct dentry *debugfs_node;
	struct mutex lock;
	struct cl_dsp_logger dl;
};

#define CL_DSP_DEBUGFS_NUM_CONTROLS		3
#define CL_DSP_DEBUGFS_RW_FILE_MODE		0600
#define CL_DSP_DEBUGFS_RO_FILE_MODE		0400
#define CL_DSP_DEBUGFS_WO_FILE_MOADE		0200
#define CL_DSP_DEBUGFS_TRACE_LOG_STRING_SIZE	3
#define CL_DSP_DEBUGFS_TRACE_LOG_DISABLE	0
#define CL_DSP_DEBUGFS_TRACE_LOG_ENABLE		1

#define CL_DSP_HOST_BUFFER_DATA_MASK		0x00FFFFFFu
#define CL_DSP_HOST_BUFFER_ERROR_OVERFLOW	BIT(0)
#define CL_DSP_HOST_BUFFER_READ_INDEX_RESET	0x00FFFFFF
#define CL_DSP_HOST_BUFFER_IRQ_MASK		BIT(0)
#define CL_DSP_HOST_BUFFER_DATA_SLOT_SIZE	10

#define HOST_BUFFER_FIELD(field) (offsetof(struct cl_dsp_host_buffer, field) / sizeof(__be32))


int cl_dsp_logger_update(struct cl_dsp_debugfs *db);
struct cl_dsp_debugfs *cl_dsp_debugfs_create(struct cl_dsp *dsp,
		struct dentry *parent_node, u32 event_log_algo_id);
void cl_dsp_debugfs_destroy(struct cl_dsp_debugfs *db);

#endif /* CONFIG_DEBUG_FS */

/* Exported Functions */
struct cl_dsp *cl_dsp_create(struct device *dev, struct regmap *regmap);
int cl_dsp_destroy(struct cl_dsp *dsp);
int cl_dsp_wavetable_create(struct cl_dsp *dsp, unsigned int id,
		const char *wt_name_xm, const char *wt_name_ym,
		const char *wt_file);
int cl_dsp_firmware_parse(struct cl_dsp *dsp, const struct firmware *fw,
		bool write_fw);
int cl_dsp_coeff_file_parse(struct cl_dsp *dsp, const struct firmware *fw);
int cl_dsp_get_reg(struct cl_dsp *dsp, const char *coeff_name,
		const unsigned int block_type, const unsigned int algo_id,
		unsigned int *reg);
int cl_dsp_get_flags(struct cl_dsp *dsp, const char *coeff_name,
		const unsigned int block_type, const unsigned int algo_id,
		unsigned int *flags);
int cl_dsp_get_length(struct cl_dsp *dsp, const char *coeff_name,
		const unsigned int block_type, const unsigned int algo_id,
		size_t *length);
bool cl_dsp_algo_is_present(struct cl_dsp *dsp, const unsigned int algo_id);
struct cl_dsp_memchunk cl_dsp_memchunk_create(void *data, int size);
int cl_dsp_memchunk_write(struct cl_dsp_memchunk *ch, int nbits, u32 val);
inline bool cl_dsp_memchunk_end(struct cl_dsp_memchunk *ch);
int cl_dsp_memchunk_read(struct cl_dsp *dsp, struct cl_dsp_memchunk *ch,
		int nbits, void *val);
int cl_dsp_memchunk_flush(struct cl_dsp_memchunk *ch);
int cl_dsp_raw_write(struct cl_dsp *dsp, unsigned int reg,
		const void *val, size_t val_len, size_t limit);
int cl_dsp_fw_id_get(struct cl_dsp *dsp, unsigned int *id);
int cl_dsp_fw_rev_get(struct cl_dsp *dsp, unsigned int *rev);

#endif /* __CL_DSP_H */

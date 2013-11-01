/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/crc16.h>
#include <linux/bitrev.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/ctype.h>
#include <mach/sps.h>
#include <mach/msm_smem.h>
#define PAGE_SIZE_2K 2048
#define PAGE_SIZE_4K 4096
#define WRITE 1
#define READ 0
/*
 * The maximum no of descriptors per transfer (page read/write) won't be more
 * than 64. For more details on what those commands are, please refer to the
 * page read and page write functions in the driver.
 */
#define SPS_MAX_DESC_NUM 64
#define SPS_DATA_CONS_PIPE_INDEX 0
#define SPS_DATA_PROD_PIPE_INDEX 1
#define SPS_CMD_CONS_PIPE_INDEX 2

#define msm_virt_to_dma(chip, vaddr) \
	((chip)->dma_phys_addr + \
	((uint8_t *)(vaddr) - (chip)->dma_virt_addr))

/*
 * A single page read/write request would typically need DMA memory of about
 * 1K memory approximately. So for a single request this memory is more than
 * enough.
 *
 * But to accommodate multiple clients we allocate 8K of memory. Though only
 * one client request can be submitted to NANDc at any time, other clients can
 * still prepare the descriptors while waiting for current client request to
 * be done. Thus for a total memory of 8K, the driver can currently support
 * maximum clients up to 7 or 8 at a time. The client for which there is no
 * free DMA memory shall wait on the wait queue until other clients free up
 * the required memory.
 */
#define MSM_NAND_DMA_BUFFER_SIZE SZ_8K
/*
 * This defines the granularity at which the buffer management is done. The
 * total number of slots is based on the size of the atomic_t variable
 * dma_buffer_busy(number of bits) within the structure msm_nand_chip.
 */
#define MSM_NAND_DMA_BUFFER_SLOT_SZ \
	(MSM_NAND_DMA_BUFFER_SIZE / (sizeof(((atomic_t *)0)->counter) * 8))

/* ONFI(Open NAND Flash Interface) parameters */
#define MSM_NAND_CFG0_RAW_ONFI_IDENTIFIER 0x88000800
#define MSM_NAND_CFG0_RAW_ONFI_PARAM_INFO 0x88040000
#define MSM_NAND_CFG1_RAW_ONFI_IDENTIFIER 0x0005045d
#define MSM_NAND_CFG1_RAW_ONFI_PARAM_INFO 0x0005045d
#define ONFI_PARAM_INFO_LENGTH 0x0200
#define ONFI_PARAM_PAGE_LENGTH 0x0100
#define ONFI_PARAMETER_PAGE_SIGNATURE 0x49464E4F
#define FLASH_READ_ONFI_SIGNATURE_ADDRESS 0x20
#define FLASH_READ_ONFI_PARAMETERS_COMMAND 0xEC
#define FLASH_READ_ONFI_PARAMETERS_ADDRESS 0x00
#define FLASH_READ_DEVICE_ID_ADDRESS 0x00

#define MSM_NAND_RESET_FLASH_STS 0x00000020
#define MSM_NAND_RESET_READ_STS 0x000000C0

/* QPIC NANDc (NAND Controller) Register Set */
#define MSM_NAND_REG(info, off)		    (info->nand_phys + off)
#define MSM_NAND_QPIC_VERSION(info)	    MSM_NAND_REG(info, 0x20100)
#define MSM_NAND_FLASH_CMD(info)	    MSM_NAND_REG(info, 0x30000)
#define MSM_NAND_ADDR0(info)                MSM_NAND_REG(info, 0x30004)
#define MSM_NAND_ADDR1(info)                MSM_NAND_REG(info, 0x30008)
#define MSM_NAND_EXEC_CMD(info)             MSM_NAND_REG(info, 0x30010)
#define MSM_NAND_FLASH_STATUS(info)         MSM_NAND_REG(info, 0x30014)
#define FS_OP_ERR (1 << 4)
#define FS_MPU_ERR (1 << 8)
#define FS_DEVICE_STS_ERR (1 << 16)
#define FS_DEVICE_WP (1 << 23)

#define MSM_NAND_BUFFER_STATUS(info)        MSM_NAND_REG(info, 0x30018)
#define BS_UNCORRECTABLE_BIT (1 << 8)
#define BS_CORRECTABLE_ERR_MSK 0x1F

#define MSM_NAND_DEV0_CFG0(info)            MSM_NAND_REG(info, 0x30020)
#define DISABLE_STATUS_AFTER_WRITE 4
#define CW_PER_PAGE	6
#define UD_SIZE_BYTES	9
#define SPARE_SIZE_BYTES 23
#define NUM_ADDR_CYCLES	27

#define MSM_NAND_DEV0_CFG1(info)            MSM_NAND_REG(info, 0x30024)
#define DEV0_CFG1_ECC_DISABLE	0
#define WIDE_FLASH		1
#define NAND_RECOVERY_CYCLES	2
#define CS_ACTIVE_BSY		5
#define BAD_BLOCK_BYTE_NUM	6
#define BAD_BLOCK_IN_SPARE_AREA 16
#define WR_RD_BSY_GAP		17
#define ENABLE_BCH_ECC		27

#define MSM_NAND_DEV0_ECC_CFG(info)	    MSM_NAND_REG(info, 0x30028)
#define ECC_CFG_ECC_DISABLE	0
#define ECC_SW_RESET	1
#define ECC_MODE	4
#define ECC_PARITY_SIZE_BYTES 8
#define ECC_NUM_DATA_BYTES 16
#define ECC_FORCE_CLK_OPEN 30

#define MSM_NAND_READ_ID(info)              MSM_NAND_REG(info, 0x30040)
#define MSM_NAND_READ_STATUS(info)          MSM_NAND_REG(info, 0x30044)
#define MSM_NAND_DEV_CMD1(info)             MSM_NAND_REG(info, 0x300A4)
#define MSM_NAND_DEV_CMD_VLD(info)          MSM_NAND_REG(info, 0x300AC)
#define MSM_NAND_EBI2_ECC_BUF_CFG(info)     MSM_NAND_REG(info, 0x300F0)
#define MSM_NAND_ERASED_CW_DETECT_CFG(info)	MSM_NAND_REG(info, 0x300E8)
#define MSM_NAND_ERASED_CW_DETECT_STATUS(info)  MSM_NAND_REG(info, 0x300EC)

#define MSM_NAND_CTRL(info)		    MSM_NAND_REG(info, 0x30F00)
#define BAM_MODE_EN	0
#define MSM_NAND_VERSION(info)         MSM_NAND_REG(info, 0x30F08)
#define MSM_NAND_READ_LOCATION_0(info)      MSM_NAND_REG(info, 0x30F20)
#define MSM_NAND_READ_LOCATION_1(info)      MSM_NAND_REG(info, 0x30F24)

/* device commands */
#define MSM_NAND_CMD_PAGE_READ          0x32
#define MSM_NAND_CMD_PAGE_READ_ECC      0x33
#define MSM_NAND_CMD_PAGE_READ_ALL      0x34
#define MSM_NAND_CMD_PRG_PAGE           0x36
#define MSM_NAND_CMD_PRG_PAGE_ECC       0x37
#define MSM_NAND_CMD_PRG_PAGE_ALL       0x39
#define MSM_NAND_CMD_BLOCK_ERASE        0x3A
#define MSM_NAND_CMD_FETCH_ID           0x0B

/* Version Mask */
#define MSM_NAND_VERSION_MAJOR_MASK	0xF0000000
#define MSM_NAND_VERSION_MAJOR_SHIFT	28
#define MSM_NAND_VERSION_MINOR_MASK	0x0FFF0000
#define MSM_NAND_VERSION_MINOR_SHIFT	16

/* Structure that defines a NAND SPS command element */
struct msm_nand_sps_cmd {
	struct sps_command_element ce;
	uint32_t flags;
};

/*
 * Structure that defines the NAND controller properties as per the
 * NAND flash device/chip that is attached.
 */
struct msm_nand_chip {
	struct device *dev;
	/*
	 * DMA memory will be allocated only once during probe and this memory
	 * will be used by all NAND clients. This wait queue is needed to
	 * make the applications wait for DMA memory to be free'd when the
	 * complete memory is exhausted.
	 */
	wait_queue_head_t dma_wait_queue;
	atomic_t dma_buffer_busy;
	uint8_t *dma_virt_addr;
	dma_addr_t dma_phys_addr;
	uint32_t ecc_parity_bytes;
	uint32_t bch_caps; /* Controller BCH ECC capabilities */
#define MSM_NAND_CAP_4_BIT_BCH      (1 << 0)
#define MSM_NAND_CAP_8_BIT_BCH      (1 << 1)
	uint32_t cw_size;
	/* NANDc register configurations */
	uint32_t cfg0, cfg1, cfg0_raw, cfg1_raw;
	uint32_t ecc_buf_cfg;
	uint32_t ecc_bch_cfg;
};

/* Structure that defines an SPS end point for a NANDc BAM pipe. */
struct msm_nand_sps_endpt {
	struct sps_pipe *handle;
	struct sps_connect config;
	struct sps_register_event event;
	struct completion completion;
};

/*
 * Structure that defines NANDc SPS data - BAM handle and an end point
 * for each BAM pipe.
 */
struct msm_nand_sps_info {
	uint32_t bam_handle;
	struct msm_nand_sps_endpt data_prod;
	struct msm_nand_sps_endpt data_cons;
	struct msm_nand_sps_endpt cmd_pipe;
};

/*
 * Structure that contains flash device information. This gets updated after
 * the NAND flash device detection.
 */
struct flash_identification {
	uint32_t flash_id;
	uint32_t density;
	uint32_t widebus;
	uint32_t pagesize;
	uint32_t blksize;
	uint32_t oobsize;
	uint32_t ecc_correctability;
};

/* Structure that defines NANDc private data. */
struct msm_nand_info {
	struct mtd_info		mtd;
	struct msm_nand_chip	nand_chip;
	struct msm_nand_sps_info sps;
	unsigned long bam_phys;
	unsigned long nand_phys;
	void __iomem *bam_base;
	int bam_irq;
	/*
	 * This lock must be acquired before submitting any command or data
	 * descriptors to BAM pipes and must be held until all the submitted
	 * descriptors are processed.
	 *
	 * This is required to ensure that both command and descriptors are
	 * submitted atomically without interruption from other clients,
	 * when there are requests from more than client at any time.
	 * Othewise, data and command descriptors can be submitted out of
	 * order for a request which can cause data corruption.
	 */
	struct mutex bam_lock;
	struct flash_identification flash_dev;
};

/* Structure that defines an ONFI parameter page (512B) */
struct onfi_param_page {
	uint32_t parameter_page_signature;
	uint16_t revision_number;
	uint16_t features_supported;
	uint16_t optional_commands_supported;
	uint8_t  reserved0[22];
	uint8_t  device_manufacturer[12];
	uint8_t  device_model[20];
	uint8_t  jedec_manufacturer_id;
	uint16_t date_code;
	uint8_t  reserved1[13];
	uint32_t number_of_data_bytes_per_page;
	uint16_t number_of_spare_bytes_per_page;
	uint32_t number_of_data_bytes_per_partial_page;
	uint16_t number_of_spare_bytes_per_partial_page;
	uint32_t number_of_pages_per_block;
	uint32_t number_of_blocks_per_logical_unit;
	uint8_t  number_of_logical_units;
	uint8_t  number_of_address_cycles;
	uint8_t  number_of_bits_per_cell;
	uint16_t maximum_bad_blocks_per_logical_unit;
	uint16_t block_endurance;
	uint8_t  guaranteed_valid_begin_blocks;
	uint16_t guaranteed_valid_begin_blocks_endurance;
	uint8_t  number_of_programs_per_page;
	uint8_t  partial_program_attributes;
	uint8_t  number_of_bits_ecc_correctability;
	uint8_t  number_of_interleaved_address_bits;
	uint8_t  interleaved_operation_attributes;
	uint8_t  reserved2[13];
	uint8_t  io_pin_capacitance;
	uint16_t timing_mode_support;
	uint16_t program_cache_timing_mode_support;
	uint16_t maximum_page_programming_time;
	uint16_t maximum_block_erase_time;
	uint16_t maximum_page_read_time;
	uint16_t maximum_change_column_setup_time;
	uint8_t  reserved3[23];
	uint16_t vendor_specific_revision_number;
	uint8_t  vendor_specific[88];
	uint16_t integrity_crc;
} __attribute__((__packed__));

#define FLASH_PART_MAGIC1	0x55EE73AA
#define FLASH_PART_MAGIC2	0xE35EBDDB
#define FLASH_PTABLE_V3		3
#define FLASH_PTABLE_V4		4
#define FLASH_PTABLE_MAX_PARTS_V3 16
#define FLASH_PTABLE_MAX_PARTS_V4 32
#define FLASH_PTABLE_HDR_LEN (4*sizeof(uint32_t))
#define FLASH_PTABLE_ENTRY_NAME_SIZE 16

struct flash_partition_entry {
	char name[FLASH_PTABLE_ENTRY_NAME_SIZE];
	u32 offset;     /* Offset in blocks from beginning of device */
	u32 length;     /* Length of the partition in blocks */
	u8 attr;	/* Flags for this partition */
};

struct flash_partition_table {
	u32 magic1;
	u32 magic2;
	u32 version;
	u32 numparts;
	struct flash_partition_entry part_entry[FLASH_PTABLE_MAX_PARTS_V4];
};

static struct flash_partition_table ptable;

static struct mtd_partition mtd_part[FLASH_PTABLE_MAX_PARTS_V4];

/*
 * Get the DMA memory for requested amount of size. It returns the pointer
 * to free memory available from the allocated pool. Returns NULL if there
 * is no free memory.
 */
static void *msm_nand_get_dma_buffer(struct msm_nand_chip *chip, size_t size)
{
	uint32_t bitmask, free_bitmask, old_bitmask;
	uint32_t need_mask, current_need_mask;
	int free_index;

	need_mask = (1UL << DIV_ROUND_UP(size, MSM_NAND_DMA_BUFFER_SLOT_SZ))
			- 1;
	bitmask = atomic_read(&chip->dma_buffer_busy);
	free_bitmask = ~bitmask;
	do {
		free_index = __ffs(free_bitmask);
		current_need_mask = need_mask << free_index;

		if (size + free_index * MSM_NAND_DMA_BUFFER_SLOT_SZ >=
						 MSM_NAND_DMA_BUFFER_SIZE)
			return NULL;

		if ((bitmask & current_need_mask) == 0) {
			old_bitmask =
				atomic_cmpxchg(&chip->dma_buffer_busy,
					       bitmask,
					       bitmask | current_need_mask);
			if (old_bitmask == bitmask)
				return chip->dma_virt_addr +
				free_index * MSM_NAND_DMA_BUFFER_SLOT_SZ;
			free_bitmask = 0;/* force return */
		}
		/* current free range was too small, clear all free bits */
		/* below the top busy bit within current_need_mask */
		free_bitmask &=
			~(~0U >> (32 - fls(bitmask & current_need_mask)));
	} while (free_bitmask);

	return NULL;
}

/*
 * Releases the DMA memory used to the free pool and also wakes up any user
 * thread waiting on wait queue for free memory to be available.
 */
static void msm_nand_release_dma_buffer(struct msm_nand_chip *chip,
					void *buffer, size_t size)
{
	int index;
	uint32_t used_mask;

	used_mask = (1UL << DIV_ROUND_UP(size, MSM_NAND_DMA_BUFFER_SLOT_SZ))
			- 1;
	index = ((uint8_t *)buffer - chip->dma_virt_addr) /
		MSM_NAND_DMA_BUFFER_SLOT_SZ;
	atomic_sub(used_mask << index, &chip->dma_buffer_busy);

	wake_up(&chip->dma_wait_queue);
}

/*
 * Calculates page address of the buffer passed, offset of buffer within
 * that page and then maps it for DMA by calling dma_map_page().
 */
static dma_addr_t msm_nand_dma_map(struct device *dev, void *addr, size_t size,
					 enum dma_data_direction dir)
{
	struct page *page;
	unsigned long offset = (unsigned long)addr & ~PAGE_MASK;
	if (virt_addr_valid(addr))
		page = virt_to_page(addr);
	else {
		if (WARN_ON(size + offset > PAGE_SIZE))
			return ~0;
		page = vmalloc_to_page(addr);
	}
	return dma_map_page(dev, page, offset, size, dir);
}

/*
 * Wrapper function to prepare a SPS command element with the data that is
 * passed to this function.
 *
 * Since for any command element it is a must to have this flag
 * SPS_IOVEC_FLAG_CMD, this function by default updates this flag for a
 * command element that is passed and thus, the caller need not explicilty
 * pass this flag. The other flags must be passed based on the need.  If a
 * command element doesn't have any other flag, then 0 can be passed to flags.
 */
static inline void msm_nand_prep_ce(struct msm_nand_sps_cmd *sps_cmd,
				uint32_t addr, uint32_t command,
				uint32_t data, uint32_t flags)
{
	struct sps_command_element *cmd = &sps_cmd->ce;

	cmd->addr = addr;
	cmd->command = (command & WRITE) ? (uint32_t) SPS_WRITE_COMMAND :
			(uint32_t) SPS_READ_COMMAND;
	cmd->data = data;
	cmd->mask = 0xFFFFFFFF;
	sps_cmd->flags = SPS_IOVEC_FLAG_CMD | flags;
}

/*
 * Read a single NANDc register as mentioned by its parameter addr. The return
 * value indicates whether read is successful or not. The register value read
 * is stored in val.
 */
static int msm_nand_flash_rd_reg(struct msm_nand_info *info, uint32_t addr,
				uint32_t *val)
{
	int ret = 0;
	struct msm_nand_sps_cmd *cmd;
	struct msm_nand_chip *chip = &info->nand_chip;
	struct {
		struct msm_nand_sps_cmd cmd;
		uint32_t data;
	} *dma_buffer;

	wait_event(chip->dma_wait_queue, (dma_buffer = msm_nand_get_dma_buffer(
		    chip, sizeof(*dma_buffer))));
	cmd = &dma_buffer->cmd;
	msm_nand_prep_ce(cmd, addr, READ, msm_virt_to_dma(chip,
			&dma_buffer->data), SPS_IOVEC_FLAG_INT);

	ret = sps_transfer_one(info->sps.cmd_pipe.handle,
			msm_virt_to_dma(chip, &cmd->ce),
			sizeof(struct sps_command_element), NULL, cmd->flags);
	if (ret) {
		pr_err("failed to submit command %x ret %d\n", addr, ret);
		goto out;
	}
	wait_for_completion_io(&info->sps.cmd_pipe.completion);
	*val = dma_buffer->data;
out:
	msm_nand_release_dma_buffer(chip, dma_buffer, sizeof(*dma_buffer));
	return ret;
}

/*
 * Read the Flash ID from the Nand Flash Device. The return value < 0
 * indicates failure. When successful, the Flash ID is stored in parameter
 * read_id.
 */
static int msm_nand_flash_read_id(struct msm_nand_info *info,
		bool read_onfi_signature,
		uint32_t *read_id)
{
	int err = 0, i;
	struct msm_nand_sps_cmd *cmd;
	struct sps_iovec *iovec;
	struct msm_nand_chip *chip = &info->nand_chip;
	uint32_t total_cnt = 4;
	/*
	 * The following 4 commands are required to read id -
	 * write commands - addr0, flash, exec
	 * read_commands - read_id
	 */
	struct {
		struct sps_transfer xfer;
		struct sps_iovec cmd_iovec[total_cnt];
		struct msm_nand_sps_cmd cmd[total_cnt];
		uint32_t data[total_cnt];
	} *dma_buffer;

	wait_event(chip->dma_wait_queue, (dma_buffer = msm_nand_get_dma_buffer
				(chip, sizeof(*dma_buffer))));
	if (read_onfi_signature)
		dma_buffer->data[0] = FLASH_READ_ONFI_SIGNATURE_ADDRESS;
	else
		dma_buffer->data[0] = FLASH_READ_DEVICE_ID_ADDRESS;

	dma_buffer->data[1] = MSM_NAND_CMD_FETCH_ID;
	dma_buffer->data[2] = 1;
	dma_buffer->data[3] = 0xeeeeeeee;

	cmd = dma_buffer->cmd;
	msm_nand_prep_ce(cmd, MSM_NAND_ADDR0(info), WRITE,
			dma_buffer->data[0], SPS_IOVEC_FLAG_LOCK);
	cmd++;

	msm_nand_prep_ce(cmd, MSM_NAND_FLASH_CMD(info), WRITE,
			dma_buffer->data[1], 0);
	cmd++;

	msm_nand_prep_ce(cmd, MSM_NAND_EXEC_CMD(info), WRITE,
			dma_buffer->data[2], SPS_IOVEC_FLAG_NWD);
	cmd++;

	msm_nand_prep_ce(cmd, MSM_NAND_READ_ID(info), READ,
		msm_virt_to_dma(chip, &dma_buffer->data[3]),
		SPS_IOVEC_FLAG_UNLOCK | SPS_IOVEC_FLAG_INT);
	cmd++;

	BUG_ON(cmd - dma_buffer->cmd > ARRAY_SIZE(dma_buffer->cmd));
	dma_buffer->xfer.iovec_count = (cmd - dma_buffer->cmd);
	dma_buffer->xfer.iovec = dma_buffer->cmd_iovec;
	dma_buffer->xfer.iovec_phys = msm_virt_to_dma(chip,
					&dma_buffer->cmd_iovec);
	iovec = dma_buffer->xfer.iovec;

	for (i = 0; i < dma_buffer->xfer.iovec_count; i++) {
		iovec->addr =  msm_virt_to_dma(chip, &dma_buffer->cmd[i].ce);
		iovec->size = sizeof(struct sps_command_element);
		iovec->flags = dma_buffer->cmd[i].flags;
		iovec++;
	}

	mutex_lock(&info->bam_lock);
	err =  sps_transfer(info->sps.cmd_pipe.handle, &dma_buffer->xfer);
	if (err) {
		pr_err("Failed to submit commands %d\n", err);
		mutex_unlock(&info->bam_lock);
		goto out;
	}
	wait_for_completion_io(&info->sps.cmd_pipe.completion);
	mutex_unlock(&info->bam_lock);

	pr_debug("Read ID register value 0x%x\n", dma_buffer->data[3]);
	if (!read_onfi_signature)
		pr_debug("nandid: %x maker %02x device %02x\n",
		       dma_buffer->data[3], dma_buffer->data[3] & 0xff,
		       (dma_buffer->data[3] >> 8) & 0xff);
	*read_id = dma_buffer->data[3];
out:
	msm_nand_release_dma_buffer(chip, dma_buffer, sizeof(*dma_buffer));
	return err;
}

/*
 * Contains data for common configuration registers that must be programmed
 * for every NANDc operation.
 */
struct msm_nand_common_cfgs {
	uint32_t cmd;
	uint32_t addr0;
	uint32_t addr1;
	uint32_t cfg0;
	uint32_t cfg1;
};

/*
 * Function to prepare SPS command elements to write into NANDc configuration
 * registers as per the data defined in struct msm_nand_common_cfgs. This is
 * required for the following NANDc operations - Erase, Bad Block checking
 * and for reading ONFI parameter page.
 */
static void msm_nand_prep_cfg_cmd_desc(struct msm_nand_info *info,
				struct msm_nand_common_cfgs data,
				struct msm_nand_sps_cmd **curr_cmd)
{
	struct msm_nand_sps_cmd *cmd;

	cmd = *curr_cmd;
	msm_nand_prep_ce(cmd, MSM_NAND_FLASH_CMD(info), WRITE, data.cmd,
			SPS_IOVEC_FLAG_LOCK);
	cmd++;

	msm_nand_prep_ce(cmd, MSM_NAND_ADDR0(info), WRITE, data.addr0, 0);
	cmd++;

	msm_nand_prep_ce(cmd, MSM_NAND_ADDR1(info), WRITE, data.addr1, 0);
	cmd++;

	msm_nand_prep_ce(cmd, MSM_NAND_DEV0_CFG0(info), WRITE, data.cfg0, 0);
	cmd++;

	msm_nand_prep_ce(cmd, MSM_NAND_DEV0_CFG1(info), WRITE, data.cfg1, 0);
	cmd++;
	*curr_cmd = cmd;
}

/*
 * Function to check the CRC integrity check on ONFI parameter page read.
 * For ONFI parameter page read, the controller ECC will be disabled. Hence,
 * it is mandatory to manually compute CRC and check it against the value
 * stored within ONFI page.
 */
static uint16_t msm_nand_flash_onfi_crc_check(uint8_t *buffer, uint16_t count)
{
	int i;
	uint16_t result;

	for (i = 0; i < count; i++)
		buffer[i] = bitrev8(buffer[i]);

	result = bitrev16(crc16(bitrev16(0x4f4e), buffer, count));

	for (i = 0; i < count; i++)
		buffer[i] = bitrev8(buffer[i]);

	return result;
}

/*
 * Structure that contains NANDc register data for commands required
 * for reading ONFI paramter page.
 */
struct msm_nand_flash_onfi_data {
	struct msm_nand_common_cfgs cfg;
	uint32_t exec;
	uint32_t devcmd1_orig;
	uint32_t devcmdvld_orig;
	uint32_t devcmd1_mod;
	uint32_t devcmdvld_mod;
	uint32_t ecc_bch_cfg;
};

struct version {
	uint16_t nand_major;
	uint16_t nand_minor;
	uint16_t qpic_major;
	uint16_t qpic_minor;
};

static int msm_nand_version_check(struct msm_nand_info *info,
			struct version *nandc_version)
{
	uint32_t qpic_ver = 0, nand_ver = 0;
	int err = 0;

	/* Lookup the version to identify supported features */
	err = msm_nand_flash_rd_reg(info, MSM_NAND_VERSION(info),
		&nand_ver);
	if (err) {
		pr_err("Failed to read NAND_VERSION, err=%d\n", err);
		goto out;
	}
	nandc_version->nand_major = (nand_ver & MSM_NAND_VERSION_MAJOR_MASK) >>
		MSM_NAND_VERSION_MAJOR_SHIFT;
	nandc_version->nand_minor = (nand_ver & MSM_NAND_VERSION_MINOR_MASK) >>
		MSM_NAND_VERSION_MINOR_SHIFT;

	err = msm_nand_flash_rd_reg(info, MSM_NAND_QPIC_VERSION(info),
		&qpic_ver);
	if (err) {
		pr_err("Failed to read QPIC_VERSION, err=%d\n", err);
		goto out;
	}
	nandc_version->qpic_major = (qpic_ver & MSM_NAND_VERSION_MAJOR_MASK) >>
			MSM_NAND_VERSION_MAJOR_SHIFT;
	nandc_version->qpic_minor = (qpic_ver & MSM_NAND_VERSION_MINOR_MASK) >>
			MSM_NAND_VERSION_MINOR_SHIFT;
	pr_info("nand_major:%d, nand_minor:%d, qpic_major:%d, qpic_minor:%d\n",
		nandc_version->nand_major, nandc_version->nand_minor,
		nandc_version->qpic_major, nandc_version->qpic_minor);
out:
	return err;
}

/*
 * Function to identify whether the attached NAND flash device is
 * complaint to ONFI spec or not. If yes, then it reads the ONFI parameter
 * page to get the device parameters.
 */
static int msm_nand_flash_onfi_probe(struct msm_nand_info *info)
{
	struct msm_nand_chip *chip = &info->nand_chip;
	struct flash_identification *flash = &info->flash_dev;
	uint32_t crc_chk_count = 0, page_address = 0;
	int ret = 0, i;

	/* SPS parameters */
	struct msm_nand_sps_cmd *cmd, *curr_cmd;
	struct sps_iovec *iovec;
	uint32_t rdata;

	/* ONFI Identifier/Parameter Page parameters */
	uint8_t *onfi_param_info_buf = NULL;
	dma_addr_t dma_addr_param_info = 0;
	struct onfi_param_page *onfi_param_page_ptr;
	struct msm_nand_flash_onfi_data data;
	uint32_t onfi_signature = 0;

	/* SPS command/data descriptors */
	uint32_t total_cnt = 13;
	/*
	 * The following 13 commands are required to get onfi parameters -
	 * flash, addr0, addr1, cfg0, cfg1, dev0_ecc_cfg, cmd_vld, dev_cmd1,
	 * read_loc_0, exec, flash_status (read cmd), dev_cmd1, cmd_vld.
	 */
	struct {
		struct sps_transfer xfer;
		struct sps_iovec cmd_iovec[total_cnt];
		struct msm_nand_sps_cmd cmd[total_cnt];
		uint32_t flash_status;
	} *dma_buffer;


	/* Lookup the version to identify supported features */
	struct version nandc_version = {0};

	ret = msm_nand_version_check(info, &nandc_version);
	if (!ret && !(nandc_version.nand_major == 1 &&
			nandc_version.nand_minor == 1 &&
			nandc_version.qpic_major == 1 &&
			nandc_version.qpic_minor == 1)) {
		ret = -EPERM;
		goto out;
	}
	wait_event(chip->dma_wait_queue, (onfi_param_info_buf =
		msm_nand_get_dma_buffer(chip, ONFI_PARAM_INFO_LENGTH)));
	dma_addr_param_info = msm_virt_to_dma(chip, onfi_param_info_buf);

	wait_event(chip->dma_wait_queue, (dma_buffer = msm_nand_get_dma_buffer
				(chip, sizeof(*dma_buffer))));

	ret = msm_nand_flash_read_id(info, 1, &onfi_signature);
	if (ret < 0) {
		pr_err("Failed to read ONFI signature\n");
		goto free_dma;
	}
	if (onfi_signature != ONFI_PARAMETER_PAGE_SIGNATURE) {
		pr_info("Found a non ONFI device\n");
		ret = -EIO;
		goto free_dma;
	}

	memset(&data, 0, sizeof(struct msm_nand_flash_onfi_data));
	ret = msm_nand_flash_rd_reg(info, MSM_NAND_DEV_CMD1(info),
				&data.devcmd1_orig);
	if (ret < 0)
		goto free_dma;
	ret = msm_nand_flash_rd_reg(info, MSM_NAND_DEV_CMD_VLD(info),
			&data.devcmdvld_orig);
	if (ret < 0)
		goto free_dma;

	/* Lookup the 'APPS' partition's first page address */
	for (i = 0; i < FLASH_PTABLE_MAX_PARTS_V4; i++) {
		if (!strncmp("apps", mtd_part[i].name,
				strlen(mtd_part[i].name))) {
			page_address = mtd_part[i].offset << 6;
			break;
		}
	}
	data.cfg.cmd = MSM_NAND_CMD_PAGE_READ_ALL;
	data.exec = 1;
	data.cfg.addr0 = (page_address << 16) |
				FLASH_READ_ONFI_PARAMETERS_ADDRESS;
	data.cfg.addr1 = (page_address >> 16) & 0xFF;
	data.cfg.cfg0 =	MSM_NAND_CFG0_RAW_ONFI_PARAM_INFO;
	data.cfg.cfg1 = MSM_NAND_CFG1_RAW_ONFI_PARAM_INFO;
	data.devcmd1_mod = (data.devcmd1_orig & 0xFFFFFF00) |
				FLASH_READ_ONFI_PARAMETERS_COMMAND;
	data.devcmdvld_mod = data.devcmdvld_orig & 0xFFFFFFFE;
	data.ecc_bch_cfg = 1 << ECC_CFG_ECC_DISABLE;
	dma_buffer->flash_status = 0xeeeeeeee;

	curr_cmd = cmd = dma_buffer->cmd;
	msm_nand_prep_cfg_cmd_desc(info, data.cfg, &curr_cmd);

	cmd = curr_cmd;
	msm_nand_prep_ce(cmd, MSM_NAND_DEV0_ECC_CFG(info), WRITE,
			data.ecc_bch_cfg, 0);
	cmd++;

	msm_nand_prep_ce(cmd, MSM_NAND_DEV_CMD_VLD(info), WRITE,
			data.devcmdvld_mod, 0);
	cmd++;

	msm_nand_prep_ce(cmd, MSM_NAND_DEV_CMD1(info), WRITE,
			data.devcmd1_mod, 0);
	cmd++;

	rdata = (0 << 0) | (ONFI_PARAM_INFO_LENGTH << 16) | (1 << 31);
	msm_nand_prep_ce(cmd, MSM_NAND_READ_LOCATION_0(info), WRITE,
			rdata, 0);
	cmd++;

	msm_nand_prep_ce(cmd, MSM_NAND_EXEC_CMD(info), WRITE,
		data.exec, SPS_IOVEC_FLAG_NWD);
	cmd++;

	msm_nand_prep_ce(cmd, MSM_NAND_FLASH_STATUS(info), READ,
		msm_virt_to_dma(chip, &dma_buffer->flash_status), 0);
	cmd++;

	msm_nand_prep_ce(cmd, MSM_NAND_DEV_CMD1(info), WRITE,
			data.devcmd1_orig, 0);
	cmd++;

	msm_nand_prep_ce(cmd, MSM_NAND_DEV_CMD_VLD(info), WRITE,
			data.devcmdvld_orig,
			SPS_IOVEC_FLAG_UNLOCK | SPS_IOVEC_FLAG_INT);
	cmd++;

	BUG_ON(cmd - dma_buffer->cmd > ARRAY_SIZE(dma_buffer->cmd));
	dma_buffer->xfer.iovec_count = (cmd - dma_buffer->cmd);
	dma_buffer->xfer.iovec = dma_buffer->cmd_iovec;
	dma_buffer->xfer.iovec_phys = msm_virt_to_dma(chip,
					&dma_buffer->cmd_iovec);
	iovec = dma_buffer->xfer.iovec;

	for (i = 0; i < dma_buffer->xfer.iovec_count; i++) {
		iovec->addr =  msm_virt_to_dma(chip,
				&dma_buffer->cmd[i].ce);
		iovec->size = sizeof(struct sps_command_element);
		iovec->flags = dma_buffer->cmd[i].flags;
		iovec++;
	}
	mutex_lock(&info->bam_lock);
	/* Submit data descriptor */
	ret = sps_transfer_one(info->sps.data_prod.handle, dma_addr_param_info,
			ONFI_PARAM_INFO_LENGTH, NULL, SPS_IOVEC_FLAG_INT);
	if (ret) {
		pr_err("Failed to submit data descriptors %d\n", ret);
		mutex_unlock(&info->bam_lock);
		goto free_dma;
	}
	/* Submit command descriptors */
	ret =  sps_transfer(info->sps.cmd_pipe.handle,
			&dma_buffer->xfer);
	if (ret) {
		pr_err("Failed to submit commands %d\n", ret);
		mutex_unlock(&info->bam_lock);
		goto free_dma;
	}
	wait_for_completion_io(&info->sps.cmd_pipe.completion);
	wait_for_completion_io(&info->sps.data_prod.completion);
	mutex_unlock(&info->bam_lock);

	/* Check for flash status errors */
	if (dma_buffer->flash_status & (FS_OP_ERR | FS_MPU_ERR)) {
		pr_err("MPU/OP err (0x%x) is set\n", dma_buffer->flash_status);
		ret = -EIO;
		goto free_dma;
	}

	for (crc_chk_count = 0; crc_chk_count < ONFI_PARAM_INFO_LENGTH
			/ ONFI_PARAM_PAGE_LENGTH; crc_chk_count++) {
		onfi_param_page_ptr =
			(struct onfi_param_page *)
			(&(onfi_param_info_buf
			[ONFI_PARAM_PAGE_LENGTH *
			crc_chk_count]));
		if (msm_nand_flash_onfi_crc_check(
			(uint8_t *)onfi_param_page_ptr,
			ONFI_PARAM_PAGE_LENGTH - 2) ==
			onfi_param_page_ptr->integrity_crc) {
			break;
		}
	}
	if (crc_chk_count >= ONFI_PARAM_INFO_LENGTH
			/ ONFI_PARAM_PAGE_LENGTH) {
		pr_err("CRC Check failed on param page\n");
		ret = -EIO;
		goto free_dma;
	}
	ret = msm_nand_flash_read_id(info, 0, &flash->flash_id);
	if (ret < 0) {
		pr_err("Failed to read flash ID\n");
		goto free_dma;
	}
	flash->widebus  = onfi_param_page_ptr->features_supported & 0x01;
	flash->pagesize = onfi_param_page_ptr->number_of_data_bytes_per_page;
	flash->blksize  = onfi_param_page_ptr->number_of_pages_per_block *
					flash->pagesize;
	flash->oobsize  = onfi_param_page_ptr->number_of_spare_bytes_per_page;
	flash->density  = onfi_param_page_ptr->number_of_blocks_per_logical_unit
					* flash->blksize;
	flash->ecc_correctability = onfi_param_page_ptr->
					number_of_bits_ecc_correctability;

	pr_info("Found an ONFI compliant device %s\n",
			onfi_param_page_ptr->device_model);
	/*
	 * Temporary hack for MT29F4G08ABC device.
	 * Since the device is not properly adhering
	 * to ONFi specification it is reporting
	 * as 16 bit device though it is 8 bit device!!!
	 */
	if (!strncmp(onfi_param_page_ptr->device_model, "MT29F4G08ABC", 12))
		flash->widebus  = 0;
free_dma:
	msm_nand_release_dma_buffer(chip, dma_buffer, sizeof(*dma_buffer));
	msm_nand_release_dma_buffer(chip, onfi_param_info_buf,
			ONFI_PARAM_INFO_LENGTH);
out:
	return ret;
}

/*
 * Structure that contains read/write parameters required for reading/writing
 * from/to a page.
 */
struct msm_nand_rw_params {
	uint32_t page;
	uint32_t page_count;
	uint32_t sectordatasize;
	uint32_t sectoroobsize;
	uint32_t cwperpage;
	uint32_t oob_len_cmd;
	uint32_t oob_len_data;
	uint32_t start_sector;
	uint32_t oob_col;
	dma_addr_t data_dma_addr;
	dma_addr_t oob_dma_addr;
	dma_addr_t data_dma_addr_curr;
	dma_addr_t oob_dma_addr_curr;
	bool read;
};

/*
 * Structure that contains NANDc register data required for reading/writing
 * from/to a page.
 */
struct msm_nand_rw_reg_data {
	uint32_t cmd;
	uint32_t addr0;
	uint32_t addr1;
	uint32_t cfg0;
	uint32_t cfg1;
	uint32_t ecc_bch_cfg;
	uint32_t exec;
	uint32_t ecc_cfg;
	uint32_t clrfstatus;
	uint32_t clrrstatus;
};

/*
 * Function that validates page read/write MTD parameters received from upper
 * layers such as MTD/YAFFS2 and returns error for any unsupported operations
 * by the driver. In case of success, it also maps the data and oob buffer
 * received for DMA.
 */
static int msm_nand_validate_mtd_params(struct mtd_info *mtd, bool read,
					loff_t offset,
					struct mtd_oob_ops *ops,
					struct msm_nand_rw_params *args)
{
	struct msm_nand_info *info = mtd->priv;
	struct msm_nand_chip *chip = &info->nand_chip;
	int err = 0;

	pr_debug("========================================================\n");
	pr_debug("offset 0x%llx mode %d\ndatbuf 0x%p datlen 0x%x\n",
			offset, ops->mode, ops->datbuf, ops->len);
	pr_debug("oobbuf 0x%p ooblen 0x%x\n", ops->oobbuf, ops->ooblen);

	if (ops->mode == MTD_OPS_PLACE_OOB) {
		pr_err("MTD_OPS_PLACE_OOB is not supported\n");
		err = -EINVAL;
		goto out;
	}

	if (mtd->writesize == PAGE_SIZE_2K)
		args->page = offset >> 11;

	if (mtd->writesize == PAGE_SIZE_4K)
		args->page = offset >> 12;

	args->oob_len_cmd = ops->ooblen;
	args->oob_len_data = ops->ooblen;
	args->cwperpage = (mtd->writesize >> 9);
	args->read = (read ? true : false);

	if (offset & (mtd->writesize - 1)) {
		pr_err("unsupported offset 0x%llx\n", offset);
		err = -EINVAL;
		goto out;
	}

	if (!read && !ops->datbuf) {
		pr_err("No data buffer provided for write!!\n");
		err = -EINVAL;
		goto out;
	}

	if (ops->mode == MTD_OPS_RAW) {
		if (!ops->datbuf) {
			pr_err("No data buffer provided for RAW mode\n");
			err =  -EINVAL;
			goto out;
		} else if ((ops->len % (mtd->writesize +
				mtd->oobsize)) != 0) {
			pr_err("unsupported data len %d for RAW mode\n",
				ops->len);
			err = -EINVAL;
			goto out;
		}
		args->page_count = ops->len / (mtd->writesize + mtd->oobsize);

	} else if (ops->mode == MTD_OPS_AUTO_OOB) {
		if (ops->datbuf && (ops->len % mtd->writesize) != 0) {
			/* when ops->datbuf is NULL, ops->len can be ooblen */
			pr_err("unsupported data len %d for AUTO mode\n",
					ops->len);
			err = -EINVAL;
			goto out;
		}
		if (read && ops->oobbuf && !ops->datbuf) {
			args->start_sector = args->cwperpage - 1;
			args->page_count = ops->ooblen / mtd->oobavail;
			if ((args->page_count == 0) && (ops->ooblen))
				args->page_count = 1;
		} else if (ops->datbuf) {
			args->page_count = ops->len / mtd->writesize;
		}
	}

	if (ops->datbuf) {
		args->data_dma_addr_curr = args->data_dma_addr =
			msm_nand_dma_map(chip->dev, ops->datbuf, ops->len,
				      (read ? DMA_FROM_DEVICE : DMA_TO_DEVICE));
		if (dma_mapping_error(chip->dev, args->data_dma_addr)) {
			pr_err("dma mapping failed for 0x%p\n", ops->datbuf);
			err = -EIO;
			goto out;
		}
	}
	if (ops->oobbuf) {
		if (read)
			memset(ops->oobbuf, 0xFF, ops->ooblen);
		args->oob_dma_addr_curr = args->oob_dma_addr =
			msm_nand_dma_map(chip->dev, ops->oobbuf, ops->ooblen,
				(read ? DMA_BIDIRECTIONAL : DMA_TO_DEVICE));
		if (dma_mapping_error(chip->dev, args->oob_dma_addr)) {
			pr_err("dma mapping failed for 0x%p\n", ops->oobbuf);
			err = -EIO;
			goto dma_map_oobbuf_failed;
		}
	}
	goto out;
dma_map_oobbuf_failed:
	if (ops->datbuf)
		dma_unmap_page(chip->dev, args->data_dma_addr, ops->len,
				(read ? DMA_FROM_DEVICE : DMA_TO_DEVICE));
out:
	return err;
}

/*
 * Function that updates NANDc register data (struct msm_nand_rw_reg_data)
 * required for page read/write.
 */
static void msm_nand_update_rw_reg_data(struct msm_nand_chip *chip,
					struct mtd_oob_ops *ops,
					struct msm_nand_rw_params *args,
					struct msm_nand_rw_reg_data *data)
{
	if (args->read) {
		if (ops->mode != MTD_OPS_RAW) {
			data->cmd = MSM_NAND_CMD_PAGE_READ_ECC;
			data->cfg0 =
			(chip->cfg0 & ~(7U << CW_PER_PAGE)) |
			(((args->cwperpage-1) - args->start_sector)
			 << CW_PER_PAGE);
			data->cfg1 = chip->cfg1;
			data->ecc_bch_cfg = chip->ecc_bch_cfg;
		} else {
			data->cmd = MSM_NAND_CMD_PAGE_READ_ALL;
			data->cfg0 = chip->cfg0_raw;
			data->cfg1 = chip->cfg1_raw;
			data->ecc_bch_cfg = 1 << ECC_CFG_ECC_DISABLE;
		}

	} else {
		if (ops->mode != MTD_OPS_RAW) {
			data->cfg0 = chip->cfg0;
			data->cfg1 = chip->cfg1;
			data->ecc_bch_cfg = chip->ecc_bch_cfg;
		} else {
			data->cfg0 = chip->cfg0_raw;
			data->cfg1 = chip->cfg1_raw;
			data->ecc_bch_cfg = 1 << ECC_CFG_ECC_DISABLE;
		}
		data->cmd = MSM_NAND_CMD_PRG_PAGE;
		data->clrfstatus = MSM_NAND_RESET_FLASH_STS;
		data->clrrstatus = MSM_NAND_RESET_READ_STS;
	}
	data->exec = 1;
	data->ecc_cfg = chip->ecc_buf_cfg;
}

/*
 * Function to prepare series of SPS command descriptors required for a page
 * read/write operation.
 */
static void msm_nand_prep_rw_cmd_desc(struct mtd_oob_ops *ops,
				struct msm_nand_rw_params *args,
				struct msm_nand_rw_reg_data *data,
				struct msm_nand_info *info,
				uint32_t curr_cw,
				struct msm_nand_sps_cmd **curr_cmd)
{
	struct msm_nand_chip *chip = &info->nand_chip;
	struct msm_nand_sps_cmd *cmd;
	uint32_t rdata;
	/* read_location register parameters */
	uint32_t offset, size, last_read;

	cmd = *curr_cmd;
	msm_nand_prep_ce(cmd, MSM_NAND_FLASH_CMD(info), WRITE, data->cmd,
			((curr_cw == args->start_sector) ?
			 SPS_IOVEC_FLAG_LOCK : 0));
	cmd++;

	if (curr_cw == args->start_sector) {
		msm_nand_prep_ce(cmd, MSM_NAND_ADDR0(info), WRITE,
				data->addr0, 0);
		cmd++;

		msm_nand_prep_ce(cmd, MSM_NAND_ADDR1(info), WRITE,
				data->addr1, 0);
		cmd++;

		msm_nand_prep_ce(cmd, MSM_NAND_DEV0_CFG0(info), WRITE,
				data->cfg0, 0);
		cmd++;

		msm_nand_prep_ce(cmd, MSM_NAND_DEV0_CFG1(info), WRITE,
				data->cfg1, 0);
		cmd++;

		msm_nand_prep_ce(cmd, MSM_NAND_DEV0_ECC_CFG(info), WRITE,
				data->ecc_bch_cfg, 0);
		cmd++;

		msm_nand_prep_ce(cmd, MSM_NAND_EBI2_ECC_BUF_CFG(info),
				WRITE, data->ecc_cfg, 0);
		cmd++;
	}

	if (!args->read)
		goto sub_exec_cmd;

	if (ops->mode == MTD_OPS_RAW) {
		rdata = (0 << 0) | (chip->cw_size << 16) | (1 << 31);
		msm_nand_prep_ce(cmd, MSM_NAND_READ_LOCATION_0(info), WRITE,
				rdata, 0);
		cmd++;
	}
	if (ops->mode == MTD_OPS_AUTO_OOB && ops->datbuf) {
		offset = 0;
		size = (curr_cw < (args->cwperpage - 1)) ? 516 :
			(512 - ((args->cwperpage - 1) << 2));
		last_read = (curr_cw < (args->cwperpage - 1)) ? 1 :
			(ops->oobbuf ? 0 : 1);
		rdata = (offset << 0) | (size << 16) | (last_read << 31);
		msm_nand_prep_ce(cmd, MSM_NAND_READ_LOCATION_0(info), WRITE,
				rdata, 0);
		cmd++;
	}
	if (ops->mode == MTD_OPS_AUTO_OOB && ops->oobbuf
			&& (curr_cw == (args->cwperpage - 1))) {
		offset = 512 - ((args->cwperpage - 1) << 2);
		size = (args->cwperpage) << 2;
		if (size > args->oob_len_cmd)
			size = args->oob_len_cmd;
		args->oob_len_cmd -= size;
		last_read = 1;
		rdata = (offset << 0) | (size << 16) | (last_read << 31);
		if (ops->datbuf) {
			msm_nand_prep_ce(cmd, MSM_NAND_READ_LOCATION_1(info),
					WRITE, rdata, 0);
		} else {
			msm_nand_prep_ce(cmd, MSM_NAND_READ_LOCATION_0(info),
					WRITE, rdata, 0);
		}
		cmd++;
	}
sub_exec_cmd:
	msm_nand_prep_ce(cmd, MSM_NAND_EXEC_CMD(info), WRITE, data->exec,
			SPS_IOVEC_FLAG_NWD);
	cmd++;
	*curr_cmd = cmd;
}

/*
 * Function to prepare and submit SPS data descriptors required for a page
 * read/write operation.
 */
static int msm_nand_submit_rw_data_desc(struct mtd_oob_ops *ops,
				struct msm_nand_rw_params *args,
				struct msm_nand_info *info,
				uint32_t curr_cw)
{
	struct msm_nand_chip *chip = &info->nand_chip;
	struct sps_pipe *data_pipe_handle;
	uint32_t sectordatasize, sectoroobsize;
	uint32_t sps_flags = 0;
	int err = 0;

	if (args->read)
		data_pipe_handle = info->sps.data_prod.handle;
	else
		data_pipe_handle = info->sps.data_cons.handle;

	if (ops->mode == MTD_OPS_RAW) {
		sectordatasize = chip->cw_size;
		if (!args->read)
			sps_flags = SPS_IOVEC_FLAG_EOT;
		if (curr_cw == (args->cwperpage - 1))
			sps_flags |= SPS_IOVEC_FLAG_INT;

		err = sps_transfer_one(data_pipe_handle,
				args->data_dma_addr_curr,
				sectordatasize, NULL,
				sps_flags);
		if (err)
			goto out;
		args->data_dma_addr_curr += sectordatasize;

	} else if (ops->mode == MTD_OPS_AUTO_OOB) {
		if (ops->datbuf) {
			sectordatasize = (curr_cw < (args->cwperpage - 1))
			? 516 : (512 - ((args->cwperpage - 1) << 2));

			if (!args->read) {
				sps_flags = SPS_IOVEC_FLAG_EOT;
				if (curr_cw == (args->cwperpage - 1) &&
						ops->oobbuf)
					sps_flags = 0;
			}
			if ((curr_cw == (args->cwperpage - 1)) && !ops->oobbuf)
				sps_flags |= SPS_IOVEC_FLAG_INT;

			err = sps_transfer_one(data_pipe_handle,
					args->data_dma_addr_curr,
					sectordatasize, NULL,
					sps_flags);
			if (err)
				goto out;
			args->data_dma_addr_curr += sectordatasize;
		}

		if (ops->oobbuf && (curr_cw == (args->cwperpage - 1))) {
			sectoroobsize = args->cwperpage << 2;
			if (sectoroobsize > args->oob_len_data)
				sectoroobsize = args->oob_len_data;

			if (!args->read)
				sps_flags |= SPS_IOVEC_FLAG_EOT;
			sps_flags |= SPS_IOVEC_FLAG_INT;
			err = sps_transfer_one(data_pipe_handle,
					args->oob_dma_addr_curr,
					sectoroobsize, NULL,
					sps_flags);
			if (err)
				goto out;
			args->oob_dma_addr_curr += sectoroobsize;
			args->oob_len_data -= sectoroobsize;
		}
	}
out:
	return err;
}

/*
 * Function that gets called from upper layers such as MTD/YAFFS2 to read a
 * page with main or/and spare data.
 */
static int msm_nand_read_oob(struct mtd_info *mtd, loff_t from,
			     struct mtd_oob_ops *ops)
{
	struct msm_nand_info *info = mtd->priv;
	struct msm_nand_chip *chip = &info->nand_chip;
	uint32_t cwperpage = (mtd->writesize >> 9);
	int err, pageerr = 0, rawerr = 0;
	uint32_t n = 0, pages_read = 0;
	uint32_t ecc_errors = 0, total_ecc_errors = 0;
	struct msm_nand_rw_params rw_params;
	struct msm_nand_rw_reg_data data;
	struct msm_nand_sps_cmd *cmd, *curr_cmd;
	struct sps_iovec *iovec;
	/*
	 * The following 6 commands will be sent only once for the first
	 * codeword (CW) - addr0, addr1, dev0_cfg0, dev0_cfg1,
	 * dev0_ecc_cfg, ebi2_ecc_buf_cfg. The following 6 commands will
	 * be sent for every CW - flash, read_location_0, read_location_1,
	 * exec, flash_status and buffer_status.
	 */
	uint32_t total_cnt = (6 * cwperpage) + 6;
	struct {
		struct sps_transfer xfer;
		struct sps_iovec cmd_iovec[total_cnt];
		struct msm_nand_sps_cmd cmd[total_cnt];
		struct {
			uint32_t flash_status;
			uint32_t buffer_status;
		} result[cwperpage];
	} *dma_buffer;

	memset(&rw_params, 0, sizeof(struct msm_nand_rw_params));
	err = msm_nand_validate_mtd_params(mtd, true, from, ops, &rw_params);
	if (err)
		goto validate_mtd_params_failed;

	wait_event(chip->dma_wait_queue, (dma_buffer = msm_nand_get_dma_buffer(
			    chip, sizeof(*dma_buffer))));

	rw_params.oob_col = rw_params.start_sector * chip->cw_size;
	if (chip->cfg1 & (1 << WIDE_FLASH))
		rw_params.oob_col >>= 1;

	memset(&data, 0, sizeof(struct msm_nand_rw_reg_data));
	msm_nand_update_rw_reg_data(chip, ops, &rw_params, &data);

	while (rw_params.page_count-- > 0) {
		data.addr0 = (rw_params.page << 16) | rw_params.oob_col;
		data.addr1 = (rw_params.page >> 16) & 0xff;
		cmd = dma_buffer->cmd;
		for (n = rw_params.start_sector; n < cwperpage; n++) {
			dma_buffer->result[n].flash_status = 0xeeeeeeee;
			dma_buffer->result[n].buffer_status = 0xeeeeeeee;

			curr_cmd = cmd;
			msm_nand_prep_rw_cmd_desc(ops, &rw_params,
					&data, info, n, &curr_cmd);

			cmd = curr_cmd;
			msm_nand_prep_ce(cmd, MSM_NAND_FLASH_STATUS(info),
				READ, msm_virt_to_dma(chip,
				&dma_buffer->result[n].flash_status), 0);
			cmd++;

			msm_nand_prep_ce(cmd, MSM_NAND_BUFFER_STATUS(info),
				READ, msm_virt_to_dma(chip,
				&dma_buffer->result[n].buffer_status),
				((n == (cwperpage - 1)) ?
				(SPS_IOVEC_FLAG_UNLOCK | SPS_IOVEC_FLAG_INT) :
				0));
			cmd++;
		}

		BUG_ON(cmd - dma_buffer->cmd > ARRAY_SIZE(dma_buffer->cmd));
		dma_buffer->xfer.iovec_count = (cmd - dma_buffer->cmd);
		dma_buffer->xfer.iovec = dma_buffer->cmd_iovec;
		dma_buffer->xfer.iovec_phys = msm_virt_to_dma(chip,
						&dma_buffer->cmd_iovec);
		iovec = dma_buffer->xfer.iovec;

		for (n = 0; n < dma_buffer->xfer.iovec_count; n++) {
			iovec->addr =  msm_virt_to_dma(chip,
					&dma_buffer->cmd[n].ce);
			iovec->size = sizeof(struct sps_command_element);
			iovec->flags = dma_buffer->cmd[n].flags;
			iovec++;
		}
		mutex_lock(&info->bam_lock);
		/* Submit data descriptors */
		for (n = rw_params.start_sector; n < cwperpage; n++) {
			err = msm_nand_submit_rw_data_desc(ops,
						&rw_params, info, n);
			if (err) {
				pr_err("Failed to submit data descs %d\n", err);
				mutex_unlock(&info->bam_lock);
				goto free_dma;
			}
		}
		/* Submit command descriptors */
		err =  sps_transfer(info->sps.cmd_pipe.handle,
				&dma_buffer->xfer);
		if (err) {
			pr_err("Failed to submit commands %d\n", err);
			mutex_unlock(&info->bam_lock);
			goto free_dma;
		}
		wait_for_completion_io(&info->sps.cmd_pipe.completion);
		wait_for_completion_io(&info->sps.data_prod.completion);
		mutex_unlock(&info->bam_lock);
		/* Check for flash status errors */
		pageerr = rawerr = 0;
		for (n = rw_params.start_sector; n < cwperpage; n++) {
			if (dma_buffer->result[n].flash_status & (FS_OP_ERR |
					FS_MPU_ERR)) {
				rawerr = -EIO;
				break;
			}
		}
		/* Check for ECC correction on empty block */
		if (rawerr && ops->datbuf && ops->mode != MTD_OPS_RAW) {
			uint8_t *datbuf = ops->datbuf +
				pages_read * mtd->writesize;

			dma_sync_single_for_cpu(chip->dev,
			rw_params.data_dma_addr_curr - mtd->writesize,
			mtd->writesize, DMA_BIDIRECTIONAL);

			for (n = 0; n < mtd->writesize; n++) {
				/* TODO: check offset for 4bit BCHECC */
				if ((n % 516 == 3 || n % 516 == 175)
						&& datbuf[n] == 0x54)
					datbuf[n] = 0xff;
				if (datbuf[n] != 0xff) {
					pageerr = rawerr;
					break;
				}
			}

			dma_sync_single_for_device(chip->dev,
			rw_params.data_dma_addr_curr - mtd->writesize,
			mtd->writesize, DMA_BIDIRECTIONAL);
		}
		if (rawerr && ops->oobbuf) {
			dma_sync_single_for_cpu(chip->dev,
			rw_params.oob_dma_addr_curr - (ops->ooblen -
			rw_params.oob_len_data),
			ops->ooblen - rw_params.oob_len_data,
			DMA_BIDIRECTIONAL);

			for (n = 0; n < ops->ooblen; n++) {
				if (ops->oobbuf[n] != 0xff) {
					pageerr = rawerr;
					break;
				}
			}

			dma_sync_single_for_device(chip->dev,
			rw_params.oob_dma_addr_curr - (ops->ooblen -
			rw_params.oob_len_data),
			ops->ooblen - rw_params.oob_len_data,
			DMA_BIDIRECTIONAL);
		}
		/* check for uncorrectable errors */
		if (pageerr) {
			for (n = rw_params.start_sector; n < cwperpage; n++) {
				if (dma_buffer->result[n].buffer_status &
					BS_UNCORRECTABLE_BIT) {
					mtd->ecc_stats.failed++;
					pageerr = -EBADMSG;
					break;
				}
			}
		}
		/* check for correctable errors */
		if (!rawerr) {
			for (n = rw_params.start_sector; n < cwperpage; n++) {
				ecc_errors =
				    dma_buffer->result[n].buffer_status
				    & BS_CORRECTABLE_ERR_MSK;
				if (ecc_errors) {
					total_ecc_errors += ecc_errors;
					mtd->ecc_stats.corrected += ecc_errors;
					/*
					 * For Micron devices it is observed
					 * that correctable errors upto 3 bits
					 * are very common.
					 */
					if (ecc_errors > 3)
						pageerr = -EUCLEAN;
				}
			}
		}
		if (pageerr && (pageerr != -EUCLEAN || err == 0))
			err = pageerr;

		if (rawerr && !pageerr) {
			pr_debug("%llx %x %x empty page\n",
			       (loff_t)rw_params.page * mtd->writesize,
			       ops->len, ops->ooblen);
		} else {
			for (n = rw_params.start_sector; n < cwperpage; n++)
				pr_debug("cw %d: flash_sts %x buffr_sts %x\n",
				n, dma_buffer->result[n].flash_status,
				dma_buffer->result[n].buffer_status);
		}
		if (err && err != -EUCLEAN && err != -EBADMSG)
			goto free_dma;
		pages_read++;
		rw_params.page++;
	}
free_dma:
	msm_nand_release_dma_buffer(chip, dma_buffer, sizeof(*dma_buffer));
	if (ops->oobbuf)
		dma_unmap_page(chip->dev, rw_params.oob_dma_addr,
				 ops->ooblen, DMA_FROM_DEVICE);
	if (ops->datbuf)
		dma_unmap_page(chip->dev, rw_params.data_dma_addr,
				 ops->len, DMA_BIDIRECTIONAL);
validate_mtd_params_failed:
	if (ops->mode != MTD_OPS_RAW)
		ops->retlen = mtd->writesize * pages_read;
	else
		ops->retlen = (mtd->writesize +  mtd->oobsize) * pages_read;
	ops->oobretlen = ops->ooblen - rw_params.oob_len_data;
	if (err)
		pr_err("0x%llx datalen 0x%x ooblen %x err %d corrected %d\n",
		       from, ops->datbuf ? ops->len : 0, ops->ooblen, err,
		       total_ecc_errors);
	pr_debug("ret %d, retlen %d oobretlen %d\n",
			err, ops->retlen, ops->oobretlen);

	pr_debug("========================================================\n");
	return err;
}

/*
 * Function that gets called from upper layers such as MTD/YAFFS2 to read a
 * page with only main data.
 */
static int msm_nand_read(struct mtd_info *mtd, loff_t from, size_t len,
	      size_t *retlen, u_char *buf)
{
	int ret;
	struct mtd_oob_ops ops;

	ops.mode = MTD_OPS_AUTO_OOB;
	ops.len = len;
	ops.retlen = 0;
	ops.ooblen = 0;
	ops.datbuf = buf;
	ops.oobbuf = NULL;
	ret =  msm_nand_read_oob(mtd, from, &ops);
	*retlen = ops.retlen;
	return ret;
}

/*
 * Function that gets called from upper layers such as MTD/YAFFS2 to write a
 * page with both main and spare data.
 */
static int msm_nand_write_oob(struct mtd_info *mtd, loff_t to,
				struct mtd_oob_ops *ops)
{
	struct msm_nand_info *info = mtd->priv;
	struct msm_nand_chip *chip = &info->nand_chip;
	uint32_t cwperpage = (mtd->writesize >> 9);
	uint32_t n, flash_sts, pages_written = 0;
	int err = 0;
	struct msm_nand_rw_params rw_params;
	struct msm_nand_rw_reg_data data;
	struct msm_nand_sps_cmd *cmd, *curr_cmd;
	struct sps_iovec *iovec;
	/*
	 * The following 7 commands will be sent only once :
	 * For first codeword (CW) - addr0, addr1, dev0_cfg0, dev0_cfg1,
	 * dev0_ecc_cfg, ebi2_ecc_buf_cfg.
	 * For last codeword (CW) - read_status(write)
	 *
	 * The following 4 commands will be sent for every CW :
	 * flash, exec, flash_status (read), flash_status (write).
	 */
	uint32_t total_cnt = (4 * cwperpage) + 7;
	struct {
		struct sps_transfer xfer;
		struct sps_iovec cmd_iovec[total_cnt];
		struct msm_nand_sps_cmd cmd[total_cnt];
		struct {
			uint32_t flash_status[cwperpage];
		} data;
	} *dma_buffer;

	memset(&rw_params, 0, sizeof(struct msm_nand_rw_params));
	err = msm_nand_validate_mtd_params(mtd, false, to, ops, &rw_params);
	if (err)
		goto validate_mtd_params_failed;

	wait_event(chip->dma_wait_queue, (dma_buffer =
			msm_nand_get_dma_buffer(chip, sizeof(*dma_buffer))));

	memset(&data, 0, sizeof(struct msm_nand_rw_reg_data));
	msm_nand_update_rw_reg_data(chip, ops, &rw_params, &data);

	while (rw_params.page_count-- > 0) {
		data.addr0 = (rw_params.page << 16);
		data.addr1 = (rw_params.page >> 16) & 0xff;
		cmd = dma_buffer->cmd;

		for (n = 0; n < cwperpage ; n++) {
			dma_buffer->data.flash_status[n] = 0xeeeeeeee;

			curr_cmd = cmd;
			msm_nand_prep_rw_cmd_desc(ops, &rw_params,
					&data, info, n, &curr_cmd);

			cmd = curr_cmd;
			msm_nand_prep_ce(cmd, MSM_NAND_FLASH_STATUS(info),
				READ, msm_virt_to_dma(chip,
				&dma_buffer->data.flash_status[n]), 0);
			cmd++;

			msm_nand_prep_ce(cmd, MSM_NAND_FLASH_STATUS(info),
				WRITE, data.clrfstatus, 0);
			cmd++;

			if (n == (cwperpage - 1)) {
				msm_nand_prep_ce(cmd,
					MSM_NAND_READ_STATUS(info), WRITE,
					data.clrrstatus, SPS_IOVEC_FLAG_UNLOCK
					| SPS_IOVEC_FLAG_INT);
				cmd++;
			}
		}

		BUG_ON(cmd - dma_buffer->cmd > ARRAY_SIZE(dma_buffer->cmd));
		dma_buffer->xfer.iovec_count = (cmd - dma_buffer->cmd);
		dma_buffer->xfer.iovec = dma_buffer->cmd_iovec;
		dma_buffer->xfer.iovec_phys = msm_virt_to_dma(chip,
						&dma_buffer->cmd_iovec);
		iovec = dma_buffer->xfer.iovec;

		for (n = 0; n < dma_buffer->xfer.iovec_count; n++) {
			iovec->addr =  msm_virt_to_dma(chip,
					&dma_buffer->cmd[n].ce);
			iovec->size = sizeof(struct sps_command_element);
			iovec->flags = dma_buffer->cmd[n].flags;
			iovec++;
		}
		mutex_lock(&info->bam_lock);
		/* Submit data descriptors */
		for (n = 0; n < cwperpage; n++) {
			err = msm_nand_submit_rw_data_desc(ops,
						&rw_params, info, n);
			if (err) {
				pr_err("Failed to submit data descs %d\n", err);
				mutex_unlock(&info->bam_lock);
				goto free_dma;
			}
		}
		/* Submit command descriptors */
		err =  sps_transfer(info->sps.cmd_pipe.handle,
				&dma_buffer->xfer);
		if (err) {
			pr_err("Failed to submit commands %d\n", err);
			mutex_unlock(&info->bam_lock);
			goto free_dma;
		}
		wait_for_completion_io(&info->sps.cmd_pipe.completion);
		wait_for_completion_io(&info->sps.data_cons.completion);
		mutex_unlock(&info->bam_lock);

		for (n = 0; n < cwperpage; n++)
			pr_debug("write pg %d: flash_status[%d] = %x\n",
				rw_params.page, n,
				dma_buffer->data.flash_status[n]);

		/*  Check for flash status errors */
		for (n = 0; n < cwperpage; n++) {
			flash_sts = dma_buffer->data.flash_status[n];
			if (flash_sts & (FS_OP_ERR | FS_MPU_ERR)) {
				pr_err("MPU/OP err (0x%x) set\n", flash_sts);
				err = -EIO;
				goto free_dma;
			}
			if (n == (cwperpage - 1)) {
				if (!(flash_sts & FS_DEVICE_WP) ||
					(flash_sts & FS_DEVICE_STS_ERR)) {
					pr_err("Dev sts err 0x%x\n", flash_sts);
					err = -EIO;
					goto free_dma;
				}
			}
		}
		pages_written++;
		rw_params.page++;
	}
free_dma:
	msm_nand_release_dma_buffer(chip, dma_buffer, sizeof(*dma_buffer));
	if (ops->oobbuf)
		dma_unmap_page(chip->dev, rw_params.oob_dma_addr,
				 ops->ooblen, DMA_TO_DEVICE);
	if (ops->datbuf)
		dma_unmap_page(chip->dev, rw_params.data_dma_addr,
				ops->len, DMA_TO_DEVICE);
validate_mtd_params_failed:
	if (ops->mode != MTD_OPS_RAW)
		ops->retlen = mtd->writesize * pages_written;
	else
		ops->retlen = (mtd->writesize + mtd->oobsize) * pages_written;

	ops->oobretlen = ops->ooblen - rw_params.oob_len_data;
	if (err)
		pr_err("to %llx datalen %x ooblen %x failed with err %d\n",
		       to, ops->len, ops->ooblen, err);
	pr_debug("ret %d, retlen %d oobretlen %d\n",
			err, ops->retlen, ops->oobretlen);

	pr_debug("================================================\n");
	return err;
}

/*
 * Function that gets called from upper layers such as MTD/YAFFS2 to write a
 * page with only main data.
 */
static int msm_nand_write(struct mtd_info *mtd, loff_t to, size_t len,
			  size_t *retlen, const u_char *buf)
{
	int ret;
	struct mtd_oob_ops ops;

	ops.mode = MTD_OPS_AUTO_OOB;
	ops.len = len;
	ops.retlen = 0;
	ops.ooblen = 0;
	ops.datbuf = (uint8_t *)buf;
	ops.oobbuf = NULL;
	ret =  msm_nand_write_oob(mtd, to, &ops);
	*retlen = ops.retlen;
	return ret;
}

/*
 * Structure that contains NANDc register data for commands required
 * for Erase operation.
 */
struct msm_nand_erase_reg_data {
	struct msm_nand_common_cfgs cfg;
	uint32_t exec;
	uint32_t flash_status;
	uint32_t clrfstatus;
	uint32_t clrrstatus;
};

/*
 * Function that gets called from upper layers such as MTD/YAFFS2 to erase a
 * block within NAND device.
 */
static int msm_nand_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	int i, err = 0;
	struct msm_nand_info *info = mtd->priv;
	struct msm_nand_chip *chip = &info->nand_chip;
	uint32_t page = 0;
	struct msm_nand_sps_cmd *cmd, *curr_cmd;
	struct msm_nand_erase_reg_data data;
	struct sps_iovec *iovec;
	uint32_t total_cnt = 9;
	/*
	 * The following 9 commands are required to erase a page -
	 * flash, addr0, addr1, cfg0, cfg1, exec, flash_status(read),
	 * flash_status(write), read_status.
	 */
	struct {
		struct sps_transfer xfer;
		struct sps_iovec cmd_iovec[total_cnt];
		struct msm_nand_sps_cmd cmd[total_cnt];
		uint32_t flash_status;
	} *dma_buffer;

	if (mtd->writesize == PAGE_SIZE_2K)
		page = instr->addr >> 11;

	if (mtd->writesize == PAGE_SIZE_4K)
		page = instr->addr >> 12;

	if (instr->addr & (mtd->erasesize - 1)) {
		pr_err("unsupported erase address, 0x%llx\n", instr->addr);
		err = -EINVAL;
		goto out;
	}
	if (instr->len != mtd->erasesize) {
		pr_err("unsupported erase len, %lld\n", instr->len);
		err = -EINVAL;
		goto out;
	}

	wait_event(chip->dma_wait_queue, (dma_buffer = msm_nand_get_dma_buffer(
			    chip, sizeof(*dma_buffer))));
	cmd = dma_buffer->cmd;

	memset(&data, 0, sizeof(struct msm_nand_erase_reg_data));
	data.cfg.cmd = MSM_NAND_CMD_BLOCK_ERASE;
	data.cfg.addr0 = page;
	data.cfg.addr1 = 0;
	data.cfg.cfg0 = chip->cfg0 & (~(7 << CW_PER_PAGE));
	data.cfg.cfg1 = chip->cfg1;
	data.exec = 1;
	dma_buffer->flash_status = 0xeeeeeeee;
	data.clrfstatus = MSM_NAND_RESET_FLASH_STS;
	data.clrrstatus = MSM_NAND_RESET_READ_STS;

	curr_cmd = cmd;
	msm_nand_prep_cfg_cmd_desc(info, data.cfg, &curr_cmd);

	cmd = curr_cmd;
	msm_nand_prep_ce(cmd, MSM_NAND_EXEC_CMD(info), WRITE, data.exec,
			SPS_IOVEC_FLAG_NWD);
	cmd++;

	msm_nand_prep_ce(cmd, MSM_NAND_FLASH_STATUS(info), READ,
		msm_virt_to_dma(chip, &dma_buffer->flash_status), 0);
	cmd++;

	msm_nand_prep_ce(cmd, MSM_NAND_FLASH_STATUS(info), WRITE,
			data.clrfstatus, 0);
	cmd++;

	msm_nand_prep_ce(cmd, MSM_NAND_READ_STATUS(info), WRITE,
			data.clrrstatus,
			SPS_IOVEC_FLAG_UNLOCK | SPS_IOVEC_FLAG_INT);
	cmd++;

	BUG_ON(cmd - dma_buffer->cmd > ARRAY_SIZE(dma_buffer->cmd));
	dma_buffer->xfer.iovec_count = (cmd - dma_buffer->cmd);
	dma_buffer->xfer.iovec = dma_buffer->cmd_iovec;
	dma_buffer->xfer.iovec_phys = msm_virt_to_dma(chip,
					&dma_buffer->cmd_iovec);
	iovec = dma_buffer->xfer.iovec;

	for (i = 0; i < dma_buffer->xfer.iovec_count; i++) {
		iovec->addr =  msm_virt_to_dma(chip, &dma_buffer->cmd[i].ce);
		iovec->size = sizeof(struct sps_command_element);
		iovec->flags = dma_buffer->cmd[i].flags;
		iovec++;
	}
	mutex_lock(&info->bam_lock);
	err =  sps_transfer(info->sps.cmd_pipe.handle, &dma_buffer->xfer);
	if (err) {
		pr_err("Failed to submit commands %d\n", err);
		mutex_unlock(&info->bam_lock);
		goto free_dma;
	}
	wait_for_completion_io(&info->sps.cmd_pipe.completion);
	mutex_unlock(&info->bam_lock);

	/*  Check for flash status errors */
	if (dma_buffer->flash_status & (FS_OP_ERR |
			FS_MPU_ERR | FS_DEVICE_STS_ERR)) {
		pr_err("MPU/OP/DEV err (0x%x) set\n", dma_buffer->flash_status);
		err = -EIO;
	}
	if (!(dma_buffer->flash_status & FS_DEVICE_WP)) {
		pr_err("Device is write protected\n");
		err = -EIO;
	}
	if (err) {
		pr_err("Erase failed, 0x%llx\n", instr->addr);
		instr->fail_addr = instr->addr;
		instr->state = MTD_ERASE_FAILED;
	} else {
		instr->state = MTD_ERASE_DONE;
		instr->fail_addr = 0xffffffff;
		mtd_erase_callback(instr);
	}
free_dma:
	msm_nand_release_dma_buffer(chip, dma_buffer, sizeof(*dma_buffer));
out:
	return err;
}

/*
 * Structure that contains NANDc register data for commands required
 * for checking if a block is bad.
 */
struct msm_nand_blk_isbad_data {
	struct msm_nand_common_cfgs cfg;
	uint32_t ecc_bch_cfg;
	uint32_t exec;
	uint32_t read_offset;
};

/*
 * Function that gets called from upper layers such as MTD/YAFFS2 to check if
 * a block is bad. This is done by reading the first page within a block and
 * checking whether the bad block byte location contains 0xFF or not. If it
 * doesn't contain 0xFF, then it is considered as bad block.
 */
static int msm_nand_block_isbad(struct mtd_info *mtd, loff_t ofs)
{
	struct msm_nand_info *info = mtd->priv;
	struct msm_nand_chip *chip = &info->nand_chip;
	int i, ret = 0, bad_block = 0;
	uint8_t *buf;
	uint32_t page = 0, rdata, cwperpage;
	struct msm_nand_sps_cmd *cmd, *curr_cmd;
	struct msm_nand_blk_isbad_data data;
	struct sps_iovec *iovec;
	uint32_t total_cnt = 9;
	/*
	 * The following 9 commands are required to check bad block -
	 * flash, addr0, addr1, cfg0, cfg1, ecc_cfg, read_loc_0,
	 * exec, flash_status(read).
	 */
	struct {
		struct sps_transfer xfer;
		struct sps_iovec cmd_iovec[total_cnt];
		struct msm_nand_sps_cmd cmd[total_cnt];
		uint32_t flash_status;
	} *dma_buffer;

	if (mtd->writesize == PAGE_SIZE_2K)
		page = ofs >> 11;

	if (mtd->writesize == PAGE_SIZE_4K)
		page = ofs >> 12;

	cwperpage = (mtd->writesize >> 9);

	if (ofs > mtd->size) {
		pr_err("Invalid offset 0x%llx\n", ofs);
		bad_block = -EINVAL;
		goto out;
	}
	if (ofs & (mtd->erasesize - 1)) {
		pr_err("unsupported block address, 0x%x\n", (uint32_t)ofs);
		bad_block = -EINVAL;
		goto out;
	}

	wait_event(chip->dma_wait_queue, (dma_buffer = msm_nand_get_dma_buffer(
				chip , sizeof(*dma_buffer) + 4)));
	buf = (uint8_t *)dma_buffer + sizeof(*dma_buffer);

	cmd = dma_buffer->cmd;
	memset(&data, 0, sizeof(struct msm_nand_blk_isbad_data));
	data.cfg.cmd = MSM_NAND_CMD_PAGE_READ_ALL;
	data.cfg.cfg0 = chip->cfg0_raw & ~(7U << CW_PER_PAGE);
	data.cfg.cfg1 = chip->cfg1_raw;

	if (chip->cfg1 & (1 << WIDE_FLASH))
		data.cfg.addr0 = (page << 16) |
			((chip->cw_size * (cwperpage-1)) >> 1);
	else
		data.cfg.addr0 = (page << 16) |
			(chip->cw_size * (cwperpage-1));

	data.cfg.addr1 = (page >> 16) & 0xff;
	data.ecc_bch_cfg = 1 << ECC_CFG_ECC_DISABLE;
	data.exec = 1;
	data.read_offset = (mtd->writesize - (chip->cw_size * (cwperpage-1)));
	dma_buffer->flash_status = 0xeeeeeeee;

	curr_cmd = cmd;
	msm_nand_prep_cfg_cmd_desc(info, data.cfg, &curr_cmd);

	cmd = curr_cmd;
	msm_nand_prep_ce(cmd, MSM_NAND_DEV0_ECC_CFG(info), WRITE,
			data.ecc_bch_cfg, 0);
	cmd++;

	rdata = (data.read_offset << 0) | (4 << 16) | (1 << 31);
	msm_nand_prep_ce(cmd, MSM_NAND_READ_LOCATION_0(info), WRITE, rdata, 0);
	cmd++;

	msm_nand_prep_ce(cmd, MSM_NAND_EXEC_CMD(info), WRITE,
			data.exec, SPS_IOVEC_FLAG_NWD);
	cmd++;

	msm_nand_prep_ce(cmd, MSM_NAND_FLASH_STATUS(info), READ,
		msm_virt_to_dma(chip, &dma_buffer->flash_status),
		SPS_IOVEC_FLAG_INT | SPS_IOVEC_FLAG_UNLOCK);
	cmd++;

	BUG_ON(cmd - dma_buffer->cmd > ARRAY_SIZE(dma_buffer->cmd));
	dma_buffer->xfer.iovec_count = (cmd - dma_buffer->cmd);
	dma_buffer->xfer.iovec = dma_buffer->cmd_iovec;
	dma_buffer->xfer.iovec_phys = msm_virt_to_dma(chip,
					&dma_buffer->cmd_iovec);
	iovec = dma_buffer->xfer.iovec;

	for (i = 0; i < dma_buffer->xfer.iovec_count; i++) {
		iovec->addr =  msm_virt_to_dma(chip, &dma_buffer->cmd[i].ce);
		iovec->size = sizeof(struct sps_command_element);
		iovec->flags = dma_buffer->cmd[i].flags;
		iovec++;
	}
	mutex_lock(&info->bam_lock);
	/* Submit data descriptor */
	ret = sps_transfer_one(info->sps.data_prod.handle,
			msm_virt_to_dma(chip, buf),
			4, NULL, SPS_IOVEC_FLAG_INT);

	if (ret) {
		pr_err("Failed to submit data desc %d\n", ret);
		mutex_unlock(&info->bam_lock);
		goto free_dma;
	}
	/* Submit command descriptor */
	ret =  sps_transfer(info->sps.cmd_pipe.handle, &dma_buffer->xfer);
	if (ret) {
		pr_err("Failed to submit commands %d\n", ret);
		mutex_unlock(&info->bam_lock);
		goto free_dma;
	}
	wait_for_completion_io(&info->sps.cmd_pipe.completion);
	wait_for_completion_io(&info->sps.data_prod.completion);
	mutex_unlock(&info->bam_lock);

	/* Check for flash status errors */
	if (dma_buffer->flash_status & (FS_OP_ERR | FS_MPU_ERR)) {
		pr_err("MPU/OP err set: %x\n", dma_buffer->flash_status);
		bad_block = -EIO;
		goto free_dma;
	}

	/* Check for bad block marker byte */
	if (chip->cfg1 & (1 << WIDE_FLASH)) {
		if (buf[0] != 0xFF || buf[1] != 0xFF)
			bad_block = 1;
	} else {
		if (buf[0] != 0xFF)
			bad_block = 1;
	}
free_dma:
	msm_nand_release_dma_buffer(chip, dma_buffer, sizeof(*dma_buffer) + 4);
out:
	return ret ? ret : bad_block;
}

/*
 * Function that gets called from upper layers such as MTD/YAFFS2 to mark a
 * block as bad. This is done by writing the first page within a block with 0,
 * thus setting the bad block byte location as well to 0.
 */
static int msm_nand_block_markbad(struct mtd_info *mtd, loff_t ofs)
{
	struct mtd_oob_ops ops;
	int ret;
	uint8_t *buf;
	size_t len;

	if (ofs > mtd->size) {
		pr_err("Invalid offset 0x%llx\n", ofs);
		ret = -EINVAL;
		goto out;
	}
	if (ofs & (mtd->erasesize - 1)) {
		pr_err("unsupported block address, 0x%x\n", (uint32_t)ofs);
		ret = -EINVAL;
		goto out;
	}
	len = mtd->writesize + mtd->oobsize;
	buf = kzalloc(len, GFP_KERNEL);
	if (!buf) {
		pr_err("unable to allocate memory for 0x%x size\n", len);
		ret = -ENOMEM;
		goto out;
	}
	ops.mode = MTD_OPS_RAW;
	ops.len = len;
	ops.retlen = 0;
	ops.ooblen = 0;
	ops.datbuf = buf;
	ops.oobbuf = NULL;
	ret =  msm_nand_write_oob(mtd, ofs, &ops);
	kfree(buf);
out:
	return ret;
}

/*
 * Function that scans for the attached NAND device. This fills out all
 * the uninitialized function pointers with the defaults. The flash ID is
 * read and the mtd/chip structures are filled with the appropriate values.
 */
int msm_nand_scan(struct mtd_info *mtd)
{
	struct msm_nand_info *info = mtd->priv;
	struct msm_nand_chip *chip = &info->nand_chip;
	struct flash_identification *supported_flash = &info->flash_dev;
	int flash_id = 0, err = 0;
	uint32_t i, mtd_writesize;
	uint8_t dev_found = 0, wide_bus;
	uint32_t manid, devid, devcfg;
	uint32_t bad_block_byte;
	struct nand_flash_dev *flashdev = NULL;
	struct nand_manufacturers  *flashman = NULL;

	/* Probe the Flash device for ONFI compliance */
	if (!msm_nand_flash_onfi_probe(info)) {
		dev_found = 1;
	} else {
		err = msm_nand_flash_read_id(info, 0, &flash_id);
		if (err < 0) {
			pr_err("Failed to read Flash ID\n");
			err = -EINVAL;
			goto out;
		}
		manid = flash_id & 0xFF;
		devid = (flash_id >> 8) & 0xFF;
		devcfg = (flash_id >> 24) & 0xFF;

		for (i = 0; !flashman && nand_manuf_ids[i].id; ++i)
			if (nand_manuf_ids[i].id == manid)
				flashman = &nand_manuf_ids[i];
		for (i = 0; !flashdev && nand_flash_ids[i].id; ++i)
			if (nand_flash_ids[i].id == devid)
				flashdev = &nand_flash_ids[i];
		if (!flashdev || !flashman) {
			pr_err("unknown nand flashid=%x manuf=%x devid=%x\n",
				flash_id, manid, devid);
			err = -ENOENT;
			goto out;
		}
		dev_found = 1;
		if (!flashdev->pagesize) {
			supported_flash->widebus = devcfg & (1 << 6) ? 1 : 0;
			supported_flash->pagesize = 1024 << (devcfg & 0x3);
			supported_flash->blksize = (64 * 1024) <<
							((devcfg >> 4) & 0x3);
			supported_flash->oobsize = (8 << ((devcfg >> 2) & 1)) *
				(supported_flash->pagesize >> 9);
		} else {
			supported_flash->widebus = flashdev->options &
				       NAND_BUSWIDTH_16 ? 1 : 0;
			supported_flash->pagesize = flashdev->pagesize;
			supported_flash->blksize = flashdev->erasesize;
			supported_flash->oobsize = flashdev->pagesize >> 5;
		}
		supported_flash->flash_id = flash_id;
		supported_flash->density = flashdev->chipsize << 20;
	}

	if (dev_found) {
		wide_bus       = supported_flash->widebus;
		mtd->size      = supported_flash->density;
		mtd->writesize = supported_flash->pagesize;
		mtd->oobsize   = supported_flash->oobsize;
		mtd->erasesize = supported_flash->blksize;
		mtd_writesize = mtd->writesize;

		/* Check whether NAND device support 8bit ECC*/
		if (supported_flash->ecc_correctability >= 8)
			chip->bch_caps = MSM_NAND_CAP_8_BIT_BCH;
		else
			chip->bch_caps = MSM_NAND_CAP_4_BIT_BCH;

		pr_info("NAND Id: 0x%x Buswidth: %dBits Density: %lld MByte\n",
			supported_flash->flash_id, (wide_bus) ? 16 : 8,
			(mtd->size >> 20));
		pr_info("pagesize: %d Erasesize: %d oobsize: %d (in Bytes)\n",
			mtd->writesize, mtd->erasesize, mtd->oobsize);
		pr_info("BCH ECC: %d Bit\n",
			(chip->bch_caps & MSM_NAND_CAP_8_BIT_BCH ? 8 : 4));
	}

	chip->cw_size = (chip->bch_caps & MSM_NAND_CAP_8_BIT_BCH) ? 532 : 528;
	chip->cfg0 = (((mtd_writesize >> 9) - 1) << CW_PER_PAGE)
		|  (516 <<  UD_SIZE_BYTES)
		|  (0 << DISABLE_STATUS_AFTER_WRITE)
		|  (5 << NUM_ADDR_CYCLES);

	bad_block_byte = (mtd_writesize - (chip->cw_size * (
					(mtd_writesize >> 9) - 1)) + 1);
	chip->cfg1 = (7 <<  NAND_RECOVERY_CYCLES)
		|    (0 <<  CS_ACTIVE_BSY)
		|    (bad_block_byte <<  BAD_BLOCK_BYTE_NUM)
		|    (0 << BAD_BLOCK_IN_SPARE_AREA)
		|    (2 << WR_RD_BSY_GAP)
		| ((wide_bus ? 1 : 0) << WIDE_FLASH)
		| (1 << ENABLE_BCH_ECC);

	chip->cfg0_raw = (((mtd_writesize >> 9) - 1) << CW_PER_PAGE)
		|	(5 << NUM_ADDR_CYCLES)
		|	(0 << SPARE_SIZE_BYTES)
		|	(chip->cw_size << UD_SIZE_BYTES);

	chip->cfg1_raw = (7 <<  NAND_RECOVERY_CYCLES)
		|    (0 <<  CS_ACTIVE_BSY)
		|    (17 <<  BAD_BLOCK_BYTE_NUM)
		|    (1 << BAD_BLOCK_IN_SPARE_AREA)
		|    (2 << WR_RD_BSY_GAP)
		| ((wide_bus ? 1 : 0) << WIDE_FLASH)
		| (1 << DEV0_CFG1_ECC_DISABLE);

	chip->ecc_bch_cfg = (0 << ECC_CFG_ECC_DISABLE)
			|   (0 << ECC_SW_RESET)
			|   (516 << ECC_NUM_DATA_BYTES)
			|   (1 << ECC_FORCE_CLK_OPEN);

	if (chip->bch_caps & MSM_NAND_CAP_8_BIT_BCH) {
		chip->cfg0 |= (wide_bus ? 0 << SPARE_SIZE_BYTES :
				2 << SPARE_SIZE_BYTES);
		chip->ecc_bch_cfg |= (1 << ECC_MODE)
			|   ((wide_bus) ? (14 << ECC_PARITY_SIZE_BYTES) :
					(13 << ECC_PARITY_SIZE_BYTES));
	} else {
		chip->cfg0 |= (wide_bus ? 2 << SPARE_SIZE_BYTES :
				4 << SPARE_SIZE_BYTES);
		chip->ecc_bch_cfg |= (0 << ECC_MODE)
			|   ((wide_bus) ? (8 << ECC_PARITY_SIZE_BYTES) :
					(7 << ECC_PARITY_SIZE_BYTES));
	}

	/*
	 * For 4bit BCH ECC (default ECC), parity bytes = 7(x8) or 8(x16 I/O)
	 * For 8bit BCH ECC, parity bytes = 13 (x8) or 14 (x16 I/O).
	 */
	chip->ecc_parity_bytes = (chip->bch_caps & MSM_NAND_CAP_8_BIT_BCH) ?
				(wide_bus ? 14 : 13) : (wide_bus ? 8 : 7);
	chip->ecc_buf_cfg = 0x203; /* No of bytes covered by ECC - 516 bytes */

	pr_info("CFG0: 0x%08x,      CFG1: 0x%08x\n"
		"            RAWCFG0: 0x%08x,   RAWCFG1: 0x%08x\n"
		"          ECCBUFCFG: 0x%08x, ECCBCHCFG: 0x%08x\n"
		"     BAD BLOCK BYTE: 0x%08x\n", chip->cfg0, chip->cfg1,
		chip->cfg0_raw, chip->cfg1_raw, chip->ecc_buf_cfg,
		chip->ecc_bch_cfg, bad_block_byte);

	if (mtd->oobsize == 64) {
		mtd->oobavail = 16;
	} else if ((mtd->oobsize == 128) || (mtd->oobsize == 224)) {
		mtd->oobavail = 32;
	} else {
		pr_err("Unsupported NAND oobsize: 0x%x\n", mtd->oobsize);
		err = -ENODEV;
		goto out;
	}

	/* Fill in remaining MTD driver data */
	mtd->type = MTD_NANDFLASH;
	mtd->flags = MTD_CAP_NANDFLASH;
	mtd->_erase = msm_nand_erase;
	mtd->_block_isbad = msm_nand_block_isbad;
	mtd->_block_markbad = msm_nand_block_markbad;
	mtd->_read = msm_nand_read;
	mtd->_write = msm_nand_write;
	mtd->_read_oob  = msm_nand_read_oob;
	mtd->_write_oob = msm_nand_write_oob;
	mtd->owner = THIS_MODULE;
out:
	return err;
}

#define BAM_APPS_PIPE_LOCK_GRP0 0
#define BAM_APPS_PIPE_LOCK_GRP1 1
/*
 * This function allocates, configures, connects an end point and
 * also registers event notification for an end point. It also allocates
 * DMA memory for descriptor FIFO of a pipe.
 */
static int msm_nand_init_endpoint(struct msm_nand_info *info,
				struct msm_nand_sps_endpt *end_point,
				uint32_t pipe_index)
{
	int rc = 0;
	struct sps_pipe *pipe_handle;
	struct sps_connect *sps_config = &end_point->config;
	struct sps_register_event *sps_event = &end_point->event;

	pipe_handle = sps_alloc_endpoint();
	if (!pipe_handle) {
		pr_err("sps_alloc_endpoint() failed\n");
		rc = -ENOMEM;
		goto out;
	}

	rc = sps_get_config(pipe_handle, sps_config);
	if (rc) {
		pr_err("sps_get_config() failed %d\n", rc);
		goto free_endpoint;
	}

	if (pipe_index == SPS_DATA_PROD_PIPE_INDEX) {
		/* READ CASE: source - BAM; destination - system memory */
		sps_config->source = info->sps.bam_handle;
		sps_config->destination = SPS_DEV_HANDLE_MEM;
		sps_config->mode = SPS_MODE_SRC;
		sps_config->src_pipe_index = pipe_index;
	} else if (pipe_index == SPS_DATA_CONS_PIPE_INDEX ||
			pipe_index == SPS_CMD_CONS_PIPE_INDEX) {
		/* WRITE CASE: source - system memory; destination - BAM */
		sps_config->source = SPS_DEV_HANDLE_MEM;
		sps_config->destination = info->sps.bam_handle;
		sps_config->mode = SPS_MODE_DEST;
		sps_config->dest_pipe_index = pipe_index;
	}

	sps_config->options = SPS_O_AUTO_ENABLE | SPS_O_DESC_DONE;

	if (pipe_index == SPS_DATA_PROD_PIPE_INDEX ||
			pipe_index == SPS_DATA_CONS_PIPE_INDEX)
		sps_config->lock_group = BAM_APPS_PIPE_LOCK_GRP0;
	else if (pipe_index == SPS_CMD_CONS_PIPE_INDEX)
		sps_config->lock_group = BAM_APPS_PIPE_LOCK_GRP1;

	/*
	 * Descriptor FIFO is a cyclic FIFO. If SPS_MAX_DESC_NUM descriptors
	 * are allowed to be submitted before we get any ack for any of them,
	 * the descriptor FIFO size should be: (SPS_MAX_DESC_NUM + 1) *
	 * sizeof(struct sps_iovec).
	 */
	sps_config->desc.size = (SPS_MAX_DESC_NUM + 1) *
					sizeof(struct sps_iovec);
	sps_config->desc.base = dmam_alloc_coherent(info->nand_chip.dev,
					sps_config->desc.size,
					&sps_config->desc.phys_base,
					GFP_KERNEL);
	if (!sps_config->desc.base) {
		pr_err("dmam_alloc_coherent() failed for size %x\n",
				sps_config->desc.size);
		rc = -ENOMEM;
		goto free_endpoint;
	}
	memset(sps_config->desc.base, 0x00, sps_config->desc.size);

	rc = sps_connect(pipe_handle, sps_config);
	if (rc) {
		pr_err("sps_connect() failed %d\n", rc);
		goto free_endpoint;
	}

	init_completion(&end_point->completion);
	sps_event->mode = SPS_TRIGGER_WAIT;
	sps_event->options = SPS_O_DESC_DONE;
	sps_event->xfer_done = &end_point->completion;
	sps_event->user = (void *)info;

	rc = sps_register_event(pipe_handle, sps_event);
	if (rc) {
		pr_err("sps_register_event() failed %d\n", rc);
		goto sps_disconnect;
	}
	end_point->handle = pipe_handle;
	pr_debug("pipe handle 0x%x for pipe %d\n", (uint32_t)pipe_handle,
			pipe_index);
	goto out;
sps_disconnect:
	sps_disconnect(pipe_handle);
free_endpoint:
	sps_free_endpoint(pipe_handle);
out:
	return rc;
}

/* This function disconnects and frees an end point */
static void msm_nand_deinit_endpoint(struct msm_nand_info *info,
				struct msm_nand_sps_endpt *end_point)
{
	sps_disconnect(end_point->handle);
	sps_free_endpoint(end_point->handle);
}

/*
 * This function registers BAM device and initializes its end points for
 * the following pipes -
 * system consumer pipe for data (pipe#0),
 * system producer pipe for data (pipe#1),
 * system consumer pipe for commands (pipe#2).
 */
static int msm_nand_bam_init(struct msm_nand_info *nand_info)
{
	struct sps_bam_props bam = {0};
	int rc = 0;

	bam.phys_addr = nand_info->bam_phys;
	bam.virt_addr = nand_info->bam_base;
	bam.irq = nand_info->bam_irq;
	/*
	 * NAND device is accessible from both Apps and Modem processor and
	 * thus, NANDc and BAM are shared between both the processors. But BAM
	 * must be enabled and instantiated only once during boot up by
	 * Trustzone before Modem/Apps is brought out from reset.
	 *
	 * This is indicated to SPS driver on Apps by marking flag
	 * SPS_BAM_MGR_DEVICE_REMOTE. The following are the global
	 * initializations that will be done by Trustzone - Execution
	 * Environment, Pipes assignment to Apps/Modem, Pipe Super groups and
	 * Descriptor summing threshold.
	 *
	 * NANDc BAM device supports 2 execution environments - Modem and Apps
	 * and thus the flag SPS_BAM_MGR_MULTI_EE is set.
	 */
	bam.manage = SPS_BAM_MGR_DEVICE_REMOTE | SPS_BAM_MGR_MULTI_EE;

	rc = sps_phy2h(bam.phys_addr, &nand_info->sps.bam_handle);
	if (!rc)
		goto init_sps_ep;
	rc = sps_register_bam_device(&bam, &nand_info->sps.bam_handle);
	if (rc) {
		pr_err("%s: sps_register_bam_device() failed with %d\n",
			__func__, rc);
		goto out;
	}
	pr_info("%s: BAM device registered: bam_handle 0x%x\n",
			__func__, nand_info->sps.bam_handle);
init_sps_ep:
	rc = msm_nand_init_endpoint(nand_info, &nand_info->sps.data_prod,
					SPS_DATA_PROD_PIPE_INDEX);
	if (rc)
		goto out;
	rc = msm_nand_init_endpoint(nand_info, &nand_info->sps.data_cons,
					SPS_DATA_CONS_PIPE_INDEX);
	if (rc)
		goto deinit_data_prod;

	rc = msm_nand_init_endpoint(nand_info, &nand_info->sps.cmd_pipe,
					SPS_CMD_CONS_PIPE_INDEX);
	if (rc)
		goto deinit_data_cons;
	goto out;
deinit_data_cons:
	msm_nand_deinit_endpoint(nand_info, &nand_info->sps.data_cons);
deinit_data_prod:
	msm_nand_deinit_endpoint(nand_info, &nand_info->sps.data_prod);
out:
	return rc;
}

/*
 * This function disconnects and frees its end points for all the pipes.
 * Since the BAM is shared resource, it is not deregistered as its handle
 * might be in use with LCDC.
 */
static void msm_nand_bam_free(struct msm_nand_info *nand_info)
{
	msm_nand_deinit_endpoint(nand_info, &nand_info->sps.data_prod);
	msm_nand_deinit_endpoint(nand_info, &nand_info->sps.data_cons);
	msm_nand_deinit_endpoint(nand_info, &nand_info->sps.cmd_pipe);
}

/* This function enables DMA support for the NANDc in BAM mode. */
static int msm_nand_enable_dma(struct msm_nand_info *info)
{
	struct msm_nand_sps_cmd *sps_cmd;
	struct msm_nand_chip *chip = &info->nand_chip;
	int ret;

	wait_event(chip->dma_wait_queue,
		   (sps_cmd = msm_nand_get_dma_buffer(chip, sizeof(*sps_cmd))));

	msm_nand_prep_ce(sps_cmd, MSM_NAND_CTRL(info), WRITE,
			(1 << BAM_MODE_EN), SPS_IOVEC_FLAG_INT);

	ret = sps_transfer_one(info->sps.cmd_pipe.handle,
			msm_virt_to_dma(chip, &sps_cmd->ce),
			sizeof(struct sps_command_element), NULL,
			sps_cmd->flags);
	if (ret) {
		pr_err("Failed to submit command: %d\n", ret);
		goto out;
	}
	wait_for_completion_io(&info->sps.cmd_pipe.completion);
out:
	msm_nand_release_dma_buffer(chip, sps_cmd, sizeof(*sps_cmd));
	return ret;

}

#ifdef CONFIG_MSM_SMD
static int msm_nand_parse_smem_ptable(int *nr_parts)
{

	uint32_t  i, j;
	uint32_t len = FLASH_PTABLE_HDR_LEN;
	struct flash_partition_entry *pentry;
	char *delimiter = ":";

	pr_info("Parsing partition table info from SMEM\n");
	/* Read only the header portion of ptable */
	ptable = *(struct flash_partition_table *)
			(smem_get_entry(SMEM_AARM_PARTITION_TABLE, &len));
	/* Verify ptable magic */
	if (ptable.magic1 != FLASH_PART_MAGIC1 ||
			ptable.magic2 != FLASH_PART_MAGIC2) {
		pr_err("Partition table magic verification failed\n");
		goto out;
	}
	/* Ensure that # of partitions is less than the max we have allocated */
	if (ptable.numparts > FLASH_PTABLE_MAX_PARTS_V4) {
		pr_err("Partition numbers exceed the max limit\n");
		goto out;
	}
	/* Find out length of partition data based on table version. */
	if (ptable.version <= FLASH_PTABLE_V3) {
		len = FLASH_PTABLE_HDR_LEN + FLASH_PTABLE_MAX_PARTS_V3 *
			sizeof(struct flash_partition_entry);
	} else if (ptable.version == FLASH_PTABLE_V4) {
		len = FLASH_PTABLE_HDR_LEN + FLASH_PTABLE_MAX_PARTS_V4 *
			sizeof(struct flash_partition_entry);
	} else {
		pr_err("Unknown ptable version (%d)", ptable.version);
		goto out;
	}

	*nr_parts = ptable.numparts;
	ptable = *(struct flash_partition_table *)
			(smem_get_entry(SMEM_AARM_PARTITION_TABLE, &len));
	for (i = 0; i < ptable.numparts; i++) {
		pentry = &ptable.part_entry[i];
		if (pentry->name == '\0')
			continue;
		/* Convert name to lower case and discard the initial chars */
		mtd_part[i].name        = pentry->name;
		for (j = 0; j < strlen(mtd_part[i].name); j++)
			*(mtd_part[i].name + j) =
				tolower(*(mtd_part[i].name + j));
		strsep(&(mtd_part[i].name), delimiter);
		mtd_part[i].offset      = pentry->offset;
		mtd_part[i].mask_flags  = pentry->attr;
		mtd_part[i].size        = pentry->length;
		pr_debug("%d: %s offs=0x%08x size=0x%08x attr:0x%08x\n",
			i, pentry->name, pentry->offset, pentry->length,
			pentry->attr);
	}
	pr_info("SMEM partition table found: ver: %d len: %d\n",
		ptable.version, ptable.numparts);
	return 0;
out:
	return -EINVAL;
}
#else
static int msm_nand_parse_smem_ptable(int *nr_parts)
{
	return -ENODEV;
}
#endif

/*
 * This function gets called when its device named msm-nand is added to
 * device tree .dts file with all its resources such as physical addresses
 * for NANDc and BAM, BAM IRQ.
 *
 * It also expects the NAND flash partition information to be passed in .dts
 * file so that it can parse the partitions by calling MTD function
 * mtd_device_parse_register().
 *
 */
static int __devinit msm_nand_probe(struct platform_device *pdev)
{
	struct msm_nand_info *info;
	struct resource *res;
	int i, err, nr_parts;

	/*
	 * The partition information can also be passed from kernel command
	 * line. Also, the MTD core layer supports adding the whole device as
	 * one MTD device when no partition information is available at all.
	 */
	info = devm_kzalloc(&pdev->dev, sizeof(struct msm_nand_info),
				GFP_KERNEL);
	if (!info) {
		pr_err("Unable to allocate memory for msm_nand_info\n");
		err = -ENOMEM;
		goto out;
	}
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"nand_phys");
	if (!res || !res->start) {
		pr_err("NAND phys address range is not provided\n");
		err = -ENODEV;
		goto out;
	}
	info->nand_phys = res->start;
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"bam_phys");
	if (!res || !res->start) {
		pr_err("BAM phys address range is not provided\n");
		err = -ENODEV;
		goto out;
	}
	info->bam_phys = res->start;
	info->bam_base = devm_ioremap(&pdev->dev, res->start,
					resource_size(res));
	if (!info->bam_base) {
		pr_err("BAM ioremap() failed for addr 0x%x size 0x%x\n",
			res->start, resource_size(res));
		err = -ENOMEM;
		goto out;
	}

	info->bam_irq = platform_get_irq_byname(pdev, "bam_irq");
	if (info->bam_irq < 0) {
		pr_err("BAM IRQ is not provided\n");
		err = -ENODEV;
		goto out;
	}

	info->mtd.name = dev_name(&pdev->dev);
	info->mtd.priv = info;
	info->mtd.owner = THIS_MODULE;
	info->nand_chip.dev = &pdev->dev;
	init_waitqueue_head(&info->nand_chip.dma_wait_queue);
	mutex_init(&info->bam_lock);

	info->nand_chip.dma_virt_addr =
		dmam_alloc_coherent(&pdev->dev, MSM_NAND_DMA_BUFFER_SIZE,
			&info->nand_chip.dma_phys_addr, GFP_KERNEL);
	if (!info->nand_chip.dma_virt_addr) {
		pr_err("No memory for DMA buffer size %x\n",
				MSM_NAND_DMA_BUFFER_SIZE);
		err = -ENOMEM;
		goto out;
	}
	err = msm_nand_bam_init(info);
	if (err) {
		pr_err("msm_nand_bam_init() failed %d\n", err);
		goto out;
	}
	err = msm_nand_enable_dma(info);
	if (err) {
		pr_err("Failed to enable DMA in NANDc\n");
		goto free_bam;
	}
	err = msm_nand_parse_smem_ptable(&nr_parts);
	if (err < 0) {
		pr_err("Failed to parse partition table in SMEM\n");
		goto free_bam;
	}
	if (msm_nand_scan(&info->mtd)) {
		pr_err("No nand device found\n");
		err = -ENXIO;
		goto free_bam;
	}
	for (i = 0; i < nr_parts; i++) {
		mtd_part[i].offset *= info->mtd.erasesize;
		mtd_part[i].size *= info->mtd.erasesize;
	}
	err = mtd_device_parse_register(&info->mtd, NULL, NULL,
		&mtd_part[0], nr_parts);
	if (err < 0) {
		pr_err("Unable to register MTD partitions %d\n", err);
		goto free_bam;
	}
	dev_set_drvdata(&pdev->dev, info);

	pr_info("NANDc phys addr 0x%lx, BAM phys addr 0x%lx, BAM IRQ %d\n",
			info->nand_phys, info->bam_phys, info->bam_irq);
	pr_info("Allocated DMA buffer at virt_addr 0x%p, phys_addr 0x%x\n",
		info->nand_chip.dma_virt_addr, info->nand_chip.dma_phys_addr);
	goto out;
free_bam:
	msm_nand_bam_free(info);
out:
	return err;
}

/*
 * Remove functionality that gets called when driver/device msm-nand
 * is removed.
 */
static int __devexit msm_nand_remove(struct platform_device *pdev)
{
	struct msm_nand_info *info = dev_get_drvdata(&pdev->dev);

	dev_set_drvdata(&pdev->dev, NULL);
	if (info) {
		mtd_device_unregister(&info->mtd);
		msm_nand_bam_free(info);
	}
	return 0;
}

#define DRIVER_NAME "msm_qpic_nand"
static const struct of_device_id msm_nand_match_table[] = {
	{ .compatible = "qcom,msm-nand", },
	{},
};
static struct platform_driver msm_nand_driver = {
	.probe		= msm_nand_probe,
	.remove		= __devexit_p(msm_nand_remove),
	.driver = {
		.name		= DRIVER_NAME,
		.of_match_table = msm_nand_match_table,
	},
};

module_platform_driver(msm_nand_driver);

MODULE_ALIAS(DRIVER_NAME);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM QPIC NAND flash driver");

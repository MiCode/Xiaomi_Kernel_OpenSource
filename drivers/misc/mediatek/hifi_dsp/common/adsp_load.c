/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/fs.h>
#include <linux/firmware.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/io.h>
#include "mtk_hifixdsp_common.h"
#include "adsp_helper.h"


#define BIT_DSP_BOOT_FROM_DRAM  BIT(0)
#define HIFIXDSP_IMAGE_NAME  "hifi4dsp_load.bin"
#define HIFIXDSP_BIN_MAGIC  ((unsigned long long)(0x4D495053444B544D))

static callback_fn user_callback_fn;
static void *callback_arg;


/*
 * HIFIxDSP has boot done or not.
 * 1 : ADSP has boot ok
 * 0 : not boot yet
 */
int hifixdsp_run_status(void)
{
	struct adsp_chip_info *adsp;

	adsp = get_adsp_chip_data();
	if (adsp)
		return adsp->adsp_bootup_done;

	return 0;
}

static void set_hifixdsp_run_status(void)
{
	struct adsp_chip_info *adsp;

	adsp = get_adsp_chip_data();
	if (!adsp)
		return;
	adsp->adsp_bootup_done = 1;
	pr_notice("[ADSP] HIFIxDSP start to run now.\n");
}

static
int check_image_header_info(u8 *data, int size)
{
	int ret = 0;
	unsigned long long magic;

	/* check miminal header size : 0x800 */
	if (size < 0x800) {
		ret = -1;
		return ret;
	}

	magic = *(unsigned long long *)data;
	if (magic != HIFIXDSP_BIN_MAGIC) {
		ret = -2;
		pr_err("HIFIXDSP_BIN_MAGIC error : 0x%llX\n", magic);
	}

	return ret;
}

static
int load_image_hifixdsp(u32 load_addr, void *data, int len)
{
	int err = 0;
	size_t size;
	void __iomem *load_vaddr;

	/*
	 * Need to (must be)
	 * remap DSP.bin load-physical-addr for kernel IO address,
	 * ITCM section/DTCM section/DRAM section
	 */
	size = roundup(len, 1024);
	load_vaddr = ioremap(load_addr, size);
	if (!load_vaddr) {
		err = -ENOMEM;
		goto TAIL;
	}
	memcpy_toio(load_vaddr, data, len);
	iounmap(load_vaddr);
TAIL:
	return err;
}

/*
 * This function(security parse) will be replaced by TEE API later.
 */
static u32 firmware_adsp_load(void *src, size_t size,
			phys_addr_t dram_base)
{
	int err;
	int loop;
	u8 *fw_data;
	u8 *data;
	int signature;
	u64 img_bin_tb_inf;
	int boot_adr_num;
	u32 dsp_boot_adr;
	int img_bin_inf_num;
	u32 section_off;
	u32 section_len;
	u32 section_ldr;
	u32 cpu_view_dram_base_paddr;
	u32 fix_offset;

	fw_data = src;
	cpu_view_dram_base_paddr = (u32)dram_base;

	err = check_image_header_info(fw_data, (int)size);
	if (err) {
		pr_err("firmware %s may be corrupted!\n",
			HIFIXDSP_IMAGE_NAME);
		goto ERROR;
	}

	/*
	 * 1), check image signature or not,
	 * 2), parse image format for important fields;
	 * 3), 28bytes =
	 * |BIN_MAGIC|BIN_TOTAL_SZ|IMG_BIN_INF_TB_SZ|TB_INF|TB_LD_ADR|
	 */
	img_bin_tb_inf = *(u8 *)(fw_data + 16);
	signature = img_bin_tb_inf ? 1 : 0;
	if (signature)
		goto ERROR;

	/* BOOT_ADR_NO(M) */
	boot_adr_num = *(u32 *)(fw_data + 28);
	/* DSP_1_ADR for DSP bootup entry */
	dsp_boot_adr = *(u32 *)(fw_data + 28 + 4);
	/* IMG_BIN_INF_NO(N) */
	img_bin_inf_num = *(u32 *)(fw_data + 32 + 4 * boot_adr_num);

	/* IMG_BIN_INF_X (20bytes, loop read info) */
	for (loop = 0; loop < img_bin_inf_num; loop++) {
		fix_offset = 32 + (4 * boot_adr_num) + 8 + (20 * loop);
		/* IMG_BIN_OFST */
		section_off = *(u32 *)(fw_data + fix_offset + 4);
		/* IMG_SZ */
		section_len = *(u32 *)(fw_data + fix_offset + 12);
		/* LD_ADR */
		section_ldr = *(u32 *)(fw_data + fix_offset + 16);

		pr_debug(
			"section%d: load_addr = 0x%08X, offset = 0x%08X, len = %u\n",
			loop, section_ldr, section_off, section_len);

		/*
		 * The shared DRAM physical base(reserved) for CPU view
		 * may be different from DSP memory view.
		 * LD_ADR: bit0 indicates if the image is the DRAM section,
		 * here only to check DRAM load_addr.
		 */
		if (section_ldr & BIT_DSP_BOOT_FROM_DRAM)
			section_ldr = cpu_view_dram_base_paddr;

		/* data offset from total header */
		data = fw_data + section_off;
		err = load_image_hifixdsp(section_ldr, data, section_len);
		if (err) {
			pr_err("%s write section%d.bin (%d bytes) fail!\n",
				__func__, loop, section_len);
			goto ERROR;
		}
	}

	return dsp_boot_adr;
ERROR:
	return 0x00; /* invalid physical base */
}


/*
 * 1. Request firmware from fs bin.
 * 2. Check and parse image header info.
 * 3. Write binary to HIFIxDSP ITCM/SRAM each image.
 */
static void async_load_hifixdsp_and_run(
			const struct firmware *fw,
			void *context)
{
	int err = 0;
	u32 adsp_bootup_addr;
	struct adsp_chip_info *adsp;

	pr_info("firmware %s load success, size = %d\n",
		HIFIXDSP_IMAGE_NAME, (int)fw->size);

	/*
	 * Step1:
	 * Parse Image Header for security image format.
	 */
	adsp = get_adsp_chip_data();
	if (!adsp) {
		err = -EINVAL;
		pr_err("adsp_chip_data is not initialized!\n");
		goto TAIL;
	}
	adsp_bootup_addr = firmware_adsp_load(
		(void *)fw->data,
		fw->size, adsp->pa_dram);
	if (!adsp_bootup_addr) {
		err = -EINVAL;
		pr_err("adsp_bootup_addr is invalid!\n");
		goto TAIL;
	}
	adsp->adsp_bootup_addr = adsp_bootup_addr;

TAIL:
	release_firmware(fw);
	if (err) {
		pr_err("[ADSP] firmware_adsp_load Error!\n");
		return;
	}

	/*
	 * Step2:
	 * Power-on HIFIxDSP boot sequence
	 */
	msleep(20);
	hifixdsp_poweron(adsp_bootup_addr);

	adsp_misc_setting_after_poweron();
	set_hifixdsp_run_status();

	/* callback function for user */
	if (user_callback_fn)
		user_callback_fn(callback_arg);
}

/*
 * Assume called by audio system only.
 */
int async_load_hifixdsp_bin_and_run(
			callback_fn callback, void *param)
{
	int ret = 0;

	if (hifixdsp_run_status()) {
		pr_err("[ADSP] error: bootup two times!\n");
		return ret;
	}

	user_callback_fn = callback;
	callback_arg = param;

	/* Async load firmware and run HIFIxDSP */
	ret = request_firmware_nowait(THIS_MODULE, true,
			HIFIXDSP_IMAGE_NAME, NULL,
			GFP_KERNEL, NULL,
			async_load_hifixdsp_and_run
			);

	return ret;
}


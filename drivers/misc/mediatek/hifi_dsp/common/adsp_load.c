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
#include "adsp_ipi.h"
#include "mtk_hifixdsp_common.h"
#include "adsp_helper.h"


#define LEN_BIN_MAGIC            (8)
#define LEN_BIN_TOTAL_SZ         (4)
#define LEN_IMG_BIN_INF_TB_SZ    (4)
#define LEN_TB_INF               (8) /* Field0 in IMG_BIN_INF_TB */
#define LEN_TB_LD_ADR            (4) /* Field1 in IMG_BIN_INF_TB */
#define LEN_BOOT_ADR_NO          (4) /* Field2 in IMG_BIN_INF_TB */
/* #define ... */
#define LEN_IMG_BINS_SZ          (4)

#define HIFIXDSP_BIN_MAGIC   ((unsigned long long)(0x4D495053444B544D))
#define HIFIXDSP_IMAGE_NAME  "hifi4dsp_load.bin"
#define BIT_DSP_BOOT_FROM_DRAM  BIT(0)

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

static void set_hifixdsp_run_status(int done)
{
	struct adsp_chip_info *adsp;

	adsp = get_adsp_chip_data();
	if (!adsp)
		return;
	adsp->adsp_bootup_done = done;
	adsp_ipi_set_wdt_status();
	pr_notice("[ADSP] HIFIxDSP %s to run now.\n",
		done ? "start" : "stop");
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
	u32 bin_total_sz;
	u32 img_bin_inf_tb_sz;
	u32 total_hdr_len;
	u64 img_bin_tb_inf;
	int boot_adr_num;
	u32 dsp_boot_adr;
	int img_bin_inf_num;
	u32 section_off;
	u32 section_len;
	u32 section_ldr;
	u32 cpu_view_dram_base_paddr;
	u32 offset;
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
	offset = LEN_BIN_MAGIC;
	bin_total_sz = *(u32 *)(fw_data + offset);
	offset += LEN_BIN_TOTAL_SZ;
	img_bin_inf_tb_sz = *(u32 *)(fw_data + offset);
	offset += LEN_IMG_BIN_INF_TB_SZ;
	img_bin_tb_inf = *(u64 *)(fw_data + offset);
	/*
	 * sizeof (TB_INF) = 8bytes.
	 * TB_INF[7]: authentication type for IMG_BIN_INF_TB
	 * TB_INF[6]: authentication type for IMG_BIN
	 * TB_INF[5]: encryption type for IMG_BIN
	 * TB_INF[4:2]: reserved
	 * TB_INF[1]:
	 * bit0: indicate if enabling authentication (1) or not (0)
	 *	for IMG_BIN_INF_TB
	 * TB_INF[0]: version of IMG_BIN_INF_TB
	 */
	signature = (img_bin_tb_inf) ? 1 : 0;
	if (signature) {
		pr_err("TB_INF = 0x%llx, decryption is not supported!\n",
				img_bin_tb_inf);
		goto ERROR;
	}

	/*
	 * #0x00000814 = total_hdr_len
	 *	= 8(BIN_MAGIC) + 4(BIN_TOTAL_SZ) + 4(IMG_BIN_INF_TB_SZ)
	 *	+ 0x800(IMG_BIN_INF_TB) + 4(IMG_BINS_SZ)
	 */
	total_hdr_len = offset + img_bin_inf_tb_sz + LEN_IMG_BINS_SZ;

	/* BOOT_ADR_NO(M) */
	offset += (LEN_TB_INF + LEN_TB_LD_ADR);
	boot_adr_num = *(u32 *)(fw_data + offset);
	/* DSP_1_ADR for DSP bootup entry */
	offset += LEN_BOOT_ADR_NO;
	dsp_boot_adr = *(u32 *)(fw_data + offset);
	/* IMG_BIN_INF_NO(N) */
	img_bin_inf_num = *(u32 *)(fw_data + offset + 4 * boot_adr_num);

	/* IMG_BIN_INF_X (20bytes, loop read info) */
	for (loop = 0; loop < img_bin_inf_num; loop++) {
		fix_offset = offset + (4 * boot_adr_num) + 8 + (20 * loop);
		/* IMG_BIN_OFST */
		section_off = *(u32 *)(fw_data + fix_offset + 4);
		/* IMG_SZ */
		section_len = *(u32 *)(fw_data + fix_offset + 12);
		/* LD_ADR */
		section_ldr = *(u32 *)(fw_data + fix_offset + 16);

		pr_info(
			"section%d: load_addr = 0x%08X, offset = 0x%08X, len = %u\n",
			(loop + 1), section_ldr, section_off, section_len);

		/*
		 * The shared DRAM physical base(reserved) for CPU view
		 * may be different from DSP memory view.
		 * LD_ADR: bit0 indicates if the image is the DRAM section,
		 * here only to check DRAM load_addr.
		 */
		if (section_ldr & BIT_DSP_BOOT_FROM_DRAM)
			section_ldr = cpu_view_dram_base_paddr;

		/* IMG_BIN_OFST: start from beginning of IMG_BINS */
		data = fw_data + total_hdr_len + section_off;
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

	adsp = get_adsp_chip_data();
	if (!adsp) {
		err = -EINVAL;
		goto TAIL;
	}

	if (!fw) {
		err = -ENOENT;
		pr_err("[ADSP] error: fw == NULL!\n");
		pr_err("request_firmware_nowait (%s) not available.\n",
				HIFIXDSP_IMAGE_NAME);
		goto TAIL;
	}

	pr_info("firmware %s load success, size = %d\n",
		HIFIXDSP_IMAGE_NAME, (int)fw->size);

	/*
	 * Step1:
	 * Parse Image Header for security image format.
	 */
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
		if (adsp)
			adsp_clock_power_off(adsp->data->dev);
		return;
	}

	/*
	 * Step2:
	 * Power-on HIFIxDSP boot sequence
	 */
	msleep(20);
	hifixdsp_boot_sequence(adsp_bootup_addr);

	adsp_misc_setting_after_poweron();
	set_hifixdsp_run_status(1);

	/* callback function for user */
	if (user_callback_fn)
		user_callback_fn(callback_arg);
}

/*
 * HIFIxDSP start to run and load bin.
 * Assume called by audio system only.
 */
int async_load_hifixdsp_bin_and_run(
			callback_fn callback, void *param)
{
	int ret = 0;
	struct adsp_chip_info *adsp;

	if (hifixdsp_run_status()) {
		pr_err("[ADSP] error: bootup two times!\n");
		return ret;
	}

	user_callback_fn = callback;
	callback_arg = param;

	/* Open HIFIxDSP power and clock */
	adsp = get_adsp_chip_data();
	if (!adsp)
		goto TAIL;
	ret = adsp_clock_power_on(adsp->data->dev);
	if (ret) {
		pr_err("[ADSP] adsp_clock_power_on fail!\n");
		goto TAIL;
	}

	/* Async load firmware and run HIFIxDSP */
	ret = request_firmware_nowait(THIS_MODULE, true,
			HIFIXDSP_IMAGE_NAME, NULL,
			GFP_KERNEL, NULL,
			async_load_hifixdsp_and_run
			);
TAIL:
	return ret;
}

/*
 * HIFIxDSP shutdown and not run.
 * Assume called by audio system only.
 */
int hifixdsp_stop_run(void)
{
	int ret = 0;
	struct adsp_chip_info *adsp;

	if (!hifixdsp_run_status()) {
		pr_notice("[ADSP] not boot to run yet!\n");
		return ret;
	}

	ret = adsp_shutdown_notify_check();
	if (ret) {
		pr_err("[ADSP] adsp_shutdown_notify_check fail!\n");
		goto TAIL;
	}

	adsp_wdt_stop();
	hifixdsp_shutdown();
	adsp_remove_setting_after_shutdown();

	adsp = get_adsp_chip_data();
	if (!adsp)
		goto TAIL;
	ret = adsp_clock_power_off(adsp->data->dev);
	if (ret) {
		pr_err("[ADSP] adsp_clock_power_off fail!\n");
		goto TAIL;
	}

	set_hifixdsp_run_status(0);

TAIL:
	return ret;
}


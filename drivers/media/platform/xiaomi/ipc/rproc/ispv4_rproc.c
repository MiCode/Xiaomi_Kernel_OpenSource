// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Xiaomi, Inc.
 */

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/remoteproc.h>
#include <linux/sched.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include "remoteproc_internal.h"
#include "remoteproc_elf_helpers.h"
#include "ispv4_rproc.h"
#include "ispv4_exception.h"
#include "ispv4_regops.h"
#include <linux/component.h>
#include <crypto/hash.h>
#include <linux/mfd/core.h>
#include <linux/mfd/ispv4_defs.h>
#include <linux/sysfs.h>
#include "ispv4_pcie.h"

/* Debug for using one stage to load firmware */
// #define DEBUG_NO_2STAGE
// #define LOAD_BY_DMA
// #define USE_USERHELPER
#define CHECK_FW

#define DDR_LOAD true
#define DDR_DUMP false

extern struct dentry *ispv4_debugfs;
static bool p_load_by_dma = true;

static struct ispv4_dev_addr ispv4_addr_region[ISPV4_MEMREGION_MAX] = {
	[ISPV4_MEMREGION_SRAM] = { 0x00000000, 0x280000 }, // 2.5MB
	[ISPV4_MEMREGION_DDR] = { 0x80000000, 0x10000000 }, // 256MB
};

enum user_deal {
	UD_SAVE_RAMLOG,
	UD_SAVE_BOOTINFO,
	UD_SAVE_DEBUGINFO,
	UD_SAVE_DDRTF,
	UD_LOAD_DDRTF,
	USER_DEAL_MAX
};

struct user_deal_path {
	char *src;
	char *dest;
	bool time;
};

#define UD_PRE1 "/sys/devices/platform/soc/1c08000.qcom,pcie/"
#define UD_PRE2 "pci0001:00/0001:00:00.0/0001:01:00.0/xm-ispv4-rproc-pci/"
#define UD_PRE3 "/data/xm_ispv4_"
#define UD_PS UD_PRE1 UD_PRE2
#define UD_PD UD_PRE3

static void ispv4_rproc_rm_debugfs(struct xm_ispv4_rproc *rp);
static void ispv4_dump_ramlog(struct xm_ispv4_rproc *rp);
static void ispv4_dump_bootinfo(struct xm_ispv4_rproc *rp);
static void ispv4_dump_debuginfo(struct xm_ispv4_rproc *rp);
static void ispv4_dump_inner128(void);
static void ispv4_dump_status(struct xm_ispv4_rproc *rp);
static int ispv4_user_dealfile(enum user_deal deal);
// TODO: Move to header file.
// struct xm_ispv4_rproc;
int ispv4_rproc_boot(struct xm_ispv4_rproc *rp, bool *only_s1_boot);
int ispv4_rproc_down(struct xm_ispv4_rproc *rp);
static int ispv4_prepare_firmware(struct xm_ispv4_rproc *xm_rp);

int ispv4_ramlog_boot(struct xm_ispv4_rproc *rp);
void ispv4_ramlog_deboot(struct xm_ispv4_rproc *rp);

static enum xm_ispv4_memregion ispv4_devaddr_match_region(u64 addr)
{
	int i = 0;
	struct ispv4_dev_addr region;
	for (i = 0; i < ISPV4_MEMREGION_MAX; i++) {
		region = ispv4_addr_region[i];
		if (region.start <= addr && addr < region.start + region.size)
			return i;
	}
	return ISPV4_MEMREGION_UNKNOWN;
}

static int ispv4_rproc_ops_init(struct platform_device *pdev)
{
	int rc = 0;
	struct xm_ispv4_rproc *xm_rp;
	struct xm_ispv4_ops *ops;

	dev_info(&pdev->dev, "%s entry", __FUNCTION__);
	xm_rp = platform_get_drvdata(pdev);
	ops = xm_rp->ops;

	if (ops && ops->init)
		rc = ops->init(xm_rp);

	return rc;
}

static void ispv4_rproc_ops_deinit(struct platform_device *pdev)
{
	struct xm_ispv4_rproc *xm_rp;
	struct xm_ispv4_ops *ops;

	dev_info(&pdev->dev, "%s entry", __FUNCTION__);
	xm_rp = platform_get_drvdata(pdev);
	ops = xm_rp->ops;

	if (ops && ops->deinit)
		ops->deinit(xm_rp);
}

static int ispv4_rproc_ops_boot(struct platform_device *pdev)
{
	int rc = 0;
	struct xm_ispv4_rproc *xm_rp;
	struct xm_ispv4_ops *ops;

	dev_info(&pdev->dev, "%s entry", __FUNCTION__);
	xm_rp = platform_get_drvdata(pdev);
	ops = xm_rp->ops;

	if (ops && ops->boot)
		rc = ops->boot(xm_rp);

	return rc;
}

static void ispv4_rproc_ops_earlydown(struct platform_device *pdev)
{
	struct xm_ispv4_rproc *xm_rp;
	struct xm_ispv4_ops *ops;

	dev_info(&pdev->dev, "%s entry", __FUNCTION__);
	xm_rp = platform_get_drvdata(pdev);
	ops = xm_rp->ops;

	if (ops && ops->earlydown)
		ops->earlydown(xm_rp);
}

static void ispv4_rproc_ops_deboot(struct platform_device *pdev)
{
	struct xm_ispv4_rproc *xm_rp;
	struct xm_ispv4_ops *ops;

	dev_info(&pdev->dev, "%s entry", __FUNCTION__);
	xm_rp = platform_get_drvdata(pdev);
	ops = xm_rp->ops;

	if (ops && ops->deboot)
		ops->deboot(xm_rp);
}

static void ispv4_rproc_ops_shutdown(struct platform_device *pdev)
{
	struct xm_ispv4_rproc *xm_rp;
	struct xm_ispv4_ops *ops;

	dev_info(&pdev->dev, "%s entry", __FUNCTION__);
	xm_rp = platform_get_drvdata(pdev);
	ops = xm_rp->ops;

	if (ops && ops->shutdown)
		ops->shutdown(xm_rp);

	return;
}

static void ispv4_rproc_ops_remove(struct platform_device *pdev)
{
	struct xm_ispv4_rproc *xm_rp;
	struct xm_ispv4_ops *ops;

	dev_info(&pdev->dev, "%s entry", __FUNCTION__);
	xm_rp = platform_get_drvdata(pdev);
	ops = xm_rp->ops;

	if (ops && ops->remove)
		ops->remove(xm_rp);

	return;
}

static int ispv4_firmware_find_section(struct device *dev,
				       const struct firmware *fw,
				       uint32_t *log_da, uint32_t *buf_size,
				       const char *sec_name)
{
	const void *shdr, *name_table_shdr;
	int i;
	const char *name_table;
	const u8 *elf_data = (void *)fw->data;
	u8 class = fw_elf_get_class(fw);
	const void *ehdr = elf_data;
	u16 shnum = elf_hdr_get_e_shnum(class, ehdr);
	u32 elf_shdr_get_size = elf_size_of_shdr(class);
	u16 shstrndx = elf_hdr_get_e_shstrndx(class, ehdr);

	shdr = elf_data + elf_hdr_get_e_shoff(class, ehdr);
	name_table_shdr = shdr + (shstrndx * elf_shdr_get_size);
	name_table = elf_data + elf_shdr_get_sh_offset(class, name_table_shdr);

	for (i = 0; i < (int)shnum; i++, shdr += elf_shdr_get_size) {
		u64 size = elf_shdr_get_sh_size(class, shdr);
		u32 name = elf_shdr_get_sh_name(class, shdr);
		u64 da = elf_shdr_get_sh_addr(class, shdr);
		if (strcmp(name_table + name, sec_name))
			continue;

		if (log_da != NULL && buf_size != NULL) {
			*log_da = da;
			*buf_size = size;
			return 0;
		} else {
			return -ENOPARAM;
		}
	}
	return -ENODATA;
}

static int ispv4_firmware_find_logaddr(struct device *dev,
				       const struct firmware *fw,
				       uint32_t *log_da, uint32_t *buf_size)
{
	return ispv4_firmware_find_section(dev, fw, log_da, buf_size,
					   XM_ISPV4_LOGSEG_NAME);
}

static int ispv4_firmware_find_deubginfoaddr(struct device *dev,
					     const struct firmware *fw,
					     uint32_t *log_da,
					     uint32_t *buf_size)
{
	return ispv4_firmware_find_section(dev, fw, log_da, buf_size,
					   XM_ISPV4_LOCKINFOSEG_NAME);
}

static int ispv4_firmware_find_bootinfoaddr(struct device *dev,
					    const struct firmware *fw,
					    uint32_t *log_da,
					    uint32_t *buf_size)
{
	return ispv4_firmware_find_section(dev, fw, log_da, buf_size,
					   XM_ISPV4_BOOTINFOSEG_NAME);
}

#ifdef CHECK_FW
#define CHECK_NUM_LEN 16
const char *check_magic = "v400chipfirmware___";
static int check_fw(const struct firmware *rfw, struct firmware *fw)
{
	u32 digestsize = 0;
	struct crypto_shash *md5;
	int ret = 0, i;
	u8 check_tmp[CHECK_NUM_LEN * 2];
	u8 check_ret[CHECK_NUM_LEN];
	SHASH_DESC_ON_STACK(smd5, md5);

	for (i = 0; i < CHECK_NUM_LEN; i++) {
		check_tmp[i] = check_magic[i];
	}

	md5 = crypto_alloc_shash("md5", 0, 0);
	if (IS_ERR(md5)) {
		ret = PTR_ERR(md5);
		pr_err("No crypto hash md5 %d\n", ret);
		goto crypto_alloc_failed;
	}

	digestsize = crypto_shash_digestsize(md5);
	pr_err("ispv4 digestsize %d\n", digestsize);
	if (digestsize != CHECK_NUM_LEN) {
		pr_err("ispv4 fw check num size err\n");
		goto init_failed;
	}
	smd5->tfm = md5;

	ret = crypto_shash_init(smd5);
	if (ret != 0)
		goto init_failed;

	ret = crypto_shash_update(smd5, rfw->data + 2 * CHECK_NUM_LEN,
				  rfw->size - 2 * CHECK_NUM_LEN);
	if (ret != 0)
		goto init_failed;

	ret = crypto_shash_final(smd5, &check_tmp[CHECK_NUM_LEN]);
	if (ret != 0)
		goto init_failed;

	ret = crypto_shash_init(smd5);
	if (ret != 0)
		goto init_failed;

	crypto_shash_update(smd5, check_tmp, sizeof(check_tmp));
	ret = crypto_shash_final(smd5, check_ret);
	if (ret != 0)
		goto init_failed;

	for (i = 0; i < CHECK_NUM_LEN; i++) {
		if (rfw->data[i] != check_ret[i]) {
			pr_err("ispv4 fw check failed!!\n");
			ret = -EFAULT;
			goto init_failed;
		}
	}

	fw->data = &rfw->data[CHECK_NUM_LEN * 2];
	fw->size = rfw->size - (CHECK_NUM_LEN * 2);
	fw->priv = rfw->priv;

	crypto_free_shash(md5);
	return 0;

init_failed:
	crypto_free_shash(md5);
crypto_alloc_failed:
	return ret;
}
#else
static int check_fw(const struct firmware *rfw, struct firmware *fw)
{
	fw->data = rfw->data;
	fw->size = rfw->size;
	fw->priv = rfw->priv;
	return 0;
}
#endif

static int ispv4_request_firmware(struct xm_ispv4_rproc *rp)
{
	int rc = 0;
	if (rp->origin_fw != NULL) {
		dev_info(rp->dev, "ISPV4 re-request firmware from fs.\n");
		release_firmware(rp->origin_fw);
		rp->origin_fw = NULL;
	}

	rp->fw = &rp->fw_obj;
	rc = request_firmware(&rp->origin_fw, XM_ISPV4_FW_NAME, rp->dev);
	if (rc < 0) {
		dev_err(rp->dev, "ISPV4 Request firmware `%s` failed, rc=%d.\n",
			XM_ISPV4_FW_NAME, rc);
		return rc;
	}

	rc = check_fw(rp->origin_fw, rp->fw);
	if (rc != 0) {
		release_firmware(rp->origin_fw);
		rp->origin_fw = NULL;
		return rc;
	}

	rc = rproc_fw_sanity_check(rp->rproc, rp->fw);
	if (rc) {
		dev_err(rp->dev, "fw check avalid EFL return %d", rc);
		release_firmware(rp->origin_fw);
		rp->origin_fw = NULL;
		return rc;
	}

	rc = ispv4_firmware_find_logaddr(rp->dev, rp->fw, &rp->ramlog_da,
					 &rp->ramlog_buf_size);
	if (rc) {
		dev_warn(rp->dev, "fw find log addr return %d", rc);
		rp->ramlog_da = 0;
		rp->ramlog_buf_size = 0;
	}
	dev_info(rp->dev, "logaddr da:0x%x, sz:%d\n", rp->ramlog_da,
		 rp->ramlog_buf_size);
	rc = ispv4_firmware_find_deubginfoaddr(
		rp->dev, rp->fw, &rp->debuginfo_da, &rp->debuginfo_buf_size);
	if (rc) {
		dev_warn(rp->dev, "fw find deubginfo addr return %d", rc);
		rp->debuginfo_da = 0;
		rp->debuginfo_buf_size = 0;
	}
	dev_info(rp->dev, "debuginfo addr da:0x%x, sz:%d\n", rp->debuginfo_da,
		 rp->debuginfo_buf_size);

	rc = ispv4_firmware_find_bootinfoaddr(rp->dev, rp->fw, &rp->bootinfo_da,
					      &rp->bootinfo_buf_size);
	if (rc) {
		dev_warn(rp->dev, "fw find bootinfo addr return %d", rc);
		rp->bootinfo_da = 0;
		rp->bootinfo_buf_size = 0;
	}
	dev_info(rp->dev, "bootinfo addr da:0x%x, sz:%d\n", rp->bootinfo_da,
		 rp->bootinfo_buf_size);

	return 0;
}

static int ispv4_parse_firmware(struct xm_ispv4_rproc *rp)
{
	int ret;

	ret = rproc_fw_sanity_check(rp->rproc, rp->fw);
	if (ret) {
		dev_err(rp->dev, "fw sanity check failed %d \n", ret);
		return ret;
	}

	rp->bootaddr = rproc_get_boot_addr(rp->rproc, rp->fw);
	if (rp->bootaddr == 0) {
		dev_err(rp->dev, "fw get boot addr is 0 failed\n");
		return -EINVAL;
	}

	ret = rproc_parse_fw(rp->rproc, rp->fw);
	if (ret) {
		dev_err(rp->dev, "fw parse fw failed %d \n", ret);
		return ret;
	}

	dev_info(rp->dev, "parse fw ok!\n");
	dev_info(rp->dev, "rsc ptr: 0x%llx\n", rp->rproc->table_ptr);
	dev_info(rp->dev, "rsc size: 0x%x\n", rp->rproc->table_sz);
	if (rp->rproc->table_ptr)
		dev_info(rp->dev, "rsc dev num: %d\n",
			 rp->rproc->table_ptr->num);
	return 0;
}

int ispv4_load_by_dma(struct xm_ispv4_rproc *rp, void *data, int len,
		      u32 dev_addr)
{
	struct pcie_hdma *hdma = rp->pdma;
	struct device *pci_dev = rp->pci_dev;
	dma_addr_t da;
	void *va;
	struct pcie_hdma_chan_ctrl *ctrl;
	int ret;
	//ktime_t times, timed;
	//int time_ms;

	va = dma_alloc_coherent(pci_dev, len, &da, GFP_KERNEL);
	if (va == NULL) {
		dev_err(pci_dev, "alloc dma coherent failed.\n");
		ret = -ENOMEM;
		goto dma_alloc_failed;
	}
	ctrl = ispv4_hdma_request_chan(hdma, HDMA_FROM_DEVICE);
	if (ctrl == NULL) {
		dev_err(rp->dev, "request dma channel failed.\n");
		ret = -EINVAL;
		goto request_chan_failed;
	}
	ret = ispv4_hdma_xfer_add_block(ctrl, da, dev_addr, len);
	if (ret != 0) {
		dev_err(rp->dev, "hdma add block failed, ret=%d.\n", ret);
		goto add_block_failed;
	}
	memcpy(va, data, len);
	//times = ktime_get();
	ret = ispv4_pcie_hdma_start_and_wait_end(hdma, ctrl);
	//timed = ktime_get();
	//timed = ktime_sub(timed, times);
	//time_ms = ktime_to_ms(timed);
	//dev_info(rp->dev, "dma trans len=%dBytes in %dms", len, time_ms);
	if (ret != 0) {
		dev_err(rp->dev, "hdma start failed, ret=%d.\n", ret);
		goto send_failed;
	}

	ispv4_hdma_release_chan(ctrl);
	dma_free_coherent(pci_dev, len, va, da);
	return 0;

send_failed:
add_block_failed:
	ispv4_hdma_release_chan(ctrl);
request_chan_failed:
	dma_free_coherent(pci_dev, len, va, da);
dma_alloc_failed:
	return ret;
}

static int __real_load(struct xm_ispv4_rproc *rp, const u8 *elf_data,
		       u64 offset, u64 filesz, u64 da, void *ptr)
{
	int ret = 0;
	if (RP_PCI(rp)) {
		if (p_load_by_dma) {
			ret = ispv4_load_by_dma(rp, (void *)(elf_data + offset),
						filesz, (u64)da);
		} else
			memcpy_toio(ptr, elf_data + offset, filesz);
	} else if (RP_SPI(rp)) {
		ispv4_regops_long_burst_write(
			(u64)da, (u8 *)(elf_data + offset), filesz);
	} else if (RP_FAKE(rp)) {
		memcpy(ptr, elf_data + offset, filesz);
	}

	return ret;
}

static int ispv4_load_firmware(struct xm_ispv4_rproc *rp,
			       enum xm_ispv4_memregion region)
{
	struct device *dev = &rp->rproc->dev;
	const void *ehdr, *phdr;
	int i, ret = 0;
	u16 phnum;
	const u8 *elf_data = rp->fw->data;
	enum xm_ispv4_memregion da_region;
	//ktime_t times, timed;

	u8 class = fw_elf_get_class(rp->fw);
	u32 elf_phdr_get_size = elf_size_of_phdr(class);

	pr_info("ispv4 prepare load fw!\n");

	ehdr = elf_data;
	phnum = elf_hdr_get_e_phnum(class, ehdr);
	phdr = elf_data + elf_hdr_get_e_phoff(class, ehdr);

	/* go through the available ELF segments */
	for (i = 0; i < phnum; i++, phdr += elf_phdr_get_size) {
		u64 da = elf_phdr_get_p_paddr(class, phdr);
		u64 memsz = elf_phdr_get_p_memsz(class, phdr);
		u64 filesz = elf_phdr_get_p_filesz(class, phdr);
		u64 offset = elf_phdr_get_p_offset(class, phdr);
		u32 type = elf_phdr_get_p_type(class, phdr);
		void *ptr;
		bool is_iomem;

		if (type != PT_LOAD)
			continue;

		//dev_info(dev,
		//	 "phdr: type %d da 0x%llx memsz 0x%llx filesz 0x%llx\n",
		//	 type, da, memsz, filesz);

		if (filesz > memsz) {
			dev_err(dev, "bad phdr filesz 0x%llx memsz 0x%llx\n",
				filesz, memsz);
			ret = -EINVAL;
			break;
		}

		if (offset + filesz > rp->fw->size) {
			dev_err(dev, "truncated fw: need 0x%llx avail 0x%zx\n",
				offset + filesz, rp->fw->size);
			ret = -EINVAL;
			break;
		}

		if (!rproc_u64_fit_in_size_t(memsz)) {
			dev_err(dev,
				"size (%llx) does not fit in size_t type\n",
				memsz);
			ret = -EOVERFLOW;
			break;
		}

		/* grab the kernel address for this device address */
		ptr = rproc_da_to_va(rp->rproc, da, memsz, &is_iomem);
		// Don't check if spi
		if (RP_PCI(rp) || RP_FAKE(rp)) {
			if (!ptr) {
				dev_err(dev, "bad phdr da 0x%llx mem 0x%llx\n",
					da, memsz);
				ret = -EINVAL;
				break;
			}
		}

		da_region = ispv4_devaddr_match_region(da);
		if (da_region == ISPV4_MEMREGION_UNKNOWN) {
			dev_err(dev, "unknown region da=0x%llx!\n", da);
			ret = -EINVAL;
			break;
		}

		/* put the segment where the remote processor expects it */
		if (filesz && region == da_region) {
			//times = ktime_get();
			if (is_iomem) {
				dev_err(dev, "Please not set iomem\n");
				dump_stack();
				ret = -EINVAL;
			} else {
				ret = __real_load(rp, elf_data, offset, filesz,
						  da, ptr);
				if (ret != 0)
					break;
			}
			//timed = ktime_get();
			//timed = ktime_sub(timed, times);
			//dev_info(
			//	rp->dev,
			//	"load fw da=0x%x va=0x%x, size=%d, time=%dms\n",
			//	da, ptr, filesz, ktime_to_ms(timed));
		}

		/*
		 * Zero out remaining memory for this segment.
		 *
		 * This isn't strictly required since dma_alloc_coherent already
		 * did this for us. albeit harmless, we may consider removing
		 * this.
		 */
		if (memsz > filesz && region == da_region) {
			if (is_iomem)
				memset_io((void __iomem *)(ptr + filesz), 0,
					  memsz - filesz);
			else {
				dev_info(
					rp->dev,
					"(no real ops)memset 0 va=0x%x, size=%d\n",
					ptr, memsz - filesz);
				// if (rp->ops == &ispv4_pci_ops || rp->ops == &ispv4_fake_ops) {
				// 	memset(ptr + filesz, 0, memsz - filesz);
				// } else if (rp->ops == &ispv4_spi_ops) {
				// 	ispv4_regops_burst_clear((u64)(ptr + filesz), memsz - filesz);
				// }
			}
		}
	}

	return ret;
}

/* Call from attach */
int ispv4_load_rsc_table(struct rproc *rproc, const struct firmware *fw)
{
	struct resource_table *loaded_table;
	struct xm_ispv4_rproc *rp = rproc->priv;

	dev_info(&rproc->dev, "prepare load rsc!\n");
	loaded_table = rproc_find_loaded_rsc_table(rproc, fw);
	if (loaded_table) {
		dev_info(&rproc->dev, "rsc copy to va:0x%llx!\n", loaded_table);
		if (rp->ops == &ispv4_pci_ops || rp->ops == &ispv4_fake_ops) {
			memcpy(loaded_table, rproc->cached_table,
			       rproc->table_sz);
		} else if (rp->ops == &ispv4_spi_ops) {
			ispv4_regops_burst_write((u64)loaded_table,
						 (u8 *)rproc->cached_table,
						 rproc->table_sz);
		}
		rproc->table_ptr = loaded_table;
		return 0;
	}
	dev_warn(&rproc->dev, "Not found resource table\n");
	return -1;
}

int ispv4_rproc_down(struct xm_ispv4_rproc *rp)
{
	struct platform_device *pdev =
		container_of(rp->dev, struct platform_device, dev);
	if (rp->state == XM_ISPV4_RPROC_STATE_OFFLINE) {
		return 0;
	}

	/* Stop send and wait all context exit */
	xm_ispv4_flush_send_stop(rp);
	ispv4_rproc_ops_earlydown(pdev);
	if (rp->irq_crash > 0)
		disable_irq(rp->irq_crash);
	rproc_shutdown(rp->rproc);
	ispv4_rproc_ops_deboot(pdev);
	ispv4_ramlog_deboot(rp);
	ispv4_mbox_excep_deboot(rp);
	rp->state = XM_ISPV4_RPROC_STATE_OFFLINE;
	return 0;
}

int ispv4_rproc_boot_stage2(struct xm_ispv4_rproc *rp)
{
	int rc = 0;
	rc = ispv4_load_firmware(rp, ISPV4_MEMREGION_DDR);
	if (rc != 0) {
		dev_err(rp->dev, "ispv4 load ddr code failed.\n");
	}
	return rc;
}

static void boot_mbox_callback(struct mbox_client *cl, void *mssg)
{
	u8 *data = mssg;
	struct xm_ispv4_rproc *rp =
		container_of(cl, struct xm_ispv4_rproc, mbox_boot);

	pr_alert("ispv4 boot stage = 0x%x error code = 0x%x",
		 data[BOOT_RET_INFO], data[BOOT_ERROR_INFO]);

	rp->boot_stage_info = 0;

	if (rp->boot_stage == BOOT_STAGE0) {
		if (data[BOOT_RET_INFO] == BOOT_PASS_1STAGE)
			rp->boot_stage = BOOT_STAGE1;
		else
			rp->boot_stage = BOOT_STAGE_ERR;
		complete(&rp->cpl_boot);
	} else if (rp->boot_stage == BOOT_STAGE1) {
		if (data[BOOT_RET_INFO] == BOOT_PASS_2STAGE) {
			// TODO: always restore ddrtf, remove this
			rp->boot_stage_info = BOOT_INFO_DDR_SAVE_DDRTF;
			rp->boot_stage = BOOT_STAGE2;
		} else
			rp->boot_stage = BOOT_STAGE_ERR;
		complete(&rp->cpl_boot);
	}
}

static void register_ddrfb_notify(struct xm_ispv4_rproc *rp,
				  int (*fn)(void *, int), void *priv)
{
	rp->notify_updateddr_priv = priv;
	rp->notify_updateddr = fn;
}

static void register_rpmsgready_notify(struct xm_ispv4_rproc *rp,
				       int (*fn)(void *, int, bool), void *priv)
{
	rp->rpmsg_ready_cb = fn;
	rp->rpmsg_ready_cb_thiz = priv;
}

static int ispv4_fullboot_ddr_thread(void *data)
{
	struct xm_ispv4_rproc *rp = data;
	int rc = 0;

	dev_info(rp->dev, "%s Entry\n", __FUNCTION__);
	rc = wait_for_completion_timeout(&rp->cpl_boot, msecs_to_jiffies(8000));
	/* release channel */
	mbox_free_channel(rp->mbox_boot_chan);
	if (rc == 0) {
		if (rp->notify_updateddr != NULL)
			rp->notify_updateddr(rp->notify_updateddr_priv,
					     -ETIMEDOUT);
	} else {
		if (rp->boot_stage != BOOT_DDR_FULL_BOOT_OK) {
			if (rp->notify_updateddr != NULL)
				rp->notify_updateddr(rp->notify_updateddr_priv,
						     -EFAULT);
		} else {
			/* v400 has dump info into ionmap buffer. */
			/* Notify userspace traning data dump finish. */
			if (rp->notify_updateddr != NULL)
				rp->notify_updateddr(rp->notify_updateddr_priv,
						     DDR_FULL_BOOT);
		}
	}
	rp->fullboot_th = NULL;
	/* Clear boot flag */
	rp->boot_stage = BOOT_STAGE0;
	dev_info(rp->dev, "%s Exit\n", __FUNCTION__);
	return 0;
}

// TODO : add ddr freq param info.
static u32 SEND_FOR_START_S2ST[4] = { 0, 0, 0, 1 };

static int timeout = 10000;
module_param_named(stout, timeout, int, 0644);

static int ispv4_deal_ddrtf(struct xm_ispv4_rproc *rp, bool load, bool use_dma)
{
	struct pcie_hdma *hdma = rp->pdma;
	struct pcie_hdma_chan_ctrl *ctrl;
	int ret;
	//ktime_t times, timed;
	//int time_ms;
	void *iramva;
	bool is_iomem;
	enum pcie_hdma_dir dir = load ? HDMA_FROM_DEVICE : HDMA_TO_DEVICE;
	u32 src = load ? rp->ddrtf_iova : XM_ISPV4_DDRTF_IRAM_OFF;
	u32 dest = load ? XM_ISPV4_DDRTF_IRAM_OFF : rp->ddrtf_iova;

	if (rp->ddrtf_buf == NULL)
		return -ENOBUFS;

	if (!RP_PCI(rp))
		return -EIO;

	if (load && !rp->ddr_buf_avalid) {
		dev_err(rp->dev, "buf is not avalid.\n");
		return -EINVAL;
	}

	// Dump use cpu.
	if (!load && !use_dma) {
		iramva = rproc_da_to_va(rp->rproc, XM_ISPV4_DDRTF_IRAM_OFF,
					XM_ISPV4_DDRTF_REALSIZE, &is_iomem);
		if (iramva == NULL)
			return -EFAULT;
		memcpy_fromio(rp->ddrtf_buf, iramva, XM_ISPV4_DDRTF_REALSIZE);
		return 0;
	}
	(void)is_iomem;

	ctrl = ispv4_hdma_request_chan(hdma, dir);
	if (ctrl == NULL) {
		dev_err(rp->dev, "request dma channel failed.\n");
		ret = -EINVAL;
		goto request_chan_failed;
	}
	ret = ispv4_hdma_xfer_add_block(ctrl, src, dest,
					XM_ISPV4_DDRTF_REALSIZE);
	if (ret != 0) {
		dev_err(rp->dev, "hdma add block failed, ret=%d.\n", ret);
		goto add_block_failed;
	}
	//times = ktime_get();
	ret = ispv4_pcie_hdma_start_and_wait_end(hdma, ctrl);
	//timed = ktime_get();
	//timed = ktime_sub(timed, times);
	//time_ms = ktime_to_ms(timed);
	//dev_info(rp->dev, "dma trans len=%dBytes in %dms",
	//	 XM_ISPV4_DDRTF_REALSIZE, time_ms);
	if (ret != 0) {
		dev_err(rp->dev, "hdma start failed, ret=%d.\n", ret);
		goto send_failed;
	}
	ispv4_hdma_release_chan(ctrl);

	return 0;

send_failed:
add_block_failed:
	ispv4_hdma_release_chan(ctrl);
request_chan_failed:
	return -EFAULT;
}

static void ispv4_try_dump_ddrtf(struct xm_ispv4_rproc *rp, bool dma)
{
	int rc;
	if ((rp->boot_stage_info & BOOT_INFO_DDR_SAVE_DDRTF) == 0)
		return;

	dev_info(rp->dev, "will dump ddr traning file from v4 to ap");
	rc = ispv4_deal_ddrtf(rp, DDR_DUMP, dma);
	if (rc != 0) {
		dev_err(rp->dev, "ispv4 dump ddrtf failed %d.\n", rc);
		rp->ddr_buf_avalid = false;
	} else {
#ifdef USE_USERHELPER
		ispv4_user_dealfile(UD_SAVE_DDRTF);
#endif
		rp->ddr_buf_avalid = true;
		rp->ddr_buf_update = true;
	}
}

__maybe_unused static int ispv4_user_dealfile(enum user_deal deal)
{
	int ret;
	char *cmdpath = "/system/bin/cp";
	const struct user_deal_path path[] = {
		[UD_SAVE_RAMLOG] = { UD_PS "ramlog", UD_PD "ramlog_", true },
		[UD_SAVE_BOOTINFO] = { UD_PS "bootinfo", UD_PD "bootinfo_",
				       true },
		[UD_SAVE_DEBUGINFO] = { UD_PS "debuginfo", UD_PD "debuginfo_",
					true },
		[UD_SAVE_DDRTF] = { UD_PS "ddrfw", UD_PD "ddrfw", false },
		[UD_LOAD_DDRTF] = { UD_PD "ddrfw", UD_PD "ddrfw", false },
	};
	const struct user_deal_path *use_path = &path[deal];
	char real_dest[128] = { 0 };
	int wait = UMH_WAIT_PROC;
	struct subprocess_info *info;
	gfp_t gfp_mask = GFP_KERNEL;
	static char *envp[] = { "HOME=/", "PATH=/sbin:/bin:/usr/sbin:/usr/bin",
				NULL };
	char *argv[] = { cmdpath, use_path->src, real_dest, NULL };

	if (use_path->time)
		snprintf(real_dest, sizeof(real_dest), "%s%ld", use_path->dest,
			 ktime_get());
	else
		snprintf(real_dest, sizeof(real_dest), "%s", use_path->dest);

	info = call_usermodehelper_setup(cmdpath, argv, envp, gfp_mask, NULL,
					 NULL, NULL);
	if (info == NULL)
		return -ENOMEM;

	info->path = cmdpath;

	ret = call_usermodehelper_exec(info, wait);
	pr_info("ispv4 userhelper src:%s dest:%s ret=%d", use_path->src,
		real_dest, ret);
	return ret;
}

bool ispv4_get_boot_status(struct xm_ispv4_rproc *rp)
{
	if (rp->state == XM_ISPV4_RPROC_STATE_RUN)
		return true;
	else
		return false;
}

static int __rproc_boot(struct xm_ispv4_rproc *rp, bool *only_s1_boot,
			bool force_s1)
{
	int rc = 0;
	bool need_dumplog = false;
	struct platform_device *pdev =
		container_of(rp->dev, struct platform_device, dev);
	// Assume not update buf, late will set if needed
	rp->ddr_buf_update = false;

	if (rp->state == XM_ISPV4_RPROC_STATE_RUN)
		return 0;

	if (!force_s1) {
		rp->mbox_boot_chan = mbox_request_channel(&rp->mbox_boot, 0);
		if (IS_ERR_OR_NULL(rp->mbox_boot_chan)) {
			dev_err(rp->dev, "request mbox chan boot failed\n");
			rc = PTR_ERR(rp->mbox_boot_chan);
			goto boot_mbox_failed;
		}
	}

	/* Lazy prepare firmware */
	if (!rp->prepare_fw_finish) {
		rc = ispv4_prepare_firmware(rp);
		if (rc != 0)
			goto pre_fw_err;
	} else if (rp->reload_fw) {
		rc = ispv4_request_firmware(rp);
		if (rc != 0) {
			dev_err(rp->dev, "ispv4 failed request firmware %d.\n",
				rc);
			goto pre_fw_err;
		}
	}

	rc = ispv4_parse_firmware(rp);
	if (rc != 0) {
		dev_err(rp->dev, "ispv4 parse firmware failed %d.\n", rc);
		goto pre_fw_err;
	}

	pr_info("ispv4 rproc load fw\n");
	rc = ispv4_load_firmware(rp, ISPV4_MEMREGION_SRAM);
	if (rc != 0) {
		dev_err(rp->dev, "ispv4 load firmware failed %d.\n", rc);
		goto load_fw_err;
	}

	dev_info(rp->dev, "will load ddr traning file from ap to v4");
	rc = ispv4_deal_ddrtf(rp, DDR_LOAD, true);
	if (rc != 0) {
		dev_err(rp->dev, "ispv4 load ddrtf failed %d.\n", rc);
	}

	rp->rproc->state = RPROC_DETACHED;

	rc = ispv4_rproc_ops_boot(pdev);
	if (rc != 0) {
		dev_err(rp->dev, "ispv4 boot pre failed %d.\n", rc);
		goto ops_boot_err;
	}

	reinit_completion(&rp->cpl_boot);

	rc = ispv4_ramlog_boot(rp);
	if (rc != 0) {
		dev_err(rp->dev, "init ramlog failed %d.\n", rc);
		goto ramlog_err;
	}

	rc = ispv4_mbox_excep_boot(rp);
	if (rc != 0) {
		dev_err(rp->dev, "init exception mbox failed %d.\n", rc);
		goto exp_mbox_err;
	}

	if (rp->irq_crash > 0)
		enable_irq(rp->irq_crash);

	rc = rproc_boot(rp->rproc);
	if (rc != 0) {
		dev_err(rp->dev, "ispv4 boot firmware failed %d.\n", rc);
		goto boot_err;
	}

	need_dumplog = true;

	if (!force_s1) {
		rc = wait_for_completion_interruptible_timeout(
			&rp->cpl_boot, msecs_to_jiffies(timeout));
		if (rc == 0) {
			dev_err(rp->dev,
				"wait for boot 1st completion timeout!\n");
			rc = -ETIMEDOUT;
			goto s1_start_err;
		} else if (rc == -ERESTARTSYS) {
			dev_err(rp->dev, "kill boot thread.!\n");
			rc = -ETIMEDOUT;
			goto s1_start_err;
		}
		reinit_completion(&rp->cpl_boot);

		/* Full boot ddr traning, will return ok. */
		if (rp->boot_stage == BOOT_DDR_FULL_BOOT) {
			dev_warn(rp->dev, "boot 1st up, full traning ddr.\n");
			if (only_s1_boot != NULL)
				*only_s1_boot = true;
			// mbox_free_channel(rp->mbox_boot_chan);
			reinit_completion(&rp->cpl_boot);
			if (rp->fullboot_th != NULL)
				dev_err(rp->dev,
					"Note Note!!! last thread is running.\n");
			/* trigger thread ispv4_fullboot_ddr_thread */
			rp->fullboot_th = kthread_run(ispv4_fullboot_ddr_thread,
						      rp, "ispv4-ddr-fullboot");
			rp->state = XM_ISPV4_RPROC_STATE_RUN;
			return 0;
		}

		// if (rp->notify_updateddr != NULL)
		// 	rp->notify_updateddr(rp->notify_updateddr_priv, DDR_QUICK_BOOT);
		if (only_s1_boot != NULL)
			*only_s1_boot = false;

		if (rp->boot_stage != BOOT_STAGE1) {
			dev_err(rp->dev, "boot 1st meet error %d!\n",
				rp->boot_stage);
			goto s1_start_err;
		}

		ispv4_try_dump_ddrtf(rp, true);
		ispv4_rproc_boot_stage2(rp);
		SEND_FOR_START_S2ST[0] = 0;
		mbox_send_message(rp->mbox_boot_chan,
				  (void *)&SEND_FOR_START_S2ST[0]);

		rc = wait_for_completion_timeout(&rp->cpl_boot,
						 msecs_to_jiffies(timeout));
		if (!rc) {
			dev_err(rp->dev,
				"wait for boot 2st completion timeout!\n");
			rc = -ETIMEDOUT;
			goto s2_start_err;
		}
		reinit_completion(&rp->cpl_boot);

		if (rp->boot_stage != BOOT_STAGE2) {
			dev_err(rp->dev, "boot 2st meet error %d!\n",
				rp->boot_stage);
			goto s2_start_err;
		}

		mbox_free_channel(rp->mbox_boot_chan);
		dev_info(rp->dev, "2stage boot finish\n");

		// Finish, reset boot stage.
		rp->boot_stage = BOOT_STAGE0;
	}

	ispv4_try_dump_ddrtf(rp, false);

	rp->state = XM_ISPV4_RPROC_STATE_RUN;
	return 0;

s2_start_err:
s1_start_err:
	ispv4_dump_status(rp);
	ispv4_rproc_pci_earlydown(rp);
	rproc_shutdown(rp->rproc);
boot_err:
	if (rp->irq_crash > 0)
		disable_irq(rp->irq_crash);
	ispv4_mbox_excep_deboot(rp);
exp_mbox_err:
	ispv4_ramlog_deboot(rp);
ramlog_err:
	ispv4_rproc_ops_deboot(pdev);
ops_boot_err:
load_fw_err:
pre_fw_err:
	if (!force_s1)
		mbox_free_channel(rp->mbox_boot_chan);
boot_mbox_failed:
	if (need_dumplog) {
		ispv4_dump_inner128();
		ispv4_dump_ramlog(rp);
		ispv4_dump_bootinfo(rp);
	} else
		pr_crit("ispv4 boot failed without dumplog");
	rp->boot_stage = BOOT_STAGE0;
	return rc;
}

int ispv4_rproc_boot(struct xm_ispv4_rproc *rp, bool *only_s1_boot)
{
	return __rproc_boot(rp, only_s1_boot, false);
}

int ispv4_rproc_boot_s1(struct xm_ispv4_rproc *rp)
{
	return __rproc_boot(rp, NULL, true);
}

static void register_crash_notify(struct xm_ispv4_rproc *rp, xm_crash_nf_t f,
				  void *thiz)
{
	rp->crash_notify = f;
	rp->crash_notify_thiz = thiz;
}

static void register_ipc_notify(struct xm_ispv4_rproc *rp, xm_ipc_cb_t f,
				void *thiz)
{
	rp->ipc_ept_recv_notify = f;
	rp->ipc_ept_recv_notify_thiz = thiz;
}

// __weak void register_exception_cb(struct xm_ispv4_rproc *rp,
// 				  int (*cb)(void *, void *, int),
// 				  void *priv)
// {
// 	pr_info("ispv4: not impl %s!\n", __FUNCTION__);
// }

__weak int ispv4_mbox_excep_boot(struct xm_ispv4_rproc *rp)
{
	pr_info("ispv4: not impl %s!\n", __FUNCTION__);
	return 0;
}

__weak void ispv4_mbox_excep_deboot(struct xm_ispv4_rproc *rp)
{
	pr_info("ispv4: not impl %s!\n", __FUNCTION__);
}

__weak int ispv4_ramlog_init(struct xm_ispv4_rproc *rp)
{
	pr_info("ispv4: not impl %s!\n", __FUNCTION__);
	return 0;
}

__weak void ispv4_ramlog_deinit(struct xm_ispv4_rproc *rp)
{
	pr_info("ispv4: not impl %s!\n", __FUNCTION__);
}

__weak int ispv4_ramlog_boot(struct xm_ispv4_rproc *rp)
{
	pr_info("ispv4: not impl %s!\n", __FUNCTION__);
	return 0;
}

__weak void ispv4_ramlog_deboot(struct xm_ispv4_rproc *rp)
{
	pr_info("ispv4: not impl %s!\n", __FUNCTION__);
}

static int ispv4_dump_trigger(void *data, u64 val)
{
	struct xm_ispv4_rproc *rp = data;
	switch (val) {
	case 1:
		ispv4_dump_ramlog(rp);
		break;
	case 2:
		ispv4_dump_bootinfo(rp);
		break;
	case 3:
		ispv4_dump_debuginfo(rp);
		break;
	}
	return 0;
}

const char *ispv4_boot_param[XM_ISPV4_RPROC_STATE_MAX] = {
	[XM_ISPV4_RPROC_STATE_OFFLINE] = "offline\n",
	[XM_ISPV4_RPROC_STATE_RUN] = "run\n",
};

static ssize_t ispv4_debug_boot_get(struct file *filp, char __user *userbuf,
				    size_t count, loff_t *ppos)
{
	char buf[16];
	struct xm_ispv4_rproc *rp = filp->private_data;
	int i;

	i = scnprintf(buf, sizeof(buf), "%s", ispv4_boot_param[rp->state]);

	return simple_read_from_buffer(userbuf, count, ppos, buf, i);
}

static ssize_t ispv4_debug_boot_set(struct file *f, const char __user *data,
				    size_t len, loff_t *off)
{
	int ret = 0;
	char k_buf[16];
	struct xm_ispv4_rproc *rp = f->private_data;

	if (len > 8)
		return -ENOPARAM;

	(void)copy_from_user(k_buf, data, len);
	k_buf[15] = 0;

	pr_info("ispv4 debug boot set\n");
	if (!strcmp(k_buf, ispv4_boot_param[XM_ISPV4_RPROC_STATE_OFFLINE]) &&
	    rp->state != XM_ISPV4_RPROC_STATE_OFFLINE) {
		pr_info("ispv4 boot cmd `stop`\n");
		ret = ispv4_rproc_down(rp);
	} else if (!strcmp(k_buf, ispv4_boot_param[XM_ISPV4_RPROC_STATE_RUN]) &&
		   rp->state != XM_ISPV4_RPROC_STATE_RUN) {
		pr_info("ispv4 boot cmd `run`\n");
		ret = ispv4_rproc_boot(rp, NULL);
	} else if (!strcmp(k_buf, "s1run\n") &&
		   rp->state != XM_ISPV4_RPROC_STATE_RUN) {
		pr_info("ispv4 boot cmd `s1run`\n");
		ret = ispv4_rproc_boot_s1(rp);
	}

	if (ret != 0)
		return ret;

	return len;
}

DEFINE_DEBUGFS_ATTRIBUTE(ispv4_tr_dump_fops, NULL, ispv4_dump_trigger,
			 "%llu\n");

static const struct file_operations ispv4_debug_boot_ops = {
	.write = ispv4_debug_boot_set,
	.read = ispv4_debug_boot_get,
	.open = simple_open,
	.llseek = generic_file_llseek,
};

static void ispv4_rproc_debugfs(struct xm_ispv4_rproc *rp)
{
	int i, ret;
	const char *file_name[] = { "boot", "dump_tr" };
	umode_t file_mode[] = { 0666, 0444 };
	struct dentry **file_entry[] = { &rp->debug_boot, &rp->debug_dump_tr };
	const struct file_operations *fops[] = {
		&ispv4_debug_boot_ops,
		&ispv4_tr_dump_fops,
	};

	rp->debug_dir = debugfs_create_dir("ispv4_rproc", ispv4_debugfs);
	if (IS_ERR_OR_NULL(rp->debug_dir)) {
		dev_err(rp->dev, "create debug dir `ispv4_rproc` failed %d.\n",
			PTR_ERR(rp->debug_dir));
	}

	debugfs_create_bool("fw_reload", 0666, rp->debug_dir, &rp->reload_fw);

	for (i = 0; i < ARRAY_SIZE(file_name); i++) {
		*file_entry[i] = debugfs_create_file(
			file_name[i], file_mode[i], rp->debug_dir, rp, fops[i]);
		if (IS_ERR_OR_NULL(*file_entry[i])) {
			dev_err(rp->dev, "create debug file `%s` failed %d.\n",
				file_name[i], PTR_ERR(*file_entry[i]));
		}
	}

	ret = ispv4_ramlog_init(rp);
	if (ret != 0) {
		dev_err(rp->dev, "ispv4_ramlog_init failed %d.\n", ret);
	}
}

static void ispv4_rproc_rm_debugfs(struct xm_ispv4_rproc *rp)
{
	ispv4_ramlog_deinit(rp);
	if (!IS_ERR_OR_NULL(rp->debug_dir))
		debugfs_remove(rp->debug_dir);
}

static void __dump_inner(struct xm_ispv4_rproc *rp, void *buf, uint32_t da,
			 uint32_t size, const char *name)
{
	uint32_t *addr;
	bool is_iomem;

	if (size != 0) {
		addr = rproc_da_to_va(rp->rproc, da, size, &is_iomem);
		if (buf != NULL) {
			memcpy_fromio(buf, addr, size);
			dev_crit(rp->dev, "Dump restore %s\n", name);
		} else
			dev_err(rp->dev, "No buffer to dump %s\n", name);
	}
	(void)is_iomem;
}

static void ispv4_dump_ramlog(struct xm_ispv4_rproc *rp)
{
	if (RP_SPI(rp))
		return;

	__dump_inner(rp, rp->ramlog_buf, rp->ramlog_da, rp->ramlog_buf_size,
		     "lastlog");
	rp->ramlog_dumped = true;
#ifdef USE_USERHELPER
	ispv4_user_dealfile(UD_SAVE_RAMLOG);
#endif
}

static void ispv4_dump_status(struct xm_ispv4_rproc *rp)
{
	int vall = 0;
	dev_info(rp->dev, "ispv4 rproc dump boot status:");
	ispv4_regops_read(0x21BE00, &vall);
	dev_info(rp->dev, "0x21BE00 addr is %x\n", vall);
}

static void ispv4_dump_inner128(void)
{
	int i = 0;
	const int addr = 0xC;
	pr_cont("ispv4 rproc dump inner128 reg: ");
	for (i = 0; i < 4; i++) {
		int iaddr = addr + i * 4;
		int val;
		ispv4_regops_inner_read(iaddr, &val);
		pr_cont("0x%x=0x%x ", iaddr, val);
	}
	pr_cont("\n");
}

static void ispv4_dump_debuginfo(struct xm_ispv4_rproc *rp)
{
	if (RP_SPI(rp))
		return;

	__dump_inner(rp, rp->debuginfo_buf, rp->debuginfo_da,
		     rp->debuginfo_buf_size, "lockinfo");
	rp->debuginfo_dumped = true;
#ifdef USE_USERHELPER
	ispv4_user_dealfile(UD_SAVE_DEBUGINFO);
#endif
}

static void ispv4_dump_bootinfo(struct xm_ispv4_rproc *rp)
{
	uint32_t intf;

	if (RP_SPI(rp)) {
		ispv4_regops_read(rp->bootinfo_da, &intf);
		dev_err(rp->dev, "boot info = 0x%x\n", intf);
		return;
	}

	__dump_inner(rp, rp->bootinfo_buf, rp->bootinfo_da,
		     rp->bootinfo_buf_size, "bootinfo");
	rp->bootinfo_dumped = true;
#ifdef USE_USERHELPER
	ispv4_user_dealfile(UD_SAVE_BOOTINFO);
#endif
}

static void ispv4_crash_handle(struct work_struct *work)
{
	struct xm_ispv4_rproc *rp =
		container_of(work, struct xm_ispv4_rproc, crash);

	/* stop all operation to v4 */
	rp->met_crash = true;
	smp_wmb();

	ispv4_dump_inner128();
	ispv4_dump_ramlog(rp);
	ispv4_dump_debuginfo(rp);
	ispv4_dump_bootinfo(rp);

	if (rp->crash_notify)
		rp->crash_notify(rp->crash_notify_thiz, rp->crash_type);

	/* HAL will do this */
	// ispv4_rproc_down(rp);
}

irqreturn_t ispv4_crash_irq(int irq, void *p)
{
	struct xm_ispv4_rproc *rp = p;
	schedule_work(&rp->crash);
	disable_irq_nosync(irq);
	return IRQ_HANDLED;
}

static bool ispv4_ddr_buf_avalid(struct xm_ispv4_rproc *rp)
{
	return rp->ddr_buf_avalid;
}

static bool ispv4_ddr_buf_update(struct xm_ispv4_rproc *rp)
{
	return rp->ddr_buf_update;
}

static int ispv4_comp_bind(struct device *comp, struct device *master,
			   void *master_data)
{
	struct ispv4_v4l2_dev *priv = master_data;
	priv->v4l2_rproc.boot = ispv4_rproc_boot;
	priv->v4l2_rproc.shutdown = ispv4_rproc_down;
	priv->v4l2_rproc.register_crash_cb = register_crash_notify;
	priv->v4l2_rproc.register_ddrfb_cb = register_ddrfb_notify;
	priv->v4l2_rproc.register_ipc_cb = register_ipc_notify;
	priv->v4l2_rproc.register_exception_cb = register_exception_cb;
	priv->v4l2_rproc.register_rpm_ready_cb = register_rpmsgready_notify;
	priv->v4l2_rproc.dump_ramlog = ispv4_dump_ramlog;
	priv->v4l2_rproc.dump_debuginfo = ispv4_dump_debuginfo;
	priv->v4l2_rproc.dump_bootinfo = ispv4_dump_bootinfo;
	priv->v4l2_rproc.ddr_kernel_data_avalid = ispv4_ddr_buf_avalid;
	priv->v4l2_rproc.ddr_kernel_data_update = ispv4_ddr_buf_update;
	priv->v4l2_rproc.get_boot_status = ispv4_get_boot_status;
	priv->v4l2_rproc.rp =
		container_of(comp, struct xm_ispv4_rproc, comp_dev);
	priv->v4l2_rproc.avalid = true;

	dev_info(comp, "avalid!!\n");
	return 0;
}

static void ispv4_comp_unbind(struct device *comp, struct device *master,
			      void *master_data)
{
	struct ispv4_v4l2_dev *priv = master_data;
	priv->v4l2_rproc.avalid = false;
}

__maybe_unused static const struct component_ops comp_ops = {
	.bind = ispv4_comp_bind,
	.unbind = ispv4_comp_unbind
};

static ssize_t ramlog_sysfs_read(struct file *f, struct kobject *obj,
				 struct bin_attribute *attr, char *buf,
				 loff_t off, size_t count)
{
	struct xm_ispv4_rproc *rp;
	rp = container_of(attr, struct xm_ispv4_rproc, ramlog_attr);
	memcpy(buf, rp->ramlog_buf + off, count);
	return count;
}

static ssize_t debuginfo_sysfs_read(struct file *f, struct kobject *obj,
				    struct bin_attribute *attr, char *buf,
				    loff_t off, size_t count)
{
	struct xm_ispv4_rproc *rp;
	rp = container_of(attr, struct xm_ispv4_rproc, debuginfo_attr);
	memcpy(buf, rp->debuginfo_buf + off, count);
	return count;
}

static ssize_t bootinfo_sysfs_read(struct file *f, struct kobject *obj,
				   struct bin_attribute *attr, char *buf,
				   loff_t off, size_t count)
{
	struct xm_ispv4_rproc *rp;
	rp = container_of(attr, struct xm_ispv4_rproc, bootinfo_attr);
	memcpy(buf, rp->bootinfo_buf + off, count);
	return count;
}

static ssize_t ddrtf_sysfs_write(struct file *f, struct kobject *obj,
				 struct bin_attribute *attr, char *buf,
				 loff_t off, size_t count)
{
	struct xm_ispv4_rproc *rp;
	rp = container_of(attr, struct xm_ispv4_rproc, ddrt_attr);
	if (rp->ddrtf_buf != NULL) {
		memcpy((char *)rp->ddrtf_buf + off, buf, count);
		rp->ddr_buf_avalid = true;
	}
	return count;
}

static ssize_t ddrtf_sysfs_read(struct file *f, struct kobject *obj,
				struct bin_attribute *attr, char *buf,
				loff_t off, size_t count)
{
	struct xm_ispv4_rproc *rp;
	rp = container_of(attr, struct xm_ispv4_rproc, ddrt_attr);
	if (rp->ddrtf_buf != NULL)
		memcpy(buf, (char *)rp->ddrtf_buf + off, count);
	return count;
}

static int ispv4_prepare_firmware(struct xm_ispv4_rproc *xm_rp)
{
	int rc;

	rc = ispv4_request_firmware(xm_rp);
	if (rc != 0) {
		dev_err(xm_rp->dev, "ispv4 firmware request error.\n");
		goto firmware_get_failed;
	}

	if (xm_rp->ramlog_buf_size > 0 && xm_rp->ramlog_buf_size < 1024 * 1024) {
		xm_rp->ramlog_buf = vmalloc(xm_rp->ramlog_buf_size);
		if (xm_rp->ramlog_buf == NULL) {
			rc = -ENOMEM;
			dev_err(xm_rp->dev,
				"ispv4 rproc alloc rambuf failed.\n");
			goto alloc_ramlog_buf_failed;
		}
	}

	if (xm_rp->debuginfo_buf_size > 0 && xm_rp->debuginfo_buf_size < 1024 * 1024) {
		xm_rp->debuginfo_buf = vmalloc(xm_rp->debuginfo_buf_size);
		if (xm_rp->debuginfo_buf == NULL) {
			rc = -ENOMEM;
			dev_err(xm_rp->dev,
				"ispv4 rproc alloc debuginfo failed.\n");
			goto alloc_debuginfo_buf_failed;
		}
	}

	if (xm_rp->bootinfo_buf_size > 0 && xm_rp->bootinfo_buf_size < 1024 * 1024) {
		xm_rp->bootinfo_buf = vmalloc(xm_rp->bootinfo_buf_size);
		if (xm_rp->bootinfo_buf == NULL) {
			rc = -ENOMEM;
			dev_err(xm_rp->dev,
				"ispv4 rproc alloc bootinfo failed.\n");
			goto alloc_bootinfo_buf_failed;
		}
	}

	sysfs_bin_attr_init(&xm_rp->ramlog_attr);
	sysfs_bin_attr_init(&xm_rp->bootinfo_attr);
	sysfs_bin_attr_init(&xm_rp->debuginfo_attr);
	xm_rp->ramlog_attr.attr.name = "ramlog";
	xm_rp->ramlog_attr.attr.mode = 0444;
	xm_rp->ramlog_attr.read = ramlog_sysfs_read;
	xm_rp->ramlog_attr.size = xm_rp->ramlog_buf_size;
	xm_rp->bootinfo_attr.attr.name = "bootinfo";
	xm_rp->bootinfo_attr.attr.mode = 0444;
	xm_rp->bootinfo_attr.read = bootinfo_sysfs_read;
	xm_rp->bootinfo_attr.size = xm_rp->bootinfo_buf_size;
	xm_rp->debuginfo_attr.attr.name = "debuginfo";
	xm_rp->debuginfo_attr.attr.mode = 0444;
	xm_rp->debuginfo_attr.read = debuginfo_sysfs_read;
	xm_rp->debuginfo_attr.size = xm_rp->debuginfo_buf_size;

	if (xm_rp->ramlog_buf_size != 0) {
		rc = device_create_bin_file(xm_rp->dev, &xm_rp->ramlog_attr);
		if (rc != 0)
			goto sysfs_ramlog_failed;
	}

	if (xm_rp->bootinfo_buf_size != 0) {
		rc = device_create_bin_file(xm_rp->dev, &xm_rp->bootinfo_attr);
		if (rc != 0)
			goto sysfs_bootinfo_failed;
	}

	if (xm_rp->debuginfo_buf_size != 0) {
		rc = device_create_bin_file(xm_rp->dev, &xm_rp->debuginfo_attr);
		if (rc != 0)
			goto sysfs_debuginfo_failed;
	}

#ifdef USE_USERHELPER
	ispv4_user_dealfile(UD_LOAD_DDRTF);
#endif

	xm_rp->prepare_fw_finish = true;
	return 0;

sysfs_debuginfo_failed:
	if (xm_rp->bootinfo_buf_size != 0)
		device_remove_bin_file(xm_rp->dev, &xm_rp->bootinfo_attr);
sysfs_bootinfo_failed:
	if (xm_rp->ramlog_buf_size != 0)
		device_remove_bin_file(xm_rp->dev, &xm_rp->ramlog_attr);
sysfs_ramlog_failed:
	if (xm_rp->bootinfo_buf_size != 0)
		vfree(xm_rp->bootinfo_buf);
alloc_bootinfo_buf_failed:
	if (xm_rp->debuginfo_buf_size != 0)
		vfree(xm_rp->debuginfo_buf);
alloc_debuginfo_buf_failed:
	if (xm_rp->ramlog_buf_size != 0)
		vfree(xm_rp->ramlog_buf);
alloc_ramlog_buf_failed:
	release_firmware(xm_rp->origin_fw);
firmware_get_failed:
	return rc;
}

static int ispv4_rproc_probe(struct platform_device *pdev)
{
	int rc = 0, i;
	struct device_node *dt_node;
	struct xm_ispv4_rproc *xm_rp;
	struct rproc *rproc;
	struct xm_ispv4_ops *ops;
	struct ispv4_data *idata, **idatap;
	struct ispv4_rpmsg_mbox_s {
		u32 data[4];
	};

	dev_info(&pdev->dev, "%s entry", __FUNCTION__);

	ops = (void *)pdev->id_entry->driver_data;
	if (ops == NULL) {
		dev_err(&pdev->dev, "ispv4 probe error, no ops.\n");
		return -ENOMEM;
	}

	rproc = rproc_alloc(&pdev->dev, dev_name(&pdev->dev), ops->rproc_ops,
			    NULL, sizeof(*xm_rp));
	// kfree(rproc->firmware);

	if (rproc == NULL) {
		dev_err(&pdev->dev, "ispv4 alloc rproc error.\n");
		rc = -ENOMEM;
		goto alloc_failed;
	}

	rproc->auto_boot = false;
	xm_rp = rproc->priv;
	xm_rp->rproc = rproc;
	xm_rp->dev = &pdev->dev;
	xm_rp->magic_num = 0x1234abcd;
	xm_rp->ops = ops;
	xm_rp->prepare_fw_finish = false;

	for (i = 0; i < XM_ISPV4_IPC_EPT_MAX; i++)
		mutex_init(&xm_rp->rpeptdev_lock[i]);

	if (ops == &ispv4_pci_ops) {
		idatap = dev_get_platdata(&pdev->dev);
		idata = *idatap;
		xm_rp->pci_dev = &idata->pci->dev;
		xm_rp->pdma = idata->pdma;
	}

	init_completion(&xm_rp->cpl_boot);
	platform_set_drvdata(pdev, xm_rp);

	dt_node = of_find_node_by_name(NULL, "xm_ispv4_rproc");
	if (dt_node == NULL) {
		dev_err(&pdev->dev, "Can not find ispv4-mcu device tree node");
		rc = -ENODEV;
		goto find_node_failed;
	}
	pdev->dev.of_node = dt_node;

	xm_rp->mbox_boot.dev = &pdev->dev;
	xm_rp->mbox_boot.tx_block = true;
	xm_rp->mbox_boot.tx_tout = 1000;
	xm_rp->mbox_boot.knows_txdone = false;
	xm_rp->mbox_boot.rx_callback = boot_mbox_callback;

	rc = ispv4_rproc_ops_init(pdev);
	if (rc < 0) {
		dev_err(&pdev->dev, "rproc init failed\n");
		goto ops_init_failed;
	}

	INIT_WORK(&xm_rp->crash, ispv4_crash_handle);

	ispv4_rproc_debugfs(xm_rp);

	rc = rproc_add(rproc);
	if (rc != 0) {
		dev_err(&pdev->dev, "ispv4 rproc add error.\n");
		goto rproc_add_failed;
	}

	xm_rp->ddrtf_buf =
		dma_alloc_coherent(xm_rp->pci_dev, XM_ISPV4_DDRTF_SIZE,
				   &xm_rp->ddrtf_iova, GFP_KERNEL);
	if (xm_rp->ddrtf_buf == NULL) {
		dev_err(&pdev->dev,
			"ispv4 dma alloc for ddr traning file failed.\n");
	}

	sysfs_bin_attr_init(&xm_rp->ddrt_attr);
	xm_rp->ddrt_attr.attr.name = "ddrfw";
	xm_rp->ddrt_attr.attr.mode = 0666;
	xm_rp->ddrt_attr.write = ddrtf_sysfs_write;
	xm_rp->ddrt_attr.read = ddrtf_sysfs_read;
	xm_rp->ddrt_attr.size = XM_ISPV4_DDRTF_SIZE;
	rc = device_create_bin_file(xm_rp->dev, &xm_rp->ddrt_attr);
	if (rc != 0)
		goto ddr_sysfs_failed;

	device_initialize(&xm_rp->comp_dev);
	dev_set_name(&xm_rp->comp_dev, "xm-ispv4-rproc");
	pr_err("comp add %s! priv = %x, comp_name = %s\n", __FUNCTION__, xm_rp,
	       dev_name(&xm_rp->comp_dev));
	rc = component_add(&xm_rp->comp_dev, &comp_ops);
	if (rc != 0) {
		dev_err(&pdev->dev, "ispv4 rproc component add error.\n");
		goto comp_add_failed;
	}

	return 0;

comp_add_failed:
	device_remove_bin_file(xm_rp->dev, &xm_rp->ddrt_attr);
ddr_sysfs_failed:
	if (xm_rp->ddrtf_buf != NULL)
		dma_free_coherent(xm_rp->pci_dev, XM_ISPV4_DDRTF_SIZE,
				  xm_rp->ddrtf_buf, xm_rp->ddrtf_iova);
	rproc_del(rproc);
rproc_add_failed:
	ispv4_rproc_rm_debugfs(xm_rp);
	ispv4_rproc_ops_deinit(pdev);
ops_init_failed:
	of_node_put(dt_node);
find_node_failed:
	rproc_free(rproc);
alloc_failed:
	return rc;
}

static int ispv4_rproc_remove(struct platform_device *pdev)
{
	struct rproc *rproc;
	struct xm_ispv4_rproc *xm_rp;

	dev_info(&pdev->dev, "%s entry", __FUNCTION__);
	xm_rp = platform_get_drvdata(pdev);
	rproc = xm_rp->rproc;

	component_del(&xm_rp->comp_dev, &comp_ops);
	device_remove_bin_file(xm_rp->dev, &xm_rp->ddrt_attr);

	cancel_work_sync(&xm_rp->crash);
	ispv4_rproc_rm_debugfs(xm_rp);

	ispv4_rproc_down(xm_rp);
	ispv4_rproc_ops_deinit(pdev);

	if (xm_rp->prepare_fw_finish) {
		if (xm_rp->ramlog_buf != NULL)
			vfree(xm_rp->ramlog_buf);
		if (xm_rp->bootinfo_buf != NULL)
			vfree(xm_rp->bootinfo_buf);
		if (xm_rp->debuginfo_buf != NULL)
			vfree(xm_rp->debuginfo_buf);
		if (xm_rp->debuginfo_buf_size != 0)
			device_remove_bin_file(xm_rp->dev, &xm_rp->debuginfo_attr);
		if (xm_rp->bootinfo_buf_size != 0)
			device_remove_bin_file(xm_rp->dev, &xm_rp->bootinfo_attr);
		if (xm_rp->ramlog_buf_size != 0)
			device_remove_bin_file(xm_rp->dev, &xm_rp->ramlog_attr);
	}

	if (xm_rp->dev->of_node) {
		of_node_put(xm_rp->dev->of_node);
		xm_rp->dev->of_node = NULL;
	}

	ispv4_rproc_ops_remove(pdev);
	if (xm_rp->origin_fw != NULL)
		release_firmware(xm_rp->origin_fw);

	if (xm_rp->ddrtf_buf != NULL)
		dma_free_coherent(xm_rp->pci_dev, XM_ISPV4_DDRTF_SIZE,
				  xm_rp->ddrtf_buf, xm_rp->ddrtf_iova);

	rproc_del(rproc);
	rproc_free(rproc);

	return 0;
}

const struct platform_device_id ispv4_rproc_match[] = {
	{
		.name = "xm-ispv4-rproc-pci",
		.driver_data = (kernel_ulong_t)&ispv4_pci_ops,
	},
	{
		.name = "xm-ispv4-rproc-spi",
		.driver_data = (kernel_ulong_t)&ispv4_spi_ops,
	},
	{
		.name = "xm-ispv4-fake",
		.driver_data = (kernel_ulong_t)&ispv4_fake_ops,
	},
	{}
};
MODULE_DEVICE_TABLE(platform, ispv4_rproc_match);

static struct platform_driver xm_ispv4_rproc_driver = {
	.driver = {
		.name = "xiaomi-ispv4-rproc",
		.owner = THIS_MODULE,
	},
	.probe = ispv4_rproc_probe,
	.remove = ispv4_rproc_remove,
	.shutdown = ispv4_rproc_ops_shutdown,
	.id_table = ispv4_rproc_match,
};

module_param(p_load_by_dma, bool, 0664);
module_platform_driver(xm_ispv4_rproc_driver);

MODULE_AUTHOR("ChenHonglin<chenhonglin@xiaomi.com>");
MODULE_DESCRIPTION("Xiaomi ISPV4.");
MODULE_LICENSE("GPL v2");

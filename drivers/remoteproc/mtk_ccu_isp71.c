// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.

#include <linux/arm-smccc.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/uaccess.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/iommu.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/fdtable.h>
#include <linux/interrupt.h>
#include <linux/remoteproc/mtk_ccu.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

#include "mtk_ccu_isp71.h"
#include "mtk_ccu_common.h"
#include "mtk-interconnect.h"
#include "remoteproc_internal.h"

#define MTK_CCU_TAG "[ccu_rproc]"
#define LOG_DBG(format, args...) \
	pr_info(MTK_CCU_TAG "[%s] " format, __func__, ##args)

static int mtk_ccu_probe(struct platform_device *dev);
static int mtk_ccu_remove(struct platform_device *dev);
static int mtk_ccu_read_platform_info_from_dt(struct device_node
	*node, uint32_t *ccu_hw_base, uint32_t *ccu_hw_size);
#if !defined(FPGA_UT)
static void mtk_ccu_get_power(struct device *dev);
static void mtk_ccu_put_power(struct device *dev);
#endif

static int
mtk_ccu_allocate_mem(struct device *dev, struct mtk_ccu_mem_handle *memHandle)
{
	if (memHandle->meminfo.size <= 0)
		return 0;

	/* get buffer virtual address */
	memHandle->meminfo.va = dma_alloc_attrs(dev, memHandle->meminfo.size,
		&memHandle->meminfo.mva, GFP_KERNEL, DMA_ATTR_WRITE_COMBINE);

	if (memHandle->meminfo.va == NULL) {
		dev_err(dev, "fail to get buffer kernel virtual address");
		return -EINVAL;
	}

	dev_info(dev, "success: size(%x), va(%lx), mva(%lx)\n",
	memHandle->meminfo.size, memHandle->meminfo.va, memHandle->meminfo.mva);

	return 0;
}

static int
mtk_ccu_deallocate_mem(struct device *dev, struct mtk_ccu_mem_handle *memHandle)
{
	dma_free_attrs(dev, memHandle->meminfo.size, memHandle->meminfo.va,
		memHandle->meminfo.mva, DMA_ATTR_WRITE_COMBINE);
	memset(memHandle, 0, sizeof(struct mtk_ccu_mem_handle));

	return 0;
}

static void mtk_ccu_set_log_memory_address(struct mtk_ccu *ccu)
{
	struct mtk_ccu_mem_info *meminfo;
	int offset;

	if (ccu->ext_buf.meminfo.va != 0) {
		meminfo = &ccu->ext_buf.meminfo;
		offset = 0;
	} else {
		dev_info(ccu->dev, "no log buf setting\n");
		return;
	}

	ccu->log_info[0].fd = meminfo->fd;
	ccu->log_info[0].size = MTK_CCU_DRAM_LOG_BUF_SIZE;
	ccu->log_info[0].offset = offset;
	ccu->log_info[0].mva = meminfo->mva + offset;
	ccu->log_info[0].va = meminfo->va + offset;

	ccu->log_info[1].fd = meminfo->fd;
	ccu->log_info[1].size = MTK_CCU_DRAM_LOG_BUF_SIZE;
	ccu->log_info[1].offset = offset + MTK_CCU_DRAM_LOG_BUF_SIZE;
	ccu->log_info[1].mva = ccu->log_info[0].mva + MTK_CCU_DRAM_LOG_BUF_SIZE;
	ccu->log_info[1].va = ccu->log_info[0].va + MTK_CCU_DRAM_LOG_BUF_SIZE;

	ccu->log_info[2].fd = meminfo->fd;
	ccu->log_info[2].size = MTK_CCU_DRAM_LOG_BUF_SIZE * 2;
	ccu->log_info[2].offset = offset;
	ccu->log_info[2].mva = ccu->log_info[0].mva;
	ccu->log_info[2].va = ccu->log_info[0].va;
}

static void mtk_ccu_clear_log_memory_address(struct mtk_ccu *ccu)
{

	memset(&ccu->log_info[0], 0, sizeof(struct mtk_ccu_buffer));
	memset(&ccu->log_info[1], 0, sizeof(struct mtk_ccu_buffer));
	memset(&ccu->log_info[2], 0, sizeof(struct mtk_ccu_buffer));
}


int mtk_ccu_sw_hw_reset(struct mtk_ccu *ccu)
{
	uint32_t duration = 0;
	uint32_t ccu_status;
	uint8_t *ccu_base = (uint8_t *)ccu->ccu_base;
	/* check halt is up */
	ccu_status = readl(ccu_base + MTK_CCU_MON_ST);
	LOG_DBG("polling CCU halt(0x%08x)\n", ccu_status);
	duration = 0;
	while ((ccu_status & 0x30) != 0x30) {
		duration++;
		if (duration > 1000) {
			dev_err(ccu->dev,
			"polling CCU halt, timeout: (0x%08x)\n", ccu_status);
			break;
		}
		udelay(10);
		ccu_status = readl(ccu_base + MTK_CCU_MON_ST);
	}
	LOG_DBG("polling CCU halt done(0x%08x)\n", ccu_status);

	return true;
}

struct platform_device *mtk_ccu_get_pdev(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *ccu_node;
	struct platform_device *ccu_pdev;

	ccu_node = of_parse_phandle(dev->of_node, "mediatek,ccu_rproc", 0);
	if (!ccu_node) {
		dev_err(dev, "failed to get ccu node\n");
		return NULL;
	}

	ccu_pdev = of_find_device_by_node(ccu_node);
	if (WARN_ON(!ccu_pdev)) {
		dev_err(dev, "failed to get ccu pdev\n");
		of_node_put(ccu_node);
		return NULL;
	}

	return ccu_pdev;
}
EXPORT_SYMBOL_GPL(mtk_ccu_get_pdev);

static int mtk_ccu_run(struct mtk_ccu *ccu)
{
	int32_t timeout = 100;
	uint8_t	*ccu_base = (uint8_t *)ccu->ccu_base;
#if defined(SECURED_CCU)
	struct arm_smccc_res res;
#else
	struct mtk_ccu_mem_info *bin_mem =
		mtk_ccu_get_meminfo(ccu, MTK_CCU_DDR);
	dma_addr_t remapOffset;

	if (!bin_mem) {
		dev_err(ccu->dev, "get binary memory info failed\n");
		return -EINVAL;
	}
#endif

	LOG_DBG("+\n");

	/*1. Set CCU remap offset & log level*/
#if !defined(SECURED_CCU)
	remapOffset = bin_mem->mva - MTK_CCU_CACHE_BASE;
	writel(remapOffset >> 4, ccu_base + MTK_CCU_REG_AXI_REMAP);
#endif
	writel(ccu->log_level, ccu_base + MTK_CCU_SPARE_REG06);
	writel(ccu->log_taglevel, ccu_base + MTK_CCU_SPARE_REG07);

#if defined(SECURED_CCU)
#ifdef CONFIG_ARM64
	arm_smccc_smc(MTK_SIP_KERNEL_CCU_CONTROL, (u64) CCU_SMC_REQ_RUN, 0, 0, 0, 0, 0, 0, &res);
#endif
#ifdef CONFIG_ARM_PSCI
	arm_smccc_smc(MTK_SIP_KERNEL_CCU_CONTROL, (u32) CCU_SMC_REQ_RUN, 0, 0, 0, 0, 0, 0, &res);
#endif
#else  /* !defined(SECURED_CCU) */
	/*2. Set CCU_RESET. CCU_HW_RST=0*/
	writel(0x0, ccu_base + MTK_CCU_REG_RESET);
#endif

	/*3. Pulling CCU init done spare register*/
	while ((readl(ccu_base + MTK_CCU_SPARE_REG10)
		!= CCU_STATUS_INIT_DONE) && (timeout >= 0)) {
		usleep_range(50, 100);
		timeout = timeout - 1;
	}
	if (timeout <= 0) {
		dev_err(ccu->dev, "CCU init timeout\n");
		dev_err(ccu->dev, "ccu initial debug info: %x\n",
			readl(ccu_base + MTK_CCU_SPARE_REG10));
		return -ETIMEDOUT;
	}

	/*5. Get mailbox address in CCU's sram */
	ccu->mb = (struct mtk_ccu_mailbox *)(uintptr_t)(ccu->dmem_base +
		readl(ccu_base + MTK_CCU_SPARE_REG00));
	LOG_DBG("ccu initial debug mb_ap2ccu: %x\n",
		readl(ccu_base + MTK_CCU_SPARE_REG00));

	mtk_ccu_rproc_ipc_init(ccu);

	mtk_ccu_ipc_register(ccu->pdev, MTK_CCU_MSG_TO_APMCU_FLUSH_LOG,
		mtk_ccu_ipc_log_handle, ccu);
	mtk_ccu_ipc_register(ccu->pdev, MTK_CCU_MSG_TO_APMCU_CCU_ASSERT,
		mtk_ccu_ipc_assert_handle, ccu);
	mtk_ccu_ipc_register(ccu->pdev, MTK_CCU_MSG_TO_APMCU_CCU_WARNING,
		mtk_ccu_ipc_warning_handle, ccu);

	/*tell ccu that driver has initialized mailbox*/
	writel(0, ccu_base + MTK_CCU_SPARE_REG10);

	timeout = 10;
	while ((readl(ccu_base + MTK_CCU_SPARE_REG10)
		!= CCU_STATUS_INIT_DONE_2) && (timeout >= 0)) {
		udelay(100);
		timeout = timeout - 1;
	}

	if (timeout <= 0) {
		dev_err(ccu->dev, "CCU init timeout 2\n");
		dev_err(ccu->dev, "ccu initial debug info: %x\n",
			readl(ccu_base + MTK_CCU_SPARE_REG10));
		return -ETIMEDOUT;
	}

	LOG_DBG("-\n");

	return 0;
}

#if defined(FPGA_UT)
static int mtk_ccu_clk_prepare(struct mtk_ccu *ccu)
{
	writel(0xFFFFFFFF, ((uint8_t *)(ccu->camsys_base))+0x08);
	writel(0xFFFFFFFF, ((uint8_t *)(ccu->ccu_main_base))+0x08);
	return 0;
}
#else
static int mtk_ccu_clk_prepare(struct mtk_ccu *ccu)
{
	int ret;
	int i = 0;
	struct device *dev = ccu->dev;

	mtk_ccu_get_power(dev);
	mtk_ccu_get_power(&ccu->pdev1->dev);

	for (i = 0; i < MTK_CCU_CLK_PWR_NUM; ++i) {
		ret = clk_prepare_enable(ccu->ccu_clk_pwr_ctrl[i]);
		if (ret) {
			dev_err(dev, "failed to enable clocks\n");
			goto ERROR;
		}
	}


	return 0;

ERROR:
	for (; i >= 0 ; i--)
		clk_disable_unprepare(ccu->ccu_clk_pwr_ctrl[i]);

	mtk_ccu_put_power(dev);
	mtk_ccu_put_power(&ccu->pdev1->dev);

	return ret;

}
#endif

#if defined(FPGA_UT)
static void mtk_ccu_clk_unprepare(struct mtk_ccu *ccu)
{
}
#else
static void mtk_ccu_clk_unprepare(struct mtk_ccu *ccu)
{
	int i;

	for (i = 0; i < MTK_CCU_CLK_PWR_NUM; ++i)
		clk_disable_unprepare(ccu->ccu_clk_pwr_ctrl[i]);
	mtk_ccu_put_power(ccu->dev);
	mtk_ccu_put_power(&ccu->pdev1->dev);

}
#endif

static int mtk_ccu_start(struct rproc *rproc)
{
	struct mtk_ccu *ccu = (struct mtk_ccu *)rproc->priv;
	uint8_t *ccu_base = (uint8_t *)ccu->ccu_base;

	/*1. Set CCU log memory address from user space*/
	mtk_ccu_set_log_memory_address(ccu);
	writel((uint32_t)((ccu->log_info[0].mva) >> 32), ccu_base + MTK_CCU_SPARE_REG02);
	writel((uint32_t)ccu->log_info[0].mva, ccu_base + MTK_CCU_SPARE_REG03);
	writel((uint32_t)((ccu->log_info[1].mva) >> 32), ccu_base + MTK_CCU_SPARE_REG04);
	writel((uint32_t)ccu->log_info[1].mva, ccu_base + MTK_CCU_SPARE_REG05);

#if !defined(FPGA_UT)
	mtk_icc_set_bw(ccu->path_ccuo, MBps_to_icc(20), MBps_to_icc(20));
	mtk_icc_set_bw(ccu->path_ccui, MBps_to_icc(10), MBps_to_icc(10));
#endif

	LOG_DBG("LogBuf_mva[0](0x%lx)(0x%x_%x)\n",
		ccu->log_info[0].mva,
		readl(ccu_base + MTK_CCU_SPARE_REG02),
		readl(ccu_base + MTK_CCU_SPARE_REG03)
		);
	LOG_DBG("LogBuf_mva[1](0x%lx)(0x%x_%x)\n",
		ccu->log_info[1].mva,
		readl(ccu_base + MTK_CCU_SPARE_REG04),
		readl(ccu_base + MTK_CCU_SPARE_REG05)
		);
	ccu->g_LogBufIdx = -1;

	/*1. Set CCU run*/
	mtk_ccu_run(ccu);
	return 0;
}

void *mtk_ccu_da_to_va(struct rproc *rproc, u64 da, size_t len, bool *is_iomem)
{
	struct mtk_ccu *ccu = (struct mtk_ccu *)rproc->priv;
	struct device *dev = ccu->dev;
	int offset;
	struct mtk_ccu_mem_info *bin_mem = mtk_ccu_get_meminfo(ccu, MTK_CCU_DDR);

	if (da < MTK_CCU_CORE_DMEM_BASE) {
		offset = da;
		if (offset >= 0 && (offset + len) < MTK_CCU_PMEM_SIZE)
			return ccu->pmem_base + offset;
	} else if (da < MTK_CCU_CACHE_BASE) {
		offset = da - MTK_CCU_CORE_DMEM_BASE;
		if (offset >= 0 && (offset + len) < MTK_CCU_DMEM_SIZE)
			return ccu->dmem_base + offset;
	} else {
		offset = da - MTK_CCU_CACHE_BASE;
		if (!bin_mem) {
			dev_err(dev, "get binary memory info failed\n");
			return NULL;
		}
		if (offset >= 0 && (offset + len) < MTK_CCU_CACHE_SIZE)
			return bin_mem->va + offset;
	}

	dev_err(dev, "failed lookup da(0x%x) len(0x%x) to va, offset(%x)\n",
		da, len, offset);
	return NULL;
}
EXPORT_SYMBOL_GPL(mtk_ccu_da_to_va);

static int mtk_ccu_stop(struct rproc *rproc)
{
	struct mtk_ccu *ccu = (struct mtk_ccu *)rproc->priv;
	/* struct device *dev = &rproc->dev; */
	int ccu_reset;
	int ret;
	uint8_t *ccu_base = (uint8_t *)ccu->ccu_base;

	/* notify CCU to shutdown*/
	ret = mtk_ccu_rproc_ipc_send(ccu->pdev, MTK_CCU_FEATURE_SYSCTRL,
		3, NULL, 0);

	mtk_ccu_sw_hw_reset(ccu);

#if !defined(SECURED_CCU)
	ret = mtk_ccu_deallocate_mem(ccu->dev,
		&ccu->buffer_handle[MTK_CCU_DDR]);
#endif

	ccu_reset = readl(ccu_base + MTK_CCU_REG_RESET);
	writel(ccu_reset|MTK_CCU_HW_RESET_BIT, ccu_base + MTK_CCU_REG_RESET);

#if !defined(FPGA_UT)
	mtk_icc_set_bw(ccu->path_ccuo, MBps_to_icc(0), MBps_to_icc(0));
	mtk_icc_set_bw(ccu->path_ccui, MBps_to_icc(0), MBps_to_icc(0));
#endif

	mtk_ccu_clear_log_memory_address(ccu);
	mtk_ccu_clk_unprepare(ccu);
	return 0;
}

#if !defined(SECURED_CCU)
static int
ccu_elf_load_segments(struct rproc *rproc, const struct firmware *fw)
{
	struct device *dev = &rproc->dev;
	struct mtk_ccu *ccu = rproc->priv;
	struct elf32_hdr *ehdr;
	struct elf32_phdr *phdr;
	int i, ret = 0;
	int timeout = 10;
	unsigned int status;
	const u8 *elf_data = fw->data;
	uint8_t *ccu_base = (uint8_t *)ccu->ccu_base;
	bool is_iomem;

	/* 1. Halt CCU HW before load binary */
	writel(MTK_CCU_HW_RESET_BIT, ccu_base + MTK_CCU_REG_RESET);
	udelay(10);

	/* 2. Polling CCU HW status until ready */
	status = readl(ccu_base + MTK_CCU_REG_RESET);
	while ((status & 0x3) != 0x3) {
		status = readl(ccu_base + MTK_CCU_REG_RESET);
		udelay(300);
		if (timeout < 0 && ((status & 0x3) != 0x3)) {
			dev_err(dev, "wait ccu halt before load bin, timeout");
			return -EFAULT;
		}
		timeout--;
	}

	ehdr = (struct elf32_hdr *)elf_data;
	phdr = (struct elf32_phdr *)(elf_data + ehdr->e_phoff);
	/* 3. go through the available ELF segments */
	for (i = 0; i < ehdr->e_phnum; i++, phdr++) {
		u32 da = phdr->p_paddr;
		u32 memsz = phdr->p_memsz;
		u32 filesz = phdr->p_filesz;
		u32 offset = phdr->p_offset;
		void *ptr;

		if (phdr->p_type != PT_LOAD)
			continue;

		LOG_DBG("phdr: type %d da 0x%x memsz 0x%x filesz 0x%x\n",
			phdr->p_type, da, memsz, filesz);

		if (filesz > memsz) {
			dev_err(dev, "bad phdr filesz 0x%x memsz 0x%x\n",
				filesz, memsz);
			ret = -EINVAL;
			break;
		}

		if (offset + filesz > fw->size) {
			dev_err(dev, "truncated fw: need 0x%x avail 0x%zx\n",
				offset + filesz, fw->size);
			ret = -EINVAL;
			break;
		}

		/* grab the kernel address for this device address */
		ptr = rproc_da_to_va(rproc, da, memsz, &is_iomem);
		if (!ptr) {
			dev_err(dev, "bad phdr da 0x%x mem 0x%x\n", da, memsz);
			ret = -EINVAL;
			break;
		}

		/* put the segment where the remote processor expects it */
		if (phdr->p_filesz)
			mtk_ccu_memcpy(ptr, elf_data + phdr->p_offset, filesz);

		/*
		 * Zero out remaining memory for this segment.
		 */
		if (memsz > filesz) {
			mtk_ccu_memclr(ptr + ((filesz + 0x3) & (~0x3)),
				memsz - filesz);
		}
	}

	return ret;
}
#endif

static int mtk_ccu_load(struct rproc *rproc, const struct firmware *fw)
{
	struct mtk_ccu *ccu = rproc->priv;
	int ret;
#if defined(SECURED_CCU)
	struct arm_smccc_res res;
#endif

	/*1. prepare CCU's clks & power*/
	ret = mtk_ccu_clk_prepare(ccu);
	if (ret) {
		dev_err(ccu->dev, "failed to prepare ccu clocks\n");
		return ret;
	}

#if defined(SECURED_CCU)
#ifdef CONFIG_ARM64
	arm_smccc_smc(MTK_SIP_KERNEL_CCU_CONTROL, (u64) CCU_SMC_REQ_LOAD,
		0, 0, 0, 0, 0, 0, &res);
#endif
#ifdef CONFIG_ARM_PSCI
	arm_smccc_smc(MTK_SIP_KERNEL_CCU_CONTROL, (u32) CCU_SMC_REQ_LOAD,
		0, 0, 0, 0, 0, 0, &res);
#endif
	if (res.a0 != 0)
		dev_err(ccu->dev, "load CCU binary failed (%d).\n", res.a0);
	else
		LOG_DBG("load CCU binary OK\n");
#else
	/*2. allocate CCU's dram memory if needed*/
	ccu->buffer_handle[MTK_CCU_DDR].meminfo.size = MTK_CCU_CACHE_SIZE;
	ccu->buffer_handle[MTK_CCU_DDR].meminfo.cached	= false;
	ret = mtk_ccu_allocate_mem(ccu->dev, &ccu->buffer_handle[MTK_CCU_DDR]);
	if (ret) {
		dev_err(ccu->dev, "alloc mem failed\n");
		return ret;
	}

	/*3. load binary*/
	ret = ccu_elf_load_segments(rproc, fw);
#endif
	return ret;
}

static int
mtk_ccu_sanity_check(struct rproc *rproc, const struct firmware *fw)
{
#if !defined(SECURED_CCU)
	struct mtk_ccu *ccu = rproc->priv;
	const char *name = rproc->firmware;
	struct elf32_hdr *ehdr;
	char class;

	if (!fw) {
		dev_err(ccu->dev, "failed to load %s\n", name);
		return -EINVAL;
	}

	if (fw->size < sizeof(struct elf32_hdr)) {
		dev_err(ccu->dev, "Image is too small\n");
		return -EINVAL;
	}

	ehdr = (struct elf32_hdr *)fw->data;

	/* We only support ELF32 at this point */
	class = ehdr->e_ident[EI_CLASS];
	if (class != ELFCLASS32) {
		dev_err(ccu->dev, "Unsupported class: %d\n", class);
		return -EINVAL;
	}

	/* We assume the firmware has the same endianness as the host */
# ifdef __LITTLE_ENDIAN
	if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
# else /* BIG ENDIAN */
	if (ehdr->e_ident[EI_DATA] != ELFDATA2MSB) {
# endif
		dev_err(ccu->dev, "Unsupported firmware endianness\n");
		return -EINVAL;
	}

	if (fw->size < ehdr->e_shoff + sizeof(struct elf32_shdr)) {
		dev_err(ccu->dev, "Image is too small\n");
		return -EINVAL;
	}

	if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG)) {
		dev_err(ccu->dev, "Image is corrupted (bad magic)\n");
		return -EINVAL;
	}

	if (ehdr->e_phnum == 0) {
		dev_err(ccu->dev, "No loadable segments\n");
		return -EINVAL;
	}

	if (ehdr->e_phoff > fw->size) {
		dev_err(ccu->dev, "Firmware size is too small\n");
		return -EINVAL;
	}
#endif

	return 0;
}

static const struct rproc_ops ccu_ops = {
	.start = mtk_ccu_start,
	.stop  = mtk_ccu_stop,
	.da_to_va = mtk_ccu_da_to_va,
	.load = mtk_ccu_load,
	.sanity_check = mtk_ccu_sanity_check,
};

static int mtk_ccu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	/* struct device_node *node1; */
	struct mtk_ccu *ccu;
	struct rproc *rproc;
	int ret = 0;
	uint32_t phy_addr;
	uint32_t phy_size;
	// phandle ccu_rproc1_phandle;

	rproc = rproc_alloc(dev, node->name, &ccu_ops,
		"lib3a.ccu_dummy", sizeof(*ccu));
	ccu = (struct mtk_ccu *)rproc->priv;
	ccu->pdev = pdev;
	ccu->dev = &pdev->dev;
	ccu->rproc = rproc;

	platform_set_drvdata(pdev, ccu);
	ret = mtk_ccu_read_platform_info_from_dt(node, &ccu->ccu_hw_base,
		&ccu->ccu_hw_size);
	if (ret) {
		dev_err(ccu->dev, "Get CLK_TOP_CCU_SEL fail.\n");
		return ret;
	}

	/*remap ccu_base*/
	phy_addr = ccu->ccu_hw_base;
	phy_size = ccu->ccu_hw_size;
	ccu->ccu_base = devm_ioremap(dev, phy_addr, phy_size);
	LOG_DBG("ccu_base pa: 0x%x, size: 0x%x\n", phy_addr, phy_size);
	LOG_DBG("ccu_base va: 0x%lx\n", ccu->ccu_base);

	/*remap dmem_base*/
	phy_addr = MTK_CCU_DMEM_BASE;
	phy_size = MTK_CCU_DMEM_SIZE;
	ccu->dmem_base = devm_ioremap(dev, phy_addr, phy_size);
	LOG_DBG("dmem_base pa: 0x%x, size: 0x%x\n", phy_addr, phy_size);
	LOG_DBG("dmem_base va: 0x%lx\n", ccu->dmem_base);

	/*remap pmem_base*/
	phy_addr = MTK_CCU_PMEM_BASE;
	phy_size = MTK_CCU_PMEM_SIZE;
	ccu->pmem_base = devm_ioremap(dev, phy_addr, phy_size);
	LOG_DBG("pmem_base pa: 0x%x, size: 0x%x\n", phy_addr, phy_size);
	LOG_DBG("pmem_base va: 0x%lx\n", ccu->pmem_base);

#if defined(FPGA_UT)
	/*remap camsys_base*/
	phy_addr = 0x1A000000;
	phy_size = 0x200000;
	ccu->camsys_base = devm_ioremap(dev, phy_addr, phy_size);
	LOG_DBG("camsys_base pa: 0x%x, size: 0x%x\n", phy_addr, phy_size);
	LOG_DBG("camsys_base va: 0x%lx\n", ccu->camsys_base);

	/*remap ccu_main_base*/
	phy_addr = 0x1B200000;
	phy_size = 0x10000;
	ccu->ccu_main_base = devm_ioremap(dev, phy_addr, phy_size);
	LOG_DBG("ccu_main_base pa: 0x%x, size: 0x%x\n", phy_addr, phy_size);
	LOG_DBG("ccu_main_base va: 0x%lx\n", ccu->ccu_main_base);
#endif

#if !defined(FPGA_UT)
	/* get Clock control from device tree.  */
	pm_runtime_enable(ccu->dev);
	ccu->ccu_clk_pwr_ctrl[0] = devm_clk_get(ccu->dev,
		"CLK_TOP_CCUSYS_SEL");
	if (IS_ERR(ccu->ccu_clk_pwr_ctrl[0])) {
		dev_err(ccu->dev, "Get CLK_TOP_CCUSYS_SEL fail.\n");
		return PTR_ERR(ccu->ccu_clk_pwr_ctrl[0]);
	}
	ccu->ccu_clk_pwr_ctrl[1] = devm_clk_get(ccu->dev,
		"CLK_TOP_CCU_AHB_SEL");
	if (IS_ERR(ccu->ccu_clk_pwr_ctrl[1])) {
		dev_err(ccu->dev, "Get CLK_TOP_CCU_AHB_SEL fail.\n");
		return PTR_ERR(ccu->ccu_clk_pwr_ctrl[1]);
	}
	ccu->ccu_clk_pwr_ctrl[2] = devm_clk_get(ccu->dev,
		"CLK_CCU_LARB");
	if (IS_ERR(ccu->ccu_clk_pwr_ctrl[2])) {
		dev_err(ccu->dev, "Get CLK_CCU_LARB fail.\n");
		return PTR_ERR(ccu->ccu_clk_pwr_ctrl[2]);
	}
	ccu->ccu_clk_pwr_ctrl[3] = devm_clk_get(ccu->dev,
		"CLK_CCU_AHB");
	if (IS_ERR(ccu->ccu_clk_pwr_ctrl[3])) {
		dev_err(ccu->dev, "Get CLK_CCU_AHB fail.\n");
		return PTR_ERR(ccu->ccu_clk_pwr_ctrl[3]);
	}
	ccu->ccu_clk_pwr_ctrl[4] = devm_clk_get(ccu->dev,
		"CLK_CCUSYS_CCU0");
	if (IS_ERR(ccu->ccu_clk_pwr_ctrl[4])) {
		dev_err(ccu->dev, "Get CLK_CCUSYS_CCU0 fail.\n");
		return PTR_ERR(ccu->ccu_clk_pwr_ctrl[4]);
	}

	ccu->path_ccuo = of_mtk_icc_get(ccu->dev, "larb_ccuo");
	ccu->path_ccui = of_mtk_icc_get(ccu->dev, "larb_ccui");
#endif

#if defined(CCU1_DEVICE)
	ret = of_property_read_u32(node, "mediatek,ccu_rproc1",
		&ccu_rproc1_phandle);
	node1 = of_find_node_by_phandle(ccu_rproc1_phandle);
	if (node1)
		ccu->pdev1 = of_find_device_by_node(node1);
	if (WARN_ON(!ccu->pdev1)) {
		dev_err(ccu->dev, "failed to get ccu rproc1 pdev\n");
		of_node_put(node1);
	}
#endif

	/* get irq from device irq*/
	ccu->irq_num = irq_of_parse_and_map(node, 0);
	LOG_DBG("ccu_probe irq_num: %d\n", ccu->irq_num);

	if (devm_request_threaded_irq(ccu->dev, ccu->irq_num, NULL,
		mtk_ccu_isr_handler, IRQF_ONESHOT, "ccu_rproc", ccu)) {
		dev_err(ccu->dev, "fail to request ccu irq!\n");
		return -ENODEV;
	}

	/*prepare mutex & log's waitqueuehead*/
	mutex_init(&ccu->ipc_desc_lock);
	mutex_init(&ccu->ipc_send_lock);
	init_waitqueue_head(&ccu->WaitQueueHead);

	/*register char dev for log ioctl*/
	ret = mtk_ccu_reg_chardev(ccu);
	if (ret)
		dev_err(ccu->dev, "failed to regist char dev");

	dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34));

	ccu->ext_buf.meminfo.size = MTK_CCU_DRAM_LOG_BUF_SIZE * 4;
	ccu->ext_buf.meminfo.cached = false;
	ret = mtk_ccu_allocate_mem(ccu->dev, &ccu->ext_buf);
	if (ret) {
		dev_err(ccu->dev, "alloc mem failed\n");
		return ret;
	}
	rproc->auto_boot = false;

	ret = rproc_add(rproc);
	return ret;
}

static int mtk_ccu_remove(struct platform_device *pdev)
{
	struct mtk_ccu *ccu = platform_get_drvdata(pdev);

	mtk_ccu_deallocate_mem(ccu->dev, &ccu->ext_buf);
	disable_irq(ccu->irq_num);
	rproc_del(ccu->rproc);
	rproc_free(ccu->rproc);
	pm_runtime_disable(ccu->dev);
	mtk_ccu_unreg_chardev(ccu);
	return 0;
}

static int mtk_ccu_read_platform_info_from_dt(struct device_node
		*node, uint32_t *ccu_hw_base, uint32_t *ccu_hw_size)
{
	uint32_t reg[4] = {0, 0, 0, 0};
	int ret = 0;

	ret = of_property_read_u32_array(node, "reg", reg, 4);
	if (ret < 0)
		return ret;

	*ccu_hw_base = reg[1];
	*ccu_hw_size = reg[3];
	return 0;
}

#if defined(CCU1_DEVICE)
static int mtk_ccu1_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34));

	pm_runtime_enable(dev);
	return 0;
}

static int mtk_ccu1_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pm_runtime_disable(dev);
	return 0;
}
#endif

#if !defined(FPGA_UT)
static void mtk_ccu_get_power(struct device *dev)
{
	int ret = pm_runtime_get_sync(dev);

	if (ret != 0)
		dev_err(dev, "pm_runtime_get_sync failed %d", ret);
}

static void mtk_ccu_put_power(struct device *dev)
{
	pm_runtime_put_sync(dev);
}
#endif

static const struct of_device_id mtk_ccu_of_ids[] = {
	{.compatible = "mediatek,ccu_rproc", },
	{},
};
MODULE_DEVICE_TABLE(of, mtk_ccu_of_ids);

static struct platform_driver ccu_rproc_driver = {
	.probe = mtk_ccu_probe,
	.remove = mtk_ccu_remove,
	.driver = {
		.name = MTK_CCU_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mtk_ccu_of_ids),
	},
};

#if defined(CCU1_DEVICE)
static const struct of_device_id mtk_ccu1_of_ids[] = {
	{.compatible = "mediatek,ccu_rproc1", },
	{},
};
MODULE_DEVICE_TABLE(of, mtk_ccu1_of_ids);

static struct platform_driver ccu_rproc1_driver = {
	.probe = mtk_ccu1_probe,
	.remove = mtk_ccu1_remove,
	.driver = {
		.name = MTK_CCU1_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mtk_ccu1_of_ids),
	},
};
#endif

module_platform_driver(ccu_rproc_driver);
#if defined(CCU1_DEVICE)
module_platform_driver(ccu_rproc1_driver);
#endif

MODULE_DESCRIPTION("MTK CCU Rproc Driver");
MODULE_LICENSE("GPL v2");

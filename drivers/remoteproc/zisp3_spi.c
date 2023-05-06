// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Xiaomi, Inc.
 */

#include <linux/completion.h>
#include <linux/mfd/ispv3_dev.h>
#include <linux/miscdevice.h>
#include <linux/iommu.h>
#include <linux/uaccess.h>

#include "zisp_rproc_utils.h"

#define ZISPV3_MEM_NR 1

#define ZISPV3_AIO_TIME0		0xe40004
#define ZISPV3_AIO_TIME1		0xe40008
#define ZISPV3_AIO_TIME2		0xe4000c
#define ZISPV3_AIO_TIME3		0xe40010

#define ZISPV3_AIO0			0xe40054
#define ZISPV3_AIO1			0xe40058
#define ZISPV3_AIO2			0xe4005c
#define ZISPV3_AIO3			0xe40060
#define ZISPV3_AIO4			0xe40064
#define ZISPV3_MCU_CLK_EN		0xe80000
#define ZISPV3_MCU_CLK_RST		0xe80200
#define ZISPV3_MCU_PD3			0xe82000
#define ZISPV3_AP_SWINT_GPIO_EN		0xea0700
#define ZISPV3_SRAM_DP			0xee0014
#define ZISPV3_SRAM_EN			0xee0018
#define ZISPV3_AP_SWINT_EN		0xef0104
#define ZISPV3_AP_SWINT_STATUS		0xef0108
#define ZISPV3_MCU_WP			0xef010c
#define ZISPV3_AP_SWINT_CAUSE		0xef0310
#define ZISPV3_MCU_SWINT		0xef030c
#define ZISPV3_AP_SWINT_INTA_SEL	0xef0604
#define ZISPV3_MCU_PD1			0xef0304
#define ZISPV3_MCU_PD2			0xef0300
#define ZISPV3_MCU_ISO			0xef0000

struct mem_segment {
	dma_addr_t dma_addr;
	dma_addr_t offset;
	resource_size_t size;
};

static struct zisp_rproc_mem memlist[ZISPV3_MEM_NR];

static struct mem_segment zispv3_mem_segment[ZISPV3_MEM_NR] = {
	{
		0x7f700000, 0, 300 * 1024,
	},
};

static inline int zispv3_spi_write_mem(struct zisp_rproc *zisp_rproc,
				       void *addr,
				       const u8 *data_buf,
				       uint32_t size)
{
	struct spi_device *spi;
	struct spi_transfer transfer;
	struct spi_message message;
	uint8_t *local_buf;
	uint32_t count;
	uint32_t remainder;
	int i;

	int ret = 0;
	spi = zisp_rproc->spi;
	count = size / 65531;
	remainder = size % 65531;

	if (!spi)
		return -ENXIO;

	local_buf = zisp_rproc->local_buf;

	for (i = 0; i < count; ++i) {
		spi_message_init(&message);
		memset(&transfer, 0, sizeof(transfer));

		local_buf[0] = 5;
		local_buf[1] = (uint64_t)addr & 0xff;
		local_buf[2] = ((uint64_t)addr >> 8) & 0xff;
		local_buf[3] = ((uint64_t)addr >> 16) & 0xff;
		local_buf[4] = ((uint64_t)addr >> 24) & 0xff;

		memcpy(&local_buf[5], data_buf, 65531);

		transfer.tx_buf = local_buf;
		transfer.len = 65536;

		spi_message_add_tail(&transfer, &message);

		ret = spi_sync(spi, &message);

		addr += 65531;
		data_buf += 65531;
	}

	if (remainder) {
		spi_message_init(&message);
		memset(&transfer, 0, sizeof(transfer));

		local_buf[0] = 5;
		local_buf[1] = (uint64_t)addr & 0xff;
		local_buf[2] = ((uint64_t)addr >> 8) & 0xff;
		local_buf[3] = ((uint64_t)addr >> 16) & 0xff;
		local_buf[4] = ((uint64_t)addr >> 24) & 0xff;

		memcpy(&local_buf[5], data_buf, remainder);

		transfer.tx_buf = local_buf;
		transfer.len = remainder + 5;

		spi_message_add_tail(&transfer, &message);

		ret = spi_sync(spi, &message);
	}

	return ret;
}

static inline int zispv3_spi_write_reg(struct spi_device *spi,
				       uint32_t reg_addr,
				       uint32_t value)
{
	struct spi_transfer transfer;
	struct spi_message message;
	uint8_t local_buf[9];
	int ret = 0;

	if (!spi)
		return -ENXIO;

	spi_message_init(&message);
	memset(&transfer, 0, sizeof(transfer));

	local_buf[0] = 9;
	local_buf[1] = reg_addr & 0xff;
	local_buf[2] = (reg_addr >> 8) & 0xff;
	local_buf[3] = (reg_addr >> 16) & 0xff;
	local_buf[4] = (reg_addr >> 24) & 0xff;

	memcpy(&local_buf[5], &value, 4);

	transfer.tx_buf = local_buf;
	transfer.len = 9;

	spi_message_add_tail(&transfer, &message);

	ret = spi_sync(spi, &message);

	return ret;
}

static int zisp3_rproc_start(struct rproc *rproc)
{
	int ret = 0;
	struct zisp_rproc *zisp_rproc = rproc->priv;

	/*
	 * setting boot address
	 */

	if ((rproc->bootaddr & 0xfff) > 0x800)
		rproc->bootaddr += 0x1000;
	zispv3_spi_write_reg(zisp_rproc->spi,
			     zisp_rproc->reg_base + ZISPV3_AIO0,
			     (rproc->bootaddr & 0xf000) | 0x137);
	zispv3_spi_write_reg(zisp_rproc->spi,
			     zisp_rproc->reg_base + ZISPV3_AIO1,
			     rproc->bootaddr >> 16);
	zispv3_spi_write_reg(zisp_rproc->spi,
			     zisp_rproc->reg_base + ZISPV3_AIO2, 0x113);
	zispv3_spi_write_reg(zisp_rproc->spi,
			     zisp_rproc->reg_base + ZISPV3_AIO3,
			     (rproc->bootaddr & 0xfff) << 4 | 0x1);
	zispv3_spi_write_reg(zisp_rproc->spi,
			     zisp_rproc->reg_base + ZISPV3_AIO4, 0x9102);
	zispv3_spi_write_reg(zisp_rproc->spi,
			     zisp_rproc->reg_base + ZISPV3_AP_SWINT_CAUSE,
			     rproc->bootaddr);

	/*
	 * enable mcu clock & start
	 */

	zispv3_spi_write_reg(zisp_rproc->spi,
			     zisp_rproc->reg_base + ZISPV3_MCU_PD1, 0x7e);
	zispv3_spi_write_reg(zisp_rproc->spi,
			     zisp_rproc->reg_base + ZISPV3_MCU_PD2, 0x7e);
	zispv3_spi_write_reg(zisp_rproc->spi,
			     zisp_rproc->reg_base + ZISPV3_MCU_PD3, 0xffffff01);

	zispv3_spi_write_reg(zisp_rproc->spi,
			     zisp_rproc->reg_base + ZISPV3_MCU_WP, 0xfffff000);
	zispv3_spi_write_reg(zisp_rproc->spi,
			     zisp_rproc->reg_base + ZISPV3_MCU_CLK_EN, 0xffffc001);
	zispv3_spi_write_reg(zisp_rproc->spi,
			     zisp_rproc->reg_base + ZISPV3_MCU_CLK_RST, 0xffffc001);


	return ret;
}

static int zisp3_rproc_stop(struct rproc *rproc)
{
	/*do your mcu stop */
	struct zisp_rproc *zisp_rproc;

	zisp_rproc = rproc->priv;
	rproc_coredump_cleanup(rproc);
	return 0;
}

static void zisp3_rproc_kick(struct rproc *rproc, int vqid)
{
	/* do your kick msg here */

}

static void *zisp3_mcu_rproc_da_to_va(struct rproc *rproc, u64 da, size_t len, bool *is_iomem)
{
	return (void *)da;
}

static void zisp3_mcu_shutdown(struct zisp_rproc *zisp_rproc)
{
	dev_info(zisp_rproc->dev, "AP shutdown ZISP3 MCU\n");

	/* do your shutdown here */
}

static void zisp3_mcu_remove(struct zisp_rproc *zisp_rproc)
{
	dev_info(zisp_rproc->dev, "AP remove ZISP3 MCU\n");

	/* do your remove here */
}

static int zisp3_mcu_parse_mem(struct platform_device *pdev,
			       struct resource **_iores)
{
	int i;
	struct zisp_rproc_mem *mem;
	struct rproc *rproc;
	struct zisp_rproc *zisp_rproc;
	struct resource *iores;

	zisp_rproc = platform_get_drvdata(pdev);
	zisp_rproc->memlist = memlist;
	rproc = zisp_rproc->rproc;

	for (i = 0 ; i < ZISPV3_MEM_NR; ++i) {
		if (!_iores) {
			iores = platform_get_resource(pdev, IORESOURCE_MEM, i);

			if (!iores) {
				pr_info("get resource %d info failed", i);
				return -EINVAL;
			}
		} else {
			iores = _iores[i];
		}

		if (!devm_request_mem_region(&pdev->dev, iores->start,
					     resource_size(iores),
					     pdev->name)) {
			pr_info("request region %d failed", i);
			return -EINVAL;
		}

		mem = &zisp_rproc->memlist[i];

		mem->phys_addr = iores->start + zispv3_mem_segment[i].offset;
		mem->size = zispv3_mem_segment[i].size;
		mem->dma_addr = zispv3_mem_segment[i].dma_addr;
		mem->virt_addr = devm_ioremap_wc(&pdev->dev,
						 mem->phys_addr, mem->size);

		rproc_coredump_add_segment(rproc, mem->dma_addr, mem->size);
	}

	return 0;
}

static int zisp3_mcu_parse_irq(struct platform_device *pdev,
			       int irq)
{
	struct zisp_rproc *zisp_rproc;

	zisp_rproc = platform_get_drvdata(pdev);
	memset(&zisp_rproc->ipi, 0, sizeof(zisp_rproc->ipi));

	if (irq < 0) {
		irq = platform_get_irq(pdev, 0);

		if (irq >= 0) {
			zisp_rproc->ipi.irq = irq;
			return 0;
		}
	} else {
		zisp_rproc->ipi.irq = irq;
		return 0;
	}

	return -EINVAL;
}

static struct elf32_shdr *
find_table(struct device *dev, struct elf32_hdr *ehdr, size_t fw_size)
{
	struct elf32_shdr *shdr;
	int i;
	const char *name_table;
	struct resource_table *table = NULL;
	const u8 *elf_data = (void *)ehdr;

	/* look for the resource table and handle it */
	shdr = (struct elf32_shdr *)(elf_data + ehdr->e_shoff);
	name_table = elf_data + shdr[ehdr->e_shstrndx].sh_offset;

	for (i = 0; i < ehdr->e_shnum; i++, shdr++) {
		u32 size = shdr->sh_size;
		u32 offset = shdr->sh_offset;

		if (strcmp(name_table + shdr->sh_name, ".resource_table"))
			continue;

		table = (struct resource_table *)(elf_data + offset);

		/* make sure we have the entire table */
		if (offset + size > fw_size || offset + size < size) {
			dev_err(dev, "resource table truncated\n");
			return NULL;
		}

		/* make sure table has at least the header */
		if (sizeof(struct resource_table) > size) {
			dev_err(dev, "header-less resource table\n");
			return NULL;
		}

		/* we don't support any version beyond the first */
		if (table->ver != 1) {
			dev_err(dev, "unsupported fw ver: %d\n", table->ver);
			return NULL;
		}

		/* make sure reserved bytes are zeroes */
		if (table->reserved[0] || table->reserved[1]) {
			dev_err(dev, "non zero reserved bytes\n");
			return NULL;
		}

		/* make sure the offsets array isn't truncated */
		if (table->num * sizeof(table->offset[0]) +
				sizeof(struct resource_table) > size) {
			dev_err(dev, "resource table incomplete\n");
			return NULL;
		}

		return shdr;
	}

	return NULL;
}

static int zisp3_elf_load_rsc_table(struct rproc *rproc, const struct firmware *fw)
{
	struct elf32_hdr *ehdr;
	struct elf32_shdr *shdr;
	struct device *dev = &rproc->dev;
	struct resource_table *table = NULL;
	const u8 *elf_data = fw->data;
	size_t tablesz;

	ehdr = (struct elf32_hdr *)elf_data;

	shdr = find_table(dev, ehdr, fw->size);
	if (!shdr)
		return -EINVAL;

	table = (struct resource_table *)(elf_data + shdr->sh_offset);
	tablesz = shdr->sh_size;

	/*
	 * Create a copy of the resource table. When a virtio device starts
	 * and calls vring_new_virtqueue() the address of the allocated vring
	 * will be stored in the cached_table. Before the device is started,
	 * cached_table will be copied into device memory.
	 */
	rproc->cached_table = kmemdup(table, tablesz, GFP_KERNEL);
	if (!rproc->cached_table)
		return -ENOMEM;

	rproc->table_ptr = rproc->cached_table;
	rproc->table_sz = 0;

	return 0;
}

static int zisp3_elf_load_segments(struct rproc *rproc, const struct firmware *fw)
{
	struct device *dev = &rproc->dev;
	struct elf32_hdr *ehdr;
	struct elf32_phdr *phdr;
	int i, ret = 0;
	const u8 *elf_data = fw->data;
	struct zisp_rproc *zisp_rproc = rproc->priv;

	ehdr = (struct elf32_hdr *)elf_data;
	phdr = (struct elf32_phdr *)(elf_data + ehdr->e_phoff);

	/* go through the available ELF segments */
	for (i = 0; i < ehdr->e_phnum; i++, phdr++) {
		u32 da = phdr->p_paddr;
		u32 memsz = phdr->p_memsz;
		u32 filesz = phdr->p_filesz;
		u32 offset = phdr->p_offset;
		void *ptr;

		if (phdr->p_type != PT_LOAD)
			continue;

		dev_dbg(dev, "phdr: type %d da 0x%x memsz 0x%x filesz 0x%x\n",
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
		ptr = rproc_da_to_va(rproc, da, memsz, NULL);
		if (!ptr) {
			dev_err(dev, "bad phdr da 0x%x mem 0x%x\n", da, memsz);
			ret = -EINVAL;
			break;
		}

		/* put the segment where the remote processor expects it */
		if (phdr->p_filesz)
			zispv3_spi_write_mem(zisp_rproc, ptr,
					     elf_data + phdr->p_offset, filesz);

	}

	return ret;
}

static int zisp3_mcu_init(struct zisp_rproc *zisp_rproc)
{
	struct platform_device *pdev;
	struct ispv3_data *data;
	int ret = 0;

	pdev = to_platform_device(zisp_rproc->dev);
	data = dev_get_drvdata(pdev->dev.parent);
	data->rproc = (void *)zisp_rproc->rproc;
	zisp_rproc->spi = data->spi;
	zisp_rproc->reg_base = 0xff000000;
	zisp_rproc->rproc->ops->load = zisp3_elf_load_segments;
	zisp_rproc->rproc->ops->parse_fw = zisp3_elf_load_rsc_table;
	zisp_rproc->local_buf = devm_kmalloc(&pdev->dev, 65536, GFP_KERNEL);

	if (!zisp_rproc->local_buf) {
		data->rproc = NULL;
		return -ENOMEM;
	}

	return ret;
}

static struct rproc_ops zisp3_rproc_ops = {
	.start = zisp3_rproc_start,
	.stop = zisp3_rproc_stop,
	.kick = zisp3_rproc_kick,
	.da_to_va = zisp3_mcu_rproc_da_to_va,
};

struct zisp_ops zisp3_ops_spi = {
	.init = zisp3_mcu_init,
	.shutdown = zisp3_mcu_shutdown,
	.remove = zisp3_mcu_remove,
	.parse_mem = zisp3_mcu_parse_mem,
	.parse_irq = zisp3_mcu_parse_irq,
	.rproc_ops = &zisp3_rproc_ops,
#ifdef CONFIG_ZISP_OCRAM_AON
	.firmware = "nuttx_aon",
#else
	.firmware = "nuttx_naon",
#endif
};

MODULE_LICENSE("GPL v2");

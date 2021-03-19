// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/elf.h>
#include <linux/dma-mapping.h>
#include <linux/of_device.h>

#define DRAM_BOOT_SIZE 0x400
#define DRAM_MAIN_SIZE 0xD000

enum LOAD_FW_METHOD {
	LOAD_NONE,
	LOAD_ELF_FILE,
};

static int load_fw_method;

struct mdla_fw {
	void *buf;
	dma_addr_t da;
};
static struct mdla_fw boot;
static struct mdla_fw code;

static int alloc_boot_mem(struct device *dev)
{
	boot.buf = dma_alloc_coherent(dev, DRAM_BOOT_SIZE, &boot.da, GFP_KERNEL);
	if (boot.buf == NULL || boot.da == 0) {
		dev_info(dev, "%s() dma_alloc_coherent bootcode fail\n\n", __func__);
		return -1;
	}

	code.buf = dma_alloc_coherent(dev, DRAM_MAIN_SIZE, &code.da, GFP_KERNEL);
	if (code.buf == NULL || code.da == 0) {
		dev_info(dev, "%s() dma_alloc_coherent maincode fail\n\n", __func__);
		dma_free_coherent(dev, DRAM_BOOT_SIZE, boot.buf, boot.da);
		return -1;
	}

	memset(boot.buf, 0, DRAM_BOOT_SIZE);
	memset(code.buf, 0, DRAM_MAIN_SIZE);

	/* AISIM: It's not necessary to get iova */

	return 0;
}

static void free_boot_mem(struct device *dev)
{
	if (code.buf && code.da)
		dma_free_coherent(dev, DRAM_MAIN_SIZE, code.buf, code.da);
	if (boot.buf && boot.da)
		dma_free_coherent(dev, DRAM_BOOT_SIZE, boot.buf, boot.da);

	code.da = 0;
	boot.da = 0;
}


static int mdla_plat_load_fw_elf(struct device *dev, unsigned int *bootcode, unsigned int *maincode)
{
	int ret = 0;
	struct file *elf;
	Elf32_Ehdr hdr;
	Elf32_Shdr *shdr;
	unsigned int shstrtab_ofs = 0;
	unsigned int shstrtab_sz = 0;
	char section_name[32] = {0};
	char *main_code_idx;
	int i, sec_idx;
	unsigned int idx;
	char *name = "/lib/firmware/mdla_firmware.elf";

	if (alloc_boot_mem(dev) < 0)
		return -1;

	elf = filp_open(name, O_RDONLY, 777);

	if (IS_ERR(elf)) {
		dev_info(dev, "doesn't load fw\n");
		return -1;
	}

	kernel_read(elf, (char *)&hdr, sizeof(hdr), &elf->f_pos);

	if (memcmp(hdr.e_ident, ELFMAG, SELFMAG) != 0) {
		dev_info(dev, "Not an ELF file - it has the wrong magic bytes at the start\n");
		ret = -1;
		goto out;
	} else if (hdr.e_ident[EI_DATA] != 1) {
		dev_info(dev, "Only support LSB data!!\n");
		ret = -1;
		goto out;
	}

	shdr = kmalloc(hdr.e_shentsize * hdr.e_shnum + hdr.e_shnum, GFP_KERNEL);
	if (!shdr) {
		ret = -1;
		goto out;
	}

	main_code_idx = ((char *)shdr) + hdr.e_shentsize * hdr.e_shnum;
	memset(main_code_idx, 0, hdr.e_shnum);

	elf->f_pos = hdr.e_shoff;

	/* Find main code section */
	for (sec_idx = 0; sec_idx < hdr.e_shnum; sec_idx++) {
		kernel_read(elf, (char *)&shdr[sec_idx], hdr.e_shentsize, &elf->f_pos);
		if (sec_idx == hdr.e_shstrndx) {
			shstrtab_ofs = shdr[sec_idx].sh_offset;
			shstrtab_sz = shdr[sec_idx].sh_size;
		}
		if (((shdr[sec_idx].sh_addr >> 16) == 0x1900)
					&& (shdr[sec_idx].sh_type != SHT_NOBITS))
			main_code_idx[sec_idx] = 1;
	}

	elf->f_pos = shstrtab_ofs;

	/* Find boot code section */
	sec_idx = 0;
	i = 0;
	while (shstrtab_sz--) {
		kernel_read(elf, &section_name[i], 1, &elf->f_pos);
		if (section_name[i] == '\0') {
			if (strncmp(section_name, ".mdla_boot", 10) == 0)
				break;
			memset(section_name, 0, sizeof(section_name));
			sec_idx++;
			i = 0;
		} else {
			i++;
		}
	}


	/* load boot code */
	elf->f_pos = shdr[sec_idx].sh_offset;
	kernel_read(elf, (char *)boot.buf, shdr[sec_idx].sh_size, &elf->f_pos);

	/* load main code */
	idx = 0;
	for (sec_idx = 0; sec_idx < hdr.e_shnum; sec_idx++) {
		if (main_code_idx[sec_idx] == 0)
			continue;

		elf->f_pos = shdr[sec_idx].sh_offset;
		kernel_read(elf, (char *)(code.buf + (shdr[sec_idx].sh_addr & 0xffff)),
					shdr[sec_idx].sh_size, &elf->f_pos);
	}

	*bootcode = (unsigned int)boot.da;
	*maincode = (unsigned int)code.da;

	kfree(shdr);
out:
	filp_close(elf, NULL);
	return ret;
}

static void mdla_plat_unload_fw_elf(struct device *dev)
{
	free_boot_mem(dev);
}



int mdla_plat_load_fw(struct device *dev, unsigned int *bootcode, unsigned int *maincode)
{
	int ret = -1;
	const char *method = NULL;

	ret = of_property_read_string(dev->of_node, "boot-method", &method);

	if (ret < 0)
		return -1;

	if (!strcmp(method, "elf"))
		load_fw_method = LOAD_ELF_FILE;

	switch (load_fw_method) {
	case LOAD_ELF_FILE:
		ret = mdla_plat_load_fw_elf(dev, bootcode, maincode);
		break;
	default:
		break;
	}


	return ret;
}

void mdla_plat_unload_fw(struct device *dev)
{
	switch (load_fw_method) {
	case LOAD_ELF_FILE:
		mdla_plat_unload_fw_elf(dev);
		break;
	default:
		break;
	}
}


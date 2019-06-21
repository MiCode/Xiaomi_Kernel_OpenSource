/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#include <linux/device.h>
#include <linux/err.h>
#include <linux/firmware.h>
#include <linux/elf.h>
#include <linux/slab.h>
#include <linux/list.h>
#include "wcd-dsp-utils.h"

static bool wdsp_is_valid_elf_hdr(const struct elf32_hdr *ehdr,
				  size_t fw_size)
{
	if (fw_size < sizeof(*ehdr)) {
		pr_err("%s: Firmware too small\n", __func__);
		goto elf_check_fail;
	}

	if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
		pr_err("%s: Not an ELF file\n", __func__);
		goto elf_check_fail;
	}

	if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
		pr_err("%s: Not an executable image\n", __func__);
		goto elf_check_fail;
	}

	if (ehdr->e_phnum == 0) {
		pr_err("%s: no segments to load\n", __func__);
		goto elf_check_fail;
	}

	if (sizeof(struct elf32_phdr) * ehdr->e_phnum +
	    sizeof(struct elf32_hdr) > fw_size) {
		pr_err("%s: Too small MDT file\n", __func__);
		goto elf_check_fail;
	}

	return true;

elf_check_fail:
	return false;
}

static int wdsp_add_segment_to_list(struct device *dev,
				    const char *img_fname,
				    const struct elf32_phdr *phdr,
				    int phdr_idx,
				    struct list_head *seg_list)
{
	struct wdsp_img_segment *seg;
	int ret = 0;

	/* Do not load segments with zero size */
	if (phdr->p_filesz == 0 || phdr->p_memsz == 0)
		goto done;

	seg = kzalloc(sizeof(*seg), GFP_KERNEL);
	if (!seg) {
		ret = -ENOMEM;
		goto done;
	}

	snprintf(seg->split_fname, sizeof(seg->split_fname),
		 "%s.b%02d", img_fname, phdr_idx);
	ret = request_firmware(&seg->split_fw, seg->split_fname, dev);
	if (ret < 0) {
		dev_err(dev, "%s: firmware %s not found\n",
			__func__, seg->split_fname);
		goto bad_seg;
	}

	if (phdr->p_filesz != seg->split_fw->size) {
		dev_err(dev,
			"%s: %s size mismatch, phdr_size: 0x%x fw_size: 0x%zx",
			__func__, seg->split_fname, phdr->p_filesz,
			seg->split_fw->size);
		ret = -EINVAL;
		goto bad_elf;
	}

	seg->load_addr = phdr->p_paddr;
	seg->size = phdr->p_filesz;
	seg->data = (u8 *) seg->split_fw->data;

	list_add_tail(&seg->list, seg_list);
done:
	return ret;
bad_elf:
	release_firmware(seg->split_fw);
bad_seg:
	kfree(seg);
	return ret;
}

/*
 * wdsp_flush_segment_list: Flush the list of segments
 * @seg_list: List of segments to be flushed
 * This API will traverse through the list of segments provided in
 * seg_list, release the firmware for each segment and delete the
 * segment from the list.
 */
void wdsp_flush_segment_list(struct list_head *seg_list)
{
	struct wdsp_img_segment *seg, *next;

	list_for_each_entry_safe(seg, next, seg_list, list) {
		release_firmware(seg->split_fw);
		list_del(&seg->list);
		kfree(seg);
	}
}
EXPORT_SYMBOL(wdsp_flush_segment_list);

/*
 * wdsp_get_segment_list: Get the list of requested segments
 * @dev: struct device pointer of caller
 * @img_fname: Image name for the mdt and split firmware files
 * @segment_type: Requested segment type, should be either
 *		  WDSP_ELF_FLAG_RE or WDSP_ELF_FLAG_WRITE
 * @seg_list: An initialized head for list of segmented to be returned
 * @entry_point: Pointer to return the entry point of the image
 * This API will parse the mdt file for img_fname and create
 * an struct wdsp_img_segment for each segment that matches segment_type
 * and add this structure to list pointed by seg_list
 */
int wdsp_get_segment_list(struct device *dev,
			  const char *img_fname,
			  unsigned int segment_type,
			  struct list_head *seg_list,
			  u32 *entry_point)
{
	const struct firmware *fw;
	const struct elf32_hdr *ehdr;
	const struct elf32_phdr *phdr;
	const u8 *elf_ptr;
	char mdt_name[WDSP_IMG_NAME_LEN_MAX];
	int ret, phdr_idx;
	bool segment_match;

	if (!dev) {
		ret = -EINVAL;
		pr_err("%s: Invalid device handle\n", __func__);
		goto done;
	}

	if (!img_fname || !seg_list || !entry_point) {
		ret = -EINVAL;
		dev_err(dev, "%s: Invalid input params\n",
			__func__);
		goto done;
	}

	if (segment_type != WDSP_ELF_FLAG_RE &&
	    segment_type != WDSP_ELF_FLAG_WRITE) {
		dev_err(dev, "%s: Invalid request for segment_type %d\n",
			__func__, segment_type);
		ret = -EINVAL;
		goto done;
	}

	snprintf(mdt_name, sizeof(mdt_name), "%s.mdt", img_fname);
	ret = request_firmware(&fw, mdt_name, dev);
	if (ret < 0) {
		dev_err(dev, "%s: firmware %s not found\n",
			__func__, mdt_name);
		goto done;
	}

	ehdr = (struct elf32_hdr *) fw->data;
	*entry_point = ehdr->e_entry;
	if (!wdsp_is_valid_elf_hdr(ehdr, fw->size)) {
		dev_err(dev, "%s: fw mdt %s is invalid\n",
			__func__, mdt_name);
		ret = -EINVAL;
		goto bad_elf;
	}

	elf_ptr = fw->data + sizeof(*ehdr);
	for (phdr_idx = 0; phdr_idx < ehdr->e_phnum; phdr_idx++) {
		phdr = (struct elf32_phdr *) elf_ptr;
		segment_match = false;

		switch (segment_type) {
		case WDSP_ELF_FLAG_RE:
			/*
			 * Flag can be READ or EXECUTE or both but
			 * WRITE flag should not be set.
			 */
			if ((phdr->p_flags & segment_type) &&
			    !(phdr->p_flags & WDSP_ELF_FLAG_WRITE))
				segment_match = true;
			break;
		case WDSP_ELF_FLAG_WRITE:
			/*
			 * If WRITE flag is set, other flags do not
			 * matter.
			 */
			if (phdr->p_flags & segment_type)
				segment_match = true;
			break;
		}

		if (segment_match) {
			ret = wdsp_add_segment_to_list(dev, img_fname, phdr,
						       phdr_idx, seg_list);
			if (ret < 0) {
				wdsp_flush_segment_list(seg_list);
				goto bad_elf;
			}
		}
		elf_ptr = elf_ptr + sizeof(*phdr);
	}

bad_elf:
	release_firmware(fw);
done:
	return ret;
}
EXPORT_SYMBOL(wdsp_get_segment_list);

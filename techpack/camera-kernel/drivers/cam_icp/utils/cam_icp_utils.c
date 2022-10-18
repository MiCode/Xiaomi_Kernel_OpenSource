// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include "cam_icp_utils.h"

int32_t cam_icp_validate_fw(const uint8_t *elf,
	uint32_t machine_id)
{
	struct elf32_hdr *elf_hdr;

	if (!elf) {
		CAM_ERR(CAM_ICP, "Invalid params");
		return -EINVAL;
	}

	elf_hdr = (struct elf32_hdr *)elf;

	if (memcmp(elf_hdr->e_ident, ELFMAG, SELFMAG)) {
		CAM_ERR(CAM_ICP, "ICP elf identifier is failed");
		return -EINVAL;
	}

	/* check architecture */
	if (elf_hdr->e_machine != machine_id) {
		CAM_ERR(CAM_ICP, "unsupported arch: 0x%x", elf_hdr->e_machine);
		return -EINVAL;
	}

	/* check elf bit format */
	if (elf_hdr->e_ident[EI_CLASS] != ELFCLASS32) {
		CAM_ERR(CAM_ICP, "elf doesn't support 32 bit format");
		return -EINVAL;
	}

	return 0;
}

int32_t cam_icp_get_fw_size(
	const uint8_t *elf, uint32_t *fw_size)
{
	int32_t rc = 0;
	int32_t i = 0;
	uint32_t num_prg_hdrs;
	unsigned char *icp_prg_hdr_tbl;
	uint32_t seg_mem_size = 0;
	struct elf32_hdr *elf_hdr;
	struct elf32_phdr *prg_hdr;

	if (!elf || !fw_size) {
		CAM_ERR(CAM_ICP, "invalid args");
		return -EINVAL;
	}

	*fw_size = 0;

	elf_hdr = (struct elf32_hdr *)elf;
	num_prg_hdrs = elf_hdr->e_phnum;
	icp_prg_hdr_tbl = (unsigned char *)elf + elf_hdr->e_phoff;
	prg_hdr = (struct elf32_phdr *)&icp_prg_hdr_tbl[0];

	if (!prg_hdr) {
		CAM_ERR(CAM_ICP, "failed to get elf program header attr");
		return -EINVAL;
	}

	CAM_DBG(CAM_ICP, "num_prg_hdrs = %d", num_prg_hdrs);
	for (i = 0; i < num_prg_hdrs; i++, prg_hdr++) {
		if (prg_hdr->p_flags == 0)
			continue;

		seg_mem_size = (prg_hdr->p_memsz + prg_hdr->p_align - 1) &
					~(prg_hdr->p_align - 1);
		seg_mem_size += prg_hdr->p_paddr;
		CAM_DBG(CAM_ICP, "memsz:%x align:%x addr:%x seg_mem_size:%x",
			(int)prg_hdr->p_memsz, (int)prg_hdr->p_align,
			(int)prg_hdr->p_paddr, (int)seg_mem_size);
		if (*fw_size < seg_mem_size)
			*fw_size = seg_mem_size;

	}

	if (*fw_size == 0) {
		CAM_ERR(CAM_ICP, "invalid elf fw file");
		return -EINVAL;
	}

	return rc;
}

int32_t cam_icp_program_fw(const uint8_t *elf,
	uintptr_t fw_kva_addr)
{
	int32_t rc = 0;
	uint32_t num_prg_hdrs;
	unsigned char *icp_prg_hdr_tbl;
	int32_t i = 0;
	u8 *dest;
	u8 *src;
	struct elf32_hdr *elf_hdr;
	struct elf32_phdr *prg_hdr;

	elf_hdr = (struct elf32_hdr *)elf;
	num_prg_hdrs = elf_hdr->e_phnum;
	icp_prg_hdr_tbl = (unsigned char *)elf + elf_hdr->e_phoff;
	prg_hdr = (struct elf32_phdr *)&icp_prg_hdr_tbl[0];

	if (!prg_hdr) {
		CAM_ERR(CAM_ICP, "failed to get elf program header attr");
		return -EINVAL;
	}

	for (i = 0; i < num_prg_hdrs; i++, prg_hdr++) {
		if (prg_hdr->p_flags == 0)
			continue;

		CAM_DBG(CAM_ICP, "Loading FW header size: %u paddr: %pK",
			prg_hdr->p_filesz, prg_hdr->p_paddr);
		if (prg_hdr->p_filesz != 0) {
			src = (u8 *)((u8 *)elf + prg_hdr->p_offset);
			dest = (u8 *)(((u8 *)fw_kva_addr) +
				prg_hdr->p_paddr);

			memcpy_toio(dest, src, prg_hdr->p_filesz);
		}
	}

	return rc;
}

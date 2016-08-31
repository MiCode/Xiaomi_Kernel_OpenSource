/*
 * drivers/video/tegra/host/vic/vic03.h
 *
 * Tegra VIC03 Module Support
 *
 * Copyright (c) 2011-2013, NVIDIA Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __NVHOST_VIC03_H__
#define __NVHOST_VIC03_H__

#include <linux/types.h>
#include <linux/dma-attrs.h>
#include <linux/firmware.h>


extern struct platform_device tegra_vic03_device;

struct ucode_bin_header_v1_vic03 {
	u32 bin_magic;        /* 0x10de */
	u32 bin_ver;          /* cya, versioning of bin format (1) */
	u32 bin_size;         /* entire image size including this header */
	u32 os_bin_header_offset;
	u32 os_bin_data_offset;
	u32 os_bin_size;
	u32 fce_bin_header_offset;
	u32 fce_bin_data_offset;
	u32 fce_bin_size;
};

struct ucode_os_code_header_v1_vic03 {
	u32 offset;
	u32 size;
};

struct ucode_os_header_v1_vic03 {
	u32 os_code_offset;
	u32 os_code_size;
	u32 os_data_offset;
	u32 os_data_size;
	u32 num_apps;
	struct ucode_os_code_header_v1_vic03 *app_code;
	struct ucode_os_code_header_v1_vic03 *app_data;
	u32 *os_ovl_offset;
	u32 *of_ovl_size;
};

struct ucode_fce_header_v1_vic03 {
	u32 fce_ucode_offset;
	u32 fce_ucode_buffer_size;
	u32 fce_ucode_size;
};

struct ucode_v1_vic03 {
	struct ucode_bin_header_v1_vic03 *bin_header;
	struct ucode_os_header_v1_vic03  *os_header;
	struct ucode_fce_header_v1_vic03 *fce_header;
};

struct vic03 {
	struct nvhost_master *host;

	struct resource *reg_mem;
	bool is_booted;

	struct {
		bool valid;
		size_t size;

		struct {
			u32 bin_data_offset;
			u32 data_offset;
			u32 data_size;
			u32 code_offset;
			u32 size;
		} os, fce;

		dma_addr_t dma_addr;
		u32 *mapped;
	} ucode;

	void (*remove_support)(struct vic03 *);
};

struct nvhost_hwctx_handler *nvhost_vic03_alloc_hwctx_handler(
		u32 syncpt, u32 base,
		struct nvhost_channel *ch);


int nvhost_vic03_prepare_poweroff(struct platform_device *);
int nvhost_vic03_finalize_poweron(struct platform_device *);
void nvhost_vic03_busy(struct platform_device *);
void nvhost_vic03_idle(struct platform_device *);
void nvhost_vic03_suspend(struct platform_device *);
int nvhost_vic03_init(struct platform_device *);
void nvhost_vic03_deinit(struct platform_device *);



/* hack, get these from elsewhere */
#define NVA0B6_VIDEO_COMPOSITOR_SET_APPLICATION_ID                                               (0x00000200)
#define NVA0B6_VIDEO_COMPOSITOR_SET_FCE_UCODE_SIZE                                               (0x0000071C)
#define NVA0B6_VIDEO_COMPOSITOR_SET_FCE_UCODE_OFFSET                                             (0x0000072C)
#define VIC_UCLASS_METHOD_OFFSET 0x10

#define NVHOST_ENCODE_VIC_VER(maj, min) \
	((((maj) & 0xff) << 8) | ((min) & 0xff))

static inline void decode_vic_ver(int version, u8 *maj, u8 *min)
{
	u32 uv32 = (u32)version;
	*maj = (u8)((uv32 >> 8) & 0xff);
	*min = (u8)(uv32 & 0xff);
}

#endif /* __NVHOST_VIC_H__ */

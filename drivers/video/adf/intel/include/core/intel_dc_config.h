/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef INTEL_DC_CONFIG_H_
#define INTEL_DC_CONFIG_H_

#include <linux/kernel.h>
#include <linux/module.h>
#include <video/intel_adf.h>

#if defined(CONFIG_ADF)
#include <video/adf.h>
#endif

#define INTEL_DC_MAX_PLANE_COUNT	0xff
#define INTEL_DC_MAX_PIPE_COUNT		0xff

struct intel_dc_component;
struct intel_plane;
struct intel_pipe;
struct intel_dc_memory;
struct intel_dc_power_ops;

enum intel_plane_blending {
	INTEL_PLANE_BLENDING_NONE,
	INTEL_PLANE_BLENDING_PREMULT,
	INTEL_PLANE_BLENDING_COVERAGE,
};

enum intel_plane_scaling {
	INTEL_PLANE_SCALING_DOWNSCALING,
	INTEL_PLANE_SCALING_UPSCALING,
};

enum intel_plane_compression {
	INTEL_PLANE_DECOMPRESSION_16X4,
};

enum intel_plane_tiling_mode {
	INTEL_PLANE_TILE_NONE,
	INTEL_PLANE_TILE_X,
	INTEL_PLANE_TILE_Y,
};

enum intel_plane_reserved_bit {
	INTEL_PLANE_RESERVED_BIT_ZERO,
	INTEL_PLANE_RESERVED_BIT_SET,
};

enum intel_plane_zorder {
	INTEL_PLANE_P1S1S2C1,
	INTEL_PLANE_P1S2S1C1,
	INTEL_PLANE_S2P1S1C1,
	INTEL_PLANE_S2S1P1C1,
	INTEL_PLANE_S1P1S2C1,
	INTEL_PLANE_S1S2P1C1,
};

struct intel_plane_capabilities {
	const u32 *supported_formats;
	const size_t n_supported_formats;

	const u32 *supported_blendings;
	const size_t n_supported_blendings;

	const u32 *supported_scalings;
	const size_t n_supported_scalings;

	const u32 *supported_transforms;
	const size_t n_supported_transforms;

	const u32 *supported_decompressions;
	const size_t n_supported_decompressions;

	const u32 *supported_tiling;
	const size_t n_supported_tiling;

	const u32 *supported_zorder;
	const size_t n_supported_zorder;

	const u32 *supported_reservedbit;
	const size_t n_supported_reservedbit;
};

struct intel_buffer {
	u32 w;
	u32 h;
	u32 format;
	unsigned long gtt_offset_in_pages;
	u32 stride;
	u32 tiling_mode;
};

struct intel_dc_buffer {
	u32 handle;
	u32 dc_mem_addr;
	struct page **pages;
	size_t n_pages;
	struct list_head list;
};

struct intel_dc_memory_ops {
	int (*import)(struct intel_dc_memory *mem, u32 handle,
		struct page **pages, size_t n_pages, u32 *addr);
	void (*free)(struct intel_dc_memory *mem, u32 handle);
};

struct intel_dc_memory {
	struct device *dev;
	size_t total_pages;
	size_t alloc_pages;
	size_t free_pages;
	const struct intel_dc_memory_ops *ops;
	struct list_head buf_list;
	size_t n_bufs;
	rwlock_t lock;
};

struct intel_dc_power {
	struct device *dev;
	const struct intel_dc_config *config;
	const struct intel_dc_power_ops *ops;
};

struct intel_dc_power_ops {
	void (*suspend)(struct intel_dc_power *power);
	void (*resume)(struct intel_dc_power *power);
};

struct intel_plane_config {
	s16 dst_x;
	s16 dst_y;
	u16 dst_w;
	u16 dst_h;
	s32 src_x;
	s32 src_y;
	u32 src_w;
	u32 src_h;
	u32 zorder;
	u8 alpha;
	enum intel_plane_compression compression:4;
	enum intel_plane_blending blending:4;
	enum intel_adf_transform transform:4;
	struct intel_pipe *pipe;
};

struct intel_dc_component_ops {
	void (*suspend)(struct intel_dc_component *component);
	void (*resume)(struct intel_dc_component *component);
};

struct intel_dc_component {
	struct device *dev;
	u8 idx;
	const char *name;
};

/**
 * intel_plane_ops - plane operations.
 * validate_custom_format: [optional]
 * validate: [required]
 * flip: [required]
 * enable: [required]
 * disable: [required]
 */
struct intel_plane_ops {
	struct intel_dc_component_ops base;
#if defined(CONFIG_ADF)
	/*FIXME: Have to put adf overlay ops*/
	const struct adf_overlay_engine_ops adf_ops;
#endif
	int (*attach)(struct intel_plane *plane, struct intel_pipe *pipe);
	int (*detach)(struct intel_plane *plane, struct intel_pipe *pipe);
	int (*validate_custom_format)(struct intel_plane *plane, u32 format,
		u32 w, u32 h);
	int (*validate)(struct intel_plane *plane,
		struct intel_buffer *buf,
		struct intel_plane_config *config);
	void (*flip)(struct intel_plane *plane,
		struct intel_buffer *buf,
		struct intel_plane_config *config);
	int (*enable)(struct intel_plane *plane);
	int (*disable)(struct intel_plane *plane);
};

struct intel_plane {
	struct intel_dc_component base;
	struct intel_pipe *pipe;
	const struct intel_plane_capabilities *caps;
	const struct intel_plane_ops *ops;
};

enum intel_pipe_type {
	INTEL_PIPE_DSI,
	INTEL_PIPE_HDMI,
};

enum intel_pipe_event {
	INTEL_PIPE_EVENT_UNKNOWN = 0x0,
	INTEL_PIPE_EVENT_VSYNC = 0x01,
	INTEL_PIPE_EVENT_HOTPLUG_CONNECTED = 0x2,
	INTEL_PIPE_EVENT_HOTPLUG_DISCONNECTED = 0x4,
	INTEL_PIPE_EVENT_AUDIO_BUFFERDONE = 0x8,
	INTEL_PIPE_EVENT_AUDIO_UNDERRUN = 0x10,
	INTEL_PIPE_EVENT_REPEATED_FRAME = 0x20,
	INTEL_PIPE_EVENT_UNDERRUN = 0x40,
	INTEL_PIPE_EVENT_SPRITE1_FLIP = 0x80,
	INTEL_PIPE_EVENT_SPRITE2_FLIP = 0x100,
	INTEL_PIPE_EVENT_PRIMARY_FLIP = 0x200,
	INTEL_PIPE_EVENT_DPST = 0x400,
};

/**
 * struct intel_pipe_ops - Intel display controller pipe operations
 *
 * @hw_init: [optional]
 * @hw_deinit: [optional]
 * @get_modelist: [required]
 * @get_preferred_mode: [required]
 * @dpms: [required]
 * @modeset: [required]
 * @get_screen_size: [required]
 * @is_screen_connected: [required]
 * @get_supported_events: [required]
 * @set_event: [required]
 * @get_events: [required]
 * @get_vsync_counter: [required]
 * @handle_events: [optional]
 */
struct intel_pipe_ops {
	struct intel_dc_component_ops base;
	int (*hw_init)(struct intel_pipe *pipe);
	void (*hw_deinit)(struct intel_pipe *pipe);

	void (*get_modelist)(struct intel_pipe *pipe,
		struct drm_mode_modeinfo **modelist, size_t *n_modes);
	void (*get_preferred_mode)(struct intel_pipe *pipe,
		struct drm_mode_modeinfo **mode);
	int (*dpms)(struct intel_pipe *pipe, u8 state);
	int (*modeset)(struct intel_pipe *pipe,
		struct drm_mode_modeinfo *mode);
	int (*get_screen_size)(struct intel_pipe *pipe,
		u16 *width_mm, u16 *height_mm);
	bool (*is_screen_connected)(struct intel_pipe *pipe);

	u32 (*get_supported_events)(struct intel_pipe *pipe);
	int (*set_event)(struct intel_pipe *pipe, u16 event, bool enabled);
	void (*get_events)(struct intel_pipe *pipe, u32 *active_events);
	u32 (*get_vsync_counter)(struct intel_pipe *pipe, u32 interval);
	void (*handle_events)(struct intel_pipe *pipe, u32 events);

	void (*pre_post)(struct intel_pipe *pipe);
	void (*on_post)(struct intel_pipe *pipe);

#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
	int (*set_brightness)(struct intel_pipe *pipe, int level);
	int (*get_brightness)(struct intel_pipe *pipe);
	void (*enable_backlight)(struct intel_pipe *pipe);
	void (*disable_backlight)(struct intel_pipe *pipe);
#endif
};

struct pri_plane_regs {
	u32 dspcntr;
	u32 stride;
	u32 pri_ddl;
	u32 pri_ddl_mask;
	u32 sp1_ddl;
	u32 sp1_ddl_mask;
	u32 sp2_ddl;
	u32 sp2_ddl_mask;
	unsigned long linearoff;
	unsigned long tileoff;
	unsigned long surfaddr;
};

struct intel_pipe {
	struct intel_dc_component base;
	bool primary;
	enum intel_pipe_type type;
	const struct intel_plane *primary_plane;
	const struct intel_pipe_ops *ops;
	bool dpst_enabled;

	/*
	 * Store the computed reg values in this to apply in
	 * one shot later in flip calls
	 */
	struct pri_plane_regs regs;
};

struct intel_dc_attachment {
	u8 plane_id;
	u8 pipe_id;
};

struct intel_dc_config {
	struct device *dev;

	/*display controller unique ID*/
	u32 id;

	struct intel_plane **planes;
	u8 n_planes;

	struct intel_pipe **pipes;
	u8 n_pipes;

	const struct intel_dc_attachment *allowed_attachments;
	u32 n_allowed_attachments;

	struct intel_dc_memory *memory;

	struct intel_dc_power *power;
};

struct intel_dc_config_entry {
	const u32 id;
	struct intel_dc_config * (*get_dc_config)(struct pci_dev *pdev,
		u32 id);
	void (*destroy_dc_config)(struct intel_dc_config *config);
};

static inline struct intel_plane *to_intel_plane(
	struct intel_dc_component *component)
{
	return container_of(component, struct intel_plane, base);
}

static inline struct intel_pipe *to_intel_pipe(
	struct intel_dc_component *component)
{
	return container_of(component, struct intel_pipe, base);
}

extern int intel_adf_plane_init(struct intel_plane *plane, struct device *dev,
	u8 idx, const struct intel_plane_capabilities *caps,
	const struct intel_plane_ops *ops, const char *name);
extern void intel_plane_destroy(struct intel_plane *plane);


extern int intel_pipe_init(struct intel_pipe *pipe, struct device *dev,
	u8 idx, bool primary, enum intel_pipe_type type,
	const struct intel_plane *primary_plane,
	const struct intel_pipe_ops *ops, const char *name);
extern void intel_pipe_destroy(struct intel_pipe *pipe);

extern int intel_dc_memory_init(struct intel_dc_memory *mem,
	struct device *dev, size_t total_pages,
	const struct intel_dc_memory_ops *ops);
extern void intel_dc_memory_destroy(struct intel_dc_memory *mem);
extern struct intel_dc_buffer *intel_dc_memory_import(
	struct intel_dc_memory *mem, u32 handle, struct page **pages,
	size_t n_pages);
extern void intel_dc_memory_free(struct intel_dc_memory *mem,
	struct intel_dc_buffer *buf);
extern int intel_dc_memory_status(struct intel_dc_memory *mem,
	size_t *n_total, size_t *n_alloc, size_t *n_free, size_t *n_bufs);

extern int intel_dc_power_init(struct intel_dc_power *power,
	struct device *dev, const struct intel_dc_config *config,
	const struct intel_dc_power_ops *ops);
extern void intel_dc_power_destroy(struct intel_dc_power *power);
extern void intel_dc_power_suspend(struct intel_dc_power *power);
extern void intel_dc_power_resume(struct intel_dc_power *power);

extern int intel_dc_component_init(struct intel_dc_component *component,
	struct device *dev, u8 idx, const char *name);
extern void intel_dc_component_destroy(struct intel_dc_component *component);

extern void intel_dc_config_add_plane(struct intel_dc_config *config,
	struct intel_plane *plane, u8 idx);
extern void intel_dc_config_add_pipe(struct intel_dc_config *config,
	struct intel_pipe *pipe, u8 idx);
static inline void intel_dc_config_add_memory(struct intel_dc_config *config,
	struct intel_dc_memory *memory)
{
	if (config)
		config->memory = memory;
}
static inline void intel_dc_config_add_power(struct intel_dc_config *config,
	struct intel_dc_power *power)
{
	if (config)
		config->power = power;
}
extern int intel_dc_config_init(struct intel_dc_config *config,
	struct device *dev, const u32 id, size_t n_planes, size_t n_pipes,
	const struct intel_dc_attachment *allowed_attachments,
	size_t n_allowed_attachments);
extern void intel_dc_config_destroy(struct intel_dc_config *config);

extern struct intel_dc_config *intel_adf_get_dc_config(
	struct pci_dev *pdev, const u32 id);
extern void intel_adf_destroy_config(struct intel_dc_config *config);

/* Supported configs */
extern struct intel_dc_config *vlv_get_dc_config(struct pci_dev *pdev, u32 id);
extern void vlv_dc_config_destroy(struct intel_dc_config *config);

#endif /* INTEL_DC_CONFIG_H_ */

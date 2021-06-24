/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _MTK_DRM_PANEL_HELPER_H_
#define _MTK_DRM_PANEL_HELPER_H_

#include <drm/drm_modes.h>
#include <drm/drm_print.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_device.h>
#include <drm/drm_connector.h>
#include "mtk_drm_gateic.h"
#include "../mediatek/mediatek_v2/mtk_log.h"
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"

extern unsigned long long mtk_lcm_total_size;

#define MTK_LCM_NAME_LENGTH (128)
#define MTK_LCM_DEBUG_DUMP (0)

#define MTK_LCM_DATA_ALIGNMENT(data, unit) (((data) + (unit) - 1) & ~((unit) - 1))
#define DO_LCM_KZALLOC(buf, size, flag, debug) \
do { \
	if (debug == 1) \
		pr_notice("%s, %d, size:%lu, align:%lu, flag:0x%x\n", \
			__func__, __LINE__, size, MTK_LCM_DATA_ALIGNMENT(size, 4), flag); \
	buf = kzalloc(MTK_LCM_DATA_ALIGNMENT(size, 4), flag); \
	if (buf != NULL) \
		mtk_lcm_total_size += size; \
	if (debug == 1 && buf != NULL) \
		pr_notice("%s, %d, buf:0x%lx, size:%lu, align:%lu, flag:0x%x\n", \
			__func__, __LINE__, buf, size, MTK_LCM_DATA_ALIGNMENT(size, 4), flag); \
} while (0)

#define LCM_KZALLOC(buf, size, flag) DO_LCM_KZALLOC(buf, size, flag, 0)

struct mtk_lcm_params_dbi {
	unsigned int dbi_private_data;
};

struct mtk_lcm_params_dpi {
	unsigned int dpi_private_data;
};

struct mtk_lcm_params_dsi {
	unsigned int density;
	unsigned int fake_resolution[2];
	unsigned int need_fake_resolution;
	unsigned int phy_type;
	unsigned long long mode_flags;
	unsigned long long mode_flags_doze_on;
	unsigned long long mode_flags_doze_off;
	unsigned int format;
	unsigned int lanes;
	unsigned int default_mode;
	unsigned int mode_list[MTK_DSI_FPS_MODE_COUNT];
	struct drm_display_mode mode[MTK_DSI_FPS_MODE_COUNT];
	struct mtk_panel_params ext_param[MTK_DSI_FPS_MODE_COUNT];
	struct device lcm_gpio_dev;
	unsigned int lcm_pinctrl_count;
	const char **lcm_pinctrl_name;
};

struct mtk_lcm_params {
	const char *name;
	unsigned int type;
	unsigned int resolution[2];
	unsigned int physical_width;
	unsigned int physical_height;
	struct mtk_lcm_params_dbi dbi_params;
	struct mtk_lcm_params_dpi dpi_params;
	struct mtk_lcm_params_dsi dsi_params;
};

/* mtk_lcm_dcs_cmd_data
 * used to MTK_LCM_CMD_TYPE_WRITE_BUFFER, MTK_LCM_CMD_TYPE_READ_BUFFER
 * cmd: the read/write command or address
 * data: the write data buffer, or returned read buffer
 * data_len: the data buffer length
 * start_id: for read command the returned data will be saved at which index of out buffer
 */
struct mtk_lcm_dcs_cmd_data {
	u8 cmd;
	u8 *data;
	size_t data_len;
	unsigned int start_id;
};

/* mtk_lcm_gpio_data
 * used to MTK_LCM_GPIO_TYPE_MODE, MTK_LCM_GPIO_TYPE_OUT
 * gpio_id: gpio index
 * data: the settings of gpio
 */
struct mtk_lcm_gpio_data {
	u8 gpio_id;
	u8 data;
};

/* mtk_lcm_cb_id_data
 * used to MTK_LCM_CB_TYPE_xx
 * id: the input parameter index
 * buffer_data: the ddic command data
 */
struct mtk_lcm_cb_id_data {
	u8 id;
	u8 *buffer_data;
};

/* the union of lcm operation data*/
union mtk_lcm_ops_data_params {
	u8 *buffer_data;
	unsigned int util_data;
	struct mtk_lcm_dcs_cmd_data cmd_data;
	struct mtk_lcm_gpio_data gpio_data;
	struct mtk_lcm_cb_id_data cb_id_data;
	void *cust_data;
};

/* mtk_lcm_ops_data
 * used to save lcm operation cmd
 * func: MTK_LCM_FUNC_DSI/DBI/DPI
 * type: operation type from MTK_LCM_UTIL_TYPE_START to MTK_LCM_CUST_TYPE_END
 * size: the dts string length for parsing lcm operation
 * param: the parsing result of lcm operation params
 */
struct mtk_lcm_ops_data {
	unsigned int func;
	unsigned int type;
	unsigned int size;
	union mtk_lcm_ops_data_params param;
};

/* mtk_lcm_ops_dbi
 * used to save the dbi operation list
 */
struct mtk_lcm_ops_dbi {
	unsigned int dbi_private_data;
};

/* mtk_lcm_ops_dpi
 * used to save the dpi operation list
 */
struct mtk_lcm_ops_dpi {
	unsigned int dpi_private_data;
};

/* mtk_lcm_ops_dsi
 * used to save the dsi operation list
 * xx_size: the operation count of xx function
 * xx: the operation data list of xx function
 */
struct mtk_lcm_ops_dsi {
	/* panel init & deinit */
	unsigned int prepare_size;
	struct mtk_lcm_ops_data *prepare;
	unsigned int unprepare_size;
	struct mtk_lcm_ops_data *unprepare;
	unsigned int enable_size;
	struct mtk_lcm_ops_data *enable;
	unsigned int disable_size;
	struct mtk_lcm_ops_data *disable;

	/* panel backlight update*/
	unsigned int set_backlight_cmdq_size;
	struct mtk_lcm_ops_data *set_backlight_cmdq;
	unsigned int set_backlight_grp_cmdq_size;
	struct mtk_lcm_ops_data *set_backlight_grp_cmdq;

	unsigned int set_aod_light_high_size;
	struct mtk_lcm_ops_data *set_aod_light_high;
	unsigned int set_aod_light_low_size;
	struct mtk_lcm_ops_data *set_aod_light_low;

	/* panel aod check*/
	unsigned int ata_check_size;
	unsigned int ata_id_value_length;
	u8 *ata_id_value_data;
	struct mtk_lcm_ops_data *ata_check;

#ifdef MTK_PANEL_SUPPORT_COMPARE_ID
	/* panel compare id check*/
	unsigned int compare_id_size;
	unsigned int compare_id_value_length;
	u8 *compare_id_value_data;
	struct mtk_lcm_ops_data *compare_id;
#endif

	/* doze feature support*/
	unsigned int doze_enable_start_size;
	struct mtk_lcm_ops_data *doze_enable_start;
	unsigned int doze_enable_size;
	struct mtk_lcm_ops_data *doze_enable;
	unsigned int doze_disable_size;
	struct mtk_lcm_ops_data *doze_disable;
	unsigned int doze_area_size;
	struct mtk_lcm_ops_data *doze_area;
	unsigned int doze_post_disp_on_size;
	struct mtk_lcm_ops_data *doze_post_disp_on;

	/* hight backligth mode feature support*/
	unsigned int hbm_set_cmdq_size;
	unsigned int hbm_set_cmdq_switch_on;
	unsigned int hbm_set_cmdq_switch_off;
	struct mtk_lcm_ops_data *hbm_set_cmdq;

	/* high frame rate feature support*/
	unsigned int fps_switch_bfoff_size[MTK_DSI_FPS_MODE_COUNT];
	unsigned int fps_switch_afon_size[MTK_DSI_FPS_MODE_COUNT];
	struct mtk_lcm_ops_data *fps_switch_bfoff[MTK_DSI_FPS_MODE_COUNT];
	struct mtk_lcm_ops_data *fps_switch_afon[MTK_DSI_FPS_MODE_COUNT];

	unsigned int gpio_test_size;
	struct mtk_lcm_ops_data *gpio_test;
};

struct mtk_lcm_ops {
	struct mtk_lcm_ops_dbi *dbi_ops;
	struct mtk_lcm_ops_dpi *dpi_ops;
	struct mtk_lcm_ops_dsi *dsi_ops;
};

/* customization callback of private panel operation
 * parse_params:
 *      used to save panel parameters parsed from dtsi
 * parse_ops:
 *      used to save panel operation cmd list parsed from dtsi
 * func:
 *      used to execute the customized operation cmd
 * dump:
 *      used to dump the customized settings in params (optional)
 */
struct mtk_panel_cust {
	atomic_t cust_enabled;
	int (*parse_params)(struct device_node *np);
	int (*parse_ops)(unsigned int func,
		int type, u8 *data_in, size_t size_in,
		void *cust_data);
	int (*func)(struct mtk_lcm_ops_data *op,
		u8 *data, size_t size);
	void (*dump_params)(void);
	void (*dump_ops)(struct mtk_lcm_ops_data *op,
		const char *owner, unsigned int id);
};

struct mtk_panel_resource {
	struct mtk_lcm_params params;
	struct mtk_lcm_ops ops;
	struct mtk_panel_cust cust;
};

int load_panel_resource_from_dts(struct device_node *lcm_np,
		struct mtk_panel_resource *data);

int parse_lcm_ops_func(struct device_node *np,
		struct mtk_lcm_ops_data *table,
		char *func, unsigned int size,
		unsigned int panel_type,
		struct mtk_panel_cust *cust, unsigned int phase);

/* function: execute lcm operations
 * input: table: lcm operation list
 *        table_size: lcm operation count
 *        panel_resource: lcm parameters
 *        data: the private data buffer of lcm operation
 *        size: the private data buffer size of lcm operation
 *        owner: the owner description
 * output: 0 for success, else of failed
 */
int mtk_panel_execute_operation(void *dev,
		struct mtk_lcm_ops_data *table, unsigned int table_size,
		const struct mtk_panel_resource *panel_resource,
		void *data, size_t size, char *owner);

void mtk_lcm_dts_read_u32(struct device_node *np, char *prop,
		u32 *out);
int mtk_lcm_dts_read_u32_array(struct device_node *np, char *prop,
		u32 *out, int min_len, int max_len);
void mtk_lcm_dts_read_u8(struct device_node *np, char *prop,
		u8 *out);
int mtk_lcm_dts_read_u8_array(struct device_node *np, char *prop,
		u8 *out, int min_len, int max_len);

/* function: parse lcm parameters
 * input: fdt: dts, nodeoffset: dts node,
 *        params:the returned parsing result
 * output: 0 for success, else of failed
 */
int parse_lcm_params_dbi(struct device_node *np,
		struct mtk_lcm_params_dbi *params);
int parse_lcm_params_dpi(struct device_node *np,
		struct mtk_lcm_params_dpi *params);
int parse_lcm_params_dsi(struct device_node *np,
		struct mtk_lcm_params_dsi *params);

/* function: parse lcm operations
 * input: fdt: dts, nodeoffset: dts node,
 *        ops:the returned parsing result
 *        params: the lcm parameters already parsed
 * output: 0 for success, else of failed
 */
int parse_lcm_ops_dbi(struct device_node *np,
		struct mtk_lcm_ops_dbi *ops,
		struct mtk_lcm_params_dbi *params,
		struct mtk_panel_cust *cust);
int parse_lcm_ops_dpi(struct device_node *np,
		struct mtk_lcm_ops_dpi *ops,
		struct mtk_lcm_params_dpi *params,
		struct mtk_panel_cust *cust);
int parse_lcm_ops_dsi(struct device_node *np,
		struct mtk_lcm_ops_dsi *ops,
		struct mtk_lcm_params_dsi *params,
		struct mtk_panel_cust *cust);

void free_lcm_params_dbi(struct mtk_lcm_params_dbi *params);
void free_lcm_params_dpi(struct mtk_lcm_params_dpi *params);
void free_lcm_params_dsi(struct mtk_lcm_params_dsi *params);
void free_lcm_ops_dbi(struct mtk_lcm_ops_dbi *ops);
void free_lcm_ops_dpi(struct mtk_lcm_ops_dpi *ops);
void free_lcm_ops_dsi(struct mtk_lcm_ops_dsi *ops);

/* function: dump dts settings of lcm driver*/
void dump_lcm_ops_func(struct mtk_lcm_ops_data *table,
		unsigned int size,
		struct mtk_panel_cust *cust,
		const char *owner);
void dump_lcm_dsi_fps_settings(struct mtk_lcm_params_dsi *params, int id);
void dump_lcm_params_basic(struct mtk_lcm_params *params);
void dump_lcm_params_dsi(struct mtk_lcm_params_dsi *params,
		struct mtk_panel_cust *cust);
void dump_lcm_ops_dsi(struct mtk_lcm_ops_dsi *ops,
		struct mtk_lcm_params_dsi *params,
		struct mtk_panel_cust *cust);
void dump_lcm_params_dbi(struct mtk_lcm_params_dbi *params,
		struct mtk_panel_cust *cust);
void dump_lcm_ops_dbi(struct mtk_lcm_ops_dbi *ops,
		struct mtk_lcm_params_dbi *params,
		struct mtk_panel_cust *cust);
void dump_lcm_params_dpi(struct mtk_lcm_params_dpi *params,
		struct mtk_panel_cust *cust);
void dump_lcm_ops_dpi(struct mtk_lcm_ops_dpi *ops,
		struct mtk_lcm_params_dpi *params,
		struct mtk_panel_cust *cust);

void mtk_lcm_dump_all(char func, struct mtk_panel_resource *resource,
		struct mtk_panel_cust *cust);

/* function: dsi ddic write
 * input: data: the data buffer
 * output: 0 for success, else for failed
 */
int mtk_panel_dsi_dcs_write_buffer(struct mipi_dsi_device *dsi,
		const void *data, size_t len);
int mtk_panel_dsi_dcs_write(struct mipi_dsi_device *dsi,
		u8 cmd, void *data, size_t len);
int mtk_panel_dsi_dcs_read(struct mipi_dsi_device *dsi,
		u8 cmd, void *data, size_t len);

/* function: dsi ddic write
 * input: data: the returned data buffer
 * output: 0 for success, else for failed
 */
int mtk_panel_dsi_dcs_read_buffer(struct mipi_dsi_device *dsi,
		const void *data_in, size_t len_in,
		void *data_out, size_t len_out);

#endif

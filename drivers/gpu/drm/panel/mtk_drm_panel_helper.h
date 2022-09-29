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
#include <video/mipi_display.h>
#include "mtk_drm_gateic.h"
#include "../mediatek/mediatek_v2/mtk_log.h"
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#include "mtk_round_corner/mtk_drm_rc.h"

extern unsigned long long mtk_lcm_total_size;

#define MTK_LCM_MODE_UNIT   (4)
#define MTK_LCM_DEBUG_DUMP  (0)
#define MTK_LCM_DATA_OFFSET (2)

/* mtk_lcm_ops_table
 * used to store the lcm operation commands
 * list: operation command list
 * size: operation command count
 */
struct mtk_lcm_ops_table {
	struct list_head list;
	unsigned int size;
};

struct mtk_lcm_params_dbi {
	unsigned int dbi_private_data;
};

struct mtk_lcm_params_dpi {
	unsigned int dpi_private_data;
};

struct mtk_lcm_mode_dsi {
/* key word */
	unsigned int id;
	unsigned int width;
	unsigned int height;
	unsigned int fps;
	unsigned int voltage;
	unsigned int fake;
	struct list_head list;
/* params */
	struct drm_display_mode mode;
	struct mtk_panel_params ext_param;
	int msync_set_min_fps_list_length;
	unsigned int *msync_set_min_fps_list;
	struct mtk_lcm_ops_table msync_set_min_fps;
/* ops */
	struct mtk_lcm_ops_table msync_switch_mte;
	struct mtk_lcm_ops_table fps_switch_bfoff;
	struct mtk_lcm_ops_table fps_switch_afon;
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
	struct mtk_lcm_mode_dsi *default_mode;
	unsigned int mode_count;
	struct list_head mode_list;
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

/* mtk_lcm_buf_data
 * used to MTK_LCM_CMD_TYPE_WRITE_BUFFER
 * data: the write data buffer
 * data_len: the data buffer length for write
 * flag: write operation customization flag
 */
struct mtk_lcm_buf_data {
	u8 *data;
	size_t data_len;
	u32 flag;
};

/* mtk_lcm_dcs_cmd_data
 * used to MTK_LCM_CMD_TYPE_WRITE_CMD, MTK_LCM_CMD_TYPE_READ_BUFFER/CMD
 * cmd: the read/write command or address
 * data: the write data buffer, or returned read buffer
 * data_len: the data buffer length of tx
 * rx_len: the data buffer length of rx
 * start_id: for read command the returned data will be saved at which index of out buffer
 * flag: read/write operation customization flag
 */
struct mtk_lcm_dcs_cmd_data {
	u8 cmd;
	u8 *tx_data;
	size_t tx_len;
	size_t rx_len;
	unsigned int rx_off;
	u32 flag;
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

/* mtk_lcm_buf_con_data
 * used to MTK_LCM_CMD_TYPE_WRITE_BUFFER_CONDITION
 * condition: the execution condition
 * name: the input data name
 * data: the ddic command data
 * data_len: the ddic command data count
 * flag: write operation customization flag
 */
struct mtk_lcm_buf_con_data {
	u8 name;
	u8 condition;
	u8 *data;
	size_t data_len;
	u32 flag;
};

/* mtk_lcm_buf_runtime_data
 * used to MTK_LCM_CMD_TYPE_WRITE_BUFFER_RUNTIME_INPUT
 * id: the parameter index of runtime input
 * id_len: the parameter count of runtime input
 * name: the input data name
 * data: the ddic command data
 * data_len: the ddic command data count
 * flag: write operation customization flag
 */
struct mtk_lcm_buf_runtime_data {
	u8 name;
	u8 *id;
	size_t id_len;
	u8 *data;
	size_t data_len;
	u32 flag;
};

/* the union of lcm operation data*/
union mtk_lcm_ops_data_params {
	unsigned int util_data;
	struct mtk_lcm_gpio_data gpio_data;
	struct mtk_lcm_dcs_cmd_data cmd_data;
	struct mtk_lcm_buf_data buf_data;
	struct mtk_lcm_buf_con_data buf_con_data;
	struct mtk_lcm_buf_runtime_data buf_runtime_data;
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
	struct list_head node;
	unsigned int func;
	unsigned int type;
	unsigned int size;
	union mtk_lcm_ops_data_params param;
};

/* mtk_lcm_ops_dbi
 * used to save the dbi operation list
 */
struct mtk_lcm_ops_dbi {
	unsigned int flag_len;
	unsigned int dbi_private_data;
};

/* mtk_lcm_ops_dpi
 * used to save the dpi operation list
 */
struct mtk_lcm_ops_dpi {
	unsigned int flag_len;
	unsigned int dpi_private_data;
};

/* mtk_lcm_ops_dsi
 * used to save the dsi operation list
 * xx: the operation data list of xx function
 */
struct mtk_lcm_ops_dsi {
	unsigned int flag_len;
	/* panel init & deinit */
	struct mtk_lcm_ops_table prepare;
	struct mtk_lcm_ops_table unprepare;
	struct mtk_lcm_ops_table enable;
	struct mtk_lcm_ops_table disable;

	/* panel backlight update*/
	unsigned int set_backlight_mask;
	struct mtk_lcm_ops_table set_backlight_cmdq;
	struct mtk_lcm_ops_table set_elvss_cmdq;
	struct mtk_lcm_ops_table set_backlight_elvss_cmdq;

	unsigned int set_aod_light_mask;
	struct mtk_lcm_ops_table set_aod_light;

	/* panel ata check*/
	unsigned int ata_id_value_length;
	u8 *ata_id_value_data;
	struct mtk_lcm_ops_table ata_check;

	/* panel aod mode check*/
	unsigned int aod_mode_value_length;
	u8 *aod_mode_value_data;
	struct mtk_lcm_ops_table aod_mode_check;

#ifdef MTK_PANEL_SUPPORT_COMPARE_ID
	/* panel compare id check*/
	unsigned int compare_id_value_length;
	u8 *compare_id_value_data;
	struct mtk_lcm_ops_table compare_id;
#endif

	/* doze feature support*/
	struct mtk_lcm_ops_table doze_enable_start;
	struct mtk_lcm_ops_table doze_enable;
	struct mtk_lcm_ops_table doze_disable;
	struct mtk_lcm_ops_table doze_area;
	struct mtk_lcm_ops_table doze_post_disp_on;

	/* hight backligth mode feature support*/
	unsigned int hbm_set_cmdq_switch_on;
	unsigned int hbm_set_cmdq_switch_off;
	struct mtk_lcm_ops_table hbm_set_cmdq;

	/* msync set min fps support*/
	struct mtk_lcm_ops_table msync_default_mte;
	struct mtk_lcm_ops_table msync_request_mte;
	struct mtk_lcm_ops_table default_msync_close_mte;

	unsigned int read_panelid_len;
	struct mtk_lcm_ops_table read_panelid;

	unsigned int msync_switch_mte_mode_count;
	u8 *msync_switch_mte_mode;
	struct mtk_lcm_ops_table default_msync_switch_mte;

	unsigned int fps_switch_afon_mode_count;
	u8 *fps_switch_afon_mode;
	struct mtk_lcm_ops_table default_fps_switch_afon;

	unsigned int fps_switch_bfoff_mode_count;
	u8 *fps_switch_bfoff_mode;
	struct mtk_lcm_ops_table default_fps_switch_bfoff;

#if MTK_LCM_DEBUG_DUMP
	struct mtk_lcm_ops_table gpio_test;
#endif
};

struct mtk_lcm_ops {
	struct mtk_lcm_ops_dbi *dbi_ops;
	struct mtk_lcm_ops_dpi *dpi_ops;
	struct mtk_lcm_ops_dsi *dsi_ops;
};

struct mtk_lcm_ops_input {
	u8 name;
	unsigned int length;
	void *data;
};

struct mtk_lcm_ops_input_packet {
	unsigned int data_count;
	unsigned int condition_count;
	struct mtk_lcm_ops_input *data;
	struct mtk_lcm_ops_input *condition;
};

/* customization callback of private panel operation
 * funcs:
 *      the private drm panel funcs used to replace common drm funcs
 * ext_funcs:
 *      the private ext panel funcs used to replace common ext funcs
 * cust_funcs:
 *      the private panel funcs customized by customer
 * parse_params:
 *      used to save panel parameters parsed from dtsi
 * parse_ops:
 *      used to save panel operation cmd list parsed from dtsi
 * execute_ops:
 *      used to execute the customized operation cmd
 * dump_params:
 *      used to dump the customized settings in customer params
 * dump_ops:
 *      used to dump the customized settings in customer ops
 * free_params:
 *      used to deallocate the memory buffer of customer params
 * free_ops:
 *      used to deallocate the memory buffer of customer ops
 */
struct mtk_panel_cust {
	struct drm_panel_funcs funcs;
	struct mtk_panel_funcs ext_funcs;
	int (*cust_funcs)(struct drm_panel *panel,
		int cmd, void *params, void *handle, void **output);
	int (*parse_params)(struct device_node *np);
	int (*parse_ops_table)(struct device_node *np,
		unsigned int flag_len);
	int (*parse_ops)(struct mtk_lcm_ops_data *lcm_op,
		u8 *dts, unsigned int flag_len);
	int (*execute_ops)(struct mtk_lcm_ops_data *op,
		struct mtk_lcm_ops_input_packet *input);
	void (*dump_params)(void);
	void (*dump_ops_table)(const char *owner, char func);
	void (*dump_ops)(struct mtk_lcm_ops_data *op,
		const char *owner, unsigned int id);
	void (*free_params)(unsigned int func);
	void (*free_ops_table)(void);
	void (*free_ops)(struct mtk_lcm_ops_data *op);
};

struct mtk_panel_resource {
	unsigned int version;
	struct mtk_lcm_params params;
	struct mtk_lcm_ops ops;
	const struct mtk_panel_cust *cust;
};

int load_panel_resource_from_dts(struct device_node *lcm_np,
		struct mtk_panel_resource *data);

/* function: parse lcm operation table in dts settings
 * input: np: the dts node
 *        table: lcm operation list
 *        func: lcm operation name
 *        flag_len: the ddic flag length
 *        panel_type: lcm type of DSI/DPI/DBI
 *        cust: customized operation
 *        phase: parsing phase of KERNEL/LK
 * output: lcm operation count
 */
int parse_lcm_ops_func(struct device_node *np,
		struct mtk_lcm_ops_table *table, char *func,
		unsigned int flag_len, unsigned int panel_type,
		const struct mtk_panel_cust *cust, unsigned int phase);

/* function: parse the common lcm operation table
 *        shared by different conditions in dts settings,
 *        u8/u32 is the data type of list array.
 * input: np: the dts node
 *        list: the condition list
 *        list_len: the condition count
 *        list_name: the condition list name
 *        table: lcm operation list
 *        func: lcm operation name
 *        flag_len: the ddic flag length
 *        panel_type: lcm type of DSI/DPI/DBI
 *        cust: customized operation
 *        phase: parsing phase of KERNEL/LK
 * output: 0 for pass, else failed
 */
int parse_lcm_common_ops_func_u8(struct device_node *np,
		u8 **list, unsigned int *list_len, char *list_name,
		struct mtk_lcm_ops_table *table, char *func,
		unsigned int flag_len, unsigned int panel_type,
		const struct mtk_panel_cust *cust, unsigned int phase);

int parse_lcm_common_ops_func_u32(struct device_node *np,
		u32 **list, unsigned int *list_len, char *list_name,
		struct mtk_lcm_ops_table *table, char *func,
		unsigned int flag_len, unsigned int panel_type,
		const struct mtk_panel_cust *cust, unsigned int phase);

/* function: execute lcm operations
 * input: dev: the target dsi device
 *        table: lcm operation list
 *        panel_resource: lcm parameters
 *        input: the runtime input data or read back data buffer
 *        handler: the cmdq handler
 *        handler_cb: you can customize the ddic callback function
 *                 or just use the default callback function with NULL
 *        owner: the owner description
 * output: 0 for success, else of failed
 */
int mtk_panel_execute_operation(struct mipi_dsi_device *dev,
		struct mtk_lcm_ops_table *table,
		const struct mtk_panel_resource *panel_resource,
		struct mtk_lcm_ops_input_packet *input,
		void *handle, mtk_dsi_ddic_handler_cb handler_cb,
		unsigned int prop, const char *owner);

/* function: execute lcm operations with callback
 * input: dsi: the dsi structure of callback function
 *        cb: callback function
 *        handle: cmdq handler
 *        table: lcm operation list
 *        input: the runtime input data package
 *        master: the owner description
 * output: 0 for success, else of failed
 */
int mtk_panel_execute_callback(void *dsi, dcs_write_gce cb,
	void *handle, struct mtk_lcm_ops_table *table,
	struct mtk_lcm_ops_input_packet *input, const char *master);

/* function: execute lcm operations with group callback
 * input: dsi: the dsi structure of callback function
 *        cb: callback function
 *        handle: cmdq handler
 *        table: lcm operation list
 *        input: the runtime input data package
 *        master: the owner description
 * output: 0 for success, else of failed
 */
int mtk_panel_execute_callback_group(void *dsi, dcs_grp_write_gce cb,
	void *handle, struct mtk_lcm_ops_table *table,
	struct mtk_lcm_ops_input_packet *input, const char *master);

void mtk_lcm_dts_read_u32(struct device_node *np, char *prop,
		u32 *out);
int mtk_lcm_dts_read_u32_array(struct device_node *np, char *prop,
		u32 *out, int min_len, int max_len);
void mtk_lcm_dts_read_u8(struct device_node *np, char *prop,
		u8 *out);
int mtk_lcm_dts_read_u8_array(struct device_node *np, char *prop,
		u8 *out, int min_len, int max_len);
int mtk_lcm_dts_read_u8_array_from_u32(struct device_node *np, char *prop,
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
		const struct mtk_panel_cust *cust);
int parse_lcm_ops_dpi(struct device_node *np,
		struct mtk_lcm_ops_dpi *ops,
		struct mtk_lcm_params_dpi *params,
		const struct mtk_panel_cust *cust);
int parse_lcm_ops_dsi(struct device_node *np,
		struct mtk_lcm_ops_dsi *ops,
		struct mtk_lcm_params_dsi *params,
		const struct mtk_panel_cust *cust);

void free_lcm_params_dbi(struct mtk_lcm_params_dbi *params,
	const struct mtk_panel_cust *cust);
void free_lcm_params_dpi(struct mtk_lcm_params_dpi *params,
	const struct mtk_panel_cust *cust);
void free_lcm_params_dsi(struct mtk_lcm_params_dsi *params,
	const struct mtk_panel_cust *cust);
void free_lcm_ops_dbi(struct mtk_lcm_ops_dbi *ops,
	const struct mtk_panel_cust *cust);
void free_lcm_ops_dpi(struct mtk_lcm_ops_dpi *ops,
	const struct mtk_panel_cust *cust);
void free_lcm_ops_dsi(struct mtk_lcm_ops_dsi *ops,
	const struct mtk_panel_cust *cust);

/* function: dump ops data*/
int dump_lcm_ops_func(struct mtk_lcm_ops_data *lcm_op,
		const struct mtk_panel_cust *cust, unsigned int id, const char *owner);

/* function: dump ops table*/
void dump_lcm_ops_table(struct mtk_lcm_ops_table *table,
		const struct mtk_panel_cust *cust,
		const char *owner);
void dump_lcm_dsi_fps_settings(struct mtk_lcm_mode_dsi *mode);
void dump_lcm_params_basic(struct mtk_lcm_params *params);
void dump_lcm_params_dsi(struct mtk_lcm_params_dsi *params,
		const struct mtk_panel_cust *cust);
void dump_lcm_ops_dsi(struct mtk_lcm_ops_dsi *ops,
		struct mtk_lcm_params_dsi *params,
		const struct mtk_panel_cust *cust);
void dump_lcm_params_dbi(struct mtk_lcm_params_dbi *params,
		const struct mtk_panel_cust *cust);
void dump_lcm_ops_dbi(struct mtk_lcm_ops_dbi *ops,
		struct mtk_lcm_params_dbi *params,
		const struct mtk_panel_cust *cust);
void dump_lcm_params_dpi(struct mtk_lcm_params_dpi *params,
		const struct mtk_panel_cust *cust);
void dump_lcm_ops_dpi(struct mtk_lcm_ops_dpi *ops,
		struct mtk_lcm_params_dpi *params,
		const struct mtk_panel_cust *cust);
void mtk_lcm_dump_all(char func, struct mtk_panel_resource *resource);

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

/* function: free lcm operation data
 * input: operation cmd list, and size
 */
void free_lcm_ops_table(struct mtk_lcm_ops_table *table,
	const struct mtk_panel_cust *cust);
void free_lcm_resource(char func, struct mtk_panel_resource *data);

/* function: create an input package
 * input: the input package address, the data count and condition count
 * output: 0 for success, else for failed
 */
int mtk_lcm_create_input_packet(struct mtk_lcm_ops_input_packet *input,
		unsigned int data_count, unsigned int condition_count);

/* function: destroy an input package
 * input: the input package address, the data count and condition count
 */
void mtk_lcm_destroy_input_packet(struct mtk_lcm_ops_input_packet *input);

/* function: create an input data
 * input: the input data address and data length
 *       name: the input data name
 * output: 0 for success, else for failed
 */
int mtk_lcm_create_input(struct mtk_lcm_ops_input *input,
		unsigned int data_len, u8 name);

/* function: destroy an input data
 * input: the input package address and data length
 */
void mtk_lcm_destroy_input(struct mtk_lcm_ops_input *input);

/* function: destroy a ddic cmd packet
 * input: the ddic packet address
 */
void mtk_lcm_destroy_ddic_packet(struct mtk_lcm_dsi_cmd_packet *packet);
#endif

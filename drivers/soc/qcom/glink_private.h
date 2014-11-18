/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#ifndef _SOC_QCOM_GLINK_PRIVATE_H_
#define _SOC_QCOM_GLINK_PRIVATE_H_

/* Logging Macros */
enum {
	QCOM_GLINK_INFO = 1U << 0,
	QCOM_GLINK_DEBUG = 1U << 1,
	QCOM_GLINK_GPIO = 1U << 2,
	QCOM_GLINK_PERF = 1U << 3,
};

enum glink_dbgfs_ss {
	GLINK_DBGFS_MPSS,
	GLINK_DBGFS_APSS,
	GLINK_DBGFS_LPASS,
	GLINK_DBGFS_DSPS,
	GLINK_DBGFS_RPM,
	GLINK_DBGFS_WCNSS,
	GLINK_DBGFS_LLOOP,
	GLINK_DBGFS_MOCK,
	GLINK_DBGFS_MAX_NUM_SUBS
};

enum glink_dbgfs_xprt {
	GLINK_DBGFS_SMEM,
	GLINK_DBGFS_SMD,
	GLINK_DBGFS_XLLOOP,
	GLINK_DBGFS_XMOCK,
	GLINK_DBGFS_MAX_NUM_XPRTS
};

struct glink_dbgfs {
	const char *curr_name;
	const char *par_name;
	bool b_dir_create;
};

struct glink_dbgfs_data {
	struct list_head flist;
	struct dentry *dent;
	void (*o_func)(struct seq_file *s);
	void *priv_data;
	bool b_priv_free_req;
};

struct xprt_ctx_iterator {
	struct list_head *xprt_list;
	struct glink_core_xprt_ctx *i_curr;
	unsigned long xprt_list_flags;
};

struct ch_ctx_iterator {
	struct list_head *ch_list;
	struct channel_ctx *i_curr;
	unsigned long ch_list_flags;
};

struct glink_ch_intent_info {
	spinlock_t *li_lst_lock;
	struct list_head *li_avail_list;
	struct list_head *li_used_list;
	spinlock_t *ri_lst_lock;
	struct list_head *ri_list;
};

struct glink_core_xprt_ctx;
struct channel_ctx;
enum transport_state_e;
enum local_channel_state_e;

/**
 * glink_get_ss_enum_string() - get the name of the subsystem based on enum value
 * @enum_id:	enum id of a specific subsystem.
 *
 * Return: name of the subsystem, NULL in case of invalid input
 */
const char *glink_get_ss_enum_string(unsigned int enum_id);

/**
 * glink_get_xprt_enum_string() - get the name of the transport based on enum value
 * @enum_id:	enum id of a specific transport.
 *
 * Return: name of the transport, NULL in case of invalid input
 */
const char *glink_get_xprt_enum_string(unsigned int enum_id);

/**
 * glink_get_xprt_state_string() - get the name of the transport based on enum value
 * @enum_id:	enum id of the state of the transport.
 *
 * Return: name of the transport state, NULL in case of invalid input
 */
const char *glink_get_xprt_state_string(enum transport_state_e enum_id);

/**
 * glink_get_ch_state_string() - get the name of the transport based on enum value
 * @enum_id:	enum id of a specific state of the channel.
 *
 * Return: name of the channel state, NULL in case of invalid input
 */
const char *glink_get_ch_state_string(enum local_channel_state_e enum_id);

#define GLINK_IPC_LOG_STR(x...) do { \
	if (glink_get_log_ctx()) \
		ipc_log_string(glink_get_log_ctx(), x); \
} while (0)

#define GLINK_DBG(x...) do {                              \
	if (glink_get_debug_mask() & QCOM_GLINK_DEBUG) \
			GLINK_IPC_LOG_STR(x);  \
} while (0)

#define GLINK_INFO(x...) do {                              \
	if (glink_get_debug_mask() & QCOM_GLINK_INFO) \
			GLINK_IPC_LOG_STR(x);  \
} while (0)

#define GLINK_INFO_PERF(x...) do {                              \
	if (glink_get_debug_mask() & (QCOM_GLINK_INFO | QCOM_GLINK_PERF)) \
			GLINK_IPC_LOG_STR(x);  \
} while (0)

#define GLINK_PERF(x...) do {                              \
	if (glink_get_debug_mask() & QCOM_GLINK_PERF) \
			GLINK_IPC_LOG_STR("<PERF> " x);  \
} while (0)

#define GLINK_UT_ERR(x...) do {                              \
	if (!(glink_get_debug_mask() & QCOM_GLINK_PERF)) \
		pr_err("<UT> " x); \
	GLINK_IPC_LOG_STR("<UT> " x);  \
} while (0)

#define GLINK_UT_DBG(x...) do {                              \
	if (glink_get_debug_mask() & QCOM_GLINK_DEBUG) \
			GLINK_IPC_LOG_STR("<UT> " x);  \
} while (0)

#define GLINK_UT_INFO(x...) do {                              \
	if (glink_get_debug_mask() & QCOM_GLINK_INFO) \
			GLINK_IPC_LOG_STR("<UT> " x);  \
} while (0)

#define GLINK_UT_INFO_PERF(x...) do {                              \
	if (glink_get_debug_mask() & (QCOM_GLINK_INFO | QCOM_GLINK_PERF)) \
			GLINK_IPC_LOG_STR("<UT> " x);  \
} while (0)

#define GLINK_UT_PERF(x...) do {                              \
	if (glink_get_debug_mask() & QCOM_GLINK_PERF) \
			GLINK_IPC_LOG_STR("<PERF> " x);  \
} while (0)

#define GLINK_PERF_XPRT(xprt, fmt, args...) do { \
	if (glink_get_debug_mask() & QCOM_GLINK_PERF) \
			GLINK_IPC_LOG_STR("<PERF> %s:%s " fmt, \
					xprt->name, xprt->edge, args);  \
} while (0)

#define GLINK_PERF_CH(ctx, fmt, args...) do { \
	if (glink_get_debug_mask() & QCOM_GLINK_PERF) \
			GLINK_IPC_LOG_STR("<PERF> %s:%s:%s[%u:%u] " fmt, \
					ctx->transport_ptr->name, \
					ctx->transport_ptr->edge, \
					ctx->name, \
					ctx->lcid, \
					ctx->rcid, args);  \
} while (0)

#define GLINK_PERF_CH_XPRT(ctx, xprt, fmt, args...) do { \
	if (glink_get_debug_mask() & QCOM_GLINK_PERF) \
			GLINK_IPC_LOG_STR("<PERF> %s:%s:%s[%u:%u] " fmt, \
					xprt->name, \
					xprt->edge, \
					ctx->name, \
					ctx->lcid, \
					ctx->rcid, args);  \
} while (0)

#define GLINK_INFO_PERF_XPRT(xprt, fmt, args...) do { \
	if (glink_get_debug_mask() & (QCOM_GLINK_INFO | QCOM_GLINK_PERF)) \
			GLINK_IPC_LOG_STR("<CORE> %s:%s " fmt, \
					xprt->name, xprt->edge, args);  \
} while (0)

#define GLINK_INFO_PERF_CH(ctx, fmt, args...) do { \
	if (glink_get_debug_mask() & (QCOM_GLINK_INFO | QCOM_GLINK_PERF)) \
			GLINK_IPC_LOG_STR("<CORE> %s:%s:%s[%u:%u] " fmt, \
					ctx->transport_ptr->name, \
					ctx->transport_ptr->edge, \
					ctx->name, \
					ctx->lcid, \
					ctx->rcid, args);  \
} while (0)

#define GLINK_INFO_PERF_CH_XPRT(ctx, xprt, fmt, args...) do { \
	if (glink_get_debug_mask() & (QCOM_GLINK_INFO | QCOM_GLINK_PERF)) \
			GLINK_IPC_LOG_STR("<CORE> %s:%s:%s[%u:%u] " fmt, \
					xprt->name, \
					xprt->edge, \
					ctx->name, \
					ctx->lcid, \
					ctx->rcid, args);  \
} while (0)

#define GLINK_INFO_XPRT(xprt, fmt, args...) do { \
	if (glink_get_debug_mask() & QCOM_GLINK_INFO) \
			GLINK_IPC_LOG_STR("<CORE> %s:%s " fmt, \
					xprt->name, xprt->edge, args);  \
} while (0)

#define GLINK_INFO_CH(ctx, fmt, args...) do { \
	if (glink_get_debug_mask() & QCOM_GLINK_INFO) \
			GLINK_IPC_LOG_STR("<CORE> %s:%s:%s[%u:%u] " fmt, \
					ctx->transport_ptr->name, \
					ctx->transport_ptr->edge, \
					ctx->name, \
					ctx->lcid, \
					ctx->rcid, args);  \
} while (0)

#define GLINK_INFO_CH_XPRT(ctx, xprt, fmt, args...) do { \
	if (glink_get_debug_mask() & QCOM_GLINK_INFO) \
			GLINK_IPC_LOG_STR("<CORE> %s:%s:%s[%u:%u] " fmt, \
					xprt->name, \
					xprt->edge, \
					ctx->name, \
					ctx->lcid, \
					ctx->rcid, args);  \
} while (0)

#define GLINK_DBG_XPRT(xprt, fmt, args...) do { \
	if (glink_get_debug_mask() & QCOM_GLINK_DEBUG) \
			GLINK_IPC_LOG_STR("<CORE> %s:%s " fmt, \
					xprt->name, xprt->edge, args);  \
} while (0)

#define GLINK_DBG_CH(ctx, fmt, args...) do { \
	if (glink_get_debug_mask() & QCOM_GLINK_DEBUG) \
			GLINK_IPC_LOG_STR("<CORE> %s:%s:%s[%u:%u] " fmt, \
					ctx->transport_ptr->name, \
					ctx->transport_ptr->edge, \
					ctx->name, \
					ctx->lcid, \
					ctx->rcid, args);  \
} while (0)

#define GLINK_DBG_CH_XPRT(ctx, xprt, fmt, args...) do { \
	if (glink_get_debug_mask() & QCOM_GLINK_DEBUG) \
			GLINK_IPC_LOG_STR("<CORE> %s:%s:%s[%u:%u] " fmt, \
					xprt->name, \
					xprt->edge, \
					ctx->name, \
					ctx->lcid, \
					ctx->rcid, args);  \
} while (0)

#define GLINK_ERR(x...) do {                              \
	pr_err("<CORE> " x); \
	GLINK_IPC_LOG_STR("<CORE> " x);  \
} while (0)

#define GLINK_ERR_XPRT(xprt, fmt, args...) do { \
	pr_err("<CORE> %s:%s " fmt, \
		xprt->name, xprt->edge, args);  \
	GLINK_INFO_XPRT(xprt, fmt, args); \
} while (0)

#define GLINK_ERR_CH(ctx, fmt, args...) do { \
	pr_err("<CORE> %s:%s:%s[%u:%u] " fmt, \
		ctx->transport_ptr->name, \
		ctx->transport_ptr->edge, \
		ctx->name, \
		ctx->lcid, \
		ctx->rcid, args);  \
	GLINK_INFO_CH(ctx, fmt, args); \
} while (0)

#define GLINK_ERR_CH_XPRT(ctx, xprt, fmt, args...) do { \
	pr_err("<CORE> %s:%s:%s[%u:%u] " fmt, \
		xprt->name, \
		xprt->edge, \
		ctx->name, \
		ctx->lcid, \
		ctx->rcid, args);  \
	GLINK_INFO_CH_XPRT(ctx, xprt, fmt, args); \
} while (0)

/**
 * OVERFLOW_ADD_UNSIGNED() - check for unsigned overflow
 *
 * type:	type to check for overflow
 * a:	left value to use
 * b:	right value to use
 * returns:	true if a + b will result in overflow; false otherwise
 */
#define OVERFLOW_ADD_UNSIGNED(type, a, b) \
	(((type)~0 - (a)) < (b) ? true : false)

/**
 * glink_get_debug_mask() - Return debug mask attribute
 *
 * Return: debug mask attribute
 */
unsigned glink_get_debug_mask(void);

/**
 * glink_get_log_ctx() - Return log context for other GLINK modules.
 *
 * Return: Log context or NULL if none.
 */
void *glink_get_log_ctx(void);

/**
 * glink_get_channel_id_for_handle() - Get logical channel ID
 *
 * @handle:	handle of channel
 *
 * Used internally by G-Link debugfs.
 *
 * Return:  Logical Channel ID or standard Linux error code
 */
int glink_get_channel_id_for_handle(void *handle);

/**
 * glink_get_channel_name_for_handle() - return channel name
 *
 * @handle:	handle of channel
 *
 * Used internally by G-Link debugfs.
 *
 * Return:  Channel name or NULL
 */
char *glink_get_channel_name_for_handle(void *handle);

/**
 * glink_debugfs_init() - initialize glink debugfs directory
 *
 * Return: error code or success.
 */
int glink_debugfs_init(void);

/**
 * glink_debugfs_exit() - removes glink debugfs directory
 */
void glink_debugfs_exit(void);

/**
 * glink_debugfs_create() - create the debugfs file
 * @name:	debugfs file name
 * @show:	pointer to the actual function which will be invoked upon
 *		opening this file.
 * @dir:	pointer to a structure debugfs_dir
 * @dbgfs_data: pointer to any private data need to be associated with debugfs
 * @b_free_req: boolean value to decide to free the memory associated with
 *		@dbgfs_data during deletion of the file
 *
 * Return:	pointer to the file/directory created, NULL in case of error
 *
 * This function checks which directory will be used to create the debugfs file
 * and calls glink_dfs_create_file. Anybody who intend to allocate some memory
 * for the dbgfs_data and required to free it in deletion, need to set
 * b_free_req to true. Otherwise, there will be a memory leak.
 */
struct dentry *glink_debugfs_create(const char *name,
		void (*show)(struct seq_file *),
		struct glink_dbgfs *dir, void *dbgfs_data, bool b_free_req);

/**
 * glink_debugfs_remove_recur() - remove the the directory & files recursively
 * @rm_dfs:	pointer to the structure glink_dbgfs
 *
 * This function removes the files & directories. This also takes care of
 * freeing any memory associated with the debugfs file.
 */
void glink_debugfs_remove_recur(struct glink_dbgfs *dfs);

/**
 * glink_debugfs_add_channel() - create channel specifc files & folder in
 *				 debugfs when channel is added
 * @ch_ctx:		pointer to the channel_contenxt
 * @xprt_ctx:		pointer to the transport_context
 *
 * This function is invoked when a new channel is created. It creates the
 * folders & other files in debugfs for that channel
 */
void glink_debugfs_add_channel(struct channel_ctx *ch_ctx,
		struct glink_core_xprt_ctx *xprt_ctx);

/**
 * glink_debugfs_add_xprt() - create transport specifc files & folder in
 *			      debugfs when new transport is registerd
 * @xprt_ctx:		pointer to the transport_context
 *
 * This function is invoked when a new transport is registered. It creates the
 * folders & other files in debugfs for that transport
 */
void glink_debugfs_add_xprt(struct glink_core_xprt_ctx *xprt_ctx);

/**
 * glink_xprt_ctx_iterator_init() - Initializes the transport context list iterator
 * @xprt_i:	pointer to the transport context iterator.
 *
 * Return: None
 *
 * This function acquires the transport context lock which must then be
 * released by glink_xprt_ctx_iterator_end()
 */
void glink_xprt_ctx_iterator_init(struct xprt_ctx_iterator *xprt_i);

/**
 * glink_xprt_ctx_iterator_end() - Ends the transport context list iteration
 * @xprt_i:	pointer to the transport context iterator.
 *
 * Return: None
 */
void glink_xprt_ctx_iterator_end(struct xprt_ctx_iterator *xprt_i);

/**
 * glink_xprt_ctx_iterator_next() - iterates element by element in transport context list
 * @xprt_i:	pointer to the transport context iterator.
 *
 * Return: pointer to the transport context structure
 */
struct glink_core_xprt_ctx *glink_xprt_ctx_iterator_next(
			struct xprt_ctx_iterator *xprt_i);

/**
 * glink_get_xprt_name() - get the transport name
 * @xprt_ctx:	pointer to the transport context.
 *
 * Return: name of the transport
 */
char  *glink_get_xprt_name(struct glink_core_xprt_ctx *xprt_ctx);

/**
 * glink_get_xprt_edge_name() - get the name of the remote processor/edge
 *				of the transport
 * @xprt_ctx:	pointer to the transport context.
 *
 * Return: name of the remote processor/edge
 */
char *glink_get_xprt_edge_name(struct glink_core_xprt_ctx *xprt_ctx);

/**
 * glink_get_xprt_state() - get the state of the transport
 * @xprt_ctx:	pointer to the transport context.
 *
 * Return: name of the transport state, NULL in case of invalid input
 */
const char *glink_get_xprt_state(struct glink_core_xprt_ctx *xprt_ctx);

/**
 * glink_get_xprt_version_features() - get the version and feature set
 *					of local transport in glink
 * @xprt_ctx:	pointer to the transport context.
 *
 * Return: pointer to the glink_core_version
 */
const struct glink_core_version *glink_get_xprt_version_features(
			struct glink_core_xprt_ctx *xprt_ctx);

/**
 * glink_ch_ctx_iterator_init() - Initializes the channel context list iterator
 * @ch_iter:	pointer to the channel context iterator.
 * @xprt:       pointer to the transport context that holds the channel list
 *
 * This function acquires the channel context lock which must then be
 * released by glink_ch_ctx_iterator_end()
 */
void  glink_ch_ctx_iterator_init(struct ch_ctx_iterator *ch_iter,
			struct glink_core_xprt_ctx *xprt);

/**
 * glink_ch_ctx_iterator_end() - Ends the channel context list iteration
 * @ch_iter:	pointer to the channel context iterator.
 *
 */
void glink_ch_ctx_iterator_end(struct ch_ctx_iterator *ch_iter,
				struct glink_core_xprt_ctx *xprt);

/**
 * glink_ch_ctx_iterator_next() - iterates element by element in channel context list
 * @c_i:	pointer to the channel context iterator.
 *
 * Return: pointer to the channel context structure
 */
struct channel_ctx *glink_ch_ctx_iterator_next(struct ch_ctx_iterator *ch_iter);

/**
 * glink_get_ch_name() - get the channel name
 * @ch_ctx:	pointer to the channel context.
 *
 * Return: name of the channel, NULL in case of invalid input
 */
char *glink_get_ch_name(struct channel_ctx *ch_ctx);

/**
 * glink_get_ch_edge_name() - get the name of the remote processor/edge
 *				of the channel
 * @xprt_ctx:	pointer to the channel context.
 *
 * Return: name of the remote processor/edge
 */
char *glink_get_ch_edge_name(struct channel_ctx *ch_ctx);

/**
 * glink_get_ch_rcid() - get the remote channel ID
 * @ch_ctx:	pointer to the channel context.
 *
 * Return: remote channel id, -EINVAL in case of invalid input
 */
int glink_get_ch_lcid(struct channel_ctx *ch_ctx);

/**
 * glink_get_ch_rcid() - get the remote channel ID
 * @ch_ctx:	pointer to the channel context.
 *
 * Return: remote channel id, -EINVAL in case of invalid input
 */
int glink_get_ch_rcid(struct channel_ctx *ch_ctx);

/**
 * glink_get_ch_state() - get the channel state
 * @ch_ctx:	pointer to the channel context.
 *
 * Return: name of the channel state, NULL in case of invalid input
 */
const char *glink_get_ch_state(struct channel_ctx *ch_ctx);

/**
 * glink_get_ch_xprt_name() - get the name of the transport to which
 *				the channel belongs
 * @ch_ctx:	pointer to the channel context.
 *
 * Return: name of the export, NULL in case of invalid input
 */
char *glink_get_ch_xprt_name(struct channel_ctx *ch_ctx);

/**
 * glink_get_tx_pkt_count() - get the total number of packets sent
 *				through this channel
 * @ch_ctx:	pointer to the channel context.
 *
 * Return: number of packets transmitted, -EINVAL in case of invalid input
 */
int glink_get_ch_tx_pkt_count(struct channel_ctx *ch_ctx);

/**
 * glink_get_ch_rx_pkt_count() - get the total number of packets
 *				recieved at this channel
 * @ch_ctx:	pointer to the channel context.
 *
 * Return: number of packets recieved, -EINVAL in case of invalid input
 */
int glink_get_ch_rx_pkt_count(struct channel_ctx *ch_ctx);

/**
 * glink_get_ch_lintents_queued() - get the total number of intents queued
 *				at local side
 * @ch_ctx:	pointer to the channel context.
 *
 * Return: number of intents queued, -EINVAL in case of invalid input
 */
int glink_get_ch_lintents_queued(struct channel_ctx *ch_ctx);

/**
 * glink_get_ch_rintents_queued() - get the total number of intents queued
 *				from remote side
 * @ch_ctx:	pointer to the channel context.
 *
 * Return: number of intents queued
 */
int glink_get_ch_rintents_queued(struct channel_ctx *ch_ctx);

/**
 * glink_get_ch_intent_info() - get the intent details of a channel
 * @ch_ctx:	pointer to the channel context.
 * @ch_ctx_i:   pointer to a structure that will contain intent details
 *
 * This funcion is used to get all the channel intent details including locks.
 */
void glink_get_ch_intent_info(struct channel_ctx *ch_ctx,
			struct glink_ch_intent_info *ch_ctx_i);
/*
 * glink_ssr() - SSR cleanup function.
 *
 * Return: Standard error code.
 */
int glink_ssr(const char *subsystem);

#endif /* _SOC_QCOM_GLINK_PRIVATE_H_ */

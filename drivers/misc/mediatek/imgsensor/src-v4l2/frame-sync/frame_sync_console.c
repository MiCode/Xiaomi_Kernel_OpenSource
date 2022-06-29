// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#include "frame_sync_console.h"





/******************************************************************************/
// Log message
/******************************************************************************/
#include "frame_sync_log.h"

#define REDUCE_FS_CON_LOG
#define PFX "FrameSyncConsole"
/******************************************************************************/





/******************************************************************************/
// frame sync console define & commands
/******************************************************************************/
#define SHOW(buf, len, fmt, ...) { \
	len += snprintf(buf + len, PAGE_SIZE - len, fmt, ##__VA_ARGS__); \
}


/* for decode frame sync console variable from user */
/* --> (_ _|_ _ _ _ _ _ _ _) */
#define FS_CON_CMD_ID_BASE    100000000
#define FS_CON_CMD_ID_MOD           100
#define FS_CON_CMD_VAL_BASE           1
#define FS_CON_CMD_VAL_MOD    100000000

/* --> for struct fs_con_usr_cfg */
/* --> (_ _|_|_ _|_ _ _ _ _) */
#define FS_CON_USR_CFG_EN_BASE 10000000
#define FS_CON_USR_CFG_EN_MOD        10
#define FS_CON_USR_CFG_SIDX_BASE 100000
#define FS_CON_USR_CFG_SIDX_MOD     100
#define FS_CON_USR_CFG_VAL_BASE       1
#define FS_CON_USR_CFG_VAL_MOD   100000

/* supported commands */
enum fs_console_cmd_id {
	FS_CON_FORCE_TO_IGNORE_SET_SYNC = 1,
	FS_CON_DEFAULT_EN_SET_SYNC,

	FS_CON_EN_OVERWRITE_SET_SYNC, /* TBD */
	FS_CON_EN_OVERWRITE_MAX_FPS, /* TBD */

	FS_CON_USR_ASYNC_MASTER_SIDX = 20,

	FS_CON_AUTO_LISTEN_EXT_VSYNC = 30,
	FS_CON_FORCE_LISTEN_EXT_VSYNC = 31,

	FS_CON_PF_LOG_TRACER = 41,

	/* last one (max value is 42) */
	FS_CON_LOG_TRACER = 42
};
/******************************************************************************/





/******************************************************************************/
// frame sync console variables
/******************************************************************************/
// static struct device **pdev;


/* log control */
unsigned int log_tracer;
unsigned int pf_log_tracer;


struct fs_con_usr_cfg {
	unsigned int en;
	unsigned int value;
};


struct fs_console_mgr {
	/* --- global configs --- */
	/* force ignore frame-sync / set sync */
	unsigned int force_to_ignore_set_sync;

	/* default enable frame-sync set sync (at streaming on) */
	unsigned int default_en_set_sync;

	/* disable algo auto listen ext vsync */
	unsigned int auto_listen_ext_vsync;

	/* listen ext vsync (control by user) */
	unsigned int listen_ext_vsync;


	/* for overwrite set sync value */
	struct fs_con_usr_cfg set_sync[SENSOR_MAX_NUM];

	/* for overwrite set max fps value */
	struct fs_con_usr_cfg max_fps[SENSOR_MAX_NUM];

	/* for overwrite user async master sidx */
	int usr_async_m_sidx;


	/* --- frame sync internal control variables --- */
	/* listen ext vsync (control by algorithm) */
	unsigned int listen_vsync_alg;
};
static struct fs_console_mgr fs_con_mgr;
/******************************************************************************/





/******************************************************************************/
// basic/utility function
/******************************************************************************/
static inline void fs_console_init_def_value(void)
{
	// *pdev = NULL;
	log_tracer = LOG_TRACER_DEF;
	pf_log_tracer = PF_LOG_TRACER_DEF;

	fs_con_mgr.force_to_ignore_set_sync = 0;
	fs_con_mgr.default_en_set_sync = 0;
	fs_con_mgr.auto_listen_ext_vsync = ALGO_AUTO_LISTEN_VSYNC;

	fs_con_mgr.usr_async_m_sidx = MASTER_IDX_NONE;

	// two stage frame-sync:
	// => use seninf-worker to trigger frame length calculation
	fs_con_mgr.listen_ext_vsync = TWO_STAGE_FS;
	fs_con_mgr.listen_vsync_alg = 0;
}


static inline unsigned int decode_cmd_value(const unsigned int cmd,
	const unsigned int base, const unsigned int mod)
{
	unsigned int ret = 0;

	ret = (base != 0) ? (cmd / base) : 0;
	ret %= mod;

	return ret;
}


static inline enum fs_console_cmd_id fs_console_get_cmd_id(
	const unsigned int cmd)
{
	return (enum fs_console_cmd_id)decode_cmd_value(cmd,
		FS_CON_CMD_ID_BASE, FS_CON_CMD_ID_MOD);
}


static inline void fs_console_setup_cmd_value(
	const unsigned int cmd, unsigned int *p_val)
{
	*p_val = decode_cmd_value(cmd,
		FS_CON_CMD_VAL_BASE, FS_CON_CMD_VAL_MOD);
}


static void fs_console_setup_usr_cfg(const unsigned int cmd,
	struct fs_con_usr_cfg p_usr_cfg[])
{
	unsigned int sidx = 0;
	unsigned int i = 0;

	sidx = decode_cmd_value(cmd,
		FS_CON_USR_CFG_SIDX_BASE, FS_CON_USR_CFG_SIDX_MOD);

	/* check sidx value get from user */
	if (sidx == 99) {
		for (i = 0; i < SENSOR_MAX_NUM; ++i) {
			p_usr_cfg[i].en = decode_cmd_value(cmd,
				FS_CON_USR_CFG_EN_BASE,
				FS_CON_USR_CFG_EN_MOD);

			p_usr_cfg[i].value = (p_usr_cfg[i].en)
				? decode_cmd_value(cmd,
					FS_CON_USR_CFG_VAL_BASE,
					FS_CON_USR_CFG_VAL_MOD)
				: 0;
		}
	} else if (sidx < SENSOR_MAX_NUM) {
		p_usr_cfg[sidx].en = decode_cmd_value(cmd,
			FS_CON_USR_CFG_EN_BASE, FS_CON_USR_CFG_EN_MOD);

		p_usr_cfg[sidx].value = (p_usr_cfg[sidx].en)
			? decode_cmd_value(cmd,
				FS_CON_USR_CFG_VAL_BASE,
				FS_CON_USR_CFG_VAL_MOD)
			: 0;
	}
}


static unsigned int fs_console_get_usr_cfg(const unsigned int sidx,
	unsigned int *p_val, struct fs_con_usr_cfg p_usr_cfg[])
{
	unsigned int ret = p_usr_cfg[sidx].en;

	if (ret != 0 && p_val)
		*p_val = p_usr_cfg[sidx].value;

	return ret;
}
/******************************************************************************/





/******************************************************************************/
// function used internally by frame-sync
/******************************************************************************/
unsigned int fs_con_chk_force_to_ignore_set_sync(void)
{
	return fs_con_mgr.force_to_ignore_set_sync;
}


unsigned int fs_con_chk_default_en_set_sync(void)
{
	return fs_con_mgr.default_en_set_sync;
}


unsigned int fs_con_chk_en_overwrite_set_sync(const unsigned int sidx,
	unsigned int *p_val)
{
	return fs_console_get_usr_cfg(sidx, p_val, fs_con_mgr.set_sync);
}


unsigned int fs_con_chk_en_overwrite_max_fps(const unsigned int sidx,
	unsigned int *p_val)
{
	return fs_console_get_usr_cfg(sidx, p_val, fs_con_mgr.max_fps);
}


int fs_con_chk_usr_async_m_sidx(void)
{
	return fs_con_mgr.usr_async_m_sidx;
}


unsigned int fs_con_get_usr_listen_ext_vsync(void)
{
	return fs_con_mgr.listen_ext_vsync;
}


unsigned int fs_con_get_usr_auto_listen_ext_vsync(void)
{
	return fs_con_mgr.auto_listen_ext_vsync;
}


unsigned int fs_con_get_listen_vsync_alg_cfg(void)
{
	return (fs_con_mgr.auto_listen_ext_vsync)
		? fs_con_mgr.listen_vsync_alg : 0;
}


void fs_con_set_listen_vsync_alg_cfg(unsigned int flag)
{
	fs_con_mgr.listen_vsync_alg = flag;
}
/******************************************************************************/





/******************************************************************************/
// console entry function
/******************************************************************************/
static ssize_t fsync_console_show(
	struct device *dev, struct device_attribute *attr,
	char *buf)
{
	int len = 0;


	SHOW(buf, len,
		"\n\t[ fsync_console ] show all information...\n");


	SHOW(buf, len,
		"\t\t[ %2u : FORCE_TO_IGNORE_SET_SYNC ] force_to_ignore_set_sync : %u\n",
		(unsigned int)FS_CON_FORCE_TO_IGNORE_SET_SYNC,
		fs_con_mgr.force_to_ignore_set_sync);


	SHOW(buf, len,
		"\t\t[ %2u : DEFAULT_EN_SET_SYNC ] default_en_set_sync : %u\n",
		(unsigned int)FS_CON_DEFAULT_EN_SET_SYNC,
		fs_con_mgr.default_en_set_sync);


	SHOW(buf, len,
		"\t\t[ %2u : EN_OVERWRITE_SET_SYNC ] (sidx:(en/val)) : 0:(%u/%u)/1:(%u/%u)/2:(%u/%u)/3:(%u/%u)/4:(%u/%u)/5:(%u/%u)\n",
		(unsigned int)FS_CON_EN_OVERWRITE_SET_SYNC,
		fs_con_mgr.set_sync[0].en, fs_con_mgr.set_sync[0].value,
		fs_con_mgr.set_sync[1].en, fs_con_mgr.set_sync[1].value,
		fs_con_mgr.set_sync[2].en, fs_con_mgr.set_sync[2].value,
		fs_con_mgr.set_sync[3].en, fs_con_mgr.set_sync[3].value,
		fs_con_mgr.set_sync[4].en, fs_con_mgr.set_sync[4].value,
		fs_con_mgr.set_sync[5].en, fs_con_mgr.set_sync[5].value);


	SHOW(buf, len,
		"\t\t[ %2u : EN_OVERWRITE_MAX_FPS ] (sidx:(en/val)) : 0:(%u/%u)/1:(%u/%u)/2:(%u/%u)/3:(%u/%u)/4:(%u/%u)/5:(%u/%u)\n",
		(unsigned int)FS_CON_EN_OVERWRITE_MAX_FPS,
		fs_con_mgr.max_fps[0].en, fs_con_mgr.max_fps[0].value,
		fs_con_mgr.max_fps[1].en, fs_con_mgr.max_fps[1].value,
		fs_con_mgr.max_fps[2].en, fs_con_mgr.max_fps[2].value,
		fs_con_mgr.max_fps[3].en, fs_con_mgr.max_fps[3].value,
		fs_con_mgr.max_fps[4].en, fs_con_mgr.max_fps[4].value,
		fs_con_mgr.max_fps[5].en, fs_con_mgr.max_fps[5].value);


	SHOW(buf, len,
		"\t\t[ %2u : USR_ASYNC_MASTER_SIDX ] usr_async_m_sidx : %d\n",
		(unsigned int)FS_CON_USR_ASYNC_MASTER_SIDX,
		fs_con_mgr.usr_async_m_sidx);


	SHOW(buf, len,
		"\t\t[ %2u : AUTO_LISTEN_EXT_VSYNC ] auto_listen_ext_vsync : %u\n",
		(unsigned int)FS_CON_AUTO_LISTEN_EXT_VSYNC,
		fs_con_mgr.auto_listen_ext_vsync);


	SHOW(buf, len,
		"\t\t[ %2u : FORCE_LISTEN_EXT_VSYNC ] listen_ext_vsync : %u\n",
		(unsigned int)FS_CON_FORCE_LISTEN_EXT_VSYNC,
		fs_con_mgr.listen_ext_vsync);


	SHOW(buf, len,
		"\t\t[ %2u : PF_LOG_TRACER ] pf_log_tracer : %u\n",
		(unsigned int)FS_CON_PF_LOG_TRACER,
		pf_log_tracer);


	SHOW(buf, len,
		"\t\t[ %2u : LOG_TRACER ] log_tracer : %u\n",
		(unsigned int)FS_CON_LOG_TRACER,
		log_tracer);


	SHOW(buf, len, "\n");

	return len;
}


static ssize_t fsync_console_store(
	struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	int ret = 0;
	// int len = 0;
	unsigned int cmd = 0;
	enum fs_console_cmd_id cmd_id = 0;
	// char str_buf[255] = {0};


	/* convert string to unsigned int */
	ret = kstrtouint(buf, 0, &cmd);
	if (ret != 0) {
		// SHOW(str_buf, len,
		//	"\n\t[fsync_console]: kstrtoint failed, input:%s, cmd:%u, ret:%d\n",
		//	buf,
		//	cmd,
		//	ret);
	}


	cmd_id = fs_console_get_cmd_id(cmd);

	switch (cmd_id) {
	case FS_CON_FORCE_TO_IGNORE_SET_SYNC:
		fs_console_setup_cmd_value(cmd,
			&fs_con_mgr.force_to_ignore_set_sync);
		break;

	case FS_CON_DEFAULT_EN_SET_SYNC:
		fs_console_setup_cmd_value(cmd,
			&fs_con_mgr.default_en_set_sync);
		break;

	case FS_CON_EN_OVERWRITE_SET_SYNC:
		fs_console_setup_usr_cfg(cmd, fs_con_mgr.set_sync);
		break;

	case FS_CON_EN_OVERWRITE_MAX_FPS:
		fs_console_setup_usr_cfg(cmd, fs_con_mgr.max_fps);
		break;

	case FS_CON_USR_ASYNC_MASTER_SIDX:
		fs_console_setup_cmd_value(cmd,
			&fs_con_mgr.usr_async_m_sidx);
		break;

	case FS_CON_AUTO_LISTEN_EXT_VSYNC:
		fs_console_setup_cmd_value(cmd,
			&fs_con_mgr.auto_listen_ext_vsync);
		break;

	case FS_CON_FORCE_LISTEN_EXT_VSYNC:
		fs_console_setup_cmd_value(cmd,
			&fs_con_mgr.listen_ext_vsync);
		break;

	case FS_CON_PF_LOG_TRACER:
		fs_console_setup_cmd_value(cmd, &pf_log_tracer);
		break;

	case FS_CON_LOG_TRACER:
		fs_console_setup_cmd_value(cmd, &log_tracer);
		break;

	default:
		break;
	}


	return count;
}


static DEVICE_ATTR_RW(fsync_console);

/******************************************************************************/





/******************************************************************************/
// sysfs create/remove file
/******************************************************************************/
int fs_con_create_sysfs_file(struct device *dev)
{
	int ret = 0;


	fs_console_init_def_value();

	if (dev == NULL) {
		LOG_MUST(
			"ERROR: failed to create sysfs file, dev is NULL, return\n");
		return 1;
	}

	// *pdev = dev;

	ret = device_create_file(dev, &dev_attr_fsync_console);
	if (ret) {
		LOG_MUST(
			"ERROR: device_create_file failed, ret:%d\n",
			ret);
	}


#if !defined(REDUCE_FS_CON_LOG)
	LOG_MUST("ret:%d", ret);
#endif // REDUCE_FS_CON_LOG


	return ret;
}


void fs_con_remove_sysfs_file(struct device *dev)
{
	if (dev == NULL) {
		LOG_MUST(
			"ERROR: *pdev is NULL, abort process device_remove_file, return\n");
		return;
	}

	device_remove_file(dev, &dev_attr_fsync_console);


#if !defined(REDUCE_FS_CON_LOG)
	LOG_MUST("sysfile remove\n");
#endif // REDUCE_FS_CON_LOG

}
/******************************************************************************/

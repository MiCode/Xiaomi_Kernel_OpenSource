/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

 /*
  * I2C controller logging/Debugfs for QTI MSM platforms
  */

#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <linux/debugfs.h>
#include <linux/msm-sps.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/i2c/i2c-msm-v2.h>

#ifdef CONFIG_DEBUG_FS

enum i2c_msm_dbgfs_file_type {
	I2C_MSM_DFS_U8,
	I2C_MSM_DFS_U32,
	I2C_MSM_DFS_FILE,
};

/*
 * i2c_msm_dbgfs_file: entry in a table of debugfs files
 *
 * @name      debugfs file name
 * @mode      file permissions
 * @fops      used when type == I2C_MSM_DFS_FILE
 * @value_ptr used when type != I2C_MSM_DFS_FILE
 */
struct i2c_msm_dbgfs_file {
	const char                   *name;
	const umode_t                 mode;
	enum i2c_msm_dbgfs_file_type  type;
	const struct file_operations *fops;
	u32                          *value_ptr;
};

static const umode_t I2C_MSM_DFS_MD_R  = S_IRUSR | S_IRGRP;
static const umode_t I2C_MSM_DFS_MD_W  = S_IWUSR | S_IWGRP;
static const umode_t I2C_MSM_DFS_MD_RW = S_IRUSR | S_IRGRP |
					   S_IWUSR | S_IWGRP;

void i2c_msm_dbgfs_create(struct i2c_msm_ctrl *ctrl,
				struct i2c_msm_dbgfs_file *itr)
{
	struct dentry *file;

	ctrl->dbgfs.root = debugfs_create_dir(dev_name(ctrl->dev), NULL);
	if (!ctrl->dbgfs.root) {
		dev_err(ctrl->dev, "error on creating debugfs root\n");
		return;
	}

	for (; itr->name; ++itr) {
		switch (itr->type) {
		case I2C_MSM_DFS_FILE:
			file = debugfs_create_file(itr->name,
						   itr->mode,
						   ctrl->dbgfs.root,
						   ctrl, itr->fops);
			break;
		case I2C_MSM_DFS_U8:
			file = debugfs_create_u8(itr->name,
						 itr->mode,
						 ctrl->dbgfs.root,
						 (u8 *) itr->value_ptr);
			break;
		default: /* I2C_MSM_DFS_U32 */
			file = debugfs_create_u32(itr->name,
						 itr->mode,
						 ctrl->dbgfs.root,
						 (u32 *) itr->value_ptr);
			break;
		}
		if (!file)
			dev_err(ctrl->dev,
				"error on creating debugfs entry:%s\n",
				itr->name);
	}
}

void i2c_msm_dbgfs_init(struct i2c_msm_ctrl *ctrl)
{
	struct i2c_msm_dbgfs_file i2c_msm_dbgfs_map[] = {
		{"dbg-lvl",         I2C_MSM_DFS_MD_RW, I2C_MSM_DFS_U8,
				NULL, &ctrl->dbgfs.dbg_lvl},
		{"xfer-force-mode", I2C_MSM_DFS_MD_RW, I2C_MSM_DFS_U8,
				NULL, &ctrl->dbgfs.force_xfer_mode},
		{NULL, 0, 0, NULL , NULL}, /* null terminator */
	};
	return i2c_msm_dbgfs_create(ctrl, i2c_msm_dbgfs_map);
}
EXPORT_SYMBOL(i2c_msm_dbgfs_init);

void i2c_msm_dbgfs_teardown(struct i2c_msm_ctrl *ctrl)
{
	debugfs_remove_recursive(ctrl->dbgfs.root);
}
EXPORT_SYMBOL(i2c_msm_dbgfs_teardown);

#else
void i2c_msm_dbgfs_init(struct i2c_msm_ctrl *ctrl) {}
EXPORT_SYMBOL(i2c_msm_dbgfs_init);

void i2c_msm_dbgfs_teardown(struct i2c_msm_ctrl *ctrl) {}
EXPORT_SYMBOL(i2c_msm_dbgfs_teardown);
#endif

/*
 * i2c_msm_dbg_tag_byte: accessor for tag as four bytes array
 */
static u8 *i2c_msm_dbg_tag_byte(struct i2c_msm_tag *tag, int byte_n)
{
	return ((u8 *)tag) + byte_n;
}
static const char * const i2c_msm_fifo_sz_str_tbl[]
		= {"x2 blk sz", "x4 blk sz" , "x8 blk sz", "x16 blk sz"};
static const char * const i2c_msm_fifo_block_sz_str_tbl[]
						= {"16", "16" , "32", "0"};

/* string table for qup_io_modes register */
static const char * const i2c_msm_qup_mode_str_tbl[] = {
	"FIFO", "Block", "Reserved", "DMA",
};

static const char * const i2c_msm_mini_core_str_tbl[] = {
	"null", "SPI", "I2C", "reserved",
};
/*
 * i2c_msm_qup_reg_fld: a register field descriptor
 * @name   field name
 * @to_str_tbl  when not null, used to interpret the bits value. The bits value
 *         is the table entry number.
 */
struct i2c_msm_qup_reg_fld {
	const char * const name;
	int                bit_idx;
	int                n_bits;
	const char * const *to_str_tbl;
};

static const char * const i2c_msm_reg_qup_state_to_str[] = {
	"Reset", "Run", "Clear", "Pause"
};

/* QUP_STATE register fields */
static struct i2c_msm_qup_reg_fld i2c_msm_qup_state_fields_map[] = {
	{ "STATE",             0,   2, i2c_msm_reg_qup_state_to_str},
	{ "VALID",             2,   1},
	{ "MAST_GEN",          4,   1},
	{ "WAIT_EOT",          5,   1},
	{ "FLUSH",             6,   1},
	{ NULL,                0,   1},
};

/* QUP_CONFIG register fields */
static struct i2c_msm_qup_reg_fld i2c_msm_qup_config_fields_map[] = {
	{ "N",               0,   5},
	{ "MINI_CORE",       8,   2, i2c_msm_mini_core_str_tbl},
	{ "NO_OUTPUT",       6,   1},
	{ "NO_INPUT",        7,   1},
	{ "EN_EXT_OUT",     16,   1},
	{ NULL,              0,   1},
};

/* QUP_OPERATIONAL register fields */
static struct i2c_msm_qup_reg_fld i2c_msm_qup_op_fields_map[] = {
	{ "OUT_FF_N_EMPTY",  4,   1},
	{ "IN_FF_N_EMPTY",   5,   1},
	{ "OUT_FF_FUL",      6,   1},
	{ "IN_FF_FUL",       7,   1},
	{ "OUT_SRV_FLG",     8,   1},
	{ "IN_SRV_FLG",      9,   1},
	{ "MX_OUT_DN",      10,   1},
	{ "MX_IN_DN",       11,   1},
	{ "OUT_BLK_WR",     12,   1},
	{ "IN_BLK_RD",      13,   1},
	{ "DONE_TGL",       14,   1},
	{ "NWD",            15,   1},
	{ NULL,              0,   1},
};

/* QUP_I2C_STATUS (a.k.a I2C_MASTER_STATUS) register fields */
static struct i2c_msm_qup_reg_fld i2c_msm_qup_i2c_stat_fields_map[] = {
	{ "BUS_ERR",        2,   1},
	{ "NACK",           3,   1},
	{ "ARB_LOST",       4,   1},
	{ "INVLD_WR",       5,   1},
	{ "FAIL",           6,   2},
	{ "BUS_ACTV",       8,   1},
	{ "BUS_MSTR",       9,   1},
	{ "DAT_STATE",     10,   3},
	{ "CLK_STATE",     13,   3},
	{ "O_FSM_STAT",    16,   3},
	{ "I_FSM_STAT",    19,   3},
	{ "INVLD_TAG",     23,   1},
	{ "INVLD_RD_ADDR", 24,   1},
	{ "INVLD_RD_SEQ",  25,   1},
	{ "SDA",           26,   1},
	{ "SCL",           27,   1},
	{ NULL,             0,   1},
};

/* QUP_ERROR_FLAGS register fields */
static struct i2c_msm_qup_reg_fld i2c_msm_qup_err_flags_fields_map[] = {
	{ "IN_OVR_RUN",        2,   1},
	{ "OUT_UNDR_RUN",      3,   1},
	{ "IN_UNDR_RUN",       4,   1},
	{ "OUT_OVR_RUN",       5,   1},
	{ NULL,                0,   1},
};

/* QUP_OPERATIONAL_MASK register fields */
static struct i2c_msm_qup_reg_fld i2c_msm_qup_op_mask_fields_map[] = {
	{ "OUT_SRVC_MASK",     8,   1},
	{ "IN_SRVC_MASK",      9,   1},
	{ NULL,                0,   1},
};

/* QUP_I2C_MASTER_CLK_CTL register fields */
static struct i2c_msm_qup_reg_fld i2c_msm_qup_master_clk_fields_map[] = {
	{ "FS_DIV",            0,   8},
	{ "HS_DIV",            8,   3},
	{ "HI_TM_DIV",        16,   8},
	{ "SCL_NS_RJCT",      24,   2},
	{ "SDA_NS_RJCT",      26,   2},
	{ "SCL_EXT_FRC_L",    28,   1},
	{ NULL,                0,   1},
};

static const char * const i2c_msm_dbg_tag_val_str_tbl[] = {
	"NOP_WAIT",		/* 0x80 */
	"START",		/* 0x81 */
	"DATAWRITE",		/* 0x82 */
	"DATAWRT_and_STOP",	/* 0x83 */
	NULL,			/* 0x84 */
	"DATAREAD",		/* 0x85 */
	"DATARD_and_NACK",	/* 0x86 */
	"DATARD_and_STOP",	/* 0x87 */
	"STOP_TAG",		/* 0x88 */
	NULL,			/* 0x89 */
	NULL,			/* 0x8A */
	NULL,			/* 0x8B */
	NULL,			/* 0x8C */
	NULL,			/* 0x8D */
	NULL,			/* 0x8E */
	NULL,			/* 0x8F */
	"NOP_MARK",		/* 0x90 */
	"NOP_ID",		/* 0x91 */
	"TIME_STAMP",		/* 0x92 */
	"INPUT_EOT",		/* 0x93 */
	"INPUT_EOT_FLUSH",	/* 0x94 */
	"NOP_LOCAL",		/* 0x95 */
	"FLUSH STOP",		/* 0x96 */
};

/* QUP_IO_MODES register fields */
static struct i2c_msm_qup_reg_fld i2c_msm_qup_io_modes_map[] = {
	{ "IN_BLK_SZ",         5,   2, i2c_msm_fifo_block_sz_str_tbl},
	{ "IN_FF_SZ",          7,   3, i2c_msm_fifo_sz_str_tbl},
	{ "OUT_BLK_SZ",        0,   2, i2c_msm_fifo_block_sz_str_tbl},
	{ "OUT_FF_SZ",         2,   3, i2c_msm_fifo_sz_str_tbl},
	{ "UNPACK",           14,   1},
	{ "PACK",             15,   1},
	{ "INP_MOD",          12,   2, i2c_msm_qup_mode_str_tbl},
	{ "OUT_MOD",          10,   2, i2c_msm_qup_mode_str_tbl},
	{ NULL,                0,   1},
};

/*
 * i2c_msm_qup_reg_dump: desc fmt of reg to dump via i2c_msm_dbg_qup_reg_dump()
 *
 * @offset    the register's offset in the QUP
 * @name      name to dump before value
 * @field_map when set i2c_msm_dbg_qup_reg_flds_to_str() is used. Otherwise
 *            if val_to_str_func() is set, then it is used. When both are NULL
 *            none is used. Only the register's value is dumped.
 */
struct i2c_msm_qup_reg_dump {
	u32                          offset;
	const char                  *name;
	struct i2c_msm_qup_reg_fld  *field_map;
};

static const struct i2c_msm_qup_reg_dump i2c_msm_qup_reg_dump_map[] = {
{QUP_CONFIG,             "QUP_CONFIG",   i2c_msm_qup_config_fields_map    },
{QUP_STATE,              "QUP_STATE",    i2c_msm_qup_state_fields_map     },
{QUP_IO_MODES,           "QUP_IO_MDS",   i2c_msm_qup_io_modes_map         },
{QUP_ERROR_FLAGS,        "QUP_ERR_FLGS", i2c_msm_qup_err_flags_fields_map },
{QUP_OPERATIONAL,        "QUP_OP",       i2c_msm_qup_op_fields_map        },
{QUP_OPERATIONAL_MASK,   "QUP_OP_MASK",  i2c_msm_qup_op_mask_fields_map   },
{QUP_I2C_STATUS,         "QUP_I2C_STAT", i2c_msm_qup_i2c_stat_fields_map  },
{QUP_I2C_MASTER_CLK_CTL, "QUP_MSTR_CLK", i2c_msm_qup_master_clk_fields_map},
{QUP_IN_DEBUG,           "QUP_IN_DBG"  },
{QUP_OUT_DEBUG,          "QUP_OUT_DBG" },
{QUP_IN_FIFO_CNT,        "QUP_IN_CNT"  },
{QUP_OUT_FIFO_CNT,       "QUP_OUT_CNT" },
{QUP_MX_READ_COUNT,      "MX_RD_CNT"   },
{QUP_MX_WRITE_COUNT,     "MX_WR_CNT"   },
{QUP_MX_INPUT_COUNT,     "MX_IN_CNT"   },
{QUP_MX_OUTPUT_COUNT,    "MX_OUT_CNT"  },
{0,                       NULL         },
};

static const char *i2c_msm_dbg_tag_val_to_str(u8 tag_val)
{
	if ((tag_val < 0x80) || (tag_val > 0x96) || (tag_val == 0x84) ||
	   ((tag_val > 0x88) && (0x90 > tag_val)))
		return "Invalid_tag";

	return i2c_msm_dbg_tag_val_str_tbl[tag_val - 0x80];
}

/*
 * i2c_msm_dbg_qup_reg_flds_to_str: format register's fields using a field map
 *
 * @fld an array of fields mapping bits of val to fields/flags values
 * @val the register's value
 * @buf buffer to format the strings into
 * @len buf's len
 */
static const char *i2c_msm_dbg_qup_reg_flds_to_str(
	u32 val, char *buf, int len, const struct i2c_msm_qup_reg_fld *fld)
{
	char *ptr = buf;
	int str_len;
	int str_len_sum = 0;
	int rem_len     = len;
	u32 field_val;
	for (; fld->name && (rem_len > 0); ++fld) {
		if (fld->n_bits == 1) {
			field_val = BIT_IS_SET(val, fld->bit_idx);
			/*
			 * Only dump interesting flags (skip flags who's value
			 * is zero).
			 */
			if (!field_val)
				continue;

			str_len = snprintf(ptr, rem_len, "%s ", fld->name);
		} else {
			field_val = BITS_AT(val, fld->bit_idx, fld->n_bits);

			/*
			 * Only dump interesting fields (skip fields who's value
			 * is zero).
			 */
			if (!field_val)
				continue;

			if (fld->to_str_tbl)
				str_len = snprintf(ptr, rem_len, "%s:%s ",
				   fld->name, fld->to_str_tbl[field_val]);
			else
				str_len = snprintf(ptr, rem_len, "%s:0x%x ",
				   fld->name, field_val);
		}

		if (str_len > rem_len) {
			pr_err("%s insufficient buffer space\n", __func__);
			/* snprintf does not guarantee NULL terminator */
			buf[len - 1] = 0;
			return buf;
		}

		rem_len     -= str_len;
		ptr         += str_len;
		str_len_sum += str_len;
	}

	/* snprintf does not guarantee NULL terminator */
	buf[len - 1] = 0;
	return buf;
}

const char *i2c_msm_dbg_tag_to_str(const struct i2c_msm_tag *tag,
						char *buf, size_t buf_len)
{
	/* cast const away. t is read-only here */
	struct i2c_msm_tag *t = (struct i2c_msm_tag *) tag;
	switch (tag->len) {
	case 6:
		snprintf(buf, buf_len, "val:0x%012llx %s:0x%x %s:0x%x %s:%d",
			tag->val,
			i2c_msm_dbg_tag_val_to_str(*i2c_msm_dbg_tag_byte(t, 0)),
			*i2c_msm_dbg_tag_byte(t, 1),
			i2c_msm_dbg_tag_val_to_str(*i2c_msm_dbg_tag_byte(t, 2)),
			*i2c_msm_dbg_tag_byte(t, 3),
			i2c_msm_dbg_tag_val_to_str(*i2c_msm_dbg_tag_byte(t, 4)),
			*i2c_msm_dbg_tag_byte(t, 5));
		break;
	case 4:
		snprintf(buf, buf_len, "val:0x%08llx %s:0x%x %s:%d",
			(tag->val & 0xffffffff),
			i2c_msm_dbg_tag_val_to_str(*i2c_msm_dbg_tag_byte(t, 0)),
			*i2c_msm_dbg_tag_byte(t, 1),
			i2c_msm_dbg_tag_val_to_str(*i2c_msm_dbg_tag_byte(t, 2)),
			*i2c_msm_dbg_tag_byte(t, 3));
		break;
	default: /* 2 bytes tag */
		snprintf(buf, buf_len, "val:0x%04llx %s:%d",
			(tag->val & 0xffff),
			i2c_msm_dbg_tag_val_to_str(*i2c_msm_dbg_tag_byte(t, 0)),
			*i2c_msm_dbg_tag_byte(t, 1));
	}

	return buf;
}
EXPORT_SYMBOL(i2c_msm_dbg_tag_to_str);

const char *
i2c_msm_dbg_dma_tag_to_str(const struct i2c_msm_dma_tag *dma_tag, char *buf,
								size_t buf_len)
{
	const char *ret;
	u32        *val;
	struct i2c_msm_tag tag;

	val = phys_to_virt(dma_tag->buf);
	if (!val) {
		pr_err("Failed phys_to_virt(0x%llx)", (u64) dma_tag->buf);
		return "Error";
	}

	tag = (struct i2c_msm_tag) {
		.val = *val,
		.len = dma_tag->len,
	};

	ret = i2c_msm_dbg_tag_to_str(&tag, buf, buf_len);
	return ret;
}
EXPORT_SYMBOL(i2c_msm_dbg_dma_tag_to_str);

/*
 * see: struct i2c_msm_qup_reg_dump for more
 */
int i2c_msm_dbg_qup_reg_dump(struct i2c_msm_ctrl *ctrl)
{
	u32 val;
	char buf[I2C_MSM_REG_2_STR_BUF_SZ];
	void __iomem *base = ctrl->rsrcs.base;
	const struct i2c_msm_qup_reg_dump *itr = i2c_msm_qup_reg_dump_map;

	for (; itr->name; ++itr) {
		val = readl_relaxed(base + itr->offset);
		buf[0] = 0;
		if (itr->field_map)
			i2c_msm_dbg_qup_reg_flds_to_str(val, buf, sizeof(buf),
								itr->field_map);
		dev_err(ctrl->dev, "%-12s:0x%08x %s\n", itr->name, val, buf);
	};
	return 0;
}
EXPORT_SYMBOL(i2c_msm_dbg_qup_reg_dump);

typedef void (*i2c_msm_prof_dump_func_func_t)(struct i2c_msm_ctrl *,
			struct i2c_msm_prof_event *, size_t msec, size_t usec);

/*
 * i2c_msm_prof_evnt_add: pushes event into end of event array
 *
 * @dump_now log a copy immediately to kernel log
 *
 * Implementation of i2c_msm_prof_evnt_add(). When array overflows, the last
 * entry is overwritten as many times as it overflows.
 */
void i2c_msm_prof_evnt_add(struct i2c_msm_ctrl *ctrl,
				enum msm_i2_debug_level dbg_level,
				enum i2c_msm_prof_evnt_type event_type,
				u64 data0, u32 data1, u32 data2)
{
	struct i2c_msm_xfer       *xfer  = &ctrl->xfer;
	struct i2c_msm_prof_event *event;
	int idx;

	if (ctrl->dbgfs.dbg_lvl < dbg_level)
		return;

	atomic_add_unless(&xfer->event_cnt, 1, I2C_MSM_PROF_MAX_EVNTS - 1);
	idx = atomic_read(&xfer->event_cnt) - 1;
	if (idx > (I2C_MSM_PROF_MAX_EVNTS - 1))
		dev_err(ctrl->dev, "error event index:%d max:%d\n",
						idx, I2C_MSM_PROF_MAX_EVNTS);
	event = &xfer->event[idx];

	getnstimeofday(&event->time);
	event->dump_func_id = event_type;
	event->data0 = data0;
	event->data1 = data1;
	event->data2 = data2;
}
EXPORT_SYMBOL(i2c_msm_prof_evnt_add);

void i2c_msm_prof_dump_xfer_beg(struct i2c_msm_ctrl *ctrl,
		struct i2c_msm_prof_event *event, size_t msec, size_t usec)
{
	dev_info(ctrl->dev,
		"-->.%03zums XFER_BEG msg_cnt:%llx addr:0x%x\n",
		usec, event->data0, event->data1);
}

void i2c_msm_prof_dump_actv_end(struct i2c_msm_ctrl *ctrl,
		struct i2c_msm_prof_event *event, size_t msec, size_t usec)
{
	dev_info(ctrl->dev,
	    "%3zu.%03zums ACTV_END ret:%lld jiffies_left:%u/%u read_cnt:%u\n",
	    msec, usec, event->data0, event->data1,
	    I2C_MSM_MAX_POLL_MSEC, event->data2);
}

void i2c_msm_prof_dump_dma_flsh(struct i2c_msm_ctrl *ctrl,
		struct i2c_msm_prof_event *event, size_t msec, size_t usec)
{
	dev_info(ctrl->dev, "%3zu.%03zums  DMA_FLSH\n", msec, usec);
}

void i2c_msm_prof_dump_pip_dscn(struct i2c_msm_ctrl *ctrl,
		struct i2c_msm_prof_event *event, size_t msec, size_t usec)
{
	struct i2c_msm_dma_chan *chan =
			(struct i2c_msm_dma_chan *) ((ulong) event->data0);
	int ret = event->data1;
	dev_info(ctrl->dev,
		"%3zu.%03zums PIP_DCNCT sps_disconnect(hndl:0x%p %s):%d\n",
		msec, usec, chan->dma_chan, chan->name, ret);
}

void i2c_msm_prof_dump_pip_cnct(struct i2c_msm_ctrl *ctrl,
		struct i2c_msm_prof_event *event, size_t msec, size_t usec)
{
	struct i2c_msm_dma_chan *chan =
			(struct i2c_msm_dma_chan *) ((ulong) event->data0);
	int ret = event->data1;
	dev_info(ctrl->dev,
		"%3zu.%03zums PIP_CNCT sps_connect(hndl:0x%p %s):%d\n",
		msec, usec, chan->dma_chan, chan->name, ret);
}

void i2c_msm_prof_reset(struct i2c_msm_ctrl *ctrl,
		struct i2c_msm_prof_event *event, size_t msec, size_t usec)
{
	dev_info(ctrl->dev, "%3zu.%03zums  QUP_RSET\n", msec, usec);
}

/* string table for enum i2c_msm_err_bit_field */
const char * const i2c_msm_err_str_tbl[] = {
	"NONE", "NACK", "ARB_LOST" , "ARB_LOST + NACK", "BUS_ERR",
	"BUS_ERR + NACK", "BUS_ERR + ARB_LOST", "BUS_ERR + ARB_LOST + NACK",
	"TIMEOUT", "TIMEOUT + NACK", "TIMEOUT + ARB_LOST",
	"TIMEOUT + ARB_LOST + NACK", "TIMEOUT + BUS_ERR",
	"TIMEOUT + BUS_ERR + NACK" , "TIMEOUT + BUS_ERR + ARB_LOST",
	"TIMEOUT + BUS_ERR + ARB_LOST + NACK",
};

void i2c_msm_prof_dump_xfer_end(struct i2c_msm_ctrl *ctrl,
		struct i2c_msm_prof_event *event, size_t msec, size_t usec)
{
	int ret = event->data0;
	int err = event->data1;
	int bc  = ctrl->xfer.rx_cnt + ctrl->xfer.rx_ovrhd_cnt +
		  ctrl->xfer.tx_cnt + ctrl->xfer.tx_ovrhd_cnt;
	int bc_sec = (bc * 1000000) / (msec * 1000 + usec);
	const char *status = (!err && (ret == ctrl->xfer.msg_cnt)) ?
								"OK" : "FAIL";
	dev_info(ctrl->dev,
		"%3zu.%03zums XFER_END ret:%d err:[%s] msgs_sent:%d BC:%d B/sec:%d i2c-stts:%s\n" ,
		msec, usec, ret, i2c_msm_err_str_tbl[err], event->data2,
		bc, bc_sec, status);
}

void i2c_msm_prof_dump_irq_begn(struct i2c_msm_ctrl *ctrl,
		struct i2c_msm_prof_event *event, size_t msec, size_t usec)
{
	dev_info(ctrl->dev, "%3zu.%03zums  IRQ_BEG irq:%lld\n",
						msec, usec, event->data0);
}

void i2c_msm_prof_dump_irq_end(struct i2c_msm_ctrl *ctrl,
		struct i2c_msm_prof_event *event, size_t msec, size_t usec)
{
	char str[I2C_MSM_REG_2_STR_BUF_SZ];
	u32 mstr_stts = event->data0;
	u32 qup_oper  = event->data1;
	u32 err_flgs  = event->data2;
	dev_info(ctrl->dev,
		"%3zu.%03zums  IRQ_END MSTR_STTS:0x%x QUP_OPER:0x%x ERR_FLGS:0x%x\n",
		msec, usec, mstr_stts, qup_oper, err_flgs);

	/*
	 * Dump fields and flags only of registers with interesting info
	 * (i.e. errors).
	 */
	 /* register I2C_MASTER_STATUS */
	if (mstr_stts & QUP_MSTR_STTS_ERR_MASK) {
		i2c_msm_dbg_qup_reg_flds_to_str(
				mstr_stts, str, sizeof(str),
				i2c_msm_qup_i2c_stat_fields_map);

		dev_info(ctrl->dev, "            |->MSTR_STTS:0x%llx %s\n",
						event->data0, str);
	}
	/* register QUP_OPERATIONAL */
	if (qup_oper &
	   (QUP_OUTPUT_SERVICE_FLAG | QUP_INPUT_SERVICE_FLAG)) {

		i2c_msm_dbg_qup_reg_flds_to_str(
				qup_oper, str, sizeof(str),
				i2c_msm_qup_op_fields_map);

		dev_info(ctrl->dev, "            |-> QUP_OPER:0x%x %s\n",
						event->data1, str);
	}
	/* register ERR_FLAGS */
	if (err_flgs) {
		i2c_msm_dbg_qup_reg_flds_to_str(
				err_flgs, str, sizeof(str),
				i2c_msm_qup_err_flags_fields_map);

		dev_info(ctrl->dev, "            |-> ERR_FLGS:0x%x %s\n",
						event->data2, str);
	}
}

void i2c_msm_prof_dump_next_buf(struct i2c_msm_ctrl *ctrl,
		struct i2c_msm_prof_event *event, size_t msec, size_t usec)
{
	struct i2c_msg *msg = ctrl->xfer.msgs + event->data0;
	dev_info(ctrl->dev,
		"%3zu.%03zums XFER_BUF msg[%lld] pos:%d adr:0x%x len:%d is_rx:0x%x last:0x%x\n",
		msec, usec, event->data0, event->data1, msg->addr, msg->len,
		(msg->flags & I2C_M_RD),
		event->data0 == (ctrl->xfer.msg_cnt - 1));

}

void i2c_msm_prof_dump_scan_sum(struct i2c_msm_ctrl *ctrl,
		struct i2c_msm_prof_event *event, size_t msec, size_t usec)
{
	u32 bc_rx       = (event->data0 & 0xff);
	u32 bc_rx_ovrhd = (event->data0 >> 16);
	u32 bc_tx       = (event->data1 & 0xff);
	u32 bc_tx_ovrhd = (event->data1 >> 16);
	u32 timeout     = (event->data2 & 0xfff);
	u32 mode        = (event->data2 >> 24);
	u32 bc      = bc_rx + bc_rx_ovrhd + bc_tx + bc_tx_ovrhd;
	dev_info(ctrl->dev,
		"%3zu.%03zums SCN_SMRY BC:%u rx:%u+ovrhd:%u tx:%u+ovrhd:%u timeout:%umsec mode:%s\n",
		msec, usec, bc, bc_rx, bc_rx_ovrhd, bc_tx, bc_tx_ovrhd,
		jiffies_to_msecs(timeout), i2c_msm_mode_str_tbl[mode]);
}

void i2c_msm_prof_dump_cmplt_ok(struct i2c_msm_ctrl *ctrl,
		struct i2c_msm_prof_event *event, size_t msec, size_t usec)
{
	dev_info(ctrl->dev,
		"%3zu.%03zums  DONE_OK timeout-used:%umsec time_left:%umsec\n",
		msec, usec, jiffies_to_msecs(event->data0),
		jiffies_to_msecs(event->data1));
}

void i2c_msm_prof_dump_cmplt_fl(struct i2c_msm_ctrl *ctrl,
		struct i2c_msm_prof_event *event, size_t msec, size_t usec)
{
	dev_info(ctrl->dev,
		"%3zu.%03zums  TIMEOUT-error timeout-used:%umsec. Check GPIOs configuration\n",
		msec, usec, jiffies_to_msecs(event->data0));
}

void i2c_msm_prof_dump_vlid_end(struct i2c_msm_ctrl *ctrl,
		struct i2c_msm_prof_event *event, size_t msec, size_t usec)
{
	int  ret        = (int)(event->data0 & 0xff);
	enum i2c_msm_qup_state state = ((event->data0 << 16) & 0xf);
	u32  status     = event->data2;

	dev_info(ctrl->dev,
	"%3zu.%03zums SET_STTE set:%s ret:%d rd_cnt:%u reg_val:0x%x vld:%d\n",
	msec, usec, i2c_msm_reg_qup_state_to_str[state], ret,
	event->data1, status, BIT_IS_SET(status, 2));
}
/* match the corresponding prof event enum to the prof function declaration */
static i2c_msm_prof_dump_func_func_t event_dump_func_tbl[] = {
	[I2C_MSM_VALID_END]	= i2c_msm_prof_dump_vlid_end,
	[I2C_MSM_PIP_DSCN]	= i2c_msm_prof_dump_pip_dscn,
	[I2C_MSM_PIP_CNCT]	= i2c_msm_prof_dump_pip_cnct,
	[I2C_MSM_ACTV_END]	= i2c_msm_prof_dump_actv_end,
	[I2C_MSM_IRQ_BGN]	= i2c_msm_prof_dump_irq_begn,
	[I2C_MSM_IRQ_END]	= i2c_msm_prof_dump_irq_end,
	[I2C_MSM_XFER_BEG]	= i2c_msm_prof_dump_xfer_beg,
	[I2C_MSM_XFER_END]	= i2c_msm_prof_dump_xfer_end,
	[I2C_MSM_SCAN_SUM]	= i2c_msm_prof_dump_scan_sum,
	[I2C_MSM_NEXT_BUF]	= i2c_msm_prof_dump_next_buf,
	[I2C_MSM_COMPLT_OK]	= i2c_msm_prof_dump_cmplt_ok,
	[I2C_MSM_COMPLT_FL]	= i2c_msm_prof_dump_cmplt_fl,
	[I2C_MSM_PROF_RESET]	= i2c_msm_prof_reset,
};

/*
 * i2c_msm_prof_evnt_dump: post processing, msg formatting and dumping of events
 */
void i2c_msm_prof_evnt_dump(struct i2c_msm_ctrl *ctrl)
{
	size_t                     cnt   = atomic_read(&ctrl->xfer.event_cnt);
	struct i2c_msm_prof_event *event = ctrl->xfer.event;
	struct timespec            time0 = event->time;
	struct timespec            time_diff;
	size_t                     diff_usec;
	size_t                     diff_msec;
	i2c_msm_prof_dump_func_func_t func;

	for (; cnt; --cnt, ++event) {
		time_diff = timespec_sub(event->time, time0);
		diff_usec = time_diff.tv_sec  * USEC_PER_SEC +
			    time_diff.tv_nsec / NSEC_PER_USEC;
		diff_msec  = diff_usec / USEC_PER_MSEC;
		diff_usec -= diff_msec * USEC_PER_MSEC;

		func = event_dump_func_tbl[event->dump_func_id];
		func(ctrl, event, diff_msec, diff_usec);
	}
}
EXPORT_SYMBOL(i2c_msm_prof_evnt_dump);

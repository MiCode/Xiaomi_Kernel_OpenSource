#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include "ispv4_debugfs.h"
#include "media/ispv4_defs.h"

extern struct dentry *ispv4_debugfs;

static struct dentry *ispv4_pcie;
static struct dentry *dfile_hdma;
static struct dentry *dfile_pm;
static struct dentry *dfile_reset;
static struct dentry *dfile_bandwidth;

struct debugfs_regset32 *iatu_regset;
struct debugfs_regset32 *hdma_regset;

static const struct debugfs_reg32 hdma_regs[] = {
	{.name = "CH0_WR_HDMA_EN",			.offset = (HDMA_CH_DIR_BASE(0, 0) + 0x00),},
	{.name = "CH0_WR_HDMA_DOORBELL",		.offset = (HDMA_CH_DIR_BASE(0, 0) + 0x04),},
	{.name = "CH0_WR_HDMA_ELEM_PF",			.offset = (HDMA_CH_DIR_BASE(0, 0) + 0x08),},
	{.name = "CH0_WR_HDMA_LLP_LOW",			.offset = (HDMA_CH_DIR_BASE(0, 0) + 0x10),},
	{.name = "CH0_WR_HDMA_LLP_HIGH",		.offset = (HDMA_CH_DIR_BASE(0, 0) + 0x14),},
	{.name = "CH0_WR_HDMA_CYCLE",			.offset = (HDMA_CH_DIR_BASE(0, 0) + 0x18),},
	{.name = "CH0_WR_HDMA_XFERSIZE",		.offset = (HDMA_CH_DIR_BASE(0, 0) + 0x1c),},
	{.name = "CH0_WR_HDMA_SAR_LOW",			.offset = (HDMA_CH_DIR_BASE(0, 0) + 0x20),},
	{.name = "CH0_WR_HDMA_SAR_HIGH",		.offset = (HDMA_CH_DIR_BASE(0, 0) + 0x24),},
	{.name = "CH0_WR_HDMA_DAR_LOW",			.offset = (HDMA_CH_DIR_BASE(0, 0) + 0x28),},
	{.name = "CH0_WR_HDMA_DAR_HIGH",		.offset = (HDMA_CH_DIR_BASE(0, 0) + 0x2c),},
	{.name = "CH0_WR_HDMA_WATERMARK_EN",		.offset = (HDMA_CH_DIR_BASE(0, 0) + 0x30),},
	{.name = "CH0_WR_HDMA_CONTROL1",		.offset = (HDMA_CH_DIR_BASE(0, 0) + 0x34),},
	{.name = "CH0_WR_HDMA_FUNC_NUM",		.offset = (HDMA_CH_DIR_BASE(0, 0) + 0x38),},
	{.name = "CH0_WR_HDMA_QOS",			.offset = (HDMA_CH_DIR_BASE(0, 0) + 0x3c),},
	{.name = "CH0_WR_HDMA_STATUS",			.offset = (HDMA_CH_DIR_BASE(0, 0) + 0x80),},
	{.name = "CH0_WR_HDMA_INT_STATUS",		.offset = (HDMA_CH_DIR_BASE(0, 0) + 0x84),},
	{.name = "CH0_WR_HDMA_INT_SETUP",		.offset = (HDMA_CH_DIR_BASE(0, 0) + 0x88),},
	{.name = "CH0_WR_HDMA_INT_CLEAR",		.offset = (HDMA_CH_DIR_BASE(0, 0) + 0x8c),},
	{.name = "CH0_WR_HDMA_MSI_STOP_LOW",		.offset = (HDMA_CH_DIR_BASE(0, 0) + 0x90),},
	{.name = "CH0_WR_HDMA_MSI_STOP_HIGH",		.offset = (HDMA_CH_DIR_BASE(0, 0) + 0x94),},
	{.name = "CH0_WR_HDMA_MSI_WATERMARK_LOW",	.offset = (HDMA_CH_DIR_BASE(0, 0) + 0x98),},
	{.name = "CH0_WR_HDMA_MSI_WATERMARK_HIGH",	.offset = (HDMA_CH_DIR_BASE(0, 0) + 0x9c),},
	{.name = "CH0_WR_HDMA_MSI_ABORT_LOW",		.offset = (HDMA_CH_DIR_BASE(0, 0) + 0xa0),},
	{.name = "CH0_WR_HDMA_MSI_ABORT_HIGH",		.offset = (HDMA_CH_DIR_BASE(0, 0) + 0xa4),},
	{.name = "CH0_WR_HDMA_MSI_MSGD",		.offset = (HDMA_CH_DIR_BASE(0, 0) + 0xa8),},

	{.name = "CH1_WR_HDMA_EN",			.offset = (HDMA_CH_DIR_BASE(1, 0) + 0x00),},
	{.name = "CH1_WR_HDMA_DOORBELL",		.offset = (HDMA_CH_DIR_BASE(1, 0) + 0x04),},
	{.name = "CH1_WR_HDMA_ELEM_PF",			.offset = (HDMA_CH_DIR_BASE(1, 0) + 0x08),},
	{.name = "CH1_WR_HDMA_LLP_LOW",			.offset = (HDMA_CH_DIR_BASE(1, 0) + 0x10),},
	{.name = "CH1_WR_HDMA_LLP_HIGH",		.offset = (HDMA_CH_DIR_BASE(1, 0) + 0x14),},
	{.name = "CH1_WR_HDMA_CYCLE",			.offset = (HDMA_CH_DIR_BASE(1, 0) + 0x18),},
	{.name = "CH1_WR_HDMA_XFERSIZE",		.offset = (HDMA_CH_DIR_BASE(1, 0) + 0x1c),},
	{.name = "CH1_WR_HDMA_SAR_LOW",			.offset = (HDMA_CH_DIR_BASE(1, 0) + 0x20),},
	{.name = "CH1_WR_HDMA_SAR_HIGH",		.offset = (HDMA_CH_DIR_BASE(1, 0) + 0x24),},
	{.name = "CH1_WR_HDMA_DAR_LOW",			.offset = (HDMA_CH_DIR_BASE(1, 0) + 0x28),},
	{.name = "CH1_WR_HDMA_DAR_HIGH",		.offset = (HDMA_CH_DIR_BASE(1, 0) + 0x2c),},
	{.name = "CH1_WR_HDMA_WATERMARK_EN",		.offset = (HDMA_CH_DIR_BASE(1, 0) + 0x30),},
	{.name = "CH1_WR_HDMA_CONTROL1",		.offset = (HDMA_CH_DIR_BASE(1, 0) + 0x34),},
	{.name = "CH1_WR_HDMA_FUNC_NUM",		.offset = (HDMA_CH_DIR_BASE(1, 0) + 0x38),},
	{.name = "CH1_WR_HDMA_QOS",			.offset = (HDMA_CH_DIR_BASE(1, 0) + 0x3c),},
	{.name = "CH1_WR_HDMA_STATUS",			.offset = (HDMA_CH_DIR_BASE(1, 0) + 0x80),},
	{.name = "CH1_WR_HDMA_INT_STATUS",		.offset = (HDMA_CH_DIR_BASE(1, 0) + 0x84),},
	{.name = "CH1_WR_HDMA_INT_SETUP",		.offset = (HDMA_CH_DIR_BASE(1, 0) + 0x88),},
	{.name = "CH1_WR_HDMA_INT_CLEAR",		.offset = (HDMA_CH_DIR_BASE(1, 0) + 0x8c),},
	{.name = "CH1_WR_HDMA_MSI_STOP_LOW",		.offset = (HDMA_CH_DIR_BASE(1, 0) + 0x90),},
	{.name = "CH1_WR_HDMA_MSI_STOP_HIGH",		.offset = (HDMA_CH_DIR_BASE(1, 0) + 0x94),},
	{.name = "CH1_WR_HDMA_MSI_WATERMARK_LOW",	.offset = (HDMA_CH_DIR_BASE(1, 0) + 0x98),},
	{.name = "CH1_WR_HDMA_MSI_WATERMARK_HIGH",	.offset = (HDMA_CH_DIR_BASE(1, 0) + 0x9c),},
	{.name = "CH1_WR_HDMA_MSI_ABORT_LOW",		.offset = (HDMA_CH_DIR_BASE(1, 0) + 0xa0),},
	{.name = "CH1_WR_HDMA_MSI_ABORT_HIGH",		.offset = (HDMA_CH_DIR_BASE(1, 0) + 0xa4),},
	{.name = "CH1_WR_HDMA_MSI_MSGD",		.offset = (HDMA_CH_DIR_BASE(1, 1) + 0xa8),},

	{.name = "CH0_RD_HDMA_EN",			.offset = (HDMA_CH_DIR_BASE(0, 1) + 0x00),},
	{.name = "CH0_RD_HDMA_DOORBELL",		.offset = (HDMA_CH_DIR_BASE(0, 1) + 0x04),},
	{.name = "CH0_RD_HDMA_ELEM_PF",			.offset = (HDMA_CH_DIR_BASE(0, 1) + 0x08),},
	{.name = "CH0_RD_HDMA_LLP_LOW",			.offset = (HDMA_CH_DIR_BASE(0, 1) + 0x10),},
	{.name = "CH0_RD_HDMA_LLP_HIGH",		.offset = (HDMA_CH_DIR_BASE(0, 1) + 0x14),},
	{.name = "CH0_RD_HDMA_CYCLE",			.offset = (HDMA_CH_DIR_BASE(0, 1) + 0x18),},
	{.name = "CH0_RD_HDMA_XFERSIZE",		.offset = (HDMA_CH_DIR_BASE(0, 1) + 0x1c),},
	{.name = "CH0_RD_HDMA_SAR_LOW",			.offset = (HDMA_CH_DIR_BASE(0, 1) + 0x20),},
	{.name = "CH0_RD_HDMA_SAR_HIGH",		.offset = (HDMA_CH_DIR_BASE(0, 1) + 0x24),},
	{.name = "CH0_RD_HDMA_DAR_LOW",			.offset = (HDMA_CH_DIR_BASE(0, 1) + 0x28),},
	{.name = "CH0_RD_HDMA_DAR_HIGH",		.offset = (HDMA_CH_DIR_BASE(0, 1) + 0x2c),},
	{.name = "CH0_RD_HDMA_WATERMARK_EN",		.offset = (HDMA_CH_DIR_BASE(0, 1) + 0x30),},
	{.name = "CH0_RD_HDMA_CONTROL1",		.offset = (HDMA_CH_DIR_BASE(0, 1) + 0x34),},
	{.name = "CH0_RD_HDMA_FUNC_NUM",		.offset = (HDMA_CH_DIR_BASE(0, 1) + 0x38),},
	{.name = "CH0_RD_HDMA_QOS",			.offset = (HDMA_CH_DIR_BASE(0, 1) + 0x3c),},
	{.name = "CH0_RD_HDMA_STATUS",			.offset = (HDMA_CH_DIR_BASE(0, 1) + 0x80),},
	{.name = "CH0_RD_HDMA_INT_STATUS",		.offset = (HDMA_CH_DIR_BASE(0, 1) + 0x84),},
	{.name = "CH0_RD_HDMA_INT_SETUP",		.offset = (HDMA_CH_DIR_BASE(0, 1) + 0x88),},
	{.name = "CH0_RD_HDMA_INT_CLEAR",		.offset = (HDMA_CH_DIR_BASE(0, 1) + 0x8c),},
	{.name = "CH0_RD_HDMA_MSI_STOP_LOW",		.offset = (HDMA_CH_DIR_BASE(0, 1) + 0x90),},
	{.name = "CH0_RD_HDMA_MSI_STOP_HIGH",		.offset = (HDMA_CH_DIR_BASE(0, 1) + 0x94),},
	{.name = "CH0_RD_HDMA_MSI_WATERMARK_LOW",	.offset = (HDMA_CH_DIR_BASE(0, 1) + 0x98),},
	{.name = "CH0_RD_HDMA_MSI_WATERMARK_HIGH",	.offset = (HDMA_CH_DIR_BASE(0, 1) + 0x9c),},
	{.name = "CH0_RD_HDMA_MSI_ABORT_LOW",		.offset = (HDMA_CH_DIR_BASE(0, 1) + 0xa0),},
	{.name = "CH0_RD_HDMA_MSI_ABORT_HIGH",		.offset = (HDMA_CH_DIR_BASE(0, 1) + 0xa4),},
	{.name = "CH0_RD_HDMA_MSI_MSGD",		.offset = (HDMA_CH_DIR_BASE(0, 1) + 0xa8),},

	{.name = "CH1_RD_HDMA_EN",			.offset = (HDMA_CH_DIR_BASE(1, 1) + 0x00),},
	{.name = "CH1_RD_HDMA_DOORBELL",		.offset = (HDMA_CH_DIR_BASE(1, 1) + 0x04),},
	{.name = "CH1_RD_HDMA_ELEM_PF",			.offset = (HDMA_CH_DIR_BASE(1, 1) + 0x08),},
	{.name = "CH1_RD_HDMA_LLP_LOW",			.offset = (HDMA_CH_DIR_BASE(1, 1) + 0x10),},
	{.name = "CH1_RD_HDMA_LLP_HIGH",		.offset = (HDMA_CH_DIR_BASE(1, 1) + 0x14),},
	{.name = "CH1_RD_HDMA_CYCLE",			.offset = (HDMA_CH_DIR_BASE(1, 1) + 0x18),},
	{.name = "CH1_RD_HDMA_XFERSIZE",		.offset = (HDMA_CH_DIR_BASE(1, 1) + 0x1c),},
	{.name = "CH1_RD_HDMA_SAR_LOW",			.offset = (HDMA_CH_DIR_BASE(1, 1) + 0x20),},
	{.name = "CH1_RD_HDMA_SAR_HIGH",		.offset = (HDMA_CH_DIR_BASE(1, 1) + 0x24),},
	{.name = "CH1_RD_HDMA_DAR_LOW",			.offset = (HDMA_CH_DIR_BASE(1, 1) + 0x28),},
	{.name = "CH1_RD_HDMA_DAR_HIGH",		.offset = (HDMA_CH_DIR_BASE(1, 1) + 0x2c),},
	{.name = "CH1_RD_HDMA_WATERMARK_EN",		.offset = (HDMA_CH_DIR_BASE(1, 1) + 0x30),},
	{.name = "CH1_RD_HDMA_CONTROL1",		.offset = (HDMA_CH_DIR_BASE(1, 1) + 0x34),},
	{.name = "CH1_RD_HDMA_FUNC_NUM",		.offset = (HDMA_CH_DIR_BASE(1, 1) + 0x38),},
	{.name = "CH1_RD_HDMA_QOS",			.offset = (HDMA_CH_DIR_BASE(1, 1) + 0x3c),},
	{.name = "CH1_RD_HDMA_STATUS",			.offset = (HDMA_CH_DIR_BASE(1, 1) + 0x80),},
	{.name = "CH1_RD_HDMA_INT_STATUS",		.offset = (HDMA_CH_DIR_BASE(1, 1) + 0x84),},
	{.name = "CH1_RD_HDMA_INT_SETUP",		.offset = (HDMA_CH_DIR_BASE(1, 1) + 0x88),},
	{.name = "CH1_RD_HDMA_INT_CLEAR",		.offset = (HDMA_CH_DIR_BASE(1, 1) + 0x8c),},
	{.name = "CH1_RD_HDMA_MSI_STOP_LOW",		.offset = (HDMA_CH_DIR_BASE(1, 1) + 0x90),},
	{.name = "CH1_RD_HDMA_MSI_STOP_HIGH",		.offset = (HDMA_CH_DIR_BASE(1, 1) + 0x94),},
	{.name = "CH1_RD_HDMA_MSI_WATERMARK_LOW",	.offset = (HDMA_CH_DIR_BASE(1, 1) + 0x98),},
	{.name = "CH1_RD_HDMA_MSI_WATERMARK_HIGH",	.offset = (HDMA_CH_DIR_BASE(1, 1) + 0x9c),},
	{.name = "CH1_RD_HDMA_MSI_ABORT_LOW",		.offset = (HDMA_CH_DIR_BASE(1, 1) + 0xa0),},
	{.name = "CH1_RD_HDMA_MSI_ABORT_HIGH",		.offset = (HDMA_CH_DIR_BASE(1, 1) + 0xa4),},
	{.name = "CH1_RD_HDMA_MSI_MSGD",		.offset = (HDMA_CH_DIR_BASE(1, 1) + 0xa8),},
};

static const struct debugfs_reg32 iatu_regs[] = {
	{.name = "OUTBOUND0_IATU_REGION_CTRL_1",	.offset = 0x200 * 0 + 0x00,},
	{.name = "OUTBOUND0_IATU_REGION_CTRL_2",	.offset = 0x200 * 0 + 0x04,},
	{.name = "OUTBOUND0_IATU_LWR_BASE",		.offset = 0x200 * 0 + 0x08,},
	{.name = "OUTBOUND0_IATU_UPPER_BASE",		.offset = 0x200 * 0 + 0x0c,},
	{.name = "OUTBOUND0_IATU_LIMIT",		.offset = 0x200 * 0 + 0x10,},
	{.name = "OUTBOUND0_IATU_LWR_TARGET",		.offset = 0x200 * 0 + 0x14,},
	{.name = "OUTBOUND0_IATU_UPPER_TARGET",		.offset = 0x200 * 0 + 0x18,},

	{.name = "OUTBOUND1_IATU_REGION_CTRL_1",	.offset = 0x200 * 1 + 0x00,},
	{.name = "OUTBOUND1_IATU_REGION_CTRL_2",	.offset = 0x200 * 1 + 0x04,},
	{.name = "OUTBOUND1_IATU_LWR_BASE",		.offset = 0x200 * 1 + 0x08,},
	{.name = "OUTBOUND1_IATU_UPPER_BASE",		.offset = 0x200 * 1 + 0x0c,},
	{.name = "OUTBOUND1_IATU_LIMIT",		.offset = 0x200 * 1 + 0x10,},
	{.name = "OUTBOUND1_IATU_LWR_TARGET",		.offset = 0x200 * 1 + 0x14,},
	{.name = "OUTBOUND1_IATU_UPPER_TARGET",		.offset = 0x200 * 1 + 0x18,},

	{.name = "OUTBOUND2_IATU_REGION_CTRL_1",	.offset = 0x200 * 2 + 0x00,},
	{.name = "OUTBOUND2_IATU_REGION_CTRL_2",	.offset = 0x200 * 2 + 0x04,},
	{.name = "OUTBOUND2_IATU_LWR_BASE",		.offset = 0x200 * 2 + 0x08,},
	{.name = "OUTBOUND2_IATU_UPPER_BASE",		.offset = 0x200 * 2 + 0x0c,},
	{.name = "OUTBOUND2_IATU_LIMIT",		.offset = 0x200 * 2 + 0x10,},
	{.name = "OUTBOUND2_IATU_LWR_TARGET",		.offset = 0x200 * 2 + 0x14,},
	{.name = "OUTBOUND2_IATU_UPPER_TARGET",		.offset = 0x200 * 2 + 0x18,},

	{.name = "OUTBOUND3_IATU_REGION_CTRL_1",	.offset = 0x200 * 3 + 0x00,},
	{.name = "OUTBOUND3_IATU_REGION_CTRL_2",	.offset = 0x200 * 3 + 0x04,},
	{.name = "OUTBOUND3_IATU_LWR_BASE",		.offset = 0x200 * 3 + 0x08,},
	{.name = "OUTBOUND3_IATU_UPPER_BASE",		.offset = 0x200 * 3 + 0x0c,},
	{.name = "OUTBOUND3_IATU_LIMIT",		.offset = 0x200 * 3 + 0x10,},
	{.name = "OUTBOUND3_IATU_LWR_TARGET",		.offset = 0x200 * 3 + 0x14,},
	{.name = "OUTBOUND3_IATU_UPPER_TARGET",		.offset = 0x200 * 3 + 0x18,},

	{.name = "OUTBOUND4_IATU_REGION_CTRL_1",	.offset = 0x200 * 4 + 0x00,},
	{.name = "OUTBOUND4_IATU_REGION_CTRL_2",	.offset = 0x200 * 4 + 0x04,},
	{.name = "OUTBOUND4_IATU_LWR_BASE",		.offset = 0x200 * 4 + 0x08,},
	{.name = "OUTBOUND4_IATU_UPPER_BASE",		.offset = 0x200 * 4 + 0x0c,},
	{.name = "OUTBOUND4_IATU_LIMIT",		.offset = 0x200 * 4 + 0x10,},
	{.name = "OUTBOUND4_IATU_LWR_TARGET",		.offset = 0x200 * 4 + 0x14,},
	{.name = "OUTBOUND4_IATU_UPPER_TARGET",		.offset = 0x200 * 4 + 0x18,},

	{.name = "OUTBOUND5_IATU_REGION_CTRL_1",	.offset = 0x200 * 5 + 0x00,},
	{.name = "OUTBOUND5_IATU_REGION_CTRL_2",	.offset = 0x200 * 5 + 0x04,},
	{.name = "OUTBOUND5_IATU_LWR_BASE",		.offset = 0x200 * 5 + 0x08,},
	{.name = "OUTBOUND5_IATU_UPPER_BASE",		.offset = 0x200 * 5 + 0x0c,},
	{.name = "OUTBOUND5_IATU_LIMIT",		.offset = 0x200 * 5 + 0x10,},
	{.name = "OUTBOUND5_IATU_LWR_TARGET",		.offset = 0x200 * 5 + 0x14,},
	{.name = "OUTBOUND5_IATU_UPPER_TARGET",		.offset = 0x200 * 5 + 0x18,},

	{.name = "OUTBOUND6_IATU_REGION_CTRL_1",	.offset = 0x200 * 6 + 0x00,},
	{.name = "OUTBOUND6_IATU_REGION_CTRL_2",	.offset = 0x200 * 6 + 0x04,},
	{.name = "OUTBOUND6_IATU_LWR_BASE",		.offset = 0x200 * 6 + 0x08,},
	{.name = "OUTBOUND6_IATU_UPPER_BASE",		.offset = 0x200 * 6 + 0x0c,},
	{.name = "OUTBOUND6_IATU_LIMIT",		.offset = 0x200 * 6 + 0x10,},
	{.name = "OUTBOUND6_IATU_LWR_TARGET",		.offset = 0x200 * 6 + 0x14,},
	{.name = "OUTBOUND6_IATU_UPPER_TARGET",		.offset = 0x200 * 6 + 0x18,},

	{.name = "OUTBOUND7_IATU_REGION_CTRL_1",	.offset = 0x200 * 7 + 0x00,},
	{.name = "OUTBOUND7_IATU_REGION_CTRL_2",	.offset = 0x200 * 7 + 0x04,},
	{.name = "OUTBOUND7_IATU_LWR_BASE",		.offset = 0x200 * 7 + 0x08,},
	{.name = "OUTBOUND7_IATU_UPPER_BASE",		.offset = 0x200 * 7 + 0x0c,},
	{.name = "OUTBOUND7_IATU_LIMIT",		.offset = 0x200 * 7 + 0x10,},
	{.name = "OUTBOUND7_IATU_LWR_TARGET",		.offset = 0x200 * 7 + 0x14,},
	{.name = "OUTBOUND7_IATU_UPPER_TARGET",		.offset = 0x200 * 7 + 0x18,},

	{.name = "INBOUND0_IATU_REGION_CTRL_1",		.offset = 0x100 + 0x200 * 0 + 0x00,},
	{.name = "INBOUND0_IATU_REGION_CTRL_2",		.offset = 0x100 + 0x200 * 0 + 0x04,},
	{.name = "INBOUND0_IATU_LWR_BASE",		.offset = 0x100 + 0x200 * 0 + 0x08,},
	{.name = "INBOUND0_IATU_UPPER_BASE",		.offset = 0x100 + 0x200 * 0 + 0x0c,},
	{.name = "INBOUND0_IATU_LIMIT",			.offset = 0x100 + 0x200 * 0 + 0x10,},
	{.name = "INBOUND0_IATU_LWR_TARGET",		.offset = 0x100 + 0x200 * 0 + 0x14,},
	{.name = "INBOUND0_IATU_UPPER_TARGET",		.offset = 0x100 + 0x200 * 0 + 0x18,},

	{.name = "INBOUND1_IATU_REGION_CTRL_1",		.offset = 0x100 + 0x200 * 1 + 0x00,},
	{.name = "INBOUND1_IATU_REGION_CTRL_2",		.offset = 0x100 + 0x200 * 1 + 0x04,},
	{.name = "INBOUND1_IATU_LWR_BASE",		.offset = 0x100 + 0x200 * 1 + 0x08,},
	{.name = "INBOUND1_IATU_UPPER_BASE",		.offset = 0x100 + 0x200 * 1 + 0x0c,},
	{.name = "INBOUND1_IATU_LIMIT",			.offset = 0x100 + 0x200 * 1 + 0x10,},
	{.name = "INBOUND1_IATU_LWR_TARGET",		.offset = 0x100 + 0x200 * 1 + 0x14,},
	{.name = "INBOUND1_IATU_UPPER_TARGET",		.offset = 0x100 + 0x200 * 1 + 0x18,},

	{.name = "INBOUND2_IATU_REGION_CTRL_1",		.offset = 0x100 + 0x200 * 2 + 0x00,},
	{.name = "INBOUND2_IATU_REGION_CTRL_2",		.offset = 0x100 + 0x200 * 2 + 0x04,},
	{.name = "INBOUND2_IATU_LWR_BASE",		.offset = 0x100 + 0x200 * 2 + 0x08,},
	{.name = "INBOUND2_IATU_UPPER_BASE",		.offset = 0x100 + 0x200 * 2 + 0x0c,},
	{.name = "INBOUND2_IATU_LIMIT",			.offset = 0x100 + 0x200 * 2 + 0x10,},
	{.name = "INBOUND2_IATU_LWR_TARGET",		.offset = 0x100 + 0x200 * 2 + 0x14,},
	{.name = "INBOUND2_IATU_UPPER_TARGET",		.offset = 0x100 + 0x200 * 2 + 0x18,},

	{.name = "INBOUND3_IATU_REGION_CTRL_1",		.offset = 0x100 + 0x200 * 3 + 0x00,},
	{.name = "INBOUND3_IATU_REGION_CTRL_2",		.offset = 0x100 + 0x200 * 3 + 0x04,},
	{.name = "INBOUND3_IATU_LWR_BASE",		.offset = 0x100 + 0x200 * 3 + 0x08,},
	{.name = "INBOUND3_IATU_UPPER_BASE",		.offset = 0x100 + 0x200 * 3 + 0x0c,},
	{.name = "INBOUND3_IATU_LIMIT",			.offset = 0x100 + 0x200 * 3 + 0x10,},
	{.name = "INBOUND3_IATU_LWR_TARGET",		.offset = 0x100 + 0x200 * 3 + 0x14,},
	{.name = "INBOUND3_IATU_UPPER_TARGET",		.offset = 0x100 + 0x200 * 3 + 0x18,},

	{.name = "INBOUND4_IATU_REGION_CTRL_1",		.offset = 0x100 + 0x200 * 4 + 0x00,},
	{.name = "INBOUND4_IATU_REGION_CTRL_2",		.offset = 0x100 + 0x200 * 4 + 0x04,},
	{.name = "INBOUND4_IATU_LWR_BASE",		.offset = 0x100 + 0x200 * 4 + 0x08,},
	{.name = "INBOUND4_IATU_UPPER_BASE",		.offset = 0x100 + 0x200 * 4 + 0x0c,},
	{.name = "INBOUND4_IATU_LIMIT",			.offset = 0x100 + 0x200 * 4 + 0x10,},
	{.name = "INBOUND4_IATU_LWR_TARGET",		.offset = 0x100 + 0x200 * 4 + 0x14,},
	{.name = "INBOUND4_IATU_UPPER_TARGET",		.offset = 0x100 + 0x200 * 4 + 0x18,},

	{.name = "INBOUND5_IATU_REGION_CTRL_1",		.offset = 0x100 + 0x200 * 5 + 0x00,},
	{.name = "INBOUND5_IATU_REGION_CTRL_2",		.offset = 0x100 + 0x200 * 5 + 0x04,},
	{.name = "INBOUND5_IATU_LWR_BASE",		.offset = 0x100 + 0x200 * 5 + 0x08,},
	{.name = "INBOUND5_IATU_UPPER_BASE",		.offset = 0x100 + 0x200 * 5 + 0x0c,},
	{.name = "INBOUND5_IATU_LIMIT",			.offset = 0x100 + 0x200 * 5 + 0x10,},
	{.name = "INBOUND5_IATU_LWR_TARGET",		.offset = 0x100 + 0x200 * 5 + 0x14,},
	{.name = "INBOUND5_IATU_UPPER_TARGET",		.offset = 0x100 + 0x200 * 5 + 0x18,},

	{.name = "INBOUND6_IATU_REGION_CTRL_1",		.offset = 0x100 + 0x200 * 6 + 0x00,},
	{.name = "INBOUND6_IATU_REGION_CTRL_2",		.offset = 0x100 + 0x200 * 6 + 0x04,},
	{.name = "INBOUND6_IATU_LWR_BASE",		.offset = 0x100 + 0x200 * 6 + 0x08,},
	{.name = "INBOUND6_IATU_UPPER_BASE",		.offset = 0x100 + 0x200 * 6 + 0x0c,},
	{.name = "INBOUND6_IATU_LIMIT",			.offset = 0x100 + 0x200 * 6 + 0x10,},
	{.name = "INBOUND6_IATU_LWR_TARGET",		.offset = 0x100 + 0x200 * 6 + 0x14,},
	{.name = "INBOUND6_IATU_UPPER_TARGET",		.offset = 0x100 + 0x200 * 6 + 0x18,},

	{.name = "INBOUND7_IATU_REGION_CTRL_1",		.offset = 0x100 + 0x200 * 7 + 0x00,},
	{.name = "INBOUND7_IATU_REGION_CTRL_2",		.offset = 0x100 + 0x200 * 7 + 0x04,},
	{.name = "INBOUND7_IATU_LWR_BASE",		.offset = 0x100 + 0x200 * 7 + 0x08,},
	{.name = "INBOUND7_IATU_UPPER_BASE",		.offset = 0x100 + 0x200 * 7 + 0x0c,},
	{.name = "INBOUND7_IATU_LIMIT",			.offset = 0x100 + 0x200 * 7 + 0x10,},
	{.name = "INBOUND7_IATU_LWR_TARGET",		.offset = 0x100 + 0x200 * 7 + 0x14,},
	{.name = "INBOUND7_IATU_UPPER_TARGET",		.offset = 0x100 + 0x200 * 7 + 0x18,},
};


static int ispv4_debugfs_parse_input(const char __user *buf,
				     size_t count, unsigned int *data)
{
	unsigned long ret;
	char *str, *str_temp;

	str = kmalloc(count + 1, GFP_KERNEL);
	if (!str)
		return -ENOMEM;

	ret = copy_from_user(str, buf, count);
	if (ret) {
		kfree(str);
		return -EFAULT;
	}

	str[count] = 0;
	str_temp = str;

	ret = get_option(&str_temp, data);
	kfree(str);
	if (ret != 1)
		return -EINVAL;

	return 0;
}

static ssize_t ispv4_debugfs_reset(struct file *file,
				   const char __user *buf,
				   size_t count, loff_t *ppos)
{
	struct ispv4_data *priv = file->private_data;
	struct pci_dev *rc_dev;
	u32 reset = 0;
	int ret;

	if (!priv)
		return -EINVAL;

	ret = ispv4_debugfs_parse_input(buf, count, &reset);
	if (ret)
		return ret;

	if (reset) {
		pr_alert("reset pci bus:\n");
		rc_dev = pcie_find_root_port(priv->pci);
		pci_reset_secondary_bus(rc_dev);
	}

	return count;
}

static const struct file_operations ispv4_debugfs_reset_ops = {
	.open = simple_open,
	.llseek = generic_file_llseek,
	.write = ispv4_debugfs_reset,
};

static void ispv4_pm_case_select(struct ispv4_data *priv, u32 ispv4_pm)
{
	int ret;

	if (!priv)
		return;

	switch (ispv4_pm) {
	case ISPV4_PCIE_DISABLE:
		pr_alert("Disable pcie link:\n");
		ret = ispv4_suspend_pci_link(priv);
		if (ret)
			pr_alert("suspend pcie link failed !\n");
		break;

	case ISPV4_PCIE_ENABLE:
		pr_alert("Enable pcie link:\n");
		ret = ispv4_resume_pci_link(priv);
		if (ret)
			pr_alert("resume pcie link failed !\n");
		break;
	case ISPV4_PCIE_FORCE_DISABLE:
		pr_alert("Force disable pcie link:\n");
		ret = ispv4_suspend_pci_force(priv);
		if (ret)
			pr_alert("resume pcie link failed !\n");
		break;

	default:
		pr_alert("Invalid pcie pm operation:\n");
		break;
	}
}

static ssize_t ispv4_debugfs_pm_case(struct file *file,
				     const char __user *buf,
				     size_t count, loff_t *ppos)
{
	struct seq_file	*s = file->private_data;
	struct ispv4_data *priv = s->private;
	u32 ispv4_pm = 0;
	int ret;

	if (!priv)
		return -EINVAL;

	ret = ispv4_debugfs_parse_input(buf, count, &ispv4_pm);
	if (ret)
		return ret;

	pr_alert("pcie pm case:\n");

	ispv4_pm_case_select(priv, ispv4_pm);

	return count;
}

static int ispv4_debugfs_pm_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < ISPV4_PCIE_MAX_OPTION; i++)
		seq_printf(m, "\t%d:\t %s\n", i, ispv4_pm_debugfs_option_desc[i]);

	return 0;
}

static int ispv4_debugfs_pm_open(struct inode *inode,
				 struct file *file)
{
	return single_open(file, ispv4_debugfs_pm_show, inode->i_private);
}

static const struct file_operations ispv4_debugfs_pm_ops = {
	.owner = THIS_MODULE,
	.open = ispv4_debugfs_pm_open,
	.read = seq_read,
	.llseek	= seq_lseek,
	.write = ispv4_debugfs_pm_case,
	.release = single_release,
};

static void ispv4_hdma_case_select(struct pcie_hdma *hdma, u32 ispv4_hdma)
{
	enum pcie_hdma_dir dir;
	int ret;

	if (!hdma)
		return;

	switch (ispv4_hdma) {
	case HDMA_SINGLE_READ:
		pr_alert("hdma single read case:\n");
		dir = HDMA_TO_DEVICE;
		ret = ispv4_hdma_single_transfer(hdma, dir);
		if (ret)
			pr_alert("hdma single read failed !\n");
		break;

	case HDMA_SINGLE_WRITE:
		pr_alert("hdma single write case:\n");
		dir = HDMA_FROM_DEVICE;
		ret = ispv4_hdma_single_transfer(hdma, dir);
		if (ret)
			pr_alert("hdma single write failed !\n");
		break;

	case HDMA_LL_READ:
		pr_alert("hdma ll read case:\n");
		dir = HDMA_TO_DEVICE;
		ret = ispv4_hdma_ll_transfer(hdma, dir);
		if (ret)
			pr_alert("hdma ll read failed !\n");
		break;

	case HDMA_LL_WRITE:
		pr_alert("hdma ll write case:\n");
		dir = HDMA_FROM_DEVICE;
		ret = ispv4_hdma_ll_transfer(hdma, dir);
		if (ret)
			pr_alert("hdma ll write failed !\n");
		break;

	default:
		pr_alert("Invalid dma case: %d.\n", ispv4_hdma);
		break;
	}
}

static ssize_t ispv4_debugfs_hdma_case(struct file *file,
				       const char __user *buf,
				       size_t count, loff_t *ppos)
{
	struct seq_file	*s = file->private_data;
	struct pcie_hdma *hdma = s->private;
	u32 ispv4_hdma = 0;
	int ret;

	if (!hdma)
		return -EINVAL;

	ret = ispv4_debugfs_parse_input(buf, count, &ispv4_hdma);
	if (ret)
		return ret;

	pr_alert("hdma test case:\n");

	ispv4_hdma_case_select(hdma, ispv4_hdma);

	return count;
}

static int ispv4_debugfs_hdma_case_dump(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < HDMA_MAX_DEBUGFS_OPTION; i++)
		seq_printf(m, "\t%d:\t %s\n", i, ispv4_hdma_debugfs_option_desc[i]);

	return 0;
}

static int ispv4_debugfs_hdma_open(struct inode *inode, struct file *file)
{
	return single_open(file, ispv4_debugfs_hdma_case_dump, inode->i_private);
}

static const struct file_operations ispv4_debugfs_hdma_ops = {
	.owner = THIS_MODULE,
	.open = ispv4_debugfs_hdma_open,
	.read = seq_read,
	.llseek	= seq_lseek,
	.release = single_release,
	.write = ispv4_debugfs_hdma_case,
};

int ispv4_debugfs_add_pcie_dump_iatu_hdma(struct ispv4_data *priv)
{
	if (!priv)
		return -EINVAL;

	iatu_regset = devm_kzalloc(&priv->pci->dev, sizeof(struct debugfs_regset32),
				   GFP_KERNEL);
	if (!iatu_regset)
		return -ENOMEM;

	iatu_regset->regs = iatu_regs;
	iatu_regset->nregs = ARRAY_SIZE(iatu_regs);
	iatu_regset->base = priv->base_bar[3] + 0x4000;

	hdma_regset = devm_kzalloc(&priv->pci->dev, sizeof(struct debugfs_regset32),
				   GFP_KERNEL);
	if (!hdma_regset)
		return -ENOMEM;

	hdma_regset->regs = hdma_regs;
	hdma_regset->nregs = ARRAY_SIZE(hdma_regs);
	hdma_regset->base = priv->base_bar[3];

	debugfs_create_regset32("iatu_regdump", 0444, ispv4_pcie, iatu_regset);
	debugfs_create_regset32("hdma_regdump", 0444, ispv4_pcie, hdma_regset);

	return 0;
}

static ssize_t ispv4_debugfs_bw_case(struct file *file, const char __user *buf,
				     size_t count, loff_t *ppos)
{
	struct ispv4_data *priv = file->private_data;
	int ret;
	int bw[3] = { 0 };
	char kbuf[16];
	u8 speed, width;

	if (!priv || count > 16)
		return -EINVAL;

	ret = copy_from_user(kbuf, buf, 16);
	if (ret != 0)
		return -EFAULT;

	(void)get_options(kbuf, 3, bw);

	speed = bw[1];
	width = bw[2];

	if (speed == 0 || width == 0)
		return -EINVAL;

	pr_alert("ispv4 pcie bw case:\n");
	pr_alert("ispv4 pcie config bw set width: %d \n", speed);
	pr_alert("ispv4 pcie config bw set speed: %d \n", width);
	ret = msm_pcie_set_link_bandwidth(priv->pci, speed, width);
	pr_alert("ispv4 pcie config bw ret %d:\n", ret);

	return count;
}

static const struct file_operations ispv4_debugfs_bw_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = ispv4_debugfs_bw_case,
};

void ispv4_debugfs_add_pcie_pm(struct ispv4_data *priv)
{
	if (!priv)
		return;

	dfile_pm = debugfs_create_file("pm", 0664,
					ispv4_pcie, priv,
					&ispv4_debugfs_pm_ops);
	if (IS_ERR_OR_NULL(dfile_pm)) {
		pr_err("PCIe: fail to create the pm file for debug_fs.\n");
		debugfs_remove_recursive(ispv4_pcie);
	}
}

void ispv4_debugfs_add_pcie_bandwidth(struct ispv4_data *priv)
{
	if (!priv)
		return;

	dfile_bandwidth = debugfs_create_file("set_bw", 0444, ispv4_pcie, priv,
					      &ispv4_debugfs_bw_ops);
	if (IS_ERR_OR_NULL(dfile_bandwidth)) {
		pr_err("PCIe: fail to create the bw file for debug_fs.\n");
		debugfs_remove_recursive(ispv4_pcie);
	}
}

void ispv4_debugfs_add_pcie_reset(struct ispv4_data *priv)
{
	if (!priv)
		return;

	dfile_reset = debugfs_create_file("reset", 0664,
					  ispv4_pcie, priv,
					  &ispv4_debugfs_reset_ops);
	if (IS_ERR_OR_NULL(dfile_reset)) {
		pr_err("PCIe: fail to create the reset file for debug_fs.\n");
		debugfs_remove_recursive(ispv4_pcie);
	}
}

void ispv4_debugfs_add_pcie_hdma(struct pcie_hdma *hdma)
{
	if (!hdma)
		return;

	dfile_hdma = debugfs_create_file("hdma", 0664,
					 ispv4_pcie, hdma,
					 &ispv4_debugfs_hdma_ops);
	if (IS_ERR_OR_NULL(dfile_hdma)) {
		pr_err("PCIe: fail to create the hdma file for debug_fs.\n");
		debugfs_remove_recursive(ispv4_pcie);
	}
}

void ispv4_debugfs_add_pcie(void)
{
	ispv4_pcie = debugfs_create_dir("ispv4_pcie", ispv4_debugfs);
	if (IS_ERR_OR_NULL(ispv4_pcie)) {
		pr_err("fail to create the ispv4_pcie folder for debug_fs.\n");
		debugfs_remove_recursive(ispv4_debugfs);
	}
}

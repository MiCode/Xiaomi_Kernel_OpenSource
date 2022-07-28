// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: Fish Wu <fish.wu@mediatek.com>
 *
 */

#include "mtk_aie.h"
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/device.h>
#include <linux/dma-heap.h>
#include "mtk_heap.h"
#include <uapi/linux/dma-heap.h>
#include <linux/scatterlist.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <aie_mp_fw/config/dma_def.h>
#include <aie_mp_fw/kernel/dma_def.h>
#include <aie_mp_fw/all_header.h>
#include "cmdq-sec.h"
#include "cmdq-sec-iwc-common.h"


#define FDVT_USE_GCE 1
#define FLD
#define FLD_ALIGN 128
#define CHECK_SERVICE_0 0
#define BUFTAG "AIE"
//#include <mtkcam-hwcore/imgsys/inc/drv/gce/mt6983/gce_module.h>

struct cmdq_pkt *g_sec_pkt;

static const unsigned int fd_wdma_en[fd_loop_num][output_WDMA_WRA_num] = {
	{1, 0, 0, 0}, {1, 0, 1, 0}, {1, 0, 1, 0}, {1, 0, 0, 0}, {1, 1, 1, 1},
	{1, 1, 1, 1}, {1, 0, 0, 0}, {1, 0, 1, 0}, {1, 1, 0, 0}, {1, 0, 0, 0},
	{1, 0, 1, 0}, {1, 1, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0},
	{1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 1, 0, 0}, {1, 1, 0, 0},
	{1, 1, 0, 0}, {1, 0, 0, 0}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 0, 0},
	{1, 1, 0, 0}, {1, 1, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0},
	{1, 0, 1, 0}, {1, 0, 1, 0}, {1, 0, 0, 0}, {1, 1, 1, 1}, {1, 1, 1, 1},
	{1, 0, 0, 0}, {1, 0, 1, 0}, {1, 1, 0, 0}, {1, 0, 0, 0}, {1, 0, 1, 0},
	{1, 1, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0},
	{1, 0, 0, 0}, {1, 0, 0, 0}, {1, 1, 0, 0}, {1, 1, 0, 0}, {1, 1, 0, 0},
	{1, 0, 0, 0}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 0, 0}, {1, 1, 0, 0},
	{1, 1, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 1, 0},
	{1, 0, 1, 0}, {1, 0, 0, 0}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 0, 0, 0},
	{1, 0, 1, 0}, {1, 1, 0, 0}, {1, 0, 0, 0}, {1, 0, 1, 0}, {1, 1, 0, 0},
	{1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0},
	{1, 0, 0, 0}, {1, 1, 0, 0}, {1, 1, 0, 0}, {1, 1, 0, 0}, {1, 0, 0, 0},
	{1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 0, 0}, {1, 1, 0, 0}, {1, 1, 0, 0},
	{1, 0, 0, 0}, {1, 0, 0, 0} };

static const unsigned int out_stride_size[fd_loop_num][output_WDMA_WRA_num] = {
	{1, 0, 0, 0}, {1, 0, 2, 0}, {1, 0, 2, 0}, {1, 0, 0, 0}, {1, 1, 2, 2},
	{1, 1, 2, 2}, {1, 0, 0, 0}, {1, 0, 2, 0}, {1, 1, 0, 0}, {1, 0, 0, 0},
	{1, 0, 2, 0}, {1, 1, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0},
	{1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 1, 0, 0}, {1, 1, 0, 0},
	{1, 1, 0, 0}, {1, 0, 0, 0}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 0, 0},
	{1, 1, 0, 0}, {1, 1, 0, 0}, {1, 0, 0, 0}, {3, 0, 0, 0}, {1, 0, 0, 0},
	{1, 0, 2, 0}, {1, 0, 2, 0}, {1, 0, 0, 0}, {1, 1, 2, 2}, {1, 1, 2, 2},
	{1, 0, 0, 0}, {1, 0, 2, 0}, {1, 1, 0, 0}, {1, 0, 0, 0}, {1, 0, 2, 0},
	{1, 1, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0},
	{1, 0, 0, 0}, {1, 0, 0, 0}, {1, 1, 0, 0}, {1, 1, 0, 0}, {1, 1, 0, 0},
	{1, 0, 0, 0}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 0, 0}, {1, 1, 0, 0},
	{1, 1, 0, 0}, {1, 0, 0, 0}, {3, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 2, 0},
	{1, 0, 2, 0}, {1, 0, 0, 0}, {1, 1, 2, 2}, {1, 1, 2, 2}, {1, 0, 0, 0},
	{1, 0, 2, 0}, {1, 1, 0, 0}, {1, 0, 0, 0}, {1, 0, 2, 0}, {1, 1, 0, 0},
	{1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0},
	{1, 0, 0, 0}, {1, 1, 0, 0}, {1, 1, 0, 0}, {1, 1, 0, 0}, {1, 0, 0, 0},
	{1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 0, 0}, {1, 1, 0, 0}, {1, 1, 0, 0},
	{1, 0, 0, 0}, {3, 0, 0, 0} };

static const unsigned int fd_ker_rdma_size[fd_loop_num][kernel_RDMA_RA_num] = {
	{240, 240},   {1168, 1168}, {1168, 1168}, {272, 272},   {2320, 2320},
	{2080, 2080}, {1040, 1040}, {4624, 4624}, {3104, 3104}, {9232, 9232},
	{4624, 4624}, {4128, 4128}, {1040, 1040}, {4624, 4624}, {4624, 4624},
	{1552, 1552}, {4624, 4624}, {4624, 4624}, {4128, 4128}, {1040, 1040},
	{1040, 1040}, {528, 528},   {4160, 4160}, {4160, 4160}, {2080, 2080},
	{2080, 2080}, {2080, 2080}, {1040, 1040}, {0, 0},       {240, 240},
	{1168, 1168}, {1168, 1168}, {272, 272},   {2320, 2320}, {2080, 2080},
	{1040, 1040}, {4624, 4624}, {3104, 3104}, {9232, 9232}, {4624, 4624},
	{4128, 4128}, {1040, 1040}, {4624, 4624}, {4624, 4624}, {1552, 1552},
	{4624, 4624}, {4624, 4624}, {4128, 4128}, {1040, 1040}, {1040, 1040},
	{528, 528},   {4160, 4160}, {4160, 4160}, {2080, 2080}, {2080, 2080},
	{2080, 2080}, {1040, 1040}, {0, 0},       {240, 240},   {1168, 1168},
	{1168, 1168}, {272, 272},   {2320, 2320}, {2080, 2080}, {1040, 1040},
	{4624, 4624}, {3104, 3104}, {9232, 9232}, {4624, 4624}, {4128, 4128},
	{1040, 1040}, {4624, 4624}, {4624, 4624}, {1552, 1552}, {4624, 4624},
	{4624, 4624}, {4128, 4128}, {1040, 1040}, {1040, 1040}, {528, 528},
	{4160, 4160}, {4160, 4160}, {2080, 2080}, {2080, 2080}, {2080, 2080},
	{1040, 1040}, {0, 0} };

static const unsigned int fd_out_stride2_in[fd_loop_num] = {
	0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
	0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static const unsigned int fd_stride[fd_loop_num] = {
	2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

static const unsigned int fd_maxpool[fd_loop_num] = {
	0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static const unsigned int out_2size[fd_loop_num] = {
	0, 1, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 1,
	0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static const unsigned int in_ch_pack[fd_loop_num] = {
	1, 16, 16, 16, 16, 16, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
	32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 0,  1,  16, 16, 16, 16, 16, 32,
	32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
	32, 32, 32, 0,  1,  16, 16, 16, 16, 16, 32, 32, 32, 32, 32, 32, 32, 32,
	32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 0};

static const unsigned int outlayer[fd_loop_num] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0};
static const unsigned int out_ch_pack[fd_loop_num] = {
	16, 16, 16, 16, 16, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
	32, 32, 32, 32, 16, 16, 16, 32, 32, 32, 32, 32, 32, 0,  16,
	16, 16, 16, 16, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
	32, 32, 32, 16, 16, 16, 32, 32, 32, 32, 32, 32, 0,  16, 16,
	16, 16, 16, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
	32, 32, 16, 16, 16, 32, 32, 32, 32, 32, 32, 0};

static const unsigned int anchor_en_num[fd_loop_num] = {
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5,  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};

/* [loop][ch][output_index] */
static const signed int fd_rdma_en[fd_loop_num][input_WDMA_WRA_num][2] = {
	{{99, 99}, {99, 99}, {99, 99}, {-1, -1} },
	{{0, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{1, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{1, 0}, {2, 0}, {-1, -1}, {-1, -1} },
	{{3, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{1, 2}, {2, 2}, {4, 2}, {4, 3} },
	{{5, 0}, {5, 1}, {-1, -1}, {-1, -1} },
	{{6, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{5, 0}, {5, 1}, {7, 0}, {-1, -1} },
	{{8, 0}, {8, 1}, {-1, -1}, {-1, -1} },
	{{9, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{5, 2}, {5, 3}, {7, 2}, {10, 2} },
	{{11, 0}, {11, 1}, {-1, -1}, {-1, -1} },
	{{12, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{13, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{11, 0}, {11, 1}, {14, 0}, {-1, -1} },
	{{15, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{16, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{11, 0}, {11, 1}, {14, 0}, {17, 0} },
	{{18, 0}, {18, 1}, {-1, -1}, {-1, -1} },
	{{18, 0}, {18, 1}, {-1, -1}, {-1, -1} },
	{{18, 0}, {18, 1}, {-1, -1}, {-1, -1} },
	{{18, 0}, {18, 1}, {-1, -1}, {-1, -1} },
	{{18, 0}, {18, 1}, {-1, -1}, {-1, -1} },
	{{18, 0}, {18, 1}, {-1, -1}, {-1, -1} },
	{{18, 0}, {18, 1}, {-1, -1}, {-1, -1} },
	{{18, 0}, {18, 1}, {-1, -1}, {-1, -1} },
	{{18, 0}, {18, 1}, {-1, -1}, {-1, -1} },
	{{19, 0}, {22, 0}, {22, 1}, {25, 0} },
	{{99, 99}, {99, 99}, {99, 99}, {-1, -1} },
	{{29, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{30, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{30, 0}, {31, 0}, {-1, -1}, {-1, -1} },
	{{32, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{30, 2}, {31, 2}, {33, 2}, {33, 3} },
	{{34, 0}, {34, 1}, {-1, -1}, {-1, -1} },
	{{35, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{34, 0}, {34, 1}, {36, 0}, {-1, -1} },
	{{37, 0}, {37, 1}, {-1, -1}, {-1, -1} },
	{{38, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{34, 2}, {34, 3}, {36, 2}, {39, 2} },
	{{40, 0}, {40, 1}, {-1, -1}, {-1, -1} },
	{{41, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{42, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{40, 0}, {40, 1}, {43, 0}, {-1, -1} },
	{{44, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{45, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{40, 0}, {40, 1}, {43, 0}, {46, 0} },
	{{47, 0}, {47, 1}, {-1, -1}, {-1, -1} },
	{{47, 0}, {47, 1}, {-1, -1}, {-1, -1} },
	{{47, 0}, {47, 1}, {-1, -1}, {-1, -1} },
	{{47, 0}, {47, 1}, {-1, -1}, {-1, -1} },
	{{47, 0}, {47, 1}, {-1, -1}, {-1, -1} },
	{{47, 0}, {47, 1}, {-1, -1}, {-1, -1} },
	{{47, 0}, {47, 1}, {-1, -1}, {-1, -1} },
	{{47, 0}, {47, 1}, {-1, -1}, {-1, -1} },
	{{47, 0}, {47, 1}, {-1, -1}, {-1, -1} },
	{{48, 0}, {51, 0}, {51, 1}, {54, 0} },
	{{99, 99}, {99, 99}, {99, 99}, {-1, -1} },
	{{58, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{59, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{59, 0}, {60, 0}, {-1, -1}, {-1, -1} },
	{{61, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{59, 2}, {60, 2}, {62, 2}, {62, 3} },
	{{63, 0}, {63, 1}, {-1, -1}, {-1, -1} },
	{{64, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{63, 0}, {63, 1}, {65, 0}, {-1, -1} },
	{{66, 0}, {66, 1}, {-1, -1}, {-1, -1} },
	{{67, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{63, 2}, {63, 3}, {65, 2}, {68, 2} },
	{{69, 0}, {69, 1}, {-1, -1}, {-1, -1} },
	{{70, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{71, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{69, 0}, {69, 1}, {72, 0}, {-1, -1} },
	{{73, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{74, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{69, 0}, {69, 1}, {72, 0}, {75, 0} },
	{{76, 0}, {76, 1}, {-1, -1}, {-1, -1} },
	{{76, 0}, {76, 1}, {-1, -1}, {-1, -1} },
	{{76, 0}, {76, 1}, {-1, -1}, {-1, -1} },
	{{76, 0}, {76, 1}, {-1, -1}, {-1, -1} },
	{{76, 0}, {76, 1}, {-1, -1}, {-1, -1} },
	{{76, 0}, {76, 1}, {-1, -1}, {-1, -1} },
	{{76, 0}, {76, 1}, {-1, -1}, {-1, -1} },
	{{76, 0}, {76, 1}, {-1, -1}, {-1, -1} },
	{{76, 0}, {76, 1}, {-1, -1}, {-1, -1} },
	{{77, 0}, {80, 0}, {80, 1}, {83, 0} } };

static const unsigned int attr_wdma_en[attr_loop_num][output_WDMA_WRA_num] = {
	{1, 0, 1, 0}, {1, 0, 1, 0}, {1, 0, 0, 0}, {1, 1, 1, 1}, {1, 1, 1, 1},
	{1, 0, 1, 0}, {1, 1, 0, 0}, {1, 0, 1, 0}, {1, 1, 0, 0}, {1, 0, 0, 0},
	{1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0},
	{1, 1, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0},
	{1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0},
	{1, 0, 0, 0} };
static const unsigned int
	attr_ker_rdma_size[attr_loop_num][kernel_RDMA_RA_num] = {
		{240, 240},   {1168, 1168}, {272, 272},   {2320, 2320},
		{2080, 2080}, {9232, 9232}, {3104, 3104}, {9232, 9232},
		{4128, 4128}, {1040, 1040}, {4624, 4624}, {4624, 4624},
		{1552, 1552}, {4624, 4624}, {4624, 4624}, {4128, 4128},
		{9232, 9232}, {272, 272},   {9232, 9232}, {2320, 2320},
		{144, 144},   {9232, 9232}, {272, 272},   {9232, 9232},
		{2320, 2320}, {144, 144} };
static const unsigned int attr_out_stride2_as_in[attr_loop_num] = {
	0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const unsigned int attr_fd_stride[attr_loop_num] = {/* H */
							   2, 1, 1, 1, 1, 1, 1,
							   1, 1, 1, 1, 1, 1, 1,
							   1, 1, 1, 1, 1, 1, 1,
							   1, 1, 1, 1, 1};
static const unsigned int attr_fd_maxpool[attr_loop_num] = {/* L */
							    1, 0, 0, 0, 0, 0, 0,
							    0, 0, 0, 0, 0, 0, 0,
							    0, 0, 0, 0, 0, 0, 0,
							    0, 0, 0, 0, 0};
static const unsigned int attr_out_2size[attr_loop_num] = {/* O */
							   1, 1, 0, 1, 1, 1, 0,
							   1, 0, 0, 0, 0, 0, 0,
							   0, 0, 0, 0, 0, 0, 0,
							   0, 0, 0, 0, 0};
static const unsigned int attr_input_ch_pack[attr_loop_num] = {
	1,  16, 16, 16, 16, 32, 32, 32, 32, 32, 32, 32, 32,
	32, 32, 32, 32, 32, 32, 32, 16, 32, 32, 32, 32, 16};
/* [loop][ch][output_index] */
static const signed int attr_rdma_en[attr_loop_num][input_WDMA_WRA_num][2] = {
	{{99, 99}, {99, 99}, {99, 99}, {-1, -1} },
	{{0, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{0, 0}, {1, 0}, {-1, -1}, {-1, -1} },
	{{2, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{0, 2}, {1, 2}, {3, 2}, {3, 3} },
	{{4, 0}, {4, 1}, {-1, -1}, {-1, -1} },
	{{4, 0}, {4, 1}, {5, 0}, {-1, -1} },
	{{6, 0}, {6, 1}, {-1, -1}, {-1, -1} },
	{{4, 2}, {4, 3}, {5, 2}, {7, 2} },
	{{8, 0}, {8, 1}, {-1, -1}, {-1, -1} },
	{{9, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{10, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{8, 0}, {8, 1}, {11, 0}, {-1, -1} },
	{{12, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{13, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{8, 0}, {8, 1}, {11, 0}, {14, 0} },
	{{15, 0}, {15, 1}, {-1, -1}, {-1, -1} },
	{{16, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{15, 0}, {15, 1}, {-1, -1}, {-1, -1} },
	{{18, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{19, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{15, 0}, {15, 1}, {-1, -1}, {-1, -1} },
	{{21, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{15, 0}, {15, 1}, {-1, -1}, {-1, -1} },
	{{23, 0}, {-1, -1}, {-1, -1}, {-1, -1} },
	{{24, 0}, {-1, -1}, {-1, -1}, {-1, -1} } };

static const unsigned int attr_wdma_size[attr_loop_num][output_WDMA_WRA_num] = {
	{16384, 0, 4096, 0},
	{16384, 0, 4096, 0},
	{16384, 0, 0, 0},
	{16384, 16384, 4096, 4096},
	{8192, 8192, 2048, 2048},
	{8192, 0, 2048, 0},
	{8192, 8192, 0, 0},
	{8192, 0, 2048, 0},
	{2048, 2048, 0, 0},
	{2048, 0, 0, 0},
	{2048, 0, 0, 0},
	{2048, 0, 0, 0},
	{2048, 0, 0, 0},
	{2048, 0, 0, 0},
	{2048, 0, 0, 0},
	{2048, 2048, 0, 0},
	{2048, 0, 0, 0},
	{0, 0, 0, 0},
	{2048, 0, 0, 0},
	{1024, 0, 0, 0},
	{0, 0, 0, 0},
	{2048, 0, 0, 0},
	{0, 0, 0, 0},
	{2048, 0, 0, 0},
	{1024, 0, 0, 0},
	{0, 0, 0, 0} };
/* (128-bits ALIGN work-around)*/
#define fld_blink_weight_size 6528 //6416 +(128-(6416%128))%128
#define fld_blink_weight_size_non_align 6416
#define fld_cv_size 1280
#define fld_cv_size_00 1536
#define fld_cv_size_00_non_align 1472
#define fld_fp_size 5376 //5344+(128-(5344%128))%128
#define fld_fp_size_non_align 5344
#define fld_leafnode_size 307200
#define fld_tree_size 8064 //8000 +(128-(8000%128))%128
#define fld_tree_size_non_align 8000
#define fld_result_size 112
#define fld_forest 14
#define fld_point 500
#define fld_cur_landmark 11
#define CHECK_SERVICE_IF_0 0

#if CHECK_SERVICE_IF_0
int FDVT_M4U_TranslationFault_callback(int port,
							   unsigned int mva,
							   void *data)
{
	pr_info("[FDVT_M4U]fault call port=%d, mva=0x%x", port, mva);

	switch (port) {
#if CHECK_SERVICE_IF_0
	case M4U_PORT_FDVT_RDA:
	case M4U_PORT_FDVT_RDB:
	case M4U_PORT_FDVT_WRA:
	case M4U_PORT_FDVT_WRB:
#endif
	default: //ISP_FDVT_BASE = 0x1b001000
		pr_info("FDVT_IN_BASE_ADR_0:0x%08x, FDVT_IN_BASE_ADR_1:0x%08x, FDVT_IN_BASE_ADR_2:0x%08x, FDVT_IN_BASE_ADR_3:0x%08x\n",
			FDVT_RD32(FDVT_IN_BASE_ADR_0_REG),
			FDVT_RD32(FDVT_IN_BASE_ADR_1_REG),
			FDVT_RD32(FDVT_IN_BASE_ADR_2_REG),
			FDVT_RD32(FDVT_IN_BASE_ADR_3_REG));
		pr_info("FDVT_OUT_BASE_ADR_0:0x%08x, FDVT_OUT_BASE_ADR_1:0x%08x, FDVT_OUT_BASE_ADR_2:0x%08x, FDVT_OUT_BASE_ADR_3:0x%08x\n",
			FDVT_RD32(FDVT_OUT_BASE_ADR_0_REG),
			FDVT_RD32(FDVT_OUT_BASE_ADR_1_REG),
			FDVT_RD32(FDVT_OUT_BASE_ADR_2_REG),
			FDVT_RD32(FDVT_OUT_BASE_ADR_3_REG));
		pr_info("FDVT_KERNEL_BASE_ADR_0:0x%08x, FDVT_KERNEL_BASE_ADR_1:0x%08x\n",
			FDVT_RD32(FDVT_KERNEL_BASE_ADR_0_REG),
			FDVT_RD32(FDVT_KERNEL_BASE_ADR_1_REG));
	break;
	}

	return 1;
}
#endif
static void aie_free_dmabuf(struct mtk_aie_dev *fd, struct imem_buf_info *bufinfo)
{
	if (bufinfo->dmabuf) {
		dma_heap_buffer_free(bufinfo->dmabuf);
		bufinfo->dmabuf = NULL;
	}
}

static void aie_free_iova(struct mtk_aie_dev *fd, struct imem_buf_info *bufinfo)
{
	if (bufinfo->pa) {
		/*free iova*/
		dma_buf_unmap_attachment(bufinfo->attach, bufinfo->sgt, DMA_BIDIRECTIONAL);
		dma_buf_detach(bufinfo->dmabuf, bufinfo->attach);
		bufinfo->pa = 0;
	}
}

static void aie_free_va(struct mtk_aie_dev *fd, struct imem_buf_info *bufinfo)
{
	if (bufinfo->va) {
		dma_buf_vunmap(bufinfo->dmabuf, bufinfo->va);
		bufinfo->va = NULL;
	}
}

struct dma_buf *aie_imem_sec_alloc(struct mtk_aie_dev *fd, u32 size, bool IsSecure)
{
	struct dma_heap *dma_heap;
	struct dma_buf *my_dma_buf;

	if (IsSecure)
		dma_heap = dma_heap_find("mtk_prot_region");
	else
		dma_heap = dma_heap_find("mtk_mm-uncached");


	if (!dma_heap) {
		dev_info(fd->dev, "heap find fail\n");
		return NULL;
	}

	my_dma_buf = dma_heap_buffer_alloc(dma_heap, size, O_RDWR |
		O_CLOEXEC, DMA_HEAP_VALID_HEAP_FLAGS);
	dma_heap_put(dma_heap);
	if (IS_ERR(my_dma_buf)) {
		dev_info(fd->dev, "buffer alloc fail\n");
		return NULL;
	}
	if (my_dma_buf == NULL)
		return NULL;

	mtk_dma_buf_set_name(my_dma_buf, BUFTAG);
	return my_dma_buf;
}

unsigned long long aie_get_sec_iova(struct mtk_aie_dev *fd, struct dma_buf *my_dma_buf,
			  struct imem_buf_info *bufinfo)
{
	struct dma_buf_attachment *attach;
	unsigned long long iova = 0;
	struct sg_table *sgt;

	attach = dma_buf_attach(my_dma_buf, fd->dev);
	if (IS_ERR(attach)) {
		dev_info(fd->dev, "attach fail, return\n");
		return 0;
	}
	bufinfo->attach = attach;

	sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		dev_info(fd->dev, "map failed, detach and return\n");
		dma_buf_detach(my_dma_buf, attach);
		return 0;
	}
	bufinfo->sgt = sgt;

	iova = sg_dma_address(sgt->sgl);

	return iova;
}

void *aie_get_va(struct mtk_aie_dev *fd, struct dma_buf *my_dma_buf)
{
	void *buf_ptr = dma_buf_vmap(my_dma_buf);

	if (!buf_ptr) {
		dev_info(fd->dev, "map failed\n");
		return NULL;
	}
	return buf_ptr;
}
#if CHECK_SERVICE_IF_0
static int aie_imem_alloc(struct mtk_aie_dev *fd, u32 size,
			  struct imem_buf_info *bufinfo)
{
	struct device *dev = fd->dev;
	void *va;
	dma_addr_t dma_handle;

	va = dma_alloc_coherent(dev, size, &dma_handle, GFP_KERNEL);
	if (!va)
		return -ENOMEM;

	bufinfo->va = va;
	bufinfo->pa = dma_handle;
	bufinfo->size = size;

	dev_info(fd->dev, "%s: vAddr(0x%p)(0x%llx), pAddr(0x%pad), size(%d)\n",
		__func__, va, (u64 *)va, &dma_handle, size);

	return 0;
}

static void aie_imem_free(struct mtk_aie_dev *fd, struct imem_buf_info *bufinfo)
{
	dev_info(fd->dev, "%s: vAddr(0x%p)(0x%llx), pAddr(0x%p), size(%d)\n",
		__func__, bufinfo->va, (u64 *)bufinfo->va, bufinfo->pa,
		bufinfo->size);

	if (bufinfo->size != 0) {
		dma_free_coherent(fd->dev, bufinfo->size, bufinfo->va, bufinfo->pa);
		bufinfo->size = 0;
	}
}
#endif
static void aie_init_table(struct mtk_aie_dev *fd, u16 pym_width,
			   u16 pym_height)
{
	int i;
	struct aie_static_info *pstv;

	pstv = &fd->st_info;

	pstv->img_width[pym2_start_loop] = pym_width / 4;
	pstv->img_height[pym2_start_loop] = pym_height / 4;

	pstv->img_width[pym1_start_loop] = pym_width / 2;
	pstv->img_height[pym1_start_loop] = pym_height / 2;

	pstv->img_width[pym0_start_loop] = pym_width;
	pstv->img_height[pym0_start_loop] = pym_height;

	for (i = 0; i < fd_loop_num; i++) {
		if (i != pym2_start_loop && i != pym1_start_loop &&
		    i != pym0_start_loop) {
			if (fd_out_stride2_in[i] == 1) {
				pstv->img_width[i] =
					pstv->stride2_out_width[i - 1];
				pstv->img_height[i] =
					pstv->stride2_out_height[i - 1];
			} else {
				pstv->img_width[i] = pstv->out_width[i - 1];
				pstv->img_height[i] = pstv->out_height[i - 1];
			}
		}

		if (fd_maxpool[i] == 1 && fd_stride[i] == 1) {
			pstv->out_width[i] =
				(pstv->img_width[i] - 1) /
				(2 * fd_maxpool[i]) + 1;
			pstv->out_height[i] = (pstv->img_height[i] - 1) /
					      (2 * fd_maxpool[i]) + 1;
		} else {
			pstv->out_width[i] =
				(pstv->img_width[i] - 1) /
					(fd_stride[i] + 2 * fd_maxpool[i]) + 1;
			pstv->out_height[i] =
				(pstv->img_height[i] - 1) /
					(fd_stride[i] + 2 * fd_maxpool[i]) + 1;
		}

		pstv->stride2_out_width[i] =
			((pstv->out_width[i] - 1) / 2 + 1) * out_2size[i];
		pstv->stride2_out_height[i] =
			((pstv->out_height[i] - 1) / 2 + 1) * out_2size[i];

		if (outlayer[i] == 1) {
			pstv->out_xsize_plus_1[i] =
				pstv->out_width[i] * out_ch_pack[i] * 2;
			pstv->out_stride[i] = round_up(
				pstv->out_xsize_plus_1[i] * anchor_en_num[i],
				16);
			pstv->out_xsize_plus_1_stride2[i] =
				((pstv->out_width[i] - 1) / 2 + 1) *
				out_ch_pack[i] * 2 * out_2size[i];
		} else {
			pstv->out_xsize_plus_1[i] =
				pstv->out_width[i] * out_ch_pack[i];
			pstv->out_stride[i] =
				round_up(pstv->out_xsize_plus_1[i], 16);
			pstv->out_xsize_plus_1_stride2[i] =
				((pstv->out_width[i] - 1) / 2 + 1) *
				out_ch_pack[i] * out_2size[i];
		}

		pstv->out_stride_stride2[i] =
			round_up(pstv->out_xsize_plus_1_stride2[i], 16);

		if (out_2size[i] == 1)
			pstv->out_ysize_plus_1_stride2[i] =
				(pstv->out_height[i] - 1) / 2 + 1;
		else
			pstv->out_ysize_plus_1_stride2[i] = pstv->out_height[i];

		if (fd_wdma_en[i][0]) {
			if (i == rpn2_loop_num || i == rpn1_loop_num ||
			    i == rpn0_loop_num) {
				pstv->fd_wdma_size[i][0] = result_size;
			} else {
				pstv->fd_wdma_size[i][0] = pstv->out_height[i] *
							   pstv->out_stride[i];
			}
		}

		if (outlayer[i] == 1) {
			if (fd_wdma_en[i][1])
				pstv->fd_wdma_size[i][1] =
					pstv->fd_wdma_size[i][0];
			if (fd_wdma_en[i][2])
				pstv->fd_wdma_size[i][2] =
					pstv->fd_wdma_size[i][0];
			if (fd_wdma_en[i][3])
				pstv->fd_wdma_size[i][3] =
					pstv->fd_wdma_size[i][0];
		} else if (i == rpn2_loop_num || i == rpn1_loop_num ||
			   i == rpn0_loop_num) {
			pstv->fd_wdma_size[i][0] = result_size;
		} else {
			if (fd_wdma_en[i][1])
				pstv->fd_wdma_size[i][1] = pstv->out_height[i] *
							   pstv->out_stride[i];
			if (fd_wdma_en[i][2])
				pstv->fd_wdma_size[i][2] =
					pstv->out_ysize_plus_1_stride2[i] *
					pstv->out_stride_stride2[i];
			if (fd_wdma_en[i][3])
				pstv->fd_wdma_size[i][3] =
					pstv->out_ysize_plus_1_stride2[i] *
					pstv->out_stride_stride2[i];
		}

		if (in_ch_pack[i] == 1)
			pstv->input_xsize_plus_1[i] =
				round_up(pstv->img_width[i], 8);
		else
			pstv->input_xsize_plus_1[i] =
				pstv->img_width[i] * in_ch_pack[i];
	}
}

static void aie_update_table(struct mtk_aie_dev *fd, u16 pym_width,
			   u16 pym_height)
{
	int i;
	struct aie_static_info *pstv;

	pstv = &fd->st_info;

	pstv->img_width[pym2_start_loop] = pym_width / 4;
	pstv->img_height[pym2_start_loop] = pym_height / 4;

	pstv->img_width[pym1_start_loop] = pym_width / 2;
	pstv->img_height[pym1_start_loop] = pym_height / 2;

	pstv->img_width[pym0_start_loop] = pym_width;
	pstv->img_height[pym0_start_loop] = pym_height;

	for (i = 0; i < fd_loop_num; i++) {
		if (i != pym2_start_loop && i != pym1_start_loop &&
		    i != pym0_start_loop) {
			if (fd_out_stride2_in[i] == 1) {
				pstv->img_width[i] =
					pstv->stride2_out_width[i - 1];
				pstv->img_height[i] =
					pstv->stride2_out_height[i - 1];
			} else {
				pstv->img_width[i] = pstv->out_width[i - 1];
				pstv->img_height[i] = pstv->out_height[i - 1];
			}
		}

		if (fd_maxpool[i] == 1 && fd_stride[i] == 1) {
			pstv->out_width[i] =
				(pstv->img_width[i] - 1) /
				(2 * fd_maxpool[i]) + 1;
			pstv->out_height[i] = (pstv->img_height[i] - 1) /
					      (2 * fd_maxpool[i]) + 1;
		} else {
			pstv->out_width[i] =
				(pstv->img_width[i] - 1) /
					(fd_stride[i] + 2 * fd_maxpool[i]) + 1;
			pstv->out_height[i] =
				(pstv->img_height[i] - 1) /
					(fd_stride[i] + 2 * fd_maxpool[i]) + 1;
		}

		pstv->stride2_out_width[i] =
			((pstv->out_width[i] - 1) / 2 + 1) * out_2size[i];
		pstv->stride2_out_height[i] =
			((pstv->out_height[i] - 1) / 2 + 1) * out_2size[i];

		if (outlayer[i] == 1) {
			pstv->out_xsize_plus_1[i] =
				pstv->out_width[i] * out_ch_pack[i] * 2;
			pstv->out_stride[i] = round_up(
				pstv->out_xsize_plus_1[i] * anchor_en_num[i],
				16);
			pstv->out_xsize_plus_1_stride2[i] =
				((pstv->out_width[i] - 1) / 2 + 1) *
				out_ch_pack[i] * 2 * out_2size[i];
		} else {
			pstv->out_xsize_plus_1[i] =
				pstv->out_width[i] * out_ch_pack[i];
			pstv->out_stride[i] =
				round_up(pstv->out_xsize_plus_1[i], 16);
			pstv->out_xsize_plus_1_stride2[i] =
				((pstv->out_width[i] - 1) / 2 + 1) *
				out_ch_pack[i] * out_2size[i];
		}

		pstv->out_stride_stride2[i] =
			round_up(pstv->out_xsize_plus_1_stride2[i], 16);

		if (out_2size[i] == 1)
			pstv->out_ysize_plus_1_stride2[i] =
				(pstv->out_height[i] - 1) / 2 + 1;
		else
			pstv->out_ysize_plus_1_stride2[i] = pstv->out_height[i];

		if (in_ch_pack[i] == 1)
			pstv->input_xsize_plus_1[i] =
				round_up(pstv->img_width[i], 8);
		else
			pstv->input_xsize_plus_1[i] =
				pstv->img_width[i] * in_ch_pack[i];
	}
}

static void aie_get_data_size(struct mtk_aie_dev *fd, u16 max_img_width,
			      u16 max_img_height)
{
	u8 i, j;
	struct aie_static_info *pstv;

	pstv = &fd->st_info;

	fd->base_para->max_img_width = max_img_width;
	fd->base_para->max_img_height = max_img_height;
	fd->fd_dma_max_size = 0;
	fd->fd_dma_rst_max_size = 0;
	fd->fd_fd_kernel_size = 0;
	fd->fd_attr_kernel_size = 0;
	fd->fd_attr_dma_max_size = 0;
	fd->fd_attr_dma_rst_max_size = 0;

	/* FDMODE Dram Buffer Size */
	fd->fd_rs_cfg_size = fd_rs_confi_size;
	fd->fd_fd_cfg_size = fd_fd_confi_size;
	fd->fd_yuv2rgb_cfg_size = fd_yuv2rgb_confi_size;

	/* ATTRMODE Dram Buffer Size */
	fd->attr_fd_cfg_size = attr_fd_confi_size;
	fd->attr_yuv2rgb_cfg_size = attr_yuv2rgb_confi_size;

	/* HW Output Buffer Size */
	fd->rs_pym_out_size[0] = fd->base_para->max_pyramid_width *
				 fd->base_para->max_pyramid_height;
	fd->rs_pym_out_size[1] = fd->rs_pym_out_size[0] / 4;
	fd->rs_pym_out_size[2] = fd->rs_pym_out_size[0] / 16;

	/* FDMODE Dram Buffer Size */
	for (i = rpn1_loop_num + 1 ; i < rpn0_loop_num - 1; i++) {
		for (j = 0; j < output_WDMA_WRA_num; j++) {
			fd->fd_dma_max_size += pstv->fd_wdma_size[i][j];
		}
	}
	fd->fd_dma_rst_max_size = pstv->fd_wdma_size[rpn2_loop_num][0] +
					pstv->fd_wdma_size[rpn1_loop_num][0] +
					pstv->fd_wdma_size[rpn0_loop_num][0];

	for (i = 0; i < fd_loop_num; i++) {
		for (j = 0; j < kernel_RDMA_RA_num; j++) {
			if (fd_ker_rdma_size[i][j])
				fd->fd_fd_kernel_size += fd_ker_rdma_size[i][j];
		}
	}

	/* ATTRMODE Dram Buffer Size */
	for (i = 0; i < attr_loop_num; i++) {
		for (j = 0; j < output_WDMA_WRA_num; j++) {
			if (attr_wdma_en[i][j]) {
				if ((i == age_out_rgs || i == gender_out_rgs ||
				     i == indian_out_rgs || i == race_out_rgs) &&
				    (j == 0)) {
					fd->fd_attr_dma_rst_max_size +=
						ATTR_OUT_SIZE * MAX_ENQUE_FRAME_NUM;
				} else {
					fd->fd_attr_dma_max_size += attr_wdma_size[i][j];
				}
			}
		}
	}

	for (i = 0; i < attr_loop_num; i++) {
		for (j = 0; j < kernel_RDMA_RA_num; j++)
			fd->fd_attr_kernel_size += attr_ker_rdma_size[i][j];
	}

	/* FD Pose secure result output buffer: result size * 3 loops */
	//fd->fd_dma_rst_max_size += result_size * 3;
}

static int aie_alloc_dram_buf(struct mtk_aie_dev *fd)
{
	u8 i;
	u32 alloc_size;
	unsigned long long addr = 0;
	unsigned int msb_bit = 0;
	struct dma_buf *ret_buf = NULL;
	unsigned long long iova = 0;
	void *va = NULL;

	/* RS DRAM */
	alloc_size = fd->fd_rs_cfg_size;
	//ret = aie_imem_alloc(fd, alloc_size, &fd->rs_cfg_data);

	ret_buf = aie_imem_sec_alloc(fd, alloc_size, false);
	if (!ret_buf)
		return -1;

	fd->rs_cfg_data.dmabuf = ret_buf;
	fd->rs_cfg_data.size = alloc_size;
	iova = aie_get_sec_iova(fd, ret_buf, &fd->rs_cfg_data);
	if (!iova)
		return -1;

	fd->rs_cfg_data.pa = iova;
	va = aie_get_va(fd, ret_buf);
	if (!va)
		return -1;

	fd->rs_cfg_data.va = va;

	addr = fd->rs_cfg_data.pa;
	msb_bit = (addr & 0Xf00000000) >> 32; //MASK MSB-BIT

	writel(msb_bit, fd->fd_base + FDVT_RS_CON_BASE_ADR_MSB);

	/* FD MODE */
	fd->base_para->fd_rs_cfg_pa = fd->rs_cfg_data.pa;
	fd->base_para->fd_rs_cfg_va = fd->rs_cfg_data.va;

	/* FD DRAM */
	alloc_size =
		fd->fd_fd_cfg_size + fd->attr_fd_cfg_size * MAX_ENQUE_FRAME_NUM;
	//ret = aie_imem_alloc(fd, alloc_size, &fd->fd_cfg_data);
	ret_buf = aie_imem_sec_alloc(fd, alloc_size, false);
	if (!ret_buf)
		return -1;

	fd->fd_cfg_data.dmabuf = ret_buf;
	fd->fd_cfg_data.size = alloc_size;
	iova = aie_get_sec_iova(fd, ret_buf, &fd->fd_cfg_data);
	if (!iova)
		return -1;

	fd->fd_cfg_data.pa = iova;
	va = aie_get_va(fd, ret_buf);
	if (!va)
		return -1;

	fd->fd_cfg_data.va = va;

	addr = fd->fd_cfg_data.pa;
	msb_bit = (addr & 0Xf00000000) >> 32; //MASK MSB-BIT

	writel(msb_bit, fd->fd_base + FDVT_FD_CON_BASE_ADR_MSB);

	/* FD MODE */
	fd->base_para->fd_fd_cfg_pa = fd->fd_cfg_data.pa;
	fd->base_para->fd_fd_cfg_va = fd->fd_cfg_data.va;
	/* ATTR MODE */
	fd->base_para->attr_fd_cfg_pa[0] =
		fd->base_para->fd_fd_cfg_pa + fd->fd_fd_cfg_size;
	fd->base_para->attr_fd_cfg_va[0] =
		fd->base_para->fd_fd_cfg_va + fd->fd_fd_cfg_size;

	for (i = 1; i < MAX_ENQUE_FRAME_NUM; i++) {
		fd->base_para->attr_fd_cfg_pa[i] =
			fd->base_para->attr_fd_cfg_pa[i - 1] +
			fd->attr_fd_cfg_size;
		fd->base_para->attr_fd_cfg_va[i] =
			fd->base_para->attr_fd_cfg_va[i - 1] +
			fd->attr_fd_cfg_size;
	}

	/* YUV2RGB DRAM */
	alloc_size = fd->fd_yuv2rgb_cfg_size +
		     fd->attr_yuv2rgb_cfg_size * MAX_ENQUE_FRAME_NUM;
	//ret = aie_imem_alloc(fd, alloc_size, &fd->yuv2rgb_cfg_data);
	ret_buf = aie_imem_sec_alloc(fd, alloc_size, false);
	if (!ret_buf)
		return -1;

	fd->yuv2rgb_cfg_data.dmabuf = ret_buf;
	fd->yuv2rgb_cfg_data.size = alloc_size;
	iova = aie_get_sec_iova(fd, ret_buf, &fd->yuv2rgb_cfg_data);
	if (!iova)
		return -1;

	fd->yuv2rgb_cfg_data.pa = iova;
	va = aie_get_va(fd, ret_buf);
	if (!va)
		return -1;

	fd->yuv2rgb_cfg_data.va = va;

	addr = fd->yuv2rgb_cfg_data.pa;
	msb_bit = (addr & 0Xf00000000) >> 32; //MASK MSB-BIT

	writel(msb_bit, fd->fd_base + FDVT_YUV2RGB_CON_BASE_ADR_MSB);


	/* FD MODE */
	fd->base_para->fd_yuv2rgb_cfg_pa = fd->yuv2rgb_cfg_data.pa;
	fd->base_para->fd_yuv2rgb_cfg_va = fd->yuv2rgb_cfg_data.va;

	/* ATTR MODE */
	fd->base_para->attr_yuv2rgb_cfg_pa[0] =
		fd->base_para->fd_yuv2rgb_cfg_pa + fd->fd_yuv2rgb_cfg_size;
	fd->base_para->attr_yuv2rgb_cfg_va[0] =
		fd->base_para->fd_yuv2rgb_cfg_va + fd->fd_yuv2rgb_cfg_size;

	for (i = 1; i < MAX_ENQUE_FRAME_NUM; i++) {
		fd->base_para->attr_yuv2rgb_cfg_pa[i] =
			fd->base_para->attr_yuv2rgb_cfg_pa[i - 1] +
			fd->attr_yuv2rgb_cfg_size;
		fd->base_para->attr_yuv2rgb_cfg_va[i] =
			fd->base_para->attr_yuv2rgb_cfg_va[i - 1] +
			fd->attr_yuv2rgb_cfg_size;
	}

	return 0;

}

static int aie_alloc_output_buf(struct mtk_aie_dev *fd)
{
	int ret = 0;
	u32 alloc_size = 0;
	int i, j, pa_off = 0, va_off = 0;
	struct dma_buf *ret_buf = NULL;
	unsigned long long iova = 0;
	void *va = NULL;

	for (i = 0; i < PYM_NUM; i++)
		alloc_size += fd->rs_pym_out_size[i] * 3;

	if (g_user_param.is_secure) {
		ret_buf = aie_imem_sec_alloc(fd, alloc_size, true);
		if (!ret_buf)
			return -1;
		fd->rs_output_hw.size = alloc_size;
		fd->rs_output_hw.dmabuf = ret_buf;
		iova = aie_get_sec_iova(fd, ret_buf, &fd->rs_output_hw);
		if (!iova)
			return -1;

		fd->rs_output_hw.pa = iova;
	} else {
		ret_buf = aie_imem_sec_alloc(fd, alloc_size, false);
		if (!ret_buf)
			return -1;

		fd->rs_output_hw.size = alloc_size;
		fd->rs_output_hw.dmabuf = ret_buf;
		iova = aie_get_sec_iova(fd, ret_buf, &fd->rs_output_hw);
		if (!iova)
			return -1;

		fd->rs_output_hw.pa = iova;
		va = aie_get_va(fd, ret_buf);
		if (!va)
			return -1;

		fd->rs_output_hw.va = va;
	}

	for (i = 0; i < PYM_NUM; i++) {
		for (j = 0; j < COLOR_NUM; j++) {
			fd->base_para->rs_pym_rst_pa[i][j] =
				fd->rs_output_hw.pa + pa_off;
			pa_off += fd->rs_pym_out_size[i];

			fd->base_para->rs_pym_rst_va[i][j] =
				fd->rs_output_hw.va + va_off;
			va_off += fd->rs_pym_out_size[i];
		}
	}

	return ret;
}

static void aie_alloc_normal(struct mtk_aie_dev *fd, unsigned int start,
			     unsigned int end)
{
	unsigned int i, j;
	unsigned int pi, pj;
	struct aie_static_info *pstv;

	pstv = &fd->st_info;
	if (start == 0 || end <= start)
		return;

	pi = start - 1;
	pj = 0;
	for (i = start; i < end + 1; i++) {
		for (j = 0; j < output_WDMA_WRA_num; j++) {
			if (fd_wdma_en[i][j]) {
				fd->dma_para->fd_out_hw_pa[i][j] =
					fd->dma_para->fd_out_hw_pa[pi][pj] +
					pstv->fd_wdma_size[pi][pj];
				pi = i;
				pj = j;
			}
		}
	}
}

static int aie_alloc_fddma_buf(struct mtk_aie_dev *fd)
{
	u32 alloc_size;
	struct dma_buf *ret_buf = NULL;
	unsigned long long iova = 0;
	void *va = NULL;


	alloc_size = fd->fd_dma_max_size;
	//ret = aie_imem_alloc(fd, alloc_size, &fd->fd_dma_hw);
	ret_buf = aie_imem_sec_alloc(fd, alloc_size, false);
	if (!ret_buf)
		return -1;

	fd->fd_dma_hw.dmabuf = ret_buf;
	fd->fd_dma_hw.size = alloc_size;
	iova = aie_get_sec_iova(fd, ret_buf, &fd->fd_dma_hw);
	if (!iova)
		return -1;

	fd->fd_dma_hw.pa = iova;
	va = aie_get_va(fd, ret_buf);
	if (!va)
		return -1;

	fd->fd_dma_hw.va = va;


	alloc_size = fd->fd_fd_kernel_size + fd->fd_attr_kernel_size;
	//ret = aie_imem_alloc(fd, alloc_size, &fd->fd_kernel_hw);
	ret_buf = aie_imem_sec_alloc(fd, alloc_size, false);
	if (!ret_buf)
		return -1;

	fd->fd_kernel_hw.dmabuf = ret_buf;
	fd->fd_kernel_hw.size = alloc_size;
	iova = aie_get_sec_iova(fd, ret_buf, &fd->fd_kernel_hw);
	if (!iova)
		return -1;

	fd->fd_kernel_hw.pa = iova;
	va = aie_get_va(fd, ret_buf);
	if (!va)
		return -1;

	fd->fd_kernel_hw.va = va;


	alloc_size = fd->fd_attr_dma_max_size;
	//ret = aie_imem_alloc(fd, alloc_size, &fd->fd_attr_dma_hw);
	ret_buf = aie_imem_sec_alloc(fd, alloc_size, false);
	if (!ret_buf)
		return -1;

	fd->fd_attr_dma_hw.dmabuf = ret_buf;
	fd->fd_attr_dma_hw.size = alloc_size;
	iova = aie_get_sec_iova(fd, ret_buf, &fd->fd_attr_dma_hw);
	if (!iova)
		return -1;

	fd->fd_attr_dma_hw.pa = iova;
	va = aie_get_va(fd, ret_buf);
	if (!va)
		return -1;

	fd->fd_attr_dma_hw.va = va;

	alloc_size = fd->fd_dma_rst_max_size + fd->fd_attr_dma_rst_max_size;
	//ret = aie_imem_alloc(fd, alloc_size, &fd->fd_dma_result_hw);
	ret_buf = aie_imem_sec_alloc(fd, alloc_size, false);
	if (!ret_buf)
		return -1;

	fd->fd_dma_result_hw.dmabuf = ret_buf;
	fd->fd_dma_result_hw.size = alloc_size;
	iova = aie_get_sec_iova(fd, ret_buf, &fd->fd_dma_result_hw);
	if (!iova)
		return -1;

	fd->fd_dma_result_hw.pa = iova;
	va = aie_get_va(fd, ret_buf);
	if (!va)
		return -1;

	fd->fd_dma_result_hw.va = va;

	return 0;
}
#ifdef FLD
static int aie_alloc_fld_buf(struct mtk_aie_dev *fd)
{

	u32 alloc_size;
	unsigned long long addr = 0;
	unsigned int msb_bit = 0;
	struct dma_buf *ret_buf = NULL;
	unsigned long long iova = 0;
	void *va = NULL;

	alloc_size = fld_blink_weight_size;
	//ret = aie_imem_alloc(fd, alloc_size, &fd->fld_blink_weight_hw);
	ret_buf = aie_imem_sec_alloc(fd, alloc_size, false);
	if (!ret_buf)
		return -1;

	fd->fld_blink_weight_hw.dmabuf = ret_buf;
	fd->fld_blink_weight_hw.size = alloc_size;
	iova = aie_get_sec_iova(fd, ret_buf, &fd->fld_blink_weight_hw);
	if (!iova)
		return -1;

	fd->fld_blink_weight_hw.pa = iova;
	va = aie_get_va(fd, ret_buf);
	if (!va)
		return -1;

	fd->fld_blink_weight_hw.va = va;

	addr = fd->fld_blink_weight_hw.pa;
	msb_bit = (addr & 0Xf00000000) >> 8; //MASK MSB-BIT
	writel(msb_bit, fd->fd_base + FLD_BS_IN_BASE_ADDR_8_15_MSB);


	alloc_size = fld_fp_size * FLD_MAX_INPUT;
	//ret = aie_imem_alloc(fd, alloc_size, &fd->fld_fp_hw);
	ret_buf = aie_imem_sec_alloc(fd, alloc_size, false);
	if (!ret_buf)
		return -1;

	fd->fld_fp_hw.dmabuf = ret_buf;
	fd->fld_fp_hw.size = alloc_size;
	iova = aie_get_sec_iova(fd, ret_buf, &fd->fld_fp_hw);
	if (!iova)
		return -1;

	fd->fld_fp_hw.pa = iova;
	va = aie_get_va(fd, ret_buf);
	if (!va)
		return -1;

	fd->fld_fp_hw.va = va;

	alloc_size = (fld_cv_size * (FLD_MAX_INPUT-1)) + fld_cv_size_00;
	//ret = aie_imem_alloc(fd, alloc_size, &fd->fld_cv_hw);
	ret_buf = aie_imem_sec_alloc(fd, alloc_size, false);
	if (!ret_buf)
		return -1;

	fd->fld_cv_hw.dmabuf = ret_buf;
	fd->fld_cv_hw.size = alloc_size;
	iova = aie_get_sec_iova(fd, ret_buf, &fd->fld_cv_hw);
	if (!iova)
		return -1;

	fd->fld_cv_hw.pa = iova;
	va = aie_get_va(fd, ret_buf);
	if (!va)
		return -1;

	fd->fld_cv_hw.va = va;


	alloc_size = fld_leafnode_size * FLD_MAX_INPUT;
	//ret = aie_imem_alloc(fd, alloc_size, &fd->fld_leafnode_hw);
	ret_buf = aie_imem_sec_alloc(fd, alloc_size, false);
	if (!ret_buf)
		return -1;

	fd->fld_leafnode_hw.dmabuf = ret_buf;
	fd->fld_leafnode_hw.size = alloc_size;
	iova = aie_get_sec_iova(fd, ret_buf, &fd->fld_leafnode_hw);
	if (!iova)
		return -1;

	fd->fld_leafnode_hw.pa = iova;
	va = aie_get_va(fd, ret_buf);
	if (!va)
		return -1;

	fd->fld_leafnode_hw.va = va;


	alloc_size = fld_tree_size * FLD_MAX_INPUT;
	//ret = aie_imem_alloc(fd, alloc_size, &fd->fld_tree_02_hw);
	ret_buf = aie_imem_sec_alloc(fd, alloc_size, false);
	if (!ret_buf)
		return -1;

	fd->fld_tree_02_hw.dmabuf = ret_buf;
	fd->fld_tree_02_hw.size = alloc_size;
	iova = aie_get_sec_iova(fd, ret_buf, &fd->fld_tree_02_hw);
	if (!iova)
		return -1;

	fd->fld_tree_02_hw.pa = iova;
	va = aie_get_va(fd, ret_buf);
	if (!va)
		return -1;

	fd->fld_tree_02_hw.va = va;

	//ret = aie_imem_alloc(fd, alloc_size, &fd->fld_tree_13_hw);
	ret_buf = aie_imem_sec_alloc(fd, alloc_size, false);
	if (!ret_buf)
		return -1;

	fd->fld_tree_13_hw.dmabuf = ret_buf;
	fd->fld_tree_13_hw.size = alloc_size;
	iova = aie_get_sec_iova(fd, ret_buf, &fd->fld_tree_13_hw);
	if (!iova)
		return -1;

	fd->fld_tree_13_hw.pa = iova;
	va = aie_get_va(fd, ret_buf);
	if (!va)
		return -1;

	fd->fld_tree_13_hw.va = va;

	alloc_size = fld_result_size;
	//ret = aie_imem_alloc(fd, alloc_size, &fd->fld_output_hw);
	ret_buf = aie_imem_sec_alloc(fd, alloc_size, false);
	if (!ret_buf)
		return -1;

	fd->fld_output_hw.dmabuf = ret_buf;
	fd->fld_output_hw.size = alloc_size;
	iova = aie_get_sec_iova(fd, ret_buf, &fd->fld_output_hw);
	if (!iova)
		return -1;

	fd->fld_output_hw.pa = iova;
	va = aie_get_va(fd, ret_buf);
	if (!va)
		return -1;

	fd->fld_output_hw.va = va;

	addr = fd->fld_output_hw.pa;
	msb_bit = (addr & 0Xf00000000) >> 32;
	writel(msb_bit, fd->fd_base + FLD_TR_OUT_BASE_ADDR_0_MSB);
	writel(msb_bit, fd->fd_base + FLD_PP_OUT_BASE_ADDR_0_MSB);

	return 0;
}
#endif
static void aie_arrange_fddma_buf(struct mtk_aie_dev *fd)
{
	dma_addr_t currentPA;
	struct aie_static_info *pstv;

	pstv = &fd->st_info;

	/* 0~18 */
	fd->dma_para->fd_out_hw_pa[0][0] = fd->fd_dma_hw.pa;
	aie_alloc_normal(fd, 1, 18);

	/* 19~27 */
	fd->dma_para->fd_out_hw_pa[19][0] =
		fd->dma_para->fd_out_hw_pa[18][1] + pstv->fd_wdma_size[18][1];
	fd->dma_para->fd_out_hw_pa[19][1] =
		fd->dma_para->fd_out_hw_pa[19][0] + pstv->out_xsize_plus_1[19];
	fd->dma_para->fd_out_hw_pa[20][0] = fd->dma_para->fd_out_hw_pa[19][0] +
					    2 * pstv->out_xsize_plus_1[20];
	fd->dma_para->fd_out_hw_pa[20][1] = fd->dma_para->fd_out_hw_pa[19][0] +
					    3 * pstv->out_xsize_plus_1[20];
	fd->dma_para->fd_out_hw_pa[21][0] = fd->dma_para->fd_out_hw_pa[19][0] +
					    4 * pstv->out_xsize_plus_1[21];
	fd->dma_para->fd_out_hw_pa[22][0] =
		fd->dma_para->fd_out_hw_pa[19][0] + pstv->fd_wdma_size[19][0] +
		pstv->fd_wdma_size[19][1] + pstv->fd_wdma_size[20][0] +
		pstv->fd_wdma_size[20][1] + pstv->fd_wdma_size[21][0];
	fd->dma_para->fd_out_hw_pa[22][1] =
		fd->dma_para->fd_out_hw_pa[22][0] + pstv->fd_wdma_size[22][0] +
		pstv->fd_wdma_size[22][2] + pstv->fd_wdma_size[23][0] +
		pstv->fd_wdma_size[23][2] + pstv->fd_wdma_size[24][0];
	fd->dma_para->fd_out_hw_pa[22][2] =
		fd->dma_para->fd_out_hw_pa[22][0] + pstv->out_xsize_plus_1[22];
	fd->dma_para->fd_out_hw_pa[22][3] =
		fd->dma_para->fd_out_hw_pa[22][1] + pstv->out_xsize_plus_1[22];
	fd->dma_para->fd_out_hw_pa[23][0] = fd->dma_para->fd_out_hw_pa[22][0] +
					    2 * pstv->out_xsize_plus_1[23];
	fd->dma_para->fd_out_hw_pa[23][1] = fd->dma_para->fd_out_hw_pa[22][1] +
					    2 * pstv->out_xsize_plus_1[23];
	fd->dma_para->fd_out_hw_pa[23][2] = fd->dma_para->fd_out_hw_pa[22][0] +
					    3 * pstv->out_xsize_plus_1[23];
	fd->dma_para->fd_out_hw_pa[23][3] = fd->dma_para->fd_out_hw_pa[22][1] +
					    3 * pstv->out_xsize_plus_1[23];
	fd->dma_para->fd_out_hw_pa[24][0] = fd->dma_para->fd_out_hw_pa[22][0] +
					    4 * pstv->out_xsize_plus_1[24];
	fd->dma_para->fd_out_hw_pa[24][1] = fd->dma_para->fd_out_hw_pa[22][1] +
					    4 * pstv->out_xsize_plus_1[24];
	fd->dma_para->fd_out_hw_pa[25][0] =
		fd->dma_para->fd_out_hw_pa[22][1] + pstv->fd_wdma_size[22][1] +
		pstv->fd_wdma_size[22][3] + pstv->fd_wdma_size[23][1] +
		pstv->fd_wdma_size[23][3] + pstv->fd_wdma_size[24][1];
	fd->dma_para->fd_out_hw_pa[25][1] =
		fd->dma_para->fd_out_hw_pa[25][0] + pstv->out_xsize_plus_1[25];
	fd->dma_para->fd_out_hw_pa[26][0] = fd->dma_para->fd_out_hw_pa[25][0] +
					    2 * pstv->out_xsize_plus_1[26];
	fd->dma_para->fd_out_hw_pa[26][1] = fd->dma_para->fd_out_hw_pa[25][0] +
					    3 * pstv->out_xsize_plus_1[26];
	fd->dma_para->fd_out_hw_pa[27][0] = fd->dma_para->fd_out_hw_pa[25][0] +
					    4 * pstv->out_xsize_plus_1[27];

	fd->dma_para->fd_out_hw_pa[29][0] = fd->fd_dma_hw.pa;
#if CHECK_SERVICE_0
	fd->dma_para->fd_out_hw_pa[29][0] =
		fd->dma_para->fd_out_hw_pa[25][0] + pstv->fd_wdma_size[25][0] +
		pstv->fd_wdma_size[25][1] + pstv->fd_wdma_size[26][0] +
		pstv->fd_wdma_size[26][1] + pstv->fd_wdma_size[27][0];
#endif
	aie_alloc_normal(fd, 30, 47);
	/* 48~56 */
	fd->dma_para->fd_out_hw_pa[48][0] =
		fd->dma_para->fd_out_hw_pa[47][1] + pstv->fd_wdma_size[47][1];
	fd->dma_para->fd_out_hw_pa[48][1] =
		fd->dma_para->fd_out_hw_pa[48][0] + pstv->out_xsize_plus_1[48];
	fd->dma_para->fd_out_hw_pa[49][0] = fd->dma_para->fd_out_hw_pa[48][0] +
					    2 * pstv->out_xsize_plus_1[49];
	fd->dma_para->fd_out_hw_pa[49][1] = fd->dma_para->fd_out_hw_pa[48][0] +
					    3 * pstv->out_xsize_plus_1[49];
	fd->dma_para->fd_out_hw_pa[50][0] = fd->dma_para->fd_out_hw_pa[48][0] +
					    4 * pstv->out_xsize_plus_1[50];
	fd->dma_para->fd_out_hw_pa[51][0] =
		fd->dma_para->fd_out_hw_pa[48][0] + pstv->fd_wdma_size[48][0] +
		pstv->fd_wdma_size[48][1] + pstv->fd_wdma_size[49][0] +
		pstv->fd_wdma_size[49][1] + pstv->fd_wdma_size[50][0];
	fd->dma_para->fd_out_hw_pa[51][1] =
		fd->dma_para->fd_out_hw_pa[51][0] + pstv->fd_wdma_size[51][0] +
		pstv->fd_wdma_size[51][2] + pstv->fd_wdma_size[52][0] +
		pstv->fd_wdma_size[52][2] + pstv->fd_wdma_size[53][0];
	fd->dma_para->fd_out_hw_pa[51][2] =
		fd->dma_para->fd_out_hw_pa[51][0] + pstv->out_xsize_plus_1[51];
	fd->dma_para->fd_out_hw_pa[51][3] =
		fd->dma_para->fd_out_hw_pa[51][1] + pstv->out_xsize_plus_1[51];
	fd->dma_para->fd_out_hw_pa[52][0] = fd->dma_para->fd_out_hw_pa[51][0] +
					    2 * pstv->out_xsize_plus_1[52];
	fd->dma_para->fd_out_hw_pa[52][1] = fd->dma_para->fd_out_hw_pa[51][1] +
					    2 * pstv->out_xsize_plus_1[52];
	fd->dma_para->fd_out_hw_pa[52][2] = fd->dma_para->fd_out_hw_pa[51][0] +
					    3 * pstv->out_xsize_plus_1[52];
	fd->dma_para->fd_out_hw_pa[52][3] = fd->dma_para->fd_out_hw_pa[51][1] +
					    3 * pstv->out_xsize_plus_1[52];
	fd->dma_para->fd_out_hw_pa[53][0] = fd->dma_para->fd_out_hw_pa[51][0] +
					    4 * pstv->out_xsize_plus_1[53];
	fd->dma_para->fd_out_hw_pa[53][1] = fd->dma_para->fd_out_hw_pa[51][1] +
					    4 * pstv->out_xsize_plus_1[53];
	fd->dma_para->fd_out_hw_pa[54][0] =
		fd->dma_para->fd_out_hw_pa[51][1] + pstv->fd_wdma_size[51][1] +
		pstv->fd_wdma_size[51][3] + pstv->fd_wdma_size[52][1] +
		pstv->fd_wdma_size[52][3] + pstv->fd_wdma_size[53][1];
	fd->dma_para->fd_out_hw_pa[54][1] =
		fd->dma_para->fd_out_hw_pa[54][0] + pstv->out_xsize_plus_1[54];
	fd->dma_para->fd_out_hw_pa[55][0] = fd->dma_para->fd_out_hw_pa[54][0] +
					    2 * pstv->out_xsize_plus_1[55];
	fd->dma_para->fd_out_hw_pa[55][1] = fd->dma_para->fd_out_hw_pa[54][0] +
					    3 * pstv->out_xsize_plus_1[55];
	fd->dma_para->fd_out_hw_pa[56][0] = fd->dma_para->fd_out_hw_pa[54][0] +
					    4 * pstv->out_xsize_plus_1[56];

	/* 58~76 */
	fd->dma_para->fd_out_hw_pa[58][0] = fd->fd_dma_hw.pa;
#if CHECK_SERVICE_0
	fd->dma_para->fd_out_hw_pa[58][0] =
		fd->dma_para->fd_out_hw_pa[54][0] + pstv->fd_wdma_size[54][0] +
		pstv->fd_wdma_size[54][1] + pstv->fd_wdma_size[55][0] +
		pstv->fd_wdma_size[55][1] + pstv->fd_wdma_size[56][0];
#endif
	aie_alloc_normal(fd, 59, 76);

	/* 77~85 */
	fd->dma_para->fd_out_hw_pa[77][0] =
		fd->dma_para->fd_out_hw_pa[76][1] + pstv->fd_wdma_size[76][1];
	fd->dma_para->fd_out_hw_pa[77][1] =
		fd->dma_para->fd_out_hw_pa[77][0] + pstv->out_xsize_plus_1[77];
	fd->dma_para->fd_out_hw_pa[78][0] = fd->dma_para->fd_out_hw_pa[77][0] +
					    2 * pstv->out_xsize_plus_1[78];
	fd->dma_para->fd_out_hw_pa[78][1] = fd->dma_para->fd_out_hw_pa[77][0] +
					    3 * pstv->out_xsize_plus_1[78];
	fd->dma_para->fd_out_hw_pa[79][0] = fd->dma_para->fd_out_hw_pa[77][0] +
					    4 * pstv->out_xsize_plus_1[79];
	fd->dma_para->fd_out_hw_pa[80][0] =
		fd->dma_para->fd_out_hw_pa[77][0] + pstv->fd_wdma_size[77][0] +
		pstv->fd_wdma_size[77][1] + pstv->fd_wdma_size[78][0] +
		pstv->fd_wdma_size[78][1] + pstv->fd_wdma_size[79][0];
	fd->dma_para->fd_out_hw_pa[80][1] =
		fd->dma_para->fd_out_hw_pa[80][0] + pstv->fd_wdma_size[80][0] +
		pstv->fd_wdma_size[80][2] + pstv->fd_wdma_size[81][0] +
		pstv->fd_wdma_size[81][2] + pstv->fd_wdma_size[82][0];
	fd->dma_para->fd_out_hw_pa[80][2] =
		fd->dma_para->fd_out_hw_pa[80][0] + pstv->out_xsize_plus_1[80];
	fd->dma_para->fd_out_hw_pa[80][3] =
		fd->dma_para->fd_out_hw_pa[80][1] + pstv->out_xsize_plus_1[80];
	fd->dma_para->fd_out_hw_pa[81][0] = fd->dma_para->fd_out_hw_pa[80][0] +
					    2 * pstv->out_xsize_plus_1[81];
	fd->dma_para->fd_out_hw_pa[81][1] = fd->dma_para->fd_out_hw_pa[80][1] +
					    2 * pstv->out_xsize_plus_1[81];
	fd->dma_para->fd_out_hw_pa[81][2] = fd->dma_para->fd_out_hw_pa[80][0] +
					    3 * pstv->out_xsize_plus_1[81];
	fd->dma_para->fd_out_hw_pa[81][3] = fd->dma_para->fd_out_hw_pa[80][1] +
					    3 * pstv->out_xsize_plus_1[81];
	fd->dma_para->fd_out_hw_pa[82][0] = fd->dma_para->fd_out_hw_pa[80][0] +
					    4 * pstv->out_xsize_plus_1[82];
	fd->dma_para->fd_out_hw_pa[82][1] = fd->dma_para->fd_out_hw_pa[80][1] +
					    4 * pstv->out_xsize_plus_1[82];
	fd->dma_para->fd_out_hw_pa[83][0] =
		fd->dma_para->fd_out_hw_pa[80][1] + pstv->fd_wdma_size[80][1] +
		pstv->fd_wdma_size[80][3] + pstv->fd_wdma_size[81][1] +
		pstv->fd_wdma_size[81][3] + pstv->fd_wdma_size[82][1];
	fd->dma_para->fd_out_hw_pa[83][1] =
		fd->dma_para->fd_out_hw_pa[83][0] + pstv->out_xsize_plus_1[83];
	fd->dma_para->fd_out_hw_pa[84][0] = fd->dma_para->fd_out_hw_pa[83][0] +
					    2 * pstv->out_xsize_plus_1[84];
	fd->dma_para->fd_out_hw_pa[84][1] = fd->dma_para->fd_out_hw_pa[83][0] +
					    3 * pstv->out_xsize_plus_1[84];
	fd->dma_para->fd_out_hw_pa[85][0] = fd->dma_para->fd_out_hw_pa[83][0] +
					    4 * pstv->out_xsize_plus_1[85];
#if CHECK_SERVICE_0
	/* VA : except 28, 57, 86 */
	/* 0~86 */
	fd->dma_para->fd_out_hw_va[0][0] = fd->fd_dma_hw.va;
	for (i = 1; i < fd_loop_num; i++) {
		if (i == rpn2_loop_num || i == rpn1_loop_num ||
		    i == rpn0_loop_num)
			continue;
		for (j = 0; j < 4; j++) {
			if (fd_wdma_en[i][j]) {
				fd->dma_para->fd_out_hw_va[i][j] =
					fd->fd_dma_hw.va +
					fd->dma_para->fd_out_hw_pa[i][j] -
					fd->fd_dma_hw.pa;
			}
		}
	}
#endif
	currentPA = fd->dma_para->fd_out_hw_pa[83][0] +
		    pstv->fd_wdma_size[83][0] + pstv->fd_wdma_size[83][1] +
		    pstv->fd_wdma_size[84][0] + pstv->fd_wdma_size[84][1] +
		    pstv->fd_wdma_size[85][0];

#if CHECK_SERVICE_0
	currentVA = fd->dma_para->fd_out_hw_va[83][0] +
		    pstv->fd_wdma_size[83][0] + pstv->fd_wdma_size[83][1] +
		    pstv->fd_wdma_size[84][0] + pstv->fd_wdma_size[84][1] +
		    pstv->fd_wdma_size[85][0];
#endif
}

static void aie_arrange_kernel_buf(struct mtk_aie_dev *fd)
{
	void *currentVA = NULL;
	dma_addr_t currentPA;
	u8 i, j;

	currentPA = fd->fd_kernel_hw.pa;
	currentVA = fd->fd_kernel_hw.va;

	for (i = 0; i < fd_loop_num; i++) {
		for (j = 0; j < kernel_RDMA_RA_num; j++) {
			if (fd_ker_rdma_size[i][j]) {
				fd->dma_para->fd_kernel_pa[i][j] = currentPA;
				fd->dma_para->fd_kernel_va[i][j] = currentVA;
				currentPA += fd_ker_rdma_size[i][j];
				currentVA += fd_ker_rdma_size[i][j];
			}
		}
	}

	for (i = 0; i < attr_loop_num; i++) {
		for (j = 0; j < kernel_RDMA_RA_num; j++) {
			fd->dma_para->attr_kernel_pa[i][j] = currentPA;
			fd->dma_para->attr_kernel_va[i][j] = currentVA;
			currentPA += attr_ker_rdma_size[i][j];
			currentVA += attr_ker_rdma_size[i][j];
		}
	}
}

static void aie_arrange_attrdma_buf(struct mtk_aie_dev *fd)
{
	void *currentVA = NULL;
	dma_addr_t currentPA;
	u8 i, j;

	currentPA = fd->fd_attr_dma_hw.pa;
	currentVA = fd->fd_attr_dma_hw.va;

	/* attribute mode */
	for (i = 0; i < attr_loop_num; i++) {
		for (j = 0; j < output_WDMA_WRA_num; j++) {
			if (attr_wdma_en[i][j]) {
				fd->dma_para->attr_out_hw_pa[i][j] = currentPA;
				fd->dma_para->attr_out_hw_va[i][j] = currentVA;
				currentPA += attr_wdma_size[i][j];
				currentVA += attr_wdma_size[i][j];
			}
		}
	}
}

static void aie_arrange_result_dma_buf(struct mtk_aie_dev *fd)
{
	void *currentResultVA = NULL;
	dma_addr_t currentResultPA;
	u8 i;
	struct aie_static_info *pstv;

	pstv = &fd->st_info;

	currentResultPA = fd->fd_dma_result_hw.pa;
	currentResultVA = fd->fd_dma_result_hw.va;

	fd->dma_para->fd_out_hw_pa[rpn2_loop_num][0] = currentResultPA;
	fd->dma_para->fd_out_hw_va[rpn2_loop_num][0] = currentResultVA;
	currentResultPA += pstv->fd_wdma_size[rpn2_loop_num][0];
	currentResultVA += pstv->fd_wdma_size[rpn2_loop_num][0];
	fd->dma_para->fd_out_hw_pa[rpn1_loop_num][0] = currentResultPA;
	fd->dma_para->fd_out_hw_va[rpn1_loop_num][0] = currentResultVA;
	currentResultPA += pstv->fd_wdma_size[rpn1_loop_num][0];
	currentResultVA += pstv->fd_wdma_size[rpn1_loop_num][0];
	fd->dma_para->fd_out_hw_pa[rpn0_loop_num][0] = currentResultPA;
	fd->dma_para->fd_out_hw_va[rpn0_loop_num][0] = currentResultVA;
	currentResultPA += pstv->fd_wdma_size[rpn0_loop_num][0];
	currentResultVA += pstv->fd_wdma_size[rpn0_loop_num][0];

	fd->dma_para->attr_out_hw_pa[age_out_rgs][0] = currentResultPA;
	fd->dma_para->attr_out_hw_va[age_out_rgs][0] = currentResultVA;
	currentResultPA += ATTR_OUT_SIZE * MAX_ENQUE_FRAME_NUM;
	currentResultVA += ATTR_OUT_SIZE * MAX_ENQUE_FRAME_NUM;
	fd->dma_para->attr_out_hw_pa[gender_out_rgs][0] = currentResultPA;
	fd->dma_para->attr_out_hw_va[gender_out_rgs][0] = currentResultVA;
	currentResultPA += ATTR_OUT_SIZE * MAX_ENQUE_FRAME_NUM;
	currentResultVA += ATTR_OUT_SIZE * MAX_ENQUE_FRAME_NUM;
	fd->dma_para->attr_out_hw_pa[indian_out_rgs][0] = currentResultPA;
	fd->dma_para->attr_out_hw_va[indian_out_rgs][0] = currentResultVA;
	currentResultPA += ATTR_OUT_SIZE * MAX_ENQUE_FRAME_NUM;
	currentResultVA += ATTR_OUT_SIZE * MAX_ENQUE_FRAME_NUM;
	fd->dma_para->attr_out_hw_pa[race_out_rgs][0] = currentResultPA;
	fd->dma_para->attr_out_hw_va[race_out_rgs][0] = currentResultVA;
	currentResultPA += ATTR_OUT_SIZE * MAX_ENQUE_FRAME_NUM;
	currentResultVA += ATTR_OUT_SIZE * MAX_ENQUE_FRAME_NUM;

	/* need to prepare 10 buffers to store 10 times result */
	fd->dma_para->age_out_hw_pa[0] =
		fd->dma_para->attr_out_hw_pa[age_out_rgs][0];
	fd->dma_para->age_out_hw_va[0] =
		fd->dma_para->attr_out_hw_va[age_out_rgs][0];
	fd->dma_para->gender_out_hw_pa[0] =
		fd->dma_para->attr_out_hw_pa[gender_out_rgs][0];
	fd->dma_para->gender_out_hw_va[0] =
		fd->dma_para->attr_out_hw_va[gender_out_rgs][0];
	fd->dma_para->isIndian_out_hw_pa[0] =
		fd->dma_para->attr_out_hw_pa[indian_out_rgs][0];
	fd->dma_para->isIndian_out_hw_va[0] =
		fd->dma_para->attr_out_hw_va[indian_out_rgs][0];
	fd->dma_para->race_out_hw_pa[0] =
		fd->dma_para->attr_out_hw_pa[race_out_rgs][0];
	fd->dma_para->race_out_hw_va[0] =
		fd->dma_para->attr_out_hw_va[race_out_rgs][0];

	for (i = 1; i < MAX_ENQUE_FRAME_NUM; i++) {
		fd->dma_para->age_out_hw_pa[i] =
			fd->dma_para->age_out_hw_pa[i - 1] + ATTR_OUT_SIZE;
		fd->dma_para->age_out_hw_va[i] =
			fd->dma_para->age_out_hw_va[i - 1] + ATTR_OUT_SIZE;
		fd->dma_para->gender_out_hw_pa[i] =
			fd->dma_para->gender_out_hw_pa[i - 1] + ATTR_OUT_SIZE;
		fd->dma_para->gender_out_hw_va[i] =
			fd->dma_para->gender_out_hw_va[i - 1] + ATTR_OUT_SIZE;
		fd->dma_para->isIndian_out_hw_pa[i] =
			fd->dma_para->isIndian_out_hw_pa[i - 1] + ATTR_OUT_SIZE;
		fd->dma_para->isIndian_out_hw_va[i] =
			fd->dma_para->isIndian_out_hw_va[i - 1] + ATTR_OUT_SIZE;
		fd->dma_para->race_out_hw_pa[i] =
			fd->dma_para->race_out_hw_pa[i - 1] + ATTR_OUT_SIZE;
		fd->dma_para->race_out_hw_va[i] =
			fd->dma_para->race_out_hw_va[i - 1] + ATTR_OUT_SIZE;
	}

	memset(fd->fd_dma_result_hw.va, 0, fd->fd_dma_result_hw.size);

}
#ifdef FLD
static void aie_arrange_fld_buf(struct mtk_aie_dev *fd)
{
	int input_index = 0;
	int msb_bit_0 = 0, msb_bit_1 = 0, msb_bit_2 = 0, msb_bit_3 = 0;
	int msb_bit_4 = 0, msb_bit_5 = 0, msb_bit_6 = 0, msb_bit_7 = 0;
	int set_msb_bit = 0;

	fd->dma_para->fld_blink_weight_va = fd->fld_blink_weight_hw.va;
	fd->dma_para->fld_blink_weight_pa = fd->fld_blink_weight_hw.pa;

	fd->dma_para->fld_output_va = fd->fld_output_hw.va;
	fd->dma_para->fld_output_pa = fd->fld_output_hw.pa;
	fd->fld_para->fld_output_va = fd->dma_para->fld_output_va;
	fd->fld_para->fld_output_pa = fd->dma_para->fld_output_pa;

	fd->dma_para->fld_cv_va[0] = fd->fld_cv_hw.va;
	fd->dma_para->fld_cv_pa[0] = fd->fld_cv_hw.pa;
	fd->dma_para->fld_fp_va[0] = fd->fld_fp_hw.va;
	fd->dma_para->fld_fp_pa[0] = fd->fld_fp_hw.pa;

	fd->dma_para->fld_leafnode_va[0] = fd->fld_leafnode_hw.va;
	fd->dma_para->fld_leafnode_pa[0] = fd->fld_leafnode_hw.pa;
	fd->dma_para->fld_tree02_va[0] = fd->fld_tree_02_hw.va;
	fd->dma_para->fld_tree02_pa[0] = fd->fld_tree_02_hw.pa;
	fd->dma_para->fld_tree13_va[0] = fd->fld_tree_13_hw.va;
	fd->dma_para->fld_tree13_pa[0] = fd->fld_tree_13_hw.pa;

	fd->dma_para->fld_cv_va[1] = fd->dma_para->fld_cv_va[0] + fld_cv_size_00;
	fd->dma_para->fld_cv_pa[1] = fd->dma_para->fld_cv_pa[0] + fld_cv_size_00;
	fd->dma_para->fld_fp_va[1] = fd->dma_para->fld_fp_va[0] + fld_fp_size;
	fd->dma_para->fld_fp_pa[1] = fd->dma_para->fld_fp_pa[0] + fld_fp_size;
	fd->dma_para->fld_leafnode_va[1] = fd->dma_para->fld_leafnode_va[0] + fld_leafnode_size;
	fd->dma_para->fld_leafnode_pa[1] = fd->dma_para->fld_leafnode_pa[0] + fld_leafnode_size;
	fd->dma_para->fld_tree02_va[1] = fd->dma_para->fld_tree02_va[0] + fld_tree_size;
	fd->dma_para->fld_tree02_pa[1] = fd->dma_para->fld_tree02_pa[0] + fld_tree_size;
	fd->dma_para->fld_tree13_va[1] = fd->dma_para->fld_tree13_va[0] + fld_tree_size;
	fd->dma_para->fld_tree13_pa[1] = fd->dma_para->fld_tree13_pa[0] + fld_tree_size;

	for (input_index = 1; input_index < FLD_MAX_INPUT - 1; input_index++) {
		fd->dma_para->fld_cv_va[input_index + 1] = fd->dma_para->fld_cv_va[input_index] +
							     fld_cv_size;
		fd->dma_para->fld_cv_pa[input_index + 1] = fd->dma_para->fld_cv_pa[input_index] +
							     fld_cv_size;
		fd->dma_para->fld_fp_va[input_index + 1] = fd->dma_para->fld_fp_va[input_index] +
							     fld_fp_size;
		fd->dma_para->fld_fp_pa[input_index + 1] = fd->dma_para->fld_fp_pa[input_index] +
							     fld_fp_size;
		fd->dma_para->fld_leafnode_va[input_index + 1] =
				fd->dma_para->fld_leafnode_va[input_index] + fld_leafnode_size;
		fd->dma_para->fld_leafnode_pa[input_index + 1] =
				fd->dma_para->fld_leafnode_pa[input_index] + fld_leafnode_size;
		fd->dma_para->fld_tree02_va[input_index + 1] =
				fd->dma_para->fld_tree02_va[input_index] + fld_tree_size;
		fd->dma_para->fld_tree02_pa[input_index + 1] =
				fd->dma_para->fld_tree02_pa[input_index] + fld_tree_size;
		fd->dma_para->fld_tree13_va[input_index + 1] =
				fd->dma_para->fld_tree13_va[input_index] + fld_tree_size;
		fd->dma_para->fld_tree13_pa[input_index + 1] =
				fd->dma_para->fld_tree13_pa[input_index] + fld_tree_size;
	}
	//fp
	msb_bit_0 = (fd->dma_para->fld_fp_pa[0] & 0xf00000000) >> 32;
	msb_bit_1 = (fd->dma_para->fld_fp_pa[1] & 0xf00000000) >> 32;
	msb_bit_2 = (fd->dma_para->fld_fp_pa[2] & 0xf00000000) >> 32;
	msb_bit_3 = (fd->dma_para->fld_fp_pa[3] & 0xf00000000) >> 32;
	msb_bit_4 = (fd->dma_para->fld_fp_pa[4] & 0xf00000000) >> 32;
	msb_bit_5 = (fd->dma_para->fld_fp_pa[5] & 0xf00000000) >> 32;
	msb_bit_6 = (fd->dma_para->fld_fp_pa[6] & 0xf00000000) >> 32;
	msb_bit_7 = (fd->dma_para->fld_fp_pa[7] & 0xf00000000) >> 32;

	set_msb_bit = msb_bit_0 | msb_bit_1 << 4 | msb_bit_2 << 8 | msb_bit_3 << 12
		| msb_bit_4 << 16 | msb_bit_5 << 20 | msb_bit_6 << 24 | msb_bit_7 << 28;

	writel(set_msb_bit, fd->fd_base + FLD_PL_IN_BASE_ADDR_3_0_7_MSB);

	msb_bit_0 = (fd->dma_para->fld_fp_pa[8] & 0xf00000000) >> 32;
	msb_bit_1 = (fd->dma_para->fld_fp_pa[9] & 0xf00000000) >> 32;
	msb_bit_2 = (fd->dma_para->fld_fp_pa[10] & 0xf00000000) >> 32;
	msb_bit_3 = (fd->dma_para->fld_fp_pa[11] & 0xf00000000) >> 32;
	msb_bit_4 = (fd->dma_para->fld_fp_pa[12] & 0xf00000000) >> 32;
	msb_bit_5 = (fd->dma_para->fld_fp_pa[13] & 0xf00000000) >> 32;
	msb_bit_6 = (fd->dma_para->fld_fp_pa[14] & 0xf00000000) >> 32;

	set_msb_bit = msb_bit_0 | msb_bit_1 << 4 | msb_bit_2 << 8 | msb_bit_3 << 12
		| msb_bit_4 << 16 | msb_bit_5 << 20 | msb_bit_6 << 24;

	writel(set_msb_bit, fd->fd_base + FLD_PL_IN_BASE_ADDR_3_8_15_MSB);

	//cv
	msb_bit_0 = (fd->dma_para->fld_cv_pa[0] & 0xf00000000) >> 32;
	msb_bit_1 = (fd->dma_para->fld_cv_pa[1] & 0xf00000000) >> 32;
	msb_bit_2 = (fd->dma_para->fld_cv_pa[2] & 0xf00000000) >> 32;
	msb_bit_3 = (fd->dma_para->fld_cv_pa[3] & 0xf00000000) >> 32;
	msb_bit_4 = (fd->dma_para->fld_cv_pa[4] & 0xf00000000) >> 32;
	msb_bit_5 = (fd->dma_para->fld_cv_pa[5] & 0xf00000000) >> 32;
	msb_bit_6 = (fd->dma_para->fld_cv_pa[6] & 0xf00000000) >> 32;
	msb_bit_7 = (fd->dma_para->fld_cv_pa[7] & 0xf00000000) >> 32;

	set_msb_bit = msb_bit_0 | msb_bit_1 << 4 | msb_bit_2 << 8 | msb_bit_3 << 12
		| msb_bit_4 << 16 | msb_bit_5 << 20 | msb_bit_6 << 24 | msb_bit_7 << 28;

	writel(set_msb_bit, fd->fd_base + FLD_PL_IN_BASE_ADDR_2_0_7_MSB);

	msb_bit_0 = (fd->dma_para->fld_cv_pa[8] & 0xf00000000) >> 32;
	msb_bit_1 = (fd->dma_para->fld_cv_pa[9] & 0xf00000000) >> 32;
	msb_bit_2 = (fd->dma_para->fld_cv_pa[10] & 0xf00000000) >> 32;
	msb_bit_3 = (fd->dma_para->fld_cv_pa[11] & 0xf00000000) >> 32;
	msb_bit_4 = (fd->dma_para->fld_cv_pa[12] & 0xf00000000) >> 32;
	msb_bit_5 = (fd->dma_para->fld_cv_pa[13] & 0xf00000000) >> 32;
	msb_bit_6 = (fd->dma_para->fld_cv_pa[14] & 0xf00000000) >> 32;

	set_msb_bit = msb_bit_0 | msb_bit_1 << 4 | msb_bit_2 << 8 | msb_bit_3 << 12
		| msb_bit_4 << 16 | msb_bit_5 << 20 | msb_bit_6 << 24;

	writel(set_msb_bit, fd->fd_base + FLD_PL_IN_BASE_ADDR_2_8_15_MSB);

	//leafnode
	msb_bit_0 = (fd->dma_para->fld_leafnode_pa[0] & 0xf00000000) >> 32;
	msb_bit_1 = (fd->dma_para->fld_leafnode_pa[1] & 0xf00000000) >> 32;
	msb_bit_2 = (fd->dma_para->fld_leafnode_pa[2] & 0xf00000000) >> 32;
	msb_bit_3 = (fd->dma_para->fld_leafnode_pa[3] & 0xf00000000) >> 32;
	msb_bit_4 = (fd->dma_para->fld_leafnode_pa[4] & 0xf00000000) >> 32;
	msb_bit_5 = (fd->dma_para->fld_leafnode_pa[5] & 0xf00000000) >> 32;
	msb_bit_6 = (fd->dma_para->fld_leafnode_pa[6] & 0xf00000000) >> 32;
	msb_bit_7 = (fd->dma_para->fld_leafnode_pa[7] & 0xf00000000) >> 32;

	set_msb_bit = msb_bit_0 | msb_bit_1 << 4 | msb_bit_2 << 8 | msb_bit_3 << 12
		| msb_bit_4 << 16 | msb_bit_5 << 20 | msb_bit_6 << 24 | msb_bit_7 << 28;

	writel(set_msb_bit, fd->fd_base + FLD_SH_IN_BASE_ADDR_0_7_MSB);

	msb_bit_0 = (fd->dma_para->fld_leafnode_pa[8] & 0xf00000000) >> 32;
	msb_bit_1 = (fd->dma_para->fld_leafnode_pa[9] & 0xf00000000) >> 32;
	msb_bit_2 = (fd->dma_para->fld_leafnode_pa[10] & 0xf00000000) >> 32;
	msb_bit_3 = (fd->dma_para->fld_leafnode_pa[11] & 0xf00000000) >> 32;
	msb_bit_4 = (fd->dma_para->fld_leafnode_pa[12] & 0xf00000000) >> 32;
	msb_bit_5 = (fd->dma_para->fld_leafnode_pa[13] & 0xf00000000) >> 32;
	msb_bit_6 = (fd->dma_para->fld_leafnode_pa[14] & 0xf00000000) >> 32;

	set_msb_bit = msb_bit_0 | msb_bit_1 << 4 | msb_bit_2 << 8 | msb_bit_3 << 12
		| msb_bit_4 << 16 | msb_bit_5 << 20 | msb_bit_6 << 24;

	writel(set_msb_bit, fd->fd_base + FLD_SH_IN_BASE_ADDR_8_15_MSB);

	//02tree
	msb_bit_0 = (fd->dma_para->fld_tree02_pa[0] & 0xf00000000) >> 32;
	msb_bit_1 = (fd->dma_para->fld_tree02_pa[1] & 0xf00000000) >> 32;
	msb_bit_2 = (fd->dma_para->fld_tree02_pa[2] & 0xf00000000) >> 32;
	msb_bit_3 = (fd->dma_para->fld_tree02_pa[3] & 0xf00000000) >> 32;
	msb_bit_4 = (fd->dma_para->fld_tree02_pa[4] & 0xf00000000) >> 32;
	msb_bit_5 = (fd->dma_para->fld_tree02_pa[5] & 0xf00000000) >> 32;
	msb_bit_6 = (fd->dma_para->fld_tree02_pa[6] & 0xf00000000) >> 32;
	msb_bit_7 = (fd->dma_para->fld_tree02_pa[7] & 0xf00000000) >> 32;

	set_msb_bit = msb_bit_0 | msb_bit_1 << 4 | msb_bit_2 << 8 | msb_bit_3 << 12
		| msb_bit_4 << 16 | msb_bit_5 << 20 | msb_bit_6 << 24 | msb_bit_7 << 28;

	writel(set_msb_bit, fd->fd_base + FLD_PL_IN_BASE_ADDR_0_0_7_MSB);

	msb_bit_0 = (fd->dma_para->fld_tree02_pa[8] & 0xf00000000) >> 32;
	msb_bit_1 = (fd->dma_para->fld_tree02_pa[9] & 0xf00000000) >> 32;
	msb_bit_2 = (fd->dma_para->fld_tree02_pa[10] & 0xf00000000) >> 32;
	msb_bit_3 = (fd->dma_para->fld_tree02_pa[11] & 0xf00000000) >> 32;
	msb_bit_4 = (fd->dma_para->fld_tree02_pa[12] & 0xf00000000) >> 32;
	msb_bit_5 = (fd->dma_para->fld_tree02_pa[13] & 0xf00000000) >> 32;
	msb_bit_6 = (fd->dma_para->fld_tree02_pa[14] & 0xf00000000) >> 32;

	set_msb_bit = msb_bit_0 | msb_bit_1 << 4 | msb_bit_2 << 8 | msb_bit_3 << 12
		| msb_bit_4 << 16 | msb_bit_5 << 20 | msb_bit_6 << 24;

	writel(set_msb_bit, fd->fd_base + FLD_PL_IN_BASE_ADDR_0_8_15_MSB);

	//13tree
	msb_bit_0 = (fd->dma_para->fld_tree13_pa[0] & 0xf00000000) >> 32;
	msb_bit_1 = (fd->dma_para->fld_tree13_pa[1] & 0xf00000000) >> 32;
	msb_bit_2 = (fd->dma_para->fld_tree13_pa[2] & 0xf00000000) >> 32;
	msb_bit_3 = (fd->dma_para->fld_tree13_pa[3] & 0xf00000000) >> 32;
	msb_bit_4 = (fd->dma_para->fld_tree13_pa[4] & 0xf00000000) >> 32;
	msb_bit_5 = (fd->dma_para->fld_tree13_pa[5] & 0xf00000000) >> 32;
	msb_bit_6 = (fd->dma_para->fld_tree13_pa[6] & 0xf00000000) >> 32;
	msb_bit_7 = (fd->dma_para->fld_tree13_pa[7] & 0xf00000000) >> 32;

	set_msb_bit = msb_bit_0 | msb_bit_1 << 4 | msb_bit_2 << 8 | msb_bit_3 << 12
		| msb_bit_4 << 16 | msb_bit_5 << 20 | msb_bit_6 << 24 | msb_bit_7 << 28;

	writel(set_msb_bit, fd->fd_base + FLD_PL_IN_BASE_ADDR_1_0_7_MSB);


	msb_bit_0 = (fd->dma_para->fld_tree13_pa[8] & 0xf00000000) >> 32;
	msb_bit_1 = (fd->dma_para->fld_tree13_pa[9] & 0xf00000000) >> 32;
	msb_bit_2 = (fd->dma_para->fld_tree13_pa[10] & 0xf00000000) >> 32;
	msb_bit_3 = (fd->dma_para->fld_tree13_pa[11] & 0xf00000000) >> 32;
	msb_bit_4 = (fd->dma_para->fld_tree13_pa[12] & 0xf00000000) >> 32;
	msb_bit_5 = (fd->dma_para->fld_tree13_pa[13] & 0xf00000000) >> 32;
	msb_bit_6 = (fd->dma_para->fld_tree13_pa[14] & 0xf00000000) >> 32;

	set_msb_bit = msb_bit_0 | msb_bit_1 << 4 | msb_bit_2 << 8 | msb_bit_3 << 12
		| msb_bit_4 << 16 | msb_bit_5 << 20 | msb_bit_6 << 24;

	writel(set_msb_bit, fd->fd_base + FLD_PL_IN_BASE_ADDR_1_8_15_MSB);

}
#endif
static void aie_update_fddma_buf(struct mtk_aie_dev *fd)
{
	struct aie_static_info *pstv;

	pstv = &fd->st_info;

	/* 19~27 */
	fd->dma_para->fd_out_hw_pa[19][0] =
		fd->dma_para->fd_out_hw_pa[18][1] + pstv->fd_wdma_size[18][1];
	fd->dma_para->fd_out_hw_pa[19][1] =
		fd->dma_para->fd_out_hw_pa[19][0] + pstv->out_xsize_plus_1[19];
	fd->dma_para->fd_out_hw_pa[20][0] = fd->dma_para->fd_out_hw_pa[19][0] +
					    2 * pstv->out_xsize_plus_1[20];
	fd->dma_para->fd_out_hw_pa[20][1] = fd->dma_para->fd_out_hw_pa[19][0] +
					    3 * pstv->out_xsize_plus_1[20];
	fd->dma_para->fd_out_hw_pa[21][0] = fd->dma_para->fd_out_hw_pa[19][0] +
					    4 * pstv->out_xsize_plus_1[21];
	fd->dma_para->fd_out_hw_pa[22][0] =
		fd->dma_para->fd_out_hw_pa[19][0] + pstv->fd_wdma_size[19][0] +
		pstv->fd_wdma_size[19][1] + pstv->fd_wdma_size[20][0] +
		pstv->fd_wdma_size[20][1] + pstv->fd_wdma_size[21][0];
	fd->dma_para->fd_out_hw_pa[22][1] =
		fd->dma_para->fd_out_hw_pa[22][0] + pstv->fd_wdma_size[22][0] +
		pstv->fd_wdma_size[22][2] + pstv->fd_wdma_size[23][0] +
		pstv->fd_wdma_size[23][2] + pstv->fd_wdma_size[24][0];
	fd->dma_para->fd_out_hw_pa[22][2] =
		fd->dma_para->fd_out_hw_pa[22][0] + pstv->out_xsize_plus_1[22];
	fd->dma_para->fd_out_hw_pa[22][3] =
		fd->dma_para->fd_out_hw_pa[22][1] + pstv->out_xsize_plus_1[22];
	fd->dma_para->fd_out_hw_pa[23][0] = fd->dma_para->fd_out_hw_pa[22][0] +
					    2 * pstv->out_xsize_plus_1[23];
	fd->dma_para->fd_out_hw_pa[23][1] = fd->dma_para->fd_out_hw_pa[22][1] +
					    2 * pstv->out_xsize_plus_1[23];
	fd->dma_para->fd_out_hw_pa[23][2] = fd->dma_para->fd_out_hw_pa[22][0] +
					    3 * pstv->out_xsize_plus_1[23];
	fd->dma_para->fd_out_hw_pa[23][3] = fd->dma_para->fd_out_hw_pa[22][1] +
					    3 * pstv->out_xsize_plus_1[23];
	fd->dma_para->fd_out_hw_pa[24][0] = fd->dma_para->fd_out_hw_pa[22][0] +
					    4 * pstv->out_xsize_plus_1[24];
	fd->dma_para->fd_out_hw_pa[24][1] = fd->dma_para->fd_out_hw_pa[22][1] +
					    4 * pstv->out_xsize_plus_1[24];
	fd->dma_para->fd_out_hw_pa[25][0] =
		fd->dma_para->fd_out_hw_pa[22][1] + pstv->fd_wdma_size[22][1] +
		pstv->fd_wdma_size[22][3] + pstv->fd_wdma_size[23][1] +
		pstv->fd_wdma_size[23][3] + pstv->fd_wdma_size[24][1];
	fd->dma_para->fd_out_hw_pa[25][1] =
		fd->dma_para->fd_out_hw_pa[25][0] + pstv->out_xsize_plus_1[25];
	fd->dma_para->fd_out_hw_pa[26][0] = fd->dma_para->fd_out_hw_pa[25][0] +
					    2 * pstv->out_xsize_plus_1[26];
	fd->dma_para->fd_out_hw_pa[26][1] = fd->dma_para->fd_out_hw_pa[25][0] +
					    3 * pstv->out_xsize_plus_1[26];
	fd->dma_para->fd_out_hw_pa[27][0] = fd->dma_para->fd_out_hw_pa[25][0] +
					    4 * pstv->out_xsize_plus_1[27];

	/* 48~56 */
	fd->dma_para->fd_out_hw_pa[48][0] =
		fd->dma_para->fd_out_hw_pa[47][1] + pstv->fd_wdma_size[47][1];
	fd->dma_para->fd_out_hw_pa[48][1] =
		fd->dma_para->fd_out_hw_pa[48][0] + pstv->out_xsize_plus_1[48];
	fd->dma_para->fd_out_hw_pa[49][0] = fd->dma_para->fd_out_hw_pa[48][0] +
					    2 * pstv->out_xsize_plus_1[49];
	fd->dma_para->fd_out_hw_pa[49][1] = fd->dma_para->fd_out_hw_pa[48][0] +
					    3 * pstv->out_xsize_plus_1[49];
	fd->dma_para->fd_out_hw_pa[50][0] = fd->dma_para->fd_out_hw_pa[48][0] +
					    4 * pstv->out_xsize_plus_1[50];
	fd->dma_para->fd_out_hw_pa[51][0] =
		fd->dma_para->fd_out_hw_pa[48][0] + pstv->fd_wdma_size[48][0] +
		pstv->fd_wdma_size[48][1] + pstv->fd_wdma_size[49][0] +
		pstv->fd_wdma_size[49][1] + pstv->fd_wdma_size[50][0];
	fd->dma_para->fd_out_hw_pa[51][1] =
		fd->dma_para->fd_out_hw_pa[51][0] + pstv->fd_wdma_size[51][0] +
		pstv->fd_wdma_size[51][2] + pstv->fd_wdma_size[52][0] +
		pstv->fd_wdma_size[52][2] + pstv->fd_wdma_size[53][0];
	fd->dma_para->fd_out_hw_pa[51][2] =
		fd->dma_para->fd_out_hw_pa[51][0] + pstv->out_xsize_plus_1[51];
	fd->dma_para->fd_out_hw_pa[51][3] =
		fd->dma_para->fd_out_hw_pa[51][1] + pstv->out_xsize_plus_1[51];
	fd->dma_para->fd_out_hw_pa[52][0] = fd->dma_para->fd_out_hw_pa[51][0] +
					    2 * pstv->out_xsize_plus_1[52];
	fd->dma_para->fd_out_hw_pa[52][1] = fd->dma_para->fd_out_hw_pa[51][1] +
					    2 * pstv->out_xsize_plus_1[52];
	fd->dma_para->fd_out_hw_pa[52][2] = fd->dma_para->fd_out_hw_pa[51][0] +
					    3 * pstv->out_xsize_plus_1[52];
	fd->dma_para->fd_out_hw_pa[52][3] = fd->dma_para->fd_out_hw_pa[51][1] +
					    3 * pstv->out_xsize_plus_1[52];
	fd->dma_para->fd_out_hw_pa[53][0] = fd->dma_para->fd_out_hw_pa[51][0] +
					    4 * pstv->out_xsize_plus_1[53];
	fd->dma_para->fd_out_hw_pa[53][1] = fd->dma_para->fd_out_hw_pa[51][1] +
					    4 * pstv->out_xsize_plus_1[53];
	fd->dma_para->fd_out_hw_pa[54][0] =
		fd->dma_para->fd_out_hw_pa[51][1] + pstv->fd_wdma_size[51][1] +
		pstv->fd_wdma_size[51][3] + pstv->fd_wdma_size[52][1] +
		pstv->fd_wdma_size[52][3] + pstv->fd_wdma_size[53][1];
	fd->dma_para->fd_out_hw_pa[54][1] =
		fd->dma_para->fd_out_hw_pa[54][0] + pstv->out_xsize_plus_1[54];
	fd->dma_para->fd_out_hw_pa[55][0] = fd->dma_para->fd_out_hw_pa[54][0] +
					    2 * pstv->out_xsize_plus_1[55];
	fd->dma_para->fd_out_hw_pa[55][1] = fd->dma_para->fd_out_hw_pa[54][0] +
					    3 * pstv->out_xsize_plus_1[55];
	fd->dma_para->fd_out_hw_pa[56][0] = fd->dma_para->fd_out_hw_pa[54][0] +
					    4 * pstv->out_xsize_plus_1[56];
	/* 77~85 */
	fd->dma_para->fd_out_hw_pa[77][0] =
		fd->dma_para->fd_out_hw_pa[76][1] + pstv->fd_wdma_size[76][1];
	fd->dma_para->fd_out_hw_pa[77][1] =
		fd->dma_para->fd_out_hw_pa[77][0] + pstv->out_xsize_plus_1[77];
	fd->dma_para->fd_out_hw_pa[78][0] = fd->dma_para->fd_out_hw_pa[77][0] +
					    2 * pstv->out_xsize_plus_1[78];
	fd->dma_para->fd_out_hw_pa[78][1] = fd->dma_para->fd_out_hw_pa[77][0] +
					    3 * pstv->out_xsize_plus_1[78];
	fd->dma_para->fd_out_hw_pa[79][0] = fd->dma_para->fd_out_hw_pa[77][0] +
					    4 * pstv->out_xsize_plus_1[79];
	fd->dma_para->fd_out_hw_pa[80][0] =
		fd->dma_para->fd_out_hw_pa[77][0] + pstv->fd_wdma_size[77][0] +
		pstv->fd_wdma_size[77][1] + pstv->fd_wdma_size[78][0] +
		pstv->fd_wdma_size[78][1] + pstv->fd_wdma_size[79][0];
	fd->dma_para->fd_out_hw_pa[80][1] =
		fd->dma_para->fd_out_hw_pa[80][0] + pstv->fd_wdma_size[80][0] +
		pstv->fd_wdma_size[80][2] + pstv->fd_wdma_size[81][0] +
		pstv->fd_wdma_size[81][2] + pstv->fd_wdma_size[82][0];
	fd->dma_para->fd_out_hw_pa[80][2] =
		fd->dma_para->fd_out_hw_pa[80][0] + pstv->out_xsize_plus_1[80];
	fd->dma_para->fd_out_hw_pa[80][3] =
		fd->dma_para->fd_out_hw_pa[80][1] + pstv->out_xsize_plus_1[80];
	fd->dma_para->fd_out_hw_pa[81][0] = fd->dma_para->fd_out_hw_pa[80][0] +
					    2 * pstv->out_xsize_plus_1[81];
	fd->dma_para->fd_out_hw_pa[81][1] = fd->dma_para->fd_out_hw_pa[80][1] +
					    2 * pstv->out_xsize_plus_1[81];
	fd->dma_para->fd_out_hw_pa[81][2] = fd->dma_para->fd_out_hw_pa[80][0] +
					    3 * pstv->out_xsize_plus_1[81];
	fd->dma_para->fd_out_hw_pa[81][3] = fd->dma_para->fd_out_hw_pa[80][1] +
					    3 * pstv->out_xsize_plus_1[81];
	fd->dma_para->fd_out_hw_pa[82][0] = fd->dma_para->fd_out_hw_pa[80][0] +
					    4 * pstv->out_xsize_plus_1[82];
	fd->dma_para->fd_out_hw_pa[82][1] = fd->dma_para->fd_out_hw_pa[80][1] +
					    4 * pstv->out_xsize_plus_1[82];
	fd->dma_para->fd_out_hw_pa[83][0] =
		fd->dma_para->fd_out_hw_pa[80][1] + pstv->fd_wdma_size[80][1] +
		pstv->fd_wdma_size[80][3] + pstv->fd_wdma_size[81][1] +
		pstv->fd_wdma_size[81][3] + pstv->fd_wdma_size[82][1];
	fd->dma_para->fd_out_hw_pa[83][1] =
		fd->dma_para->fd_out_hw_pa[83][0] + pstv->out_xsize_plus_1[83];
	fd->dma_para->fd_out_hw_pa[84][0] = fd->dma_para->fd_out_hw_pa[83][0] +
					    2 * pstv->out_xsize_plus_1[84];
	fd->dma_para->fd_out_hw_pa[84][1] = fd->dma_para->fd_out_hw_pa[83][0] +
					    3 * pstv->out_xsize_plus_1[84];
	fd->dma_para->fd_out_hw_pa[85][0] = fd->dma_para->fd_out_hw_pa[83][0] +
					    4 * pstv->out_xsize_plus_1[85];

	/* VA : except 28, 57, 86 */
	/* 0~86 */
#if CHECK_SERVICE_0
	fd->dma_para->fd_out_hw_va[0][0] = fd->fd_dma_hw.va;
	for (i = 1; i < fd_loop_num; i++) {
		if (i == rpn2_loop_num || i == rpn1_loop_num ||
		    i == rpn0_loop_num)
			continue;
		for (j = 0; j < 4; j++) {
			if (fd_wdma_en[i][j]) {
				fd->dma_para->fd_out_hw_va[i][j] =
					fd->fd_dma_hw.va +
					fd->dma_para->fd_out_hw_pa[i][j] -
					fd->fd_dma_hw.pa;
			}
		}
	}
#endif
}
static void aie_free_sec_buf(struct mtk_aie_dev *fd)
{
	aie_free_iova(fd, &fd->rs_output_hw);
	aie_free_dmabuf(fd, &fd->rs_output_hw);
}

static void aie_free_dram_buf(struct mtk_aie_dev *fd)
{
	//aie_imem_free(fd, &fd->rs_cfg_data);
	aie_free_iova(fd, &fd->rs_cfg_data);
	aie_free_va(fd, &fd->rs_cfg_data);
	aie_free_dmabuf(fd, &fd->rs_cfg_data);

	//aie_imem_free(fd, &fd->fd_cfg_data);
	aie_free_iova(fd, &fd->fd_cfg_data);
	aie_free_va(fd, &fd->fd_cfg_data);
	aie_free_dmabuf(fd, &fd->fd_cfg_data);

	//aie_imem_free(fd, &fd->yuv2rgb_cfg_data);
	aie_free_iova(fd, &fd->yuv2rgb_cfg_data);
	aie_free_va(fd, &fd->yuv2rgb_cfg_data);
	aie_free_dmabuf(fd, &fd->yuv2rgb_cfg_data);

}

static void aie_free_output_buf(struct mtk_aie_dev *fd)
{
	aie_free_iova(fd, &fd->rs_output_hw);
	aie_free_va(fd, &fd->rs_output_hw);
	aie_free_dmabuf(fd, &fd->rs_output_hw);

}

static void aie_free_fddma_buf(struct mtk_aie_dev *fd)
{
	//aie_imem_free(fd, &fd->fd_dma_hw);
	aie_free_iova(fd, &fd->fd_dma_hw);
	aie_free_va(fd, &fd->fd_dma_hw);
	aie_free_dmabuf(fd, &fd->fd_dma_hw);

	//aie_imem_free(fd, &fd->fd_kernel_hw);
	aie_free_iova(fd, &fd->fd_kernel_hw);
	aie_free_va(fd, &fd->fd_kernel_hw);
	aie_free_dmabuf(fd, &fd->fd_kernel_hw);

	//aie_imem_free(fd, &fd->fd_attr_dma_hw);
	aie_free_iova(fd, &fd->fd_attr_dma_hw);
	aie_free_va(fd, &fd->fd_attr_dma_hw);
	aie_free_dmabuf(fd, &fd->fd_attr_dma_hw);

	//aie_imem_free(fd, &fd->fd_dma_result_hw);
	aie_free_iova(fd, &fd->fd_dma_result_hw);
	aie_free_va(fd, &fd->fd_dma_result_hw);
	aie_free_dmabuf(fd, &fd->fd_dma_result_hw);

}
#ifdef FLD
static void aie_free_fld_buf(struct mtk_aie_dev *fd)
{
	//aie_imem_free(fd, &fd->fld_blink_weight_hw);
	aie_free_iova(fd, &fd->fld_blink_weight_hw);
	aie_free_va(fd, &fd->fld_blink_weight_hw);
	aie_free_dmabuf(fd, &fd->fld_blink_weight_hw);

	//aie_imem_free(fd, &fd->fld_cv_hw);
	aie_free_iova(fd, &fd->fld_cv_hw);
	aie_free_va(fd, &fd->fld_cv_hw);
	aie_free_dmabuf(fd, &fd->fld_cv_hw);

	//aie_imem_free(fd, &fd->fld_fp_hw);
	aie_free_iova(fd, &fd->fld_fp_hw);
	aie_free_va(fd, &fd->fld_fp_hw);
	aie_free_dmabuf(fd, &fd->fld_fp_hw);

	//aie_imem_free(fd, &fd->fld_leafnode_hw);
	aie_free_iova(fd, &fd->fld_leafnode_hw);
	aie_free_va(fd, &fd->fld_leafnode_hw);
	aie_free_dmabuf(fd, &fd->fld_leafnode_hw);

	//aie_imem_free(fd, &fd->fld_tree_02_hw);
	aie_free_iova(fd, &fd->fld_tree_02_hw);
	aie_free_va(fd, &fd->fld_tree_02_hw);
	aie_free_dmabuf(fd, &fd->fld_tree_02_hw);

	//aie_imem_free(fd, &fd->fld_tree_13_hw);
	aie_free_iova(fd, &fd->fld_tree_13_hw);
	aie_free_va(fd, &fd->fld_tree_13_hw);
	aie_free_dmabuf(fd, &fd->fld_tree_13_hw);

	//aie_imem_free(fd, &fd->fld_output_hw);
	aie_free_iova(fd, &fd->fld_output_hw);
	aie_free_va(fd, &fd->fld_output_hw);
	aie_free_dmabuf(fd, &fd->fld_output_hw);
}
#endif
#if CHECK_SERVICE_0
static int aie_copy_fw(struct mtk_aie_dev *fd, const char *name, void *buf,
		       unsigned int size)
{
	int ret = 0;
	const struct firmware *fw = NULL;

	ret = request_firmware(&fw, name, fd->dev);
	if (ret) {
		dev_info(fd->dev, "%s: fail to load firmware %s\n", __func__,
			 name);
		return ret;
	}

	if (size < fw->size) {
		release_firmware(fw);
		return -EINVAL;
	}

	memcpy(buf, fw->data, fw->size);
	release_firmware(fw);

	return ret;
}
#endif
static int aie_load_fw(struct mtk_aie_dev *fd)
{
	int ret = 0;
	int i = 0;

	memcpy(fd->base_para->fd_fd_cfg_va, &fdvt_fd_confi_frame01[0], fd->fd_fd_cfg_size);
	memcpy(fd->base_para->fd_rs_cfg_va, &fdvt_rs_confi_frame01[0], fd->fd_rs_cfg_size);
	memcpy(fd->base_para->fd_yuv2rgb_cfg_va, &fdvt_yuv2rgb_confi_frame01[0],
			fd->fd_yuv2rgb_cfg_size);


	memcpy(fd->base_para->attr_fd_cfg_va[0], &attr_fd_confi_frame01[0], fd->attr_fd_cfg_size);
	memcpy(fd->base_para->attr_yuv2rgb_cfg_va[0], &attr_yuv2rgb_confi_frame01[0],
			fd->attr_yuv2rgb_cfg_size);

	for (i = 1; i < MAX_ENQUE_FRAME_NUM; i++) {
		memcpy(fd->base_para->attr_fd_cfg_va[i],
		       fd->base_para->attr_fd_cfg_va[0], fd->attr_fd_cfg_size);
		memcpy(fd->base_para->attr_yuv2rgb_cfg_va[i],
		       fd->base_para->attr_yuv2rgb_cfg_va[0],
		       fd->attr_yuv2rgb_cfg_size);
	}

	/*0~10*/
	memcpy(fd->dma_para->fd_kernel_va[0][0], &fdvt_kernel_bias_loop00_0_frame01[0],
						fd_ker_rdma_size[0][0]);
	memcpy(fd->dma_para->fd_kernel_va[0][1], &fdvt_kernel_bias_loop00_1_frame01[0],
						fd_ker_rdma_size[0][1]);

	memcpy(fd->dma_para->fd_kernel_va[1][0], &fdvt_kernel_bias_loop01_0_frame01[0],
						fd_ker_rdma_size[1][0]);
	memcpy(fd->dma_para->fd_kernel_va[1][1], &fdvt_kernel_bias_loop01_1_frame01[0],
						fd_ker_rdma_size[1][1]);

	memcpy(fd->dma_para->fd_kernel_va[2][0], &fdvt_kernel_bias_loop02_0_frame01[0],
						fd_ker_rdma_size[2][0]);
	memcpy(fd->dma_para->fd_kernel_va[2][1], &fdvt_kernel_bias_loop02_1_frame01[0],
						fd_ker_rdma_size[2][1]);

	memcpy(fd->dma_para->fd_kernel_va[3][0], &fdvt_kernel_bias_loop03_0_frame01[0],
						fd_ker_rdma_size[3][0]);
	memcpy(fd->dma_para->fd_kernel_va[3][1], &fdvt_kernel_bias_loop03_1_frame01[0],
						fd_ker_rdma_size[3][1]);

	memcpy(fd->dma_para->fd_kernel_va[4][0], &fdvt_kernel_bias_loop04_0_frame01[0],
						fd_ker_rdma_size[4][0]);
	memcpy(fd->dma_para->fd_kernel_va[4][1], &fdvt_kernel_bias_loop04_1_frame01[0],
						fd_ker_rdma_size[4][1]);

	memcpy(fd->dma_para->fd_kernel_va[5][0], &fdvt_kernel_bias_loop05_0_frame01[0],
						fd_ker_rdma_size[5][0]);
	memcpy(fd->dma_para->fd_kernel_va[5][1], &fdvt_kernel_bias_loop05_1_frame01[0],
						fd_ker_rdma_size[5][1]);

	memcpy(fd->dma_para->fd_kernel_va[6][0], &fdvt_kernel_bias_loop06_0_frame01[0],
						fd_ker_rdma_size[6][0]);
	memcpy(fd->dma_para->fd_kernel_va[6][1], &fdvt_kernel_bias_loop06_1_frame01[0],
						fd_ker_rdma_size[6][1]);

	memcpy(fd->dma_para->fd_kernel_va[7][0], &fdvt_kernel_bias_loop07_0_frame01[0],
						fd_ker_rdma_size[7][0]);
	memcpy(fd->dma_para->fd_kernel_va[7][1], &fdvt_kernel_bias_loop07_1_frame01[0],
						fd_ker_rdma_size[7][1]);

	memcpy(fd->dma_para->fd_kernel_va[8][0], &fdvt_kernel_bias_loop08_0_frame01[0],
						fd_ker_rdma_size[8][0]);
	memcpy(fd->dma_para->fd_kernel_va[8][1], &fdvt_kernel_bias_loop08_1_frame01[0],
						fd_ker_rdma_size[8][1]);

	memcpy(fd->dma_para->fd_kernel_va[9][0], &fdvt_kernel_bias_loop09_0_frame01[0],
						fd_ker_rdma_size[9][0]);
	memcpy(fd->dma_para->fd_kernel_va[9][1], &fdvt_kernel_bias_loop09_1_frame01[0],
						fd_ker_rdma_size[9][1]);

	memcpy(fd->dma_para->fd_kernel_va[10][0], &fdvt_kernel_bias_loop10_0_frame01[0],
						fd_ker_rdma_size[10][0]);
	memcpy(fd->dma_para->fd_kernel_va[10][1], &fdvt_kernel_bias_loop10_1_frame01[0],
						fd_ker_rdma_size[10][1]);

	/*11~20*/
	memcpy(fd->dma_para->fd_kernel_va[11][0], &fdvt_kernel_bias_loop11_0_frame01[0],
						fd_ker_rdma_size[11][0]);
	memcpy(fd->dma_para->fd_kernel_va[11][1], &fdvt_kernel_bias_loop11_1_frame01[0],
						fd_ker_rdma_size[11][1]);

	memcpy(fd->dma_para->fd_kernel_va[12][0], &fdvt_kernel_bias_loop12_0_frame01[0],
						fd_ker_rdma_size[12][0]);
	memcpy(fd->dma_para->fd_kernel_va[12][1], &fdvt_kernel_bias_loop12_1_frame01[0],
						fd_ker_rdma_size[12][1]);

	memcpy(fd->dma_para->fd_kernel_va[13][0], &fdvt_kernel_bias_loop13_0_frame01[0],
						fd_ker_rdma_size[13][0]);
	memcpy(fd->dma_para->fd_kernel_va[13][1], &fdvt_kernel_bias_loop13_1_frame01[0],
						fd_ker_rdma_size[13][1]);

	memcpy(fd->dma_para->fd_kernel_va[14][0], &fdvt_kernel_bias_loop14_0_frame01[0],
						fd_ker_rdma_size[14][0]);
	memcpy(fd->dma_para->fd_kernel_va[14][1], &fdvt_kernel_bias_loop14_1_frame01[0],
						fd_ker_rdma_size[14][1]);

	memcpy(fd->dma_para->fd_kernel_va[15][0], &fdvt_kernel_bias_loop15_0_frame01[0],
						fd_ker_rdma_size[15][0]);
	memcpy(fd->dma_para->fd_kernel_va[15][1], &fdvt_kernel_bias_loop15_1_frame01[0],
						fd_ker_rdma_size[15][1]);

	memcpy(fd->dma_para->fd_kernel_va[16][0], &fdvt_kernel_bias_loop16_0_frame01[0],
						fd_ker_rdma_size[16][0]);
	memcpy(fd->dma_para->fd_kernel_va[16][1], &fdvt_kernel_bias_loop16_1_frame01[0],
						fd_ker_rdma_size[16][1]);

	memcpy(fd->dma_para->fd_kernel_va[17][0], &fdvt_kernel_bias_loop17_0_frame01[0],
						fd_ker_rdma_size[17][0]);
	memcpy(fd->dma_para->fd_kernel_va[17][1], &fdvt_kernel_bias_loop17_1_frame01,
						fd_ker_rdma_size[17][1]);

	memcpy(fd->dma_para->fd_kernel_va[18][0], &fdvt_kernel_bias_loop18_0_frame01[0],
						fd_ker_rdma_size[18][0]);
	memcpy(fd->dma_para->fd_kernel_va[18][1], &fdvt_kernel_bias_loop18_1_frame01[0],
						fd_ker_rdma_size[18][1]);

	memcpy(fd->dma_para->fd_kernel_va[19][0], &fdvt_kernel_bias_loop19_0_frame01[0],
						fd_ker_rdma_size[19][0]);
	memcpy(fd->dma_para->fd_kernel_va[19][1], &fdvt_kernel_bias_loop19_1_frame01[0],
						fd_ker_rdma_size[19][1]);

	memcpy(fd->dma_para->fd_kernel_va[20][0], &fdvt_kernel_bias_loop20_0_frame01[0],
						fd_ker_rdma_size[20][0]);
	memcpy(fd->dma_para->fd_kernel_va[20][1], &fdvt_kernel_bias_loop20_1_frame01[0],
						fd_ker_rdma_size[20][1]);

	/*21~30: except 28*/
	memcpy(fd->dma_para->fd_kernel_va[21][0], &fdvt_kernel_bias_loop21_0_frame01[0],
						fd_ker_rdma_size[21][0]);
	memcpy(fd->dma_para->fd_kernel_va[21][1], &fdvt_kernel_bias_loop21_1_frame01[0],
						fd_ker_rdma_size[21][1]);

	memcpy(fd->dma_para->fd_kernel_va[22][0], &fdvt_kernel_bias_loop22_0_frame01[0],
						fd_ker_rdma_size[22][0]);
	memcpy(fd->dma_para->fd_kernel_va[22][1], &fdvt_kernel_bias_loop22_1_frame01[0],
						fd_ker_rdma_size[22][1]);

	memcpy(fd->dma_para->fd_kernel_va[23][0], &fdvt_kernel_bias_loop23_0_frame01[0],
						fd_ker_rdma_size[23][0]);
	memcpy(fd->dma_para->fd_kernel_va[23][1], &fdvt_kernel_bias_loop23_1_frame01[0],
						fd_ker_rdma_size[23][1]);

	memcpy(fd->dma_para->fd_kernel_va[24][0], &fdvt_kernel_bias_loop24_0_frame01[0],
						fd_ker_rdma_size[24][0]);
	memcpy(fd->dma_para->fd_kernel_va[24][1], &fdvt_kernel_bias_loop24_1_frame01[0],
						fd_ker_rdma_size[24][1]);

	memcpy(fd->dma_para->fd_kernel_va[25][0], &fdvt_kernel_bias_loop25_0_frame01[0],
						fd_ker_rdma_size[25][0]);
	memcpy(fd->dma_para->fd_kernel_va[25][1], &fdvt_kernel_bias_loop25_1_frame01[0],
						fd_ker_rdma_size[25][1]);

	memcpy(fd->dma_para->fd_kernel_va[26][0], &fdvt_kernel_bias_loop26_0_frame01[0],
						fd_ker_rdma_size[26][0]);
	memcpy(fd->dma_para->fd_kernel_va[26][1], &fdvt_kernel_bias_loop26_1_frame01[0],
						fd_ker_rdma_size[26][1]);

	memcpy(fd->dma_para->fd_kernel_va[27][0], &fdvt_kernel_bias_loop27_0_frame01[0],
						fd_ker_rdma_size[27][0]);
	memcpy(fd->dma_para->fd_kernel_va[27][1], &fdvt_kernel_bias_loop27_1_frame01[0],
						fd_ker_rdma_size[27][1]);

	memcpy(fd->dma_para->fd_kernel_va[29][0], &fdvt_kernel_bias_loop29_0_frame01[0],
						fd_ker_rdma_size[29][0]);
	memcpy(fd->dma_para->fd_kernel_va[29][1], &fdvt_kernel_bias_loop29_1_frame01[0],
						fd_ker_rdma_size[29][1]);

	memcpy(fd->dma_para->fd_kernel_va[30][0], &fdvt_kernel_bias_loop30_0_frame01[0],
						fd_ker_rdma_size[30][0]);
	memcpy(fd->dma_para->fd_kernel_va[30][1], &fdvt_kernel_bias_loop30_1_frame01[0],
						fd_ker_rdma_size[30][1]);

	/*31~40*/
	memcpy(fd->dma_para->fd_kernel_va[31][0], &fdvt_kernel_bias_loop31_0_frame01[0],
						fd_ker_rdma_size[31][0]);
	memcpy(fd->dma_para->fd_kernel_va[31][1], &fdvt_kernel_bias_loop31_1_frame01[0],
						fd_ker_rdma_size[31][1]);

	memcpy(fd->dma_para->fd_kernel_va[32][0], &fdvt_kernel_bias_loop32_0_frame01[0],
						fd_ker_rdma_size[32][0]);
	memcpy(fd->dma_para->fd_kernel_va[32][1], &fdvt_kernel_bias_loop32_1_frame01[0],
						fd_ker_rdma_size[32][1]);

	memcpy(fd->dma_para->fd_kernel_va[33][0], &fdvt_kernel_bias_loop33_0_frame01[0],
						fd_ker_rdma_size[33][0]);
	memcpy(fd->dma_para->fd_kernel_va[33][1], &fdvt_kernel_bias_loop33_1_frame01[0],
						fd_ker_rdma_size[33][1]);

	memcpy(fd->dma_para->fd_kernel_va[34][0], &fdvt_kernel_bias_loop34_0_frame01[0],
						fd_ker_rdma_size[34][0]);
	memcpy(fd->dma_para->fd_kernel_va[34][1], &fdvt_kernel_bias_loop34_1_frame01[0],
						fd_ker_rdma_size[34][1]);

	memcpy(fd->dma_para->fd_kernel_va[35][0], &fdvt_kernel_bias_loop35_0_frame01[0],
						fd_ker_rdma_size[35][0]);
	memcpy(fd->dma_para->fd_kernel_va[35][1], &fdvt_kernel_bias_loop35_1_frame01[0],
						fd_ker_rdma_size[35][1]);

	memcpy(fd->dma_para->fd_kernel_va[36][0], &fdvt_kernel_bias_loop36_0_frame01[0],
						fd_ker_rdma_size[36][0]);
	memcpy(fd->dma_para->fd_kernel_va[36][1], &fdvt_kernel_bias_loop36_1_frame01[0],
						fd_ker_rdma_size[36][1]);

	memcpy(fd->dma_para->fd_kernel_va[37][0], &fdvt_kernel_bias_loop37_0_frame01[0],
						fd_ker_rdma_size[37][0]);
	memcpy(fd->dma_para->fd_kernel_va[37][1], &fdvt_kernel_bias_loop37_1_frame01[0],
						fd_ker_rdma_size[37][1]);

	memcpy(fd->dma_para->fd_kernel_va[38][0], &fdvt_kernel_bias_loop38_0_frame01[0],
						fd_ker_rdma_size[38][0]);
	memcpy(fd->dma_para->fd_kernel_va[38][1], &fdvt_kernel_bias_loop38_1_frame01[0],
						fd_ker_rdma_size[38][1]);

	memcpy(fd->dma_para->fd_kernel_va[39][0], &fdvt_kernel_bias_loop39_0_frame01[0],
						fd_ker_rdma_size[39][0]);
	memcpy(fd->dma_para->fd_kernel_va[39][1], &fdvt_kernel_bias_loop39_1_frame01[0],
						fd_ker_rdma_size[39][1]);

	memcpy(fd->dma_para->fd_kernel_va[40][0], &fdvt_kernel_bias_loop40_0_frame01[0],
						fd_ker_rdma_size[40][0]);
	memcpy(fd->dma_para->fd_kernel_va[40][1], &fdvt_kernel_bias_loop40_1_frame01[0],
						fd_ker_rdma_size[40][1]);

	/*41~50*/
	memcpy(fd->dma_para->fd_kernel_va[41][0], &fdvt_kernel_bias_loop41_0_frame01[0],
						fd_ker_rdma_size[41][0]);
	memcpy(fd->dma_para->fd_kernel_va[41][1], &fdvt_kernel_bias_loop41_1_frame01[0],
						fd_ker_rdma_size[41][1]);

	memcpy(fd->dma_para->fd_kernel_va[42][0], &fdvt_kernel_bias_loop42_0_frame01[0],
						fd_ker_rdma_size[42][0]);
	memcpy(fd->dma_para->fd_kernel_va[42][1], &fdvt_kernel_bias_loop42_1_frame01[0],
						fd_ker_rdma_size[42][1]);

	memcpy(fd->dma_para->fd_kernel_va[43][0], &fdvt_kernel_bias_loop43_0_frame01[0],
						fd_ker_rdma_size[43][0]);
	memcpy(fd->dma_para->fd_kernel_va[43][1], &fdvt_kernel_bias_loop43_1_frame01[0],
						fd_ker_rdma_size[43][1]);

	memcpy(fd->dma_para->fd_kernel_va[44][0], &fdvt_kernel_bias_loop44_0_frame01[0],
						fd_ker_rdma_size[44][0]);
	memcpy(fd->dma_para->fd_kernel_va[44][1], &fdvt_kernel_bias_loop44_1_frame01[0],
						fd_ker_rdma_size[44][1]);

	memcpy(fd->dma_para->fd_kernel_va[45][0], &fdvt_kernel_bias_loop45_0_frame01[0],
						fd_ker_rdma_size[45][0]);
	memcpy(fd->dma_para->fd_kernel_va[45][1], &fdvt_kernel_bias_loop45_1_frame01[0],
						fd_ker_rdma_size[45][1]);

	memcpy(fd->dma_para->fd_kernel_va[46][0], &fdvt_kernel_bias_loop46_0_frame01[0],
						fd_ker_rdma_size[46][0]);
	memcpy(fd->dma_para->fd_kernel_va[46][1], &fdvt_kernel_bias_loop46_1_frame01[0],
						fd_ker_rdma_size[46][1]);

	memcpy(fd->dma_para->fd_kernel_va[47][0], &fdvt_kernel_bias_loop47_0_frame01[0],
						fd_ker_rdma_size[47][0]);
	memcpy(fd->dma_para->fd_kernel_va[47][1], &fdvt_kernel_bias_loop47_1_frame01[0],
						fd_ker_rdma_size[47][1]);

	memcpy(fd->dma_para->fd_kernel_va[48][0], &fdvt_kernel_bias_loop48_0_frame01[0],
						fd_ker_rdma_size[48][0]);
	memcpy(fd->dma_para->fd_kernel_va[48][1], &fdvt_kernel_bias_loop48_1_frame01[0],
						fd_ker_rdma_size[48][1]);

	memcpy(fd->dma_para->fd_kernel_va[49][0], &fdvt_kernel_bias_loop49_0_frame01[0],
						fd_ker_rdma_size[49][0]);
	memcpy(fd->dma_para->fd_kernel_va[49][1], &fdvt_kernel_bias_loop49_1_frame01[0],
						fd_ker_rdma_size[49][1]);

	memcpy(fd->dma_para->fd_kernel_va[50][0], &fdvt_kernel_bias_loop50_0_frame01[0],
						fd_ker_rdma_size[50][0]);
	memcpy(fd->dma_para->fd_kernel_va[50][1], &fdvt_kernel_bias_loop50_1_frame01[0],
						fd_ker_rdma_size[50][1]);

	/*51~60: except 57*/
	memcpy(fd->dma_para->fd_kernel_va[51][0], &fdvt_kernel_bias_loop51_0_frame01[0],
						fd_ker_rdma_size[51][0]);
	memcpy(fd->dma_para->fd_kernel_va[51][1], &fdvt_kernel_bias_loop51_1_frame01[0],
						fd_ker_rdma_size[51][1]);

	memcpy(fd->dma_para->fd_kernel_va[52][0], &fdvt_kernel_bias_loop52_0_frame01[0],
						fd_ker_rdma_size[52][0]);
	memcpy(fd->dma_para->fd_kernel_va[52][1], &fdvt_kernel_bias_loop52_1_frame01[0],
						fd_ker_rdma_size[52][1]);

	memcpy(fd->dma_para->fd_kernel_va[53][0], &fdvt_kernel_bias_loop53_0_frame01[0],
						fd_ker_rdma_size[53][0]);
	memcpy(fd->dma_para->fd_kernel_va[53][1], &fdvt_kernel_bias_loop53_1_frame01[0],
						fd_ker_rdma_size[53][1]);

	memcpy(fd->dma_para->fd_kernel_va[54][0], &fdvt_kernel_bias_loop54_0_frame01[0],
						fd_ker_rdma_size[54][0]);
	memcpy(fd->dma_para->fd_kernel_va[54][1], &fdvt_kernel_bias_loop54_1_frame01[0],
						fd_ker_rdma_size[54][1]);

	memcpy(fd->dma_para->fd_kernel_va[55][0], &fdvt_kernel_bias_loop55_0_frame01[0],
						fd_ker_rdma_size[55][0]);
	memcpy(fd->dma_para->fd_kernel_va[55][1], &fdvt_kernel_bias_loop55_1_frame01[0],
						fd_ker_rdma_size[55][1]);

	memcpy(fd->dma_para->fd_kernel_va[56][0], &fdvt_kernel_bias_loop56_0_frame01[0],
						fd_ker_rdma_size[56][0]);
	memcpy(fd->dma_para->fd_kernel_va[56][1], &fdvt_kernel_bias_loop56_1_frame01[0],
						fd_ker_rdma_size[56][1]);

	memcpy(fd->dma_para->fd_kernel_va[58][0], &fdvt_kernel_bias_loop58_0_frame01[0],
						fd_ker_rdma_size[58][0]);
	memcpy(fd->dma_para->fd_kernel_va[58][1], &fdvt_kernel_bias_loop58_1_frame01[0],
						fd_ker_rdma_size[58][1]);

	memcpy(fd->dma_para->fd_kernel_va[59][0], &fdvt_kernel_bias_loop59_0_frame01[0],
						fd_ker_rdma_size[59][0]);
	memcpy(fd->dma_para->fd_kernel_va[59][1], &fdvt_kernel_bias_loop59_1_frame01[0],
						fd_ker_rdma_size[59][1]);

	memcpy(fd->dma_para->fd_kernel_va[60][0], &fdvt_kernel_bias_loop60_0_frame01[0],
						fd_ker_rdma_size[60][0]);
	memcpy(fd->dma_para->fd_kernel_va[60][1], &fdvt_kernel_bias_loop60_1_frame01[0],
						fd_ker_rdma_size[60][1]);

	/*61~70*/
	memcpy(fd->dma_para->fd_kernel_va[61][0], &fdvt_kernel_bias_loop61_0_frame01[0],
						fd_ker_rdma_size[61][0]);
	memcpy(fd->dma_para->fd_kernel_va[61][1], &fdvt_kernel_bias_loop61_1_frame01[0],
						fd_ker_rdma_size[61][1]);

	memcpy(fd->dma_para->fd_kernel_va[62][0], &fdvt_kernel_bias_loop62_0_frame01[0],
						fd_ker_rdma_size[62][0]);
	memcpy(fd->dma_para->fd_kernel_va[62][1], &fdvt_kernel_bias_loop62_1_frame01[0],
						fd_ker_rdma_size[62][1]);

	memcpy(fd->dma_para->fd_kernel_va[63][0], &fdvt_kernel_bias_loop63_0_frame01[0],
						fd_ker_rdma_size[63][0]);
	memcpy(fd->dma_para->fd_kernel_va[63][1], &fdvt_kernel_bias_loop63_1_frame01[0],
						fd_ker_rdma_size[63][1]);

	memcpy(fd->dma_para->fd_kernel_va[64][0], &fdvt_kernel_bias_loop64_0_frame01[0],
						fd_ker_rdma_size[64][0]);
	memcpy(fd->dma_para->fd_kernel_va[64][1], &fdvt_kernel_bias_loop64_1_frame01[0],
						fd_ker_rdma_size[64][1]);

	memcpy(fd->dma_para->fd_kernel_va[65][0], &fdvt_kernel_bias_loop65_0_frame01[0],
						fd_ker_rdma_size[65][0]);
	memcpy(fd->dma_para->fd_kernel_va[65][1], &fdvt_kernel_bias_loop65_1_frame01[0],
						fd_ker_rdma_size[65][1]);

	memcpy(fd->dma_para->fd_kernel_va[66][0], &fdvt_kernel_bias_loop66_0_frame01[0],
						fd_ker_rdma_size[66][0]);
	memcpy(fd->dma_para->fd_kernel_va[66][1], &fdvt_kernel_bias_loop66_1_frame01[0],
						fd_ker_rdma_size[66][1]);

	memcpy(fd->dma_para->fd_kernel_va[67][0], &fdvt_kernel_bias_loop67_0_frame01[0],
						fd_ker_rdma_size[67][0]);
	memcpy(fd->dma_para->fd_kernel_va[67][1], &fdvt_kernel_bias_loop67_1_frame01[0],
						fd_ker_rdma_size[67][1]);

	memcpy(fd->dma_para->fd_kernel_va[68][0], &fdvt_kernel_bias_loop68_0_frame01[0],
						fd_ker_rdma_size[68][0]);
	memcpy(fd->dma_para->fd_kernel_va[68][1], &fdvt_kernel_bias_loop68_1_frame01[0],
						fd_ker_rdma_size[68][1]);

	memcpy(fd->dma_para->fd_kernel_va[69][0], &fdvt_kernel_bias_loop69_0_frame01[0],
						fd_ker_rdma_size[69][0]);
	memcpy(fd->dma_para->fd_kernel_va[69][1], &fdvt_kernel_bias_loop69_1_frame01[0],
						fd_ker_rdma_size[69][1]);

	memcpy(fd->dma_para->fd_kernel_va[70][0], &fdvt_kernel_bias_loop70_0_frame01[0],
						fd_ker_rdma_size[70][0]);
	memcpy(fd->dma_para->fd_kernel_va[70][1], &fdvt_kernel_bias_loop70_1_frame01[0],
						fd_ker_rdma_size[70][1]);

	/*71~80*/
	memcpy(fd->dma_para->fd_kernel_va[71][0], &fdvt_kernel_bias_loop71_0_frame01[0],
						fd_ker_rdma_size[71][0]);
	memcpy(fd->dma_para->fd_kernel_va[71][1], &fdvt_kernel_bias_loop71_1_frame01[0],
						fd_ker_rdma_size[71][1]);

	memcpy(fd->dma_para->fd_kernel_va[72][0], &fdvt_kernel_bias_loop72_0_frame01[0],
						fd_ker_rdma_size[72][0]);
	memcpy(fd->dma_para->fd_kernel_va[72][1], &fdvt_kernel_bias_loop72_1_frame01[0],
						fd_ker_rdma_size[72][1]);

	memcpy(fd->dma_para->fd_kernel_va[73][0], &fdvt_kernel_bias_loop73_0_frame01[0],
						fd_ker_rdma_size[73][0]);
	memcpy(fd->dma_para->fd_kernel_va[73][1], &fdvt_kernel_bias_loop73_1_frame01[0],
						fd_ker_rdma_size[73][1]);

	memcpy(fd->dma_para->fd_kernel_va[74][0], &fdvt_kernel_bias_loop74_0_frame01[0],
						fd_ker_rdma_size[74][0]);
	memcpy(fd->dma_para->fd_kernel_va[74][1], &fdvt_kernel_bias_loop74_1_frame01[0],
						fd_ker_rdma_size[74][1]);

	memcpy(fd->dma_para->fd_kernel_va[75][0], &fdvt_kernel_bias_loop75_0_frame01[0],
						fd_ker_rdma_size[75][0]);
	memcpy(fd->dma_para->fd_kernel_va[75][1], &fdvt_kernel_bias_loop75_1_frame01[0],
						fd_ker_rdma_size[75][1]);

	memcpy(fd->dma_para->fd_kernel_va[76][0], &fdvt_kernel_bias_loop76_0_frame01[0],
						fd_ker_rdma_size[76][0]);
	memcpy(fd->dma_para->fd_kernel_va[76][1], &fdvt_kernel_bias_loop76_1_frame01[0],
						fd_ker_rdma_size[76][1]);

	memcpy(fd->dma_para->fd_kernel_va[77][0], &fdvt_kernel_bias_loop77_0_frame01[0],
						fd_ker_rdma_size[77][0]);
	memcpy(fd->dma_para->fd_kernel_va[77][1], &fdvt_kernel_bias_loop77_1_frame01[0],
						fd_ker_rdma_size[77][1]);

	memcpy(fd->dma_para->fd_kernel_va[78][0], &fdvt_kernel_bias_loop78_0_frame01[0],
						fd_ker_rdma_size[78][0]);
	memcpy(fd->dma_para->fd_kernel_va[78][1], &fdvt_kernel_bias_loop78_1_frame01[0],
						fd_ker_rdma_size[78][1]);

	memcpy(fd->dma_para->fd_kernel_va[79][0], &fdvt_kernel_bias_loop79_0_frame01[0],
						fd_ker_rdma_size[79][0]);
	memcpy(fd->dma_para->fd_kernel_va[79][1], &fdvt_kernel_bias_loop79_1_frame01[0],
						fd_ker_rdma_size[79][1]);

	memcpy(fd->dma_para->fd_kernel_va[80][0], &fdvt_kernel_bias_loop80_0_frame01[0],
						fd_ker_rdma_size[80][0]);
	memcpy(fd->dma_para->fd_kernel_va[80][1], &fdvt_kernel_bias_loop80_1_frame01[0],
						fd_ker_rdma_size[80][1]);

	/*81~85*/
	memcpy(fd->dma_para->fd_kernel_va[81][0], &fdvt_kernel_bias_loop81_0_frame01[0],
						fd_ker_rdma_size[81][0]);
	memcpy(fd->dma_para->fd_kernel_va[81][1], &fdvt_kernel_bias_loop81_1_frame01[0],
						fd_ker_rdma_size[81][1]);

	memcpy(fd->dma_para->fd_kernel_va[82][0], &fdvt_kernel_bias_loop82_0_frame01[0],
						fd_ker_rdma_size[82][0]);
	memcpy(fd->dma_para->fd_kernel_va[82][1], &fdvt_kernel_bias_loop82_1_frame01[0],
						fd_ker_rdma_size[82][1]);

	memcpy(fd->dma_para->fd_kernel_va[83][0], &fdvt_kernel_bias_loop83_0_frame01[0],
						fd_ker_rdma_size[83][0]);
	memcpy(fd->dma_para->fd_kernel_va[83][1], &fdvt_kernel_bias_loop83_1_frame01[0],
						fd_ker_rdma_size[83][1]);

	memcpy(fd->dma_para->fd_kernel_va[84][0], &fdvt_kernel_bias_loop84_0_frame01[0],
						fd_ker_rdma_size[84][0]);
	memcpy(fd->dma_para->fd_kernel_va[84][1], &fdvt_kernel_bias_loop84_1_frame01[0],
						fd_ker_rdma_size[84][1]);

	memcpy(fd->dma_para->fd_kernel_va[85][0], &fdvt_kernel_bias_loop85_0_frame01[0],
						fd_ker_rdma_size[85][0]);
	memcpy(fd->dma_para->fd_kernel_va[85][1], &fdvt_kernel_bias_loop85_1_frame01[0],
						fd_ker_rdma_size[85][1]);

	memcpy(fd->dma_para->attr_kernel_va[0][0], &gender_kernel_bias_loop00_0_frame01[0],
						 attr_ker_rdma_size[0][0]);
	memcpy(fd->dma_para->attr_kernel_va[0][1], &gender_kernel_bias_loop00_1_frame01[0],
						attr_ker_rdma_size[0][1]);

	memcpy(fd->dma_para->attr_kernel_va[1][0], &gender_kernel_bias_loop01_0_frame01[0],
						attr_ker_rdma_size[1][0]);
	memcpy(fd->dma_para->attr_kernel_va[1][1], &gender_kernel_bias_loop01_1_frame01[0],
						attr_ker_rdma_size[1][1]);

	memcpy(fd->dma_para->attr_kernel_va[2][0], &gender_kernel_bias_loop02_0_frame01[0],
						attr_ker_rdma_size[2][0]);
	memcpy(fd->dma_para->attr_kernel_va[2][1], &gender_kernel_bias_loop02_1_frame01[0],
						attr_ker_rdma_size[2][1]);

	memcpy(fd->dma_para->attr_kernel_va[3][0], &gender_kernel_bias_loop03_0_frame01[0],
						attr_ker_rdma_size[3][0]);
	memcpy(fd->dma_para->attr_kernel_va[3][1], &gender_kernel_bias_loop03_1_frame01[0],
						attr_ker_rdma_size[3][1]);

	memcpy(fd->dma_para->attr_kernel_va[4][0], &gender_kernel_bias_loop04_0_frame01[0],
						attr_ker_rdma_size[4][0]);
	memcpy(fd->dma_para->attr_kernel_va[4][1], &gender_kernel_bias_loop04_1_frame01[0],
						attr_ker_rdma_size[4][1]);

	memcpy(fd->dma_para->attr_kernel_va[5][0], &gender_kernel_bias_loop05_0_frame01[0],
						attr_ker_rdma_size[5][0]);
	memcpy(fd->dma_para->attr_kernel_va[5][1], &gender_kernel_bias_loop05_1_frame01[0],
						attr_ker_rdma_size[5][1]);

	memcpy(fd->dma_para->attr_kernel_va[6][0], &gender_kernel_bias_loop06_0_frame01[0],
						attr_ker_rdma_size[6][0]);
	memcpy(fd->dma_para->attr_kernel_va[6][1], &gender_kernel_bias_loop06_1_frame01[0],
						attr_ker_rdma_size[6][1]);

	memcpy(fd->dma_para->attr_kernel_va[7][0], &gender_kernel_bias_loop07_0_frame01[0],
						attr_ker_rdma_size[7][0]);
	memcpy(fd->dma_para->attr_kernel_va[7][1], &gender_kernel_bias_loop07_1_frame01[0],
						attr_ker_rdma_size[7][1]);

	memcpy(fd->dma_para->attr_kernel_va[8][0], &gender_kernel_bias_loop08_0_frame01[0],
						attr_ker_rdma_size[8][0]);
	memcpy(fd->dma_para->attr_kernel_va[8][1], &gender_kernel_bias_loop08_1_frame01[0],
						attr_ker_rdma_size[8][1]);

	memcpy(fd->dma_para->attr_kernel_va[9][0], &gender_kernel_bias_loop09_0_frame01[0],
						attr_ker_rdma_size[9][0]);
	memcpy(fd->dma_para->attr_kernel_va[9][1], &gender_kernel_bias_loop09_1_frame01[0],
						attr_ker_rdma_size[9][1]);

	memcpy(fd->dma_para->attr_kernel_va[10][0], &gender_kernel_bias_loop10_0_frame01[0],
						attr_ker_rdma_size[10][0]);
	memcpy(fd->dma_para->attr_kernel_va[10][1], &gender_kernel_bias_loop10_1_frame01[0],
						attr_ker_rdma_size[10][1]);

	/*11~20*/
	memcpy(fd->dma_para->attr_kernel_va[11][0], &gender_kernel_bias_loop11_0_frame01[0],
						attr_ker_rdma_size[11][0]);
	memcpy(fd->dma_para->attr_kernel_va[11][1], &gender_kernel_bias_loop11_1_frame01[0],
						attr_ker_rdma_size[11][1]);

	memcpy(fd->dma_para->attr_kernel_va[12][0], &gender_kernel_bias_loop12_0_frame01[0],
						attr_ker_rdma_size[12][0]);
	memcpy(fd->dma_para->attr_kernel_va[12][1], &gender_kernel_bias_loop12_1_frame01[0],
						attr_ker_rdma_size[12][1]);

	memcpy(fd->dma_para->attr_kernel_va[13][0], &gender_kernel_bias_loop13_0_frame01[0],
						attr_ker_rdma_size[13][0]);
	memcpy(fd->dma_para->attr_kernel_va[13][1], &gender_kernel_bias_loop13_1_frame01[0],
						attr_ker_rdma_size[13][1]);

	memcpy(fd->dma_para->attr_kernel_va[14][0], &gender_kernel_bias_loop14_0_frame01[0],
						attr_ker_rdma_size[14][0]);
	memcpy(fd->dma_para->attr_kernel_va[14][1], &gender_kernel_bias_loop14_1_frame01[0],
						attr_ker_rdma_size[14][1]);

	memcpy(fd->dma_para->attr_kernel_va[15][0], &gender_kernel_bias_loop15_0_frame01[0],
						attr_ker_rdma_size[15][0]);
	memcpy(fd->dma_para->attr_kernel_va[15][1], &gender_kernel_bias_loop15_1_frame01[0],
						attr_ker_rdma_size[15][1]);

	memcpy(fd->dma_para->attr_kernel_va[16][0], &gender_kernel_bias_loop16_0_frame01[0],
						attr_ker_rdma_size[16][0]);
	memcpy(fd->dma_para->attr_kernel_va[16][1], &gender_kernel_bias_loop16_1_frame01[0],
						attr_ker_rdma_size[16][1]);

	memcpy(fd->dma_para->attr_kernel_va[17][0], &gender_kernel_bias_loop17_0_frame01[0],
						attr_ker_rdma_size[17][0]);
	memcpy(fd->dma_para->attr_kernel_va[17][1], &gender_kernel_bias_loop17_1_frame01[0],
						attr_ker_rdma_size[17][1]);

	memcpy(fd->dma_para->attr_kernel_va[18][0], &gender_kernel_bias_loop18_0_frame01[0],
						attr_ker_rdma_size[18][0]);
	memcpy(fd->dma_para->attr_kernel_va[18][1], &gender_kernel_bias_loop18_1_frame01[0],
						attr_ker_rdma_size[18][1]);

	memcpy(fd->dma_para->attr_kernel_va[19][0], &gender_kernel_bias_loop19_0_frame01[0],
						attr_ker_rdma_size[19][0]);
	memcpy(fd->dma_para->attr_kernel_va[19][1], &gender_kernel_bias_loop19_1_frame01[0],
						attr_ker_rdma_size[19][1]);

	memcpy(fd->dma_para->attr_kernel_va[20][0], &gender_kernel_bias_loop20_0_frame01[0],
						attr_ker_rdma_size[20][0]);
	memcpy(fd->dma_para->attr_kernel_va[20][1], &gender_kernel_bias_loop20_1_frame01[0],
						attr_ker_rdma_size[20][1]);

	/*21~30: except 28*/
	memcpy(fd->dma_para->attr_kernel_va[21][0], &gender_kernel_bias_loop21_0_frame01[0],
						attr_ker_rdma_size[21][0]);
	memcpy(fd->dma_para->attr_kernel_va[21][1], &gender_kernel_bias_loop21_1_frame01[0],
						attr_ker_rdma_size[21][1]);

	memcpy(fd->dma_para->attr_kernel_va[22][0], &gender_kernel_bias_loop22_0_frame01[0],
						attr_ker_rdma_size[22][0]);
	memcpy(fd->dma_para->attr_kernel_va[22][1], &gender_kernel_bias_loop22_1_frame01[0],
						attr_ker_rdma_size[22][1]);

	memcpy(fd->dma_para->attr_kernel_va[23][0], &gender_kernel_bias_loop23_0_frame01[0],
						attr_ker_rdma_size[23][0]);
	memcpy(fd->dma_para->attr_kernel_va[23][1], &gender_kernel_bias_loop23_1_frame01[0],
						attr_ker_rdma_size[23][1]);

	memcpy(fd->dma_para->attr_kernel_va[24][0], &gender_kernel_bias_loop24_0_frame01[0],
						attr_ker_rdma_size[24][0]);
	memcpy(fd->dma_para->attr_kernel_va[24][1], &gender_kernel_bias_loop24_1_frame01[0],
						attr_ker_rdma_size[24][1]);

	memcpy(fd->dma_para->attr_kernel_va[25][0], &gender_kernel_bias_loop25_0_frame01[0],
						attr_ker_rdma_size[25][0]);
	memcpy(fd->dma_para->attr_kernel_va[25][1], &gender_kernel_bias_loop25_1_frame01[0],
						attr_ker_rdma_size[25][1]);


	memcpy(fd->dma_para->fld_blink_weight_va, &fdvt_fld_blink_weight_forest14[0],
						fld_blink_weight_size_non_align);
	memcpy(fd->dma_para->fld_cv_va[0], &fdvt_fld_cv_forest00_iom3, fld_cv_size_00_non_align);
	memcpy(fd->dma_para->fld_cv_va[1], &fdvt_fld_cv_forest01_iom3, fld_cv_size);
	memcpy(fd->dma_para->fld_cv_va[2], &fdvt_fld_cv_forest02_iom3, fld_cv_size);
	memcpy(fd->dma_para->fld_cv_va[3], &fdvt_fld_cv_forest03_iom3, fld_cv_size);
	memcpy(fd->dma_para->fld_cv_va[4], &fdvt_fld_cv_forest04_iom3, fld_cv_size);
	memcpy(fd->dma_para->fld_cv_va[5], &fdvt_fld_cv_forest05_iom3, fld_cv_size);
	memcpy(fd->dma_para->fld_cv_va[6], &fdvt_fld_cv_forest06_iom3, fld_cv_size);
	memcpy(fd->dma_para->fld_cv_va[7], &fdvt_fld_cv_forest07_iom3, fld_cv_size);
	memcpy(fd->dma_para->fld_cv_va[8], &fdvt_fld_cv_forest08_iom3, fld_cv_size);
	memcpy(fd->dma_para->fld_cv_va[9], &fdvt_fld_cv_forest09_iom3, fld_cv_size);
	memcpy(fd->dma_para->fld_cv_va[10], &fdvt_fld_cv_forest10_iom3, fld_cv_size);
	memcpy(fd->dma_para->fld_cv_va[11], &fdvt_fld_cv_forest11_iom3, fld_cv_size);
	memcpy(fd->dma_para->fld_cv_va[12], &fdvt_fld_cv_forest12_iom3, fld_cv_size);
	memcpy(fd->dma_para->fld_cv_va[13], &fdvt_fld_cv_forest13_iom3, fld_cv_size);
	memcpy(fd->dma_para->fld_cv_va[14], &fdvt_fld_cv_forest14_iom3, fld_cv_size);

	memcpy(fd->dma_para->fld_fp_va[0], &fdvt_fld_fp_forest00_om45, fld_fp_size_non_align);
	memcpy(fd->dma_para->fld_fp_va[1], &fdvt_fld_fp_forest01_om45, fld_fp_size_non_align);
	memcpy(fd->dma_para->fld_fp_va[2], &fdvt_fld_fp_forest02_om45, fld_fp_size_non_align);
	memcpy(fd->dma_para->fld_fp_va[3], &fdvt_fld_fp_forest03_om45, fld_fp_size_non_align);
	memcpy(fd->dma_para->fld_fp_va[4], &fdvt_fld_fp_forest04_om45, fld_fp_size_non_align);
	memcpy(fd->dma_para->fld_fp_va[5], &fdvt_fld_fp_forest05_om45, fld_fp_size_non_align);
	memcpy(fd->dma_para->fld_fp_va[6], &fdvt_fld_fp_forest06_om45, fld_fp_size_non_align);
	memcpy(fd->dma_para->fld_fp_va[7], &fdvt_fld_fp_forest07_om45, fld_fp_size_non_align);
	memcpy(fd->dma_para->fld_fp_va[8], &fdvt_fld_fp_forest08_om45, fld_fp_size_non_align);
	memcpy(fd->dma_para->fld_fp_va[9], &fdvt_fld_fp_forest09_om45, fld_fp_size_non_align);
	memcpy(fd->dma_para->fld_fp_va[10], &fdvt_fld_fp_forest10_om45, fld_fp_size_non_align);
	memcpy(fd->dma_para->fld_fp_va[11], &fdvt_fld_fp_forest11_om45, fld_fp_size_non_align);
	memcpy(fd->dma_para->fld_fp_va[12], &fdvt_fld_fp_forest12_om45, fld_fp_size_non_align);
	memcpy(fd->dma_para->fld_fp_va[13], &fdvt_fld_fp_forest13_om45, fld_fp_size_non_align);
	memcpy(fd->dma_para->fld_fp_va[14], &fdvt_fld_fp_forest14_om45, fld_fp_size_non_align);

	memcpy(fd->dma_para->fld_leafnode_va[0], &fdvt_fld_leafnode_forest00, fld_leafnode_size);
	memcpy(fd->dma_para->fld_leafnode_va[1], &fdvt_fld_leafnode_forest01, fld_leafnode_size);
	memcpy(fd->dma_para->fld_leafnode_va[2], &fdvt_fld_leafnode_forest02, fld_leafnode_size);
	memcpy(fd->dma_para->fld_leafnode_va[3], &fdvt_fld_leafnode_forest03, fld_leafnode_size);
	memcpy(fd->dma_para->fld_leafnode_va[4], &fdvt_fld_leafnode_forest04, fld_leafnode_size);
	memcpy(fd->dma_para->fld_leafnode_va[5], &fdvt_fld_leafnode_forest05, fld_leafnode_size);
	memcpy(fd->dma_para->fld_leafnode_va[6], &fdvt_fld_leafnode_forest06, fld_leafnode_size);
	memcpy(fd->dma_para->fld_leafnode_va[7], &fdvt_fld_leafnode_forest07, fld_leafnode_size);
	memcpy(fd->dma_para->fld_leafnode_va[8], &fdvt_fld_leafnode_forest08, fld_leafnode_size);
	memcpy(fd->dma_para->fld_leafnode_va[9], &fdvt_fld_leafnode_forest09, fld_leafnode_size);
	memcpy(fd->dma_para->fld_leafnode_va[10], &fdvt_fld_leafnode_forest10, fld_leafnode_size);
	memcpy(fd->dma_para->fld_leafnode_va[11], &fdvt_fld_leafnode_forest11, fld_leafnode_size);
	memcpy(fd->dma_para->fld_leafnode_va[12], &fdvt_fld_leafnode_forest12, fld_leafnode_size);
	memcpy(fd->dma_para->fld_leafnode_va[13], &fdvt_fld_leafnode_forest13, fld_leafnode_size);
	memcpy(fd->dma_para->fld_leafnode_va[14], &fdvt_fld_leafnode_forest14, fld_leafnode_size);

	memcpy(fd->dma_para->fld_tree13_va[0], &fdvt_fld_tree_forest00_km13,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree13_va[1], &fdvt_fld_tree_forest01_km13,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree13_va[2], &fdvt_fld_tree_forest02_km13,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree13_va[3], &fdvt_fld_tree_forest03_km13,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree13_va[4], &fdvt_fld_tree_forest04_km13,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree13_va[5], &fdvt_fld_tree_forest05_km13,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree13_va[6], &fdvt_fld_tree_forest06_km13,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree13_va[7], &fdvt_fld_tree_forest07_km13,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree13_va[8], &fdvt_fld_tree_forest08_km13,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree13_va[9], &fdvt_fld_tree_forest09_km13,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree13_va[10], &fdvt_fld_tree_forest10_km13,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree13_va[11], &fdvt_fld_tree_forest11_km13,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree13_va[12], &fdvt_fld_tree_forest12_km13,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree13_va[13], &fdvt_fld_tree_forest13_km13,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree13_va[14], &fdvt_fld_tree_forest14_km13,
								fld_tree_size_non_align);

	memcpy(fd->dma_para->fld_tree02_va[0], &fdvt_fld_tree_forest00_km02,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree02_va[1], &fdvt_fld_tree_forest01_km02,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree02_va[2], &fdvt_fld_tree_forest02_km02,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree02_va[3], &fdvt_fld_tree_forest03_km02,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree02_va[4], &fdvt_fld_tree_forest04_km02,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree02_va[5], &fdvt_fld_tree_forest05_km02,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree02_va[6], &fdvt_fld_tree_forest06_km02,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree02_va[7], &fdvt_fld_tree_forest07_km02,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree02_va[8], &fdvt_fld_tree_forest08_km02,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree02_va[9], &fdvt_fld_tree_forest09_km02,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree02_va[10], &fdvt_fld_tree_forest10_km02,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree02_va[11], &fdvt_fld_tree_forest11_km02,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree02_va[12], &fdvt_fld_tree_forest12_km02,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree02_va[13], &fdvt_fld_tree_forest13_km02,
								fld_tree_size_non_align);
	memcpy(fd->dma_para->fld_tree02_va[14], &fdvt_fld_tree_forest14_km02,
								fld_tree_size_non_align);

#if CHECK_SERVICE_0
	u8 i, j;
	int ret;
	char name[128];
	char *sel_folder;
	char *mp_folder = "aie_mp_fw";

	sel_folder = mp_folder;

	ret = sprintf(name, "%s/config/aie_fd_fd_config.bin", sel_folder);
	if (ret < 0)
		return ret;

	ret = aie_copy_fw(fd, name, fd->base_para->fd_fd_cfg_va,
			  fd->fd_fd_cfg_size);
	if (ret)
		return ret;

	ret = sprintf(name, "%s/config/aie_fd_rs_config.bin", sel_folder);
	if (ret < 0)
		return ret;

	ret = aie_copy_fw(fd, name, fd->base_para->fd_rs_cfg_va,
			  fd->fd_rs_cfg_size);
	if (ret)
		return ret;

	ret = sprintf(name, "%s/config/aie_fd_yuv2rgb_config.bin", sel_folder);
	if (ret < 0)
		return ret;

	ret = aie_copy_fw(fd, name, fd->base_para->fd_yuv2rgb_cfg_va,
			  fd->fd_yuv2rgb_cfg_size);
	if (ret)
		return ret;

	ret = sprintf(name, "%s/config/aie_attr_fd_config.bin", sel_folder);
	if (ret < 0)
		return ret;

	ret = aie_copy_fw(fd, name, fd->base_para->attr_fd_cfg_va[0],
			  fd->attr_fd_cfg_size);
	if (ret)
		return ret;

	ret = sprintf(name, "%s/config/aie_attr_yuv2rgb_config.bin", sel_folder);
	if (ret < 0)
		return ret;

	ret = aie_copy_fw(fd, name, fd->base_para->attr_yuv2rgb_cfg_va[0],
			  fd->attr_yuv2rgb_cfg_size);
	if (ret)
		return ret;

	for (i = 1; i < MAX_ENQUE_FRAME_NUM; i++) {
		memcpy(fd->base_para->attr_fd_cfg_va[i],
		       fd->base_para->attr_fd_cfg_va[0], fd->attr_fd_cfg_size);
		memcpy(fd->base_para->attr_yuv2rgb_cfg_va[i],
		       fd->base_para->attr_yuv2rgb_cfg_va[0],
		       fd->attr_yuv2rgb_cfg_size);
	}

	for (i = 0; i < fd_loop_num; i++) {
		for (j = 0; j < kernel_RDMA_RA_num; j++) {
			if (fd_ker_rdma_size[i][j]) {
				ret = sprintf(name,
					"%s/kernel/aie_fd_kernel_bias_loop%02d_%d.bin",
					sel_folder, i, j);
				if (ret < 0)
					return ret;

				ret = aie_copy_fw(
					fd, name,
					fd->dma_para->fd_kernel_va[i][j],
					fd_ker_rdma_size[i][j]);
				if (ret)
					return ret;
			}
		}
	}

	for (i = 0; i < attr_loop_num; i++) {
		for (j = 0; j < kernel_RDMA_RA_num; j++) {
			ret = sprintf(name,
				"%s/kernel/aie_attr_kernel_bias_loop%02d_%d.bin",
				sel_folder, i, j);
			if (ret < 0)
				return ret;

			ret = aie_copy_fw(fd, name,
					  fd->dma_para->attr_kernel_va[i][j],
					  attr_ker_rdma_size[i][j]);
			if (ret)
				return ret;
		}
	}
#ifdef FLD
	ret = sprintf(name, "%s/fldmodel/aie_fld_blink_weight_forest14.bin", sel_folder);
	if (ret < 0)
		return ret;

	ret = aie_copy_fw(fd, name, fd->dma_para->fld_blink_weight_va,
			fld_blink_weight_size);

	for (i = 0; i < FLD_MAX_INPUT; i++) {
		/*cv forest*/
		ret = sprintf(name, "%s/fldmodel/aie_fld_cv_forest%02d_iom3.bin", sel_folder, i);
		if (ret < 0)
			return ret;

		ret = aie_copy_fw(fd, name, fd->dma_para->fld_cv_va[i], fld_cv_size);
		if (ret)
			return ret;

		/*leafnode forest*/
		ret = sprintf(name, "%s/fldmodel/aie_fld_leafnode_forest%02d.bin", sel_folder, i);
		if (ret < 0)
			return ret;

		ret = aie_copy_fw(fd, name, fd->dma_para->fld_leafnode_va[i], fld_leafnode_size);
		if (ret)
			return ret;

		/*fp forest*/
		ret = sprintf(name, "%s/fldmodel/aie_fld_fp_forest%02d_om45.bin", sel_folder, i);
		if (ret < 0)
			return ret;

		ret = aie_copy_fw(fd, name, fd->dma_para->fld_fp_va[i], fld_fp_size);
		if (ret)
			return ret;

		/*tree forest13*/
		ret = sprintf(name, "%s/fldmodel/aie_fld_tree_forest%02d_km13.bin", sel_folder, i);
		if (ret < 0)
			return ret;

		ret = aie_copy_fw(fd, name, fd->dma_para->fld_tree13_va[i], fld_tree_size);
		if (ret)
			return ret;

		/*tree forest02*/
		ret = sprintf(name, "%s/fldmodel/aie_fld_tree_forest%02d_km02.bin", sel_folder, i);
		if (ret < 0)
			return ret;

		ret = aie_copy_fw(fd, name, fd->dma_para->fld_tree02_va[i], fld_tree_size);
		if (ret)
			return ret;
	}
#endif
#endif

	return ret;
}
#if  CHECK_SERVICE_0
static void aie_reset_output_buf(struct mtk_aie_dev *fd,
				 struct aie_enq_info *aie_cfg)
{
	if (aie_cfg->sel_mode == 0) {
		memset(fd->rs_output_hw.va, 0, fd->rs_output_hw.size);

		memset(fd->dma_para->fd_out_hw_va[rpn0_loop_num][0], 0,
		       result_size);
		memset(fd->dma_para->fd_out_hw_va[rpn1_loop_num][0], 0,
		       result_size);
		memset(fd->dma_para->fd_out_hw_va[rpn2_loop_num][0], 0,
		       result_size);
	} else if (aie_cfg->sel_mode == 1) {
		memset(fd->base_para->rs_pym_rst_va[0][0], 0,
		       fd->rs_pym_out_size[0]);
		memset(fd->base_para->rs_pym_rst_va[0][1], 0,
		       fd->rs_pym_out_size[0]);
		memset(fd->base_para->rs_pym_rst_va[0][2], 0,
		       fd->rs_pym_out_size[0]);
	}
}
#endif
static int aie_update_cfg(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg)
{
	int crop_width;
	int crop_height;

	crop_width = aie_cfg->src_img_width;
	crop_height = aie_cfg->src_img_height;

	if (aie_cfg->en_roi) {
		crop_width = aie_cfg->src_roi.x2 - aie_cfg->src_roi.x1 + 1;
		crop_height = aie_cfg->src_roi.y2 - aie_cfg->src_roi.y1 + 1;
	}

	if (crop_width == 0 || crop_height == 0) {
		dev_info(fd->dev, "AIE error:crop size is wrong");
		return -EINVAL;
	}

	if (aie_cfg->en_padding) {
		crop_width = crop_width + aie_cfg->src_padding.right +
			     aie_cfg->src_padding.left;
		crop_height = crop_height + aie_cfg->src_padding.up +
			      aie_cfg->src_padding.down;
	}

	if (aie_cfg->sel_mode == 0) {
		fd->base_para->sel_mode = aie_cfg->sel_mode;
		fd->base_para->crop_width = crop_width;
		fd->base_para->crop_height = crop_height;
		fd->base_para->src_img_addr = aie_cfg->src_img_addr;
		fd->base_para->src_img_addr_uv = aie_cfg->src_img_addr_uv;
		fd->base_para->img_width = aie_cfg->src_img_width;
		fd->base_para->img_height = aie_cfg->src_img_height;
		fd->base_para->src_img_fmt = aie_cfg->src_img_fmt;
		fd->base_para->rotate_degree = aie_cfg->rotate_degree;
	} else if (aie_cfg->sel_mode == 1) {
		fd->attr_para->sel_mode[fd->attr_para->w_idx] =
			aie_cfg->sel_mode;
		fd->attr_para->crop_width[fd->attr_para->w_idx] = crop_width;
		fd->attr_para->crop_height[fd->attr_para->w_idx] = crop_height;
		fd->attr_para->src_img_addr[fd->attr_para->w_idx] =
			aie_cfg->src_img_addr;
		fd->attr_para->src_img_addr_uv[fd->attr_para->w_idx] =
			aie_cfg->src_img_addr_uv;
		fd->attr_para->img_width[fd->attr_para->w_idx] =
			aie_cfg->src_img_width;
		fd->attr_para->img_height[fd->attr_para->w_idx] =
			aie_cfg->src_img_height;
		fd->attr_para->src_img_fmt[fd->attr_para->w_idx] =
			aie_cfg->src_img_fmt;
		fd->attr_para->rotate_degree[fd->attr_para->w_idx] =
			aie_cfg->rotate_degree;
	}

	return 0;
}

static u32 aie_combine_u16(u16 low, u16 high)
{
	return ((u32)high << 16) | low;
}

static u32 aie_combine_stride(u16 low, u16 high)
{
	return ((u32)high << 16) | (low & 0x000F);
}

static int aie_config_y2r(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg,
			  int mode)
{
	u32 img_addr = 0;
	u32 img_addr_UV = 0;
	u32 img_off = 0;
	u32 img_off_uv = 0;
	u32 *yuv2rgb_cfg = NULL;
	u32 srcbuf = 0, srcbuf_UV = 0;
	u16 xmag_0 = 0, ymag_0 = 0;
	u16 pym0_out_w = 0;
	u16 pym0_out_h = 0;
	u16 stride_pym0_out_w = 0;
	u16 src_crop_w = 0;
	u16 src_crop_h = 0;
	unsigned int msb_bit_0 = 0, msb_bit_1 = 0, msb_bit_2 = 0;


	if (aie_cfg->en_roi == false) {
		img_off = 0;
		img_off_uv = 0;
	} else {
		if (aie_cfg->src_img_fmt == FMT_MONO ||
		    aie_cfg->src_img_fmt == FMT_YUV_2P ||
		    aie_cfg->src_img_fmt == FMT_YVU_2P) {
			img_off =
				aie_cfg->src_img_stride * aie_cfg->src_roi.y1 +
				aie_cfg->src_roi.x1;
			img_off_uv =
				aie_cfg->src_img_stride * aie_cfg->src_roi.y1 +
				aie_cfg->src_roi.x1;
		} else if (aie_cfg->src_img_fmt == FMT_YUV420_2P ||
			   aie_cfg->src_img_fmt == FMT_YUV420_1P) {
			img_off =
				aie_cfg->src_img_stride * aie_cfg->src_roi.y1 +
				aie_cfg->src_roi.x1;
			img_off_uv = aie_cfg->src_img_stride *
				     aie_cfg->src_roi.y1 / 2 +
				     aie_cfg->src_roi.x1;
		} else if (aie_cfg->src_img_fmt == FMT_YUYV ||
			   aie_cfg->src_img_fmt == FMT_YVYU ||
			   aie_cfg->src_img_fmt == FMT_UYVY ||
			   aie_cfg->src_img_fmt == FMT_VYUY) {
			img_off =
				aie_cfg->src_img_stride * aie_cfg->src_roi.y1 +
				aie_cfg->src_roi.x1 * 2;
			img_off_uv =
				aie_cfg->src_img_stride * aie_cfg->src_roi.y1 +
				aie_cfg->src_roi.x1 * 2;
		} else {
			dev_info(fd->dev,
				 "AIE error: Unsupport input format %d",
				 aie_cfg->src_img_fmt);
			return -EINVAL;
		}
	}

	img_addr = aie_cfg->src_img_addr + img_off;
	img_addr_UV = aie_cfg->src_img_addr_uv + img_off_uv;

	srcbuf = img_addr;
	if (aie_cfg->src_img_fmt == FMT_YUV420_2P ||
	    aie_cfg->src_img_fmt == FMT_YUV420_1P ||
	    aie_cfg->src_img_fmt == FMT_YUV_2P ||
	    aie_cfg->src_img_fmt == FMT_YVU_2P)
		srcbuf_UV = img_addr_UV;
	else
		srcbuf_UV = 0;

	if (mode == 0) {
		src_crop_w = fd->base_para->crop_width;
		src_crop_h = fd->base_para->crop_height;
		yuv2rgb_cfg = (u32 *)fd->base_para->fd_yuv2rgb_cfg_va;
		pym0_out_w = fd->base_para->pyramid_width;
	} else if (mode == 1) {
		src_crop_w = fd->attr_para->crop_width[fd->attr_para->w_idx];
		src_crop_h = fd->attr_para->crop_height[fd->attr_para->w_idx];
		yuv2rgb_cfg =
			(u32 *)fd->base_para
				->attr_yuv2rgb_cfg_va[fd->attr_para->w_idx];
		pym0_out_w = ATTR_MODE_PYRAMID_WIDTH;
	}
	if (src_crop_w == 0) {
		dev_info(fd->dev, "src_crop_w = 0: cannot by divide");
		return -1;
	}
	pym0_out_h = pym0_out_w * src_crop_h / src_crop_w;

	if (pym0_out_w != 0) {
		xmag_0 = 512 * src_crop_w / pym0_out_w;
		ymag_0 = xmag_0;
	} else {
		xmag_0 = 0;
		ymag_0 = 0;
	}

	yuv2rgb_cfg[Y2R_SRC_DST_FORMAT] =
		(yuv2rgb_cfg[Y2R_SRC_DST_FORMAT] & 0xFFFFFFF8) |
		((aie_cfg->src_img_fmt) & 0x7);
	if (aie_cfg->src_img_fmt == FMT_YUV420_2P ||
	    aie_cfg->src_img_fmt == FMT_YUV420_1P) { /* for match patten */
		yuv2rgb_cfg[Y2R_SRC_DST_FORMAT] =
			(yuv2rgb_cfg[Y2R_SRC_DST_FORMAT] & 0xFFFFFFF8) |
			((0x3) & 0x7);
	}
	yuv2rgb_cfg[Y2R_IN_W_H] = (yuv2rgb_cfg[Y2R_IN_W_H] & 0xF800F800) |
				  ((src_crop_w << 16) & 0x7FF0000) |
				  (src_crop_h & 0x7FF);
	yuv2rgb_cfg[Y2R_OUT_W_H] = (yuv2rgb_cfg[Y2R_OUT_W_H] & 0xF800F800) |
				   ((pym0_out_w << 16) & 0x7FF0000) |
				   (pym0_out_h & 0x7FF);

	if (aie_cfg->src_img_fmt == FMT_YUV_2P ||
	    aie_cfg->src_img_fmt == FMT_YVU_2P) { /* 2 plane */
		yuv2rgb_cfg[Y2R_RA0_RA1_EN] =
			(yuv2rgb_cfg[Y2R_RA0_RA1_EN] & 0xFFFFFFEE) | 0x11;
		if (aie_cfg->en_roi) {
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE0] = aie_combine_u16(
				aie_cfg->src_roi.x2 - aie_cfg->src_roi.x1,
				aie_cfg->src_roi.y2 - aie_cfg->src_roi.y1);
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE1] = aie_combine_u16(
				aie_cfg->src_roi.x2 - aie_cfg->src_roi.x1,
				aie_cfg->src_roi.y2 - aie_cfg->src_roi.y1);
		} else {
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE0] =
				aie_combine_u16(src_crop_w - 1, src_crop_h - 1);
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE1] =
				aie_combine_u16(src_crop_w - 1, src_crop_h - 1);
		}
		yuv2rgb_cfg[Y2R_IN_STRIDE0_BUS_SIZE0] =
			(yuv2rgb_cfg[Y2R_IN_STRIDE0_BUS_SIZE0] & 0xFFF0) |
			((aie_cfg->src_img_stride << 16) & 0xFFFF0000) | 0x1;
		yuv2rgb_cfg[Y2R_IN_STRIDE1_BUS_SIZE1] =
			(yuv2rgb_cfg[Y2R_IN_STRIDE1_BUS_SIZE1] & 0xFFF0) |
			((aie_cfg->src_img_stride << 16) & 0xFFFF0000) | 0x1;
	} else if (aie_cfg->src_img_fmt == FMT_MONO) {
		yuv2rgb_cfg[Y2R_RA0_RA1_EN] =
			(yuv2rgb_cfg[Y2R_RA0_RA1_EN] & 0xFFFFFFEE) | 0x01;
		if (aie_cfg->en_roi) {
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE0] = aie_combine_u16(
				aie_cfg->src_roi.x2 - aie_cfg->src_roi.x1,
				aie_cfg->src_roi.y2 - aie_cfg->src_roi.y1);
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE1] = aie_combine_u16(
				aie_cfg->src_roi.x2 - aie_cfg->src_roi.x1,
				aie_cfg->src_roi.y2 - aie_cfg->src_roi.y1);
		} else {
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE0] =
				aie_combine_u16(src_crop_w - 1, src_crop_h - 1);
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE1] =
				aie_combine_u16(src_crop_w - 1, src_crop_h - 1);
		}
		yuv2rgb_cfg[Y2R_IN_STRIDE0_BUS_SIZE0] =
			(yuv2rgb_cfg[Y2R_IN_STRIDE0_BUS_SIZE0] & 0xFFF0) |
			((aie_cfg->src_img_stride << 16) & 0xFFFF0000) | 0x0;
		yuv2rgb_cfg[Y2R_IN_STRIDE1_BUS_SIZE1] =
			(yuv2rgb_cfg[Y2R_IN_STRIDE1_BUS_SIZE1] & 0xFFF0) |
			((aie_cfg->src_img_stride << 16) & 0xFFFF0000) | 0x0;
	} else if (aie_cfg->src_img_fmt == FMT_YUYV ||
		   aie_cfg->src_img_fmt == FMT_YVYU ||
		   aie_cfg->src_img_fmt == FMT_UYVY ||
		   aie_cfg->src_img_fmt == FMT_VYUY) { /* 1 plane */
		yuv2rgb_cfg[Y2R_RA0_RA1_EN] =
			(yuv2rgb_cfg[Y2R_RA0_RA1_EN] & 0xFFFFFFEE) | 0x1;
		if (aie_cfg->en_roi) {
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE0] = aie_combine_u16(
				2 * (aie_cfg->src_roi.x2 - aie_cfg->src_roi.x1 +
				     1) -
					1,
				aie_cfg->src_roi.y2 - aie_cfg->src_roi.y1);
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE1] = aie_combine_u16(
				2 * (aie_cfg->src_roi.x2 - aie_cfg->src_roi.x1 +
				     1) -
					1,
				aie_cfg->src_roi.y2 - aie_cfg->src_roi.y1);
		} else {
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE0] = aie_combine_u16(
				2 * src_crop_w - 1, src_crop_h - 1);
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE1] = aie_combine_u16(
				2 * src_crop_w - 1, src_crop_h - 1);
		}
		yuv2rgb_cfg[Y2R_IN_STRIDE0_BUS_SIZE0] =
			(yuv2rgb_cfg[Y2R_IN_STRIDE0_BUS_SIZE0] & 0xFFF0) |
			((aie_cfg->src_img_stride << 16) & 0xFFFF0000) | 0x3;
		yuv2rgb_cfg[Y2R_IN_STRIDE1_BUS_SIZE1] =
			(yuv2rgb_cfg[Y2R_IN_STRIDE1_BUS_SIZE1] & 0xFFF0) |
			((aie_cfg->src_img_stride << 16) & 0xFFFF0000) | 0x3;
	}

	/* AIE3.0 */
	if (aie_cfg->src_img_fmt == FMT_YUV420_2P ||
	    aie_cfg->src_img_fmt == FMT_YUV420_1P) {
		yuv2rgb_cfg[Y2R_RA0_RA1_EN] =
			(yuv2rgb_cfg[Y2R_RA0_RA1_EN] & 0xFFFFFFEE) | 0x11;
		if (aie_cfg->en_roi) {
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE0] = aie_combine_u16(
				aie_cfg->src_roi.x2 - aie_cfg->src_roi.x1,
				aie_cfg->src_roi.y2 - aie_cfg->src_roi.y1);
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE1] = aie_combine_u16(
				aie_cfg->src_roi.x2 - aie_cfg->src_roi.x1,
				(aie_cfg->src_roi.y2 - aie_cfg->src_roi.y1) /
					2);
		} else {
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE0] =
				aie_combine_u16(src_crop_w - 1, src_crop_h - 1);
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE1] = aie_combine_u16(
				src_crop_w - 1, src_crop_h / 2 - 1);
		}
		yuv2rgb_cfg[Y2R_IN_STRIDE0_BUS_SIZE0] =
			(yuv2rgb_cfg[Y2R_IN_STRIDE0_BUS_SIZE0] & 0xFFF0) |
			((aie_cfg->src_img_stride << 16) & 0xFFFF0000) | 0x0;
		yuv2rgb_cfg[Y2R_IN_STRIDE1_BUS_SIZE1] =
			(yuv2rgb_cfg[Y2R_IN_STRIDE1_BUS_SIZE1] & 0xFFF0) |
			((aie_cfg->src_img_stride << 16) & 0xFFFF0000) | 0x0;

		yuv2rgb_cfg[Y2R_CO2_FMT_MODE_EN] =
			(yuv2rgb_cfg[Y2R_CO2_FMT_MODE_EN] & 0xFFFFFFFE) | 0x01;
		if (aie_cfg->en_roi) {
			yuv2rgb_cfg[Y2R_CO2_CROP_X] = aie_combine_u16(
				0, aie_cfg->src_roi.x2 - aie_cfg->src_roi.x1);
			yuv2rgb_cfg[Y2R_CO2_CROP_Y] = aie_combine_u16(
				0, aie_cfg->src_roi.y2 - aie_cfg->src_roi.y1);
		} else {
			yuv2rgb_cfg[Y2R_CO2_CROP_X] =
				aie_combine_u16(0, src_crop_w - 1);
			yuv2rgb_cfg[Y2R_CO2_CROP_Y] =
				aie_combine_u16(0, src_crop_h - 1);
		}
	} else {
		yuv2rgb_cfg[Y2R_CO2_FMT_MODE_EN] =
			(yuv2rgb_cfg[Y2R_CO2_FMT_MODE_EN] & 0xFFFFFFFE);

		if (aie_cfg->en_roi) {
			yuv2rgb_cfg[Y2R_CO2_CROP_X] = aie_combine_u16(
				0, aie_cfg->src_roi.x2 - aie_cfg->src_roi.x1);
			yuv2rgb_cfg[Y2R_CO2_CROP_Y] = aie_combine_u16(
				0, aie_cfg->src_roi.y2 - aie_cfg->src_roi.y1);
		} else {
			yuv2rgb_cfg[Y2R_CO2_CROP_X] =
				aie_combine_u16(0, src_crop_w - 1);
			yuv2rgb_cfg[Y2R_CO2_CROP_Y] =
				aie_combine_u16(0, src_crop_h - 1);
		}
	}

	stride_pym0_out_w = round_up(pym0_out_w, 8);

	yuv2rgb_cfg[Y2R_OUT_X_Y_SIZE0] =
		aie_combine_u16(pym0_out_w - 1, pym0_out_h - 1);
	yuv2rgb_cfg[Y2R_OUT_STRIDE0_BUS_SIZE0] = aie_combine_u16(
		yuv2rgb_cfg[Y2R_OUT_STRIDE0_BUS_SIZE0], stride_pym0_out_w);
	yuv2rgb_cfg[Y2R_OUT_X_Y_SIZE1] =
		aie_combine_u16(pym0_out_w - 1, pym0_out_h - 1);
	yuv2rgb_cfg[Y2R_OUT_STRIDE1_BUS_SIZE1] = aie_combine_u16(
		yuv2rgb_cfg[Y2R_OUT_STRIDE1_BUS_SIZE1], stride_pym0_out_w);
	yuv2rgb_cfg[Y2R_OUT_X_Y_SIZE2] =
		aie_combine_u16(pym0_out_w - 1, pym0_out_h - 1);
	yuv2rgb_cfg[Y2R_OUT_STRIDE2_BUS_SIZE2] = aie_combine_u16(
		yuv2rgb_cfg[Y2R_OUT_STRIDE2_BUS_SIZE2], stride_pym0_out_w);

	if (aie_cfg->en_padding) {
		yuv2rgb_cfg[Y2R_PADDING_EN_UP_DOWN] =
			1 | ((aie_cfg->src_padding.up << 4) & 0x1FF0) |
			((aie_cfg->src_padding.down << 16) & 0x01FF0000);
		yuv2rgb_cfg[Y2R_PADDING_RIGHT_LEFT] =
			(aie_cfg->src_padding.right & 0x01FF) |
			((aie_cfg->src_padding.left << 16) & 0x01FF0000);
	} else {
		yuv2rgb_cfg[Y2R_PADDING_EN_UP_DOWN] = 0;
		yuv2rgb_cfg[Y2R_PADDING_RIGHT_LEFT] = 0;
	}

	yuv2rgb_cfg[Y2R_IN_0] = srcbuf;
	yuv2rgb_cfg[Y2R_IN_1] = srcbuf_UV;


	//yuv2rgb_cfg[POS_Y2RCON_IN_BA_MSB] = (u32)0x00000303; //for UT
	yuv2rgb_cfg[POS_Y2RCON_IN_BA_MSB] = (u32)(fd->img_msb_y | fd->img_msb_uv << 8);
	msb_bit_0 = (fd->base_para->rs_pym_rst_pa[0][0] &
						0xf00000000) >> 32;
	msb_bit_1 = (fd->base_para->rs_pym_rst_pa[0][1] &
						0xf00000000) >> 32;
	msb_bit_2 = (fd->base_para->rs_pym_rst_pa[0][2] &
						0xf00000000) >> 32;

	yuv2rgb_cfg[POS_Y2RCON_OUT_BA_MSB] = (u32)(msb_bit_0 | msb_bit_1 << 8 |
						msb_bit_2 << 16);//0x00030303

	yuv2rgb_cfg[Y2R_OUT_0] = (u32)fd->base_para->rs_pym_rst_pa[0][0];
	yuv2rgb_cfg[Y2R_OUT_1] = (u32)fd->base_para->rs_pym_rst_pa[0][1];
	yuv2rgb_cfg[Y2R_OUT_2] = (u32)fd->base_para->rs_pym_rst_pa[0][2];

	yuv2rgb_cfg[Y2R_X_Y_MAG] =
		(xmag_0 & 0x3FFF) | ((ymag_0 << 16) & 0x3FFF0000);

	if (src_crop_w >= pym0_out_w) { /* down scale AIE1.0 by FRZ */
		yuv2rgb_cfg[Y2R_RS_SEL_SRZ_EN] =
			(yuv2rgb_cfg[Y2R_RS_SEL_SRZ_EN] & 0x00100070) |
			FDRZ_BIT;
		yuv2rgb_cfg[Y2R_SRZ_HORI_STEP] = 0;
		yuv2rgb_cfg[Y2R_SRZ_VERT_STEP] = 0;
	} else { /* SRZ */
		/* 0: FDRZ for down scaling */
		/* 1: SRZ for up scaling */
		yuv2rgb_cfg[Y2R_RS_SEL_SRZ_EN] =
			(yuv2rgb_cfg[Y2R_RS_SEL_SRZ_EN] & 0x00100070) | SRZ_BIT;
		yuv2rgb_cfg[Y2R_SRZ_HORI_STEP] =
			((src_crop_w - 1) << 15) / (pym0_out_w - 1);
		yuv2rgb_cfg[Y2R_SRZ_VERT_STEP] =
			((src_crop_h - 1) << 15) / (pym0_out_h - 1);
	}

	return 0;
}

static int aie_config_rs(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg)
{
	u32 *rs_cfg = NULL;
	u32 *rs_tbl[2] = {NULL, NULL};
	u16 xmag_0 = 0, ymag_0 = 0;
	u16 pym_out_w[3] = {0, 0, 0};
	u16 pym_out_h[3] = {0, 0, 0};
	u16 round_w = 0;
	u16 src_crop_w = 0;
	u16 src_crop_h = 0;
	int i = 0, msb_bit_0 = 0, msb_bit_1 = 0, msb_bit_2 = 0;

	if (aie_cfg->sel_mode == 0) {
		src_crop_w = fd->base_para->crop_width;
		src_crop_h = fd->base_para->crop_height;
	} else if (aie_cfg->sel_mode == 1) {
		src_crop_w = fd->attr_para->crop_width[fd->attr_para->w_idx];
		src_crop_h = fd->attr_para->crop_height[fd->attr_para->w_idx];
	}

	rs_cfg = (u32 *)fd->base_para->fd_rs_cfg_va;

	pym_out_w[0] = fd->base_para->pyramid_width;
	pym_out_w[1] = pym_out_w[0] >> 1;
	pym_out_w[2] = pym_out_w[1] >> 1;

	if (src_crop_w == 0) {
		dev_info(fd->dev, "src_crop_w = 0: cannot by divide");
		return -1;
	}
	pym_out_h[0] = pym_out_w[0] * src_crop_h / src_crop_w;
	pym_out_h[1] = pym_out_h[0] >> 1;
	pym_out_h[2] = pym_out_h[1] >> 1;

	for (i = 0; i < 2; i++) {
		rs_tbl[i] = rs_cfg + RS_CONFIG_SIZE * i;

		msb_bit_0 = (fd->base_para->rs_pym_rst_pa[i][0] &
							0xf00000000) >> 32;
		msb_bit_1 = (fd->base_para->rs_pym_rst_pa[i][1] &
							0xf00000000) >> 32;
		msb_bit_2 = (fd->base_para->rs_pym_rst_pa[i][2] &
							0xf00000000) >> 32;

		rs_tbl[i][POS_RSCON_IN_BA_MSB] = (u32)(msb_bit_0 | msb_bit_1 << 8 |
							msb_bit_2 << 16); //0x00030303

		rs_tbl[i][RS_IN_0] = (u32)fd->base_para->rs_pym_rst_pa[i][0];
		rs_tbl[i][RS_IN_1] = (u32)fd->base_para->rs_pym_rst_pa[i][1];
		rs_tbl[i][RS_IN_2] = (u32)fd->base_para->rs_pym_rst_pa[i][2];

		msb_bit_0 = (fd->base_para->rs_pym_rst_pa[i + 1][0] &
							0xf00000000) >> 32;
		msb_bit_1 = (fd->base_para->rs_pym_rst_pa[i + 1][1] &
							0xf00000000) >> 32;
		msb_bit_2 = (fd->base_para->rs_pym_rst_pa[i + 1][2] &
							0xf00000000) >> 32;

		rs_tbl[i][POS_RSCON_OUT_BA_MSB] = (u32)(msb_bit_0 | msb_bit_1 << 8 |
							msb_bit_2 << 16); //0x00030303

		rs_tbl[i][RS_OUT_0] =
			(u32)fd->base_para->rs_pym_rst_pa[i + 1][0];
		rs_tbl[i][RS_OUT_1] =
			(u32)fd->base_para->rs_pym_rst_pa[i + 1][1];
		rs_tbl[i][RS_OUT_2] =
			(u32)fd->base_para->rs_pym_rst_pa[i + 1][2];

		rs_tbl[i][RS_INPUT_W_H] =
			(rs_tbl[i][RS_INPUT_W_H] & 0xF800F800) |
			(pym_out_h[i] & 0x7FF) |
			((pym_out_w[i] << 16) & 0x7FF0000);
		rs_tbl[i][RS_OUTPUT_W_H] =
			(rs_tbl[i][RS_OUTPUT_W_H] & 0xF800F800) |
			(pym_out_h[i + 1] & 0x7FF) |
			((pym_out_w[i + 1] << 16) & 0x7FF0000);
		rs_tbl[i][RS_IN_X_Y_SIZE0] =
			aie_combine_u16(pym_out_w[i] - 1, pym_out_h[i] - 1);
		rs_tbl[i][RS_IN_X_Y_SIZE1] =
			aie_combine_u16(pym_out_w[i] - 1, pym_out_h[i] - 1);
		rs_tbl[i][RS_IN_X_Y_SIZE2] =
			aie_combine_u16(pym_out_w[i] - 1, pym_out_h[i] - 1);
		rs_tbl[i][RS_IN_STRIDE0] =
			aie_combine_u16(rs_tbl[i][RS_IN_STRIDE0], pym_out_w[i]);
		rs_tbl[i][RS_IN_STRIDE1] =
			aie_combine_u16(rs_tbl[i][RS_IN_STRIDE1], pym_out_w[i]);
		rs_tbl[i][RS_IN_STRIDE2] =
			aie_combine_u16(rs_tbl[i][RS_IN_STRIDE2], pym_out_w[i]);
		rs_tbl[i][RS_OUT_X_Y_SIZE0] = aie_combine_u16(
			pym_out_w[i + 1] - 1, pym_out_h[i + 1] - 1);
		rs_tbl[i][RS_OUT_X_Y_SIZE1] = aie_combine_u16(
			pym_out_w[i + 1] - 1, pym_out_h[i + 1] - 1);
		rs_tbl[i][RS_OUT_X_Y_SIZE2] = aie_combine_u16(
			pym_out_w[i + 1] - 1, pym_out_h[i + 1] - 1);

		if (i == 0)
			round_w = pym_out_w[i + 1];
		else
			round_w = round_up(pym_out_w[i + 1], 8);

		rs_tbl[i][RS_OUT_STRIDE0] =
			aie_combine_u16(rs_tbl[i][RS_OUT_STRIDE0], round_w);
		rs_tbl[i][RS_OUT_STRIDE1] =
			aie_combine_u16(rs_tbl[i][RS_OUT_STRIDE1], round_w);
		rs_tbl[i][RS_OUT_STRIDE2] =
			aie_combine_u16(rs_tbl[i][RS_OUT_STRIDE2], round_w);

		xmag_0 = 512 * pym_out_w[i] / pym_out_w[i + 1];
		ymag_0 = xmag_0;

		rs_tbl[i][RS_X_Y_MAG] =
			(xmag_0 & 0x3FFF) | ((ymag_0 << 16) & 0x3FFF0000);
	}

	return 0;
}

static int aie_config_network(struct mtk_aie_dev *fd,
			      struct aie_enq_info *aie_cfg)
{
	u16 conv_width = 0;
	u16 conv_height = 0;
	u8 i = 0;
	u8 j = 0;
	u8 uch = 0;
	u8 uloop = 0;
	u16 fd_xsize[4] = {0, 0, 0, 0};
	void *fd_cfg = NULL;
	u32 *fd_cur_cfg = NULL;
	u32 *fd_cur_set = NULL;
	u16 pyramid0_out_w = 0;
	u16 pyramid0_out_h = 0;
	u16 pyramid1_out_h = 0;
	u16 pyramid2_out_h = 0;
	u16 input_height = 0;
	u16 out_height = 0;
	u16 out_ysize_plus_1 = 0;
	u16 out_ysize_plus_1_stride2 = 0;
	u32 src_crop_w  = 0;
	u32 src_crop_h = 0;
	struct aie_static_info *pstv = NULL;
	int msb_bit_0 = 0, msb_bit_1 = 0, msb_bit_2 = 0, msb_bit_3 = 0;
	int filter = 0;

	pstv = &fd->st_info;

	if (aie_cfg->sel_mode == 0) {
		src_crop_w = fd->base_para->crop_width;
		src_crop_h = fd->base_para->crop_height;
	} else if (aie_cfg->sel_mode == 1) {
		src_crop_w = fd->attr_para->crop_width[fd->attr_para->w_idx];
		src_crop_h = fd->attr_para->crop_height[fd->attr_para->w_idx];
	}

	pyramid0_out_w = fd->base_para->pyramid_width;
	if (src_crop_w == 0) {
		dev_info(fd->dev, "src_crop_w = 0: cannot by divide");
		return -1;
	}
	pyramid0_out_h = pyramid0_out_w * src_crop_h / src_crop_w;

	pyramid1_out_h = pyramid0_out_h / 2;
	pyramid2_out_h = pyramid1_out_h / 2;

	fd_cfg = fd->base_para->fd_fd_cfg_va;

	for (i = 0; i < fd_loop_num; i++) {
		fd_cur_cfg = (u32 *)fd_cfg + FD_CONFIG_SIZE * i;
		fd_cur_cfg[FD_INPUT_ROTATE] =
			(fd_cur_cfg[FD_INPUT_ROTATE] & 0xFFFF0FFF) |
			((aie_cfg->rotate_degree << 12) & 0x3000);

		if (i == 0) {
			input_height = pyramid2_out_h;
		} else if (i == (rpn2_loop_num + 1)) {
			input_height = pyramid1_out_h;
		} else if (i == (rpn1_loop_num + 1)) {
			input_height = pyramid0_out_h;
		} else {
			if (fd_out_stride2_in[i] == 0)
				input_height = out_height;
			else
				input_height = (out_height + 1) / 2;
		}
		if (i == rpn0_loop_num)
			fd->pose_height = input_height;

		if (fd_maxpool[i] == 1 && fd_stride[i] == 1)
			out_height =
				DIV_ROUND_UP(input_height, 2 * fd_maxpool[i]);
		else
			out_height = DIV_ROUND_UP(
				input_height, fd_stride[i] + 2 * fd_maxpool[i]);

		if (i == rpn0_loop_num || i == rpn1_loop_num ||
		    i == rpn2_loop_num) {
			conv_width = fd->base_para->img_width;
			conv_height = fd->base_para->img_height;
			fd_xsize[0] =
				pstv->img_width[i] * 2 * 16 * anchor_en_num[i] -
				1;
			fd_xsize[1] = fd_xsize[2] = fd_xsize[3] =
				pstv->img_width[i] * 2 * 32 * anchor_en_num[i] -
				1;
		} else {
			conv_width =
				DIV_ROUND_UP(pstv->img_width[i], fd_stride[i]);
			conv_height = DIV_ROUND_UP(input_height, fd_stride[i]);

			fd_xsize[0] = fd_xsize[1] = fd_xsize[2] = fd_xsize[3] =
				pstv->input_xsize_plus_1[i] - 1;
		}

		fd_cur_cfg[FD_CONV_WIDTH_MOD6] =
			(fd_cur_cfg[FD_CONV_WIDTH_MOD6] & 0xFF8FFFFF) |
			(((conv_width % 6) << 20) & 0x00700000);
		fd_cur_cfg[FD_CONV_IMG_W_H] =
			aie_combine_u16(conv_height, conv_width);

		fd_cur_cfg[FD_IN_IMG_W_H] =
			aie_combine_u16(input_height, pstv->img_width[i]);
		fd_cur_cfg[FD_OUT_IMG_W_H] =
			aie_combine_u16(out_height, pstv->out_width[i]);

		if (fd_rdma_en[i][0][0] != -1) {
			for (j = 0; j < 4; j++) {
				fd_cur_cfg[FD_IN_X_Y_SIZE0 + 2 * j] =
					aie_combine_u16(fd_xsize[j],
							input_height - 1);

				fd_cur_cfg[FD_IN_STRIDE0_BUS_SIZE0 + 2 * j] =
					aie_combine_stride(
						fd_cur_cfg
							[FD_IN_STRIDE0_BUS_SIZE0 +
							 2 * j],
						fd_xsize[j] + 1);
			}
		}

		out_ysize_plus_1 = out_height - 1;
		out_ysize_plus_1_stride2 = (out_height + 1) / 2 - 1;

		for (j = 0; j < output_WDMA_WRA_num; j++) {
			fd_cur_set = fd_cur_cfg + 2 * j;
			if (!fd_wdma_en[i][j])
				continue;

			if (out_stride_size[i][j] == 1) {
				fd_cur_set[FD_OUT_X_Y_SIZE0] = aie_combine_u16(
					pstv->out_xsize_plus_1[i] - 1,
					out_ysize_plus_1);
				fd_cur_set[FD_OUT_STRIDE0_BUS_SIZE0] =
					aie_combine_stride(
						fd_cur_set
							[FD_OUT_STRIDE0_BUS_SIZE0],
						pstv->out_stride[i]);
			} else if (out_stride_size[i][j] == 2) {
				fd_cur_set[FD_OUT_X_Y_SIZE0] = aie_combine_u16(
					pstv->out_xsize_plus_1_stride2[i] - 1,
					out_ysize_plus_1_stride2);
				fd_cur_set[FD_OUT_STRIDE0_BUS_SIZE0] =
					aie_combine_stride(
						fd_cur_set
							[FD_OUT_STRIDE0_BUS_SIZE0],
						pstv->out_stride_stride2[i]);
			}
		}

		if (i == rpn0_loop_num || i == rpn1_loop_num || i == rpn2_loop_num) {

			fd_cur_cfg[FD_RPN_SET] =
				aie_combine_u16(fd_cur_cfg[FD_RPN_SET],
						fd->base_para->rpn_anchor_thrd);
				fd_cur_cfg[FD_IN_CHANNEL_PACK] = fd_cur_cfg[Y2R_SRC_DST_FORMAT] |
									0x30000000;
		}

		if (i == rpn0_loop_num) {
			fd_cur_cfg[FD_IMAGE_COORD] =
				(fd_cur_cfg[FD_IMAGE_COORD] & 0xF) |
				(((src_crop_w * 100 /
				   (int)fd->base_para->pyramid_width * 512 /
				   100)
				  << 4) &
				 0x7FFF0);
			fd_cur_cfg[FD_IMAGE_COORD_XY_OFST] = 0;
			if (aie_cfg->en_roi) {
				fd_cur_cfg[FD_IMAGE_COORD_XY_OFST] =
				(aie_cfg->src_roi.x1 - aie_cfg->src_padding.left) |
				(aie_cfg->src_roi.y1 - aie_cfg->src_padding.up) << 16;
			}
		} else if (i == rpn1_loop_num) {
			fd_cur_cfg[FD_IMAGE_COORD] =
				(fd_cur_cfg[FD_IMAGE_COORD] & 0xF) |
				(((src_crop_w * 100 /
				   (int)fd->base_para->pyramid_width * 2 * 512 /
				   100)
				  << 4) &
				 0x7FFF0);
			fd_cur_cfg[FD_IMAGE_COORD_XY_OFST] = 0;
			if (aie_cfg->en_roi) {
				fd_cur_cfg[FD_IMAGE_COORD_XY_OFST] =
				(aie_cfg->src_roi.x1 - aie_cfg->src_padding.left) |
				(aie_cfg->src_roi.y1 - aie_cfg->src_padding.up) << 16;
			}
		} else if (i == rpn2_loop_num) {
			fd_cur_cfg[FD_IMAGE_COORD] =
				(fd_cur_cfg[FD_IMAGE_COORD] & 0xF) |
				(((src_crop_w * 100 /
				   (int)fd->base_para->pyramid_width * 4 * 512 /
				   100)
				  << 4) &
				 0x7FFF0);
			fd_cur_cfg[FD_IMAGE_COORD_XY_OFST] = 0;
			if (aie_cfg->en_roi) {
				fd_cur_cfg[FD_IMAGE_COORD_XY_OFST] =
				(aie_cfg->src_roi.x1 - aie_cfg->src_padding.left) |
				(aie_cfg->src_roi.y1 - aie_cfg->src_padding.up) << 16;
			}
		}

		/* IN_FM_BASE_ADR */
		if (i == 0) {
			msb_bit_0 = (fd->base_para->rs_pym_rst_pa[2][0] &
								0xf00000000) >> 32;
			msb_bit_1 = (fd->base_para->rs_pym_rst_pa[2][1] &
								0xf00000000) >> 32;
			msb_bit_2 = (fd->base_para->rs_pym_rst_pa[2][2] &
								0xf00000000) >> 32;

			fd_cur_cfg[POS_FDCON_IN_BA_MSB] = (u32)(msb_bit_0 |
				msb_bit_1 << 8 | msb_bit_2 << 16);
			fd_cur_cfg[FD_IN_0] =
				(u32)(fd->base_para->rs_pym_rst_pa[2][0]);
			fd_cur_cfg[FD_IN_1] =
				(u32)(fd->base_para->rs_pym_rst_pa[2][1]);
			fd_cur_cfg[FD_IN_2] =
				(u32)(fd->base_para->rs_pym_rst_pa[2][2]);
		} else if (i == (rpn2_loop_num + 1)) {
			msb_bit_0 = (fd->base_para->rs_pym_rst_pa[1][0] &
								0xf00000000) >> 32;
			msb_bit_1 = (fd->base_para->rs_pym_rst_pa[1][1] &
								0xf00000000) >> 32;
			msb_bit_2 = (fd->base_para->rs_pym_rst_pa[1][2] &
								0xf00000000) >> 32;

			fd_cur_cfg[POS_FDCON_IN_BA_MSB] = (u32)(msb_bit_0 |
				msb_bit_1 << 8 |  msb_bit_2 << 16);

			fd_cur_cfg[FD_IN_0] =
				(u32)(fd->base_para->rs_pym_rst_pa[1][0]);
			fd_cur_cfg[FD_IN_1] =
				(u32)(fd->base_para->rs_pym_rst_pa[1][1]);
			fd_cur_cfg[FD_IN_2] =
				(u32)(fd->base_para->rs_pym_rst_pa[1][2]);
		} else if (i == (rpn1_loop_num + 1)) {
			msb_bit_0 = (fd->base_para->rs_pym_rst_pa[0][0] &
								0xf00000000) >> 32;
			msb_bit_1 = (fd->base_para->rs_pym_rst_pa[0][1] &
								0xf00000000) >> 32;
			msb_bit_2 = (fd->base_para->rs_pym_rst_pa[0][2] &
								0xf00000000) >> 32;

			fd_cur_cfg[POS_FDCON_IN_BA_MSB] = (u32)(msb_bit_0 |
						msb_bit_1 << 8 |  msb_bit_2 << 16);
			fd_cur_cfg[FD_IN_0] =
				(u32)(fd->base_para->rs_pym_rst_pa[0][0]);
			fd_cur_cfg[FD_IN_1] =
				(u32)(fd->base_para->rs_pym_rst_pa[0][1]);
			fd_cur_cfg[FD_IN_2] =
				(u32)(fd->base_para->rs_pym_rst_pa[0][2]);
		} else {
			for (j = 0; j < input_WDMA_WRA_num; j++) {
				if (fd_rdma_en[i][j][0] != -1) {
					uloop = fd_rdma_en[i][j][0];
					uch = fd_rdma_en[i][j][1];
					if (j == 0) {
						msb_bit_0 = (fd->dma_para->fd_out_hw_pa[uloop][uch]
								& 0xf00000000) >> 32;
						filter = 0xfffffffc | msb_bit_0;
						fd_cur_cfg[POS_FDCON_IN_BA_MSB]
							= (u32)(fd_cur_cfg[POS_FDCON_IN_BA_MSB] &
								filter);
					} else if (j == 1) {
						msb_bit_1 = (fd->dma_para->fd_out_hw_pa[uloop][uch]
								& 0xf00000000) >> 32;
						filter = 0xfffffcff | (msb_bit_1 << 8);
						fd_cur_cfg[POS_FDCON_IN_BA_MSB]
							= (u32)(fd_cur_cfg[POS_FDCON_IN_BA_MSB] &
								filter);
					} else if (j == 2) {
						msb_bit_2 = (fd->dma_para->fd_out_hw_pa[uloop][uch]
								& 0xf00000000) >> 32;
						filter = 0xfffcffff | (msb_bit_2 << 16);
						fd_cur_cfg[POS_FDCON_IN_BA_MSB]
							= (u32)(fd_cur_cfg[POS_FDCON_IN_BA_MSB]
								& filter);
					} else if (j == 3) {
						msb_bit_3 = (fd->dma_para->fd_out_hw_pa[uloop][uch]
								& 0xf00000000) >> 32;
						filter = 0xfcffffff | (msb_bit_3 << 24);
						fd_cur_cfg[POS_FDCON_IN_BA_MSB]
							= (u32)(fd_cur_cfg[POS_FDCON_IN_BA_MSB]
								& filter);
					}
					fd_cur_cfg[FD_IN_0 + j] = (u32)(
						fd->dma_para
							->fd_out_hw_pa[uloop]
								      [uch]);
				}
			}
		}

		/* OUT_FM_BASE_ADR */
		for (j = 0; j < output_WDMA_WRA_num; j++) {
			if (fd_wdma_en[i][j]) {
				if (j == 0) {
					msb_bit_0 = (fd->dma_para->fd_out_hw_pa[i][j] &
								0xf00000000) >> 32;
					filter = 0xfffffffc | msb_bit_0;
					fd_cur_cfg[POS_FDCON_OUT_BA_MSB] =
						(u32)(fd_cur_cfg[POS_FDCON_OUT_BA_MSB] & filter);
				} else if (j == 1) {
					msb_bit_1 = (fd->dma_para->fd_out_hw_pa[i][j] &
								0xf00000000) >> 32;
					filter = 0xfffffcff | (msb_bit_1 << 8);
					fd_cur_cfg[POS_FDCON_OUT_BA_MSB] =
						(u32)(fd_cur_cfg[POS_FDCON_OUT_BA_MSB] & filter);
				} else if (j == 2) {
					msb_bit_2 = (fd->dma_para->fd_out_hw_pa[i][j] &
								0xf00000000) >> 32;
					filter = 0xfffcffff | (msb_bit_2 << 16);
					fd_cur_cfg[POS_FDCON_OUT_BA_MSB] =
						(u32)(fd_cur_cfg[POS_FDCON_OUT_BA_MSB] & filter);
				} else if (j == 3) {
					msb_bit_3 = (fd->dma_para->fd_out_hw_pa[i][j] &
								0xf00000000) >> 32;
					filter = 0xfcffffff | (msb_bit_3 << 24);
					fd_cur_cfg[POS_FDCON_OUT_BA_MSB] =
						(u32)(fd_cur_cfg[POS_FDCON_OUT_BA_MSB] & filter);
				}

				fd_cur_cfg[FD_OUT_0 + j] =
					(u32)(fd->dma_para->fd_out_hw_pa[i][j]);
			}
		}

		/* KERNEL_BASE_ADR */
		for (j = 0; j < kernel_RDMA_RA_num; j++) {
			if (fd_ker_rdma_size[i][j]) {
				if (j == 0) {
					msb_bit_0 = (fd->dma_para->fd_kernel_pa[i][j] &
								0xf00000000) >> 32;
					filter = 0xfffffffc | msb_bit_0;
					fd_cur_cfg[POS_FDCON_KERNEL_BA_MSB] =
						(u32)(fd_cur_cfg[POS_FDCON_KERNEL_BA_MSB] & filter);

				} else if (j == 1) {
					msb_bit_1 = (fd->dma_para->fd_kernel_pa[i][j] &
								0xf00000000) >> 32;
					filter = 0xfffffcff | (msb_bit_1 << 8);
					fd_cur_cfg[POS_FDCON_KERNEL_BA_MSB] =
						(u32)(fd_cur_cfg[POS_FDCON_KERNEL_BA_MSB] & filter);
				}

				fd_cur_cfg[FD_KERNEL_0 + j] =
					(u32)(fd->dma_para->fd_kernel_pa[i][j]);
			}
		}
	}

	return 0;
}

static int aie_config_attr_network(struct mtk_aie_dev *fd,
				   struct aie_enq_info *aie_cfg)
{
	bool isRegressionLoop = false;
	void *fd_cfg;
	u32 *fd_cur_cfg;
	u16 fd_input_ht, fd_output_ht = 0x0;
	u16 fd_out_y[4];
	u8 i, j;
	u8 uloop, uch, uidx;
	u16 pyramid0_out_w, pyramid0_out_h;
	int fd_conv_ht;
	u16 src_crop_w;
	u16 src_crop_h;
	int msb_bit_0 = 0, msb_bit_1 = 0, msb_bit_2 = 0, msb_bit_3 = 0;

	src_crop_w = fd->attr_para->crop_width[fd->attr_para->w_idx];
	src_crop_h = fd->attr_para->crop_height[fd->attr_para->w_idx];

	pyramid0_out_w = ATTR_MODE_PYRAMID_WIDTH;
	if (src_crop_w == 0) {
		dev_info(fd->dev, "src_crop_w = 0: cannot by divide");
		return -1;
	}
	pyramid0_out_h = pyramid0_out_w * src_crop_h / src_crop_w;

	fd_cfg = fd->base_para->attr_fd_cfg_va[fd->attr_para->w_idx];

	for (i = 0; i < attr_loop_num; i++) {
		fd_cur_cfg = (u32 *)fd_cfg + FD_CONFIG_SIZE * i;
		fd_cur_cfg[FD_INPUT_ROTATE] =
			(fd_cur_cfg[FD_INPUT_ROTATE] & 0xFFFF0FFF) |
			((aie_cfg->rotate_degree << 12) & 0x3000);

		if (i == 0) {
			fd_input_ht = pyramid0_out_h;
		} else {
			if (attr_out_stride2_as_in[i] == 0)
				fd_input_ht = fd_output_ht;
			else if (attr_out_stride2_as_in[i] == 1)
				fd_input_ht = (fd_output_ht + 1) / 2;
		}
		fd_output_ht = DIV_ROUND_UP(fd_input_ht,
					    attr_fd_stride[i] +
						    2 * attr_fd_maxpool[i]);
		fd_conv_ht = DIV_ROUND_UP(fd_input_ht, attr_fd_stride[i]);

		fd_cur_cfg[FD_CONV_IMG_W_H] =
			(fd_cur_cfg[FD_CONV_IMG_W_H] & 0xFFFF0000) |
			(fd_conv_ht & 0xFFFF);
		fd_cur_cfg[FD_IN_IMG_W_H] =
			(fd_cur_cfg[FD_IN_IMG_W_H] & 0xFFFF0000) |
			(fd_input_ht & 0xFFFF);
		fd_cur_cfg[FD_OUT_IMG_W_H] =
			(fd_cur_cfg[FD_OUT_IMG_W_H] & 0xFFFF0000) |
			(fd_output_ht & 0xFFFF);
		fd_cur_cfg[FD_IN_X_Y_SIZE0] = aie_combine_u16(
			fd_cur_cfg[FD_IN_X_Y_SIZE0], fd_input_ht - 1);
		fd_cur_cfg[FD_IN_X_Y_SIZE1] = aie_combine_u16(
			fd_cur_cfg[FD_IN_X_Y_SIZE1], fd_input_ht - 1);
		fd_cur_cfg[FD_IN_X_Y_SIZE2] = aie_combine_u16(
			fd_cur_cfg[FD_IN_X_Y_SIZE2], fd_input_ht - 1);
		fd_cur_cfg[FD_IN_X_Y_SIZE3] = aie_combine_u16(
			fd_cur_cfg[FD_IN_X_Y_SIZE3], fd_input_ht - 1);

		isRegressionLoop = (i == age_out_rgs || i == gender_out_rgs ||
				    i == indian_out_rgs || i == race_out_rgs);

		if (isRegressionLoop) {
			fd_out_y[0] = 0;
			fd_out_y[1] = 0;
			fd_out_y[2] = 0;
			fd_out_y[3] = 0;
		} else {
			fd_out_y[0] = fd_output_ht - 1;
			fd_out_y[1] = fd_output_ht - 1;
			if (attr_out_2size[i] == 0) {
				fd_out_y[2] = fd_output_ht - 1;
				fd_out_y[3] = fd_output_ht - 1;
			} else {
				fd_out_y[2] = (fd_output_ht + 1) / 2 - 1;
				fd_out_y[3] = (fd_output_ht + 1) / 2 - 1;
			}
		}

		for (j = 0; j < 4; j++)
			fd_cur_cfg[FD_OUT_X_Y_SIZE0 + 2 * j] = aie_combine_u16(
				fd_cur_cfg[FD_OUT_X_Y_SIZE0 + 2 * j],
				fd_out_y[j]);

		/* IN_FM_BASE_ADR */
		if (i == 0) {
			msb_bit_0 = (fd->base_para->rs_pym_rst_pa[0][0] &
								0xf00000000) >> 32;
			msb_bit_1 = (fd->base_para->rs_pym_rst_pa[0][1] &
								0xf00000000) >> 32;
			msb_bit_2 = (fd->base_para->rs_pym_rst_pa[0][2] &
								0xf00000000) >> 32;

			fd_cur_cfg[POS_FDCON_IN_BA_MSB] = (u32)(msb_bit_0 |
							msb_bit_1 << 8 |
							msb_bit_2 << 16);
			fd_cur_cfg[FD_IN_0] =
				(u32)(fd->base_para->rs_pym_rst_pa[0][0]);
			fd_cur_cfg[FD_IN_1] =
				(u32)(fd->base_para->rs_pym_rst_pa[0][1]);
			fd_cur_cfg[FD_IN_2] =
				(u32)(fd->base_para->rs_pym_rst_pa[0][2]);
		} else {
			for (j = 0; j < input_WDMA_WRA_num; j++) {

				if (attr_rdma_en[i][j][0] != -1) {
					uloop = attr_rdma_en[i][j][0];
					uch = attr_rdma_en[i][j][1];
					if (j == 0) {
						msb_bit_0 =
						(fd->dma_para->attr_out_hw_pa[uloop][uch] &
								0xf00000000) >> 32;
						fd_cur_cfg[POS_FDCON_IN_BA_MSB] |= (u32)(msb_bit_0);
					} else if (j == 1) {
						msb_bit_1 =
						(fd->dma_para->attr_out_hw_pa[uloop][uch] &
								0xf00000000) >> 32;
						fd_cur_cfg[POS_FDCON_IN_BA_MSB] |=
								(u32)(msb_bit_1 << 8);
					} else if (j == 2) {
						msb_bit_2 =
						(fd->dma_para->attr_out_hw_pa[uloop][uch] &
								0xf00000000) >> 32;
						fd_cur_cfg[POS_FDCON_IN_BA_MSB] |=
								(u32)(msb_bit_2 << 16);
					} else if (j == 3) {
						msb_bit_3 =
						(fd->dma_para->attr_out_hw_pa[uloop][uch] &
								0xf00000000) >> 32;
						fd_cur_cfg[POS_FDCON_IN_BA_MSB] |=
								(u32)(msb_bit_3 << 24);
					}

					fd_cur_cfg[FD_IN_0 + j] = (u32)(
						fd->dma_para
							->attr_out_hw_pa[uloop]
									[uch]);
				}
			}
		}

		/* OUT_FM_BASE_ADR */
		for (j = 0; j < output_WDMA_WRA_num; j++) {
			if (attr_wdma_en[i][j]) {
				uidx = fd->attr_para->w_idx;
				if (i == age_out_rgs && j == 0) {
					msb_bit_0 = (fd->dma_para->age_out_hw_pa[uidx] &
									0xf00000000) >> 32;
					fd_cur_cfg[POS_FDCON_OUT_BA_MSB] |= (u32)msb_bit_0;
					fd_cur_cfg[FD_OUT_0 + j] = (u32)(
						fd->dma_para
							->age_out_hw_pa[uidx]);
				} else if (i == gender_out_rgs && j == 0) {
					msb_bit_0 = (fd->dma_para->gender_out_hw_pa[uidx] &
									0xf00000000) >> 32;
					fd_cur_cfg[POS_FDCON_OUT_BA_MSB] |= (u32)msb_bit_0;
					fd_cur_cfg[FD_OUT_0 + j] = (u32)(
						fd->dma_para->gender_out_hw_pa
							[uidx]);
				} else if (i == indian_out_rgs && j == 0) {
					msb_bit_0 = (fd->dma_para->isIndian_out_hw_pa[uidx] &
									0xf00000000) >> 32;
					fd_cur_cfg[POS_FDCON_OUT_BA_MSB] |= (u32)msb_bit_0;

					fd_cur_cfg[FD_OUT_0 + j] = (u32)(
						fd->dma_para->isIndian_out_hw_pa
							[uidx]);
				} else if (i == race_out_rgs && j == 0) {
					msb_bit_0 = (fd->dma_para->race_out_hw_pa[uidx] &
									0xf00000000) >> 32;
					fd_cur_cfg[POS_FDCON_OUT_BA_MSB] |= (u32)msb_bit_0;
					fd_cur_cfg[FD_OUT_0 + j] = (u32)(
						fd->dma_para
							->race_out_hw_pa[uidx]);
				} else {
					if (j == 0) {
						msb_bit_0 = (fd->dma_para->attr_out_hw_pa[i][j] &
									0xf00000000) >> 32;
						fd_cur_cfg[POS_FDCON_OUT_BA_MSB] |=
									(u32)(msb_bit_0);
					} else if (j == 1) {
						msb_bit_1 = (fd->dma_para->attr_out_hw_pa[i][j] &
									0xf00000000) >> 32;
						fd_cur_cfg[POS_FDCON_OUT_BA_MSB] |=
									(u32)(msb_bit_1 << 8);
					} else if (j == 2) {
						msb_bit_2 = (fd->dma_para->attr_out_hw_pa[i][j] &
									0xf00000000) >> 32;
						fd_cur_cfg[POS_FDCON_OUT_BA_MSB] |=
									(u32)(msb_bit_2 << 16);
					} else if (j == 3) {
						msb_bit_3 = (fd->dma_para->attr_out_hw_pa[i][j] &
									0xf00000000) >> 32;
						fd_cur_cfg[POS_FDCON_OUT_BA_MSB] |=
									(u32)(msb_bit_3 << 24);
					}

					fd_cur_cfg[FD_OUT_0 + j] = (u32)(
						fd->dma_para
							->attr_out_hw_pa[i][j]);
			}
		}
		}

		/* KERNEL_BASE_ADR */
		for (j = 0; j < kernel_RDMA_RA_num; j++) {
			if (j == 0) {
				msb_bit_0 = (fd->dma_para->attr_kernel_pa[i][j] &
								0xf00000000) >> 32;
				fd_cur_cfg[POS_FDCON_KERNEL_BA_MSB] |= (u32)(msb_bit_0);
			} else if (j == 1) {
				msb_bit_1 = (fd->dma_para->attr_kernel_pa[i][j] &
								0xf00000000) >> 32;
				fd_cur_cfg[POS_FDCON_KERNEL_BA_MSB] |= (u32)(msb_bit_1 << 8);
			}
			fd_cur_cfg[FD_KERNEL_0 + j] =
				(u32)(fd->dma_para->attr_kernel_pa[i][j]);
		}
	}
	return 0;
}

static int aie_config_dram(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg)
{
	int ret = 0;

	if (aie_cfg->sel_mode == 0) { /* FDMODE */
		ret = aie_config_y2r(fd, aie_cfg, aie_cfg->sel_mode);
		if (ret)
			return ret;

		ret = aie_config_rs(fd, aie_cfg);
		if (ret)
			return ret;

		ret = aie_config_network(fd, aie_cfg);
		if (ret)
			return ret;

	} else if (aie_cfg->sel_mode == 1) { /* ATTRIBUTEMODE */
		ret = aie_config_y2r(fd, aie_cfg, aie_cfg->sel_mode);
		if (ret)
			return ret;

		ret = aie_config_attr_network(fd, aie_cfg);
		if (ret)
			return ret;
	}

	return ret;
}

void aie_reset(struct mtk_aie_dev *fd)
{
	writel(0x30000, fd->fd_base + AIE_START_REG);
	writel(0x0, fd->fd_base + AIE_START_REG);
}

int aie_alloc_aie_buf(struct mtk_aie_dev *fd)
{
	int ret = -ENOMEM;
	int err_tag = 0;

	memset(&fd->st_info, 0, sizeof(fd->st_info));
	aie_init_table(fd, fd->base_para->max_pyramid_width,
		       fd->base_para->max_pyramid_height);
	aie_get_data_size(fd, fd->base_para->max_img_width,
				      fd->base_para->max_img_height);
	ret = aie_alloc_dram_buf(fd); //config
	if (ret)
		goto dram_fail;

	ret = aie_alloc_output_buf(fd); //pyramid
	if (ret)
		goto output_fail;

	ret = aie_alloc_fddma_buf(fd); //inter-production
	if (ret)
		goto fddma_fail;
#ifdef FLD
	ret = aie_alloc_fld_buf(fd);
	if (ret)
		goto fld_fail;
#endif

	dev_info(fd->dev,
	"c(%llx/%llx/%llx)o(%llx/%llx/%llx/%llx/%llx)f(%llx/%llx/%llx/%llx/%llx/%llx/%llx)\n",
		fd->rs_cfg_data.pa, fd->fd_cfg_data.pa, fd->yuv2rgb_cfg_data.pa,
		fd->rs_output_hw.pa, fd->fd_dma_hw.pa, fd->fd_dma_result_hw.pa,
		fd->fd_kernel_hw.pa, fd->fd_attr_dma_hw.pa, fd->fld_cv_hw.pa,
		fd->fld_fp_hw.pa, fd->fld_leafnode_hw.pa, fd->fld_tree_02_hw.pa,
		fd->fld_tree_13_hw.pa, fd->fld_blink_weight_hw.pa, fd->fld_output_hw.pa
	);
	aie_arrange_fddma_buf(fd);
	aie_arrange_kernel_buf(fd);
	aie_arrange_attrdma_buf(fd);
	aie_arrange_result_dma_buf(fd);
#ifdef FLD
	aie_arrange_fld_buf(fd);
#endif
	ret = aie_load_fw(fd);
	if (ret)
		goto load_fw_fail;

	return ret;

load_fw_fail:
	aie_free_fddma_buf(fd);
	err_tag++;
#ifdef FLD
fld_fail:
	aie_free_fld_buf(fd);
	err_tag++;
#endif
fddma_fail:
	aie_free_output_buf(fd);
	err_tag++;

output_fail:
	aie_free_dram_buf(fd);
	err_tag++;

dram_fail:
	kfree(fd->dma_para);
	fd->dma_para = NULL;
	err_tag++;

	dev_info(fd->dev, "Failed to alloc aie buf: %d\n", err_tag);
	return ret;


}

int aie_init(struct mtk_aie_dev *fd)
{
	int err_tag = 0;

	fd->fd_state = STATE_NA;

	writel(0x00400020, fd->fd_base + FDVT_RDA_0_CON3_REG);
	writel(0x00400020, fd->fd_base + FDVT_RDA_1_CON3_REG);

	writel(0x00400020, fd->fd_base + FDVT_RDB_0_CON3_REG);
	writel(0x00400020, fd->fd_base + FDVT_RDB_1_CON3_REG);

	writel(0x00400020, fd->fd_base + FDVT_WRA_0_CON3_REG);
	writel(0x00400020, fd->fd_base + FDVT_WRA_1_CON3_REG);

	writel(0x00400020, fd->fd_base + FDVT_WRB_0_CON3_REG);
	writel(0x00400020, fd->fd_base + FDVT_WRB_0_CON3_REG);

#if CHECK_SERVICE_IF_0
	mtk_iommu_register_fault_callback(M4U_PORT_L12_IPE_FDVT_2ND_RDA0,
		(mtk_iommu_fault_callback_t)FDVT_M4U_TranslationFault_callback,
		NULL, false);
	mtk_iommu_register_fault_callback(M4U_PORT_L12_IPE_FDVT_2ND_RDB0,
		(mtk_iommu_fault_callback_t)FDVT_M4U_TranslationFault_callback,
		NULL, false);
	mtk_iommu_register_fault_callback(M4U_PORT_L12_IPE_FDVT_2ND_WRA0,
		(mtk_iommu_fault_callback_t)FDVT_M4U_TranslationFault_callback,
		NULL, false);
	mtk_iommu_register_fault_callback(M4U_PORT_L12_IPE_FDVT_2ND_WRB0,
		(mtk_iommu_fault_callback_t)FDVT_M4U_TranslationFault_callback,
		NULL, false);
#endif
	fd->base_para = kmalloc(sizeof(struct aie_para), GFP_KERNEL);
	if (fd->base_para == NULL)
		return -ENOMEM;

	fd->attr_para = kmalloc(sizeof(struct aie_attr_para), GFP_KERNEL);
	if (fd->attr_para == NULL)
		goto attr_para_fail;
#ifdef FLD
	fd->fld_para = kmalloc(sizeof(struct aie_fld_para), GFP_KERNEL);
	if (fd->fld_para == NULL)
		goto fld_para_fail;
#endif
	fd->dma_para = kmalloc(sizeof(struct aie_fd_dma_para), GFP_KERNEL);
	if (fd->dma_para == NULL)
		goto dma_para_fail;

	fd->attr_para->r_idx = 0;
	fd->attr_para->w_idx = 0;

	fd->fd_state = STATE_INIT;

	return 0;

dma_para_fail:
	kfree(fd->attr_para);
	fd->attr_para = NULL;
	err_tag++;
#ifdef FLD
fld_para_fail:
	kfree(fd->fld_para);
	fd->fld_para = NULL;
	err_tag++;
#endif
attr_para_fail:
	kfree(fd->base_para);
	fd->base_para = NULL;
	err_tag++;

	dev_info(fd->dev, "Failed to init aie: %d\n", err_tag);

	return -ENOMEM;
}

void aie_uninit(struct mtk_aie_dev *fd)
{
	fd->fd_state = STATE_NA;

	aie_free_dram_buf(fd);
	aie_free_fddma_buf(fd);
#ifdef FLD
	aie_free_fld_buf(fd);
#endif
	if (g_user_param.is_secure)
		aie_free_sec_buf(fd);
	else
		aie_free_output_buf(fd);

	if (fd->base_para != NULL) {
		kfree(fd->base_para);
		fd->base_para = NULL;
	}
	if (fd->attr_para != NULL) {
		kfree(fd->attr_para);
		fd->attr_para = NULL;
	}
	if (fd->dma_para != NULL) {
		kfree(fd->dma_para);
		fd->dma_para = NULL;
	}
#ifdef FLD
	if (fd->fld_para != NULL) {
		kfree(fd->fld_para);
		fd->fld_para = NULL;
	}
#endif
}

int aie_prepare(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg)
{
	int ret = 0;

	if (fd->fd_state != STATE_INIT) {
		dev_info(fd->dev, "%s fd state fail: %d\n",
			 __func__, fd->fd_state);
		return -EINVAL;
	}

	memset(&fd->reg_cfg, 0, sizeof(fd->reg_cfg));

	if (aie_cfg->pyramid_base_width == 0) {
		fd->base_para->pyramid_width =
			fd->base_para->max_pyramid_width;
		fd->base_para->pyramid_height =
			fd->base_para->max_pyramid_height;
		fd->base_para->number_of_pyramid = 3;
	} else {
		if (aie_cfg->pyramid_base_width >
			fd->base_para->max_pyramid_width ||
		    aie_cfg->pyramid_base_height >
			fd->base_para->max_pyramid_height ||
		    aie_cfg->number_of_pyramid > 3 ||
		    aie_cfg->number_of_pyramid <= 0) {
			dev_info(fd->dev, "err: base w: %d, h: %d, num: %d\n",
			    aie_cfg->pyramid_base_width,
			    aie_cfg->pyramid_base_height,
			    aie_cfg->number_of_pyramid);
			dev_info(fd->dev, "err: max w: %d, h: %d\n",
			    fd->base_para->max_pyramid_width,
			    fd->base_para->max_pyramid_height);

			return -EINVAL;
		}

		fd->base_para->pyramid_height =
			fd->base_para->max_pyramid_height;
		fd->base_para->number_of_pyramid =
			aie_cfg->number_of_pyramid;
		if (aie_cfg->pyramid_base_width !=
			fd->base_para->pyramid_width) {
			dev_dbg(fd->dev, "pre: %d, cur: %d, num: %d\n",
				fd->base_para->pyramid_width,
				aie_cfg->pyramid_base_width,
				fd->base_para->number_of_pyramid);
			fd->base_para->pyramid_width =
				aie_cfg->pyramid_base_width;
			aie_update_table(
				fd, fd->base_para->pyramid_width,
				fd->base_para->pyramid_height);
			aie_update_fddma_buf(fd);
		}
	}

	if ((aie_cfg->src_img_width > fd->base_para->max_img_width) ||
	    (aie_cfg->src_img_height > fd->base_para->max_img_height)) {
		dev_info(
			fd->dev,
			"AIE error: Enque Size error, Src_WD: %d, Src_HT: %d\n",
			aie_cfg->src_img_width, aie_cfg->src_img_height);

		dev_info(fd->dev, "AIE error: MAX_Src_WD: %d, MAX_Src_HT: %d\n",
			 fd->base_para->max_img_width,
			 fd->base_para->max_img_height);
		return -EINVAL;
	}

	//aie_reset_output_buf(fd, aie_cfg);

	fd->reg_cfg.fd_mode = aie_cfg->sel_mode;
	if (aie_cfg->sel_mode == 0) { /* FDMODE */
		fd->reg_cfg.rs_adr = (u32)fd->base_para->fd_rs_cfg_pa;
		fd->reg_cfg.yuv2rgb_adr = (u32)fd->base_para->fd_yuv2rgb_cfg_pa;
		fd->reg_cfg.fd_adr = (u32)fd->base_para->fd_fd_cfg_pa +
			FD_CONFIG_SIZE * 4 * fd_loop_num /
			3 * (3 - aie_cfg->number_of_pyramid);

	} else if (aie_cfg->sel_mode == 1) { /* ATTRMODE */
		fd->reg_cfg.yuv2rgb_adr =
			(u32)fd->base_para
				->attr_yuv2rgb_cfg_pa[fd->attr_para->w_idx];
		fd->reg_cfg.fd_adr =
			(u32)fd->base_para
				->attr_fd_cfg_pa[fd->attr_para->w_idx];
	} else {
		dev_info(fd->dev, "AIE error, Mode: %d", aie_cfg->sel_mode);
		return -EINVAL;
	}

	ret = aie_update_cfg(fd, aie_cfg);
	if (ret)
		return ret;

	ret = aie_config_dram(fd, aie_cfg);
	if (ret)
		return ret;

	if (aie_cfg->sel_mode == 1) { /* ATTRMODE */
		fd->attr_para->w_idx =
			(fd->attr_para->w_idx + 1) % MAX_ENQUE_FRAME_NUM;
	}

	return ret;
}

#ifdef FDVT_USE_GCE
static void AIECmdqCB(struct cmdq_cb_data data)
{
	struct mtk_aie_dev *fd = (struct mtk_aie_dev *)data.data;

	queue_work(fd->frame_done_wq, &fd->req_work.work);
}

static void AIECmdqSecCB(struct cmdq_cb_data data)
{
	struct mtk_aie_dev *fd = (struct mtk_aie_dev *)data.data;

	dev_info(fd->dev, "AIE SEC CMDQ CB\n");
}

static void AieSecPktCB(struct cmdq_cb_data data)
{
	struct cmdq_pkt *sec_pkt = (struct cmdq_pkt *)data.data;

	cmdq_pkt_destroy(sec_pkt);
	g_sec_pkt = NULL;

}

void config_aie_cmdq_secure_init(struct mtk_aie_dev *fd)
{
	g_sec_pkt = cmdq_pkt_create(fd->fdvt_secure_clt);

	cmdq_sec_pkt_set_data(g_sec_pkt, 0, 0, CMDQ_SEC_DEBUG, CMDQ_METAEX_TZMP);
	cmdq_sec_pkt_set_mtee(g_sec_pkt, true);
	cmdq_pkt_finalize_loop(g_sec_pkt);
	cmdq_pkt_flush_threaded(g_sec_pkt, AieSecPktCB, (void *)g_sec_pkt);
}

void aie_enable_secure_domain(struct mtk_aie_dev *fd)
{
	struct cmdq_pkt *pkt = NULL;

	pkt = cmdq_pkt_create(fd->fdvt_clt);
	cmdq_pkt_set_event(pkt, fd->fdvt_sec_wait);
	cmdq_pkt_wfe(pkt, fd->fdvt_sec_set);
	cmdq_pkt_flush_async(pkt, AIECmdqSecCB, (void *)fd);	/* flush and destry in cmdq*/
	cmdq_pkt_wait_complete(pkt);
	cmdq_pkt_destroy(pkt);
}

void aie_disable_secure_domain(struct mtk_aie_dev *fd)
{
	struct cmdq_pkt *pkt = NULL;

	pkt = cmdq_pkt_create(fd->fdvt_clt);
	cmdq_pkt_set_event(pkt, fd->fdvt_sec_wait);
	cmdq_pkt_wfe(pkt, fd->fdvt_sec_set);
	cmdq_pkt_flush_async(pkt, AIECmdqSecCB, (void *)fd);/* flush and destry in cmdq*/
	cmdq_pkt_wait_complete(pkt);
	cmdq_pkt_destroy(pkt);
}

void config_aie_cmdq_hw(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg)
{
	struct cmdq_pkt *pkt = NULL;
	unsigned int loop_num = 0;
	unsigned int loop_reg_val = 0;

	pkt = cmdq_pkt_create(fd->fdvt_clt);
	/*for early porting*/
	if (aie_cfg->sel_mode == 0) {
		cmdq_pkt_write(pkt, NULL, FDVT_ENABLE_HW, 0x00000111,
			CMDQ_REG_MASK);
		loop_num = fd_loop_num / 3 * (aie_cfg->number_of_pyramid);
		loop_reg_val = (loop_num << 8) |
			(aie_cfg->number_of_pyramid - 1);
		cmdq_pkt_write(pkt, NULL, FDVT_LOOP_HW, loop_reg_val, CMDQ_REG_MASK);

		cmdq_pkt_write(pkt, NULL, FDVT_INT_EN_HW, 0x1, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_RS_CON_BASE_ADR_HW,
				fd->reg_cfg.rs_adr, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_FD_CON_BASE_ADR_HW,
				fd->reg_cfg.fd_adr, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_YUV2RGB_CON_BASE_ADR_HW,
				fd->reg_cfg.yuv2rgb_adr, CMDQ_REG_MASK);

		cmdq_pkt_write(pkt, NULL, FDVT_START_HW, 0x1, CMDQ_REG_MASK);

		cmdq_pkt_wfe(pkt, fd->fdvt_event_id);
		/*cmdqRecWait(handle, CMDQ_EVENT_IPE_EVENT_TX_FRAME_DONE_0);*/
		cmdq_pkt_write(pkt, NULL, FDVT_START_HW, 0x0, CMDQ_REG_MASK);

	} else if (aie_cfg->sel_mode == 1) {
		cmdq_pkt_write(pkt, NULL, FDVT_ENABLE_HW, 0x00000101,
			       CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_LOOP_HW, 0x00001A00,
			       CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_INT_EN_HW, 0x1, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_RS_CON_BASE_ADR_HW,
			       fd->reg_cfg.rs_adr,
			       CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_FD_CON_BASE_ADR_HW,
			       fd->reg_cfg.fd_adr,
			       CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_YUV2RGB_CON_BASE_ADR_HW,
			       fd->reg_cfg.yuv2rgb_adr,
			       CMDQ_REG_MASK);

		cmdq_pkt_write(pkt, NULL, FDVT_START_HW, 0x1, CMDQ_REG_MASK);

		cmdq_pkt_wfe(pkt, fd->fdvt_event_id);
		/*cmdqRecWait(handle, CMDQ_EVENT_IPE_EVENT_TX_FRAME_DONE_0);*/
		cmdq_pkt_write(pkt, NULL, FDVT_START_HW, 0x0, CMDQ_REG_MASK);

	} else if (aie_cfg->sel_mode == 3) {
		int i = 0;
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + AIE_START_REG, 0x10, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_DMA_CTL_HW, 0x00011111, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_EN, 0x01111111, CMDQ_REG_MASK);

		for (i = 0; i < aie_cfg->fld_face_num; i++) {
			cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_BASE_ADDR_FACE_0 + i * 0x4,
					aie_cfg->src_img_addr, CMDQ_REG_MASK);
			cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + fld_face_info_0[i],
					(aie_cfg->fld_input[i].fld_in_crop.x1 << 16) |
					aie_cfg->fld_input[i].fld_in_crop.y1, CMDQ_REG_MASK);
			cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + fld_face_info_1[i],
					(aie_cfg->fld_input[i].fld_in_crop.x2 << 16) |
					aie_cfg->fld_input[i].fld_in_crop.y2, CMDQ_REG_MASK);
			cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + fld_face_info_2[i],
					(aie_cfg->fld_input[i].fld_in_rip << 4) |
					aie_cfg->fld_input[i].fld_in_rop, CMDQ_REG_MASK);
		}


		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_MODEL_PARA1,
				(fld_forest << 16) | (aie_cfg->fld_face_num << 28) | fld_point,
				CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_MODEL_PARA14,
				(0xd << 16) | 0xfe9, CMDQ_REG_MASK);

		/*fld kernel model pa setting*/
		for (i = 0; i < FLD_MAX_INPUT; i++) {
			cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + fld_pl_in_addr_0[i],
						fd->dma_para->fld_tree02_pa[i], CMDQ_REG_MASK);
			cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + fld_pl_in_addr_1[i],
						fd->dma_para->fld_tree13_pa[i], CMDQ_REG_MASK);
			cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + fld_pl_in_addr_2[i],
						fd->dma_para->fld_cv_pa[i], CMDQ_REG_MASK);
			cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + fld_pl_in_addr_3[i],
						fd->dma_para->fld_fp_pa[i], CMDQ_REG_MASK);
			cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + fld_sh_in_addr[i],
						fd->dma_para->fld_leafnode_pa[i], CMDQ_REG_MASK);
	}
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_BS_IN_BASE_ADDR_14,
				fd->dma_para->fld_blink_weight_pa, CMDQ_REG_MASK);

		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_SRC_WD_HT,
				(aie_cfg->src_img_width << 16) | aie_cfg->src_img_height,
				CMDQ_REG_MASK);

		/*input settings*/
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_PL_IN_SIZE_0,
				0x007c003f, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_PL_IN_STRIDE_0,
				0x0040000f, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_PL_IN_SIZE_1,
				0x007c003f, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_PL_IN_STRIDE_1,
				0x0040000f, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_PL_IN_SIZE_2_0,
				0x0016003f, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_PL_IN_STRIDE_2_0,
				0x0040000f, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + + FLD_PL_IN_SIZE_2_1,
				0x0013003f, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + + FLD_PL_IN_STRIDE_2_1,
				0x0040000f, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_PL_IN_SIZE_2_2,
				0x0013003f, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_PL_IN_STRIDE_2_2,
				0x0040000f, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_PL_IN_SIZE_3,
				0x00a6001f, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_PL_IN_STRIDE_3,
				0x0020000f, CMDQ_REG_MASK);

		/*output setting*/
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_SH_IN_SIZE_0,
					((2400 * aie_cfg->fld_face_num - 1) << 16) | 127,
					CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_SH_IN_STRIDE_0, 0x0010000f,
					CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_TR_OUT_BASE_ADDR_0,
					fd->dma_para->fld_output_pa, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_TR_OUT_SIZE_0,
					((aie_cfg->fld_face_num-1) << 16) | 0x6f, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_TR_OUT_STRIDE_0,
					0x0070000f, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_PP_OUT_BASE_ADDR_0,
					fd->dma_para->fld_output_pa, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_PP_OUT_SIZE_0,
					((aie_cfg->fld_face_num-1) << 16) | 0x6f, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_PP_OUT_STRIDE_0,
					0x0070000f, CMDQ_REG_MASK);

		/*cv score*/
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_BS_BIAS, 0x00000001, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_CV_FM_RANGE_0,
				0x0000b835, CMDQ_REG_MASK); //8E8
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_CV_FM_RANGE_1,
				0xffff5cba, CMDQ_REG_MASK); //8EC
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_CV_PM_RANGE_0,
				0x00005ed5, CMDQ_REG_MASK); //8F0
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_CV_PM_RANGE_1,
				0xffff910d, CMDQ_REG_MASK); //8F4 //TEMP
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_BS_RANGE_0,
				0x0000031e, CMDQ_REG_MASK); //8F8
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + FLD_BS_RANGE_1,
				0xfffffcae, CMDQ_REG_MASK); //8FC

		/*fld mode + trigger start*/
		cmdq_pkt_write(pkt, NULL, FDVT_BASE_HW + AIE_START_REG, 0x11, CMDQ_REG_MASK);

		cmdq_pkt_wfe(pkt, fd->fdvt_event_id);
		/*cmdqRecWait(handle, CMDQ_EVENT_IPE_EVENT_TX_FRAME_DONE_0);*/
		cmdq_pkt_write(pkt, NULL, FDVT_START_HW, 0x0, CMDQ_REG_MASK);
	}

	//cmdq_pkt_flush(pkt);
	cmdq_pkt_flush_async(pkt, AIECmdqCB, (void *)fd);	/* flush and destry in cmdq*/
	cmdq_pkt_wait_complete(pkt);
	/* release resource */
	cmdq_pkt_destroy(pkt);
}
#endif
void aie_execute(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg)
{
#ifndef FDVT_USE_GCE

	unsigned int loop_num = 0;
	unsigned int loop_reg_val = 0;

	if (aie_cfg->sel_mode == 0) {
		writel(0x00000111, fd->fd_base + AIE_ENABLE_REG);
		loop_num = fd_loop_num / 3 * (aie_cfg->number_of_pyramid);
		loop_reg_val = (loop_num << 8) |
			(aie_cfg->number_of_pyramid - 1);
		writel(loop_reg_val, fd->fd_base + AIE_LOOP_REG);
		writel(0x1, fd->fd_base + AIE_INT_EN_REG);
		writel(fd->reg_cfg.rs_adr,
			fd->fd_base + AIE_RS_CON_BASE_ADR_REG);
		writel(fd->reg_cfg.fd_adr,
			fd->fd_base + AIE_FD_CON_BASE_ADR_REG);
		writel(fd->reg_cfg.yuv2rgb_adr,
			fd->fd_base + AIE_YUV2RGB_CON_BASE_ADR_REG);
		writel(0x1, fd->fd_base + AIE_START_REG);
	} else if (aie_cfg->sel_mode == 1) {
		writel(0x00000101, fd->fd_base + AIE_ENABLE_REG);
		writel(0x00001A00, fd->fd_base + AIE_LOOP_REG);
		writel(0x1, fd->fd_base + AIE_INT_EN_REG);
		writel(fd->reg_cfg.rs_adr,
		       fd->fd_base + AIE_RS_CON_BASE_ADR_REG);
		writel(fd->reg_cfg.fd_adr,
		       fd->fd_base + AIE_FD_CON_BASE_ADR_REG);
		writel(fd->reg_cfg.yuv2rgb_adr,
		       fd->fd_base + AIE_YUV2RGB_CON_BASE_ADR_REG);
		writel(0x1, fd->fd_base + AIE_START_REG);
	} else if (aie_cfg->sel_mode == 3) {
		int i = 0;

		writel(0x10, fd->fd_base + AIE_START_REG);
		writel(0x00011111, fd->fd_base + AIE_DMA_CTL_REG);
		writel(0x01111111, fd->fd_base + FLD_EN);
		for (i = 0; i < aie_cfg->fld_face_num; i++) {
			writel(aie_cfg->src_img_addr, fd->fd_base + FLD_BASE_ADDR_FACE_0 + i * 0x4);
			writel((aie_cfg->fld_input[i].fld_in_crop.x1 << 16) |
			aie_cfg->fld_input[i].fld_in_crop.y1,
						fd->fd_base + fld_face_info_0[i]);
			writel((aie_cfg->fld_input[i].fld_in_crop.x2 << 16) |
						aie_cfg->fld_input[i].fld_in_crop.y2,
						fd->fd_base + fld_face_info_1[i]);
			writel(aie_cfg->fld_input[i].fld_in_rip << 4 |
						aie_cfg->fld_input[i].fld_in_rop,
						fd->fd_base + fld_face_info_2[i]);
		}


		writel((fld_forest << 16) | (aie_cfg->fld_face_num << 28) | fld_point,
								fd->fd_base + FLD_MODEL_PARA1);
		writel((0xd << 16) | 0xfe9, fd->fd_base + FLD_MODEL_PARA14);

		/*fld kernel model pa setting*/
		for (i = 0; i < FLD_MAX_INPUT; i++) {
			writel(fd->dma_para->fld_tree02_pa[i], fd->fd_base + fld_pl_in_addr_0[i]);
			writel(fd->dma_para->fld_tree13_pa[i], fd->fd_base + fld_pl_in_addr_1[i]);
			writel(fd->dma_para->fld_cv_pa[i], fd->fd_base + fld_pl_in_addr_2[i]);
			writel(fd->dma_para->fld_fp_pa[i], fd->fd_base + fld_pl_in_addr_3[i]);
			writel(fd->dma_para->fld_leafnode_pa[i], fd->fd_base + fld_sh_in_addr[i]);
		}
		writel(fd->dma_para->fld_blink_weight_pa, fd->fd_base + FLD_BS_IN_BASE_ADDR_14);

		writel((aie_cfg->src_img_width << 16) |
					aie_cfg->src_img_height, fd->fd_base + FLD_SRC_WD_HT);

		/*input settings*/
		writel(0x007c003f, fd->fd_base + FLD_PL_IN_SIZE_0);
		writel(0x0040000f, fd->fd_base + FLD_PL_IN_STRIDE_0);
		writel(0x007c003f, fd->fd_base + FLD_PL_IN_SIZE_1);
		writel(0x0040000f, fd->fd_base + FLD_PL_IN_STRIDE_1);
		writel(0x0016003f, fd->fd_base + FLD_PL_IN_SIZE_2_0);
		writel(0x0040000f, fd->fd_base + FLD_PL_IN_STRIDE_2_0);
		writel(0x0013003f, fd->fd_base + FLD_PL_IN_SIZE_2_1);
		writel(0x0040000f, fd->fd_base + FLD_PL_IN_STRIDE_2_1);
		writel(0x0013003f, fd->fd_base + FLD_PL_IN_SIZE_2_2);
		writel(0x0040000f, fd->fd_base + FLD_PL_IN_STRIDE_2_2);
		writel(0x00a6001f, fd->fd_base + FLD_PL_IN_SIZE_3);
		writel(0x0020000f, fd->fd_base + FLD_PL_IN_STRIDE_3);

		/*output setting*/
		writel((2400 * aie_cfg->fld_face_num - 1) << 16 | 127,
						fd->fd_base + FLD_SH_IN_SIZE_0);
		writel(0x0010000f, fd->fd_base + FLD_SH_IN_STRIDE_0);
		writel(fd->dma_para->fld_output_pa, fd->fd_base + FLD_TR_OUT_BASE_ADDR_0);
		writel((aie_cfg->fld_face_num - 1) << 16 | 0x6f, fd->fd_base + FLD_TR_OUT_SIZE_0);
		writel(0x0070000f, fd->fd_base + FLD_TR_OUT_STRIDE_0);
		writel(fd->dma_para->fld_output_pa, fd->fd_base + FLD_PP_OUT_BASE_ADDR_0);
		writel((aie_cfg->fld_face_num - 1) << 16 | 0x6f, fd->fd_base + FLD_PP_OUT_SIZE_0);
		writel(0x0070000f, fd->fd_base + FLD_PP_OUT_STRIDE_0);

		/*cv score*/
		writel(0x00000001, fd->fd_base + FLD_BS_BIAS);
		writel(0x0000b835, fd->fd_base + FLD_CV_FM_RANGE_0); //8E8
		writel(0xffff5cba, fd->fd_base + FLD_CV_FM_RANGE_1); //8EC
		writel(0x00005ed5, fd->fd_base + FLD_CV_PM_RANGE_0); //8F0
		writel(0xffff910d, fd->fd_base + FLD_CV_PM_RANGE_1); //8F4 //temp 310
		writel(0x0000031e, fd->fd_base + FLD_BS_RANGE_0); //8F8
		writel(0xfffffcae, fd->fd_base + FLD_BS_RANGE_1); //8FC

		/*fld mode + trigger start*/
		writel(0x11, fd->fd_base + AIE_START_REG);
	}
#else
	config_aie_cmdq_hw(fd, aie_cfg);
#endif

}

void aie_execute_pose(struct mtk_aie_dev *fd)
{
	writel(0x00000100, fd->fd_base + AIE_ENABLE_REG);
	writel(0x00000300, fd->fd_base + AIE_LOOP_REG);
	writel(0x1, fd->fd_base + AIE_INT_EN_REG);
	writel(fd->reg_cfg.fd_pose_adr, fd->fd_base + AIE_FD_CON_BASE_ADR_REG);
	writel(0x1, fd->fd_base + AIE_START_REG);
}

void aie_irqhandle(struct mtk_aie_dev *fd)
{
	int status;

	writel(0x0, fd->fd_base + AIE_START_REG);

	/* interrupt read clear */
	status = readl(fd->fd_base + AIE_INT_REG);
}

/* return aie_cfg to user space */
void aie_get_fd_result(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg)
{
	void *fd_pym_result[PYM_NUM];
	u32 fd_result_hw, fd_result_1_hw;
	u32 fd_total_num;
	u32 fd_pyramid_num[PYM_NUM];

	aie_cfg->sel_mode = fd->base_para->sel_mode;
	aie_cfg->rotate_degree = fd->base_para->rotate_degree;
	aie_cfg->src_img_addr = fd->base_para->src_img_addr;
	aie_cfg->src_img_addr_uv = fd->base_para->src_img_addr_uv;
	aie_cfg->src_img_width = fd->base_para->img_width;
	aie_cfg->src_img_height = fd->base_para->img_height;
	aie_cfg->src_img_fmt = fd->base_para->src_img_fmt;
	aie_cfg->fd_version = FD_VERSION;
	aie_cfg->attr_version = ATTR_VERSION;

	fd_pym_result[0] = fd->dma_para->fd_out_hw_va[rpn0_loop_num][0];
	fd_pym_result[1] = fd->dma_para->fd_out_hw_va[rpn1_loop_num][0];
	fd_pym_result[2] = fd->dma_para->fd_out_hw_va[rpn2_loop_num][0];

	fd_result_hw = fd->reg_cfg.hw_result;
	fd_result_1_hw = fd->reg_cfg.hw_result1;
	fd_total_num = fd_result_hw & 0xFFF;
	fd_pyramid_num[0] = (fd_result_hw & 0xFFF0000) >> 16;
	fd_pyramid_num[1] = fd_result_1_hw & 0xFFF;
	fd_pyramid_num[2] = (fd_result_1_hw & 0xFFF0000) >> 16;

	aie_cfg->fd_out.fd_total_num = fd_total_num;
	aie_cfg->fd_out.fd_pyramid0_num = fd_pyramid_num[0];
	aie_cfg->fd_out.fd_pyramid1_num = fd_pyramid_num[1];
	aie_cfg->fd_out.fd_pyramid2_num = fd_pyramid_num[2];

	memcpy(aie_cfg->fd_out.rpn31_rlt,
	       fd->dma_para->fd_out_hw_va[rpn2_loop_num][0],
	       sizeof(aie_cfg->fd_out.rpn31_rlt));
	memcpy(aie_cfg->fd_out.rpn63_rlt,
	       fd->dma_para->fd_out_hw_va[rpn1_loop_num][0],
	       sizeof(aie_cfg->fd_out.rpn63_rlt));
	memcpy(aie_cfg->fd_out.rpn95_rlt,
	       fd->dma_para->fd_out_hw_va[rpn0_loop_num][0],
	       sizeof(aie_cfg->fd_out.rpn95_rlt));
}

void aie_get_attr_result(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg)
{
	u32 *attr_race_result, *attr_gender_result;
	u32 *attr_age_result, *attr_isIndian_result;

	aie_cfg->sel_mode = fd->attr_para->sel_mode[fd->attr_para->r_idx];
	aie_cfg->rotate_degree =
		fd->attr_para->rotate_degree[fd->attr_para->r_idx];
	aie_cfg->src_img_addr =
		fd->attr_para->src_img_addr[fd->attr_para->r_idx];
	aie_cfg->src_img_addr_uv =
		fd->attr_para->src_img_addr_uv[fd->attr_para->r_idx];
	aie_cfg->src_img_width = fd->attr_para->img_width[fd->attr_para->r_idx];
	aie_cfg->src_img_height =
		fd->attr_para->img_height[fd->attr_para->r_idx];
	aie_cfg->src_img_fmt = fd->attr_para->src_img_fmt[fd->attr_para->r_idx];
	aie_cfg->fd_version = FD_VERSION;
	aie_cfg->attr_version = ATTR_VERSION;

	/* 64 feature * 32 bytes */
	attr_age_result =
		(u32 *)fd->dma_para->age_out_hw_va[fd->attr_para->r_idx];
	attr_gender_result =
		(u32 *)fd->dma_para->gender_out_hw_va[fd->attr_para->r_idx];
	attr_isIndian_result =
		(u32 *)fd->dma_para->isIndian_out_hw_va[fd->attr_para->r_idx];
	attr_race_result =
		(u32 *)fd->dma_para->race_out_hw_va[fd->attr_para->r_idx];

	memcpy(aie_cfg->attr_out.rpn17_rlt, attr_age_result,
	       sizeof(aie_cfg->attr_out.rpn17_rlt));
	memcpy(aie_cfg->attr_out.rpn20_rlt, attr_gender_result,
	       sizeof(aie_cfg->attr_out.rpn20_rlt));
	memcpy(aie_cfg->attr_out.rpn22_rlt, attr_isIndian_result,
	       sizeof(aie_cfg->attr_out.rpn22_rlt));
	memcpy(aie_cfg->attr_out.rpn25_rlt, attr_race_result,
	       sizeof(aie_cfg->attr_out.rpn25_rlt));

	fd->attr_para->r_idx = (fd->attr_para->r_idx + 1) % MAX_ENQUE_FRAME_NUM;
}


void aie_get_fld_result(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg)
{
	aie_cfg->sel_mode = fd->fld_para->sel_mode;
	aie_cfg->src_img_width = fd->fld_para->img_width;
	aie_cfg->src_img_height = fd->fld_para->img_height;
	aie_cfg->fd_version = FD_VERSION;
	aie_cfg->attr_version = ATTR_VERSION;
	aie_cfg->src_img_addr = fd->fld_para->src_img_addr;
	aie_cfg->fld_face_num = fd->fld_para->face_num;

	memcpy(aie_cfg->fld_raw_out, fd->dma_para->fld_output_va, FLD_MAX_OUT);
	memcpy((char *)&(aie_cfg->fld_input[0]), (char *)fd->fld_para->fld_input,
		sizeof(struct FLD_CROP_RIP_ROP) * aie_cfg->fld_face_num);

}

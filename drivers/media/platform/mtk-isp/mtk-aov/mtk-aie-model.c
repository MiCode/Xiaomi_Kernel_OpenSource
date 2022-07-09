// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#define REDUCE_MEM_NETWORK 0

#if REDUCE_MEM_NETWORK
#include <aie_mp_fw_reduce/kernel/dma_def.h>
#include <aie_mp_fw_reduce/all_header.h>
#include "./aie_aov_config_reduce/dma_def.h"
#include "./aie_aov_config_reduce/aov_fd_confi_frame01.h"
#include "./aie_aov_config_reduce/aov_rs_confi_frame01.h"
#include "./aie_aov_config_reduce/aov_yuv2rgb_confi_frame01.h"
#else
#include <aie_mp_fw/kernel/dma_def.h>
#include <aie_mp_fw/all_header.h>
#include "./aie_aov_config/dma_def.h"
#include "./aie_aov_config/aov_fd_confi_frame01.h"
#include "./aie_aov_config/aov_rs_confi_frame01.h"
#include "./aie_aov_config/aov_yuv2rgb_confi_frame01.h"
#endif

#define AOV_GOLDEN 0
#define AOV_RESEULT 0

#if (AOV_GOLDEN || AOV_RESEULT)
#if REDUCE_MEM_NETWORK
#include <aie_golden_reduce/fmap/dma_def.h>
#include <aie_golden_reduce/yuv2rgb/dma_def.h>
#include <aie_golden_reduce/rs/dma_def.h>
#include <aie_golden_reduce/all_header.h>
#else
#include <aie_golden/fmap/dma_def.h>
#include <aie_golden/yuv2rgb/dma_def.h>
#include <aie_golden/rs/dma_def.h>
#include <aie_golden/all_header.h>
#endif
#endif

#define fd_loop_num 29
#define kernel_RDMA_RA_num 2

#if REDUCE_MEM_NETWORK
static const unsigned int fd_ker_rdma_size[fd_loop_num][kernel_RDMA_RA_num] = {
	{240, 240},   {1168, 1168}, {1168, 1168}, {272, 272},   {1168, 1168},
	{784, 784}, {272, 272}, {1168, 1168}, {784, 784}, {2320, 2320},
	{1168, 1168}, {1040, 1040}, {272, 272}, {1168, 1168}, {1168, 1168},
	{400, 400}, {1168, 1168}, {1168, 1168}, {2080, 2080}, {1040, 1040},
	{1040, 1040}, {528, 528},   {4160, 4160}, {4160, 4160}, {2080, 2080},
	{2080, 2080}, {2080, 2080}, {1040, 1040}, {0, 0}};
#else
static const unsigned int fd_ker_rdma_size[fd_loop_num][kernel_RDMA_RA_num] = {
	{240, 240},   {1168, 1168}, {1168, 1168}, {272, 272},   {2320, 2320},
	{2080, 2080}, {1040, 1040}, {4624, 4624}, {3104, 3104}, {9232, 9232},
	{4624, 4624}, {4128, 4128}, {1040, 1040}, {4624, 4624}, {4624, 4624},
	{1552, 1552}, {4624, 4624}, {4624, 4624}, {4128, 4128}, {1040, 1040},
	{1040, 1040}, {528, 528},   {4160, 4160}, {4160, 4160}, {2080, 2080},
	{2080, 2080}, {2080, 2080}, {1040, 1040}, {0, 0}};
#endif

void mtk_aie_aov_memcpy(char *buffer)
{
	char *tmp = buffer;

	memcpy(tmp, &aov_yuv2rgb_confi_frame01[0], aov_yuv2rgb_confi_frame01_size);
	tmp += aov_yuv2rgb_confi_frame01_size;
	memcpy(tmp, &aov_fd_confi_frame01[0], aov_fd_confi_frame01_size);
	tmp += aov_fd_confi_frame01_size;

	/*0~10*/
	memcpy(tmp, &fdvt_kernel_bias_loop00_0_frame01[0], fd_ker_rdma_size[0][0]);
	tmp += fd_ker_rdma_size[0][0];
	memcpy(tmp, &fdvt_kernel_bias_loop00_1_frame01[0], fd_ker_rdma_size[0][1]);
	tmp += fd_ker_rdma_size[0][1];

	memcpy(tmp, &fdvt_kernel_bias_loop01_0_frame01[0], fd_ker_rdma_size[1][0]);
	tmp += fd_ker_rdma_size[1][0];
	memcpy(tmp, &fdvt_kernel_bias_loop01_1_frame01[0], fd_ker_rdma_size[1][1]);
	tmp += fd_ker_rdma_size[1][1];

	memcpy(tmp, &fdvt_kernel_bias_loop02_0_frame01[0], fd_ker_rdma_size[2][0]);
	tmp += fd_ker_rdma_size[2][0];
	memcpy(tmp, &fdvt_kernel_bias_loop02_1_frame01[0], fd_ker_rdma_size[2][1]);
	tmp += fd_ker_rdma_size[2][1];

	memcpy(tmp, &fdvt_kernel_bias_loop03_0_frame01[0], fd_ker_rdma_size[3][0]);
	tmp += fd_ker_rdma_size[3][0];
	memcpy(tmp, &fdvt_kernel_bias_loop03_1_frame01[0], fd_ker_rdma_size[3][1]);
	tmp += fd_ker_rdma_size[3][1];

	memcpy(tmp, &fdvt_kernel_bias_loop04_0_frame01[0], fd_ker_rdma_size[4][0]);
	tmp += fd_ker_rdma_size[4][0];
	memcpy(tmp, &fdvt_kernel_bias_loop04_1_frame01[0], fd_ker_rdma_size[4][1]);
	tmp += fd_ker_rdma_size[4][1];

	memcpy(tmp, &fdvt_kernel_bias_loop05_0_frame01[0], fd_ker_rdma_size[5][0]);
	tmp += fd_ker_rdma_size[5][0];
	memcpy(tmp, &fdvt_kernel_bias_loop05_1_frame01[0], fd_ker_rdma_size[5][1]);
	tmp += fd_ker_rdma_size[5][1];

	memcpy(tmp, &fdvt_kernel_bias_loop06_0_frame01[0], fd_ker_rdma_size[6][0]);
	tmp += fd_ker_rdma_size[6][0];
	memcpy(tmp, &fdvt_kernel_bias_loop06_1_frame01[0], fd_ker_rdma_size[6][1]);
	tmp += fd_ker_rdma_size[6][1];

	memcpy(tmp, &fdvt_kernel_bias_loop07_0_frame01[0], fd_ker_rdma_size[7][0]);
	tmp += fd_ker_rdma_size[7][0];
	memcpy(tmp, &fdvt_kernel_bias_loop07_1_frame01[0], fd_ker_rdma_size[7][1]);
	tmp += fd_ker_rdma_size[7][1];
	memcpy(tmp, &fdvt_kernel_bias_loop08_0_frame01[0], fd_ker_rdma_size[8][0]);
	tmp += fd_ker_rdma_size[8][0];
	memcpy(tmp, &fdvt_kernel_bias_loop08_1_frame01[0], fd_ker_rdma_size[8][1]);
	tmp += fd_ker_rdma_size[8][1];
	memcpy(tmp, &fdvt_kernel_bias_loop09_0_frame01[0], fd_ker_rdma_size[9][0]);
	tmp += fd_ker_rdma_size[9][0];
	memcpy(tmp, &fdvt_kernel_bias_loop09_1_frame01[0], fd_ker_rdma_size[9][1]);
	tmp += fd_ker_rdma_size[9][1];

	memcpy(tmp, &fdvt_kernel_bias_loop10_0_frame01[0], fd_ker_rdma_size[10][0]);
	tmp += fd_ker_rdma_size[10][0];
	memcpy(tmp, &fdvt_kernel_bias_loop10_1_frame01[0], fd_ker_rdma_size[10][1]);
	tmp += fd_ker_rdma_size[10][1];
	memcpy(tmp, &fdvt_kernel_bias_loop11_0_frame01[0], fd_ker_rdma_size[11][0]);
	tmp += fd_ker_rdma_size[11][0];
	memcpy(tmp, &fdvt_kernel_bias_loop11_1_frame01[0], fd_ker_rdma_size[11][1]);
	tmp += fd_ker_rdma_size[11][1];
	memcpy(tmp, &fdvt_kernel_bias_loop12_0_frame01[0], fd_ker_rdma_size[12][0]);
	tmp += fd_ker_rdma_size[12][0];
	memcpy(tmp, &fdvt_kernel_bias_loop12_1_frame01[0], fd_ker_rdma_size[12][1]);
	tmp += fd_ker_rdma_size[12][1];
	memcpy(tmp, &fdvt_kernel_bias_loop13_0_frame01[0], fd_ker_rdma_size[13][0]);
	tmp += fd_ker_rdma_size[13][0];
	memcpy(tmp, &fdvt_kernel_bias_loop13_1_frame01[0], fd_ker_rdma_size[13][1]);
	tmp += fd_ker_rdma_size[13][1];

	memcpy(tmp, &fdvt_kernel_bias_loop14_0_frame01[0], fd_ker_rdma_size[14][0]);
	tmp += fd_ker_rdma_size[14][0];
	memcpy(tmp, &fdvt_kernel_bias_loop14_1_frame01[0], fd_ker_rdma_size[14][1]);
	tmp += fd_ker_rdma_size[14][1];
	memcpy(tmp, &fdvt_kernel_bias_loop15_0_frame01[0], fd_ker_rdma_size[15][0]);
	tmp += fd_ker_rdma_size[15][0];
	memcpy(tmp, &fdvt_kernel_bias_loop15_1_frame01[0], fd_ker_rdma_size[15][1]);
	tmp += fd_ker_rdma_size[15][1];
	memcpy(tmp, &fdvt_kernel_bias_loop16_0_frame01[0], fd_ker_rdma_size[16][0]);
	tmp += fd_ker_rdma_size[16][0];
	memcpy(tmp, &fdvt_kernel_bias_loop16_1_frame01[0], fd_ker_rdma_size[16][1]);
	tmp += fd_ker_rdma_size[16][1];
	memcpy(tmp, &fdvt_kernel_bias_loop17_0_frame01[0], fd_ker_rdma_size[17][0]);
	tmp += fd_ker_rdma_size[17][0];
	memcpy(tmp, &fdvt_kernel_bias_loop17_1_frame01[0], fd_ker_rdma_size[17][1]);
	tmp += fd_ker_rdma_size[17][1];
	memcpy(tmp, &fdvt_kernel_bias_loop18_0_frame01[0], fd_ker_rdma_size[18][0]);
	tmp += fd_ker_rdma_size[18][0];
	memcpy(tmp, &fdvt_kernel_bias_loop18_1_frame01[0], fd_ker_rdma_size[18][1]);
	tmp += fd_ker_rdma_size[18][1];
	memcpy(tmp, &fdvt_kernel_bias_loop19_0_frame01[0], fd_ker_rdma_size[19][0]);
	tmp += fd_ker_rdma_size[19][0];
	memcpy(tmp, &fdvt_kernel_bias_loop19_1_frame01[0], fd_ker_rdma_size[19][1]);
	tmp += fd_ker_rdma_size[19][1];
	memcpy(tmp, &fdvt_kernel_bias_loop20_0_frame01[0], fd_ker_rdma_size[20][0]);
	tmp += fd_ker_rdma_size[20][0];
	memcpy(tmp, &fdvt_kernel_bias_loop20_1_frame01[0], fd_ker_rdma_size[20][1]);
	tmp += fd_ker_rdma_size[20][1];
	memcpy(tmp, &fdvt_kernel_bias_loop21_0_frame01[0], fd_ker_rdma_size[21][0]);
	tmp += fd_ker_rdma_size[21][0];
	memcpy(tmp, &fdvt_kernel_bias_loop21_1_frame01[0], fd_ker_rdma_size[21][1]);
	tmp += fd_ker_rdma_size[21][1];
	memcpy(tmp, &fdvt_kernel_bias_loop22_0_frame01[0], fd_ker_rdma_size[22][0]);
	tmp += fd_ker_rdma_size[22][0];
	memcpy(tmp, &fdvt_kernel_bias_loop22_1_frame01[0], fd_ker_rdma_size[22][1]);
	tmp += fd_ker_rdma_size[22][1];
	memcpy(tmp, &fdvt_kernel_bias_loop23_0_frame01[0], fd_ker_rdma_size[23][0]);
	tmp += fd_ker_rdma_size[23][0];
	memcpy(tmp, &fdvt_kernel_bias_loop23_1_frame01[0], fd_ker_rdma_size[23][1]);
	tmp += fd_ker_rdma_size[23][1];
	memcpy(tmp, &fdvt_kernel_bias_loop24_0_frame01[0], fd_ker_rdma_size[24][0]);
	tmp += fd_ker_rdma_size[24][0];
	memcpy(tmp, &fdvt_kernel_bias_loop24_1_frame01[0], fd_ker_rdma_size[24][1]);
	tmp += fd_ker_rdma_size[24][1];
	memcpy(tmp, &fdvt_kernel_bias_loop25_0_frame01[0], fd_ker_rdma_size[25][0]);
	tmp += fd_ker_rdma_size[25][0];
	memcpy(tmp, &fdvt_kernel_bias_loop25_1_frame01[0], fd_ker_rdma_size[25][1]);
	tmp += fd_ker_rdma_size[25][1];
	memcpy(tmp, &fdvt_kernel_bias_loop26_0_frame01[0], fd_ker_rdma_size[26][0]);
	tmp += fd_ker_rdma_size[26][0];
	memcpy(tmp, &fdvt_kernel_bias_loop26_1_frame01[0], fd_ker_rdma_size[26][1]);
	tmp += fd_ker_rdma_size[26][1];
	memcpy(tmp, &fdvt_kernel_bias_loop27_0_frame01[0], fd_ker_rdma_size[27][0]);
	tmp += fd_ker_rdma_size[27][0];
	memcpy(tmp, &fdvt_kernel_bias_loop27_1_frame01[0], fd_ker_rdma_size[27][1]);

#if AOV_RESEULT
	tmp += fd_ker_rdma_size[27][1];
	memcpy(tmp, &fdvt_fdvt_in1_frame01[0], fdvt_fdvt_in1_frame01_size);
	tmp += fdvt_fdvt_in1_frame01_size;
	memcpy(tmp, &fdvt_fdvt_in2_frame01[0], fdvt_fdvt_in2_frame01_size);
	tmp += fdvt_fdvt_in2_frame01_size;
	memcpy(tmp, &fdvt_fd_out_loop28_0[0], fdvt_fd_out_loop28_0_size);
#endif

#if AOV_GOLDEN
	tmp += fd_ker_rdma_size[27][1];
	memcpy(tmp, &fdvt_fdvt_in1_frame01[0], fdvt_fdvt_in1_frame01_size);
	tmp += fdvt_fdvt_in1_frame01_size;
	memcpy(tmp, &fdvt_fdvt_in2_frame01[0], fdvt_fdvt_in2_frame01_size);
	tmp += fdvt_fdvt_in2_frame01_size;

	memcpy(tmp, &fdvt_rs_out_frame01_scale00_r[0], fdvt_rs_out_frame01_scale00_r_size);
	tmp += fdvt_rs_out_frame01_scale00_r_size;
	memcpy(tmp, &fdvt_rs_out_frame01_scale00_g[0], fdvt_rs_out_frame01_scale00_g_size);
	tmp += fdvt_rs_out_frame01_scale00_g_size;
	memcpy(tmp, &fdvt_rs_out_frame01_scale00_b[0], fdvt_rs_out_frame01_scale00_b_size);
	tmp += fdvt_rs_out_frame01_scale00_b_size;


	memcpy(tmp, &fdvt_fd_out_loop00_0[0], fdvt_fd_out_loop00_0_size);
	tmp += fdvt_fd_out_loop00_0_size;

	memcpy(tmp, &fdvt_fd_out_loop01_0[0], fdvt_fd_out_loop01_0_size);
	tmp += fdvt_fd_out_loop01_0_size;

	memcpy(tmp, &fdvt_fd_out_loop01_2[0], fdvt_fd_out_loop01_2_size);
	tmp += fdvt_fd_out_loop01_2_size;
	memcpy(tmp, &fdvt_fd_out_loop02_0[0], fdvt_fd_out_loop02_0_size);
	tmp += fdvt_fd_out_loop02_0_size;

	memcpy(tmp, &fdvt_fd_out_loop02_2[0], fdvt_fd_out_loop02_2_size);
	tmp += fdvt_fd_out_loop02_2_size;
	memcpy(tmp, &fdvt_fd_out_loop03_0[0], fdvt_fd_out_loop03_0_size);
	tmp += fdvt_fd_out_loop03_0_size;
	memcpy(tmp, &fdvt_fd_out_loop04_0[0], fdvt_fd_out_loop04_0_size);
	tmp += fdvt_fd_out_loop04_0_size;
#if (!REDUCE_MEM_NETWORK)
	memcpy(tmp, &fdvt_fd_out_loop04_1[0], fdvt_fd_out_loop04_1_size);
	tmp += fdvt_fd_out_loop04_1_size;
#endif
	memcpy(tmp, &fdvt_fd_out_loop04_2[0], fdvt_fd_out_loop04_2_size);
	tmp += fdvt_fd_out_loop04_2_size;
#if (!REDUCE_MEM_NETWORK)
	memcpy(tmp, &fdvt_fd_out_loop04_3[0], fdvt_fd_out_loop04_3_size);
	tmp += fdvt_fd_out_loop04_3_size;
#endif
	memcpy(tmp, &fdvt_fd_out_loop05_0[0], fdvt_fd_out_loop05_0_size);
	tmp += fdvt_fd_out_loop05_0_size;
	memcpy(tmp, &fdvt_fd_out_loop05_1[0], fdvt_fd_out_loop05_1_size);
	tmp += fdvt_fd_out_loop05_1_size;
	memcpy(tmp, &fdvt_fd_out_loop05_2[0], fdvt_fd_out_loop05_2_size);
	tmp += fdvt_fd_out_loop05_2_size;
	memcpy(tmp, &fdvt_fd_out_loop05_3[0], fdvt_fd_out_loop05_3_size);
	tmp += fdvt_fd_out_loop05_3_size;

	memcpy(tmp, &fdvt_fd_out_loop06_0[0], fdvt_fd_out_loop06_0_size);
	tmp += fdvt_fd_out_loop06_0_size;
	memcpy(tmp, &fdvt_fd_out_loop07_0[0], fdvt_fd_out_loop07_0_size);
	tmp += fdvt_fd_out_loop07_0_size;
	memcpy(tmp, &fdvt_fd_out_loop07_2[0], fdvt_fd_out_loop07_2_size);
	tmp += fdvt_fd_out_loop07_2_size;
	memcpy(tmp, &fdvt_fd_out_loop08_0[0], fdvt_fd_out_loop08_0_size);
	tmp += fdvt_fd_out_loop08_0_size;

	memcpy(tmp, &fdvt_fd_out_loop08_1[0], fdvt_fd_out_loop08_1_size);
	tmp += fdvt_fd_out_loop08_1_size;
	memcpy(tmp, &fdvt_fd_out_loop09_0[0], fdvt_fd_out_loop09_0_size);
	tmp += fdvt_fd_out_loop09_0_size;
	memcpy(tmp, &fdvt_fd_out_loop10_0[0], fdvt_fd_out_loop10_0_size);
	tmp += fdvt_fd_out_loop10_0_size;
	memcpy(tmp, &fdvt_fd_out_loop10_2[0], fdvt_fd_out_loop10_2_size);
	tmp += fdvt_fd_out_loop10_2_size;
	memcpy(tmp, &fdvt_fd_out_loop11_0[0], fdvt_fd_out_loop11_0_size);
	tmp += fdvt_fd_out_loop11_0_size;
	memcpy(tmp, &fdvt_fd_out_loop11_1[0], fdvt_fd_out_loop11_1_size);
	tmp += fdvt_fd_out_loop11_1_size;
	memcpy(tmp, &fdvt_fd_out_loop12_0[0], fdvt_fd_out_loop12_0_size);
	tmp += fdvt_fd_out_loop12_0_size;
	memcpy(tmp, &fdvt_fd_out_loop13_0[0], fdvt_fd_out_loop13_0_size);
	tmp += fdvt_fd_out_loop13_0_size;
	memcpy(tmp, &fdvt_fd_out_loop14_0[0], fdvt_fd_out_loop14_0_size);
	tmp += fdvt_fd_out_loop14_0_size;
	memcpy(tmp, &fdvt_fd_out_loop15_0[0], fdvt_fd_out_loop15_0_size);
	tmp += fdvt_fd_out_loop15_0_size;
	memcpy(tmp, &fdvt_fd_out_loop16_0[0], fdvt_fd_out_loop16_0_size);
	tmp += fdvt_fd_out_loop16_0_size;
	memcpy(tmp, &fdvt_fd_out_loop17_0[0], fdvt_fd_out_loop17_0_size);
	tmp += fdvt_fd_out_loop17_0_size;
	memcpy(tmp, &fdvt_fd_out_loop18_0[0], fdvt_fd_out_loop18_0_size);
	tmp += fdvt_fd_out_loop18_0_size;
	memcpy(tmp, &fdvt_fd_out_loop18_1[0], fdvt_fd_out_loop18_1_size);
	tmp += fdvt_fd_out_loop18_1_size;

	memcpy(tmp, &fdvt_fd_out_loop19_0[0], fdvt_fd_out_loop19_0_size);
	tmp += fdvt_fd_out_loop19_0_size;
	memcpy(tmp, &fdvt_fd_out_loop19_1[0], fdvt_fd_out_loop19_1_size);
	tmp += fdvt_fd_out_loop19_1_size;

	memcpy(tmp, &fdvt_fd_out_loop20_0[0], fdvt_fd_out_loop20_0_size);
	tmp += fdvt_fd_out_loop20_0_size;
	memcpy(tmp, &fdvt_fd_out_loop20_1[0], fdvt_fd_out_loop20_1_size);
	tmp += fdvt_fd_out_loop20_1_size;
	memcpy(tmp, &fdvt_fd_out_loop21_0[0], fdvt_fd_out_loop21_0_size);
	tmp += fdvt_fd_out_loop21_0_size;
	memcpy(tmp, &fdvt_fd_out_loop22_0[0], fdvt_fd_out_loop22_0_size);
	tmp += fdvt_fd_out_loop22_0_size;
	memcpy(tmp, &fdvt_fd_out_loop22_1[0], fdvt_fd_out_loop22_1_size);
	tmp += fdvt_fd_out_loop22_1_size;
	memcpy(tmp, &fdvt_fd_out_loop22_2[0], fdvt_fd_out_loop22_2_size);
	tmp += fdvt_fd_out_loop22_2_size;
	memcpy(tmp, &fdvt_fd_out_loop22_3[0], fdvt_fd_out_loop22_3_size);
	tmp += fdvt_fd_out_loop22_3_size;
	memcpy(tmp, &fdvt_fd_out_loop23_0[0], fdvt_fd_out_loop23_0_size);
	tmp += fdvt_fd_out_loop23_0_size;
	memcpy(tmp, &fdvt_fd_out_loop23_1[0], fdvt_fd_out_loop23_1_size);
	tmp += fdvt_fd_out_loop23_1_size;
	memcpy(tmp, &fdvt_fd_out_loop23_2[0], fdvt_fd_out_loop23_2_size);
	tmp += fdvt_fd_out_loop23_2_size;
	memcpy(tmp, &fdvt_fd_out_loop23_3[0], fdvt_fd_out_loop23_3_size);
	tmp += fdvt_fd_out_loop23_3_size;
	memcpy(tmp, &fdvt_fd_out_loop24_0[0], fdvt_fd_out_loop24_0_size);
	tmp += fdvt_fd_out_loop24_0_size;
	memcpy(tmp, &fdvt_fd_out_loop24_1[0], fdvt_fd_out_loop24_1_size);
	tmp += fdvt_fd_out_loop24_1_size;
	memcpy(tmp, &fdvt_fd_out_loop25_0[0], fdvt_fd_out_loop25_0_size);
	tmp += fdvt_fd_out_loop25_0_size;

	memcpy(tmp, &fdvt_fd_out_loop25_1[0], fdvt_fd_out_loop25_1_size);
	tmp += fdvt_fd_out_loop25_1_size;
	memcpy(tmp, &fdvt_fd_out_loop26_0[0], fdvt_fd_out_loop26_0_size);
	tmp += fdvt_fd_out_loop26_0_size;
	memcpy(tmp, &fdvt_fd_out_loop26_1[0], fdvt_fd_out_loop26_1_size);
	tmp += fdvt_fd_out_loop26_1_size;
	memcpy(tmp, &fdvt_fd_out_loop27_0[0], fdvt_fd_out_loop27_0_size);
	tmp += fdvt_fd_out_loop27_0_size;

	memcpy(tmp, &fdvt_fd_out_loop28_0[0], fdvt_fd_out_loop28_0_size);

#endif
}

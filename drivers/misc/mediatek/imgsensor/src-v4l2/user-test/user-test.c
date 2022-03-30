// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2020 MediaTek Inc.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <linux/types.h>
#include <linux/videodev2.h>

#include "kd_imgsensor_define_v4l2.h"
#include "imgsensor-user.h"

#define ARRAY_SIZE(arr)	\
	(sizeof(arr) / sizeof((arr)[0]))

static int test_crop_by_scenario(int fd, int scenario_id)
{
	int ret;
	struct mtk_crop_by_scenario info;
	struct SENSOR_WINSIZE_INFO_STRUCT ws;

	info.scenario_id = scenario_id;
	info.p_winsize = &ws;

	ret = ioctl(fd, VIDIOC_MTK_G_CROP_BY_SCENARIO, &info);
	if (ret) {
		printf("%s ret %d\n", __func__, ret);
		return ret;
	}

	printf("full: %d %d\n",
		ws.full_w, ws.full_h);
	printf("crop before scale: %d %d %d %d\n",
		ws.x0_offset, ws.y0_offset,
		ws.w0_size, ws.h0_size);
	printf("scale: %d %d\n",
		ws.scale_w, ws.scale_h);
	printf("crop after scale: %d %d %d %d\n",
		ws.x1_offset, ws.y1_offset,
		ws.w1_size, ws.h1_size);
	printf("crop by tg: %d %d %d %d\n",
		ws.x2_tg_offset, ws.y2_tg_offset,
		ws.w2_tg_size, ws.h2_tg_size);

	return 0;
}

static int test_vcinfo_by_scenario(int fd, int scenario_id)
{
	int ret;
	struct mtk_vcinfo_by_scenario info;
	struct SENSOR_VC_INFO2_STRUCT vcinfo;
	struct SINGLE_VC_INFO2 *vc = vcinfo.vc_info;

	info.scenario_id = scenario_id;
	info.p_vcinfo = &vcinfo;

	ret = ioctl(fd, VIDIOC_MTK_G_VCINFO_BY_SCENARIO, &info);
	if (ret) {
		printf("%s ret %d\n", __func__, ret);
		return ret;
	}

	printf("vc[0] 0x%x 0x%x\n",
		vc[0].VC_ID, vc[0].VC_DataType);
	printf("vc[1] 0x%x 0x%x\n",
		vc[1].VC_ID, vc[1].VC_DataType);
	printf("vc[2] 0x%x 0x%x\n",
		vc[2].VC_ID, vc[2].VC_DataType);

	return 0;
}

static int test_def_fps_by_scenario(int fd, int scenario_id)
{
	int ret;
	struct mtk_fps_by_scenario info;

	info.scenario_id = scenario_id;

	ret = ioctl(fd, VIDIOC_MTK_G_DEF_FPS_BY_SCENARIO, &info);
	if (ret) {
		printf("%s ret %d\n", __func__, ret);
		return ret;
	}

	printf("def_fps %d\n",
		info.fps);

	return 0;
}

static int test_pclk_by_scenario(int fd, int scenario_id)
{
	int ret;
	struct mtk_pclk_by_scenario info;

	info.scenario_id = scenario_id;

	ret = ioctl(fd, VIDIOC_MTK_G_PCLK_BY_SCENARIO, &info);
	if (ret) {
		printf("%s ret %d\n", __func__, ret);
		return ret;
	}

	printf("pclk %d\n",
		info.pclk);

	return 0;
}

static int test_llp_fll_by_scenario(int fd, int scenario_id)
{
	int ret;
	struct mtk_llp_fll_by_scenario info;

	info.scenario_id = scenario_id;

	ret = ioctl(fd, VIDIOC_MTK_G_LLP_FLL_BY_SCENARIO, &info);
	if (ret) {
		printf("%s ret %d\n", __func__, ret);
		return ret;
	}

	printf("llp %d fll %d\n",
		info.llp, info.fll);

	return 0;
}

static int test_min_shutter_by_scenario(int fd, int scenario_id)
{
	int ret;
	struct mtk_min_shutter_by_scenario info;

	info.scenario_id = scenario_id;

	ret = ioctl(fd, VIDIOC_MTK_G_MIN_SHUTTER_BY_SCENARIO, &info);
	if (ret) {
		printf("%s ret %d\n", __func__, ret);
		return ret;
	}

	printf("min_shutter %d\n",
		info.min_shutter);

	return 0;
}

static int test_gain_range_by_scenario(int fd, int scenario_id)
{
	int ret;
	struct mtk_gain_range_by_scenario info;

	info.scenario_id = scenario_id;

	ret = ioctl(fd, VIDIOC_MTK_G_GAIN_RANGE_BY_SCENARIO, &info);
	if (ret) {
		printf("%s ret %d\n", __func__, ret);
		return ret;
	}

	printf("min_gain %d max_gain %d\n",
		info.min_gain, info.max_gain);

	return 0;
}

static int test_pdaf_info_by_scenario(int fd, int scenario_id)
{
	int ret;
	struct mtk_pdaf_info_by_scenario info;
	struct SET_PD_BLOCK_INFO_T pd;

	info.scenario_id = scenario_id;
	info.p_pd = &pd;

	ret = ioctl(fd, VIDIOC_MTK_G_PDAF_INFO_BY_SCENARIO, &info);
	if (ret) {
		printf("%s ret %d\n", __func__, ret);
		return ret;
	}

	printf("pdaf_info %d %d %d %d\n",
		pd.i4OffsetX, pd.i4OffsetY,
		pd.i4PitchX, pd.i4PitchY);

	return 0;
}

static int test_pclk(int fd)
{
	int ret;
	__u32 info;

	ret = ioctl(fd, VIDIOC_MTK_G_PCLK, &info);
	if (ret) {
		printf("%s ret %d\n", __func__, ret);
		return ret;
	}

	printf("pclk %d\n", info);

	return 0;
}

static int test_llp_fll(int fd)
{
	int ret;
	struct mtk_llp_fll info;

	ret = ioctl(fd, VIDIOC_MTK_G_LLP_FLL, &info);
	if (ret) {
		printf("%s ret %d\n", __func__, ret);
		return ret;
	}

	printf("llp %d fll %d\n", info.llp, info.fll);

	return 0;
}

static int test_binning_type(int fd, int scenario_id, int HDRMode)
{
	int ret;
	struct mtk_binning_type info;

	info.scenario_id = scenario_id;
	info.HDRMode = HDRMode;

	ret = ioctl(fd, VIDIOC_MTK_G_BINNING_TYPE, &info);
	if (ret) {
		printf("%s ret %d\n", __func__, ret);
		return ret;
	}

	printf("binning_type %d\n",
		info.binning_type);

	return 0;
}

static int test_pdaf_cap(int fd, int scenario_id)
{
	int ret;
	struct mtk_cap info;

	info.scenario_id = scenario_id;

	ret = ioctl(fd, VIDIOC_MTK_G_PDAF_CAP, &info);
	if (ret) {
		printf("%s ret %d\n", __func__, ret);
		return ret;
	}

	printf("pdaf_cap %d\n",
		info.cap);

	return 0;
}

static int test_hdr_cap(int fd, int scenario_id)
{
	int ret;
	struct mtk_cap info;

	info.scenario_id = scenario_id;

	ret = ioctl(fd, VIDIOC_MTK_G_HDR_CAP, &info);
	if (ret) {
		printf("%s ret %d\n", __func__, ret);
		return ret;
	}

	printf("hdr_cap %d\n",
		info.cap);

	return 0;
}

static int test_mipi_pixel_rate(int fd, int scenario_id)
{
	int ret;
	struct mtk_mipi_pixel_rate info;

	info.scenario_id = scenario_id;

	ret = ioctl(fd, VIDIOC_MTK_G_MIPI_PIXEL_RATE, &info);
	if (ret) {
		printf("%s ret %d\n", __func__, ret);
		return ret;
	}

	printf("mipi_pixel_rate %d\n",
		info.mipi_pixel_rate);

	return 0;
}

static int test_scenario_combo_info(int fd, int scenario_id)
{
	int ret;
	struct mtk_scenario_combo_info info;

	memset(&info, 0, sizeof(info));

	info.scenario_id = scenario_id;
	info.p_timing = malloc(sizeof(*info.p_timing));
	info.p_winsize = malloc(sizeof(*info.p_winsize));
	info.p_vcinfo = malloc(sizeof(*info.p_vcinfo));

	ret = ioctl(fd, VIDIOC_MTK_G_SCENARIO_COMBO_INFO, &info);
	if (ret) {
		printf("%s ret %d\n", __func__, ret);
		return ret;
	}

	printf("combo timing: llp %d fll %d w %d h %d linetime_in_ns %lld\n",
		info.p_timing->llp, info.p_timing->fll,
		info.p_timing->width, info.p_timing->height,
		info.p_timing->linetime_in_ns);
	printf("combo full: %d %d\n",
		info.p_winsize->full_w, info.p_winsize->full_h);
	printf("combo vc[0]: 0x%x 0x%x\n",
		info.p_vcinfo->vc_info[0].VC_ID, info.p_vcinfo->vc_info[0].VC_DataType);

	free(info.p_timing);
	free(info.p_winsize);
	free(info.p_vcinfo);

	return 0;
}

static int test_test_pattern_checksum(int fd)
{
	int ret;
	__u32 info;

	ret = ioctl(fd, VIDIOC_MTK_G_TEST_PATTERN_CHECKSUM, &info);
	if (ret) {
		printf("%s ret %d\n", __func__, ret);
		return ret;
	}

	printf("test_pattern_checksum 0x%x\n", info);

	return 0;
}

static int test_base_gain_iso_n_step(int fd)
{
	int ret;
	struct mtk_base_gain_iso_n_step info;

	ret = ioctl(fd, VIDIOC_MTK_G_BASE_GAIN_ISO_N_STEP, &info);
	if (ret) {
		printf("%s ret %d\n", __func__, ret);
		return ret;
	}

	printf("min_gain_iso %d gain_step %d gain_type %d\n",
		info.min_gain_iso, info.gain_step, info.gain_type);

	return 0;
}

static int test_offset_to_start_of_exposure(int fd)
{
	int ret;
	__u32 info;

	ret = ioctl(fd, VIDIOC_MTK_G_OFFSET_TO_START_OF_EXPOSURE, &info);
	if (ret) {
		printf("%s ret %d\n", __func__, ret);
		return ret;
	}

	printf("offset_to_start_of_exposure %d\n", info);

	return 0;
}

static int test_ana_gain_table(int fd)
{
	int i, ret;
	struct mtk_ana_gain_table info;

	ret = ioctl(fd, VIDIOC_MTK_G_ANA_GAIN_TABLE_SIZE, &info);
	if (ret) {
		printf("%s ret %d\n", __func__, ret);
		return ret;
	}

	printf("ana_gain_table size %d\n", info.size);

	if (!info.size)
		return 0;

	info.p_buf = malloc(info.size);

	ret = ioctl(fd, VIDIOC_MTK_G_ANA_GAIN_TABLE, &info);
	if (ret) {
		printf("%s ret %d\n", __func__, ret);
		return ret;
	}

	for (i = 0; i < 4; i++)
		printf("ana_gain_table buf[0x%x] 0x%x\n", i, info.p_buf[i]);

	free(info.p_buf);

	return 0;
}

static int test_g_pdaf_regs(int fd)
{
	int i, ret;
	struct mtk_regs info;
	static __u16 buf[] = {0x111, 0, 0x112, 0, 0x113, 0, 0x114, 0, 0x115, 0};

	info.size = sizeof(buf);
	info.p_buf = buf;

	ret = ioctl(fd, VIDIOC_MTK_G_PDAF_REGS, &info);
	if (ret) {
		printf("%s ret %d\n", __func__, ret);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(buf); i += 2)
		printf("pdaf_regs[0x%x] 0x%x\n", buf[i], buf[i+1]);

	return 0;
}

static int test_g_feature_info(int fd, int scenario_id)
{
	int ret;
	struct mtk_feature_info info;

	info.scenario_id = scenario_id;
	info.p_info = malloc(sizeof(*info.p_info));
	info.p_config = malloc(sizeof(*info.p_config));

	ret = ioctl(fd, VIDIOC_MTK_G_FEATURE_INFO, &info);
	if (ret) {
		printf("%s ret %d\n", __func__, ret);
		return ret;
	}


	printf("SensorMIPILaneNumber %d\n", info.p_info->SensorMIPILaneNumber);

	free(info.p_info);
	free(info.p_config);

	return 0;
}

static int test_g_delay_info(int fd)
{
	int ret;
	struct SENSOR_DELAY_INFO_STRUCT info;

	ret = ioctl(fd, VIDIOC_MTK_G_DELAY_INFO, &info);
	if (ret) {
		printf("%s ret %d\n", __func__, ret);
		return ret;
	}

	printf("InitDelay %d\n", info.InitDelay);
	printf("EffectDelay %d\n", info.EffectDelay);
	printf("AwbDelay %d\n", info.AwbDelay);

	return 0;
}

#ifdef V4L2_CID_PD_PIXEL_REGION
static int test_pd_pixel_region(int fd)
{
	int ret;
	struct v4l2_ext_controls ext_ctrls;
	struct v4l2_ext_control ext_ctrl;
	struct v4l2_ctrl_image_pd_pixel_region region;

	memset(&ext_ctrls, 0, sizeof(ext_ctrls));
	memset(&ext_ctrl, 0, sizeof(ext_ctrl));

	ext_ctrls.count = 1;
	ext_ctrls.controls = &ext_ctrl;

	ext_ctrl.id = V4L2_CID_PD_PIXEL_REGION;
	ext_ctrl.size = sizeof(region);
	ext_ctrl.ptr = &region;

	ret = ioctl(fd, VIDIOC_G_EXT_CTRLS, &ext_ctrls);
	if (ret) {
		printf("%s ret %d errno %d\n", __func__, ret, errno);
		return ret;
	}

	printf("offset %d/%d pitch %d/%d blk_num %d/%d subblk %d/%d\n",
		region.offset_x, region.offset_y,
		region.pitch_x, region.pitch_y,
		region.blk_num_x, region.blk_num_y,
		region.subblk_w, region.subblk_h);

	return 0;
}
#endif

static int test_temperature(int fd)
{
	int ret;
	struct v4l2_ext_controls ext_ctrls;
	struct v4l2_ext_control ext_ctrl;

	memset(&ext_ctrls, 0, sizeof(ext_ctrls));
	memset(&ext_ctrl, 0, sizeof(ext_ctrl));

	ext_ctrls.count = 1;
	ext_ctrls.controls = &ext_ctrl;

	ext_ctrl.id = V4L2_CID_MTK_TEMPERATURE;

	ret = ioctl(fd, VIDIOC_G_EXT_CTRLS, &ext_ctrls);
	if (ret) {
		printf("%s ret %d errno %d\n", __func__, ret, errno);
		return ret;
	}

	printf("temperature %d\n", ext_ctrl.value);

	return 0;
}

static int test_anti_flicker(int fd, int en)
{
	int ret;
	struct v4l2_ext_controls ext_ctrls;
	struct v4l2_ext_control ext_ctrl;

	memset(&ext_ctrls, 0, sizeof(ext_ctrls));
	memset(&ext_ctrl, 0, sizeof(ext_ctrl));

	ext_ctrls.count = 1;
	ext_ctrls.controls = &ext_ctrl;

	ext_ctrl.id = V4L2_CID_MTK_ANTI_FLICKER;
	ext_ctrl.value = en;

	ret = ioctl(fd, VIDIOC_S_EXT_CTRLS, &ext_ctrls);
	if (ret) {
		printf("%s ret %d errno %d\n", __func__, ret, errno);
		return ret;
	}

	return 0;
}

static int test_shutter_gain_sync(int fd, __u32 shutter, __u32 gain)
{
	int ret;
	struct v4l2_ext_controls ext_ctrls;
	struct v4l2_ext_control ext_ctrl;
	struct mtk_shutter_gain_sync *info;

	info = malloc(sizeof(*info));
	info->shutter = shutter;
	info->gain = gain;

	memset(&ext_ctrls, 0, sizeof(ext_ctrls));
	memset(&ext_ctrl, 0, sizeof(ext_ctrl));

	ext_ctrls.count = 1;
	ext_ctrls.controls = &ext_ctrl;

	ext_ctrl.id = V4L2_CID_MTK_SHUTTER_GAIN_SYNC;
	ext_ctrl.size = sizeof(*info);
	ext_ctrl.ptr = info;

	ret = ioctl(fd, VIDIOC_S_EXT_CTRLS, &ext_ctrls);
	if (ret) {
		printf("%s ret %d errno %d\n", __func__, ret, errno);
		return ret;
	}

	free(info);

	return 0;
}

struct scenario_entry {
	int id;
	const char *name;
};

static const struct scenario_entry scenario_list[] = {
	{SENSOR_SCENARIO_ID_NORMAL_PREVIEW, "preview"},
	{SENSOR_SCENARIO_ID_NORMAL_CAPTURE, "capture"},
	{SENSOR_SCENARIO_ID_NORMAL_VIDEO, "video"},
};

int main(int argc, char *argv[])
{
	int fd, i;
	const struct scenario_entry *s;

	if (argc < 2) {
		printf("no device path\n");
		return -1;
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		printf("open %s fail\n", argv[1]);
		return -1;
	}

	/* ioctl (by scenario) */
	for (i = 0; i < ARRAY_SIZE(scenario_list); i++) {
		s = &scenario_list[i];
		printf("=== scenario %d : %s ===\n", s->id, s->name);
		test_crop_by_scenario(fd, s->id);
		test_vcinfo_by_scenario(fd, s->id);
		test_def_fps_by_scenario(fd, s->id);
		test_pclk_by_scenario(fd, s->id);
		test_llp_fll_by_scenario(fd, s->id);
		test_min_shutter_by_scenario(fd, s->id);
		test_gain_range_by_scenario(fd, s->id);
		test_pdaf_info_by_scenario(fd, s->id);
		test_binning_type(fd, s->id, 0);
		test_pdaf_cap(fd, s->id);
		test_hdr_cap(fd, s->id);
		test_mipi_pixel_rate(fd, s->id);
		test_scenario_combo_info(fd, s->id);
		printf("\n\n");
	}

	/* ioctl */
	test_pclk(fd);
	test_llp_fll(fd);
	test_test_pattern_checksum(fd);
	test_base_gain_iso_n_step(fd);
	test_offset_to_start_of_exposure(fd);
	test_ana_gain_table(fd);
	test_g_pdaf_regs(fd);
	test_g_feature_info(fd, 0);
	test_g_delay_info(fd);

	/* ctrls */
#ifdef V4L2_CID_PD_PIXEL_REGION
	test_pd_pixel_region(fd);
#endif
	test_temperature(fd);
	test_anti_flicker(fd, 1);
	test_shutter_gain_sync(fd, 1000, 64);

	return 0;
}

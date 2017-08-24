/* Copyright (c) 2016-2017 The Linux Foundation. All rights reserved.
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

#include <linux/pm_opp.h>
#include "a5xx_gpu.h"

/*
 * The GPMU data block is a block of shared registers that can be used to
 * communicate back and forth. These "registers" are by convention with the GPMU
 * firwmare and not bound to any specific hardware design
 */

#define AGC_INIT_BASE REG_A5XX_GPMU_DATA_RAM_BASE
#define AGC_INIT_MSG_MAGIC (AGC_INIT_BASE + 5)
#define AGC_MSG_BASE (AGC_INIT_BASE + 7)

#define AGC_MSG_STATE (AGC_MSG_BASE + 0)
#define AGC_MSG_COMMAND (AGC_MSG_BASE + 1)
#define AGC_MSG_PAYLOAD_SIZE (AGC_MSG_BASE + 3)
#define AGC_MSG_PAYLOAD(_o) ((AGC_MSG_BASE + 5) + (_o))

#define AGC_POWER_CONFIG_PRODUCTION_ID 1
#define AGC_INIT_MSG_VALUE 0xBABEFACE

/* AGC_LM_CONFIG (A540+) */
#define AGC_LM_CONFIG (136/4)
#define AGC_LM_CONFIG_GPU_VERSION_SHIFT 17
#define AGC_LM_CONFIG_ENABLE_GPMU_ADAPTIVE 1
#define AGC_LM_CONFIG_THROTTLE_DISABLE (2 << 8)
#define AGC_LM_CONFIG_ISENSE_ENABLE (1 << 4)
#define AGC_LM_CONFIG_ENABLE_ERROR (3 << 4)
#define AGC_LM_CONFIG_LLM_ENABLED (1 << 16)
#define AGC_LM_CONFIG_BCL_DISABLED (1 << 24)

#define AGC_LEVEL_CONFIG (140/4)

static struct {
	uint32_t reg;
	uint32_t value;
} a5xx_sequence_regs[] = {
	{ 0xB9A1, 0x00010303 },
	{ 0xB9A2, 0x13000000 },
	{ 0xB9A3, 0x00460020 },
	{ 0xB9A4, 0x10000000 },
	{ 0xB9A5, 0x040A1707 },
	{ 0xB9A6, 0x00010000 },
	{ 0xB9A7, 0x0E000904 },
	{ 0xB9A8, 0x10000000 },
	{ 0xB9A9, 0x01165000 },
	{ 0xB9AA, 0x000E0002 },
	{ 0xB9AB, 0x03884141 },
	{ 0xB9AC, 0x10000840 },
	{ 0xB9AD, 0x572A5000 },
	{ 0xB9AE, 0x00000003 },
	{ 0xB9AF, 0x00000000 },
	{ 0xB9B0, 0x10000000 },
	{ 0xB828, 0x6C204010 },
	{ 0xB829, 0x6C204011 },
	{ 0xB82A, 0x6C204012 },
	{ 0xB82B, 0x6C204013 },
	{ 0xB82C, 0x6C204014 },
	{ 0xB90F, 0x00000004 },
	{ 0xB910, 0x00000002 },
	{ 0xB911, 0x00000002 },
	{ 0xB912, 0x00000002 },
	{ 0xB913, 0x00000002 },
	{ 0xB92F, 0x00000004 },
	{ 0xB930, 0x00000005 },
	{ 0xB931, 0x00000005 },
	{ 0xB932, 0x00000005 },
	{ 0xB933, 0x00000005 },
	{ 0xB96F, 0x00000001 },
	{ 0xB970, 0x00000003 },
	{ 0xB94F, 0x00000004 },
	{ 0xB950, 0x0000000B },
	{ 0xB951, 0x0000000B },
	{ 0xB952, 0x0000000B },
	{ 0xB953, 0x0000000B },
	{ 0xB907, 0x00000019 },
	{ 0xB927, 0x00000019 },
	{ 0xB947, 0x00000019 },
	{ 0xB967, 0x00000019 },
	{ 0xB987, 0x00000019 },
	{ 0xB906, 0x00220001 },
	{ 0xB926, 0x00220001 },
	{ 0xB946, 0x00220001 },
	{ 0xB966, 0x00220001 },
	{ 0xB986, 0x00300000 },
	{ 0xAC40, 0x0340FF41 },
	{ 0xAC41, 0x03BEFED0 },
	{ 0xAC42, 0x00331FED },
	{ 0xAC43, 0x021FFDD3 },
	{ 0xAC44, 0x5555AAAA },
	{ 0xAC45, 0x5555AAAA },
	{ 0xB9BA, 0x00000008 },
};

/*
 * Get the actual voltage value for the operating point at the specified
 * frequency
 */
static inline uint32_t _get_mvolts(struct msm_gpu *gpu, uint32_t freq)
{
	struct drm_device *dev = gpu->dev;
	struct msm_drm_private *priv = dev->dev_private;
	struct platform_device *pdev = priv->gpu_pdev;
	struct dev_pm_opp *opp;

	opp = dev_pm_opp_find_freq_exact(&pdev->dev, freq, true);

	return (!IS_ERR(opp)) ? dev_pm_opp_get_voltage(opp) / 1000 : 0;
}

#define PAYLOAD_SIZE(_size) ((_size) * sizeof(u32))
#define LM_DCVS_LIMIT 1
#define LEVEL_CONFIG ~(0x303)

/* Setup thermal limit management for A540 */
static void a540_lm_setup(struct msm_gpu *gpu)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	u32 max_power = 0;
	u32 rate = gpu->gpufreq[gpu->active_level];
	u32 config;

	/* The battery current limiter isn't enabled for A540 */
	config = AGC_LM_CONFIG_BCL_DISABLED;
	config |= adreno_gpu->rev.patchid << AGC_LM_CONFIG_GPU_VERSION_SHIFT;

	/* For now disable GPMU side throttling */
	config |= AGC_LM_CONFIG_THROTTLE_DISABLE;

	/* Get the max-power from the device tree */
	of_property_read_u32(GPU_OF_NODE(gpu), "qcom,lm-max-power", &max_power);

	gpu_write(gpu, AGC_MSG_STATE, 0x80000001);
	gpu_write(gpu, AGC_MSG_COMMAND, AGC_POWER_CONFIG_PRODUCTION_ID);

	/*
	 * For now just write the one voltage level - we will do more when we
	 * can do scaling
	 */
	gpu_write(gpu, AGC_MSG_PAYLOAD(0), max_power);
	gpu_write(gpu, AGC_MSG_PAYLOAD(1), 1);

	gpu_write(gpu, AGC_MSG_PAYLOAD(2), _get_mvolts(gpu, rate));
	gpu_write(gpu, AGC_MSG_PAYLOAD(3), rate / 1000000);

	gpu_write(gpu, AGC_MSG_PAYLOAD(AGC_LM_CONFIG), config);
	gpu_write(gpu, AGC_MSG_PAYLOAD(AGC_LEVEL_CONFIG), LEVEL_CONFIG);
	gpu_write(gpu, AGC_MSG_PAYLOAD_SIZE,
		PAYLOAD_SIZE(AGC_LEVEL_CONFIG + 1));

	gpu_write(gpu, AGC_INIT_MSG_MAGIC, AGC_INIT_MSG_VALUE);
}

/* Setup thermal limit management for A530 */
static void a530_lm_setup(struct msm_gpu *gpu)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct a5xx_gpu *a5xx_gpu = to_a5xx_gpu(adreno_gpu);
	uint32_t rate = gpu->gpufreq[gpu->active_level];
	uint32_t tsens = 0;
	uint32_t max_power = 0;
	unsigned int i;

	/* Write the block of sequence registers */
	for (i = 0; i < ARRAY_SIZE(a5xx_sequence_regs); i++)
		gpu_write(gpu, a5xx_sequence_regs[i].reg,
			a5xx_sequence_regs[i].value);

	of_property_read_u32(GPU_OF_NODE(gpu), "qcom,gpmu-tsens", &tsens);

	gpu_write(gpu, REG_A5XX_GPMU_TEMP_SENSOR_ID, tsens);
	gpu_write(gpu, REG_A5XX_GPMU_DELTA_TEMP_THRESHOLD, 0x01);
	gpu_write(gpu, REG_A5XX_GPMU_TEMP_SENSOR_CONFIG, 0x01);

	gpu_write(gpu, REG_A5XX_GPMU_BASE_LEAKAGE, a5xx_gpu->lm_leakage);

	gpu_write(gpu, REG_A5XX_GPMU_BEC_ENABLE, 0x10001FFF);
	gpu_write(gpu, REG_A5XX_GDPM_CONFIG1, 0x00201FF1);

	/* Write the voltage table */

	/* Get the max-power from the device tree */
	of_property_read_u32(GPU_OF_NODE(gpu), "qcom,lm-max-power", &max_power);

	gpu_write(gpu, REG_A5XX_GPMU_BEC_ENABLE, 0x10001FFF);
	gpu_write(gpu, REG_A5XX_GDPM_CONFIG1, 0x201FF1);

	gpu_write(gpu, AGC_MSG_STATE, 1);
	gpu_write(gpu, AGC_MSG_COMMAND, AGC_POWER_CONFIG_PRODUCTION_ID);

	/*
	 * For now just write the one voltage level - we will do more when we
	 * can do scaling
	 */
	gpu_write(gpu, AGC_MSG_PAYLOAD(0), max_power);
	gpu_write(gpu, AGC_MSG_PAYLOAD(1), 1);

	gpu_write(gpu, AGC_MSG_PAYLOAD(2), _get_mvolts(gpu, rate));
	gpu_write(gpu, AGC_MSG_PAYLOAD(3), rate / 1000000);

	gpu_write(gpu, AGC_MSG_PAYLOAD_SIZE, 4 * sizeof(uint32_t));
	gpu_write(gpu, AGC_INIT_MSG_MAGIC, AGC_INIT_MSG_VALUE);
}

/* Enable SP/TP cpower collapse */
static void a5xx_pc_init(struct msm_gpu *gpu)
{
	gpu_write(gpu, REG_A5XX_GPMU_PWR_COL_INTER_FRAME_CTRL, 0x7F);
	gpu_write(gpu, REG_A5XX_GPMU_PWR_COL_BINNING_CTRL, 0);
	gpu_write(gpu, REG_A5XX_GPMU_PWR_COL_INTER_FRAME_HYST, 0xA0080);
	gpu_write(gpu, REG_A5XX_GPMU_PWR_COL_STAGGER_DELAY, 0x600040);
}

/* Enable the GPMU microcontroller */
static int a5xx_gpmu_init(struct msm_gpu *gpu)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct a5xx_gpu *a5xx_gpu = to_a5xx_gpu(adreno_gpu);
	struct msm_ringbuffer *ring = gpu->rb[0];

	if (!a5xx_gpu->gpmu_dwords)
		return 0;

	/* Turn off protected mode for this operation */
	OUT_PKT7(ring, CP_SET_PROTECTED_MODE, 1);
	OUT_RING(ring, 0);

	/* Kick off the IB to load the GPMU microcode */
	OUT_PKT7(ring, CP_INDIRECT_BUFFER_PFE, 3);
	OUT_RING(ring, lower_32_bits(a5xx_gpu->gpmu_iova));
	OUT_RING(ring, upper_32_bits(a5xx_gpu->gpmu_iova));
	OUT_RING(ring, a5xx_gpu->gpmu_dwords);

	/* Turn back on protected mode */
	OUT_PKT7(ring, CP_SET_PROTECTED_MODE, 1);
	OUT_RING(ring, 1);

	gpu->funcs->flush(gpu, ring);

	/* This is "fatal" because the CP is left in a bad state */
	if (!a5xx_idle(gpu, ring)) {
		DRM_ERROR("%s: Unable to load GPMU firmwaren",
			gpu->name);
		return -EINVAL;
	}

	/* Clock gating setup for A530 targets */
	if (adreno_is_a530(adreno_gpu))
		gpu_write(gpu, REG_A5XX_GPMU_WFI_CONFIG, 0x4014);

	/* Kick off the GPMU */
	gpu_write(gpu, REG_A5XX_GPMU_CM3_SYSRESET, 0x0);

	/*
	 * Wait for the GPMU to respond. It isn't fatal if it doesn't, we just
	 * won't have advanced power collapse.
	 */
	if (spin_usecs(gpu, 25, REG_A5XX_GPMU_GENERAL_0, 0xFFFFFFFF,
		0xBABEFACE)) {
		DRM_ERROR("%s: GPMU firmware initialization timed out\n",
			gpu->name);
		return 0;
	}

	if (!adreno_is_a530(adreno_gpu)) {
		u32 val = gpu_read(gpu, REG_A5XX_GPMU_GENERAL_1);

		if (val)
			DRM_ERROR("%s: GPMU firmare initialization failed: %d\n",
				gpu->name, val);
	}

	/* FIXME: Clear GPMU interrupts? */
	return 0;
}

/* Enable limits management */
static void a5xx_lm_enable(struct msm_gpu *gpu)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);

	/* This init sequence only applies to A530 */
	if (!adreno_is_a530(adreno_gpu))
		return;

	gpu_write(gpu, REG_A5XX_GDPM_INT_MASK, 0x0);
	gpu_write(gpu, REG_A5XX_GDPM_INT_EN, 0x0A);
	gpu_write(gpu, REG_A5XX_GPMU_GPMU_VOLTAGE_INTR_EN_MASK, 0x01);
	gpu_write(gpu, REG_A5XX_GPMU_TEMP_THRESHOLD_INTR_EN_MASK, 0x50000);
	gpu_write(gpu, REG_A5XX_GPMU_THROTTLE_UNMASK_FORCE_CTRL, 0x30000);

	gpu_write(gpu, REG_A5XX_GPMU_CLOCK_THROTTLE_CTRL, 0x011);
}

int a5xx_power_init(struct msm_gpu *gpu)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	int ret;
	u32 lm_limit = 6000;

	/*
	 * Set up the limit management
	 * first, do some generic setup:
	 */
	gpu_write(gpu, REG_A5XX_GPMU_GPMU_VOLTAGE, 0x80000000 | 0);

	of_property_read_u32(GPU_OF_NODE(gpu), "qcom,lm-limit", &lm_limit);
	gpu_write(gpu, REG_A5XX_GPMU_GPMU_PWR_THRESHOLD, 0x80000000 | lm_limit);

	/* Now do the target specific setup */
	if (adreno_is_a530(adreno_gpu))
		a530_lm_setup(gpu);
	else
		a540_lm_setup(gpu);

	/* Set up SP/TP power collpase */
	a5xx_pc_init(gpu);

	/* Start the GPMU */
	ret = a5xx_gpmu_init(gpu);
	if (ret)
		return ret;

	/* Start the limits management */
	a5xx_lm_enable(gpu);

	return 0;
}

static int _read_header(unsigned int *data, uint32_t fwsize,
		unsigned int *major, unsigned int *minor)
{
	uint32_t size;
	unsigned int i;

	/* First dword of the header is the header size */
	if (fwsize < 4)
		return -EINVAL;

	size = data[0];

	/* Make sure the header isn't too big and is a multiple of two */
	if ((size % 2) || (size > 10) || size > (fwsize >> 2))
		return -EINVAL;

	/* Read the values in pairs */
	for (i = 1; i < size; i += 2) {
		switch (data[i]) {
		case 1:
			*major = data[i + 1];
			break;
		case 2:
			*minor = data[i + 1];
			break;
		default:
			/* Invalid values are non fatal */
			break;
		}
	}

	return 0;
}

/*
 * Make sure cur_major and cur_minor are greater than or equal to the minimum
 * allowable major/minor
 */
static inline bool _check_gpmu_version(uint32_t cur_major, uint32_t cur_minor,
		uint32_t min_major, uint32_t min_minor)
{
	return ((cur_major > min_major) ||
		((cur_major == min_major) && (cur_minor >= min_minor)));
}

void a5xx_gpmu_ucode_init(struct msm_gpu *gpu)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct a5xx_gpu *a5xx_gpu = to_a5xx_gpu(adreno_gpu);
	struct drm_device *drm = gpu->dev;
	const char *name;
	const struct firmware *fw;
	uint32_t version[2] = { 0, 0 };
	uint32_t dwords = 0, offset = 0;
	uint32_t major = 0, minor = 0, bosize;
	unsigned int *data, *ptr, *cmds;
	unsigned int cmds_size;

	if (a5xx_gpu->gpmu_bo)
		return;

	/*
	 * Read the firmware name from the device tree - if it doesn't exist
	 * then don't initialize the GPMU for this target
	 */
	if (of_property_read_string(GPU_OF_NODE(gpu), "qcom,gpmu-firmware",
		&name))
		return;

	/*
	 * The version isn't mandatory, but if it exists, we need to enforce
	 * that the version of the GPMU firmware matches or is newer than the
	 * value
	 */
	of_property_read_u32_array(GPU_OF_NODE(gpu), "qcom,gpmu-version",
		version, 2);

	/* Get the firmware */
	if (request_firmware(&fw, name, drm->dev)) {
		DRM_ERROR("%s: Could not get GPMU firmware. GPMU will not be active\n",
			gpu->name);
		return;
	}

	data = (unsigned int *) fw->data;

	/*
	 * The first dword is the size of the remaining data in dwords. Use it
	 * as a checksum of sorts and make sure it matches the actual size of
	 * the firmware that we read
	 */

	if (fw->size < 8 || (data[0] < 2) || (data[0] >= (fw->size >> 2)))
		goto out;

	/* The second dword is an ID - look for 2 (GPMU_FIRMWARE_ID) */
	if (data[1] != 2)
		goto out;

	/* Read the header and get the major/minor of the read firmware */
	if (_read_header(&data[2], fw->size - 8, &major, &minor))
		goto out;

	if (!_check_gpmu_version(major, minor, version[0], version[1])) {
		DRM_ERROR("%s: Loaded GPMU version %d.%d is too old\n",
			gpu->name, major, minor);
		goto out;
	}

	cmds = data + data[2] + 3;
	cmds_size = data[0] - data[2] - 2;

	/*
	 * A single type4 opcode can only have so many values attached so
	 * add enough opcodes to load the all the commands
	 */
	bosize = (cmds_size + (cmds_size / TYPE4_MAX_PAYLOAD) + 1) << 2;

	ptr = msm_gem_kernel_new(drm, bosize,
		MSM_BO_UNCACHED | MSM_BO_GPU_READONLY, gpu->aspace,
		&a5xx_gpu->gpmu_bo, &a5xx_gpu->gpmu_iova);
	if (IS_ERR(ptr))
		goto err;

	while (cmds_size > 0) {
		int i;
		uint32_t _size = cmds_size > TYPE4_MAX_PAYLOAD ?
			TYPE4_MAX_PAYLOAD : cmds_size;

		ptr[dwords++] = PKT4(REG_A5XX_GPMU_INST_RAM_BASE + offset,
			_size);

		for (i = 0; i < _size; i++)
			ptr[dwords++] = *cmds++;

		offset += _size;
		cmds_size -= _size;
	}

	a5xx_gpu->gpmu_dwords = dwords;

	goto out;

err:
	if (a5xx_gpu->gpmu_iova)
		msm_gem_put_iova(a5xx_gpu->gpmu_bo, gpu->aspace);
	if (a5xx_gpu->gpmu_bo)
		drm_gem_object_unreference_unlocked(a5xx_gpu->gpmu_bo);

	a5xx_gpu->gpmu_bo = NULL;
	a5xx_gpu->gpmu_iova = 0;
	a5xx_gpu->gpmu_dwords = 0;

out:
	/* No need to keep that firmware laying around anymore */
	release_firmware(fw);
}

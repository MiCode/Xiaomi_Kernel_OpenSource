/*
 *Copyright 2015 NXP Semiconductors
 *
 *Licensed under the Apache License, Version 2.0 (the "License");
 *you may not use this file except in compliance with the License.
 *You may obtain a copy of the License at
 *
 *http://www.apache.org/licenses/LICENSE-2.0
 *
 *Unless required by applicable law or agreed to in writing, software
 *distributed under the License is distributed on an "AS IS" BASIS,
 *WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *See the License for the specific language governing permissions and
 *limitations under the License.
 */

#include "config.h"

#include "tfa98xx_tfafieldnames.h"
#include "tfa_internal.h"
#include "tfa.h"
#include "tfa_service.h"
#include "tfa_container.h"
#include "tfa_dsp_fw.h"
/* TODO: remove genregs usage? */
#include "tfa98xx_genregs_N1C.h"

/* handle macro for bitfield */
#define TFA_MK_BF(reg, pos, len) ((reg<<8)|(pos<<4)|(len-1))

/* abstract family for register */
#define FAM_TFA98XX_CF_CONTROLS (TFA_FAM(handle, RST) >> 8)
#define FAM_TFA98XX_CF_MEM      (TFA_FAM(handle, MEMA) >> 8)
#define FAM_TFA98XX_MTP0        (TFA_FAM(handle, MTPOTC) >> 8)
#define FAM_TFA98xx_INT_EN      (TFA_FAM(handle, INTENVDDS) >> 8)

#define CF_STATUS_I2C_CMD_ACK 0x01

#if (defined(TFA9888) || defined(TFA98XX_FULL))
void tfa9888_ops(struct tfa_device_ops *ops);
#endif
#if (defined(TFA9891) || defined(TFA98XX_FULL))
void tfa9891_ops(struct tfa_device_ops *ops);
#endif
#if (defined(TFA9897) || defined(TFA98XX_FULL))
void tfa9897_ops(struct tfa_device_ops *ops);
#endif
#if (defined(TFA9890) || defined(TFA98XX_FULL))
void tfa9890_ops(struct tfa_device_ops *ops);
#endif
#if (defined(TFA9887B) || defined(TFA98XX_FULL))
int tfa9887B_is87(Tfa98xx_handle_t handle);
void tfa9887B_ops(struct tfa_device_ops *ops);
#endif
#if (defined(TFA9887) || defined(TFA98XX_FULL))
void tfa9887_ops(struct tfa_device_ops *ops);
#endif

#ifndef MIN
#define MIN(A, B) (A < B ? A : B)
#endif

/* retry values */
#define CFSTABLE_TRIES   10
#define PWDNWAIT_TRIES   50
#define AMPOFFWAIT_TRIES 50
#define MTPBWAIT_TRIES   50
#define MTPEX_WAIT_NTRIES 25

static int tfa98xx_runtime_verbose;
static int tfa98xx_trace_level;

extern struct tfa98xx *getHandle(int dev);
extern void tfa98xx_dump_register(struct tfa98xx *tfa98xx, int level, char *msg);

/* 4 possible I2C addresses
 */
#define MAX_HANDLES 4
TFA_INTERNAL struct Tfa98xx_handle_private handles_local[MAX_HANDLES];

/*
 * static functions
 */

TFA_INTERNAL int tfa98xx_handle_is_open(Tfa98xx_handle_t h)
{
	int retval = 0;

	if ((h >= 0) && (h < MAX_HANDLES))
		retval = handles_local[h].in_use != 0;

	return retval;
}

int print_calibration(Tfa98xx_handle_t handle, char *str, size_t size)
{
	return snprintf(str, size, " Prim:%d mOhms, Sec:%d mOhms\n",
				handles_local[handle].mohm[0],
				handles_local[handle].mohm[1]);
}

int tfa_get_calibration_info(Tfa98xx_handle_t handle, int channel)
{
	return handles_local[handle].mohm[channel];
}
/*
 * open
 */

/*
 *  set device info and register device ops
 */
static void tfa_set_query_info(int dev_idx)
{
	if (dev_idx > MAX_HANDLES) {
		_ASSERT(dev_idx >= MAX_HANDLES);
		return;
	}

	/* invalidate  device struct cached values */
	handles_local[dev_idx].hw_feature_bits = -1;
	handles_local[dev_idx].sw_feature_bits[0] = -1;
	handles_local[dev_idx].sw_feature_bits[1] = -1;
	handles_local[dev_idx].profile = -1;
	handles_local[dev_idx].vstep[0] = -1;
	handles_local[dev_idx].vstep[1] = -1;
	/* defaults */
	handles_local[dev_idx].tfa_family = 1;
	handles_local[dev_idx].daimap = Tfa98xx_DAI_I2S;		/* all others */
	handles_local[dev_idx].spkr_count = 1;
	handles_local[dev_idx].spkr_select = 0;
	handles_local[dev_idx].support_tcoef = supportYes;
	handles_local[dev_idx].supportDrc = supportNotSet;
	handles_local[dev_idx].support_saam = supportNotSet;

	/* TODO use the getfeatures() for retrieving the features [artf103523]
	handles_local[dev_idx].supportDrc = supportNotSet;*/

	switch (handles_local[dev_idx].rev & 0xff) {
	case 0x88:
		/* tfa9888 */
		handles_local[dev_idx].tfa_family = 2;
		handles_local[dev_idx].spkr_count = 2;
		handles_local[dev_idx].daimap = Tfa98xx_DAI_TDM;
		tfa9888_ops(&handles_local[dev_idx].dev_ops); /* register device operations */
		break;
	case 0x97:
		/* tfa9897 */
		handles_local[dev_idx].supportDrc = supportNo;
		handles_local[dev_idx].spkr_count = 1;
		handles_local[dev_idx].daimap = Tfa98xx_DAI_TDM;
		tfa9897_ops(&handles_local[dev_idx].dev_ops); /* register device operations */
		break;
	case 0x92:
		/* tfa9891 */
		handles_local[dev_idx].spkr_count = 1;
		handles_local[dev_idx].daimap = (Tfa98xx_DAI_PDM | Tfa98xx_DAI_I2S);
		tfa9891_ops(&handles_local[dev_idx].dev_ops); /* register device operations */
		break;
	case 0x91:
		/* tfa9890B */
		handles_local[dev_idx].spkr_count = 1;
		handles_local[dev_idx].daimap = (Tfa98xx_DAI_PDM | Tfa98xx_DAI_I2S);
		break;
	case 0x80:
	case 0x81:
		/* tfa9890 */
		handles_local[dev_idx].spkr_count = 1;
		handles_local[dev_idx].daimap = Tfa98xx_DAI_I2S;
		handles_local[dev_idx].supportDrc = supportNo;
		handles_local[dev_idx].supportFramework = supportNo;
		tfa9890_ops(&handles_local[dev_idx].dev_ops); /* register device operations */
		break;
	case 0x12:
		/* tfa9887 / tfa9887B / tfa9895 */
		handles_local[dev_idx].spkr_count = 1;
		handles_local[dev_idx].daimap = Tfa98xx_DAI_I2S;
		if (tfa9887B_is87(dev_idx)) {
			handles_local[dev_idx].support_tcoef = supportNo;
			tfa9887_ops(&handles_local[dev_idx].dev_ops); /* register device operations */
		} else
			tfa9887B_ops(&handles_local[dev_idx].dev_ops); /* register device operations */
		break;
	default:
		pr_err("unknown device type : 0x%02x\n", handles_local[dev_idx].rev);
		_ASSERT(0);
		break;
	}
}

/*
 * lookup the device type and return the family type
 */
int tfa98xx_dev2family(int dev_type)
{
	/* only look at the die ID part (lsb byte) */
	switch (dev_type & 0xff) {
	case 0x12:
	case 0x80:
	case 0x81:
	case 0x91:
	case 0x92:
	case 0x97:
		return 1;
	case 0x88:
		return 2;
	case 0x50:
		return 3;
	default:
		return 0;
	}
}

int tfa98xx_dev_family(Tfa98xx_handle_t dev_idx)
{
	return handles_local[dev_idx].tfa_family;
}

unsigned short tfa98xx_dev_revision(Tfa98xx_handle_t dev_idx)
{
	return handles_local[dev_idx].rev;
}

void tfa98xx_set_spkr_select(Tfa98xx_handle_t dev_idx, char *configuration)
{
	 char firstLetter;

	/* 4=Left, 2=Right, 1=none, 0=default */
	if (configuration == NULL)
		handles_local[dev_idx].spkr_select = 0;
	else {
		firstLetter = (char)tolower((unsigned char)configuration[0]);
		switch (firstLetter) {
		case 'b': /* SC / both -> apply primary also to secondary */
			handles_local[dev_idx].spkr_select = 8;
			handles_local[dev_idx].spkr_count = 2;
			break;
		case 'l':
		case 'p': /* DS / left -> only use primary channel */
			handles_local[dev_idx].spkr_select = 4;
			handles_local[dev_idx].spkr_count = 1;
			break;
		case 'r':
		case 's': /* DP / right -> only use secondary channel */
			handles_local[dev_idx].spkr_select = 2;
			handles_local[dev_idx].spkr_count = 1;
			break;
		case 'd': /* DC / disable -> skip applying configuration for both */
			handles_local[dev_idx].spkr_select = 1;
			handles_local[dev_idx].spkr_count = 2;
			break;
		default:
			handles_local[dev_idx].spkr_select = 0;
			handles_local[dev_idx].spkr_count = 2;
			break;
		}
	}
}

void tfa_mock_probe(int dev_idx, unsigned short revid, int slave_address)
{
	handles_local[dev_idx].slave_address = (unsigned char)slave_address*2;
	handles_local[dev_idx].rev = revid;
	tfa_set_query_info(dev_idx);
}

enum Tfa98xx_Error tfa_soft_probe(int dev_idx, int revid)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	error = tfaContGetSlave(dev_idx, &handles_local[dev_idx].slave_address);
	handles_local[dev_idx].slave_address *= 2;
	if (error)
		return error;

	handles_local[dev_idx].rev = (unsigned short)revid;
	tfa_set_query_info(dev_idx);

	return error;
}

/*
 * TODO The slave/cnt check in tfa98xx_register_dsp() should be done here in tfa_probe()
 */
enum Tfa98xx_Error tfa_probe(unsigned char slave_address,
								Tfa98xx_handle_t *pHandle)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	int rev;
	int idx;

	_ASSERT(pHandle != NULL);
	*pHandle = -1;

	/* when available select index used in container file */
	idx = tfa98xx_cnt_slave2idx(slave_address>>1);
	if (idx < 0)
		idx = 0; /* when no container file, use first instance */

	if (handles_local[idx].in_use == 1)
		return Tfa98xx_Error_InUse;

	handles_local[idx].in_use = 1;

	switch (slave_address) {
	case TFA98XX_GENERIC_SLAVE_ADDRESS:     /* same as (0x0E<<1) test adr */
	case 0x68:
	case 0x6A:
	case 0x6C:
	case 0x6E:
	case (0x1a<<1): /* TODO properly implement foreign i2c addressing like uda1355 */
		handles_local[idx].buffer_size =  NXP_I2C_BufferSize();
		handles_local[idx].slave_address = slave_address;
/* TODO: how do we deal with old bugs? */
#if (defined(TFA9887)  || defined (TFA9887B) || defined(TFA98XX_FULL))
		/* do a dummy read in order to generate
		 * i2c clocks when accessing the device
		 * for the first time
		 */
		rev = TFA_READ_REG(idx, REV);
#endif
		/* this can be the very first read, so check error here */
		rev = TFA_READ_REG(idx, REV);
		if (rev < 0) /* returns negative if error */
			error = -rev;
		if (Tfa98xx_Error_Ok != error) {
			handles_local[idx].in_use = 0;
			pr_debug("\nError: Unable to read revid from slave:0x%02x\n", slave_address / 2);
			return error;
		}
		handles_local[idx].rev = (unsigned short) rev;
		*pHandle = idx;
		error = Tfa98xx_Error_Ok;
#ifdef __KERNEL__ /* don't spam userspace with information */
		tfa98xx_trace_printk("slave:0x%02x revid:0x%04x\n", slave_address, rev);
		pr_debug("slave:0x%02x revid:0x%04x\n", slave_address, rev);
#endif
		break;
	default:
		pr_info("Unknown slave adress! \n");
		/* wrong slave address */
		error = Tfa98xx_Error_Bad_Parameter;
	}

	tfa_set_query_info(idx);

	handles_local[idx].in_use = 0;

	return error;
}

enum Tfa98xx_Error
tfa98xx_open(Tfa98xx_handle_t handle)
{
	if (tfa98xx_handle_is_open(handle)) {
		return Tfa98xx_Error_InUse;
	} else {
		handles_local[handle].in_use = 1;
		return Tfa98xx_Error_Ok;
	}
}

/*
 * close
 */
enum Tfa98xx_Error tfa98xx_close(Tfa98xx_handle_t handle)
{
	if (tfa98xx_handle_is_open(handle)) {
		handles_local[handle].in_use = 0;
		return Tfa98xx_Error_Ok;
	} else {
		return Tfa98xx_Error_NotOpen;
	}
}

/*
 * 	return the target address for the filter on this device

  filter_index:
	[0..9] reserved for EQ (not deployed, calc. is available)
	[10..12] anti-alias filter
	[13]  integrator filter

 */
enum Tfa98xx_DMEM tfa98xx_filter_mem(Tfa98xx_handle_t dev, int filter_index, unsigned short *address, int channel)
{
	enum Tfa98xx_DMEM  dmem = -1;
	int idx;
	unsigned short bq_table[7][4] = {
	/* index: 10, 11, 12, 13 */
		{346, 351, 356, 288}, /* 87 BRA_MAX_MRA4-2_7.00 */
		{346, 351, 356, 288}, /* 90 BRA_MAX_MRA6_9.02 */
		{467, 472, 477, 409}, /* 95 BRA_MAX_MRA7_10.02 */
		{406, 411, 416, 348}, /* 97 BRA_MAX_MRA9_12.01 */
		{467, 472, 477, 409}, /* 91 BRA_MAX_MRAA_13.02 */
		{8832, 8837, 8842, 8847}, /* 88 part1 */
		{8853, 8858, 8863, 8868}  /* 88 part2 */
		/* Since the 88 is stereo we have 2 parts.
		 * Every index has 5 values except index 13 this one has 6 values
		 */
	};

	if ((filter_index >= 10) && (filter_index <= 13)) {
		dmem = Tfa98xx_DMEM_YMEM; /* for all devices */
		idx = filter_index-10;

		switch (handles_local[dev].rev & 0xff) { /* only compare lower byte */
		case 0x12:
			if (tfa9887B_is87(dev))
				*address = bq_table[0][idx];
			else
				*address = bq_table[2][idx];
			break;
		case 0x97:
			*address = bq_table[3][idx];
			break;
		case 0x80:
		case 0x81:  /* for the RAM version */
		case 0x91:
			*address = bq_table[1][idx];
			break;
		case 0x92:
			*address = bq_table[4][idx];
			break;
		case 0x88:
			/* Channel 1 = primary, 2 = secondary */
			if (channel == 1)
				*address = bq_table[5][idx];
			else
				*address = bq_table[6][idx];
			break;
		default:
			/* unsupported case, possibly intermediate version */
			return -EPERM;
			_ASSERT(0);
		}
	}
	return dmem;
}

/************************ query functions *****************/
/* no device involved */
/**
 * return revision
 */
void tfa98xx_rev(int *major, int *minor, int *revision)
{
	*major = TFA98XX_API_REV_MAJOR;
	*minor = TFA98XX_API_REV_MINOR;
	*revision = TFA98XX_API_REV_REVISION;
}

/**
 * Return the maximum nr of devices (SC39786)
 */
int tfa98xx_max_devices(void) /* TODO get from cnt (now only called from contOpen) */
{
	return MAX_HANDLES;
}

/* return the device revision id
 */
unsigned short tfa98xx_get_device_revision(Tfa98xx_handle_t handle)
{
	/* local function. Caller must make sure handle is valid */
	return handles_local[handle].rev;
}

/**
 * return the device digital audio interface (DAI) type bitmap
 */
enum Tfa98xx_DAI tfa98xx_get_device_dai(Tfa98xx_handle_t handle)
{
	/* local function. Caller must make sure handle is valid */
	return handles_local[handle].daimap;
}

/**
 * get the feature bits from the DSP
 *  - the older tfa9887 does not support this getfeature and
 *    also no tcoef support so we use this as the info for returning
 *    this feature
 */
enum Tfa98xx_Error tfa98xx_dsp_support_tcoef(Tfa98xx_handle_t dev_idx,
		int *pb_support_tCoef)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	/* if already set return that state , always assume supported */
	if (handles_local[dev_idx].support_tcoef != supportNotSet)
		*pb_support_tCoef = (handles_local[dev_idx].support_tcoef == supportYes);

	handles_local[dev_idx].support_tcoef = *pb_support_tCoef ? supportYes : supportNo;

	return error;
}

/**
 * tfa98xx_supported_speakers
 *  returns the number of the supported speaker count
 */
enum Tfa98xx_Error tfa98xx_supported_speakers(Tfa98xx_handle_t handle, int *spkr_count)
{
	if (tfa98xx_handle_is_open(handle)) {
		*spkr_count = handles_local[handle].spkr_count;
	} else
		return Tfa98xx_Error_NotOpen;

	return Tfa98xx_Error_Ok;
}

/**
 * tfa98xx_supported_dai
 *  returns the bitmap of the supported Digital Audio Interfaces
 */
enum Tfa98xx_Error tfa98xx_supported_dai(Tfa98xx_handle_t handle, enum Tfa98xx_DAI *daimap)
{
	if (tfa98xx_handle_is_open(handle)) {
		*daimap = handles_local[handle].daimap;
	} else
		return Tfa98xx_Error_NotOpen;

	return Tfa98xx_Error_Ok;
}

/*
 * tfa98xx_supported_saam
 *  returns the supportedspeaker as microphone feature
 */
enum Tfa98xx_Error tfa98xx_supported_saam(Tfa98xx_handle_t handle, enum Tfa98xx_saam *saam)
{
	int features;
	enum Tfa98xx_Error error;

	if (handles_local[handle].support_saam == supportNotSet) {
		error = tfa98xx_dsp_get_hw_feature_bits(handle, &features);
		if (error != Tfa98xx_Error_Ok)
			return error;
		handles_local[handle].support_saam =
				(features & 0x8000) ? supportYes : supportNo; /* SAAM is bit15 */
	}
	*saam = handles_local[handle].support_saam == supportYes ? Tfa98xx_saam : Tfa98xx_saam_none ;

	return Tfa98xx_Error_Ok;
}

/*
 * tfa98xx_compare_features
 *  Obtains features_from_MTP and features_from_cnt
 */
enum Tfa98xx_Error tfa98xx_compare_features(Tfa98xx_handle_t handle, int features_from_MTP[3], int features_from_cnt[3])
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	uint32_t value;
	uint16_t mtpbf;
	unsigned char bytes[3 * 2];

	/* int sw_feature_bits[2]; *//* cached feature bits data */
	/* int hw_feature_bits; *//* cached feature bits data */

	/* Nothing to test without clock: */
	int status;

	tfa98xx_dsp_system_stable(handle, &status);
	if (!status)
		return Tfa98xx_Error_NoClock; /* Only test when we have a clock. */

	/* Set proper MTP location per device: */
	if (tfa98xx_dev_family(handle) == 1)
		mtpbf = 0x850f;  /* MTP5 for tfa1,16 bits */
	else
		mtpbf = 0xf907;  /* MTP9 for tfa2, 8 bits */

	/* Read HW features from MTP: */
	value = tfa_read_reg(handle, mtpbf) & 0xffff;
	features_from_MTP[0] = handles_local[handle].hw_feature_bits = value;

	/* Read SW features: */
	error = tfa_dsp_cmd_id_write_read(handle, MODULE_FRAMEWORK,
			FW_PAR_ID_GET_FEATURE_INFO, sizeof(bytes), bytes);
	if (error != Tfa98xx_Error_Ok)
		return error; /* old ROM code may respond with Tfa98xx_Error_RpcParamId */
	tfa98xx_convert_bytes2data(sizeof(bytes), bytes, &features_from_MTP[1]);

	/* check if feature bits from MTP match feature bits from cnt file: */
	get_hw_features_from_cnt(handle, &features_from_cnt[0]);
	get_sw_features_from_cnt(handle, &features_from_cnt[1]);

	return error;
}

/********************************* device specific ops ************************************************/
/* the wrapper for DspReset, in case of full */
enum Tfa98xx_Error tfa98xx_dsp_reset(Tfa98xx_handle_t dev_idx, int state)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	if (tfa98xx_handle_is_open(dev_idx)) {
		if (handles_local[dev_idx].dev_ops.tfa_dsp_reset)
			error = (*handles_local[dev_idx].dev_ops.tfa_dsp_reset)(dev_idx, state);
		else
			/* generic function */
			TFA_SET_BF_VOLATILE(dev_idx, RST, (uint16_t)state);
	}
	return error;
}

/* tfa98xx_dsp_system_stable
 *  return: *ready = 1 when clocks are stable to allow DSP subsystem access
 */

/* This is the clean, default static
 */
static enum Tfa98xx_Error _tfa98xx_dsp_system_stable(Tfa98xx_handle_t handle,
						int *ready)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	unsigned short status;
	int value;

	/* check the contents of the STATUS register */
	value = TFA_READ_REG(handle, AREFS);
	if (value < 0) {
		error = -value;
		*ready = 0;
		_ASSERT(error);		/* an error here can be fatal */
		return error;
	}
	status = (unsigned short)value;

	/* check AREFS and CLKS and MTPB: not ready if either is clear */
	*ready = !((TFA_GET_BF_VALUE(handle, AREFS, status) == 0)
				|| (TFA_GET_BF_VALUE(handle, CLKS, status) == 0)
				|| (TFA_GET_BF_VALUE(handle, MTPB, status) == 1));

	return error;
}

/* deferred calibration */
void tfa98xx_apply_deferred_calibration(Tfa98xx_handle_t handle)
{
	struct tfa98xx_controls *controls = &(handles_local[handle].dev_ops.controls);
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	unsigned short value;

	if (controls->otc.deferrable && controls->otc.triggered) {
		pr_debug("Deferred writing otc = %d\n", controls->otc.wr_value);
		err = tfa98xx_set_mtp(handle,
			(uint16_t)controls->otc.wr_value << TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_POS,
			1 << TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_POS);
		if (err != Tfa98xx_Error_Ok) {
			pr_err("Unable to apply deferred MTP OTC write. Error=%d\n",
									err);
		} else {
			controls->otc.triggered = false;
			controls->otc.rd_valid = true;
			err = tfa98xx_get_mtp(handle, &value);
			if (err == Tfa98xx_Error_Ok)
				controls->otc.rd_value =
					(value & TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_MSK)
					>> TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_POS;
			else
				controls->otc.rd_value = controls->otc.wr_value;
		}
	}

	if (controls->mtpex.deferrable && controls->mtpex.triggered) {
		pr_debug("Deferred writing mtpex = %d\n", controls->mtpex.wr_value);
		err = tfa98xx_set_mtp(handle,
			(uint16_t)controls->mtpex.wr_value << TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_POS,
			1 << TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_POS);
		if (err != Tfa98xx_Error_Ok) {
			pr_err("Unable to apply deferred MTPEX write. Rrror=%d\n",
									err);
		} else {
			controls->mtpex.triggered = false;
			controls->mtpex.rd_valid = true;
			err = tfa98xx_get_mtp(handle, &value);
			if (err == Tfa98xx_Error_Ok)
				controls->mtpex.rd_value =
					(value & TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_MSK)
					>> TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_POS;
			else
				controls->mtpex.rd_value = controls->mtpex.wr_value;
		}
	}

	if (controls->calib.triggered) {
		err = tfa_calibrate(handle);
		if (err) {
			pr_info("Deferred calibration failed: %d\n", err);
		} else {
			pr_debug("Deferred calibration ok\n");
			controls->calib.triggered = false;
		}
	}
}

/* the ops wrapper for tfa98xx_dsp_SystemStable */
enum Tfa98xx_Error tfa98xx_dsp_system_stable(Tfa98xx_handle_t dev_idx, int *ready)
{
	enum Tfa98xx_Error error;

	if (!tfa98xx_handle_is_open(dev_idx))
		return Tfa98xx_Error_NotOpen;

	if (handles_local[dev_idx].dev_ops.tfa_dsp_system_stable)
		error = (*handles_local[dev_idx].dev_ops.tfa_dsp_system_stable)(dev_idx, ready);
	else
		/* generic function */
		error = _tfa98xx_dsp_system_stable(dev_idx, ready);
	return error;
}

/* the ops wrapper for tfa98xx_dsp_SystemStable */
int tfa98xx_cf_enabled(Tfa98xx_handle_t dev_idx)
{
	if (!tfa98xx_handle_is_open(dev_idx))
		return Tfa98xx_Error_NotOpen;

	return TFA_GET_BF(dev_idx, CFE);
}


/*
 * bring the device into a state similar to reset
 */
enum Tfa98xx_Error tfa98xx_init(Tfa98xx_handle_t handle)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	uint16_t value = 0;

	if (!tfa98xx_handle_is_open(handle))
		return Tfa98xx_Error_NotOpen;

	/* reset all i2C registers to default
	 *  Write the register directly to avoid the read in the bitfield function.
	 *  The I2CR bit may overwrite the full register because it is reset anyway.
	 *  This will save a reg read transaction.
	 */
	TFA_SET_BF_VALUE(handle, I2CR, 1, &value);
	TFA_WRITE_REG(handle, I2CR, value);

	if (tfa98xx_dev_family(handle) == 2) {
		/* restore MANSCONF and MANCOLD to POR state */
		TFA_SET_BF_VOLATILE(handle, MANSCONF, 0);
		TFA_SET_BF_VOLATILE(handle, MANCOLD, 1);
	} else {
		/* Mark TFA1 family chips OTC and MTPEX calibration accesses
		 * as deferrable, since these registers cannot be accesed
		 * while the I2S link is not up and running
		 */
		handles_local[handle].dev_ops.controls.otc.deferrable = true;
		handles_local[handle].dev_ops.controls.mtpex.deferrable = true;
	}
	switch (TFA_GET_BF(handle, REV) & 0xff) {
	case 0x80:
		pr_debug("tfa98xx_init Dev ID %x\n", (TFA_GET_BF(handle, REV) & 0xff));
		break;
	default:
		tfa98xx_dsp_reset(handle, 1); /* in pair of tfaRunStartDSP() */
	}

	/* some other registers must be set for optimal amplifier behaviour
	 * This is implemented in a file specific for the type number
	 */

	if (handles_local[handle].dev_ops.tfa_init)
		error = (*handles_local[handle].dev_ops.tfa_init)(handle);

	return error;
}

enum Tfa98xx_Error tfa98xx_dsp_write_tables(Tfa98xx_handle_t handle, int sample_rate)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	if (!tfa98xx_handle_is_open(handle))
		return Tfa98xx_Error_NotOpen;

	if (handles_local[handle].dev_ops.tfa_dsp_write_tables)
		error = (*handles_local[handle].dev_ops.tfa_dsp_write_tables)(handle, sample_rate);

	return error;
}

/********************* new tfa2 *********************************************************************/
/* newly added messaging for tfa2 tfa1? */
enum Tfa98xx_Error tfa98xx_dsp_get_memory(Tfa98xx_handle_t handle, int memoryType,
		int offset, int length, unsigned char bytes[])
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	char msg[12];

	msg[0] = 8;
	msg[1] = MODULE_FRAMEWORK + 128;
	msg[2] = FW_PAR_ID_GET_MEMORY;

	msg[3] = 0;
	msg[4] = 0;
	msg[5] = (char)memoryType;

	msg[6] = 0;
	msg[7] = (offset>>8) & 0xff;
	msg[8] = offset & 0xff;

	msg[9] = 0;
	msg[10] = (length>>8) & 0xff;
	msg[11] = length & 0xff;

	/* send msg */
	error = tfa_dsp_msg(handle, sizeof(msg), (char *)msg);

	if (error != Tfa98xx_Error_Ok)
		return error;

	/* read the data from the device (length * 3 = words) */
	error = tfa_dsp_msg_read(handle, length * 3, bytes);

	return error;
}

enum Tfa98xx_Error tfa98xx_dsp_set_memory(Tfa98xx_handle_t handle, int memoryType,
		int offset, int length, int value)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	char msg[15];

	msg[0] = 8;
	msg[1] = MODULE_FRAMEWORK + 128;
	msg[2] = FW_PAR_ID_SET_MEMORY;

	msg[3] = 0;
	msg[4] = 0;
	msg[5] = (char)memoryType;

	msg[6] = 0;
	msg[7] = (offset>>8) & 0xff;
	msg[8] = offset & 0xff;

	msg[9] = 0;
	msg[10] = (length>>8) & 0xff;
	msg[11] = length & 0xff;

	msg[12] = (value>>16) & 0xff;
	msg[13] = (value>>8) & 0xff;
	msg[14] = value & 0xff;

	/* send msg */
	error = tfa_dsp_msg(handle, sizeof(msg), (char *)msg);

	return error;
}
/****************************** calibration support **************************/
/*
 * get/set the mtp with user controllable values
 *
 *  check if the relevant clocks are available
 */
enum Tfa98xx_Error tfa98xx_get_mtp(Tfa98xx_handle_t handle, uint16_t *value)
{
	int status;
	int result;

	/* not possible if PLL in powerdown */
	if (TFA_GET_BF(handle, PWDN)) {
		pr_debug("PLL in powerdown\n");
		return Tfa98xx_Error_NoClock;
	}

	/* tfa1 needs audio PLL */
	if (tfa98xx_dev_family(handle) == 1) {
		tfa98xx_dsp_system_stable(handle, &status);
		if (status == 0) {
			pr_debug("PLL not running\n");
			return Tfa98xx_Error_NoClock;
		}
	}

	result = TFA_READ_REG(handle, MTP0);
	if (result <  0) {
		return -result;
	}
	*value = (uint16_t)result;

	return Tfa98xx_Error_Ok;
}

/*
 * lock or unlock KEY2
 *  lock = 1 will lock
 *  lock = 0 will unlock
 *
 *  note that on return all the hidden key will be off
 */
void tfa98xx_key2(Tfa98xx_handle_t handle, int lock)
{

	/* unhide lock registers */
	tfa98xx_write_register16(handle,
				(tfa98xx_dev_family(handle) == 1) ? 0x40 : 0x0F, 0x5A6B);
	/* lock/unlock key2 MTPK */
	TFA_WRITE_REG(handle, MTPKEY2, lock ? 0 : 0x5A);
	/* unhide lock registers */
	tfa98xx_write_register16(handle,
				(tfa98xx_dev_family(handle) == 1) ? 0x40 : 0x0F, 0);


}

enum Tfa98xx_Error tfa98xx_set_mtp(Tfa98xx_handle_t handle,
									uint16_t value,
									uint16_t mask)
{
	unsigned short mtp_old, mtp_new;
	int loop, status;
	enum Tfa98xx_Error error;

	error = tfa98xx_get_mtp(handle, &mtp_old);

	if (error != Tfa98xx_Error_Ok)
		return error;

	mtp_new = (value & mask) | (mtp_old & ~mask);

	if (mtp_old == mtp_new) /* no change */
		return Tfa98xx_Error_Ok;

	/* assure that the clock is up, else we can't write MTP */
	error = tfa98xx_dsp_system_stable(handle, &status);
	if (error)
		return error;
	if (status == 0)
		return Tfa98xx_Error_NoClock;

	tfa98xx_key2(handle, 0); /* unlock */
	TFA_WRITE_REG(handle, MTP0, mtp_new); 	/* write to i2c shadow reg */
	/* CIMTP=1 start copying all the data from i2c regs_mtp to mtp*/
	TFA_SET_BF(handle, CIMTP, 1);
	/* no check for MTPBUSY here, i2c delay assumed to be enough */
	tfa98xx_key2(handle, 1); /* lock */

	/* wait until MTP write is done
	 */
	for (loop = 0; loop < 100; loop++) {
		msleep_interruptible(10); 			/* wait 10ms to avoid busload */
		if (TFA_GET_BF(handle, MTPB) == 0)
			return Tfa98xx_Error_Ok;
	}

	return Tfa98xx_Error_StateTimedOut;
}
/*
 * clear mtpex
 * set ACS
 * start tfa
 */
int tfa_calibrate(Tfa98xx_handle_t handle)
{
	enum Tfa98xx_Error error;

	/* clear mtpex */
	error = tfa98xx_set_mtp(handle, 0, 0x2);
	if (error)
		return error ;

	/* set ACS/coldboot state */
	error = tfaRunColdboot(handle, 1);

	/* start tfa by playing */
	return error;
}

static short twos(short x)
{
	return (x < 0) ? x+512 : x;
}
void tfa98xx_set_exttemp(Tfa98xx_handle_t handle, short ext_temp)
{
	if ((-256 <= ext_temp) && (ext_temp <= 255)) {
		/* make twos complement */
		pr_debug("Using ext temp %d C\n", twos(ext_temp));
		TFA_SET_BF(handle, TROS, 1);
		TFA_SET_BF(handle, EXTTS, twos(ext_temp));
	} else {
		pr_debug("Clearing ext temp settings\n");
		TFA_SET_BF(handle, TROS, 0);
	}
}
short tfa98xx_get_exttemp(Tfa98xx_handle_t handle)
{
	short ext_temp = (short)TFA_GET_BF(handle, EXTTS);
	return twos(ext_temp);
}

/************************** tfa simple bitfield interfacing ************************/
/* convenience functions */
enum Tfa98xx_Error tfa98xx_set_volume_level(Tfa98xx_handle_t handle, unsigned short vol)
{
	if (!tfa98xx_handle_is_open(handle))
		return Tfa98xx_Error_NotOpen;

	if (vol > 255)	/* restricted to 8 bits */
		vol = 255;

	/* 0x00 -> 0.0 dB
	 * 0x01 -> -0.5 dB
	 * ...
	 * 0xFE -> -127dB
	 * 0xFF -> muted
	 */

	/* volume value is in the top 8 bits of the register */
	return -TFA_SET_BF(handle, VOL, (uint16_t)vol);
}

static enum Tfa98xx_Error
tfa98xx_set_mute_tfa2(Tfa98xx_handle_t handle, enum Tfa98xx_Mute mute)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	switch (mute) {
	case Tfa98xx_Mute_Off:
		TFA_SET_BF(handle, CFSMR, 0);
		TFA_SET_BF(handle, CFSML, 0);
		break;
	case Tfa98xx_Mute_Amplifier:
	case Tfa98xx_Mute_Digital:
		TFA_SET_BF(handle, CFSMR, 1);
		TFA_SET_BF(handle, CFSML, 1);
		break;
	default:
		return Tfa98xx_Error_Bad_Parameter;
	}

	return error;
}

static enum Tfa98xx_Error
tfa98xx_set_mute_tfa1(Tfa98xx_handle_t handle, enum Tfa98xx_Mute mute)
{
	enum Tfa98xx_Error error;
	unsigned short audioctrl_value;
	unsigned short sysctrl_value;
	int value;

	value = TFA_READ_REG(handle, CFSM); /* audio control register */
	if (value < 0)
		return -value;
	audioctrl_value = (unsigned short)value;
	value = TFA_READ_REG(handle, AMPE); /* system control register */
	if (value < 0)
		return -value;
	sysctrl_value = (unsigned short)value;

	switch (mute) {
	case Tfa98xx_Mute_Off:
		/* previous state can be digital or amplifier mute,
		 * clear the cf_mute and set the enbl_amplifier bits
		 *
		 * To reduce PLOP at power on it is needed to switch the
		 * amplifier on with the DCDC in follower mode
		 * (enbl_boost = 0 ?).
		 * This workaround is also needed when toggling the
		 * powerdown bit!
		 */
		TFA_SET_BF_VALUE(handle, CFSM, 0, &audioctrl_value);
		TFA_SET_BF_VALUE(handle, AMPE, 1, &sysctrl_value);
		TFA_SET_BF_VALUE(handle, DCA, 1, &sysctrl_value);
		break;
	case Tfa98xx_Mute_Digital:
		/* expect the amplifier to run */
		/* set the cf_mute bit */
		TFA_SET_BF_VALUE(handle, CFSM, 1, &audioctrl_value);
		/* set the enbl_amplifier bit */
		TFA_SET_BF_VALUE(handle, AMPE, 1, &sysctrl_value);
		/* clear active mode */
		TFA_SET_BF_VALUE(handle, DCA, 0, &sysctrl_value);
		break;
	case Tfa98xx_Mute_Amplifier:
		/* clear the cf_mute bit */
		TFA_SET_BF_VALUE(handle, CFSM, 0, &audioctrl_value);
		/* clear the enbl_amplifier bit and active mode */
		TFA_SET_BF_VALUE(handle, AMPE, 0, &sysctrl_value);
		TFA_SET_BF_VALUE(handle, DCA, 0, &sysctrl_value);
		break;
	default:
		return Tfa98xx_Error_Bad_Parameter;
	}

	error = -TFA_WRITE_REG(handle, CFSM, audioctrl_value);
	if (error)
		return error;
	error = -TFA_WRITE_REG(handle, AMPE, sysctrl_value);
	return error;
}

enum Tfa98xx_Error
tfa98xx_set_mute(Tfa98xx_handle_t handle, enum Tfa98xx_Mute mute)
{
	if (!tfa98xx_handle_is_open(handle)) {
		pr_err("device not opened\n");
		return Tfa98xx_Error_NotOpen;
	}

	if (tfa98xx_dev_family(handle) == 1)
		return tfa98xx_set_mute_tfa1(handle, mute);
	else
		return tfa98xx_set_mute_tfa2(handle, mute);
}

/****************** patching **********************************************************/
static enum Tfa98xx_Error
tfa98xx_process_patch_file(Tfa98xx_handle_t handle, int length,
		 const unsigned char *bytes)
{
	unsigned short size;
	int index = 0;
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	while (index < length) {
		size = bytes[index] + bytes[index + 1] * 256;
		index += 2;
		if ((index + size) > length) {
			/* outside the buffer, error in the input data */
			return Tfa98xx_Error_Bad_Parameter;
		}

		if (size > handles_local[handle].buffer_size) {
			/* too big, must fit buffer */
			return Tfa98xx_Error_Bad_Parameter;
		}

		error = tfa98xx_write_raw(handle, size, &bytes[index]);
		if (error != Tfa98xx_Error_Ok)
			break;
		index += size;
	}
	return  error;
}



/* the patch contains a header with the following
 * IC revision register: 1 byte, 0xFF means don't care
 * XMEM address to check: 2 bytes, big endian, 0xFFFF means don't care
 * XMEM value to expect: 3 bytes, big endian
 */
static enum Tfa98xx_Error
tfa98xx_check_ic_rom_version(Tfa98xx_handle_t handle, const unsigned char patchheader[])
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	unsigned short checkrev, revid;
	unsigned char lsb_revid;
	unsigned short checkaddress;
	int checkvalue;
	int value = 0;
	int status;
	checkrev = patchheader[0];
	lsb_revid = handles_local[handle].rev & 0xff; /* only compare lower byte */

	if ((checkrev != 0xFF) && (checkrev != lsb_revid))
		return Tfa98xx_Error_Not_Supported;

	checkaddress = (patchheader[1] << 8) + patchheader[2];
	checkvalue =
	    (patchheader[3] << 16) + (patchheader[4] << 8) + patchheader[5];
	if (checkaddress != 0xFFFF) {
		/* before reading XMEM, check if we can access the DSP */
		error = tfa98xx_dsp_system_stable(handle, &status);
		if (error == Tfa98xx_Error_Ok) {
			if (!status) {
				/* DSP subsys not running */
				error = Tfa98xx_Error_DSP_not_running;
			}
		}
		/* read register to check the correct ROM version */
		if (error == Tfa98xx_Error_Ok) {
			error =
			tfa98xx_dsp_read_mem(handle, checkaddress, 1, &value);
		}
		if (error == Tfa98xx_Error_Ok) {
			if (value != checkvalue) {
				pr_err("patch file romid type check failed [0x%04x]: expected 0x%02x, actual 0x%02x\n",
						checkaddress, value, checkvalue);
				error = Tfa98xx_Error_Not_Supported;
			}
		}
	} else { /* == 0xffff */
		/* check if the revid subtype is in there */
		if (checkvalue != 0xFFFFFF && checkvalue != 0) {
			revid = patchheader[5]<<8 | patchheader[0]; /* full revid */
			if (revid != handles_local[handle].rev) {
				pr_err("patch file device type check failed: expected 0x%02x, actual 0x%02x\n",
						handles_local[handle].rev, revid);
				return Tfa98xx_Error_Not_Supported;
			}
		}
	}

	return error;
}


#define PATCH_HEADER_LENGTH 6
enum Tfa98xx_Error
tfa_dsp_patch(Tfa98xx_handle_t handle, int patchLength,
		 const unsigned char *patchBytes)
{
	enum Tfa98xx_Error error;
	if (!tfa98xx_handle_is_open(handle))
		return Tfa98xx_Error_NotOpen;
	if (patchLength < PATCH_HEADER_LENGTH)
		return Tfa98xx_Error_Bad_Parameter;
	error = tfa98xx_check_ic_rom_version(handle, patchBytes);
	if (Tfa98xx_Error_Ok != error) {
		return error;
	}
	error =
	    tfa98xx_process_patch_file(handle, patchLength - PATCH_HEADER_LENGTH,
			     patchBytes + PATCH_HEADER_LENGTH);
	return error;
}

/******************  end patching **************************/

TFA_INTERNAL enum Tfa98xx_Error
tfa98xx_wait_result(Tfa98xx_handle_t handle, int wait_retry_count)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	int cf_status; /* the contents of the CF_STATUS register */
	int tries = 0;
	do {
		cf_status = TFA_GET_BF(handle, ACK);
		if (cf_status < 0)
			error = -cf_status;
		tries++;
	/* i2c_cmd_ack */
	/* don't wait forever, DSP is pretty quick to respond (< 1ms) */
	} while ((error == Tfa98xx_Error_Ok) &&
			((cf_status & CF_STATUS_I2C_CMD_ACK) == 0) &&
			(tries < wait_retry_count));

	if (tries >= wait_retry_count) {
		/* something wrong with communication with DSP */
		error = Tfa98xx_Error_DSP_not_running;
	}
	return error;
}

/*
 * *  support functions for data conversion
 */
/**
 * convert memory bytes to signed 24 bit integers
 * input:  bytes contains "num_bytes" byte elements
 * output: data contains "num_bytes/3" int24 elements
*/
void tfa98xx_convert_bytes2data(int num_bytes,
				const unsigned char bytes[],
				int data[])
{
	int i;			/* index for data */
	int k;			/* index for bytes */
	int d;
	int num_data = num_bytes / 3;
	_ASSERT((num_bytes % 3) == 0);
	for (i = 0, k = 0; i < num_data; ++i, k += 3) {
		d = (bytes[k] << 16) | (bytes[k + 1] << 8) | (bytes[k + 2]);
		_ASSERT(d >= 0);
		_ASSERT(d < (1 << 24));	/* max 24 bits in use */
		if (bytes[k] & 0x80)	/* sign bit was set */
			d = -((1 << 24) - d);

		data[i] = d;
	}
}

/**
 convert signed 24 bit integers to 32bit aligned bytes
   input:   data contains "num_bytes/3" int24 elements
   output:  bytes contains "num_bytes" byte elements
*/
void tfa98xx_convert_data2bytes(int num_data, const int data[],
				unsigned char bytes[])
{
	int i;			/* index for data */
	int k;			/* index for bytes */
	int d;
	/* note: cannot just take the lowest 3 bytes from the 32 bit
	 * integer, because also need to take care of clipping any
	 * value > 2&23 */
	for (i = 0, k = 0; i < num_data; ++i, k += 3) {
		if (data[i] >= 0)
			d = MIN(data[i], (1 << 23) - 1);
		else {
			/* 2's complement */
			d = (1 << 24) - MIN(-data[i], 1 << 23);
		}
		_ASSERT(d >= 0);
		_ASSERT(d < (1 << 24));	/* max 24 bits in use */
		bytes[k] = (d >> 16) & 0xFF;	/* MSB */
		bytes[k + 1] = (d >> 8) & 0xFF;
		bytes[k + 2] = (d) & 0xFF;	/* LSB */
	}
}

/*
 *  DSP RPC message support functions
 *   depending on framework to be up and running
 *   need base i2c of memaccess (tfa1=0x70/tfa2=0x90)
 */


/* write dsp messages in function tfa_dsp_msg() */
/*  note the 'old' write_parameter() was more efficient because all i2c was in one burst transaction */

/*TODO properly handle bitfields: state should be restored! (now it will change eg dmesg field to xmem)*/
enum Tfa98xx_Error tfa_dsp_msg_write(Tfa98xx_handle_t handle, int length, const char *buffer)
{
	int offset = 0;
	int chunk_size = ROUND_DOWN(handles_local[handle].buffer_size, 3);  /* XMEM word size */
	int remaining_bytes = length;
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	uint16_t cfctl;
	int value;

	value = TFA_READ_REG(handle, DMEM);
	if (value < 0) {
		error = -value;
		return error;
	}
	cfctl = (uint16_t)value;
	/* assume no I2C errors from here */

	TFA_SET_BF_VALUE(handle, DMEM, (uint16_t)Tfa98xx_DMEM_XMEM, &cfctl); /* set cf ctl to DMEM  */
	TFA_SET_BF_VALUE(handle, AIF, 0, &cfctl); /* set to autoincrement */
	TFA_WRITE_REG(handle, DMEM, cfctl);

	/* xmem[1] is start of message
	 *  direct write to register to save cycles avoiding read-modify-write
	 */
	TFA_WRITE_REG(handle, MADD, 1);

	/* due to autoincrement in cf_ctrl, next write will happen at
	 * the next address */
	while ((error == Tfa98xx_Error_Ok) && (remaining_bytes > 0)) {
		if (remaining_bytes < chunk_size)
			chunk_size = remaining_bytes;
		/* else chunk_size remains at initialize value above */
		error = tfa98xx_write_data(handle, FAM_TFA98XX_CF_MEM,
					chunk_size, (const unsigned char *)buffer + offset);
		remaining_bytes -= chunk_size;
		offset += chunk_size;
	}

	/* notify the DSP */
	if (error == Tfa98xx_Error_Ok) {
		/* cf_int=0, cf_aif=0, cf_dmem=XMEM=01, cf_rst_dsp=0 */
		/* set the cf_req1 and cf_int bit */
		TFA_SET_BF_VALUE(handle, REQCMD, 0x01, &cfctl); /* bit 0 */
		TFA_SET_BF_VALUE(handle, CFINT, 1, &cfctl);
		error = -TFA_WRITE_REG(handle, CFINT, cfctl);
	}

	return error;
}

enum Tfa98xx_Error tfa_dsp_msg_write_id(Tfa98xx_handle_t handle, int length, const char *buffer, uint8_t cmdid[3])
{
	int offset = 0;
	int chunk_size = ROUND_DOWN(handles_local[handle].buffer_size, 3);  /* XMEM word size */
	int remaining_bytes = length;
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	uint16_t cfctl;
	int value;

	value = TFA_READ_REG(handle, DMEM);
	if (value < 0) {
		error = -value;
		return error;
	}
	cfctl = (uint16_t)value;
	/* assume no I2C errors from here */

	TFA_SET_BF_VALUE(handle, DMEM, (uint16_t)Tfa98xx_DMEM_XMEM, &cfctl); /* set cf ctl to DMEM  */
	TFA_SET_BF_VALUE(handle, AIF, 0, &cfctl); /* set to autoincrement */
	TFA_WRITE_REG(handle, DMEM, cfctl);

	/* xmem[1] is start of message
	 *  direct write to register to save cycles avoiding read-modify-write
	 */
	TFA_WRITE_REG(handle, MADD, 1);

	/* write cmd-id */
	error = tfa98xx_write_data(handle, FAM_TFA98XX_CF_MEM, 3, (const unsigned char *)cmdid);

	/* due to autoincrement in cf_ctrl, next write will happen at
	 * the next address */
	while ((error == Tfa98xx_Error_Ok) && (remaining_bytes > 0)) {
		if (remaining_bytes < chunk_size)
			chunk_size = remaining_bytes;
		/* else chunk_size remains at initialize value above */
		error = tfa98xx_write_data(handle, FAM_TFA98XX_CF_MEM,
				      chunk_size, (const unsigned char *)buffer + offset);
		remaining_bytes -= chunk_size;
		offset += chunk_size;
	}

	/* notify the DSP */
	if (error == Tfa98xx_Error_Ok) {
		/* cf_int=0, cf_aif=0, cf_dmem=XMEM=01, cf_rst_dsp=0 */
		/* set the cf_req1 and cf_int bit */
		TFA_SET_BF_VALUE(handle, REQCMD, 0x01, &cfctl); /* bit 0 */
		TFA_SET_BF_VALUE(handle, CFINT, 1, &cfctl);
		error = -TFA_WRITE_REG(handle, CFINT, cfctl);
	}

	return error;
}

/*
* status function used by tfa_dsp_msg() to retrieve command/msg status:
* return a <0 status of the DSP did not ACK.
*/
enum Tfa98xx_Error tfa_dsp_msg_status(Tfa98xx_handle_t handle, int *pRpcStatus)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	error = tfa98xx_wait_result(handle, 2); /* 2 is only one try */
	if (error == Tfa98xx_Error_DSP_not_running) {
		*pRpcStatus = -1;
		return Tfa98xx_Error_Ok;
	} else if (error != Tfa98xx_Error_Ok)
		return error;

	error = tfa98xx_check_rpc_status(handle, pRpcStatus);

	return error;
}

const char *tfa98xx_get_i2c_status_id_string(int status)
{
	const char *p_id_str;

	switch (status) {
	case Tfa98xx_DSP_Not_Running:
		p_id_str = "No response from DSP";
		break;
	case Tfa98xx_I2C_Req_Done:
		p_id_str = "Ok";
		break;
	case Tfa98xx_I2C_Req_Busy:
		p_id_str = "Request is being processed";
		break;
	case Tfa98xx_I2C_Req_Invalid_M_ID:
		p_id_str = "Provided M-ID does not fit in valid rang [0..2]";
		break;
	case Tfa98xx_I2C_Req_Invalid_P_ID:
		p_id_str = "Provided P-ID is not valid in the given M-ID context";
		break;
	case Tfa98xx_I2C_Req_Invalid_CC:
		p_id_str = "Invalid channel configuration bits (SC|DS|DP|DC) combination";
		break;
	case Tfa98xx_I2C_Req_Invalid_Seq:
		p_id_str = "Invalid sequence of commands, in case the DSP expects some commands in a specific order";
		break;
	case Tfa98xx_I2C_Req_Invalid_Param:
		p_id_str = "Generic error";
		break;
	case Tfa98xx_I2C_Req_Buffer_Overflow:
		p_id_str = "I2C buffer has overflowed: host has sent too many parameters, memory integrity is not guaranteed";
		break;
	default:
		p_id_str = "Unspecified error";
	}

	return p_id_str;
}

enum Tfa98xx_Error tfa_dsp_msg_read(Tfa98xx_handle_t handle, int length, unsigned char *bytes)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	int burst_size; /* number of words per burst size */
	int bytes_per_word = 3;
	int num_bytes;
	int offset = 0;
	unsigned short start_offset = 2; /* msg starts @xmem[2] ,[1]=cmd */

	if (length > TFA2_MAX_PARAM_SIZE)
		return Tfa98xx_Error_Bad_Parameter;

	TFA_SET_BF(handle, DMEM, (uint16_t)Tfa98xx_DMEM_XMEM);
	error = -TFA_WRITE_REG(handle, MADD, start_offset);
	if (error != Tfa98xx_Error_Ok)
		return error;

	num_bytes = length; /* input param */
	while (num_bytes > 0) {
		burst_size = ROUND_DOWN(handles_local[handle].buffer_size, bytes_per_word);
		if (num_bytes < burst_size)
			burst_size = num_bytes;
		error = tfa98xx_read_data(handle, FAM_TFA98XX_CF_MEM, burst_size, bytes + offset);
		if (error != Tfa98xx_Error_Ok)
			return error;

		num_bytes -= burst_size;
		offset += burst_size;
	}

	return Tfa98xx_Error_Ok;
}

/*
 *  write/read raw msg functions :
 *  the buffer is provided in little endian format, each word occupying 3 bytes, length is in bytes.
 *  The functions will return immediately and do not not wait for DSP reponse.
 */
#define MAX_WORDS (300)
enum Tfa98xx_Error tfa_dsp_msg(Tfa98xx_handle_t handle, int length, const char *buf)
{
	enum Tfa98xx_Error error;
	int tries, rpc_status = Tfa98xx_I2C_Req_Done;

	/* write the message and notify the DSP */
	error = tfa_dsp_msg_write(handle, length, buf);
	if (error != Tfa98xx_Error_Ok)
		return error;

	/* get the result from the DSP (polling) */
	for (tries = TFA98XX_WAITRESULT_NTRIES; tries > 0; tries--) {
		error = tfa_dsp_msg_status(handle, &rpc_status);
		if (error == Tfa98xx_Error_Ok && rpc_status == Tfa98xx_I2C_Req_Done)
			break;
		/* If the rpc status is a specific error we want to know it.
		 * If it is busy or not running it should retry
		 */
		if (rpc_status != Tfa98xx_I2C_Req_Busy && rpc_status != Tfa98xx_DSP_Not_Running)
			break;
	}

	/* if (tfa98xx_runtime_verbose)
		PRINT("Number of tries: %d \n", TFA98XX_WAITRESULT_NTRIES-tries); */

	if (rpc_status != Tfa98xx_I2C_Req_Done) {
		/* DSP RPC call returned an error */
		error = (enum Tfa98xx_Error) (rpc_status + Tfa98xx_Error_RpcBase);
		pr_debug("DSP msg status: %d (%s)\n",
					rpc_status, tfa98xx_get_i2c_status_id_string(rpc_status));
	}

	return error;
}

/**
 *  write/read raw msg functions:
 *  the buffer is provided in little endian format, each word occupying 3 bytes, length is in bytes.
 *  The functions will return immediately and do not not wait for DSP reponse.
 *  An ID is added to modify the command-ID
 */
enum Tfa98xx_Error tfa_dsp_msg_id(Tfa98xx_handle_t handle,
								int length,
								const char *buf,
								uint8_t cmdid[3])
{
	enum Tfa98xx_Error error;
	int tries, rpc_status = Tfa98xx_I2C_Req_Done;

	/* write the message and notify the DSP */
	error = tfa_dsp_msg_write_id(handle, length, buf, cmdid);
	if (error != Tfa98xx_Error_Ok)
		return error;

	/* get the result from the DSP (polling) */
	for (tries = TFA98XX_WAITRESULT_NTRIES; tries > 0; tries--) {
		error = tfa_dsp_msg_status(handle, &rpc_status);
		if (error == Tfa98xx_Error_Ok && rpc_status == Tfa98xx_I2C_Req_Done)
			break;
	}

	/* if (tfa98xx_runtime_verbose)
		PRINT("Number of tries: %d \n", TFA98XX_WAITRESULT_NTRIES-tries);
	*/

	if (rpc_status != Tfa98xx_I2C_Req_Done) {
		/* DSP RPC call returned an error */
		error = (enum Tfa98xx_Error) (rpc_status + Tfa98xx_Error_RpcBase);
		pr_debug("DSP msg status: %d (%s)\n",
					rpc_status, tfa98xx_get_i2c_status_id_string(rpc_status));
	}

	return error;
}

/* read the return code for the RPC call */
TFA_INTERNAL enum Tfa98xx_Error
tfa98xx_check_rpc_status(Tfa98xx_handle_t handle, int *pRpcStatus)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	/* the value to sent to the * CF_CONTROLS register: cf_req=00000000,
	 * cf_int=0, cf_aif=0, cf_dmem=XMEM=01, cf_rst_dsp=0 */
	unsigned short cf_ctrl = 0x0002;
	/* memory address to be accessed (0: Status, 1: ID, 2: parameters) */
	unsigned short cf_mad = 0x0000;

	if (!tfa98xx_handle_is_open(handle))
		return Tfa98xx_Error_NotOpen;
	if (pRpcStatus == 0)
		return Tfa98xx_Error_Bad_Parameter;

	/* 1) write DMEM=XMEM to the DSP XMEM */
	{
		/* minimize the number of I2C transactions by making use of the autoincrement in I2C */
		unsigned char buffer[4];
		/* first the data for CF_CONTROLS */
		buffer[0] = (unsigned char)((cf_ctrl >> 8) & 0xFF);
		buffer[1] = (unsigned char)(cf_ctrl & 0xFF);
		/* write the contents of CF_MAD which is the subaddress following CF_CONTROLS */
		buffer[2] = (unsigned char)((cf_mad >> 8) & 0xFF);
		buffer[3] = (unsigned char)(cf_mad & 0xFF);
		error = tfa98xx_write_data(handle, FAM_TFA98XX_CF_CONTROLS, sizeof(buffer), buffer);
	}
	if (error == Tfa98xx_Error_Ok) {
		/* read 1 word (24 bit) from XMEM */
		error = tfa98xx_dsp_read_mem(handle, 0, 1, pRpcStatus);
	}

	return error;
}

/***************************** xmem only **********************************/
enum Tfa98xx_Error
tfa98xx_dsp_read_mem(Tfa98xx_handle_t handle,
		unsigned int start_offset, int num_words, int *pValues)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	unsigned char *bytes;
	int burst_size;		/* number of words per burst size */
	const int bytes_per_word = 3;
	int dmem;
	int num_bytes;
	int *p;

	bytes = (unsigned char *)kmalloc(num_words*bytes_per_word, GFP_KERNEL);
	if (bytes == NULL)
		return Tfa98xx_Error_Fail;

	/* If no offset is given, assume XMEM! */
	if (((start_offset>>16) & 0xf) > 0)
		dmem = (start_offset>>16) & 0xf;
	else
		dmem = Tfa98xx_DMEM_XMEM;

	/* Remove offset from adress */
	start_offset = start_offset & 0xffff;
	num_bytes = num_words * bytes_per_word;
	p = pValues;

	TFA_SET_BF(handle, DMEM, (uint16_t)dmem);
	error = -TFA_WRITE_REG(handle, MADD, (unsigned short)start_offset);
	if (error != Tfa98xx_Error_Ok)
		goto tfa98xx_dsp_read_mem_exit;

	for (; num_bytes > 0;) {
		burst_size = ROUND_DOWN(handles_local[handle].buffer_size, bytes_per_word);
		if (num_bytes < burst_size)
			burst_size = num_bytes;

		_ASSERT(burst_size <= sizeof(bytes));
		error = tfa98xx_read_data(handle, FAM_TFA98XX_CF_MEM, burst_size, bytes);
		if (error != Tfa98xx_Error_Ok)
			goto tfa98xx_dsp_read_mem_exit;

		tfa98xx_convert_bytes2data(burst_size, bytes, p);

		num_bytes -= burst_size;
		p += burst_size / bytes_per_word;
	}

tfa98xx_dsp_read_mem_exit:
	kfree(bytes);
	return error;
}

enum Tfa98xx_Error
tfa98xx_dsp_write_mem_word(Tfa98xx_handle_t handle, unsigned short address, int value, int memtype)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	unsigned char bytes[3];

	TFA_SET_BF(handle, DMEM, (uint16_t)memtype);

	error = -TFA_WRITE_REG(handle, MADD, address);
	if (error != Tfa98xx_Error_Ok)
		return error;

	tfa98xx_convert_data2bytes(1, &value, bytes);
	error = tfa98xx_write_data(handle, FAM_TFA98XX_CF_MEM, 3, bytes);

	return error;
}

enum Tfa98xx_Error tfa_cont_write_filterbank(int device, nxpTfaFilter_t *filter)
{
	unsigned char biquad_index;
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	for (biquad_index = 0; biquad_index < 10; biquad_index++) {
		if (filter[biquad_index].enabled) {
			error = tfa_dsp_cmd_id_write(device, MODULE_BIQUADFILTERBANK,
					biquad_index+1,
					sizeof(filter[biquad_index].biquad.bytes),
						filter[biquad_index].biquad.bytes);
		} else {
			error = Tfa98xx_DspBiquad_Disable(device, biquad_index+1);
		}
		if (error)
			return error;

	}
	return error;
}

enum Tfa98xx_Error
Tfa98xx_DspBiquad_Disable(Tfa98xx_handle_t handle, int biquad_index)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	int coeff_buffer[BIQUAD_COEFF_SIZE];
	unsigned char bytes[3 + BIQUAD_COEFF_SIZE * 3];

	if (biquad_index > TFA98XX_BIQUAD_NUM)
		return Tfa98xx_Error_Bad_Parameter;
	if (biquad_index < 1)
		return Tfa98xx_Error_Bad_Parameter;

	/* make opcode */

	/* set in correct order and format for the DSP */
	coeff_buffer[0] = (int) -8388608;	/* -1.0f */
	coeff_buffer[1] = 0;
	coeff_buffer[2] = 0;
	coeff_buffer[3] = 0;
	coeff_buffer[4] = 0;
	coeff_buffer[5] = 0;
	/* convert to packed 24 bits data */
	tfa98xx_convert_data2bytes(BIQUAD_COEFF_SIZE, coeff_buffer, &bytes[3]);

	bytes[0] = 0;
	bytes[1] = MODULE_BIQUADFILTERBANK+128;
	bytes[2] = (unsigned char)biquad_index;

	error = tfa_dsp_msg(handle, 3 + BIQUAD_COEFF_SIZE * 3, (char *)bytes);

	return error;
}

/* wrapper for dsp_msg that adds opcode */
enum Tfa98xx_Error tfa_dsp_cmd_id_write(Tfa98xx_handle_t handle,
					unsigned char module_id,
					unsigned char param_id, int num_bytes,
					const unsigned char data[])
{
	enum Tfa98xx_Error error;
	unsigned char *buffer;

	if (!tfa98xx_handle_is_open(handle))
		return Tfa98xx_Error_NotOpen;

	buffer = kmalloc(3 + num_bytes, GFP_KERNEL);
	if (buffer == NULL)
		return Tfa98xx_Error_Fail;

	buffer[0] = handles_local[handle].spkr_select;
	buffer[1] = module_id + 128;
	buffer[2] = param_id;

	memcpy(&buffer[3], data, num_bytes);

	error = tfa_dsp_msg(handle, 3 + num_bytes, (char *)buffer);

	kfree(buffer);

	return error;
}

/* wrapper for dsp_msg that adds opcode */
/* this is as the former tfa98xx_dsp_get_param() */
enum Tfa98xx_Error tfa_dsp_cmd_id_write_read(Tfa98xx_handle_t handle,
						unsigned char module_id,
						unsigned char param_id, int num_bytes,
						unsigned char data[])
{
	enum Tfa98xx_Error error;
	unsigned char buffer[3];

	if (!tfa98xx_handle_is_open(handle))
		return Tfa98xx_Error_NotOpen;

	buffer[0] = handles_local[handle].spkr_select;
	buffer[1] = module_id + 128;
	buffer[2] = param_id;

	error = tfa_dsp_msg(handle, sizeof(unsigned char[3]), (char *)buffer);
	if (error != Tfa98xx_Error_Ok)
		return error;
    /* read the data from the dsp */
	error = tfa_dsp_msg_read(handle, num_bytes, data);

	return error;
}

/* wrapper for dsp_msg that adds opcode and 3 bytes required for coefs */
enum Tfa98xx_Error tfa_dsp_cmd_id_coefs(Tfa98xx_handle_t handle,
			unsigned char module_id,
			unsigned char param_id, int num_bytes,
			unsigned char data[])
{
	enum Tfa98xx_Error error;
	unsigned char buffer[6];

	if (!tfa98xx_handle_is_open(handle))
		return Tfa98xx_Error_NotOpen;

	buffer[0] = handles_local[handle].spkr_select;
	buffer[1] = module_id + 128;
	buffer[2] = param_id;
	buffer[3] = 0;
	buffer[4] = 0;
	buffer[5] = 0;

	error = tfa_dsp_msg(handle, sizeof(unsigned char[6]), (char *)buffer);
	if (error != Tfa98xx_Error_Ok)
		return error;

	/* read the data from the dsp */
	error = tfa_dsp_msg_read(handle, num_bytes, data);

	return error;
}

/* wrapper for dsp_msg that adds opcode and 3 bytes required for MBDrcDynamics */
enum Tfa98xx_Error tfa_dsp_cmd_id_MBDrc_dynamics(Tfa98xx_handle_t handle,
			unsigned char module_id,
			unsigned char param_id, int index_subband,
			int num_bytes, unsigned char data[])
{
	enum Tfa98xx_Error error;
	unsigned char buffer[6];

	if (!tfa98xx_handle_is_open(handle))
		return Tfa98xx_Error_NotOpen;

	buffer[0] = handles_local[handle].spkr_select;
	buffer[1] = module_id + 128;
	buffer[2] = param_id;
	buffer[3] = 0;
	buffer[4] = 0;
	buffer[5] = (unsigned char)index_subband;

	error = tfa_dsp_msg(handle, sizeof(unsigned char[6]), (char *)buffer);
	if (error != Tfa98xx_Error_Ok)
		return error;

	/* read the data from the dsp */
	error = tfa_dsp_msg_read(handle, num_bytes, data);

	return error;
}

enum Tfa98xx_Error
tfa98xx_dsp_write_preset(Tfa98xx_handle_t handle, int length,
			const unsigned char *p_preset_bytes)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	if (p_preset_bytes != 0) {
		/* by design: keep the data opaque and no
		 * interpreting/calculation */
		error = tfa_dsp_cmd_id_write(handle, MODULE_SPEAKERBOOST,
					SB_PARAM_SET_PRESET, length,
					p_preset_bytes);
	} else {
		error = Tfa98xx_Error_Bad_Parameter;
	}
	return error;
}

/*
 * get features from MTP
 */
enum Tfa98xx_Error
tfa98xx_dsp_get_hw_feature_bits(Tfa98xx_handle_t handle, int *features)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	uint32_t value;
	uint16_t mtpbf;

	/* return the cache data if it's valid */
	if (handles_local[handle].hw_feature_bits != -1) {
		*features = handles_local[handle].hw_feature_bits;
	} else {
		/* for tfa1 check if we have clock */
		if (tfa98xx_dev_family(handle) == 1) {
			int status;
			tfa98xx_dsp_system_stable(handle, &status);
			if (!status) {
				get_hw_features_from_cnt(handle, features);
				/* skip reading MTP: */
				return (*features == -1) ? Tfa98xx_Error_Fail : Tfa98xx_Error_Ok;
			}
			mtpbf = 0x850f;  /* MTP5 for tfa1,16 bits */
		} else
			mtpbf = 0xf907;  /* MTP9 for tfa2, 8 bits */

		value = tfa_read_reg(handle, mtpbf) & 0xffff;
		*features = handles_local[handle].hw_feature_bits = value;
	}

	return error;
}

enum Tfa98xx_Error
tfa98xx_dsp_get_sw_feature_bits(Tfa98xx_handle_t handle, int features[2])
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	unsigned char bytes[3 * 2];

	/* return the cache data if it's valid */
	if (handles_local[handle].sw_feature_bits[0] != -1) {
		features[0] = handles_local[handle].sw_feature_bits[0];
		features[1] = handles_local[handle].sw_feature_bits[1];
	} else {
		/* for tfa1 check if we have clock */
		if (tfa98xx_dev_family(handle) == 1) {
			int status;

			tfa98xx_dsp_system_stable(handle, &status);
			if (!status) {
				get_sw_features_from_cnt(handle, features);
				/* skip reading MTP: */
				return (features[0] == -1) ? Tfa98xx_Error_Fail : Tfa98xx_Error_Ok;
			}
		}
		error = tfa_dsp_cmd_id_write_read(handle, MODULE_FRAMEWORK,
				FW_PAR_ID_GET_FEATURE_INFO, sizeof(bytes), bytes);

		if (error != Tfa98xx_Error_Ok) {
			/* old ROM code may respond with Tfa98xx_Error_RpcParamId */
			return error;
		}
		tfa98xx_convert_bytes2data(sizeof(bytes), bytes, features);
	}
	return error;
}

enum Tfa98xx_Error tfa98xx_dsp_get_state_info(Tfa98xx_handle_t handle, unsigned char bytes[], unsigned int *statesize)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	int bSupportFramework = 0;
	unsigned int stateSize = 9;

	err = tfa98xx_dsp_support_framework(handle, &bSupportFramework);
	if (err == Tfa98xx_Error_Ok) {
		if (bSupportFramework) {
			err = tfa_dsp_cmd_id_write_read(handle, MODULE_FRAMEWORK,
				FW_PARAM_GET_STATE, 3 * stateSize, bytes);
		} else {
			/* old ROM code, ask SpeakerBoost and only do first portion */
			stateSize = 8;
			err = tfa_dsp_cmd_id_write_read(handle, MODULE_SPEAKERBOOST,
				SB_PARAM_GET_STATE, 3 * stateSize, bytes);
		}
	}

	*statesize = stateSize;

	return err;
}

enum Tfa98xx_Error tfa98xx_dsp_support_drc(Tfa98xx_handle_t handle, int *pbSupportDrc)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	*pbSupportDrc = 0;

	if (!tfa98xx_handle_is_open(handle))
		return Tfa98xx_Error_NotOpen;
	if (handles_local[handle].supportDrc != supportNotSet) {
		*pbSupportDrc = (handles_local[handle].supportDrc == supportYes);
	} else {
		int featureBits[2];

		error = tfa98xx_dsp_get_sw_feature_bits(handle, featureBits);
		if (error == Tfa98xx_Error_Ok) {
			/* easy case: new API available */
			/* bit=0 means DRC enabled */
			*pbSupportDrc = (featureBits[0] & FEATURE1_DRC) == 0;
		} else if (error == Tfa98xx_Error_RpcParamId) {
			/* older ROM code, doesn't support it */
			*pbSupportDrc = 0;
			error = Tfa98xx_Error_Ok;
		}
		/* else some other error, return transparently */
		/* pbSupportDrc only changed when error == Tfa98xx_Error_Ok */

		if (error == Tfa98xx_Error_Ok) {
			handles_local[handle].supportDrc = *pbSupportDrc ? supportYes : supportNo;
		}
	}
	return error;
}

enum Tfa98xx_Error
tfa98xx_dsp_support_framework(Tfa98xx_handle_t handle, int *pbSupportFramework)
{
	int featureBits[2] = {0, 0};
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	_ASSERT(pbSupportFramework != 0);

	if (!tfa98xx_handle_is_open(handle))
		return Tfa98xx_Error_NotOpen;

	if (handles_local[handle].supportFramework != supportNotSet) {
		if (handles_local[handle].supportFramework == supportNo)
			*pbSupportFramework = 0;
		else
			*pbSupportFramework = 1;
	} else {
		error = tfa98xx_dsp_get_sw_feature_bits(handle, featureBits);
		if (error == Tfa98xx_Error_Ok) {
			*pbSupportFramework = 1;
			handles_local[handle].supportFramework = supportYes;
		} else {
			*pbSupportFramework = 0;
			handles_local[handle].supportFramework = supportNo;
			error = Tfa98xx_Error_Ok;
		}
	}

	/* *pbSupportFramework only changed when error == Tfa98xx_Error_Ok */
	return error;
}

enum Tfa98xx_Error
tfa98xx_dsp_write_speaker_parameters(Tfa98xx_handle_t handle,
									int length,
									const unsigned char *p_speaker_bytes)
{
	enum Tfa98xx_Error error;
	if (p_speaker_bytes != 0) {
		/* by design: keep the data opaque and no
		 * interpreting/calculation */
		/* Use long WaitResult retry count */
		error = tfa_dsp_cmd_id_write(
					handle,
					MODULE_SPEAKERBOOST,
					SB_PARAM_SET_LSMODEL, length,
					p_speaker_bytes);
	} else {
		error = Tfa98xx_Error_Bad_Parameter;
	}

#if (defined(TFA9887B) || defined(TFA98XX_FULL))
	{
		int bSupportDrc;

		if (error != Tfa98xx_Error_Ok)
			return error;

		error = tfa98xx_dsp_support_drc(handle, &bSupportDrc);
		if (error != Tfa98xx_Error_Ok)
			return error;

		if (bSupportDrc) {
		/* Need to set AgcGainInsert back to PRE,
		* as the SetConfig forces it to POST */
			uint8_t bytes[3] = {0, 0, 0};

			error = tfa_dsp_cmd_id_write(handle,
							MODULE_SPEAKERBOOST,
							SB_PARAM_SET_AGCINS,
							sizeof(bytes),
							bytes);
		}
	}
#endif

	return error;
}

enum Tfa98xx_Error
tfa98xx_dsp_write_config(Tfa98xx_handle_t handle, int length,
							const unsigned char *p_config_bytes)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	error = tfa_dsp_cmd_id_write(handle,
								MODULE_SPEAKERBOOST,
								SB_PARAM_SET_CONFIG, length,
								p_config_bytes);

#if (defined(TFA9887B) || defined(TFA98XX_FULL))
	{
		int bSupportDrc;

		if (error != Tfa98xx_Error_Ok)
			return error;

		error = tfa98xx_dsp_support_drc(handle, &bSupportDrc);
		if (error != Tfa98xx_Error_Ok)
			return error;

		if (bSupportDrc) {
			/* Need to set AgcGainInsert back to PRE,
			* as the SetConfig forces it to POST */
			uint8_t bytes[3] = {0, 0, 0};

			error = tfa_dsp_cmd_id_write(handle,
							MODULE_SPEAKERBOOST,
							SB_PARAM_SET_AGCINS,
							sizeof(bytes),
							bytes);
		}
	}
#endif

	return error;
}

#if (defined(TFA9887B) || defined(TFA98XX_FULL))
/* load all the parameters for the DRC settings from a file */
enum Tfa98xx_Error tfa98xx_dsp_write_drc(Tfa98xx_handle_t handle,
					int length,
					const unsigned char *p_drc_bytes)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	if (p_drc_bytes != 0) {
	error = tfa_dsp_cmd_id_write(handle,
					MODULE_SPEAKERBOOST,
					SB_PARAM_SET_DRC, length,
					p_drc_bytes);

	} else {
		error = Tfa98xx_Error_Bad_Parameter;
	}
	return error;
}
#endif

enum Tfa98xx_Error tfa98xx_powerdown(Tfa98xx_handle_t handle, int powerdown)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	if (!tfa98xx_handle_is_open(handle))
		return Tfa98xx_Error_NotOpen;

	TFA_SET_BF(handle, PWDN, (uint16_t)powerdown);
	return error;
}

enum Tfa98xx_Error
tfa98xx_select_mode(Tfa98xx_handle_t handle, enum Tfa98xx_Mode mode)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	if (!tfa98xx_handle_is_open(handle))
		return Tfa98xx_Error_NotOpen;

	if (error == Tfa98xx_Error_Ok) {
		switch (mode) {

		default:
			error = Tfa98xx_Error_Bad_Parameter;
		}
	}

	return error;
}

int tfa_set_bf(Tfa98xx_handle_t dev_idx, const uint16_t bf, const uint16_t value)
{
	enum Tfa98xx_Error err;
	uint16_t regvalue, msk, oldvalue;

	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;
	uint8_t address = (bf >> 8) & 0xff;

	err = tfa98xx_read_register16(dev_idx, address, &regvalue);
	if (err) {
		pr_err("Error getting bf :%d \n", -err);
		return -err;
	}

	oldvalue = regvalue;
	msk = ((1<<(len+1))-1)<<pos;
	regvalue &= ~msk;
	regvalue |= value<<pos;

	/* Only write when the current register value is not the same as the new value */
	if (oldvalue != regvalue) {
		err = tfa98xx_write_register16(dev_idx, address, regvalue);
		if (err) {
			pr_err("Error setting bf :%d \n", -err);
			return -err;
		}
	}

	return 0;
}

int tfa_set_bf_volatile(Tfa98xx_handle_t dev_idx, const uint16_t bf, const uint16_t value)
{
	enum Tfa98xx_Error err;
	uint16_t regvalue, msk;

	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;
	uint8_t address = (bf >> 8) & 0xff;

	err = tfa98xx_read_register16(dev_idx, address, &regvalue);
	if (err) {
		pr_err("Error getting bf :%d \n", -err);
		return -err;
	}

	msk = ((1<<(len+1))-1)<<pos;
	regvalue &= ~msk;
	regvalue |= value<<pos;

	err = tfa98xx_write_register16(dev_idx, address, regvalue);
	if (err) {
		pr_err("Error setting bf :%d \n", -err);
		return -err;
	}

	return 0;
}

int tfa_get_bf(Tfa98xx_handle_t dev_idx, const uint16_t bf)
{
	enum Tfa98xx_Error err;
	uint16_t regvalue, msk;
	uint16_t value;

	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;
	uint8_t address = (bf >> 8) & 0xff;

	err = tfa98xx_read_register16(dev_idx, address, &regvalue);
	if (err) {
		pr_err("Error getting bf :%d \n", -err);
		return -err;
	}

	msk = ((1<<(len+1))-1)<<pos;
	regvalue &= msk;
	value = regvalue>>pos;

	return value;
}

int tfa_set_bf_value(const uint16_t bf, const uint16_t bf_value, uint16_t *p_reg_value)
{
	uint16_t regvalue, msk;

	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;

	regvalue = *p_reg_value;

	msk = ((1<<(len+1))-1)<<pos;
	regvalue &= ~msk;
	regvalue |= bf_value<<pos;

	*p_reg_value = regvalue;

	return 0;
}

uint16_t tfa_get_bf_value(const uint16_t bf, const uint16_t reg_value)
{
	uint16_t msk, value;

	/*
	 * bitfield enum:
	 * - 0..3  : len
	 * - 4..7  : pos
	 * - 8..15 : address
	 */
	uint8_t len = bf & 0x0f;
	uint8_t pos = (bf >> 4) & 0x0f;

	msk = ((1<<(len+1))-1)<<pos;
	value = (reg_value & msk) >> pos;

	return value;
}


int tfa_write_reg(Tfa98xx_handle_t dev_idx, const uint16_t bf, const uint16_t reg_value)
{
	enum Tfa98xx_Error err;

	/* bitfield enum - 8..15 : address */
	uint8_t address = (bf >> 8) & 0xff;

	err = tfa98xx_write_register16(dev_idx, address, reg_value);
	if (err)
		return -err;

	return 0;
}

int tfa_read_reg(Tfa98xx_handle_t dev_idx, const uint16_t bf)
{
	enum Tfa98xx_Error err;
	uint16_t regvalue;

	/* bitfield enum - 8..15 : address */
	uint8_t address = (bf >> 8) & 0xff;

	err = tfa98xx_read_register16(dev_idx, address, &regvalue);
	if (err)
		return -err;

	return regvalue;
}

/*
 * powerup the coolflux subsystem and wait for it
 */
enum Tfa98xx_Error tfa_cf_powerup(Tfa98xx_handle_t handle)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	int tries, status;

	/* power on the sub system */
	TFA_SET_BF_VOLATILE(handle, PWDN, 0);

	/* wait until everything is stable, in case clock has been off */
	if (tfa98xx_runtime_verbose)
		pr_info("Waiting for DSP system stable...\n");
	for (tries = CFSTABLE_TRIES; tries > 0; tries--) {
		err = tfa98xx_dsp_system_stable(handle, &status);
		_ASSERT(err == Tfa98xx_Error_Ok);
		if (status)
			break;
		else
			udelay(2000); /* wait 2ms to avoid busload */
	}
	if (tries == 0) { /* timedout */
		pr_err("DSP subsystem start timed out\n");
		return Tfa98xx_Error_StateTimedOut;
	}

	return err;
}

/*
 * Enable/Disable the I2S output for TFA1 devices
 * without TDM interface
 */
static enum Tfa98xx_Error tfa98xx_aec_output(Tfa98xx_handle_t handle, int enable)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;

	if ((tfa98xx_get_device_dai(handle) & Tfa98xx_DAI_TDM) == Tfa98xx_DAI_TDM)
		return err;

	if (tfa98xx_dev_family(handle) == 1)
		err = -tfa_set_bf(handle, TFA1_BF_I2SDOE, (enable != 0));
	else {
		pr_err("I2SDOE on unsupported family\n");
		err = Tfa98xx_Error_Not_Supported;
	}

	return err;
}

/*
 * Print the current state of the hardware manager
 * Device manager status information, man_state from TFA9888_N1B_I2C_regmap_V12
 */
enum Tfa98xx_Error show_current_state(Tfa98xx_handle_t handle)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	int manstate = -1;

	if (tfa98xx_dev_family(handle) == 2) {
		manstate = TFA_GET_BF(handle, MANSTATE);
		if (manstate < 0)
			return -manstate;
	}

	pr_info("Current HW manager state: ");

	switch (manstate) {
	case 0:
		pr_info("power_down_state\n");
		break;
	case 1:
		pr_info("wait_for_source_settings_state\n");
		break;
	case 2:
		pr_info("connnect_pll_input_state\n");
		break;
	case 3:
		pr_info("disconnect_pll_input_state\n");
		break;
	case 4:
		pr_info("enable_pll_state\n");
		break;
	case 5:
		pr_info("enable_cgu_state\n");
		break;
	case 6:
		pr_info("init_cf_state\n");
		break;
	case 7:
		pr_info("enable_amplifier_state\n");
		break;
	case 8:
		pr_info("alarm_state\n");
		break;
	case 9:
		pr_info("operating_state\n");
		break;
	case 10:
		pr_info("mute_audio_state\n");
		break;
	case 11:
		pr_info("disable_cgu_pll_state\n");
		break;
	default:
		pr_info("Unable to find current state\n");
		break;
	}

	return err;
}

/*
 *  start the speakerboost algorithm
 *  this implies a full system startup when the system was not already started
 *
 */
enum Tfa98xx_Error tfaRunSpeakerBoost(Tfa98xx_handle_t handle, int force, int profile)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	int value;
	int istap_prof = 0;

	tfa98xx_dump_register(getHandle(handle), 2, "[dump]  tfaRunSpeakerBoost()   begin");
	if (force) {
		err = tfaRunColdStartup(handle, profile);
		if (err)
			return err;
		/* DSP is running now */
	}

	value = TFA_GET_BF(handle, ACS);
	pr_debug("%s: ACS = %d\n", __func__, value);

#ifdef __KERNEL__ /* TODO try to combine this with the pr_debug below */
	tfa98xx_trace_printk("%s %sstart\n",
						tfaContDeviceName(handle),
						value ? "cold" : "warm");
#endif
	/* Check if next profile is a tap profile */
	istap_prof = tfaContIsTapProfile(handle, profile);

	if ((value == 1) && (!istap_prof)) {
		/* SL: If cold start, make sure partial update is disabled */
		tfa_set_partial_update(0);

		/* Run startup and write all files */
		err = tfaRunSpeakerStartup(handle, force, profile);
		if (err)
			return err;

		/* Save the current profile and set the vstep to 0 */
		/* This needs to be overwriten even in CF bypass */
		tfa_set_swprof(handle, (unsigned short)profile);
		tfa_set_swvstep(handle, 0);

		/* Startup with CF in bypass then return here */
		if (TFA_GET_BF(handle, CFE) == 0) {
			return err;
		}

#ifdef __KERNEL__ /* TODO check if this can move to the tfa98xx.c */
			/* Necessary here for early calibrate (MTPEX + ACS) */
			tfa98xx_apply_deferred_calibration(handle);
#endif
		/* calibrate */
		err = tfaRunSpeakerCalibration(handle, profile);

		switch (TFA_GET_BF(handle, REV) & 0xff) {
		case 0x96:
		case 0x97:
			err = tfa98xx_dsp_reset(handle, 1);
			err = tfa98xx_dsp_reset(handle, 0);
			pr_info("Reset 97 at first cold start up\n");
			break;
		default:
			break;
		}

	} else if (istap_prof)  {
		/* Save the current profile and set the vstep to 0 */
		/* This needs to be overwriten even in CF bypass and tap*/
		tfa_set_swprof(handle, (unsigned short)profile);
		tfa_set_swvstep(handle, 0);
	}
#ifdef __KERNEL__ /* For kernel, DSP is turned off during cold start in tfa_probe */
	else {
		if ((tfa98xx_dev_family(handle) == 2) && !(TFA_GET_BF(handle, CFE)))
			TFA_SET_BF_VOLATILE(handle, CFE, 1);
	}
#endif

	return err;
}

enum Tfa98xx_Error tfaRunSpeakerStartup(Tfa98xx_handle_t handle, int force, int profile)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	pr_debug("coldstart%s :", force ? " (forced)" : "");

	if (!force) { /* in case of force CF already runnning */
		err = tfaRunStartup(handle, profile);
		PRINT_ASSERT(err);
		if (err)
			return err;
		/* Startup with CF in bypass then return here */
		if (TFA_GET_BF(handle, CFE) == 0)
			return err;

		err = tfaRunStartDSP(handle);
		if (err)
			return err;
	}
	/* DSP is running now
	* NOTE that ACS may be active
	*  no DSP reset/sample rate may be done until configured (SBSL)
	* For the first configuration the DSP expects at least
	* the speaker, config and a preset.
	* Therefore all files from the device list as well as the file
	* from the default profile are loaded before SBSL is set.
	* Note that the register settings were already done before loading the patch
	* write all the files from the device list (speaker, vstep and all messages)
	*/
	err = tfaContWriteFiles(handle);
	if (err)
		return err;

	/* write all the files from the profile list (typically preset) */
	err = tfaContWriteFilesProf(handle, profile, 0); /* use volumestep 0 */
	if (err)
		return err;

	return err;
}

/*
 * Run calibration
 */
enum Tfa98xx_Error tfaRunSpeakerCalibration(Tfa98xx_handle_t handle, int profile)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	int calibrateDone, spkr_count = 0;
	/* Avoid warning in user-space */
	profile = profile;

#ifdef __KERNEL__ /* Necessary otherwise we are thrown out of operating mode in kernel (because of internal clock) */
	/* SL: In kernel, there's no clock available during cold start, therefore power down chip */
	if (tfa98xx_dev_family(handle) == 2) {
		TFA_SET_BF_VOLATILE(handle, PWDN, 1);
		TFA_SET_BF_VOLATILE(handle, SBSL, 1);
		TFA_SET_BF_VOLATILE(handle, CFE, 0);
		return Tfa98xx_Error_NoClock;
	} else if (tfa98xx_dev_family(handle) != 2)
#endif
		TFA_SET_BF_VOLATILE(handle, SBSL, 1);

	/* return if there is no audio running */
	if (TFA_GET_BF(handle, NOCLK) && tfa98xx_dev_family(handle) == 2)
		return Tfa98xx_Error_NoClock;

	/* When MTPOTC is set (cal=once) unlock key2 */
	if (TFA_GET_BF(handle, MTPOTC) == 1) {
		tfa98xx_key2(handle, 0);
	}

	/* await calibration, this should return ok */
	err = tfaRunWaitCalibration(handle, &calibrateDone);
	if (err == Tfa98xx_Error_Ok) {
		err = tfa_dsp_get_calibration_impedance(handle);
		PRINT_ASSERT(err);
	}

	/* Give reason why calibration failed! */
	if (err != Tfa98xx_Error_Ok) {
		if ((tfa98xx_dev_family(handle) == 2 && TFA_GET_BF(handle, REFCKSEL) == 1)) {
			pr_err("Unable to calibrate the device with the internal clock! \n");
		}
	}

	if (err == Tfa98xx_Error_Ok) {
		err = tfa98xx_supported_speakers(handle, &spkr_count);

		if (spkr_count == 1) {
			pr_debug(" %d mOhms \n", handles_local[handle].mohm[0]);
		} else {
			pr_debug(" Prim:%d mOhms, Sec:%d mOhms\n",
						handles_local[handle].mohm[0],
						handles_local[handle].mohm[1]);
		}
	}

	/* When MTPOTC is set (cal=once) re-lock key2 */
	if (TFA_GET_BF(handle, MTPOTC) == 1) {
		tfa98xx_key2(handle, 1);
	}

	return err;
}

/*
 * Set the debug option
 */
void tfa_verbose(int level)
{
	tfa98xx_trace_level = level;
	tfa98xx_runtime_verbose = level != 0; /* any non-zero */
	tfa_cnt_verbose(level);
}

enum Tfa98xx_Error tfaRunColdboot(Tfa98xx_handle_t handle, int state)
{
#define CF_CONTROL 0x8100
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	int tries = 10;

	/* repeat set ACS bit until set as requested */
	while (state != TFA_GET_BF(handle, ACS)) {
		/* set colstarted in CF_CONTROL to force ACS */
		err = tfa98xx_dsp_write_mem_word(handle, CF_CONTROL, state, Tfa98xx_DMEM_IOMEM);
		PRINT_ASSERT(err);

		if (tries-- == 0) {
			pr_info("coldboot (ACS) did not %s\n", state ? "set":"clear");
			return Tfa98xx_Error_Other;
		}
	}

	return err;
}



/*
 * load the patch if any
 *   else tell no loaded
 */
static enum Tfa98xx_Error tfa_run_load_patch(Tfa98xx_handle_t handle)
{
	return tfaContWritePatch(handle);
}

/*
 *  this will load the patch witch will implicitly start the DSP
 *   if no patch is available the DPS is started immediately
 */
enum Tfa98xx_Error tfaRunStartDSP(Tfa98xx_handle_t handle)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;

	err = tfa_run_load_patch(handle);
	if (err) { /* patch load is fatal so return immediately*/
		return err;
	}

	/* Clear count_boot, should be reset to 0 before the DSP reset is released */
	err = tfa98xx_dsp_write_mem_word(handle, 512, 0, Tfa98xx_DMEM_XMEM);
	PRINT_ASSERT(err);

	/* Reset DSP once for sure after initializing */
	if (err == Tfa98xx_Error_Ok) {
		err = tfa98xx_dsp_reset(handle, 0); /* in pair of tfa98xx_init() - tfaRunStartup() */
		PRINT_ASSERT(err);
	}

	/* Sample rate is needed to set the correct tables */
	err = tfa98xx_dsp_write_tables(handle, TFA_GET_BF(handle, AUDFS));
	PRINT_ASSERT(err);

	return err;
}

/*
 * start the clocks and wait until the AMP is switching
 *  on return the DSP sub system will be ready for loading
 */
enum Tfa98xx_Error tfaRunStartup(Tfa98xx_handle_t handle, int profile)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	nxpTfaDeviceList_t *dev = tfaContDevice(handle);
	int tries, status, i, noinit = 0;

	if (dev == NULL)
		return Tfa98xx_Error_Fail;

	/* process the device list to see if the user implemented the noinit */
	for (i = 0; i < dev->length; i++) {
		if (dev->list[i].type == dscNoInit) {
			noinit = 1;
			break;
		}
	}

	if (!noinit) {
		/* load the optimal TFA98XX in HW settings */
		err = tfa98xx_init(handle);
		PRINT_ASSERT(err);
	} else {
		pr_debug("\nWarning: No init keyword found in the cnt file. Init is skipped! \n");
	}

	/* I2S settings to define the audio input properties
	 *  these must be set before the subsys is up */
	/* this will run the list until a non-register item is encountered */
	err = tfaContWriteRegsDev(handle); /* write device register settings */
	PRINT_ASSERT(err);
	/* also write register the settings from the default profile
	   NOTE we may still have ACS=1 so we can switch sample rate here */
	err = tfaContWriteRegsProf(handle, profile);
	PRINT_ASSERT(err);

	if (tfa98xx_dev_family(handle) == 2) {
		/* Factory trimming for the Boost converter */
		tfa_factory_trimmer(handle);
	}

	/* leave power off state */
	err = tfa98xx_powerdown(handle, 0);
	PRINT_ASSERT(err);

	if (tfa98xx_dev_family(handle) == 2) {
	/* signal that the clock settings are done
	 *  - PLL can start */
		TFA_SET_BF_VOLATILE(handle, MANSCONF, 1);
	}

	switch (TFA_GET_BF(handle, REV) & 0xff) {
	case 0x80:
		err = tfa98xx_dsp_reset(handle, 1);
		pr_debug("tfaRunStartup Reset DSP after power on\n");
		break;
	default:
		break;
	}

	/*  wait until the PLL is ready
	 *    note that the DSP CPU is not running (RST=1) */
	if (tfa98xx_runtime_verbose) {
		if (TFA_GET_BF(handle, NOCLK) && (tfa98xx_dev_family(handle) == 2))
			pr_debug("Using internal clock\n");
		pr_debug("Waiting for DSP system stable...\n");
	}
	for (tries = 1; tries < CFSTABLE_TRIES; tries++) {
		err = tfa98xx_dsp_system_stable(handle, &status);
		_ASSERT(err == Tfa98xx_Error_Ok);
		if (status)
			break;
		else
			msleep_interruptible(10); /* wait 10ms to avoid busload */
	}
	if (tries == CFSTABLE_TRIES) {
		if (tfa98xx_runtime_verbose)
			pr_debug("Timed out\n");
		return Tfa98xx_Error_StateTimedOut;
	}  else
		if (tfa98xx_runtime_verbose)
			pr_debug(" OK (tries=%d)\n", tries);

	if (tfa98xx_runtime_verbose && tfa98xx_dev_family(handle) == 2)
		err = show_current_state(handle);

	return err;
}

/*
 * run the startup/init sequence and set ACS bit
 */
enum Tfa98xx_Error tfaRunColdStartup(Tfa98xx_handle_t handle, int profile)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;

	err = tfaRunStartup(handle, profile);
	PRINT_ASSERT(err);
	if (err)
		return err;

	/* force cold boot */
	err = tfaRunColdboot(handle, 1);
	PRINT_ASSERT(err);
	if (err)
		return err;

	/* start */
	err = tfaRunStartDSP(handle);
	PRINT_ASSERT(err);

	return err;
}

/*
 *
 */
enum Tfa98xx_Error tfaRunMute(Tfa98xx_handle_t handle)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	int status;
	int tries = 0;

	/* signal the TFA98XX to mute  */
	if (tfa98xx_dev_family(handle) == 1) {
		err = tfa98xx_set_mute(handle, Tfa98xx_Mute_Amplifier);

		if (err == Tfa98xx_Error_Ok) {
			/* now wait for the amplifier to turn off */
			do {
				status = TFA_GET_BF(handle, SWS);
				if (status != 0)
					msleep_interruptible(10); /* wait 10ms to avoid busload */
				else
					break;
				tries++;
			}  while (tries < AMPOFFWAIT_TRIES);


			/* if (tfa98xx_runtime_verbose) */
				pr_info("-------------------- muted --------------------\n");

			/*The amplifier is always switching*/
			if (tries == AMPOFFWAIT_TRIES)
				return Tfa98xx_Error_Other;
		}
	}

	return err;
}
/*
 *
 */
enum Tfa98xx_Error tfaRunUnmute(Tfa98xx_handle_t handle)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	int retry = 0;

	if (tfa98xx_runtime_verbose)
		pr_debug("Entering unmute\n");

	/* signal the TFA98XX to mute  */
	err = tfa98xx_set_mute(handle, Tfa98xx_Mute_Off);
	if (err)
		pr_err("Unmute failed\n");

	if (tfa98xx_dev_family(handle) == 2) {
		int manstate = TFA_GET_BF(handle, MANSTATE);
		show_current_state(handle);

		if ((TFA_GET_BF(handle, CFE) != 0) && (9 == manstate)) {
			/* handset mode */
			TFA_SET_BF_VOLATILE(handle, AMPE, 1);
			return err;
		} else {
			if ((TFA_GET_BF(handle, REFCKSEL) == 1) && (TFA_GET_BF(handle, MANSTATE) == 6)) {
				pr_info("tfaUnmute() MANSCONF and MANCOLD will be switching to external clock\n");
				TFA_SET_BF_VOLATILE(handle, MANSCONF, 0);
				TFA_SET_BF_VOLATILE(handle, RST, 1);
				TFA_SET_BF_VOLATILE(handle, MANCOLD, 1);
				/* set ACS=0 to avoid DSP think it's real cold start */
				err = tfaRunColdboot(handle, 0);
				TFA_SET_BF_VOLATILE(handle, SBSL, 1);
				pr_info("tfaUnmute()  SBSL=1  switching to external clock\n");
				TFA_SET_BF_VOLATILE(handle, REFCKSEL, 0);
				TFA_SET_BF_VOLATILE(handle, RST, 0);
				TFA_SET_BF_VOLATILE(handle, SBSL, 0);
				pr_info("tfaUnmute() REFCKSEL=0  SBSL=0 switched to external clock\n");
			}
		}

		do {
			manstate = TFA_GET_BF(handle, MANSTATE);
			if (manstate <= 1) {
				TFA_SET_BF_VOLATILE(handle, MANSCONF, 1);
				break;
			}
			retry++;
			pr_info("tfaUnmute() MANSTATE %d, retry times %d\n", manstate, retry);
			udelay(300);
		} while (retry < 10);

		udelay(5000);
		/* SL: Enable everything,
		 * there will be sound after clock and sound
		 * applied
		 */
		TFA_SET_BF_VOLATILE(handle, SBSL, 1);
		TFA_SET_BF_VOLATILE(handle, AMPE, 1);
		show_current_state(handle);
		/* SL: when device is warm, enable partial update for next profile switching */
		tfa_set_partial_update(1);
	}

	/* if (tfa98xx_runtime_verbose) */
		pr_info("-------------------unmuted ------------------\n");

	return err;
}


/*
 * wait for calibrateDone
 */
enum Tfa98xx_Error tfaRunWaitCalibration(Tfa98xx_handle_t handle, int *calibrateDone)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	int tries = 0, mtp_busy = 1, tries_mtp_busy = 0;

	*calibrateDone = 0;

	/* in case of calibrate once wait for MTPEX */
	if (TFA_GET_BF(handle, MTPOTC)) {
		/* Check if MTP_busy is clear! */
		while (tries_mtp_busy < MTPBWAIT_TRIES) {
			mtp_busy = TFA_GET_BF(handle, MTPB);
			if (mtp_busy == 1)
				msleep_interruptible(10); /* wait 10ms to avoid busload */
			else
				break;
			tries_mtp_busy++;
		}

		if (tries_mtp_busy < MTPBWAIT_TRIES) {
			/* Because of the msleep TFA98XX_API_WAITRESULT_NTRIES is way to long!
				* Setting this to 25 will take it atleast 25*50ms = 1.25 sec
				*/
			while ((*calibrateDone == 0) && (tries < MTPEX_WAIT_NTRIES)) {
				*calibrateDone = TFA_GET_BF(handle, MTPEX);
				if (*calibrateDone == 1)
					break;
				msleep_interruptible(50); /* wait 50ms to avoid busload */
				tries++;
			}

			if (tries >= MTPEX_WAIT_NTRIES) {
				tries = TFA98XX_API_WAITRESULT_NTRIES;
			}
		} else {
			pr_err("MTP bussy after %d tries\n", MTPBWAIT_TRIES);
		}
	}

	/* poll xmem for calibrate always
		* calibrateDone = 0 means "calibrating",
		* calibrateDone = -1 (or 0xFFFFFF) means "fails"
		* calibrateDone = 1 means calibration done
		*/
	while ((*calibrateDone != 1) && (tries < TFA98XX_API_WAITRESULT_NTRIES)) {
		err = tfa98xx_dsp_read_mem(handle, TFA_FW_XMEM_CALIBRATION_DONE, 1, calibrateDone);
		tries++;
	}

	if (*calibrateDone != 1) {
		pr_err("Calibration failed! \n");
		err = Tfa98xx_Error_Bad_Parameter;
	} else if (tries == TFA98XX_API_WAITRESULT_NTRIES) {
		pr_debug("Calibration has timedout! \n");
		err = Tfa98xx_Error_StateTimedOut;
	} else if (tries_mtp_busy == 1000) {
		pr_err("Calibrate Failed: MTP_busy stays high! \n");
		err = Tfa98xx_Error_StateTimedOut;
	}

	/* Check which speaker calibration failed. Only for 88C */
	if ((err != Tfa98xx_Error_Ok) && ((handles_local[handle].rev & 0x0FFF) == 0xc88)) {
		individual_calibration_results(handle);
	}

#ifdef CONFIG_DEBUG_FS
	tfa98xx_deferred_calibration_status(handle, *calibrateDone);
#endif
	return err;
}

enum tfa_error tfa_start(int next_profile, int *vstep)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	int dev, devcount = tfa98xx_cnt_max_device();
	int cal_profile = -1, istap_prof = 0, active_profile = -1;

	if (devcount < 1) {
		pr_err("No or wrong container file loaded\n");
		return	tfa_error_bad_param;
	}

	for (dev = 0; dev < devcount; dev++) {
		pr_info("tfa_start()    dev=%d    will perform tfaContOpen()\n", dev);
		err = tfaContOpen(dev);
		if (err != Tfa98xx_Error_Ok) {
			pr_err("tfa_start()	   tfaContOpen error(%d)\n", err);
			goto error_exit;
		}

		/* Get currentprofile */
		active_profile = tfa_get_swprof(dev);
		pr_info("tfa_start()	   tfa_get_swprof active_profile(%d)\n", active_profile);
		if (active_profile == 0xff)
			active_profile = -1;

		/* Search if there is a calibration profile
		 * Only if the user did not give a specific profile and coldstart
		 */
		if (active_profile == -1 && next_profile < 1) {
			pr_info("tfa_start()    will perform tfaContGetCalProfile()\n");
			cal_profile = tfaContGetCalProfile(dev);
			if (cal_profile >= 0)
				next_profile = cal_profile;
		}
		/* Check if next profile is a tap profile */
		istap_prof = tfaContIsTapProfile(dev, next_profile);

		/* tfaRun_SpeakerBoost implies un-mute */
		if (tfa98xx_runtime_verbose) {
			pr_debug("active_profile:%s, next_profile:%s\n",
					tfaContProfileName(dev, active_profile),
					tfaContProfileName(dev, next_profile));
			pr_debug("Starting device [%s]\n", tfaContDeviceName(dev));

			if (tfa98xx_dev_family(dev) == 2) {
				err = show_current_state(dev);
			}
		}

		/* enable I2S output on TFA1 devices without TDM */
		pr_info("tfa_start()    will perform tfa98xx_aec_output()\n");
		err = tfa98xx_aec_output(dev, 1);
		if (err != Tfa98xx_Error_Ok) {
			pr_err("tfa_start()	   tfa98xx_aec_output error(%d)\n", err);
			goto error_exit;
		}
		tfa98xx_dump_register(getHandle(dev), 2, "tfa_start after tfa98xx_aec_output");

		/* Check if we need coldstart or ACS is set */
		pr_info("tfa_start()    will perform tfaRunSpeakerBoost()\n");
		err = tfaRunSpeakerBoost(dev, 0, next_profile);
		show_current_state(dev);
		if (err != Tfa98xx_Error_Ok) {
			pr_err("tfa_start()	   tfaRunSpeakerBoost error(%d)\n", err);
			goto error_exit;
		}
		active_profile = tfa_get_swprof(dev);
		tfa98xx_dump_register(getHandle(dev), 2, "tfa_start after tfaRunSpeakerBoost");

		/* After loading calibration profile we need to load acoustic shock profile */
		if (cal_profile >= 0) {
			next_profile = 0;
			pr_debug("Loading %s profile! \n", tfaContProfileName(dev, next_profile));
		}
	}

	for (dev = 0; dev < devcount; dev++) {
		/* check if the profile and steps are the one we want */
		/* was it not done already */
		if ((next_profile != active_profile && active_profile != -1)
		       || (istap_prof == 1)) {
			tfa98xx_dump_register(getHandle(dev), 2, "tfa_start before tfaContWriteProfile");
			pr_info("tfa_start()	will perform tfaContWriteProfile()\n");
			err = tfaContWriteProfile(dev, next_profile, vstep[dev]);
			if (err != Tfa98xx_Error_Ok) {
				pr_err("tfa_start()	   tfaContWriteProfile error(%d)\n", err);
				goto error_exit;
			}
		}

		pr_info("tfa_start()	after performed tfaContWriteProfile()\n");
		/* If the profile contains the .standby suffix go to powerdown
		 * else we should be in operating state
		 */
		if (strnstr(tfaContProfileName(dev, next_profile), ".standby", strlen(tfaContProfileName(dev, next_profile))) != NULL) {
			/* SL: standby profile is called and goes to return immediately */
			tfa_set_swprof(dev, (unsigned short)next_profile);
			tfa_set_swvstep(dev, (unsigned short)tfaContGetCurrentVstep(dev));
			goto error_exit;
		} else if (TFA_GET_BF(dev, PWDN) != 0) {
			err = tfa_cf_powerup(dev);
		}

		if ((TFA_GET_BF(dev, CFE) != 0)
			 && (vstep[dev] != tfaContGetCurrentVstep(dev) && vstep[dev] != -1)) {

			err = tfaContWriteFilesVstep(dev, next_profile, vstep[dev]);
			if (err != Tfa98xx_Error_Ok)
				goto error_exit;
			/* Always search and apply filters after a new vstep is applied */
			err = tfa_set_filters(dev, next_profile);
			if (err != Tfa98xx_Error_Ok)
				goto error_exit;
		}
		if (err != Tfa98xx_Error_Ok)
			goto error_exit;

		if (tfa98xx_runtime_verbose && tfa98xx_dev_family(dev) == 2)
			err = show_current_state(dev);

		tfa_set_swprof(dev, (unsigned short)next_profile);
		tfa_set_swvstep(dev, (unsigned short)tfaContGetCurrentVstep(dev));

		tfaRunUnmute(dev);	/* unmute at final stage */
	}

error_exit:
	for (dev = 0; dev < devcount; dev++) {
		if (tfa98xx_runtime_verbose && tfa98xx_dev_family(dev) == 2)
			show_current_state(dev);
		tfaContClose(dev); /* close all of them */
	}
	return err;
}

enum tfa_error tfa_stop(void)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	int dev, devcount = tfa98xx_cnt_max_device();

	if (devcount == 0) {
		pr_err("No or wrong container file loaded\n");
		return	tfa_error_bad_param;
	}

	for (dev = 0; dev < devcount; dev++) {
		err = tfaContOpen(dev);
		if (err != Tfa98xx_Error_Ok)
			goto error_exit;
		if (tfa98xx_runtime_verbose)
			pr_debug("Stopping device [%s]\n", tfaContDeviceName(dev));

		if (tfa98xx_dev_family(dev) == 2) {/* Max2 */
			int stop_profile = 0;
			int vstep[2] = {0};

			do {
				if (strnstr(tfaContProfileName(dev, stop_profile), ".standby", strlen(tfaContProfileName(dev, stop_profile))) != NULL) {
					/*SL: Found the stop profile*/
					break;
				}
				stop_profile++;
			} while (stop_profile <= tfaContMaxProfile(dev));

			vstep[dev] = tfaContGetCurrentVstep(dev);
			if (stop_profile != tfaContMaxProfile(dev)) {
				/*SL: Switch to powerdown profile */
				err = tfaContWriteProfile(dev, stop_profile, vstep[dev]);
				/* SL: standby profile is called and goes to return immediately */
				tfa_set_swprof(dev, (unsigned short)stop_profile);
				tfa_set_swvstep(dev, (unsigned short)tfaContGetCurrentVstep(dev));
			}
			if (err != Tfa98xx_Error_Ok)
				goto error_exit;
		} else {/* Max1 */
			/* mute */
			tfaRunMute(dev);
			/* powerdown CF */
			err = tfa98xx_powerdown(dev, 1);
			if (err != Tfa98xx_Error_Ok)
				goto error_exit;

			/* disable I2S output on TFA1 devices without TDM */
			err = tfa98xx_aec_output(dev, 0);
			if (err != Tfa98xx_Error_Ok)
				goto error_exit;
		}
	}

error_exit:
	for (dev = 0; dev < devcount; dev++)
		tfaContClose(dev); /* close all of them */
	return err;
}

/*
 *  int registers and coldboot dsp
 */
int tfa98xx_reset(Tfa98xx_handle_t handle)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;

	/* TFA_SET_BF_VOLATILE(handle, I2CR, 1); */
    if (tfa98xx_dev_family(handle) == 2) {
		/* restore MANSCONF and MANCOLD to POR state */
		TFA_SET_BF_VOLATILE(handle, MANSCONF, 0);
		TFA_SET_BF_VOLATILE(handle, MANCOLD, 1);
    }

	/* for clock */
	err = tfa_cf_powerup(handle);
	PRINT_ASSERT(err);

	/* force cold boot */
	err = tfaRunColdboot(handle, 1);
	PRINT_ASSERT(err);

	/* reset all i2C registers to default */
	err = -TFA_SET_BF(handle, I2CR, 1);
	PRINT_ASSERT(err);

	return err;
}

enum tfa_error tfa_reset(void)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	int dev, devcount = tfa98xx_cnt_max_device();

	for (dev = 0; dev < devcount; dev++) {
		err = tfaContOpen(dev);
		if (err != Tfa98xx_Error_Ok)
			break;
		if (tfa98xx_runtime_verbose)
			pr_debug("resetting device [%s]\n", tfaContDeviceName(dev));
		err = tfa98xx_reset(dev);
		if (err != Tfa98xx_Error_Ok)
			break;
	}

	for (dev = 0; dev < devcount; dev++) {
		tfaContClose(dev);
	}

	return err;
}

/*
 * Write all the bytes specified by num_bytes and data
 */
enum Tfa98xx_Error
tfa98xx_write_data(Tfa98xx_handle_t handle,
		  unsigned char subaddress, int num_bytes,
		  const unsigned char data[])
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	/* subaddress followed by data */
	const int bytes2write = num_bytes + 1;
	unsigned char *write_data;

	if (num_bytes > TFA2_MAX_PARAM_SIZE)
		return Tfa98xx_Error_Bad_Parameter;

	write_data = (unsigned char *)kmalloc(bytes2write, GFP_KERNEL);
	if (write_data == NULL)
		return Tfa98xx_Error_Fail;

	write_data[0] = subaddress;
	memcpy(&write_data[1], data, num_bytes);

	error = tfa98xx_write_raw(handle, bytes2write, write_data);

	kfree (write_data);
	return error;
}

/*
 * fill the calibration value as milli ohms in the struct
 *
 *  assume that the device has been calibrated
 */
enum Tfa98xx_Error tfa_dsp_get_calibration_impedance(Tfa98xx_handle_t handle)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	int spkr_count, nr_bytes, i;
	unsigned char bytes[6] = {0};
	int data[2];

	error = tfa98xx_supported_speakers(handle, &spkr_count);
	if (error == Tfa98xx_Error_Ok) {
		/* If calibrate=once then get values from MTP */
		if (TFA_GET_BF(handle, MTPOTC) && ((handles_local[handle].rev & 0xff) == 0x88)) {
			if (tfa98xx_runtime_verbose)
				pr_debug("Getting calibration values from MTP\n");

			for (i = 0; i < spkr_count; i++) {
				handles_local[handle].mohm[i] = tfa_read_reg(handle, (uint16_t)TFA_MK_BF((0xF4 + i), 0, 16));
			}
		} else {
			/* Get values from speakerboost */
			if (tfa98xx_runtime_verbose)
				pr_debug("Getting calibration values from Speakerboost\n");
			nr_bytes = spkr_count * 3;
			error = tfa_dsp_cmd_id_write_read(handle, MODULE_SPEAKERBOOST, SB_PARAM_GET_RE0, nr_bytes, bytes);
			if (error == Tfa98xx_Error_Ok) {
				tfa98xx_convert_bytes2data(nr_bytes, bytes, data);
				for (i = 0; i < spkr_count; i++) {
					handles_local[handle].mohm[i] = (data[i]*1000)/TFA_FW_ReZ_SCALE;
				}
			} else {
				for (i = 0; i < spkr_count; i++)
					handles_local[handle].mohm[i] = -1;
			}
		}
	}

	return error;
}

/* start count from 1, 0 is invalid */
int tfa_get_swprof(Tfa98xx_handle_t handle)
{
	/* get from register if not set yet */
	if (handles_local[handle].profile < 0)
		/* get current profile, consider invalid if 0 */
		handles_local[handle].profile = TFA_GET_BF(handle, SWPROFIL)-1;

	return handles_local[handle].profile;
}

int tfa_set_swprof(Tfa98xx_handle_t handle, unsigned short new_value)
{
	int mtpk, active_value = tfa_get_swprof(handle);

	handles_local[handle].profile = new_value;

	if (handles_local[handle].tfa_family > 1) {
		TFA_SET_BF_VOLATILE(handle, SWPROFIL, new_value+1);
	} else {
		/* it's in MTP shadow, so unlock if not done already */
		mtpk = TFA_GET_BF(handle, MTPK); /* get current key */
		TFA_SET_BF_VOLATILE(handle, MTPK, 0x5a);
		TFA_SET_BF_VOLATILE(handle, SWPROFIL, new_value+1); /* set current profile */
		TFA_SET_BF_VOLATILE(handle, MTPK, (uint16_t)mtpk); /* restore key */
	}

	return active_value;
}

/*   same value for all channels
 * start count from 1, 0 is invalid */
int tfa_get_swvstep(Tfa98xx_handle_t handle)
{
	int value;

	if (handles_local[handle].vstep[0] > 0)
		return handles_local[handle].vstep[0] - 1;

	value = TFA_GET_BF(handle, SWVSTEP); /* get current vstep[0] */

	handles_local[handle].vstep[0] = value;
	handles_local[handle].vstep[1] = value;
	return value-1; /* invalid if 0 */
}
int tfa_set_swvstep(Tfa98xx_handle_t handle, unsigned short new_value)
{
	int mtpk, active_value = tfa_get_swvstep(handle);

	handles_local[handle].vstep[0] = new_value;
	handles_local[handle].vstep[1] = new_value;

	if (handles_local[handle].tfa_family > 1) {
		TFA_SET_BF_VOLATILE(handle, SWVSTEP, new_value+1);
	} else {
		/* it's in MTP shadow, so unlock if not done already */
		mtpk = TFA_GET_BF(handle, MTPK); /* get current key */
		TFA_SET_BF_VOLATILE(handle, MTPK, 0x5a);
		TFA_SET_BF_VOLATILE(handle, SWVSTEP, new_value+1); /* set current vstep[0] */
		TFA_SET_BF_VOLATILE(handle, MTPK, (uint16_t)mtpk); /* restore key */
	}

	return active_value;
}

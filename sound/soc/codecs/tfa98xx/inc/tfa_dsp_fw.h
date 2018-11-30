/*
 * Copyright 2014-2017 NXP Semiconductors
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TFA98XX_INTERNALS_H
#define TFA98XX_INTERNALS_H

#include "config.h"

#include "tfa_service.h"
/*
 * tfadsp_fw_api.c
 */
/**
 * Return a text version of the firmware status ID code
 * @param status the given status ID code
 * @return the firmware status ID string
 */
const char *tfadsp_fw_status_string(enum Tfa98xx_Status_ID status);
int tfadsp_fw_start(struct tfa_device *tfa, int prof_idx, int vstep_idx);
int tfadsp_fw_get_api_version(struct tfa_device *tfa, uint8_t *buffer);
#define FW_MAXTAG 150
int tfadsp_fw_get_tag(struct tfa_device *tfa, uint8_t *buffer);
int tfadsp_fw_get_status_change(struct tfa_device *tfa, uint8_t *buffer);
int tfadsp_fw_set_re25(struct tfa_device *tfa, int prim, int sec);
int tfadsp_fw_get_re25(struct tfa_device *tfa, uint8_t *buffer);

/*
 * the order matches the ACK bits order in TFA98XX_CF_STATUS
 */
enum tfa_fw_event { /* not all available on each device */
	tfa_fw_i2c_cmd_ack,
	tfa_fw_reset_start,
	tfa_fw_short_on_mips,
	tfa_fw_soft_mute_ready,
	tfa_fw_volume_ready,
	tfa_fw_error_damage,
	tfa_fw_calibrate_done,
	tfa_fw_max
};

/* the following type mappings are compiler specific */
#define subaddress_t unsigned char

/* module Ids */
#define MODULE_FRAMEWORK        0
#define MODULE_SPEAKERBOOST     1
#define MODULE_BIQUADFILTERBANK 2
#define MODULE_TAPTRIGGER		5
#define MODULE_SETRE 			9

/* RPC commands */
/* SET */
#define FW_PAR_ID_SET_MEMORY            0x03
#define FW_PAR_ID_SET_SENSES_DELAY      0x04
#define FW_PAR_ID_SETSENSESCAL          0x05
#define FW_PAR_ID_SET_INPUT_SELECTOR    0x06
#define FW_PAR_ID_SET_OUTPUT_SELECTOR   0x08
#define FW_PAR_ID_SET_PROGRAM_CONFIG    0x09
#define FW_PAR_ID_SET_GAINS             0x0A
#define FW_PAR_ID_SET_MEMTRACK          0x0B
#define FW_PAR_ID_SET_FWKUSECASE		0x11
#define TFA1_FW_PAR_ID_SET_CURRENT_DELAY 0x03
#define TFA1_FW_PAR_ID_SET_CURFRAC_DELAY 0x06
/* GET */
#define FW_PAR_ID_GET_MEMORY            0x83
#define FW_PAR_ID_GLOBAL_GET_INFO       0x84
#define FW_PAR_ID_GET_FEATURE_INFO      0x85
#define FW_PAR_ID_GET_MEMTRACK          0x8B
#define FW_PAR_ID_GET_TAG               0xFF
#define FW_PAR_ID_GET_API_VERSION 		0xFE
#define FW_PAR_ID_GET_STATUS_CHANGE		0x8D

/* Load a full model into SpeakerBoost. */
/* SET */
#define SB_PARAM_SET_ALGO_PARAMS        0x00
#define SB_PARAM_SET_LAGW               0x01
#define SB_PARAM_SET_ALGO_PARAMS_WITHOUT_RESET 0x02
#define SB_PARAM_SET_RE25C				0x05
#define SB_PARAM_SET_LSMODEL            0x06
#define SB_PARAM_SET_MBDRC              0x07
#define SB_PARAM_SET_MBDRC_WITHOUT_RESET 0x08
#define SB_PARAM_SET_EXCURSION_FILTERS	0x0A
#define SB_PARAM_SET_DRC                0x0F
/* GET */
#define SB_PARAM_GET_ALGO_PARAMS        0x80
#define SB_PARAM_GET_LAGW               0x81
#define SB_PARAM_GET_RE25C              0x85
#define SB_PARAM_GET_LSMODEL            0x86
#define SB_PARAM_GET_MBDRC				0x87
#define SB_PARAM_GET_MBDRC_DYNAMICS		0x89
#define SB_PARAM_GET_EXCURSION_FILTERS	0x8A
#define SB_PARAM_GET_TAG                0xFF

#define SB_PARAM_SET_EQ					0x0A	/* 2 Equaliser Filters. */
#define SB_PARAM_SET_PRESET             0x0D	/* Load a preset */
#define SB_PARAM_SET_CONFIG				0x0E	/* Load a config */
#define SB_PARAM_SET_AGCINS             0x10
#define SB_PARAM_SET_CURRENT_DELAY      0x03
#define SB_PARAM_GET_STATE              0xC0
#define SB_PARAM_GET_XMODEL             0xC1	/* Gets current Excursion Model. */
#define SB_PARAM_GET_XMODEL_COEFFS      0x8C    /* Get coefficients for XModel */
#define SB_PARAM_GET_EXCURSION_FILTERS  0x8A    /* Get excursion filters */
#define SB_PARAM_SET_EXCURSION_FILTERS  0x0A    /* Set excursion filters */

/*	SET: TAPTRIGGER */
#define TAP_PARAM_SET_ALGO_PARAMS		0x01
#define TAP_PARAM_SET_DECIMATION_PARAMS 0x02

/* GET: TAPTRIGGER*/
#define TAP_PARAM_GET_ALGO_PARAMS		0x81
#define TAP_PARAM_GET_TAP_RESULTS		0x84

/* sets the speaker calibration impedance (@25 degrees celsius) */
#define SB_PARAM_SET_RE0                0x89

#define BFB_PAR_ID_SET_COEFS            0x00
#define BFB_PAR_ID_GET_COEFS            0x80
#define BFB_PAR_ID_GET_CONFIG           0x81

/* for compatibility */
#define FW_PARAM_GET_STATE        	FW_PAR_ID_GLOBAL_GET_INFO
#define FW_PARAM_GET_FEATURE_BITS 	FW_PAR_ID_GET_FEATURE_BITS

/* RPC Status results */
#define STATUS_OK                  0
#define STATUS_INVALID_MODULE_ID   2
#define STATUS_INVALID_PARAM_ID    3
#define STATUS_INVALID_INFO_ID     4

/* the maximum message length in the communication with the DSP */
#define TFA2_MAX_PARAM_SIZE (507*3) /* TFA2 */
#define TFA1_MAX_PARAM_SIZE (145*3) /* TFA1 */

#define ROUND_DOWN(a, n) (((a)/(n))*(n))

/* feature bits */
#define FEATURE1_TCOEF 0x100 /* bit8 set means tCoefA expected */
#define FEATURE1_DRC   0x200 /* bit9 NOT set means DRC expected */

/* DSP firmware xmem defines */
#define TFA1_FW_XMEM_CALIBRATION_DONE	231
#define TFA2_FW_XMEM_CALIBRATION_DONE   516
#define TFA1_FW_XMEM_COUNT_BOOT			0xa1
#define TFA2_FW_XMEM_COUNT_BOOT			512
#define TFA2_FW_XMEM_CMD_COUNT			520

/* note that the following defs rely on the handle variable */
#define TFA_FW_XMEM_CALIBRATION_DONE 	TFA_FAM_FW(tfa, XMEM_CALIBRATION_DONE)
#define TFA_FW_XMEM_COUNT_BOOT 			TFA_FAM_FW(tfa, XMEM_COUNT_BOOT)
#define TFA_FW_XMEM_CMD_COUNT 			TFA_FAM_FW(tfa, XMEM_CMD_COUNT)

#define TFA2_FW_ReZ_SCALE	65536
#define TFA1_FW_ReZ_SCALE	16384

#endif /* TFA98XX_INTERNALS_H */

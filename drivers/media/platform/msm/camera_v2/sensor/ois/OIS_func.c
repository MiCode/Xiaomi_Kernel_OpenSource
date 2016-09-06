
/***** ROHM Confidential ***************************************************/
/*
#define	_USE_MATH_DEFINES
#include <math.h>
#include <conio.h>
#include <ctype.h>
*/
#include "OIS_head.h"
#include "OIS_prog.h"
#include "OIS_coef.h"
#include "OIS_prog1.h"
#include "OIS_coef1.h"
#include "OIS_defi.h"


OIS_UBYTE       *DOWNLOAD_BIN;
OIS_UBYTE       *DOWNLOAD_COEF;
OIS_UWORD	DOWNLOAD_COEF_LEN;
OIS_UWORD	DOWNLOAD_BIN_LEN;

extern	OIS_UWORD	OIS_REQUEST;

extern	double		OIS_PIXEL[2];

extern	OIS_WORD	CROP_X;
extern	OIS_WORD	CROP_Y;
extern	OIS_WORD	CROP_WIDTH;
extern	OIS_WORD 	CROP_HEIGHT;
extern	OIS_UBYTE	SLICE_LEVE;

extern	double		DISTANCE_BETWEEN_CIRCLE;
extern	double		DISTANCE_TO_CIRCLE;
extern	double		D_CF;

extern	OIS_UWORD 	ACT_DRV;
extern	OIS_UWORD 	FOCAL_LENGTH;
extern	double 		MAX_OIS_SENSE;
extern	double		MIN_OIS_SENSE;
extern	OIS_UWORD 	MAX_COIL_R;
extern	OIS_UWORD 	MIN_COIL_R;


OIS_UWORD 		u16_ofs_tbl[] = {

	0x0DFC,
	0x0A7D,
	0x06FE,
	0x037F,
	0x0000,
	0xFC80,
	0xF901,
	0xF582,
	0xF203,

};

ADJ_STS func_PROGRAM_DOWNLOAD(void)
{
	OIS_UWORD	sts;

	download(0, 0);
	sts = I2C_OIS_mem__read(_M_OIS_STS);

	if ((sts & 0x0004) == 0x0004) {

		OIS_UWORD u16_dat;

		u16_dat = I2C_OIS_mem__read(_M_FIRMVER);

		printk("%s: ADJ_OK\n", __func__);
		return ADJ_OK;
	} else {
		printk("%s: ADJ_DL_ERR\n", __func__);
		return PROG_DL_ERR;
	}
}


void func_COEF_DOWNLOAD(OIS_UWORD u16_coef_type)
{
	download(1, u16_coef_type);

}


extern int8_t  g_ois_vendor;
void	download(OIS_UWORD u16_type, OIS_UWORD u16_coef_type)
{
	#define		DWNLD_TRNS_SIZE		(32)

	OIS_UBYTE	temp[DWNLD_TRNS_SIZE+1];
	OIS_UWORD	block_cnt;
	OIS_UWORD	total_cnt;
	OIS_UWORD	lp;
	OIS_UWORD	n;
	OIS_UWORD	u16_i;

	if (g_ois_vendor == 1) {
		DOWNLOAD_BIN = DOWNLOAD_BIN_LITEON;
		DOWNLOAD_BIN_LEN = sizeof(DOWNLOAD_BIN_LITEON);
		DOWNLOAD_COEF = DOWNLOAD_COEF_LITEON;
		DOWNLOAD_COEF_LEN = sizeof(DOWNLOAD_COEF_LITEON);
	} else {
		DOWNLOAD_BIN = DOWNLOAD_BIN_SEMCO;
		DOWNLOAD_BIN_LEN = sizeof(DOWNLOAD_BIN_SEMCO);
		DOWNLOAD_COEF = DOWNLOAD_COEF_SEMCO;
		DOWNLOAD_COEF_LEN = sizeof(DOWNLOAD_COEF_SEMCO);
	}

	if	(u16_type == 0) {
		n		= DOWNLOAD_BIN_LEN;
	} else {
		n = DOWNLOAD_COEF_LEN;
	}
	block_cnt	= n / DWNLD_TRNS_SIZE + 1;
	total_cnt	= block_cnt;

	while (1) {

		if (block_cnt == 1)
			lp = n % DWNLD_TRNS_SIZE;
		else
			lp = DWNLD_TRNS_SIZE;

		if (lp != 0) {
			if (u16_type == 0) {
				temp[0] = _OP_FIRM_DWNLD;
				for (u16_i = 1; u16_i <= lp; u16_i += 1) {
					temp[u16_i] = DOWNLOAD_BIN[(total_cnt - block_cnt) * DWNLD_TRNS_SIZE + u16_i - 1];
				}
			} else {
				temp[0] = _OP_COEF_DWNLD;
				for (u16_i = 1; u16_i <= lp; u16_i += 1) {
					temp[u16_i] = DOWNLOAD_COEF[(total_cnt - block_cnt) * DWNLD_TRNS_SIZE + u16_i - 1];
				}
			}


pr_debug("DL_I2C");
		g_i2c_ctrl->i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
		g_i2c_ctrl->i2c_client.i2c_func_tbl->i2c_write_seq(
		&g_i2c_ctrl->i2c_client, temp[0], &(temp[1]), lp);

		}


		block_cnt = block_cnt - 1;
		if (block_cnt == 0) {
			break;
		}
	}
}


OIS_UWORD	INTG__INPUT;
OIS_UWORD	KGNTG_VALUE;


void SET_FADJ_PARAM(const _FACT_ADJ *param)
{

	I2C_OIS_per_write(_P_30_ADC_CH0, param->gl_CURDAT);

	I2C_OIS_per_write(_P_31_ADC_CH1, param->gl_HALOFS_X);
	I2C_OIS_per_write(_P_32_ADC_CH2, param->gl_HALOFS_Y);

	I2C_OIS_mem_write(_M_X_H_ofs, param->gl_HX_OFS);
	I2C_OIS_mem_write(_M_Y_H_ofs, param->gl_HY_OFS);

	I2C_OIS_per_write(_P_39_Ch3_VAL_1, param->gl_PSTXOF);
	I2C_OIS_per_write(_P_3B_Ch3_VAL_3, param->gl_PSTYOF);

	I2C_OIS_mem_write(_M_Kgx00, param->gl_GX_OFS);
	I2C_OIS_mem_write(_M_Kgy00, param->gl_GY_OFS);
	I2C_OIS_mem_write(_M_TMP_X_, param->gl_TMP_X_);
	I2C_OIS_mem_write(_M_TMP_Y_, param->gl_TMP_Y_);

	I2C_OIS_mem_write(_M_KgxHG, param->gl_KgxHG);
	I2C_OIS_mem_write(_M_KgyHG, param->gl_KgyHG);

	I2C_OIS_mem_write(_M_KgxH0, param->gl_KgxH0);
	I2C_OIS_mem_write(_M_KgyH0, param->gl_KgyH0);

	I2C_OIS_mem_write(_M_KgxG, param->gl_KGXG);
	I2C_OIS_mem_write(_M_KgyG, param->gl_KGYG);

	INTG__INPUT = I2C_OIS_mem__read(0x38);
	KGNTG_VALUE = I2C_OIS_mem__read(_M_KgxTG);

	I2C_OIS_mem_write(_M_EQCTL, 0x0C0C);
}

#define	ANGLE_LIMIT	(0x3020)

#define	G_SENSE		131
ADJ_STS	func_SET_SCENE_PARAM(OIS_UBYTE u16_scene, OIS_UBYTE u16_mode, OIS_UBYTE filter, OIS_UBYTE range, const _FACT_ADJ *param)
{
	OIS_UWORD u16_i;
	OIS_UWORD u16_dat;


	OIS_UBYTE u16_adr_target[3]        = { _M_Kgxdr, _M_X_LMT, _M_X_TGT,  };

	OIS_UWORD u16_dat_SCENE_NIGHT_1[3] = { 0x7FFE,   ANGLE_LIMIT,   G_SENSE * 16,    };
	OIS_UWORD u16_dat_SCENE_NIGHT_2[3] = { 0x7FFC,   ANGLE_LIMIT,   G_SENSE * 16,    };
	OIS_UWORD u16_dat_SCENE_NIGHT_3[3] = { 0x7FFA,   ANGLE_LIMIT,   G_SENSE * 16,    };

	OIS_UWORD u16_dat_SCENE_D_A_Y_1[3] = { 0x7FFE,   ANGLE_LIMIT,   G_SENSE * 40,    };
	OIS_UWORD u16_dat_SCENE_D_A_Y_2[3] = { 0x7FFA,   ANGLE_LIMIT,   G_SENSE * 40,    };
	OIS_UWORD u16_dat_SCENE_D_A_Y_3[3] = { 0x7FF0,   ANGLE_LIMIT,   G_SENSE * 40,    };

	OIS_UWORD u16_dat_SCENE_SPORT_1[3] = { 0x7FFE,   ANGLE_LIMIT,   G_SENSE * 60,    };
	OIS_UWORD u16_dat_SCENE_SPORT_2[3] = { 0x7FF0,   ANGLE_LIMIT,   G_SENSE * 60,    };
	OIS_UWORD u16_dat_SCENE_SPORT_3[3] = { 0x7FE0,   ANGLE_LIMIT,   G_SENSE * 60,    };

	OIS_UWORD u16_dat_SCENE_TEST___[3] = { 0x7FF0,   0x7FFF,   0x7FFF,    };


	OIS_UWORD *u16_dat_SCENE_;

	OIS_UBYTE	size_SCENE_tbl = sizeof(u16_dat_SCENE_NIGHT_1) / sizeof(OIS_UWORD);


	u16_dat = I2C_OIS_mem__read(_M_EQCTL);
	u16_dat = (u16_dat &  0xFEFE);
	I2C_OIS_mem_write(_M_EQCTL, u16_dat);


	switch (u16_scene) {
	case _SCENE_NIGHT_1:
		u16_dat_SCENE_ = u16_dat_SCENE_NIGHT_1;
		break;
	case _SCENE_NIGHT_2:
		u16_dat_SCENE_ = u16_dat_SCENE_NIGHT_2;
		break;
	case _SCENE_NIGHT_3:
		u16_dat_SCENE_ = u16_dat_SCENE_NIGHT_3;
		break;
	case _SCENE_D_A_Y_1:
		u16_dat_SCENE_ = u16_dat_SCENE_D_A_Y_1;
		break;
	case _SCENE_D_A_Y_2:
		u16_dat_SCENE_ = u16_dat_SCENE_D_A_Y_2;
		break;
	case _SCENE_D_A_Y_3:
		u16_dat_SCENE_ = u16_dat_SCENE_D_A_Y_3;
		break;
	case _SCENE_SPORT_1:
		u16_dat_SCENE_ = u16_dat_SCENE_SPORT_1;
		break;
	case _SCENE_SPORT_2:
		u16_dat_SCENE_ = u16_dat_SCENE_SPORT_2;
		break;
	case _SCENE_SPORT_3:
		u16_dat_SCENE_ = u16_dat_SCENE_SPORT_3;
		break;
	case _SCENE_TEST___:
		u16_dat_SCENE_ = u16_dat_SCENE_TEST___;
		break;
	default:
		u16_dat_SCENE_ = u16_dat_SCENE_TEST___;
		break;
	}


	for (u16_i = 0; u16_i < size_SCENE_tbl; u16_i += 1) {
		I2C_OIS_mem_write(u16_adr_target[u16_i], u16_dat_SCENE_[u16_i]);
	}
	for (u16_i = 0; u16_i < size_SCENE_tbl; u16_i += 1) {
		I2C_OIS_mem_write(u16_adr_target[u16_i] + 0x80,	u16_dat_SCENE_[u16_i]);
	}


	if (filter == 1) {
		u16_dat = I2C_OIS_mem__read(_M_EQCTL);
		u16_dat |= 0x4000;
		I2C_OIS_mem_write(_M_EQCTL, u16_dat);
	} else {
		u16_dat = I2C_OIS_mem__read(_M_EQCTL);
		u16_dat &= 0xBFFF;
		I2C_OIS_mem_write(_M_EQCTL, u16_dat);
	}


	I2C_OIS_mem_write(_M_wDgx02, 0x0000);
	I2C_OIS_mem_write(_M_wDgx03, 0x0000);
	I2C_OIS_mem_write(_M_wDgx06, 0x7FFF);
	I2C_OIS_mem_write(_M_Kgx15,  0x0000);

	I2C_OIS_mem_write(_M_wDgy02, 0x0000);
	I2C_OIS_mem_write(_M_wDgy03, 0x0000);
	I2C_OIS_mem_write(_M_wDgy06, 0x7FFF);
	I2C_OIS_mem_write(_M_Kgy15,  0x0000);



	if	(range == 1) {
		I2C_OIS_per_write(_P_31_ADC_CH1, param->gl_SFTHAL_X);
		I2C_OIS_per_write(_P_32_ADC_CH2, param->gl_SFTHAL_Y);
	} else {
		I2C_OIS_per_write(_P_31_ADC_CH1, param->gl_HALOFS_X);
		I2C_OIS_per_write(_P_32_ADC_CH2, param->gl_HALOFS_Y);
	}



	if ((u16_mode == 1)) {
		u16_dat = I2C_OIS_mem__read(_M_EQCTL);
		u16_dat = (u16_dat |  0x0101);
		I2C_OIS_mem_write(_M_EQCTL, u16_dat);
		DEBUG_printf(("SET : EQCTL:%.4x\n", u16_dat));
	} else {
		u16_dat = I2C_OIS_mem__read(_M_EQCTL);
		u16_dat = (u16_dat &  0xFEFE);
		I2C_OIS_mem_write(_M_EQCTL, u16_dat);
		DEBUG_printf(("SET : EQCTL:%.4x\n", u16_dat));
	}

	return ADJ_OK;
}


ADJ_STS	func_SET_SCENE_PARAM_for_NewGYRO_Fil(OIS_UBYTE u16_scene, OIS_UBYTE u16_mode, OIS_UBYTE filter, OIS_UBYTE range, const _FACT_ADJ *param)
{
	OIS_UWORD u16_i;
	OIS_UWORD u16_dat;

	OIS_UBYTE u16_adr_target[4]        = { _M_Kgxdr, _M_X_LMT, 		_M_X_TGT, 		0x1B,    };

	OIS_UWORD u16_dat_SCENE_NIGHT_1[4] = { 0x7FE0,   ANGLE_LIMIT,   G_SENSE * 16,   0x0300,  };
	OIS_UWORD u16_dat_SCENE_NIGHT_2[4] = { 0x7FFF,   ANGLE_LIMIT,   G_SENSE * 16,   0x0080,  };
	OIS_UWORD u16_dat_SCENE_NIGHT_3[4] = { 0x7FE0,   ANGLE_LIMIT,   G_SENSE * 16,   0x0200,  };

	OIS_UWORD u16_dat_SCENE_D_A_Y_1[4] = { 0x7FE0,   ANGLE_LIMIT,   G_SENSE * 40,   0x0300,  };
	OIS_UWORD u16_dat_SCENE_D_A_Y_2[4] = { 0x7F80,   ANGLE_LIMIT,   G_SENSE * 40,   0x0140,  };
	OIS_UWORD u16_dat_SCENE_D_A_Y_3[4] = { 0x7FE0,   ANGLE_LIMIT,   G_SENSE * 40,   0x0300,  };

	OIS_UWORD u16_dat_SCENE_SPORT_1[4] = { 0x7FE0,   ANGLE_LIMIT,   G_SENSE * 60,   0x0300,  };


	OIS_UWORD u16_dat_SCENE_SPORT_2[4] = { 0x7FFF,   ANGLE_LIMIT,   G_SENSE * 60,   0x0080,  };
	OIS_UWORD u16_dat_SCENE_SPORT_3[4] = { 0x7FC0,   ANGLE_LIMIT,   G_SENSE * 60,   0x0080,  };

	OIS_UWORD u16_dat_SCENE_TEST___[4] = { 0x7FFF,   0x7FFF,   		0x7FFF,   		0x0080,  };



	OIS_UWORD *u16_dat_SCENE_;

	OIS_UBYTE	size_SCENE_tbl = sizeof(u16_dat_SCENE_NIGHT_1) / sizeof(OIS_UWORD);


	u16_dat = I2C_OIS_mem__read(_M_EQCTL);
	u16_dat = (u16_dat &  0xFEFE);
	I2C_OIS_mem_write(_M_EQCTL, u16_dat);


	switch (u16_scene) {
	case _SCENE_NIGHT_1:
		u16_dat_SCENE_ = u16_dat_SCENE_NIGHT_1;
		break;
	case _SCENE_NIGHT_2:
		u16_dat_SCENE_ = u16_dat_SCENE_NIGHT_2;
		break;
	case _SCENE_NIGHT_3:
		u16_dat_SCENE_ = u16_dat_SCENE_NIGHT_3;
		break;
	case _SCENE_D_A_Y_1:
		u16_dat_SCENE_ = u16_dat_SCENE_D_A_Y_1;
		break;
	case _SCENE_D_A_Y_2:
		u16_dat_SCENE_ = u16_dat_SCENE_D_A_Y_2;
		break;
	case _SCENE_D_A_Y_3:
		u16_dat_SCENE_ = u16_dat_SCENE_D_A_Y_3;
		break;
	case _SCENE_SPORT_1:
		u16_dat_SCENE_ = u16_dat_SCENE_SPORT_1;
		break;
	case _SCENE_SPORT_2:
		u16_dat_SCENE_ = u16_dat_SCENE_SPORT_2;
		break;
	case _SCENE_SPORT_3:
		u16_dat_SCENE_ = u16_dat_SCENE_SPORT_3;
		break;
	case _SCENE_TEST___:
		u16_dat_SCENE_ = u16_dat_SCENE_TEST___;
		break;
	default:
		u16_dat_SCENE_ = u16_dat_SCENE_TEST___;
		break;
	}


	for (u16_i = 0; u16_i < size_SCENE_tbl; u16_i += 1) {
		I2C_OIS_mem_write(u16_adr_target[u16_i],          	u16_dat_SCENE_[u16_i]);
	}
	for (u16_i = 0; u16_i < size_SCENE_tbl; u16_i += 1) {
		I2C_OIS_mem_write(u16_adr_target[u16_i] + 0x80,	u16_dat_SCENE_[u16_i]);
	}


	{
		OIS_ULONG	temp;


		temp = (INTG__INPUT * 16384);
		u16_dat = temp / ANGLE_LIMIT;

		I2C_OIS_mem_write(0x38, u16_dat);
		I2C_OIS_mem_write(0xB8, u16_dat);
		temp = (KGNTG_VALUE * ANGLE_LIMIT);
		u16_dat = temp / 16384;

		I2C_OIS_mem_write(0x47, u16_dat);
		I2C_OIS_mem_write(0xC7, u16_dat);

		I2C_OIS_mem_write(0x40, 0x7FF0);
		I2C_OIS_mem_write(0xC0, 0x7FF0);
	}


	if (filter == 1) {
		u16_dat = I2C_OIS_mem__read(_M_EQCTL);
		u16_dat |= 0x4000;
		I2C_OIS_mem_write(_M_EQCTL, u16_dat);
	} else {
		u16_dat = I2C_OIS_mem__read(_M_EQCTL);
		u16_dat &= 0xBFFF;
		I2C_OIS_mem_write(_M_EQCTL, u16_dat);
	}

	if	(range == 1) {
		I2C_OIS_per_write(_P_31_ADC_CH1, param->gl_SFTHAL_X);
		I2C_OIS_per_write(_P_32_ADC_CH2, param->gl_SFTHAL_Y);
	} else {
		I2C_OIS_per_write(_P_31_ADC_CH1, param->gl_HALOFS_X);
		I2C_OIS_per_write(_P_32_ADC_CH2, param->gl_HALOFS_Y);
	}

	if ((u16_mode == 1)) {
		u16_dat = I2C_OIS_mem__read(_M_EQCTL);
		u16_dat = (u16_dat &  0xEFFF);
		u16_dat = (u16_dat |  0x0101);
		I2C_OIS_mem_write(_M_EQCTL, u16_dat);
		DEBUG_printf(("SET : EQCTL:%.4x\n", u16_dat));
	} else if (u16_mode == 2) {
		u16_dat = I2C_OIS_mem__read(_M_EQCTL);
		u16_dat = (u16_dat |  0x1101);
		I2C_OIS_mem_write(_M_EQCTL, u16_dat);
		DEBUG_printf(("SET : EQCTL:%.4x\n", u16_dat));
	} else {
		u16_dat = I2C_OIS_mem__read(_M_EQCTL);
		u16_dat = (u16_dat &  0xFEFE);
		I2C_OIS_mem_write(_M_EQCTL, u16_dat);
		DEBUG_printf(("SET : EQCTL:%.4x\n", u16_dat));
	}

	return ADJ_OK;
}

void	HalfShutterOn(void)
{
	OIS_UWORD u16_dat = 0;

	u16_dat = I2C_OIS_mem__read(_M_EQCTL);
	u16_dat = (u16_dat |  0x1101);
	I2C_OIS_mem_write(_M_EQCTL, u16_dat);
	DEBUG_printf(("SET : EQCTL:%.4x\n", u16_dat));
}

#define DEFAULT_10CM_GAIN (0x3489)

void	EnableShiftOIS(void)
{
	I2C_OIS_per_write(0x8B, DEFAULT_10CM_GAIN);
	I2C_OIS_per_write(0xCB, DEFAULT_10CM_GAIN);
	I2C_OIS_mem_write(0x10, 0x0000);
}

void	DisableShiftOIS(void)
{
	I2C_OIS_mem_write(0x10, 0x7FFF);
	I2C_OIS_per_write(0x8B, 0x0000);
	I2C_OIS_per_write(0xCB, 0x0000);
}

void	ChangeShiftOISGain(int distance)
{
	int shift_gain = DEFAULT_10CM_GAIN * 10 / distance;

	I2C_OIS_per_write(0x8B, shift_gain);
	I2C_OIS_per_write(0xCB, shift_gain);
}

void	I2C_OIS_per_write(OIS_UBYTE u08_adr, OIS_UWORD u16_dat)
{

	OIS_UBYTE	out[4];

	out[0] = _OP_Periphe_RW;
	out[1] = u08_adr;
	out[2] = (u16_dat) & 0xFF;
	out[3] = (u16_dat >> 8) & 0xFF;

	WR_I2C(_SLV_OIS_, 4, out);
}

void	I2C_OIS_mem_write(OIS_UBYTE u08_adr, OIS_UWORD u16_dat)
{

	OIS_UBYTE	out[4];

	out[0] = _OP_Memory__RW;
	out[1] = u08_adr;
	out[2] = (u16_dat) & 0xFF;
	out[3] = (u16_dat >> 8) & 0xFF;

	WR_I2C(_SLV_OIS_, 4, out);
}

OIS_UWORD	I2C_OIS_per__read(OIS_UBYTE u08_adr)
{

	OIS_UBYTE	u08_dat[2];

	u08_dat[0] = _OP_Periphe_RW;
	u08_dat[1] = u08_adr;

	return RD_I2C(_SLV_OIS_, 2, u08_dat);
}

OIS_UWORD	I2C_OIS_mem__read(OIS_UBYTE u08_adr)
{

	OIS_UBYTE	u08_dat[2];

	u08_dat[0] = _OP_Memory__RW;
	u08_dat[1] = u08_adr;

	return RD_I2C(_SLV_OIS_, 2, u08_dat);
}

void	I2C_OIS_spcl_cmnd(OIS_UBYTE u08_on, OIS_UBYTE u08_dat)
{

	if ((u08_dat == _cmd_8C_EI) ||
		(u08_dat == _cmd_8C_DI)) {

		OIS_UBYTE out[2];

		out[0] = _OP_SpecialCMD;
		out[1] = u08_dat;

pr_debug("SPCL WR_I2C 0x%x 0x%x", out[0], out[1]);

		g_i2c_ctrl->i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
		g_i2c_ctrl->i2c_client.i2c_func_tbl->i2c_write(
		&g_i2c_ctrl->i2c_client, out[0], out[1], MSM_CAMERA_I2C_BYTE_DATA);

	}
}

void	I2C_OIS_F0123_wr_(OIS_UBYTE u08_dat0, OIS_UBYTE u08_dat1, OIS_UWORD u16_dat2)
{

	OIS_UBYTE out[5];

	out[0] = 0xF0;
	out[1] = u08_dat0;
	out[2] = u08_dat1;
	out[3] = u16_dat2 / 256;
	out[4] = u16_dat2 % 256;

pr_debug("SPCL WR_I2C 0x%x 0x%x dat2:%d", out[0], out[1], u16_dat2*2);

	g_i2c_ctrl->i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
	g_i2c_ctrl->i2c_client.i2c_func_tbl->i2c_write_seq(
		&g_i2c_ctrl->i2c_client, out[0], &(out[1]), 4);

}

OIS_UWORD	I2C_OIS_F0123__rd(void)
{

	OIS_UBYTE	u08_dat;
	u08_dat = 0xF0;
	return RD_I2C(_SLV_OIS_, 1, &u08_dat);
}


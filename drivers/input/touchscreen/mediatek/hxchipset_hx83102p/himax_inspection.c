/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include "himax_inspection.h"

static int g_gap_vertical_partial = 3;
static int *g_gap_vertical_part;
static int g_gap_horizontal_partial = 3;
static int *g_gap_horizontal_part;

static int g_dc_max;

static int g_1kind_raw_size;
uint32_t g_rslt_data_len;
int **g_inspection_criteria;
int *g_inspt_crtra_flag;
int *g_test_item_flag;
int do_lpwg_test;
int HX_CRITERIA_ITEM;
int HX_CRITERIA_SIZE;
char *g_rslt_data;
bool file_w_flag;
static char g_file_path[256];
static char g_rslt_log[256];
static char g_start_log[512];
#define FAIL_IN_INDEX "%s: %s FAIL in index %d\n"
#define FAIL_IN_INDEX_CRTRA \
	"%s: %s FAIL in index %d,max=%d, min=%d, RAW=%d\n"
char *g_hx_head_str[] = {
	"TP_Info",
	"Project_Info",
	"TestItem",
	"TestCriteria",
	NULL
};

/*Need to map THP_INSPECTION_ENUM*/
char *g_himax_inspection_mode[] = {
	"HIMAX_OPEN",
	"HIMAX_MICRO_OPEN",
	"HIMAX_SHORT",
	"HIMAX_SC",
	"HIMAX_WEIGHT_NOISE",
	"HIMAX_ABS_NOISE",
	"HIMAX_RAWDATA",
	"HIMAX_BPN_RAWDATA",
	"HIMAX_SORTING",
	"HIMAX_GAPTEST_RAW",
	/*"HIMAX_GAPTEST_RAW_X",*/
	/*"HIMAX_GAPTEST_RAW_Y",*/

	"HIMAX_ACT_IDLE_NOISE",
	"HIMAX_ACT_IDLE_RAWDATA",
	"HIMAX_ACT_IDLE_BPN_RAWDATA",

	"HIMAX_LPWUG_WEIGHT_NOISE",
	"HIMAX_LPWUG_ABS_NOISE",
	"HIMAX_LPWUG_RAWDATA",
	"HIMAX_LPWUG_BPN_RAWDATA",

	"HIMAX_LPWUG_IDLE_NOISE",
	"HIMAX_LPWUG_IDLE_RAWDATA",
	"HIMAX_LPWUG_IDLE_BPN_RAWDATA",

	"HIMAX_BACK_NORMAL",
	NULL
};

/* for criteria */
char *g_hx_inspt_crtra_name[] = {
	"CRITERIA_RAW_MIN",
	"CRITERIA_RAW_MAX",
	"CRITERIA_RAW_BPN_MIN",
	"CRITERIA_RAW_BPN_MAX",
	"CRITERIA_SC_MIN",
	"CRITERIA_SC_MAX",
	"CRITERIA_SC_GOLDEN",
	"CRITERIA_SHORT_MIN",
	"CRITERIA_SHORT_MAX",
	"CRITERIA_OPEN_MIN",
	"CRITERIA_OPEN_MAX",
	"CRITERIA_MICRO_OPEN_MIN",
	"CRITERIA_MICRO_OPEN_MAX",
	"CRITERIA_NOISE_WT_MIN",
	"CRITERIA_NOISE_WT_MAX",
	"CRITERIA_NOISE_ABS_MIN",
	"CRITERIA_NOISE_ABS_MAX",
	"CRITERIA_SORT_MIN",
	"CRITERIA_SORT_MAX",

	"CRITERIA_GAP_RAW_HOR_MIN",
	"CRITERIA_GAP_RAW_HOR_MAX",
	"CRITERIA_GAP_RAW_VER_MIN",
	"CRITERIA_GAP_RAW_VER_MAX",

	"ACT_IDLE_NOISE_MIN",
	"ACT_IDLE_NOISE_MAX",
	"ACT_IDLE_RAWDATA_MIN",
	"ACT_IDLE_RAWDATA_MAX",
	"ACT_IDLE_RAW_BPN_MIN",
	"ACT_IDLE_RAW_BPN_MAX",

	"LPWUG_NOISE_WT_MIN",
	"LPWUG_NOISE_WT_MAX",
	"LPWUG_NOISE_ABS_MIN",
	"LPWUG_NOISE_ABS_MAX",
	"LPWUG_RAWDATA_MIN",
	"LPWUG_RAWDATA_MAX",
	"LPWUG_RAW_BPN_MIN",
	"LPWUG_RAW_BPN_MAX",

	"LPWUG_IDLE_NOISE_MIN",
	"LPWUG_IDLE_NOISE_MAX",
	"LPWUG_IDLE_RAWDATA_MIN",
	"LPWUG_IDLE_RAWDATA_MAX",
	"LPWUG_IDLE_RAW_BPN_MIN",
	"LPWUG_IDLE_RAW_BPN_MAX",
	NULL
};


void (*fp_himax_self_test_init)(void) = himax_inspection_init;


static void himax_press_powerkey(void)
{
	I(" %s POWER KEY event %x press\n", __func__, KEY_POWER);
	input_report_key(private_ts->input_dev, KEY_POWER, 1);
	input_sync(private_ts->input_dev);

	msleep(100);

	I(" %s POWER KEY event %x release\n", __func__, KEY_POWER);
	input_report_key(private_ts->input_dev, KEY_POWER, 0);
	input_sync(private_ts->input_dev);
}


static uint8_t NOISEMAX;
static uint8_t g_recal_thx;

static int arraydata_max1, arraydata_max2, arraydata_max3;
static int arraydata_min1, arraydata_min2, arraydata_min3;

void himax_get_arraydata_edge(uint32_t *RAW)
{
	int temp, i, j;
	int len = ic_data->HX_RX_NUM * ic_data->HX_TX_NUM;
	uint32_t ArrayData[len];

	for (i = 0; i < len; i++)
		ArrayData[i] = RAW[i];
	for (j = len-1; j > 0; j--) { /*min to max*/
		for (i = 0; i < j; i++) {
			if (ArrayData[i] > ArrayData[i+1]) {
				temp = ArrayData[i];
				ArrayData[i] = ArrayData[i+1];
				ArrayData[i+1] = temp;
			}
		}
	}

	arraydata_min1 = ArrayData[0];
	arraydata_min2 = ArrayData[1];
	arraydata_min3 = ArrayData[2];
	arraydata_max1 = ArrayData[len-3];
	arraydata_max2 = ArrayData[len-2];
	arraydata_max3 = ArrayData[len-1];

}

static int hx_test_data_get(uint32_t RAW[], char *start_log, char *result,
			 int now_item)
{
	uint32_t i;

	ssize_t len = 0;
	char *testdata = NULL;
	uint32_t SZ_SIZE = g_1kind_raw_size;

	I("%s: Entering, Now type=%s!\n", __func__,
		 g_himax_inspection_mode[now_item]);

	testdata = kzalloc(sizeof(char) * SZ_SIZE, GFP_KERNEL);
	if (!testdata)
		return MEM_ALLOC_FAIL;


	len += snprintf((testdata + len), SZ_SIZE - len, "%s", start_log);
	for (i = 0; i < ic_data->HX_TX_NUM*ic_data->HX_RX_NUM; i++) {
		if (i > 1 && ((i + 1) % ic_data->HX_RX_NUM) == 0) {
			if (now_item == HX_WT_NOISE) {
				len += snprintf((testdata + len), SZ_SIZE - len,
					 "%5d,\n", (int16_t)RAW[i]);
			} else {
				len += snprintf((testdata + len), SZ_SIZE - len,
					 "%5d,\n", RAW[i]);
			}

		} else {
			if (now_item == HX_WT_NOISE) {
				len += snprintf((testdata + len), SZ_SIZE - len,
					 "%5d,", (int16_t)RAW[i]);
			} else {
				len += snprintf((testdata + len), SZ_SIZE - len,
					 "%5d,", RAW[i]);
			}
		}
	}
	len += snprintf((testdata + len), SZ_SIZE - len, "\n%s", result);

	memcpy(&g_rslt_data[0], testdata, len);
	g_rslt_data_len = len;
	I("%s: g_rslt_data_len=%d!\n", __func__, g_rslt_data_len);

	/* dbg */
	/* for(i = 0; i < SZ_SIZE; i++)
	 * {
	 *	I("0x%04X, ", g_rslt_data[i + (now_item * SZ_SIZE)]);
	 *	if(i > 0 && (i % 16 == 15))
	 *		PCT("\n");
	 * }
	 */

	kfree(testdata);
	I("%s: End!\n", __func__);
	return NO_ERR;
}

static int himax_switch_mode_inspection(int mode)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4] = {0};

	I("%s: Entering\n", __func__);

	/*Stop Handshaking*/
	himax_parse_assign_cmd(sram_adr_rawdata_addr, tmp_addr,
			sizeof(tmp_addr));
	g_core_fp.fp_register_write(tmp_addr, 4, tmp_data, 0);


	switch (mode) {
	case HX_SORTING:
		tmp_data[3] = 0x00; tmp_data[2] = 0x00;
		tmp_data[1] = PWD_SORTING_START;
		tmp_data[0] = PWD_SORTING_START;
		break;
	case HX_OPEN:
		tmp_data[3] = 0x00; tmp_data[2] = 0x00;
		tmp_data[1] = PWD_OPEN_START;
		tmp_data[0] = PWD_OPEN_START;
		break;
	case HX_MICRO_OPEN:
		tmp_data[3] = 0x00; tmp_data[2] = 0x00;
		tmp_data[1] = PWD_OPEN_START;
		tmp_data[0] = PWD_OPEN_START;
		break;
	case HX_SHORT:
		tmp_data[3] = 0x00; tmp_data[2] = 0x00;
		tmp_data[1] = PWD_SHORT_START;
		tmp_data[0] = PWD_SHORT_START;
		break;

	case HX_GAPTEST_RAW:
	case HX_RAWDATA:
	case HX_BPN_RAWDATA:
	case HX_SC:
		tmp_data[3] = 0x00; tmp_data[2] = 0x00;
		tmp_data[1] = PWD_RAWDATA_START;
		tmp_data[0] = PWD_RAWDATA_START;
		break;

	case HX_WT_NOISE:
	case HX_ABS_NOISE:
		tmp_data[3] = 0x00; tmp_data[2] = 0x00;
		tmp_data[1] = PWD_NOISE_START;
		tmp_data[0] = PWD_NOISE_START;
		break;

	case HX_ACT_IDLE_RAWDATA:
	case HX_ACT_IDLE_BPN_RAWDATA:
	case HX_ACT_IDLE_NOISE:
		tmp_data[3] = 0x00; tmp_data[2] = 0x00;
		tmp_data[1] = PWD_ACT_IDLE_START;
		tmp_data[0] = PWD_ACT_IDLE_START;
		break;

	case HX_LP_RAWDATA:
	case HX_LP_BPN_RAWDATA:
	case HX_LP_ABS_NOISE:
	case HX_LP_WT_NOISE:
		tmp_data[3] = 0x00; tmp_data[2] = 0x00;
		tmp_data[1] = PWD_LP_START;
		tmp_data[0] = PWD_LP_START;
		break;
	case HX_LP_IDLE_RAWDATA:
	case HX_LP_IDLE_BPN_RAWDATA:
	case HX_LP_IDLE_NOISE:
		tmp_data[3] = 0x00; tmp_data[2] = 0x00;
		tmp_data[1] = PWD_LP_IDLE_START;
		tmp_data[0] = PWD_LP_IDLE_START;
		break;

	default:
		I("%s,Nothing to be done!\n", __func__);
		break;
	}

	if (g_core_fp.fp_assign_sorting_mode != NULL)
		g_core_fp.fp_assign_sorting_mode(tmp_data);
	I("%s: End of setting!\n", __func__);

	return 0;

}

static uint32_t himax_get_rawdata(uint32_t RAW[], uint32_t datalen)
{
	uint8_t *tmp_rawdata;
	bool get_raw_rlst;
	uint8_t retry = 0;
	uint32_t i = 0;
	uint32_t j = 0;
	uint32_t index = 0;
	uint32_t Min_DATA = 0xFFFFFFFF;
	uint32_t Max_DATA = 0x00000000;

	/* We use two bytes to combine a value of rawdata.*/
	tmp_rawdata = kzalloc(sizeof(uint8_t) * (datalen * 2), GFP_KERNEL);
	if (!tmp_rawdata)
		return HX_INSP_MEMALLCTFAIL;


	while (retry < 50) {
		get_raw_rlst = g_core_fp.fp_get_DSRAM_data(tmp_rawdata, false);
		if (get_raw_rlst)
			break;
		retry++;
	}

	if (retry >= 50)
		goto DIRECT_END;

	/* Copy Data*/
	for (i = 0; i < ic_data->HX_TX_NUM*ic_data->HX_RX_NUM; i++)
		RAW[i] = tmp_rawdata[(i * 2) + 1] * 256 + tmp_rawdata[(i * 2)];
	if (private_ts->debug_log_level & BIT(4)) {
		for (j = 0; j < ic_data->HX_RX_NUM; j++) {
			if (j == 0)
				PCT("      RX%2d", j + 1);
			else
				PCT("  RX%2d", j + 1);
		}
		PCT("\n");
	}
	for (i = 0; i < ic_data->HX_TX_NUM; i++) {
		if (private_ts->debug_log_level & BIT(4))
			PCT("TX%2d", i + 1);
		for (j = 0; j < ic_data->HX_RX_NUM; j++) {
			if (private_ts->debug_log_level & BIT(4))
				PCT("%5d ", RAW[index]);
			if (RAW[index] > Max_DATA)
				Max_DATA = RAW[index];

			if (RAW[index] < Min_DATA)
				Min_DATA = RAW[index];

			index++;
		}
		if (private_ts->debug_log_level & BIT(4))
			PCT("\n");
	}
	I("Max = %5d, Min = %5d\n", Max_DATA, Min_DATA);
DIRECT_END:
	kfree(tmp_rawdata);

	if (get_raw_rlst)
		return HX_INSP_OK;
	else
		return HX_INSP_EGETRAW;

}

static void himax_switch_data_type(uint8_t checktype)
{
	uint8_t datatype = 0x00;

	if (private_ts->debug_log_level & BIT(4)) {
		I("%s,Expected type[%d]=%s"
			, __func__
			, checktype, g_himax_inspection_mode[checktype]);
	}
	switch (checktype) {
	case HX_SORTING:
		datatype = DATA_SORTING;
		break;
	case HX_OPEN:
		datatype = DATA_OPEN;
		break;
	case HX_MICRO_OPEN:
		datatype = DATA_MICRO_OPEN;
		break;
	case HX_SHORT:
		datatype = DATA_SHORT;
		break;
	case HX_RAWDATA:
	case HX_BPN_RAWDATA:
	case HX_SC:
	case HX_GAPTEST_RAW:
		datatype = DATA_RAWDATA;
		break;

	case HX_WT_NOISE:
	case HX_ABS_NOISE:
		datatype = DATA_NOISE;
		break;
	case HX_BACK_NORMAL:
		datatype = DATA_BACK_NORMAL;
		break;
	case HX_ACT_IDLE_RAWDATA:
	case HX_ACT_IDLE_BPN_RAWDATA:
		datatype = DATA_ACT_IDLE_RAWDATA;
		break;
	case HX_ACT_IDLE_NOISE:
		datatype = DATA_ACT_IDLE_NOISE;
		break;

	case HX_LP_RAWDATA:
	case HX_LP_BPN_RAWDATA:
		datatype = DATA_LP_RAWDATA;
		break;
	case HX_LP_WT_NOISE:
	case HX_LP_ABS_NOISE:
		datatype = DATA_LP_NOISE;
		break;
	case HX_LP_IDLE_RAWDATA:
	case HX_LP_IDLE_BPN_RAWDATA:
		datatype = DATA_LP_IDLE_RAWDATA;
		break;
	case HX_LP_IDLE_NOISE:
		datatype = DATA_LP_IDLE_NOISE;
		break;

	default:
		E("Wrong type=%d\n", checktype);
		break;
	}
	g_core_fp.fp_diag_register_set(datatype, 0x00, false);
}

static void himax_bank_search_set(uint16_t Nframe, uint8_t checktype)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	/*skip frame 0x100070F4*/
	himax_parse_assign_cmd(addr_skip_frame, tmp_addr, sizeof(tmp_addr));
	g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, false);

	switch (checktype) {
	case HX_ACT_IDLE_RAWDATA:
	case HX_ACT_IDLE_BPN_RAWDATA:
	case HX_ACT_IDLE_NOISE:
		tmp_data[0] = BS_ACT_IDLE;
		break;
	case HX_LP_RAWDATA:
	case HX_LP_BPN_RAWDATA:
	case HX_LP_ABS_NOISE:
	case HX_LP_WT_NOISE:
		tmp_data[0] = BS_LPWUG;
		break;
	case HX_LP_IDLE_RAWDATA:
	case HX_LP_IDLE_BPN_RAWDATA:
	case HX_LP_IDLE_NOISE:
		tmp_data[0] = BS_LP_dile;
		break;
	case HX_RAWDATA:
	case HX_BPN_RAWDATA:
	case HX_SC:
		tmp_data[0] = BS_RAWDATA;
		break;
	case HX_WT_NOISE:
	case HX_ABS_NOISE:
		tmp_data[0] = BS_NOISE;
		break;
	default:
		tmp_data[0] = BS_OPENSHORT;
		break;
	}
	if (private_ts->debug_log_level & BIT(4)) {
		I("%s,Now BankSearch Value=%d\n",
			__func__, tmp_data[0]);
	}
	g_core_fp.fp_register_write(tmp_addr, 4, tmp_data, 0);
}

static void himax_neg_noise_sup(uint8_t *data)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	/*0x10007FD8 Check support negative value or not */
	himax_parse_assign_cmd(addr_neg_noise_sup, tmp_addr,
		sizeof(tmp_addr));
	g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, false);

	if ((tmp_data[3] & 0x04) == 0x04) {
		himax_parse_assign_cmd(data_neg_noise, tmp_data,
			sizeof(tmp_data));
		data[2] = tmp_data[2]; data[3] = tmp_data[3];
	} else
		I("%s Not support negative noise\n", __func__);
}

static void himax_set_N_frame(uint16_t Nframe, uint8_t checktype)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	himax_bank_search_set(Nframe, checktype);

	/*IIR MAX - 0x10007294*/
	himax_parse_assign_cmd(fw_addr_set_frame_addr,
			tmp_addr, sizeof(tmp_addr));
	tmp_data[3] = 0x00; tmp_data[2] = 0x00;
	tmp_data[1] = (uint8_t)((Nframe & 0xFF00) >> 8);
	tmp_data[0] = (uint8_t)(Nframe & 0x00FF);
	g_core_fp.fp_register_write(tmp_addr, 4, tmp_data, 0);

	if (checktype == HX_WT_NOISE ||
		checktype == HX_ABS_NOISE ||
		checktype == HX_LP_WT_NOISE ||
		checktype == HX_LP_ABS_NOISE)
		himax_neg_noise_sup(tmp_data);
	if (private_ts->debug_log_level & BIT(4)) {
		I("%s,Now N frame Value=0x%02X%02X\n",
			__func__, tmp_data[1], tmp_data[0]);
	}
	g_core_fp.fp_register_write(tmp_addr, 4, tmp_data, 0);
}

static void himax_get_noise_base(uint8_t checktype)/*Normal Threshold*/
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	switch (checktype) {
	case HX_WT_NOISE:
		himax_parse_assign_cmd(addr_normal_noise_thx,
				tmp_addr, sizeof(tmp_addr));
		break;
	case HX_LP_WT_NOISE:
		himax_parse_assign_cmd(addr_lpwug_noise_thx,
				tmp_addr, sizeof(tmp_addr));
		break;
	default:
		I("%s Not support type\n", __func__);
	}

	/*normal : 0x1000708F, LPWUG:0x10007093*/
	g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, false);
	NOISEMAX = tmp_data[3];

	himax_parse_assign_cmd(addr_recal_thx,
			tmp_addr, sizeof(tmp_addr));
	g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, false);
	g_recal_thx = tmp_data[2];/*0x10007092*/
	I("%s: NOISEMAX=%d, g_recal_thx = %d\n", __func__,
		NOISEMAX, g_recal_thx);
}

static uint16_t himax_get_palm_num(void)/*Palm Number*/
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	uint16_t palm_num;

	himax_parse_assign_cmd(addr_palm_num,
			tmp_addr, sizeof(tmp_addr));
	g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, false);
	palm_num = tmp_data[3];/*0x100070AB*/
	I("%s: palm_num = %d ", __func__, palm_num);

	return palm_num;
}

static int himax_get_noise_weight_test(uint8_t checktype)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	uint16_t weight = 0;
	uint16_t value = 0;

	himax_parse_assign_cmd(addr_weight_sup,
			tmp_addr, sizeof(tmp_addr));

	/*0x100072C8 weighting value*/
	g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, false);
	if (tmp_data[3] != tmp_addr[1] || tmp_data[2] != tmp_addr[0])
		return FW_NOT_READY;

	value = (tmp_data[1] << 8) | tmp_data[0];
	I("%s: value = %d, %d, %d ", __func__, value, tmp_data[2], tmp_data[3]);

	switch (checktype) {
	case HX_WT_NOISE:
		himax_parse_assign_cmd(addr_normal_weight_a,
				tmp_addr, sizeof(tmp_addr));
		break;
	case HX_LP_WT_NOISE:
		himax_parse_assign_cmd(addr_lpwug_weight_a,
				tmp_addr, sizeof(tmp_addr));
		break;
	default:
		I("%s Not support type\n", __func__);
	}

	/*Normal:0x1000709C, LPWUG:0x100070A0 weighting threshold*/
	g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, false);
	weight = tmp_data[0];

	himax_parse_assign_cmd(addr_weight_b, tmp_addr, sizeof(tmp_addr));

	g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, false);
	weight = tmp_data[1] * weight;/*0x10007095 weighting threshold*/
	I("%s: weight = %d ", __func__, weight);

	if (value > weight)
		return ERR_TEST_FAIL;
	else
		return 0;
}

static uint32_t himax_check_mode(uint8_t checktype)
{
	uint8_t tmp_data[4] = {0};
	uint8_t wait_pwd[2] = {0};

	switch (checktype) {
	case HX_SORTING:
		wait_pwd[0] = PWD_SORTING_END;
		wait_pwd[1] = PWD_SORTING_END;
		break;
	case HX_OPEN:
		wait_pwd[0] = PWD_OPEN_END;
		wait_pwd[1] = PWD_OPEN_END;
		break;
	case HX_MICRO_OPEN:
		wait_pwd[0] = PWD_OPEN_END;
		wait_pwd[1] = PWD_OPEN_END;
		break;
	case HX_SHORT:
		wait_pwd[0] = PWD_SHORT_END;
		wait_pwd[1] = PWD_SHORT_END;
		break;
	case HX_RAWDATA:
	case HX_BPN_RAWDATA:
	case HX_SC:
	case HX_GAPTEST_RAW:
		wait_pwd[0] = PWD_RAWDATA_END;
		wait_pwd[1] = PWD_RAWDATA_END;
		break;

	case HX_WT_NOISE:
	case HX_ABS_NOISE:
		wait_pwd[0] = PWD_NOISE_END;
		wait_pwd[1] = PWD_NOISE_END;
		break;

	case HX_ACT_IDLE_RAWDATA:
	case HX_ACT_IDLE_BPN_RAWDATA:
	case HX_ACT_IDLE_NOISE:
		wait_pwd[0] = PWD_ACT_IDLE_END;
		wait_pwd[1] = PWD_ACT_IDLE_END;
		break;

	case HX_LP_RAWDATA:
	case HX_LP_BPN_RAWDATA:
	case HX_LP_ABS_NOISE:
	case HX_LP_WT_NOISE:
		wait_pwd[0] = PWD_LP_END;
		wait_pwd[1] = PWD_LP_END;
		break;
	case HX_LP_IDLE_RAWDATA:
	case HX_LP_IDLE_BPN_RAWDATA:
	case HX_LP_IDLE_NOISE:
		wait_pwd[0] = PWD_LP_IDLE_END;
		wait_pwd[1] = PWD_LP_IDLE_END;
		break;

	default:
		E("Wrong type=%d\n", checktype);
		break;
	}

	if (g_core_fp.fp_check_sorting_mode != NULL)
		g_core_fp.fp_check_sorting_mode(tmp_data);

	if ((wait_pwd[0] == tmp_data[0]) && (wait_pwd[1] == tmp_data[1])) {
		I("%s,It had been changed to [%d]=%s\n",
			__func__,
			checktype, g_himax_inspection_mode[checktype]);
		return 0;
	} else {
		return 1;
	}
}

#define TEMP_LOG \
"%s:%s,tmp_data[0]=%x,tmp_data[1]=%x,tmp_data[2]=%x,tmp_data[3]=%x\n"

static uint32_t himax_wait_sorting_mode(uint8_t checktype)
{
	uint8_t tmp_addr[4] = {0};
	uint8_t tmp_data[4] = {0};
	uint8_t wait_pwd[2] = {0};
	int count = 0;

	if (private_ts->debug_log_level & BIT(4))
		I("%s:start!\n", __func__);

	switch (checktype) {
	case HX_SORTING:
		wait_pwd[0] = PWD_SORTING_END;
		wait_pwd[1] = PWD_SORTING_END;
		break;
	case HX_OPEN:
		wait_pwd[0] = PWD_OPEN_END;
		wait_pwd[1] = PWD_OPEN_END;
		break;
	case HX_MICRO_OPEN:
		wait_pwd[0] = PWD_OPEN_END;
		wait_pwd[1] = PWD_OPEN_END;
		break;
	case HX_SHORT:
		wait_pwd[0] = PWD_SHORT_END;
		wait_pwd[1] = PWD_SHORT_END;
		break;
	case HX_RAWDATA:
	case HX_BPN_RAWDATA:
	case HX_SC:
	case HX_GAPTEST_RAW:
		wait_pwd[0] = PWD_RAWDATA_END;
		wait_pwd[1] = PWD_RAWDATA_END;
		break;
	case HX_WT_NOISE:
	case HX_ABS_NOISE:
		wait_pwd[0] = PWD_NOISE_END;
		wait_pwd[1] = PWD_NOISE_END;
		break;
	case HX_ACT_IDLE_RAWDATA:
	case HX_ACT_IDLE_BPN_RAWDATA:
	case HX_ACT_IDLE_NOISE:
		wait_pwd[0] = PWD_ACT_IDLE_END;
		wait_pwd[1] = PWD_ACT_IDLE_END;
		break;

	case HX_LP_RAWDATA:
	case HX_LP_BPN_RAWDATA:
	case HX_LP_ABS_NOISE:
	case HX_LP_WT_NOISE:
		wait_pwd[0] = PWD_LP_END;
		wait_pwd[1] = PWD_LP_END;
		break;
	case HX_LP_IDLE_RAWDATA:
	case HX_LP_IDLE_BPN_RAWDATA:
	case HX_LP_IDLE_NOISE:
		wait_pwd[0] = PWD_LP_IDLE_END;
		wait_pwd[1] = PWD_LP_IDLE_END;
		break;

	default:
		I("No Change Mode and now type=%d\n", checktype);
		break;
	}
	I("%s:NowType[%d] = %s, Expected=0x%02X%02X\n",
		__func__, checktype, g_himax_inspection_mode[checktype],
		 wait_pwd[1], wait_pwd[0]);
	do {
		if (private_ts->debug_log_level & BIT(4))
			I("%s:start check_sorting_mode!\n", __func__);
		if (g_core_fp.fp_check_sorting_mode != NULL)
			g_core_fp.fp_check_sorting_mode(tmp_data);
		if (private_ts->debug_log_level & BIT(4))
			I("%s:end check_sorting_mode!\n", __func__);
		if ((wait_pwd[0] == tmp_data[0]) &&
			(wait_pwd[1] == tmp_data[1]))
			return HX_INSP_OK;
		if (private_ts->debug_log_level & BIT(4)) {
			himax_parse_assign_cmd(fw_addr_chk_fw_status,
					tmp_addr, sizeof(tmp_addr));
			g_core_fp.fp_register_read(tmp_addr, 4,
				tmp_data, false);
			I(TEMP_LOG, __func__, "0x900000A8",
				tmp_data[0], tmp_data[1],
				tmp_data[2], tmp_data[3]);

			himax_parse_assign_cmd(fw_addr_flag_reset_event,
					tmp_addr, sizeof(tmp_addr));
			g_core_fp.fp_register_read(tmp_addr, 4,
				tmp_data, false);
			I(TEMP_LOG, __func__, "0x900000E4",
				tmp_data[0], tmp_data[1],
				tmp_data[2], tmp_data[3]);

			himax_parse_assign_cmd(fw_addr_fw_dbg_msg_addr,
					tmp_addr, sizeof(tmp_addr));
			g_core_fp.fp_register_read(tmp_addr, 4,
				tmp_data, false);
			I(TEMP_LOG, __func__, "0x10007F40",
				tmp_data[0], tmp_data[1],
				tmp_data[2], tmp_data[3]);
		}
		I("Now retry %d times!\n", count++);
		msleep(50);
	} while (count < 50);

	if (private_ts->debug_log_level & BIT(4))
		I("%s:end\n", __func__);
	return HX_INSP_ESWITCHMODE;
}

static int hx_turn_on_mp_func(int on)
{
	int rslt = 0;
	int retry = 3;
	uint8_t tmp_addr[4] = {0};
	uint8_t tmp_data[4] = {0};
	uint8_t tmp_read[4] = {0};
	/* char *tmp_chipname = private_ts->chip_name; */

	himax_parse_assign_cmd(addr_ctrl_mpap_ovl, tmp_addr,
		sizeof(tmp_addr));
	if (on) {
		I("%s : Turn on!\n", __func__);
		if (strcmp(HX_83102D_SERIES_PWON, private_ts->chip_name) == 0) {
			I("%s: need to enter Mp mode!\n", __func__);
			himax_parse_assign_cmd(PWD_TURN_ON_MPAP_OVL,
					tmp_data, sizeof(tmp_data));
			do {
				g_core_fp.fp_register_write(tmp_addr, 4,
					tmp_data, 0);
				usleep_range(10000, 10001);
				g_core_fp.fp_register_read(tmp_addr, 4,
					tmp_read, false);

				I("%s:read2=0x%02X,read1=0x%02X,read0=0x%02X\n",
					__func__,
					tmp_read[2],
					tmp_read[1],
					tmp_read[0]);

				retry--;
			} while (((retry > 0)
				&& (tmp_read[2] != tmp_data[2]
				&& tmp_read[1] != tmp_data[1]
				&& tmp_read[0] != tmp_data[0])));
		} else {
			I("%s:Nothing to be done!\n", __func__);
		}
	} else {
		I("%s : Turn off!\n", __func__);
		if (strcmp(HX_83102D_SERIES_PWON, private_ts->chip_name) == 0) {
			I("%s: need to enter Mp mode!\n", __func__);

			himax_parse_assign_cmd(ic_cmd_rst, tmp_data,
				sizeof(tmp_data));
			do {
				g_core_fp.fp_register_write(tmp_addr, 4,
					tmp_data, 0);
				usleep_range(10000, 10001);
				g_core_fp.fp_register_read(tmp_addr, 4,
					tmp_read, false);

				I("%s:read2=0x%02X,read1=0x%02X,read0=0x%02X\n",
					__func__,
					tmp_read[2],
					tmp_read[1],
					tmp_read[0]);

				retry--;
			} while ((retry > 0)
				&& (tmp_read[2] != tmp_data[2]
				&& tmp_read[1] != tmp_data[1]
				&& tmp_read[0] != tmp_data[0]));
		} else {
			I("%s Nothing to be done!\n", __func__);
		}
	}
	return rslt;
}

/* HX_GAP START gap test function */
/* extern int himax_write_to_ic_flash_flow(uint32_t start_addr,*/
/*		uint32_t *write_data, uint32_t write_len);*/

static int himax_gap_test_vertical_setting(void)
{
	g_gap_vertical_part[0] = 0;
	g_gap_vertical_part[1] = 4;
	g_gap_vertical_part[2] = 8;

	return NO_ERR;
}

static void himax_cal_gap_data_vertical(int start, int end_idx, int direct,
		uint32_t *org_raw, uint32_t *result_raw)
{
	int i = 0;
	int rx_num = ic_data->HX_RX_NUM;

	I("%s:start=%d,end_idx=%d\n", __func__, start, end_idx);

	for (i = start; i < (start + rx_num*end_idx); i++) {
		if (direct == 0) { /* up - down */
			if (i < start+rx_num)
				result_raw[i] = 0;
			else
				result_raw[i] = org_raw[i-rx_num] - org_raw[i];

		} else { /* down - up */
			if (i > (start + rx_num*(end_idx-1)-1))
				result_raw[i] = 0;
			else
				result_raw[i] = org_raw[i+rx_num] - org_raw[i];

		}
	}
}

static int himax_gap_test_vertical_raw(int test_type, uint32_t *org_raw)
{
	int i_partial = 0;
	int tmp_start = 0;
	int tmp_end_idx = 0;
	uint32_t *result_raw = NULL;
	int i = 0;
	int ret_val = NO_ERR;

	int tx_num = ic_data->HX_TX_NUM;
	int rx_num = ic_data->HX_RX_NUM;

	g_gap_vertical_part = kcalloc(g_gap_vertical_partial,
		sizeof(int), GFP_KERNEL);
	if (g_gap_vertical_part == NULL)
		return MEM_ALLOC_FAIL;

	result_raw = kcalloc(tx_num*rx_num, sizeof(uint32_t), GFP_KERNEL);
	if (result_raw == NULL) {
		ret_val = MEM_ALLOC_FAIL;
		goto alloc_result_raw_failed;
	}

	himax_gap_test_vertical_setting();

	I("Print vertical ORG RAW\n");
	for (i = 0; i < tx_num*rx_num; i++) {
		I("%04d,", org_raw[i]);
		if (i > 0 && i%rx_num == (rx_num-1))
			I("\n");
	}

	for (i_partial = 0; i_partial < g_gap_vertical_partial; i_partial++) {

		tmp_start = g_gap_vertical_part[i_partial]*rx_num;
		if (i_partial+1 == g_gap_vertical_partial)
			tmp_end_idx = tx_num - g_gap_vertical_part[i_partial];
		else
			tmp_end_idx = g_gap_vertical_part[i_partial+1] -
				 g_gap_vertical_part[i_partial];

		if (i_partial % 2 == 0)
			himax_cal_gap_data_vertical(tmp_start, tmp_end_idx, 0,
						org_raw, result_raw);
		else
			himax_cal_gap_data_vertical(tmp_start, tmp_end_idx, 1,
						org_raw, result_raw);

	}

	I("Print Vertical New RAW\n");
	for (i = 0; i < tx_num*rx_num; i++) {
		I("%04d,", result_raw[i]);
		if (i > 0 && i%rx_num == (rx_num-1))
			I("\n");
	}

	for (i = 0; i < tx_num*rx_num; i++) {
		if (result_raw[i] < g_inspection_criteria[IDX_GAP_VER_RAWMIN][i]
		 &&
		 result_raw[i] > g_inspection_criteria[IDX_GAP_VER_RAWMAX][i]) {
			ret_val = NO_ERR - i;
			break;
		}
	}

	/* himax_write_to_ic_flash_flow(0x1A000,result_raw,tx_num*rx_num); */
	kfree(result_raw);
alloc_result_raw_failed:
	kfree(g_gap_vertical_part);
	g_gap_vertical_part = NULL;

	return ret_val;
}

static int himax_gap_test_horizontal_setting(void)
{
	g_gap_horizontal_part[0] = 0;
	g_gap_horizontal_part[1] = 8;
	g_gap_horizontal_part[2] = 24;

	return NO_ERR;
}

static void himax_cal_gap_data_horizontal(int start, int end_idx, int direct,
		uint32_t *org_raw, uint32_t *result_raw)
{
	int i = 0;
	int j = 0;
	int rx_num = ic_data->HX_RX_NUM;
	int tx_num = ic_data->HX_TX_NUM;

	I("start=%d,end_idx=%d\n", start, end_idx);

	for (j = 0; j < tx_num; j++) {
		for (i = (start + (j*rx_num));
			i < (start + (j*rx_num) + end_idx); i++) {
			/* left - right */
			if (direct == 0) {
				if (i == (start + (j*rx_num)))
					result_raw[i] = 0;
				else
					result_raw[i] =
						org_raw[i-1] - org_raw[i];

			} else { /* right - left */
				if (i == ((start + (j*rx_num) + end_idx) - 1))
					result_raw[i] = 0;
				else
					result_raw[i] =
						org_raw[i + 1] - org_raw[i];
			}
		}
	}
}

static int himax_gap_test_honrizontal_raw(int test_type, uint32_t *raw)
{
	int rx_num = ic_data->HX_RX_NUM;
	int tx_num = ic_data->HX_TX_NUM;
	int tmp_start = 0;
	int tmp_end_idx = 0;
	int i_partial = 0;
	uint32_t *result_raw;
	int i = 0;
	int ret_val = NO_ERR;


	g_gap_horizontal_part = kcalloc(g_gap_horizontal_partial,
		sizeof(int), GFP_KERNEL);
	if (!g_gap_horizontal_part)
		return MEM_ALLOC_FAIL;


	result_raw = kcalloc(tx_num*rx_num, sizeof(uint32_t), GFP_KERNEL);
	if (!result_raw) {

		ret_val = MEM_ALLOC_FAIL;
		goto alloc_result_raw_failed;
	}

	himax_gap_test_horizontal_setting();

	I("Print Horizontal ORG RAW\n");
	for (i = 0; i < tx_num*rx_num; i++) {
		I("%04d,", raw[i]);
		if (i > 0 && i%rx_num == (rx_num-1))
			I("\n");
	}

	for (i_partial = 0;
	i_partial < g_gap_horizontal_partial;
	i_partial++) {
		tmp_start	= g_gap_horizontal_part[i_partial];
		if (i_partial+1 == g_gap_horizontal_partial)
			tmp_end_idx = rx_num - g_gap_horizontal_part[i_partial];
		else
			tmp_end_idx = g_gap_horizontal_part[i_partial+1] -
				g_gap_horizontal_part[i_partial];

		if (i_partial % 2 == 0)
			himax_cal_gap_data_horizontal(tmp_start, tmp_end_idx,
						0, raw, result_raw);
		else
			himax_cal_gap_data_horizontal(tmp_start, tmp_end_idx,
						1, raw, result_raw);

	}
	I("Print Horizontal New RAW\n");
	for (i = 0; i < tx_num*rx_num; i++) {
		I("%04d,", result_raw[i]);
		if (i > 0 && i%rx_num == (rx_num-1))
			I("\n");
	}

	for (i = 0; i < tx_num*rx_num; i++) {
		if (result_raw[i] < g_inspection_criteria[IDX_GAP_HOR_RAWMIN][i]
		&&
		result_raw[i] > g_inspection_criteria[IDX_GAP_HOR_RAWMAX][i]) {
			ret_val = NO_ERR - i;
			break;
		}
	}

	/* himax_write_to_ic_flash_flow(0x1A800,result_raw,tx_num*rx_num); */
	kfree(result_raw);
alloc_result_raw_failed:
	kfree(g_gap_horizontal_part);
	g_gap_horizontal_part = NULL;

	return ret_val;
}

static uint32_t himax_data_campare(uint8_t checktype, uint32_t *RAW,
		int ret_val)
{
	int i = 0;
	int idx_max = 0;
	int idx_min = 0;
	int block_num = ic_data->HX_TX_NUM*ic_data->HX_RX_NUM;
	uint16_t palm_num = 0;
	uint16_t noise_count = 0;

	switch (checktype) {
	case HX_SORTING:
		idx_min = IDX_SORTMIN;
		break;
	case HX_OPEN:
		idx_max = IDX_OPENMAX;
		idx_min = IDX_OPENMIN;
		break;

	case HX_MICRO_OPEN:
		idx_max = IDX_M_OPENMAX;
		idx_min = IDX_M_OPENMIN;
		break;

	case HX_SHORT:
		idx_max = IDX_SHORTMAX;
		idx_min = IDX_SHORTMIN;
		break;

	case HX_RAWDATA:
		idx_max = IDX_RAWMAX;
		idx_min = IDX_RAWMIN;
		break;

	case HX_BPN_RAWDATA:
		idx_max = IDX_BPN_RAWMAX;
		idx_min = IDX_BPN_RAWMIN;
		break;
	case HX_SC:
		idx_max = IDX_SCMAX;
		idx_min = IDX_SCMIN;
		break;
	case HX_WT_NOISE:
		idx_max = IDX_WT_NOISEMAX;
		idx_min = IDX_WT_NOISEMIN;
		break;
	case HX_ABS_NOISE:
		idx_max = IDX_ABS_NOISEMAX;
		idx_min = IDX_ABS_NOISEMIN;
		break;
	case HX_GAPTEST_RAW:
		break;

	case HX_ACT_IDLE_RAWDATA:
		idx_max = IDX_ACT_IDLE_RAWDATA_MAX;
		idx_min = IDX_ACT_IDLE_RAWDATA_MIN;
		break;

	case HX_ACT_IDLE_BPN_RAWDATA:
		idx_max = IDX_ACT_IDLE_RAW_BPN_MAX;
		idx_min = IDX_ACT_IDLE_RAW_BPN_MIN;
		break;

	case HX_ACT_IDLE_NOISE:
		idx_max = IDX_ACT_IDLE_NOISE_MAX;
		idx_min = IDX_ACT_IDLE_NOISE_MIN;
		break;

	case HX_LP_RAWDATA:
		idx_max = IDX_LP_RAWDATA_MAX;
		idx_min = IDX_LP_RAWDATA_MIN;
		break;

	case HX_LP_BPN_RAWDATA:
		idx_max = IDX_LP_RAW_BPN_MAX;
		idx_min = IDX_LP_RAW_BPN_MIN;
		break;

	case HX_LP_WT_NOISE:
		idx_max = IDX_LP_WT_NOISEMAX;
		idx_min = IDX_LP_WT_NOISEMIN;
		break;

	case HX_LP_ABS_NOISE:
		idx_max = IDX_LP_NOISE_ABS_MAX;
		idx_min = IDX_LP_NOISE_ABS_MIN;
		break;

	case HX_LP_IDLE_RAWDATA:
		idx_max = IDX_LP_IDLE_RAWDATA_MAX;
		idx_min = IDX_LP_IDLE_RAWDATA_MIN;
		break;

	case HX_LP_IDLE_BPN_RAWDATA:
		idx_max = IDX_LP_IDLE_RAW_BPN_MAX;
		idx_min = IDX_LP_IDLE_RAW_BPN_MIN;
		break;

	case HX_LP_IDLE_NOISE:
		idx_max = IDX_LP_IDLE_NOISE_MAX;
		idx_min = IDX_LP_IDLE_NOISE_MIN;
		break;

	default:
		E("Wrong type=%d\n", checktype);
		break;
	}

	/*data process*/
	switch (checktype) {
	case HX_SORTING:
		for (i = 0; i < block_num; i++)
			g_inspection_criteria[idx_max][i] = 999999;
		break;
	case HX_BPN_RAWDATA:
	case HX_ACT_IDLE_BPN_RAWDATA:
	case HX_LP_BPN_RAWDATA:
	case HX_LP_IDLE_BPN_RAWDATA:
		for (i = 0; i < block_num; i++)
			RAW[i] = (int)RAW[i] * 100 / g_dc_max;
		break;
	case HX_SC:
		for (i = 0; i < block_num; i++) {
			RAW[i] = ((int)RAW[i]
				- g_inspection_criteria[IDX_SC_GOLDEN][i])
				* 100 / g_inspection_criteria[IDX_SC_GOLDEN][i];
		}
		break;
	}

	/*data campare*/
	switch (checktype) {
	case HX_GAPTEST_RAW:
		if (
		himax_gap_test_vertical_raw(HX_GAPTEST_RAW, RAW) != NO_ERR) {
			E("%s: HX_GAPTEST_RAW FAIL\n", __func__);
			ret_val |= 1 << (checktype + ERR_SFT);
			break;
		}
		if (himax_gap_test_honrizontal_raw(HX_GAPTEST_RAW, RAW)
		!= NO_ERR) {
			E("%s: HX_GAPTEST_RAW FAIL\n", __func__);
			ret_val |= 1 << (checktype + ERR_SFT);
			break;
		}
		break;

	case HX_WT_NOISE:
	case HX_LP_WT_NOISE:
		noise_count = 0;
		himax_get_noise_base(checktype);
		palm_num = himax_get_palm_num();
		for (i = 0; i < (ic_data->HX_TX_NUM * ic_data->HX_RX_NUM);
		i++) {
			if ((int16_t)RAW[i] > NOISEMAX)
				noise_count++;
		}
		I("noise_count=%d\n", noise_count);
		if (noise_count > palm_num) {
			E("%s: noise test FAIL\n", __func__);
			ret_val |= 1 << (checktype + ERR_SFT);
			break;
		}
		snprintf(g_start_log, 256 * sizeof(char), "\n Threshold = %d\n",
				NOISEMAX);
		/*Check weightingt*/
		if (himax_get_noise_weight_test(checktype) < 0) {
			I("%s: %s FAIL %X\n", __func__,
				g_himax_inspection_mode[checktype], ret_val);
			ret_val |= 1 << (checktype + ERR_SFT);
			break;
		}

		/*Check negative side noise*/
		for (i = 0; i < block_num; i++) {
			if ((int16_t)RAW[i]
			> (g_inspection_criteria[idx_max][i]
			* NOISEMAX / 100)
			|| (int16_t)RAW[i]
			< (g_inspection_criteria[idx_min][i]
			* g_recal_thx / 100)) {
				E(FAIL_IN_INDEX, __func__,
					g_himax_inspection_mode[checktype], i);
				ret_val |= 1 << (checktype + ERR_SFT);
				break;
			}
		}
		break;

	case HX_LP_IDLE_RAWDATA:
	case HX_LP_IDLE_BPN_RAWDATA:
	case HX_LP_IDLE_NOISE:
	case HX_ACT_IDLE_RAWDATA:
	case HX_ACT_IDLE_BPN_RAWDATA:
	case HX_ACT_IDLE_NOISE:
		block_num = ic_data->ic_adc_num;
	case HX_SORTING:
	case HX_OPEN:
	case HX_MICRO_OPEN:
	case HX_SHORT:
	case HX_RAWDATA:
	case HX_BPN_RAWDATA:
	case HX_SC:
	case HX_ABS_NOISE:
	case HX_LP_RAWDATA:
	case HX_LP_BPN_RAWDATA:
	case HX_LP_ABS_NOISE:
		for (i = 0; i < block_num; i++) {
			if ((int)RAW[i] > g_inspection_criteria[idx_max][i]
			|| (int)RAW[i] < g_inspection_criteria[idx_min][i]) {
				if (private_ts->debug_log_level & BIT(4)) {
					E(FAIL_IN_INDEX_CRTRA, __func__,
					g_himax_inspection_mode[checktype], i
					, g_inspection_criteria[idx_max][i]
					, g_inspection_criteria[idx_min][i]
					, RAW[i]);
					ret_val |= 1 << (checktype + ERR_SFT);
				} else {
					E(FAIL_IN_INDEX, __func__,
					g_himax_inspection_mode[checktype], i);
					ret_val |= 1 << (checktype + ERR_SFT);
				}
				break;
			}
#ifdef HX_INSPT_DBG
			if ((private_ts->debug_log_level & BIT(4))) {
				I("%s,type=%s, idx[%d]=%d\n",
				__func__,
				g_himax_inspection_mode[checktype],
				i, RAW[i]);
				I("%s, crteria,max=%d,min=%d\n",
				__func__,
				g_inspection_criteria[idx_max][i],
				g_inspection_criteria[idx_min][i]);
			}
#endif
		}
		break;
	default:
		E("Wrong type[%d] = %s\n",
		checktype, g_himax_inspection_mode[checktype]);
		break;
	}

	I("%s: %s %s\n", __func__, g_himax_inspection_mode[checktype],
			(ret_val == HX_INSP_OK)?"PASS":"FAIL");

	return ret_val;
}

static int himax_get_max_dc(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t tmp_addr[DATA_LEN_4];
	int dc_max = 0;

	himax_parse_assign_cmd(addr_max_dc, tmp_addr, sizeof(tmp_addr));

	g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, tmp_data, 0);
	I("%s: tmp_data[0-3] = %02x%02x%02x%02x\n", __func__,
		tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);

	dc_max = tmp_data[3]<<24 | tmp_data[2]<<16 |
			tmp_data[1]<<8 | tmp_data[0];
	I("%s: dc max = %d\n", __func__, dc_max);
	return dc_max;
}

/*	 HX_GAP END*/
static uint32_t mpTestFunc(uint8_t checktype, uint32_t datalen)
{
	uint32_t len = 0;
	uint32_t *RAW = NULL;
	int n_frame = 0;
	uint32_t ret_val = HX_INSP_OK;
#if 0
	uint8_t tmp_addr[4] = {0x00, 0x7F, 0x00, 0x10};
	uint8_t tmp_data[4] = {0};
#endif
	/*uint16_t* pInspectGridData = &gInspectGridData[0];*/
	/*uint16_t* pInspectNoiseData = &gInspectNoiseData[0];*/

	I("Now Check type = %d\n", checktype);

	RAW = kcalloc(datalen, sizeof(uint32_t), GFP_KERNEL);
	if (RAW == NULL) {
		E("%s, Failed to allocate memory\n", __func__);
		return HX_INSP_MEMALLCTFAIL;
	}

	if (himax_check_mode(checktype)) {
		/*himax_check_mode(checktype);*/

		I("Need Change Mode ,target=%s\n",
		g_himax_inspection_mode[checktype]);

		if (private_ts->debug_log_level & BIT(4))
			I("%s:start sense off!\n", __func__);
		g_core_fp.fp_sense_off(true);
		if (private_ts->debug_log_level & BIT(4))
			I("%s:end sense off!\n", __func__);
#if !defined(HX_ZERO_FLASH)
		hx_turn_on_mp_func(1);
		if (g_core_fp.fp_reload_disable != NULL)
			g_core_fp.fp_reload_disable(1);
#endif
#if 0
		/* reload disable with keeping high byte value */
		g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, 0);
		tmp_data[1] = 0xA5;
		tmp_data[0] = 0x5A;
		g_core_fp.fp_register_write(
			tmp_addr,
			DATA_LEN_4,
			tmp_data,
			0);
		/*******************/
#endif
		himax_switch_mode_inspection(checktype);

		switch (checktype) {
		case HX_WT_NOISE:
		case HX_ABS_NOISE:
			n_frame = NOISEFRAME;
			break;
		case HX_ACT_IDLE_RAWDATA:
		case HX_ACT_IDLE_NOISE:
		case HX_ACT_IDLE_BPN_RAWDATA:
			n_frame = NORMAL_IDLE_RAWDATA_NOISEFRAME;
			break;
		case HX_LP_RAWDATA:
		case HX_LP_BPN_RAWDATA:
			n_frame = LP_RAWDATAFRAME;
			break;
		case HX_LP_WT_NOISE:
		case HX_LP_ABS_NOISE:
			n_frame = LP_NOISEFRAME;
			break;
		case HX_LP_IDLE_RAWDATA:
		case HX_LP_IDLE_BPN_RAWDATA:
			n_frame = LP_IDLE_RAWDATAFRAME;
			break;
		case HX_LP_IDLE_NOISE:
			n_frame = LP_IDLE_NOISEFRAME;
			break;
		default:
			n_frame = OTHERSFRAME;
		}
		himax_set_N_frame(n_frame, checktype);
		if (private_ts->debug_log_level & BIT(4))
			I("%s:start sense on!\n", __func__);
		g_core_fp.fp_sense_on(1);
		if (private_ts->debug_log_level & BIT(4))
			I("%s:end sense on!\n", __func__);

	}

	ret_val |= himax_wait_sorting_mode(checktype);
	if (ret_val) {
		E("%s: himax_wait_sorting_mode FAIL\n", __func__);
		ret_val |= (1 << (checktype + ERR_SFT));
		goto fail_wait_sorting_mode;
	}
	himax_switch_data_type(checktype);

	ret_val |= himax_get_rawdata(RAW, datalen);
	if (ret_val) {
		E("%s: himax_get_rawdata FAIL\n", __func__);
		ret_val |= (1 << (checktype + ERR_SFT));
		goto fail_get_rawdata;
	}

	/*get Max DC from FW*/
	g_dc_max = himax_get_max_dc();

	/* back to normal */
	himax_switch_data_type(HX_BACK_NORMAL);

	I("%s: Init OK, start to test!\n", __func__);

	len += snprintf(g_start_log+len, 256 * sizeof(char), "\n%s%s\n",
		g_himax_inspection_mode[checktype], ": data as follow!\n");

	ret_val |= himax_data_campare(checktype, RAW, ret_val);

	himax_get_arraydata_edge(RAW);

	len += snprintf(g_start_log + len, 256 * sizeof(char) - len,
			"\n arraydata_min1 = %d,", arraydata_min1);
	len += snprintf(g_start_log + len, 256 * sizeof(char) - len,
			"  arraydata_min2 = %d,", arraydata_min2);
	len += snprintf(g_start_log + len, 256 * sizeof(char) - len,
			"  arraydata_min3 = %d,", arraydata_min3);
	len += snprintf(g_start_log + len, 256 * sizeof(char) - len,
			"\n arraydata_max1 = %d,", arraydata_max1);
	len += snprintf(g_start_log + len, 256 * sizeof(char) - len,
			"  arraydata_max2 = %d,", arraydata_max2);
	len += snprintf(g_start_log + len, 256 * sizeof(char) - len,
			"  arraydata_max3 = %d\n", arraydata_max3);

	if (!ret_val) {/*PASS*/
		snprintf(g_rslt_log, 256 * sizeof(char), "\n%s%s\n",
			g_himax_inspection_mode[checktype], ":Test Pass!");
		I("pass write log\n");
	} else {/*FAIL*/
		snprintf(g_rslt_log, 256 * sizeof(char), "\n%s%s\n",
			g_himax_inspection_mode[checktype], ":Test Fail!");
	  I("fail write log\n");
	}

	hx_test_data_get(RAW, g_start_log, g_rslt_log, checktype);
fail_get_rawdata:
fail_wait_sorting_mode:
	kfree(RAW);
	return ret_val;
}

/* claculate 10's power function */
static int himax_power_cal(int pow, int number)
{
	int i = 0;
	int result = 1;

	for (i = 0; i < pow; i++)
		result *= 10;
	result = result * number;

	return result;

}

/* String to int */
static int hiamx_parse_str2int(char *str)
{
	int i = 0;
	int temp_cal = 0;
	int result = 0;
	unsigned int str_len = strlen(str);
	int negtive_flag = 0;

	for (i = 0; i < str_len; i++) {
		if (str[i] != '-' && str[i] > '9' && str[i] < '0') {
			E("%s: Parsing fail!\n", __func__);
			result = -9487;
			negtive_flag = 0;
			break;
		}
		if (str[i] == '-') {
			negtive_flag = 1;
			continue;
		}
		temp_cal = str[i] - '0';
		result += himax_power_cal(str_len-i-1, temp_cal);
		/* str's the lowest char is the number's the highest number
		 * So we should reverse this number before using the power
		 * function
		 * -1: starting number is from 0 ex:10^0 = 1,10^1=10
		 */
	}

	if (negtive_flag == 1)
		result = 0 - result;

	return result;
}





int hx_find_crtra_id(char *input)
{
	int i = 0;
	int result = 0;

	for (i = 0 ; i < HX_CRITERIA_SIZE ; i++) {
		if (strcmp(g_hx_inspt_crtra_name[i], input) == 0) {
			result = i;
			I("find the str=%s,idx=%d\n",
			  g_hx_inspt_crtra_name[i], i);
			break;
		}
	}
	if (i > (HX_CRITERIA_SIZE - 1)) {
		E("%s: find Fail!\n", __func__);
		return LENGTH_FAIL;
	}

	return result;
}
#ifdef HX_INSPT_DBG
int hx_print_crtra_after_parsing(void)
{
	int i = 0, j = 0;
	int all_mut_len = ic_data->HX_TX_NUM*ic_data->HX_RX_NUM;

	for (i = 0; i < HX_CRITERIA_SIZE; i++) {
		I("Now is %s\n", g_hx_inspt_crtra_name[i]);
		if (g_inspt_crtra_flag[i] == 1) {
			for (j = 0; j < all_mut_len; j++) {
				PCT("%d, ", g_inspection_criteria[i][j]);
				if (j % 16 == 15)
					PCT("\n");
			}
		} else {
			I("No this Item in this criteria file!\n");
		}
		PCT("\n");
	}

	return 0;
}
#endif

static int hx_crtra_get(char *result, int himax_count_type, int comprae_data)
{
	int temp = 0;

	temp = hiamx_parse_str2int(result);

	if (temp != -9487)
		g_inspection_criteria[himax_count_type][comprae_data] = temp;
	else {
		I("%s: Parsing Fail in %d, rslt = %d\n",
		__func__, comprae_data, temp);
		return HX_INSP_EFILE;
	}

#ifdef HX_INSPT_DBG
		/* dbg:print all of criteria from parsing file */
		hx_print_crtra_after_parsing();
#endif

	return HX_INSP_OK;
}

static int himax_parse_criteria_str(char *str_data, int hx_str_len)
{
	int err = HX_INSP_OK;
	char result[100] = {0};
	char str_rslt[100] = {0};
	int str_rslt_nm = 0;
	int result_nm = 0;
	char str_len = 0;
	char *str_addr = NULL;
	int str_flag = 1;
	int i, j, k = 0;
	int crtra_id = 0;
	int mul_num = ic_data->HX_TX_NUM * ic_data->HX_RX_NUM;
	int flag = 1;
	int temp;

	if (private_ts->debug_log_level & BIT(4))
		I("%s,Entering\n", __func__);

	for (str_rslt_nm = 0; str_rslt_nm < hx_str_len; str_rslt_nm++)
		str_rslt[str_rslt_nm] = *(str_data + str_rslt_nm);

	crtra_id = hx_find_crtra_id(str_rslt);
	if (crtra_id == -1) {
		E("Please check criteria file again!\n");
		return HX_INSP_EFILE;
	}
	g_inspt_crtra_flag[crtra_id] = 1;

	str_data = str_data + hx_str_len + 1;

	for (i = 0; i < mul_num; i++) {
		if (i <= mul_num - 2) {
			while (flag) {
				if (*(str_data + flag) == ',') {
					str_addr = str_data + flag;
					flag = 1;
					break;
				}
				flag++;
			}
			if (str_addr == NULL)
				continue;
			str_flag = 1;
			str_len = str_addr - str_data;
			for (j = 1; j <= str_len; j++) {
				if ((*(str_data+j) == '\r'
					|| *(str_data+j) == '\n')) {
					memset(result, 0, 100);
					for (k = 0; k < j ; k++)
						result[k] = *(str_data + k);
					str_flag = 0;
					break;
				}
			}
			if (str_flag) {
				memset(result, 0, 100);
				for (k = 0; k < str_len; k++)
					result[k] = *(str_data + k);
			}
			err = hx_crtra_get(result, crtra_id, i);
			if (err != HX_INSP_OK) {
				E("%s:Get crrteria Fail!!\n", __func__);
				return HX_INSP_EFILE;
			}
			str_data = str_addr + 1;
		} else{
			temp = 1;
			while (*(str_data + temp) <= '9'
			&& *(str_data + temp) >= '0')
				temp++;
			str_len = temp;
			memset(result, 0, 100);
			for (result_nm = 0; result_nm < str_len; result_nm++)
				result[result_nm] = *(str_data + result_nm);

			err = hx_crtra_get(result, crtra_id, mul_num - 1);
			if (err != HX_INSP_OK) {
				E("%s:Get crrteria Fail!\n", __func__);
				return HX_INSP_EFILE;
			}
		}
	}

	if (private_ts->debug_log_level & BIT(4))
		I("%s,END\n", __func__);
	return err;
	/* parsing Criteria end */
}

static int himax_test_item_parse(char *str_data, int str_size)
{
	int size = str_size;
	char *str_ptr = str_data;
	char *end_ptr = NULL;
	int i = 0;
	int ret = HX_INSP_EFILE;

	I("%s,str_data: %p, str_size: %d\n", __func__, str_data, str_size);

	do {
		str_ptr = strnstr(str_ptr, "HIMAX", size);
		end_ptr = strnstr(str_ptr, "\x0d\x0a", size);
		if (str_ptr != NULL && end_ptr != NULL) {
			while (g_himax_inspection_mode[i]) {
				if (strncmp(str_ptr, g_himax_inspection_mode[i],
				end_ptr - str_ptr) == 0) {
					I("%s,Find item : %s\n", __func__,
						g_himax_inspection_mode[i]);
					g_test_item_flag[i] = 1;
					ret = HX_INSP_OK;
					break;
				}
				i++;
			}
			size = str_size - (end_ptr - str_data);
			str_ptr = end_ptr++;
			i = 0;
		} else {
			I("%s,Can't find %s or %s\n", __func__,
				"HIMAX", "\x0d\x0a");
			break;
		}
	} while (size > strlen("HIMAX"));

	return ret;
}


static int himax_parse_criteria(const struct firmware *file_entry)
{
	int ret = 0;
	int i = 0;
	int start_str_len = 0;
	char *start_ptr = NULL;

	while (g_hx_inspt_crtra_name[i] != NULL) {

		start_ptr = strnstr(file_entry->data,
			g_hx_inspt_crtra_name[i], file_entry->size);

		if (start_ptr != NULL) {
			I("g_hx_inspt_crtra_name[%d] = %s\n",
				i, g_hx_inspt_crtra_name[i]);
			start_str_len = strlen(g_hx_inspt_crtra_name[i]);

			ret |= himax_parse_criteria_str(start_ptr,
				start_str_len);
		}
		i++;
	}

	return ret;
}


static int himax_parse_test_dri_file(const struct firmware *file_entry)
{
	int start_str_len = 0;
	int str_size = 0;
	char *start_ptr = NULL;
	char *end_ptr = NULL;
	int i = 0;
	int j = 0;
	char str[2][60]; /*[0]->Start string, [1]->End string*/
	char *str_tail[2] = {"_Begin]\x0d\x0a", "_End]\x0d\x0a"};
	int ret = HX_INSP_OK;

	while (g_hx_head_str[i]) {
		/*compose header string of .dri file*/
		for (j = 0; j < 2; j++) {
			strlcpy(str[j], "[", sizeof(str[j]));
			strlcat(str[j], g_hx_head_str[i], sizeof(str[j]));
			strlcat(str[j], str_tail[j], sizeof(str[j]));
			/*I("%s string[%d] : %s\n", __func__, j, str[j]);*/
		}

		/*find each group of .dri file*/
		start_str_len = strlen(str[0]);
		start_ptr = strnstr(file_entry->data, str[0], file_entry->size);
		end_ptr = strnstr(file_entry->data, str[1], file_entry->size);

		if (start_ptr == NULL || end_ptr == NULL) {
			E("%s,Can't find string %s\n", __func__,
				g_hx_head_str[i]);
		} else {
			/*parse each sub group string*/
			/*if (strncmp(g_hx_head_str[i], "Project_Info",*/
			/*strlen(g_hx_head_str[i])) == 0) {*/
				/* get project informaion - Not Use*/
			/*}*/
			str_size = end_ptr - start_ptr - start_str_len;
			/*I("%s,String Length = %d\n", __func__, str_size);*/

			if (strncmp(g_hx_head_str[i], "TestItem",
			strlen(g_hx_head_str[i])) == 0) {
				/*get Test Item*/
				I("%s,Start to parse %s\n", __func__,
					g_hx_head_str[i]);
				ret |= himax_test_item_parse(start_ptr
					+ start_str_len,
					str_size);
			}
			/*if (strncmp(g_hx_head_str[i], "TestCriteria_Weight",*/
			/*strlen(g_hx_head_str[i])) == 0) {*/
				/*get Test Criteria Weight - Not Use*/
			/*}*/
			if (strncmp(g_hx_head_str[i], "TestCriteria",
			strlen(g_hx_head_str[i])) == 0) {
				/*get Test Criteria*/
				I("%s,Start to parse %s\n", __func__,
				g_hx_head_str[i]);
				ret |= himax_parse_criteria(file_entry);
			}
		}
		i++;
	}

	return ret;
}

static void himax_test_item_chk(int csv_test)
{
	int i = 0;

	if (csv_test)
		for (i = 0; i < HX_CRITERIA_ITEM - 1; i++)
			g_test_item_flag[i] = 1;

	g_test_item_flag[HX_OPEN] &=
		(g_inspt_crtra_flag[IDX_OPENMIN] == 1
		&& g_inspt_crtra_flag[IDX_OPENMAX] == 1) ? 1 : 0;

	g_test_item_flag[HX_MICRO_OPEN] &=
		(g_inspt_crtra_flag[IDX_M_OPENMIN] == 1
		&& g_inspt_crtra_flag[IDX_M_OPENMAX] == 1) ? 1 : 0;

	g_test_item_flag[HX_SHORT] &=
		(g_inspt_crtra_flag[IDX_SHORTMIN] == 1
		&& g_inspt_crtra_flag[IDX_SHORTMAX] == 1) ? 1 : 0;

	g_test_item_flag[HX_RAWDATA] &=
		(g_inspt_crtra_flag[IDX_RAWMIN] == 1
		&& g_inspt_crtra_flag[IDX_RAWMAX] == 1) ? 1 : 0;

	g_test_item_flag[HX_BPN_RAWDATA] &=
		(g_inspt_crtra_flag[IDX_BPN_RAWMIN] == 1
		&& g_inspt_crtra_flag[IDX_BPN_RAWMAX] == 1) ? 1 : 0;

	g_test_item_flag[HX_SC] &=
		(g_inspt_crtra_flag[IDX_SCMIN] == 1
		&& g_inspt_crtra_flag[IDX_SCMAX] == 1
		&& g_inspt_crtra_flag[IDX_SC_GOLDEN] == 1) ? 1 : 0;

	g_test_item_flag[HX_WT_NOISE] &=
		(g_inspt_crtra_flag[IDX_WT_NOISEMIN] == 1
		&& g_inspt_crtra_flag[IDX_WT_NOISEMAX] == 1) ? 1 : 0;

	g_test_item_flag[HX_ABS_NOISE] &=
		(g_inspt_crtra_flag[IDX_ABS_NOISEMIN] == 1
		&& g_inspt_crtra_flag[IDX_ABS_NOISEMAX] == 1) ? 1 : 0;

	g_test_item_flag[HX_SORTING] &=
		(g_inspt_crtra_flag[IDX_SORTMIN] == 1
		&& g_inspt_crtra_flag[IDX_SORTMAX] == 1) ? 1 : 0;

	g_test_item_flag[HX_GAPTEST_RAW] &=
		(g_inspt_crtra_flag[IDX_GAP_HOR_RAWMAX] == 1
		&& g_inspt_crtra_flag[IDX_GAP_HOR_RAWMIN] == 1
		&& g_inspt_crtra_flag[IDX_GAP_VER_RAWMAX] == 1
		&& g_inspt_crtra_flag[IDX_GAP_VER_RAWMIN] == 1) ? 1 : 0;

	g_test_item_flag[HX_ACT_IDLE_RAWDATA] &=
		(g_inspt_crtra_flag[IDX_ACT_IDLE_RAWDATA_MIN] == 1
		&& g_inspt_crtra_flag[IDX_ACT_IDLE_RAWDATA_MAX] == 1) ? 1 : 0;

	g_test_item_flag[HX_ACT_IDLE_BPN_RAWDATA] &=
		(g_inspt_crtra_flag[IDX_ACT_IDLE_RAW_BPN_MIN] == 1
		&& g_inspt_crtra_flag[IDX_ACT_IDLE_RAW_BPN_MAX] == 1) ? 1 : 0;

	g_test_item_flag[HX_ACT_IDLE_NOISE] &=
		(g_inspt_crtra_flag[IDX_ACT_IDLE_NOISE_MIN] == 1
		&& g_inspt_crtra_flag[IDX_ACT_IDLE_NOISE_MAX] == 1) ? 1 : 0;

	g_test_item_flag[HX_LP_RAWDATA] &=
		(g_inspt_crtra_flag[IDX_LP_RAWDATA_MIN] == 1
		&& g_inspt_crtra_flag[IDX_LP_RAWDATA_MAX] == 1) ? 1 : 0;

	g_test_item_flag[HX_LP_BPN_RAWDATA] &=
		(g_inspt_crtra_flag[IDX_LP_RAW_BPN_MIN] == 1
		&& g_inspt_crtra_flag[IDX_LP_RAW_BPN_MAX] == 1) ? 1 : 0;

	g_test_item_flag[HX_LP_WT_NOISE] &=
		(g_inspt_crtra_flag[IDX_LP_WT_NOISEMAX] == 1
		&& g_inspt_crtra_flag[IDX_LP_WT_NOISEMIN] == 1) ? 1 : 0;

	g_test_item_flag[HX_LP_ABS_NOISE] &=
		(g_inspt_crtra_flag[IDX_LP_NOISE_ABS_MAX] == 1
		&& g_inspt_crtra_flag[IDX_LP_NOISE_ABS_MIN] == 1) ? 1 : 0;

	g_test_item_flag[HX_LP_IDLE_RAWDATA] &=
		(g_inspt_crtra_flag[IDX_LP_IDLE_RAWDATA_MAX] == 1
		&& g_inspt_crtra_flag[IDX_LP_IDLE_RAWDATA_MIN] == 1) ? 1 : 0;

	g_test_item_flag[HX_LP_IDLE_BPN_RAWDATA] &=
		(g_inspt_crtra_flag[IDX_LP_IDLE_RAW_BPN_MIN] == 1
		&& g_inspt_crtra_flag[IDX_LP_IDLE_RAW_BPN_MAX] == 1) ? 1 : 0;

	g_test_item_flag[HX_LP_IDLE_NOISE] &=
		(g_inspt_crtra_flag[IDX_LP_IDLE_NOISE_MAX] == 1
		&& g_inspt_crtra_flag[IDX_LP_IDLE_NOISE_MIN] == 1) ? 1 : 0;

	do_lpwg_test = g_test_item_flag[HX_LP_RAWDATA]
			| g_test_item_flag[HX_LP_BPN_RAWDATA]
			| g_test_item_flag[HX_LP_WT_NOISE]
			| g_test_item_flag[HX_LP_ABS_NOISE]
			| g_test_item_flag[HX_LP_IDLE_RAWDATA]
			| g_test_item_flag[HX_LP_IDLE_BPN_RAWDATA]
			| g_test_item_flag[HX_LP_IDLE_NOISE];

	if (private_ts->debug_log_level & BIT(4)) {
		for (i = 0; i < HX_CRITERIA_ITEM - 1; i++)
			I("g_test_item_flag[%d] = %d\n",
				i, g_test_item_flag[i]);
	}
}

int hx_get_size_str_arr(char **input)
{
	int i = 0;
	int result = 0;

	while (input[i] != NULL)
		i++;

	result = i;
	if (private_ts->debug_log_level & BIT(4))
		I("There is %d in [0]=%s\n", result, input[0]);

	return result;
}

static void hx_print_ic_id(void)
{
	uint8_t i;
	uint32_t len = 0;
	char *prt_data = NULL;

	prt_data = kzalloc(sizeof(char) * HX_SZ_ICID, GFP_KERNEL);
	if (prt_data == NULL)
		return;


	len += snprintf(prt_data + len, HX_SZ_ICID - len,
			"IC ID : ");
	for (i = 0; i < 13; i++) {
		len += snprintf(prt_data + len, HX_SZ_ICID - len,
				"%02X", ic_data->vendor_ic_id[i]);
	}
	len += snprintf(prt_data + len, HX_SZ_ICID - len,
			"\n");

	memcpy(&g_rslt_data[0], prt_data, len);
	g_rslt_data_len = len;
	I("%s: g_rslt_data_len=%d!\n", __func__, g_rslt_data_len);

	kfree(prt_data);
}

static int himax_self_test_data_init(void)
{
	const struct firmware *file_entry = NULL;
	struct himax_ts_data *ts = private_ts;
	char *file_name_1 = "hx_criteria.dri";
	char *file_name_2 = "hx_criteria.csv";
	int ret = HX_INSP_OK;
	int err = 0;
	int i = 0;

	/*
	 * 5: one value will not over than 99999, so get this size of string
	 * 2: get twice size
	 */
	g_1kind_raw_size = 5 * ic_data->HX_RX_NUM * ic_data->HX_TX_NUM * 2;

	/* get test item and its items of criteria*/
	HX_CRITERIA_ITEM = hx_get_size_str_arr(g_himax_inspection_mode);
	HX_CRITERIA_SIZE = hx_get_size_str_arr(g_hx_inspt_crtra_name);
	I("There is %d HX_CRITERIA_ITEM and %d HX_CRITERIA_SIZE\n",
	  HX_CRITERIA_ITEM, HX_CRITERIA_SIZE);

	/* init criteria data*/
	g_test_item_flag = kcalloc(HX_CRITERIA_ITEM, sizeof(int), GFP_KERNEL);
	if (g_test_item_flag == NULL) {

		ret = HX_INSP_MEMALLCTFAIL;
		goto err_malloc_test_item_flag;
	}

	g_inspt_crtra_flag = kcalloc(HX_CRITERIA_SIZE, sizeof(int), GFP_KERNEL);
	if (g_inspt_crtra_flag == NULL) {

		ret = HX_INSP_MEMALLCTFAIL;
		goto err_malloc_inspt_crtra_flag;
	}

	g_inspection_criteria = kcalloc(HX_CRITERIA_SIZE,
			sizeof(int *), GFP_KERNEL);
	if (g_inspection_criteria == NULL) {

		ret = HX_INSP_MEMALLCTFAIL;
		goto err_malloc_inspection_criteria;
	}

	for (i = 0; i < HX_CRITERIA_SIZE; i++) {
		g_inspection_criteria[i] = kcalloc((ic_data->HX_TX_NUM
				* ic_data->HX_RX_NUM),
				sizeof(int), GFP_KERNEL);
		if (g_inspection_criteria[i] == NULL) {


			ret = HX_INSP_MEMALLCTFAIL;
			goto err_malloc_inspection_criteria2;
		}
	}

	g_rslt_data_len = 0;
	if (g_rslt_data == NULL) {
		g_rslt_data = kcalloc(g_1kind_raw_size, sizeof(char),
			GFP_KERNEL);
		if (g_rslt_data == NULL) {
			E("%s,%d: Memory allocation failed!\n",
				__func__, __LINE__);
			ret = HX_INSP_MEMALLCTFAIL;
			goto err_malloc_rslt_data;
		}
	}
	I("%s: initialize g_rslt_data, length = %d\n",
		__func__, g_1kind_raw_size);
	memset(g_rslt_data, 0x00, g_1kind_raw_size  * sizeof(char));

	/* default path is /system/etc/firmware */
	/* request criteria file*/
	err = request_firmware(&file_entry, file_name_1, ts->dev);
	if (err < 0) {
		E("%s,Fail to get %s\n", __func__, file_name_1);
		err = request_firmware(&file_entry, file_name_2, ts->dev);
		if (err < 0) {
			E("%s,Fail to get %s\n", __func__, file_name_2);
			I("No criteria file file");
			ret = HX_INSP_EFILE;
			goto err_open_criteria_file;
		} else {
			I("%s,Success to get %s\n", __func__, file_name_2);
			/* parsing criteria from file .csv*/
			ret = himax_parse_criteria(file_entry);
			release_firmware(file_entry);
			if (ret > 0)
				goto err_open_criteria_file;
			himax_test_item_chk(true);
		}
	} else {
		/* parsing test file .dri*/
		I("%s,Success to get %s\n", __func__, file_name_1);
		ret = himax_parse_test_dri_file(file_entry);
		release_firmware(file_entry);
		if (ret > 0)
			goto err_open_criteria_file;
		himax_test_item_chk(false);
	}

	if (private_ts->debug_log_level & BIT(4)) {
		/* print get criteria string */
		for (i = 0 ; i < HX_CRITERIA_SIZE ; i++) {
			if (g_inspt_crtra_flag[i] != 0)
				I("%s: [%d]There is String=%s\n",
					__func__, i, g_hx_inspt_crtra_name[i]);
		}
	}

	snprintf(g_file_path, (int)(strlen(HX_RSLT_OUT_PATH)
			+ strlen(HX_RSLT_OUT_FILE)+1),
			"%s%s", HX_RSLT_OUT_PATH, HX_RSLT_OUT_FILE);

	file_w_flag = true;
	return ret;

err_open_criteria_file:
	kfree(g_rslt_data);
	g_rslt_data = NULL;
err_malloc_rslt_data:

err_malloc_inspection_criteria2:
	for (i = 0; i < HX_CRITERIA_SIZE; i++) {
		if (g_inspection_criteria[i] != NULL) {
			kfree(g_inspection_criteria[i]);
			g_inspection_criteria[i] = NULL;
		}
	}
	kfree(g_inspection_criteria);
	g_inspection_criteria = NULL;
err_malloc_inspection_criteria:
	kfree(g_inspt_crtra_flag);
	g_inspt_crtra_flag = NULL;
err_malloc_inspt_crtra_flag:
	kfree(g_test_item_flag);
	g_test_item_flag = NULL;
err_malloc_test_item_flag:
	return ret;
}

static void himax_self_test_data_deinit(void)
{
	int i = 0;

	/*dbg*/
	/* for (i = 0; i < HX_CRITERIA_ITEM; i++)
	 *	I("%s:[%d]%d\n", __func__, i, g_inspection_criteria[i]);
	 */

	I("%s: release allocated memory\n", __func__);

	for (i = 0; i < HX_CRITERIA_SIZE; i++) {
		if (g_inspection_criteria[i] != NULL) {
			kfree(g_inspection_criteria[i]);
			g_inspection_criteria[i] = NULL;
		}
	}
	kfree(g_inspection_criteria);
	g_inspection_criteria = NULL;

	kfree(g_inspt_crtra_flag);
	g_inspt_crtra_flag = NULL;

	kfree(g_test_item_flag);
	g_test_item_flag = NULL;
	I("%s: release finished\n", __func__);

}

static int himax_chip_self_test(struct seq_file *s, void *v)
{
	uint32_t ret = HX_INSP_OK;
	uint32_t test_size = ic_data->HX_TX_NUM	* ic_data->HX_RX_NUM
			+ ic_data->HX_TX_NUM + ic_data->HX_RX_NUM;
	int i = 0;
	int j = 0;
	char test_result[] = "0P-1P-2P-3P-4P";
	uint8_t tmp_addr[DATA_LEN_4] = {0x94, 0x72, 0x00, 0x10};
	uint8_t tmp_data[DATA_LEN_4] = {0x01, 0x00, 0x00, 0x00};
	struct file *raw_file = NULL;
	struct filename *vts_name = NULL;
	mm_segment_t fs;
	loff_t pos = 0;
	uint32_t rslt = HX_INSP_OK;
//#if 0
	uint8_t tmp_addr2[4] = {0x00, 0x7F, 0x00, 0x10};
	uint8_t tmp_data2[4] = {0};
//#endif
	I("%s:IN\n", __func__);

	private_ts->suspend_resume_done = 0;

	ret = himax_self_test_data_init();
	if (ret > 0) {
		E("%s: initialize self test failed\n", __func__);
		goto END;
	}

#if defined(HX_ZERO_FLASH)
	g_core_fp.fp_0f_op_file_dirly(MPAP_FWNAME);
	hx_turn_on_mp_func(1);
	g_core_fp.fp_reload_disable(0);
	if (private_ts->debug_log_level & BIT(4))
		I("%s:start sense on!\n", __func__);
	g_core_fp.fp_sense_on(0x00);
	if (private_ts->debug_log_level & BIT(4))
		I("%s:end sense on!\n", __func__);
#endif

	if (!kp_getname_kernel) {
		E("kp_getname_kernel is NULL, not open file!\n");
		file_w_flag = false;
	} else
		vts_name = kp_getname_kernel(g_file_path);

	if (raw_file == NULL && file_w_flag) {
		raw_file = kp_file_open_name(vts_name,
			O_TRUNC|O_CREAT|O_RDWR, 0660);

		if (IS_ERR(raw_file)) {
			E("%s open file failed = %ld\n",
				__func__, PTR_ERR(raw_file));
			file_w_flag = false;
		}
	}

	fs = get_fs();
	set_fs(get_ds());

	hx_print_ic_id();
	if (file_w_flag) {
		vfs_write(raw_file, g_rslt_data, g_rslt_data_len, &pos);
		pos += g_rslt_data_len;
	}

	/*Do normal test items*/
	for (i = 0; i < HX_CRITERIA_ITEM; i++) {
		if (i < HX_LP_RAWDATA) {
			if (g_test_item_flag[i] == 1) {
				I("%d. %s Start\n", i,
					g_himax_inspection_mode[i]);
				rslt = mpTestFunc(i, test_size);
				if (file_w_flag &&
					((rslt & HX_INSP_EGETRAW) == 0) &&
					((rslt & HX_INSP_ESWITCHMODE) == 0)) {
					vfs_write(raw_file, g_rslt_data,
					g_rslt_data_len, &pos);
					pos += g_rslt_data_len;
				}
				ret |= rslt;

				I("%d. %s End, ret = %d\n", i,
					g_himax_inspection_mode[i], ret);
			}
		} else {
			break;
		}
	}

	/* Press power key and do LPWUG test items*/
	if (do_lpwg_test) {
		himax_press_powerkey();
		/* Wait suspend done */
		while (private_ts->suspend_resume_done != 1) {
			usleep_range(1000, 1001);
			if (private_ts->debug_log_level & BIT(4))
				I("Waiting for tp suspend!\n");
		}
		private_ts->suspend_resume_done = 0;

		for (; i < HX_CRITERIA_ITEM; i++) {
			if (g_test_item_flag[i] == 1) {
				I("%d.%s Start\n", i,
					g_himax_inspection_mode[i]);
				rslt = mpTestFunc(i, test_size);
				if (file_w_flag &&
					((rslt & HX_INSP_EGETRAW) == 0) &&
					((rslt & HX_INSP_ESWITCHMODE) == 0)) {
					vfs_write(raw_file, g_rslt_data,
						g_rslt_data_len, &pos);
					pos += g_rslt_data_len;
				}
				ret |= rslt;

				I("%d.%s End\n", i, g_himax_inspection_mode[i]);
			}
		}
		himax_press_powerkey();
		/* Wait resume done */
		while (private_ts->suspend_resume_done != 1)
			usleep_range(1000, 1001);
	}
	if (file_w_flag)
		filp_close(raw_file, NULL);
	set_fs(fs);

#if defined(HX_ZERO_FLASH)
	private_ts->in_self_test = 0;
	g_core_fp.fp_0f_op_file_dirly(BOOT_UPGRADE_FWNAME);
	g_core_fp.fp_sense_off(true);
	hx_turn_on_mp_func(0);
//#if 0
	/* reload enable with keeping high byte values */
	g_core_fp.fp_register_read(tmp_addr2, 4, tmp_data2, 0);
	tmp_data2[1] = 0x9A;
	tmp_data2[0] = 0xA9;
	g_core_fp.fp_register_write(
		tmp_addr2,
		DATA_LEN_4,
		tmp_data2,
		0);
	/************************************************/
//#endif
	/* set N frame back to default value 1*/
	g_core_fp.fp_register_write(tmp_addr, 4, tmp_data, 0);
//	g_core_fp.fp_reload_disable(0);
#else
	if (private_ts->debug_log_level & BIT(4))
		I("%s:start sense off!\n", __func__);
	g_core_fp.fp_sense_off(true);
	if (private_ts->debug_log_level & BIT(4))
		I("%s:end sense off!\n", __func__);
	hx_turn_on_mp_func(0);
	/*himax_set_N_frame(1, HX_INSPECTION_WT_NOISE);*/
	/* set N frame back to default value 1*/
	g_core_fp.fp_register_write(tmp_addr, 4, tmp_data, 0);
#if !defined(HX_ZERO_FLASH)
	if (g_core_fp.fp_reload_disable != NULL)
		g_core_fp.fp_reload_disable(0);
#endif
#endif

	if (himax_check_mode(HX_RAWDATA)) {
		I("%s:try to  Need to back to Normal!\n", __func__);
		himax_switch_mode_inspection(HX_RAWDATA);
		if (private_ts->debug_log_level & BIT(4))
			I("%s:start sense on!\n", __func__);
		g_core_fp.fp_sense_on(0);
		if (private_ts->debug_log_level & BIT(4))
			I("%s:end sense on!\n", __func__);
		himax_wait_sorting_mode(HX_RAWDATA);
	} else {
		I("%s: It has been in Normal!\n", __func__);
		if (private_ts->debug_log_level & BIT(4))
			I("%s:start sense on!\n", __func__);
		g_core_fp.fp_sense_on(0);
		if (private_ts->debug_log_level & BIT(4))
			I("%s:end sense on!\n", __func__);
	}

	if (ret == HX_INSP_OK) {
		//seq_puts(s, "Self_Test Pass:\n");
		//seq_puts(s, "0P-1P-2P-3P-4P");
		I("%s:Self_Test Pass\n", __func__);
		seq_puts(s, "result=1\n");
	} else {
		//seq_puts(s, "Self_Test Fail:\n");
		for (i = 0, j = 1; i < HX_CRITERIA_ITEM - 1; i++) {
			if (g_test_item_flag[i] == 1) {
				if (j <= strlen(test_result)) {
					test_result[j] = ((ret & (1 << (i + ERR_SFT)))
					== (1 << (i + ERR_SFT))) ? 'F':'P';
					j += 3;
				}
			}
		}
		//seq_printf(s, "%s", test_result);
		I("%s:Self_Test FAIL\n", __func__);
		seq_puts(s, "result=0\n");
	}
	himax_self_test_data_deinit();

END:
	I("running status = %X\n", ret);

	/*if (ret != 0)*/
		/*ret = 1;*/

	I("OUT\n", __func__);
	return ret;
}

void himax_inspect_data_clear(void)
{
	if (!g_rslt_data) {
		kfree(g_rslt_data);
		g_rslt_data = NULL;
	}
}

void himax_inspection_init(void)
{
	I("%s: enter, %d\n", __func__, __LINE__);
	g_core_fp.fp_chip_self_test = himax_chip_self_test;
}

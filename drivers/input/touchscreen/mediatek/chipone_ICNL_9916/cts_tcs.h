#ifndef _CTS_TCS_H_
#define _CTS_TCS_H_

#include "cts_config.h"

#define    TCS_RD_ADDR        0xF1
#define    TCS_WR_ADDR        0xF0

#define    INT_DATA_MAX_SIZ     8192

#pragma pack(1)
typedef struct {
	u8 addr;
	u16 cmd;
	u16 datlen;
	u16 crc16;
} tcs_tx_head;

typedef struct {
	u8 ecode;
	u16 cmd;
	u16 crc16;
} tcs_rx_tail;

typedef struct {
	u8 baseFlag;
	u8 classID;
	u8 cmdID;
	u8 isRead;
	u8 isWrite;
	u8 isData;
} TcsCmdValue_t;
#pragma pack()

int cts_tcs_get_fw_ver(const struct cts_device *cts_dev, u16 *fwver);
int cts_tcs_get_lib_ver(const struct cts_device *cts_dev, u16 *libver);
int cts_tcs_get_ddi_ver(const struct cts_device *cts_dev, u8 *ddiver);
int cts_tcs_get_res_x(const struct cts_device *cts_dev, u16 *res_x);
int cts_tcs_get_res_y(const struct cts_device *cts_dev, u16 *res_y);
int cts_tcs_get_rows(const struct cts_device *cts_dev, u8 *rows);
int cts_tcs_get_cols(const struct cts_device *cts_dev, u8 *cols);
int cts_tcs_get_flip_x(const struct cts_device *cts_dev, bool *flip_x);
int cts_tcs_get_flip_y(const struct cts_device *cts_dev, bool *flip_y);
int cts_tcs_get_swap_axes(const struct cts_device *cts_dev, bool *swap_axes);

int cts_tcs_get_int_mode(const struct cts_device *cts_dev, u8 *int_mode);
int cts_tcs_get_int_keep_time(const struct cts_device *cts_dev,
			      u16 *int_keep_time);
int cts_tcs_get_rawdata_target(const struct cts_device *cts_dev,
			      u16 *rawdata_target);
int cts_tcs_get_esd_method(const struct cts_device *cts_dev, u8 *esd_method);

int cts_tcs_get_touchinfo(struct cts_device *cts_dev,
			  struct cts_device_touch_info *touch_info);
int cts_tcs_get_esd_protection(const struct cts_device *cts_dev,
			       u8 *esd_protection);

int cts_tcs_get_data_ready_flag(const struct cts_device *cts_dev, u8 *ready);
int cts_tcs_clr_data_ready_flag(const struct cts_device *cts_dev);
int cts_tcs_clr_gstr_ready_flag(const struct cts_device *cts_dev);
int cts_tcs_enable_get_rawdata(const struct cts_device *cts_dev);
int cts_tcs_is_enabled_get_rawdata(const struct cts_device *cts_dev,
				   u8 *enabled);
int cts_tcs_disable_get_rawdata(const struct cts_device *cts_dev);
int cts_tcs_enable_get_cneg(const struct cts_device *cts_dev);
int cts_tcs_disable_get_cneg(const struct cts_device *cts_dev);
int cts_tcs_is_cneg_ready(const struct cts_device *cts_dev, u8 *ready);

int cts_tcs_quit_guesture_mode(const struct cts_device *cts_dev);

int cts_tcs_get_rawdata(const struct cts_device *cts_dev, u8 *buf);
int cts_tcs_get_diffdata(const struct cts_device *cts_dev, u8 *buf);
int cts_tcs_get_basedata(const struct cts_device *cts_dev, u8 *buf);
int cts_tcs_get_cneg(const struct cts_device *cts_dev, u8 *buf, size_t size);

int cts_tcs_read_hw_reg(const struct cts_device *cts_dev, u32 addr,
			u8 *buf, size_t size);
int cts_tcs_write_hw_reg(const struct cts_device *cts_dev, u32 addr,
			 u8 *buf, size_t size);
int cts_tcs_read_ddi_reg(const struct cts_device *cts_dev, u32 addr,
			 u8 *regbuf, size_t size);
int cts_tcs_write_ddi_reg(const struct cts_device *cts_dev, u32 addr,
			  u8 *regbuf, size_t size);
int cts_tcs_read_fw_reg(const struct cts_device *cts_dev, u32 addr,
			u8 *buf, size_t size);
int cts_tcs_write_fw_reg(const struct cts_device *cts_dev, u32 addr,
			 u8 *buf, size_t size);
int cts_tcs_read_reg(const struct cts_device *cts_dev, uint16_t cmd,
		     u8 *rbuf, size_t rlen);
int cts_tcs_write_reg(const struct cts_device *cts_dev, uint16_t cmd,
		      u8 *wbuf, size_t wlen);

int cts_tcs_get_fw_id(const struct cts_device *cts_dev, u16 *fwid);
int cts_tcs_get_workmode(const struct cts_device *cts_dev, u8 *workmode);
int cts_tcs_set_workmode(const struct cts_device *cts_dev, u8 workmode);
int cts_tcs_set_openshort_mode(const struct cts_device *cts_dev, u8 mode);
int cts_tcs_set_tx_vol(const struct cts_device *cts_dev, u8 txvol);

int cts_tcs_set_short_test_type(const struct cts_device *cts_dev,
				u8 short_type);
int cts_tcs_set_openshort_enable(const struct cts_device *cts_dev, u8 enable);
int cts_tcs_is_openshort_enabled(const struct cts_device *cts_dev,
				 u8 *enabled);

int cts_tcs_set_esd_enable(const struct cts_device *cts_dev, u8 enable);
int cts_tcs_set_cneg_enable(const struct cts_device *cts_dev, u8 enable);
int cts_tcs_set_mnt_enable(const struct cts_device *cts_dev, u8 enable);
int cts_tcs_is_display_on(const struct cts_device *cts_dev, u8 *display_on);
int cts_tcs_set_display_on(const struct cts_device *cts_dev, u8 display_on);
int cts_tcs_is_cneg_enabled(const struct cts_device *cts_dev, u8 *enabled);
int cts_tcs_is_mnt_enabled(const struct cts_device *cts_dev, u8 *enabled);
int cts_tcs_set_pwr_mode(const struct cts_device *cts_dev, u8 pwr_mode);

int cts_tcs_read_sram_normal_mode(const struct cts_device *cts_dev,
				  u32 addr, void *dst, size_t len, int retry,
				  int delay);

enum TcsCmdIndex {
	TP_STD_CMD_INFO_CHIP_FW_ID_RO,
	TP_STD_CMD_INFO_FW_VER_RO,
	TP_STD_CMD_INFO_TOUCH_XY_INFO_RO,
    TP_STD_CMD_INFO_MODULE_ID_RO,

	TP_STD_CMD_TP_DATA_OFFSET_AND_TYPE_CFG_RW,
	TP_STD_CMD_TP_DATA_READ_START_RO,
	TP_STD_CMD_TP_DATA_COORDINATES_RO,
	TP_STD_CMD_TP_DATA_RAW_RO,
	TP_STD_CMD_TP_DATA_DIFF_RO,
	TP_STD_CMD_TP_DATA_BASE_RO,
	TP_STD_CMD_TP_DATA_CNEG_RO,
	TP_STD_CMD_TP_DATA_WR_REG_RAM_SEQUENCE_WO,
	TP_STD_CMD_TP_DATA_WR_REG_RAM_BATCH_WO,
	TP_STD_CMD_TP_DATA_WR_DDI_REG_SEQUENCE_WO,
	TP_STD_CMD_GET_DATA_BY_POLLING_RO,

	TP_STD_CMD_SYS_STS_READ_RO,
	TP_STD_CMD_SYS_STS_WORK_MODE_RW,
	TP_STD_CMD_SYS_STS_DAT_RDY_FLAG_RW,
	TP_STD_CMD_SYS_STS_PWR_STATE_RW,
	TP_STD_CMD_SYS_STS_CHARGER_PLUGIN_RW,
	TP_STD_CMD_SYS_STS_DDI_CODE_VER_RO,
	TP_STD_CMD_SYS_STS_DAT_TRANS_IN_NORMAL_RW,
	TP_STD_CMD_SYS_STS_VSTIM_LVL_RW,
	TP_STD_CMD_SYS_STS_CNEG_RDY_FLAG_RW,
	TP_STD_CMD_SYS_STS_EP_PLUGIN_RW,
	TP_STD_CMD_SYS_STS_RESET_WO,
    TP_STD_CMD_SYS_STS_INT_TEST_EN_RW,
    TP_STD_CMD_SYS_STS_SET_INT_PIN_RW,
	TP_STD_CMD_SYS_STS_CNEG_RD_EN_RW,
	TP_STD_CMD_SYS_STS_INT_MODE_RW,
	TP_STD_CMD_SYS_STS_INT_KEEP_TIME_RW,
	TP_STD_CMD_SYS_STS_CURRENT_WORKMODE_RO,
	TP_STD_CMD_SYS_STS_DATA_CAPTURE_SUPPORT_RO,
	TP_STD_CMD_SYS_STS_DATA_CAPTURE_EN_RW,
	TP_STD_CMD_SYS_STS_DATA_CAPTURE_FUNC_MAP_RW,
	TP_STD_CMD_SYS_STS_PANEL_DIRECTION_RW,
	TP_STD_CMD_SYS_STS_GAME_MODE_RW,
	/*C3T code for HQ-229320 by jishen at 2022/10/20  start*/
	TP_STD_CMD_SYS_STS_POCKET_MODE_EN_RW,
	/*C3T code for HQ-229320 by jishen at 2022/10/20  end*/

    TP_STD_CMD_GSTR_WAKEUP_EN_RW,
    TP_STD_CMD_GSTR_DAT_RDY_FLAG_GSTR_RW,
    TP_STD_CMD_GSTR_ENTER_MAP_RW,

	TP_STD_CMD_MNT_EN_RW,
	TP_STD_CMD_MNT_FORCE_EXIT_MNT_WO,

	TP_STD_CMD_DDI_ESD_EN_RW,
	TP_STD_CMD_DDI_ESD_OPTIONS_RW,

	TP_STD_CMD_CNEG_EN_RW,
	TP_STD_CMD_CNEG_OPTIONS_RW,

	TP_STD_CMD_COORD_FLIP_X_EN_RW,
	TP_STD_CMD_COORD_FLIP_Y_EN_RW,
	TP_STD_CMD_COORD_SWAP_AXES_EN_RW,

	TP_STD_CMD_PARA_PROXI_EN_RW,

	TP_STD_CMD_OPENSHORT_EN_RW,
	TP_STD_CMD_OPENSHORT_MODE_SEL_RW,
	TP_STD_CMD_OPENSHORT_SHORT_SEL_RW,
	TP_STD_CMD_OPENSHORT_SHORT_DISP_ON_EN_RW,
};

#ifdef _CTS_TCS_C_
static TcsCmdValue_t TcsCmdValue[] = {
/*---------------------------------------------------
 *baseFlage, classID, cmdID, isRead, isWrite, isData
 *---------------------------------------------------
 */
	{ 0, 0, 3, 1, 0, 0 },	/* TP_STD_CMD_INFO_CHIP_FW_ID_RO */
	{ 0, 0, 5, 1, 0, 0 },	/* TP_STD_CMD_INFO_FW_VER_RO */
	{ 0, 0, 7, 1, 0, 0 },	/* TP_STD_CMD_INFO_TOUCH_XY_INFO_RO */
	{ 0, 0, 17, 1, 0, 0 },	/* TP_STD_CMD_INFO_MODULE_ID_RO */

	{ 0, 1, 1, 1, 1, 0 },	/* TP_STD_CMD_TP_DATA_OFFSET_AND_TYPE_CFG_RW */
	{ 0, 1, 2, 1, 0, 1 },	/* TP_STD_CMD_TP_DATA_READ_START_RO */
	{ 0, 1, 3, 1, 0, 1 },	/* TP_STD_CMD_TP_DATA_COORDINATES_RO */
	{ 0, 1, 4, 1, 0, 1 },	/* TP_STD_CMD_TP_DATA_RAW_RO */
	{ 0, 1, 5, 1, 0, 1 },	/* TP_STD_CMD_TP_DATA_DIFF_RO */
	{ 0, 1, 6, 1, 0, 1 },	/* TP_STD_CMD_TP_DATA_BASE_RO */
	{ 0, 1, 10, 1, 0, 1 },	/* TP_STD_CMD_TP_DATA_CNEG_RO */
	{ 0, 1, 20, 0, 1, 1 },	/* TP_STD_CMD_TP_DATA_WR_REG_RAM_SEQUENCE_WO */
	{ 0, 1, 21, 0, 1, 1 },	/* TP_STD_CMD_TP_DATA_WR_REG_RAM_BATCH_WO */
	{ 0, 1, 22, 0, 1, 1 },	/* TP_STD_CMD_TP_DATA_WR_DDI_REG_SEQUENCE_WO */
	{ 0, 1, 35, 1, 0, 0 },	/* TP_STD_CMD_GET_DATA_BY_POLLING_RO */

	{ 0, 2, 0, 1, 0, 1 },	/* TP_STD_CMD_SYS_STS_READ_RO *//*CHECK!!*/
	{ 0, 2, 1, 1, 1, 0 },	/* TP_STD_CMD_SYS_STS_WORK_MODE_RW */
	{ 0, 2, 3, 1, 1, 0 },	/* TP_STD_CMD_SYS_STS_DAT_RDY_FLAG_RW */
	{ 0, 2, 4, 1, 1, 1 },	/* TP_STD_CMD_SYS_STS_PWR_STATE_RW */
	{ 0, 2, 5, 1, 1, 0 },	/* TP_STD_CMD_SYS_STS_CHARGER_PLUGIN_RW */
	{ 0, 2, 6, 1, 0, 0 },	/* TP_STD_CMD_SYS_STS_DDI_CODE_VER_RO */
	{ 0, 2, 7, 1, 1, 0 },	/* TP_STD_CMD_SYS_STS_DAT_TRANS_IN_NORMAL_RW */
	{ 0, 2, 8, 1, 1, 0 },	/* TP_STD_CMD_SYS_STS_VSTIM_LVL_RW */
	{ 0, 2, 17, 1, 1, 0 },	/* TP_STD_CMD_SYS_STS_CNEG_RDY_FLAG_RW */
	{ 0, 2, 19, 1, 1, 0 },	/* TP_STD_CMD_SYS_STS_EP_PLUGIN_RW */
	{ 0, 2, 22, 0, 1, 0 },	/* TP_STD_CMD_SYS_STS_RESET_WO */
	{ 0, 2, 23, 1, 1, 0 },	/* TP_STD_CMD_SYS_STS_INT_TEST_EN_RW */
	{ 0, 2, 24, 1, 1, 0 },	/* TP_STD_CMD_SYS_STS_SET_INT_PIN_RW */
	{ 0, 2, 25, 1, 1, 0 },	/* TP_STD_CMD_SYS_STS_CNEG_RD_EN_RW */
	{ 0, 2, 35, 1, 1, 0 },	/* TP_STD_CMD_SYS_STS_INT_MODE_RW */
	{ 0, 2, 36, 1, 1, 0 },	/* TP_STD_CMD_SYS_STS_INT_KEEP_TIME_RW */
	{ 0, 2, 51, 1, 0, 0 },	/* TP_STD_CMD_SYS_STS_CURRENT_WORKMODE_RO */
	{ 0, 2, 63, 1, 0, 0 },	/* TP_STD_CMD_SYS_STS_DATA_CAPTURE_SUPPORT_RO */
	{ 0, 2, 64, 1, 1, 0 },	/* TP_STD_CMD_SYS_STS_DATA_CAPTURE_EN_RW */
	{ 0, 2, 65, 1, 1, 0 },	/* TP_STD_CMD_SYS_STS_DATA_CAPTURE_FUNC_MAP_RW */
	{ 0, 2, 66, 1, 1,  0 }, /* TP_STD_CMD_SYS_STS_PANEL_DIRECTION_RW */
	{ 0, 2, 78, 1, 1,  0 }, /* TP_STD_CMD_SYS_STS_GAME_MODE_RW */

	/*C3T code for HQ-229320 by jishen at 2022/10/20  start*/
	{ 0, 2, 80, 1, 1,  0 }, /* TP_STD_CMD_SYS_STS_POCKET_MODE_EN_RW */
	/*C3T code for HQ-229320 by jishen at 2022/10/20  end*/

    { 0, 3,  1,  1,  1,  0 }, /* TP_STD_CMD_GSTR_WAKEUP_EN_RW */
    { 0, 3, 30,  1,  1,  0 }, /* TP_STD_CMD_GSTR_DAT_RDY_FLAG_GSTR_RW */
    { 0, 3, 40,  1,  1,  0 }, /* TP_STD_CMD_GSTR_ENTER_MAP_RW */

	{ 0, 4, 1, 1, 1, 0 },	/* TP_STD_CMD_MNT_EN_RW */
	{ 0, 4, 3, 0, 1, 0 },	/* TP_STD_CMD_MNT_FORCE_EXIT_MNT_WO */

	{ 0, 5, 1, 1, 1, 0 },	/* TP_STD_CMD_DDI_ESD_EN_RW */
	{ 0, 5, 2, 1, 1, 0 },	/* TP_STD_CMD_DDI_ESD_OPTIONS_RW */

	{ 0, 6, 1, 1, 1, 0 },	/* TP_STD_CMD_CNEG_EN_RW */
	{ 0, 6, 2, 1, 1, 0 },	/* TP_STD_CMD_CNEG_OPTIONS_RW */

	{ 0, 7, 2, 1, 1, 0 },	/* TP_STD_CMD_COORD_FLIP_X_EN_RW */
	{ 0, 7, 3, 1, 1, 0 },	/* TP_STD_CMD_COORD_FLIP_Y_EN_RW */
	{ 0, 7, 4, 1, 1, 0 },	/* TP_STD_CMD_COORD_SWAP_AXES_EN_RW */

	{ 0, 9, 42, 1, 1, 0 },	/* TP_STD_CMD_PARA_PROXI_EN_RW */

	{ 0, 11, 1, 1, 1, 0 },	/* TP_STD_CMD_OPENSHORT_EN_RW */
	{ 0, 11, 2, 1, 1, 0 },	/* TP_STD_CMD_OPENSHORT_MODE_SEL_RW */
	{ 0, 11, 3, 1, 1, 0 },	/* TP_STD_CMD_OPENSHORT_SHORT_SEL_RW */
	{ 0, 11, 4, 1, 1, 0 },	/* TP_STD_CMD_OPENSHORT_SHORT_DISP_ON_EN_RW */
};
#endif /* _CTS_TCS_C_ */

extern int cts_tcs_spi_read(const struct cts_device * cts_dev,
        enum TcsCmdIndex cmdIdx, u8 *rdata, size_t rdatalen);
extern int cts_tcs_spi_write(const struct cts_device * cts_dev,
        enum TcsCmdIndex cmdIdx, u8 *wdata, size_t wdatalen);

extern struct cts_dev_ops tcs_ops;

#endif /* _CTS_TCS_H_ */

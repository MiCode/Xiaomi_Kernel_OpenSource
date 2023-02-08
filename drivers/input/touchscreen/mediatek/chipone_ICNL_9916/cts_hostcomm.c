#include "cts_config.h"
#include "cts_firmware.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_hostcomm.h"

extern bool set_short_test_type(const struct cts_device *cts_dev, u8 type);
extern int disable_fw_monitor_mode(const struct cts_device *cts_dev);
extern int disable_fw_auto_compensate(const struct cts_device *cts_dev);
extern int disable_fw_esd_protection(const struct cts_device *cts_dev);
extern int set_fw_work_mode(const struct cts_device *cts_dev, u8 mode);
extern int set_display_state(const struct cts_device *cts_dev, bool active);

int cts_hostcomm_get_fw_ver(const struct cts_device *cts_dev, u16 *fwver)
{
	return cts_get_firmware_version(cts_dev, fwver);
}

int cts_hostcomm_get_lib_ver(const struct cts_device *cts_dev, u16 *libver)
{
	return cts_get_lib_version(cts_dev, libver);
}

int cts_hostcomm_get_ddi_ver(const struct cts_device *cts_dev, u8 *ddiver)
{
	return cts_get_ddi_version(cts_dev, ddiver);
}

int cts_hostcomm_get_res_x(const struct cts_device *cts_dev, u16 *res_x)
{
	return cts_get_x_resolution(cts_dev, res_x);
}

int cts_hostcomm_get_res_y(const struct cts_device *cts_dev, u16 *res_y)
{
	return cts_get_y_resolution(cts_dev, res_y);
}

int cts_hostcomm_get_rows(const struct cts_device *cts_dev, u8 *rows)
{
	return cts_get_num_rows(cts_dev, rows);
}

int cts_hostcomm_get_cols(const struct cts_device *cts_dev, u8 *cols)
{
	return cts_get_num_cols(cts_dev, cols);
}

int cts_hostcomm_get_flip_x(const struct cts_device *cts_dev, bool *flip_x)
{
	u8 val;
	int ret;

	ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_FLAG_BITS, &val);
	if (ret < 0)
		return ret;

	*flip_x = !!(val & BIT(2));
	return 0;
}

int cts_hostcomm_get_flip_y(const struct cts_device *cts_dev, bool *flip_y)
{
	u8 val;
	int ret;

	ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_FLAG_BITS, &val);
	if (ret < 0)
		return ret;

	*flip_y = !!(val & BIT(3));
	return 0;
}

int cts_hostcomm_get_swap_axes(const struct cts_device *cts_dev,
			       bool *swap_axes)
{
	u8 val;
	int ret;

	ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_SWAP_AXES, &val);
	if (ret < 0)
		return ret;

	*swap_axes = !!val;
	return 0;
}

int cts_hostcomm_get_int_mode(const struct cts_device *cts_dev, u8 *int_mode)
{
	return cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_INT_MODE, int_mode);
}

int cts_hostcomm_get_int_keep_time(const struct cts_device *cts_dev,
				   u16 *int_keep_time)
{
	return cts_fw_reg_readw(cts_dev, CTS_DEVICE_FW_REG_INT_KEEP_TIME,
				int_keep_time);
}

int cts_hostcomm_get_rawdata_target(const struct cts_device *cts_dev,
				   u16 *rawdata_target)
{
	return cts_fw_reg_readw(cts_dev, CTS_DEVICE_FW_REG_RAWDATA_TARGET,
				rawdata_target);
}

int cts_hostcomm_get_esd_method(const struct cts_device *cts_dev,
				u8 *esd_method)
{
	return cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_ESD_PROTECTION,
				esd_method);
}

int cts_hostcomm_get_esd_protection(const struct cts_device *cts_dev,
				    u8 *esd_protection)
{
	return cts_fw_reg_readb(cts_dev, 0x8000 + 342, esd_protection);
}

int cts_hostcomm_get_data_ready_flag(const struct cts_device *cts_dev,
				     u8 *ready)
{
	return cts_get_data_ready_flag(cts_dev, ready);
}

int cts_hostcomm_clr_data_ready_flag(const struct cts_device *cts_dev)
{
	return cts_clr_data_ready_flag(cts_dev);
}

int cts_hostcomm_enable_get_rawdata(const struct cts_device *cts_dev)
{
	return cts_enable_get_rawdata(cts_dev);
}

int cts_hostcomm_disable_get_rawdata(const struct cts_device *cts_dev)
{
	return cts_disable_get_rawdata(cts_dev);
}

int cts_hostcomm_enable_get_cneg(const struct cts_device *cts_dev)
{
	return cts_send_command(cts_dev, CTS_CMD_ENABLE_READ_CNEG);
}

int cts_hostcomm_disable_get_cneg(const struct cts_device *cts_dev)
{
	return cts_send_command(cts_dev, CTS_CMD_DISABLE_READ_CNEG);
}

int cts_hostcomm_is_cneg_ready(const struct cts_device *cts_dev, u8 *ready)
{
	return cts_fw_reg_readb(cts_dev,
				CTS_DEVICE_FW_REG_COMPENSATE_CAP_READY, ready);
}

int cts_hostcomm_quit_guesture_mode(const struct cts_device *cts_dev)
{
	//TODO:
	return 0;
}

int cts_hostcomm_get_rawdata(const struct cts_device *cts_dev, u8 *buf)
{
	return cts_fw_reg_readsb_delay_idle(cts_dev,
					    CTS_DEVICE_FW_REG_RAW_DATA,
					    buf,
					    cts_dev->fwdata.rows *
					    cts_dev->fwdata.cols * 2, 500);
}

int cts_hostcomm_get_diffdata(const struct cts_device *cts_dev, u8 *buf)
{
	return cts_fw_reg_readsb_delay_idle(cts_dev,
					    CTS_DEVICE_FW_REG_DIFF_DATA,
					    buf,
					    (cts_dev->fwdata.rows + 2) *
					    (cts_dev->fwdata.cols + 2) * 2,
					    500);
}

int cts_hostcomm_get_basedata(const struct cts_device *cts_dev, u8 *buf)
{
	//TODO:
	return 0;
}

int cts_hostcomm_get_cneg(const struct cts_device *cts_dev, u8 *buf,
			  size_t size)
{
	return cts_fw_reg_readsb_delay_idle(cts_dev,
					    CTS_DEVICE_FW_REG_COMPENSATE_CAP,
					    buf,
					    cts_dev->hwdata->num_row *
					    cts_dev->hwdata->num_col, 500);
}

int cts_hostcomm_read_hw_reg(const struct cts_device *cts_dev, u32 addr,
			     u8 *regbuf, size_t size)
{

	return cts_hw_reg_readsb_retry(cts_dev, addr, regbuf, size, 3, 100);
}

int cts_hostcomm_write_hw_reg(const struct cts_device *cts_dev, u32 addr,
			      u8 *regbuf, size_t size)
{
	return cts_hw_reg_writesb_retry(cts_dev, addr, regbuf, size, 3, 100);
}

int cts_hostcomm_read_ddi_reg(const struct cts_device *cts_dev, u32 addr,
			      u8 *regbuf, size_t size)
{
	int ret = -1;

	cts_err("%s: BUG! Should not be here!", __func__);
	return ret;
}

int cts_hostcomm_write_ddi_reg(const struct cts_device *cts_dev, u32 addr,
			       u8 *regbuf, size_t size)
{
	int ret = -1;

	cts_err("%s: BUG! Should not be here!", __func__);
	return ret;
}

int cts_hostcomm_read_reg(const struct cts_device *cts_dev, uint16_t cmd,
			  u8 *rbuf, size_t rlen)
{
	int ret = -1;

	cts_err("%s: BUG! Should not be here!", __func__);
	return ret;
}

int cts_hostcomm_write_reg(const struct cts_device *cts_dev, uint16_t cmd,
			   u8 *wbuf, size_t wlen)
{
	int ret = -1;

	cts_err("%s: BUG! Should not be here!", __func__);
	return ret;
}

int cts_hostcomm_read_fw_reg(const struct cts_device *cts_dev, u32 addr,
			     u8 *regbuf, size_t size)
{
	return cts_fw_reg_readsb(cts_dev, addr, regbuf, size);
}

int cts_hostcomm_write_fw_reg(const struct cts_device *cts_dev, u32 addr,
			      u8 *regbuf, size_t size)
{
	return cts_fw_reg_writesb(cts_dev, addr, regbuf, size);
}

int cts_hostcomm_get_touchinfo(struct cts_device *cts_dev,
			       struct cts_device_touch_info *touch_info)
{
	memset(touch_info, 0, sizeof(*touch_info));
	return cts_fw_reg_readsb(cts_dev, CTS_DEVICE_FW_REG_TOUCH_INFO,
				 touch_info, sizeof(*touch_info));
}

int cts_hostcomm_get_fw_id(const struct cts_device *cts_dev, u16 *fwid)
{
	u16 tmp = 0xffff;
	int ret;

	ret = cts_fw_reg_readw(cts_dev, CTS_DEVICE_FW_REG_CHIP_TYPE, &tmp);
	tmp = be16_to_cpu(tmp);
	*fwid = tmp;
	cts_info("%s:%04X", __func__, tmp);

	return ret;
}

int cts_hostcomm_get_workmode(const struct cts_device *cts_dev, u8 *workmode)
{
	return cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_GET_WORK_MODE,
				workmode);
}

int cts_hostcomm_set_workmode(const struct cts_device *cts_dev, u8 workmode)
{
	return set_fw_work_mode(cts_dev, workmode);
}

int cts_hostcomm_set_openshort_mode(const struct cts_device *cts_dev, u8 type)
{
	int i, ret;

	cts_info("Set test type %d", type);

	for (i = 0; i < 5; i++) {
		u8 type_readback;

		ret = cts_fw_reg_writeb(cts_dev, 0x34, type);
		if (ret) {
			cts_err("Write test type register to failed %d", ret);
			continue;
		}

		ret = cts_fw_reg_readb(cts_dev, 0x34, &type_readback);
		if (ret) {
			cts_err("Read test type register failed %d", ret);
			continue;
		}

		if (type != type_readback) {
			cts_err("Set test type %u != readback %u", type,
				type_readback);
			ret = -EFAULT;
			continue;
		}
	}

	return ret;
}

int cts_hostcomm_set_tx_vol(const struct cts_device *cts_dev, u8 txvol)
{
	return cts_send_command(cts_dev, CTS_CMD_RECOVERY_TX_VOL);
}

int cts_hostcomm_is_enabled_get_rawdata(const struct cts_device *cts_dev,
					u8 *enabled)
{
	return cts_fw_reg_readb(cts_dev, 0x12, enabled);
}

int cts_hostcomm_set_short_test_type(const struct cts_device *cts_dev,
				     u8 short_type)
{
	return set_short_test_type(cts_dev, short_type);
}

int cts_hostcomm_is_openshort_enabled(const struct cts_device *cts_dev,
				      u8 *enabled)
{
	int ret = -1;

	cts_err("BUG! Should not be here!");
	return ret;
}

int cts_hostcomm_set_openshort_enable(const struct cts_device *cts_dev,
				      u8 enable)
{
	int ret = -1;

	cts_err("BUG! Should not be here!");
	return ret;
}

int cts_hostcomm_set_esd_enable(const struct cts_device *cts_dev, u8 enable)
{
	int ret = -1;

	if (!enable)
		return disable_fw_esd_protection(cts_dev);

	return ret;
}

int cts_hostcomm_is_cneg_enabled(const struct cts_device *cts_dev, u8 *enabled)
{
	return cts_fw_reg_readb(cts_dev, 0x8000 + 276, enabled);
}

int cts_hostcomm_is_mnt_enabled(const struct cts_device *cts_dev, u8 *enabled)
{
	return cts_fw_reg_readb(cts_dev, 0x8000 + 344, enabled);
}

int cts_hostcomm_set_cneg_enable(const struct cts_device *cts_dev, u8 enable)
{
	int ret = -1;

	if (!enable)
		return disable_fw_auto_compensate(cts_dev);

	return ret;
}

int cts_hostcomm_set_mnt_enable(const struct cts_device *cts_dev, u8 enable)
{
	int ret = -1;

	if (!enable)
		return disable_fw_monitor_mode(cts_dev);

	return ret;
}

int cts_hostcomm_is_display_on(const struct cts_device *cts_dev,
			       u8 *display_on)
{
	return cts_fw_reg_readb(cts_dev, 0x8000 + 163, display_on);
}

int cts_hostcomm_set_pwr_mode(const struct cts_device *cts_dev, u8 pwr_mode)
{
	//TODO:
	return 0;
}

int cts_hostcomm_set_display_on(const struct cts_device *cts_dev, u8 display_on)
{
	return set_display_state(cts_dev, false);
}

int cts_hostcomm_top_get_rawdata(struct cts_device *cts_dev, u8 *buf,
				 size_t size)
{
	//TODO: moved from sysfs:rawdata_show
	//TODO:
	return 0;
}

int cts_hostcomm_top_get_manual_diff(struct cts_device *cts_dev, u8 *buf,
				     size_t size)
{
	//TODO:
	return 0;
}

int cts_hostcomm_top_get_real_diff(struct cts_device *cts_dev, u8 *buf,
				   size_t size)
{
	//TODO:
	return 0;
}

int cts_hostcomm_top_get_noise_diff(struct cts_device *cts_dev, u8 *buf,
				    size_t size)
{
	//TODO:
	return 0;
}

int cts_hostcomm_top_get_basedata(struct cts_device *cts_dev, u8 *buf,
				  size_t size)
{
	//TODO:
	return 0;
}

int cts_hostcomm_top_get_cnegdata(struct cts_device *cts_dev, u8 *buf,
				  size_t size)
{
	return cts_get_compensate_cap(cts_dev, buf);
}

int cts_hostcomm_reset_device(const struct cts_device *cts_dev)
{
	return cts_plat_reset_device(cts_dev->pdata);
}

int cts_hostcomm_set_int_pin(const struct cts_device *cts_dev, u8 high)
{
    int ret;

    if (high) {
	ret = cts_send_command(cts_dev, CTS_CMD_WRTITE_INT_HIGH);
    } else {
	ret = cts_send_command(cts_dev, CTS_CMD_WRTITE_INT_LOW);
    }
    if (ret) {
	cts_err("Send command WRTITE_INT_%s failed %d", high ? "HIGH" : "LOW", ret);
    }

    return ret;
}

struct cts_dev_ops hostcomm_ops = {
	.get_fw_ver = cts_hostcomm_get_fw_ver,
	.get_lib_ver = cts_hostcomm_get_lib_ver,
	.get_ddi_ver = cts_hostcomm_get_ddi_ver,
	.get_res_x = cts_hostcomm_get_res_x,
	.get_res_y = cts_hostcomm_get_res_y,
	.get_rows = cts_hostcomm_get_rows,
	.get_cols = cts_hostcomm_get_cols,
	.get_flip_x = cts_hostcomm_get_flip_x,
	.get_flip_y = cts_hostcomm_get_flip_y,
	.get_swap_axes = cts_hostcomm_get_swap_axes,
	.get_int_mode = cts_hostcomm_get_int_mode,
	.get_int_keep_time = cts_hostcomm_get_int_keep_time,
	.get_rawdata_target = cts_hostcomm_get_rawdata_target,
	.get_esd_method = cts_hostcomm_get_esd_method,
	.get_touchinfo = cts_hostcomm_get_touchinfo,
	.get_esd_protection = cts_hostcomm_get_esd_protection,
	.get_data_ready_flag = cts_hostcomm_get_data_ready_flag,
	.clr_data_ready_flag = cts_hostcomm_clr_data_ready_flag,
	.enable_get_rawdata = cts_hostcomm_enable_get_rawdata,
	.is_enabled_get_rawdata = cts_hostcomm_is_enabled_get_rawdata,
	.disable_get_rawdata = cts_hostcomm_disable_get_rawdata,
	.enable_get_cneg = cts_hostcomm_enable_get_cneg,
	.disable_get_cneg = cts_hostcomm_disable_get_cneg,
	.is_cneg_ready = cts_hostcomm_is_cneg_ready,
	.quit_guesture_mode = cts_hostcomm_quit_guesture_mode,
	.get_rawdata = cts_hostcomm_get_rawdata,
	.get_diffdata = cts_hostcomm_get_diffdata,
	.get_basedata = cts_hostcomm_get_basedata,
	.get_cneg = cts_hostcomm_get_cneg,
	.read_hw_reg = cts_hostcomm_read_hw_reg,
	.write_hw_reg = cts_hostcomm_write_hw_reg,
	.read_ddi_reg = cts_hostcomm_read_ddi_reg,
	.write_ddi_reg = cts_hostcomm_write_ddi_reg,
	.read_fw_reg = cts_hostcomm_read_fw_reg,
	.write_fw_reg = cts_hostcomm_write_fw_reg,
	.read_reg = cts_hostcomm_read_reg,
	.write_reg = cts_hostcomm_write_reg,
	.get_fw_id = cts_hostcomm_get_fw_id,
	.get_workmode = cts_hostcomm_get_workmode,
	.set_workmode = cts_hostcomm_set_workmode,
	.set_openshort_mode = cts_hostcomm_set_openshort_mode,
	.set_tx_vol = cts_hostcomm_set_tx_vol,
	.set_short_test_type = cts_hostcomm_set_short_test_type,
	.set_openshort_enable = cts_hostcomm_set_openshort_enable,
	.is_openshort_enabled = cts_hostcomm_is_openshort_enabled,
	.set_esd_enable = cts_hostcomm_set_esd_enable,
	.set_cneg_enable = cts_hostcomm_set_cneg_enable,
	.set_mnt_enable = cts_hostcomm_set_mnt_enable,
	.is_display_on = cts_hostcomm_is_display_on,
	.set_display_on = cts_hostcomm_set_display_on,
	.is_cneg_enabled = cts_hostcomm_is_cneg_enabled,
	.is_mnt_enabled = cts_hostcomm_is_mnt_enabled,
	.set_pwr_mode = cts_hostcomm_set_pwr_mode,
	.read_sram_normal_mode = cts_read_sram_normal_mode,
	.init_int_data = NULL,	//TODO
	.deinit_int_data = NULL,	//TODO
	.get_has_int_data = NULL,
	.get_int_data_types = NULL,
	.set_int_data_types = NULL,
	.get_int_data_method = NULL,
	.set_int_data_method = NULL,
	.calc_int_data_size = NULL,
	.polling_data = NULL,
	.polling_test_data = NULL,

	.top_get_rawdata = cts_hostcomm_top_get_rawdata,
	.top_get_manual_diff = cts_hostcomm_top_get_manual_diff,
	.top_get_real_diff = cts_hostcomm_top_get_real_diff,
	.top_get_noise_diff = cts_hostcomm_top_get_noise_diff,
	.top_get_basedata = cts_hostcomm_top_get_basedata,
	.top_get_cnegdata = cts_hostcomm_top_get_cnegdata,

	.reset_device = cts_hostcomm_reset_device,

	.set_int_test = NULL,
	.set_int_pin = cts_hostcomm_set_int_pin,
	.get_module_id = NULL,
};

//TODO:
//fwdata->rawdata_target;

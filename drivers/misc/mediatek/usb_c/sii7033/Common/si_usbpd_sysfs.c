/*
*Copyright (C) 2013-2014 Silicon Image, Inc.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation version 2.
* This program is distributed AS-IS WITHOUT ANY WARRANTY of any
* kind, whether express or implied; INCLUDING without the implied warranty
* of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
* See the GNU General Public License for more details at
* http://www.gnu.org/licenses/gpl-2.0.html.
*/
#include <Wrap.h>
#include "si_usbpd_core.h"
#include "si_usbpd_main.h"
/*#include "si_usbpd_sysfs.h"*/
/*#include "si_usbpd.h"*/

#define CTRL_MSG_SIZE		0
#define DATA_MSG_SIZE		2
#define VDM_MSG_SIZE		2
#define VDM_RESP_MSG_SIZE	7

#if defined(I2C_DBG_SYSFS)
#define DEBUG_I2C_WRITE          1
#define DEBUG_I2C_READ           0
#define MAX_DEBUG_TRANSFER_SIZE  32
static int si_strtoul(char **str, int base, unsigned long *val);
#define sii_log_debug pr_debug
#define sii_log_info pr_info
#endif

#define USBPD_EVENT_STRING_LEN		128

#define SII_DRIVER_MINOR_MAX 1
/*SysFs attributes*/
#define SYS_ATTR_NAME_SRC_CAP			send_get_src_cap
#define SYS_ATTR_NAME_SET_PR_SWAP		send_pr_swap
#define SYS_ATTR_NAME_SET_DR_SWAP		send_dr_swap
#define SYS_ATTR_NAME_SET_VCONN_SWAP		send_vconn_swap
#define SYS_ATTR_NAME_SET_CUSTOM_MESSAGE	send_custom_message
#define SYS_ATTR_NAME_VDM_DISC_IDENTITY		init_alt_mode
#define SYS_ATTR_NAME_VDM_EXIT_MODE		exit_alt_mode

#if defined(I2C_DBG_SYSFS)
#define SYS_OBJECT_NAME_REG_ACCESS      reg_access
#define SYS_ATTR_NAME_REG_ACCESS_PAGE   page
#define SYS_ATTR_NAME_REG_ACCESS_OFFSET offset
#define SYS_ATTR_NAME_REG_ACCESS_LENGTH length
#define SYS_ATTR_NAME_REG_ACCESS_DATA   data
#endif

ssize_t show_xmit_ctrl(struct device *dev, struct device_attribute *attr, char *buf)
{
	/*TODO: API Implementation is Needed! */
	return 0;
}


/*
 * Wrapper for kstrtoul() that nul-terminates the input string at
 * the first non-digit character instead of returning an error.
 *
 * This function is destructive to the input string.
 *
 */
static int si_strtoul(char **str, int base, unsigned long *val)
{
	int tok_length, status, nul_offset;
	char *tstr = *str;

	nul_offset = 1;
	status = -EINVAL;
	if ((base == 0) && (tstr[0] == '0') && (tolower(tstr[1]) == 'x')) {
		tstr += 2;
		base = 16;
	}

	tok_length = strspn(tstr, "0123456789ABCDEFabcdef");
	if (tok_length) {
		if ((tstr[tok_length] == '\n') || (tstr[tok_length] == 0))
			nul_offset = 0;

		tstr[tok_length] = 0;
		status = kstrtoul(tstr, base, val);
		if (status == 0) {
			tstr = (tstr + tok_length) + nul_offset;
			*str = tstr;
		}
	}
	return status;
}

ssize_t set_src_cap(struct device *dev, struct device_attribute *attr,
		    const char *buf, size_t count)
{
	int status = -EINVAL;
	struct sii70xx_drv_context *drv_context = dev_get_drvdata(dev);
	unsigned long value;
	uint32_t src_cap[7];
	uint8_t bus_id = 0;
	int i = 0;
	char *str;
	char *pinput = kmalloc(count, GFP_KERNEL);

	pr_info("received string\n");

	if (pinput == 0)
		return -EINVAL;
	memcpy(pinput, buf, count);


	str = pinput;

	for (i = 0; ((i < 8) && ('\0' != *str)); i++) {
		status = si_strtoul(&str, 0, &value);
		if (-ERANGE == status) {
			pr_info("ERANGE %s%s%s\n", ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_src_cap_data;
		} else if (-EINVAL == status) {
			pr_info("EINVAL %s%s%s\n", ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_src_cap_data;
		} else if (status != 0) {
			pr_info("status:%d %s%s%s\n", status,
				ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_src_cap_data;
		} else if (value > 0xFFFFFFFF) {
			pr_info("value:0x%x str:%s%s%s\n", (uint) value,
				ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_src_cap_data;
		} else {
			switch (i) {
			case 0:
				bus_id = value;
				break;
			default:
				src_cap[i - 1] = value;
				break;
			}

			pr_info("entered array value %d : 0x%x", i, (uint) value);
		}
	}
	if (i == 0) {
		pr_info("No bus_id specified\n");
		goto exit_src_cap_data;
	} else if (i == 0) {
		pr_info("No data specified\n");
		goto exit_src_cap_data;
	} else {
		if (down_interruptible(&drv_context->isr_lock)) {
			status = -ERESTARTSYS;
			goto exit_src_cap_data;
		}
		if (!sii_drv_get_sr_cap(drv_context, bus_id)) {
			status = -ENODATA;
			pr_info("\n Error in Execution !!!!!!\n");
		} else {
			status = count;
		}
	}
exit_src_cap_data:
	up(&drv_context->isr_lock);
	kfree(pinput);
	return status;
}

ssize_t set_vdm_disc_identity(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	int status = -EINVAL;
	unsigned long value;
	uint32_t vdm_disc_id[7];
	uint8_t bus_id = 0;
	int i;
	char *str;
	char *pinput = kmalloc(count, GFP_KERNEL);
	struct sii70xx_drv_context *drv_context = dev_get_drvdata(dev);
	uint8_t svid_mode = 0x00;

	pr_info("received string\n");
	pr_info("count = %zx\n", count);

	if (pinput == 0)
		return -EINVAL;
	if ((count > 0x0C) || (count < 0x0C)) {
		pr_info("invlaid format given!!!");
		goto exit_vdm_disc_id_data;
	}
	memcpy(pinput, buf, count);

	str = pinput;

	for (i = 0; (i < 3) && ('\0' != *str); i++) {

		status = si_strtoul(&str, 0, &value);
		if (-ERANGE == status) {
			pr_info("ERANGE %s%s%s\n", ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_vdm_disc_id_data;
		} else if (-EINVAL == status) {
			pr_info("EINVAL %s%s%s\n", ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_vdm_disc_id_data;
		} else if (status != 0) {
			pr_info("status:%d %s%s%s\n", status,
				ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_vdm_disc_id_data;
		} else if (value > 0xFFFFFFFF) {
			pr_info("value:0x%x str:%s%s%s\n", (uint) value,
				ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_vdm_disc_id_data;
		} else {
			switch (i) {
			case 0:
				bus_id = value;
				break;
			default:
				vdm_disc_id[i - 1] = value;
				break;
			}
			pr_info("entered array value %d : 0x%x", i, (uint) value);
		}
	}
	if (i == 0) {
		pr_info("No bus_id specified\n");
		status = -ENODATA;
		goto exit_vdm_disc_id_data;
	} else if (vdm_disc_id[0] != 0x0F) {
		status = -ENODATA;
		goto exit_vdm_disc_id_data;
	} else {
		if (down_interruptible(&drv_context->isr_lock)) {
			status = -ERESTARTSYS;
			goto exit_vdm_disc_id_data;
		}
		if (vdm_disc_id[1] == 0x01) {
			pr_info("\nMHL mode initiated\n");
			svid_mode = 0x01;
		} else if (vdm_disc_id[1] == 0x02) {
			pr_info("\n DP mode initiated\n");
			svid_mode = 0x02;
		} else {
			pr_info("\n No mode initiated\n");
			svid_mode = 0x00;
			status = -ENODATA;
			goto exit;
		}
		if (!sii_drv_set_alt_mode(drv_context, bus_id, svid_mode)) {
			status = -ENODATA;
			pr_info("\n Error in Execution !!!!!!\n");
		} else {
			status = count;
		}
	}
exit:	up(&drv_context->isr_lock);
exit_vdm_disc_id_data:
	kfree(pinput);
	return status;
}

ssize_t set_vdm_exit_mode(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct sii70xx_drv_context *drv_context = dev_get_drvdata(dev);
	int status = -EINVAL;
	unsigned long value;
	uint32_t vdm_exit_mode[7] = { 0 };
	uint8_t bus_id = 0;
	int i;
	char *str;
	char *pinput = kmalloc(count, GFP_KERNEL);

	pr_info("count = %zx\n", count);
	pr_info("received string\n");

	if (pinput == 0)
		return -EINVAL;
	if ((count > 0x07) || (count < 0x07)) {
		pr_info("invlaid format given!!!");
		goto exit_vdm_exit_mode;
	}
	memcpy(pinput, buf, count);

	str = pinput;

	for (i = 0; (i < 2) && ('\0' != *str); i++) {
		status = si_strtoul(&str, 0, &value);
		if (-ERANGE == status) {
			pr_info("ERANGE %s%s%s\n", ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_vdm_exit_mode;
		} else if (-EINVAL == status) {
			pr_info("EINVAL %s%s%s\n", ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_vdm_exit_mode;
		} else if (status != 0) {
			pr_info("status:%d %s%s%s\n", status,
				ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_vdm_exit_mode;
		} else if (value > 0xFFFFFFFF) {
			pr_info("value:0x%x str:%s%s%s\n", (uint) value,
				ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_vdm_exit_mode;
		} else {
			switch (i) {
			case 0:
				bus_id = value;
				break;
			default:
				vdm_exit_mode[i - 1] = value;
				break;
			}
			pr_info("entered array value %d : 0x%x", i, (uint) value);
		}
	}
	if (i == 0) {
		pr_info("No bus_id specified\n");
		status = -ENODATA;
		goto exit_vdm_exit_mode;
	} else {
		if (down_interruptible(&drv_context->isr_lock)) {
			status = -ERESTARTSYS;
			goto exit_vdm_exit_mode;
		}
		if (!sii_drv_set_exit_mode(drv_context, bus_id)) {
			status = -ENODATA;
			pr_info("\n Error in Execution !!!!!!\n");
		} else {
			status = count;
		}
	}
	up(&drv_context->isr_lock);
exit_vdm_exit_mode:kfree(pinput);
	return status;
}

ssize_t set_vconn_swap(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	int status = -EINVAL;
	struct sii70xx_drv_context *drv_context = dev_get_drvdata(dev);
	unsigned long value;
	uint8_t bus_id = 0;
	int i = 0;
	char *str;
	char *pinput = kmalloc(count, GFP_KERNEL);

	pr_info("received string %s\n", buf);
	pr_info("received string copied %s\n", pinput);
	pr_info("count %zx\n", count);

	if (pinput == 0)
		return -EINVAL;

	if (count > 2) {
		pr_info("invlaid format given!!!");
		goto exit_vconn;
	}

	memcpy(pinput, buf, count);

	str = pinput;

	for (i = 0; ((i < 1) && ('\0' != *str)); i++) {
		status = si_strtoul(&str, 0, &value);
		if (-ERANGE == status) {
			pr_info("ERANGE %s%s%s\n", ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_vconn;
		} else if (-EINVAL == status) {
			pr_info("EINVAL %s%s%s\n", ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_vconn;
		} else if (status != 0) {
			pr_info("status:%d %s%s%s\n", status,
				ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_vconn;
		} else if (value > 0xFFFFFFFF) {
			pr_info("value:0x%x str:%s%s%s\n", (uint) value,
				ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_vconn;
		} else {
			switch (i) {
			case 0:
				bus_id = value;
				break;
			default:
				/*msg_type = value; */
				break;
			}
			pr_info("entered array value %d : 0x%x", i, (uint) value);
		}
	}

	if (i == 0) {
		pr_info("No bus_id specified\n");
		status = -ENODATA;
		goto exit_vconn;
	} else {
		if (bus_id != 0) {
			pr_info("\n Invalid BusID given !!!!!!\n");
			status = -ENODATA;
		} else {
			if (down_interruptible(&drv_context->isr_lock)) {
				status = -ERESTARTSYS;
				goto exit;
			}
			if (!sii_drv_set_vconn_swap(drv_context, bus_id)) {
				status = -ENODATA;
				pr_info("\n Error in Execution !!!!!!\n");
			} else {
				status = count;
			}
		}
	}
exit:
	up(&drv_context->isr_lock);
exit_vconn:
	kfree(pinput);
	return status;
}

ssize_t set_dr_swap(struct device *dev, struct device_attribute *attr,
		    const char *buf, size_t count)
{
	struct sii70xx_drv_context *drv_context = dev_get_drvdata(dev);
	int status = -EINVAL;
	unsigned long value;
	/*enum ctrl_msg msg_type = 0x09; */
	/*uint32_t ctrl_data[1] = {0}; */
	uint8_t bus_id = 0;
	int i = 0;
	char *str;
	char *pinput = kmalloc(count, GFP_KERNEL);

	pr_info("received string %s\n", buf);
	pr_info("received string copied %s\n", pinput);
	pr_info("count %zx\n", count);

	if (pinput == 0)
		return -EINVAL;

	if (count > 2) {
		pr_info("invlaid format given!!!");
		goto exit_xmit_ctrl;
	}
	memcpy(pinput, buf, count);

	str = pinput;

	for (i = 0; ((i < 1) && ('\0' != *str)); i++) {
		status = si_strtoul(&str, 0, &value);
		if (-ERANGE == status) {
			pr_info("ERANGE %s%s%s\n", ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_xmit_ctrl;
		} else if (-EINVAL == status) {
			pr_info("EINVAL %s%s%s\n", ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_xmit_ctrl;
		} else if (status != 0) {
			pr_info("status:%d %s%s%s\n", status,
				ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_xmit_ctrl;
		} else if (value > 0xFFFFFFFF) {
			pr_info("value:0x%x str:%s%s%s\n", (uint) value,
				ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_xmit_ctrl;
		} else {
			switch (i) {
			case 0:
				bus_id = value;
				break;
			default:
				/*msg_type = value; */
				break;
			}
			pr_info("entered array value %d : 0x%x", i, (uint) value);
		}
	}

	if (i == 0) {
		pr_info("No bus_id specified\n");
		status = -ENODATA;
		goto exit_xmit_ctrl;
	} else {
		if (bus_id != 0) {
			pr_info("\n Invalid BusID given !!!!!!\n");
			status = -ENODATA;
		} else {
			if (down_interruptible(&drv_context->isr_lock)) {
				status = -ERESTARTSYS;
				goto exit;
			}
			if (!sii_drv_set_dr_swap(drv_context, bus_id)) {
				status = -ENODATA;
				pr_info("\n Error in Execution !!!!!!\n");
			} else {
				status = count;
			}
		}
	}
exit:
	up(&drv_context->isr_lock);
exit_xmit_ctrl:
	kfree(pinput);
	return status;
}

ssize_t set_pr_swap(struct device *dev, struct device_attribute *attr,
		    const char *buf, size_t count)
{
	struct sii70xx_drv_context *drv_context = dev_get_drvdata(dev);
	int status = -EINVAL;
	unsigned long value;
	/*enum ctrl_msg msg_type = 0x0A;
	   uint32_t ctrl_data[1] = {0}; */
	uint8_t bus_id = 0;
	int i = 0;
	char *str;
	char *pinput = kmalloc(count, GFP_KERNEL);

	pr_info("received string %s\n", buf);
	pr_info("received string copied %s\n", pinput);
	pr_info("count %zx\n", count);

	if (pinput == 0)
		return -EINVAL;

	if (count > 2) {
		pr_info("invlaid format given!!!");
		goto exit_xmit_ctrl;
	}
	memcpy(pinput, buf, count);

	str = pinput;

	for (i = 0; ((i < 1) && ('\0' != *str)); i++) {
		status = si_strtoul(&str, 0, &value);
		if (-ERANGE == status) {
			pr_info("ERANGE %s%s%s\n", ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_xmit_ctrl;
		} else if (-EINVAL == status) {
			pr_info("EINVAL %s%s%s\n", ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_xmit_ctrl;
		} else if (status != 0) {
			pr_info("status:%d %s%s%s\n", status,
				ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_xmit_ctrl;
		} else if (value > 0xFFFFFFFF) {
			pr_info("value:0x%x str:%s%s%s\n", (uint) value,
				ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_xmit_ctrl;
		} else {
			switch (i) {
			case 0:
				bus_id = value;
				break;
			default:
				/*msg_type = value; */
				break;
			}
			pr_info("entered array value %d : 0x%x", i, (uint) value);
		}
	}

	if (i == 0) {
		pr_info("No bus_id specified\n");
		status = -ENODATA;
		goto exit_xmit_ctrl;
	} else {
		if (bus_id != 0) {
			pr_info("\n Invalid BusID given !!!!!!\n");
			status = -ENODATA;
		} else {
			if (down_interruptible(&drv_context->isr_lock)) {
				status = -ERESTARTSYS;
				goto exit;
			}
			if (!sii_drv_set_pr_swap(drv_context, bus_id)) {
				status = -ENODATA;
				pr_info("\n Error in Execution !!!!!!\n");
			} else {
				status = count;
			}
		}
	}
exit:
	up(&drv_context->isr_lock);
exit_xmit_ctrl:
	kfree(pinput);
	return status;
}

ssize_t set_custom_msg(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct sii70xx_drv_context *drv_context = dev_get_drvdata(dev);
	int status = -EINVAL;
	unsigned long value;
	uint8_t dfp_contract = 0;
	uint8_t bus_id = 0;
	int i;
	char *str;
	char *pinput = kmalloc(count, GFP_KERNEL);

	pr_info("received string %s\n", buf);
	pr_info("received string copied %s\n", pinput);
	pr_info("count %zx\n", count);

	if (pinput == 0)
		return -EINVAL;

	if ((count < 0x02)) {
		pr_info("invlaid format given!!!");
		goto exit_custom_msg;
	}
	memcpy(pinput, buf, count);

	str = pinput;

	for (i = 0; (i < 1) && ('\0' != *str); i++) {
		status = si_strtoul(&str, 0, &value);
		if (-ERANGE == status) {
			pr_info("ERANGE %s%s%s\n", ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_custom_msg;
		} else if (-EINVAL == status) {
			pr_info("EINVAL %s%s%s\n", ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_custom_msg;
		} else if (status != 0) {
			pr_info("status:%d %s%s%s\n", status,
				ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_custom_msg;
		} else if (value > 0xFF) {
			pr_info("value:0x%x str:%s%s%s\n", (uint) value,
				ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_custom_msg;
		} else {
			switch (i) {
			case 0:
				bus_id = value;
				break;
			default:
				dfp_contract = value;
				break;
			}
			pr_info("entered array value %d : 0x%x", i, (uint) value);
		}
	}
	if (i == 0) {
		pr_info("No bus_id specified\n");
		status = -ENODATA;
		goto exit_custom_msg;
	} else {
		if (down_interruptible(&drv_context->isr_lock)) {
			status = -ERESTARTSYS;
			goto exit;
		}
		if (!sii_drv_set_custom_msg(drv_context, bus_id, dfp_contract, true))
			status = -ENODATA;
		else
			status = count;
	}
exit:
	up(&drv_context->isr_lock);
exit_custom_msg:kfree(pinput);
	return status;
}

#if defined(I2C_DBG_SYSFS)
static int rpt_device_dbg_i2c_reg_xfer(struct sii70xx_drv_context *drv_context, u16 offset,
				       u16 count, bool rw_flag, u8 *buffer)
{
	if (rw_flag == DEBUG_I2C_WRITE)
		sii_platform_block_write8(offset, buffer, count);
	else
		sii_platform_block_read8(offset, buffer, count);
	return 0;
}

/*
 * set_reg_access_page() - Handle write request to set the
 *		reg access page value.
 *
 * The format of the string in buf must be:
 *	<pageaddr>
 * Where: <pageaddr> specifies the reg page of the register(s)
 *			to be written/read
 */
ssize_t set_reg_access_page(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct sii70xx_drv_context *drv_context = dev_get_drvdata(dev);
	unsigned long address = 0x100;
	int status = -EINVAL;
	char my_buf[20];
	unsigned int i;

	sii_log_debug("received string: %c%s%c\n", '"', buf, '"');
	if (count >= sizeof(my_buf)) {
		sii_log_debug("string too long %c%s%c\n", '"', buf, '"');
		return status;
	}
	for (i = 0; i < count; ++i) {
		if ('\n' == buf[i]) {
			my_buf[i] = '\0';
			break;
		}
		if ('\t' == buf[i]) {
			my_buf[i] = '\0';
			break;
		}
		if (' ' == buf[i]) {
			my_buf[i] = '\0';
			break;
		}
		my_buf[i] = buf[i];
	}

	status = kstrtoul(my_buf, 0, &address);
	if (-ERANGE == status)
		sii_log_debug("ERANGE %s\n", my_buf);
	else if (-EINVAL == status)
		sii_log_debug("EINVAL %s\n", my_buf);
	else if (status != 0)
		sii_log_debug("status:%d buf:%s\n", status, my_buf);
	else if (address > 0xFF)
		sii_log_debug("address:0x%lx buf:%s\n", address, my_buf);
	else {
		sii_log_debug("address:0x%lx buf:%s\n", address, my_buf);
		if (down_interruptible(&drv_context->isr_lock)) {
			sii_log_debug("could not get mutex\n");
			return -ERESTARTSYS;
		}
		if (drv_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			sii_log_debug("DEV_FLAG_SHUTDOWN\n");
			status = -ENODEV;
		} else {
			drv_context->debug_i2c_address = address;
			status = count;
		}
		up(&drv_context->isr_lock);
	}

	return status;
}

/*
 * show_reg_access_page() - Show the current page number to be used when
 *	reg_access/data is accessed.
 *
 */
ssize_t show_reg_access_page(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sii70xx_drv_context *drv_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	sii_log_debug("called\n");

	status = scnprintf(buf, PAGE_SIZE, "0x%02x", drv_context->debug_i2c_address);

	return status;
}

/*
 * set_reg_access_offset() - Handle write request to set the
 *		reg access page value.
 *
 * The format of the string in buf must be:
 *	<pageaddr>
 * Where: <pageaddr> specifies the reg page of the register(s)
 *			to be written/read
 */
ssize_t set_reg_access_offset(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct sii70xx_drv_context *drv_context = dev_get_drvdata(dev);
	unsigned long offset = 0x100;
	int status = -EINVAL;

	sii_log_info("received string: " "%s" "\n", buf);

	status = kstrtoul(buf, 0, &offset);
	if (-ERANGE == status) {
		sii_log_debug("ERANGE %s%s%s\n", ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (-EINVAL == status) {
		sii_log_debug("EINVAL %s%s%s\n", ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (status != 0) {
		sii_log_debug("status:%d buf:%c%s%c\n", status, '"', buf, '"');
	} else if (offset > 0xFFFF) {
		sii_log_debug("offset:0x%x buf:%c%s%c\n", (uint) offset, '"', buf, '"');
	} else {

		if (down_interruptible(&drv_context->isr_lock))
			return -ERESTARTSYS;
		if (drv_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			status = -ENODEV;
		} else {
			drv_context->debug_i2c_offset = offset;
			status = count;
		}
		up(&drv_context->isr_lock);
	}

	return status;
}

/*
 * show_reg_access_offset()	- Show the current page number to be used when
 *	reg_access/data is accessed.
 *
 */
ssize_t show_reg_access_offset(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sii70xx_drv_context *drv_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	sii_log_info("called\n");

	status = scnprintf(buf, PAGE_SIZE, "0x%02x\n", drv_context->debug_i2c_offset);

	return status;
}

/*
 * set_reg_access_length() - Handle write request to set the
 *		reg access page value.
 *
 * The format of the string in buf must be:
 *	<pageaddr>
 * Where: <pageaddr> specifies the reg page of the register(s)
 *			to be written/read
 */
ssize_t set_reg_access_length(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct sii70xx_drv_context *drv_context = dev_get_drvdata(dev);
	unsigned long length = 0x100;
	int status = -EINVAL;

	sii_log_info("received string: " "%s" "\n", buf);

	status = kstrtoul(buf, 0, &length);
	if (-ERANGE == status) {
		sii_log_debug("ERANGE %s%s%s\n", ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (-EINVAL == status) {
		sii_log_debug("EINVAL %s'%s'%s\n", ANSI_ESC_RED_TEXT, buf, ANSI_ESC_RESET_TEXT);
	} else if (status != 0) {
		sii_log_debug("status:%d buf:%c%s%c\n", status, '"', buf, '"');
	} else if (length > 0xFF) {
		sii_log_debug("length:0x%x buf:%c%s%c\n", (uint) length, '"', buf, '"');
	} else {

		if (down_interruptible(&drv_context->isr_lock))
			return -ERESTARTSYS;
		if (drv_context->dev_flags & DEV_FLAG_SHUTDOWN) {
			status = -ENODEV;
		} else {
			drv_context->debug_i2c_xfer_length = length;
			status = count;
		}
		up(&drv_context->isr_lock);
	}

	return status;
}

/*
 * show_reg_access_length()	- Show the current page number to be used when
 *	reg_access/data is accessed.
 *
 */
ssize_t show_reg_access_length(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sii70xx_drv_context *drv_context = dev_get_drvdata(dev);
	int status = -EINVAL;

	sii_log_info("called\n");

	status = scnprintf(buf, PAGE_SIZE, "0x%02x", drv_context->debug_i2c_xfer_length);

	return status;
}

/*
 * set_reg_access_data() - Handle write request to the
 *	reg_access_data attribute file.
 *
 * This file is used to either perform a write to registers of the transmitter
 * or to set the address, offset and byte count for a subsequent from the
 * register(s) of the transmitter.
 *
 * The format of the string in buf must be:
 *	data_byte_0 ... data_byte_length-1
 * Where:	data_byte is a space separated list of <length_value> data
 *		bytes to be written.  If no data bytes are present then
 *		the write to this file will only be used to set
 *		the  page address, offset and length for a
 *		subsequent read from this file.
 */
ssize_t set_reg_access_data(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct sii70xx_drv_context *drv_context = dev_get_drvdata(dev);
	unsigned long value;
	u8 data[MAX_DEBUG_TRANSFER_SIZE];
	int i;
	char *str;
	int status = -EINVAL;
	char *pinput = kmalloc(count, GFP_KERNEL);

	sii_log_info("received string: %c%s%c\n", '"', buf, '"');

	if (pinput == 0)
		return -EINVAL;
	memcpy(pinput, buf, count);

	str = pinput;
	for (i = 0; (i < MAX_DEBUG_TRANSFER_SIZE) && ('\0' != *str); i++) {

		status = si_strtoul(&str, 0, &value);
		if (-ERANGE == status) {
			sii_log_debug("ERANGE %s%s%s\n", ANSI_ESC_RED_TEXT, str,
				      ANSI_ESC_RESET_TEXT);
			goto exit_reg_access_data;
		} else if (-EINVAL == status) {
			sii_log_debug("EINVAL %s%s%s\n", ANSI_ESC_RED_TEXT, str,
				      ANSI_ESC_RESET_TEXT);
			goto exit_reg_access_data;
		} else if (status != 0) {
			sii_log_debug("status:%d %s%s%s\n", status,
				      ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_reg_access_data;
		} else if (value > 0xFF) {
			sii_log_debug("value:0x%x str:%s%s%s\n", (uint) value,
				      ANSI_ESC_RED_TEXT, str, ANSI_ESC_RESET_TEXT);
			goto exit_reg_access_data;
		} else {
			data[i] = value;
		}
	}

	if (i == 0) {
		sii_log_info("No data specified\n");
		goto exit_reg_access_data;
	}

	if (down_interruptible(&drv_context->isr_lock)) {
		status = -ERESTARTSYS;
		goto exit_reg_access_data;
	}

	if (drv_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
	} else {

		status = rpt_device_dbg_i2c_reg_xfer(drv_context,
						     drv_context->debug_i2c_offset, i,
						     DEBUG_I2C_WRITE, data);
		if (status == 0)
			status = count;
	}

	up(&drv_context->isr_lock);

exit_reg_access_data:kfree(pinput);
	return status;
}

/*
 * show_reg_access_data()	- Handle read request to the
 reg_access_data attribute file.
 *
 * Reads from this file return one or more transmitter register values in
 * hexadecimal string format.  The registers returned are specified by the
 * address, offset and length values previously written to this file.
 *
 * The return value is the number characters written to buf, or an error
 * code if the I2C read fails.
 *
 * The format of the string returned in buf is:
 * "address:<pageaddr> offset:<offset> length:<lenvalue> data:<datavalues>
 * where:	<pageaddr>  is the last I2C register page address written
 *				to this file
 *		<offset>    is the last register offset written to this file
 *		<lenvalue>  is the last register transfer length written
 *				to this file
 *		<datavalue> space separated list of <lenvalue> register
 *				values in OxXX format
 */
ssize_t show_reg_access_data(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sii70xx_drv_context *drv_context = dev_get_drvdata(dev);
	u8 data[MAX_DEBUG_TRANSFER_SIZE] = { 0 };
	u8 idx;
	int status = -EINVAL;



	if (down_interruptible(&drv_context->isr_lock))
		return -ERESTARTSYS;


	if (drv_context->dev_flags & DEV_FLAG_SHUTDOWN) {
		status = -ENODEV;
		goto no_dev;
	}

	pr_info("\ndrv_context->debug_i2c_offset:%x\n", drv_context->debug_i2c_offset);
	status = rpt_device_dbg_i2c_reg_xfer(drv_context,
					     drv_context->debug_i2c_offset,
					     drv_context->debug_i2c_xfer_length, DEBUG_I2C_READ,
					     data);
no_dev:up(&drv_context->isr_lock);

	if (status == 0) {
		status = scnprintf(buf, PAGE_SIZE, "0x%02x:", drv_context->debug_i2c_offset);

		for (idx = 0; idx < drv_context->debug_i2c_xfer_length; idx++)
			status += scnprintf(&buf[status], PAGE_SIZE, " 0x%02x", data[idx]);
		status += scnprintf(&buf[status], PAGE_SIZE, "\n");
	}

	return status;
}

static DEVICE_ATTR(send_get_src_cap, S_IWUSR, NULL, set_src_cap);
static DEVICE_ATTR(init_alt_mode, S_IWUSR, NULL, set_vdm_disc_identity);
static DEVICE_ATTR(exit_alt_mode, S_IWUSR, NULL, set_vdm_exit_mode);
static DEVICE_ATTR(send_pr_swap, S_IWUSR, NULL, set_pr_swap);
static DEVICE_ATTR(send_dr_swap, S_IWUSR, NULL, set_dr_swap);
static DEVICE_ATTR(send_vconn_swap, S_IWUSR, NULL, set_vconn_swap);
static DEVICE_ATTR(send_custom_message, S_IWUSR, NULL, set_custom_msg);

struct device_attribute *sii_driver_attribs[] = {
	&dev_attr_send_get_src_cap,
	&dev_attr_init_alt_mode,
	&dev_attr_exit_alt_mode,
	&dev_attr_send_pr_swap,
	&dev_attr_send_dr_swap,
	&dev_attr_send_vconn_swap,
	&dev_attr_send_custom_message,
	NULL
};


#ifdef NEVER
struct device_attribute driver_attribs[] = {
	__ATTR(SYS_ATTR_NAME_SRC_CAP, S_IWUSR,
	       NULL, set_src_cap),
	__ATTR(SYS_ATTR_NAME_VDM_DISC_IDENTITY, S_IWUSR,
	       NULL, set_vdm_disc_identity),
	__ATTR(SYS_ATTR_NAME_VDM_EXIT_MODE, S_IWUSR,
	       NULL, set_vdm_exit_mode),
	__ATTR(SYS_ATTR_NAME_SET_PR_SWAP, S_IWUSR,
	       NULL, set_pr_swap),
	__ATTR(SYS_ATTR_NAME_SET_DR_SWAP, S_IWUSR,
	       NULL, set_dr_swap),
	__ATTR(SYS_ATTR_NAME_SET_VCONN_SWAP, S_IWUSR,
	       NULL, set_vconn_swap),
	__ATTR(SYS_ATTR_NAME_SET_CUSTOM_MESSAGE, S_IWUSR,
	       NULL, set_custom_msg),
	__ATTR_NULL
};
#endif				/* NEVER */

#if defined(I2C_DBG_SYSFS)
struct device_attribute reg_access_page_attr =
__ATTR(SYS_ATTR_NAME_REG_ACCESS_PAGE, S_IWUSR | S_IRUSR,
show_reg_access_page, set_reg_access_page);

struct device_attribute reg_access_offset_attr =
__ATTR(SYS_ATTR_NAME_REG_ACCESS_OFFSET, S_IWUSR | S_IRUSR,
show_reg_access_offset, set_reg_access_offset);

struct device_attribute reg_access_length_attr =
__ATTR(SYS_ATTR_NAME_REG_ACCESS_LENGTH, S_IWUSR | S_IRUSR,
show_reg_access_length, set_reg_access_length);

struct device_attribute reg_access_data_attr =
__ATTR(SYS_ATTR_NAME_REG_ACCESS_DATA, S_IWUSR | S_IRUSR,
show_reg_access_data, set_reg_access_data);

struct attribute *reg_access_attrs[] = {
	&reg_access_page_attr.attr, &reg_access_offset_attr.attr,
	&reg_access_length_attr.attr, &reg_access_data_attr.attr, NULL
};

struct attribute_group reg_access_attribute_group = {
	.name = __stringify(SYS_OBJECT_NAME_REG_ACCESS), .attrs = reg_access_attrs
};
#endif
static const struct file_operations usbpd_fops = {
	.owner = THIS_MODULE
};

bool sii_drv_sysfs_init(struct sii70xx_drv_context *pdev, struct device *dev)
{
	int ret = 0;
	bool result = true;
	struct device_attribute **attrs = sii_driver_attribs;
	struct device_attribute *attr;

	pr_debug("\n%s: class created:\n", __func__);

	if (pdev->usbpd_class == NULL) {
		pdev->usbpd_class = class_create(THIS_MODULE, "usbpd");
		if (IS_ERR(pdev->usbpd_class)) {
			ret = PTR_ERR(pdev->usbpd_class);
			pr_debug("%s: class_create failed %d\n", __func__, ret);
			return false;
		}
		/* pdev->usbpd_class->dev_attrs = driver_attribs; */
		ret = alloc_chrdev_region(&pdev->dev_num, 0, 1, "sii70xx");
		if (ret) {
			pr_warn("%s: alloc_char_region failed\n", __func__);
			return false;
		}

		cdev_init(&pdev->usbpd_cdev, &usbpd_fops);
		pdev->usbpd_cdev.owner = THIS_MODULE;
		ret = cdev_add(&pdev->usbpd_cdev, MINOR(pdev->dev_num), 1);
		if (ret) {
			pr_warn("%s: cdev_add failed\n", __func__);
			return false;
		}
	}

	pdev->dev = device_create(pdev->usbpd_class, dev, pdev->dev_num, pdev, "%s", "sii70xx");

	if (IS_ERR(pdev->dev)) {
		ret = PTR_ERR(pdev->dev);
		pr_warn("%s: device_create failed %s %d\n", __func__, "sii70xx", ret);
		return false;
	}

	dev_set_drvdata(pdev->dev, pdev);

	while ((attr = *attrs++)) {
		ret = device_create_file(pdev->dev, attr);
		if (ret) {
			pr_warn("%s: device_create_file failed\n", __func__);
			device_destroy(pdev->usbpd_class, pdev->dev_num);
			return false;
		}
	}

#if defined(I2C_DBG_SYSFS)
	ret = sysfs_create_group(&pdev->dev->kobj, &reg_access_attribute_group);
	if (ret)
		pr_warn("Debug sysfs_create_group failed:%d\n", ret);
	sema_init(&pdev->isr_lock, 1);
	pdev->dev_flags &= DEV_FLAG_RESET;
#endif
	return result;
}

void sii_drv_sysfs_exit(struct sii70xx_drv_context *pdev)
{
	/*usbpd_sysfs_delete(pdev); */
	cdev_del(&pdev->usbpd_cdev);
	device_destroy(pdev->usbpd_class, pdev->dev_num);
	class_destroy(pdev->usbpd_class);
	unregister_chrdev_region(pdev->dev_num, SII_DRIVER_MINOR_MAX);
	pdev->dev_num = 0;
	pdev->usbpd_class = NULL;
}

void usbpd_event_notify(struct sii70xx_drv_context *pdev, unsigned int events,
			unsigned int event_param, void *data)
{

	char event_string[USBPD_EVENT_STRING_LEN];
	char *envp[2];

	envp[0] = event_string;
	envp[1] = NULL;

	/*pr_info("called, event: 0x%08x event_param: 0x%08x\n",
	   events, event_param); */

	switch (events) {
	case PD_UFP_ATTACHED:
		sysfs_notify(&pdev->dev->kobj, NULL, __stringify(SYS_ATTR_NAME_UFP_CONN));

		strlcpy(event_string, "UFP=attached", USBPD_EVENT_STRING_LEN);
		kobject_uevent_env(&pdev->dev->kobj, KOBJ_CHANGE, envp);
		break;
	case PD_UFP_DETACHED:
		sysfs_notify(&pdev->dev->kobj, NULL, __stringify(SYS_ATTR_NAME_UFP_CONN));

		strlcpy(event_string, "UFP=detached", USBPD_EVENT_STRING_LEN);
		kobject_uevent_env(&pdev->dev->kobj, KOBJ_CHANGE, envp);
		break;
	case PD_DFP_ATTACHED:
		sysfs_notify(&pdev->dev->kobj, NULL, __stringify(SYS_ATTR_NAME_DFP_CONN));

		strlcpy(event_string, "DFP=attached", USBPD_EVENT_STRING_LEN);
		kobject_uevent_env(&pdev->dev->kobj, KOBJ_CHANGE, envp);
		break;
	case PD_DFP_DETACHED:
		sysfs_notify(&pdev->dev->kobj, NULL, __stringify(SYS_ATTR_NAME_DFP_CONN));

		strlcpy(event_string, "DFP=detached", USBPD_EVENT_STRING_LEN);
		kobject_uevent_env(&pdev->dev->kobj, KOBJ_CHANGE, envp);
		break;
	case PD_POWER_LEVELS:
		sysfs_notify(&pdev->dev->kobj, NULL, __stringify(SYS_ATTR_NAME_POWER_LEVELS));

		strlcpy(event_string, "5V, 900mv", USBPD_EVENT_STRING_LEN);
		kobject_uevent_env(&pdev->dev->kobj, KOBJ_CHANGE, envp);
		break;
	case PD_UNSTRUCTURED_VDM:
		sysfs_notify(&pdev->dev->kobj, NULL,
			__stringify(SYS_ATTR_NAME_UNSTRUCTURED_VDM));

		strlcpy(event_string,
			"Unstructred VDM is set",
			USBPD_EVENT_STRING_LEN);
		kobject_uevent_env(&pdev->dev->kobj, KOBJ_CHANGE,
			envp);
		break;
	case PD_PR_SWAP_DONE:
		sysfs_notify(&pdev->dev->kobj, NULL,
			__stringify(SYS_ATTR_NAME_PR_SWAP_COMPLETED));

		strlcpy(event_string,
			"Power role swap completed",
			USBPD_EVENT_STRING_LEN);
		kobject_uevent_env(&pdev->dev->kobj, KOBJ_CHANGE,
			envp);
		break;
	case PD_DR_SWAP_DONE:
		sysfs_notify(&pdev->dev->kobj, NULL,
			__stringify(SYS_ATTR_NAME_DR_SWAP_COMPLETED));

		strlcpy(event_string,
			"Data role swap completed",
			USBPD_EVENT_STRING_LEN);
		kobject_uevent_env(&pdev->dev->kobj, KOBJ_CHANGE,
			envp);
		break;
	case PD_PR_SWAP_EXIT:
		sysfs_notify(&pdev->dev->kobj, NULL,
			__stringify(SYS_ATTR_NAME_PR_SWAP_EXIT));

		strlcpy(event_string,
			"Power role swap exited",
			USBPD_EVENT_STRING_LEN);
		kobject_uevent_env(&pdev->dev->kobj, KOBJ_CHANGE,
			envp);
		break;
	case PD_DR_SWAP_EXIT:
		sysfs_notify(&pdev->dev->kobj, NULL,
			__stringify(SYS_ATTR_NAME_DR_SWAP_EXIT));

		strlcpy(event_string,
			"Data role swap exited",
			USBPD_EVENT_STRING_LEN);
		kobject_uevent_env(&pdev->dev->kobj, KOBJ_CHANGE,
			envp);
		break;
	case PD_DFP_EXIT_MODE_DONE:
		sysfs_notify(&pdev->dev->kobj, NULL,
			__stringify(SYS_ATTR_NAME_DFP_ALT_MODE_EXIT));

		strlcpy(event_string,
			"DFP - Alt mode exit",
			USBPD_EVENT_STRING_LEN);
		kobject_uevent_env(&pdev->dev->kobj, KOBJ_CHANGE,
			envp);
		break;
	case PD_UFP_EXIT_MODE_DONE:
		sysfs_notify(&pdev->dev->kobj, NULL,
			__stringify(SYS_ATTR_NAME_UFP_ALT_MODE_EXIT));

		strlcpy(event_string,
			"UFP - Alt mode exit",
			USBPD_EVENT_STRING_LEN);
		kobject_uevent_env(&pdev->dev->kobj, KOBJ_CHANGE,
			envp);
		break;
	case PD_DFP_ENTER_MODE_DONE:
		sysfs_notify(&pdev->dev->kobj, NULL,
			__stringify(SYS_ATTR_NAME_DFP_ALT_MODE_DONE));

		strlcpy(event_string,
			"DFP - Alt mode completed",
			USBPD_EVENT_STRING_LEN);
		kobject_uevent_env(&pdev->dev->kobj, KOBJ_CHANGE,
			envp);
		break;
	case PD_UFP_ENTER_MODE_DONE:
		sysfs_notify(&pdev->dev->kobj, NULL,
			__stringify(SYS_ATTR_NAME_UFP_ALT_MODE_DONE));

		strlcpy(event_string,
			"UFP - Alt mode completed",
			USBPD_EVENT_STRING_LEN);
		kobject_uevent_env(&pdev->dev->kobj, KOBJ_CHANGE,
			envp);
		break;
	default:
		break;
	}
}
#endif

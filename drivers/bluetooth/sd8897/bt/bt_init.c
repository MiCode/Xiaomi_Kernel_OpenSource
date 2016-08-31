/** @file bt_init.c
  *
  * @brief This file contains the init functions for BlueTooth
  * driver.
  *
  * Copyright (C) 2011-2012, Marvell International Ltd.
  *
  * This software file (the "File") is distributed by Marvell International
  * Ltd. under the terms of the GNU General Public License Version 2, June 1991
  * (the "License").  You may use, redistribute and/or modify this File in
  * accordance with the terms and conditions of the License, a copy of which
  * is available along with the File in the gpl.txt file or by writing to
  * the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
  * 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
  *
  * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
  * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
  * this warranty disclaimer.
  *
  */

#include <linux/string.h>
#include <linux/firmware.h>

#include "bt_drv.h"

#define isxdigit(c)	(('0' <= (c) && (c) <= '9') \
			 || ('a' <= (c) && (c) <= 'f') \
			 || ('A' <= (c) && (c) <= 'F'))

#define isdigit(c)	(('0' <= (c) && (c) <= '9'))
#define isspace(c)  (c <= ' ' && (c == ' ' || (c <= 13 && c >= 9)))
/**
 *  @brief Returns hex value of a give character
 *
 *  @param chr	Character to be converted
 *
 *  @return	The converted character if chr is a valid hex, else 0
 */
static int
bt_hexval(char chr)
{
	ENTER();

	if (chr >= '0' && chr <= '9')
		return chr - '0';
	if (chr >= 'A' && chr <= 'F')
		return chr - 'A' + 10;
	if (chr >= 'a' && chr <= 'f')
		return chr - 'a' + 10;

	LEAVE();
	return 0;
}

/**
 *  @brief Extension of strsep lib command. This function will also take care
 *	   escape character
 *
 *  @param s         A pointer to array of chars to process
 *  @param delim     The delimiter character to end the string
 *  @param esc       The escape character to ignore for delimiter
 *
 *  @return          Pointer to the separated string if delim found, else NULL
 */
static char *
bt_strsep(char **s, char delim, char esc)
{
	char *se = *s, *sb;

	ENTER();

	if (!(*s) || (*se == '\0')) {
		LEAVE();
		return NULL;
	}

	for (sb = *s; *sb != '\0'; ++sb) {
		if (*sb == esc && *(sb + 1) == esc) {
			/*
			 * We get a esc + esc seq then keep the one esc
			 * and chop off the other esc character
			 */
			memmove(sb, sb + 1, strlen(sb));
			continue;
		}
		if (*sb == esc && *(sb + 1) == delim) {
			/*
			 * We get a delim + esc seq then keep the delim
			 * and chop off the esc character
			 */
			memmove(sb, sb + 1, strlen(sb));
			continue;
		}
		if (*sb == delim)
			break;
	}

	if (*sb == '\0')
		sb = NULL;
	else
		*sb++ = '\0';

	*s = sb;

	LEAVE();
	return se;
}

/**
 *  @brief Returns hex value of a given ascii string
 *
 *  @param a	String to be converted
 *
 *  @return	hex value
 */
static int
bt_atox(const char *a)
{
	int i = 0;
	ENTER();
	while (isxdigit(*a))
		i = i * 16 + bt_hexval(*a++);

	LEAVE();
	return i;
}

/**
 *  @brief Converts mac address from string to t_u8 buffer.
 *
 *  @param mac_addr The buffer to store the mac address in.
 *  @param buf      The source of mac address which is a string.
 *
 *  @return	N/A
 */
static void
bt_mac2u8(u8 * mac_addr, char *buf)
{
	char *begin = buf, *end;
	int i;

	ENTER();

	for (i = 0; i < ETH_ALEN; ++i) {
		end = bt_strsep(&begin, ':', '/');
		if (end)
			mac_addr[i] = bt_atox(end);
	}

	LEAVE();
}

/**
 *  @brief Returns integer value of a given ascii string
 *
 *  @param data    Converted data to be returned
 *  @param a       String to be converted
 *
 *  @return        BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
static int
bt_atoi(int *data, char *a)
{
	int i, val = 0, len;

	ENTER();

	len = strlen(a);
	if (!strncmp(a, "0x", 2)) {
		a = a + 2;
		len -= 2;
		*data = bt_atox(a);
		return BT_STATUS_SUCCESS;
	}
	for (i = 0; i < len; i++) {
		if (isdigit(a[i])) {
			val = val * 10 + (a[i] - '0');
		} else {
			PRINTM(ERROR, "Invalid char %c in string %s\n", a[i],
			       a);
			return BT_STATUS_FAILURE;
		}
	}
	*data = val;

	LEAVE();
	return BT_STATUS_SUCCESS;
}

/**
 *  @brief parse cal-data
 *
 *  @param src      a pointer to cal-data string
 *  @param len      len of cal-data
 *  @param dst      a pointer to return cal-data
 *  @param dst_size size of dest buffer
 *
 *  @return        BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
static int
bt_parse_cal_cfg(const u8 * src, u32 len, u8 * dst, u32 * dst_size)
{
	const u8 *ptr;
	u8 *dptr;
	u32 count = 0;
	int ret = BT_STATUS_FAILURE;

	ENTER();
	ptr = src;
	dptr = dst;

	while ((ptr - src) < len) {
		if (*ptr && isspace(*ptr)) {
			ptr++;
			continue;
		}

		if (isxdigit(*ptr)) {
			if ((dptr - dst) >= *dst_size) {
				PRINTM(ERROR, "cal_file size too big!!!\n");
				goto done;
			}
			*dptr++ = bt_atox(ptr);
			ptr += 2;
			count++;
		} else {
			ptr++;
		}
	}
	if (dptr == dst) {
		ret = BT_STATUS_FAILURE;
		goto done;
	}

	*dst_size = count;
	ret = BT_STATUS_SUCCESS;
done:
	LEAVE();
	return ret;
}

/**
 *    @brief BT get one line data from ASCII format data
 *
 *    @param data         Source data
 *    @param size         Source data length
 *    @param line_pos     Destination data
 *    @return             -1 or length of the line
 */
int
parse_cfg_get_line(u8 * data, u32 size, u8 * line_pos)
{
	static s32 pos;
	u8 *src, *dest;

	if (pos >= size) {	/* reach the end */
		pos = 0;	/* Reset position for rfkill */
		return -1;
	}
	memset(line_pos, 0, MAX_LINE_LEN);
	src = data + pos;
	dest = line_pos;

	while (*src != '\x0A' && *src != '\0') {
		if (*src != ' ' && *src != '\t')	/* parse space */
			*dest++ = *src++;
		else
			src++;
		pos++;
	}
	*dest = '\0';
	/* parse new line */
	pos++;
	return strlen(line_pos);
}

/**
 *    @brief BT parse ASCII format data to MAC address
 *
 *    @param priv          BT private handle
 *    @param data          Source data
 *    @param size          data length
 *    @return              BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_process_init_cfg(bt_private * priv, u8 * data, u32 size)
{
	u8 *pos;
	u8 *intf_s, *intf_e;
	u8 s[MAX_LINE_LEN];	/* 1 line data */
	u32 line_len;
	char dev_name[MAX_PARAM_LEN];
	u8 buf[MAX_PARAM_LEN];
	u8 bt_addr[MAX_MAC_ADDR_LEN];
	u8 bt_mac[ETH_ALEN];
	int setting = 0;
	u8 type = 0;
	u16 value = 0;
	u32 offset = 0;
	int ret = BT_STATUS_FAILURE;

	memset(dev_name, 0, sizeof(dev_name));
	memset(bt_addr, 0, sizeof(bt_addr));
	memset(bt_mac, 0, sizeof(bt_mac));

	while ((line_len = parse_cfg_get_line(data, size, s)) != -1) {
		pos = s;
		while (*pos == ' ' || *pos == '\t')
			pos++;

		if (*pos == '#' || (*pos == '\r' && *(pos + 1) == '\n') ||
		    *pos == '\n' || *pos == '\0')
			continue;	/* Need n't process this line */

		/* Process MAC addr */
		if (strncmp(pos, "mac_addr", 8) == 0) {
			intf_s = strchr(pos, '=');
			if (intf_s != NULL)
				intf_e = strchr(intf_s, ':');
			else
				intf_e = NULL;
			if (intf_s != NULL && intf_e != NULL) {
				if ((intf_e - intf_s) > MAX_PARAM_LEN) {
					PRINTM(ERROR,
					       "BT: Too long interface name %d\n",
					       __LINE__);
					goto done;
				}
				strncpy(dev_name, intf_s + 1,
					intf_e - intf_s - 1);
				dev_name[intf_e - intf_s - 1] = '\0';
				if (strcmp
				    (dev_name,
				     priv->bt_dev.m_dev[BT_SEQ].name) == 0) {
					/* found hci device */
					strncpy(bt_addr, intf_e + 1,
						MAX_MAC_ADDR_LEN - 1);
					bt_addr[MAX_MAC_ADDR_LEN - 1] = '\0';
					/* Convert MAC format */
					bt_mac2u8(bt_mac, bt_addr);
					PRINTM(CMD,
					       "HCI: %s new BT Address " MACSTR
					       "\n", dev_name, MAC2STR(bt_mac));
					if (BT_STATUS_SUCCESS !=
					    bt_set_mac_address(priv, bt_mac)) {
						PRINTM(FATAL,
						       "BT: Fail to set mac address\n");
						goto done;
					}
				}
			} else {
				PRINTM(ERROR,
				       "BT: Wrong config file format %d\n",
				       __LINE__);
				goto done;
			}
		}
		/* Process REG value */
		else if (strncmp(pos, "bt_reg", 6) == 0) {
			intf_s = strchr(pos, '=');
			if (intf_s != NULL)
				intf_e = strchr(intf_s, ',');
			else
				intf_e = NULL;
			if (intf_s != NULL && intf_e != NULL) {
				/* Copy type */
				memset(buf, 0, sizeof(buf));
				strncpy(buf, intf_s + 1, 1);
				buf[1] = '\0';
				if (0 == bt_atoi(&setting, buf))
					type = (u8) setting;
				else {
					PRINTM(ERROR,
					       "BT: Fail to parse reg type\n");
					goto done;
				}
			} else {
				PRINTM(ERROR,
				       "BT: Wrong config file format %d\n",
				       __LINE__);
				goto done;
			}
			intf_s = intf_e + 1;
			if (intf_s != NULL)
				intf_e = strchr(intf_s, ',');
			else
				intf_e = NULL;
			if (intf_s != NULL && intf_e != NULL) {
				if ((intf_e - intf_s) >= MAX_PARAM_LEN) {
					PRINTM(ERROR,
					       "BT: Regsier offset is too long %d\n",
					       __LINE__);
					goto done;
				}
				/* Copy offset */
				memset(buf, 0, sizeof(buf));
				strncpy(buf, intf_s, intf_e - intf_s);
				buf[intf_e - intf_s] = '\0';
				if (0 == bt_atoi(&setting, buf))
					offset = (u32) setting;
				else {
					PRINTM(ERROR,
					       "BT: Fail to parse reg offset\n");
					goto done;
				}
			} else {
				PRINTM(ERROR,
				       "BT: Wrong config file format %d\n",
				       __LINE__);
				goto done;
			}
			intf_s = intf_e + 1;
			if (intf_s != NULL) {
				if ((strlen(intf_s) >= MAX_PARAM_LEN)) {
					PRINTM(ERROR,
					       "BT: Regsier value is too long %d\n",
					       __LINE__);
					goto done;
				}
				/* Copy value */
				memset(buf, 0, sizeof(buf));
				strncpy(buf, intf_s, sizeof(buf));
				if (0 == bt_atoi(&setting, buf))
					value = (u16) setting;
				else {
					PRINTM(ERROR,
					       "BT: Fail to parse reg value\n");
					goto done;
				}
			} else {
				PRINTM(ERROR,
				       "BT: Wrong config file format %d\n",
				       __LINE__);
				goto done;
			}

			PRINTM(CMD,
			       "BT: Write reg type: %d offset: 0x%x value: 0x%x\n",
			       type, offset, value);
			if (BT_STATUS_SUCCESS !=
			    bt_write_reg(priv, type, offset, value)) {
				PRINTM(FATAL,
				       "BT: Write reg failed. type: %d offset: 0x%x value: 0x%x\n",
				       type, offset, value);
				goto done;
			}
		}
	}
	ret = BT_STATUS_SUCCESS;

done:
	LEAVE();
	return ret;
}

/**
 *    @brief BT set user defined init data and param
 *
 *    @param priv     BT private handle
 *    @param cfg_file user cofig file
 *    @return         BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_init_config(bt_private * priv, char *cfg_file)
{
	const struct firmware *cfg = NULL;
	int ret = BT_STATUS_SUCCESS;

	ENTER();
	if ((request_firmware(&cfg, cfg_file, priv->hotplug_device)) < 0) {
		PRINTM(FATAL, "BT: request_firmware() %s failed\n", cfg_file);
		ret = BT_STATUS_FAILURE;
		goto done;
	}
	if (cfg)
		ret = bt_process_init_cfg(priv, (u8 *) cfg->data, cfg->size);
	else
		ret = BT_STATUS_FAILURE;
done:
	if (cfg)
		release_firmware(cfg);
	LEAVE();
	return ret;
}

/**
 *    @brief BT process calibration data
 *
 *    @param priv    a pointer to bt_private structure
 *    @param data    a pointer to cal data
 *    @param size    cal data size
 *    @param mac     mac address buf
 *    @return         BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_process_cal_cfg(bt_private * priv, u8 * data, u32 size, char *mac)
{
	u8 bt_mac[ETH_ALEN];
	u8 cal_data[32];
	u8 *mac_data = NULL;
	u32 cal_data_len;
	int ret = BT_STATUS_FAILURE;

	memset(bt_mac, 0, sizeof(bt_mac));
	cal_data_len = sizeof(cal_data);
	if (BT_STATUS_SUCCESS !=
	    bt_parse_cal_cfg(data, size, cal_data, &cal_data_len)) {
		goto done;
	}
	if (mac != NULL) {
		/* Convert MAC format */
		bt_mac2u8(bt_mac, mac);
		PRINTM(CMD, "HCI: new BT Address " MACSTR "\n",
		       MAC2STR(bt_mac));
		mac_data = bt_mac;
	}
	if (BT_STATUS_SUCCESS != bt_load_cal_data(priv, cal_data, mac_data)) {
		PRINTM(FATAL, "BT: Fail to load calibrate data\n");
		goto done;
	}
	ret = BT_STATUS_SUCCESS;

done:
	LEAVE();
	return ret;
}

/**
 *    @brief BT process calibration EXT data
 *
 *    @param priv    a pointer to bt_private structure
 *    @param data    a pointer to cal data
 *    @param size    cal data size
 *    @param mac     mac address buf
 *    @return         BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_process_cal_cfg_ext(bt_private * priv, u8 * data, u32 size)
{
	u8 cal_data[128];
	u32 cal_data_len;
	int ret = BT_STATUS_FAILURE;

	cal_data_len = sizeof(cal_data);
	if (BT_STATUS_SUCCESS !=
	    bt_parse_cal_cfg(data, size, cal_data, &cal_data_len)) {
		goto done;
	}
	if (BT_STATUS_SUCCESS !=
	    bt_load_cal_data_ext(priv, cal_data, cal_data_len)) {
		PRINTM(FATAL, "BT: Fail to load calibrate data\n");
		goto done;
	}
	ret = BT_STATUS_SUCCESS;

done:
	LEAVE();
	return ret;
}

/**
 *    @brief BT process calibration file
 *
 *    @param priv    a pointer to bt_private structure
 *    @param cal_file calibration file name
 *    @param mac     mac address buf
 *    @return         BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_cal_config(bt_private * priv, char *cal_file, char *mac)
{
	const struct firmware *cfg = NULL;
	int ret = BT_STATUS_SUCCESS;

	ENTER();
	if ((request_firmware(&cfg, cal_file, priv->hotplug_device)) < 0) {
		PRINTM(FATAL, "BT: request_firmware() %s failed\n", cal_file);
		ret = BT_STATUS_FAILURE;
		goto done;
	}
	if (cfg)
		ret = bt_process_cal_cfg(priv, (u8 *) cfg->data, cfg->size,
					 mac);
	else
		ret = BT_STATUS_FAILURE;
done:
	if (cfg)
		release_firmware(cfg);
	LEAVE();
	return ret;
}

/**
 *    @brief BT process calibration EXT file
 *
 *    @param priv    a pointer to bt_private structure
 *    @param cal_file calibration file name
 *    @param mac     mac address buf
 *    @return         BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_cal_config_ext(bt_private * priv, char *cal_file)
{
	const struct firmware *cfg = NULL;
	int ret = BT_STATUS_SUCCESS;

	ENTER();
	if ((request_firmware(&cfg, cal_file, priv->hotplug_device)) < 0) {
		PRINTM(FATAL, "BT: request_firmware() %s failed\n", cal_file);
		ret = BT_STATUS_FAILURE;
		goto done;
	}
	if (cfg)
		ret = bt_process_cal_cfg_ext(priv, (u8 *) cfg->data, cfg->size);
	else
		ret = BT_STATUS_FAILURE;
done:
	if (cfg)
		release_firmware(cfg);
	LEAVE();
	return ret;
}

/**
 *    @brief BT init mac address from bt_mac parametre when insmod
 *
 *    @param priv    a pointer to bt_private structure
 *    @param bt_mac  mac address buf
 *    @return        BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_init_mac_address(bt_private * priv, char *mac)
{
	u8 bt_mac[ETH_ALEN];
	int ret = BT_STATUS_FAILURE;

	ENTER();
	memset(bt_mac, 0, sizeof(bt_mac));
	bt_mac2u8(bt_mac, mac);
	PRINTM(CMD, "HCI: New BT Address " MACSTR "\n", MAC2STR(bt_mac));
	ret = bt_set_mac_address(priv, bt_mac);
	if (ret != BT_STATUS_SUCCESS)
		PRINTM(FATAL,
		       "BT: Fail to set mac address from insmod parametre.\n");

	LEAVE();
	return ret;
}

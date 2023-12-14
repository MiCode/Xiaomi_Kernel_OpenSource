// SPDX-License-Identifier: ISC
/* Copyright (c) 2019, The Linux Foundation. All rights reserved. */

#include <linux/firmware.h>
#include <linux/ctype.h>
#include "wil6210.h"
#include "wil_platform.h"
#include "txrx_edma.h"
#include "config.h"

#define WIL_CONFIG_MAX_INI_ITEMS 256
#define WIL_CONFIG_INI_FILE "wigig.ini"

#define WIL_CONFIG_BOOL_MIN 0
#define WIL_CONFIG_BOOL_MAX 1
#define WIL_CONFIG_MAX_UNLIMITED UINT_MAX
#define WIL_CONFIG_BOOL_SIZE sizeof(u8)

/** struct wil_config_file_entry - ini configuration entry
 *
 * name: name of the entry
 * value: value of the entry
 */
struct wil_config_file_entry {
	char *name;
	char *value;
};

/* bool parameter */
#define WIL_CONFIG_COUNTRY_BOARD_FILE_NAME "country_specific_board_file"

/* bool parameter */
#define WIL_CONFIG_REG_HINTS_NAME "ignore_reg_hints"

#define WIL_CONFIG_SCAN_DWELL_TIME_NAME "scan_dwell_time"

#define WIL_CONFIG_SCAN_TIMEOUT_NAME "scan_timeout"

/* bool parameter */
#define WIL_CONFIG_DEBUG_FW_NAME "debug_fw"

#define WIL_CONFIG_OOB_MODE_NAME "oob_mode"
#define WIL_CONFIG_OOB_MODE_MIN 0
#define WIL_CONFIG_OOB_MODE_MAX 2

/* bool parameter */
#define WIL_CONFIG_NO_FW_RECOVERY_NAME "no_fw_recovery"

#define WIL_CONFIG_MTU_MAX_NAME "mtu_max"
#define WIL_CONFIG_MTU_MAX_MIN 68
#define WIL_CONFIG_MTU_MAX_MAX WIL_MAX_ETH_MTU

/* bool parameter */
#define WIL_CONFIG_RX_ALIGN_2_NAME "align_ip_header"

/* bool parameter */
#define WIL_CONFIG_RX_LARGE_BUF_NAME "rx_large_buf"

#define WIL_CONFIG_HEADROOM_SIZE_NAME "skb_headroom_size"
#define WIL_CONFIG_HEADROOM_SIZE_MIN 0
#define WIL_CONFIG_HEADROOM_SIZE_MAX WIL6210_MAX_HEADROOM_SIZE

#define WIL_CONFIG_BCAST_MCS0_LIMIT_NAME "bcast_mcs0_limit"
#define WIL_CONFIG_BCAST_MCS0_LIMIT_MIN 0
#define WIL_CONFIG_BCAST_MCS0_LIMIT_MAX WIL_BCAST_MCS0_LIMIT

#define WIL_CONFIG_BCAST_MCS_NAME "bcast_mcs"
#define WIL_CONFIG_BCAST_MCS_MIN 1
#define WIL_CONFIG_BCAST_MCS_MAX WIL_MCS_MAX

#define WIL_CONFIG_N_MSI_NAME "num_of_msi"
#define WIL_CONFIG_N_MSI_MIN 0
#define WIL_CONFIG_N_MSI_MAX 3

/* bool parameter */
#define WIL_CONFIG_FTM_MODE_NAME "factory_test_mode"

#define WIL_CONFIG_MAX_ASSOC_STA_NAME "max_assoc_sta"
#define WIL_CONFIG_MAX_ASSOC_STA_MIN 1
#define WIL_CONFIG_MAX_ASSOC_STA_MAX WIL6210_MAX_CID

#define WIL_CONFIG_AGG_WSIZE_NAME "block_ack_window_size"
#define WIL_CONFIG_AGG_WSIZE_MIN -1
#define WIL_CONFIG_AGG_WSIZE_MAX 64

/* bool parameter */
#define WIL_CONFIG_AC_QUEUES_NAME "ac_queues"

/* bool parameter */
#define WIL_CONFIG_Q_PER_STA_NAME "q_per_sta"

/* bool parameter */
#define WIL_CONFIG_DROP_IF_FULL_NAME "drop_if_ring_full"

#define WIL_CONFIG_RX_RING_ORDER_NAME "rx_ring_order"

#define WIL_CONFIG_TX_RING_ORDER_NAME "tx_ring_order"

#define WIL_CONFIG_BCAST_RING_ORDER_NAME "bcast_ring_order"

/* configuration for debug fs paramaeters */
#define WIL_CONFIG_DISCOVERY_MODE_NAME "discovery_scan"
#define WIL_CONFIG_DISCOVERY_MODE_MIN 0
#define WIL_CONFIG_DISCOVERY_MODE_MAX 1

#define WIL_CONFIG_ABFT_LEN_NAME "abft_len"
#define WIL_CONFIG_ABFT_LEN_MIN 0
#define WIL_CONFIG_ABFT_LEN_MAX 255

#define WIL_CONFIG_WAKEUP_TRIGGER_NAME "wakeup_trigger"
#define WIL_CONFIG_WAKEUP_TRIGGER_MIN 0
#define WIL_CONFIG_WAKEUP_TRIGGER_MAX WMI_WAKEUP_TRIGGER_BCAST

#define WIL_CONFIG_RX_STATUS_ORDER_NAME "rx_status_ring_order"

#define WIL_CONFIG_TX_STATUS_ORDER_NAME "tx_status_ring_order"

#define WIL_CONFIG_RX_BUFF_COUNT_NAME "rx_buff_count"

/* bool parameter */
#define WIL_CONFIG_AMSDU_EN_NAME "amsdu_en"

#define WIL_CONFIG_BOARD_FILE_NAME "board_file"

#define WIL_CONFIG_SNR_THRESH_NAME "snr_thresh"

#define WIL_CONFIG_FTM_OFFSET_NAME "ftm_offset"

#define WIL_CONFIG_TT_NAME "thermal_throttling"

#define WIL_CONFIG_LED_BLINK_NAME "led_blink"

#define WIL_CONFIG_VR_PROFILE_NAME "vr_profile"

#define WIL_CONFIG_LED_ID_NAME "led_id"
#define WIL_CONFIG_LED_ID_MIN 0
#define WIL_CONFIG_LED_ID_MAX 0xF

#define WIL_CONFIG_LED_BLINK_TIME_NAME "led_blink_time"

#define WIL_CONFIG_PS_PROFILE_NAME "ps_profile"
#define WIL_CONFIG_PS_PROFILE_MIN WMI_PS_PROFILE_TYPE_DEFAULT
#define WIL_CONFIG_PS_PROFILE_MAX WMI_PS_PROFILE_TYPE_LOW_LATENCY_PS

/* bool parameter */
#define WIL_CONFIG_ARC_ENABLE_NAME "config_arc_enable"
#define WIL_CONFIG_ARC_MONITORING_PERIOD_NAME "config_arc_monitoring_period"
#define WIL_CONFIG_ARC_RATE_LIMIT_FRAC_NAME "config_arc_rate_frac"

static int wil_board_file_handler(struct wil6210_priv *wil, const char *buf,
				  size_t count);
static int wil_snr_thresh_handler(struct wil6210_priv *wil, const char *buf,
				  size_t count);
static int wil_ftm_offset_handler(struct wil6210_priv *wil, const char *buf,
				  size_t count);
static int wil_tt_handler(struct wil6210_priv *wil, const char *buf,
			  size_t count);
static int wil_led_blink_handler(struct wil6210_priv *wil, const char *buf,
				 size_t count);

static struct wil_config_entry config_table[] = {
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_COUNTRY_BOARD_FILE_NAME,
			     wil_ini_param_type_unsigned,
			     &country_specific_board_file, 0,
			     WIL_CONFIG_BOOL_SIZE, WIL_CONFIG_BOOL_MIN,
			     WIL_CONFIG_BOOL_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_REG_HINTS_NAME,
			     wil_ini_param_type_unsigned, &ignore_reg_hints, 0,
			     WIL_CONFIG_BOOL_SIZE, WIL_CONFIG_BOOL_MIN,
			     WIL_CONFIG_BOOL_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_DEBUG_FW_NAME,
			     wil_ini_param_type_unsigned, &debug_fw, 0,
			     WIL_CONFIG_BOOL_SIZE, WIL_CONFIG_BOOL_MIN,
			     WIL_CONFIG_BOOL_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_OOB_MODE_NAME,
			     wil_ini_param_type_unsigned, &oob_mode, 0,
			     sizeof(oob_mode), WIL_CONFIG_OOB_MODE_MIN,
			     WIL_CONFIG_OOB_MODE_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_NO_FW_RECOVERY_NAME,
			     wil_ini_param_type_unsigned, &no_fw_recovery, 0,
			     WIL_CONFIG_BOOL_SIZE, WIL_CONFIG_BOOL_MIN,
			     WIL_CONFIG_BOOL_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_MTU_MAX_NAME,
			     wil_ini_param_type_unsigned, &mtu_max, 0,
			     sizeof(mtu_max), WIL_CONFIG_MTU_MAX_MIN,
			     WIL_CONFIG_MTU_MAX_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_RX_ALIGN_2_NAME,
			     wil_ini_param_type_unsigned, &rx_align_2, 0,
			     WIL_CONFIG_BOOL_SIZE, WIL_CONFIG_BOOL_MIN,
			     WIL_CONFIG_BOOL_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_RX_LARGE_BUF_NAME,
			     wil_ini_param_type_unsigned, &rx_large_buf, 0,
			     WIL_CONFIG_BOOL_SIZE, WIL_CONFIG_BOOL_MIN,
			     WIL_CONFIG_BOOL_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_HEADROOM_SIZE_NAME,
			     wil_ini_param_type_unsigned, &headroom_size, 0,
			     sizeof(headroom_size),
			     WIL_CONFIG_HEADROOM_SIZE_MIN,
			     WIL_CONFIG_HEADROOM_SIZE_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_N_MSI_NAME,
			     wil_ini_param_type_unsigned, &n_msi, 0,
			     sizeof(n_msi), WIL_CONFIG_N_MSI_MIN,
			     WIL_CONFIG_N_MSI_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_FTM_MODE_NAME,
			     wil_ini_param_type_unsigned, &ftm_mode, 0,
			     WIL_CONFIG_BOOL_SIZE, WIL_CONFIG_BOOL_MIN,
			     WIL_CONFIG_BOOL_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_MAX_ASSOC_STA_NAME,
			     wil_ini_param_type_unsigned, &max_assoc_sta, 0,
			     sizeof(max_assoc_sta),
			     WIL_CONFIG_MAX_ASSOC_STA_MIN,
			     WIL_CONFIG_MAX_ASSOC_STA_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_AGG_WSIZE_NAME,
			     wil_ini_param_type_signed, &agg_wsize, 0,
			     sizeof(agg_wsize), WIL_CONFIG_AGG_WSIZE_MIN,
			     WIL_CONFIG_AGG_WSIZE_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_AC_QUEUES_NAME,
			     wil_ini_param_type_unsigned, &ac_queues, 0,
			     WIL_CONFIG_BOOL_SIZE, WIL_CONFIG_BOOL_MIN,
			     WIL_CONFIG_BOOL_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_DROP_IF_FULL_NAME,
			     wil_ini_param_type_unsigned, &drop_if_ring_full,
			     0, WIL_CONFIG_BOOL_SIZE, WIL_CONFIG_BOOL_MIN,
			     WIL_CONFIG_BOOL_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_RX_RING_ORDER_NAME,
			     wil_ini_param_type_unsigned, &rx_ring_order, 0,
			     sizeof(rx_ring_order), WIL_RING_SIZE_ORDER_MIN,
			     WIL_RING_SIZE_ORDER_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_TX_RING_ORDER_NAME,
			     wil_ini_param_type_unsigned, &tx_ring_order, 0,
			     sizeof(tx_ring_order), WIL_RING_SIZE_ORDER_MIN,
			     WIL_RING_SIZE_ORDER_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_BCAST_RING_ORDER_NAME,
			     wil_ini_param_type_unsigned, &bcast_ring_order, 0,
			     sizeof(bcast_ring_order), WIL_RING_SIZE_ORDER_MIN,
			     WIL_RING_SIZE_ORDER_MAX),

	/* wil6210_priv fields */
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_DISCOVERY_MODE_NAME,
			     wil_ini_param_type_unsigned, NULL,
			     WIL_CONFIG_VAR_OFFSET(struct wil6210_priv,
						   discovery_mode),
			     WIL_CONFIG_VAR_SIZE(struct wil6210_priv,
						 discovery_mode),
			     WIL_CONFIG_DISCOVERY_MODE_MIN,
			     WIL_CONFIG_DISCOVERY_MODE_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_ABFT_LEN_NAME,
			     wil_ini_param_type_unsigned, NULL,
			     WIL_CONFIG_VAR_OFFSET(struct wil6210_priv,
						   abft_len),
			     WIL_CONFIG_VAR_SIZE(struct wil6210_priv,
						 abft_len),
			     WIL_CONFIG_ABFT_LEN_MIN,
			     WIL_CONFIG_ABFT_LEN_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_WAKEUP_TRIGGER_NAME,
			     wil_ini_param_type_unsigned, NULL,
			     WIL_CONFIG_VAR_OFFSET(struct wil6210_priv,
						   wakeup_trigger),
			     WIL_CONFIG_VAR_SIZE(struct wil6210_priv,
						 wakeup_trigger),
			     WIL_CONFIG_WAKEUP_TRIGGER_MIN,
			     WIL_CONFIG_WAKEUP_TRIGGER_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_RX_STATUS_ORDER_NAME,
			     wil_ini_param_type_unsigned, NULL,
			     WIL_CONFIG_VAR_OFFSET(struct wil6210_priv,
						   rx_status_ring_order),
			     WIL_CONFIG_VAR_SIZE(struct wil6210_priv,
						 rx_status_ring_order),
			     WIL_SRING_SIZE_ORDER_MIN,
			     WIL_SRING_SIZE_ORDER_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_TX_STATUS_ORDER_NAME,
			     wil_ini_param_type_unsigned, NULL,
			     WIL_CONFIG_VAR_OFFSET(struct wil6210_priv,
						   tx_status_ring_order),
			     WIL_CONFIG_VAR_SIZE(struct wil6210_priv,
						 tx_status_ring_order),
			     WIL_SRING_SIZE_ORDER_MIN,
			     WIL_SRING_SIZE_ORDER_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_RX_BUFF_COUNT_NAME,
			     wil_ini_param_type_unsigned, NULL,
			     WIL_CONFIG_VAR_OFFSET(struct wil6210_priv,
						   rx_buff_id_count),
			     WIL_CONFIG_VAR_SIZE(struct wil6210_priv,
						 rx_buff_id_count),
			     0, WIL_CONFIG_MAX_UNLIMITED),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_AMSDU_EN_NAME,
			     wil_ini_param_type_unsigned, NULL,
			     WIL_CONFIG_VAR_OFFSET(struct wil6210_priv,
						   amsdu_en),
			     WIL_CONFIG_BOOL_SIZE, WIL_CONFIG_BOOL_MIN,
			     WIL_CONFIG_BOOL_MAX),
	WIL_CONFIG_INI_PARAM_WITH_HANDLER(WIL_CONFIG_BOARD_FILE_NAME,
					  wil_board_file_handler),
	WIL_CONFIG_INI_PARAM_WITH_HANDLER(WIL_CONFIG_SNR_THRESH_NAME,
					  wil_snr_thresh_handler),
	WIL_CONFIG_INI_PARAM_WITH_HANDLER(WIL_CONFIG_FTM_OFFSET_NAME,
					  wil_ftm_offset_handler),
	WIL_CONFIG_INI_PARAM_WITH_HANDLER(WIL_CONFIG_TT_NAME,
					  wil_tt_handler),
	WIL_CONFIG_INI_PARAM_WITH_HANDLER(WIL_CONFIG_LED_BLINK_NAME,
					  wil_led_blink_handler),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_LED_ID_NAME,
			     wil_ini_param_type_unsigned, &led_id, 0,
			     sizeof(led_id), WIL_CONFIG_LED_ID_MIN,
			     WIL_CONFIG_LED_ID_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_VR_PROFILE_NAME,
			     wil_ini_param_type_unsigned, NULL,
			     WIL_CONFIG_VAR_OFFSET(struct wil6210_priv,
						   vr_profile),
			     WIL_CONFIG_VAR_SIZE(struct wil6210_priv,
						 vr_profile),
			     0, U8_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_PS_PROFILE_NAME,
			     wil_ini_param_type_unsigned, NULL,
			     WIL_CONFIG_VAR_OFFSET(struct wil6210_priv,
						   ps_profile),
			     WIL_CONFIG_VAR_SIZE(struct wil6210_priv,
						 ps_profile),
			     WIL_CONFIG_PS_PROFILE_MIN,
			     WIL_CONFIG_PS_PROFILE_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_ARC_ENABLE_NAME,
			     wil_ini_param_type_unsigned, NULL,
			     WIL_CONFIG_VAR_OFFSET(struct wil6210_priv,
						   config_arc_enable),
			     WIL_CONFIG_BOOL_SIZE,
			     WIL_CONFIG_BOOL_MIN,
			     WIL_CONFIG_BOOL_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_ARC_MONITORING_PERIOD_NAME,
			     wil_ini_param_type_unsigned, NULL,
			     WIL_CONFIG_VAR_OFFSET(struct wil6210_priv,
						   config_arc_monitoring_period),
			     WIL_CONFIG_VAR_SIZE(struct wil6210_priv,
						 config_arc_monitoring_period),
			     0, U16_MAX),
	WIL_CONFIG_INI_PARAM(WIL_CONFIG_ARC_RATE_LIMIT_FRAC_NAME,
			     wil_ini_param_type_unsigned, NULL,
			     WIL_CONFIG_VAR_OFFSET(struct wil6210_priv,
						   config_arc_rate_limit_frac),
			     WIL_CONFIG_VAR_SIZE(struct wil6210_priv,
						 config_arc_rate_limit_frac),
			     0, U16_MAX),

};

/**
 * find and locate the new line pointer
 *
 * str: pointer to string
 *
 * This function returns a pointer to the character after the
 * occurrence of a new line character. It also modifies the
 * original string by replacing the '\n' character with the null
 * character.
 *
 * Return: the pointer to the character at new line,
 *            or NULL if no new line character was found
 */
static char *get_next_line(char *str)
{
	char c;

	if (!str || *str == '\0')
		return NULL;

	c = *str;
	while (c != '\n' && c != '\0' && c != 0xd) {
		str = str + 1;
		c = *str;
	}

	if (c == '\0')
		return NULL;

	*str = '\0';
	return str + 1;
}

/**
 * trim - Removes leading whitespace from buffer.
 * s: The string to be stripped.
 *
 * Return pointer to the first non-whitespace character in
 * buffer s.
 */
static char *trim(char *s)
{
	while (isspace(*s))
		s++;

	return s;
}

/**
 * find the configuration item
 * file_ini_table: pointer to configuration table
 * entries: number of configuration entries
 * name: the interested configuration to find
 * value: the value to read back
 *
 * Return: 0 if the interested configuration is found
 */
static int find_cfg_item(struct wil6210_priv *wil,
			 struct wil_config_file_entry *file_ini_table,
			 u32 entries, char *name, char **value)
{
	u32 i;

	for (i = 0; i < entries; i++) {
		if (strcmp(file_ini_table[i].name, name) == 0) {
			*value = file_ini_table[i].value;
			wil_dbg_misc(wil, "parameter=[%s] value=[%s]\n",
				     name, *value);
			return 0;
		}
	}

	return -ENODATA;
}

static int cfg_item_read_unsigned(struct wil6210_priv *wil, void *field,
				  struct wil_config_entry *entry,
				  char *value_str)
{
	u32 value;
	int rc;
	size_t len_value_str = strlen(value_str);

	if (len_value_str > 2 && value_str[0] == '0' && value_str[1] == 'x')
		/* param value is in hex format */
		rc = kstrtou32(value_str, 16, &value);
	else
		/* param value is in dec format */
		rc = kstrtou32(value_str, 10, &value);

	if (rc < 0) {
		wil_err(wil, "Parameter %s invalid\n", entry->name);
		return -EINVAL;
	}

	/* Do the range check here for the parameter converted int value,
	 * overwrite if exceeds from range with min and max values.
	 */
	if (value > entry->max_val) {
		wil_dbg_misc(wil,
			     "Parameter %s > allowed Maximum [%u > %lu]. Enforcing maximum\n",
			     entry->name, value, entry->max_val);
		value = entry->max_val;
	}

	if (value < entry->min_val) {
		wil_dbg_misc(wil,
			     "Parameter %s < allowed Minimum [%u < %lu]. Enforcing Minimum\n",
			     entry->name, value, entry->min_val);
		value = entry->min_val;
	}

	/* Move the variable into the output field */
	memcpy(field, &value, entry->var_size);
	wil_dbg_misc(wil, "Parameter %s: Value: %u\n", entry->name, value);

	return 0;
}

static int cfg_item_read_signed(struct wil6210_priv *wil, void *field,
				struct wil_config_entry *entry,
				char *value_str)
{
	int value, rc;

	/* convert the value string to a signed integer value */
	rc = kstrtos32(value_str, 10, &value);
	if (rc < 0) {
		wil_err(wil, "Parameter %s invalid\n", entry->name);
		return -EINVAL;
	}

	/* Do the range check here for the parameter converted int value,
	 * overwrite if exceeds from range with min and max values.
	 */
	if (value > (int)entry->max_val) {
		wil_dbg_misc(wil,
			     "Parameter %s > allowed Maximum [%d > %d]. Enforcing Maximum\n",
			     entry->name, value, (int)entry->max_val);
		value = (int32_t)entry->max_val;
	}

	if (value < (int)entry->min_val) {
		wil_dbg_misc(wil,
			     "Parameter %s < allowed Minimum [%d < %d]. Enforcing Minimum\n",
			     entry->name, value, (int)entry->min_val);
		value = (int)entry->min_val;
	}

	/* Move the variable into the output field */
	memcpy(field, &value, entry->var_size);

	return 0;
}

static int cfg_item_read_string(struct wil6210_priv *wil, void *field,
				struct wil_config_entry *entry,
				char *value_str)
{
	int len_value_str;

	wil_dbg_misc(wil, "name = %s, var_size %u\n", entry->name,
		     entry->var_size);

	len_value_str = strlen(value_str);
	if (entry->handler)
		return entry->handler(wil, value_str, len_value_str);

	if (len_value_str > (entry->var_size - 1)) {
		wil_err(wil, "too long value=[%s] specified for param=[%s]\n",
			value_str, entry->name);
		return -EINVAL;
	}

	memcpy(field, (void *)(value_str), len_value_str);
	((u8 *)field)[len_value_str] = '\0';

	return 0;
}

static int cfg_item_read_mac(struct wil6210_priv *wil, void *field,
			     struct wil_config_entry *entry, char *buffer)
{
	int len, rc;
	char mac[ETH_ALEN];
	char *c;
	int mac_len = 0;
	s8 value;

	if (entry->var_size != ETH_ALEN) {
		wil_err(wil, "Invalid var_size %u for name=[%s]\n",
			entry->var_size, entry->name);
		return -EINVAL;
	}

	len = strlen(buffer);
	/* mac format is as the following mac=00:11:22:33:44:55 */
	if (len != (ETH_ALEN * 2 + 5)) { /* 5 ':' separators */
		wil_err(wil, "Invalid MAC addr [%s] specified for name=[%s]\n",
			buffer, entry->name);
		return -EINVAL;
	}

	/* parse the string and store it in the byte array */
	for (c = buffer; (c[2] == ':' || c[2] == '\0') && mac_len < ETH_ALEN;
	     c += 2) {
		c[2] = '\0';
		rc = kstrtos8(c, 16, &value);
		if (rc < 0) {
			wil_err(wil, "kstrtos8 failed with status %d\n", rc);
			return rc;
		}
		mac[mac_len++] = value;
	}

	if (mac_len < ETH_ALEN) {
		wil_err(wil, "failed to read MAC addr [%s] specified for name=[%s]\n",
			buffer, entry->name);
		return -EINVAL;
	}

	memcpy(field, (void *)(mac), ETH_ALEN);

	return 0;
}

/**
 * apply the ini configuration file
 *
 * wil: the pointer to wil6210 context
 * file_ini_table: pointer to configuration table
 * file_entries: number fo the configuration entries
 *
 * Return: 0 if the ini configuration file is correctly parsed
 */
static int wil_apply_cfg_ini(struct wil6210_priv *wil,
			     struct wil_config_file_entry *file_ini_table,
			     u32 file_entries)
{
	int match;
	unsigned int idx;
	void *field;
	char *val;
	struct wil_config_entry *entry = config_table;
	u32 table_entries = ARRAY_SIZE(config_table);
	int rc;

	for (idx = 0; idx < table_entries; idx++, entry++) {
		/* Calculate the address of the destination field in the
		 * structure.
		 */
		if (entry->var_ref)
			field = entry->var_ref;
		else
			field = (void *)wil + entry->var_offset;

		match = find_cfg_item(wil, file_ini_table, file_entries,
				      entry->name, &val);

		if (match != 0)
			/* keep the default value */
			continue;

		switch (entry->type) {
		case wil_ini_param_type_unsigned:
			rc = cfg_item_read_unsigned(wil, field, entry, val);
			break;
		case wil_ini_param_type_signed:
			rc = cfg_item_read_signed(wil, field, entry, val);
			break;
		case wil_ini_param_type_string:
			rc = cfg_item_read_string(wil, field, entry, val);
			break;
		case wil_ini_param_type_macaddr:
			rc = cfg_item_read_mac(wil, field, entry, val);
			break;
		default:
			wil_err(wil, "Unknown param type [%d] for name [%s]\n",
				entry->type, entry->name);
			return -EINVAL;
		}
		if (rc)
			return rc;
	}

	return 0;
}

/**
 * parse the ini configuration file
 *
 * This function reads the wigig.ini file and
 * parses each 'Name=Value' pair in the ini file
 *
 * Return: 0 if the wigig.ini is correctly read,
 *	   fail code otherwise
 */
int wil_parse_config_ini(struct wil6210_priv *wil)
{
	int rc, i = 0, line_index = 0;
	const struct firmware *fw = NULL;
	const char *start;
	char *buffer, *line;
	char *name, *value;
	/* will hold list of strings [param, value] */
	struct wil_config_file_entry *cfg_ini_table;
	size_t cfg_size = sizeof(struct wil_config_file_entry) *
		WIL_CONFIG_MAX_INI_ITEMS;

	rc = request_firmware(&fw, WIL_CONFIG_INI_FILE, wil_to_dev(wil));
	if (rc) {
		wil_dbg_misc(wil, "Couldn't load configuration %s rc %d\n",
			     WIL_CONFIG_INI_FILE, rc);
		return rc;
	}
	if (!fw || !fw->data || !fw->size) {
		wil_err(wil, "%s no data\n", WIL_CONFIG_INI_FILE);
		rc = -ENOMEM;
		goto release_fw;
	}

	wil_dbg_misc(wil, "Parsing <%s>, %zu bytes\n", WIL_CONFIG_INI_FILE,
		     fw->size);

	/* save the start of the buffer */
	buffer = kmemdup(fw->data, fw->size, GFP_KERNEL);
	if (!buffer) {
		rc = -ENOMEM;
		goto release_fw;
	}
	start = buffer;

	cfg_ini_table = kzalloc(cfg_size, GFP_KERNEL);
	if (!cfg_ini_table) {
		kfree(buffer);
		rc = -ENOMEM;
		goto release_fw;
	}

	while (buffer && buffer - start < fw->size) {
		line_index++;
		line = get_next_line(buffer);
		buffer = trim(buffer);

		if (strlen((char *)buffer) == 0 || *buffer == '#') {
			buffer = line;
			continue;
		} else if (memcmp(buffer, "END", 3) == 0) {
			break;
		}

		/* parse new line */
		name = strsep(&buffer, "=");
		if (!name || !buffer) {
			wil_err(wil, "file parse error at line %d. expecting '='\n",
				line_index);
			rc = -EINVAL;
			goto out;
		}

		/* get the value [string] */
		value = trim(buffer);
		if (strlen(value) == 0) {
			wil_err(wil, "file parse error, no value for param %s at line %d\n",
				name, line_index);
			rc = -EINVAL;
			goto out;
		}

		cfg_ini_table[i].name = name;
		cfg_ini_table[i++].value = value;

		if (i > WIL_CONFIG_MAX_INI_ITEMS) {
			wil_err(wil, "too many items in %s (%d) > max items (%d)\n",
				WIL_CONFIG_INI_FILE, i,
				WIL_CONFIG_MAX_INI_ITEMS);
			break;
		}

		buffer = line;
	}

	/* Loop through the parsed config table and apply all these configs */
	rc = wil_apply_cfg_ini(wil, cfg_ini_table, i);

out:
	kfree(start);
	kfree(cfg_ini_table);
release_fw:
	release_firmware(fw);

	return rc;
}

static int wil_board_file_handler(struct wil6210_priv *wil, const char *buf,
				  size_t count)
{
	return wil_board_file_set(wil, buf, count);
}

static int wil_snr_thresh_handler(struct wil6210_priv *wil, const char *buf,
				  size_t count)
{
	return wil_snr_thresh_set(wil, buf);
}

static int wil_ftm_offset_handler(struct wil6210_priv *wil, const char *buf,
				  size_t count)
{
	return wil_ftm_offset_set(wil, buf);
}

static int wil_tt_handler(struct wil6210_priv *wil, const char *buf,
			  size_t count)
{
	return wil_tt_set(wil, buf, count);
}

static int wil_led_blink_handler(struct wil6210_priv *wil, const char *buf,
				 size_t count)
{
	return wil_led_blink_set(wil, buf);
}


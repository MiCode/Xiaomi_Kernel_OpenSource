/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
 * Copyright (c) 2008, Tungsten Graphics, Inc. Cedar Park, TX., USA.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 **************************************************************************/
#include <linux/gpio.h>
#include <asm/intel_scu_pmic.h>
#include <asm/intel-mid.h>
#include <video/adf_client.h>

#include "mofd_hdcp_reg.h"
#include "../hdmi_hdcp.h"

static void hdcp_off(void);
static uint32_t hdcp_get_status(void);
static void hdcp_read_an(uint8_t *an);
static void hdcp_capture_an(void);
static bool hdcp_is_an_ready(void);
static void hdcp_enable_port(bool enable);
static void hdcp_set_config(int val);
static int hdcp_get_config(void);
static void hdcp_write_rx_ri(uint16_t rx_ri);
static bool hdcp_is_hdcp_on(void);
static bool hdcp_config_is_encrypting(void);
static uint8_t hdcp_get_repeater_status(void);
static bool hdcp_repeater_rdy_for_nxt_data(void);
static bool hdcp_repeater_wait_for_next_data(void);
static void hdcp_set_repeater_control(int value);
static bool hdcp_repeater_is_idle(void);
static bool hdcp_repeater_wait_for_idle(void);

/**
 * Description: switch off hdcp by setting in the config register
 *
 * Returns:	none
 */
static void hdcp_off(void)
{
	hdcp_set_config(HDCP_Off);
}

/**
 * Description: read register for hdcp status
 *
 * Returns:	value of hdcp status register
 */
static uint32_t hdcp_get_status(void)
{
	return REG_READ(HDCP_STATUS);
}

/**
 * Description: read an from hdcp tx
 *
 * @an		buffer to copy the an into
 *
 * Returns:	none
 */
static void hdcp_read_an(uint8_t *an)
{
	uint8_t i = 0;
	struct double_word_t temp;
	temp.value = 0;
	temp.low = REG_READ(HDCP_AN_LO);
	temp.high = REG_READ(HDCP_AN_HI);
	for (i = 0; i < HDCP_AN_SIZE; i++)
		an[i] = temp.byte[i];
}

/**
 * Description: generate an for new authentication
 *
 * Returns:	none
 */
static void hdcp_capture_an(void)
{
	REG_WRITE(HDCP_INIT, (uint32_t) jiffies);
	REG_WRITE(HDCP_INIT, (uint32_t) (jiffies >> 1));
	REG_WRITE(HDCP_CONFIG, HDCP_CFG_CAPTURE);
}

/**
 * Description: check if an is ready for use
 *
 * Returns:	true if ready else false
 */
static bool hdcp_is_an_ready(void)
{
	struct mofd_hdcp_status_reg_t status;
	status.value = hdcp_get_status();

	if (status.an_ready)
		return true;

	return false;
}

/**
 * Description: enable hdcp on hdmi port
 *
 * @enable	enable or disable hdcp on hdmi port
 *
 * Returns:	none
 */
static void hdcp_enable_port(bool enable)
{
	uint32_t hdmib_reg = REG_READ(HDMIB);
	if (enable)
		hdmib_reg |= ENABLE_HDCP;
	else
		hdmib_reg &= ~ENABLE_HDCP;
	REG_WRITE(HDMIB, hdmib_reg);
}

/**
 * Description: set config value in hdcp tx configuration register
 *
 * @val		value to be written into the configuration register's
 *		config bits
 *
 * Returns:	none
 */
static void hdcp_set_config(int val)
{
	struct mofd_hdcp_config_reg_t config;
	config.value = REG_READ(HDCP_CONFIG);
	config.hdcp_config = val;
	REG_WRITE(HDCP_CONFIG, config.value);
}

/**
 * Description: read hdcp tx config bits
 *
 * Returns:	hdcp tx configuration register's config bits
 */
static int hdcp_get_config(void)
{
	struct mofd_hdcp_config_reg_t config;
	config.value = REG_READ(HDCP_CONFIG);
	return config.hdcp_config;
}

/**
 * Description: write rx_ri into hdcp tx register
 *
 * @rx_ri	downstream device's ri value
 *
 * Returns:	none
 */
static void hdcp_write_rx_ri(uint16_t rx_ri)
{
	REG_WRITE(HDCP_RI, rx_ri);
}

/**
 * Description: check if hdcp is enabled on hdmi port
 *
 * Returns:	true if enabled else false
 */
static bool hdcp_is_hdcp_on(void)
{
	struct mofd_hdcp_status_reg_t status;
	status.value = hdcp_get_status();

	if (status.hdcp_on)
		return true;

	return false;
}

/**
 * Description: check whether hdcp configuration is set to encrypting
 *
 * Returns:	true if set to encrypting else false
 */
static bool hdcp_config_is_encrypting(void)
{
	if (hdcp_get_config() == HDCP_AUTHENTICATE_AND_ENCRYPT)
		return true;
	return false;
}

/**
 * Description: get status bits of hdcp-tx repeater reguister
 *
 * Returns:	repeater status bits
 */
static uint8_t hdcp_get_repeater_status(void)
{
	struct mofd_hdcp_repeater_reg_t repeater;
	repeater.value = REG_READ(HDCP_REP);
	return repeater.status;
}

/**
 * Description: check if repeater is ready for next data
 *
 * Returns:	true if ready else false
 */
static bool hdcp_repeater_rdy_for_nxt_data(void)
{
	uint8_t status = hdcp_get_repeater_status();
	if (status == HDCP_REPEATER_STATUS_RDY_NEXT_DATA)
		return true;
	return false;
}

/**
 * Description: wait for hdcp repeater to be ready for next data
 *
 * Returns:	true if ready else false
 */
static bool hdcp_repeater_wait_for_next_data(void)
{
	uint16_t i = 0;
	for (; i < HDCP_MAX_RETRY_STATUS; i++) {
		if (hdcp_repeater_rdy_for_nxt_data())
			return true;
	}
	return false;
}

/**
 * Description: set control bits of hdcp-tx repeater reguister
 *
 * @value	value of the control bits
 *
 * Returns:	none
 */
static void hdcp_set_repeater_control(int value)
{
	struct mofd_hdcp_repeater_reg_t repeater;
	repeater.value = REG_READ(HDCP_REP);
	repeater.control = value;
	REG_WRITE(HDCP_REP, repeater.value);
}

/**
 * Description: check if repeater is idle
 *
 * Returns:	true if idle else false
 */
#if 0
static bool hdcp_repeater_is_idle(void)
{
	uint8_t status = hdcp_get_repeater_status();
	if (status == HDCP_REPEATER_STATUS_IDLE)
		return true;
	return false;
}
#endif
/**
 * Description: check if repeater is idle
 *
 * Returns:	true if idle else false
 */
static bool hdcp_repeater_is_idle(void)
{
	uint8_t status = hdcp_get_repeater_status();
	if (status == HDCP_REPEATER_STATUS_IDLE)
		return true;
	return false;
}

/**
 * Description: wait for hdcp repeater to get into idle state
 *
 * Returns:	true if repeater is in idle state else false
 */
static bool hdcp_repeater_wait_for_idle(void)
{
	uint16_t i = 0;
	for (; i < HDCP_MAX_RETRY_STATUS; i++) {
		if (hdcp_repeater_is_idle())
			return true;
	}
	return false;
}

/**
 * Description: check the status of SHA1 match
 *
 * Returns:	0	on error
 *		1	on match
 *		-1	if busy
 */
static int hdcp_repeater_v_match_check(void)
{
	uint8_t status = hdcp_get_repeater_status();
	switch (status) {
	case HDCP_REPEATER_STATUS_COMPLETE_MATCH:
		return 1;
	case HDCP_REPEATER_STATUS_BUSY:
		return -1;
	default:
		return 0;
	}
}

/**
 * Description: compare hdcp tx & hdcp rx sha1 results
 *
 * @rep_prime_v sha1 value from downstream repeater
 *
 * Returns:	true if same else false
 */
bool mofd_hdcp_compare_v(uint32_t *rep_prime_v)
{
	bool ret = false;
	uint32_t i = 10, stat;

	/* Load V' */
	REG_WRITE(HDCP_V0, *rep_prime_v);

	REG_WRITE(HDCP_V1, *(rep_prime_v + 1));

	REG_WRITE(HDCP_V2, *(rep_prime_v + 2));

	REG_WRITE(HDCP_V3, *(rep_prime_v + 3));

	REG_WRITE(HDCP_V4, *(rep_prime_v + 4));

	if (false == hdcp_repeater_wait_for_next_data())
		goto exit;

	/* Set HDCP_REP to do the comparison, start
	 * transmitter's V calculation
	 */
	hdcp_set_repeater_control(HDCP_REPEATER_COMPLETE_SHA1);

	usleep_range(5000, 5500)
	do {
		stat = hdcp_repeater_v_match_check();
		if (1 == stat) {
			ret = true; /* match */
			break;
		} else if (-1 == stat)
			usleep_range(5000, 5500)
		else
			break; /* mismatch */
	} while (--i);

exit:
	return ret;
}

/**
 * Description: compute v for repeater authentication
 *
 * @rep_ksv_list	 ksv list from downstream repeater
 * @rep_ksv_list_entries number of entries in the ksv list
 * @topology_data	bstatus value
 *
 * Returns:	true on successfully computing v else false
 */
bool mofd_hdcp_compute_tx_v(uint8_t *rep_ksv_list,
				   uint32_t rep_ksv_list_entries,
				   uint16_t topology_data)
{
	bool ret = false;
	const uint8_t BSTAT_M0_LEN = 18; /* 2 (bstatus) + 8 (M0) + 8 (length) */
	const uint8_t BSTAT_M0 = 10; /* 2 (bstatus) + 8 (M0) */
	const uint8_t M0 = 8; /* 8 (M0) */
	uint32_t num_devices = rep_ksv_list_entries;
	uint32_t lower_num_bytes_for_sha = 0, num_pad_bytes = 0, temp_data = 0;
	uint32_t rem_text_data = 0, num_mo_bytes_left = M0, value = 0, i = 0;
	uint8_t *buffer = NULL, *temp_buffer = NULL, *temp_data_ptr = NULL;
	struct sqword_t buffer_len;

	/* Clear SHA hash generator for new V calculation and
	 * set the repeater to idle state
	 */
	REG_WRITE(HDCP_SHA1_IN, 0);

	hdcp_set_repeater_control(HDCP_REPEATER_CTRL_IDLE);
	if (!hdcp_repeater_wait_for_idle())
		return false;

	/* Start the SHA buffer creation to find the number of pad bytes */
	num_pad_bytes = (64 - ((rep_ksv_list_entries * HDCP_KSV_SIZE)
			 + BSTAT_M0_LEN)
			 % 64);

	/* Get the number of bytes for SHA */
	lower_num_bytes_for_sha = (HDCP_KSV_SIZE * num_devices)
				   + BSTAT_M0_LEN
				   + num_pad_bytes; /* multiple of 64 bytes */

	buffer = kzalloc(lower_num_bytes_for_sha, GFP_KERNEL);
	if (!buffer)
		return false;

	/* 1. Copy the KSV buffer
	 * Note: data is in little endian format
	 */
	temp_buffer = buffer;
	memcpy((void *)temp_buffer, (void *)rep_ksv_list,
		     num_devices * HDCP_KSV_SIZE);
	temp_buffer += num_devices * HDCP_KSV_SIZE;

	/* 2. Copy the topology_data
	 */
	memcpy((void *)temp_buffer, (void *)&topology_data, 2);
	/* bstatus is copied in little endian format */
	temp_buffer += 2;

	/* 3. Offset the pointer buffer by 8 bytes
	 * These 8 bytes are zeroed and are place holders for M0
	 */
	temp_buffer += 8;

	/* 4. Pad the buffer with extra bytes
	 * The first padding byte must be 0x80 based on SHA1 message digest
	 * algorithm. HW automatically appends 0x80 while creating
	 * the buffer if M0 is not 32-bit aligned
	 * If M0 is 32-bit aligned we need to explicitly inject 0x80 to the
	 * buffer
	 */
	if (num_pad_bytes &&
			((num_devices * HDCP_KSV_SIZE + BSTAT_M0) % 4 == 0))
		*temp_buffer = 0x80;
	temp_buffer += num_pad_bytes;

	/* 5. Construct the length byte */
	buffer_len.quad_part = (unsigned long long)(rep_ksv_list_entries *
				HDCP_KSV_SIZE + BSTAT_M0) * 8; /* in bits */
	temp_data_ptr = (uint8_t *)&buffer_len.quad_part;
	/* Store in big endian form, it is reversed to little endian
	 * when fed to SHA1
	 */
	for (i = 1; i <= 8; i++) {
		*temp_buffer = *(temp_data_ptr + 8 - i);
		temp_buffer++;
	}

	/* 6. Write KSV's and bstatus into SHA */
	temp_buffer = buffer;
	for (i = 0; i < (HDCP_KSV_SIZE * num_devices + 2)/4; i++) {
		hdcp_set_repeater_control(HDCP_REPEATER_32BIT_TEXT_IP);

		/* As per HDCP spec sample SHA is in little endian format.
		 * But the data fed to the cipher needs to be in big endian
		 * format for it to compute it correctly
		 */
		memcpy(&value, temp_buffer, 4);
		value = HDCP_CONVERT_ENDIANNESS(value);
		REG_WRITE(HDCP_SHA1_IN, value);
		temp_buffer += 4;

		if (false == hdcp_repeater_wait_for_next_data())
			goto exit;
	}

	/* 7. Write the remaining bstatus data and M0
	 * Text input must be aligned to LSB of the SHA1
	 * in register when inputting partial text and partial M0
	 */
	rem_text_data = (HDCP_KSV_SIZE * num_devices + 2) % 4;
	if (rem_text_data) {
		/* Update the number of M0 bytes */
		num_mo_bytes_left = num_mo_bytes_left - (4-rem_text_data);

		if (false == hdcp_repeater_wait_for_next_data())
			goto exit;

		switch (rem_text_data) {
		case 1:
			hdcp_set_repeater_control(
					HDCP_REPEATER_8BIT_TEXT_24BIT_MO_IP);
			break;
		case 2:
			hdcp_set_repeater_control(
					HDCP_REPEATER_16BIT_TEXT_16BIT_MO_IP);
			break;
		case 3:
			hdcp_set_repeater_control(
					HDCP_REPEATER_24BIT_TEXT_8BIT_MO_IP);
			break;
		default:
			goto exit;
		}

		memcpy(&value, temp_buffer, 4);

		/* Swap the text data in big endian format leaving the M0 data
		 * as it is. LSB should contain the data in big endian format.
		 * Since the M0 specfic data is all zeros while it's fed to the
		 * cipher, those bit don't need to be modified.
		 */
		temp_data = 0;
		for (i = 0; i < rem_text_data; i++) {
			temp_data |= ((value & 0xff << (i * 8)) >>
					(i * 8)) <<
					((rem_text_data - i - 1) * 8);
		}
		REG_WRITE(HDCP_SHA1_IN, temp_data);
		temp_buffer += 4;
	}

	/* write 4 bytes of M0 */
	if (false == hdcp_repeater_wait_for_next_data())
		goto exit;

	hdcp_set_repeater_control(HDCP_REPEATER_32BIT_MO_IP);
	REG_WRITE(HDCP_SHA1_IN, (uint32_t)temp_buffer);
	temp_buffer += 4;
	num_mo_bytes_left -= 4;

	if (num_mo_bytes_left) {
		/* The remaining M0 + padding bytes need to be added */
		num_pad_bytes = num_pad_bytes - (4 - num_mo_bytes_left);

		/* write 4 bytes of M0 */
		if (false == hdcp_repeater_wait_for_next_data())
			goto exit;
		switch (num_mo_bytes_left) {
		case 1:
			hdcp_set_repeater_control(
					HDCP_REPEATER_24BIT_TEXT_8BIT_MO_IP);
			break;
		case 2:
			hdcp_set_repeater_control(
					HDCP_REPEATER_16BIT_TEXT_16BIT_MO_IP);
			break;
		case 3:
			hdcp_set_repeater_control(
					HDCP_REPEATER_8BIT_TEXT_24BIT_MO_IP);
			break;
		case 4:
			hdcp_set_repeater_control(
					HDCP_REPEATER_32BIT_MO_IP);
			break;
		default:
			/* should never happen */
			goto exit;
		}

		REG_WRITE(HDCP_SHA1_IN, (uint32_t)temp_buffer);
		temp_buffer += 4;
		num_mo_bytes_left = 0;
	}

	/* 8. Write the remaining padding bytes and length */
	/* Remaining data = remaining padding data + 64 bits of length data */
	rem_text_data = num_pad_bytes + 8;

	if (rem_text_data % 4) {
		/* Should not happen */
		pr_debug("hdcp: compute_tx_v - data not aligned\n");
		goto exit;
	}

	for (i = 0; i < rem_text_data / 4; i++) {
		if (false == hdcp_repeater_wait_for_next_data())
			goto exit;

		hdcp_set_repeater_control(HDCP_REPEATER_32BIT_TEXT_IP);
		memcpy(&value, temp_buffer, 4);
		/* Do the big endian conversion */
		value = HDCP_CONVERT_ENDIANNESS(value);
		REG_WRITE(HDCP_SHA1_IN, value);
		temp_buffer += 4;
	}

	/* Done */
	ret = true;

exit:
	kfree(buffer);
	return ret;
}

/**
 * Description: Enable encryption once r0 matches
 *
 * Returns:	true on enabling encryption else false
 */
bool mofd_hdcp_enable_encryption(void)
{
	struct mofd_hdcp_status_reg_t status;
	uint32_t hdmib_reg = REG_READ(HDMIB);
	status.value = hdcp_get_status();

	if (hdcp_is_hdcp_on() &&
	    hdcp_config_is_encrypting() &&
	    status.ri_match &&
	    (hdmib_reg & ENABLE_HDCP))
		return true;
	return false;
}

/**
 * Description: disable hdcp
 *
 * Returns:	true on successfully disabling hdcp else false
 */
bool mofd_hdcp_disable(void)
{
	hdcp_off();
	/* Set Rx_Ri to 0 */
	hdcp_write_rx_ri(0);
	/* Set Repeater to Not Present */
	mofd_hdcp_set_repeater(false);
	/* Disable HDCP on this Port */
	/* ips_hdcp_enable_port(false); */
	return true;
}

/**
 * Description: check if hdcp tx & rx ri matches
 *
 * @rx_ri	ri of downstream device
 *
 * Returns:	true if ri matches else false
 */
bool mofd_hdcp_does_ri_match(uint16_t rx_ri)
{
	struct mofd_hdcp_status_reg_t status;

	hdcp_write_rx_ri(rx_ri);
	status.value = hdcp_get_status();
	if (status.ri_match)
		return true;
	return false;
}

/**
 * Description: check if hdcp tx R0 is ready after starting authentication
 *
 * Returns:	true if r0 is ready else false
 */
bool mofd_hdcp_is_r0_ready(void)
{
	struct mofd_hdcp_status_reg_t status;
	status.value = hdcp_get_status();

	if (status.ri_ready)
		return true;
	return false;
}

/**
 * Description: initialize hdcp tx for authentication
 *
 * Returns:	true success else false
 */
bool mofd_hdcp_init(void)
{
	return true;
}

/**
 * Description: check whether hdcp hardware is ready
 *
 * Returns:	true if ready else false
 */
bool mofd_hdcp_is_ready(void)
{
	struct mofd_hdcp_status_reg_t status;
	int count = 20;
	bool ret  = false;

	REG_WRITE(HDCP_CONFIG, HDCP_FUSE_PULL_ENABLE | HDCP_PULL_FUSE);
	while ((count--) > 0) {
		udelay(20);
		status.value = hdcp_get_status();
		if (status.fus_success && status.fus_complete) {
			ret = true;
			break;
		}
	}

	pr_debug("hdcp: read count left = %d\n", count);
	return ret;
}

/**
 * Description: get hardware frame count for cipher Ri update
 *
 * @count   framer count for cipher Ri update
 *
 * Returns: true if successful else false
 */
bool mofd_hdcp_get_ri_frame_count(uint8_t *count)
{
	struct mofd_hdcp_status_reg_t status;

	status.value = hdcp_get_status();
	*count       = status.frame_count;

	return true;
}

/**
 * Description: read an from hdcp tx
 *
 * @an	  buffer to return an in
 *
 * Returns:	true on succesful read else false
 */
bool mofd_hdcp_get_an(uint8_t *an, uint32_t size)
{
	if (an != NULL && size == HDCP_AN_SIZE) {
		bool ret = false;
		hdcp_off();
		hdcp_capture_an();
		do {
			ret = hdcp_is_an_ready();
		} while (ret == false);
		hdcp_read_an(an);

		return true;
	}
	return false;
}

/**
 * Description: set repeater bit in hdcp tx if downstream is a repeater else
 *		reset the bit
 *
 * @present	indicates whether downstream is repeater or not
 *
 * Returns:	true on succesful write else false
 */
bool mofd_hdcp_set_repeater(bool present)
{
	struct mofd_hdcp_repeater_reg_t repeater;
	repeater.value = REG_READ(HDCP_REP);
	repeater.present = present;
	REG_WRITE(HDCP_REP, repeater.value);
	/* delay for hardware change of repeater status */
	usleep_range(1000, 1500);
	return true;
}

/**
 * Description: set downstream bksv in hdcp tx
 *
 * @bksv	bksv from downstream device
 *
 * Returns:	true on succesful write else false
 */
bool mofd_hdcp_set_bksv(uint8_t *bksv)
{
	uint8_t i = 0;
	struct double_word_t temp;
	if (bksv == NULL)
		return false;
	temp.value = 0;
	for (i = 0; i < HDCP_KSV_SIZE; i++)
		temp.byte[i] = bksv[i];

	REG_WRITE(HDCP_BKSV_LO, temp.low);
	REG_WRITE(HDCP_BKSV_HI, temp.high);
	return true;
}

/**
 * Description: start first stage of authentication by writing an aksv
 *
 * Returns:	true on succesfully starting authentication else false
 */
bool mofd_hdcp_start_authentication(void)
{
	hdcp_enable_port(true);
	hdcp_set_config(HDCP_AUTHENTICATE_AND_ENCRYPT);
	return true;
}

/**
 * Description: read aksv from hdcp tx
 *
 * @aksv	buffer to return aksv
 *
 * Returns:	true on succesful read else false
 */
bool mofd_hdcp_get_aksv(uint8_t *aksv, int size)
{
	int i = 0;
	struct double_word_t temp;
	static uint8_t save_aksv[HDCP_KSV_SIZE] = {0, 0, 0, 0, 0};
	static bool aksv_read_once;

	if (aksv == 0 || size != HDCP_KSV_SIZE)
		return false;

	if (aksv_read_once == false) {
		temp.value = 0;
		temp.low = REG_READ(HDCP_ASKV_LO);
		temp.high = REG_READ(HDCP_AKSV_HI);
		aksv_read_once = true;
		for (i = 0; i < HDCP_KSV_SIZE; i++)
			save_aksv[i] = temp.byte[i];
	}
	for (i = 0; i < HDCP_KSV_SIZE; i++)
		aksv[i] = save_aksv[i];
	return true;
}

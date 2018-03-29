/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB Power delivery module */

#ifndef _USB_PD_FUNC_H
#define _USB_PD_FUNC_H

/**
 * Decide which PDO to choose from the source capabilities.
 *
 * @param cnt  the number of Power Data Objects.
 * @param src_caps Power Data Objects representing the source capabilities.
 * @param rdo  requested Request Data Object.
 * @param ma  selected current limit (stored on success)
 * @param mv  selected supply voltage (stored on success)
 * @param req_type request type
 * @return <0 if invalid, else EC_SUCCESS
 */
extern int pd_build_request(int cnt, uint32_t *src_caps, uint32_t *rdo,
		     uint32_t *ma, uint32_t *mv, enum pd_request_type req_type);

/**
 * Check if max voltage request is allowed (only used if
 * CONFIG_USB_PD_CHECK_MAX_REQUEST_ALLOWED is defined).
 *
 * @return True if max voltage request allowed, False otherwise
 */
extern int pd_is_max_request_allowed(void);

/**
 * Process source capabilities packet
 *
 * @param port USB-C port number
 * @param cnt  the number of Power Data Objects.
 * @param src_caps Power Data Objects representing the source capabilities.
 */
extern void pd_process_source_cap(struct typec_hba *hba, int cnt, uint32_t *src_caps);

/**
 * Put a cap on the max voltage requested as a sink.
 * @param mv maximum voltage in millivolts.
 */
extern void pd_set_max_voltage(unsigned mv);

/**
 * Get the max voltage that can be requested as set by pd_set_max_voltage().
 * @return max voltage
 */
extern unsigned pd_get_max_voltage(void);

/**
 * Check if this board supports the given input voltage.
 *
 * @mv input voltage
 * @return 1 if voltage supported, 0 if not
 */
extern int pd_is_valid_input_voltage(int mv);

/**
 * Request a new operating voltage.
 *
 * @param rdo  Request Data Object with the selected operating point.
 * @return EC_SUCCESS if we can get the requested voltage/OP, <0 else.
 */
extern int pd_check_requested_voltage(uint32_t rdo);

/**
 * Select a new output voltage.
 *
 * param idx index of the new voltage in the source PDO table.
 */
extern void pd_transition_voltage(int idx);

/**
 * Go back to the default/safe state of the power supply
 *
 * @param port USB-C port number
 */
extern void pd_power_supply_reset(struct typec_hba *hba);

/**
 * Enable the power supply output after the ready delay.
 *
 * @param port USB-C port number
 * @return EC_SUCCESS if the power supply is ready, <0 else.
 */
extern int pd_set_power_supply_ready(struct typec_hba *hba);

/**
 * Ask the specified voltage from the PD source.
 *
 * It triggers a new negotiation sequence with the source.
 * @param port USB-C port number
 * @param mv request voltage in millivolts.
 */
extern void pd_request_source_voltage(struct typec_hba *hba, int mv);

/**
 * Set a voltage limit from the PD source.
 *
 * If the source is currently active, it triggers a new negotiation.
 * @param port USB-C port number
 * @param mv limit voltage in millivolts.
 */
extern void pd_set_external_voltage_limit(struct typec_hba *hba, int mv);

/**
 * Set the PD input current limit.
 *
 * @param port USB-C port number
 * @param max_ma Maximum current limit
 * @param supply_voltage Voltage at which current limit is applied
 */
extern void pd_set_input_current_limit(struct typec_hba *hba, uint32_t max_ma,
				uint32_t supply_voltage);

/**
 * Set the type-C input current limit.
 *
 * @param port USB-C port number
 * @param max_ma Maximum current limit
 * @param supply_voltage Voltage at which current limit is applied
 */
extern void typec_set_input_current_limit(struct typec_hba *hba, uint32_t max_ma,
				   uint32_t supply_voltage);

/**
 * Verify board specific health status : current, voltages...
 *
 * @return EC_SUCCESS if the board is good, <0 else.
 */
extern int pd_board_checks(void);

/**
 * Return if VBUS is detected on type-C port
 *
 * @param port USB-C port number
 * @return VBUS is detected
 */
extern int pd_snk_is_vbus_provided(struct typec_hba *hba);

/**
 * Check if power swap is allowed.
 *
 * @param port USB-C port number
 * @return True if power swap is allowed, False otherwise
 */
extern int pd_check_power_swap(struct typec_hba *hba);

/**
 * Check if data swap is allowed.
 *
 * @param port USB-C port number
 * @return True if data swap is allowed, False otherwise
 */
extern int pd_check_data_swap(struct typec_hba *hba);

/**
 * Check if vconn swap is allowed.
 *
 * @param port USB-C port number
 * @return True if vconn swap is allowed, False otherwise
 */

extern int pd_check_vconn_swap(struct typec_hba *hba);

/**
 * Check current power role for potential power swap
 *
 * @param port USB-C port number
 * @param pr_role Our power role
 * @param flags PD flags
 */
extern void pd_check_pr_role(struct typec_hba *hba, int pr_role, int flags);

/**
 * Check current data role for potential data swap
 *
 * @param port USB-C port number
 * @param dr_role Our data role
 * @param flags PD flags
 */
extern void pd_check_dr_role(struct typec_hba *hba, int dr_role, int flags);

/**
 * Check if we should charge from this device. This is
 * basically a white-list for chargers that are dual-role,
 * don't set the externally powered bit, but we should charge
 * from by default.
 *
 * @param vid Port partner Vendor ID
 * @param pid Port partner Product ID
 */
extern int pd_charge_from_device(uint16_t vid, uint16_t pid);

/**
 * Execute data swap.
 *
 * @param port USB-C port number
 * @param data_role new data role
 */
extern void pd_execute_data_swap(struct typec_hba *hba, int data_role);

/**
 * Get PD device info used for VDO_CMD_SEND_INFO / VDO_CMD_READ_INFO
 *
 * @param info_data pointer to info data array
 */
extern void pd_get_info(uint32_t *info_data);

/**
 * Handle Vendor Defined Messages
 *
 * @param port     USB-C port number
 * @param cnt      number of data objects in the payload.
 * @param payload  payload data.
 * @param rpayload pointer to the data to send back.
 * @return if >0, number of VDOs to send back.
 */
extern int pd_custom_vdm(struct typec_hba *hba, int cnt, uint32_t *payload, uint32_t **rpayload);

/**
 * Handle Structured Vendor Defined Messages
 *
 * @param port     USB-C port number
 * @param cnt      number of data objects in the payload.
 * @param payload  payload data.
 * @param rpayload pointer to the data to send back.
 * @return if >0, number of VDOs to send back.
 */
extern int pd_svdm(struct typec_hba *hba, int cnt, uint32_t *payload, uint32_t **rpayload);

/**
 * Handle Custom VDMs for flashing.
 *
 * @param port     USB-C port number
 * @param cnt      number of data objects in the payload.
 * @param payload  payload data.
 * @return if >0, number of VDOs to send back.
 */
extern int pd_custom_flash_vdm(struct typec_hba *hba, int cnt, uint32_t *payload);

/**
 * Enter alternate mode on DFP
 *
 * @param port     USB-C port number
 * @param svid USB standard or vendor id to exit or zero for DFP amode reset.
 * @param opos object position of mode to exit.
 * @return vdm for UFP to be sent to enter mode or zero if not.
 */
extern uint32_t pd_dfp_enter_mode(struct typec_hba *hba, uint16_t svid, int opos);

/**
 *  Get DisplayPort pin mode for DFP to request from UFP's capabilities.
 *
 * @param port     USB-C port number.
 * @param status   DisplayPort Status VDO.
 * @return one-hot PIN config to request.
 */
extern int pd_dfp_dp_get_pin_mode(struct typec_hba *hba, uint32_t status);

/**
 * Exit alternate mode on DFP
 *
 * @param port USB-C port number
 * @param svid USB standard or vendor id to exit or zero for DFP amode reset.
 * @param opos object position of mode to exit.
 * @return 1 if UFP should be sent exit mode VDM.
 */
extern int pd_dfp_exit_mode(struct typec_hba *hba, uint16_t svid, int opos);

/**
 * Initialize policy engine for DFP
 *
 * @param port     USB-C port number
 */
extern void pd_dfp_pe_init(struct typec_hba *hba);

/**
 * Return the VID of the USB PD accessory connected to a specified port
 *
 * @param port  USB-C port number
 * @return      the USB Vendor Identifier or 0 if it doesn't exist
 */
extern uint16_t pd_get_identity_vid(struct typec_hba *hba);

/**
 * Return the PID of the USB PD accessory connected to a specified port
 *
 * @param port  USB-C port number
 * @return      the USB Product Identifier or 0 if it doesn't exist
 */
extern uint16_t pd_get_identity_pid(struct typec_hba *hba);

/**
 * Store Device ID & RW hash of device
 *
 * @param port			USB-C port number
 * @param dev_id		device identifier
 * @param rw_hash		pointer to rw_hash
 * @param current_image		current image: RW or RO
 * @return			true if the dev / hash match an existing hash
 *				in our table, false otherwise
 */
int pd_dev_store_rw_hash(struct typec_hba *hba, uint16_t dev_id, uint32_t *rw_hash,
			 uint32_t ec_current_image);

/**
 * Try to fetch one PD log entry from accessory
 *
 * @param port	USB-C accessory port number
 * @return	EC_RES_SUCCESS if the VDM was sent properly else error code
 */
int pd_fetch_acc_log_entry(struct typec_hba *hba);

/**
 * Analyze the log entry received as the VDO_CMD_GET_LOG payload.
 *
 * @param port		USB-C accessory port number
 * @param cnt		number of data objects in payload
 * @param payload	payload data
 */
void pd_log_recv_vdm(struct typec_hba *hba, int cnt, uint32_t *payload);

/**
 * Send Vendor Defined Message
 *
 * @param port     USB-C port number
 * @param vid      Vendor ID
 * @param cmd      VDO command number
 * @param data     Pointer to payload to send
 * @param count    number of data objects in payload
 */
extern void pd_send_vdm(struct typec_hba *hba, uint32_t vid, int cmd, const uint32_t *data,
		 int count);

/**
 * Get PD source power data objects.
 *
 * @param src_pdo pointer to the data to return.
 * @return number of PDOs returned.
 */
extern int pd_get_source_pdo(const uint32_t **src_pdo);

/**
 * Request that a host event be sent to notify the AP of a PD power event.
 *
 * @param mask host event mask.
 */
extern void pd_send_host_event(int mask);

/**
 * Determine if in alternate mode or not.
 *
 * @param port port number.
 * @param svid USB standard or vendor id
 * @return object position of mode chosen in alternate mode otherwise zero.
 */
extern int pd_alt_mode(struct typec_hba *hba, uint16_t svid);

/**
 * Send hpd over USB PD.
 *
 * @param port port number.
 * @param hpd hotplug detect type.
 */
extern void pd_send_hpd(struct typec_hba *hba, enum hpd_event hpd);

/**
 * Enable USB Billboard Device.
 */
extern void pd_usb_billboard_deferred(void);
/* --- Physical layer functions : chip specific --- */

/* Packet preparation/retrieval */

/**
 * Prepare packet reading state machine.
 *
 * @param port USB-C port number
 */
extern void pd_init_dequeue(struct typec_hba *hba);

/**
 * Prepare packet reading state machine.
 *
 * @param port USB-C port number
 * @param off  current position in the packet buffer.
 * @param len  minimum size to read in bits.
 * @param val  the read bits.
 * @return new position in the packet buffer.
 */
extern int pd_dequeue_bits(struct typec_hba *hba, int off, int len, uint32_t *val);

/**
 * Advance until the end of the preamble.
 *
 * @param port USB-C port number
 * @return new position in the packet buffer.
 */
extern int pd_find_preamble(struct typec_hba *hba);

/**
 * Write the preamble in the TX buffer.
 *
 * @param port USB-C port number
 * @return new position in the packet buffer.
 */
extern int pd_write_preamble(struct typec_hba *hba);

/**
 * Write one 10-period symbol in the TX packet.
 * corresponding to a quartet with 4b5b encoding
 * and Biphase Mark Coding.
 *
 * @param port USB-C port number
 * @param bit_off current position in the packet buffer.
 * @param val10    the 10-bit integer.
 * @return new position in the packet buffer.
 */
extern int pd_write_sym(struct typec_hba *hba, int bit_off, uint32_t val10);


/**
 * Ensure that we have an edge after EOP and we end up at level 0,
 * also fill the last byte.
 *
 * @param port USB-C port number
 * @param bit_off current position in the packet buffer.
 * @return new position in the packet buffer.
 */
extern int pd_write_last_edge(struct typec_hba *hba, int bit_off);

/**
 * Do 4B5B encoding on a 32-bit word.
 *
 * @param port USB-C port number
 * @param off current offset in bits inside the message
 * @param val32 32-bit word value to encode
 * @return new offset in the message in bits.
 */
extern int encode_word(struct typec_hba *hba, int off, uint32_t val32);

/**
 * Ensure that we have an edge after EOP and we end up at level 0,
 * also fill the last byte.
 *
 * @param port USB-C port number
 * @param header PD packet header
 * @param cnt number of payload words
 * @param data payload content
 * @return length of the message in bits.
 */
extern int prepare_message(struct typec_hba *hba, uint16_t header, uint8_t cnt,
		    const uint32_t *data);

/**
 * Dump the current PD packet on the console for debug.
 *
 * @param port USB-C port number
 * @param msg  context string.
 */
extern void pd_dump_packet(struct typec_hba *hba, const char *msg);

/**
 * Change the TX data clock frequency.
 *
 * @param port USB-C port number
 * @param freq frequency in hertz.
 */
extern void pd_set_clock(struct typec_hba *hba, int freq);

/* TX/RX callbacks */

/**
 * Start sending over the wire the prepared packet.
 *
 * @param port USB-C port number
 * @param polarity plug polarity (0=CC1, 1=CC2).
 * @param bit_len size of the packet in bits.
 * @return length transmitted or negative if error
 */
extern int pd_start_tx(struct typec_hba *hba, int polarity, int bit_len);

/**
 * Set PD TX DMA to use circular mode. Call this before pd_start_tx() to
 * continually loop over the transmit buffer given in pd_start_tx().
 *
 * @param port USB-C port number
 */
extern void pd_tx_set_circular_mode(struct typec_hba *hba);

/**
 * Stop PD TX DMA circular mode transaction already in progress.
 *
 * @param port USB-C port number
 */
extern void pd_tx_clear_circular_mode(struct typec_hba *hba);

/**
 * Call when we are done sending a packet.
 *
 * @param port USB-C port number
 * @param polarity plug polarity (0=CC1, 1=CC2).
 */
extern void pd_tx_done(struct typec_hba *hba, int polarity);

/**
 * Check whether the PD reception is started.
 *
 * @param port USB-C port number
 * @return true if the reception is on-going.
 */
extern int pd_rx_started(struct typec_hba *hba);

/**
 * Suspend the PD task.
 * @param port USB-C port number
 * @param enable pass 0 to resume, anything else to suspend
 */
extern void pd_set_suspend(struct typec_hba *hba, int enable);

/* Callback when the hardware has detected an incoming packet */
extern void pd_rx_event(struct typec_hba *hba);
/* Start sampling the CC line for reception */
extern void pd_rx_start(struct typec_hba *hba);
/* Call when we are done reading a packet */
extern void pd_rx_complete(struct typec_hba *hba);

/* restart listening to the CC wire */
extern void pd_rx_enable_monitoring(struct typec_hba *hba);
/* stop listening to the CC wire during transmissions */
extern void pd_rx_disable_monitoring(struct typec_hba *hba);

/* get time since last RX edge interrupt */
extern uint64_t get_time_since_last_edge(struct typec_hba *hba);

/**
 * Deinitialize the hardware used for PD.
 *
 * @param port USB-C port number
 */
extern void pd_hw_release(struct typec_hba *hba);

/**
 * Initialize the hardware used for PD RX/TX.
 *
 * @param port USB-C port number
 * @param role Role to initialize pins in
 */
extern void pd_hw_init(struct typec_hba *hba, int role);

/**
 * Initialize the reception side of hardware used for PD.
 *
 * This is a subset of pd_hw_init() including only :
 * the comparators + the RX edge delay timer + the RX DMA.
 *
 * @param port USB-C port number
 */
extern void pd_hw_init_rx(struct typec_hba *hba);

/* --- Protocol layer functions --- */

/**
 * Decode a raw packet in the RX buffer.
 *
 * @param port USB-C port number
 * @param payload buffer to store the packet payload (must be 7x 32-bit)
 * @return the packet header or <0 in case of error
 */
extern int pd_analyze_rx(struct typec_hba *hba, uint32_t *payload);

/**
 * Get connected state
 *
 * @param port USB-C port number
 * @return True if port is in connected state
 */
extern int pd_is_connected(struct typec_hba *hba);

/**
 * Execute a hard reset
 *
 * @param port USB-C port number
 */
extern void pd_execute_hard_reset(struct typec_hba *hba);

/**
 * Signal to protocol layer that PD transmit is complete
 *
 * @param port USB-C port number
 * @param status status of the transmission
 */
extern void pd_transmit_complete(struct typec_hba *hba, int status);

/**
 * Get port polarity.
 *
 * @param port USB-C port number
 */
extern int pd_get_polarity(struct typec_hba *hba);

/**
 * Get port partner data swap capable status
 *
 * @param port USB-C port number
 */
extern int pd_get_partner_data_swap_capable(struct typec_hba *hba);

/**
 * Request power swap command to be issued
 *
 * @param port USB-C port number
 */
extern void pd_request_power_swap(struct typec_hba *hba);

/**
 * Request data swap command to be issued
 *
 * @param port USB-C port number
 */
extern void pd_request_data_swap(struct typec_hba *hba);

/**
 * Set the PD communication enabled flag. When communication is disabled,
 * the port can still detect connection and source power but will not
 * send or respond to any PD communication.
 *
 * @param enable Enable flag to set
 */
extern void pd_comm_enable(struct typec_hba *hba, int enable);

/**
 * Set the PD pings enabled flag. When source has negotiated power over
 * PD successfully, it can optionally send pings periodically based on
 * this enable flag.
 *
 * @param port USB-C port number
 * @param enable Enable flag to set
 */
extern void pd_ping_enable(struct typec_hba *hba, int enable);

/* Issue PD soft reset */
extern void pd_soft_reset(void);

/* Prepare PD communication for reset */
extern void pd_prepare_reset(void);

/**
 * Signal power request to indicate a charger update that affects the port.
 *
 * @param port USB-C port number
 */
extern void pd_set_new_power_request(struct typec_hba *hba);

/* ----- Logging ----- */
#ifdef CONFIG_USB_PD_LOGGING
/**
 * Record one event in the PD logging FIFO.
 *
 * @param type event type as defined by PD_EVENT_xx in ec_commands.h
 * @param size_port payload size and port num (defined by PD_LOG_PORT_SIZE)
 * @param data type-defined information
 * @param payload pointer to the optional payload (0..16 bytes)
 */
extern void pd_log_event(uint8_t type, uint8_t size_port,
		  uint16_t data, void *payload);

/**
 * Retrieve one logged event and prepare a VDM with it.
 *
 * Used to answer the VDO_CMD_GET_LOG unstructured VDM.
 *
 * @param payload pointer to the payload data buffer (must be 7 words)
 * @return number of 32-bit words in the VDM payload.
 */
extern int pd_vdm_get_log_entry(uint32_t *payload);
#else  /* CONFIG_USB_PD_LOGGING */
static inline void pd_log_event(uint8_t type, uint8_t size_port,
				uint16_t data, void *payload) {}
static inline int pd_vdm_get_log_entry(uint32_t *payload) { return 0; }
#endif /* CONFIG_USB_PD_LOGGING */

#endif

/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2015-2019 TrustKernel Incorporated
 */

#ifndef _TEE_TUI_H_
#define _TEE_TUI_H_

#define TRUSTEDUI_MODE_OFF				0x00
#define TRUSTEDUI_MODE_ALL				0xff
#define TRUSTEDUI_MODE_TUI_SESSION		0x01
#define TRUSTEDUI_MODE_VIDEO_SECURED	0x02
#define TRUSTEDUI_MODE_INPUT_SECURED	0x04

int teec_wait_cmd(uint32_t *cmd_id);
bool teec_notify_event(uint32_t event_type);

int trustedui_blank_inc(void);
int trustedui_blank_dec(void);
int trustedui_blank_get_counter(void);
void trustedui_blank_set_counter(int counter);

int trustedui_get_current_mode(void);
void trustedui_set_mode(int mode);
int trustedui_set_mask(int mask);
int trustedui_clear_mask(int mask);


/**
 * Notification ID's for communication Trustlet Connector -> Driver.
 */
#define NOT_TUI_NONE			0
/* NWd system event that closes the current TUI session*/
#define NOT_TUI_CANCEL_EVENT	1

#endif /* _TEE_TUI_H_ */

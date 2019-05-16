/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 TRUSTONIC LIMITED
 */

#ifndef TLCTUI_H_
#define TLCTUI_H_

#include "tui_ioctl.h"
#define TUI_MOD_TAG "t-base-tui "

#define ION_PHYS_WORKING_BUFFER_IDX (0)
#define ION_PHYS_FRAME_BUFFER_IDX   (1)

void reset_global_command_id(void);
int tlc_wait_cmd(struct tlc_tui_command_t *cmd);
int tlc_ack_cmd(struct tlc_tui_response_t *rsp_id);
bool tlc_notify_event(u32 event_type);
int tlc_init_driver(void);
u32 send_cmd_to_user(u32 command_id, u32 data0, u32 data1);
struct mc_session_handle *get_session_handle(void);

extern atomic_t fileopened;
extern struct tui_dci_msg_t *dci;
extern struct tlc_tui_response_t g_user_rsp;
extern u64 g_ion_phys[MAX_BUFFER_NUMBER];
extern u32 g_ion_size[MAX_BUFFER_NUMBER];
#endif /* TLCTUI_H_ */

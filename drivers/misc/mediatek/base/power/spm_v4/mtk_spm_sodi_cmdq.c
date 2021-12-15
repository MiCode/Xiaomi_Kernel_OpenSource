/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <mtk_spm_sodi_cmdq.h>

/*
 * GCE write spm SW2SPM_MAILBOX_3 (0x100065F8)
 *     [15:4] : Key, must write 9CE
 *     [1]: CG mode
 *     [0]: Need Bus clock
 * Then polling SPM2SW_MAILBOX_3 (0x100065DC) = 0
 */

void exit_pd_by_cmdq(struct cmdqRecStruct *handler)
{
	/* Switch to CG mode */
	cmdqRecWrite(handler, 0x100065F8, 0x9ce2, 0xffff);
	/* Polling ack */
	cmdqRecPoll(handler, 0x100065DC, 0x0, ~0);
}

void enter_pd_by_cmdq(struct cmdqRecStruct *handler)
{
	/* Switch to PD mode */
	cmdqRecWrite(handler, 0x100065F8, 0x9ce0, 0xffff);
}

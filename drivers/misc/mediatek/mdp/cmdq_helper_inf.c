// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/mailbox_controller.h>
#include "cmdq_helper_ext.h"
#include "mdp_common.h"
#include <cmdq-util.h>

struct ContextStruct cmdq_ctx; /* cmdq driver context */
EXPORT_SYMBOL(cmdq_ctx);
struct CmdqCBkStruct *cmdq_group_cb;
EXPORT_SYMBOL(cmdq_group_cb);
struct CmdqDebugCBkStruct cmdq_debug_cb;
EXPORT_SYMBOL(cmdq_debug_cb);
struct cmdq_client *cmdq_clients[CMDQ_MAX_THREAD_COUNT];
EXPORT_SYMBOL(cmdq_clients);
struct cmdqMDPFuncStruct mdp_funcs;
EXPORT_SYMBOL(mdp_funcs);

static struct cmdqMDPFuncStruct *cmdq_mdp_get_func_priv(void)
{
	return &mdp_funcs;
}

static bool cmdq_core_is_valid_group_priv(u32 engGroup)
{
	/* check range */
	if (engGroup >= cmdq_mdp_get_func_priv()->getGroupMax())
		return false;

	return true;
}

bool cmdq_core_should_print_msg(void)
{
	bool logLevel = (cmdq_ctx.logLevel & (1 << CMDQ_LOG_LEVEL_MSG)) ?
		(1) : (0);
	return logLevel;
}
EXPORT_SYMBOL(cmdq_core_should_print_msg);

u32 mdp_get_group_isp(void)
{
	return cmdq_mdp_get_func_priv()->getGroupIsp();
}
EXPORT_SYMBOL(mdp_get_group_isp);

u32 mdp_get_group_wpe(void)
{
	return cmdq_mdp_get_func_priv()->getGroupWpe();
}
EXPORT_SYMBOL(mdp_get_group_wpe);

s32 cmdqCoreRegisterCB(u32 engGroup,
	CmdqClockOnCB clockOn, CmdqDumpInfoCB dumpInfo,
	CmdqResetEngCB resetEng, CmdqClockOffCB clockOff)
{
	struct CmdqCBkStruct *callback;

	if (!cmdq_core_is_valid_group_priv(engGroup))
		return -EFAULT;

	CMDQ_MSG("Register %d group engines' callback\n", engGroup);
	CMDQ_MSG("clockOn:%ps dumpInfo:%ps resetEng:%ps clockOff:%ps\n",
		clockOn, dumpInfo, resetEng, clockOff);

	callback = &cmdq_group_cb[engGroup];
	callback->clockOn = clockOn;
	callback->dumpInfo = dumpInfo;
	callback->resetEng = resetEng;
	callback->clockOff = clockOff;

	return 0;
}
EXPORT_SYMBOL(cmdqCoreRegisterCB);

s32 cmdqCoreRegisterDebugRegDumpCB(
	CmdqDebugRegDumpBeginCB beginCB, CmdqDebugRegDumpEndCB endCB)
{
	CMDQ_VERBOSE("Register reg dump: begin:%p end:%p\n",
		beginCB, endCB);
	cmdq_debug_cb.beginDebugRegDump = beginCB;
	cmdq_debug_cb.endDebugRegDump = endCB;
	return 0;
}
EXPORT_SYMBOL(cmdqCoreRegisterDebugRegDumpCB);

s32 cmdq_core_save_first_dump(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	cmdq_util_error_save_lst(format, args);
	va_end(args);
	return 0;
}
EXPORT_SYMBOL(cmdq_core_save_first_dump);

const char *cmdq_core_query_first_err_mod(void)
{
	u8 i;

	for (i = 0; i < ARRAY_SIZE(cmdq_clients); i++)
		if (cmdq_clients[i])
			return cmdq_util_get_first_err_mod(
				cmdq_clients[i]->chan);
	return NULL;
}
EXPORT_SYMBOL(cmdq_core_query_first_err_mod);

MODULE_LICENSE("GPL");

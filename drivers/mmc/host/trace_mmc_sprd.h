/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mmc_sprd

#if !defined(_TRACE_MMC_SPRD) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MMC_SPRD

#include <linux/blkdev.h>
#include <linux/mmc/core.h>
#include <linux/mmc/host.h>
#include <linux/tracepoint.h>

TRACE_EVENT(mmc_cmd_send,
	TP_PROTO(struct mmc_host *host, struct gendisk *disk,
		 struct mmc_request *mrq, struct mmc_request *p_mrq),
	TP_ARGS(host, disk, mrq, p_mrq),
	TP_STRUCT__entry(
		__field(dev_t,			dev)
		__field(u32,                    cmd_opcode)
		__field(u32,			cmd_arg)
		__field(unsigned int,           cmd_task_id)
		__field(unsigned int,           cmd_sbc_task_id)
		__field(unsigned int,           cmd_flags)
		__field(int,                    tag)
		__field(unsigned int,           blksz)
		__field(unsigned int,           blocks)
		__field(unsigned int,           blk_addr)
		__field(struct mmc_request *,   mrq)
		__field(struct mmc_request *,   p_mrq)
		__string(name,                  mmc_hostname(host))
	),

	TP_fast_assign(
		__entry->dev = disk_devt(disk);
		__entry->cmd_opcode = mrq->cmd ? mrq->cmd->opcode : 0;
		__entry->cmd_arg = mrq->cmd ? mrq->cmd->arg : 0;
		__entry->cmd_task_id = mrq->cmd ? ((mrq->cmd->arg >> 16) & 0x1f) : 0;
		__entry->cmd_sbc_task_id = mrq->sbc ? ((mrq->sbc->arg >> 16) & 0x1f) : 0;
		__entry->cmd_flags = mrq->cmd ? mrq->cmd->flags : 0;
		__entry->tag = mrq->tag;
		__entry->blksz = mrq->data ? mrq->data->blksz : 0;
		__entry->blocks = mrq->data ? mrq->data->blocks : 0;
		__entry->blk_addr = mrq->data ? mrq->data->blk_addr : 0;
		__entry->mrq = mrq;
		__entry->p_mrq = p_mrq ? p_mrq : 0;
		__assign_str(name, mmc_hostname(host));
	),

	TP_printk("mmc_cmd_send %s dev=%d mmc_request[%p]: p_mrq=%p cmd_opcode=%u cmd_arg=0x%x cmd_task_id=%u cmd_sbc_task_id=%u cmd_flags=0x%x tag=%d blocks=%u block_size=%u blk_addr=%u",
		  __get_str(name), __entry->dev,  __entry->mrq, __entry->p_mrq,
		  __entry->cmd_opcode,  __entry->cmd_arg, __entry->cmd_task_id,
		  __entry->cmd_sbc_task_id, __entry->cmd_flags, __entry->tag,
		  __entry->blocks, __entry->blksz, __entry->blk_addr)
);

TRACE_EVENT(mmc_cmd_done,
	TP_PROTO(struct mmc_host *host, struct mmc_request *mrq),
	TP_ARGS(host, mrq),
	TP_STRUCT__entry(
		__field(u32,                    cmd_opcode)
		__field(int,                    cmd_err)
		__field(unsigned int,           cmd_resp)
		__field(unsigned int,           cmd_retries)
		__field(int,                    tag)
		__field(struct mmc_request *,   mrq)
		__string(name,                  mmc_hostname(host))
	),

	TP_fast_assign(
		__entry->cmd_opcode = mrq->cmd ? mrq->cmd->opcode : 0;
		__entry->cmd_err = mrq->cmd ? mrq->cmd->error : 0;
		__entry->cmd_resp = mrq->cmd ? mrq->cmd->resp[0] : 0;
		__entry->cmd_retries = mrq->cmd ? mrq->cmd->retries : 0;
		__entry->tag = mrq->tag;
		__assign_str(name, mmc_hostname(host));
		__entry->mrq = mrq;
	),

	TP_printk("mmc_cmd_done %s mmc_request[%p]: cmd_opcode=%u cmd_err=%d cmd_resp=%d cmd_retries=%u tag=%d",
		  __get_str(name), __entry->mrq, __entry->cmd_opcode, __entry->cmd_err,
		  __entry->cmd_resp, __entry->cmd_retries, __entry->tag)
);

#endif

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE trace_mmc_sprd

#include <trace/define_trace.h>

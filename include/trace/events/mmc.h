#undef TRACE_SYSTEM
#define TRACE_SYSTEM mmc

#if !defined(_TRACE_MMC_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MMC_H

#include <linux/blkdev.h>
#include <linux/mmc/core.h>
#include <linux/mmc/host.h>
#include <linux/tracepoint.h>

TRACE_EVENT(mmc_request_start,

	TP_PROTO(struct mmc_host *host, struct mmc_request *mrq),

	TP_ARGS(host, mrq),

	TP_STRUCT__entry(
		__field(u32,			cmd_opcode)
		__field(u32,			cmd_arg)
		__field(unsigned int,		cmd_flags)
		__field(unsigned int,		cmd_retries)
		__field(u32,			stop_opcode)
		__field(u32,			stop_arg)
		__field(unsigned int,		stop_flags)
		__field(unsigned int,		stop_retries)
		__field(u32,			sbc_opcode)
		__field(u32,			sbc_arg)
		__field(unsigned int,		sbc_flags)
		__field(unsigned int,		sbc_retries)
		__field(unsigned int,		blocks)
		__field(unsigned int,		blksz)
		__field(unsigned int,		data_flags)
		__field(unsigned int,		can_retune)
		__field(unsigned int,		doing_retune)
		__field(unsigned int,		retune_now)
		__field(int,			need_retune)
		__field(int,			hold_retune)
		__field(unsigned int,		retune_period)
		__field(struct mmc_request *,	mrq)
		__string(name,			mmc_hostname(host))
	),

	TP_fast_assign(
		__entry->cmd_opcode = mrq->cmd->opcode;
		__entry->cmd_arg = mrq->cmd->arg;
		__entry->cmd_flags = mrq->cmd->flags;
		__entry->cmd_retries = mrq->cmd->retries;
		__entry->stop_opcode = mrq->stop ? mrq->stop->opcode : 0;
		__entry->stop_arg = mrq->stop ? mrq->stop->arg : 0;
		__entry->stop_flags = mrq->stop ? mrq->stop->flags : 0;
		__entry->stop_retries = mrq->stop ? mrq->stop->retries : 0;
		__entry->sbc_opcode = mrq->sbc ? mrq->sbc->opcode : 0;
		__entry->sbc_arg = mrq->sbc ? mrq->sbc->arg : 0;
		__entry->sbc_flags = mrq->sbc ? mrq->sbc->flags : 0;
		__entry->sbc_retries = mrq->sbc ? mrq->sbc->retries : 0;
		__entry->blksz = mrq->data ? mrq->data->blksz : 0;
		__entry->blocks = mrq->data ? mrq->data->blocks : 0;
		__entry->data_flags = mrq->data ? mrq->data->flags : 0;
		__entry->can_retune = host->can_retune;
		__entry->doing_retune = host->doing_retune;
		__entry->retune_now = host->retune_now;
		__entry->need_retune = host->need_retune;
		__entry->hold_retune = host->hold_retune;
		__entry->retune_period = host->retune_period;
		__assign_str(name, mmc_hostname(host));
		__entry->mrq = mrq;
	),

	TP_printk("%s: start struct mmc_request[%p]: "
		  "cmd_opcode=%u cmd_arg=0x%x cmd_flags=0x%x cmd_retries=%u "
		  "stop_opcode=%u stop_arg=0x%x stop_flags=0x%x stop_retries=%u "
		  "sbc_opcode=%u sbc_arg=0x%x sbc_flags=0x%x sbc_retires=%u "
		  "blocks=%u block_size=%u data_flags=0x%x "
		  "can_retune=%u doing_retune=%u retune_now=%u "
		  "need_retune=%d hold_retune=%d retune_period=%u",
		  __get_str(name), __entry->mrq,
		  __entry->cmd_opcode, __entry->cmd_arg,
		  __entry->cmd_flags, __entry->cmd_retries,
		  __entry->stop_opcode, __entry->stop_arg,
		  __entry->stop_flags, __entry->stop_retries,
		  __entry->sbc_opcode, __entry->sbc_arg,
		  __entry->sbc_flags, __entry->sbc_retries,
		  __entry->blocks, __entry->blksz, __entry->data_flags,
		  __entry->can_retune, __entry->doing_retune,
		  __entry->retune_now, __entry->need_retune,
		  __entry->hold_retune, __entry->retune_period)
);

TRACE_EVENT(mmc_request_done,

	TP_PROTO(struct mmc_host *host, struct mmc_request *mrq),

	TP_ARGS(host, mrq),

	TP_STRUCT__entry(
		__field(u32,			cmd_opcode)
		__field(int,			cmd_err)
		__array(u32,			cmd_resp,	4)
		__field(unsigned int,		cmd_retries)
		__field(u32,			stop_opcode)
		__field(int,			stop_err)
		__array(u32,			stop_resp,	4)
		__field(unsigned int,		stop_retries)
		__field(u32,			sbc_opcode)
		__field(int,			sbc_err)
		__array(u32,			sbc_resp,	4)
		__field(unsigned int,		sbc_retries)
		__field(unsigned int,		bytes_xfered)
		__field(int,			data_err)
		__field(unsigned int,		can_retune)
		__field(unsigned int,		doing_retune)
		__field(unsigned int,		retune_now)
		__field(int,			need_retune)
		__field(int,			hold_retune)
		__field(unsigned int,		retune_period)
		__field(struct mmc_request *,	mrq)
		__string(name,			mmc_hostname(host))
	),

	TP_fast_assign(
		__entry->cmd_opcode = mrq->cmd->opcode;
		__entry->cmd_err = mrq->cmd->error;
		memcpy(__entry->cmd_resp, mrq->cmd->resp, 4);
		__entry->cmd_retries = mrq->cmd->retries;
		__entry->stop_opcode = mrq->stop ? mrq->stop->opcode : 0;
		__entry->stop_err = mrq->stop ? mrq->stop->error : 0;
		__entry->stop_resp[0] = mrq->stop ? mrq->stop->resp[0] : 0;
		__entry->stop_resp[1] = mrq->stop ? mrq->stop->resp[1] : 0;
		__entry->stop_resp[2] = mrq->stop ? mrq->stop->resp[2] : 0;
		__entry->stop_resp[3] = mrq->stop ? mrq->stop->resp[3] : 0;
		__entry->stop_retries = mrq->stop ? mrq->stop->retries : 0;
		__entry->sbc_opcode = mrq->sbc ? mrq->sbc->opcode : 0;
		__entry->sbc_err = mrq->sbc ? mrq->sbc->error : 0;
		__entry->sbc_resp[0] = mrq->sbc ? mrq->sbc->resp[0] : 0;
		__entry->sbc_resp[1] = mrq->sbc ? mrq->sbc->resp[1] : 0;
		__entry->sbc_resp[2] = mrq->sbc ? mrq->sbc->resp[2] : 0;
		__entry->sbc_resp[3] = mrq->sbc ? mrq->sbc->resp[3] : 0;
		__entry->sbc_retries = mrq->sbc ? mrq->sbc->retries : 0;
		__entry->bytes_xfered = mrq->data ? mrq->data->bytes_xfered : 0;
		__entry->data_err = mrq->data ? mrq->data->error : 0;
		__entry->can_retune = host->can_retune;
		__entry->doing_retune = host->doing_retune;
		__entry->retune_now = host->retune_now;
		__entry->need_retune = host->need_retune;
		__entry->hold_retune = host->hold_retune;
		__entry->retune_period = host->retune_period;
		__assign_str(name, mmc_hostname(host));
		__entry->mrq = mrq;
	),

	TP_printk("%s: end struct mmc_request[%p]: "
		  "cmd_opcode=%u cmd_err=%d cmd_resp=0x%x 0x%x 0x%x 0x%x "
		  "cmd_retries=%u stop_opcode=%u stop_err=%d "
		  "stop_resp=0x%x 0x%x 0x%x 0x%x stop_retries=%u "
		  "sbc_opcode=%u sbc_err=%d sbc_resp=0x%x 0x%x 0x%x 0x%x "
		  "sbc_retries=%u bytes_xfered=%u data_err=%d "
		  "can_retune=%u doing_retune=%u retune_now=%u need_retune=%d "
		  "hold_retune=%d retune_period=%u",
		  __get_str(name), __entry->mrq,
		  __entry->cmd_opcode, __entry->cmd_err,
		  __entry->cmd_resp[0], __entry->cmd_resp[1],
		  __entry->cmd_resp[2], __entry->cmd_resp[3],
		  __entry->cmd_retries,
		  __entry->stop_opcode, __entry->stop_err,
		  __entry->stop_resp[0], __entry->stop_resp[1],
		  __entry->stop_resp[2], __entry->stop_resp[3],
		  __entry->stop_retries,
		  __entry->sbc_opcode, __entry->sbc_err,
		  __entry->sbc_resp[0], __entry->sbc_resp[1],
		  __entry->sbc_resp[2], __entry->sbc_resp[3],
		  __entry->sbc_retries,
		  __entry->bytes_xfered, __entry->data_err,
		  __entry->can_retune, __entry->doing_retune,
		  __entry->retune_now, __entry->need_retune,
		  __entry->hold_retune, __entry->retune_period)
);

TRACE_EVENT(mmc_cmd_rw_start,
	TP_PROTO(unsigned int cmd, unsigned int arg, unsigned int flags),
	TP_ARGS(cmd, arg, flags),
	TP_STRUCT__entry(
		__field(unsigned int, cmd)
		__field(unsigned int, arg)
		__field(unsigned int, flags)
	),
	TP_fast_assign(
		__entry->cmd = cmd;
		__entry->arg = arg;
		__entry->flags = flags;
	),
	TP_printk("cmd=%u,arg=0x%08x,flags=0x%08x",
		  __entry->cmd, __entry->arg, __entry->flags)
);

TRACE_EVENT(mmc_cmd_rw_end,
	TP_PROTO(unsigned int cmd, unsigned int status, unsigned int resp),
	TP_ARGS(cmd, status, resp),
	TP_STRUCT__entry(
		__field(unsigned int, cmd)
		__field(unsigned int, status)
		__field(unsigned int, resp)
	),
	TP_fast_assign(
		__entry->cmd = cmd;
		__entry->status = status;
		__entry->resp = resp;
	),
	TP_printk("cmd=%u,int_status=0x%08x,response=0x%08x",
		  __entry->cmd, __entry->status, __entry->resp)
);

TRACE_EVENT(mmc_data_rw_end,
	TP_PROTO(unsigned int cmd, unsigned int status),
	TP_ARGS(cmd, status),
	TP_STRUCT__entry(
		__field(unsigned int, cmd)
		__field(unsigned int, status)
	),
	TP_fast_assign(
		__entry->cmd = cmd;
		__entry->status = status;
	),
	TP_printk("cmd=%u,int_status=0x%08x",
		  __entry->cmd, __entry->status)
);

DECLARE_EVENT_CLASS(mmc_adma_class,
	TP_PROTO(unsigned int cmd, unsigned int len),
	TP_ARGS(cmd, len),
	TP_STRUCT__entry(
		__field(unsigned int, cmd)
		__field(unsigned int, len)
	),
	TP_fast_assign(
		__entry->cmd = cmd;
		__entry->len = len;
	),
	TP_printk("cmd=%u,sg_len=0x%08x", __entry->cmd, __entry->len)
);

DEFINE_EVENT(mmc_adma_class, mmc_adma_table_pre,
	TP_PROTO(unsigned int cmd, unsigned int len),
	TP_ARGS(cmd, len));

DEFINE_EVENT(mmc_adma_class, mmc_adma_table_post,
	TP_PROTO(unsigned int cmd, unsigned int len),
	TP_ARGS(cmd, len));

TRACE_EVENT(mmc_clk,
	TP_PROTO(char *print_info),

	TP_ARGS(print_info),

	TP_STRUCT__entry(
		__string(print_info, print_info)
	),

	TP_fast_assign(
		__assign_str(print_info, print_info);
	),

	TP_printk("%s",
		__get_str(print_info)
	)
);

DECLARE_EVENT_CLASS(mmc_pm_template,
	TP_PROTO(const char *dev_name, int err, s64 usecs),

	TP_ARGS(dev_name, err, usecs),

	TP_STRUCT__entry(
		__field(s64, usecs)
		__field(int, err)
		__string(dev_name, dev_name)
	),

	TP_fast_assign(
		__entry->usecs = usecs;
		__entry->err = err;
		__assign_str(dev_name, dev_name);
	),

	TP_printk(
		"took %lld usecs, %s err %d",
		__entry->usecs,
		__get_str(dev_name),
		__entry->err
	)
);

DEFINE_EVENT(mmc_pm_template, mmc_runtime_suspend,
	     TP_PROTO(const char *dev_name, int err, s64 usecs),
	     TP_ARGS(dev_name, err, usecs));

DEFINE_EVENT(mmc_pm_template, mmc_runtime_resume,
	     TP_PROTO(const char *dev_name, int err, s64 usecs),
	     TP_ARGS(dev_name, err, usecs));

DEFINE_EVENT(mmc_pm_template, mmc_suspend,
	     TP_PROTO(const char *dev_name, int err, s64 usecs),
	     TP_ARGS(dev_name, err, usecs));

DEFINE_EVENT(mmc_pm_template, mmc_resume,
	     TP_PROTO(const char *dev_name, int err, s64 usecs),
	     TP_ARGS(dev_name, err, usecs));

DEFINE_EVENT(mmc_pm_template, sdhci_msm_suspend,
	     TP_PROTO(const char *dev_name, int err, s64 usecs),
	     TP_ARGS(dev_name, err, usecs));

DEFINE_EVENT(mmc_pm_template, sdhci_msm_resume,
	     TP_PROTO(const char *dev_name, int err, s64 usecs),
	     TP_ARGS(dev_name, err, usecs));

DEFINE_EVENT(mmc_pm_template, sdhci_msm_runtime_suspend,
	     TP_PROTO(const char *dev_name, int err, s64 usecs),
	     TP_ARGS(dev_name, err, usecs));

DEFINE_EVENT(mmc_pm_template, sdhci_msm_runtime_resume,
	     TP_PROTO(const char *dev_name, int err, s64 usecs),
	     TP_ARGS(dev_name, err, usecs));
#endif /* if !defined(_TRACE_MMC_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#include <trace/define_trace.h>

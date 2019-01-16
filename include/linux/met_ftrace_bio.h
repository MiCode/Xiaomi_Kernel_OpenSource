
#undef TRACE_SYSTEM
#define TRACE_SYSTEM met_bio

#if !defined(__TRACE_MET_FTRACE_BIO_H__) || defined(TRACE_HEADER_MULTI_READ)
#define __TRACE_MET_FTRACE_BIO_H__

#include <linux/tracepoint.h>

#include <linux/met_drv.h>
#include <linux/mmc/host.h>

DECLARE_EVENT_CLASS(met_mmc_async_req_template,
	TP_PROTO(struct mmc_blk_data *md, struct mmc_async_req *areq, char type),

	TP_ARGS(md, areq, type),

	TP_STRUCT__entry(
		__field(int, major)
		__field(int, minor)
		__array(char, lba_dir, 2)
		__field(unsigned int, start_lba)
		__field(unsigned int, lba_len)
		__array(char, comm, TASK_COMM_LEN)
	),

	TP_fast_assign(
		memset(__entry->lba_dir, 0, sizeof(__entry->lba_dir));
		__entry->major = md->disk->major;
		__entry->minor = md->disk->first_minor;
		memcpy(__entry->lba_dir, &type, sizeof(char));
		__entry->start_lba = areq->mrq->cmd->arg;
		__entry->lba_len = areq->mrq->data->blocks;
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
	),

	TP_printk("%d,%d %s %d + %d [%s]",
		  __entry->major, __entry->minor, __entry->lba_dir,
		  __entry->start_lba, __entry->lba_len, __entry->comm)
);

DECLARE_EVENT_CLASS(met_mmc_dma_async_req_template,
	TP_PROTO(struct mmc_blk_data *md, struct mmc_async_req *areq, char type, unsigned int bd_num),

	TP_ARGS(md, areq, type, bd_num),

	TP_STRUCT__entry(
		__field(int, major)
		__field(int, minor)
		__array(char, lba_dir, 2)
		__field(unsigned int, start_lba)
		__field(unsigned int, lba_len)
		__field(unsigned int, bdnum)
		__array(char, comm, TASK_COMM_LEN)
	),

	TP_fast_assign(
		memset(__entry->lba_dir, 0, sizeof(__entry->lba_dir));
		__entry->major = md->disk->major;
		__entry->minor = md->disk->first_minor;
		memcpy(__entry->lba_dir, &type, sizeof(char));
		__entry->start_lba = areq->mrq->cmd->arg;
		__entry->lba_len = areq->mrq->data->blocks;
		__entry->bdnum = bd_num;
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
	),

	TP_printk("%d,%d %s %d %d + %d [%s]",
		  __entry->major, __entry->minor, __entry->lba_dir, __entry->bdnum,
		  __entry->start_lba, __entry->lba_len, __entry->comm)
);

DECLARE_EVENT_CLASS(met_mmc_req_template,
	TP_PROTO(struct mmc_blk_data *md, struct mmc_request *req, char type),

	TP_ARGS(md, req, type),

	TP_STRUCT__entry(
		__field(int, major)
		__field(int, minor)
		__array(char, lba_dir, 2)
		__field(unsigned int, start_lba)
		__field(unsigned int, lba_len)
		__array(char, comm, TASK_COMM_LEN)
	),

	TP_fast_assign(
		memset(__entry->lba_dir, 0, sizeof(__entry->lba_dir));
		__entry->major = md->disk->major;
		__entry->minor = md->disk->first_minor;
		memcpy(__entry->lba_dir, &type, sizeof(char));
		__entry->start_lba = req->cmd->arg;
		__entry->lba_len = req->data->blocks;
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
	),

	TP_printk("%d,%d %s %d + %d [%s]",
		  __entry->major, __entry->minor, __entry->lba_dir,
		  __entry->start_lba, __entry->lba_len, __entry->comm)
);

#if 0
DECLARE_EVENT_CLASS(met_mmc_dma_req_template,
	TP_PROTO(struct mmc_blk_data *md, struct mmc_request *req, char type, unsigned int bd_num),

	TP_ARGS(md, req, type, bd_num),

	TP_STRUCT__entry(
		__field(int, major)
		__field(int, minor)
		__array(char, lba_dir, 2)
		__field(unsigned int, start_lba)
		__field(unsigned int, lba_len)
		__field(unsigned int, bdnum)
		__array(char, comm, TASK_COMM_LEN)
	),

	TP_fast_assign(
		memset(__entry->lba_dir, 0, sizeof(__entry->lba_dir));
		__entry->major = md->disk->major;
		__entry->minor = md->disk->first_minor;
		memcpy(__entry->lba_dir, &type, sizeof(char));
		__entry->start_lba = req->cmd->arg;
		__entry->lba_len = req->data->blocks;
		__entry->bdnum = bd_num;
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
	),

	TP_printk("%d,%d %s %d %d + %d [%s]",
		  __entry->major, __entry->minor, __entry->lba_dir, __entry->bdnum,
		  __entry->start_lba, __entry->lba_len, __entry->comm)
);
#endif

/*
 * Tracepoint for met_mmc_insert
 */
DEFINE_EVENT(met_mmc_async_req_template, met_mmc_insert,
	     TP_PROTO(struct mmc_blk_data *md, struct mmc_async_req *areq, char type),
	     TP_ARGS(md, areq, type));

/*
 * Tracepoint for met_mmc_dma_map
 */
DEFINE_EVENT(met_mmc_async_req_template, met_mmc_dma_map,
	     TP_PROTO(struct mmc_blk_data *md, struct mmc_async_req *areq, char type),
	     TP_ARGS(md, areq, type));

#if 0
/*
 * Tracepoint for met_mmc_issue
 */
DEFINE_EVENT(met_mmc_async_req_template, met_mmc_issue,
	     TP_PROTO(struct mmc_blk_data *md, struct mmc_async_req *areq, char type),
	     TP_ARGS(md, areq, type));
#endif

/*
 * Tracepoint for met_mmc_issue
 */
DEFINE_EVENT(met_mmc_req_template, met_mmc_issue,
	     TP_PROTO(struct mmc_blk_data *md, struct mmc_request *req, char type),
	     TP_ARGS(md, req, type));

/*
 * Tracepoint for met_mmc_complete
 */
DEFINE_EVENT(met_mmc_async_req_template, met_mmc_complete,
	     TP_PROTO(struct mmc_blk_data *md, struct mmc_async_req *areq, char type),
	     TP_ARGS(md, areq, type));

/*
 * Tracepoint for met_mmc_dma_unmap_start
 */
DEFINE_EVENT(met_mmc_async_req_template, met_mmc_dma_unmap_start,
	     TP_PROTO(struct mmc_blk_data *md, struct mmc_async_req *areq, char type),
	     TP_ARGS(md, areq, type));

/*
 * Tracepoint for met_mmc_dma_unmap_stop
 */
DEFINE_EVENT(met_mmc_async_req_template, met_mmc_dma_unmap_stop,
	     TP_PROTO(struct mmc_blk_data *md, struct mmc_async_req *areq, char type),
	     TP_ARGS(md, areq, type));

/*
 * Tracepoint for met_mmc_continue_req_end
 */
DEFINE_EVENT(met_mmc_async_req_template, met_mmc_continue_req_end,
	     TP_PROTO(struct mmc_blk_data *md, struct mmc_async_req *areq, char type),
	     TP_ARGS(md, areq, type));

/*
 * Tracepoint for met_mmc_dma_stop
 */
DEFINE_EVENT(met_mmc_dma_async_req_template, met_mmc_dma_stop,
	     TP_PROTO(struct mmc_blk_data *md, struct mmc_async_req *areq, char type, unsigned int bd_num),
	     TP_ARGS(md, areq, type, bd_num));

#if 0
/*
 * Tracepoint for met_testevent
 */
TRACE_EVENT(met_testevent,

	TP_PROTO(int cnt),

	TP_ARGS(cnt),

	TP_STRUCT__entry(
		__field(int, test_cnt)
	),

	TP_fast_assign(
		__entry->test_cnt = cnt;
	),

	TP_printk("%d",
		  __entry->test_cnt)
);
#endif

#endif /* __TRACE_MET_FTRACE_BIO_H__ */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#undef linux
#define TRACE_INCLUDE_PATH ../../../include/linux
#define TRACE_INCLUDE_FILE met_ftrace_bio
#include <trace/define_trace.h>

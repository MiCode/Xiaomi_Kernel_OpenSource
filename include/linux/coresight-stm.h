#ifndef _LINUX_CORESIGHT_STM_H
#define _LINUX_CORESIGHT_STM_H

enum {
	OST_ENTITY_NONE			= 0x00,
	OST_ENTITY_FTRACE_EVENTS	= 0x01,
	OST_ENTITY_TRACE_PRINTK		= 0x02,
	OST_ENTITY_TRACE_MARKER		= 0x04,
	OST_ENTITY_DEV_NODE		= 0x08,
	OST_ENTITY_QVIEW		= 0xFE,
	OST_ENTITY_MAX			= 0xFF,
};

enum {
	STM_OPTION_NONE			= 0x0,
	STM_OPTION_TIMESTAMPED		= 0x08,
	STM_OPTION_GUARANTEED		= 0x80,
};

#ifdef __KERNEL__
#define stm_log_inv(entity_id, proto_id, data, size)			\
	stm_trace(STM_OPTION_NONE, entity_id, proto_id, data, size)

#define stm_log_inv_ts(entity_id, proto_id, data, size)			\
	stm_trace(STM_OPTION_TIMESTAMPED, entity_id, proto_id,		\
		  data, size)

#define stm_log_gtd(entity_id, proto_id, data, size)			\
	stm_trace(STM_OPTION_GUARANTEED, entity_id, proto_id,		\
		  data, size)

#define stm_log_gtd_ts(entity_id, proto_id, data, size)			\
	stm_trace(STM_OPTION_GUARANTEED | STM_OPTION_TIMESTAMPED,	\
		  entity_id, proto_id, data, size)

#define stm_log(entity_id, data, size)					\
	stm_log_inv_ts(entity_id, 0, data, size)

#ifdef CONFIG_CORESIGHT_STM
extern int stm_trace(uint32_t options, uint8_t entity_id, uint8_t proto_id,
		     const void *data, uint32_t size);
#else
static inline int stm_trace(uint32_t options, uint8_t entity_id,
			    uint8_t proto_id, const void *data, uint32_t size)
{
	return 0;
}
#endif
#endif /* __KERNEL__ */

#endif

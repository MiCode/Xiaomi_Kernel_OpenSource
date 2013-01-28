#ifndef __LINUX_CORESIGHT_STM_H_
#define __LINUX_CORESIGHT_STM_H_

#include <uapi/linux/coresight-stm.h>

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

#endif

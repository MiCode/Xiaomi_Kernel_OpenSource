#ifndef __UAPI_LINUX_MSMB_QCA402X_H
#define __UAPI_LINUX_MSMB_QCA402X_H

#include <linux/types.h>
#include <linux/ioctl.h>
#define QCA402X_HTC

#define MSM_QCA_EVENT_ENQ_BUF  0
#define MSM_QCA_EVENT_SEND_MSG 1
#define MSM_QCA_EVENT_RECV_MSG 2

struct msm_qca_message_type {
	__u64 header;
	__u64 buff_addr;
	int fd;
	__u32 header_size;
	__u32 data_size;
	__u8 cmd;
	__u8 channel_id;
	__u8 is_ion_data;
	__u8 last_data;
};

struct msm_qca_event_type {
	__u8 cmd;
	__u8 channel_id;
};

struct msm_qca_event_list_type {
	__u64 events;
	__u32 num_events;
};

#define MSM_QCA402X_ENQUEUE_BUFFER \
	_IOWR(0xdd, 1, struct msm_qca_message_type)
#define MSM_QCA402X_SEND_MESSAGE \
	_IOWR(0xdd, 2, struct msm_qca_message_type)
#define MSM_QCA402X_RECEIVE_MESSAGE \
	_IOWR(0xdd, 3, struct msm_qca_message_type)
#define MSM_QCA402X_FLUSH_BUFFERS \
	_IOWR(0xdd, 4, struct msm_qca_event_list_type)
#define MSM_QCA402X_ABORT_MESSAGE \
	_IOWR(0xdd, 5, struct msm_qca_event_list_type)
#define MSM_QCA402X_REGISTER_EVENT \
	_IOWR(0xdd, 6, struct msm_qca_event_list_type)
#define MSM_QCA402X_UNREGISTER_EVENT \
	_IOWR(0xdd, 7, struct msm_qca_event_list_type)
#endif /*__UAPI_LINUX_MSMB_QCA402X_H*/


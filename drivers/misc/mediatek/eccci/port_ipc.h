#ifndef __PORT_IPC_H__
#define __PORT_IPC_H__

#include <linux/wait.h>
#include "ccci_core.h"

#define MAX_NUM_IPC_TASKS 10
#define CCCI_TASK_PENDING 0x01
#define IPC_MSGSVC_RVC_DONE 0x12344321
#define CCCI_IPC_MINOR_BASE 100 

/* MD <-> AP Msg_id mapping enum */
typedef enum
{
    IPC_L4C_MSG_ID_BEGIN = 0x80000000,
    IPC_L4C_MSG_ID_RANGE = 0x80,
    IPC_EL1_MSG_ID_BEGIN = IPC_L4C_MSG_ID_BEGIN + IPC_L4C_MSG_ID_RANGE,
    IPC_EL1_MSG_ID_RANGE = 0x20,
} CCCI_IPC_MSG_ID_RANGE;

struct ccci_ipc_ctrl {
	unsigned char task_id;
	unsigned char md_is_ready;
	unsigned long flag;
	wait_queue_head_t tx_wq;
	wait_queue_head_t md_rdy_wq;
	struct ccci_port *port;
};

/* IPC MD/AP id map table */
struct ipc_task_id_map
{
    u32  extq_id;            /* IPC universal mapping external queue */
    u32  task_id;            /* IPC processor internal task id */ 
};

typedef struct local_para {
	u8  ref_count;
	u8  _stub; // MD complier will align ref_count to 16bit
	u16 msg_len;
	u8  data[0];
} __attribute__ ((packed)) local_para_struct;

typedef struct peer_buff {
   u16	pdu_len; 
   u8	ref_count; 
   u8   pb_resvered; 
   u16	free_header_space; 
   u16	free_tail_space;
   u8	data[0];
} __attribute__ ((packed)) peer_buff_struct;

typedef struct {
    u32 src_mod_id;
    u32 dest_mod_id;
    u32 sap_id;
    u32 msg_id;
    struct local_para *local_para_ptr;
    struct peer_buff *peer_buff_ptr;
} ipc_ilm_t; // for conn_md

struct ccci_ipc_ilm {
    u32 src_mod_id;
    u32 dest_mod_id;
    u32 sap_id;
    u32 msg_id;
    u32 local_para_ptr;
    u32 peer_buff_ptr;
} __attribute__ ((packed)); // for MD


int port_ipc_init(struct ccci_port *port);
int port_ipc_req_match(struct ccci_port *port, struct ccci_request *req);
int port_ipc_tx_wait(struct ccci_port *port);
int port_ipc_rx_ack(struct ccci_port *port);
int port_ipc_ioctl(struct ccci_port *port, unsigned int cmd, unsigned long arg);
void port_ipc_md_state_notice(struct ccci_port *port, MD_STATE state);
int port_ipc_write_check_id(struct ccci_port *port, struct ccci_request *req);
unsigned int port_ipc_poll(struct file *fp, struct poll_table_struct *poll);
void port_ipc_kernel_read(struct ccci_port *port);

#endif //__PORT_IPC_H__
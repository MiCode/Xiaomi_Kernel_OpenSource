#ifndef __TEEI_CLIENT_MAIN_H__
#define __TEEI_CLIENT_MAIN_H__

#define TLOG_SIZE	(256 * 1024)

extern int create_nq_buffer(void);
extern unsigned long create_fp_fdrv(int buff_size);
extern unsigned long create_keymaster_fdrv(int buff_size);
extern unsigned long create_gatekeeper_fdrv(int buff_size);
extern long init_all_service_handlers(void);
extern int register_sched_irq_handler(void);
extern int register_soter_irq_handler(void);
extern int register_error_irq_handler(void);
extern int register_fp_ack_handler(void);
extern int register_keymaster_ack_handler(void);
extern int register_bdrv_handler(void);
extern int register_tlog_handler(void);
extern int register_boot_irq_handler(void);
extern int register_switch_irq_handler(void);

extern int register_ut_irq_handler(void);

extern struct teei_context *teei_create_context(int dev_count);
extern struct teei_session *teei_create_session(struct teei_context *cont);
extern int teei_client_context_init(void *private_data, void *argp);
extern int teei_client_context_close(void *private_data, void *argp);
extern int teei_client_session_init(void *private_data, void *argp);
extern int teei_client_session_open(void *private_data, void *argp);
extern int teei_client_session_close(void *private_data, void *argp);
extern int teei_client_send_cmd(void *private_data, void *argp);
extern int teei_client_operation_release(void *private_data, void *argp);
extern int teei_client_prepare_encode(void *private_data,
		struct teei_client_encode_cmd *enc,
		struct teei_encode **penc_context,
		struct teei_session **psession);
extern int teei_client_encode_uint32(void *private_data, void *argp);
extern int teei_client_encode_array(void *private_data, void *argp);
extern int teei_client_encode_mem_ref(void *private_data, void *argp);
extern int teei_client_encode_uint32_64bit(void *private_data, void *argp);
extern int teei_client_encode_array_64bit(void *private_data, void *argp);
extern int teei_client_encode_mem_ref_64bit(void *private_data, void *argp);
extern int teei_client_prepare_decode(void *private_data,
		struct teei_client_encode_cmd *dec,
		struct teei_encode **pdec_context);
extern int teei_client_decode_uint32(void *private_data, void *argp);
extern int teei_client_decode_array_space(void *private_data, void *argp);
extern int teei_client_get_decode_type(void *private_data, void *argp);
extern int teei_client_shared_mem_alloc(void *private_data, void *argp);
extern int teei_client_shared_mem_free(void *private_data, void *argp);
extern int teei_client_close_session_for_service(
		void *private_data,
		struct teei_session *temp_ses);
extern int teei_client_service_exit(void *private_data);
extern void init_tlog_entry(void);
extern int global_fn(void);

extern long create_tlog_thread(unsigned long tlog_virt_addr, unsigned long buff_size);
extern long create_utgate_log_thread(unsigned long tlog_virt_addr, unsigned long buff_size);

struct semaphore api_lock;
extern unsigned long fp_buff_addr;
extern unsigned long keymaster_buff_addr;
extern unsigned long gatekeeper_buff_addr;

struct work_queue *secure_wq;

unsigned long fdrv_message_buff;
unsigned long bdrv_message_buff;
unsigned long tlog_message_buff;
unsigned long message_buff;

#endif /* __TEEI_CLIENT_MAIN_H__ */

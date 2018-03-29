static int print_context(void);

struct NQ_head {
	unsigned int start_index;
	unsigned int end_index;
	unsigned int Max_count;
	unsigned char reserve[20];
};

struct NQ_entry {
	unsigned int valid_flag;
	unsigned int length;
	unsigned int buffer_addr;
	unsigned char reserve[20];
};

struct teei_contexts_head {
	u32 dev_file_cnt;
	struct list_head context_list;
	struct rw_semaphore teei_contexts_sem;
};
extern struct teei_contexts_head teei_contexts_head;
extern void *tz_malloc_shared_mem(size_t size, int flags);
extern int teei_smc_call(u32 teei_cmd_type,
		u32 dev_file_id,
		u32 svc_id,
		u32 cmd_id,
		u32 context,
		u32 enc_id,
		const void *cmd_buf,
		size_t cmd_len,
		void *resp_buf,
		size_t resp_len,
		const void *meta_data,
		const void *info_data,
		size_t info_len,
		int *ret_resp_len,
		int *error_code,
		struct semaphore *psema);

extern void tz_free_shared_mem(void *addr, size_t size);
extern void *tz_malloc_shared_mem(size_t size, int flags);

extern void *tz_malloc(size_t size, int flags);


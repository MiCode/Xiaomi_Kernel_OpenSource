#include <linux/semaphore.h>
#define TEE_NAME_SIZE   (255)

struct teei_encode {
	struct list_head head;
	int encode_id;
	void *ker_req_data_addr;
	void *ker_res_data_addr;
	u32  enc_req_offset;
	u32  enc_res_offset;
	u32  enc_req_pos;
	u32  enc_res_pos;
	u32  dec_res_pos;
	u32  dec_offset;
	struct teei_encode_meta *meta;
};

struct teei_shared_mem {
	struct list_head head;
	struct list_head s_head;
	void *index;
	void *k_addr;
	void *u_addr;
	u32  len;
};

struct teei_context {
	unsigned long cont_id;                  /* ID */
	char tee_name[TEE_NAME_SIZE];           /* Name */
	unsigned long sess_cnt;                 /* session counter */
	unsigned long shared_mem_cnt;           /* share memory counter */
	struct list_head link;                  /* link list for teei_context */
	struct list_head sess_link;                     /* link list for the sessions of this context */
	struct list_head shared_mem_list;       /* link list for the share memory of this context */
	struct semaphore cont_lock;
};

struct teei_session {
	int sess_id;                    /* ID */
	struct teei_context *parent_cont;       /* the teei_context pointer of this session */
	struct list_head link;          /* link list for teei_session */
	struct list_head encode_list;   /* link list for the encode of this session */
	struct list_head shared_mem_list;       /* link list for the share memory of this session */
};


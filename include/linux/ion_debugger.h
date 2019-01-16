#ifndef ION_DEBUGGER_DEF
#define ION_DEBUGGER_DEF
#define BACKTRACE_SIZE 10
/* #include <linux/mutex.h> */
typedef enum {
	ION_FUNCTION_OPEN,
	ION_FUNCTION_CLOSE,
	ION_FUNCTION_CREATE_CLIENT,
	ION_FUNCTION_DESTROY_CLIENT,
	ION_FUNCTION_ALLOC,
	ION_FUNCTION_ALLOC_MM,
	ION_FUNCTION_ALLOC_CONT,
	ION_FUNCTION_FREE,
	ION_FUNCTION_IMPORT,
	ION_FUNCTION_MMAP,
	ION_FUNCTION_MUNMAP,
	ION_FUNCTION_SHARE,
	ION_FUNCTION_SHARE_CLOSE,
	ION_FUNCTION_CHECK_ENABLE
} ION_FUNCTION_TYPE;

typedef enum {
	BUFFER_ALLOCATION_LIST,
	BUFFER_FREE_LIST,
	ADDRESS_ALLOCATION_LIST,
	ADDRESS_FREE_LIST,
	FD_ALLOCATION_LIST,
	FD_FREE_LIST
} ION_DEBUGGER_LIST_TYPE;
typedef enum {
	ADDRESS_USER_VIRTUAL,
	ADDRESS_KERNEL_VIRTUAL,
	ADDRESS_KERNEL_PHYSICAL,
	ADDRESS_MAX
} ION_MAPPING_ADDRESS_TYPE;
typedef enum {
	RECORD_ID,
	RECORD_CLIENT,
	RECORD_HANDLE,
	RECORD_ALLOCATE_BACKTRACE_NUM,
	RECORD_FREED_BACKTRACE_NUM,
	RECORD_ALLOCATE_MAPPING_NUM,
	RECORD_FREED_MAPPING_NUM,
	RECORD_FD,
	RECORD_ADDRESS,
	RECORD_SIZE,
	RECORD_NEXT
} ION_DEBUGGDER_RECORD_DATA;
typedef enum {
	SEARCH_PID,
	SEARCH_PID_CLIENT,
	SEARCH_PROCESS_PID,
	SEARCH_BUFFER,
	SEARCH_FD_GPID,
	SEARCH_FILE,
	SEARCH_MAX
} ION_SEARCH_METHOD;
typedef enum {
	LIST_BUFFER,
	LIST_PROCESS,
	NODE_BUFFER,
	NODE_FD,
	NODE_CLIENT,
	NODE_MMAP,
	NODE_MAX
} ION_RECORD_TYPE;
typedef enum {
	HASH_NODE_CLIENT,
	HASH_NODE_HANDLE,
	HASH_NODE_BUFFER,
	HASH_NODE_USER_BACKTRACE,
	HASH_NODE_KERNEL_BACKTRACE,
	HASH_NODE_USER_MAPPING,
	HASH_NODE_KERNEL_SYMBOL,
	HASH_NODE_MAX
} ION_HASH_NODE_TYPE;
typedef enum {
	ALLOCATE_BACKTRACE_INFO,
	RELEASE_BACKTRACE_INFO,
	KERNEL_BACKTRACE,
	USER_BACKTRACE,
	BACKTRACE_MAX
} BACKTRACE_INFO;
struct mapping {
	char *name;
	unsigned int address;
	unsigned int size;
};
typedef struct ion_sys_record_param {
	pid_t group_id;
	pid_t pid;
	unsigned int action;
	unsigned int address_type;
	unsigned int address;
	unsigned int length;
	unsigned int backtrace[BACKTRACE_SIZE];
	unsigned int kernel_symbol[BACKTRACE_SIZE];
	struct mapping mapping_record[BACKTRACE_SIZE];
	unsigned int backtrace_num;
	struct ion_handle *handle;
	struct ion_client *client;
	struct ion_buffer *buffer;
	struct file *file;
	int fd;
} ion_sys_record_t;

typedef struct ion_record_ID {
	pid_t pid;
	pid_t group_pid;
	unsigned int client_address;
	struct ion_client *client;
	union {
		struct ion_buffer_record *buffer;
		struct ion_process_record *process_record;
	};
} ion_record_ID_t;
typedef struct ion_record_basic_info {
	struct ion_record_ID recordID;
	unsigned int record_type;
	unsigned int from_kernel;
	unsigned int allocate_backtrace_type;
	unsigned int *allocate_backtrace;
	unsigned int *allocate_map;
	unsigned int release_backtrace_type;
	unsigned int *release_backtrace;
	unsigned int *release_map;
} ion_record_basic_info_t;
typedef struct ion_buffer_usage_record {
	struct ion_buffer_usage_record *next;
	struct ion_record_basic_info tracking_info;
	struct ion_handle *handle;
	int fd;
	struct file *file;
	unsigned int function_type;
} ion_buffer_usage_record_t;
typedef struct ion_address_usage_record {
	struct ion_address_usage_record *next;
	struct ion_record_basic_info tracking_info;
	unsigned int address_type;
	unsigned int mapping_address;
	unsigned int size;
	int fd;
	struct ion_buffer *buffer;
} ion_address_usage_record_t;
typedef struct ion_fd_usage_record {
	struct ion_fd_usage_record *next;
	struct ion_record_basic_info tracking_info;
	int fd;
	struct ion_handle *handle;
	struct ion_buffer *buffer;
	struct file *file;
} ion_fd_usage_record_t;

typedef struct ion_client_usage_record {
	struct ion_client_usage_recrod *next;
	struct ion_record_basic_info tracking_info;
	int fd;
} ion_client_usage_record_t;
#if 0
struct ion_buffer_record {
	struct ion_buffer_record *next;
	struct ion_buffer *buffer;
	unsigned int heap_type;
	union {
		void *priv_virt;
		unsigned long priv_phys;
	};
	struct ion_buffer_usage_record *buffer_using_list;
	struct ion_buffer_usage_record *buffer_freed_list;
	struct mutex ion_buffer_usage_mutex;
	struct ion_address_usage_record *address_using_list;
	struct ion_address_usage_record *address_freed_list;
	struct mutex ion_address_usage_mutex;
	struct ion_fd_usage_record *fd_using_list;
	struct ion_fd_usage_record *fd_freed_list;
	struct mutex ion_fd_usage_mutex;
};
extern void *ion_get_list(struct ion_buffer_record *buffer, unsigned int list_type);
extern struct ion_buffer_usage_record *ion_get_inuse_buffer_record();
extern struct ion_buffer_usage_record *ion_get_freed_buffer_record();
extern unsigned int ion_get_data_from_record(void *record, unsigned int data_type);
#endif
#endif

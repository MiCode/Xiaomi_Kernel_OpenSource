#ifndef _UAPI_SMCINVOKE_H_
#define _UAPI_SMCINVOKE_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define SMCINVOKE_USERSPACE_OBJ_NULL	-1

struct smcinvoke_buf {
	uint64_t addr;
	uint64_t size;
};

struct smcinvoke_obj {
	int64_t fd;
	int32_t cb_server_fd;
	int32_t reserved;
};

union smcinvoke_arg {
	struct smcinvoke_buf b;
	struct smcinvoke_obj o;
};

/*
 * struct smcinvoke_cmd_req: This structure is transparently sent to TEE
 * @op - Operation to be performed
 * @counts - number of aruments passed
 * @result - result of invoke operation
 * @argsize - size of each of arguments
 * @args - args is pointer to buffer having all arguments
 */
struct smcinvoke_cmd_req {
	uint32_t op;
	uint32_t counts;
	int32_t result;
	uint32_t argsize;
	uint64_t args;
};

/*
 * struct smcinvoke_accept: structure to process CB req from TEE
 * @has_resp: IN: Whether IOCTL is carrying response data
 * @txn_id: OUT: An id that should be passed as it is for response
 * @result: IN: Outcome of operation op
 * @cbobj_id: OUT: Callback object which is target of operation op
 * @op: OUT: Operation to be performed on target object
 * @counts: OUT: Number of arguments, embedded in buffer pointed by
 *               buf_addr, to complete operation
 * @reserved: IN/OUT: Usage is not defined but should be set to 0.
 * @argsize: IN: Size of any argument, all of equal size, embedded
 *               in buffer pointed by buf_addr
 * @buf_len: IN: Len of buffer pointed by buf_addr
 * @buf_addr: IN: Buffer containing all arguments which are needed
 *                to complete operation op
 */
struct smcinvoke_accept {
	uint32_t has_resp;
	uint32_t txn_id;
	int32_t result;
	int32_t cbobj_id;
	uint32_t op;
	uint32_t counts;
	int32_t reserved;
	uint32_t argsize;
	uint64_t buf_len;
	uint64_t buf_addr;
};

/*
 * @cb_buf_size: IN: Max buffer size for any callback obj implemented by client
 */
struct smcinvoke_server {
	uint32_t cb_buf_size;
};

#define SMCINVOKE_IOC_MAGIC    0x98

#define SMCINVOKE_IOCTL_INVOKE_REQ \
	_IOWR(SMCINVOKE_IOC_MAGIC, 1, struct smcinvoke_cmd_req)

#define SMCINVOKE_IOCTL_ACCEPT_REQ \
	_IOWR(SMCINVOKE_IOC_MAGIC, 2, struct smcinvoke_accept)

#define SMCINVOKE_IOCTL_SERVER_REQ \
	_IOWR(SMCINVOKE_IOC_MAGIC, 3, struct smcinvoke_server)

#define SMCINVOKE_IOCTL_ACK_LOCAL_OBJ \
	_IOWR(SMCINVOKE_IOC_MAGIC, 4, int32_t)

#endif /* _UAPI_SMCINVOKE_H_ */

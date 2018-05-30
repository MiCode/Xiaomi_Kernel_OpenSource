#ifndef _UAPI_SMCINVOKE_H_
#define _UAPI_SMCINVOKE_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define SMCINVOKE_USERSPACE_OBJ_NULL	-1

struct smcinvoke_buf {
	uint64_t	addr;
	uint64_t	size;
};

struct smcinvoke_obj {
	int64_t		fd;
	int64_t		reserved;
};

union smcinvoke_arg {
	struct smcinvoke_buf	b;
	struct smcinvoke_obj	o;
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
	uint32_t	op;
	uint32_t	counts;
	int32_t		result;
	uint32_t	argsize;
	uint64_t __user args;
};

#define SMCINVOKE_IOC_MAGIC    0x98

#define SMCINVOKE_IOCTL_INVOKE_REQ \
	_IOWR(SMCINVOKE_IOC_MAGIC, 1, struct smcinvoke_cmd_req)

#endif /* _UAPI_SMCINVOKE_H_ */

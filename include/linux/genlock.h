#ifndef _GENLOCK_H_
#define _GENLOCK_H_

#ifdef __KERNEL__

struct genlock;
struct genlock_handle;

struct genlock_handle *genlock_get_handle(void);
struct genlock_handle *genlock_get_handle_fd(int fd);
void genlock_put_handle(struct genlock_handle *handle);
struct genlock *genlock_create_lock(struct genlock_handle *);
struct genlock *genlock_attach_lock(struct genlock_handle *, int fd);
int genlock_wait(struct genlock_handle *handle, u32 timeout);
/* genlock_release_lock was deprecated */
int genlock_lock(struct genlock_handle *handle, int op, int flags,
	u32 timeout);
#endif

#define GENLOCK_UNLOCK 0
#define GENLOCK_WRLOCK 1
#define GENLOCK_RDLOCK 2

#define GENLOCK_NOBLOCK       (1 << 0)
#define GENLOCK_WRITE_TO_READ (1 << 1)

struct genlock_lock {
	int fd;
	int op;
	int flags;
	int timeout;
};

#define GENLOCK_IOC_MAGIC     'G'

#define GENLOCK_IOC_NEW _IO(GENLOCK_IOC_MAGIC, 0)
#define GENLOCK_IOC_EXPORT _IOR(GENLOCK_IOC_MAGIC, 1, \
	struct genlock_lock)
#define GENLOCK_IOC_ATTACH _IOW(GENLOCK_IOC_MAGIC, 2, \
	struct genlock_lock)

/* Deprecated */
#define GENLOCK_IOC_LOCK _IOW(GENLOCK_IOC_MAGIC, 3, \
	struct genlock_lock)

/* Deprecated */
#define GENLOCK_IOC_RELEASE _IO(GENLOCK_IOC_MAGIC, 4)
#define GENLOCK_IOC_WAIT _IOW(GENLOCK_IOC_MAGIC, 5, \
	struct genlock_lock)
#define GENLOCK_IOC_DREADLOCK _IOW(GENLOCK_IOC_MAGIC, 6, \
	struct genlock_lock)
#endif

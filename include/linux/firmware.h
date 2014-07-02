#ifndef _LINUX_FIRMWARE_H
#define _LINUX_FIRMWARE_H

#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/gfp.h>

#define FW_ACTION_NOHOTPLUG 0
#define FW_ACTION_HOTPLUG 1

struct firmware {
	size_t size;
	const u8 *data;
	struct page **pages;

	/* firmware loader private fields */
	void *priv;
};

struct module;
struct device;

struct builtin_fw {
	char *name;
	void *data;
	unsigned long size;
};

/* We have to play tricks here much like stringify() to get the
   __COUNTER__ macro to be expanded as we want it */
#define __fw_concat1(x, y) x##y
#define __fw_concat(x, y) __fw_concat1(x, y)

#define DECLARE_BUILTIN_FIRMWARE(name, blob)				     \
	DECLARE_BUILTIN_FIRMWARE_SIZE(name, &(blob), sizeof(blob))

#define DECLARE_BUILTIN_FIRMWARE_SIZE(name, blob, size)			     \
	static const struct builtin_fw __fw_concat(__builtin_fw,__COUNTER__) \
	__used __section(.builtin_fw) = { name, blob, size }

#if defined(CONFIG_FW_LOADER) || (defined(CONFIG_FW_LOADER_MODULE) && defined(MODULE))
int request_firmware(const struct firmware **fw, const char *name,
		     struct device *device);
int request_firmware_nowait(
	struct module *module, bool uevent,
	const char *name, struct device *device, gfp_t gfp, void *context,
	void (*cont)(const struct firmware *fw, void *context));
int request_firmware_direct(const char *name, struct device *device,
			    phys_addr_t dest_addr, size_t dest_size,
			    void * (*map_fw_mem)(phys_addr_t phys,
						 size_t size, void *data),
			    void (*unmap_fw_mem)(void *virt, size_t size,
							void *data),
			    void *data);
int request_firmware_nowait_direct(
	struct module *module, bool uevent,
	const char *name, struct device *device, gfp_t gfp, void *context,
	void (*cont)(const struct firmware *fw, void *context),
	phys_addr_t dest_addr, size_t dest_size,
	void * (*map_fw_mem)(phys_addr_t phys, size_t size, void *data),
	void (*unmap_fw_mem)(void *virt, size_t size, void *data), void *data);
void release_firmware(const struct firmware *fw);
int cache_firmware(const char *name);
int uncache_firmware(const char *name);
#else
static inline int request_firmware(const struct firmware **fw,
				   const char *name,
				   struct device *device)
{
	return -EINVAL;
}
static inline int request_firmware_direct(const char *name,
					  struct device *device,
					  phys_addr_t dest_addr,
					  size_t dest_size,
					  void * (*map_fw_mem)(phys_addr_t phys,
						       size_t size, void *data),
					  void (*unmap_fw_mem)(void *virt),
					  void *data)
{
	return -EINVAL;
}
static inline int request_firmware_nowait(
	struct module *module, bool uevent,
	const char *name, struct device *device, gfp_t gfp, void *context,
	void (*cont)(const struct firmware *fw, void *context))
{
	return -EINVAL;
}
static inline int request_firmware_nowait_direct(
	struct module *module, bool uevent,
	const char *name, struct device *device, gfp_t gfp, void *context,
	void (*cont)(const struct firmware *fw, void *context),
	phys_addr_t dest_addr, size_t dest_size,
	void * (*map_fw_mem)(phys_addr_t phys, size_t size, void *data),
	void (*unmap_fw_mem)(void *virt, void *data), void *data)
{
	return -EINVAL;
}
static inline void release_firmware(const struct firmware *fw)
{
}

static inline int cache_firmware(const char *name)
{
	return -ENOENT;
}

static inline int uncache_firmware(const char *name)
{
	return -EINVAL;
}
#endif

#endif

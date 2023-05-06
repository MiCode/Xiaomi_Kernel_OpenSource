
#ifndef XM_ADAPTER_CLASS_H
#define XM_ADAPTER_CLASS_H

#define PD_ROLE_SINK_FOR_ADAPTER   0
#define PD_ROLE_SOURCE_FOR_ADAPTER 1

#define to_adapter_device(obj) container_of(obj, struct adapter_device, dev)
struct adapter_device *adapter_device_register(const char *name,
		struct device *parent, void *devdata,
		const struct adapter_ops *ops,
		const struct adapter_properties *props);
void adapter_device_unregister(struct adapter_device *adapter_dev);
struct adapter_device *get_adapter_by_name(const char *name);
void adapter_class_exit(void);
int adapter_class_init(void);

#endif /*XM_ADAPTER_CLASS_H*/


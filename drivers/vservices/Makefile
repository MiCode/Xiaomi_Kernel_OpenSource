ccflags-y += -Werror
ccflags-$(CONFIG_VSERVICES_DEBUG) += -DDEBUG

obj-$(CONFIG_VSERVICES_SUPPORT)	+= vservices.o
vservices-objs-$(CONFIG_VSERVICES_CHAR_DEV) += devio.o
vservices-objs = session.o $(vservices-objs-y)

obj-$(CONFIG_VSERVICES_CLIENT) += core_client.o
obj-$(CONFIG_VSERVICES_SERVER) += core_server.o

obj-$(CONFIG_VSERVICES_SKELETON_DRIVER) += vservices_skeleton_driver.o
vservices_skeleton_driver-objs = skeleton_driver.o

obj-$(CONFIG_VSERVICES_SUPPORT) += transport/

obj-$(CONFIG_VSERVICES_SUPPORT) += protocol/

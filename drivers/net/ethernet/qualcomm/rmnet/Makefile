#
# Makefile for the RMNET module
#

CFLAGS_rmnet_handlers.o = -I$(src)

rmnet-y		 := rmnet_config.o
rmnet-y		 += rmnet_vnd.o
rmnet-y		 += rmnet_handlers.o
rmnet-y		 += rmnet_map_data.o
rmnet-y		 += rmnet_map_command.o
rmnet-y		 += rmnet_descriptor.o
rmnet-y		 += rmnet_genl.o
obj-$(CONFIG_RMNET) += rmnet.o

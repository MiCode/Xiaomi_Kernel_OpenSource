#
# Makefile for the RMNET module
#

rmnet-y		 := rmnet_main.o
rmnet-y		 += rmnet_config.o
rmnet-y		 += rmnet_vnd.o
rmnet-y		 += rmnet_handlers.o
rmnet-y		 += rmnet_map_data.o
rmnet-y		 += rmnet_map_command.o
rmnet-y		 += rmnet_stats.o
obj-$(CONFIG_RMNET) += rmnet.o

CFLAGS_rmnet_main.o := -I$(src)

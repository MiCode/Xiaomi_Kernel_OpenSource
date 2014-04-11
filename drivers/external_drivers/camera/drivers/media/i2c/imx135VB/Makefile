obj-$(CONFIG_VIDEO_IMXVB) += imx1x5.o

# Select the correct sensor objects.
imx1x5-objs-$(CONFIG_VIDEO_IMXVB)	+= dw9719.o

imx1x5-objs	:= imx.o $(imx1x5-objs-m)

ccflags-y += -Werror

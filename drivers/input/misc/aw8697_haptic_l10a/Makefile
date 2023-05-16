
ifeq ($(TARGET_PRODUCT),lmi)
CDEFINES += -DUSE_CONT_F0_CALI
KBUILD_CPPFLAGS += $(CDEFINES)
endif

aw8697-haptic-objs = aw_haptic.o ringbuffer.o aw8697.o aw86927.o
obj-$(CONFIG_INPUT_AW8697_HAPTIC)	+= aw8697-haptic.o

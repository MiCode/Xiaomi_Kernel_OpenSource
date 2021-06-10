# 32-bit x86 compiler
ifeq ($(MULTIARCH),1)
 # Ignore MULTIARCH setting if this is a 32-bit build
 ifeq ($(ARCH),i386)
  TARGET_PRIMARY_ARCH   := target_i686
 else
  TARGET_SECONDARY_ARCH := target_i686
 endif
else
 TARGET_PRIMARY_ARCH    := target_i686
endif

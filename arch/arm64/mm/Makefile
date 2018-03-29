sr-ccflags-y += -Werror

obj-y				:= dma-mapping.o extable.o fault.o init.o \
				   cache.o copypage.o flush.o \
				   ioremap.o mmap.o pgd.o mmu.o \
				   context.o proc.o pageattr.o
obj-$(CONFIG_HUGETLB_PAGE)	+= hugetlbpage.o

MTK_PLATFORM := $(CONFIG_MTK_PLATFORM:"%"=%)
KBUILD_CPPFLAGS += -Werror -I$(srctree)/drivers/misc/mediatek/include

obj-$(CONFIG_MEDIATEK_SOLUTION)	+= mt_cache_v8.o
obj-$(CONFIG_MEDIATEK_SOLUTION)	+= mt_innercache.o

obj-$(CONFIG_ARM64_PTDUMP)	+= dump.o

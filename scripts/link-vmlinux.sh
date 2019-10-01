#!/bin/sh
# SPDX-License-Identifier: GPL-2.0
#
# link vmlinux
#
# vmlinux is linked from the objects selected by $(KBUILD_VMLINUX_INIT) and
# $(KBUILD_VMLINUX_MAIN) and $(KBUILD_VMLINUX_LIBS). Most are built-in.a files
# from top-level directories in the kernel tree, others are specified in
# arch/$(ARCH)/Makefile. Ordering when linking is important, and
# $(KBUILD_VMLINUX_INIT) must be first. $(KBUILD_VMLINUX_LIBS) are archives
# which are linked conditionally (not within --whole-archive), and do not
# require symbol indexes added.
#
# vmlinux
#   ^
#   |
#   +-< $(KBUILD_VMLINUX_INIT)
#   |   +--< init/version.o + more
#   |
#   +--< $(KBUILD_VMLINUX_MAIN)
#   |    +--< drivers/built-in.a mm/built-in.a + more
#   |
#   +--< $(KBUILD_VMLINUX_LIBS)
#   |    +--< lib/lib.a + more
#   |
#   +-< ${kallsymso} (see description in KALLSYMS section)
#
# vmlinux version (uname -v) cannot be updated during normal
# descending-into-subdirs phase since we do not yet know if we need to
# update vmlinux.
# Therefore this step is delayed until just before final link of vmlinux.
#
# System.map is generated to document addresses of all kernel symbols

# Error out on error
set -e

# Nice output in kbuild format
# Will be supressed by "make -s"
info()
{
	if [ "${quiet}" != "silent_" ]; then
		printf "  %-7s %s\n" ${1} ${2}
	fi
}

# Thin archive build here makes a final archive with symbol table and indexes
# from vmlinux objects INIT and MAIN, which can be used as input to linker.
# KBUILD_VMLINUX_LIBS archives should already have symbol table and indexes
# added.
#
# Traditional incremental style of link does not require this step
#
# built-in.a output file
#
archive_builtin()
{
	info AR built-in.a
	rm -f built-in.a;
	${AR} rcsTP${KBUILD_ARFLAGS} built-in.a			\
				${KBUILD_VMLINUX_INIT}		\
				${KBUILD_VMLINUX_MAIN}

	# rebuild with llvm-ar to update the symbol table
	if [ -n "${CONFIG_LTO_CLANG}" ]; then
		mv -f built-in.a built-in.a.tmp
		${LLVM_AR} rcsT${KBUILD_ARFLAGS} built-in.a $(${AR} t built-in.a.tmp)
		rm -f built-in.a.tmp
	fi
}

# If CONFIG_LTO_CLANG is selected, collect generated symbol versions into
# .tmp_symversions
modversions()
{
	if [ -z "${CONFIG_LTO_CLANG}" ]; then
		return
	fi

	if [ -z "${CONFIG_MODVERSIONS}" ]; then
		return
	fi

	rm -f .tmp_symversions

	for a in built-in.a ${KBUILD_VMLINUX_LIBS}; do
		for o in $(${AR} t $a); do
			if [ -f ${o}.symversions ]; then
				cat ${o}.symversions >> .tmp_symversions
			fi
		done
	done

	echo "-T .tmp_symversions"
}

# Link of vmlinux.o used for section mismatch analysis
# ${1} output file
modpost_link()
{
	local objects

	objects="--whole-archive				\
		built-in.a					\
		--no-whole-archive				\
		--start-group					\
		${KBUILD_VMLINUX_LIBS}				\
		--end-group"

	if [ -n "${CONFIG_LTO_CLANG}" ]; then
		# This might take a while, so indicate that we're doing
		# an LTO link
		info LTO vmlinux.o
	fi

	${LD} ${KBUILD_LDFLAGS} -r -o ${1} $(modversions) ${objects}
}

# If CONFIG_LTO_CLANG is selected, we postpone running recordmcount until
# we have compiled LLVM IR to an object file.
recordmcount()
{
	if [ -z "${CONFIG_LTO_CLANG}" ]; then
		return
	fi

	if [ -n "${CONFIG_FTRACE_MCOUNT_RECORD}" ]; then
		scripts/recordmcount ${RECORDMCOUNT_FLAGS} $*
	fi
}

# Link of vmlinux
# ${1} - optional extra .o files
# ${2} - output file
vmlinux_link()
{
	local lds="${objtree}/${KBUILD_LDS}"
	local objects

	if [ "${SRCARCH}" != "um" ]; then
		if [ -z "${CONFIG_LTO_CLANG}" ]; then
			objects="--whole-archive		\
				built-in.a			\
				--no-whole-archive		\
				--start-group			\
				${KBUILD_VMLINUX_LIBS}		\
				--end-group			\
				${1}"
		else
			objects="--start-group			\
				vmlinux.o			\
				--end-group			\
				${1}"
		fi

		${LD} ${KBUILD_LDFLAGS} ${LDFLAGS_vmlinux} -o ${2}	\
			-T ${lds} ${objects}
	else
		objects="-Wl,--whole-archive			\
			built-in.a				\
			-Wl,--no-whole-archive			\
			-Wl,--start-group			\
			${KBUILD_VMLINUX_LIBS}			\
			-Wl,--end-group				\
			${1}"

		${CC} ${CFLAGS_vmlinux} -o ${2}			\
			-Wl,-T,${lds}				\
			${objects}				\
			-lutil -lrt -lpthread
		rm -f linux
	fi
}

# Create ${2} .o file with all symbols from the ${1} object file
kallsyms()
{
	info KSYM ${2}
	local kallsymopt;

	if [ -n "${CONFIG_KALLSYMS_ALL}" ]; then
		kallsymopt="${kallsymopt} --all-symbols"
	fi

	if [ -n "${CONFIG_KALLSYMS_ABSOLUTE_PERCPU}" ]; then
		kallsymopt="${kallsymopt} --absolute-percpu"
	fi

	if [ -n "${CONFIG_KALLSYMS_BASE_RELATIVE}" ]; then
		kallsymopt="${kallsymopt} --base-relative"
	fi

	local aflags="${KBUILD_AFLAGS} ${KBUILD_AFLAGS_KERNEL}               \
		      ${NOSTDINC_FLAGS} ${LINUXINCLUDE} ${KBUILD_CPPFLAGS}"

	local afile="`basename ${2} .o`.S"

	${NM} -n ${1} | scripts/kallsyms ${kallsymopt} > ${afile}
	${CC} ${aflags} -c -o ${2} ${afile}
}

# Generates ${2} .o file with RTIC MP's from the ${1} object file (vmlinux)
# ${3} the file name where the sizes of the RTIC MP structure are stored
# just in case, save copy of the RTIC mp to ${4}
# Note: RTIC_MPGEN has to be set if MPGen is available
rtic_mp()
{
	# assume that RTIC_MP_O generation may fail
	RTIC_MP_O=

	local aflags="${KBUILD_AFLAGS} ${KBUILD_AFLAGS_KERNEL}               \
		      ${NOSTDINC_FLAGS} ${LINUXINCLUDE} ${KBUILD_CPPFLAGS}"

	${RTIC_MPGEN} --objcopy="${OBJCOPY}" --objdump="${OBJDUMP}" \
	--binpath='' --vmlinux=${1} --config=${KCONFIG_CONFIG} && \
	cat rtic_mp.c | ${CC} ${aflags} -c -o ${2} -x c - && \
	cp rtic_mp.c ${4} && \
	${NM} --print-size --size-sort ${2} > ${3} && \
	RTIC_MP_O=${2} || echo “RTIC MP generation has failed”
	# NM - save generated variable sizes for verification
	# RTIC_MP_O is our retval - great success if set to generated .o file
	# Echo statement above prints the error message in case any of the
	# above RTIC MP generation commands fail and it ensures rtic mp failure
	# does not cause kernel compilation to fail.
}

# Create map file with all symbols from ${1}
# See mksymap for additional details
mksysmap()
{
	${CONFIG_SHELL} "${srctree}/scripts/mksysmap" ${1} ${2}
}

sortextable()
{
	${objtree}/scripts/sortextable ${1}
}

# Delete output files in case of error
cleanup()
{
	rm -f .tmp_System.map
	rm -f .tmp_kallsyms*
	rm -f .tmp_symversions
	rm -f .tmp_vmlinux*
	rm -f built-in.a
	rm -f System.map
	rm -f vmlinux
	rm -f vmlinux.o
	rm -f .tmp_rtic_mp_sz*
	rm -f rtic_mp.*
}

on_exit()
{
	if [ $? -ne 0 ]; then
		cleanup
	fi
}
trap on_exit EXIT

on_signals()
{
	exit 1
}
trap on_signals HUP INT QUIT TERM

#
#
# Use "make V=1" to debug this script
case "${KBUILD_VERBOSE}" in
*1*)
	set -x
	;;
esac

if [ "$1" = "clean" ]; then
	cleanup
	exit 0
fi

# We need access to CONFIG_ symbols
case "${KCONFIG_CONFIG}" in
*/*)
	. "${KCONFIG_CONFIG}"
	;;
*)
	# Force using a file from the current directory
	. "./${KCONFIG_CONFIG}"
esac

# Update version
info GEN .version
if [ -r .version ]; then
	VERSION=$(expr 0$(cat .version) + 1)
	echo $VERSION > .version
else
	rm -f .version
	echo 1 > .version
fi;

# final build of init/
${MAKE} -f "${srctree}/scripts/Makefile.build" obj=init

archive_builtin

#link vmlinux.o
modpost_link vmlinux.o

# modpost vmlinux.o to check for section mismatches
${MAKE} -f "${srctree}/scripts/Makefile.modpost" vmlinux.o

if [ -n "${CONFIG_LTO_CLANG}" ]; then
	# Call recordmcount if needed
	recordmcount vmlinux.o
fi

# Generate RTIC MP placeholder compile unit of the correct size
# and add it to the list of link objects
# this needs to be done before generating kallsyms
if [ ! -z ${RTIC_MPGEN+x} ]; then
	rtic_mp vmlinux.o rtic_mp.o .tmp_rtic_mp_sz1 .tmp_rtic_mp1.c
	KBUILD_VMLINUX_LIBS+=" "
	KBUILD_VMLINUX_LIBS+=$RTIC_MP_O
fi

kallsymso=""
kallsyms_vmlinux=""
if [ -n "${CONFIG_KALLSYMS}" ]; then

	# kallsyms support
	# Generate section listing all symbols and add it into vmlinux
	# It's a three step process:
	# 1)  Link .tmp_vmlinux1 so it has all symbols and sections,
	#     but __kallsyms is empty.
	#     Running kallsyms on that gives us .tmp_kallsyms1.o with
	#     the right size
	# 2)  Link .tmp_vmlinux2 so it now has a __kallsyms section of
	#     the right size, but due to the added section, some
	#     addresses have shifted.
	#     From here, we generate a correct .tmp_kallsyms2.o
	# 3)  That link may have expanded the kernel image enough that
	#     more linker branch stubs / trampolines had to be added, which
	#     introduces new names, which further expands kallsyms. Do another
	#     pass if that is the case. In theory it's possible this results
	#     in even more stubs, but unlikely.
	#     KALLSYMS_EXTRA_PASS=1 may also used to debug or work around
	#     other bugs.
	# 4)  The correct ${kallsymso} is linked into the final vmlinux.
	#
	# a)  Verify that the System.map from vmlinux matches the map from
	#     ${kallsymso}.

	kallsymso=.tmp_kallsyms2.o
	kallsyms_vmlinux=.tmp_vmlinux2

	# step 1
	vmlinux_link "" .tmp_vmlinux1
	kallsyms .tmp_vmlinux1 .tmp_kallsyms1.o

	# step 2
	vmlinux_link .tmp_kallsyms1.o .tmp_vmlinux2
	kallsyms .tmp_vmlinux2 .tmp_kallsyms2.o

	# step 3
	size1=$(${CONFIG_SHELL} "${srctree}/scripts/file-size.sh" .tmp_kallsyms1.o)
	size2=$(${CONFIG_SHELL} "${srctree}/scripts/file-size.sh" .tmp_kallsyms2.o)

	if [ $size1 -ne $size2 ] || [ -n "${KALLSYMS_EXTRA_PASS}" ]; then
		kallsymso=.tmp_kallsyms3.o
		kallsyms_vmlinux=.tmp_vmlinux3

		vmlinux_link .tmp_kallsyms2.o .tmp_vmlinux3

		kallsyms .tmp_vmlinux3 .tmp_kallsyms3.o
	fi
fi

# Update RTIC MP object by replacing the place holder
# with actual MP data of the same size
# Also double check that object size did not change
# Note: Check initilally if RTIC_MP_O is not empty or uninitialized,
# as incase RTIC_MPGEN is set and failure occurs in RTIC_MP_O
# generation, below check for comparing object sizes fails
# due to an empty RTIC_MP_O object.
if [ ! -z ${RTIC_MP_O} ]; then
	rtic_mp "${kallsyms_vmlinux}" rtic_mp.o .tmp_rtic_mp_sz2 \
		.tmp_rtic_mp2.c
	if ! cmp -s .tmp_rtic_mp_sz1 .tmp_rtic_mp_sz2; then
		echo >&2 'ERROR: RTIC MP object files size mismatch'
		exit 1
	fi
fi

info LD vmlinux
vmlinux_link "${kallsymso}" vmlinux

if [ -n "${CONFIG_BUILDTIME_EXTABLE_SORT}" ]; then
	info SORTEX vmlinux
	sortextable vmlinux
fi

info SYSMAP System.map
mksysmap vmlinux System.map

# step a (see comment above)
if [ -n "${CONFIG_KALLSYMS}" ]; then
	mksysmap ${kallsyms_vmlinux} .tmp_System.map

	if ! cmp -s System.map .tmp_System.map; then
		echo >&2 Inconsistent kallsyms data
		echo >&2 Try "make KALLSYMS_EXTRA_PASS=1" as a workaround
		exit 1
	fi
fi

# Starting Android Q, the DTB's are part of dtb.img and not part
# of the kernel image. RTIC DTS relies on the kernel environment
# and could not build outside of the kernel. Generate RTIC DTS after
# successful kernel build if MPGen is enabled. The DTB will be
# generated with dtb.img in kernel_definitions.mk.
if [ ! -z ${RTIC_MPGEN+x} ]; then
	${RTIC_MPGEN} --objcopy="${OBJCOPY}" --objdump="${OBJDUMP}" \
		--binpath="" --vmlinux="vmlinux" --config=${KCONFIG_CONFIG} \
		--cc="${CC} ${KBUILD_AFLAGS}" --dts=rtic_mp.dts \
		|| echo “RTIC MP DTS generation has failed”
	# Echo statement above prints the error message in case above
	# RTIC MP DTS generation command fails and it ensures rtic mp
	# failure does not cause kernel compilation to fail.
fi

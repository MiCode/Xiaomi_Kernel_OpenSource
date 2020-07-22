#!/bin/sh
# SPDX-License-Identifier: GPL-2.0
#
# link vmlinux
#
# vmlinux is linked from the objects selected by $(KBUILD_VMLINUX_OBJS) and
# $(KBUILD_VMLINUX_LIBS). Most are built-in.a files from top-level directories
# in the kernel tree, others are specified in arch/$(ARCH)/Makefile.
# $(KBUILD_VMLINUX_LIBS) are archives which are linked conditionally
# (not within --whole-archive), and do not require symbol indexes added.
#
# vmlinux
#   ^
#   |
#   +--< $(KBUILD_VMLINUX_OBJS)
#   |    +--< init/built-in.a drivers/built-in.a mm/built-in.a + more
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
		printf "  %-7s %s\n" "${1}" "${2}"
	fi
}

# If CONFIG_LTO_CLANG is selected, generate a linker script to ensure correct
# ordering of initcalls, and with CONFIG_MODVERSIONS also enabled, collect the
# previously generated symbol versions into the same script.
lto_lds()
{
	if [ -z "${CONFIG_LTO_CLANG}" ]; then
		return
	fi

	${srctree}/scripts/generate_initcall_order.pl \
		${KBUILD_VMLINUX_OBJS} ${KBUILD_VMLINUX_LIBS} \
		> .tmp_lto.lds

	if [ -n "${CONFIG_MODVERSIONS}" ]; then
		for a in ${KBUILD_VMLINUX_OBJS} ${KBUILD_VMLINUX_LIBS}; do
			for o in $(${AR} t $a 2>/dev/null); do
				if [ -f ${o}.symversions ]; then
					cat ${o}.symversions >> .tmp_lto.lds
				fi
			done
		done
	fi

	echo "-T .tmp_lto.lds"
}

# Link of vmlinux.o used for section mismatch analysis
# ${1} output file
modpost_link()
{
	local objects

	objects="--whole-archive				\
		${KBUILD_VMLINUX_OBJS}				\
		--no-whole-archive				\
		--start-group					\
		${KBUILD_VMLINUX_LIBS}				\
		--end-group"

	if [ -n "${CONFIG_LTO_CLANG}" ]; then
		# This might take a while, so indicate that we're doing
		# an LTO link
		info LTO ${1}
	else
		info LD ${1}
	fi

	${LD} ${KBUILD_LDFLAGS} -r -o ${1} $(lto_lds) ${objects}
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
# ${1} - output file
# ${2}, ${3}, ... - optional extra .o files
vmlinux_link()
{
	local lds="${objtree}/${KBUILD_LDS}"
	local output=${1}
	local objects
	local strip_debug

	info LD ${output}

	# skip output file argument
	shift

	# The kallsyms linking does not need debug symbols included.
	if [ "$output" != "${output#.tmp_vmlinux.kallsyms}" ] ; then
		strip_debug=-Wl,--strip-debug
	fi

	if [ "${SRCARCH}" != "um" ]; then
		if [ -n "${CONFIG_LTO_CLANG}" ]; then
			# Use vmlinux.o instead of performing the slow LTO
			# link again.
			objects="--whole-archive		\
				vmlinux.o 			\
				--no-whole-archive		\
				${@}"

			if [ -n "${CONFIG_QCOM_RTIC}" ] &&	\
				[ -n "${RTIC_MP_O}" ]; then
				objects=${objects}" "${RTIC_MP_O}
			fi
		else
			objects="--whole-archive		\
				${KBUILD_VMLINUX_OBJS}		\
				--no-whole-archive		\
				--start-group			\
				${KBUILD_VMLINUX_LIBS}		\
				--end-group			\
				${@}"
		fi

		${LD} ${KBUILD_LDFLAGS} ${LDFLAGS_vmlinux}	\
			${strip_debug#-Wl,}			\
			-o ${output}				\
			-T ${lds} ${objects}
	else
		objects="-Wl,--whole-archive			\
			${KBUILD_VMLINUX_OBJS}			\
			-Wl,--no-whole-archive			\
			-Wl,--start-group			\
			${KBUILD_VMLINUX_LIBS}			\
			-Wl,--end-group				\
			${@}"

		${CC} ${CFLAGS_vmlinux}				\
			${strip_debug}				\
			-o ${output}				\
			-Wl,-T,${lds}				\
			${objects}				\
			-lutil -lrt -lpthread
		rm -f linux
	fi
}

# generate .BTF typeinfo from DWARF debuginfo
# ${1} - vmlinux image
# ${2} - file to dump raw BTF data into
gen_btf()
{
	local pahole_ver

	if ! [ -x "$(command -v ${PAHOLE})" ]; then
		echo >&2 "BTF: ${1}: pahole (${PAHOLE}) is not available"
		return 1
	fi

	pahole_ver=$(${PAHOLE} --version | sed -E 's/v([0-9]+)\.([0-9]+)/\1\2/')
	if [ "${pahole_ver}" -lt "113" ]; then
		echo >&2 "BTF: ${1}: pahole version $(${PAHOLE} --version) is too old, need at least v1.13"
		return 1
	fi

	vmlinux_link ${1}

	info "BTF" ${2}
	LLVM_OBJCOPY=${OBJCOPY} ${PAHOLE} -J ${1}

	# Create ${2} which contains just .BTF section but no symbols. Add
	# SHF_ALLOC because .BTF will be part of the vmlinux image. --strip-all
	# deletes all symbols including __start_BTF and __stop_BTF, which will
	# be redefined in the linker script. Add 2>/dev/null to suppress GNU
	# objcopy warnings: "empty loadable segment detected at ..."
	${OBJCOPY} --only-section=.BTF --set-section-flags .BTF=alloc,readonly \
		--strip-all ${1} ${2} 2>/dev/null
	# Change e_type to ET_REL so that it can be used to link final vmlinux.
	# Unlike GNU ld, lld does not allow an ET_EXEC input.
	printf '\1' | dd of=${2} conv=notrunc bs=1 seek=16 status=none
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

# Perform one step in kallsyms generation, including temporary linking of
# vmlinux.
kallsyms_step()
{
	kallsymso_prev=${kallsymso}
	kallsyms_vmlinux=.tmp_vmlinux.kallsyms${1}
	kallsymso=${kallsyms_vmlinux}.o

	vmlinux_link ${kallsyms_vmlinux} "${kallsymso_prev}" ${btf_vmlinux_bin_o}
	kallsyms ${kallsyms_vmlinux} ${kallsymso}
}

# Generates ${2} .o file with RTIC MP's from the ${1} object file (vmlinux)
# ${3} the file name where the sizes of the RTIC MP structure are stored
# just in case, save copy of the RTIC mp to ${4}
# Note: RTIC_MPGEN has to be set if MPGen is available
rtic_mp()
{
	if [ -n "${CONFIG_QCOM_RTIC}" ]; then
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
	fi
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
	rm -f .btf.*
	rm -f .tmp_System.map
	rm -f .tmp_lto.lds
	rm -f .tmp_vmlinux*
	rm -f System.map
	rm -f vmlinux
	rm -f vmlinux.o
if [ -n "${CONFIG_QCOM_RTIC}" ]; then
	rm -f .tmp_rtic_mp_sz*
	rm -f rtic_mp.*
fi
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
. include/config/auto.conf

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

#link vmlinux.o
modpost_link vmlinux.o

# modpost vmlinux.o to check for section mismatches
${MAKE} -f "${srctree}/scripts/Makefile.modpost" MODPOST_VMLINUX=1

if [ -n "${CONFIG_LTO_CLANG}" ]; then
	# Call recordmcount if needed
	recordmcount vmlinux.o
fi

info MODINFO modules.builtin.modinfo
${OBJCOPY} -j .modinfo -O binary vmlinux.o modules.builtin.modinfo

btf_vmlinux_bin_o=""
if [ -n "${CONFIG_DEBUG_INFO_BTF}" ]; then
	btf_vmlinux_bin_o=.btf.vmlinux.bin.o
	if ! gen_btf .tmp_vmlinux.btf $btf_vmlinux_bin_o ; then
		echo >&2 "Failed to generate BTF for vmlinux"
		echo >&2 "Try to disable CONFIG_DEBUG_INFO_BTF"
		exit 1
	fi
fi

if [ -n "${CONFIG_QCOM_RTIC}" ]; then
	# Generate RTIC MP placeholder compile unit of the correct size
	# and add it to the list of link objects
	# this needs to be done before generating kallsyms
	if [ ! -z ${RTIC_MPGEN+x} ]; then
		rtic_mp vmlinux.o rtic_mp.o .tmp_rtic_mp_sz1 .tmp_rtic_mp1.c
		KBUILD_VMLINUX_LIBS=$KBUILD_VMLINUX_LIBS" "$RTIC_MP_O
	fi
fi

kallsymso=""
kallsymso_prev=""
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

	kallsyms_step 1
	kallsyms_step 2

	# step 3
	size1=$(${CONFIG_SHELL} "${srctree}/scripts/file-size.sh" ${kallsymso_prev})
	size2=$(${CONFIG_SHELL} "${srctree}/scripts/file-size.sh" ${kallsymso})

	if [ $size1 -ne $size2 ] || [ -n "${KALLSYMS_EXTRA_PASS}" ]; then
		kallsyms_step 3
	fi
fi

if [ -n "${CONFIG_QCOM_RTIC}" ]; then
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
fi

vmlinux_link vmlinux "${kallsymso}" ${btf_vmlinux_bin_o}

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

if [ -n "${CONFIG_QCOM_RTIC}" ]; then
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
fi

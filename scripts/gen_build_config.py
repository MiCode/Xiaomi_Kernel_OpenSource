# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2020 Mediatek Inc.

#!/usr/bin/env python

from argparse import ArgumentParser, FileType, ArgumentDefaultsHelpFormatter
import os
import sys
import stat
import shutil
import re

def get_rel_path(path):
    rel_path = ''
    split_path = path.split("/")
    count = 0
    while True:
       if count == len(split_path):
           break
       count = count + 1
       rel_path = '%s../' % (rel_path)
    return '%s' % (rel_path.rstrip('/'))

# Parse project defconfig to get special config file, build config file and out-of-tree kernel modules
def get_config_in_defconfig(file_name, kernel_dir):
    file_handle = open(file_name, 'r')
    pattern_cficlang = re.compile('^CONFIG_CFI_CLANG\s*=\s*(.+)$')
    pattern_buildconfig = re.compile('^CONFIG_BUILD_CONFIG_FILE\s*=\s*(.+)$')
    pattern_extmodules = re.compile('^CONFIG_EXT_MODULES\s*=\s*(.+)$')
    special_defconfig = ''
    build_config = ''
    ext_modules = ''
    for line in file_handle.readlines():
        result = pattern_cficlang.match(line)
        if not result:
            special_defconfig = "gki_defconfig"
        result = pattern_buildconfig.match(line)
        if result:
            build_config = result.group(1).strip('"')
        result = pattern_extmodules.match(line)
        if result:
            ext_modules = result.group(1).strip('"')
    file_handle.close()
    return (special_defconfig, build_config, ext_modules)

def help():
    print 'Usage:'
    print '  python scripts/gen_build_config.py --project <project> --kernel-defconfig <kernel project defconfig file> --build-mode <mode> --out-file <gen build.config>'
    print 'Or:'
    print '  python scripts/gen_build_config.py -p <project> --kernel-defconfig <kernel project defconfig file> -m <mode> -o <gen build.config>'
    print ''
    print 'Attention: Must set generated build.config, and project or kernel project defconfig file!!'
    sys.exit(2)

def main(**args):

    project = args["project"]
    kernel_defconfig = args["kernel_defconfig"]
    build_mode = args["build_mode"]
    abi_mode = args["abi_mode"]
    out_file = args["out_file"]
    if ((project == '') and (kernel_defconfig == ''))  or (out_file == ''):
        help()

    # get gen_build_config.py absolute path and name by sys.argv[0]: /***/{kernel dir}/scripts/gen_build_config.py
    # kernel directory is dir(sys.argv[0])/../.., absolute path
    current_file_path = os.path.abspath(sys.argv[0])
    abs_kernel_dir = os.path.dirname(os.path.dirname(current_file_path))

    mode_config = ''
    if (build_mode == 'eng') or (build_mode == 'userdebug'):
        mode_config = '%s.config' % (build_mode)
    project_defconfig_name = ''
    if kernel_defconfig:
        project_defconfig_name = kernel_defconfig
        pattern_project = re.compile('^(.+)_defconfig$')
        project = pattern_project.match(kernel_defconfig).group(1).strip()
    else:
        project_defconfig_name = '%s_defconfig' % (project)
    defconfig_dir = ''
    if os.path.exists('%s/arch/arm/configs/%s' % (abs_kernel_dir, project_defconfig_name)):
        defconfig_dir = 'arch/arm/comfigs'
    elif os.path.exists('%s/arch/arm64/configs/%s' % (abs_kernel_dir, project_defconfig_name)):
        defconfig_dir = 'arch/arm64/configs'
    else:
        print 'Error: cannot find project defconfig file under ' + abs_kernel_dir
        sys.exit(2)
    project_defconfig = '%s/%s/%s' % (abs_kernel_dir, defconfig_dir, project_defconfig_name)

    special_defconfig = ''
    build_config = ''
    ext_modules = ''
    kernel_dir = ''
    (special_defconfig, build_config, ext_modules) = get_config_in_defconfig(project_defconfig, os.path.basename(abs_kernel_dir))
    build_config = '%s/%s' % (abs_kernel_dir, build_config)
    file_text = []
    if os.path.exists(build_config):
      file_handle = open(build_config, 'r')
      for line in file_handle.readlines():
          line_strip = line.strip()
          pattern_cc = re.compile('^CC\s*=\s*(.+)$')
          result = pattern_cc.match(line_strip)
          if result:
              line_strip = 'CC=\"${CC_WRAPPER} %s\"' % (result.group(1).strip())
          line_strip = line_strip.replace("$$","$")
          file_text.append(line_strip)
          pattern_kernel_dir = re.compile('^KERNEL_DIR\s*=\s*(.+)$')
          result = pattern_kernel_dir.match(line_strip)
          if result:
              kernel_dir = result.group(1).strip('')
      file_handle.close()
    else:
      print 'Error: cannot get build.config under ' + abs_kernel_dir + '.'
      print 'Please check whether ' + project_defconfig + ' defined CONFIG_BUILD_CONFIG_FILE.'
      sys.exit(2)

    file_text.append("PATH=${ROOT_DIR}/prebuilts/perl/linux-x86/bin:${ROOT_DIR}/prebuilts/kernel-build-tools/linux-x86/bin:/usr/bin:/bin:$PATH")
    file_text.append("HERMETIC_TOOLCHAIN=")
    file_text.append("DTC='${OUT_DIR}/scripts/dtc/dtc'")
    file_text.append("DEPMOD=")
    file_text.append("KMI_ENFORCED=1")
    if abi_mode == 'yes':
        file_text.append("IN_KERNEL_MODULES=1")
        file_text.append("KMI_SYMBOL_LIST_MODULE_GROUPING=0")
        file_text.append("KMI_SYMBOL_LIST_ADD_ONLY=1")
    else:
        file_text.append("IN_KERNEL_MODULES=")

    all_defconfig = ''
    pre_defconfig_cmds = ''
    if not special_defconfig:
        all_defconfig = '%s %s' % (project_defconfig_name, mode_config)
    else:
        # get relative path from {kernel dir} to curret working dir
        rel_o_path = get_rel_path(kernel_dir)
        all_defconfig = '%s ../../../%s/${OUT_DIR}/%s.config %s' % (special_defconfig, rel_o_path, project, mode_config)
        pre_defconfig_cmds = 'PRE_DEFCONFIG_CMDS=\"cp -p ${KERNEL_DIR}/%s/%s ${OUT_DIR}/%s.config\"' % (defconfig_dir, project_defconfig_name, project)
    all_defconfig = 'DEFCONFIG=\"%s\"' % (all_defconfig.strip())
    file_text.append(all_defconfig)
    if pre_defconfig_cmds:
        file_text.append(pre_defconfig_cmds)

    ext_modules_list = ''
    ext_modules_file = '%s/kernel/configs/ext_modules.list' % (abs_kernel_dir)
    if os.path.exists(ext_modules_file):
      file_handle = open(ext_modules_file, 'r')
      for line in file_handle.readlines():
          line_strip = line.strip()
          ext_modules_list = '%s %s' % (ext_modules_list, line_strip)
      ext_modules_list = 'EXT_MODULES=\"%s\"' % (ext_modules_list.strip())
      file_handle.close()
    if ext_modules:
        ext_modules_list = 'EXT_MODULES=\"%s\"' % (ext_modules.strip())
    file_text.append(ext_modules_list)

    file_text.append("DIST_CMDS='cp -p ${OUT_DIR}/.config ${DIST_DIR}'")
    if special_defconfig:
        # remove useless folder generated by scripts/Makefile.build due to relative {project}.config
        if abi_mode == 'yes':
            post_defconfig_cmds = 'POST_DEFCONFIG_CMDS=\"clean_empty_folder ${POST_DEFCONFIG_CMDS}\"'
        else:
            post_defconfig_cmds = 'POST_DEFCONFIG_CMDS=\"clean_empty_folder; ${POST_DEFCONFIG_CMDS}\"'
        file_text.append(post_defconfig_cmds)
        clean_empty_folder_func = 'function clean_empty_folder() {\n\
    out_dir=${OUT_DIR}\n\
    rel_out_dir=`./${KERNEL_DIR}/scripts/get_rel_path.sh ${out_dir} ${ROOT_DIR}`\n\
    abs_out_dir=$(readlink -m ${rel_out_dir}/%s/../../../${rel_out_dir%%/$KERNEL_DIR})\n\
    if [ -d "${abs_out_dir}" ]; then\n\
        tmp_dir=${abs_out_dir}/\n\
        cd ${tmp_dir}\n\
        while [ -n "{tmp_dir}" ]; do\n\
            sub_dir=${tmp_dir##*/}\n\
            [ -n "${sub_dir}" ] && [ -z "`ls -A ${sub_dir}`" ] && rmdir ${sub_dir}\n\
            tmp_dir=${tmp_dir%%/*}\n\
            [ -n "`ls -A ${tmp_dir}`" ] && break\n\
            cd ..\n\
        done\n\
    fi\n\
    cd ${ROOT_DIR}\n}' % (rel_o_path)
        file_text.append(clean_empty_folder_func)

    gen_build_config = out_file
    gen_build_config_dir = os.path.dirname(gen_build_config)
    if not os.path.exists(gen_build_config_dir):
        os.makedirs(gen_build_config_dir)
    gen_build_config_mtk = '%s.mtk' % (gen_build_config)
    file_handle = open(gen_build_config, 'w')
    kernel_dir_line = 'KERNEL_DIR=%s' % (kernel_dir)
    file_handle.write(kernel_dir_line + '\n')
    rel_gen_build_config_dir = 'REL_GEN_BUILD_CONFIG_DIR=`./${KERNEL_DIR}/scripts/get_rel_path.sh %s ${ROOT_DIR}`' % (gen_build_config_dir)
    file_handle.write(rel_gen_build_config_dir + '\n')
    build_config_fragments = 'BUILD_CONFIG_FRAGMENTS="${KERNEL_DIR}/build.config.common ${REL_GEN_BUILD_CONFIG_DIR}/%s"' % (os.path.basename(gen_build_config_mtk))
    file_handle.write(build_config_fragments)
    file_handle.close()
    file_handle = open(gen_build_config_mtk, 'w')
    for line in file_text:
        file_handle.write(line + '\n')
    file_handle.close()

if __name__ == '__main__':
    parser = ArgumentParser(formatter_class=ArgumentDefaultsHelpFormatter)

    parser.add_argument("-p","--project", dest="project", help="specify the project to build kernel.", default="")
    parser.add_argument("--kernel-defconfig", dest="kernel_defconfig", help="special kernel project defconfig file.",default="")
    parser.add_argument("-m","--build-mode", dest="build_mode", help="specify the build mode to build kernel.", default="")
    parser.add_argument("--abi", dest="abi_mode", help="specify whether build.config is used to check ABI.", default="no")
    parser.add_argument("-o","--out-file", dest="out_file", help="specify the generated build.config file.", default="")

    args = parser.parse_args()
    main(**args.__dict__)

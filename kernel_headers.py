# Copyright 2019 Google Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Generates gen_headers_<arch>.bp or generates/checks kernel headers."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import filecmp
import os
import re
import subprocess
import sys


def gen_version_h(verbose, gen_dir, version_makefile):
  """Generate linux/version.h

  Scan the version_makefile for the version info, and then generate
  linux/version.h in the gen_dir as done in kernel Makefile function
  filechk_version.h

  Args:
    verbose: Set True to print progress messages.
    gen_dir: Where to place the generated files.
    version_makefile: The makefile that contains version info.
  Return:
    If version info not found, False. Otherwise, True.
  """

  version_re = re.compile(r'VERSION\s*=\s*(\d+)')
  patchlevel_re = re.compile(r'PATCHLEVEL\s*=\s*(\d+)')
  sublevel_re = re.compile(r'SUBLEVEL\s*=\s*(\d+)')

  version_str = None
  patchlevel_str = None
  sublevel_str = None

  if verbose:
    print('gen_version_h: processing [%s]' % version_makefile)

  with open(version_makefile, 'r') as f:
    while not version_str or not patchlevel_str or not sublevel_str:
      line = f.readline()

      if not line:
        print(
            'error: gen_version_h: failed to parse kernel version from %s' %
            version_makefile)
        return False

      line = line.rstrip()

      if verbose:
        print('gen_version_h: line is %s' % line)

      if not version_str:
        match = version_re.match(line)
        if match:
          if verbose:
            print('gen_version_h: matched version [%s]' % line)
          version_str = match.group(1)
          continue

      if not patchlevel_str:
        match = patchlevel_re.match(line)
        if match:
          if verbose:
            print('gen_version_h: matched patchlevel [%s]' % line)
          patchlevel_str = match.group(1)
          continue

      if not sublevel_str:
        match = sublevel_re.match(line)
        if match:
          if verbose:
            print('gen_version_h: matched sublevel [%s]' % line)
          sublevel_str = match.group(1)
          continue

  version = int(version_str)
  patchlevel = int(patchlevel_str)
  sublevel = int(sublevel_str)

  if verbose:
    print(
        'gen_version_h: found kernel version %d.%d.%d' %
        (version, patchlevel, sublevel))

  version_h = os.path.join(gen_dir, 'linux', 'version.h')

  with open(version_h, 'w') as f:
    # This code must match the code in Makefile in the make function
    # filechk_version.h
    version_code = (version << 16) + (patchlevel << 8) + sublevel
    f.write('#define LINUX_VERSION_CODE %d\n' % version_code)
    f.write(
        '#define KERNEL_VERSION(a,b,c) ' +
        '(((a) << 16) + ((b) << 8) + (c))\n')

  return True


def scan_arch_kbuild(verbose, arch_asm_kbuild, asm_generic_kbuild, arch_include_uapi):
  """Scan arch_asm_kbuild for generated headers.

  This function processes the Kbuild file to scan for three types of files that
  need to be generated. The first type are syscall generated headers, which are
  identified by adding to the generated-y make variable. The second type are
  generic headers, which are arch-specific headers that simply wrap the
  asm-generic counterpart, and are identified by adding to the generic-y make
  variable. The third type are mandatory headers that should be present in the
  /usr/include/asm folder.

  Args:
    verbose: Set True to print progress messages.
    arch_asm_kbuild: The Kbuild file containing lists of headers to generate.
    asm_generic_kbuild: The Kbuild file containing lists of mandatory headers.
    arch_include_uapi: Headers in /arch/<arch>/include/uapi directory
  Return:
    Two lists of discovered headers, one for generated and one for generic.
  """

  generated_y_re = re.compile(r'generated-y\s*\+=\s*(\S+)')
  generic_y_re = re.compile(r'generic-y\s*\+=\s*(\S+)')
  mandatory_y_re = re.compile(r'mandatory-y\s*\+=\s*(\S+)')

  # This loop parses arch_asm_kbuild for various kinds of headers to generate.

  if verbose:
    print('scan_arch_kbuild: processing [%s]' % arch_asm_kbuild)

  generated_list = []
  generic_list = []
  arch_include_uapi_list = [os.path.basename(x) for x in arch_include_uapi]
  mandatory_pre_list = []
  mandatory_list = []


  with open(arch_asm_kbuild, 'r') as f:
    while True:
      line = f.readline()

      if not line:
        break

      line = line.rstrip()

      if verbose:
        print('scan_arch_kbuild: line is %s' % line)

      match = generated_y_re.match(line)

      if match:
        if verbose:
          print('scan_arch_kbuild: matched [%s]' % line)
        generated_list.append(match.group(1))
        continue

      match = generic_y_re.match(line)

      if match:
        if verbose:
          print('scan_arch_kbuild: matched [%s]' % line)
        generic_list.append(match.group(1))
        continue

  # This loop parses asm_generic_kbuild for various kinds of headers to generate.

  if verbose:
    print('scan_arch_kbuild: processing [%s]' % asm_generic_kbuild)

  with open(asm_generic_kbuild, 'r') as f:
    while True:
      line = f.readline()

      if not line:
        break

      line = line.rstrip()

      if verbose:
        print('scan_arch_kbuild: line is %s' % line)

      match = mandatory_y_re.match(line)

      if match:
        if verbose:
          print('scan_arch_kbuild: matched [%s]' % line)
        mandatory_pre_list.append(match.group(1))
        continue

  # Mandatory headers need to be generated if they are not already generated.
  comb_list = generic_list + generated_list + arch_include_uapi_list
  mandatory_list = [x for x in mandatory_pre_list if x not in comb_list]
  if verbose:
    print("generic")
    for x in generic_list:
      print(x)
    print("generated")
    for x in generated_list:
      print(x)
    print("mandatory")
    for x in mandatory_list:
      print(x)
    print("arch_include_uapi_list")
    for x in arch_include_uapi_list:
      print(x)

  return (generated_list, generic_list, mandatory_list)


def gen_arch_headers(
    verbose, gen_dir, arch_asm_kbuild, asm_generic_kbuild, arch_syscall_tool, arch_syscall_tbl, arch_include_uapi):
  """Process arch-specific and asm-generic uapi/asm/Kbuild to generate headers.

  The function consists of a call to scan_arch_kbuild followed by three loops.
  The first loop generates headers found and placed in the generated_list by
  scan_arch_kbuild. The second loop generates headers found and placed in the
  generic_list by the scan_arch_kbuild. The third loop generates headers found
  in mandatory_list by scan_arch_kbuild.

  The function does some parsing of file names and tool invocations. If that
  parsing fails for some reason (e.g., we don't know how to generate the
  header) or a tool invocation fails, then this function will count that as
  an error but keep processing. In the end, the function returns the number of
  errors encountered.

  Args:
    verbose: Set True to print progress messages.
    gen_dir: Where to place the generated files.
    arch_asm_kbuild: The Kbuild file containing lists of headers to generate.
    asm_generic_kbuild: The Kbuild file containing lists of mandatory headers.
    arch_syscall_tool: The arch script that generates syscall headers, or None.
    arch_syscall_tbl: The arch script that defines syscall vectors, or None.
    arch_include_uapi: Headers in arch/<arch>/include/uapi directory.
  Return:
    The number of parsing errors encountered.
  """

  error_count = 0

  # First generate the lists

  (generated_list, generic_list, mandatory_list) = scan_arch_kbuild(verbose, arch_asm_kbuild, asm_generic_kbuild ,arch_include_uapi)

  # Now we're at the first loop, which is able to generate syscall headers
  # found in the first loop, and placed in generated_list. It's okay for this
  # list to be empty. In that case, of course, the loop does nothing.

  abi_re = re.compile(r'unistd-(\S+)\.h')

  for generated in generated_list:
    gen_h = os.path.join(gen_dir, 'asm', generated)
    match = abi_re.match(generated)

    if match:
      abi = match.group(1)

      cmd = [
          '/bin/bash',
          arch_syscall_tool,
          arch_syscall_tbl,
          gen_h,
          abi,
          '',
          '__NR_SYSCALL_BASE',
      ]

      if verbose:
        print('gen_arch_headers: cmd is %s' % cmd)

      result = subprocess.call(cmd)

      if result != 0:
        print('error: gen_arch_headers: cmd %s failed %d' % (cmd, result))
        error_count += 1
    else:
      print('error: gen_arch_headers: syscall header has bad filename: %s' % generated)
      error_count += 1

  # Now we're at the second loop, which generates wrappers from arch-specific
  # headers listed in generic_list to the corresponding asm-generic header.

  for generic in generic_list:
    wrap_h = os.path.join(gen_dir, 'asm', generic)
    with open(wrap_h, 'w') as f:
      f.write('#include <asm-generic/%s>\n' % generic)

  # Now we're at the third loop, which generates wrappers from asm
  # headers listed in mandatory_list to the corresponding asm-generic header.

  for mandatory in mandatory_list:
    wrap_h = os.path.join(gen_dir, 'asm', mandatory)
    with open(wrap_h, 'w') as f:
      f.write('#include <asm-generic/%s>\n' % mandatory)
  return error_count


def run_headers_install(verbose, gen_dir, headers_install, prefix, h):
  """Process a header through the headers_install script.

  The headers_install script does some processing of a header so that it is
  appropriate for inclusion in a userland program. This function invokes that
  script for one header file.

  The input file is a header file found in the directory named by prefix. This
  function stips the prefix from the header to generate the name of the
  processed header.

  Args:
    verbose: Set True to print progress messages.
    gen_dir: Where to place the generated files.
    headers_install: The script that munges the header.
    prefix: The prefix to strip from h to generate the output filename.
    h: The input header to process.
  Return:
    If parsing or the tool fails, False. Otherwise, True
  """

  if not h.startswith(prefix):
    print('error: expected prefix [%s] on header [%s]' % (prefix, h))
    return False

  out_h = os.path.join(gen_dir, h[len(prefix):])
  (out_h_dirname, out_h_basename) = os.path.split(out_h)
  h_dirname = os.path.dirname(h)

  cmd = [headers_install, out_h_dirname, h_dirname, out_h_basename]

  if verbose:
    print('run_headers_install: cmd is %s' % cmd)

  result = subprocess.call(cmd)

  if result != 0:
    print('error: run_headers_install: cmd %s failed %d' % (cmd, result))
    return False

  return True


def glob_headers(prefix, rel_glob, excludes):
  """Recursively scan the a directory for headers.

  This function recursively scans the directory identified by prefix for
  headers. We don't yet have a new enough version of python3 to use the
  better glob function, so right now we assume the glob is '**/*.h'.

  The function filters out any files that match the items in excludes.

  Args:
    prefix: The directory to recursively scan for headers.
    rel_glob: The shell-style glob that identifies the header pattern.
    excludes: A list of headers to exclude from the glob.
  Return:
    A list of headers discovered with excludes excluded.
  """

  # If we had python 3.5+, we could use the fancy new glob.glob.
  # full_glob = os.path.join(prefix, rel_glob)
  # full_srcs = glob.glob(full_glob, recursive=True)

  full_dirs = [prefix]
  full_srcs = []

  while full_dirs:
    full_dir = full_dirs.pop(0)
    items = sorted(os.listdir(full_dir))

    for item in items:
      full_item = os.path.join(full_dir, item)

      if os.path.isdir(full_item):
        full_dirs.append(full_item)
        continue

      if full_item in excludes:
        continue

      if full_item.endswith('.h'):
        full_srcs.append(full_item)

  return full_srcs


def find_out(verbose, module_dir, prefix, rel_glob, excludes, outs):
  """Build a list of outputs for the genrule that creates kernel headers.

  This function scans for headers in the source tree and produces a list of
  output (generated) headers.

  Args:
    verbose: Set True to print progress messages.
    module_dir: The root directory of the kernel source.
    prefix: The prefix with in the kernel source tree to search for headers.
    rel_glob: The pattern to use when matching headers under prefix.
    excludes: A list of files to exclude from the glob.
    outs: The list to populdate with the headers that will be generated.
  Return:
    The number of errors encountered.
  """

  # Turn prefix, which is relative to the soong module, to a full prefix that
  # is relative to the Android source tree.

  full_prefix = os.path.join(module_dir, prefix)

  # Convert the list of excludes, which are relative to the soong module, to a
  # set of excludes (for easy hashing), relative to the Android source tree.

  full_excludes = set()

  if excludes:
    for exclude in excludes:
      full_exclude = os.path.join(full_prefix, exclude)
      full_excludes.add(full_exclude)

  # Glob those headers.

  full_srcs = glob_headers(full_prefix, rel_glob, full_excludes)

  # Now convert the file names, which are relative to the Android source tree,
  # to be relative to the gen dir. This means stripping off the module prefix
  # and the directory within this module.

  module_dir_sep = module_dir + os.sep
  prefix_sep = prefix + os.sep

  if verbose:
    print('find_out: module_dir_sep [%s]' % module_dir_sep)
    print('find_out: prefix_sep [%s]' % prefix_sep)

  error_count = 0

  for full_src in full_srcs:
    if verbose:
      print('find_out: full_src [%s]' % full_src)

    if not full_src.startswith(module_dir_sep):
      print('error: expected %s to start with %s' % (full_src, module_dir_sep))
      error_count += 1
      continue

    local_src = full_src[len(module_dir_sep):]

    if verbose:
      print('find_out: local_src [%s]' % local_src)

    if not local_src.startswith(prefix_sep):
      print('error: expected %s to start with %s' % (local_src, prefix_sep))
      error_count += 1
      continue

    # After stripping the module directory and the prefix, we're left with the
    # name of a header that we'll generate, relative to the base of of a the
    # the include path.

    local_out = local_src[len(prefix_sep):]

    if verbose:
      print('find_out: local_out [%s]' % local_out)

    outs.append(local_out)

  return error_count


def gen_blueprints(
    verbose, header_arch, gen_dir, arch_asm_kbuild, asm_generic_kbuild, module_dir,
    rel_arch_asm_kbuild, rel_asm_generic_kbuild, arch_include_uapi, techpack_include_uapi):
  """Generate a blueprints file containing modules that invoke this script.

  This function generates a blueprints file that contains modules that
  invoke this script to generate kernel headers. We generate the blueprints
  file as needed, but we don't actually use the generated file. The blueprints
  file that we generate ends up in the out directory, and we can use it to
  detect if the checked-in version of the file (in the source directory) is out
  of date. This pattern occurs in the Android source tree in several places.

  Args:
    verbose: Set True to print progress messages.
    header_arch: The arch for which to generate headers.
    gen_dir: Where to place the generated files.
    arch_asm_kbuild: The Kbuild file containing lists of headers to generate.
    asm_generic_kbuild: The Kbuild file containing lists of mandatory headers.
    module_dir: The root directory of the kernel source.
    rel_arch_asm_kbuild: arch_asm_kbuild relative to module_dir.
  Return:
    The number of errors encountered.
  """
  error_count = 0

  # The old and new blueprints files. We generate the new one, but we need to
  # refer to the old one in the modules that we generate.
  old_gen_headers_bp = 'gen_headers_%s.bp' % header_arch
  new_gen_headers_bp = os.path.join(gen_dir, old_gen_headers_bp)

  # Tools and tool files.
  headers_install_sh = 'headers_install.sh'
  kernel_headers_py = 'kernel_headers.py'
  arm_syscall_tool = 'arch/arm/tools/syscallhdr.sh'

  # Sources
  makefile = 'Makefile'
  arm_syscall_tbl = 'arch/arm/tools/syscall.tbl'
  rel_glob = '**/*.h'
  generic_prefix = 'include/uapi'
  arch_prefix = os.path.join('arch', header_arch, generic_prefix)
  generic_src = os.path.join(generic_prefix, rel_glob)
  arch_src = os.path.join(arch_prefix, rel_glob)
  techpack_src = os.path.join('techpack/*',generic_prefix, '*',rel_glob)

  # Excluded sources, architecture specific.
  exclude_srcs = []

  if header_arch == "arm":
    exclude_srcs = ['linux/a.out.h']

  if header_arch == "arm64":
    exclude_srcs = ['linux/a.out.h', 'linux/kvm_para.h']

  # Scan the arch_asm_kbuild file for files that need to be generated and those
  # that are generic (i.e., need to be wrapped).

  (generated_list, generic_list, mandatory_list) = scan_arch_kbuild(verbose,
					arch_asm_kbuild, asm_generic_kbuild, arch_include_uapi)

  generic_out = []
  error_count += find_out(
      verbose, module_dir, generic_prefix, rel_glob, exclude_srcs, generic_out)

  arch_out = []
  error_count += find_out(
      verbose, module_dir, arch_prefix, rel_glob, None, arch_out)

  techpack_out = [x.split('include/uapi/')[1] for x in techpack_include_uapi]

  if error_count != 0:
    return error_count

  # Generate the blueprints file.

  if verbose:
    print('gen_blueprints: generating %s' % new_gen_headers_bp)

  with open(new_gen_headers_bp, 'w') as f:
    f.write('// ***** DO NOT EDIT *****\n')
    f.write('// This file is generated by %s\n' % kernel_headers_py)
    f.write('\n')
    f.write('gen_headers_srcs_%s = [\n' % header_arch)
    f.write('    "%s",\n' % rel_arch_asm_kbuild)
    f.write('    "%s",\n' % rel_asm_generic_kbuild)
    f.write('    "%s",\n' % makefile)

    if header_arch == "arm":
      f.write('    "%s",\n' % arm_syscall_tbl)

    f.write('    "%s",\n' % generic_src)
    f.write('    "%s",\n' % arch_src)
    f.write('    "%s",\n' % techpack_src)
    f.write(']\n')
    f.write('\n')

    if exclude_srcs:
      f.write('gen_headers_exclude_srcs_%s = [\n' % header_arch)
      for h in exclude_srcs:
        f.write('    "%s",\n' % os.path.join(generic_prefix, h))
      f.write(']\n')
      f.write('\n')

    f.write('gen_headers_out_%s = [\n' % header_arch)

    if generated_list:
      f.write('\n')
      f.write('    // Matching generated-y:\n')
      f.write('\n')
      for h in generated_list:
        f.write('    "asm/%s",\n' % h)

    if generic_list:
      f.write('\n')
      f.write('    // Matching generic-y:\n')
      f.write('\n')
      for h in generic_list:
        f.write('    "asm/%s",\n' % h)

    if mandatory_list:
      f.write('\n')
      f.write('    // Matching mandatory-y:\n')
      f.write('\n')
      for h in mandatory_list:
        f.write('    "asm/%s",\n' % h)

    if generic_out:
      f.write('\n')
      f.write('    // From %s\n' % generic_src)
      f.write('\n')
      for h in generic_out:
        f.write('    "%s",\n' % h)

    if arch_out:
      f.write('\n')
      f.write('    // From %s\n' % arch_src)
      f.write('\n')
      for h in arch_out:
        f.write('    "%s",\n' % h)

    if techpack_out:
      f.write('\n')
      f.write('    // From %s\n' % techpack_src)
      f.write('\n')
      for h in techpack_out:
        f.write('    "%s",\n' % h)

    f.write(']\n')
    f.write('\n')

    gen_blueprints_module_name = 'qti_generate_gen_headers_%s' % header_arch

    f.write('genrule {\n')
    f.write('    // This module generates the gen_headers_<arch>.bp file\n')
    f.write('    // (i.e., a new version of this file) so that it can be\n')
    f.write('    // checked later to ensure that it matches the checked-\n')
    f.write('    // in version (this file).\n')
    f.write('    name: "%s",\n' % gen_blueprints_module_name)
    f.write('    srcs: gen_headers_srcs_%s,\n' % header_arch)
    if exclude_srcs:
      f.write('    exclude_srcs: gen_headers_exclude_srcs_%s,\n' % header_arch)

    f.write('    tool_files: ["kernel_headers.py"],\n')
    f.write('    cmd: "python3 $(location kernel_headers.py) " +\n')
    f.write('        kernel_headers_verbose +\n')
    f.write('        "--header_arch %s " +\n' % header_arch)
    f.write('        "--gen_dir $(genDir) " +\n')
    f.write('        "--arch_asm_kbuild $(location %s) " +\n' % rel_arch_asm_kbuild)
    f.write('        "--arch_include_uapi $(locations %s) " +\n' % arch_src)
    f.write('        "--techpack_include_uapi $(locations %s) " +\n' % techpack_src)
    f.write('        "--asm_generic_kbuild $(location %s) " +\n' % rel_asm_generic_kbuild)
    f.write('        "blueprints " +\n')
    f.write('        "# $(in)",\n')
    f.write('    out: ["gen_headers_%s.bp"],\n' % header_arch)
    f.write('}\n')
    f.write('\n')

    f.write('genrule {\n')
    f.write('    name: "qti_generate_kernel_headers_%s",\n' % header_arch)
    f.write('    tools: ["%s"],\n' % headers_install_sh)
    f.write('    tool_files: [\n')
    f.write('        "%s",\n' % kernel_headers_py)

    if header_arch == "arm":
      f.write('        "%s",\n' % arm_syscall_tool)

    f.write('    ],\n')
    f.write('    srcs: gen_headers_srcs_%s +[\n' % header_arch)
    f.write('        "%s",\n' % old_gen_headers_bp)
    f.write('        ":%s",\n' % gen_blueprints_module_name)
    f.write('    ],\n')

    if exclude_srcs:
      f.write('    exclude_srcs: gen_headers_exclude_srcs_%s,\n' % header_arch)

    f.write('    cmd: "python3 $(location %s) " +\n' % kernel_headers_py)
    f.write('        kernel_headers_verbose +\n')
    f.write('        "--header_arch %s " +\n' % header_arch)
    f.write('        "--gen_dir $(genDir) " +\n')
    f.write('        "--arch_asm_kbuild $(location %s) " +\n' % rel_arch_asm_kbuild)
    f.write('        "--arch_include_uapi $(locations %s) " +\n' % arch_src)
    f.write('        "--techpack_include_uapi $(locations %s) " +\n' % techpack_src)
    f.write('        "--asm_generic_kbuild $(location %s) " +\n' % rel_asm_generic_kbuild)
    f.write('        "headers " +\n')
    f.write('        "--old_gen_headers_bp $(location %s) " +\n' % old_gen_headers_bp)
    f.write('        "--new_gen_headers_bp $(location :%s) " +\n' % gen_blueprints_module_name)
    f.write('        "--version_makefile $(location %s) " +\n' % makefile)

    if header_arch == "arm":
      f.write('        "--arch_syscall_tool $(location %s) " +\n' % arm_syscall_tool)
      f.write('        "--arch_syscall_tbl $(location %s) " +\n' % arm_syscall_tbl)

    f.write('        "--headers_install $(location %s) " +\n' % headers_install_sh)
    f.write('        "--include_uapi $(locations %s)",\n' % generic_src)
    f.write('    out: ["linux/version.h"] + gen_headers_out_%s,\n' % header_arch)
    f.write('}\n')

    return 0

def parse_bp_for_headers(file_name, headers):
  parsing_headers = False
  pattern = re.compile("gen_headers_out_[a-zA-Z0-9]+\s*=\s*\[\s*")
  with open(file_name, 'r') as f:
    for line in f:
      line = line.strip()
      if pattern.match(line):
        parsing_headers = True
        continue

      if line.find("]") != -1 and parsing_headers:
        break

      if not parsing_headers:
        continue

      if line.find("//") == 0:
        continue

      headers.add(line[1:-2])

def headers_diff(old_file, new_file):
  old_headers = set()
  new_headers = set()
  diff_detected = False

  parse_bp_for_headers(old_file, old_headers)
  parse_bp_for_headers(new_file, new_headers)

  diff = old_headers - new_headers
  if len(diff):
    diff_detected = True
    print("Headers to remove:")
    for x in diff:
      print("\t{}".format(x))

  diff = new_headers - old_headers
  if len(diff):
    diff_detected = True
    print("Headers to add:")
    for x in diff:
      print("\t{}".format(x))

  return diff_detected

def gen_headers(
    verbose, header_arch, gen_dir, arch_asm_kbuild, asm_generic_kbuild, module_dir,
    old_gen_headers_bp, new_gen_headers_bp, version_makefile,
    arch_syscall_tool, arch_syscall_tbl, headers_install, include_uapi,
    arch_include_uapi, techpack_include_uapi):
  """Generate the kernel headers.

  This script generates the version.h file, the arch-specific headers including
  syscall-related generated files and wrappers around generic files, and uses
  the headers_install tool to process other generic uapi and arch-specifc uapi
  files.

  Args:
    verbose: Set True to print progress messages.
    header_arch: The arch for which to generate headers.
    gen_dir: Where to place the generated files.
    arch_asm_kbuild: The Kbuild file containing lists of headers to generate.
    asm_generic_kbuild: The Kbuild file containing mandatory headers.
    module_dir: The root directory of the kernel source.
    old_gen_headers_bp: The old gen_headers_<arch>.bp file to check.
    new_gen_headers_bp: The new gen_headers_<arch>.bp file to check.
    version_makefile: The kernel Makefile that contains version info.
    arch_syscall_tool: The arch script that generates syscall headers.
    arch_syscall_tbl: The arch script that defines syscall vectors.
    headers_install: The headers_install tool to process input headers.
    include_uapi: The list of include/uapi header files.
    arch_include_uapi: The list of arch/<arch>/include/uapi header files.
  Return:
    The number of errors encountered.
  """

  if headers_diff(old_gen_headers_bp, new_gen_headers_bp):
    print('error: gen_headers blueprints file is out of date, suggested fix:')
    print('#######Please add or remove the above mentioned headers from %s' % (old_gen_headers_bp))
    print('then re-run the build')
    return 1

  error_count = 0

  if not gen_version_h(verbose, gen_dir, version_makefile):
    error_count += 1

  error_count += gen_arch_headers(
      verbose, gen_dir, arch_asm_kbuild, asm_generic_kbuild, arch_syscall_tool, arch_syscall_tbl ,arch_include_uapi)

  uapi_include_prefix = os.path.join(module_dir, 'include', 'uapi') + os.sep

  arch_uapi_include_prefix = os.path.join(
      module_dir, 'arch', header_arch, 'include', 'uapi') + os.sep

  for h in include_uapi:
    if not run_headers_install(
        verbose, gen_dir, headers_install,
        uapi_include_prefix, h):
      error_count += 1

  for h in arch_include_uapi:
    if not run_headers_install(
        verbose, gen_dir, headers_install,
        arch_uapi_include_prefix, h):
      error_count += 1

  for h in techpack_include_uapi:
    techpack_uapi_include_prefix = os.path.join(h.split('/include/uapi')[0], 'include', 'uapi') + os.sep
    if not run_headers_install(
        verbose, gen_dir, headers_install,
        techpack_uapi_include_prefix, h):
      error_count += 1

  return error_count

def extract_techpack_uapi_headers(verbose, module_dir):

  """EXtract list of uapi headers from techpack/* directories. We need to export
     these headers to userspace.

  Args:
      verbose: Verbose option is provided to script
      module_dir: Base directory
  Returs:
      List of uapi headers
  """

  techpack_subdir = []
  techpack_dir = os.path.join(module_dir,'techpack')
  techpack_uapi = []
  techpack_uapi_sub = []

  #get list of techpack directories under techpack/
  if os.path.isdir(techpack_dir):
    items = sorted(os.listdir(techpack_dir))
    for x in items:
      p = os.path.join(techpack_dir, x)
      if os.path.isdir(p):
        techpack_subdir.append(p)

  #Print list of subdirs obtained
  if (verbose):
    for x in techpack_subdir:
      print(x)

  #For every subdirectory get list of .h files under include/uapi and append to techpack_uapi list
  for x in techpack_subdir:
    techpack_uapi_path = os.path.join(x, 'include/uapi')
    if (os.path.isdir(techpack_uapi_path)):
      techpack_uapi_sub = []
      find_out(verbose, x, 'include/uapi', '**/*.h', None, techpack_uapi_sub)
      tmp = [os.path.join(techpack_uapi_path, y) for y in techpack_uapi_sub]
      techpack_uapi = techpack_uapi + tmp

  if (verbose):
    for x in techpack_uapi:
      print(x)

  return techpack_uapi

def main():
  """Parse command line arguments and perform top level control."""

  parser = argparse.ArgumentParser(
      description=__doc__,
      formatter_class=argparse.RawDescriptionHelpFormatter)

  # Arguments that apply to every invocation of this script.

  parser.add_argument(
      '--verbose',
      action='store_true',
      help='Print output that describes the workings of this script.')
  parser.add_argument(
      '--header_arch',
      required=True,
      help='The arch for which to generate headers.')
  parser.add_argument(
      '--gen_dir',
      required=True,
      help='Where to place the generated files.')
  parser.add_argument(
      '--arch_asm_kbuild',
      required=True,
      help='The Kbuild file containing lists of headers to generate.')
  parser.add_argument(
      '--asm_generic_kbuild',
      required=True,
      help='The Kbuild file containing lists of mandatory headers.')
  parser.add_argument(
      '--arch_include_uapi',
      required=True,
      nargs='*',
      help='The list of arch/<arch>/include/uapi header files.')
  parser.add_argument(
      '--techpack_include_uapi',
      required=True,
      nargs='*',
      help='The list of techpack/*/include/uapi header files.')

  # The modes.

  subparsers = parser.add_subparsers(
      dest='mode',
      help='Select mode')
  parser_blueprints = subparsers.add_parser(
      'blueprints',
      help='Generate the gen_headers_<arch>.bp file.')
  parser_headers = subparsers.add_parser(
      'headers',
      help='Check blueprints, then generate kernel headers.')

  # Arguments that apply to headers mode.

  parser_headers.add_argument(
      '--old_gen_headers_bp',
      required=True,
      help='The old gen_headers_<arch>.bp file to check.')
  parser_headers.add_argument(
      '--new_gen_headers_bp',
      required=True,
      help='The new gen_headers_<arch>.bp file to check.')
  parser_headers.add_argument(
      '--version_makefile',
      required=True,
      help='The kernel Makefile that contains version info.')
  parser_headers.add_argument(
      '--arch_syscall_tool',
      help='The arch script that generates syscall headers, if applicable.')
  parser_headers.add_argument(
      '--arch_syscall_tbl',
      help='The arch script that defines syscall vectors, if applicable.')
  parser_headers.add_argument(
      '--headers_install',
      required=True,
      help='The headers_install tool to process input headers.')
  parser_headers.add_argument(
      '--include_uapi',
      required=True,
      nargs='*',
      help='The list of include/uapi header files.')

  args = parser.parse_args()

  if args.verbose:
    print('mode [%s]' % args.mode)
    print('header_arch [%s]' % args.header_arch)
    print('gen_dir [%s]' % args.gen_dir)
    print('arch_asm_kbuild [%s]' % args.arch_asm_kbuild)
    print('asm_generic_kbuild [%s]' % args.asm_generic_kbuild)

  # Extract the module_dir from args.arch_asm_kbuild and rel_arch_asm_kbuild.

  rel_arch_asm_kbuild = os.path.join(
      'arch', args.header_arch, 'include/uapi/asm/Kbuild')

  suffix = os.sep + rel_arch_asm_kbuild

  if not args.arch_asm_kbuild.endswith(suffix):
    print('error: expected %s to end with %s' % (args.arch_asm_kbuild, suffix))
    return 1

  module_dir = args.arch_asm_kbuild[:-len(suffix)]

  rel_asm_generic_kbuild = os.path.join('include/uapi/asm-generic', os.path.basename(args.asm_generic_kbuild))


  if args.verbose:
    print('module_dir [%s]' % module_dir)


  if args.mode == 'blueprints':
    return gen_blueprints(
        args.verbose, args.header_arch, args.gen_dir, args.arch_asm_kbuild,
        args.asm_generic_kbuild, module_dir, rel_arch_asm_kbuild, rel_asm_generic_kbuild, args.arch_include_uapi, args.techpack_include_uapi)

  if args.mode == 'headers':
    if args.verbose:
      print('old_gen_headers_bp [%s]' % args.old_gen_headers_bp)
      print('new_gen_headers_bp [%s]' % args.new_gen_headers_bp)
      print('version_makefile [%s]' % args.version_makefile)
      print('arch_syscall_tool [%s]' % args.arch_syscall_tool)
      print('arch_syscall_tbl [%s]' % args.arch_syscall_tbl)
      print('headers_install [%s]' % args.headers_install)

    return gen_headers(
        args.verbose, args.header_arch, args.gen_dir, args.arch_asm_kbuild,
        args.asm_generic_kbuild, module_dir, args.old_gen_headers_bp, args.new_gen_headers_bp,
        args.version_makefile, args.arch_syscall_tool, args.arch_syscall_tbl,
        args.headers_install, args.include_uapi, args.arch_include_uapi, args.techpack_include_uapi)

  print('error: unknown mode: %s' % args.mode)
  return 1


if __name__ == '__main__':
  sys.exit(main())

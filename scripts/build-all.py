#! /usr/bin/env python

# Copyright (c) 2009-2014, The Linux Foundation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of The Linux Foundation nor
#       the names of its contributors may be used to endorse or promote
#       products derived from this software without specific prior written
#       permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Build the kernel for all targets using the Android build environment.

import glob
from optparse import OptionParser
import os
import re
import shutil
import subprocess
import sys

version = 'build-all.py, version 1.99'

build_dir = '../all-kernels'
make_command = ["vmlinux", "modules", "dtbs"]
all_options = {}
compile64 = os.environ.get('CROSS_COMPILE64')

def error(msg):
    sys.stderr.write("error: %s\n" % msg)

def fail(msg):
    """Fail with a user-printed message"""
    error(msg)
    sys.exit(1)

if not os.environ.get('CROSS_COMPILE'):
    fail("CROSS_COMPILE must be set in the environment")

def check_kernel():
    """Ensure that PWD is a kernel directory"""
    if (not os.path.isfile('MAINTAINERS') or
        not os.path.isfile('arch/arm/mach-msm/Kconfig')):
        fail("This doesn't seem to be an MSM kernel dir")

def check_build():
    """Ensure that the build directory is present."""
    if not os.path.isdir(build_dir):
        try:
            os.makedirs(build_dir)
        except OSError as exc:
            if exc.errno == errno.EEXIST:
                pass
            else:
                raise

failed_targets = []

class LogRunner:
    def __init__(self, logname, make_env):
        self.logname = logname
        self.fd = open(logname, 'w')
        self.make_env = make_env

    def run(self, args):
        devnull = open('/dev/null', 'r')
        proc = subprocess.Popen(args, stdin=devnull,
                env=self.make_env,
                bufsize=0,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT)
        count = 0
        # for line in proc.stdout:
        rawfd = proc.stdout.fileno()
        while True:
            line = os.read(rawfd, 1024)
            if not line:
                break
            self.fd.write(line)
            self.fd.flush()
            if all_options.verbose:
                sys.stdout.write(line)
                sys.stdout.flush()
            else:
                for i in range(line.count('\n')):
                    count += 1
                    if count == 64:
                        count = 0
                        print
                    sys.stdout.write('.')
                sys.stdout.flush()
        print
        result = proc.wait()

        self.fd.flush()
        return result

class Builder():

    def __init__(self, name, defconfig):
        self.name = name
        self.defconfig = defconfig

        self.confname = self.defconfig.split('/')[-1]

        # Determine if this is a 64-bit target based on the location
        # of the defconfig.
        self.make_env = os.environ.copy()
        if "/arm64/" in defconfig:
            if compile64:
                self.make_env['CROSS_COMPILE'] = compile64
            else:
                fail("Attempting to build 64-bit, without setting CROSS_COMPILE64")
            self.make_env['ARCH'] = 'arm64'
        else:
            self.make_env['ARCH'] = 'arm'
        self.make_env['KCONFIG_NOTIMESTAMP'] = 'true'

    def build(self):
        dest_dir = os.path.join(build_dir, self.name)
        log_name = "%s/log-%s.log" % (build_dir, self.name)
        print 'Building %s in %s log %s' % (self.name, dest_dir, log_name)
        if not os.path.isdir(dest_dir):
            os.mkdir(dest_dir)
        defconfig = self.defconfig
        dotconfig = '%s/.config' % dest_dir
        savedefconfig = '%s/defconfig' % dest_dir
        # shutil.copyfile(defconfig, dotconfig)  # Not really right.

        staging_dir = 'install_staging'
        modi_dir = '%s' % staging_dir
        hdri_dir = '%s/usr' % staging_dir
        shutil.rmtree(os.path.join(dest_dir, staging_dir), ignore_errors=True)

        with open('/dev/null', 'r') as devnull:
            subprocess.check_call(['make', 'O=%s' % dest_dir,
                self.confname], env=self.make_env,
                stdin=devnull)

        if not all_options.updateconfigs:
            # Build targets can be dependent upon the completion of
            # previous build targets, so build them one at a time.
            cmd_line = ['make',
                'INSTALL_HDR_PATH=%s' % hdri_dir,
                'INSTALL_MOD_PATH=%s' % modi_dir,
                'O=%s' % dest_dir]
            build_targets = []
            for c in make_command:
                if re.match(r'^-{1,2}\w', c):
                    cmd_line.append(c)
                else:
                    build_targets.append(c)
            build = LogRunner(log_name, self.make_env)
            for t in build_targets:
                result = build.run(cmd_line + [t])
                if result != 0:
                    if all_options.keep_going:
                        failed_targets.append(target)
                        fail_or_error = error
                    else:
                        fail_or_error = fail
                    fail_or_error("Failed to build %s, see %s" %
                            (t, build.logname))

        # Copy the defconfig back.
        if all_options.configs or all_options.updateconfigs:
            with open('/dev/null', 'r') as devnull:
                subprocess.check_call(['make', 'O=%s' % dest_dir,
                    'savedefconfig'], env=self.make_env, stdin=devnull)
            shutil.copyfile(savedefconfig, defconfig)

def update_config(file, str):
    print 'Updating %s with \'%s\'\n' % (file, str)
    with open(file, 'a') as defconfig:
        defconfig.write(str + '\n')

def scan_configs():
    """Get the full list of defconfigs appropriate for this tree."""
    names = []
    arch_pats = (
        r'[fm]sm[0-9]*_defconfig',
        r'apq*_defconfig',
        r'qsd*_defconfig',
        r'mdm*_defconfig',
	r'mpq*_defconfig',
        )
    arch64_pats = (
        r'msm_defconfig',
        )
    for p in arch_pats:
        for n in glob.glob('arch/arm/configs/' + p):
            name = os.path.basename(n)[:-10]
            names.append(Builder(name, n))
    if 'CROSS_COMPILE64' in os.environ:
        for p in arch64_pats:
            for n in glob.glob('arch/arm64/configs/' + p):
                name = os.path.basename(n)[:-10] + "-64"
                names.append(Builder(name, n))
    return names

def build_many(targets):
    print "Building %d target(s)" % len(targets)
    for target in targets:
        if all_options.updateconfigs:
            update_config(target.defconfig, all_options.updateconfigs)
        target.build()
    if failed_targets:
        fail("\n  ".join(["Failed targets:"] +
            [target.name for target in failed_targets]))

def main():
    global make_command

    check_kernel()
    check_build()

    configs = scan_configs()

    usage = ("""
           %prog [options] all                 -- Build all targets
           %prog [options] target target ...   -- List specific targets
           %prog [options] perf                -- Build all perf targets
           %prog [options] noperf              -- Build all non-perf targets""")
    parser = OptionParser(usage=usage, version=version)
    parser.add_option('--configs', action='store_true',
            dest='configs',
            help="Copy configs back into tree")
    parser.add_option('--list', action='store_true',
            dest='list',
            help='List available targets')
    parser.add_option('-v', '--verbose', action='store_true',
            dest='verbose',
            help='Output to stdout in addition to log file')
    parser.add_option('--oldconfig', action='store_true',
            dest='oldconfig',
            help='Only process "make oldconfig"')
    parser.add_option('--updateconfigs',
            dest='updateconfigs',
            help="Update defconfigs with provided option setting, "
                 "e.g. --updateconfigs=\'CONFIG_USE_THING=y\'")
    parser.add_option('-j', '--jobs', type='int', dest="jobs",
            help="Number of simultaneous jobs")
    parser.add_option('-l', '--load-average', type='int',
            dest='load_average',
            help="Don't start multiple jobs unless load is below LOAD_AVERAGE")
    parser.add_option('-k', '--keep-going', action='store_true',
            dest='keep_going', default=False,
            help="Keep building other targets if a target fails")
    parser.add_option('-m', '--make-target', action='append',
            help='Build the indicated make target (default: %s)' %
                 ' '.join(make_command))

    (options, args) = parser.parse_args()
    global all_options
    all_options = options

    if options.list:
        print "Available targets:"
        for target in configs:
            print "   %s" % target.name
        sys.exit(0)

    if options.oldconfig:
        make_command = ["oldconfig"]
    elif options.make_target:
        make_command = options.make_target

    if options.jobs:
        make_command.append("-j%d" % options.jobs)
    if options.load_average:
        make_command.append("-l%d" % options.load_average)

    if args == ['all']:
        build_many(configs)
    elif args == ['perf']:
        targets = []
        for t in configs:
            if "perf" in t.name:
                targets.append(t)
        build_many(targets)
    elif args == ['noperf']:
        targets = []
        for t in configs:
            if "perf" not in t.name:
                targets.append(t)
        build_many(targets)
    elif len(args) > 0:
        all_configs = {}
        for t in configs:
            all_configs[t.name] = t
        targets = []
        for t in args:
            if t not in all_configs:
                parser.error("Target '%s' not one of %s" % (t, all_configs.keys()))
            targets.append(all_configs[t])
        build_many(targets)
    else:
        parser.error("Must specify a target to build, or 'all'")

if __name__ == "__main__":
    main()

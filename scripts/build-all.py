#! /usr/bin/env python2

# Copyright (c) 2009-2015, The Linux Foundation. All rights reserved.
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

from collections import namedtuple
import glob
from optparse import OptionParser
import os
import re
import shutil
import subprocess
import sys
import threading
import Queue

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

BuildResult = namedtuple('BuildResult', ['status', 'messages'])

class BuildSequence(namedtuple('BuildSequence', ['log_name', 'short_name', 'steps'])):

    def set_width(self, width):
        self.width = width

    def __enter__(self):
        self.log = open(self.log_name, 'w')
    def __exit__(self, type, value, traceback):
        self.log.close()

    def run(self):
        self.status = None
        messages = ["Building: " + self.short_name]
        def printer(line):
            text = "[%-*s] %s" % (self.width, self.short_name, line)
            messages.append(text)
            self.log.write(text)
            self.log.write('\n')
        for step in self.steps:
            st = step.run(printer)
            if st:
                self.status = BuildResult(self.short_name, messages)
                break
        if not self.status:
            self.status = BuildResult(None, messages)

class BuildTracker:
    """Manages all of the steps necessary to perform a build.  The
    build consists of one or more sequences of steps.  The different
    sequences can be processed independently, while the steps within a
    sequence must be done in order."""

    def __init__(self, parallel_builds):
        self.sequence = []
        self.lock = threading.Lock()
        self.parallel_builds = parallel_builds

    def add_sequence(self, log_name, short_name, steps):
        self.sequence.append(BuildSequence(log_name, short_name, steps))

    def longest_name(self):
        longest = 0
        for seq in self.sequence:
            longest = max(longest, len(seq.short_name))
        return longest

    def __repr__(self):
        return "BuildTracker(%s)" % self.sequence

    def run_child(self, seq):
        seq.set_width(self.longest)
        tok = self.build_tokens.get()
        with self.lock:
            print "Building:", seq.short_name
        with seq:
            seq.run()
            self.results.put(seq.status)
        self.build_tokens.put(tok)

    def run(self):
        self.longest = self.longest_name()
        self.results = Queue.Queue()
        children = []
        errors = []
        self.build_tokens = Queue.Queue()
        nthreads = self.parallel_builds
        print "Building with", nthreads, "threads"
        for i in range(nthreads):
            self.build_tokens.put(True)
        for seq in self.sequence:
            child = threading.Thread(target=self.run_child, args=[seq])
            children.append(child)
            child.start()
        for child in children:
            stats = self.results.get()
            if all_options.verbose:
                with self.lock:
                    for line in stats.messages:
                        print line
                    sys.stdout.flush()
            if stats.status:
                errors.append(stats.status)
        for child in children:
            child.join()
        if errors:
            fail("\n  ".join(["Failed targets:"] + errors))

class PrintStep:
    """A step that just prints a message"""
    def __init__(self, message):
        self.message = message

    def run(self, outp):
        outp(self.message)

class MkdirStep:
    """A step that makes a directory"""
    def __init__(self, direc):
        self.direc = direc

    def run(self, outp):
        outp("mkdir %s" % self.direc)
        os.mkdir(self.direc)

class RmtreeStep:
    def __init__(self, direc):
        self.direc = direc

    def run(self, outp):
        outp("rmtree %s" % self.direc)
        shutil.rmtree(self.direc, ignore_errors=True)

class CopyfileStep:
    def __init__(self, src, dest):
        self.src = src
        self.dest = dest

    def run(self, outp):
        outp("cp %s %s" % (self.src, self.dest))
        shutil.copyfile(self.src, self.dest)

class ExecStep:
    def __init__(self, cmd, **kwargs):
        self.cmd = cmd
        self.kwargs = kwargs

    def run(self, outp):
        outp("exec: %s" % (" ".join(self.cmd),))
        with open('/dev/null', 'r') as devnull:
            proc = subprocess.Popen(self.cmd, stdin=devnull,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    **self.kwargs)
            stdout = proc.stdout
            while True:
                line = stdout.readline()
                if not line:
                    break
                line = line.rstrip('\n')
                outp(line)
            result = proc.wait()
            if result != 0:
                return ('error', result)
            else:
                return None

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
        self.log_name = "%s/log-%s.log" % (build_dir, self.name)

    def build(self):
        steps = []
        dest_dir = os.path.join(build_dir, self.name)
        log_name = "%s/log-%s.log" % (build_dir, self.name)
        steps.append(PrintStep('Building %s in %s log %s' %
            (self.name, dest_dir, log_name)))
        if not os.path.isdir(dest_dir):
            steps.append(MkdirStep(dest_dir))
        defconfig = self.defconfig
        dotconfig = '%s/.config' % dest_dir
        savedefconfig = '%s/defconfig' % dest_dir

        staging_dir = 'install_staging'
        modi_dir = '%s' % staging_dir
        hdri_dir = '%s/usr' % staging_dir
        steps.append(RmtreeStep(os.path.join(dest_dir, staging_dir)))

        steps.append(ExecStep(['make', 'O=%s' % dest_dir,
            self.confname], env=self.make_env))

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
            for t in build_targets:
                steps.append(ExecStep(cmd_line + [t], env=self.make_env))

        # Copy the defconfig back.
        if all_options.configs or all_options.updateconfigs:
            steps.append(ExecStep(['make', 'O=%s' % dest_dir,
                'savedefconfig'], env=self.make_env))
            steps.append(CopyfileStep(savedefconfig, defconfig))

        return steps

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
	r'msm*_defconfig',
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

    # To try and make up for the link phase being serial, try to do
    # two full builds in parallel.  Don't do too many because lots of
    # parallel builds tends to use up available memory rather quickly.
    parallel = 2
    if all_options.jobs and all_options.jobs > 1:
        j = max(all_options.jobs / parallel, 2)
        make_command.append("-j" + str(j))

    tracker = BuildTracker(parallel)
    for target in targets:
        if all_options.updateconfigs:
            update_config(target.defconfig, all_options.updateconfigs)
        steps = target.build()
        tracker.add_sequence(target.log_name, target.name, steps)
    tracker.run()

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

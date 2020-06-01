#! /usr/bin/env python
# SPDX-License-Identifier: GPL-2.0-only

# Copyright (c) 2015, 2018 The Linux Foundation. All rights reserved.

"""
Android kernel configuration validator.

The Android kernel reference trees contain some config stubs of
configuration options that are required for Android to function
correctly, and additional ones that are recommended.

This script can help compare these base configs with the ".config"
output of the compiler to determine if the proper configs are defined.
"""

from collections import namedtuple
from optparse import OptionParser
import re
import sys

version = "check-config.py, version 0.0.1"

req_re = re.compile(r'''^CONFIG_(.*)=(.*)$''')
forb_re = re.compile(r'''^# CONFIG_(.*) is not set$''')
comment_re = re.compile(r'''^(#.*|)$''')

Enabled = namedtuple('Enabled', ['name', 'value'])
Disabled = namedtuple('Disabled', ['name'])

def walk_config(name):
    with open(name, 'r') as fd:
        for line in fd:
            line = line.rstrip()
            m = req_re.match(line)
            if m:
                yield Enabled(m.group(1), m.group(2))
                continue

            m = forb_re.match(line)
            if m:
                yield Disabled(m.group(1))
                continue

            m = comment_re.match(line)
            if m:
                continue

            print "WARNING: Unknown .config line: ", line

class Checker():
    def __init__(self):
        self.required = {}
        self.exempted = set()
        self.forbidden = set()

    def add_required(self, fname):
        for ent in walk_config(fname):
            if type(ent) is Enabled:
                self.required[ent.name] = ent.value
            elif type(ent) is Disabled:
                if ent.name in self.required:
                    del self.required[ent.name]
                self.forbidden.add(ent.name)

    def add_exempted(self, fname):
        with open(fname, 'r') as fd:
            for line in fd:
                line = line.rstrip()
                self.exempted.add(line)

    def check(self, path):
        failure = False

        # Don't run this for mdm targets
        if re.search('mdm', path):
            print "Not applicable to mdm targets... bypassing"
        else:
            for ent in walk_config(path):
                # Go to the next iteration if this config is exempt
                if ent.name in self.exempted:
                   continue

                if type(ent) is Enabled:
                    if ent.name in self.forbidden:
                        print "error: Config should not be present: %s" %ent.name
                        failure = True

                    if ent.name in self.required and ent.value != self.required[ent.name]:
                        print "error: Config has wrong value: %s %s expecting: %s" \
                                 %(ent.name, ent.value, self.required[ent.name])
                        failure = True

                elif type(ent) is Disabled:
                    if ent.name in self.required:
                        print "error: Config should be present, but is disabled: %s" %ent.name
                        failure = True

        if failure:
            sys.exit(1)

def main():
    usage = """%prog [options] path/to/.config"""
    parser = OptionParser(usage=usage, version=version)
    parser.add_option('-r', '--required', dest="required",
            action="append")
    parser.add_option('-e', '--exempted', dest="exempted",
            action="append")
    (options, args) = parser.parse_args()
    if len(args) != 1:
        parser.error("Expecting a single path argument to .config")
    elif options.required is None or options.exempted is None:
        parser.error("Expecting a file containing required configurations")

    ch = Checker()
    for r in options.required:
        ch.add_required(r)
    for e in options.exempted:
        ch.add_exempted(e)

    ch.check(args[0])

if __name__ == '__main__':
    main()

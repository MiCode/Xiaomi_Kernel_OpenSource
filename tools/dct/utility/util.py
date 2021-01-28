#! /usr/bin/python
# -*- coding: utf-8 -*-

# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2019 MediaTek Inc.
#

import re
import string


LEVEL_INFO = '[DCT_INFO]: '
LEVEL_WARN = '[DCT_WARNING]: '
LEVEL_ERROR = '[DCT_ERROR]: '

class LogLevel:
    info = 1
    warn = 2
    error = 3

def log(level, msg):
    if level == LogLevel.info:
        print LEVEL_INFO + msg
    elif level == LogLevel.warn:
        print LEVEL_WARN + msg
    elif level == LogLevel.error:
        print LEVEL_ERROR + msg

def compare(value):
    lst = re.findall(r'\d+', value)
    if len(lst) != 0:
        return string.atoi(lst[0])

    # if can not find numbers
    return value

def sorted_key(lst):
    return sorted(lst, key=compare)



#! /usr/bin/python
# -*- coding: utf-8 -*-

# Copyright (C) 2016 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.

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



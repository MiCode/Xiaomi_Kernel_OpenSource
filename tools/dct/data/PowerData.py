#! /usr/bin/python
# -*- coding: utf-8 -*-

# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2019 MediaTek Inc.
#

class PowerData:
    def __init__(self):
        self.__varName = ''

    def set_varName(self, name):
        self.__varName = name

    def get_varName(self):
        return self.__varName




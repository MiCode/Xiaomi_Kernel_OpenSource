#! /usr/bin/python
# -*- coding: utf-8 -*-

# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2019 MediaTek Inc.
#

class PmicData:
    _var_list = []
    def __init__(self):
        self.__ldoName = ''
        self.__defEn = -1
        self.__nameList = []

    def set_ldoName(self, name):
        self.__ldoName = name

    def get_ldoName(self):
        return self.__ldoName

    def set_defEnable(self, number):
        self.__defEn = number

    def get_defEnable(self):
        return self.__defEn

    def set_nameList(self, name_list):
        self.__nameList = name_list

    def get_nameList(self):
        return self.__nameList

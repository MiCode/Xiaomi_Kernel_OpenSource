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

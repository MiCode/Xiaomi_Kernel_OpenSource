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

class ClkData:
    _varList = ['DISABLE', 'SW_CONTROL', 'HW_CONTROL']
    _count = 0

    def __init__(self):
        self.__varName = ''

    def set_defVarName(self, idx):
        self.__varName = self._varList[idx]

    def set_varName(self, name):
        self.__varName = name

    def get_varName(self):
        return self.__varName


class OldClkData(ClkData):
    def __init__(self):
        ClkData.__init__(self)
        self.__current = ''
        self.__curList = []

    def set_defCurrent(self, idx):
        self.__current = self.__curList[idx]

    def set_current(self, current):
        self.__current = current

    def get_current(self):
        return self.__current

    def set_curList(self, cur_list):
        self.__curList = cur_list

    def get_curList(self):
        return self.__curList


class NewClkData(ClkData):
    def __init__(self):
        ClkData.__init__(self)
        self.__cur_buf_output = ""
        self.__cur_buf_output_list = []
        self.__cur_driving_control = ""
        self.__cur_driving_control_list = []

    def set_def_buf_output(self, index):
        self.__cur_buf_output = self.cur_buf_output_list[index]

    @property
    def cur_buf_output(self):
        return self.__cur_buf_output

    @cur_buf_output.setter
    def cur_buf_output(self, value):
        self.__cur_buf_output = value

    @property
    def cur_buf_output_list(self):
        return self.__cur_buf_output_list

    @cur_buf_output_list.setter
    def cur_buf_output_list(self, value):
        self.__cur_buf_output_list = value

    def set_def_driving_control(self, index):
        self.__cur_driving_control = self.cur_driving_control_list[index]

    @property
    def cur_driving_control(self):
        return self.__cur_driving_control

    @cur_driving_control.setter
    def cur_driving_control(self, value):
        self.__cur_driving_control = value

    @property
    def cur_driving_control_list(self):
        return self.__cur_driving_control_list

    @cur_driving_control_list.setter
    def cur_driving_control_list(self, value):
        self.__cur_driving_control_list = value

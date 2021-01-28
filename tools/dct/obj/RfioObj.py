#! /usr/bin/python
# -*- coding: utf-8 -*-

# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2019 MediaTek Inc.
#

import sys, os
import re
import ConfigParser
import string
import xml.dom.minidom

from ModuleObj import ModuleObj
from data.RfioData import RfioData

from utility.util import log
from utility.util import LogLevel
from utility.util import compare
from utility.util import sorted_key


class RfioObj(ModuleObj):
    def __init__(self):
        ModuleObj.__init__(self, '', '')
        self.__fileName = 'mml1_io_cfgtbl.c'
        self.__chipName = ''
        self.__mMode = ''
        self.__padCount = 0
        self.__mapChipId = {}
        self.__mapMMode = {}
        self.__mapPadName = {}

    def get_cfgInfo(self):
        cp = ConfigParser.ConfigParser(allow_no_value=True)
        path = os.path.join(sys.path[0], 'config', 'RFIO.cmp')
        if not os.path.exists(path) or not os.path.isfile(path):
            log(LogLevel.error, 'Can not find YuSu.cmp file!')
            sys.exit(-1)
        cp.read(path)

        #padName = 'PAD_NAME_' + self.__chipName
        keys = cp.options(self.__chipName)
        for key in keys:
            value = cp.get(self.__chipName, key)
            self.__mapPadName[value] = key

    def read(self, node):
        nodes = node.childNodes
        for node in nodes:
            if node.nodeType == xml.dom.Node.ELEMENT_NODE:
                if cmp(node.nodeName, 'chip') == 0:
                    if len(node.childNodes) == 0:
                       break
                    self.__chipName = node.childNodes[0].nodeValue
                    continue
                if cmp(node.nodeName, 'count') == 0:
                    if len(node.childNodes) == 0:
                        break
                    self.__count = string.atoi(node.childNodes[0].nodeValue)
                if cmp(node.nodeName, 'mmode') == 0:
                    if len(node.childNodes) == 0:
                        break
                    self.__mMode = node.childNodes[0].nodeValue

                data = RfioData()
                data.set_padName(node.nodeName)

                slaveNode = node.getElementsByTagName('slave_mode')
                drvNode = node.getElementsByTagName('driving')
                pupdNode = node.getElementsByTagName('pupd')
                dirNode = node.getElementsByTagName('dir')
                outDefNode = node.getElementsByTagName('outdef')
                iesNode = node.getElementsByTagName('ies')
                smtNode = node.getElementsByTagName('smt')
                analogPadNode = node.getElementsByTagName('analog_pad')

                if len(slaveNode) != 0 and len(slaveNode[0].childNodes) != 0:
                    data.set_slaveMode(slaveNode[0].childNodes[0].nodeValue)

                if len(drvNode) != 0 and len(drvNode[0].childNodes) != 0:
                    data.set_driving(drvNode[0].childNodes[0].nodeValue)

                if len(pupdNode) != 0 and len(pupdNode[0].childNodes) != 0:
                    data.set_pupd(pupdNode[0].childNodes[0].nodeValue)

                if len(dirNode) != 0 and len(dirNode[0].childNodes) != 0:
                    data.set_dir(dirNode[0].childNodes[0].nodeValue)

                if len(outDefNode) != 0 and len(outDefNode[0].childNodes) != 0:
                    data.set_outDef(outDefNode[0].childNodes[0].nodeValue)

                if len(iesNode) != 0 and len(iesNode[0].childNodes) != 0:
                    flag = False
                    if cmp(iesNode[0].childNodes[0].nodeValue, 'true') == 0:
                        flag = True
                    data.set_ies(flag)

                if len(smtNode) != 0 and len(smtNode[0].childNodes) != 0:
                    flag = False
                    if cmp(smtNode[0].childNodes[0].nodeValue, 'true') == 0:
                        flag = True
                    data.set_smt(flag)

                if len(analogPadNode) != 0 and len(analogPadNode[0].childNodes) != 0:
                    flag = False
                    if cmp(analogPadNode[0].childNodes[0].nodeValue, 'true') == 0:
                        flag = True
                    data.set_analogPad(flag)

                ModuleObj.set_data(self, node.nodeName, data)

        return True

    def parse(self, node):
        self.read(node)
        self.get_cfgInfo()

    def gen_files(self):
        self.gen_cFile()

    def gen_cFile(self):
        fp = open(os.path.join(ModuleObj.get_genPath(), self.__fileName), 'w')
        gen_str = ''
        gen_str += ModuleObj.writeComment()
        gen_str += self.fill_cFile()
        fp.write(gen_str)
        fp.close()


    def fill_hFile(self):
        return ''

    def fill_dtsiFile(self):
        return ''

    def fill_cFile(self):
        gen_str = '''#include "digrf_iomux.h"\n'''

        gen_str += '''const io_cfg_table_t digrf_io_cfg_table = {\n\t%s_CHIP_ID,\n\t%s,\n\t0,\n\t{\n''' %(self.__chipName, self.__mMode)
        for key in sorted_key(self.__mapPadName.keys()):
            padName = self.__mapPadName[key]
            item = ModuleObj.get_data(self)[padName.upper()]
            iesStr = 'RFIO_DISABLE'
            if item.get_ies():
                iesStr = 'RFIO_ENABLE'

            smtStr = 'RFIO_DISABLE'
            if item.get_smt():
                smtStr = 'RFIO_ENABLE'

            analogStr = 'RFIO_DISABLE'
            if item.get_analogPad():
                analogStr = 'RFIO_ENABLE'

            gen_str += '''\t{%s, RFIO_%s, RFIO_DRIVING_%s, RFIO_%s, RFIO_%s, RFIO_%s, %s, %s, %s, 0, 0},\n''' \
            %(padName.upper(),item.get_slaveMode(), item.get_driving(), item.get_pupd(), item.get_dir(), item.get_outDef(), iesStr, smtStr, analogStr)

        gen_str = gen_str[:-2]
        gen_str += '''\n\t},\n};'''

        return gen_str


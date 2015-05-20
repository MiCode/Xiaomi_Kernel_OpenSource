#!/usr/bin/env python
 ############################################################################
 #
 # This file is provided under a dual BSD/GPLv2 license.  When using or
 # redistributing this file, you may do so under either license.
 #
 # GPL LICENSE SUMMARY
 #
 # Copyright(c) 2007 - 2013 Intel Corporation. All rights reserved.
 #
 # This program is free software; you can redistribute it and/or modify
 # it under the terms of version 2 of the GNU General Public License as
 # published by the Free Software Foundation.
 #
 # This program is distributed in the hope that it will be useful, but
 # WITHOUT ANY WARRANTY; without even the implied warranty of
 # MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 # General Public License for more details.
 #
 # You should have received a copy of the GNU General Public License
 # along with this program; if not, write to the Free Software
 # Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 # USA
 #
 # The full GNU General Public License is included in this distribution
 # in the file called COPYING.
 #
 # Contact Information:
 #  Intel Linux Wireless <ilw@linux.intel.com>
 # Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 #
 # BSD LICENSE
 #
 # Copyright(c) 2005 - 2013 Intel Corporation. All rights reserved.
 # All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in
 #    the documentation and/or other materials provided with the
 #    distribution.
 #  * Neither the name Intel Corporation nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 # "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 # LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 # A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 # OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 # SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 # LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 # DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 # THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 #
 #############################################################################

import sys, os

# this works for files of the form "a.ko:" as well, so we can use this
# for the dependencies part
def compat_name(filename, toplevel_mod):
	# we can't change the name of the toplevel module, as iwlwifi loads
	# it explicitly using load_module()
	if os.path.basename(filename).find(toplevel_mod) != -1:
		return filename

	return os.path.dirname(filename) + "/compat_" + os.path.basename(filename)

def ren_compat_deps(filename, compat_prefix, toplevel_mod):
	fd = open(filename, "rb")
	data = fd.read()
	fd.close()

	res_lines = list()
	for line in data.split("\n"):
		# rename the modules built by compat, delete all other lines
		if line.find(compat_prefix) == 0:
			res_line = " ".join([compat_name(item, toplevel_mod) for item in line.split()])
			res_lines.append(res_line)

	res_data = "\n".join(res_lines)
	res_data += "\n"

	fd = open(filename, "wb")
	fd.write(res_data)
	fd.close()

	print "compat deps successfully resolved in %s!" % filename

if __name__ == "__main__":
	if len(sys.argv) < 4:
		raise Exception("Usage: %s <modules.dep> <compat_path_prefix> <toplevel module>" % sys.argv[0])

	ren_compat_deps(sys.argv[1], sys.argv[2], sys.argv[3])
	sys.exit(0)

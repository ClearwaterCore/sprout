# Project Clearwater - IMS in the Cloud
# Copyright (C) 2015  Metaswitch Networks Ltd
#
# This program is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version, along with the "Special Exception" for use of
# the program along with SSL, set forth below. This program is distributed
# in the hope that it will be useful, but WITHOUT ANY WARRANTY;
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR
# A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details. You should have received a copy of the GNU General Public
# License along with this program.  If not, see
# <http://www.gnu.org/licenses/>.
#
# The author can be reached by email at clearwater@metaswitch.com or by
# post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
#
# Special Exception
# Metaswitch Networks Ltd  grants you permission to copy, modify,
# propagate, and distribute a work formed by combining OpenSSL with The
# Software, or a work derivative of such a combination, even if such
# copying, modification, propagation, or distribution would otherwise
# violate the terms of the GPL. You must comply with the GPL in all
# respects for all of the code used other than OpenSSL.
# "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
# Project and licensed under the OpenSSL Licenses, or a work based on such
# software and licensed under the OpenSSL Licenses.
# "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
# under which the OpenSSL Project distributes the OpenSSL toolkit software,
# as those licenses appear in the file LICENSE-OPENSSL.

from metaswitch.clearwater.config_manager.plugin_base import ConfigPluginBase, FileStatus
from metaswitch.clearwater.etcd_shared.plugin_utils import run_command
import logging
import subprocess

_log = logging.getLogger("sprout_json_plugin")

class SproutJsonPlugin(ConfigPluginBase):
    def __init__(self, file, key):
        self._file = file
        self._key = key

    def key(self):
        return self._key

    def file(self):
        return self._file

    def status(self, value):
        try:
            with open(self._file, "r") as ifile:
                current = ifile.read()
                if current == value:
                    return FileStatus.UP_TO_DATE
                else:
                    with open("/tmp/value.tmp", "w") as ofile:
                        ofile.write(value)
                    print " + {} is present but is out of sync:".format(self._file)
                    print "     # diff -bw <etcd-conman-value> {}".format(self._file)
                    subprocess.call("diff -bw /tmp/value.tmp {}|sed -e 's#^#     #'".format(self._file), shell=True);
                    return FileStatus.OUT_OF_SYNC
        except IOError:
            return FileStatus.MISSING

    def on_config_changed(self, value, alarm):
        _log.info("Updating {}".format(self._file))

        with open(self._file, "w") as ofile:
            ofile.write(value);

        run_command("service sprout reload")

        alarm.update_file(self._file)

#!/bin/bash
############################################################################ ###
#@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
#@License       Dual MIT/GPLv2
#
# The contents of this file are subject to the MIT license as set out below.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# Alternatively, the contents of this file may be used under the terms of
# the GNU General Public License Version 2 ("GPL") in which case the provisions
# of GPL are applicable instead of those above.
#
# If you wish to allow use of your version of this file only under the terms of
# GPL, and not to allow others to use your version of this file under the terms
# of the MIT license, indicate your decision by deleting the provisions above
# and replace them with the notice and other provisions required by GPL as set
# out in the file called "GPL-COPYING" included in this distribution. If you do
# not delete the provisions above, a recipient may use your version of this file
# under the terms of either the MIT license or GPL.
#
# This License is also included in this distribution in the file called
# "MIT-COPYING".
#
# EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
# PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
# BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
# PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#### ###########################################################################
# Help on how to invoke
#
function usage {
    echo "usage: $0 [options...]"
    echo ""
    echo "Options: -v            Verbose mode."
    echo "         -n            Dry-run mode."
    echo "         -u            Uninstall-only mode."
    echo "         --root <path> Use <path> as the root of the install file system."
    echo "                       (Overrides the DISCIMAGE environment variable.)"
    echo "         -p <target>   Pack mode: Don't install anything.  Just copy files"
    echo "                       required for installation to <target>."
    echo "                       (Sets/overrides the PACKAGEDIR environment variable.)"
    echo "         --fw          firmware binaries."
    echo "         --km          kernel modules only."
    echo "         --um          user mode."
    exit 1
}

WD=`pwd`
SCRIPT_ROOT=`dirname $0`
cd $SCRIPT_ROOT

INSTALL_UM_SH_PRESENT=

INSTALL_PREFIX="i"
INSTALL_PREFIX_CAP="I"

PVRVERSION=[PVRVERSION]
PVRBUILD=[PVRBUILD]
PRIMARY_ARCH="[PRIMARY_ARCH]"
ARCHITECTURES=([ARCHITECTURES])

# These destination directories are the same for 32- or 64-bit binaries.

APP_DESTDIR=[APP_DESTDIR]
BIN_DESTDIR=[BIN_DESTDIR]
FW_DESTDIR=[FW_DESTDIR]
DATA_DESTDIR=[BIN_DESTDIR]
TEST_DESTDIR=[TEST_DESTDIR]

# Exit with an error messages.
# $1=blurb
#
function bail {
    if [ ! -z "$1" ]; then
        echo "$1" >&2
    fi

    echo "" >&2
    echo $INSTALL_PREFIX_CAP"nstallation failed" >&2
    exit 1
}

# Copy the files that we are going to install into $PACKAGEDIR
function copy_files_locally() {
    # Create versions of the installation functions that just copy files to a useful place.
    function check_module_directory() { true; }
    function uninstall() { true; }
    function link_library() { true; }
    function symlink_library_if_not_present() { true; }

    # basic installation function
    # $1=fromfile, $4=chmod-flags
    # plus other stuff that we aren't interested in.
    function install_file() {
        if [ -f "$1" ]; then
            $DOIT cp $1 $PACKAGEDIR/$THIS_ARCH
            $DOIT chmod $4 $PACKAGEDIR/$THIS_ARCH/$1
        fi
    }

    # Tree-based installation function
    # $1 = fromdir
    # plus other stuff that we aren't interested in.
    function install_tree() {
        if [ -d "$1" ]; then
            cp -Rf $1 $PACKAGEDIR/$THIS_ARCH
        fi
    }

    echo "Copying files to $PACKAGEDIR."

    if [ -d $PACKAGEDIR ]; then
        rm -Rf $PACKAGEDIR
    fi
    mkdir -p $PACKAGEDIR

    if [ "$INCLUDE_INDIVIDUAL_MODULE" != "y" ] || [ "$INCLUDE_COMPONENTS" == "y" ]; then
        for THIS_ARCH in "${ARCHITECTURES[@]}"; do
            if [ ! -d $THIS_ARCH ]; then
                continue
            fi

            mkdir -p $PACKAGEDIR/$THIS_ARCH
            pushd $THIS_ARCH > /dev/null
            if [ -f install_um.sh ]; then
                source install_um.sh
                install_file install_um.sh x x 0644
            fi
            popd > /dev/null
        done
    fi

    THIS_ARCH="target_neutral"
    if [ "$INCLUDE_INDIVIDUAL_MODULE" != "y" ] || [ "$INCLUDE_FW" == "y" ]; then
        if [ -d "$THIS_ARCH" ]; then
            mkdir -p "$PACKAGEDIR/$THIS_ARCH"
            pushd "$THIS_ARCH" > /dev/null
            if [ -f install_fw.sh ]; then
                source install_fw.sh
                install_file install_fw.sh x x 0644
                install_file rgxfw_debug.zip x x 0644
            fi
            popd > /dev/null
        fi
    fi

    THIS_ARCH=$PRIMARY_ARCH
    if [ "$INCLUDE_INDIVIDUAL_MODULE" != "y" ] || [ "$INCLUDE_KM" == "y" ]; then
        if [ -d "$THIS_ARCH" ]; then
            mkdir -p "$PACKAGEDIR/$THIS_ARCH"
            pushd $THIS_ARCH > /dev/null
            if [ -f install_km.sh ]; then
                source install_km.sh
                install_file install_km.sh x x 0644
            fi
            popd > /dev/null
        fi
    fi

    unset THIS_ARCH
    install_file install.sh x x 0755
}

# Install the files on the remote machine using SSH
# We do this by:
#  - Copying the required files to a place on the local disk
#  - rsync these files to the remote machine
#  - run the install via SSH on the remote machine
function install_via_ssh() {
    # Default to port 22 (SSH) if not otherwise specified
    if [ -z "$INSTALL_TARGET_PORT" ]; then
        INSTALL_TARGET_PORT=22
    fi

    # Execute something on the target machine via SSH
    # $1 The command to execute
    function remote_execute() {
        COMMAND=$1
        ssh -p "$INSTALL_TARGET_PORT" -q -o "BatchMode=yes" root@$INSTALL_TARGET "$1"
    }

    if ! remote_execute "test 1"; then
        echo "Can't access $INSTALL_TARGET via ssh."
        echo "Have you installed your public key into root@$INSTALL_TARGET:~/.ssh/authorized_keys?"
        echo "If root has a password on the target system, you can do so by executing:"
        echo "ssh root@$INSTALL_TARGET \"mkdir -p .ssh; cat >> .ssh/authorized_keys\" < ~/.ssh/id_rsa.pub"
        bail
    fi

    # Create a directory to contain all the files we are going to install.
    PACKAGEDIR_PREFIX=`mktemp -d` || bail "Couldn't create local temporary directory"
    PACKAGEDIR=$PACKAGEDIR_PREFIX/Rogue_DDK_Install_Root
    PACKAGEDIR_REMOTE=/tmp/Rogue_DDK_Install_Root
    copy_files_locally

    echo "RSyncing $PACKAGEDIR to $INSTALL_TARGET:$INSTALL_TARGET_PORT."
    $DOIT rsync -crlpt -e "ssh -p \"$INSTALL_TARGET_PORT\"" --delete $PACKAGEDIR/ root@$INSTALL_TARGET:$PACKAGEDIR_REMOTE || bail "Couldn't rsync $PACKAGEDIR to root@$INSTALL_TARGET"
    echo "Running "$INSTALL_PREFIX"nstall remotely."

    REMOTE_COMMAND="bash $PACKAGEDIR_REMOTE/install.sh -r /"

    if [ "$UNINSTALL_ONLY" == "y" ]; then
	REMOTE_COMMAND="$REMOTE_COMMAND -u"
    fi

    remote_execute "$REMOTE_COMMAND"
    rm -Rf $PACKAGEDIR_PREFIX
}

# Copy all the required files into their appropriate places on the local machine.
function install_locally {
    # Define functions required for local installs

    # basic installation function
    # $1=fromfile, $2=destfilename, $3=blurb, $4=chmod-flags, $5=chown-flags
    #
    function install_file {
        if [ -z "$DDK_INSTALL_LOG" ]; then
            bail "INTERNAL ERROR: Invoking install without setting logfile name"
        fi
        DESTFILE=${DISCIMAGE}$2
        DESTDIR=`dirname $DESTFILE`

        if [ ! -e $1 ]; then
            [ -n "$VERBOSE" ] && echo "skipping file $1 -> $2"
            return
        fi

        # Destination directory - make sure it's there and writable
        #
        if [ -d "${DESTDIR}" ]; then
            if [ ! -w "${DESTDIR}" ]; then
                bail "${DESTDIR} is not writable."
            fi
        else
            $DOIT mkdir -p ${DESTDIR} || bail "Couldn't mkdir -p ${DESTDIR}"
            [ -n "$VERBOSE" ] && echo "Created directory `dirname $2`"
        fi

        # Delete the original so that permissions don't persist.
        #
        $DOIT rm -f $DESTFILE

        $DOIT cp -f $1 $DESTFILE || bail "Couldn't copy $1 to $DESTFILE"
        $DOIT chmod $4 ${DESTFILE}

        echo "$3 `basename $1` -> $2"
        $DOIT echo "file $2" >> $DDK_INSTALL_LOG
    }

    # If we install to an empty $DISCIMAGE, then we need to create some
    # dummy directories, even if they contain no files, otherwise 'adb
    # sync' may fail. (It allows '/vendor' to not exist currently.)
    [ ! -d ${DISCIMAGE}/data ]   && mkdir ${DISCIMAGE}/data
    [ ! -d ${DISCIMAGE}/system ] && mkdir ${DISCIMAGE}/system

    # Install UM components
    if [ "$INCLUDE_INDIVIDUAL_MODULE" != "y" ] || [ "$INCLUDE_COMPONENTS" == "y" ]; then
        for arch in "${ARCHITECTURES[@]}"; do
            if [ ! -d $arch ]; then
                echo "Missing architecture $arch"
                continue
            fi

            BASE_DESTDIR=`dirname ${BIN_DESTDIR}`
            case $arch in
                target*64*)
                    SHLIB_DESTDIR=${BASE_DESTDIR}/lib64
                    ;;
                *)
                    SHLIB_DESTDIR=${BASE_DESTDIR}/lib
                    ;;
            esac
            EGL_DESTDIR=${SHLIB_DESTDIR}/egl

            pushd $arch > /dev/null
            if [ -f install_um.sh ]; then
                DDK_INSTALL_LOG=$UMLOG
                echo "Installing user components for architecture $arch"
                $DOIT echo "version $PVRVERSION" > $DDK_INSTALL_LOG
                source install_um.sh
                echo
            fi
            popd > /dev/null
        done
    fi

    # Install FW components
    THIS_ARCH="target_neutral"
    if [ "$INCLUDE_INDIVIDUAL_MODULE" != "y" ] || [ "$INCLUDE_FW" == "y" ]; then
        if [ -d "$THIS_ARCH" ]; then
            pushd "$THIS_ARCH" > /dev/null
            if [ -f install_fw.sh ]; then
                DDK_INSTALL_LOG=$FWLOG
                echo "Installing firmware components for architecture $arch"
                $DOIT echo "version $PVRVERSION" > $DDK_INSTALL_LOG
                source install_fw.sh
                echo
            fi
            popd > /dev/null
        fi
    fi

    # Install KM components
    if [ "$INCLUDE_INDIVIDUAL_MODULE" != "y" ] || [ "$INCLUDE_KM" == "y" ]; then
        if [ -d "$PRIMARY_ARCH" ]; then
            pushd $PRIMARY_ARCH > /dev/null
            if [ -f install_km.sh ]; then
                DDK_INSTALL_LOG=$KMLOG
                echo "Installing kernel components for architecture $PRIMARY_ARCH"
                $DOIT echo "version $PVRVERSION" > $DDK_INSTALL_LOG
                source install_km.sh
                echo
            fi
            popd > /dev/null
        fi
    fi

    if [ -f $UMLOG ] || [ -f $FWLOG ]; then
        # Create an OLDUMLOG so old versions of the driver can uninstall UM + FW.
        $DOIT echo "version $PVRVERSION" > $OLDUMLOG
        if [ -f $UMLOG ]; then
            # skip the first line which is DDK version information
            tail -n +2 $UMLOG >> $OLDUMLOG
            echo "file $UMLOG" >> $OLDUMLOG
        fi
        if [ -f $FWLOG ]; then
            tail -n +2 $FWLOG >> $OLDUMLOG
            echo "file $FWLOG" >> $OLDUMLOG
        fi
    fi
}

# Read the appropriate install log and delete anything therein.
function uninstall_locally {
    # Function to uninstall something.
    function do_uninstall {
        LOG=$1

        if [ ! -f $LOG ]; then
            echo "Nothing to un-install."
            return;
        fi

        BAD=false
        VERSION=""
        while read type data; do
            case $type in
            version)
                echo "Uninstalling existing version $data"
                VERSION="$data"
                ;;
            link|file)
                if [ -z "$VERSION" ]; then
                    BAD=true;
                    echo "No version record at head of $LOG"
                elif ! $DOIT rm -f ${DISCIMAGE}${data}; then
                    BAD=true;
                else
                    [ -n "$VERBOSE" ] && echo "Deleted $type $data"
                fi
                ;;
            tree)
                ;;
            esac
        done < $1;

        if ! $BAD ; then
            echo "Uninstallation completed."
            $DOIT rm -f $LOG
        else
            echo "Uninstallation failed!!!"
        fi
    }

    if [ -z "$OLDUMLOG" ] || [ -z "$KMLOG" ] || [ -z "$UMLOG" ] || [ -z "$FWLOG" ]; then
        bail "INTERNAL ERROR: Invoking uninstall without setting logfile name"
    fi

    # Check if last install was using legacy UM log (combined FW and UM components)
    DO_LEGACY_UM_UNINSTALL=
    if [ -f $OLDUMLOG ] && [ ! -f $UMLOG ] && [ ! -f $FWLOG ]; then
        if [ "$INCLUDE_INDIVIDUAL_MODULE" != "y" ]; then
            DO_LEGACY_UM_UNINSTALL=1
        elif [ "$INCLUDE_FW" == "y" ] && [ "$INCLUDE_COMPONENTS" == "y" ]; then
            DO_LEGACY_UM_UNINSTALL=1
        elif [ "$INCLUDE_FW" != "$INCLUDE_COMPONENTS" ]; then
            if [ "$INCLUDE_FW" == "y" ]; then
                echo "Previous driver installation doesn't support ${INSTALL_PREFIX}nstalling"
                echo "firmware components separately to user components."
                bail
            else
                echo "Previous driver installation doesn't support ${INSTALL_PREFIX}nstalling"
                echo "user components separately to firmware components."
                bail
            fi
        fi
    fi

    # Uninstall KM components if we are doing a KM install.
    if [ "$INCLUDE_INDIVIDUAL_MODULE" != "y" ] || [ "$INCLUDE_KM" == "y" ]; then
        if [ -f install_km.sh ] && [ -f $KMLOG ]; then
            echo "Uninstalling kernel components"
            do_uninstall $KMLOG
            echo
        fi
    fi

    if [ -n "$DO_LEGACY_UM_UNINSTALL" ]; then
        # Uninstall FW and UM components if we are doing a FW+UM install.
        if [ -n "$INSTALL_UM_SH_PRESENT" ]; then
            echo "Uninstalling all (firmware + user mode) components from legacy log."
            do_uninstall "$OLDUMLOG"
            echo
        fi
    else
        # Uninstall FW binaries if we are doing a FW install.
        if [ "$INCLUDE_INDIVIDUAL_MODULE" != "y" ] || [ "$INCLUDE_FW" == "y" ]; then
            if [ -f target_neutral/install_fw.sh ] && [ -f "$FWLOG" ]; then
                echo "Uninstalling firmware components"
                if [ -f "$UMLOG" ]; then
                    # Update legacy UM log
                    cp "$UMLOG" "$OLDUMLOG"
                    echo "file $UMLOG" >> $OLDUMLOG
                elif [ -f $OLDUMLOG ]; then
                    rm "$OLDUMLOG"
                fi
                do_uninstall "$FWLOG"
                echo
            fi
        fi

        # Uninstall UM components if we are doing a UM install.
        if [ "$INCLUDE_INDIVIDUAL_MODULE" != "y" ] || [ "$INCLUDE_COMPONENTS" == "y" ]; then
            if [ -n "$INSTALL_UM_SH_PRESENT" ] && [ -f "$UMLOG" ]; then
                echo "Uninstalling user components"
                if [ -f "$FWLOG" ]; then
                    # Update legacy UM log
                    cp "$FWLOG" "$OLDUMLOG"
                    echo "file $FWLOG" >> $OLDUMLOG
                elif [ -f $OLDUMLOG ]; then
                    rm "$OLDUMLOG"
                fi
                do_uninstall "$UMLOG"
                echo
            fi
        fi
    fi
}

# Work out if there are any special instructions.
#
while [ "$1" ]; do
    case "$1" in
    -v|--verbose)
        VERBOSE=v
        ;;
    -r|--root)
        DISCIMAGE=$2
        shift;
        ;;
    -u|--uninstall)
        UNINSTALL_ONLY=y
	INSTALL_PREFIX="uni"
	INSTALL_PREFIX_CAP="Uni"
        ;;
    -n)
        DOIT=echo
        ;;
    -p|--package)
        PACKAGEDIR=$2
        if [ ${PACKAGEDIR:0:1} != '/' ]; then
            PACKAGEDIR=$WD/$PACKAGEDIR
        fi
        shift;
        ;;
    --fw)
        INCLUDE_INDIVIDUAL_MODULE=y
        INCLUDE_FW=y
        ;;
    --km)
        INCLUDE_INDIVIDUAL_MODULE=y
        INCLUDE_KM=y
        ;;
    --um)
        INCLUDE_INDIVIDUAL_MODULE=y
        INCLUDE_COMPONENTS=y
        ;;
    -h | --help | *)
        usage
        ;;
    esac
    shift
done

for i in "${ARCHITECTURES[@]}"; do
    if [ -f "$i"/install_um.sh ]; then
        INSTALL_UM_SH_PRESENT=1
    fi
done

if [ "$INCLUDE_COMPONENTS" == "y" ] && [ ! -n "$INSTALL_UM_SH_PRESENT" ]; then
    bail "Cannot ${INSTALL_PREFIX}nstall user components only (install_um.sh missing)."
fi

if [ "$INCLUDE_FW" == "y" ] && [ ! -f target_neutral/install_fw.sh ]; then
    bail "Cannot ${INSTALL_PREFIX}nstall firmware components only (install_fw.sh missing)."
fi

if [ "$INCLUDE_KM" == "y" ] && [ ! -f "${PRIMARY_ARCH}"/install_km.sh ]; then
    bail "Cannot ${INSTALL_PREFIX}nstall kernel components only (install_km.sh missing)."
fi

if [ ! -z "$PACKAGEDIR" ]; then
    copy_files_locally $PACKAGEDIR
    echo "Copy complete!"

elif [ ! -z "$DISCIMAGE" ]; then

    if [ ! -d "$DISCIMAGE" ]; then
       bail "$0: $DISCIMAGE does not exist."
    fi

    echo
    echo "File system root is $DISCIMAGE"
    echo

    OLDUMLOG=$DISCIMAGE/powervr_ddk_install_um.log
    KMLOG=$DISCIMAGE/powervr_ddk_install_km.log
    UMLOG=$DISCIMAGE/powervr_ddk_install_components.log
    FWLOG=$DISCIMAGE/powervr_ddk_install_fw.log

    uninstall_locally

    if [ "$UNINSTALL_ONLY" != "y" ]; then
	if [ $DISCIMAGE == "/" ]; then
            echo "Installing PowerVR '$PVRVERSION ($PVRBUILD)' locally"
	else
            echo "Installing PowerVR '$PVRVERSION ($PVRBUILD)' on $DISCIMAGE"
	fi
	echo

        install_locally
    fi

else
    bail "DISCIMAGE must be set for "$INSTALL_PREFIX"nstallation to be possible."
fi

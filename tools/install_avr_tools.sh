#!/bin/bash

# This file should be used to install all the tools needed for avr programming and debugging.
#
# Most of the content is adapted from the scripts provided by the Proxmark3 community
# and Bingo from the AVR Freaks community.
#
# Many thanks to the gcc and avr-gcc communities for the help in building this script
#
# Author: Omar Choudary
# Last updated on 27 April 2011
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License version 3
# as published by the Free Software Foundation:
# http://www.gnu.org/licenses/gpl.html
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# Before you start, be sure to install the following packages in your distro
# All of them should be available via apt-get (ubuntu) or similar.
# libppl0.10-dev
# libcloog-ppl-dev
# zlib1g-dev
# autoconf
# gettext
# gperf
# dejagnu
# patch
# autogen
# guile-1.8
# flex
# texinfo
# tcl
# gcc
# g++
# binutils-dev
# libusb-dev
# bison


############################################################################
# Let's get started. Variables and other stuff.
############################################################################

# Some things for you to configure
BINUTILS_VER="2.21"
GCC_VER="4.5.2"
GDB_VER="7.2"
GMP_VER="5.0.1"
MPFR_VER="3.0.1"
MPC_VER="0.8.2"
INSIGHT_VER="6.8-1"
AVARICE_VER="2.10"
AVRDUDE_VER="5.10"
AVRLIBC_VER="1.7.0"
DFUPROG_VER="0.5.4"

# Where you want to install the tools
if [ "${1}" = "" ]; then
   echo "Syntax: ${0} <installation_target_directory (e.g. /usr/local/avr)> [download & build directory (default ${PWD})]"
   exit 1
else
   DESTDIR="${1}"
fi

# Where do you want to build the tools. This is where the log files
# will be written (which you can monitor with 'tail' during compilation).
# You can delete this directory after everything is done.
if [ ! "${2}" = "" ]; then
   SRCDIR="${2}"
else
   SRCDIR="${PWD}"
fi
BUILDDIR=${SRCDIR}/build-avr

# Where to get each of the toolchain components
BINUTILS=http://www.mirrorservice.org/sites/ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VER}.tar.bz2
GCCCORE=http://www.mirrorservice.org/sites/ftp.gnu.org/gnu/gcc/gcc-${GCC_VER}/gcc-core-${GCC_VER}.tar.bz2
GPP=http://www.mirrorservice.org/sites/ftp.gnu.org/gnu/gcc/gcc-${GCC_VER}/gcc-g++-${GCC_VER}.tar.bz2
INSIGHT=http://www.mirrorservice.org/sites/sources.redhat.com/pub/insight/releases/insight-${INSIGHT_VER}.tar.bz2
GDB=http://www.mirrorservice.org/sites/ftp.gnu.org/gnu/gdb/gdb-${GDB_VER}.tar.bz2
GMP=http://ftp.sunet.se/pub/gnu/gmp/gmp-${GMP_VER}.tar.bz2
MPFR=http://www.mpfr.org/mpfr-current/mpfr-${MPFR_VER}.tar.bz2
MPC=http://www.multiprecision.org/mpc/download/mpc-${MPC_VER}.tar.gz
AVARICE=http://downloads.sourceforge.net/avarice/avarice-${AVARICE_VER}.tar.bz2
AVRDUDE=http://download.savannah.gnu.org/releases/avrdude/avrdude-${AVRDUDE_VER}.tar.gz
AVRLIBC=http://download.savannah.gnu.org/releases/avr-libc/avr-libc-${AVRLIBC_VER}.tar.bz2
DFUPROG=http://sourceforge.net/projects/dfu-programmer/files/dfu-programmer/${DFUPROG_VER}/dfu-programmer-${DFUPROG_VER}.tar.gz

# Patches information
#PATCH_GCC_1=http://gcc.gnu.org/bugzilla/attachment.cgi?id=22484 # use this for GCC 4.5.1
PATCH_GCC_1=http://gcc.gnu.org/bugzilla/attachment.cgi?id=23050 # use this for GCC 4.5.2
PATCH_GCC_1_NAME=gcc_452_avr.patch

PATCH_AVRLIBC_1=https://savannah.nongnu.org/support/download.php?file_id=21669 # use for AVR-LIBC-1.7.0 and GCC-4.5.0 to GCC-4.5.2
PATCH_AVRLIBC_1_NAME=avr_libc_170.patch

PATCH_AVARICE_1=http://www.cl.cam.ac.uk/~osc22/files/avr_gcc/avarice_210.patch # use for AVARICE-2.10, fixed later
PATCH_AVARICE_1_NAME=avarice_210.patch


# Common configuration options (i.e., things to pass to 'configure')
COMMON_CFG="--target=avr --prefix=${DESTDIR} -v"

# Extra configuration options for each toolchain component
BINUTILS_CFG="--program-prefix=avr- --with-gnu-ld --with-gnu-as --quiet --enable-install-libbfd --with-dwarf2 --enable-languages=c,c++ --disable-werror --disable-nls"
GCCCORE_CFG="--program-prefix=avr- --with-gcc --with-gnu-ld --with-gnu-as --with-dwarf2 --disable-libssp --enable-languages=c,c++ --disable-werror --disable-nls\
 --disable-shared --disable-threads" 
AVRLIBC_CFG="--host=avr --quiet --build=`../config.guess`" # check this
INSIGHT_CFG="--program-prefix=avr- --with-gnu-ld --with-gnu-as --quiet --disable-werror --disable-nls"
GDB_CFG="--program-prefix=avr- --with-gnu-ld --with-gnu-as --quiet --enable-languages=c,c++ --disable-werror --disable-nls"
AVARICE_CFG="--disable-werror"
AVRDUDE_CFG=

# Make flags
MAKEFLAGS="-j 4"

# wget options
# -nv: non-verbose but not too quiet (still print errors/warnings)
# -nc: no-clobber, do not download a file that already exists
# -t 0: retry indefinitely
# -a wget.log: append errors/warnings to wget.log file
# -c continue
#WGET_OPTS="-nv -nc -t 0 -a wget.log"
WGET_OPTS="-c -t 0"

############################################################################
# End of configuration section. You shouldn't have to modify anything below.
############################################################################

if [[ `whoami` != "root" ]]; then
  echo "*** Warning! Not running as root!"
  echo "Installation may fail if you do not have appropriate permissions!"
fi

mkdir -p ${BUILDDIR}
cd ${SRCDIR}


############################################################################
# Download section
############################################################################

echo Now downloading BINUTILS...
wget ${WGET_OPTS} ${BINUTILS}

echo Now downloading GCC...
wget ${WGET_OPTS} ${GCCCORE}

echo Now downloading G++...
wget ${WGET_OPTS} ${GPP}

echo Now downloading INSIGHT...
wget ${WGET_OPTS} ${INSIGHT}

echo Now downloading GDB...
wget ${WGET_OPTS} ${GDB}

echo Now downloading GMP...
wget ${WGET_OPTS} ${GMP}

echo Now downloading MPFR...
wget ${WGET_OPTS} ${MPFR}

echo Now downloading MPC...
wget ${WGET_OPTS} ${MPC}

echo Now downloading AVARICE...
wget ${WGET_OPTS} ${AVARICE}

echo Now downloading AVRDUDE...
wget ${WGET_OPTS} ${AVRDUDE}

echo Now downloading AVRLIBC...
wget ${WGET_OPTS} ${AVRLIBC}

echo Now downloading DFU-PROGRAMMER...
wget ${WGET_OPTS} ${DFUPROG}

# Get any necessary patches
echo Now downloading GCC Patch...
wget ${WGET_OPTS} -O ${PATCH_GCC_1_NAME} ${PATCH_GCC_1}

echo Now downloading AVR-LIBC Patch...
wget ${WGET_OPTS} -O ${PATCH_AVRLIBC_1_NAME} ${PATCH_AVRLIBC_1}

echo Now downloading AVARICE Patch...
wget ${WGET_OPTS} -O ${PATCH_AVARICE_1_NAME} ${PATCH_AVARICE_1}


cd ${BUILDDIR}

############################################################################
# Install section
############################################################################

# Set the paths before anything
  echo ___________________  > make.log
  echo Adding ${DESTDIR}/bin to PATH >> make.log
  PATH=${DESTDIR}/bin:$PATH
  export PATH;
  echo ___________________  >> make.log

  # lines below are special for Ubuntu
  echo Adding ${DESTDIR}/lib to LD_LIBRARY_PATH and LD_RUN_PATH >> make.log
  LD_LIBRARY_PATH=${DESTDIR}/lib:$LD_LIBRARY_PATH
  export LD_LIBRARY_PATH
  LD_RUN_PATH=${DESTDIR}/lib:$LD_RUN_PATH
  export LD_RUN_PATH
  echo ___________________  >> make.log

# Install binutils
if [[ -f binutils.built ]]; then
  echo Looks like BINUTILS was already built.
else
  echo Building BINUTILS...
  tar -xjf ../`basename ${BINUTILS}`
  echo ___________________  >> make.log
  echo Building binutils... >> make.log
  cd `find . -maxdepth 1 -type d -name 'binutils*'`
  mkdir buildavr
  cd buildavr
  ../configure ${COMMON_CFG} ${BINUTILS_CFG} >> ../../make.log 2>&1
  make ${MAKEFLAGS} MAKEINFO=`which makeinfo` >> ../../make.log 2>&1
  make install >> ../../make.log 2>&1
  cd ../..
  touch binutils.built
fi


# Install gcc
if [[ -f gcc.built ]]; then
  echo Looks like GCC was already built.
else
  echo Building GCC...
  tar -xjf ../`basename ${GCCCORE}`
  tar -xjf ../`basename ${GPP}`
  tar -xjf ../`basename ${GMP}`
  ln -s "${BUILDDIR}/gmp-${GMP_VER}" "${BUILDDIR}/gcc-${GCC_VER}/gmp"
  tar -xjf ../`basename ${MPFR}`
  ln -s "${BUILDDIR}/mpfr-${MPFR_VER}" "${BUILDDIR}/gcc-${GCC_VER}/mpfr"
  tar -xzf ../`basename ${MPC}`
  ln -s "${BUILDDIR}/mpc-${MPC_VER}" "${BUILDDIR}/gcc-${GCC_VER}/mpc"

  echo ___________________  >> make.log

  # Apply patch if you are using version 4.5.1 or 4.5.2. See more here:
  # http://gcc.gnu.org/bugzilla/show_bug.cgi?id=45261
  echo Patching gcc... >> make.log
  patch -p0 < ../${PATCH_GCC_1_NAME}

  echo Building gcc... >> make.log
  GCC_SRC_DIR=${BUILDDIR}/gcc-${GCC_VER}
  GCC_BUILD_DIR=${BUILDDIR}/gcc-build-${GCC_VER}
  echo GCC_SRC_DIR is ${GCC_SRC_DIR} and GCC_BUILD_DIR is ${GCC_BUILD_DIR} >> make.log
  mkdir ${GCC_BUILD_DIR}
  cd ${GCC_BUILD_DIR}
  ${GCC_SRC_DIR}/configure ${COMMON_CFG} ${GCCCORE_CFG} >> ${BUILDDIR}/make.log 2>&1
  make ${MAKEFLAGS} all >> ${BUILDDIR}/make.log 2>&1
  make install >> ${BUILDDIR}/make.log 2>&1

  cd ${BUILDDIR}
  touch gcc.built
fi

# Install AVR-LIBC
if [[ -f avrlibc.built ]]; then
  echo Looks like AVR-LIBC was already built.
else
  echo Building AVR-LIBC...
  tar -xjf ../`basename ${AVRLIBC}`
  echo ___________________ >> make.log

  cd `find . -maxdepth 1 -type d -name 'avr-libc*'`

  # Apply patch if you are using version 4.5.1 or 4.5.2 with AVR-LIBC-1.7.0 
  # See more here:
  # https://savannah.nongnu.org/bugs/index.php?30363
  echo Patching avr-libc... >> make.log
  patch -p0 < ../../${PATCH_AVRLIBC_1_NAME}
  
  echo Building avr-libc... >> make.log
  mkdir buildavr
  cd buildavr
  ../configure ${COMMON_CFG} ${AVRLIBC_CFG} >> ../../make.log 2>&1
  make ${MAKEFLAGS} all >> ../../make.log 2>&1
  make install >> ../../make.log 2>&1
  cd ../..
  touch avrlibc.built
fi

# Install GDB
if [[ -f gdb.built ]]; then
  echo Looks like GDB was already built.
else
  echo Building GDB...
  tar -xjf ../`basename ${GDB}`
  echo ___________________  >> make.log
  echo Building insight... >> make.log
  cd `find . -maxdepth 1 -type d -name 'gdb*'`
  mkdir buildavr
  cd buildavr
  ../configure ${COMMON_CFG} ${GDB_CFG} >> ../../make.log 2>&1
  make ${MAKEFLAGS} all >> ../../make.log 2>&1
  make install >> ../../make.log 2>&1
  cd ../..
  touch gdb.built
fi

# Install Insight
if [[ -f insight.built ]]; then
  echo Looks like INSIGHT was already built.
else
  echo Building INSIGHT...
  tar -xjf ../`basename ${INSIGHT}`
  echo ___________________  >> make.log
  echo Building insight... >> make.log
  cd `find . -maxdepth 1 -type d -name 'insight*'`
  mkdir buildavr
  cd buildavr
  ../configure ${COMMON_CFG} ${INSIGHT_CFG} >> ../../make.log 2>&1
  make ${MAKEFLAGS} all >> ../../make.log 2>&1
  make install >> ../../make.log 2>&1
  cd ../..
  touch insight.built
fi

# Install Avarice
if [[ -f avarice.built ]]; then
  echo Looks like AVARICE was already built.
else
  echo Building AVARICE...
  tar -xjf ../`basename ${AVARICE}`
  echo ___________________ >> make.log
  echo Building avarice... >> make.log

  # Apply patch if you are using version 2.10
  # See more here:
  # http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=565280
  echo Patching avarice... >> make.log
  patch -p0 < ../${PATCH_AVARICE_1_NAME}

  cd `find . -maxdepth 1 -type d -name 'avarice*'`
  mkdir buildavr
  cd buildavr
  ../configure -v --prefix=${DESTDIR} ${AVARICE_CFG} >> ../../make.log 2>&1
  make ${MAKEFLAGS} all >> ../../make.log 2>&1
  make install >> ../../make.log 2>&1
  cd ../..
  touch avarice.built
fi

# Install Avrdude
if [[ -f avrdude.built ]]; then
  echo Looks like AVRDUDE was already built.
else
  echo Building AVRDUDE...
  tar -xzf ../`basename ${AVRDUDE}`
  echo ___________________ >> make.log
  echo Building avrdude... >> make.log
  cd `find . -maxdepth 1 -type d -name 'avrdude*'`
  mkdir buildavr
  cd buildavr
  ../configure -v --prefix=${DESTDIR} ${AVRDUDE_CFG} >> ../../make.log 2>&1
  make ${MAKEFLAGS} all >> ../../make.log 2>&1
  make install >> ../../make.log 2>&1
  cd ../..
  touch avrdude.built
fi

# Install DFU-Programmer
if [[ -f dfuprog.built ]]; then
  echo Looks like DFU-PROGRAMMER was already built.
else
  echo Building DFU-PROGRAMMER...
  tar -xzf ../`basename ${DFUPROG}`
  echo ___________________ >> make.log
  echo Building dfu-programmer... >> make.log
  cd `find . -maxdepth 1 -type d -name 'dfu-programmer*'`
  mkdir buildavr
  cd buildavr
  ../configure -v --prefix=${DESTDIR} ${DFUPROG_CFG} >> ../../make.log 2>&1
  make ${MAKEFLAGS} all >> ../../make.log 2>&1
  make install >> ../../make.log 2>&1
  cd ../..
  touch dfuprog.built
fi

echo ___________________  >> make.log
echo Build complete. >> make.log

cd ${DESTDIR}
chmod -R a+rX .

echo Downloaded archives are in ${SRCDIR}
echo build directory: ${BUILDDIR}
echo install directory: ${DESTDIR}
exit 0

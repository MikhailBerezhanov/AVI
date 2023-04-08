#!/bin/bash

set -e

SDK_DIR="../../sim_open_sdk"

#
# Toolchain update
#
LIB_DEST=${SDK_DIR}/sim_crosscompile/sysroots/mdm9607-perf/usr/lib
INCLUDE_DEST=${SDK_DIR}/sim_crosscompile/sysroots/mdm9607-perf/usr/include

LIB_DIR="./target/lib"
INCLUDE_DIR="./target/include"

# libconfig installation
echo -e "libconfig installation .."

cp ${INCLUDE_DIR}/libconfig.* ${INCLUDE_DEST}

cp ${LIB_DIR}/libconfig* ${LIB_DEST}
ln -s -f libconfig.so.11.1.0 ${LIB_DEST}/libconfig.so
ln -s -f libconfig.so.11.1.0 ${LIB_DEST}/libconfig.so.11
ln -s -f libconfig++.so.11.1.0 ${LIB_DEST}/libconfig++.so
ln -s -f libconfig++.so.11.1.0 ${LIB_DEST}/libconfig++.so.11

# libprotobuf installation
echo -e "libprotobuf installation .."

cp -r ${INCLUDE_DIR}/google ${INCLUDE_DEST}

cp ${LIB_DIR}/libproto* ${LIB_DEST}
ln -s -f libprotobuf-lite.so.31.0.0 ${LIB_DEST}/libprotobuf-lite.so
ln -s -f libprotobuf-lite.so.31.0.0 ${LIB_DEST}/libprotobuf-lite.so.31
ln -s -f libprotobuf.so.31.0.0 ${LIB_DEST}/libprotobuf.so
ln -s -f libprotobuf.so.31.0.0 ${LIB_DEST}/libprotobuf.so.31
ln -s -f libprotoc.so.31.0.0 ${LIB_DEST}/libprotoc.so
ln -s -f libprotoc.so.31.0.0 ${LIB_DEST}/libprotoc.so.31

# libamqpcpp installation
echo -e "libamqpcpp installation .."

cp -r ${INCLUDE_DIR}/amqpcpp ${INCLUDE_DEST}
cp ${INCLUDE_DIR}/amqpcpp.h ${INCLUDE_DEST}

cp ${LIB_DIR}/libamqpcpp* ${LIB_DEST}
ln -s -f libamqpcpp.so.4.3.18 ${LIB_DEST}/libamqpcpp.so.4.3
ln -s -f libamqpcpp.so.4.3.18 ${LIB_DEST}/libamqpcpp.so.4
ln -s -f libamqpcpp.so.4.3.18 ${LIB_DEST}/libamqpcpp.so
ln -s -f libamqpcpp.a.4.3.18 ${LIB_DEST}/libamqpcpp.a

echo -e "[+] Toolchain patch succeed"

#
# Img rootfs update
#
ROOTFS_DIR=${SDK_DIR}/sim_rootfs

sudo cp ./target/rc/start_autostart ${ROOTFS_DIR}/etc/init.d/
sudo ln -s -f ../init.d/start_autostart ${ROOTFS_DIR}/etc/rc5.d/S99start_autostart

echo -e "[+] Img rootfs patch succeed"

#
# Img usrfs update
#
USRFS_DIR=${SDK_DIR}/sim_usrfs

mkdir -p ${USRFS_DIR}/autoinformer
mkdir -p ${USRFS_DIR}/avi
mkdir -p ${USRFS_DIR}/lib

cp ./target/bin/autostart.sh ${USRFS_DIR}/autoinformer 
cp ./target/bin/test_avi ${USRFS_DIR}/autoinformer 

cp ./target/bin/avi ${USRFS_DIR}/avi
cp ../configs/avi.conf ${USRFS_DIR}/avi

cp ${LIB_DIR}/libconfig.so.11.1.0 ${USRFS_DIR}/lib
cp ${LIB_DIR}/libprotobuf.so.31.0.0 ${USRFS_DIR}/lib
cp ${LIB_DIR}/libamqpcpp.so.4.3.18 ${USRFS_DIR}/lib

ln -s -f libconfig.so.11.1.0 ${USRFS_DIR}/lib/libconfig.so
ln -s -f libconfig.so.11.1.0 ${USRFS_DIR}/lib/libconfig.so.11

ln -s -f libprotobuf.so.31.0.0 ${USRFS_DIR}/lib/libprotobuf.so
ln -s -f libprotobuf.so.31.0.0 ${USRFS_DIR}/lib/libprotobuf.so.31

ln -s -f libamqpcpp.so.4.3.18 ${USRFS_DIR}/lib/libamqpcpp.so
ln -s -f libamqpcpp.so.4.3.18 ${USRFS_DIR}/lib/libamqpcpp.so.4
ln -s -f libamqpcpp.so.4.3.18 ${USRFS_DIR}/lib/libamqpcpp.so.4.3

echo -e "[+] Img usrfs patch succeed"


echo -e "[OK] SDK patch finished"
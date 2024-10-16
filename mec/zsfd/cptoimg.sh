#!/bin/bash

object_type="x86-dbg"
zas_lib_path="x86_64"
if [ "$1" = "arm" ]
    then
    object_type="arm64-rel"
    zas_lib_path="arm_64"
    if [ "$2" = "dbg" ]
        then
        object_type="arm64-dbg"
    elif [ "$2" = "rel" ]
        then
        object_type="arm64-rel"
    fi
elif [ "$2" = "rel" ]
    then
    object_type="x86-rel"
fi

set -x

sudo cp -r ./targets/$object_type/libmware.so /zassys/lib/$zas_lib_path-linux-gnu/
sudo cp -r ./targets/$object_type/libutils.so /zassys/lib/$zas_lib_path-linux-gnu/
sudo cp -r ./targets/$object_type/libwebcore.so /zassys/lib/$zas_lib_path-linux-gnu/
sudo cp -r ./targets/$object_type/sys/zhost /zassys/bin/
sudo cp -r ./targets/$object_type/sys/zlaunch /zassys/bin/
sudo cp -r ./targets/$object_type/sys/coreapp/zsysd /zassys/sysapp/zas.system/daemons/
sudo cp -r ./targets/$object_type/sys/coreapp/rpcbridge /zassys/sysapp/zas.system/daemons/
sudo cp -r ./targets/$object_type/sys/coreapp/services/servicebundle.so /zassys/sysapp/zas.system/services/

sudo cp -r ./targets/$object_type/test/* /zassys/sysapp/others/test/
sudo cp -r ./test/mware/test_sysconfig.json /zassys/etc/syssvrconfig/sysconfig.json
sudo cp -r ./test/webcore/sysconfig.json /zassys/etc/webconfig/sysconfig.json
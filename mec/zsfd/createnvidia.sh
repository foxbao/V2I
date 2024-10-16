#!/bin/bash

object_type="x86-dbg"
zas_lib_path="x86_64"
zas_root_path=/home/jimview
if [ "$1" = "rel" ]
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
sudo cp -r ./targets/$object_type/sys/coreapp/services/servicebundle.so /zassys/sysapp/zas.system/services/


sudo cp -r ./targets/$object_type/libprotomsg.so /zassys/lib/$zas_lib_path-linux-gnu/

sudo cp -r ./targets/$object_type/libdigdup_dataprovider.so /zassys/sysapp/zas.digdup/services/
sudo cp -r ./targets/$object_type/zdigtwins /zassys/sysapp/zas.digtwins/

sudo cp -r $zas_root_path/vpoc/csys/digtwins/config/digtwins.json /zassys/sysapp/zas.digtwins/
sudo cp -r $zas_root_path/vpoc/csys/digdup-dataprovider/config/digdup.json /zassys/sysapp/zas.digtwins/
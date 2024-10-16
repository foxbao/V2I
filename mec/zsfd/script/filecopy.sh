#/bin/bash/

if [ "$1" = "rel" ]\
 || [ "$1" = "dbg" ]
then
sudo cp -r ~/zas/targets/x86-$1/sys/zlaunch /zassys/bin/
sudo cp -r ~/zas/targets/x86-$1/sys/zsysd /zassys/app//zas.system.daemons/
sudo cp -r ~/zas/targets/x86-$1/sys/zsyssvr /zassys/app/zas.system.daemons/
sudo cp -r ~/zas/targets/x86-$1/*.so /zassys/lib/x86_64-linux-gnu/
sudo cp -r ~/zas/targets/x86-$1/*.so /zassys/lib/x86_64-linux-gnu/
else
echo "parameter error.parameter: debug: 'dbg' ; release: 'rel' ;"
fi 



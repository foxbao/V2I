#/bin/bash/


function read_dir()
{
chown $3":"$4 $1
chmod $2 $1
for file in `ls $1`
do
echo $1"/"$file
if [ -d $1"/"$file ]
then
chown $3":"$4 $1"/"$file
chmod $2 $1"/"$file
read_dir $1"/"$file $2 $3 $4 $5 $6 $7
else
chown $6":"$7 $1"/"$file
chmod $5 $1"/"$file
fi
done
} 


read_dir "/zassys/bin" "750" "0" "1000" "750" "0" "1000"
read_dir "/zassys/lib" "755" "0" "1000" "644" "0" "1000"
read_dir "/zassys/app" "750" "0" "1000" "750" "0" "1000"

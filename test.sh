./build.sh
dd if=/dev/random of=a bs=10M count=1 conv=sync
dd if=/dev/random of=b bs=10M count=1 conv=sync
time ./bqpatch diff a b c
time ./bqpatch patch a b1 c
md5a=$(md5sum b |awk '{print $1}')
md5b=$(md5sum b1 |awk '{print $1}')
echo $md5a $md5b
if [ $md5a == $md5b ] ;
then
    echo OK
else
    echo FAILED
fi

rm a b b1 c
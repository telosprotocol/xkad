set -e

rm -rf log
rm -rf core.*

mkdir -p log
ulimit -c unlimited

IP=`/usr/sbin/ip a | grep 'inet ' | grep -v 'inet 127.' | grep -v 'inet 10.' | grep -v 'inet 172.16.' | awk '{print $ 2}' | awk -F/  '{print $1}'`
echo ${IP}

./xkad_bench -a ${IP} -l 10900 -L log/run2fg.log -i 1 -p 157.245.138.194:11000

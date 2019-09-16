set -e

killall -w xkad_bench || echo "no such process"
rm -rf log
rm -rf core.*

mkdir -p log
ulimit -c unlimited

IP=`/usr/sbin/ip a | grep 'inet ' | grep -v 'inet 127.' | grep -v 'inet 10.' | grep -v 'inet 172.16.' | awk '{print $ 2}' | awk -F/  '{print $1}'`
echo ${IP}

for ((i=0;i<25;i++)); do
    echo "launching ${i} ..."
    PORT=$((10000+$i))
    nohup ./xkad_bench -a ${IP} -l $PORT -L log/${i}.log -i 1 -p 157.245.138.194:11000 -g 0 >/dev/null 2>&1 -D 0  &
done

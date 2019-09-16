set -e

rm -rf log2
mkdir -p log2

# launch second nodes
for ((i=1;i<=400;i++)); do # 288bits
# for ((i=1;i<=600;i++)); do # 40bits
    echo "launching ${i} ..."
    port=$((10000+i))
    nohup ./xkad_bench -a 127.0.0.1 -l $port -L log2/${i}.log -i 1 -p 127.0.0.1:8833 -g 0 >/dev/null 2>&1 -D 0  &
    # nohup ./xkad_bench -a 192.168.1.101 -l 0 -L log2/${i}.log -i 1 -p 192.168.50.211:8833 -g 0 >/dev/null 2>&1 &
    # nohup ./xkad_bench -a 192.168.50.211 -l 0 -L log2/${i}.log -i 1 -p 192.168.50.211:8833 -g 0 >/dev/null 2>&1 &
    # nohup ./xkad_bench -a 192.168.50.214 -l 0 -L log2/${i}.log -i 1 -p 192.168.50.211:8833 -g 0 >/dev/null 2>&1 &

    # nohup ./xkad_bench -a 192.168.50.211 -l 0 -L log2/${i}.log -i 1 -p 192.168.50.242:8833 -g 0 >/dev/null 2>&1 &
    # nohup ./xkad_bench -a 192.168.50.214 -l 0 -L log2/${i}.log -i 1 -p 192.168.50.242:8833 -g 0 >/dev/null 2>&1 &
    # nohup ./xkad_bench -a 192.168.50.242 -l 0 -L log2/${i}.log -i 1 -p 192.168.50.242:8833 -g 0 >/dev/null 2>&1 &

    sleep 0.002
done

# foreground
# ./xkad_bench -a 127.0.0.1 -l 8822 -L log2/1.log -i 1 -p 127.0.0.1:8833

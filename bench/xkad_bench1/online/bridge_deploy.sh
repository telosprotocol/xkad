set -e

target=src/xtopcom/xkad/bench/xkad_bench1

# copy xkad_bench
rsync -avz cbuild/bin/Linux/xkad_bench $target/
# rsync -avz cbuild/bin/Darwin/xkad_bench $target/
# strip $target/xkad_bench

# copy bench1
# rsync -avz $target ~/run/

ips=(
    "142.93.88.86"
)

for ip in ${ips[@]}; do
    echo "===== rsyncing $ip ... ====="
    rsync -avz -e "ssh -p 1022 -i src/xtopcom/xkad/bench/xkad_bench1/online/bridge.pem" $target root@$ip:~/blueshi/
done

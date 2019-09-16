# scp -P 1022 -i node.pem topuser@$1:/root/blueshi/xkad_bench1/log.tgz ~/pub/xudp_core/$1_log.tgz
scp -P 1022 -i xkad_bench1/online/node.pem topuser@$1:/root/blueshi/xkad_bench1/log.tgz /root/blueshi/log_collect/$1_log.tgz

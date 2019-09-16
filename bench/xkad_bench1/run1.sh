set -e

rm -rf log1
mkdir -p log1

./xkad_bench -a 127.0.0.1 -l 8833 -L log1/first_node.log
# ./xkad_bench -a 192.168.50.211 -l 8833 -L log1/first_node.log
# ./xkad_bench -a 192.168.50.242 -l 8833 -L log1/first_node.log

set -e

rm -rf log2
mkdir -p log2

# foreground
./xkad_bench -a 127.0.0.1 -l 8822 -L log2/8822.log -i 1 -p 127.0.0.1:8833

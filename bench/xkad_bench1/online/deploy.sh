set -e

target=src/xtopcom/xkad/bench/xkad_bench1

# copy xkad_bench
rsync -avz cbuild/bin/Linux/xkad_bench $target/
# rsync -avz cbuild/bin/Darwin/xkad_bench $target/
# strip $target/xkad_bench

# copy bench1
rsync -avz $target ~/run/

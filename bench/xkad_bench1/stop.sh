set -e

# kill -9 `ps -ef | grep xkad_bench | awk '{print $2}'`
killall xkad_bench || echo "no such processes"

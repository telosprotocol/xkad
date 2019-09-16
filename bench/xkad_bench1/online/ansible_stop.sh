set -e

# stop
ansible -i /root/blueshi/blue_hosts m_bg -m shell -a "cd /root/blueshi/xkad_bench1 && sh stop.sh"

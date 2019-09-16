set -e

# show
ansible -i /root/blueshi/blue_hosts m_bg -m shell -a "cd /root/blueshi/xkad_bench1 && sh show.sh"

# grep core
# ansible -i /root/blueshi/blue_hosts m_bg -m shell -a "cd /root/blueshi/xkad_bench1 && ls -l | grep core"

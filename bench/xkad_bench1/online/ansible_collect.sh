set -e

mkdir -p /root/blueshi_log_collect/x

# collect fg
ansible -i /root/blueshi/blue_hosts m_fg -m synchronize -a "src=/root/blueshi/xkad_bench1 dest=/root/blueshi_log_collect/x/{{inventory_hostname}} mode=pull"

# collect bg
ansible -i /root/blueshi/blue_hosts m_bg -m synchronize -a "src=/root/blueshi/xkad_bench1 dest=/root/blueshi_log_collect/x/{{inventory_hostname}} mode=pull"

set -e

# collect fg
ansible -i /root/blueshi/blue_hosts m_fg -m shell -a "uptime"

# collect bg
ansible -i /root/blueshi/blue_hosts m_bg -m shell -a "uptime"

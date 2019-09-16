set -e

# rsync
ansible -i /root/blueshi/blue_hosts m_fg -m synchronize -a "src=/root/blueshi dest=/root/"

# rsync
ansible -i /root/blueshi/blue_hosts m_bg -m synchronize -a "src=/root/blueshi dest=/root/"

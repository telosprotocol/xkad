set -e

ps -ef | grep xkad_bench | grep -v grep | grep -v 'show.sh'
/usr/sbin/lsof -nPi | grep xkad_ben
uptime

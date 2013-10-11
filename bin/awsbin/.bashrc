# .bashrc

# Source global definitions
if [ -f /etc/bashrc ]; then
        . /etc/bashrc
fi

# User specific aliases and functions
#ulimit -c unlimited
#ulimit -n 2048
#ulimit -u 2048
alias countg='ps ax | grep [g]psi | wc -l'
alias countc='ps ax | grep [s]yncd | wc -l'
alias killg='killall -ABRT gpsi'
alias killc='killall -ABRT syncd lsd adbd'
alias cring='~/bin/grep_local_log.sh 50 lsd 0 total|awk '\''{total=total+$8}END{print total/50}'\'''

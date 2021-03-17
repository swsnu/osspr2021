#!/bin/bash

############################################
# OSSPR2021                                #
#                                          #
# Installs Tmux inside QEMU.               #
############################################

set -ev

TMUX='tmux-arm.tar.gz'
IMAGEDIR='tizen-image'
TMPD="$(mktemp -d)"
TMPF="$(mktemp)"

sudo mount "$IMAGEDIR/rootfs.img" "$TMPD"
sudo tar xzf "$TMUX" -C "$TMPD/root"
cat > "$TMPF" << 'EOF'
# tmux
export PATH="/root/tmux/bin:$PATH"
export LD_LIBRARY_PATH=/root/tmux/lib
export TERMINFO=/root/tmux/share/terminfo

# Detect and set appropriate stty size
old=$(stty -g)
stty raw -echo min 0 time 5
printf '\0337\033[r\033[999;999H\033[6n\0338' > /dev/tty
IFS='[;R' read -r _ rows cols _ < /dev/tty
stty "$old"
stty cols "$cols" rows "$rows"
EOF
sudo mv "$TMPF" "$TMPD/root/.bash_profile"
sync
sudo umount "$TMPD"

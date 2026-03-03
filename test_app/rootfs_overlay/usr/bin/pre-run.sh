#!/bin/sh
# Pre-run script: load kernel modules before BEAM starts

# WiFi: Realtek RTW8852BE (out-of-tree driver)
/sbin/modprobe rtw89_8852be_git 2>/dev/null

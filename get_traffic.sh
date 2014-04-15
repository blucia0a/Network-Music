#!/bin/bash
# Usage:
#   get_traffic.sh [interface]
#
# Example:
#   get_traffic.sh en1
#
# Requires the following ENV variable:
#   $NETWORK_MUSIC_WIFI_KEY_FILE=$HOME/.network-music/keyfile
#
# In that file, put your:
#   -o wlan... argument.

if [ "$#" -eq 0 ]; then
  echo "Usage: get_traffic.sh [interface name (en1, etc)]"
  exit 
fi

#tshark -l -ien1 -o wlan.enable_decryption:TRUE -o wlan.wep_key1:wpa-psk:2dcc65f8d1f2fa08f6789b6533775bf19897bc6434987fc0a756734d82c4332a -nn -Tfields -Eseparator="/t" -e ip.src -e ip.dst -e tcp.srcport -e tcp.dstport -e icmp.code
#echo "Running \"tshark -l -i$1 -o wlan.enable_decryption:TRUE `cat $NETWORK_MUSIC_WIFI_KEY_FILE` -nn -Tfields -Eseparator="/t" -e ip.src -e ip.dst -e tcp.srcport -e tcp.dstport -e icmp.code\""
tshark -l -i$1 -o wlan.enable_decryption:TRUE -nn -Tfields -Eseparator="/t" -e ip.src -e ip.dst -e tcp.srcport -e tcp.dstport
#tshark -l -ien1 -o wlan.enable_decryption:TRUE -o "wlan.wep_key1:sv4j6xma7yz6je" -nn -Tfields -Eseparator="/t" -e ip.src -e ip.dst -e tcp.srcport -e tcp.dstport -e icmp.code -e icmp.mip.length
#tshark -ien1 -o 'wlan.enable_decryption:TRUE' -o "wlan.wep_key1:sv4j6xma7yz6je" -nn -Tfields -Eseparator="/t" -e ip.src -e tcp.srcport -e ip.dst -e tcp.dstport 
#tshark -ien1 -o 'wlan.enable_decryption:TRUE' -o "wlan.wep_key1:sv4j6xma7yz6je" -nn -Tfields -Eseparator="/t" -e ip.src -e tcp.srcport -e ip.dst -e tcp.dstport -e bittorrent.msg

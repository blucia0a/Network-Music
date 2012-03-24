#!/bin/bash

#tshark -l -ien1 -o wlan.enable_decryption:TRUE -o wlan.wep_key1:wpa-psk:2dcc65f8d1f2fa08f6789b6533775bf19897bc6434987fc0a756734d82c4332a -nn -Tfields -Eseparator="/t" -e ip.src -e ip.dst -e tcp.srcport -e tcp.dstport -e icmp.code
tshark -l -ien1 -o wlan.enable_decryption:TRUE -o wlan.wep_key1:sv4j6xma7yz6je -nn -Tfields -Eseparator="/t" -e ip.src -e ip.dst -e tcp.srcport -e tcp.dstport -e icmp.code
#tshark -ien1 -o 'wlan.enable_decryption:TRUE' -o "wlan.wep_key1:sv4j6xma7yz6je" -nn -Tfields -Eseparator="/t" -e ip.src -e tcp.srcport -e ip.dst -e tcp.dstport 
#tshark -ien1 -o 'wlan.enable_decryption:TRUE' -o "wlan.wep_key1:sv4j6xma7yz6je" -nn -Tfields -Eseparator="/t" -e ip.src -e tcp.srcport -e ip.dst -e tcp.dstport -e bittorrent.msg

#!/bin/bash

echo "Configuration of tap interface."

# Which interface is this?
INTERFACE_ID="1"

MAC_1="12:34:56:78:90:ab"
MAC_2="12:34:56:78:90:ac"

IP_1="192.168.123.1"
IP_2="192.168.123.2"

if [[ $INTERFACE_ID -eq 1 ]]; then
	MAC=$MAC_1
	IP=$IP_1
	ARP_IP=$IP_2
	ARP_MAC=$MAC_2
fi
if [[ $INTERFACE_ID -eq 2 ]]; then
	MAC=$MAC_2
	IP=$IP_2
	ARP_IP=$IP_1
	ARP_MAC=$MAC_1
fi

sudo ip tuntap add dev tap0 user ${USER} mode tap; sleep 1

sudo ifconfig tap0 down; sleep 1
sudo ifconfig tap0 hw ether $MAC; sleep 1
sudo ifconfig tap0 mtu 440; sleep 1
sudo ifconfig tap0 up; sleep 1
sudo ifconfig tap0 $IP; sleep 1

sudo route del -net 192.168.123.0/24; sleep 1
sudo route add -net 192.168.123.0/24 mss 400 dev tap0; sleep 1

sudo tc qdisc del dev tap0 root; sleep 1
sudo tc qdisc add dev tap0 root netem delay 10ms; sleep 1

sudo arp -s $ARP_IP $ARP_MAC

echo ""
echo "Configura is done! Check for any errors above."
echo ""
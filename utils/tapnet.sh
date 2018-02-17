#!/bin/bash -e

 ##############################################################################
 #                                                                            #
 # Copyright 2018 Jan Henrik Weinstock                                        #
 #                                                                            #
 # Licensed under the Apache License, Version 2.0 (the "License");            #
 # you may not use this file except in compliance with the License.           #
 # You may obtain a copy of the License at                                    #
 #                                                                            #
 #     http://www.apache.org/licenses/LICENSE-2.0                             #
 #                                                                            #
 # Unless required by applicable law or agreed to in writing, software        #
 # distributed under the License is distributed on an "AS IS" BASIS,          #
 # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   #
 # See the License for the specific language governing permissions and        #
 # limitations under the License.                                             #
 #                                                                            #
 ##############################################################################

# defaults
tapdev=$2
ipaddr="10.0.0.1"
netmask="255.0.0.0"
tapctl="vcml-tapctl"

tapctl_file=$(dirname $0)/$tapctl
if ! [ -f "$tapctl_file" ]; then
    echo "Error: $tapctl not found." >&2
    exit 1
fi

if [ -z $SUDO_USER ]; then
    echo "error: please run with sudo, e.g. 'sudo $0 $@'" >&2
    exit 1
fi

# ask for tap device name if none given in $2
if [ -z $tapdev ]; then
    tapdev="tap0"
    read -p "specify tap interface name: " -ei $tapdev tapdev
fi

start() {
    # check if tapdev is already running
    if [ -n "`grep $tapdev /proc/net/dev`" ]; then
        echo "TAP device $tapdev already running"
        exit 1
    fi

    hifs=($(tail -n +3 /proc/net/dev | cut -f 1 -d ':'))
    ethdev=${hifs[0]}
    echo "* available host interfaces: ${hifs[*]}"

    # get input from user, we need to know which ethernet device is used
    read -p "specify host network interface: "  -ei $ethdev ethdev
    read -p "specify local NAT IP for host: "   -ei $ipaddr ipaddr
    read -p "specify local NETMASK for host: " -ei $netmask netmask

    # create a TAP device for the current user (needs root priv.)
    echo "starting $tapdev with IP $ipaddr/$netmask for user $SUDO_USER"
    $tapctl_file start $tapdev $ipaddr $netmask

    # assign local (NAT) IP address
    ifconfig $tapdev $ipaddr promisc up

    # enable NAT
    echo "starting NAT"
    modprobe iptable_nat
    echo 1 > /proc/sys/net/ipv4/ip_forward
    iptables -t nat -A POSTROUTING -o $ethdev -j MASQUERADE
    iptables -F FORWARD
    iptables -A FORWARD -i $tapdev -j ACCEPT
}

stop() {
    # ask for tap device name
    if [ -z "`grep $tapdev /proc/net/dev`" ] ; then
        echo "TAP device $tapdev not active"
        exit 1
    fi

    # disable NAT
    echo "stopping NAT"
    echo 0 > /proc/sys/net/ipv4/ip_forward
    iptables -F FORWARD
    iptables -A FORWARD -j REJECT --reject-with icmp-host-prohibited
    iptables -t nat -F

    # stop and remove TAP device
    echo "stopping $tapdev"
    ifconfig $tapdev down
    $tapctl_file stop $tapdev
}

restart() {
    stop
    start
}

case "$1" in
  start)
        start
        ;;
  stop)
        stop
        ;;
  restart)
        restart
        ;;
  *)
        echo $"Usage: $0 {start|stop|restart} [tapdev]"
        exit 2
esac

exit $?

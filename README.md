# Kernel Space TAP Tunnel Example

This is an example of TAP tunnel in kernel space.

Only the following packets can be passed in the TAP tunnel:

* ARP request / reply
* IPV4 ICMP request / reply
* IPV4 TCP packets
* IPV4 UDP packets

The Tunnel is built in UDP with source port 50000 = destination port

## File Descriptions

| File                  | Descriptions                                                          |
|-----------------------|-----------------------------------------------------------------------|
| kmain.c               | kernel init / exit functions.                                         |
| ksyscall.c            | re-write some system calls in kernel space.                           |
| ktap.c                | creating and using tap tunnel in kernel space.                        |
| ktx.c                 | tunnel tx part (udp or netpoll).                                      |
| ktunnel.h             | linux heraders, defined values, macros, type / function declarations. |
| krx.c                 | tunnel rx part (udp or net filter).                                   |
| ktuunel_wireshark.lua | simple wireshark dissector for this project.                          |

## Verification Environment

This project works in the following linux distributions:

* Ubuntu 14.04 i386 and amd64 - Kernel version 3.13.0
* Mint 17 i386 and amd64      - Kernel version 3.13.0

## How to test?

Prepare 2 computers : COMPUTER A and COMPUTER B

* COMPUTER A : Real ip address 192.168.1.1
* COMPUTER B : Real ip address 192.168.1.2

Build your proejct, and insert kernel module in COMPUTER A:

    $ cd kernel_tap/
    $ make
    $ sudo insmod ktunnel.ko g_dst=192.168.1.2 g_ip=10.10.10.1 g_mask=255.255.255.0 g_port=50000

You will see the new network interface "tap01" with ipaddr "10.10.10.1"

Do it again in COMPUTER B:

    $ sudo insmod ktunnel.ko g_dst=192.168.1.1 g_ip=10.10.10.2 g_mask=255.255.255.0 g_port=50000

you will see the new network interface "tap01" with ipaddr "10.10.10.2"

In COMPUTER A

    $ ping 10.10.10.2"

If you check packets in wireshark, you will see the ARP and ICMP packets encapsulated in the UDP Tunnel.


## Tunnel Data Transmission Flow

There are 2 Tx modes and 2 Rx modes in this repository.

You can use various combinations by your wish.

### Tx

    User Space Program (e.g. PING)
    -> LINUX TCP/IP Protocol Stack
    -> TAP net device ndo_start_xmit() 
       put data into TAP's queue, wake up poll of this queue

#### UDP Tx

Default tx mode is UDP. Or you could specify the txmode when inserting module.

$ sudo insmod ktunnel.ko ...... g_txmode=udp

    Kernel Tap Read Thread : _readThread
    -> read data from TAP's queue : _tapRead()
       send data by UDP socket

#### Netpoll Tx

$ sudo insmod ktunnel.ko ..... g_txmode=netpoll
    
    Kernel Tap Read Thread : _readThread
    -> read data from TAP's queue : _tapRead()
       get routing table, get arp table information, send data by netpoll_send_udp()

### Rx

#### UDP Rx

Default tx mode is UDP. Or you could specify the rxmode when inserting module.

$ sudo insmod ktunnel.ko ...... g_rxmode=udp
   
    Kernel UDP Recv Thread : _rxThread
    -> recv data from UDP socket : _udpRecv()
       write data to TAP's queue
    -> LINUX TCP/IP Protocol Stack
    -> User Space Program (e.g. PING)

#### Net Filter Rx
   
$ sudo insmod ktunnel.ko .... g_rxmode=filter
 
    -> Netfilter hook function : _netfilterRecv()
       write data to TAP's queue
    -> LINUX TCP/IP Protocol stack
    -> User Space Program (e.g. PING)

### Block Diagram (TX:UDP, RX:UDP)

             COMPUTER A                           COMPUTER B
        192.186.1.1(10.10.10.1)              192.168.1.2(10.10.10.2)

              process                              process
                 |                                    |
                 |                USER                |
        --------------------                --------------------
                 |               KERNEL               |
             +--------+                          +--------+
             |  KTAP  |                          |  KTAP  |
             +--------+                          +--------+
                 | read / write                       |  read / write
                 |                                    |
             +--------+                          +--------+
             |  KUDP  |--------------------------|  KUDP  |
             +--------+       send / recv        +--------+

### Performance Comparison (Ignorable)

In fact, performance difference is *NOT OBVIOUS* between 2 Tx modes and 2 Rx Modes.

But I still list the test results ...

| Tx mode \ Rx mode | UDP    |  Net filter |
|-------------------|--------|-------------|
| UDP               | 4th    |  3rd        |
| Netpoll           | 2nd    |  1st        |

## How to use wireshark dissector?

Check wireshark is installed in your computer and works.

Find the installed path of wireshark, In windows default path is C:\Program Files\Wireshark\

Put "ktuunel_wireshark.lua" to the wireshark installed path.

Open init.lua (in the isntalled path) in your text editor

Go to the file bottom. find the line:

    dofile(DATA_DIR.."console.lua")

Add a new line after it:

    dofile(DATA_DIR.."ktunnel_wireshark.lua")

Save the init.lua, and re-open (not refresh) you wireshark.

The simple dissector will works.

You could use "ktunnel" as the keyword to filter packets.

## References

1.[Example of TUN in User Space](http://neokentblog.blogspot.tw/2014/05/linux-virtual-interface-tuntap.html)

2.[Example of Kernel Space UDP Socket](http://kernelnewbies.org/Simple_UDP_Server)

3.[Example of Sending UDP by Netpoll APIs](http://goo.gl/is95GX)

4.[Example of Net Filter Hook](http://neokentblog.blogspot.tw/2014/06/netfilter-hook.html)


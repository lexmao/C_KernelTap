//////////////////////////////////////////////////////////////////////////////
//
//      Headers
//
//////////////////////////////////////////////////////////////////////////////
#include "ktunnel.h"

//////////////////////////////////////////////////////////////////////////////
//
//      Type Definitions
//
//////////////////////////////////////////////////////////////////////////////
struct my_ktap
{
    bool enable;
    char ifname[IFNAMSIZ];
    char ipaddr[IPADDR_SIZE];
    char netmask[IPADDR_SIZE];

    struct file* file;
    struct task_struct* processThread;
    int interruptNum;

    unsigned char buffer[BUFFER_SIZE];
};


//////////////////////////////////////////////////////////////////////////////
//
//      Global Variables
//
//////////////////////////////////////////////////////////////////////////////
static struct my_ktap _ktap = {};

//////////////////////////////////////////////////////////////////////////////
//
//      Static Functions
//
//////////////////////////////////////////////////////////////////////////////
static struct file* _alloc(char* filename, char* ifname, int flags)
{
    struct ifreq ifr    = {};
    struct file* tapfp = NULL;
    long         ret    = 0;

    if (NULL == filename)
    {
        dprint("filename is null");
        return NULL;
    }

    if (NULL == ifname)
    {
        dprint("ifname is null");
        return NULL;
    }

    tapfp = my_open(filename, O_RDWR);
    if (NULL == tapfp)
    {
        dprint("my open failed");
        return NULL;
    }

    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_flags = flags;

    ret = my_ioctl(tapfp, TUNSETIFF, (unsigned long)&ifr);
    if (0 > ret)
    {
        dprint("my ioctl failed");
        my_close(tapfp);
        return NULL;
    }

    return tapfp;
}

static int _setIpaddr(char* ifname, char* ipaddr)
{
    struct ifreq   ifr    = {};
    long           ret    = 0;
    struct socket* socket = NULL;

    if (NULL == ifname)
    {
        dprint("ifname is null");
        goto _ERROR;
    }

    if (NULL == ipaddr)
    {
        dprint("ipaddr is null");
        goto _ERROR;
    }

    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_addr.sa_family = AF_INET;

    ret = my_inet_pton(AF_INET, ipaddr, ifr.ifr_addr.sa_data+2);
    if (0 > ret)
    {
        dprint("my inet pton failed");
        goto _ERROR;
    }

    socket = my_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (NULL == socket)
    {
        dprint("my socket failed");
        goto _ERROR;
    }

    if (NULL == socket->file)
    {
        dprint("socket->file is null");
        goto _ERROR;
    }

    ret = my_ioctl(socket->file, SIOCSIFADDR, (unsigned long)&ifr);
    if (0 > ret)
    {
        dprint("my ioctl set failed");
        goto _ERROR;
    }

    sock_release(socket);
    return 0;

_ERROR:
    if (NULL != socket)
    {
        sock_release(socket);
    }
    return -1;
}

static int _setNetmask(char* ifname, char* netmask)
{
    struct ifreq   ifr    = {};
    long           ret    = 0;
    struct socket* socket = NULL;

    if (NULL == ifname)
    {
        dprint("ifname is null");
        goto _ERROR;
    }

    if (NULL == netmask)
    {
        dprint("netmask is null");
        goto _ERROR;
    }

    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_addr.sa_family = AF_INET;

    ret = my_inet_pton(AF_INET, netmask, ifr.ifr_addr.sa_data+2);
    if (0 > ret)
    {
        dprint("my inet pton failed");
        goto _ERROR;
    }

    socket = my_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (NULL == socket)
    {
        dprint("my socket failed");
        goto _ERROR;
    }

    if (NULL == socket->file)
    {
        dprint("socket->file is null");
        goto _ERROR;
    }

    ret = my_ioctl(socket->file, SIOCSIFNETMASK, (unsigned long)&ifr);
    if (0> ret)
    {
        dprint("my ioctl failed");
        goto _ERROR;
    }

    sock_release(socket);
    return 0;

_ERROR:
    if (NULL != socket)
    {
        sock_release(socket);
    }
    return -1;
}

static int _enableInterface(char* ifname)
{
    struct ifreq   ifr    = {};
    long           ret    = 0;
    struct socket* socket = NULL;

    if (NULL == ifname)
    {
        dprint("ifname is null");
        goto _ERROR;
    }

    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_addr.sa_family = AF_INET;

    socket = my_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (NULL == socket)
    {
        dprint("my socket failed");
        goto _ERROR;
    }

    if (NULL == socket->file)
    {
        dprint("socket->file is null");
        goto _ERROR;
    }

    ret = my_ioctl(socket->file, SIOCGIFFLAGS, (unsigned long)&ifr);
    if (0 > ret)
    {
        dprint("my ioctl get flags failed");
        goto _ERROR;
    }

    ifr.ifr_flags |= ( IFF_UP | IFF_RUNNING );

    ret = my_ioctl(socket->file, SIOCSIFFLAGS, (unsigned long)&ifr);
    if (0 > ret)
    {
        dprint("my ioctl set flags failed");
        goto _ERROR;
    }

    sock_release(socket);
    return 0;

_ERROR:
    if (NULL != socket)
    {
        sock_release(socket);
    }
    return -1;
}

static bool _isWantedData(void* data, int dataLen)
{
    u16 ethtype;
    u8  ipproto;

    if (NULL == data)
    {
        dprint("data is null");
        return false;
    }

    if (ETH_HLEN >= dataLen)
    {
        dprint("dataLen = %d is too short", dataLen);
        return false;
    }

    data += 12; // DST mac address + SRC mac address
    ethtype = htons( *((u16*)data) );
    data += 2;
    if (ethtype == ETH_P_IP) // IP protocol
    {
        data += 9;
        ipproto = *((u8*)data);
        data += 1;
        if (ipproto == IPPROTO_ICMP) // ICMP
        {
            return true;
        }
        else if (ipproto == IPPROTO_TCP)
        {
            return true;
        }
        else if (ipproto == IPPROTO_UDP)
        {
            return true;
        }
    }
    else if (ethtype == ETH_P_ARP) // ARP
    {
        return true;
    }

    return false;
}

static int _processTapReadData(void *arg)
{
    struct my_ktap *ktap = (struct my_ktap*)arg;
    int  readLen         = 0;
    int  sendLen         = 0;

    if (NULL == arg)
    {
        dprint("arg is null");
        return 0;
    }

    allow_signal(ktap->interruptNum);

    while (!kthread_should_stop())
    {
        readLen = my_read(ktap->file, ktap->buffer, BUFFER_SIZE);
        if (0 >= readLen)
        {
            dprint("readLen = %d <= 0, failed", readLen);
            break;
        }

        if (_isWantedData(ktap->buffer, readLen))
        {
            sendLen = kudp_send(ktap->buffer, readLen);
            if (0 >= sendLen)
            {
                dprint("kudp send failed, sendLen = %d", sendLen);
            }
        }
    }

    dprint("over");
    return 0;
}

static int _initTapProcessThread(struct my_ktap *ktap,
                                 my_threadFn fn,
                                 int irqNum)
{
    if (NULL == ktap)
    {
        dprint("ktap is null");
        return -1;
    }

    if (NULL == fn)
    {
        dprint("thread fn is null");
        return -1;
    }

    ktap->processThread = kthread_create(fn, ktap, "tap process thread");
    if (IS_ERR(ktap->processThread))
    {
        dprint("kthread create failed");
        PTR_ERR(ktap->processThread);
        return -1;
    }

    memset(ktap->buffer, 0, BUFFER_SIZE);
    ktap->interruptNum = irqNum;

    return 0;
}

static int _uninitTapProcessThread(struct my_ktap* ktap)
{
    return 0;
}

static int _startTapProcessThread(struct my_ktap *ktap)
{
    if (NULL == ktap)
    {
        dprint("ktap is null");
        return -1;
    }

    if (NULL == ktap->processThread)
    {
        dprint("process thread is not initialized");
        return -1;
    }

    // start the thread.
    wake_up_process(ktap->processThread);
    return 0;
}

static int _stopTapProcessThread(struct my_ktap *ktap)
{
    if (NULL == ktap)
    {
        dprint("ktap is null");
        return -1;
    }

    if (NULL == ktap->processThread)
    {
        dprint("process thread is not initialized");
        return -1;
    }

    kill_pid(find_vpid(ktap->processThread->pid), ktap->interruptNum, 1);
    kthread_stop(ktap->processThread);
    ktap->processThread = NULL;

    return 0;
}

//////////////////////////////////////////////////////////////////////////////
//
//      Functions
//
//////////////////////////////////////////////////////////////////////////////
int ktap_write(void* data, int dataLen)
{
    if (NULL == _ktap.file)
    {
        dprint("ktap is not initialized");
        return -1;
    }

    if (NULL == data)
    {
        dprint("data is null");
        return -1;
    }

    if (0 == dataLen)
    {
        dprint("dataLen = 0");
        return -1;
    }

    return my_write(_ktap.file, data, dataLen);
}

int ktap_init(char* ifname, char* ipaddr, char* netmask)
{
    int ret = 0;

    if (NULL == ifname)
    {
        dprint("ifname is null");
        goto _ERROR;
    }

    if (NULL == ipaddr)
    {
        dprint("ipaddr is null");
        goto _ERROR;
    }

    if (NULL == netmask)
    {
        dprint("netmask is null");
        goto _ERROR;
    }

    strncpy(_ktap.ifname,  ifname,  IFNAMSIZ);
    strncpy(_ktap.ipaddr,  ipaddr,  IPADDR_SIZE);
    strncpy(_ktap.netmask, netmask, IPADDR_SIZE);

    _ktap.file = _alloc(TAP_FILE_PATH, ifname, IFF_TAP | IFF_NO_PI);
    if (NULL == _ktap.file)
    {
        dprint("alloc tap fp failed");
        goto _ERROR;
    }

    if (0 > _setIpaddr(ifname, ipaddr))
    {
        dprint("tap set ip failed");
        goto _ERROR;
    }

    if (0 > _setNetmask(ifname, netmask))
    {
        dprint("tap set netmask failed");
        goto _ERROR;
    }

    ret = _enableInterface(ifname);
    if (0 > ret)
    {
        dprint("enable interface failed");
        goto _ERROR;
    }

    ret = _initTapProcessThread(&_ktap, _processTapReadData, SIGINT);
    if (0 > ret)
    {
        dprint("init process thread failed");
        goto _ERROR;
    }

    ret = _startTapProcessThread(&_ktap);
    if (0 > ret)
    {
        dprint("start process thread failed");
        goto _ERROR;
    }

    _ktap.enable = true;

    dprint("ifname = %s, ipaddr = %s", ifname, ipaddr);
    dprint("ok");
    return 0;

_ERROR:
    _ktap.enable = false;
    if (NULL == _ktap.file)
    {
        my_close(_ktap.file);
    }
    return -1;
}

void ktap_uninit(void)
{
    if (false == _ktap.enable)
    {
        dprint("ktap is not initialized");
        return;
    }

    _stopTapProcessThread(&_ktap);

    _uninitTapProcessThread(&_ktap);

    my_close(_ktap.file);
    _ktap.file = NULL;

    _ktap.enable = false;

    dprint("ok");
    return;
}
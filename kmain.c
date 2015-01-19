//////////////////////////////////////////////////////////////////////////////
//
//      Headers
//
//////////////////////////////////////////////////////////////////////////////
#include "ktunnel.h"

//////////////////////////////////////////////////////////////////////////////
//
//      License Declarations
//
//////////////////////////////////////////////////////////////////////////////
MODULE_AUTHOR("tacolin");
MODULE_DESCRIPTION("KERNEL SPACE TUNNEL");
MODULE_LICENSE("GPL");

//////////////////////////////////////////////////////////////////////////////
//
//      Global Variables
//
//////////////////////////////////////////////////////////////////////////////
char* g_ifname     = "tap01";
char* g_ip         = NULL;
char* g_mask       = NULL;
char* g_dstRealip  = NULL;
int   g_tunnelPort = 50000;
char* g_txmode     = "udp";
char* g_rxmode     = "udp";

static mm_segment_t _oldfs;

//////////////////////////////////////////////////////////////////////////////
//
//      Static Functions : handle kernel memory access
//
//////////////////////////////////////////////////////////////////////////////
static int _checkModuleNecessaryParameters(void)
{
    if (NULL == g_dstRealip) { return -1; }
    return 0;
}

static void _showModuleParameters(void)
{
    dprint("g_ifname     = %s", g_ifname);
    dprint("g_ip         = %s", g_ip);
    dprint("g_mask       = %s", g_mask);
    dprint("g_dstRealip  = %s", g_dstRealip);
    dprint("g_tunnelPort = %d", g_tunnelPort);
    dprint("g_txmode     = %s", g_txmode);
    dprint("g_rxmode     = %s", g_rxmode);
    return;
}

//////////////////////////////////////////////////////////////////////////////
//
//      Functions : Kernel Modules Init / Exit
//
//////////////////////////////////////////////////////////////////////////////
static int __init ktunnel_init(void)
{
    int retval = -1;

    dprint("");
    dprint("===");

    _oldfs = get_fs();
    set_fs(KERNEL_DS);

    retval = _checkModuleNecessaryParameters();
    CHECK_IF(0 > retval, goto _err_return, "module parameters check failed");

    _showModuleParameters();

    retval = ktunnel_initTap(g_ifname, g_ip, g_mask);
    CHECK_IF(0 > retval, goto _err_return, "init tap failed");

    retval = ktunnel_initTx(g_txmode);
    CHECK_IF(0 > retval, goto _err_return, "init tx failed");

    retval = ktunnel_initRx(g_rxmode, ktunnel_writeTap);
    CHECK_IF(0 > retval, goto _err_return, "init rx failed");

    dprint("ktuunel init ok");
    return 0;

_err_return:
    ktunnel_uninitRx(g_rxmode);
    ktunnel_uninitTx(g_txmode);
    ktunnel_uninitTap();
    set_fs(_oldfs);
    return retval;
}

static void __exit ktunnel_exit(void)
{
    ktunnel_uninitRx(g_rxmode);
    ktunnel_uninitTx(g_txmode);
    ktunnel_uninitTap();

    set_fs(_oldfs);

    dprint("ktuunel exit ok");
    dprint("===");
    dprint("");

    return;
}


module_init(ktunnel_init);
module_exit(ktunnel_exit);

module_param(g_ifname,    charp, S_IRUSR);
module_param(g_ip,        charp, S_IRUSR);
module_param(g_mask,      charp, S_IRUSR);
module_param(g_dstRealip, charp, S_IRUSR);
module_param(g_tunnelPort, int,  S_IRUSR);
module_param(g_txmode,    charp, S_IRUSR);
module_param(g_rxmode,    charp, S_IRUSR);

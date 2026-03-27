/*
 * Copyright (c) 2026 Kyland Inc.
 * Intewell-RTOS is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <assert.h>
#include <errno.h>
#include <net/ethernet_dev.h>
#include <net/if_arp.h>
#include <net/netdev.h>
#include <net/packet.h>
#include <netinet/if_ether.h>
#include <system/kconfig.h>

#if IS_ENABLED(CONFIG_LWIP_2_2_0)
#include <lwip/netif.h>
#include <lwip/pbuf.h>
#include <net/lwip_ethernet.h>
#endif

/*
  len为0时netpkt不包括数据负载空间
  可根据实际情况对netpkt设置ETH_NETPKT_DATA_FREE_INITIATIVE和ETH_NETPKT_BUF_TO_PBUF标志
  netpkt需要通过eth_netpkt_free()进行释放
  注意,如果分配的是POOL类型的PBUF,则netpkt结构在PBUF的payload中,请勿使用eth_netpkt_free()进行释放
*/
ETH_NETPKT *eth_netpkt_alloc(unsigned int len)
{
    ETH_NETPKT *netpkt = NULL;

#ifdef CONFIG_LWIP_RX_USE_PBUF_POOL
    /* 仅len大于0时申请POOL类型的PBUF才有意义 */
    if (len > 0)
    {
        netpkt = eth_lwip_alloc_pool_pbuf(len);

        return netpkt;
    }
#endif

    netpkt = memalign(CONFIG_NETPKT_ALIGNMENT_LEN, sizeof(ETH_NETPKT) + len + CONFIG_NETPKT_RESERVED_LEN);
    if (netpkt == NULL)
    {
        return NULL;
    }

    netpkt->base = (void *)netpkt;
    atomic_set(&netpkt->refs, 1);

    netpkt->next = NULL;

    netpkt->flags = ETH_NETPKT_STANDALONE;

    /* 根据是否分配数据空间设置标志位 */
    if (len == 0)
    {
        netpkt->flags |= ETH_NETPKT_DATA_SEPARATED;
    }
    netpkt->len = len;

    netpkt->reserved_len = CONFIG_NETPKT_RESERVED_LEN;

    if (len == 0)
    {
        netpkt->buf = NULL;
    }
    else
    {
        netpkt->buf = (unsigned char *)netpkt + sizeof(ETH_NETPKT) + CONFIG_NETPKT_RESERVED_LEN;
    }

    netpkt->reserved = (unsigned char *)netpkt + sizeof(ETH_NETPKT);

    return netpkt;
}

void eth_netpkt_inc_ref(ETH_NETPKT *netpkt)
{
    if (netpkt == NULL)
    {
        return;
    }

    assert(atomic_inc_return(&netpkt->refs) >= 2);
}

/*
    引用计数为0时才真正释放，引用计数不为0时返回值为1
    仅释放有ETH_NETPKT_STANDALONE标志的netpkt,ETH_NETPKT_WITHIN_PBUF类型的netpkt由协议栈管理
*/
int eth_netpkt_free(ETH_NETPKT *netpkt)
{
    if (netpkt != NULL && (netpkt->flags & ETH_NETPKT_STANDALONE))
    {
        if (atomic_dec_return(&netpkt->refs) == 0)
        {
            if (netpkt->flags & ETH_NETPKT_DATA_FREE_INITIATIVE)
            {
                free(netpkt->buf);
            }

            free(netpkt);

            return OK;
        }

        return 1;
    }
    else
    {
        return -1;
    }
}

/*
    将netpkt中的数据负载拷贝至指定地址，如网卡描述符中指向的地址空间
    当使用LwIP协议栈时，netpkt->buf指向的是pbuf结构体，因此网卡驱动必须使用本函数进行数据拷贝
    完成拷贝后由调用者决定是否进行Cache刷新操作
*/
int eth_netpkt_to_desc(void *desc_buf, ETH_NETPKT *netpkt)
{
    unsigned char *buf = (unsigned char *)desc_buf;
    unsigned int copy_len = netpkt->len;

    if (netpkt == NULL || netpkt->buf == NULL || desc_buf == NULL)
    {
        return -1;
    }

#if defined(CONFIG_LWIP_2_2_0)
    if (netpkt->flags & ETH_NETPKT_BUF_TO_PBUF)
    {
        eth_lwip_netpkt_to_desc(netpkt, desc_buf);
    }
    else
#endif
    {
        memcpy(buf, netpkt->buf, netpkt->len);
    }

    if (copy_len < ETH_FRAME_MIN_LEN)
    {
        memset(buf + copy_len, 0, ETH_FRAME_MIN_LEN - copy_len);
        netpkt->len = ETH_FRAME_MIN_LEN;
    }

    return OK;
}

/*
    克隆一个包括数据负载的新netpkt
*/
ETH_NETPKT *eth_netpkt_clone(const ETH_NETPKT *netpkt)
{
    ETH_NETPKT *new_netpkt = NULL;

    if (netpkt == NULL || netpkt->len == 0)
    {
        return NULL;
    }

    new_netpkt = memalign(CONFIG_NETPKT_ALIGNMENT_LEN, sizeof(ETH_NETPKT) + CONFIG_NETPKT_RESERVED_LEN + netpkt->len);
    if (new_netpkt == NULL)
    {
        return NULL;
    }

    new_netpkt->base = (void *)new_netpkt;
    atomic_set(&new_netpkt->refs, 1);
    new_netpkt->next = NULL;
    new_netpkt->flags = ETH_NETPKT_STANDALONE;
    new_netpkt->len = netpkt->len;
    new_netpkt->reserved_len = CONFIG_NETPKT_RESERVED_LEN;
    new_netpkt->buf = (unsigned char *)new_netpkt + sizeof(ETH_NETPKT) + CONFIG_NETPKT_RESERVED_LEN;
    new_netpkt->reserved = (unsigned char *)new_netpkt + sizeof(ETH_NETPKT);

    if (new_netpkt != NULL)
    {
        memcpy(new_netpkt->buf, netpkt->buf, netpkt->len);
    }

    return new_netpkt;
}

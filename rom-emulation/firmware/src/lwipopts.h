#ifndef ROM_EMULATION_FIRMWARE_SRC_LWIPOPTS_H
#define ROM_EMULATION_FIRMWARE_SRC_LWIPOPTS_H

// Trimmed-down from pico-examples's lwipopts_examples_common.h.

#define NO_SYS 1

// Memory options.
#define MEM_LIBC_MALLOC 1
#define MEM_ALIGNMENT 4

// DHCP options.
#define LWIP_DHCP 1
#define LWIP_DHCP_DOES_ACD_CHECK 0

// TCP options.
#define TCP_MSS 1460

// Network Interfaces options.
#define LWIP_NETIF_STATUS_CALLBACK 1
#define LWIP_NETIF_LINK_CALLBACK 1
#define LWIP_NETIF_TX_SINGLE_PBUF 1

// Sequential layer options.
#define LWIP_NETCONN 0

// Socket options.
#define LWIP_SOCKET 0

// Statistics options.
#define LWIP_STATS 0

// Checksum options.
#define LWIP_CHKSUM_ALGORITHM 3

#endif

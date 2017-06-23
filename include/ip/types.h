/* ***************************************************************************

  ip4types.h

  header file for the IPv4 data types and helper macros

  May 2017 - Created

*************************************************************************** */

#ifndef __IP4TYPES_H__
#define __IP4TYPES_H__

#include    <sys/types.h>

#include    "ip/error.h"
#include    "ip/options.h"

/* -----------------------------------------
   Ethernet
----------------------------------------- */
#define     HW_ADDR_LENGTH          6           // for IPv4 this is always 6

typedef uint8_t     hwaddr_t[HW_ADDR_LENGTH];   // MAC HW address type
typedef uint32_t    ip4_addr_t;                 // IP address packed into unsigned 32bits

#define IP4_ADDR(a,b,c,d)     (((uint32_t)((d) & 0xff) << 24) | \
                               ((uint32_t)((c) & 0xff) << 16) | \
                               ((uint32_t)((b) & 0xff) << 8)  | \
                                (uint32_t)((a) & 0xff))

#define  IP4_ADDR_ANY               0           // equivalent to 0.0.0.0

#define copy_hwaddr(d,s)      memcpy((d),(s),HW_ADDR_LENGTH)

struct ethernet_frame_t
{
    hwaddr_t    dest;                           // MAC address of destination
    hwaddr_t    src;                            // MAC address of source
    uint16_t    type;                           // ethernet type
    uint8_t     payloadStart;                   // the *address* of this byte is the pointer to the start of the IP packet
};

typedef enum                                    // https://en.wikipedia.org/wiki/EtherType
{
    TYPE_IPV4 = 0x0800,                         // Internet Protocol version 4 (IPv4)
    TYPE_ARP = 0x0806,                          // Address Resolution Protocol (ARP)
    TYPE_WOL = 0x0842,                          // Wake-on-LAN[7]
    TYPE_IETF = 0x22F3,                         // IETF TRILL Protocol
    TYPE_DECNET = 0x6003,                       // DECnet Phase IV
    TYPE_RARP = 0x8035,                         // Reverse Address Resolution Protocol
    TYPE_APPLTALK = 0x809B,                     // AppleTalk (Ethertalk)
    TYPE_AARP = 0x80F3,                         // AppleTalk Address Resolution Protocol (AARP)
    TYPE_VLANTAG = 0x8100,                      // VLAN-tagged frame (IEEE 802.1Q) and Shortest Path Bridging IEEE 802.1aq[8]
    TYPE_IPX = 0x8137,                          // IPX
    TYPE_QNX = 0x8204,                          // QNX Qnet
    TYPE_IPV6 = 0x86DD,                         // Internet Protocol Version 6 (IPv6)
    TYPE_ETHFLOW = 0x8808,                      // Ethernet flow control
    TYPE_COBRA = 0x8819,                        // CobraNet
    TYPE_MPLSUNI = 0x8847,                      // MPLS unicast
    TYPE_MPLSMULTI = 0x8848,                    // MPLS multicast
    TYPE_PPPOEDIC = 0x8863,                     // PPPoE Discovery Stage
    TYPE_PPPOESES = 0x8864,                     // PPPoE Session Stage
    TYPE_JUMBOFRM = 0x8870,                     // Jumbo Frames (Obsoleted draft-ietf-isis-ext-eth-01)
    TYPE_MME = 0x887B,                          // HomePlug 1.0 MME
    TYPE_EAPLAN = 0x888E,                       // EAP over LAN (IEEE 802.1X)
    TYPE_PROFINET = 0x8892,                     // PROFINET Protocol
    TYPE_SCSI = 0x889A,                         // HyperSCSI (SCSI over Ethernet)
    TYPE_ATA = 0x88A2,                          // ATA over Ethernet
    TYPE_ETHCAT = 0x88A4,                       // EtherCAT Protocol
    TYPE_BRIDGE = 0x88A8,                       // Provider Bridging (IEEE 802.1ad) & Shortest Path Bridging IEEE 802.1aq[8]
    TYPE_POWER = 0x88AB,                        // Ethernet Power
    TYPE_GOOSE = 0x88B8,                        // GOOSE (Generic Object Oriented Substation event)
    TYPE_GSE = 0x88B9,                          // GSE (Generic Substation Events) Management Services
    TYPE_ST = 0x88BA,                           // SV (Sampled Value Transmission)
    TYPE_LLDP = 0x88CC,                         // Link Layer Discovery Protocol (LLDP)
    TYPE_SERCOS = 0x88CD,                       // SERCOS III
    TYPE_AVMME = 0x88E1,                        // HomePlug AV MME
    TYPE_MRP = 0x88E3,                          // Media Redundancy Protocol (IEC62439-2)
    TYPE_MACSEC = 0x88E5,                       // MAC security (IEEE 802.1AE)
    TYPE_PBBB = 0x88E7,                         // Provider Backbone Bridges (PBB) (IEEE 802.1ah)
    TYPE_PTP = 0x88F7,                          // Precision Time Protocol (PTP) over Ethernet (IEEE 1588)
    TYPE_PRP = 0x88FB,                          // Parallel Redundancy Protocol (PRP)
    TYPE_CFM = 0x8902,                          // IEEE 802.1ag Connectivity Fault Management (CFM) Protocol / ITU-T Recommendation Y.1731 (OAM)
    TYPE_FCOE = 0x8906,                         // Fibre Channel over Ethernet (FCoE)
    TYPE_FCOEINIT = 0x8914,                     // FCoE Initialization Protocol
    TYPE_ROCE = 0x8915,                         // RDMA over Converged Ethernet (RoCE)
    TYPE_TTE = 0x891D,                          // TTEthernet Protocol Control Frame (TTE)
    TYPE_HSR = 0x892F,                          // High-availability Seamless Redundancy (HSR)
    TYPE_ECTP = 0x9000,                         // Ethernet Configuration Testing Protocol
    TYPE_VLAN2TAG = 0x9100                      // VLAN-tagged (IEEE 802.1Q) frame with double tagging
} ether_type_t;

/* -----------------------------------------
   IPv4
----------------------------------------- */
#define     IP_VER          0x40                // IPv4 version
#define     IP_IHL          0x05                // default header length
#define     IP_QOS          0                   // best effort, no congestion control possible
#define     IP_FLAG_DF      0x4000              // don't fragment on send
#define     IP_FLAG_MF      0x2000              // more fragments when receiving fragmented packets
#define     IP_TTL          64                  // default TTL value

struct ip_header_t                              // https://en.wikipedia.org/wiki/Network_packet
{
    uint8_t     verHeaderLength;                // version (top 4 bits) and header length (bottom 4 bits)
    uint8_t     qos;                            // quality of service
    uint16_t    length;                         // packet length in bytes
    uint16_t    id;                             // packet ID for reassembly
    uint16_t    defrag;                         // fragment offset and flags
    uint8_t     ttl;                            // time to live
    uint8_t     protocol;                       // protocol ID: TCP, UDP, ICMP, etc (ip4_protocol_t)
    uint16_t    checksum;                       // header checksum
    ip4_addr_t  srcIp;                          // source IP address
    ip4_addr_t  destIp;                         // destination IP address
    uint8_t     payloadStart;                   // the *address* of this byte is the pointer to the IP options or the per-protocol options
};

typedef enum
{
    IP4_ICMP =  1,
    IP4_TCP  =  6,
    IP4_UDP  = 17
} ip4_protocol_t;

/* -----------------------------------------
   ARP
----------------------------------------- */
typedef enum
{
    ARP_FLAG_INIT = 0,                          // empty ARP table slot
    ARP_FLAG_STATIC = 1,                        // static entry in the ARP table, will not be removed or replaced
    ARP_FLAG_DYNA = 2,                          // dynamic entry, can we swapped out if table becomes full
    ARP_FLAG_SHOW = 128                         // used with arp_show()
} arp_flags_t;

#define     ARP_ETH_TYPE            1
#define     ARP_OP_REQUEST          1
#define     ARP_OP_REPLY            2
#define     ARP_HLEN                HW_ADDR_LENGTH
#define     ARP_PLEN                4

struct arp_t                                    // https://en.wikipedia.org/wiki/Address_Resolution_Protocol
{
    uint16_t    htype;                          // Hardware type (HTYPE) This field specifies the network protocol type. Example: Ethernet is 1
    uint16_t    ptype;                          // protocol type from ether_type_t
    uint8_t     hlen;                           // hardware address length (IPv4 = 6)
    uint8_t     plen;                           // protocol address length  (IPv4 = 4)
    uint16_t    oper;                           // 1 = request, 2 = reply
    hwaddr_t    sha;                            // sender HW address (MAC address
    ip4_addr_t  spa;                            // sender protocol address (IP address)
    hwaddr_t    tha;                            // target HW address (MAC address
    ip4_addr_t  tpa;                            // target protocol address (IP address)
};

struct arp_tbl_t                                // data structure of ARP table entry
{
    ip4_addr_t  ipAddress;                      // IP address
    hwaddr_t    hwAddress;                      // MAC address
    arp_flags_t flags;                          // flags
    uint8_t     lru;                            // address usage counter for optional LRU garbage collection
};

/* -----------------------------------------
   ICMP
----------------------------------------- */
struct icmp_t                                   // https://en.wikipedia.org/wiki/Internet_Control_Message_Protocol
{
    uint16_t    type_code;                      // ICMP type and subtype (icmp_msg_t)
    uint16_t    checksum;                       // ICMP header and data checksum
    uint16_t    id;                             // identifier
    uint16_t    seq;                            // sequence number
    uint8_t     payloadStart;                   // the *address* of this byte is the pointer to the start of the ICMP optional payload
};

typedef enum
{
    ECHO_REPLY = 0x0000,                        // Echo reply to a ping
    ECHO_REQ = 0x0800,                          // Echo request (used by ping)
    ROUTER_SOLIC = 0x0a00                       // sent to any routers to get their info
} icmp_msg_t;

/* -----------------------------------------
   UDP
----------------------------------------- */
struct udp_t
{
    uint16_t    srcPort;                                        // source application port
    uint16_t    destPort;                                       // destination application port
    uint16_t    length;                                         // length in bytes of the UDP header and UDP data, min. 8
    uint16_t    checksum;                                       // checksum. can be '0' if checksum is not used
    uint8_t     payloadStart;
};

typedef enum                                                    // protocol control block state
{
    FREE        = 0,                                            // PCB is free to use
    BOUND       = 1                                             // protocol is bound
} pcb_state_t;

typedef void (*udp_recv_callback)(struct pbuf_t* const,         // UDP data receive callback function
                                  const ip4_addr_t,             // passes pointer to pbuf, source IP
                                  const uint16_t);              // and source port

struct udp_pcb_t
{
    ip4_addr_t  localIP;                                        // local IP address to bind this PCB
    uint16_t    localPort;                                      // local port to bind this PCB
    ip4_addr_t  remoteIP;                                       // remote IP source
    uint16_t    remotePort;                                     // remote port of source IP
    udp_recv_callback   udp_callback;                           // UDP data receiver callback handler
    pcb_state_t state;                                          // PCB state
};

/* -----------------------------------------
   Packet buffer
----------------------------------------- */
#define     PBUF_FREE               0                           // packet buffer is free to use
#define     PBUF_MARKED            -1                           // packet buffer is in use, but not populated with data

struct pbuf_t
{
    int         len;                                            // bytes count in buffer, == 0 is puffer is free
    uint8_t     pbuf[PACKET_BUF_SIZE];                          // packet buffer type
};

/* -----------------------------------------
   Network interface
----------------------------------------- */
#define     IF_FLAG_INIT            NETIF_FLAG_NONE
#define     NETIF_FLAG_NONE         0x00                        // clear all flags
#define     NETIF_FLAG_UP           0x01                        // interface is up (note: link may be down)
#define     NETIF_FLAG_LINK_UP      0x02                        // link is up or down
#define     NETIF_FLAG_MULTICAST    0x04                        // process multicast
#define     NETIF_FLAG_BROADCAST    0x08                        // process broadcast

struct net_interface_t                                          // general network interface type
{
    hwaddr_t    hwaddr;                                         // interface's MAC address
    ip4_addr_t  ip4addr;                                        // IPv4 address association
    ip4_addr_t  subnet;                                         // IPv4 subnet mask
    ip4_addr_t  gateway;                                        // IPv4 gateway
    ip4_addr_t  network;                                        // calculate bitwise: subnet & gateway
    uint8_t     flags;                                          // status flags
    char        name[ETHIF_NAME_LENGTH];                        // interface name identifier string
    uint32_t    sent;                                           // sent frames count
    uint32_t    recv;                                           // received frames count
    uint32_t    rxDrop;                                         // dropped frames count
    uint32_t    txDrop;                                         // dropped frames count
    struct arp_tbl_t arpTable[ARP_TABLE_LENGTH];                // this isterface's ARP table
    void       *state;                                          // pointer to interface's ethernet driver 'private' data

    ip4_err_t   (*output)(struct net_interface_t* const,        // packet output function with address resolution,
                          struct pbuf_t* const);                // first resolve HW address then send with 'linkoutput'
    void        (*forward_input)(struct pbuf_t* const,          // forward data to next layer   // @@ don't think we need a 'netif' here?
                                 struct net_interface_t* const);

    struct pbuf_t* const (*linkinput)(struct enc28j60_t* const);// link input function, set to link_input()
    ip4_err_t   (*linkoutput)(struct enc28j60_t*,               // packet output function, send data buffer as-is
                              struct pbuf_t* const);            // set to link_output()
    struct enc28j60_t* (*driver_init)(void);                    // pointer to HW and driver initialization function
    int         (*linkstate)(void);                             // check and return link state 'up' or 'down'
                                                                // @@ 'linkstate' link state change call-back function
};

/* -----------------------------------------
   IPv4 stack main data structure
----------------------------------------- */
#define     FRAME_HDR_LEN   (sizeof(struct ethernet_frame_t)-1) // =14
#define     IP_HDR_LEN      (sizeof(struct ip_header_t)-1)      // =20
#define     ARP_LEN         sizeof(struct arp_t)                // =28
#define     ICMP_HDR_LEN    (sizeof(struct icmp_t)-1)           // =8
#define     UDP_HDR_LEN     (sizeof(struct udp_t)-1)            // =8

typedef enum                                                    // stack event signals
{
    SIG_TX_PACKET_DROP,
    SIG_RX_PACKET_DROP,
    SIG_ARP_ERROR
} stack_sig_t;

struct route_tbl_t                                              // source: https://en.wikipedia.org/wiki/Routing_table
{
    ip4_addr_t      destNet;                                    // destination network, calculated with subnet mask
    ip4_addr_t      netMask;                                    // subnet mask
    ip4_addr_t      gateway;                                    // default gateway
    uint8_t         netIf;                                      // network interface
};

struct ip4stack_t
{
    char                    hostname[HOSTNAME_LENGTH];          // host name string identifier
    stack_sig_t             signal;                             // event signal
    uint8_t                 routeTableLength;                   // max number of entries in the route table
    struct route_tbl_t      routeTable[ROUTE_TABLE_LENGTH];     // routing table info
    uint8_t                 interfaceCount;                     // number of attached interfaces
    struct net_interface_t  interfaces[INTERFACE_COUNT];        // array of interface data structures
    void (*icmp_input_handler)(struct pbuf_t* const);           // ICMP response handler
    void (*udp_input_handler)(struct pbuf_t* const);            // UDP packet input handler
    void (*tcp_input_handler)(struct pbuf_t* const);            // TCP packet input handler
};

struct timer_t
{
    uint32_t    timeout;                                        // timeout
};

#endif /* __IP4TYPES_H__ */

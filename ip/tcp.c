/* ***************************************************************************

  tcp.c

  Code module for TCP transport protocol
  This module implements the functionality needed for TCP transport

  resources:
    https://tools.ietf.org/html/rfc793
    https://tools.ietf.org/html/rfc1122
    http://www.cs.dartmouth.edu/~campbell/cs60/socketprogramming.html
    https://en.wikipedia.org/wiki/Transmission_Control_Protocol
    http://www.tcpipguide.com/

  May 2017 - Created

    TODO:
    Errata ID: 1572, 3300, 3305, 4785, 2298,
    RFC 1122 sections:
        4.2.2.3  Window Size: RFC-793 Section 3.1

*************************************************************************** */

#include    <malloc.h>
#include    <string.h>
#include    <assert.h>

#include    <stdio.h>           // for debug time

#include    "ip/tcp.h"
#include    "ip/ipv4.h"
#include    "ip/stack.h"

/* -----------------------------------------
   module macros and types
----------------------------------------- */
#define     CIRC_BUFFER_MASK    ((uint16_t)TCP_DATA_BUF_SIZE -1)

#define     send_syn(p)         send_segment(p, TCP_FLAG_SYN)
#define     send_syn_ack(p)     send_segment(p, (TCP_FLAG_SYN+TCP_FLAG_ACK))
#define     send_ack(p)         send_segment(p, TCP_FLAG_ACK)
#define     send_fin(p)         send_segment(p, TCP_FLAG_FIN)
#define     send_fin_ack(p)     send_segment(p, (TCP_FLAG_FIN+TCP_FLAG_ACK))
#define     send_rst(p)         send_segment(p, TCP_FLAG_RST)

#define     set_state(p,s)      {                                                                           \
                                    printf(" connection %d, state change %d -> %d\n",p,tcpPCB[p].state,s);  \
                                    tcpPCB[p].state = s;                                                    \
                                    tcpPCB[p].timeInState = stack_time();                                   \
                                }

#define     send_sig(p,s)       {                                               \
                                    if ( tcpPCB[p].tcp_notify_fn != NULL )      \
                                        tcpPCB[p].tcp_notify_fn(p, s);          \
                                }

struct syn_opt_t                // structure to ease options setup when SYN flag is on
{
    uint8_t     tsOpt;          // time stamp option =8
    uint8_t     tsOptLen;       // time stamp option length =10
    uint32_t    tsTime;         // tx time stamp
    uint32_t    tsEcho;         // echo of received time stamp
    uint8_t     mssOpt;         // mss option =2
    uint8_t     mssOptLen;      // mss option length =4
    uint16_t    mss;            // mss value
    uint16_t    endOfOpt;       // filler =0
};

struct opt_t                    // structure to ease options setup when SYN flag is off
{
    uint8_t     tsOpt;          // time stamp option =8
    uint8_t     tsOptLen;       // time stamp option length =10
    uint32_t    tsTime;         // tx time stamp
    uint32_t    tsEcho;         // echo of received time stamp
    uint16_t    endOfOpt;       // filler =0
};

#define         SYN_OPT_BYTES   sizeof(struct syn_opt_t)        // in uint8_t
#define         SYN_OPT_LEN     ((SYN_OPT_BYTES / 4)+5)         // in uint32_t
#define         OPT_BYTES       sizeof(struct opt_t)            // in uint8_t
#define         OPT_LEN         ((OPT_BYTES / 4)+5)             // in uint32_t

struct pseudo_header_t
{
    ip4_addr_t  srcIp;
    ip4_addr_t  destIp;
    uint8_t     zero;
    uint8_t     protocol;
    uint16_t    tcpLen;
};

/* -----------------------------------------
   module globals
----------------------------------------- */
struct tcp_pcb_t    tcpPCB[TCP_PCB_COUNT];                      // TCP protocol control blocks
uint8_t             sendBuff[TCP_PCB_COUNT][TCP_DATA_BUF_SIZE]; // set of transmit buffers, one per PCB
uint8_t             recvBuff[TCP_PCB_COUNT][TCP_DATA_BUF_SIZE]; // set of receive buffers, one per PCB

/* -----------------------------------------
   static functions
----------------------------------------- */
static void      tcp_input_handler(struct pbuf_t* const);
static pcbid_t   find_pcb(pcb_state_t, ip4_addr_t, uint16_t, ip4_addr_t, uint16_t);
static ip4_err_t send_segment(pcbid_t, uint16_t);
static void      get_tcp_opt(uint8_t, uint8_t*, struct tcp_opt_t*);
static uint32_t  pseudo_header_sum(pcbid_t, uint16_t);
static void      tcp_timeout_handler(uint32_t);
static void      free_tcp_pcb(pcbid_t);

/*------------------------------------------------
 * tcp_init()
 *
 *  initialize TCP
 *
 * param:  none
 * return: none
 *
 */
void tcp_init(void)
{
    pcbid_t     i;

    for (i = 0; i < TCP_PCB_COUNT; i++)                     // initialize PCB list
    {
        memset(&(tcpPCB[i]), 0, sizeof(struct tcp_pcb_t));  // clear variable and set state
        tcpPCB[i].state = FREE;

        tcpPCB[i].send = &(sendBuff[i][0]);                 // link to send and receive buffers
        tcpPCB[i].recv = &(recvBuff[i][0]);
    }

    stack_set_protocol_handler(IP4_TCP, tcp_input_handler); // setup the stack handler for incoming TCP segments
    stack_set_timer(250, tcp_timeout_handler);              // timeout handler runs every 250mSec
}

/*------------------------------------------------
 * tcp_new()
 *
 *  return a TCP PCB that was created.
 *  this is the first function to call before using a TCP connection.
 *
 * param:  none
 * return: PCB ID or negative ip4_err_t value if failed, possible out of memory for PCB (see options.h)
 *
 */
pcbid_t tcp_new(void)
{
    int     i;

    for (i = 0; i < TCP_PCB_COUNT; i++)                     // scan PCB list
    {
        if ( tcpPCB[i].state == FREE )
        {
            return i;                                       // return TCP PCB address if a free one exists
        }
    }

    return ERR_PCB_ALLOC;                                   // return error if no TCP PCB is available
}

/*------------------------------------------------
 * tcp_notify()
 *
 *  notify a bound connection of changes, such as remote disconnection
 *
 * param:  valid PCB ID and pointer to callback function
 * return: ERR_OK if no errors or ip4_err_t with error code
 *
 */
ip4_err_t tcp_notify(pcbid_t pcbId, tcp_notify_callback notify_callback_fn)
{
    if ( pcbId >= TCP_PCB_COUNT )
        return ERR_PCB_ALLOC;

    if ( tcpPCB[pcbId].state != BOUND &&                    // only works for BOUND or LISTEN'ing PCBs
         tcpPCB[pcbId].state != LISTEN )
    {
        return ERR_NOT_BOUND;
    }

    tcpPCB[pcbId].tcp_notify_fn = notify_callback_fn;
    return ERR_OK;
}

/*------------------------------------------------
 * tcp_bind()
 *
 *  bind a PCB connection to a local IP address and a port
 *  this function should be called after tcp_new() and before using other TCP functions
 *  TODO if port number is '0', the function will bind to first available arbitrary port number
 *
 * param:  PCB ID to bind, local IP 'addr' and 'port'
 * return: ERR_OK if no errors or ip4_err_t with error code
 *
 */
ip4_err_t tcp_bind(pcbid_t pcbId, ip4_addr_t addr, uint16_t port)
{
    int         i;

    if ( pcbId >= TCP_PCB_COUNT )
        return ERR_PCB_ALLOC;

    for (i = 0; i < TCP_PCB_COUNT; i++)                     // scan PCB list
    {
        if ( tcpPCB[i].state == FREE ||                     // skip free PCBs or this 'pcbId'
             i == pcbId )
            continue;

        if ( tcpPCB[i].localIP == addr &&                   // check if another PCB is already bound to this local IP/port
             tcpPCB[i].localPort == port)
        {
            return ERR_IN_USE;                              // exit with error if already in use
        }
    }

    tcpPCB[pcbId].localIP = addr;                           // ok to bind
    tcpPCB[pcbId].localPort = port;
    set_state(pcbId,BOUND);

    return ERR_OK;
}

/*------------------------------------------------
 * tcp_close()
 *
 *  close a TCP connection created with tcp_new(), and clear its PCB
 *  this is the last function to call to close a TCP connection and free its PCB.
 *
 * param:  PCB ID to close
 * return: ERR_OK if no errors or ip4_err_t with error code
 *
 */
ip4_err_t tcp_close(pcbid_t pcbId)
{
    ip4_err_t   result = ERR_OK;

    PRNT_FUNC;

    printf(" pcbId %d, state=%d\n", pcbId, tcpPCB[pcbId].state);

    if ( pcbId >= TCP_PCB_COUNT )
        return ERR_PCB_ALLOC;

    switch ( tcpPCB[pcbId].state )
    {
        case FREE:
            break;

        /* close the connection and free any resources
         */
        case BOUND:
        case LISTEN:
        case SYN_SENT:
            free_tcp_pcb(pcbId);
            break;

        /* in this TCP implementation no SENDs have been issued and there is
         * no pending data to send, then form a FIN segment and send it,
         * and enter FIN-WAIT-1 state;
         */
        case SYN_RECEIVED:
            result = send_fin_ack(pcbId);
            if ( result == ERR_OK ||
                 result == ERR_ARP_QUEUE )
            {
                set_state(pcbId,FIN_WAIT1);
            }
            break;

        /* TODO queue this until all preceding SENDs have been segmentized, then
         * form a FIN segment and send it.  In any case, enter FIN-WAIT-1 state.
         */
        case ESTABLISHED:
            result = send_fin_ack(pcbId);
            if ( result == ERR_OK ||
                 result == ERR_ARP_QUEUE )
            {
                set_state(pcbId,FIN_WAIT1);
            }
            break;

        /* TODO queue this request until all preceding SENDs have been
         * segmentized; then send a FIN segment, enter LAST_ACK state.
         */
        case CLOSE_WAIT:
            result = send_fin_ack(pcbId);
            if ( result == ERR_OK ||
                 result == ERR_ARP_QUEUE )
            {
                set_state(pcbId,LAST_ACK);
            }
            break;

        /* Strictly speaking, this is an error and should receive a "error:
         * connection closing" response.  An "ok" response would be
         * acceptable, too, as long as a second FIN is not emitted (the first
         * FIN may be retransmitted though).
         */
        case FIN_WAIT1:
        case FIN_WAIT2:
        case CLOSING:
        case LAST_ACK:
        case TIME_WAIT:
            result = ERR_TCP_CLOSING;
            break;

        default:;
    }

    return result;
}

/*------------------------------------------------
 * tcp_listen()
 *
 *  this function starts a server's passive-open waiting
 *  to accept up to TCP_MAX_ACCEPT incoming client connections
 *
 * param:  valid PCB ID
 * return: ERR_OK if no errors or ip4_err_t with error code
 *
 */
ip4_err_t tcp_listen(pcbid_t pcbId)
{
    if ( pcbId >= TCP_PCB_COUNT )
        return ERR_PCB_ALLOC;

    if ( tcpPCB[pcbId].state != BOUND )                     // only works for BOUND state PCBs
        return ERR_NOT_BOUND;

    set_state(pcbId,LISTEN);                                // set to LISTEN state

    return ERR_OK;
}

/*------------------------------------------------
 * tcp_accept()
 *
 *  this function register an accept callback that will
 *  be called when a client connects to the open server PCB
 *
 * param:  valid PCB ID and pointer to callback function
 * return: ERR_OK if no errors or ip4_err_t with error code
 *
 */
ip4_err_t tcp_accept(pcbid_t pcbId, tcp_accept_callback accect_callback_fn)
{
    if ( pcbId >= TCP_PCB_COUNT )
        return ERR_PCB_ALLOC;

    if ( tcpPCB[pcbId].state != LISTEN )                    // only works for LISTENing state PCBs
        return ERR_NOT_LSTN;

    tcpPCB[pcbId].tcp_accept_fn = accect_callback_fn;
    return ERR_OK;
}

/*------------------------------------------------
 * tcp_connect()
 *
 *  this function connects a TCP client to a server's IP address and a port
 *
 * param:  valid PCB ID, remote server IP and port
 * return: ERR_OK if no errors or ip4_err_t with error code
 *
 */
ip4_err_t tcp_connect(pcbid_t pcbId, ip4_addr_t serverIP, uint16_t serverPort)
{
    ip4_err_t       result;

    if ( pcbId >= TCP_PCB_COUNT )
        return ERR_PCB_ALLOC;

    if ( tcpPCB[pcbId].state != BOUND )                     // contrary to typical socket API and RFC 793, must be BOUND state PCBs
            return ERR_NOT_BOUND;

    tcpPCB[pcbId].remoteIP = serverIP;                      // update PCB with local and remote servers IP/port
    tcpPCB[pcbId].remotePort = serverPort;
    tcpPCB[pcbId].ISS = stack_time();                       // select an initial ISS
    tcpPCB[pcbId].SND_UNA = tcpPCB[pcbId].ISS;              // UNA and NXT are equal before sending the SYN
    tcpPCB[pcbId].SND_NXT = tcpPCB[pcbId].ISS;              // NXT will be updated by send_syn() if it is successful
    tcpPCB[pcbId].SND_opt.mss = MSS;

    tcpPCB[pcbId].SND_WND = 0;
    tcpPCB[pcbId].SND_UP = 0;
    tcpPCB[pcbId].SND_WL1 = 0;
    tcpPCB[pcbId].SND_WL2 = 0;

    tcpPCB[pcbId].RCV_WND = TCP_DEF_WINDOW;

    result = send_syn(pcbId);                               // send SYN segment to server

    if ( result == ERR_OK ||
         result == ERR_ARP_QUEUE )
    {
        set_state(pcbId,SYN_SENT);                          // (if send successful) put PCB into SYN_SENT state
        result = ERR_OK;
    }

    return result;
}

/*------------------------------------------------
 * tcp_is_connected()
 *
 *  this function is used to poll if a tcp_connect() request was successful
 *  create this function instead of a callback method upon connection
 *
 * param:  valid PCB ID
 * return: '0' not connected, '1' connected
 *
 */
int tcp_is_connected(pcbid_t pcbId)
{
    if ( pcbId >= TCP_PCB_COUNT )
        return 0;

    return ( tcpPCB[pcbId].state == ESTABLISHED ? 1 : 0);   // return state
}

/*------------------------------------------------
 * tcp_send()
 *
 *  send data from a buffer, returns byte count actually sent
 *  create this function instead of a callback method upon connection
 *
 * param:  valid PCB ID, application/user source buffer, byte count to send,
 *         flags: 0 or TCP_FLAG_PSH or TCP_FLAG_URG (TCP_FLAG_URG not implemented)
 * return: byte count actually sent, or ip4_err_t error code 
 *
 */
int tcp_send(pcbid_t pcbId, uint8_t* const data, uint16_t count, uint16_t flags)
{
    return 0;
}

/*------------------------------------------------
 * tcp_recv()
 *
 *  received data, returns byte counts read into application/user buffer
 *  create this function instead of a callback method upon connection
 *
 * param:  valid PCB ID, application/user receive buffer, byte count available in receive buffer
 * return: byte counts read into application/user buffer, or ip4_err_t error code 
 *
 */
int tcp_recv(pcbid_t pcbId, uint8_t* const data, uint16_t count)
{
    int     result = 0;
    int     bytes, i;

    if ( count == 0 || data == NULL )
        return 0;
    
    if ( pcbId >= TCP_PCB_COUNT )
        return ERR_PCB_ALLOC;

    switch ( tcpPCB[pcbId].state )
    {
        case FREE:
        case BOUND:
            result = ERR_TCP_CLOSED;
            break;

        /* TODO: Queue for processing after entering ESTABLISHED state.
         * If there is no room to queue this request, respond with
         *  "error: insufficient resources".
         */
        case LISTEN:
        case SYN_RECEIVED:
        case SYN_SENT:
            result = ERR_MEM;
            break;
            
        /* Since the remote side has already sent FIN, RECEIVEs must be
         * satisfied by text already on hand, but not yet delivered to the
         * user.  If no text is awaiting delivery, the RECEIVE will get a
         * "error:  connection closing" response.  Otherwise, any remaining
         * text can be used to satisfy the RECEIVE.
         *
         * TODO: If RCV.UP is in advance of the data currently being passed to the
         * user notify the user of the presence of urgent data.
         */
        case CLOSE_WAIT:
            if ( tcpPCB[pcbId].recvCnt == 0 )                                           // if no data, none will be received any more
            {
                result = ERR_TCP_CLOSING;                                               // signal the connection is closing
            }                                                                           // otherwise fall through to transfer the data to the application
            /* no break */

        case ESTABLISHED:
        case FIN_WAIT1:
        case FIN_WAIT2:
            if ( (bytes = tcpPCB[pcbId].recvCnt) > 0 )                                  // determine if any data is available for the application
            {
                if (bytes > count)                                                      // adjust count to lower number
                    bytes = count;

                for ( i = 0; i < bytes; i++ )                                           // byte copy loop
                {
                    data[i] = tcpPCB[pcbId].recv[tcpPCB[pcbId].recvRDp];                // copy a byte
                    tcpPCB[pcbId].recvCnt--;                                            // decrement count
                    tcpPCB[pcbId].recvRDp++;                                            // adjust buffer read pointer
                    tcpPCB[pcbId].recvRDp &= CIRC_BUFFER_MASK;                          // quick way to make pointer circular
                }
                tcpPCB[pcbId].RCV_WND += bytes;                                         // adjust windows size to space in buffer
                result = bytes;                                                         // return number of bytes copied
            }
            break;

        case CLOSING:
        case LAST_ACK:
        case TIME_WAIT:
            result = ERR_TCP_CLOSING;
            break;
            
        default:;
    }
    
    return result;
}

/*------------------------------------------------
 * tcp_remote_addr()
 *
 *  get remote address of a connection
 *
 * param:  valid PCB ID
 * return: IPv4 address in network order
 *
 */
ip4_addr_t tcp_remote_addr(pcbid_t pcbId)
{
    if ( pcbId >= TCP_PCB_COUNT )
        return 0;

    return tcpPCB[pcbId].remoteIP;
}


/*------------------------------------------------
 * tcp_remote_port()
 *
 *  get remote port of a connection
 *
 * param:  valid PCB ID
 * return: port number
 *
 */
uint16_t tcp_remote_port(pcbid_t pcbId)
{
    if ( pcbId >= TCP_PCB_COUNT )
        return 0;

    return tcpPCB[pcbId].remotePort;
}

/*------------------------------------------------
 * tcp_input_handler()
 *
 *  this function links to the stack and provides a general
 *  entry point into TCP handling.
 *  This handler will be called by the stack when a TCP protocol packet is waiting
 *  for input processing. The function will parse the input packet and call
 *  the appropriate bound TCP PCB. The function implements the TCP state machine.
 *
 * param:  pointer to an input pbuf
 * return: none
 *
 */
static void tcp_input_handler(struct pbuf_t* const p)
{
    struct ip_header_t *ip;
    struct tcp_t       *tcp;
    uint8_t            *dp;
    ip4_addr_t          addrLocal, addrRemote;
    uint16_t            portLocal, portRemote;
    uint16_t            flags;
    uint16_t            dataOff;
    pcbid_t             pcbId, newConnPcb;
    int                 bytes, i;
    ip4_err_t           result;

    PRNT_FUNC;

    ip = (struct ip_header_t*) &(((struct ethernet_frame_t*)(p->pbuf))->payloadStart);  // pointer to IP packet header
    tcp = (struct tcp_t*)(((uint8_t*) ip) + ((ip->verHeaderLength & 0x0f) * 4));        // pointer to the TCP header

    addrLocal = ip->destIp;                                                             // extract destination IP and port
    portLocal = stack_ntoh(tcp->destPort);
    addrRemote = ip->srcIp;                                                             // extract source IP and port
    portRemote = stack_ntoh(tcp->srcPort);

    /* TODO run TCP checksum test and drop packet if
     * TCP checksum does not match.
     */

    /* parse the incoming packet by examining the TCP flags and determining
     * the actions/state-change to be taken. The processing here follows
     * RFP 793 Event Processing for Arriving Segments
     *
     */
    pcbId = find_pcb(ANY_STATE, addrLocal, portLocal, addrRemote, portRemote);              // first find a fully qualified PCB
    if ( pcbId < 0 )                                                                        // if PCB not found
    {
        pcbId = find_pcb(LISTEN, addrLocal, portLocal, IP4_ADDR_ANY, 0);                    // try to find a LISTENing PCB
        if ( pcbId < 0 )                                                                    // can only have one match, so if not found
            {
                /* TODO all data in the incoming segment is discarded.
                 * An incoming segment containing a RST is discarded.
                 * An incoming segment not containing a RST causes a RST
                 * to be sent in response. The acknowledgment and sequence
                 * field values are selected to make the reset sequence acceptable
                 * to the TCP that sent the offending segment.
                 * If the ACK bit is off, sequence number zero is used,
                 *      <SEQ=0><ACK=SEG.SEQ+SEG.LEN><CTL=RST,ACK>
                 * If the ACK bit is on,
                 *      <SEQ=SEG.ACK><CTL=RST>
                 */
                return;                                                                     // drop the packet
            }
    }                                                                                       // otherwise pcbId is a PCB matching the incoming segment

    flags = stack_ntoh(tcp->dataOffsAndFlags) & FLAGS_MASK;                                 // extract the rest of the segment info
    dataOff = 4 * (stack_ntoh(tcp->dataOffsAndFlags) >> 12);

    tcpPCB[pcbId].SEG_SEQ = stack_ntohl(tcp->seq);
    tcpPCB[pcbId].SEG_ACK = stack_ntohl(tcp->ack);
    tcpPCB[pcbId].SEG_LEN = p->len - FRAME_HDR_LEN - ((ip->verHeaderLength & 0x0f) * 4) - dataOff;  // number of bytes occupied by the data in the segment
    //printf("tcpPCB[pcbId].SEG_LEN = %d\n", tcpPCB[pcbId].SEG_LEN);
    tcpPCB[pcbId].SEG_WND = stack_ntoh(tcp->window);
    tcpPCB[pcbId].SEG_UP  = stack_ntoh(tcp->urgentPtr);

    if ( tcpPCB[pcbId].SEG_WND != tcpPCB[pcbId].SND_WND )                                   // check for window update
    {
        tcpPCB[pcbId].SND_WND = tcpPCB[pcbId].SEG_WND;                                      // update window size
        tcpPCB[pcbId].SND_WL1 = tcpPCB[pcbId].SEG_SEQ;
        tcpPCB[pcbId].SND_WL2 = tcpPCB[pcbId].SEG_ACK;
    }

    if ( dataOff > 20 )                                                                     // get TCP options
    {
        get_tcp_opt((dataOff-20), &(tcp->payloadStart), &(tcpPCB[pcbId].RCV_opt));
    }

    if ( tcpPCB[pcbId].state == LISTEN )
    {
        if ( flags & TCP_FLAG_RST )                                                         // first check for an RST
            return;                                                                         // an incoming RST should be ignored

        if ( flags & TCP_FLAG_ACK )                                                         // second check for an ACK
        {
            /* any acknowledgment is bad if it arrives on a connection still in
             * the LISTEN state.  An acceptable reset segment should be formed
             * for any arriving ACK-bearing segment.
             *
             * The RST should be formatted as follows:
             *      <SEQ=SEG.ACK><CTL=RST>
             */
            send_rst(pcbId);
            return;
        }

        if ( flags & TCP_FLAG_SYN )                                                         // third check for a SYN
        {
            newConnPcb = tcp_new();                                                         // allocate connection PCB
            if ( newConnPcb < ERR_OK )                                                      // drop the connection request if no PCB resources
                return;
            tcpPCB[newConnPcb].state = LISTEN;                                              // duplicate the LISTENing PCB to the new active connection
            tcpPCB[newConnPcb].timeInState = tcpPCB[pcbId].timeInState;
            tcpPCB[newConnPcb].localIP = tcpPCB[pcbId].localIP;
            tcpPCB[newConnPcb].localPort = tcpPCB[pcbId].localPort;
            tcpPCB[newConnPcb].remoteIP = addrRemote;
            tcpPCB[newConnPcb].remotePort = portRemote;
            tcpPCB[newConnPcb].RCV_NXT = tcpPCB[pcbId].SEG_SEQ + 1;
            tcpPCB[newConnPcb].RCV_WND = TCP_DEF_WINDOW;
            tcpPCB[newConnPcb].IRS = tcpPCB[pcbId].SEG_SEQ;
            tcpPCB[newConnPcb].ISS = stack_time();
            tcpPCB[newConnPcb].SND_UNA = tcpPCB[newConnPcb].ISS;
            tcpPCB[newConnPcb].SND_NXT = tcpPCB[newConnPcb].ISS;
            tcpPCB[newConnPcb].SND_opt.mss = MSS;
            memcpy(&(tcpPCB[newConnPcb].RCV_opt), &(tcpPCB[pcbId].RCV_opt), sizeof(struct tcp_opt_t));
            tcpPCB[newConnPcb].tcp_accept_fn = tcpPCB[pcbId].tcp_accept_fn;
            tcpPCB[newConnPcb].tcp_notify_fn = tcpPCB[pcbId].tcp_notify_fn;
            result = send_syn_ack(newConnPcb);                                              // send <SEQ=ISS><ACK=RCV.NXT><CTL=SYN,ACK>
            if ( result == ERR_OK ||
                 result == ERR_ARP_QUEUE )
            {
                set_state(newConnPcb,SYN_RECEIVED);                                         // set new PCB state to SYN_RECEIVED
            }
            else
            {
                free_tcp_pcb(newConnPcb);                                                   // abort if fatal failure, close the connection and free resources
            }
        }
    }
    else if ( tcpPCB[pcbId].state == SYN_SENT )
    {
        if ( flags & TCP_FLAG_ACK )                                                         // first check for an ACK
        {
            /* if the ACK bit is set and if SEG.ACK =< ISS, or SEG.ACK > SND.NXT,
             * send a reset (unless the RST bit is set, if so drop the segment and return)
             * <SEQ=SEG.ACK><CTL=RST> and discard the segment.
             * TODO Errata ID: 3300
             * if SND.UNA =< SEG.ACK =< SND.NXT then the ACK is acceptable.
             */
            if ( tcpPCB[pcbId].SEG_ACK <= tcpPCB[pcbId].ISS ||                              // out of order, bad, ACK number
                 tcpPCB[pcbId].SEG_ACK > tcpPCB[pcbId].SND_NXT )
            {
                if ( flags & TCP_FLAG_RST )
                    return;
                else
                {
                    send_rst(pcbId);                                                        // send reset <SEQ=SEG.ACK><CTL=RST>
                    return;
                }
            }

            if ( tcpPCB[pcbId].SEG_ACK < tcpPCB[pcbId].SND_UNA ||                           // is ACK acceptable number range?
                    tcpPCB[pcbId].SEG_ACK > tcpPCB[pcbId].SND_NXT )
            {
                send_rst(pcbId);                                                            // TODO send reset? <SEQ=SEG.ACK><CTL=RST>
                return;
            }
        }

        /* if the RST bit is set then signal the user "error:
         * connection reset", drop the segment, enter CLOSED state,
         * delete TCB, and return.  Otherwise (no ACK) drop the segment
         * and return.
         */
        if ( flags & TCP_FLAG_RST )                                                         // second check for an RST
        {
            send_sig(pcbId,TCP_EVENT_REMOTE_RST);
            free_tcp_pcb(pcbId);                                                            // close the connection and free resources
            return;
        }

        /* third check the security and precedence
         * not implementing here
         */

        /* fourth check the SYN bit, this step should be reached only if the ACK is ok,
         * or there is no ACK, and if the segment did not contain a RST.
         *
         * if the SYN bit is on: RCV.NXT is set to SEG.SEQ+1, IRS is set to
         * SEG.SEQ.  SND.UNA should be advanced to equal SEG.ACK (if there
         * is an ACK), and any segments on the retransmission queue which
         * are thereby acknowledged should be removed.
         * if SND.UNA > ISS (our SYN has been ACKed), change the connection
         * state to ESTABLISHED, form an ACK segment
         *      <SEQ=SND.NXT><ACK=RCV.NXT><CTL=ACK>
         * and send it.
         *
         * Data or controls which were queued for transmission may be included.
         * If there are other controls or text in the segment then continue processing
         * at the sixth step below where the URG bit is checked, otherwise return.
         *
         * Otherwise enter SYN-RECEIVED, form a SYN,ACK segment
         *      <SEQ=ISS><ACK=RCV.NXT><CTL=SYN,ACK>
         * and send it. If there are other controls or text in the
         * segment, queue them for processing after the ESTABLISHED state
         * has been reached, return.
         */
        if ( flags & TCP_FLAG_SYN )
        {
            tcpPCB[pcbId].RCV_NXT = tcpPCB[pcbId].SEG_SEQ + 1;
            tcpPCB[pcbId].IRS = tcpPCB[pcbId].SEG_SEQ;
            if ( flags & TCP_FLAG_ACK)
                tcpPCB[pcbId].SND_UNA = tcpPCB[pcbId].SEG_ACK;
            if ( tcpPCB[pcbId].SND_UNA > tcpPCB[pcbId].ISS )
            {
                send_ack(pcbId);
                if ( tcpPCB[pcbId].tcp_accept_fn != NULL )                                  // guard, but should never be NULL
                    tcpPCB[pcbId].tcp_accept_fn(pcbId);                                     // call the listner's accept callback
                set_state(pcbId,ESTABLISHED);
            }
            else
            {
                result = send_syn_ack(pcbId);
                if ( result == ERR_OK ||
                     result == ERR_ARP_QUEUE )
                {
                    set_state(pcbId,SYN_RECEIVED);
                }
            }
        }
    }
    else
    {
        /* first check sequence number. there are four cases for the acceptability
         * test for an incoming segment:
         *
         *  Segment Receive  Test
         *  Length  Window
         *  ------- -------  ------------------------------------------
         *     0       0     SEG.SEQ = RCV.NXT
         *     0      >0     RCV.NXT =< SEG.SEQ < RCV.NXT+RCV.WND
         *    >0       0     not acceptable
         *    >0      >0     RCV.NXT =< SEG.SEQ < RCV.NXT+RCV.WND
         *                or RCV.NXT =< SEG.SEQ+SEG.LEN-1 < RCV.NXT+RCV.WND
         *
         * If the RCV.WND is zero, no segments will be acceptable, but
         * special allowance should be made to accept valid ACKs, URGs and RSTs.
         *
         * If an incoming segment is not acceptable, an acknowledgment
         * should be sent in reply (unless the RST bit is set, if so drop
         * the segment and return):
         *      <SEQ=SND.NXT><ACK=RCV.NXT><CTL=ACK>
         *
         * After sending the acknowledgment, drop the unacceptable segment
         */
        if ( (tcpPCB[pcbId].SEG_LEN > 0 && tcpPCB[pcbId].RCV_WND == 0) ||
             (tcpPCB[pcbId].SEG_SEQ < tcpPCB[pcbId].RCV_NXT) ||
             (tcpPCB[pcbId].SEG_SEQ >= (tcpPCB[pcbId].RCV_NXT + tcpPCB[pcbId].RCV_WND)) )
        {
            if ( !(flags & TCP_FLAG_RST) )
            {
                send_ack(pcbId);
            }
            return;
        }

        /* second check the RST bit
         * I'm simplifying the handling here by closing the connection and
         * sending a reset event to the registered application
         *
         */
        if ( flags & TCP_FLAG_RST )
        {
            send_sig(pcbId,TCP_EVENT_REMOTE_RST);
            free_tcp_pcb(pcbId);                                                            // close the connection and free resources
            return;
        }

        /* third check security and precedence
         * not implemented here
         */

        /* fourth, check the SYN bit,
         * If the SYN is in the window it is an error, send a reset, any
         * outstanding RECEIVEs and SEND should receive "reset" responses,
         * all segment queues should be flushed, the user should also
         * receive an unsolicited general "connection reset" signal, enter
         * the CLOSED state, delete the TCB, and return.
         */
        if ( flags & TCP_FLAG_SYN )
        {
            send_rst(pcbId);
            send_sig(pcbId,TCP_EVENT_REMOTE_RST);
            free_tcp_pcb(pcbId);
            return;
        }

        /* fifth check the ACK field,
         * if the ACK bit is off drop the segment and return
         * if the ACK bit is on:
         */
        if ( !(flags & TCP_FLAG_ACK) )
        {
            return;
        }

/*
        printf("SEG_ACK %lu, SND_UNA %lu, SND_NXT %lu\n",
                tcpPCB[pcbId].SEG_ACK,
                tcpPCB[pcbId].SND_UNA,
                tcpPCB[pcbId].SND_NXT);
*/
        switch ( tcpPCB[pcbId].state )
        {
            case SYN_RECEIVED:
                if ( tcpPCB[pcbId].SEG_ACK > tcpPCB[pcbId].SND_UNA &&                       // check segment validity (Errata ID: 3301)
                     tcpPCB[pcbId].SEG_ACK <= tcpPCB[pcbId].SND_NXT )
                {
                    if ( tcpPCB[pcbId].tcp_accept_fn != NULL )                              // guard, but should never be NULL
                        tcpPCB[pcbId].tcp_accept_fn(pcbId);                                 // call the listner's accept callback
                    set_state(pcbId,ESTABLISHED);                                           // enter ESTABLISHED state and continue processing
                }
                else
                {
                    send_rst(pcbId);                                                        // send reset if segment is not valid
                    return;
                }
                break;

            case ESTABLISHED:
            case FIN_WAIT1:
            case FIN_WAIT2:
            case CLOSE_WAIT:
            case CLOSING:
            case LAST_ACK:
            case TIME_WAIT:
                /* If SND.UNA < SEG.ACK =< SND.NXT then, set SND.UNA <- SEG.ACK.
                 * TODO Any segments on the retransmission queue which are thereby
                 * entirely acknowledged are removed.  Users should receive
                 * positive acknowledgments for buffers which have been SENT and
                 * fully acknowledged (i.e., SEND buffer should be returned with
                 * "ok" response).
                 *
                 * If SND.UNA < SEG.ACK =< SND.NXT, the send window should be
                 * updated.  If (SND.WL1 < SEG.SEQ or (SND.WL1 = SEG.SEQ and
                 * SND.WL2 =< SEG.ACK)), set SND.WND <- SEG.WND, set
                 * SND.WL1 <- SEG.SEQ, and set SND.WL2 <- SEG.ACK.
                 *
                 * Note that SND.WND is an offset from SND.UNA, that SND.WL1
                 * records the sequence number of the last segment used to update
                 * SND.WND, and that SND.WL2 records the acknowledgment number of
                 * the last segment used to update SND.WND.  The check here
                 * prevents using old segments to update the window.
                 */
                if ( tcpPCB[pcbId].SND_UNA < tcpPCB[pcbId].SEG_ACK &&                       // check segment validity
                     tcpPCB[pcbId].SEG_ACK <= tcpPCB[pcbId].SND_NXT )
                {
                    tcpPCB[pcbId].SND_UNA = tcpPCB[pcbId].SEG_ACK;

                    if ( tcpPCB[pcbId].SND_WL1 < tcpPCB[pcbId].SEG_SEQ ||
                         (tcpPCB[pcbId].SND_WL1 == tcpPCB[pcbId].SEG_SEQ &&
                          tcpPCB[pcbId].SND_WL2 <= tcpPCB[pcbId].SEG_ACK))
                    {
                        tcpPCB[pcbId].SND_WND = tcpPCB[pcbId].SEG_WND;
                        tcpPCB[pcbId].SND_WL1 = tcpPCB[pcbId].SEG_SEQ;
                        tcpPCB[pcbId].SND_WL2 = tcpPCB[pcbId].SEG_ACK;
                    }
                }

                /* If the ACK is a duplicate (SEG.ACK < SND.UNA), it can be ignored.
                 * If the ACK acks something not yet sent (SEG.ACK > SND.NXT)
                 * TODO then send an ACK, drop the segment, and return.
                 */
                if ( tcpPCB[pcbId].SEG_ACK < tcpPCB[pcbId].SND_UNA )
                    return;

                if ( tcpPCB[pcbId].SEG_ACK > tcpPCB[pcbId].SND_NXT )
                {
                    //send_ack(pcbId);
                    return;
                }

                switch ( tcpPCB[pcbId].state )                                              // in addition to the processing for the ESTABLISHED state
                {
                    case FIN_WAIT1:
                        set_state(pcbId,FIN_WAIT2);                                         // our FIN is now acknowledged then enter FIN-WAIT-2
                        break;

                    case FIN_WAIT2:                                                         // TODO wait here for FIN
                        break;

                    case CLOSING:                                                           // TODO if the ACK acknowledges our FIN then
                        set_state(pcbId,TIME_WAIT);                                         // enter the TIME-WAIT state
                        // TODO otherwise ignore the segment
                        break;

                    case LAST_ACK:                                                          // The only thing that can arrive in this state is an acknowledgment of our FIN
                        free_tcp_pcb(pcbId);                                                // close the connection and free resources
                        return;

                    case TIME_WAIT:                                                         // The only thing that can arrive in this state is a
                        send_ack(pcbId);                                                    // retransmission of the remote FIN.  Acknowledge it, and
                        tcpPCB[pcbId].timeInState = stack_time();                           // restart the 2 MSL timeout
                        break;

                    default:;
                }
                break;

            default:;
        }

        /* sixth, check the URG bit,
         * ESTABLISHED, FIN-WAIT-1, FIN-WAIT-2
         *   If the URG bit is set, RCV.UP <- max(RCV.UP,SEG.UP), and signal
         *   the user that the remote side has urgent data if the urgent
         *   pointer (RCV.UP) is in advance of the data consumed.  If the
         *   user has already been signaled (or is still in the "urgent
         *   mode") for this continuous sequence of urgent data, do not
         *   signal the user again.
         * CLOSE-WAIT, CLOSING, LAST-ACK, TIME-WAIT
         *   This should not occur, since a FIN has been received from the
         *   remote side.  Ignore the URG.
         */
        if ( flags & TCP_FLAG_URG )
        {
            // TODO implement
            assert(1);
        }

        /* seventh, process the segment text,
         * ESTABLISHED, FIN-WAIT-1, FIN-WAIT-2
         *   Once in the ESTABLISHED state, it is possible to deliver segment
         *   text to user RECEIVE buffers.  Text from segments can be moved
         *   into buffers until either the buffer is full or the segment is
         *   empty.  If the segment empties and carries an PUSH flag, then
         *   the user is informed, when the buffer is returned, that a PUSH
         *   has been received.
         *
         *   When the TCP takes responsibility for delivering the data to the
         *   user it must also acknowledge the receipt of the data.
         *
         *   Once the TCP takes responsibility for the data it advances
         *   RCV.NXT over the data accepted, and adjusts RCV.WND as
         *   apporopriate to the current buffer availability.  The total of
         *   RCV.NXT and RCV.WND should not be reduced.
         *
         *   Please note the window management suggestions in section 3.7.
         *
         *   Send an acknowledgment of the form:
         *      <SEQ=SND.NXT><ACK=RCV.NXT><CTL=ACK>
         *
         *   This acknowledgment should be piggybacked on a segment being
         *   transmitted if possible without incurring undue delay.
         */
        switch ( tcpPCB[pcbId].state )
        {
            case ESTABLISHED:
            case FIN_WAIT1:
            case FIN_WAIT2:
                if ( tcpPCB[pcbId].SEG_LEN > 0 &&
                     (bytes = TCP_DATA_BUF_SIZE - tcpPCB[pcbId].recvCnt) > 0 )              // determine space available for data
                {
                    dp = (uint8_t *)(((uint8_t *)tcp) +  dataOff);                          // pointer to segment's data (text)
                        
                    if ( bytes > tcpPCB[pcbId].SEG_LEN )                                    // adjust count to lower number
                        bytes = tcpPCB[pcbId].SEG_LEN;

                    for ( i = 0; i < bytes; i++ )                                           // byte copy loop
                    {
                        tcpPCB[pcbId].recv[tcpPCB[pcbId].recvWRp] = dp[i];                  // copy a byte
                        tcpPCB[pcbId].recvCnt++;
                        tcpPCB[pcbId].recvWRp++;                                            // adjust buffer read pointer
                        tcpPCB[pcbId].recvWRp &= CIRC_BUFFER_MASK;                          // quick way to make pointer circular
                    }
                    
                    tcpPCB[pcbId].RCV_NXT += bytes;                                         // adjust next ACK parameter
                    tcpPCB[pcbId].RCV_WND -= bytes;                                         // adjust windows size to space in buffer
                    send_ack(pcbId);
                }
                break;

            /* CLOSE-WAIT, CLOSING, LAST-ACK, TIME-WAIT STATE
             *   This should not occur, since a FIN has been received from the
             *   remote side.  Ignore the segment text.
             */
            default:;
        }

        /* eighth, check the FIN bit,
         * Do not process the FIN if the state is CLOSED, LISTEN or SYN-SENT
         * since the SEG.SEQ cannot be validated; drop the segment and return.
         *
         * If the FIN bit is set, signal the user "connection closing" and
         * return any pending RECEIVEs with same message, advance RCV.NXT
         * over the FIN, and send an acknowledgment for the FIN.  Note that
         * FIN implies PUSH for any segment text not yet delivered to the user.
         */
        if ( flags & TCP_FLAG_FIN )
        {
            if ( tcpPCB[pcbId].state == SYN_SENT )
            {
                return;
            }

            send_sig(pcbId,TCP_EVENT_CLOSE);
            tcpPCB[pcbId].RCV_NXT++;
            send_ack(pcbId);

            switch ( tcpPCB[pcbId].state )
            {
                case SYN_RECEIVED:
                case ESTABLISHED:
                    set_state(pcbId,CLOSE_WAIT);
                    break;

                case FIN_WAIT1:
                    /* TODO If our FIN has been ACKed (perhaps in this segment), then
                     * enter TIME-WAIT, start the time-wait timer, turn off the other
                     * timers; otherwise enter the CLOSING state.
                     */
                    break;

                case FIN_WAIT2:
                    set_state(pcbId,TIME_WAIT);                                             // enter TIME-WAIT
                    // TODO need to clear all other timers associated with this connection
                    break;

                case CLOSE_WAIT:
                case CLOSING:
                case LAST_ACK:
                    break;                                                                  // Remain in the state

                case TIME_WAIT:
                    tcpPCB[pcbId].timeInState = stack_time();                               // Restart the 2 MSL (2 x 'TCP_MSL_TIMEOUT') time-wait timeout.
                    break;

                default:;
            }
        }
    }
}

/*------------------------------------------------
 * find_pcb()
 *
 *  this function scans the PCB list and returns a PCB ID
 *  to the PCB that matches the criteria.
 *
 * param:  PCB state, and IP/port pais for local and remote
 * return: PCB ID, or ERR_PCB_ALLOC if not found
 *
 */
static pcbid_t find_pcb(pcb_state_t state, ip4_addr_t localIP, uint16_t localPort,
                                           ip4_addr_t remoteIP, uint16_t remotePort)
{
    int     i, done = 0, result = ERR_PCB_ALLOC;

    for (i = 0; !done && i < TCP_PCB_COUNT; i++)                    // scan PCB list
    {
        switch ( state )
        {
            case FREE:                                              // find FREE (empty) PCBs
                result = i;
                done = 1;
                break;

            case ANY_STATE:
                {
                    if ( tcpPCB[i].localIP == localIP &&            // match only local and remote IP/port
                         tcpPCB[i].localPort == localPort &&
                         tcpPCB[i].remoteIP == remoteIP &&
                         tcpPCB[i].remotePort == remotePort)
                    {
                        result = i;
                        done = 1;
                    }
                }
                break;

            case BOUND:
            case LISTEN:
                if ( tcpPCB[i].state == state &&                    // if searching for a bound or listening PCB
                     tcpPCB[i].localIP == localIP &&                // only local IP/port matches are needed
                     tcpPCB[i].localPort == localPort )
                {
                    result = i;
                    done = 1;
                }
                break;

            default:
                {
                    if ( tcpPCB[i].state == state &&                // for all other PCB states
                         tcpPCB[i].localIP == localIP &&            // match both local and remote IP/port
                         tcpPCB[i].localPort == localPort &&
                         tcpPCB[i].remoteIP == remoteIP &&
                         tcpPCB[i].remotePort == remotePort)
                    {
                        result = i;
                        done = 1;
                    }
                }
        }
    }

    return result;
}

/*------------------------------------------------
 * send_segment()
 *
 *  this function send a TCP segment and flags.
 *
 * param:  PCB ID to use for transmit, segment flags
 * return: ERR_OK if no errors or ip4_err_t with error code
 *
 */
static ip4_err_t send_segment(pcbid_t pcbId, uint16_t flags)
{
    ip4_err_t           result = ERR_OK;
    uint16_t            checksumTemp = 0;
    uint32_t            pseudoHdrSum;
    uint16_t            sendCount = 0;
    struct pbuf_t      *p;
    struct tcp_t       *tcp;
    struct syn_opt_t   *synOpt;
    struct opt_t       *opt;

    PRNT_FUNC;

    p = pbuf_allocate();                                                                // allocate a transmit buffer
    if ( p == NULL )                                                                    // exit here is error
        return ERR_MEM;

    tcp = (struct tcp_t*) &(p->pbuf[FRAME_HDR_LEN + IP_HDR_LEN]);                       // pointer to TCP header
    tcp->srcPort = stack_hton(tcpPCB[pcbId].localPort);                                 // populate TCP header with common elements
    tcp->destPort = stack_hton(tcpPCB[pcbId].remotePort);
    tcp->window = stack_hton(tcpPCB[pcbId].RCV_WND);
    tcp->checksum = 0;
    tcp->urgentPtr = stack_hton(tcpPCB[pcbId].SND_UP);

    tcpPCB[pcbId].SND_opt.time = stack_time();                                          // set this up here, this is a common point for all 'send's

    if ( flags & TCP_FLAG_RST )                                                         // send a reset segment
    {
        tcp->seq = stack_htonl(tcpPCB[pcbId].SEG_ACK);                                  // send the reset with the last ACK number
        tcp->ack = 0;
        tcp->dataOffsAndFlags = stack_hton((5<<12) + flags);                            // simple segment with no options

        pseudoHdrSum = pseudo_header_sum(pcbId, TCP_HDR_LEN);                           // calculate pseudo-header checksum
        checksumTemp = stack_checksumEx(tcp, TCP_HDR_LEN, pseudoHdrSum);
        tcp->checksum = ~checksumTemp;

        p->len = FRAME_HDR_LEN + IP_HDR_LEN + TCP_HDR_LEN;                              // set packet length
    }
    else if ( flags & TCP_FLAG_SYN )                                                    // when sending a SYN or SYN+ACK
    {
        tcp->seq = stack_htonl(tcpPCB[pcbId].SND_NXT);
        if ( flags & TCP_FLAG_ACK )
            tcp->ack = stack_htonl(tcpPCB[pcbId].RCV_NXT);
        else
            tcp->ack = 0;
        tcp->dataOffsAndFlags = stack_hton((SYN_OPT_LEN<<12) + flags);                  // option (MSS and time-stamp) with flags

        synOpt = (struct syn_opt_t*) &(tcp->payloadStart);                              // setup options
        synOpt->mssOpt = 2;                                                             // MSS
        synOpt->mssOptLen = 4;
        synOpt->mss = stack_hton(tcpPCB[pcbId].SND_opt.mss);
        synOpt->tsOpt = 8;                                                              // time stamp
        synOpt->tsOptLen = 10;
        synOpt->tsTime = stack_htonl(tcpPCB[pcbId].SND_opt.time);
        synOpt->tsEcho = stack_htonl(tcpPCB[pcbId].RCV_opt.time);
        synOpt->endOfOpt = 0;                                                           // padding

        pseudoHdrSum = pseudo_header_sum(pcbId, TCP_HDR_LEN + SYN_OPT_BYTES);           // calculate pseudo-header checksum
        checksumTemp = stack_checksumEx(tcp, TCP_HDR_LEN + SYN_OPT_BYTES, pseudoHdrSum);
        tcp->checksum = ~checksumTemp;

        p->len = FRAME_HDR_LEN + IP_HDR_LEN + TCP_HDR_LEN + SYN_OPT_BYTES;              // set packet length
        sendCount = 1;                                                                  // SYN signal
    }
    else
    {
        tcp->seq = stack_htonl(tcpPCB[pcbId].SND_NXT);
        if ( flags & TCP_FLAG_ACK )
            tcp->ack = stack_htonl(tcpPCB[pcbId].RCV_NXT);
        else
            tcp->ack = 0;
        tcp->dataOffsAndFlags = stack_hton((OPT_LEN<<12) + flags);                      // option (time-stamp) with flags

        opt = (struct opt_t*) &(tcp->payloadStart);                                     // setup options
        opt->tsOpt = 8;                                                                 // time stamp
        opt->tsOptLen = 10;
        opt->tsTime = stack_htonl(tcpPCB[pcbId].SND_opt.time);
        opt->tsEcho = stack_htonl(tcpPCB[pcbId].RCV_opt.time);
        opt->endOfOpt = 0;                                                              // padding
        
        /* TODO: if PCB is in ESTABLISHED state
                 then  send data in the outgoing segment
         */
        sendCount = 0;

        pseudoHdrSum = pseudo_header_sum(pcbId, TCP_HDR_LEN + OPT_BYTES);               // calculate pseudo-header checksum
        checksumTemp = stack_checksumEx(tcp, TCP_HDR_LEN + OPT_BYTES, pseudoHdrSum);
        tcp->checksum = ~checksumTemp;

        p->len = FRAME_HDR_LEN + IP_HDR_LEN + TCP_HDR_LEN + OPT_BYTES;                  // set packet length
        if ( flags & TCP_FLAG_FIN )                                                     // optional count of FIN signal
            sendCount++;
    }

    result = ip4_output(tcpPCB[pcbId].remoteIP, IP4_TCP, p);                            // transmit the TCP segment
    if ( result == ERR_OK ||                                                            // if transmit was successful
         result == ERR_ARP_QUEUE )
    {
        tcpPCB[pcbId].SND_NXT += sendCount;                                             // adjust the send next count
    }

    pbuf_free(p);                                                                       // free the transmit buffer

    return result;
}


/*------------------------------------------------
 * tcp_get_opt()
 *
 *  this function extracts TCP options from the
 *  option field byte list into the PCB's options member
 *
 * parse  option
 *   y      0 (8 bits): End of options list
 *   y      1 (8 bits): No operation (NOP, Padding) This may be used to align option fields on 32-bit boundaries for better performance.
 *   y      2,4,SS (32 bits): Maximum segment size (see maximum segment size) [SYN]
 *   y      3,3,S (24 bits): Window scale (see window scaling for details) [SYN][6]
 *   n      4,2 (16 bits): Selective Acknowledgement permitted. [SYN] (See selective acknowledgments for details)[7]
 *   n      5,N,BBBB,EEEE,... (variable bits, N is either 10, 18, 26, or 34)- Selective ACKnowledgement (SACK)[8] These first two bytes are followed by a list of 1–4 blocks being selectively acknowledged, specified as 32-bit begin/end pointers.
 *   n      8,10,TTTT,EEEE (80 bits)- Timestamp and echo of previous timestamp (see TCP timestamps for details)[9]
 *
 * param:  options byte count, pointer to options' byte list, pointer to options PCB member
 * return: none
 *
 */
static void get_tcp_opt(uint8_t bytes, uint8_t *optList, struct tcp_opt_t *options)
{
    uint8_t     byteCount;

    PRNT_FUNC;

    while ( bytes )
    {
        switch ( *(optList++) )
        {
            case 0:
                bytes = 0;                              // no more options only filler, exit
                break;

            case 1:
                bytes--;
                break;

            case 2:
                byteCount = *(optList++);               // get MSS
                options->mss = stack_ntoh(*((uint16_t*)optList));
                printf(" remote mss=%d\n", options->mss);
                optList += 2;
                bytes -= byteCount;
                break;

            case 3:
                byteCount = *(optList++);               // get window scale
                options->winScale = *optList;
                printf(" remote win scale=%d\n", options->winScale);
                optList += 1;
                bytes -= byteCount;
                break;

            case 8:
                byteCount = *(optList++);               // get time stamp info
                options->time = stack_ntohl(*((uint32_t*)optList));
                printf(" remote time stamp=%lu\n", options->time);
                optList += 8;
                bytes -= byteCount;
                break;

            default:
                byteCount = *(optList++);               // skip any other options
                optList += (byteCount-2);
                bytes -= byteCount;
        }
    }
}

/*------------------------------------------------
 * pseudo_header_sum()
 *
 *  this function calculates a TCP pseudo header sum.
 *  the output is only useful as input to stack_checksumEx()
 *
 * param:  PCB ID of connection, length of TCP segment
 * return: 32bit accumulated sum of pseudo-header bytes
 *
 */
static uint32_t pseudo_header_sum(pcbid_t pcbId, uint16_t tcpLen)
{
    struct pseudo_header_t  header;
    const uint8_t          *octetptr;
    uint32_t                acc;
    uint16_t                src;
    int                     i;

    header.srcIp = tcpPCB[pcbId].localIP;
    header.destIp = tcpPCB[pcbId].remoteIP;
    header.zero = 0;
    header.protocol = IP4_TCP;
    header.tcpLen = stack_hton(tcpLen);

    octetptr = (const uint8_t*)&header;
    acc = 0;
    for (i = 0; i < sizeof(struct pseudo_header_t); i += 2)
    {
        src = (*octetptr) << 8;
        octetptr++;
        src |= (*octetptr);
        octetptr++;
        acc += src;
    }

    return acc;
}

/*------------------------------------------------
 * tcp_timeout_handler()
 *
 *  timeout handler is invoked every 250mSec and will scan
 *  PCB list and TODO other lists to handle timeout conditions
 *
 * param:  long unsigned integer of time at which handler was invoked
 * return: none
 *
 */
static void tcp_timeout_handler(uint32_t now)
{
    pcbid_t     i;

    for (i = 0; i < TCP_PCB_COUNT; i++)                                     // scan PCB list
    {
        switch ( tcpPCB[i].state )
        {
            case TIME_WAIT:
                if ((now - tcpPCB[i].timeInState) >= (2 * TCP_MSL_TIMEOUT)) // and 2xMSL timeout has expired
                {
                    free_tcp_pcb(i);                                        // close the connection and free resources
                }
                break;

            case LAST_ACK:
            case SYN_RECEIVED:
            case SYN_SENT:
                if ((now - tcpPCB[i].timeInState) >= TCP_HSTATE_TIMEOUT )   // half open/close state timeout has expired
                {
                    free_tcp_pcb(i);                                        // close the connection and free resources
                }
                break;
        }
    }
}

/*------------------------------------------------
 * close_tcp_pcb()
 *
 *  close a clean up TCP PCB connection and resources
 *
 * param:  a valid TCP PCB ID
 * return: none
 *
 */
static void free_tcp_pcb(pcbid_t pcbId)
{
    if ( pcbId >= TCP_PCB_COUNT )
        return;

    // TODO clear all resources associated with this PCB
    tcpPCB[pcbId].localIP = 0;
    tcpPCB[pcbId].localPort = 0;
    tcpPCB[pcbId].remoteIP = 0;
    tcpPCB[pcbId].remotePort = 0;
    
    tcpPCB[pcbId].sendWRp = 0;
    tcpPCB[pcbId].sendRDp = 0;
    tcpPCB[pcbId].sendCnt = 0;
    tcpPCB[pcbId].recvWRp = 0;
    tcpPCB[pcbId].recvRDp = 0;
    tcpPCB[pcbId].recvCnt = 0;

    set_state(pcbId,FREE);                                                  // close the connection
}
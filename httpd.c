/*
 *  httpd.c
 *
 *      http server test program for V25 FlashLite board with ENC28J60 ethernet
 *      the program is written in a way that will be easy to separate it
 *      into three independent modules that will run as tasks under my LMTE
 *      executive. all code is in one file in order to keep things compact.
 *
 *      Created: October 11, 2017
 *
 */

#define     __STDC_WANT_LIB_EXT1__  1

#define     __HTTPD_DEBUG__         0

#include    <stdlib.h>
#include    <stdio.h>
#include    <string.h>
#include    <assert.h>
#include    <conio.h>
#include    <dos.h>
#include    <time.h>

#include    "ip/netif.h"

#include    "ip/stack.h"
#include    "ip/arp.h"
#include    "ip/tcp.h"

#include    "ip/error.h"
#include    "ip/types.h"

#include    "ppispi.h"

/*================================================
 * HTTPD types and definitions
 *
 */
#define     HTTPD_IP                IP4_ADDR(192,168,1,19)
#define     NETMASK                 IP4_ADDR(255,255,255,0)
#define     GATEWAY                 IP4_ADDR(192,168,1,1)
#define     HTTPD_PORT              80
#define     SESS_BUFF_SIZE          512
#define     MAX_ACTIVE_SESS         (TCP_PCB_COUNT-1)

#define     CMD_DELIM               "\r\n"                  // command line white-space delimiters
#define     FIELD_DELIM             " \t"
#define     FILE_SPEC_LEN           256
#define     SCRATCH_PAD             FILE_SPEC_LEN
#define     WWW_ROOT_DIR            "b:\\www"
#define     WWW_ROOT_PAGE           "\\index.htm"
#define     HTTP_RESP_HEADER        "HTTP/1.1 %s\r\n"                       \
                                    "Server: Flashlite NEC V25 v1.0\r\n"    \
                                    "Content-Type: text/html\r\n"           \
                                    "\r\n\0"

#define     DUMMY_TEST_PAGE         "<html> \r\n"                                                   \
                                    "<head>\r\n"                                                    \
                                    " <title>An Example Page</title>\r\n"                           \
                                    "</head>\r\n"                                                   \
                                    "<body>\r\n"                                                    \
                                    "  Hello World, this is a very simple HTML document.<br>\r\n"   \
                                    "</body>\r\n"                                                   \
                                    "</html>\r\n\0"

#define     MAX_RESOURCES           MAX_ACTIVE_SESS
#define     NO_RESOURCE             -1

typedef enum
{
    NO_SESSION,
    WAITING,
    PARSE_HEADER,
    SEND_RESP_HEADER,
    SEND_RESP,
    CLOSE
} http_handler_state_t;

typedef enum
{
    HTTP_NONE,
    HTTP_GET,
    HTTP_HEAD,
    HTTP_OPTIONS,
    HTTP_BAD_REQ
} http_cmd_t;

typedef enum
{
    HTTP_200_OK,
    HTTP_400_BAD_REQUEST,
    HTTP_404_NOT_FOUND,
    HTTP_501_NOT_IMPLEMENTED,
    HTTP_505_HTTP_VERSION_NOT_SUPPORTED
} http_resp_code_t;

struct http_req_t
{
    http_cmd_t          command;
    http_resp_code_t    status;
    char                pageRef[FILE_SPEC_LEN];
};

struct http_resource_t
{
    char                fileSpec[FILE_SPEC_LEN];
    FILE               *hFile;
    long                fileLen;
    int                 refCount;
};

struct http_session_t
{
    http_handler_state_t    state;
    pcbid_t                 connection;
    struct http_req_t       request;
    int                     resourceId;
    long                    filePos;
    int                     recvBytes;
    uint8_t                 data[SESS_BUFF_SIZE];
};

/*================================================
 * HTTPD globals
 *
 */

struct http_session_t   sessions[MAX_ACTIVE_SESS];
int                     activeSessions;
struct http_resource_t  resources[MAX_RESOURCES];
int                     openResources;
char   *httpResponse[] = { "200 OK",
                           "400 BAD REQUEST",
                           "404 NOT FOUND",
                           "501 NOT IMPLEMENTED",
                           "505 HTTP VERSION NOT SUPPORTED" };

/*================================================
 * function prototypes
 *
 */
void  notify_callback(pcbid_t, tcp_event_t);
void  accept_callback(pcbid_t);

int   resource_open(char*);
int   resource_read(int, long, uint8_t*, int);
int   resource_eof(int, long);
int   resource_close(int);

void  http_session_clear(int);
char* get_text_field(char*, char*, int, char*);
int   send_http_resp_header(int);
int   http_session_handler(int);

/*================================================
 * HTTPD main modules
 *
 */

/*------------------------------------------------
 * main()
 *
 *
 */
int main(int argc, char* argv[])
{
    int                     done = 0;

    int                     linkState, i;
    int                     result;
    int                     conn;
    pcbid_t                 tcpListner;
    struct net_interface_t *netif;
    struct net_interface_t *slipif;

    printf("Build: httpd.exe %s %s\n", __DATE__, __TIME__);

    /* initialization
     * - SPI interface
     * - IP stack
     * - default routing
     * - interface and link HW
     * - static IP addressing
     */
    spiIoInit();
    printf("Initialized IO\n");
    stack_init();
    printf("Initialized IP stack\n");
    assert(stack_set_route(NETMASK, GATEWAY, 0) == ERR_OK);
    netif = stack_get_ethif(0);
    assert(netif);
    assert(interface_init(netif) == ERR_OK);
    interface_set_addr(netif, HTTPD_IP, NETMASK, GATEWAY);
    printf("Initialized network interface\n");

    /* test link state and send gratuitous ARP
     * if link is 'up' send a Gratuitous ARP with our IP address
     */
    linkState = interface_link_state(netif);
    printf("Link state = '%s'\n", linkState ? "up" : "down");

    if ( linkState )
        result = arp_gratuitous(netif);
    else
    {
        printf("Exiting\n");
        return -1;
    }

    if ( result != ERR_OK )
    {
        printf("Failed arp_gratuitous() with %d\n", result);
    }

    /* prepare a TCP connection for an HTTP server
     * listening to connections on port 80
     * the setup of the server will allow accept notification
     * and also provide a callback for the optional connection
     * state notifications
     */
    activeSessions = 0;
    openResources = 0;

    tcp_init();
    tcpListner = tcp_new();
    assert(tcpListner >= 0);
    assert(tcp_bind(tcpListner, HTTPD_IP, HTTPD_PORT) == ERR_OK);
    assert(tcp_listen(tcpListner) == ERR_OK);
    assert(tcp_notify(tcpListner, notify_callback) == ERR_OK);
    assert(tcp_accept(tcpListner, accept_callback) == ERR_OK);
    for (i = 0; i < MAX_ACTIVE_SESS; i++)
    {
        http_session_clear(i);
    }
    for (i = 0; i < MAX_RESOURCES; i++)
    {
         memset(&(resources[i]), 0, sizeof(struct http_resource_t));;
    }
    printf("Created listening connection ID %d\n", tcpListner);

    /* main loop
     *
     */
    printf("---- entering main loop ----\n");

    while ( !done )
    {
        /* periodically poll link state and if a change occurred from the last
         * test propagate the notification
         */
        if ( interface_link_state(netif) != linkState )
        {
            linkState = interface_link_state(netif);
            printf("  Link state change. Now = '%s'\n", linkState ? "up" : "down");
        }

        /* periodically poll for received frames,
         * drop or feed them up the stack for processing
         */
        interface_input(netif);

        /* cyclic timer update and check
         * required for ARP and TCP processing
         */
        stack_timers();

        /* HTTP server
         * processing will scan active connection list and will serially
         * service connections
         */
        if ( activeSessions )
        {
            for (i = 0; i < MAX_ACTIVE_SESS; i++)
            {
                if ( sessions[i].state != NO_SESSION )
                {
                    conn = sessions[i].connection;  // save connection number so we can print it even after it is closed
                    result = http_session_handler(i);
#if __HTTPD_DEBUG__
                    printf("[s:%d/%d,c:%d] handler exit code %d, state is now %d\n", i, activeSessions, conn, result, sessions[i].state);
#endif
                }
            }
        } /* poll active connections for HTTP requests/response */
    } /* main loop */

    tcp_close(tcpListner);
    printf("  Closed listener connection\n  Exiting\n");

    return 0;
}

/*------------------------------------------------
 * notify_callback()
 *
 *  callback to notify TCP connection closure
 *
 * param:  PCB ID of new connection
 * return: none
 *
 */
void notify_callback(pcbid_t connection, tcp_event_t reason)
{
    ip4_addr_t      ip4addr;
    char            ip[17];
    int             i;


    ip4addr = tcp_remote_addr(connection);
    stack_ip4addr_ntoa(ip4addr, ip, 17);

    switch ( reason )
    {
            /* a remote close means that a FIN was received from the client
             * and it has nothing more to send. this server should send what's left
             * and then close
             */
        case TCP_EVENT_CLOSE:
            printf("  Connection %d closed by: %s\n", connection, ip);
            break;

            /* a reset from the remote client or connection closure due
             * to excessive retries should close the active http session.
             * the stack took care of removing the TCP connection
             */
        case TCP_EVENT_ABORTED:
        case TCP_EVENT_REMOTE_RST:
            for (i = 0; i < MAX_ACTIVE_SESS; i++)
            {
                if ( sessions[i].connection == connection )
                {
                    resource_close(sessions[i].resourceId);
                    http_session_clear(i);
                    activeSessions--;
                    i = MAX_ACTIVE_SESS;
                }
            }
            printf("  Connection %d reset/aborted with: %s\n", connection, ip);
            break;

            /* if data arrives/pushed from the client, then just
             * announce the arrival of data. the individual connection
             * handler will read the data and parse it
             */
        case TCP_EVENT_DATA_RECV:
        case TCP_EVENT_PUSH:
            //printf("  Received text for connection %d from: %s\n", connection, ip);
            break;

        default:
            printf("  Unknown event %d from: %s\n", reason, ip);
    }
}

/*------------------------------------------------
 * accept_callback()
 *
 *  callback to accept TCP connections
 *  Check if the connection can be accepted and add it to
 *  the active session list as a new session for processing
 *  TODO: how to recover if a connection is accepted but there is no
 *        http protocol session slot for it? assert() for now
 *
 * param:  PCB ID of new connection
 * return: none
 *
 */
void accept_callback(pcbid_t connection)
{
    ip4_addr_t      ip4addr;
    char            ip[17];
    int             i;

    assert(activeSessions < MAX_ACTIVE_SESS);

    ip4addr = tcp_remote_addr(connection);
    stack_ip4addr_ntoa(ip4addr, ip, 17);

    i = 0;
    while ( i < MAX_ACTIVE_SESS )
    {
        if ( sessions[i].state == NO_SESSION )
        {
            sessions[i].state = WAITING;
            sessions[i].connection = connection;
            activeSessions++;
            printf("[s:%d/%d,c:%d] Accepted connection %d from: %s:%u\n", i, activeSessions, connection, connection, ip, tcp_remote_port(connection));

            i = MAX_ACTIVE_SESS;
        }
        else
        {
            i++;
        }
    }
}

/*================================================
 * File system module
 *
 *  the functions in this module implement a way to allow
 *  multiple session to read from the same open resource file
 *  open, read and close functions manage file read pointer placement
 *  and open/close coordination from multiple sources for the same
 *  resource file.
 *
 */

/*------------------------------------------------
 * resource_open()
 *
 *  open a file resource for reading. the file name
 *  should be a fully qualified name including the base WWW server directory
 *  and with proper (OS dependent) directory name separators
 *
 * param:  valid file name
 * return: '-1' if file open error (not found),
 *         '0' or positive number of open resource reference
 */
int resource_open(char *fileSpec)
{
    int         i, freeSlot = 0;
    FILE       *fh;

    if ( openResources == MAX_RESOURCES )
        return -1;

    /* when receiving a request to open a file, first consult the open resource
     * table, if any are open. if a matching resource is already open then return
     * a reference to the resource to be used later by the read or close functions,
     * and increment the reference count to the number of users
     */
    if ( openResources )
    {
        for (i = 0; i < MAX_RESOURCES; i++)
        {
            if ( strcmp(fileSpec, resources[i].fileSpec) == 0 )
            {
                resources[i].refCount++;
#if __HTTPD_DEBUG__
                printf("  [%s] ref count = %d\n", fileSpec, resources[i].refCount);
#endif
                return i;
            }
            if ( resources[i].hFile == NULL )
            {
                freeSlot = i;
            }
        }
    }

    /* at this point we've determined that there is no matching
     * open resource, so we need to open the requested resource.
     * use the 'freeSlot' found above.
     * initialize the file size and the reference count to '1'
     */
    fh = fopen(fileSpec, "rb");
    if ( fh != NULL )
    {
        /* TODO using strncpy() here is probably not safe if src string
         * is longer than dst string.
         */
        strncpy(resources[freeSlot].fileSpec, fileSpec, FILE_SPEC_LEN);
        resources[freeSlot].hFile = fh;
        fseek(fh, 0L, SEEK_END);
        resources[freeSlot].fileLen = ftell(fh);
        resources[freeSlot].refCount = 1;
        openResources++;
#if __HTTPD_DEBUG__
        printf("  [%s] ref count = %d\n", fileSpec, resources[freeSlot].refCount);
#endif
        return freeSlot;
    }

    printf("  resource_open() open error [%s]\n", fileSpec);
    return -1;
}

/*------------------------------------------------
 * resource_read()
 *
 *  read a file resource. the resource to read is specified by
 *  its resource identifier and a reading start position must be
 *  tracked and provided by the caller.
 *
 * param:  valid resource ID, file read position, buffer pointer, bytes to read
 * return: '-1' if read error,
 *         '0' or positive number indicating number of bytes read
 */
int resource_read(int resource, long position, uint8_t *buffer, int readCount)
{
    int         result;
    size_t      bytes;

    if ( resource >= MAX_RESOURCES ||
         resource < 0 )
        return -1;

    /* get the resource in the resources[] table and validate that it is open,
     * reposition read pointer with fseek() and issue an fread() to
     * return the requested number of bytes.
     */
    if ( resources[resource].hFile != NULL )
    {
        result = fseek(resources[resource].hFile, position, SEEK_SET);
        if ( result == 0 )
        {
            /* determine the max number of bytes to read between what is left
             * in the resource file and the requested bytes count.
             * the size of available buffer should have been accounted
             * for before calling read_resource()!
             * TODO the way this is implemented only allows files that
             * are 64KB or less
             */
            bytes = (size_t)(resources[resource].fileLen - position);
            if ( bytes > readCount )
                bytes = readCount;
            result = fread(buffer, 1, bytes, resources[resource].hFile);
            return result;
        }
    }

    printf("  resource_read() not open\n");
    return -1;
}

/*------------------------------------------------
 * resource_eof()
 *
 *  detect resource end-of-file
 *
 * param:  valid resource ID, file position
 * return: non '0' if end-of-file,
 */
int resource_eof(int resource, long position)
{
    if ( resource >= MAX_RESOURCES ||
         resource < 0 )
        return -1;

    if ( resources[resource].hFile != NULL )
    {
        /* eof is determined using current read position vs file length
         * not using feof() function because it may be necessary to 'rewind'
         * the read position based on success/fail of a tcp_send() function
         */
        if ( (resources[resource].fileLen - position) > 0 )
            return 0;
        else
            return -1;
    }

#if __HTTPD_DEBUG__
    printf("  resource_eof() not open\n");
#endif
    return -1;
}

/*------------------------------------------------
 * resource_close()
 *
 *  close a resource.
 *
 * param:  valid resource ID
 * return: '-1' if read error,
 *         '0' or positive number indicating number of bytes read
 */
int resource_close(int resource)
{
    int         result;

    if ( resource >= MAX_RESOURCES ||
         resource < 0 )
        return -1;

    /* get the resource in the resources[] table and validate that it is open,
     * decrement the reference count and either close the resource with fclose()
     * if reference count is '0' or exit if '> 0'.
     */
    if ( resources[resource].hFile != NULL )
    {
        resources[resource].refCount--;
        if ( resources[resource].refCount == 0 )
        {
            result = fclose(resources[resource].hFile);
#if __HTTPD_DEBUG__
            printf("  fclose() [%s] status %d\n", resources[resource].fileSpec, result);
#endif
            if ( result == 0 )
            {
                resources[resource].hFile = NULL;
                resources[resource].fileLen = 0L;
                resources[resource].refCount = 0;
                memset(resources[resource].fileSpec, 0, FILE_SPEC_LEN);
                openResources--;
            }

            return result;
        }
        else
        {
            return resources[resource].refCount;
        }
    }

    printf("  resource_close() not open\n");
    return -1;
}

/*================================================
 * HTTPD page server module
 *
 */

/*------------------------------------------------
 * http_session_clear()
 *
 *  clear and initialize a session structure
 *
 * param:  session structure ID
 * return: none
 *
 */
void http_session_clear(int httpSes)
{
    memset(&(sessions[httpSes]), 0, sizeof(struct http_session_t));
    sessions[httpSes].resourceId = NO_RESOURCE;
    sessions[httpSes].state = NO_SESSION;
}

/*------------------------------------------------
 * get_text_field()
 *
 * param:  source text string to search/process,
 *         list of field delimiter characters,
 *         field number to return (0-based),
 *         pointer to text buffer holding the field text
 * return: pointer to text field, NULL is none found
 *
 */
char* get_text_field(char *line, char *delim, int fieldNum, char *textField)
{
    static char scratchPad[SCRATCH_PAD];

    char       *field;
    int         i = 0;

    strcpy(scratchPad, line);
    field = strtok(scratchPad, delim);
    while ( field != NULL && i < fieldNum )
    {
        field = strtok(NULL, delim);
        i++;
    }

    if ( field != NULL )
    {
        strcpy(textField, field);
    }
    else
    {
        textField[0] = '\0';
    }

    return field;
}

/*------------------------------------------------
 * send_http_resp_header()
 *
 *  outputs an HTTP response header
 *
 * param:  the ID of an accepted HTTP connection,
 * return: integer result of tcp_send()
 *
 */
int send_http_resp_header(int httpSes)
{
    uint8_t            *buffer;
    struct http_req_t  *req;

    buffer = sessions[httpSes].data;
    req = &(sessions[httpSes].request);

    sprintf(buffer, HTTP_RESP_HEADER, httpResponse[req->status]);

    return tcp_send(sessions[httpSes].connection, buffer, strlen(buffer), TCP_FLAG_PSH);
}

/*------------------------------------------------
 * http_session_handler()
 *
 *  this function handles the HTTP session after a connection is accepted from
 *  a client. because the server is a single threaded application it runs in a tight
 *  loop polling interface, timers and then the session (or sessions).
 *  this function is written in a way to breaks processing into individual states.
 *  the function then returnes control to the main loop between each state so that
 *  other session can get processing time or so that the interface and timers can be polled
 *
 * param:  the ID of an accepted HTTP connection
 * return: negative for failure error code, anything '0' or positive if ok
 *
 */
int http_session_handler(int httpSes)
{
    static char         tempText[SCRATCH_PAD];

    int                 result;
    char               *line;
    char               *cp;
    struct http_req_t  *req;
    size_t              strMaxLen;
    char               *tempSTRTOK;

    req = &(sessions[httpSes].request);

    switch ( sessions[httpSes].state )
    {
        /* should not get here with this state
         */
        case NO_SESSION:
            assert(0);
            break;

        /* session is connected because a connection was accepted
         * so we need to wait for the first segment (HTTP command)
         * to be received
         */
        case WAITING:
            result = tcp_recv(sessions[httpSes].connection, sessions[httpSes].data, SESS_BUFF_SIZE);
            if ( result == ERR_TCP_CLOSING )
            {
                sessions[httpSes].state = CLOSE;
            }
            else if ( result < 0 )
            {
                sessions[httpSes].recvBytes = 0;
            }
            else if ( result > 0 )
            {
                sessions[httpSes].recvBytes = result;
                sessions[httpSes].state = PARSE_HEADER;
            }
#if __HTTPD_DEBUG__
            printf("[s:%d/%d,c:%d] Receive status %d\n", httpSes, activeSessions, sessions[httpSes].connection, result);
#endif
            break;

        /* parse the received data, this is expected to be an HTTP request
         * TODO: for simplicity, we'll assume that the entire request is contained
         * in one segment and all of it is available if the tcp_recv() was
         * successful.
         */
        case PARSE_HEADER:
            /* separate HTTP request into text lines and into tokens
             * and process the HTTP request
             */
            result = 0;
            strMaxLen = sessions[httpSes].recvBytes;
#if __HTTPD_DEBUG__
            printf("[s:%d/%d,c:%d] HTTP command (%d bytes):\n", httpSes, activeSessions, sessions[httpSes].connection, strMaxLen);
#endif

            line = strtok_s(sessions[httpSes].data, &strMaxLen, CMD_DELIM, &tempSTRTOK);

            while ( line != NULL )
            {
#if __HTTPD_DEBUG__
                printf("  %s\n", line);
#endif

                /* assume that first line in HTTP request holds the
                 * HTTP command. we first parse the command and its
                 * parameters
                 */
                if ( req->command == HTTP_NONE )
                {
                    /* identify the HTTP command and then
                     * for a GET or HEAD command, retrieve the page request
                     * for the response.
                     */
                    if ( strstr(line, "GET") != 0 )
                    {
                        req->command = HTTP_GET;
                        req->status = HTTP_200_OK;
                    }
                    else if ( strstr(line, "HEAD") != 0 )
                    {
                        req->command = HTTP_HEAD;
                        req->status = HTTP_200_OK;
                    }
                    else
                    {
                        req->command = HTTP_BAD_REQ;
                        req->status = HTTP_400_BAD_REQUEST;
                    }

                    /* read the next token and if required
                     * translate it to the root web page path
                     */
                    if ( req->command != HTTP_BAD_REQ &&
                         get_text_field(line, FIELD_DELIM, 1, tempText) != NULL )
                    {
                        /* TODO consider using safe function strcat_s() and
                         * strcpy_s()
                         */
                        strcpy(req->pageRef, WWW_ROOT_DIR);
                        if ( strcmp(tempText, "/") == 0 )
                        {
                            strcat(req->pageRef, WWW_ROOT_PAGE);
                        }
                        else
                        {
                            strcat(req->pageRef, tempText);
                        }

                        /* form a proper file specifier in req->pageRef that is
                         * appropriate for DOS, with '\' directory name delimiters
                         */
                        cp = strchr(req->pageRef, '/');
                        while ( cp != NULL )
                        {
                            *cp = '\\';
                            cp = strchr(cp, '/');
                        }

                        /* try to open the resource page and save
                         * the resource identifier and initial position in the
                         * session structure. if failed to open file resource
                         * then set 'status' to HTTP_404_NOT_FOUND and
                         * 'command' to HTTP_BAD_REQ
                         */
                        if ( (sessions[httpSes].resourceId = resource_open(req->pageRef)) >= 0 )
                        {
                            sessions[httpSes].filePos = 0L;
                            sessions[httpSes].recvBytes = 0;
                        }
                        else
                        {
                            req->command = HTTP_BAD_REQ;
                            req->status = HTTP_404_NOT_FOUND;
                        }
                    }

                    /* TODO: for simplicity we will ignore
                     * the HTTP version. if version other that 1.1
                     * is present set 'status' to HTTP_505_HTTP_VERSION_NOT_SUPPORTED
                     * and 'command' to HTTP_BAD_REQ
                     */
                }

                /* if we have a valid HTTP command in the request
                 * we now parse the rest of the request parameters
                 * we should never get here with HTTP_BAD_REQ because
                 * we are breaking the parsing process below
                 */
                else
                {
                    if ( strstr(line, "Host") != 0 )
                    {
                    }
                    else if (strstr(line, "User-Agent") != 0 )
                    {
                    }
                }

                /* continue parsing only if the command is well formed
                 * and all of the header parameters are recognizable
                 */
                if ( req->command == HTTP_BAD_REQ )
                {
#if __HTTPD_DEBUG__
                    printf("  -- bad request\n");
#endif
                    line = NULL;
                }
                else
                {
                    line = strtok_s(NULL, &strMaxLen, CMD_DELIM, &tempSTRTOK);
                }
            }
            sessions[httpSes].state = SEND_RESP_HEADER;
            break;

            /* once the header has been parsed we need to send a response
             * header. from here we either go to close if the response was an
             * error report to the client or simply a response to a HEAD request,
             * otherwise, we go to SEND_RESP state
             */
            case SEND_RESP_HEADER:
#if __HTTPD_DEBUG__
                printf("[s:%d/%d,c:%d] Command: %d, resource: [%s]\n", httpSes, activeSessions, sessions[httpSes].connection, req->command, req->pageRef);
#endif
                result = send_http_resp_header(httpSes);
#if __HTTPD_DEBUG__
                printf("  Response header sent (%d)\n", result);
#endif
                if ( req->command == HTTP_HEAD ||
                     req->command == HTTP_BAD_REQ )
                {
                    sessions[httpSes].state = CLOSE;
                }
                else
                {
                    sessions[httpSes].state = SEND_RESP;
                }
                break;

            /* send body of HTTP request if a GET command was issued
             * resource validation was already done in state PARSE_HEADER
             * if the resource did not exist an error code would
             * have been sent in state SEND_RESP_HEADER
             */
            case SEND_RESP:
                /* read a set of bytes from the file resource into the send buffer
                 * adjust the read position based on the number of bytes read.
                 * adjust the read position only if the TCP send is successful.
                 * determine if end of the resource file was reached and if true
                 * set the state to CLOSE.
                 * send the buffer as a TCP segment
                 */
#if __HTTPD_DEBUG__
                printf("[s:%d/%d,c:%d] Data send\n", httpSes, activeSessions, sessions[httpSes].connection);
#endif
                result = resource_read(sessions[httpSes].resourceId, sessions[httpSes].filePos, sessions[httpSes].data, SESS_BUFF_SIZE);
#if __HTTPD_DEBUG__
                printf("  Read:%d", result);
#endif
                if ( result > 0 )
                {
                    result = tcp_send(sessions[httpSes].connection, sessions[httpSes].data, result, TCP_FLAG_PSH);
#if __HTTPD_DEBUG__
                    printf(", Sent:%d", result);
#endif
                }
                if ( result > 0 )
                {
                    sessions[httpSes].filePos += (long) result;
                }
                if ( resource_eof(sessions[httpSes].resourceId, sessions[httpSes].filePos) )
                {
                    sessions[httpSes].state = CLOSE;
                }
#if __HTTPD_DEBUG__
                printf("\n");
#endif
                break;

            /* once header response and page response have been sent
             * close the resource then the connection. finally reset the
             * session data structure for next session
             */
            case CLOSE:
#if __HTTPD_DEBUG__
                printf("[s:%d/%d,c:%d] Closing...\n", httpSes, activeSessions, sessions[httpSes].connection);
#endif
                result = tcp_close(sessions[httpSes].connection);
                if ( result == ERR_OK ||
                     result == ERR_ARP_QUEUE )
                {
                    resource_close(sessions[httpSes].resourceId);
                    http_session_clear(httpSes);
                    activeSessions--;
#if __HTTPD_DEBUG__
                    printf("  Closed\n");
#endif
                }
                else
                {
#if __HTTPD_DEBUG__
                    printf("  TCP close failed with code %d\n", result);
#endif
                }
                break;

            /* should not get here
             */
            default:
                assert(0);
    }

    return result;
}

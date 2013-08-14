#define INCL_DOSSESMGR
#define INCL_DOSPROCESS
#define INCL_ERRORS
#define INCL_BASE

#include <os2.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <ctype.h>

#define     CRON_TYPE_VDM   0
#define     CRON_TYPE_OS2   1
#define     CRON_TYPE_PM    2

#define     CRON_PRIOR_FG   0
#define     CRON_PRIOR_BG   1

#define     CRON_VISUAL_WIND 0
#define     CRON_VISUAL_FULL 1

// Error codes ---------------------------
#define NO_PROBLEM          0
#define BAD_CRON2_ENTRY     -1
#define BAD_MEMORY          -2

// TCP/IP stuff --------------------------
#ifdef TCPIP
#define     TCPIP_SOCKET_FAILURE    -1
#define     TCPIP_BIND_FAILURE      -2
#define     TCPIP_ACCEPT_FAILURE    -3
#define     TCPIP_SOCLOSE_FAILURE   -4

#define     SERV_PORT_ADDR    10000

#define     HOSTNAMELEN       50

#define     CRON2_SERVER_ALSO   1
#define     CRON2_SERVER_ONLY   2

// rename the brain-dead function names to something
// more usable

#define soclose         _soclose
#define bind            _bind
#define connect         _connect
#define accept          _accept
#define bswap           _bswap
#define lswap           _lswap
#define socket          _socket
#define send            _send
#define recv            _recv
#define listen          _listen
#define inet_addr       _inet_addr
#define gethostbyname   _gethostbyname

// thunking pragmas for 16-bit TCP/IP calls
// The latest NSD for OS/2's TCP/IP has
// done something similar for most calls.

#pragma linkage(sock_init, far16)
#pragma stack16(8192)
#endif
// ---------------------------------------

// AT stuff ------------------------------
#define PIPE_NAME  "\\pipe\\CRON2AT"
#define FS_SRV_PIPE_OPEN_MODE  NP_ACCESS_DUPLEX | NP_NOINHERIT | NP_WRITEBEHIND
#define FS_SRV_PIPE_MODE       NP_WAIT | NP_READMODE_MESSAGE | NP_TYPE_MESSAGE | 1

#define FS_AT_OPEN_MODE  OPEN_ACCESS_READWRITE | OPEN_SHARE_DENYREADWRITE | OPEN_FLAGS_WRITE_THROUGH

#define PIPE_TIMEOUT  1000      /* milliseconds till pipe times out */

#define PIPE_OPEN_FAILED        -1
#define PIPE_BROKE              -2
#define PIPE_CONNECT_FAILED     -3
#define PIPE_READ_FAILED        -4
#define PIPE_WRITE_FAILED       -5
#define PIPE_DISCONNECT_FAILED  -6

struct _PIPE_MSG
{
   char     submission[81];
};

typedef struct _PIPE_MSG  PIPE_MSG;
// ---------------------------------------

#ifndef TRUE
#define     TRUE    1
#define     FALSE   0
#endif

typedef struct  tagPROCESS
{
    struct  tagPROCESS *next;

    short   minutes[60];
    short   hours[24];
    short   days[31];
    short   months[12];
    short   dow[7];

    short   type;
    short   priority;
    short   visual;
    short   minimized;
    short   cmdfile;
    short   drive;

    char    path[100];
    char    exec[256];
    char    opts[100];
    char    comment[80];
    char    *origline;        // original line as it appeared in CRON2.DAT

    // TCP/IP stuff
#ifdef TCPIP
    char    hostname[HOSTNAMELEN];
#endif
    // Items used by the AT command

    char    date[10];
    char    time[7];
    short   remove;

    int     as_child;
    unsigned long sid;
    PID     pid;
} PROCESS;

// EXTERN  PROCESS     *procHead = NULL;
// EXTERN  PROCESS     *atHead = NULL;

extern PROCESS     *procHead; /* = NULL;  */ /* @DG 1.41 */
extern PROCESS     *atHead;   /* = NULL;  */   /* @DG 1.41 */


typedef struct tagMSGSTANDBY
{
    struct  tagMSGSTANDBY *next;
    char    message[81];
} MSGSTANDBY;

// EXTERN  MSGSTANDBY  *stdbyHead = NULL;
// EXTERN  MSGSTANDBY  *thisStdby = NULL;

// EXTERN  short       at_error;

extern MSGSTANDBY  *stdbyHead; /* = NULL;  */         /* @DG 1.41 */
extern MSGSTANDBY  *thisStdby; /* = NULL;  */         /* @DG 1.41 */

short    at_error;                       /* @DG 1.41 */


#ifdef TCPIP
EXTERN  PROCESS     *remoteProc = NULL;
EXTERN  char        remote_launch[256];
EXTERN  short       tcpip_error,serv_security,use_server;
EXTERN  unsigned    short tcpip_port;
EXTERN  short       sockfd;
#endif

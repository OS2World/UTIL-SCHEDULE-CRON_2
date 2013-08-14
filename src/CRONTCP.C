//--------------------------------------------------------------------------
//   C R O N / 2        A UN*X CRON clone for OS/2
//                      Copyright (C) 1993-1994 Bob Hood
//
//   TCP/IP module for CRON/2.  Provides a server thread for accepting
//   connections from other CRON/2 processes.
//
//   Author:  Bob Hood
//   Date  :  04.06.93
//   Notes :  This is a CRON clone for OS/2.  It uses a CRON-compatible
//            launch file with some OS/2 tokenisms for controlling processes.
//            This is the command-line version.
//
//            See the documentation for more information.
//
//--------------------------------------------------------------------------

#define     EXTERN   extern
#include    "cron2.h"
#undef      EXTERN

#include    <sys\types.h>
#include    <netinet/in.h>
#include    <arpa/inet.h>
#include    <sys/socket.h>

static  int newsockfd;

void    process_request(int);
void    servdie(void);

// functions we'll use in the main module for launching a remote request...

extern  PROCESS * parse_line(char *,short);
extern  void      launch_app(PROCESS *);
extern  PROCESS * killProc(PROCESS *);
extern  void      log(PSZ format,...);

void cron2serv(PVOID p)
{
    short                   clilen;
    struct      sockaddr_in cli_addr;

    remoteProc = NULL;

    while(TRUE)
    {
        // Wait for a connection...

        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd,&cli_addr,&clilen);

        if(newsockfd < 0)
        {
            if(use_server == CRON2_SERVER_ONLY)
            {
                // we're running as a server only.  we need to display
                // the error to the user, and die

                printf("CRON/2: accept() failure occurred in TCP/IP server!\n");
                log("CRON/2: accept() failure occurred in TCP/IP server!\n");
                DosExit(1,2);
            }
            else
            {
                tcpip_error = TCPIP_ACCEPT_FAILURE;
                _endthread();
            }
        }

        printf("CRON/2: SERVER: accepted connection from client\n");

        process_request(newsockfd);

        if(soclose(newsockfd) < 0)
        {
            soclose(sockfd);

            if(use_server == CRON2_SERVER_ONLY)
            {
                // we're running as a server only.  we need to display
                // the error to the user, and die

                printf("CRON/2: soclose() failure occurred in TCP/IP server!\n");
                log("CRON/2: soclose() failure occurred in TCP/IP server!\n");
                DosExit(1,2);
            }
            else
            {
                tcpip_error = TCPIP_SOCLOSE_FAILURE;
                _endthread();
            }
        }

        soclose(newsockfd);
        DosSleep(500L);     // sleep for half a second

        // loop back up and wait for next connection...
    }
}

void process_request(int newsockfd)
{
    ULONG   bytes,bytes_rcvd;
    short   slength,count;
    char    buffer[256];
    char    *buf;
    PROCESS *thisProc;

    // we have a connection from a remote host, so get the process to
    // spawn from them

    // if the `serv_security' value is > 0, the requested process must
    // reside in this system's CRON2.DAT file verbatim (which is now
    // in the procHead list).
    // A `serv_security' of 0 means that we should execute the request
    // with no further verification.

    // we should receive two items:  the length of the forthcoming string
    // (as a short), and the process executable string as it would appear
    // in the CRON2.DAT file.  The TIME/DATE/DOW fields should all be
    // asteriks (*).

    bytes_rcvd = 0L;
    do
    {
        buf = (char *)buffer + bytes_rcvd;
        bytes = recv(newsockfd,buf,sizeof(short) - bytes_rcvd,0);

        if(bytes < 0) return;   // a problem, return and close socket

        bytes_rcvd += bytes;
    } while(bytes_rcvd < sizeof(short));

    buffer[2] = '\0';
    slength = atoi(buffer);
    printf("CRON/2: SERVER: message length = %d\n",slength);
    log("CRON/2: SERVER: message length = %d\n",slength);

    bytes_rcvd = 0L;
    do
    {
        buf = buffer + bytes_rcvd;
        bytes = recv(newsockfd,buf,slength - bytes_rcvd,0);

        if(bytes < 0) return;   // a problem, return and close socket

        bytes_rcvd += bytes;
    } while(bytes_rcvd < slength);

    buffer[slength] = '\0';
    printf("CRON/2: SERVER: string = %s\n",buffer);
    log("CRON/2: SERVER: string = %s\n",buffer);

    // if the `remoteProc' variable is not NULL, then the main module has
    // not launched our request yet.  We need to block until the main
    // has time to clear this variable for us...

    if(remoteProc)
    {
        printf("CRON/2: Waiting for queue to clear...\n");
        log("CRON/2: Waiting for queue to clear...\n");
        count = 0;
        while(remoteProc)
        {
            DosSleep(500L);     // sleep for half a second
            ++count;
            if(count > 60)
            {
                // we've waited for more than 30 seconds, so clear the
                // variable ourselves and continue...
                remoteProc = killProc(remoteProc);
                break;
            }
        }
    }

    if((remoteProc = parse_line(buffer,-1)) == NULL)
    {
        // an error occurred parsing the line, so just return to the caller
        // and terminate the connection...

        printf("CRON/2: ERROR: malformed message from client process!\n");
        log("CRON/2: ERROR: malformed message from client process!\n");
        return;
    }

    // check the `serv_security' variable
    if(serv_security)
    {
        thisProc = procHead;
        while(thisProc)
        {
            count = 0;
            if(strcmp(thisProc->exec,remoteProc->exec) == 0) ++count;
            if(strcmp(thisProc->path,remoteProc->path) == 0) ++count;
            if(thisProc->drive == remoteProc->drive) ++count;
            if(thisProc->type == remoteProc->type) ++count;
            if(thisProc->priority == remoteProc->priority) ++count;
            if(thisProc->visual == remoteProc->visual) ++count;

            if(count == 6) break;
            thisProc = thisProc->next;
        }

        if(thisProc == NULL)
        {
            // invalid request!!

            printf("CRON/2: ERROR: remote request is invalid!  Ignoring!\n");
            log("CRON/2: ERROR: remote request is invalid!  Ignoring!\n");
            remoteProc = killProc(remoteProc);
            return;
        }
    }

    // if we are running server-only, we need to do the launching ourselves

    if(use_server == CRON2_SERVER_ONLY)
    {
        printf("CRON/2: SERVER: launching \"%s\"\n",remoteProc->exec);
        log("CRON/2: SERVER: launching \"%s\"\n",remoteProc->exec);
        launch_app(remoteProc);
        remoteProc = killProc(remoteProc);
    }
    else
        tcpip_error = 1;        // flag to main module that a launch is needed

    // return and close the connection
}

// This function is used by DosExitList when the CRON2 process is being
// shut down.

void servdie(void)
{
    printf("Closing down sockets...");
    log("Closing down sockets...");

    fflush(stdout);

    soclose(sockfd);
    soclose(newsockfd);

    printf("done!\n");
    log("done!\n");
}

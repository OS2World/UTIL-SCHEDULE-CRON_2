//#define DEBUG
//--------------------------------------------------------------------------
//   C R O N / 2        A UN*X CRON clone for OS/2
//                      Copyright (C) 1993-1994 Bob Hood
//
//   Author:  Bob Hood
//   Date  :  04.03.93
//   Notes :  This is a CRON clone for OS/2.  It uses a CRON-compatible
//            launch file with some OS/2-isms for controlling processes.
//            This is the command-line version.
//
//            See the documentation CRON2.DOC for more information.
//
//   Modifications:
//
//             The source can now be compiled with IBM VAC++ 3.0
//             Minor changes has been made to the source. (e. g. logging
//             is more detailed)
//             Detlef Graef, february, 2001
//--------------------------------------------------------------------------

#include    "cron2.h"
#include    <stdio.h>
#include    <stdarg.h>
#include    <process.h>     // for thread functions
#include    <io.h>          // for access()
#include    <sys\types.h>
#include    <sys\stat.h>
#include    <ctype.h>
#ifdef TCPIP
#include    <netinet/in.h>
#include    <arpa/inet.h>
#include    <sys/socket.h>
#include    <netdb.h>
#endif

char        *days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

void        display_queue(void);
void        process_file(void);
PROCESS *   parse_line(char *,short);
void        check_launch(void);
void        launch_app(PROCESS *);
void        crondie(void);
void        free_memory(void);
PROCESS *   killProc(PROCESS *theProc);
void        log(PSZ format,...);
void        flushlog(void);
void        clean_AT(void);

static      short       total_OS2,total_VDM,total_PM;
static      short       inAT,total_AT;
static      DATETIME    dt;
static      char        *comspec;
static      char        logname[80];
static      char        logname1[80];         /* @DG 1.41  */
static      char        cron2bakfile[80];
            char        cron2datfile[80];

            int         c2errno;
struct      stat        startstat,newstat;

// Items from the TCP/IP server module
#ifdef TCPIP
static      unsigned    short use_tcpip;
extern      void cron2serv(PVOID);
extern      void process_request(int);
extern      void servdie(void);
#endif

// Items from the AT module

extern      void cron2at(PVOID);
extern      int open_pipe(void);
extern      void atdie(void);


PROCESS  *procHead;    /*  @DG 1.41 */
PROCESS  *atHead;      /*  @DG 1.41 */

MSGSTANDBY  *stdbyHead;         /* @DG 1.41 */
MSGSTANDBY  *thisStdby;         /* @DG 1.41 */

//--------------------------------------------------------------------------
//
//--------------------------------------------------------------------------
int main(int argc,char *argv[])
{
    short       x;
    int         atthread;
    int         last_minute;
    unsigned    long Drive,driveMap,dhold;
    char        buf1[300];
#ifdef TCPIP
    int         tcpthread;
    char        strport[20];
    struct      sockaddr_in serv_addr;
#endif

    printf("ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿\n");
    printf("³            C R O N / 2             ³\n");
    printf("ÃÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ´\n");
    printf("³   A UN*X CRON clone for OS/2       ³\n");
#ifdef TCPIP
    printf("³   with client/server networking    ³\n");
    printf("³                                    ³\n");
    printf("³            version 1.4n            ³\n");
#else
    printf("³                                    ³\n");
    printf("³           version 1.41             ³\n");
#endif
    printf("³                                    ³\n");
    printf("³    Copyright (C) 1993 Bob Hood     ³\n");
    printf("³                  2001 Detlef Graef ³\n");
    printf("ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ\n\n");

    DosQCurDisk(&Drive,&driveMap);
    dhold = (unsigned long)sizeof(buf1);
    DosQCurDir(Drive,buf1,&dhold);

    c2errno       = NO_PROBLEM;
    stdbyHead     = NULL;
    thisStdby     = NULL;
    *logname      = 0;
    sprintf(cron2datfile,"%c:\\%s\\CRON2.DAT",Drive + 64,
                                              buf1);
    sprintf(cron2bakfile,"%c:\\%s\\CRON2.BAK",Drive + 64,
                                              buf1);
#ifdef TCPIP
    use_tcpip     = FALSE;
    use_server    = FALSE;
    tcpip_port    = SERV_PORT_ADDR;
    serv_security = 0;          // preset for "-n" or "-n0"
#endif

    if(argc > 1)
    {
        x = 1;
        while(x < argc)
        {
            if(argv[x][0] == '-')
            {
                switch(argv[x][1])
                {
#ifdef TCPIP
                    case 'n':   // activate TCP/IP stuff
                        if(use_server)      // the '-s' option has already
                        {                   // been specified!
                            printf("WARNING: You have already specified the '-s' option!  Ignoring\n");
                            break;
                        }

                        use_tcpip = (sock_init() == 0);
                        if(!use_tcpip)
                            printf("WARNING: TCP/IP protocol not available! Ignoring\n");
                        else
                        {
                            if(argv[x][2] == '\0') break;

                            // see if server-also has been specified

                            if(argv[x][2] == 's')
                                use_server = CRON2_SERVER_ALSO;

                            if(argv[x][3] == '\0') break;

                            // a security level has been specified

                            if(argv[x][3] == '1') ++serv_security;
                        }

                        break;

                    case 's':   // activate the server thread
                        if(use_tcpip)       // the 'n' option has already
                        {                   // been specified!
                            printf("WARNING: You have already specified the '-n' option!  Ignoring\n");
                            log("WARNING: You have already specified the '-n' option!  Ignoring\n");
                            break;
                        }

                        use_server = CRON2_SERVER_ONLY;
                        use_tcpip = (sock_init() == 0);
                        if(sock_init() != 0)
                        {
                            printf("FATAL: TCP/IP protocol not available! Cannot initiate server!\n");
                            log("FATAL: TCP/IP protocol not available! Cannot initiate server!\n");
                            DosExit(1,1);
                        }

                        if(argv[x][2] == '\0') break;

                        // a security level has been specified

                        if(argv[x][2] == '1') ++serv_security;

                        break;

                    case 'p':   // port to use for TCP/IP
                        tcpip_port = atoi(&argv[x][2]);
                        break;
#endif
                    case 'l':
                        sprintf(logname,"%c:\\%s\\CRON2.LOG",Drive + 64, buf1);

                      if(argv[x][2] != '\0')
                           {
                            strcpy(logname1,&argv[x][2]);
                            sprintf(logname,"%c:\\%s\\%s",Drive + 64, buf1,logname1);
                           }

                        DosGetDateTime(&dt);
                        log("===================================================\nCRON/2 STARTED  on %s %2d.%02d.%02d @ %02d:%02d\n===================================================\n",
                            days[dt.weekday],
                            dt.month,
                            dt.day,
                            dt.year,
                            dt.hours,
                            dt.minutes);

                        break;

                    default:
                        printf("ERROR: Invalid command line switch: %s\n",argv[x]);
#ifdef TCPIP
                        printf("       Usage: CRON2 [[-n[s[0/1]]] [-s[0/1]] [-p<port>]] [-l[logfile]]\n\n");
#else
                        printf("       Usage: CRON2 [-l[logfile]]\n\n");
#endif

                        DosExit(1,2);
                }
            }
            else
            {
                printf("ERROR: Invalid command line switch: %s\n",argv[x]);
#ifdef TCPIP
                printf("       Usage: CRON2 [[-n[s[0/1]]] [-s[0/1]] [-p<port>]] [-l[logfile]]\n\n");
#else
                printf("       Usage: CRON2 [-l[logfile]]\n\n");
#endif

                DosExit(1,2);
            }

            ++x;
        }
    }

    comspec = getenv("COMSPEC");

    total_PM  = 0;
    total_OS2 = 0;
    total_VDM = 0;
    DosExitList(1,(PFNEXITLIST)crondie);

    process_file();

    if(total_PM == 0 && total_OS2 == 0 && total_VDM == 0)
    {
#ifdef TCPIP
        if(use_server && serv_security == 1)
        {
            printf("FATAL: Nothing to do! No processes configured in CRON2.DAT!\n\n");
            log("FATAL: Nothing to do! No processes configured in CRON2.DAT!\n\n");

            DosExit(1,2);
        }
#else
        printf("FATAL: Nothing to do! No processes configured in CRON2.DAT!\n\n");
        log("FATAL: Nothing to do! No processes configured in CRON2.DAT!\n\n");

        DosExit(1,2);
#endif
    }

    stat(cron2datfile,&startstat);

    display_queue();

    DosGetDateTime(&dt);
/*    printf("current date: %s %2d.%02d.%02d current time: %02d:%02d\r",   */
    printf("%s %2d.%02d.%02d %02d:%02d\r",
                                          days[dt.weekday],
                                          dt.month,
                                          dt.day,
                                          dt.year,
                                          dt.hours,
                                          dt.minutes);
    fflush(stdout);

#ifdef TCPIP
    // initialize the TCP/IP global error bucket...
    //
    //    o if this value is greater than 0, then a process needs to be
    //      started as a result of a remote request
    //
    //    o if this value is less than 0, then an error occurred in the
    //      server thread, and it is now dead

    tcpip_error = 0;

    if(use_server)
    {
        // add a TCP/IP shut-down function to the exit list...

        DosExitList(1,(PFNEXITLIST)servdie);

        if((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0)
        {
            printf("FATAL: TCP/IP socket() call failed!\n");
            log("FATAL: TCP/IP socket() call failed!\n");
            DosExit(1,1);
        }

        bzero((char *)&serv_addr,sizeof(serv_addr));
        serv_addr.sin_family        = AF_INET;
        serv_addr.sin_addr.s_addr   = htonl(INADDR_ANY);
        serv_addr.sin_port          = htons(tcpip_port);

        if(bind(sockfd,&serv_addr,sizeof(serv_addr)) < 0)
        {
            printf("FATAL: TCP/IP bind() call failed!\n");
            log("FATAL: TCP/IP bind() call failed!\n");
            DosExit(1,1);
        }

        listen(sockfd,5);

        if(use_server == CRON2_SERVER_ONLY)
            // we are running only as a server (no launching is initiated
            // locally).  Run the cron2serv() function only...

            cron2serv(NULL);

            // we never reach here!
        else
            // start a thread for the TCP/IP server process...

            tcpthread = _beginthread(cron2serv,NULL,16384,NULL);
    }
#endif

    // start the thread to deal with the AT pipe...

    at_error = 0;
    atthread = _beginthread(cron2at,NULL,16384,NULL);
    DosExitList(1,(PFNEXITLIST)atdie);

    last_minute = 0;

    while(TRUE)                   /*   working loop   */
    {
        DosGetDateTime(&dt);

        if(last_minute != dt.minutes)
        {
            last_minute = dt.minutes;

/*            printf("current date: %s %2d.%02d.%02d current time: %02d:%02d\r",days[dt.weekday],    */
            printf("%s %2d.%02d.%02d %02d:%02d\r",days[dt.weekday],
                                                                              dt.month,
                                                                              dt.day,
                                                                              dt.year,
                                                                              dt.hours,
                                                                              dt.minutes);
            fflush(stdout);

            if(stdbyHead) flushlog();

            stat(cron2datfile,&newstat);   /* read last modified time of "cron2.dat"  */

            if(startstat.st_mtime != newstat.st_mtime)
            {
               printf("Reprocessing CRON2.DAT... on %s %2d.%02d.%02d @ %02d:%02d\n",   /*  @DG  1.41  */
               days[dt.weekday],
               dt.month,
               dt.day,
               dt.year,
               dt.hours,
               dt.minutes);

//                printf("Reprocessing CRON2.DAT...      \n");
//                log("Reprocessing CRON2.DAT...\n");
               log("---------------------------------------------------\n");
               log("Reprocessing CRON2.DAT... on %s %2d.%02d.%02d @ %02d:%02d\n",   /*  @DG  1.41  */
               days[dt.weekday],
               dt.month,
               dt.day,
               dt.year,
               dt.hours,
               dt.minutes);

                free_memory();

                total_PM  = 0;
                total_OS2 = 0;
                total_VDM = 0;

                process_file();       /*  read "cron2.dat" */

                if(total_PM == 0 && total_OS2 == 0 && total_VDM == 0)
                {
#ifdef TCPIP
                    if(use_server && serv_security == 1)
                    {
                        printf("FATAL: Nothing to do! No processes configured in CRON2.DAT!\n\n");
                        log("FATAL: Nothing to do! No processes configured in CRON2.DAT!\n\n");
                        DosExit(1,2);
                    }
#else
                    printf("FATAL: Nothing to do! No processes configured in CRON2.DAT!\n\n");
                    log("FATAL: Nothing to do! No processes configured in CRON2.DAT!\n\n");
                    DosExit(1,2);
#endif
                }

                stat(cron2datfile,&startstat);

                display_queue();
            }

            DosSleep(500L);   /* wait 0.5 seconds */
            check_launch();
        }

        DosSleep(10000L);     /* wait 10 seconds    */


#ifdef TCPIP
        if(use_server)
        {
            // I need a semaphore to manage contention of the `tcpip_error'
            // variable.  I'll put it in someday when I have more time (Ha!)

            if(tcpip_error == 0) continue;

            if(tcpip_error < 0)     // an error
            {
                use_server = FALSE;  // shut off any further checks
                continue;
            }

            // at this point, a remote has requested that a process
            // be started.  the `remoteProc' pointer contains a valid
            // process entry, so we need to launch it and reset for the
            // next request...

            launch_app(remoteProc);
            remoteProc = killProc(remoteProc);

            tcpip_error = 0;
        }
#endif

        if(at_error < 0)
        {
            // the AT pipe has failed for some reason, notify the
            // user

            printf("\nWARNING: The AT pipe thread has failed: ");
            log("\nWARNING: The AT pipe thread has failed: ");

            switch(at_error)
            {
                case PIPE_OPEN_FAILED:
                    printf("OPEN FAILED\n");
                    log("OPEN FAILED\n");
                    break;

                case PIPE_BROKE:
                    printf("PIPE BROKE\n");
                    log("PIPE BROKE\n");
                    break;

                case PIPE_CONNECT_FAILED:
                    printf("PIPE CONNECT FAILED\n");
                    log("PIPE CONNECT FAILED\n");
                    break;

                case PIPE_READ_FAILED:
                    printf("PIPE READ FAILED\n");
                    log("PIPE READ FAILED\n");
                    break;

                case PIPE_DISCONNECT_FAILED:
                    printf("PIPE DISCONNECT FAILED\n");
                    log("PIPE DISCONNECT FAILED\n");
                    break;

                case PIPE_WRITE_FAILED:
                    printf("PIPE WRITE FAILED\n");
                    log("PIPE WRITE FAILED\n");
                    break;

                default:
                    printf("UNKNOWN ERROR: code=%d\n",at_error);
                    log("UNKNOWN ERROR: code=%d\n",at_error);
                    break;
            }    /*   switch()   */

            at_error = 0;
        }   /*   end if  */

    }

   return 0;    /*  main()  */
}

// ------------------------------ End of MAIN() ----------------------------


//--------------------------------------------------------------------------
//
//--------------------------------------------------------------------------
void display_queue(void)
{
    printf("ÚÄ QUEUE ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿\n");
    log("Queue:\n");
#ifdef TCPIP
    if(!use_server)
    {
#endif
        if(total_PM == 0)
            {
             printf("³  no ");
             log(" no ");   /*  @DG 1.41 */
            }
        else
            {
            printf("³ %3d ",total_PM);
            log("%3d ",total_PM);
            }
        printf("Presentation Manager process%s ³\n",total_PM == 1 ? "  " : "es");
        log("Presentation Manager process%s \n",total_PM == 1 ? "  " : "es");        /*  @DG 1.41 */

        if(total_OS2 == 0)
           {
            printf("³  no ");
            log(" no ");   /*  @DG 1.41 */
           }
        else
           {
            printf("³ %3d ",total_OS2);
            log("%3d ",total_OS2);
           }
           printf("OS/2 process%s                 ³\n",total_OS2 == 1 ? "  " : "es");
           log("OS/2 process%s  \n",total_OS2 == 1 ? "  " : "es");

        if(total_VDM == 0)
           {
            printf("³  no ");
            log(" no ");
           }
        else
           {
            printf("³ %3d ",total_VDM);
            log("%3d ",total_VDM);
           }
           printf("VDM process%s                  ³\n",total_VDM == 1 ? "  " : "es");
           log("VDM process%s  \n",total_VDM == 1 ? "  " : "es");
           log("---------------------------------------------------\n");

#ifdef TCPIP
    }
    if(use_tcpip || use_server)
        printf("³ <CRON/2 TCP/IP Server active>      ³\n");
    printf("ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ\n\n");

    if(use_server == CRON2_SERVER_ONLY)
        printf("ÄÄ SERVER ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ\n");
    else if(use_server == CRON2_SERVER_ALSO)
        printf("ÄÄ LAUNCH/SERVER ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ\n");
    else
        printf("ÄÄ LAUNCH ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ\n");
#else
    printf("ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ\n\n");
    printf("ÄÄ LAUNCH ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ\n");
#endif
}

//--------------------------------------------------------------------------
//   END of display_queue()
//--------------------------------------------------------------------------



//--------------------------------------------------------------------------
void process_file(void)
{
    FILE    *file;
    char    buf[300];
    char    comment[80];
    char    *p;
    short   line,x,y;
    PROCESS *thisProc,*newProc;
    PROCESS *thisAT;

    procHead = NULL;
    atHead = NULL;
    inAT = FALSE;

    if((file = fopen(cron2datfile,"r")) == NULL)
    {
#ifdef TCPIP
        if(use_server == CRON2_SERVER_ONLY) return;
#endif

        printf("FATAL: Cannot access CRON2.DAT file\n");
        log("FATAL: Cannot access CRON2.DAT file\n");
        DosExit(1,1);
    }

    line = 0;
    while(fgets(buf,299,file) != NULL)
    {
        ++line;

        if(*buf == '#')
        {
            *comment = 0;

            if(*(buf + 1) == '$')
            {
                p = buf + 2;
                while(*p == ' ' || *p == '\t') ++p;
                strncpy(comment,p,79);
            }

            continue;   // comment line
        }

        *(strchr(buf,'\n')) = 0;
        if(*buf == 0)
        {
            *comment = 0;
            continue;     // blank line
        }

        if(*buf == '@')
        {
            *comment = 0;
            inAT = TRUE;
            continue;
        }

        if((newProc = parse_line(buf,line)) == NULL)
        {
            *comment = 0;
            if(c2errno == BAD_CRON2_ENTRY)
                continue;   // not a fatal error

            fclose(file);
            DosExit(1,2);
        }

        if(inAT)
        {
            if(atHead == NULL)
                atHead = newProc;
            else
                thisAT->next = newProc;

            thisAT = newProc;
            thisAT->next = NULL;

            thisAT->remove = FALSE;
            *thisAT->comment = 0;

            if(*comment)
            {
                strcpy(thisAT->comment,comment);
                *comment = 0;
            }
        }
        else
        {
            if(procHead == NULL)
                procHead = newProc;
            else
                thisProc->next = newProc;

            thisProc = newProc;
            thisProc->next = NULL;
            *thisProc->comment = 0;

            if(*comment)
            {
                strcpy(thisProc->comment,comment);
                *comment = 0;
            }
        }
    }

    fclose(file);

#ifdef DEBUG
    {
        short ndx;

        // dump the data for verification

        thisProc = procHead;
        while(thisProc)
        {
            if(thisProc->minutes[0] == -1)
                printf("*");
            else
            {
                for(ndx = 0;ndx < 60;ndx++)
                {
                    printf("%d",thisProc->minutes[ndx]);
                    if(thisProc->minutes[ndx+1] != -1)
                        printf(",");
                    else
                        break;
                }
            }

            printf(" ");

            if(thisProc->hours[0] == -1)
                printf("*");
            else
            {
                for(ndx = 0;ndx < 24;ndx++)
                {
                    printf("%d",thisProc->hours[ndx]);
                    if(thisProc->hours[ndx+1] != -1)
                        printf(",");
                    else
                        break;
                }
            }

            printf(" ");

            if(thisProc->days[0] == -1)
                printf("*");
            else
            {
                for(ndx = 0;ndx < 31;ndx++)
                {
                    printf("%d",thisProc->days[ndx]);
                    if(thisProc->days[ndx+1] != -1)
                        printf(",");
                    else
                        break;
                }
            }

            printf(" ");

            if(thisProc->months[0] == -1)
                printf("*");
            else
            {
                for(ndx = 0;ndx < 12;ndx++)
                {
                    printf("%d",thisProc->months[ndx]);
                    if(thisProc->months[ndx+1] != -1)
                        printf(",");
                    else
                        break;
                }
            }

            printf(" ");

            if(thisProc->dow[0] == -1)
                printf("*");
            else
            {
                for(ndx = 0;ndx < 7;ndx++)
                {
                    printf("%d",thisProc->dow[ndx]);
                    if(thisProc->dow[ndx+1] != -1)
                        printf(",");
                    else
                        break;
                }
            }

            printf(" ");

            switch(thisProc->type)
            {
                case CRON_TYPE_OS2:
                    printf("OS2, ");
                    break;
                case CRON_TYPE_VDM:
                    printf("VDM, ");
                    break;
                case CRON_TYPE_PM:
                    printf("PM, ");
                    break;
            }

#ifdef TCPIP
            printf("%s, (%c) (%s) {%s}%s %s\n",thisProc->priority ? "BG":"FG",
                                thisProc->drive + 'A',
                                thisProc->path,
                                thisProc->hostname,
                                thisProc->exec,
                                thisProc->opts);
#else
            printf("%s, (%c) (%s) %s %s\n",thisProc->priority ? "BG":"FG",
                                thisProc->drive + 'A',
                                thisProc->path,
                                thisProc->exec,
                                thisProc->opts);
#endif
            thisProc = thisProc->next;
        }
    }
#endif

}

//--------------------------------------------------------------------------
PROCESS *parse_line(char *line,short ln)
{
    char    exe[256];
    char    opts[100];
    char    buf[100];
    char    from[5],to[5];
    char    *token,*subtoken,*p;
    char    *hold;
    char    drive;
    short   w,x,y,z,ndx;
    PROCESS *theProc;

    if((theProc = (PROCESS *)malloc(sizeof(PROCESS))) == NULL)
    {
        log("Memory allocation failure in parse_line()\n");
        c2errno = BAD_MEMORY;
        return(NULL);
    }

    theProc->origline = (char *)malloc(strlen(line) + 1);
    strcpy(theProc->origline,line);
    theProc->as_child = FALSE;

    x = 0;
    token = strtok(line," ");

    while(token)
    {
        ++x;

        if(inAT)
        {
            // AT line format:
            //      06.24.93 13:00 OS2 BG FULL- c:\uucp_os2.cmd

            switch(x)
            {
                case 1:
                    strcpy(theProc->date,token);
                    break;

                case 2:
                    strcpy(theProc->time,token);
                    break;

                case 3:
                    if(stricmp(token,"VDM") == 0)
                        theProc->type = CRON_TYPE_VDM;
                    else if(stricmp(token,"OS2") == 0)
                        theProc->type = CRON_TYPE_OS2;
                    else if(stricmp(token,"PM") == 0)
                        theProc->type = CRON_TYPE_PM;
                    else
                    {
                        printf("ERROR: Invalid process type (%s) on line %d of CRON2.DAT (AT section)\n",token,ln);
                        log("ERROR: Invalid process type (%s) on line %d of CRON2.DAT (AT section)\n",token,ln);

                        c2errno = BAD_CRON2_ENTRY;
                        free(theProc->origline);
                        free(theProc);
                        return(NULL);
                    }

                    break;

                case 4:
                    if(stricmp(token,"FG") == 0)
                        theProc->priority = CRON_PRIOR_FG;
                    else if(stricmp(token,"BG") == 0)
                        theProc->priority = CRON_PRIOR_BG;
                    else
                    {
                        printf("ERROR: Invalid process priority (%s) on line %d of CRON2.DAT (AT section)\n",token,ln);
                        log("ERROR: Invalid process priority (%s) on line %d of CRON2.DAT (AT section)\n",token,ln);

                        c2errno = BAD_CRON2_ENTRY;
                        free(theProc->origline);
                        free(theProc);
                        return(NULL);
                    }

                    break;

                case 5:
                    theProc->minimized = FALSE;
                    if(token[4] == '-')
                    {
                        theProc->minimized = TRUE;
                        token[4] = '\0';
                    }

                    if(stricmp(token,"WIND") == 0)
                        theProc->visual = CRON_VISUAL_WIND;
                    else if(stricmp(token,"FULL") == 0)
                        theProc->visual = CRON_VISUAL_FULL;
                    else
                    {
                        printf("ERROR: Invalid process visual (%s) on line %d of CRON2.DAT (AT section)\n",token,ln);
                        log("ERROR: Invalid process visual (%s) on line %d of CRON2.DAT (AT section)\n",token,ln);

                        c2errno = BAD_CRON2_ENTRY;
                        free(theProc->origline);
                        free(theProc);
                        return(NULL);
                    }

                    break;

                case 6:
#ifdef TCPIP
                    theProc->hostname[0] = '\0';
#endif

                    // because we're using strtok(), we will need to
                    // piece together any remaining items into a single
                    // string for parsing...

                    strncpy(exe,token,255);

                    *opts = 0;
                    while((token = strtok(NULL," ")) != NULL)
                    {
                        strcat(opts,token);
                        strcat(opts," ");
                    }

                    if(*opts)
                        opts[strlen(opts)] = '\0';  // delete trailing space

                    strcpy(theProc->opts,opts);

                    theProc->drive = -1;
                    subtoken = exe;

                    if(*(subtoken + 1) == ':')
                    {
                        drive = *subtoken;
                        if(islower(drive)) drive = _toupper(drive);
                        theProc->drive = drive - 'A';
                        subtoken += 2;      // skip drive designator
                    }

                    theProc->path[0] = '\0';
                    if(*subtoken == '\\')
                    {
                        // there's a path provided.  We need to strip it
                        // from the executable's name, and store it

                        // make a quick check to see if there are any more
                        // back-slashes in the string...

                        ++subtoken;
                        y = 1;
                        theProc->path[0] = '\\';

                        while(strchr(subtoken,'\\') != NULL)
                        {
                            // copy up to the next back-slash

                            while(*subtoken != '\\')
                            {
                                theProc->path[y] = *subtoken++;
                                ++y;
                            }
                            theProc->path[y] = *subtoken;
                            ++y;
                            ++subtoken;
                        }

                        theProc->path[y] = '\0';
                        x = strlen(theProc->path);
                        if(x > 1 && theProc->path[x-1] == '\\')
                            theProc->path[x-1] = '\0';

                    }

                    // what remains on subtoken is the executable name

                    strcpy(theProc->exec,subtoken);

                    // is this an OS/2 .CMD file? (need to explicity
                    // launch CMD.EXE to run it or we will generate the
                    // "bad EXE format" error)

                    if((hold = strchr(subtoken,'.')) != NULL)
                    {
                        ++hold;
                        theProc->cmdfile = (stricmp(hold,"cmd") == 0);
                    }

                    // make sure that this application actually exists

                    if(access(exe,0))
                    {
                        theProc->drive = -1;
                        *theProc->path = 0;
                        theProc->cmdfile = TRUE;
                    }

                    break;
            }

            token = strtok(NULL," ");
            continue;
        }

        switch(x)
        {
            case 1:     // minutes
                for(ndx = 0;ndx < 60;ndx++)
                    theProc->minutes[ndx] = -1;

                if(*token == '*')       // every minute
                    break;

                if(strchr(token,',') == NULL && strchr(token,'-') == NULL)
                {
                    theProc->minutes[0] = atoi(token);
                    break;
                }
                else
                {
                    ndx = 0;

                    subtoken = token;
                    while(TRUE)
                    {
                        from[0] = '\0';
                        to[0] = '\0';

                        p = (char *)from;
                        while(isdigit(*subtoken))
                        {
                            *p = *subtoken;
                            ++subtoken;
                            ++p;
                        }
                        *p = 0;

                        if(*subtoken == '-')
                        {
                            ++subtoken;
                            p = (char *)to;
                            while(isdigit(*subtoken))
                            {
                                *p = *subtoken;
                                ++subtoken;
                                ++p;
                            }

                            *p = 0;
                        }

                        if(to[0] == '\0')
                        {
                            // no range operator...

                            theProc->minutes[ndx] = atoi(from);
                            ++ndx;
                        }
                        else
                        {
                            y = atoi(from);
                            z = atoi(to);

                            for(w = y;w <= z;w++)
                            {
                                theProc->minutes[ndx] = w;
                                ++ndx;
                            }
                        }

                        if(*subtoken != ',')
                            break;

                        ++subtoken;
                    }

                    token = subtoken;
                }

                break;

            case 2:     // hours
                for(ndx = 0;ndx < 24;ndx++)
                    theProc->hours[ndx] = -1;

                if(*token == '*')       // every hour
                    break;

                if(strchr(token,',') == NULL && strchr(token,'-') == NULL)
                {
                    theProc->hours[0] = atoi(token);
                    break;
                }
                else
                {
                    ndx = 0;

                    subtoken = token;
                    while(TRUE)
                    {
                        from[0] = '\0';
                        to[0] = '\0';

                        p = (char *)from;
                        while(isdigit(*subtoken))
                        {
                            *p = *subtoken;
                            ++subtoken;
                            ++p;
                        }
                        *p = 0;

                        if(*subtoken == '-')
                        {
                            ++subtoken;
                            p = (char *)to;
                            while(isdigit(*subtoken))
                            {
                                *p = *subtoken;
                                ++subtoken;
                                ++p;
                            }

                            *p = 0;
                        }

                        if(to[0] == '\0')
                        {
                            // no range operator...

                            theProc->hours[ndx] = atoi(from);
                            ++ndx;
                        }
                        else
                        {
                            y = atoi(from);
                            z = atoi(to);

                            for(w = y;w <= z;w++)
                            {
                                theProc->hours[ndx] = w;
                                ++ndx;
                            }
                        }

                        if(*subtoken != ',')
                            break;

                        ++subtoken;
                    }

                    token = subtoken;
                }

                break;

            case 3:     // days
                for(ndx = 0;ndx < 31;ndx++)
                    theProc->days[ndx] = -1;

                if(*token == '*')       // every day
                    break;

                if(strchr(token,',') == NULL && strchr(token,'-') == NULL)
                {
                    theProc->days[0] = atoi(token);
                    break;
                }
                else
                {
                    ndx = 0;

                    subtoken = token;
                    while(TRUE)
                    {
                        from[0] = '\0';
                        to[0] = '\0';

                        p = (char *)from;
                        while(isdigit(*subtoken))
                        {
                            *p = *subtoken;
                            ++subtoken;
                            ++p;
                        }
                        *p = 0;

                        if(*subtoken == '-')
                        {
                            ++subtoken;
                            p = (char *)to;
                            while(isdigit(*subtoken))
                            {
                                *p = *subtoken;
                                ++subtoken;
                                ++p;
                            }

                            *p = 0;
                        }

                        if(to[0] == '\0')
                        {
                            // no range operator...

                            theProc->days[ndx] = atoi(from);
                            ++ndx;
                        }
                        else
                        {
                            y = atoi(from);
                            z = atoi(to);

                            for(w = y;w <= z;w++)
                            {
                                theProc->days[ndx] = w;
                                ++ndx;
                            }
                        }

                        if(*subtoken != ',')
                            break;

                        ++subtoken;
                    }

                    token = subtoken;
                }

                break;

            case 4:     // months
                for(ndx = 0;ndx < 12;ndx++)
                    theProc->months[ndx] = -1;

                if(*token == '*')       // every month
                    break;

                if(strchr(token,',') == NULL && strchr(token,'-') == NULL)
                {
                    theProc->months[0] = atoi(token);
                    break;
                }
                else
                {
                    ndx = 0;

                    subtoken = token;
                    while(TRUE)
                    {
                        from[0] = '\0';
                        to[0] = '\0';

                        p = (char *)from;
                        while(isdigit(*subtoken))
                        {
                            *p = *subtoken;
                            ++subtoken;
                            ++p;
                        }
                        *p = 0;

                        if(*subtoken == '-')
                        {
                            ++subtoken;
                            p = (char *)to;
                            while(isdigit(*subtoken))
                            {
                                *p = *subtoken;
                                ++subtoken;
                                ++p;
                            }

                            *p = 0;
                        }

                        if(to[0] == '\0')
                        {
                            // no range operator...

                            theProc->months[ndx] = atoi(from);
                            ++ndx;
                        }
                        else
                        {
                            y = atoi(from);
                            z = atoi(to);

                            for(w = y;w <= z;w++)
                            {
                                theProc->months[ndx] = w;
                                ++ndx;
                            }
                        }

                        if(*subtoken != ',')
                            break;

                        ++subtoken;
                    }

                    token = subtoken;
                }

                break;

            case 5:     // dow's
                for(ndx = 0;ndx < 7;ndx++)
                    theProc->dow[ndx] = -1;

                if(*token == '*')       // every day-of-the-week
                    break;

                if(strchr(token,',') == NULL && strchr(token,'-') == NULL)
                {
                    theProc->dow[0] = atoi(token);
                    break;
                }
                else
                {
                    ndx = 0;

                    subtoken = token;
                    while(TRUE)
                    {
                        from[0] = '\0';
                        to[0] = '\0';

                        p = (char *)from;
                        while(isdigit(*subtoken))
                        {
                            *p = *subtoken;
                            ++subtoken;
                            ++p;
                        }
                        *p = 0;

                        if(*subtoken == '-')
                        {
                            ++subtoken;
                            p = (char *)to;
                            while(isdigit(*subtoken))
                            {
                                *p = *subtoken;
                                ++subtoken;
                                ++p;
                            }

                            *p = 0;
                        }

                        if(to[0] == '\0')
                        {
                            // no range operator...

                            theProc->dow[ndx] = atoi(from);
                            ++ndx;
                        }
                        else
                        {
                            y = atoi(from);
                            z = atoi(to);

                            for(w = y;w <= z;w++)
                            {
                                theProc->dow[ndx] = w;
                                ++ndx;
                            }
                        }

                        if(*subtoken != ',')
                            break;

                        ++subtoken;
                    }

                    token = subtoken;
                }

                break;

            case 6:     // process type (VDM, OS2, or PM)
                if(stricmp(token,"VDM") == 0)
                {
                    theProc->type = CRON_TYPE_VDM;
                    ++total_VDM;
                }
                else if(stricmp(token,"OS2") == 0)
                {
                    theProc->type = CRON_TYPE_OS2;
                    ++total_OS2;
                }
                else if(stricmp(token,"PM") == 0)
                {
                    theProc->type = CRON_TYPE_PM;
                    ++total_PM;
                }
                else
                {
                    printf("ERROR: Invalid process type (%s) on line %d of CRON2.DAT\n",token,ln);
                    log("ERROR: Invalid process type (%s) on line %d of CRON2.DAT\n",token,ln);

                    c2errno = BAD_CRON2_ENTRY;
                    free(theProc->origline);
                    free(theProc);
                    return(NULL);
                }

                break;

            case 7:     // priority (FG or BG)
                if(stricmp(token,"FG") == 0)
                    theProc->priority = CRON_PRIOR_FG;
                else if(stricmp(token,"BG") == 0)
                    theProc->priority = CRON_PRIOR_BG;
                else
                {
                    printf("ERROR: Invalid process priority (%s) on line %d of CRON2.DAT\n",token,ln);
                    log("ERROR: Invalid process priority (%s) on line %d of CRON2.DAT\n",token,ln);

                    switch(theProc->type)
                    {
                        case CRON_TYPE_PM:
                            --total_PM;
                            break;

                        case CRON_TYPE_OS2:
                            --total_OS2;
                            break;

                        case CRON_TYPE_VDM:
                            --total_VDM;
                            break;
                    }

                    c2errno = BAD_CRON2_ENTRY;
                    free(theProc->origline);
                    free(theProc);
                    return(NULL);
                }

                break;

            case 8:     // visual (WIND or FULL, optionally minimized [-])
                theProc->minimized = FALSE;
                if(token[4] == '-')
                {
                    theProc->minimized = TRUE;
                    token[4] = '\0';
                }

                if(stricmp(token,"WIND") == 0)
                    theProc->visual = CRON_VISUAL_WIND;
                else if(stricmp(token,"FULL") == 0)
                    theProc->visual = CRON_VISUAL_FULL;
                else
                {
                    printf("ERROR: Invalid process visual (%s) on line %d of CRON2.DAT\n",token,ln);
                    log("ERROR: Invalid process visual (%s) on line %d of CRON2.DAT\n",token,ln);

                    switch(theProc->type)
                    {
                        case CRON_TYPE_PM:
                            --total_PM;
                            break;

                        case CRON_TYPE_OS2:
                            --total_OS2;
                            break;

                        case CRON_TYPE_VDM:
                            --total_VDM;
                            break;
                    }

                    c2errno = BAD_CRON2_ENTRY;
                    free(theProc->origline);
                    free(theProc);
                    return(NULL);
                }

                break;

            case 9:     // line to execute
#ifdef TCPIP
                // see if there is a hostname attached to this
                // line (denoted by braces surrounding the host name)

                theProc->hostname[0] = '\0';
                if(*token == '{')
                {
                    // suck out the host name...

                    ++token;
                    subtoken = theProc->hostname;
                    x = 0;
                    while(*token != '}')
                    {
                        *subtoken++ = *token++;
                        ++x;
                        if(x == (HOSTNAMELEN - 1))
                        {
                            while(*token != '}') ++token;
                            break;
                        }
                    }
                    *subtoken = 0;
                    ++token;        // skip last brace
                }

                if(theProc->hostname[0] && !use_tcpip)
                {
                    // we have not initialized TCP/IP, so host names are
                    // superfluous

                    theProc->hostname[0] = '\0';
                    printf("WARNING: hostname found without TCP/IP initialization!  Ignoring\n");
                    log("WARNING: hostname found without TCP/IP initialization!  Ignoring\n");
                }
#endif

                // because we're using strtok(), we will need to
                // piece together any remaining items into a single
                // string for parsing...

                strncpy(exe,token,255);

                *opts = 0;
                while((token = strtok(NULL," ")) != NULL)
                {
                    strcat(opts,token);
                    strcat(opts," ");
                }

                if(*opts)
                    opts[strlen(opts)] = '\0';  // delete trailing space

                strcpy(theProc->opts,opts);

                theProc->drive = -1;
                subtoken = exe;

                if(*(subtoken + 1) == ':')
                {
                    drive = *subtoken;
                    if(islower(drive)) drive = _toupper(drive);
                    theProc->drive = drive - 'A';
                    subtoken += 2;      // skip drive designator
                }

                theProc->path[0] = '\0';
                if(*subtoken == '\\')
                {
                    // there's a path provided.  We need to strip it
                    // from the executable's name, and store it

                    // make a quick check to see if there are any more
                    // back-slashes in the string...

                    ++subtoken;
                    y = 1;
                    theProc->path[0] = '\\';

                    while(strchr(subtoken,'\\') != NULL)
                    {
                        // copy up to the next back-slash

                        while(*subtoken != '\\')
                        {
                            theProc->path[y] = *subtoken++;
                            ++y;
                        }
                        theProc->path[y] = *subtoken;
                        ++y;
                        ++subtoken;
                    }

                    theProc->path[y] = '\0';
                    x = strlen(theProc->path);
                    if(x > 1 && theProc->path[x-1] == '\\')
                        theProc->path[x-1] = '\0';

                }

                // what remains on subtoken is the executable name

                strcpy(theProc->exec,subtoken);

                // is this an OS/2 .CMD file? (need to explicity
                // launch CMD.EXE to run it or we will generate the
                // "bad EXE format" error)

                if((hold = strchr(subtoken,'.')) != NULL)
                {
                    ++hold;
                    theProc->cmdfile = (stricmp(hold,"cmd") == 0);
                }

                // make sure that this application actually exists

                if(access(exe,0))
                {
                    theProc->drive = -1;
                    *theProc->path = 0;
                    theProc->cmdfile = TRUE;
                }

                break;
        }

        token = strtok(NULL," ");
    }

    return(theProc);
}

//--------------------------------------------------------------------------
void check_launch(void)
{
    PROCESS     *thisProc,*thisAT;
    short       ndx;
    short       launched_AT;
    char        date[10],time[7];

    printf("%s %2d.%02d.%02d %02d:%02d\r",days[dt.weekday],
                                          dt.month,
                                          dt.day,
                                          dt.year,
                                          dt.hours,
                                          dt.minutes);
    fflush(stdout);

    // look for launchable processes

    thisProc = procHead;
    while(thisProc)
    {
        // walk through each minute
        if(thisProc->minutes[0] != -1)  // -1 means match any (we fall through)
        {
            for(ndx = 0;ndx < 60;ndx++)
            {
                if(thisProc->minutes[ndx] == dt.minutes ||
                   thisProc->minutes[ndx] == -1) break;
            }

            if(thisProc->minutes[ndx] == -1)     // we didn't match
            {
                thisProc = thisProc->next;
                continue;       // loop back up top
            }
        }

        // walk through each hour
        if(thisProc->hours[0] != -1)
        {
            for(ndx = 0;ndx < 24;ndx++)
            {
                if(thisProc->hours[ndx] == dt.hours ||
                   thisProc->hours[ndx] == -1) break;
            }

            if(thisProc->hours[ndx] == -1)       // we didn't match
            {
                thisProc = thisProc->next;
                continue;       // loop back up top
            }
        }

        // walk through each day
        if(thisProc->days[0] != -1)
        {
            for(ndx = 0;ndx < 31;ndx++)
            {
                if(thisProc->days[ndx] == dt.day ||
                   thisProc->days[ndx] == -1) break;
            }

            if(thisProc->days[ndx] == -1)        // we didn't match
            {
                thisProc = thisProc->next;
                continue;       // loop back up top
            }
        }

        // walk through each month
        if(thisProc->months[0] != -1)
        {
            for(ndx = 0;ndx < 31;ndx++)
            {
                if(thisProc->months[ndx] == dt.month ||
                   thisProc->months[ndx] == -1) break;
            }

            if(thisProc->months[ndx] == -1)      // we didn't match
            {
                thisProc = thisProc->next;
                continue;       // loop back up top
            }
        }

        // walk through each dow
        if(thisProc->dow[0] != -1)
        {
            for(ndx = 0;ndx < 7;ndx++)
            {
                if(thisProc->dow[ndx] == (unsigned short)dt.weekday ||
                   thisProc->dow[ndx] == -1) break;
            }

            if(thisProc->dow[ndx] == -1)        // we didn't match
            {
                thisProc = thisProc->next;
                continue;       // loop back up top
            }
        }

        // EVERYTHING MATCHED!  Launch this puppy!

//        printf("Launching %s @ %02d:%02d",    /* @DG */
//               thisProc->exec,
//               dt.hours,
//               dt.minutes);

        printf("Launching %s  on %s %2d.%02d.%02d @ %02d:%02d",   /*  @DG  1.41  */
               thisProc->exec,
               days[dt.weekday],
               dt.month,
               dt.day,
               dt.year,
               dt.hours,
               dt.minutes);

        if(*thisProc->comment)
            log("\n# %s",thisProc->comment);

        log("Launching %s  on %s %2d.%02d.%02d @ %02d:%02d",
            thisProc->exec,
            days[dt.weekday],
            dt.month,
            dt.day,
            dt.year,
            dt.hours,
            dt.minutes);

#ifdef TCPIP
        if(*thisProc->hostname)
        {
            printf(" on host \"%s\"",thisProc->hostname);
            log(" on host \"%s\"",thisProc->hostname);
        }
#endif

        printf("...");
        log("...");

        fflush(stdout);

        launch_app(thisProc);

        // ...and skip to the next entry

        thisProc = thisProc->next;

        // don't be a CPU hog - sleep for half a second between launches

        DosSleep(500L);
    }

    // look for launchable AT processes

    if(atHead)
    {
        sprintf(date,"%02d.%02d.%02d",dt.month,dt.day,dt.year - 1900);
        sprintf(time,"%02d:%02d",dt.hours,dt.minutes);
        launched_AT = FALSE;

        thisProc = atHead;
        while(thisProc)
        {
            if(strcmp(thisProc->time,time) != 0)
            {
                thisProc = thisProc->next;
                continue;       // loop back up top
            }

            if(strcmp(thisProc->date,date) != 0)
            {
                thisProc = thisProc->next;
                continue;       // loop back up top
            }

            // EVERYTHING MATCHED!  Launch this puppy!

            printf("Launching AT command %s",thisProc->exec);
            log("Launching AT command %s",thisProc->exec);

            printf("...");
            log("...");

            fflush(stdout);

            launch_app(thisProc);

            // strip this AT entry from the CRON2.DAT file

            thisProc->remove = launched_AT = TRUE;

            // ...and skip to the next entry

            thisProc = thisProc->next;

            // don't be a CPU hog - sleep for half a second between launches

            DosSleep(500L);
        }

        if(launched_AT) clean_AT();
    }

    DosGetDateTime(&dt);
    printf("%s %2d.%02d.%02d %02d:%02d\r",days[dt.weekday],
                                          dt.month,
                                          dt.day,
                                          dt.year,
                                          dt.hours,
                                          dt.minutes);
    fflush(stdout);
}

//--------------------------------------------------------------------------
void launch_app(PROCESS *theProc)
{
    unsigned    long newSID,hold;
    PID         newPID;
    unsigned    rc;
    unsigned    long oldDrive;
    unsigned    long driveMap;
    char        buf1[300];
    char        exec[256],options[256];
    STARTDATA   sdata;

#ifdef TCPIP
    if(*theProc->hostname)
    {
        // we need to send this process to a remote host...

        // first, resolve the host name...

        struct  sockaddr_in serv_addr;
        struct  hostent     *host;
        short               sockfd;

        // Fill in the structure "serv_addr" with the address of the
        // server that we want to connect with.

        bzero((char *)&serv_addr,sizeof(serv_addr));
        serv_addr.sin_family    = AF_INET;
        serv_addr.sin_port      = htons(tcpip_port);
        if((serv_addr.sin_addr.s_addr = inet_addr(theProc->hostname)) == INADDR_NONE)
        {
            // host's name is not in dotted-decimal, look it up

            if((host = gethostbyname(theProc->hostname)) == NULL)
            {
                // we can't grok this host

                printf("ERROR: unable to resolve hostname \"%s\"!\n",theProc->hostname);
                log("ERROR: unable to resolve hostname \"%s\"!\n",theProc->hostname);
                return;
            }

            serv_addr.sin_family = host->h_addrtype;
            bcopy(host->h_addr,(caddr_t)&serv_addr.sin_addr,host->h_length);
        }

        // Open a TCP socket

        if((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0)
        {
            printf("ERROR: unable to open socket to server \"%s\"!\n",theProc->hostname);
            log("ERROR: unable to open socket to server \"%s\"!\n",theProc->hostname);
            return;
        }

        // Connect to the server

        if(connect(sockfd,&serv_addr,sizeof(serv_addr)) < 0)
        {
            printf("ERROR: unable to connect to server \"%s\"!\n",theProc->hostname);
            log("ERROR: unable to connect to server \"%s\"!\n",theProc->hostname);
            return;
        }

        sprintf(buf1,"%02d",strlen(theProc->origline));
        rc = send(sockfd,buf1,2,0);
        if(rc != 2)
        {
            printf("ERROR: error in send() call to server \"%s\": %d\n",theProc->hostname,rc);
            log("ERROR: error in send() call to server \"%s\": %d\n",theProc->hostname,rc);
            return;
        }

        rc = send(sockfd,theProc->origline,strlen(theProc->origline),0);
        if(rc != strlen(theProc->origline))
        {
            printf("ERROR: error in send() call to server \"%s\": %d\n",theProc->hostname,rc);
            log("ERROR: error in send() call to server \"%s\": %d\n",theProc->hostname,rc);
            return;
        }

        soclose(sockfd);
        return;
    }
#endif

    // set the drive and path before we launch...

    DosQCurDisk(&oldDrive,&driveMap);
    hold = (unsigned long)sizeof(buf1);
    DosQCurDir(oldDrive,buf1,&hold);

    if(theProc->drive != -1)
    {
        rc = DosSelectDisk(theProc->drive + 1);
        if(rc)
        {
            printf("ERROR: DosSelectDisk failed: %d (disk: %c)\n",rc,theProc->drive + 1);
            log("ERROR: DosSelectDisk failed: %d (disk: %c)\n",rc,theProc->drive + 1);
            return;
        }
    }

    if(*theProc->path)
    {
        rc = DosSetCurrentDir(theProc->path);
        if(rc)
        {
            DosSelectDisk(oldDrive);
            printf("ERROR: DosChdir failed: %d (path: %s)\n",rc,theProc->path);
            log("ERROR: DosChdir failed: %d (path: %s)\n",rc,theProc->path);
            return;
        }
    }

    if(theProc->cmdfile)
    {
        strcpy(exec,comspec);
        strcpy(options," /c ");     // this works with both CMD and 4OS2-32
        strcat(options,theProc->exec);
        strcat(options," ");
        strcat(options,theProc->opts);
    }
    else
    {
        strcpy(exec,theProc->exec);
        strcpy(options,theProc->opts);
    }

    sdata.Length        = 50;
    if(theProc->as_child)
        sdata.Related   = SSF_RELATED_CHILD;
    else
        sdata.Related   = SSF_RELATED_INDEPENDENT;
    sdata.FgBg          = theProc->priority ? SSF_FGBG_BACK : SSF_FGBG_FORE;
    if(theProc->minimized) sdata.FgBg = SSF_FGBG_BACK;
    sdata.TraceOpt      = SSF_TRACEOPT_NONE;
    sdata.PgmTitle      = NULL;     // use program name as title
    sdata.PgmName       = exec;
    sdata.PgmInputs     = options;
    sdata.TermQ         = NULL;     // no queue for termination notification
    sdata.Environment   = NULL;     // pass a copy of the parent's environment
    sdata.InheritOpt    = SSF_INHERTOPT_PARENT;
    switch(theProc->type)
    {
        case CRON_TYPE_OS2:
            sdata.SessionType = SSF_TYPE_FULLSCREEN;
            break;
        case CRON_TYPE_VDM:
            sdata.SessionType = SSF_TYPE_VDM;
            break;
        case CRON_TYPE_PM:
            sdata.SessionType = SSF_TYPE_PM;
            break;
    }
    sdata.IconFile      = NULL;
    sdata.PgmHandle     = 0;
    sdata.PgmControl    = 0;
    switch(theProc->visual)
    {
        case CRON_VISUAL_WIND:
            if(sdata.SessionType == SSF_TYPE_FULLSCREEN) // OS/2
                sdata.SessionType = SSF_TYPE_WINDOWABLEVIO;
            else if(sdata.SessionType == SSF_TYPE_VDM)
                sdata.SessionType = SSF_TYPE_WINDOWEDVDM;

            // an SSF_TYPE_PM type is already windowed...

            break;

        case CRON_VISUAL_FULL:
            if(sdata.SessionType == SSF_TYPE_PM)
                sdata.PgmControl = SSF_CONTROL_MAXIMIZE;

            break;
    }
    if(theProc->minimized)  sdata.PgmControl = SSF_CONTROL_MINIMIZE;
    sdata.InitXPos      = 0;
    sdata.InitYPos      = 0;
    sdata.InitXSize     = 0;
    sdata.InitYSize     = 0;

    rc = DosStartSession(&sdata,&newSID,&newPID);

    // reset the drive and path...

    DosSelectDisk(oldDrive);
    DosSetCurrentDir(buf1);

    if(rc)
    {
        printf("ERROR: DosStartSession failed: %d\n",rc);
        log("ERROR: DosStartSession failed: %d\n",rc);
    }
    else
    {
        // save these for the pending termination...

        theProc->pid = newPID;
        theProc->sid = newSID;

        printf("done!\n");
        log("done!\n");
        if(*theProc->comment)
            log("\n");
    }
}

//--------------------------------------------------------------------------
// Clean-up routines for leaving CRON2

void crondie(void)
{
    printf("Cleaning up CRON/2...");
    log("Cleaning up CRON/2...");

    fflush(stdout);

    free_memory();
    clean_AT();

    printf("done!\n");
    log("done!\n");

    printf("Shutdown performed on %s %2d.%02d.%02d @ %02d:%02d\n",
            days[dt.weekday],
            dt.month,
            dt.day,
            dt.year,
            dt.hours,
            dt.minutes);

    log("Shutdown performed on %s %2d.%02d.%02d @ %02d:%02d\n",
            days[dt.weekday],
            dt.month,
            dt.day,
            dt.year,
            dt.hours,
            dt.minutes);

}

//--------------------------------------------------------------------------
void free_memory(void)
{
    PROCESS *nextProc;

    while(procHead)
    {
        nextProc = procHead->next;
        killProc(procHead);
        procHead = nextProc;
    }

    procHead = NULL;

    while(atHead)
    {
        nextProc = atHead->next;
        killProc(atHead);
        atHead = nextProc;
    }

    atHead = NULL;
}

//--------------------------------------------------------------------------
PROCESS * killProc(PROCESS *theProc)
{
    free(theProc->origline);
    free(theProc);

    return(NULL);
}

//--------------------------------------------------------------------------
void log(PSZ format, ...)
{
    va_list     pArg;
    char        buf[300];
    LONG        lCount;
    FILE        *logfile;
    MSGSTANDBY  *newStdby;

    if(!*logname) return;   // logging is not active...

    logfile = fopen(logname,"a");

    va_start(pArg, format);

    lCount = vsprintf(buf, format, pArg);

    va_end(pArg);

    if(logfile != NULL)
    {
        if(stdbyHead)
        {
            // dump the pending messages first

            while(stdbyHead)
            {
                fprintf(logfile,"%s",stdbyHead->message);
                thisStdby = stdbyHead->next;
                free(stdbyHead);
                stdbyHead = thisStdby;
            }

            stdbyHead = NULL;
        }

        fprintf(logfile,"%s",buf);
        fclose(logfile);
    }
    else
    {
        // the log file is probably open by
        // another process right now, so
        // put the entry on the standby list

        newStdby = (MSGSTANDBY *) malloc(sizeof(MSGSTANDBY));
        if(newStdby == NULL) return;

        if(stdbyHead == NULL)
            stdbyHead = newStdby;
        else
            thisStdby->next = newStdby;

        thisStdby = newStdby;
        thisStdby->next = NULL;

        strncpy(thisStdby->message,buf,80);
    }
}

//--------------------------------------------------------------------------
void flushlog(void)
{
    FILE        *logfile;

    if(!*logname) return;

    if((logfile = fopen(logname,"a")) != NULL)
    {
        // dump the pending messages

        while(stdbyHead)
        {
            fprintf(logfile,"%s",stdbyHead->message);
            thisStdby = stdbyHead->next;
            free(stdbyHead);
            stdbyHead = thisStdby;
        }

        stdbyHead = NULL;
        fclose(logfile);
    }
}

//--------------------------------------------------------------------------
void clean_AT(void)
{
    char    line[300];
    FILE    *cron2;
    FILE    *cron2bak;
    PROCESS *thisAT;

    inAT = FALSE;

    if((cron2 = fopen(cron2datfile,"r")) == NULL)
        return;

    if((cron2bak = fopen(cron2bakfile,"w")) == NULL)
    {
        fclose(cron2);
        return;
    }

    while(fgets(line,299,cron2) != NULL)
    {
        *(strchr(line,'\n')) = 0;

        if(*line == '@')
        {
            inAT = TRUE;
            fprintf(cron2bak,"%s\n",line);
            continue;
        }

        if(!inAT || *line == '#')
        {
            fprintf(cron2bak,"%s\n",line);
            continue;
        }

        thisAT = atHead;
        while(thisAT)
        {
            if(thisAT->remove && strcmp(line,thisAT->origline) == 0)
            {
                *line = 0;
                break;
            }
            thisAT = thisAT->next;
        }

        if(*line) fprintf(cron2bak,"%s\n",line);
    }

    fclose(cron2);
    fclose(cron2bak);
    unlink(cron2datfile);
    rename(cron2bakfile,cron2datfile);

    free_memory();

    total_PM  = 0;
    total_OS2 = 0;
    total_VDM = 0;

    process_file();

    if(total_PM == 0 && total_OS2 == 0 && total_VDM == 0)
    {
#ifdef TCPIP
        if(use_server && serv_security == 1)
        {
            printf("FATAL: Nothing to do! No processes configured in CRON2.DAT!\n\n");
            log("FATAL: Nothing to do! No processes configured in CRON2.DAT!\n\n");
            DosExit(1,2);
        }
#else
        printf("FATAL: Nothing to do! No processes configured in CRON2.DAT!\n\n");
        log("FATAL: Nothing to do! No processes configured in CRON2.DAT!\n\n");
        DosExit(1,2);
#endif
    }

    stat(cron2datfile,&startstat);
}




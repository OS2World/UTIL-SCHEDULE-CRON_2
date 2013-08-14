//--------------------------------------------------------------------------
//   C R O N / 2        A UN*X CRON clone for OS/2
//                      Copyright (C) 1993-1994 Bob Hood
//
//   AT module for CRON/2.  Opens (or creates) a named pipe for the AT
//   command to send entries through.  When entries arrive, this module
//   will place them in the '@' section for the main CRON/2 module to
//   locate and process.
//
//   Author:  Bob Hood
//   Date  :  06.14.93
//   Notes :  This is a CRON clone for OS/2.  It uses a CRON-compatible
//            launch file with some OS/2 tokenisms for controlling processes.
//            This is the command-line version.
//
//            See the documentation for more information.
//
//--------------------------------------------------------------------------

#define INCL_DOSPROCESS
#define INCL_DOSNMPIPES
#define INCL_DOSSEMAPHORES
#define INCL_DOSMEMMGR
#define INCL_WINMESSAGEMGR
#define INCL_DOSERRORS
#define INCL_DOSFILEMGR

#include    <os2.h>

#include    <stddef.h>
#include    <stdlib.h>
#include    <string.h>
#include    <stdio.h>
#include    <sys\types.h>

// #define     EXTERN   extern
#include    "cron2.h"
// #undef      EXTERN

#define PIPE_READ   1
#define PIPE_WRITE  2

HPIPE   hpPipe;     // handle to pipe start requests are made through

int     open_pipe(void);
int     check_pipe(int status,int type);
void    atdie(void);

// functions we'll use in the main module for launching a remote request...

extern  char      cron2datfile[];
extern  void      log(PSZ format,...);

void cron2at(PVOID p)
{
    USHORT  result;
    ULONG   bytes_read;
    char    data[81];

    short   at_error;  /*  @DG 1.41 */

    if((result = open_pipe()))
    {
        at_error = PIPE_OPEN_FAILED;
        _endthread();
    }

    while(TRUE)
    {
        *data = 0;

        result = DosConnectNPipe(hpPipe);
        if(check_pipe(result,PIPE_CONNECT_FAILED)) break;

        result = DosRead(hpPipe,data,sizeof(data),&bytes_read);
        if(check_pipe(result,PIPE_READ_FAILED)) break;

        if(strcmp(data,"-l") == 0)
        {
            // we need to send them everything we currently have

            if(atHead == NULL)
            {
                strcpy(data,"none");

                result = DosWrite(hpPipe,data,sizeof(data),&bytes_read);
                if(check_pipe(result,PIPE_WRITE_FAILED)) break;
            }
            else
            {
                PROCESS *t = atHead;
                while(t)
                {
                    *data = 0;
                    sprintf(data,"%s %s ",t->date,t->time);

                    switch(t->type)
                    {
                        case CRON_TYPE_OS2:
                            strcat(data,"OS2 ");
                            break;

                        case CRON_TYPE_VDM:
                            strcat(data,"VDM ");
                            break;

                        case CRON_TYPE_PM:
                            strcat(data,"PM ");
                            break;
                    }

                    switch(t->priority)
                    {
                        case CRON_PRIOR_FG:
                            strcat(data,"FG ");
                            break;

                        case CRON_PRIOR_BG:
                            strcat(data,"BG ");
                            break;
                    }

                    switch(t->visual)
                    {
                        case CRON_VISUAL_WIND:
                            strcat(data,"WIND");
                            break;

                        case CRON_VISUAL_FULL:
                            strcat(data,"FULL");
                            break;
                    }

                    if(t->minimized) strcat(data,"-");
                    strcat(data," ");
                    strcat(data,t->path);
                    strcat(data," ");
                    strcat(data,t->exec);
                    strcat(data," ");
                    strcat(data,t->opts);

                    result = DosWrite(hpPipe,data,sizeof(data),&bytes_read);
                    if(check_pipe(result,PIPE_WRITE_FAILED)) break;

                    t = t->next;
                }
/*
                strcpy(data,"end");

                result = DosWrite(hpPipe,data,sizeof(data),&bytes_read);
                if(check_pipe(result,PIPE_WRITE_FAILED)) break;

                result = DosRead(hpPipe,data,sizeof(data),&bytes_read);
                if(check_pipe(result,PIPE_READ_FAILED)) break;
*/
                // don't confuse the sections below

                *data = 0;
            }
        }

        result = DosDisConnectNPipe(hpPipe);
        if(result)
        {
            at_error = PIPE_DISCONNECT_FAILED;
            _endthread();
        }

        if(*data)
        {
            // we have received an entry, add it to CRON2.DAT

            DosEnterCritSec();      // the update to CRON2.DAT should be atomic

            log("Accepted entry on AT pipe:\n           \"%s\"\n",data);

            p = fopen(cron2datfile,"a");
            fprintf(p,"%s\n",data);
            fclose(p);

            DosExitCritSec();
        }

        DosSleep(5000L);     // sleep for five seconds
    }
}

int open_pipe(void)
{
    return(DosCreateNPipe(PIPE_NAME,
                          &hpPipe,
                          FS_SRV_PIPE_OPEN_MODE,
                          FS_SRV_PIPE_MODE,
                          sizeof(PIPE_MSG)*3,
                          sizeof(PIPE_MSG)*3,
                          PIPE_TIMEOUT));
}

int check_pipe(int status,int type)
{
    int     problem;

    problem = FALSE;

    switch(status)
    {
        case 0:
            break;

        case ERROR_DISCARDED:
        case ERROR_PIPE_NOT_CONNECTED:
            problem = TRUE;
            break;

        default:
            // error status has already been set
            DosDisConnectNPipe(hpPipe);
            at_error = type;
            _endthread();
    }

    return(problem);
}

// This function is used by DosExitList when the CRON2 process is being
// shut down.  We need to close the pipe.

void atdie(void)
{
    printf("Closing down AT pipe...");
    log("Closing down AT pipe...");

    fflush(stdout);

    DosClose(hpPipe);

    printf("done!\n");
    log("done!\n");
}

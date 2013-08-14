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
#include    <ctype.h>
#include    <time.h>

#define     EXTERN   extern
#include    "cron2.h"
#undef      EXTERN

#define PIPE_READ   1
#define PIPE_WRITE  2

#define INC_MINUTE  1
#define INC_DAY     2
#define INC_WEEK    3
#define INC_MONTH   4
#define INC_YEAR    5

HPIPE   hpPipe;     // handle to pipe start requests are made through

char    *months[] = {"january",
                     "february",
                     "march",
                     "april",
                     "may",
                     "june",
                     "july",
                     "august",
                     "september",
                     "october",
                     "november",
                     "december",
                     NULL };

char    *days[] =   {"sunday",
                     "monday",
                     "tuesday",
                     "wednesday",
                     "thursday",
                     "friday",
                     "saturday",
                     NULL };

int     send_mail,list_only;

void    dominoe(struct tm *lt);
int     open_pipe(void);

// functions we'll use in the main module for launching a remote request...

int main(int argc, char *argv[])
{
    int     result;
    int     increment;
    int     inctype;
    int     nextseen;
    int     plusseen;
    ULONG   bytes_written;
    char    data[81];
    int     x,y,z;
    char    opts[10][80];
    long    t;
    struct  tm *l,lt1,lt2;
    char    d[50];
    char    *hold;

    if(argc == 1)
    {
        printf("Missing arguments\n");
        printf("at [-m] [-l] time [date] [[next | +increment] time_designation] job\n");
        exit(1);
    }

    send_mail = FALSE;
    list_only = FALSE;

    x = 1;
    while(x < argc)
    {
        if(argv[x][0] != '-') break;

        y = 1;
        while(argv[x][y] != '\0')
        {
            switch(argv[x][y])
            {
                case 'm':
                    send_mail = TRUE;
                    break;

                case 'l':
                    list_only = TRUE;
                    break;

                default:
                    printf("Invalid command line option: %s\n",argv[x]);
                    printf("at [-m] [-l] time [date] [[next | +increment] time_designation] job\n");
                    exit(1);
            }

            ++y;
        }

        ++x;
    }

    if(list_only)
    {
        ULONG   bytes_read;

        if(open_pipe())
        {
            printf("Failed to open named pipe \"%s\"\n",PIPE_NAME);
            exit(1);
        }

        strcpy(data,"-l");
        result = DosWrite(hpPipe,
                          data,
                          sizeof(data),
                          &bytes_written);

        if(result)
        {
            DosClose(hpPipe);
            printf("Error writing to pipe!\n");
            exit(1);
        }

        while(TRUE)
        {
            result = DosRead(hpPipe,
                             data,
                             sizeof(data),
                             &bytes_read);

            if(result == ERROR_PIPE_NOT_CONNECTED)
                break;
            else if(result == ERROR_BROKEN_PIPE)
            {
                printf("Error: pipe has broken! (%d)\n",result);
                exit(1);
            }
            else if(result)
            {
                DosClose(hpPipe);
                printf("Error reading from pipe! (%d)\n",result);
                exit(1);
            }

            if(strcmp(data,"end") == 0)
                break;
            else if(strcmp(data,"none") == 0)
            {
                printf("No AT jobs are currently pending\n");
                break;
            }
            else
                printf("%s\n",data);
        }

        strcpy(data,"thank you");
        result = DosWrite(hpPipe,
                          data,
                          sizeof(data),
                          &bytes_written);

        DosClose(hpPipe);

        exit(0);
    }

    if(x == argc)   // no time/date/increment
    {
        printf("Missing time/date/increment values\n");
        printf("at [-m] [-l] time [date] [[next | +increment] time_designation] job\n");
        exit(1);
    }

    /* get the remaining arguments */

    y = 0;
    while(x < argc)
    {
        strcpy(opts[y],argv[x]);
        strlwr(opts[y]);
        ++y;
        ++x;
        *opts[y] = 0;
    }

    /* deal with the time token */

    time(&t);
    l = localtime(&t);

    memcpy((void *)&lt1,(void *)l,sizeof(struct tm));
    memcpy((void *)&lt2,(void *)l,sizeof(struct tm));

    x = 0;

    if(strcmp(opts[x],"now") == 0)
    {
        /* already taken care of above ^ */
    }
    else if(strcmp(opts[x],"noon") == 0)
    {
        lt1.tm_sec = 0;
        lt1.tm_min = 0;
        lt1.tm_hour = 12;
    }
    else if(strcmp(opts[x],"midnight") == 0)
    {
        lt1.tm_sec = 0;
        lt1.tm_min = 0;
        lt1.tm_hour = 0;
    }
    else if(isdigit(opts[x][0]))
    {
        hold = opts[x];
        y = 0;
        while(isdigit(*hold))
        {
            d[y] = *hold++;
            ++y;
        }
        d[y] = '\0';
        lt1.tm_hour = atoi(d);

        lt1.tm_min = 0;
        if(*hold == ':')
        {
            ++hold;
            y = 0;
            while(isdigit(*hold))
            {
                d[y] = *hold++;
                ++y;
            }
            d[y] = '\0';
            lt1.tm_min = atoi(d);
        }

        /* hold is either pointing at '\0' or */
        /* an "am" or "pm"                    */

        if(isalpha(*hold))
        {
            if((*hold == 'p' || *hold == 'P') &&
                lt1.tm_hour != 12)
                lt1.tm_hour += 12;
        }
    }
    else
    {
        printf("Bad time value: %s\n",opts[0]);
        exit(1);
    }

    ++x;

    // check for a date

    increment = 0;
    inctype = 0;
    lt1.tm_wday = -1;
    nextseen = FALSE;
    plusseen = FALSE;

    while(*opts[x])
    {
        if(strcmp(opts[x],"next") ==0)      // equivalent to  "+ 1"
        {
            increment = 1;
            nextseen = TRUE;
        }
        else if(strcmp(opts[x],"+") == 0)
        {
            ++x;
            increment = atoi(opts[x]);
            plusseen = TRUE;
        }
        else if(strncmp(opts[x],"minute",6) == 0)
            inctype = INC_MINUTE;
        else if(strncmp(opts[x],"day",3) == 0)
            inctype = INC_DAY;
        else if(strncmp(opts[x],"week",4) == 0)
            inctype = INC_WEEK;
        else if(strncmp(opts[x],"month",5) == 0)
            inctype = INC_MONTH;
        else if(strncmp(opts[x],"year",4) == 0)
            inctype = INC_YEAR;
        else if(strcmp(opts[x],"tomorrow") == 0)
        {
            increment = 1;
            inctype = INC_DAY;
        }
        else
        {
            lt1.tm_mon  = -1;
            lt1.tm_mday = -1;

            if(isdigit(opts[x][0]))
            {
                char    month[5],day[5],year[5];

                // either 99.99.99 or 99/99/99

                y = 0;
                z = 0;
                while(isdigit(opts[x][y]))
                {
                    month[z] = opts[x][y];
                    ++z;
                    ++y;
                }
                month[z] = '\0';

                ++y;

                z = 0;
                while(isdigit(opts[x][y]))
                {
                    day[z] = opts[x][y];
                    ++z;
                    ++y;
                }
                day[z] = '\0';

                ++y;

                z = 0;
                while(isdigit(opts[x][y]))
                {
                    year[z] = opts[x][y];
                    ++z;
                    ++y;
                }
                year[z] = '\0';

                lt1.tm_mon = atoi(month) + 1;
                lt1.tm_mday = atoi(day);
                lt1.tm_year = atoi(year);
            }
            else
            {
                // see if it is a month
                y = 0;
                while(months[y] != NULL)
                {
                    if(strncmp(months[y],opts[x],strlen(opts[x])) == 0)
                        break;
                    ++y;
                }

                // see if it is a day
                if(months[y] == NULL)
                {
                    // not a month...
                    y = 0;
                    while(days[y] != NULL)
                    {
                        if(strncmp(days[y],opts[x],strlen(opts[x])) == 0)
                            break;
                        ++y;
                    }

                    if(days[y] != NULL)     // it's a day
                        lt1.tm_wday = y;
                }
                else        // it's a month
                {
                    lt1.tm_mon = y;

                    ++x;
                    lt1.tm_mday = atoi(opts[x]);

                    if(isdigit(opts[x + 1][0]))
                    {
                        ++x;

                        // a year is specified
                        lt1.tm_year = atoi(opts[x]);
                        if(strlen(opts[x]) == 4)
                            lt1.tm_year -= 1900;
                    }
                    else
                    {
                        if(y < lt2.tm_mon ||
                          (y == lt2.tm_mon && lt1.tm_mday <= lt2.tm_mday))
                            ++lt1.tm_year;  // next year
                    }
                }
            }
        }

        ++x;

        // check for the start of the CRON/2 entry

        if(stricmp(opts[x],"os2") == 0 ||
           stricmp(opts[x],"vdm") == 0 ||
           stricmp(opts[x],"pm") == 0)
            break;
    }

    if(lt1.tm_mon == -1) lt1.tm_mon = lt2.tm_mon;
    if(lt1.tm_mday == -1) lt1.tm_mday = lt2.tm_mday;

    // if there is a weekday specified, we need to set the date to
    // that day of the week before any increments are applied...

    if(lt1.tm_wday != -1)
    {
        int z;                      // lt2.tm_wday is current weekday

        if(lt1.tm_wday > lt2.tm_wday)
            z = lt1.tm_wday - lt2.tm_wday;
        else
        {
            if(nextseen &&
               inctype == INC_WEEK &&
               lt1.tm_wday == lt2.tm_wday)
                z = 0;
            else
            {
                // reduce to remainder of days left in week

                z = 6 - lt2.tm_wday;
                ++z;
                z += lt1.tm_wday;
            }
        }

        while(z)
        {
            ++lt1.tm_mday;
            dominoe(&lt1);
            --z;
        }
    }

    if(increment > 0)
    {
        // we have to "walk" the increments, observing roll-overs
        // (59 minutes to 60, December to January, etc.)

        switch(inctype)
        {
            case INC_MINUTE:
                while(increment)
                {
                    ++lt1.tm_min;
                    if(lt1.tm_min == 60)
                        dominoe(&lt1);
                    --increment;
                }

                break;

            case INC_WEEK:
                increment *= 7;
                // fall through to  INC_DAY

            case INC_DAY:
                while(increment)
                {
                    ++lt1.tm_mday;
                    dominoe(&lt1);
                    --increment;
                }

                break;

            case INC_MONTH:
                while(increment)
                {
                    ++lt1.tm_mon;
                    dominoe(&lt1);
                    --increment;
                }

                break;

            case INC_YEAR:
                lt1.tm_year += increment;
                dominoe(&lt1);
                break;

            default:
                printf("Unknown date modifier\n");
                exit(1);
        }
    }

    // months are zero-based (0=Jan), so we need to add one
    // for the correct value...

    ++lt1.tm_mon;

    // OK!  At this point, all remaining options are CRON/2-specific
    // (i.e., type, priority, visibility, command [options])

    sprintf(data,"%02d.%02d.%02.02d %02d:%02d ",
                 lt1.tm_mon,
                 lt1.tm_mday,
                 lt1.tm_year,
                 lt1.tm_hour,
                 lt1.tm_min);

    if(*opts[x] != 0)
    {
        strcat(data,opts[x]);
        strcat(data," ");
    }
    else
    {
        printf("missing type\n");
        exit(1);
    }

    ++x;

    if(*opts[x] != 0)
    {
        strcat(data,opts[x]);
        strcat(data," ");
    }
    else
    {
        printf("missing priority\n");
        exit(1);
    }

    ++x;

    if(*opts[x] != 0)
    {
        strcat(data,opts[x]);
        strcat(data," ");
    }
    else
    {
        printf("missing visibility\n");
        exit(1);
    }

    ++x;

    if(*opts[x] != 0)
    {
        strcat(data,opts[x]);
        strcat(data," ");
    }
    else
    {
        printf("missing command\n");
        exit(1);
    }

    ++x;

    while(*opts[x])
    {
        strcat(data,opts[x]);
        strcat(data," ");
        ++x;
    }

    printf("submitting \"%s\"...",data);
    fflush(stdout);

    if(open_pipe())
    {
        printf("Failed to open named pipe \"%s\"\n",PIPE_NAME);
        exit(1);
    }

    // 06.24.93 13:00 OS2 BG FULL- c:\uucp_os2.cmd

    result = DosWrite(hpPipe,
                      data,
                      strlen(data) + 1,
                      &bytes_written);
    DosClose(hpPipe);

    if(result)
        printf("\nError in DosWrite() to \"%s\" pipe\n",PIPE_NAME);
    else
        printf("complete!\n",data);
}

void dominoe(struct tm *lt)
{
    if(lt->tm_min == 60)
    {
        lt->tm_min = 0;
        ++lt->tm_hour;
    }

    if(lt->tm_hour == 24)
    {
        lt->tm_hour = 0;
        ++lt->tm_mday;
    }

    // jan 31
    // feb 28
    // mar 31
    // apr 30
    // may 31
    // jun 30
    // jul 31
    // aug 31
    // sep 30
    // oct 31
    // nov 30
    // dec 31

    switch(lt->tm_mon)
    {
        case 0:
        case 2:
        case 4:
        case 6:
        case 7:
        case 9:
        case 11:
            if(lt->tm_mday == 32)
            {
                lt->tm_mday = 1;
                ++lt->tm_mon;
            }
            break;

        case 3:
        case 5:
        case 8:
        case 10:
            if(lt->tm_mday == 31)
            {
                lt->tm_mday = 1;
                ++lt->tm_mon;
            }
            break;

        case 1:
            if(lt->tm_mday == 29)
            {
                lt->tm_mday = 1;
                ++lt->tm_mon;
            }
            break;
    }

    if(lt->tm_mon == 12)
    {
        lt->tm_mon = 0;
        ++lt->tm_year;
    }

    if(lt->tm_year > 99)
    {
        lt->tm_year -= 100;
    }
}

int open_pipe(void)
{
   BOOL    fSuccess = TRUE;
   USHORT  usMsg;
   PSZ     pszServer;
   ULONG   ulAction;
   int     apiRC;

   // Attempt to open named pipe.  If the pipe is busy, wait on it, and when
   // it becomes free try to open it again.  Continue until success or error.

   do
   {
      apiRC = DosOpen(PIPE_NAME,             /* pointer to filename      */
                (PHFILE) &hpPipe,            /* pointer to variable for file handle          */
                &ulAction,                   /* pointer to variable for action taken         */
                0L,                          /* file size if created or truncated            */
                0L,                          /* file attribute                               */
                FILE_OPEN,                   /* action taken if file exists/does not exist   */
                FS_AT_OPEN_MODE,             /* open mode of file                            */
                NULL);                       /* pointer to structure for extended attributes */
   }
   while(apiRC == ERROR_PIPE_BUSY &&
          !(BOOL)(apiRC = DosWaitNPipe(pszServer,NP_INDEFINITE_WAIT)));
}

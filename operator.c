/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: operator.c
**
**  Description:
**      Provide operator interface for CDC 6600 emulation. This is required
**      to enable a human "operator" to change tapes, remove paper from the
**      printer, shutdown etc.
**
**  This program is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License version 3 as
**  published by the Free Software Foundation.
**  
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License version 3 for more details.
**  
**  You should have received a copy of the GNU General Public License
**  version 3 along with this program in file "license-gpl-3.0.txt".
**  If not, see <http://www.gnu.org/licenses/gpl-3.0.txt>.
**
**--------------------------------------------------------------------------
*/

/*
**  -------------
**  Include Files
**  -------------
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>
#include "const.h"
#include "types.h"
#include "proto.h"
#if defined(_WIN32)
#include <windows.h>
#include <winsock.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#endif


/*
**  -----------------
**  Private Constants
**  -----------------
*/
#define CwdPathSize   256
#define MaxCardParams 10
#define MaxCmdStkSize 10

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/

/*
**  -----------------------------------------
**  Private Typedef and Structure Definitions
**  -----------------------------------------
*/
typedef struct opCmd
    {
    char            *name;               /* command name */
    void            (*handler)(bool help, char *cmdParams);
    } OpCmd;

typedef struct opCmdStackEntry
    {
    FILE *in;
    FILE *out;
    bool isNetConn;
    char cwd[CwdPathSize];
    } OpCmdStackEntry;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void opCreateThread(void);
#if defined(_WIN32)
static void opThread(void *param);
#else
static void *opThread(void *param);
#endif

static void opAcceptConnection(void);
static char *opGetString(char *inStr, char *outStr, int outSize);
static bool opHasInput(OpCmdStackEntry *entry);
static int opStartListening(int port);

static void opCmdDumpMemory(bool help, char *cmdParams);
static void opCmdDumpCM(int fwa, int count);
static void opCmdDumpEM(int fwa, int count);
static void opCmdDumpPP(int pp, int fwa, int count);
static void opHelpDumpMemory(void);

static void opCmdEnterKeys(bool help, char *cmdParams);
static void opHelpEnterKeys(void);
static void opWaitKeyConsume();
static void opWait(long milliseconds);

static void opCmdHelp(bool help, char *cmdParams);
static void opHelpHelp(void);

static void opCmdLoadCards(bool help, char *cmdParams);
static void opHelpLoadCards(void);
static int opPrepCards(char *fname, FILE *fcb);

static void opCmdLoadTape(bool help, char *cmdParams);
static void opHelpLoadTape(void);

static void opCmdPrompt(void);

static void opCmdShowTape(bool help, char *cmdParams);
static void opHelpShowTape(void);

static void opCmdUnloadTape(bool help, char *cmdParams);
static void opHelpUnloadTape(void);

static void opCmdRemoveCards(bool help, char *cmdParams);
static void opHelpRemoveCards(void);

static void opCmdRemovePaper(bool help, char *cmdParams);
static void opHelpRemovePaper(void);

static void opCmdSetKeyInterval(bool help, char *cmdParams);
static void opHelpSetKeyInterval(void);

static void opCmdSetOperatorPort(bool help, char *cmdParams);
static void opHelpSetOperatorPort(void);

static void opCmdShutdown(bool help, char *cmdParams);
static void opHelpShutdown(void);

static void opCmdPause(bool help, char *cmdParams);
static void opHelpPause(void);

/*
**  ----------------
**  Public Variables
**  ----------------
*/
volatile bool opActive = FALSE;

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static OpCmd decode[] = 
    {
    "d",                        opCmdDumpMemory,
    "dm",                       opCmdDumpMemory,
    "e",                        opCmdEnterKeys,
    "ek",                       opCmdEnterKeys,
    "lc",                       opCmdLoadCards,
    "lt",                       opCmdLoadTape,
    "rc",                       opCmdRemoveCards,
    "rp",                       opCmdRemovePaper,
    "p",                        opCmdPause,
    "ski",                      opCmdSetKeyInterval,
    "sop",                      opCmdSetOperatorPort,
    "st",                       opCmdShowTape,
    "ut",                       opCmdUnloadTape,
    "dump_memory",              opCmdDumpMemory,
    "enter_keys",               opCmdEnterKeys,
    "load_cards",               opCmdLoadCards,
    "load_tape",                opCmdLoadTape,
    "remove_cards",             opCmdRemoveCards,
    "remove_paper",             opCmdRemovePaper,
    "set_key_interval",         opCmdSetKeyInterval,
    "set_operator_port",        opCmdSetOperatorPort,
    "show_tape",                opCmdShowTape,
    "unload_tape",              opCmdUnloadTape,
    "?",                        opCmdHelp,
    "help",                     opCmdHelp,
    "shutdown",                 opCmdShutdown,
    "pause",                    opCmdPause,
    NULL,                       NULL
    };

static FILE *in;
static FILE *out;

static void (*opCmdFunction)(bool help, char *cmdParams);
static char opCmdParams[256];
static OpCmdStackEntry opCmdStack[MaxCmdStkSize];
static int opCmdStackPtr = 0;
#if defined(_WIN32)
static SOCKET opListenHandle = 0;
#else
static int opListenHandle = 0;
#endif
static int opListenPort = 0;
static volatile bool opPaused = FALSE;

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Operator interface initialisation.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void opInit(void)
    {
    /*
    **  Create the operator thread which accepts command input.
    */
    opCreateThread();
    }

/*--------------------------------------------------------------------------
**  Purpose:        Operator request handler called from the main emulation
**                  thread to avoid race conditions.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void opRequest(void)
    {
    if (opActive)
        {
        opCmdFunction(FALSE, opCmdParams);
        opActive = FALSE;

        if (emulationActive) opCmdPrompt();

        fflush(out);
        }
    }

/*
**--------------------------------------------------------------------------
**
**  Private Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Create operator thread.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCreateThread(void)
    {
#if defined(_WIN32)
    DWORD dwThreadId; 
    HANDLE hThread;

    /*
    **  Create operator thread.
    */
    hThread = CreateThread( 
        NULL,                                       // no security attribute 
        0,                                          // default stack size 
        (LPTHREAD_START_ROUTINE)opThread, 
        (LPVOID)NULL,                               // thread parameter 
        0,                                          // not suspended 
        &dwThreadId);                               // returns thread ID 

    if (hThread == NULL)
        {
        fprintf(stderr, "Failed to create operator thread\n");
        exit(1);
        }
#else
    int rc;
    pthread_t thread;
    pthread_attr_t attr;

    /*
    **  Create POSIX thread with default attributes.
    */
    pthread_attr_init(&attr);
    rc = pthread_create(&thread, &attr, opThread, NULL);
    if (rc < 0)
        {
        fprintf(stderr, "Failed to create operator thread\n");
        exit(1);
        }
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Operator thread.
**
**  Parameters:     Name        Description.
**                  param       Thread parameter (unused)
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
#if defined(_WIN32)
#include <conio.h>
static void opThread(void *param)
#else
static void *opThread(void *param)
#endif
    {
    OpCmd *cp;
    char cmd[256];
    char *line;
    char name[80];
    FILE *newIn;
    char *params;
    char path[256];
    char *pos;
    char *sp;

    opCmdStack[opCmdStackPtr].in  = in  = stdin;
    opCmdStack[opCmdStackPtr].out = out = stdout;
    opCmdStack[opCmdStackPtr].isNetConn = FALSE;

    fprintf(out, "\n%s.", DtCyberVersion " - " DtCyberCopyright);
    fprintf(out, "\n%s.", DtCyberLicense);
    fprintf(out, "\n%s.", DtCyberLicenseDetails);
    fputs("\n\nOperator interface", out);
    fprintf(out, "\nPlease enter 'help' to get a list of commands\n");
    opCmdPrompt();

    if (getcwd(opCmdStack[opCmdStackPtr].cwd, CwdPathSize) == NULL)
        {
        fputs("Failed to get current working directory path\n", stderr);
        exit(1);
        }
    if (initOpenOperatorSection() == 1)
        {
        opCmdStackPtr += 1;
        opCmdStack[opCmdStackPtr].in  = NULL;
        opCmdStack[opCmdStackPtr].out = out;
        opCmdStack[opCmdStackPtr].isNetConn = FALSE;
        strcpy(opCmdStack[opCmdStackPtr].cwd, opCmdStack[opCmdStackPtr - 1].cwd);
        }

    while (emulationActive)
        {
        opAcceptConnection();

        in  = opCmdStack[opCmdStackPtr].in;
        out = opCmdStack[opCmdStackPtr].out;
        fflush(out);

        if (opActive)
            {
            opWait(1);
            continue;
            }

        if (opHasInput(&opCmdStack[opCmdStackPtr]) == FALSE)
            {
            opWait(10);
            continue;
            }

        /*
        **  Wait for command input.
        */
        if (in == NULL)
            {
            line = initGetNextLine();
            if (line == NULL)
                {
                opCmdStackPtr -= 1;
                continue;
                }
            strcpy(cmd, line);
            }
        else if (fgets(cmd, sizeof(cmd), in) == NULL)
            {
            if (opCmdStack[opCmdStackPtr].isNetConn && errno == EAGAIN) continue;
            if (opCmdStackPtr > 0)
                {
                fclose(in);
                if (opCmdStack[opCmdStackPtr].isNetConn)
                    {
                    fclose(out);
                    opStartListening(opListenPort);
                    }
                opCmdStackPtr -= 1;
                if (opCmdStackPtr == 0)
                    {
                    in = opCmdStack[opCmdStackPtr].in;
                    out = opCmdStack[opCmdStackPtr].out;
                    opCmdPrompt();
                    fflush(out);
                    };
                }
            else
                {
                fputs("\nConsole closed\n", out);
                emulationActive = FALSE;
                }
            continue;
            }
        else if (strlen(cmd) == 0)
            {
            continue;
            }

        if (in != stdin && opCmdStack[opCmdStackPtr].isNetConn == FALSE)
            {
            fputs(cmd, out);
            fputs("\n", out);
            fflush(out);
            }

        if (opPaused)
            {
            /*
            **  Unblock main emulation thread.
            */
            opPaused = FALSE;
            continue;
            }

        if (opActive)
            {
            /*
            **  The main emulation thread is still busy executing the previous command.
            */
            fputs("\nPrevious request still busy", out);
            continue;
            }

        /*
        **  Replace newline by zero terminator.
        */
        pos = strchr(cmd, '\n');
        if (pos != NULL)
            {
            *pos = 0;
            }

        /*
        **  Extract the command name.
        */
        params = opGetString(cmd, name, sizeof(name));
        if (*name == 0)
            {
            opCmdPrompt();
            continue;
            }
        else if (*name == '@')
            {
            if (opCmdStackPtr + 1 >= MaxCmdStkSize)
                {
                fputs("Too many nested command scripts\n", out);
                opCmdPrompt();
                continue;
                }
            sp = name + 1;
            if (*sp == '/')
                {
                strcpy(path, sp);
                }
            else
                {
                sprintf(path, "%s/%s", opCmdStack[opCmdStackPtr].cwd, sp);
                }
            newIn = fopen(path, "r");
            if (newIn != NULL)
                {
                opCmdStackPtr += 1;
                opCmdStack[opCmdStackPtr].in  = newIn;
                opCmdStack[opCmdStackPtr].out = out;
                opCmdStack[opCmdStackPtr].isNetConn = FALSE;
                pos = strrchr(path, '/');
                *pos = '\0';
                strcpy(opCmdStack[opCmdStackPtr].cwd, path);
                }
            else
                {
                fprintf(out, "Failed to open %s\n", path);
                }
            opCmdPrompt();
            continue;
            }

        /*
        **  Find the command handler.
        */
        for (cp = decode; cp->name != NULL; cp++)
            {
            if (strcmp(cp->name, name) == 0)
                {
                if (cp->handler == opCmdEnterKeys)
                    {
                    opCmdEnterKeys(FALSE, params);
                    opActive = FALSE;
                    break;
                    }
                /*
                **  Request the main emulation thread to execute the command.
                */
                strcpy(opCmdParams, params);
                opCmdFunction = cp->handler;
                opActive = TRUE;
                break;
                }
            }

        if (cp->name == NULL)
            {
            /*
            **  Try to help user.
            */
            fprintf(out, "Command not implemented: %s\n\n", name);
            fputs("Try 'help' to get a list of commands or 'help <command>'\n", out);
            fputs("to get a brief description of a command.\n", out);
            opCmdPrompt();
            continue;
            }
        }

#if !defined(_WIN32)
    return(NULL);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Determine whether input is available on a command
**                  stack entry.
**
**  Parameters:     Name        Description.
**                  param       Thread parameter (unused)
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static bool opHasInput(OpCmdStackEntry *entry)
    {
    int fd;
    fd_set fds;
    int n;
    struct timeval timeout;

    if (entry->in == NULL) return TRUE;

#if defined(_WIN32)
    if (entry->in == stdin) return kbhit();
#endif

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    fd = fileno(entry->in);
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    return select(fd + 1, &fds, NULL, NULL, &timeout) > 0;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Accept operator connection.
**
**  Parameters:     Name        Description.
**                  param       Thread parameter (unused)
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opAcceptConnection(void)
    {
    int acceptFd;
    fd_set acceptFds;
    struct sockaddr_in from;
    int n;
    struct timeval timeout;
#if defined(_WIN32)
    int fromLen;
#else
    socklen_t fromLen;
#endif

    if (opListenHandle == 0) return;

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    FD_ZERO(&acceptFds);
    FD_SET(opListenHandle, &acceptFds);

    n = select(opListenHandle + 1, &acceptFds, NULL, NULL, &timeout);
    if (n <= 0) return;
    fromLen = sizeof(from);
    acceptFd = accept(opListenHandle, (struct sockaddr *)&from, &fromLen);
    if (acceptFd >= 0)
        {
        if (opCmdStackPtr + 1 >= MaxCmdStkSize)
            {
            fputs("Too many nested operator input sources\n", out);
#if defined(_WIN32)
            closesocket(acceptFd);
#else
            close(acceptFd);
#endif
            return;
            }
        fputs("\nOperator connection accepted\n", out);
        fflush(out);
        opCmdStackPtr += 1;
        in  = opCmdStack[opCmdStackPtr].in  = fdopen(acceptFd, "r");
        out = opCmdStack[opCmdStackPtr].out = fdopen(acceptFd, "w");
        opCmdStack[opCmdStackPtr].isNetConn = TRUE;
        strcpy(opCmdStack[opCmdStackPtr].cwd, opCmdStack[opCmdStackPtr - 1].cwd);
#if defined(_WIN32)
        closesocket(opListenHandle);
#else
        close(opListenHandle);
#endif
        opListenHandle = 0;
        opCmdPrompt();
        fflush(out);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Parse command string and return the first string
**                  terminated by whitespace
**
**  Parameters:     Name        Description.
**                  inStr       Input string
**                  outStr      Output string
**                  outSize     Size of output field
**
**  Returns:        Pointer to first character in input string after
**                  the extracted string..
**
**------------------------------------------------------------------------*/
static char *opGetString(char *inStr, char *outStr, int outSize)
    {
    u8 pos = 0;
    u8 len = 0;

    /*
    **  Skip leading whitespace.
    */
    while (inStr[pos] != 0 && isspace(inStr[pos]))
        {
        pos += 1;
        }

    /*
    **  Return pointer to end of input string when there was only
    **  whitespace left.
    */
    if (inStr[pos] == 0)
        {
        *outStr = 0;
        return(inStr + pos);
        }

    /*
    **  Copy string to output buffer.
    */
    while (inStr[pos] != 0 && !isspace(inStr[pos]))
        {
        if (len >= outSize - 1)
            {
            return(NULL);
            }

        outStr[len++] = inStr[pos++];
        }

    outStr[len] = 0;

    /*
    **  Skip trailing whitespace.
    */
    while (inStr[pos] != 0 && isspace(inStr[pos]))
        {
        pos += 1;
        }

    return(inStr + pos);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Dump CM, EM, or PP memory.
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdDumpMemory(bool help, char *cmdParams)
    {
    int count;
    char *cp;
    int fwa;
    char *memType;
    int numParam;
    int pp;

    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpDumpMemory();
        return;
        }

    cp = memType = cmdParams;
    while (*cp != '\0' && *cp != ',') ++cp;
    if (*cp != ',')
        {
        fputs("Not enough parameters\n", out);
        return;
        }
    *cp++ = '\0';
    numParam = sscanf(cp, "%o,%d", &fwa, &count);
    if (numParam != 2)
        {
        fputs("Not enough or invalid parameters\n", out);
        return;
        }
    if (strcasecmp(memType, "CM") == 0)
        {
        opCmdDumpCM(fwa, count);
        }
    else if (strcasecmp(memType, "EM") == 0)
        {
        opCmdDumpEM(fwa, count);
        }
    else if (strncasecmp(memType, "PP", 2) == 0)
        {
        numParam = sscanf(memType + 2, "%o", &pp);
        if (numParam != 1)
            {
            fputs("Missing or invalid PP number\n", out);
            }
        opCmdDumpPP(pp, fwa, count);
        }
    else
        {
        fputs("Invalid memory type\n", out);
        }
    }

static void opCmdDumpCM(int fwa, int count)
    {
    char buf[42];
    char *cp;
    int limit;
    int n;
    int shiftCount;
    CpWord word;

    if (fwa < 0 || count < 0 || fwa + count > cpuMaxMemory)
        {
        fputs("Invalid CM address or count\n", out);
        return;
        }
    for (limit = fwa + count; fwa < limit; fwa++)
        {
        word = cpMem[fwa];
        n = sprintf(buf, "%08o %020lo ", fwa, word);
        cp = buf + n;
        for (shiftCount = 54; shiftCount >= 0; shiftCount -= 6)
            {
            *cp++ = cdcToAscii[(word >> shiftCount) & 077];
            }
        *cp++ = '\n';
        *cp = '\0';
        fputs(buf, out);
        }
    }

static void opCmdDumpEM(int fwa, int count)
    {
    char buf[42];
    char *cp;
    int limit;
    int n;
    int shiftCount;
    CpWord word;

    if (fwa < 0 || count < 0 || fwa + count > extMaxMemory)
        {
        fputs("Invalid EM address or count\n", out);
        return;
        }
    for (limit = fwa + count; fwa < limit; fwa++)
        {
        word = extMem[fwa];
        n = sprintf(buf, "%08o %020lo ", fwa, word);
        cp = buf + n;
        for (shiftCount = 54; shiftCount >= 0; shiftCount -= 6)
            {
            *cp++ = cdcToAscii[(word >> shiftCount) & 077];
            }
        *cp++ = '\n';
        *cp = '\0';
        fputs(buf, out);
        }
    }

static void opCmdDumpPP(int ppNum, int fwa, int count)
    {
    char buf[20];
    char *cp;
    int limit;
    int n;
    PpSlot *pp;
    PpWord word;

    if (ppNum >= 020 && ppNum <= 031)
        {
        ppNum -= 6;
        }
    else if (ppNum < 0 || ppNum > 011)
        {
        fputs("Invalid PP number\n", out);
        return;
        }
    if (ppNum >= ppuCount)
        {
        fputs("Invalid PP number\n", out);
        return;
        }
    if (fwa < 0 || count < 0 || fwa + count > 010000)
        {
        fputs("Invalid PP address or count\n", out);
        return;
        }
    pp = &ppu[ppNum];
    for (limit = fwa + count; fwa < limit; fwa++)
        {
        word = pp->mem[fwa];
        n = sprintf(buf, "%04o %04o ", fwa, word);
        cp = buf + n;
        *cp++ = cdcToAscii[(word >> 6) & 077];
        *cp++ = cdcToAscii[word & 077];
        *cp++ = '\n';
        *cp = '\0';
        fputs(buf, out);
        }
    }

static void opHelpDumpMemory(void)
    {
    fputs("'dump_memory CM,<fwa>,<count>' dump <count> words of central memory starting from octal address <fwa>.\n", out);
    fputs("'dump_memory EM,<fwa>,<count>' dump <count> words of extended memory starting from octal address <fwa>.\n", out);
    fputs("'dump_memory PP<nn>,<fwa>,<count>' dump <count> words of PP nn's memory starting from octal address <fwa>.\n", out);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Enter keys on the system console.
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
char opKeyIn = 0;
long opKeyInterval = 250;

static void opCmdEnterKeys(bool help, char *cmdParams)
    {
    char *bp;
    time_t clock;
    char *cp;
    char keybuf[256];
    char *kp;
    char *limit;
    long msec;
    char timestamp[20];
    struct tm *tmp;

    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpEnterKeys();
        return;
        }

    /*
     *  First, edit the parameter string to subtitute values
     *  for keywords. Keywords are delimited by '%'.
     */
    clock = time(NULL);
    tmp = localtime(&clock);
    sprintf(timestamp, "%02d%02d%02d%02d%02d%02d",
        (u8)tmp->tm_year - 100, (u8)tmp->tm_mon + 1, (u8)tmp->tm_mday,
        (u8)tmp->tm_hour, (u8)tmp->tm_min, (u8)tmp->tm_sec);
    cp = cmdParams;
    bp = keybuf;
    limit = bp + sizeof(keybuf) - 2;
    while (*cp != '\0' && bp < limit)
        {
        if (*cp == '%')
            {
            kp = ++cp;
            while (*cp != '%' && *cp != '\0') cp += 1;
            if (*cp == '%') *cp++ = '\0';
            if (strcasecmp(kp, "year") == 0)
                {
                memcpy(bp, timestamp, 2);
                bp += 2;
                }
            else if (strcasecmp(kp, "mon") == 0)
                {
                memcpy(bp, timestamp + 2, 2);
                bp += 2;
                }
            else if (strcasecmp(kp, "day") == 0)
                {
                memcpy(bp, timestamp + 4, 2);
                bp += 2;
                }
            else if (strcasecmp(kp, "hour") == 0)
                {
                memcpy(bp, timestamp + 6, 2);
                bp += 2;
                }
            else if (strcasecmp(kp, "min") == 0)
                {
                memcpy(bp, timestamp + 8, 2);
                bp += 2;
                }
            else if (strcasecmp(kp, "sec") == 0)
                {
                memcpy(bp, timestamp + 10, 2);
                bp += 2;
                }
            else
                {
                fprintf(out, "Unrecognized keyword: %%%s%%\n", kp);
                opCmdPrompt();
                return;
                }
            }
        else
            {
            *bp++ = *cp++;
            }
        }
    if (bp > limit)
        {
        fputs("Key sequence is too long\n", out);
        opCmdPrompt();
        return;
        }
    *bp = '\0';
    /*
     *  Next, traverse the key sequence, supplying keys to the console
     *  one by one. Recognize and process special characters along the
     *  way as follows:
     *
     *    ! - end the sequence, and do not send an <enter> key
     *    ; - send an <enter> key within a sequence
     *    _ - send a blank
     *    ^ - send a backspace
     *    # - delimit a milliseconds value (e.g., #500#) and pause for
     *        the speccified amount of time
     */
    opWaitKeyConsume(); // just in case
    cp = keybuf;
    while (*cp != '\0' && *cp != '!')
        {
        switch (*cp)
            {
        default:
            opKeyIn = *cp;
            break;
        case ';':
            opKeyIn = '\r';
            break;
        case '_':
            opKeyIn = ' ';
            break;
        case '^':
            opKeyIn = '\b';
            break;
        case '#':
            msec = 0;
            cp += 1;
            while (*cp >= '0' && *cp <= '9')
                {
                msec = (msec * 10) + (*cp++ - '0');
                }
            if (*cp != '#') cp -= 1;
            opWait(msec);
            break;
            }
        cp += 1;
        opWait(opKeyInterval);
        opWaitKeyConsume();
        }
    if (*cp != '!')
        {
        opKeyIn = '\r';
        opWait(opKeyInterval);
        opWaitKeyConsume();
        }
    opCmdPrompt();
    }

static void opHelpEnterKeys(void)
    {
    fputs("'enter_keys <key-sequence>' supply a sequence of key entries to the system console .\n", out);
    fputs("     Special keys:\n", out);
    fputs("       ! - end sequence without sending <enter> key\n", out);
    fputs("       ; - send <enter> key within a sequence\n", out);
    fputs("       _ - send <blank> key\n", out);
    fputs("       ^ - send <backspace> key\n", out);
    fputs("       % - keyword delimiter for keywords:\n", out);
    fputs("           %year% insert current year\n", out);
    fputs("           %mon%  insert current month\n", out);
    fputs("           %day%  insert current day\n", out);
    fputs("           %hour% insert current hour\n", out);
    fputs("           %min%  insert current minute\n", out);
    fputs("           %sec%  insert current second\n", out);
    fputs("       # - delimiter for milliseconds pause value (e.g., #500#)\n", out);
    }

static void opWaitKeyConsume()
    {
    while (opKeyIn != 0 || ppKeyIn != 0)
        {
        #if defined(_WIN32)
        Sleep(100);
        #else
        usleep(100000);
        #endif
        }
    }

static void opWait(long milliseconds)
    {
    #if defined(_WIN32)
    Sleep(milliseconds);
    #else
    usleep((useconds_t)(milliseconds * 1000));
    #endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set interval between console key entries.
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdSetKeyInterval(bool help, char *cmdParams)
    {
    int msec;
    int numParam;

    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpSetKeyInterval();
        return;
        }
    numParam = sscanf(cmdParams, "%d", &msec);
    if (numParam != 1)
        {
        fputs("Missing or invalid parameter\n", out);
        return;
        }
    opKeyInterval = msec;
    }

static void opHelpSetKeyInterval(void)
    {
    fputs("'set_key_interval <millisecs>' set the interval between key entries to the system console.\n", out);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set TCP port on which to listen for operator connections
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdSetOperatorPort(bool help, char *cmdParams)
    {
    int numParam;
    int port;

    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpSetOperatorPort();
        return;
        }
    numParam = sscanf(cmdParams, "%d", &port);
    if (numParam != 1)
        {
        fputs("Missing or invalid parameter\n", out);
        return;
        }
    if (port < 0 || port > 65535)
        {
        fputs("Invalid port number\n", out);
        return;
        }
    if (opListenHandle != 0)
        {
#if defined(_WIN32)
        closesocket(opListenHandle);
#else
        close(opListenHandle);
#endif
        opListenHandle = 0;
        if (port == 0) fputs("Operator port closed\n", out);
        }
    if (port == 0) return;

    if (opStartListening(port))
        {
        opListenPort = port;
        fprintf(out, "Listening for operator connections on port %d\n", port);
        }
    }

static void opHelpSetOperatorPort(void)
    {
    fputs("'set_operator_port <port>' set the TCP port on which to listen for operator connections.\n", out);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Start listening for operator connections
**
**  Parameters:     Name        Description.
**                  port        TCP port number on which to listen
**
**  Returns:        TRUE  if success
**                  FALSE if failure
**
**------------------------------------------------------------------------*/
static int opStartListening(int port)
    {
#if defined(_WIN32)
    u_long blockEnable = 1;
#endif
    int optEnable = 1;
    struct sockaddr_in server;

    /*
    **  Create TCP socket and bind to specified port.
    */
    opListenHandle = socket(AF_INET, SOCK_STREAM, 0);
    if (opListenHandle < 0)
        {
        fprintf(out, "Failed to create socket for port %d\n", port);
        opListenHandle = 0;
        return FALSE;
        }
    /*
    **  Accept will block if client drops connection attempt between select and accept.
    **  We can't block so make listening socket non-blocking to avoid this condition.
    */
#if defined(_WIN32)
    ioctlsocket(opListenHandle, FIONBIO, &blockEnable);
#else
    fcntl(opListenHandle, F_SETFL, O_NONBLOCK);
#endif

    /*
    **  Bind to configured TCP port number
    */
    setsockopt(opListenHandle, SOL_SOCKET, SO_REUSEADDR, (void *)&optEnable, sizeof(optEnable));
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("0.0.0.0");
    server.sin_port = htons(port);

    if (bind(opListenHandle, (struct sockaddr *)&server, sizeof(server)) < 0)
        {
        fprintf(out, "Failed to bind to listen socket for port %d\n", port);
#if defined(_WIN32)
        closesocket(opListenHandle);
#else
        close(opListenHandle);
#endif
        opListenHandle = 0;
        return FALSE;
        }
    /*
    **  Start listening for new connections on this TCP port number
    */
    if (listen(opListenHandle, 1) < 0)
        {
        fprintf(out, "Failed to listen on port %d\n", port);
#if defined(_WIN32)
        closesocket(opListenHandle);
#else
        close(opListenHandle);
#endif
        opListenHandle = 0;
        return FALSE;
        }

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Pause emulation.
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdPause(bool help, char *cmdParams)
    {
    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpPause();
        return;
        }

    /*
    **  Check parameters.
    */
    if (strlen(cmdParams) != 0)
        {
        fputs("No parameters expected\n", out);
        opHelpPause();
        return;
        }

    /*
    **  Process command.
    */
    fputs("Emulation paused - hit Enter to resume\n", out);

    /*
    **  Wait for Enter key.
    */
    opPaused = TRUE;
    while (opPaused)
        {
        /* wait for operator thread to clear the flag */
        #if defined(_WIN32)
        Sleep(500);
        #else
        usleep(500000);
        #endif
        }
    }

static void opHelpPause(void)
    {
    fputs("'pause' suspends emulation to reduce CPU load.\n", out);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Issue a command prompt.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdPrompt(void)
    {
    fputs("\nOperator> ", out);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Terminate emulation.
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdShutdown(bool help, char *cmdParams)
    {
    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpShutdown();
        return;
        }

    /*
    **  Check parameters.
    */
    if (strlen(cmdParams) != 0)
        {
        fputs("No parameters expected\n", out);
        opHelpShutdown();
        return;
        }

    /*
    **  Process command.
    */
    opActive = FALSE;
    emulationActive = FALSE;

    fprintf(out, "\nThanks for using %s\nGoodbye for now.\n\n", DtCyberVersion);
    }

static void opHelpShutdown(void)
    {
    fputs("'shutdown' terminates emulation.\n", out);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Provide command help.
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdHelp(bool help, char *cmdParams)
    {
    OpCmd *cp;

    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpHelp();
        return;
        }

    /*
    **  Check parameters and process command.
    */
    if (strlen(cmdParams) == 0)
        {
        /*
        **  List all available commands.
        */
        fputs("\nList of available commands:\n\n", out);
        for (cp = decode; cp->name != NULL; cp++)
            {
            fprintf(out, "%s\n", cp->name);
            }

        fputs("\nTry 'help <command> to get a brief description of a command.\n", out);
        return;
        }
    else
        {
        /*
        **  Provide help for specified command.
        */
        for (cp = decode; cp->name != NULL; cp++)
            {
            if (strcmp(cp->name, cmdParams) == 0)
                {
                fputs("\n", out);
                cp->handler(TRUE, NULL);
                return;
                }
            }

        fprintf(out, "Command not implemented: %s\n", cmdParams);
        }
    }

static void opHelpHelp(void)
    {
    fputs("'help'       list all available commands.\n", out);
    fputs("'help <cmd>' provide help for <cmd>.\n", out);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Load a stack of cards
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdLoadCards(bool help, char *cmdParams)
    {
    int channelNo;
    int equipmentNo;
    FILE *fcb;
    char fname[80];
    int numParam;
    int rc;
    static int seqNo = 1;
    static char str[200];

    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpLoadCards();
        return;
        }

    numParam = sscanf(cmdParams,"%o,%o,%s",&channelNo, &equipmentNo, str);

    /*
    **  Check parameters.
    */
    if (numParam < 3)
        {
        fputs("Not enough or invalid parameters\n", out);
        opHelpLoadCards();
        return;
        }

    if (channelNo < 0 || channelNo >= MaxChannels)
        {
        fputs("Invalid channel no\n", out);
        return;
        }

    if (equipmentNo < 0 || equipmentNo >= MaxEquipment)
        {
        fputs("Invalid equipment no\n", out);
        return;
        }

    if (str[0] == 0)
        {
        fputs("Invalid file name\n", out);
        return;
        }
    /*
    **  Create temporary file for preprocessed card deck
    */
    sprintf(fname, "CR_C%o_E%o_%d", channelNo, equipmentNo, seqNo++);
    fcb = fopen(fname, "w");
    if (fcb == NULL)
        {
        fprintf(out, "Failed to create %s\n", fname);
        return;
        }

    /*
    **  Preprocess card file
    */
    rc = opPrepCards(str, fcb);
    fclose(fcb);
    if (rc == -1)
        {
        unlink(fname);
        return;
        }
    cr405LoadCards(fname, channelNo, equipmentNo, out);
    cr3447LoadCards(fname, channelNo, equipmentNo, out);
    }

static void opHelpLoadCards(void)
    {
    fputs("'load_cards <channel>,<equipment>,<filename>[,<p1>,<p2>,...,<pn>]' load specified card file with optional parameters.\n", out);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Preprocess a card file
**
**                  The specified source file is read, nested "~include"
**                  directives are detected and processed recursively,
**                  and embedded parameter references are interpolated.
**
**  Parameters:     Name        Description.
**                  str         Source file path and optional parameters
**                  fcb         handle of output file
**
**  Returns:        0 if success
**                 -1 if failure
**
**------------------------------------------------------------------------*/
static int opPrepCards(char *str, FILE *fcb)
    {
    int argc;
    int argi;
    char *argv[MaxCardParams];
    char *cp;
    char dbuf[400];
    char *dfltVal;
    int dfltValLen;
    char *dp;
    FILE *in;
    char *lastnb;
    char params[400];
    char path[256];
    char sbuf[400];
    char *sp;

    /*
    **  The parameter string has the form:
    **
    **    <filepath>,<arg1>,<arg2>,...,<argn>
    **
    **  where the args are optional.
    */
    argc = 0;
    cp = strchr(str, ',');
    if (cp != NULL)
        {
        *cp++ = '\0';
        while (*cp != '\0')
            {
            if (argc < MaxCardParams) argv[argc++] = cp;
            cp = strchr(cp, ',');
            if (cp == NULL) break;
            *cp++ = '\0';
            }
        }
    /*
    **  Open and parse the input file
    */
    if (*str == '/')
        {
        strcpy(path, str);
        }
    else
        {
        sprintf(path, "%s/%s", opCmdStack[opCmdStackPtr].cwd, str);
        }
    in = fopen(path, "r");
    if (in == NULL)
        {
        fprintf(out, "Failed to open %s\n", path);
        return -1;
        }
    while (TRUE)
        {
        sp = fgets(sbuf, sizeof(sbuf), in);
        if (sp == NULL)
            {
            fclose(in);
            return 0;
            }
        /*
        **  Scan the source line for parameter references and interpolate
        **  any found. A parameter reference has the form "${n}" where "n"
        **  is an integer greater than 0.
        */
        dp = dbuf;
        while (*sp != '\0')
            {
            if (*sp == '$' && *(sp + 1) == '{' && isdigit(*(sp + 2)))
                {
                argi = 0;
                dfltVal = "";
                dfltValLen = 0;
                cp = sp + 2;
                while (isdigit(*cp))
                    {
                    argi = (argi * 10) + (*cp++ - '0');
                    }
                if (*cp == ':')
                    {
                    dfltVal = ++cp;
                    while (*cp != '}' && *cp != '\0') cp += 1;
                    dfltValLen = cp - dfltVal;
                    }
                if (*cp == '}')
                    {
                    sp = cp + 1;
                    argi -= 1;
                    if (argi >= 0 && argi < argc)
                        {
                        cp = argv[argi];
                        while (*cp != '\0') *dp++ = *cp++;
                        }
                    else
                        {
                        while (dfltValLen-- > 0) *dp++ = *dfltVal++;
                        }
                    continue;
                    }
                }
            *dp++ = *sp++;
            }
        *dp = '\0';
        /*
        **  Recognize nested "~include" directives and
        **  process them recursively.
        */
        sp = dbuf;
        if (strncmp(sp, "~include ", 9) == 0)
            {
            sp += 9;
            while (isspace(*sp)) sp += 1;
            if (*sp == '\0')
                {
                fprintf(out, "File name missing from ~include in %s\n", path);
                fclose(in);
                return -1;
                }
            if (*sp != '/')
                {
                cp = strrchr(path, '/');
                if (cp != NULL)
                    {
                    *cp = '\0';
                    sprintf(params, "%s/%s", path, sp);
                    *cp = '/';
                    sp = params;
                    }
                }
            /*
            **  Trim trailing whitespace from pathname and parameters
            */
            lastnb = sp;
            for (cp = sp; *cp != '\0'; cp++)
                 {
                 if (!isspace(*cp)) lastnb = cp;
                 }
            *(lastnb + 1) = '\0';
            /*
            **  Process nested include file recursively
            */
            if (opPrepCards(sp, fcb) == -1)
                {
                fclose(in);
                return -1;
                }
            }
        /*
        **  Recognize and ignore embedded comments. Embedded comments
        **  are lines beginning with "~*".
        */
        else if (strncmp(sp, "~*", 2) != 0)
            {
            fputs(sp, fcb);
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Load a new tape
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdLoadTape(bool help, char *cmdParams)
    {
    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpLoadTape();
        return;
        }

    /*
    **  Check parameters and process command.
    */
    if (strlen(cmdParams) == 0)
        {
        printf("parameters expected\n");
        opHelpLoadTape();
        return;
        }

    mt669LoadTape(cmdParams, out);
    mt679LoadTape(cmdParams, out);
    mt362xLoadTape(cmdParams, out);
    }

static void opHelpLoadTape(void)
    {
    fputs("'load_tape <channel>,<equipment>,<unit>,<r|w>,<filename>' load specified tape.\n", out);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Unload a mounted tape
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdUnloadTape(bool help, char *cmdParams)
    {
    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpUnloadTape();
        return;
        }

    /*
    **  Check parameters and process command.
    */
    if (strlen(cmdParams) == 0)
        {
        fputs("Parameters expected\n", out);
        opHelpUnloadTape();
        return;
        }

    mt669UnloadTape(cmdParams, out);
    mt679UnloadTape(cmdParams, out);
    mt362xUnloadTape(cmdParams, out);
    }

static void opHelpUnloadTape(void)
    {
    fputs("'unload_tape <channel>,<equipment>,<unit>' unload specified tape unit.\n", out);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Show status of all tape units
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdShowTape(bool help, char *cmdParams)
    {
    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpShowTape();
        return;
        }

    /*
    **  Check parameters and process command.
    */
    if (strlen(cmdParams) != 0)
        {
        fputs("No parameters expected\n", out);
        opHelpShowTape();
        return;
        }

    mt669ShowTapeStatus(out);
    mt679ShowTapeStatus(out);
    mt362xShowTapeStatus(out);
    mt5744ShowTapeStatus(out);
    }

static void opHelpShowTape(void)
    {
    fputs("'show_tape' show status of all tape units.\n", out);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Remove paper from printer.
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdRemovePaper(bool help, char *cmdParams)
    {
    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpRemovePaper();
        return;
        }

    /*
    **  Check parameters and process command.
    */
    if (strlen(cmdParams) == 0)
        {
        fputs("Parameters expected\n", out);
        opHelpRemovePaper();
        return;
        }

    lp1612RemovePaper(cmdParams, out);
    lp3000RemovePaper(cmdParams, out);
    }

static void opHelpRemovePaper(void)
    {
    fputs("'remove_paper <channel>,<equipment>[,<filename>]' remover paper from printer.\n", out);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Remove cards from card puncher.
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdRemoveCards(bool help, char *cmdParams)
    {
    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpRemoveCards();
        return;
        }

    /*
    **  Check parameters and process command.
    */
    if (strlen(cmdParams) == 0)
        {
        fputs("Parameters expected\n", out);
        opHelpRemoveCards();
        return;
        }

    cp3446RemoveCards(cmdParams, out);
    }

static void opHelpRemoveCards(void)
    {
    fputs("'remove_cards <channel>,<equipment>[,<filename>]' remover cards from card puncher.\n", out);
    }

/*---------------------------  End Of File  ------------------------------*/

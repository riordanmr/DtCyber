/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**            (c) 2017       Steven Zoppi 10-Nov-2017
**                           Added status messaging support
**
**  Name: cr405b.c
**
**  Description:
**      Perform emulation of channel-connected CDC 405-B card reader.
**      It does not use a 3000 series channel converter.
**
**  20171110: SZoppi - Added Filesystem Watcher Support
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
#include <errno.h>
#include <sys/stat.h>
#include "const.h"
#include "types.h"
#include "proto.h"

#if defined(_WIN32)

//  Filesystem Watcher Machinery

#include <Windows.h>
#include "dirent_win.h"
#else
#include <dirent.h>
#include <ctype.h>
#endif

/*
**  -----------------
**  Private Constants
**  -----------------
*/
#if DEBUG
static FILE *cr405Log = NULL;
#endif

/*
**  CDC 405 card reader function and status codes.
**
**      Function codes
**
**      ----------------------------------
**      |  Equip select  |   function    |
**      ----------------------------------
**      11              6 5             0
**
**      0700 = Deselect
**      0701 = Gate Card to Secondary bin
**      0702 = Read Non-stop
**      0704 = Status request
**
**      Note: To read one card, execute successive S702 and
**      S704 functions.
**      One column of card data per 12-bit data word.
*/
#define FcCr405Deselect       00700
#define FcCr405GateToSec      00701
#define FcCr405ReadNonStop    00702
#define FcCr405StatusReq      00704

/*
**      Status reply
**
**      0000 = Ready
**      0001 = Not ready
**      0002 = End of file
**      0004 = Compare error
**
*/
#define StCr405Ready          00000
#define StCr405NotReady       00001
#define StCr405EOF            00002
#define StCr405CompareErr     00004

#define Cr405MaxDecks         10

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
typedef struct cr405Context
    {
    /*
    **  Info for show_tape operator command.
    */
    struct cr405Context *nextUnit;
    u8                  channelNo;
    u8                  eqNo;
    u8                  unitNo;

    const u16           *table;
    u32                 getCardCycle;
    int                 col;
    PpWord              card[80];
    int                 inDeck;
    int                 outDeck;
    char                *decks[Cr405MaxDecks];

    char                curFileName[_MAX_PATH + 1];
    char                dirInput[_MAX_PATH];
    char                dirOutput[_MAX_PATH];
    int                 seqNum;
    bool                isWatched;
    } Cr405Context;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static FcStatus cr405Func(PpWord funcCode);
static void cr405Io(void);
static void cr405Activate(void);
static void cr405Disconnect(void);
static void cr405NextCard(DevSlot *dp);
static bool cr405StartNextDeck(DevSlot *up, Cr405Context *cc, FILE *out);
// static char* strlwr(char* str);

/*
**  ----------------
**  Public Variables
**  ----------------
*/

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static Cr405Context *firstCr405 = NULL;
static Cr405Context *lastCr405  = NULL;

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */
/*--------------------------------------------------------------------------
**  Purpose:        Initialise card reader.
**
**  Parameters:     Name        Description.
**                  eqNo        equipment number
**                  unitNo      unit number
**                  channelNo   channel number the device is attached to
**                  deviceName  optional device file name
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cr405Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    Cr405Context *cc;
    DevSlot      *dp;

    (void)deviceName;

    fswContext *threadParms;

    //  Extensions for Filesystem Watcher
    char *xlateTable;
    char *crInput;
    char *crOutput;
    char *Auto;
    bool watchRequested;
    char tokenString[80] = " ";     //  Silly workaround because of strtok
                                    //  treating multiple consecutive delims
                                    //  as one.
    struct stat s;

    /*
    **  For FileSystem Watcher
    **
    **  We pass the fswContext structure to be used by the thread.
    **
    **  deviceName is the "space delimited" remainder of the .ini
    **  file line.
    **
    **  It can have up to three additional parameters of which NONE
    **  may contain a space.  Specifying Parameters 2 or 3 REQUIRES
    **  specification of the Previous parameter.  You may not specify
    **  Parameter 2 without Parameter 1.
    **
    **  Parameter   Description
    **  ---------   -------------------------------------------------------
    **
    **      1       <Optional:> "*"=NULL Placeholder|"026"|"029" Default="026"
    **              "026" or "029" <Translate Table Specification>
    **
    **      2       <optional> "*"=NULL Placeholder|CRInputFolder
    **
    **              The directory of the card reader's virtual "hopper"
    **              although the user can still load cards directly through
    **              the operator interface, a directory can be specified
    **              into which card decks can be submitted for sequential
    **              processing based on create date.
    **
    **              NO Thread will be created if this parameter doesn't exist.
    **
    **      3       <optional> "*"=NULL Placeholder|CROutputFolder
    **
    **              If specified, indicates the directory into which the
    **              processed cards will be deposited.  Naming conflicts
    **              will be serialized with a suffix.
    **
    **              If not specified, all input files will simply be deleted
    **              upon closure.
    **
    **      4       <optional> "*"=NULL Placeholder|"AUTO"|"NOAUTO" Default = "Auto"
    **              If a Virtual Card Hopper is defined, then this
    **              parameter indicates whether or not to initiate a Filewatcher
    **              thread to automatically submit jobs in file creation order
    **              from the CRInputFolder
    **
    **  The context can be declared locally because it's just
    **  a structure used to marshal the parameters into the
    **  thread's context.
    */

#if DEBUG
    if (cr405Log == NULL)
        {
        cr405Log = fopen("cr405Log.txt", "wt");
        }
#endif

    if (eqNo != 0)
        {
        fprintf(stderr, "(cr405  ) Invalid equipment number - hardwired to equipment number 0\n");
        exit(1);
        }

    if (unitNo != 0)
        {
        fprintf(stderr, "(cr405  ) Invalid unit number - hardwired to unit number 0\n");
        exit(1);
        }

    dp = channelAttach(channelNo, eqNo, DtCr405);

    dp->activate     = cr405Activate;
    dp->disconnect   = cr405Disconnect;
    dp->func         = cr405Func;
    dp->io           = cr405Io;
    dp->selectedUnit = 0;

    /*
    **  Only one card reader unit is possible per equipment.
    */
    if (dp->context[0] != NULL)
        {
        fprintf(stderr, "(cr405  ) Only one unit is possible per equipment\n");
        exit(1);
        }

    cc = calloc(1, sizeof(Cr405Context));
    if (cc == NULL)
        {
        fprintf(stderr, "(cr405  ) Failed to allocate context block\n");
        exit(1);
        }

    dp->context[0] = (void *)cc;

    threadParms = calloc(1, sizeof(fswContext));    //  Need to check for null result
    if (cc == NULL)
        {
        fprintf(stderr, "(cr405  ) Failed to allocate CR3447 FileWatcher Context block\n");
        exit(1);
        }

    threadParms->LoadCards = 0;
    strcat(tokenString, deviceName);
    xlateTable = strtok(tokenString, ",");
    crInput    = strtok(NULL, ",");
    crOutput   = strtok(NULL, ",");
    Auto       = strtok(NULL, ",");

    /*
    **  Process the Request for FileSystem Watcher
    */
    watchRequested = TRUE;     // Default = Run Filewatcher Thread
    if (Auto != NULL)
        {
        strlwr(Auto);
        if (!strcmp(Auto, "noauto"))
            {
            watchRequested = FALSE;
            }
        else if ((strcmp(Auto, "auto") != 0) && (strcmp(Auto, "*") != 0))
            {
            fprintf(stderr, "(cr405  ) Unrecognized Automation Type '%s'\n", Auto);
            exit(1);
            }
        }


    /*
    **  Setup character set translation table.
    */
    cc->table     = asciiTo026; // default translation table
    cc->isWatched = FALSE;

    cc->channelNo = channelNo;
    cc->eqNo      = eqNo;
    cc->unitNo    = unitNo;

    strcpy(cc->dirInput, "");
    strcpy(cc->dirOutput, "");

    if (xlateTable != NULL)
        {
        if (strcmp(xlateTable, "029") == 0)
            {
            cc->table = asciiTo029;
            }
        else if ((strcmp(xlateTable, "026") != 0)
                 && (strcmp(xlateTable, " *") != 0)
                 && (strcmp(xlateTable, " ") != 0))
            {
            fprintf(stderr, "(cr405  ) Unrecognized card code name %s\n", xlateTable);
            exit(1);
            }
        }

    /*
    **  Incorrect specifications for input / output directories
    **  are fatal.  Even though files can still be submitted
    **  through the operator interface, we want the parameters
    **  supplied through the ini file to be correct from the start.
    */

    if ((crOutput != NULL) && (crOutput[0] != '*'))
        {
        if (stat(crOutput, &s) != 0)
            {
            fprintf(stderr, "(cr405  ) The Output location specified '%s' does not exist.\n", crOutput);
            exit(1);
            }

        if ((s.st_mode & S_IFDIR) == 0)
            {
            fprintf(stderr, "(cr405  ) The Output location specified '%s' is not a directory.\n", crOutput);
            exit(1);
            }
#if defined(SAFECALLS)
        strcpy_s(threadParms->outDoneDir, sizeof(threadParms->outDoneDir), crOutput);
        strcpy_s(cc->dirOutput, sizeof(cc->dirOutput), crOutput);
#else
        strcpy(threadParms->outDoneDir, crOutput);
        strcpy(cc->dirOutput, crOutput);
#endif
        fprintf(stderr, "(cr405  ) Submissions will be preserved in '%s'.\n", crOutput);
        }
    else
        {
        threadParms->outDoneDir[0] = '\0';
        cc->dirOutput[0]           = '\0';
        fprintf(stderr, "(cr405  ) Submissions will be purged after processing.\n");
        }

    if ((crInput != NULL) && (crInput[0] != '*'))
        {
        if (stat(crInput, &s) != 0)
            {
            fprintf(stderr, "(cr405  ) The Input location specified '%s' does not exist.\n", crInput);
            exit(1);
            }

        if ((s.st_mode & S_IFDIR) == 0)
            {
            fprintf(stderr, "(cr405  ) The Input location specified '%s' is not a directory.\n", crInput);
            exit(1);
            }
        //  We only care about the "Auto" "NoAuto" flag if there is a good input location

        /*
        **  The thread needs to know what directory to watch.
        **
        **  The Card Reader Context needs to remember what directory
        **  will supply the input files so more can be found at EOD.
        */
#if defined(SAFECALLS)
        strcpy_s(threadParms->inWatchDir, sizeof(threadParms->inWatchDir), crInput);
        strcpy_s(cc->dirInput, sizeof(cc->dirInput), crInput);
#else
        strcpy(threadParms->inWatchDir, crInput);
        strcpy(cc->dirInput, crInput);
#endif

        threadParms->eqNo      = eqNo;
        threadParms->unitNo    = unitNo;
        threadParms->channelNo = channelNo;
        threadParms->LoadCards = cr405LoadCards;
        threadParms->devType   = DtCr405;

        /*
        **  At this point, we should have a completed context
        **  and should be ready to launch the thread and pass
        **  the context along.  We don't free the block, the
        **  thread will do that if it is launched correctly.
        */

        /*
        **  Now establish the filesystem watcher thread.
        **
        **  It is non-fatal if the filesystem watcher thread
        **  cannot be started.
        */
        cc->isWatched = FALSE;
        if (watchRequested)
            {
            cc->isWatched = fsCreateThread(threadParms);
            if (!cc->isWatched)
                {
                printf("(cr405  ) Unable to create filesystem watch thread for '%s'.\n", crInput);
                printf("          Card Loading is still possible via Operator Console.\n");
                }
            else
                {
                printf("(cr405  ) Filesystem watch thread for '%s' created successfully.\n", crInput);
                }
            }
        else
            {
            printf("(cr405  ) Filesystem watch thread not requested for '%s'.\n", crInput);
            printf("          Card Loading is required via Operator Console.\n");
            }
        }

    cc->col = 80;

    /*
    **  Print a friendly message.
    */
    printf("(cr405  ) Initialised on channel %o equipment %o type '%s'\n",
           channelNo,
           eqNo,
           cc->table == asciiTo026 ? "026" : "029");

    if (!cc->isWatched)
        {
        free(threadParms);
        }

    /*
    **  Link into list of 405 Card Reader units.
    */
    if (lastCr405 == NULL)
        {
        firstCr405 = cc;
        }
    else
        {
        lastCr405->nextUnit = cc;
        }

    lastCr405 = cc;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Load cards on 405 card reader.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cr405LoadCards(char *fname, int channelNo, int equipmentNo, FILE *out, char *params)
    {
    Cr405Context *cc;
    DevSlot      *dp;
    int          len;
    char         *sp;

    int numParam;

    static char str[_MAX_PATH];
    static char strWork[_MAX_PATH];
    static char fOldest[_MAX_PATH] = "";
    time_t      tOldest;

    struct stat   s;
    struct dirent *curDirEntry;

    DIR *curDir;

    /*
    **  Locate the device control block.
    */
    dp = channelFindDevice((u8)channelNo, DtCr405);
    if (dp == NULL)
        {
        return;
        }

    cc = (Cr405Context *)(dp->context[0]);

    /*
    **  Ensure the tray is not full.
    */
    if (((cc->inDeck + 1) % Cr405MaxDecks) == cc->outDeck)
        {
        printf("(cr405  ) Input tray full\n");

        return;
        }
    len = strlen(fname) + 1;
    sp  = (char *)malloc(len);
    memcpy(sp, fname, len);
    cc->decks[cc->inDeck] = sp;
    cc->inDeck            = (cc->inDeck + 1) % Cr405MaxDecks;

    if (dp->fcb[0] == NULL)
        {
        if (cr405StartNextDeck(dp, cc, out))
            {
            return;
            }
        }


    /*
    **  If the string for the filename = "*" Then we
    **  invoke the logic that selects the next file from
    **  the input directory (if one exists) in create
    **  date order.
    **
    **  The asterisk convention works even if the
    **  filewatcher thread cannot be started.  It
    **  simply means: "Pick the next oldest file
    **  found in the input directory.
    **
    **  For anything other than an asterisk, the
    **  string is assumed to be a filename.
    */
    if ((strcmp(str, "*") == 0) && (cc->dirInput[0] != '\0'))
        {
        curDir = opendir(cc->dirInput);

        /*
        **  Scan the input directory (if specified)
        **  for the oldest file queued.
        */

        do
            {
            curDirEntry = readdir(curDir);
            if (curDirEntry == NULL)
                {
                continue;
                }

            //  Pop over the dot (.) directories
            if (curDirEntry->d_name[0] == '.')
                {
                continue;
                }
#if defined(SAFECALLS)
            sprintf_s(strWork, sizeof(strWork), "%s/%s", cc->dirInput, curDirEntry->d_name);
#else
            sprintf(strWork, "%s/%s", cc->dirInput, curDirEntry->d_name);
#endif
            stat(strWork, &s);
            if (fOldest[0] == '\0')
                {
                strcpy(fOldest, strWork);
                tOldest = s.st_ctime;
                }
            else
                {
                if (s.st_ctime > tOldest)
                    {
                    strcpy(fOldest, strWork);
                    tOldest = s.st_ctime;
                    }
                }
            } while (curDirEntry != NULL);
        if (fOldest[0] != '\0')
            {
            printf("(cr405  ) Dequeueing Unprocessed File '%s'.\n", fOldest);
            strcpy(str, fOldest);
            }
        }

    if (stat(str, &s) != 0)
        {
        fprintf(stderr, "(cr405  ) The Input location specified '%s' does not exist.\n", str);

        return;
        }

    dp->fcb[0] = fopen(str, "r");

    /*
    **  Check if the open succeeded.
    */
    if (dp->fcb[0] == NULL)
        {
        printf("(cr405  ) Failed to open '%s'\n", str);

        return;
        }

    cr405NextCard(dp);

#if defined(SAFECALLS)
    strcpy_s(cc->curFileName, sizeof(cc->curFileName), str);
#else
    strcpy(cc->curFileName, str);
#endif

    printf("(cr405  ) Loaded with '%s'", str);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Show card reader status (operator interface).
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cr405ShowStatus(void)
    {
    Cr405Context *cp = firstCr405;

    if (cp == NULL)
        {
        return;
        }

    printf("\n    > Card Reader (cr405) Status:\n");

    while (cp)
        {
        printf("    > CH %02o EQ %02o UN %02o Col %02i Seq:%i File '%s'\n",
               cp->channelNo,
               cp->eqNo,
               cp->unitNo,
               cp->col,
               cp->seqNum,
               cp->curFileName);

        if (cp->isWatched)
            {
            printf("    >   Autoloading from '%s' to '%s'\n",
                   cp->dirInput,
                   cp->dirOutput);
            }

        cp = cp->nextUnit;
        }
    printf("\n");
    }

/*
 **--------------------------------------------------------------------------
 **
 **  Private Functions
 **
 **--------------------------------------------------------------------------
 */

#if !defined(_WIN32)
/*--------------------------------------------------------------------------
**  Purpose:        Convert String to Lower-Case
**
**  Parameters:     Name        Description.
**                  str         Character String
**
**  Returns:        Character String
**
**------------------------------------------------------------------------*/

//char* strlwr(char* str)
//{
//    unsigned char* p = (unsigned char*)str;
//
//    while (*p) {
//        *p = tolower((unsigned char)*p);
//        p++;
//    }
//
//    return str;
//}
//
#endif
/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on 405 card reader.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus cr405Func(PpWord funcCode)
    {
    switch (funcCode)
        {
    default:
        return (FcDeclined);

    case FcCr405Deselect:
    case FcCr405GateToSec:
        activeDevice->fcode = 0;

        return (FcProcessed);

    case FcCr405ReadNonStop:
    case FcCr405StatusReq:
        activeDevice->fcode = funcCode;
        break;
        }

    return (FcAccepted);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on 405 card reader.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cr405Io(void)
    {
    Cr405Context *cc = activeDevice->context[0];

    switch (activeDevice->fcode)
        {
    default:
    case FcCr405Deselect:
    case FcCr405GateToSec:
        break;

    case FcCr405StatusReq:
        if ((activeDevice->fcb[0] == NULL) && (cc->col >= 80))
            {
            activeChannel->data = StCr405NotReady;
            }
        else
            {
            activeChannel->data = StCr405Ready;
            }
        activeChannel->full = TRUE;
        break;

    case FcCr405ReadNonStop:
        /*
        **  Simulate card in motion for 20 major cycles.
        */
        if (cycles - cc->getCardCycle < 20)
            {
            break;
            }

        if (activeChannel->full)
            {
            break;
            }

        activeChannel->data = cc->card[cc->col++] & Mask12;
        activeChannel->full = TRUE;

        if (cc->col >= 80)
            {
            cr405NextCard(activeDevice);
            }

        break;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle channel activation.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cr405Activate(void)
    {
#if DEBUG
    fprintf(cr405Log, "\n(cr405  ) %06d PP:%02o CH:%02o Activate",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle disconnecting of channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cr405Disconnect(void)
    {
#if DEBUG
    fprintf(cr405Log, "\n(cr405  ) %06d PP:%02o CH:%02o Disconnect",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Start reading next card deck.
**
**  Parameters:     Name        Description.
**
**  Returns:        TRUE if the deck is successfully loaded.
**                  FALSE if not.
**
**------------------------------------------------------------------------*/
static bool cr405StartNextDeck(DevSlot *dp, Cr405Context *cc, FILE *out)
    {
    char *fname;

    while (cc->outDeck != cc->inDeck)
        {
        fname      = cc->decks[cc->outDeck];
        dp->fcb[0] = fopen(fname, "r");
        if (dp->fcb[0] != NULL)
            {
            cr405NextCard(dp);
            fprintf(out, "Cards loaded on card reader C%o,E%o\n", cc->channelNo, cc->eqNo);

            return TRUE;
            }
        fprintf(stderr, "Failed to open card deck %s\n", fname);
        unlink(fname);
        free(fname);
        cc->outDeck = (cc->outDeck + 1) % Cr405MaxDecks;
        }
    dp->fcb[0] = NULL;

    return FALSE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Read next card, update card reader status.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cr405NextCard(DevSlot *dp)
    {
    Cr405Context *cc = dp->context[0];
    static char  buffer[322];
    bool         binaryCard;
    char         *cp;
    char         c;
    int          value;
    int          i;
    int          j;

    static char fnwork[_MAX_PATH];
    bool        isFromInput;
    int         fnindex;

    if (dp->fcb[0] == NULL)
        {
        return;
        }

    /*
    **  Initialise read.
    */
    cc->getCardCycle = cycles;
    cc->col          = 0;
    binaryCard       = FALSE;

    /*
    **  Read the next card.
    */
    cp = fgets(buffer, sizeof(buffer), dp->fcb[0]);
    if (cp == NULL)
        {
        /*
        **  If the last card wasn't a 6/7/8/9 card, fake one.
        */
        if (cc->card[0] != 00017)
            {
            memset(cc->card, 0, sizeof(cc->card));
            cc->card[0] = 00017;
            }
        else
            {
            cc->col = 80;
            }

        fclose(dp->fcb[0]);
        dp->fcb[0] = NULL;

        printf("(cr405  ) End of Deck '%s' reached on channel %o equipment %o\n",
               cc->curFileName,
               dp->channel->id,
               dp->eqNo);

        /*
        **  If the current file comes from the "input" directory specified
        **      then
        **          if the output directory exists
        **              then we move it to the "output" directory if specified
        **              else we remove the file from the "input" directory
        **      else we leave it alone
        **
        **  clear the filename string.
        **
        **  If the "output" directory is specified, move the file
        **  to the "processed" state
        */
        isFromInput = (!strncmp(cc->curFileName, cc->dirInput, strlen(cc->dirInput)));
        if (isFromInput)
            {
            //  Files from the input directory are handled specially
            if (cc->dirOutput[0] == '\0')
                {
                //  Once a file is closed, we can simply delete it
                //  from the input directory.  This will also trigger
                //  the filechange watcher who will initiate the next
                //  file load automatically.

                remove(cc->curFileName);
                }
            else
                {
                //  perform the rename of the current file to the "Processed"
                //  directory.  This rename will ALSO trigger the filechange
                //  watcher.  Otherwise the operator will need to use the load
                //  command from the console.

                fnindex = 0;

                while (TRUE)
                    {
                    //  create the file's new name
#if defined(SAFECALLS)
                    sprintf_s(fnwork, sizeof(fnwork), "%s/%s_%04i", cc->dirOutput, cc->curFileName + strlen(cc->dirInput) + 1, fnindex);
#else
                    sprintf(fnwork, "%s/%s_%04i", cc->dirOutput, cc->curFileName + strlen(cc->dirInput) + 1, fnindex);
#endif
                    if (rename(cc->curFileName, fnwork) == 0)
                        {
                        printf("(cr405  ) Deck '%s' moved to '%s'. (Input Preserved)\n",
                               cc->curFileName + strlen(cc->dirInput) + 1,
                               fnwork);
                        break;
                        }
                    else
                        {
                        printf("(cr405  ) Rename Failure on '%s' - (%s). Retrying (%d)...\n",
                               cc->curFileName + strlen(cc->dirInput) + 1,
                               strerror(errno),
                               fnindex);
                        }
                    fnindex++;
                    } // while(TRUE)
                if (fnindex > 0)
                    {
                    printf("\n");
                    }
                } // else
            }     // if (isFromInput)

        cc->curFileName[0] = '\0';

        return;
        }

    /*
    **  Deal with special first-column codes.
    */
    if (buffer[0] == '~')
        {
        if (memcmp(buffer + 1, "eoi\n", 4) == 0)
            {
            /*
            **  EOI = 6/7/8/9 card.
            */
            memset(cc->card, 0, sizeof(cc->card));
            cc->card[0] = 00017;

            return;
            }

        if (memcmp(buffer + 1, "eof\n", 4) == 0)
            {
            /*
            **  EOF = 6/7/9 card.
            */
            memset(cc->card, 0, sizeof(cc->card));
            cc->card[0] = 00015;

            return;
            }

        if (memcmp(buffer + 1, "eor\n", 4) == 0)
            {
            /*
            **  EOR = 7/8/9 card.
            */
            memset(cc->card, 0, sizeof(cc->card));
            cc->card[0] = 00007;

            return;
            }

        if (memcmp(buffer + 1, "bin", 3) == 0)
            {
            /*
            **  Binary = 7/9 card.
            */
            binaryCard  = TRUE;
            cc->card[0] = 00005;
            }
        }

    /*
    **  Convert cards.
    */
    if (!binaryCard)
        {
        /*
        **  Skip over any characters past column 80 (if line is longer).
        */
        if ((cp = strchr(buffer, '\n')) == NULL)
            {
            do
                {
                c = fgetc(dp->fcb[0]);
                } while (c != '\n' && c != EOF);
            cp = buffer + 80;
            }

        /*
        **  Blank fill line shorter then 80 characters.
        */
        for ( ; cp < buffer + 80; cp++)
            {
            *cp = ' ';
            }

        /*
        **  Convert ASCII card.
        */
        for (i = 0; i < 80; i++)
            {
            cc->card[i] = cc->table[buffer[i]];
            }
        }
    else
        {
        /*
        **  Skip over any characters past column 320 (if line is longer).
        */
        if ((cp = strchr(buffer, '\n')) == NULL)
            {
            do
                {
                c = fgetc(dp->fcb[0]);
                } while (c != '\n' && c != EOF);
            cp = buffer + 320;
            }

        /*
        **  Zero fill line shorter then 320 characters.
        */
        for ( ; cp < buffer + 320; cp++)
            {
            *cp = '0';
            }

        /*
        **  Convert binary card (79 x 4 octal digits).
        */
        cp = buffer + 4;
        for (i = 1; i < 80; i++)
            {
            value = 0;
            for (j = 0; j < 4; j++)
                {
                if ((cp[j] >= '0') && (cp[j] <= '7'))
                    {
                    value = (value << 3) | (cp[j] - '0');
                    }
                else
                    {
                    value = 0;
                    break;
                    }
                }

            cc->card[i] = value;

            cp += 4;
            }
        }
    }


/*---------------------------  End Of File  ------------------------------*/

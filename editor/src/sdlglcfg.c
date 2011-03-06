/*******************************************************************************
*  SDLGLCFG.C                                                                  *
*      - Read procedures for the configuration of SDLGL                        *
*                                                                              *
*  FREE SPACE COLONISATION                                                     *
*      (c)2005 Paul Mueller <pmtech@swissonline.ch>                            *
*                                                                              *
*  This program is free software; you can redistribute it and/or modify        *
*  it under the terms of the GNU General Public License as published by        *
*  the Free Software Foundation; either version 2 of the License, or           *
*  (at your option) any later version.                                         *
*                                                                              *
*  This program is distributed in the hope that it will be useful,             *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of              *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               *
*  GNU Library General Public License for more details.                        *
*                                                                              *
*  You should have received a copy of the GNU General Public License           *
*  along with this program; if not, write to the Free Software                 *
*  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  *
*******************************************************************************/

/*******************************************************************************
* INCLUDES                                 				                       *
*******************************************************************************/

#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <stdlib.h>      /* malloc(), free(), don't use alloc.h it's obsolete*/

#include "platform.h"
#include "sdlglcfg.h"

/*******************************************************************************
* DEFINES                             				                           *
*******************************************************************************/

#define SDLGLCFG_LINELEN  256

#define SDLGLCFG_BLOCKSIGN_START 0
#define SDLGLCFG_BLOCKSIGN_END   1
#define SDLGLCFG_COMMENT_SIGN    2

#define EGOBOO_FILE_CREATED "// File generated by EGOBOO - Map Editor V 0.1.5\n"


/*******************************************************************************
* DATA                                				                           *
*******************************************************************************/

static FILE *TextFile;
static char BlockName[SDLGLCFG_LINELEN];     /* Holds the name of a block, if any */
static char BlockSigns[4];

/*******************************************************************************
* CODE                                				                           *
*******************************************************************************/

/*
 * Name:
 *     sdlglcfgGetValidLine
 * Description:
 *     Gets a line from the textfile. Comments ( ";" will be stripped ).
 *     The returned line holds at least one char.
 * Input:
 *      f *:      This file
 *      data *:   Where to put the line, must be 254 or more chars long!
 *      bufsize:  Size of buffer for line
 *      com_sign: Signs used for comment
 * Output:
 *      Is a valid line, yes/no   0: No more line available
 */
static int sdlglcfgGetValidLine(FILE *f, char *data, int bufsize, char com_sign)
{

    char *outstr,
         *pcomment;


    do {

    	outstr = fgets(data, bufsize, f);

    	if (outstr == 0) {		/* EOF reached or error */

            fclose(f);
            return 0;

    	}

    	/* Look for comments: */
        pcomment = strchr(outstr, com_sign);
        if (pcomment != 0) {

            *pcomment = 0;

        }
        else {

            /* Remove carriage-return */
            pcomment = strchr(outstr,'\n');
            if (pcomment != 0) {

                *pcomment = 0;

            }

        }

        if (*outstr == 0) {		/* Empty string */

            outstr = 0;

        }
        else {

            return 1;

        }

    }
    while (outstr == 0);

    return 0;

}

/*
 * Name:
 *     sdlglcfgStrToVal
 * Description:
 *     Convertes the given string to a value, based on given 'valtype' 
 * Input:
 *      string *: To change to value
 *      valtype:  This kind of value is in given string
 *      value *:  Where to return the value
 *      vallen:   length, if string.
 */
static void sdlglcfgStrToVal(char *string, char valtype, void *value, char vallen)
{

    int   ivalue;
    float fvalue;
    char *pstr;


    if (string[0] == 0) {

        return;

    }

    if (valtype == SDLGLCFG_VAL_STRING || valtype == SDLGLCFG_VAL_ONECHAR) {

        pstr = (char *)value;

        /* Remove trailing spaces: 2010-01-02 <bitnapper> */
        while (*string == ' ') {

            string++;

        }

        if (valtype == SDLGLCFG_VAL_STRING) {

            strncpy(pstr, string, vallen);
            pstr[vallen] = 0;
            /* --- Remove underlines for editing --- */
            while(*pstr) {
            
                if (*pstr == '_') {
                    *pstr = ' ';
                }
                
                pstr++;
                
            }

        }
        else {

            *pstr = *string;

        }

    }
    else {

        /* TODO: (2008-06-20) : Support an array of values,  if 'vallen' is given ==> do while (vallen > 0) */
        /* It's a value */
        sscanf(string, "%d", &ivalue);
        switch(valtype) {

            case SDLGLCFG_VAL_CHAR:
                *(char *)value = (char)ivalue;
                break;

            case SDLGLCFG_VAL_UCHAR:
                *(unsigned char *)value = (unsigned char)ivalue;
                break;

            case SDLGLCFG_VAL_SHORT:
                *(short int *)value = (short int)ivalue;
                break;

            case SDLGLCFG_VAL_USHORT:
                *(unsigned short int *)value = (unsigned short int)ivalue;
                break;

            case SDLGLCFG_VAL_INT:
                *(int *)value = (int)ivalue;
                break;

            case SDLGLCFG_VAL_UINT:
                *(unsigned int *)value = (unsigned int)ivalue;
                break;

            case SDLGLCFG_VAL_FLOAT:
                sscanf(string, "%f", &fvalue);
                *(float *)value = fvalue;
                break;

        }

    }

}

/*
 * Name:
 *     sdlglcfgValToStr
 * Description:
 *     Converts the given value to a string
 * Input:
 *      val_str *: Where to return the string
 *      valtype:   This kind of value is in given value
 *      value *:   Value to convert to string
 *      vallen:    length, if string.
 */
static void sdlglcfgValToStr(char *val_str, char valtype, void *value, char vallen)
{

    switch(valtype) {

        case SDLGLCFG_VAL_ONECHAR:
            vallen = 1;
        case SDLGLCFG_VAL_NONE:
        case SDLGLCFG_VAL_STRING:
            strncpy(val_str, (char *)value, vallen);
            val_str[vallen] = 0;
            break;

        case SDLGLCFG_VAL_CHAR:
            sprintf(val_str, "%d", (int)*(char *)value);
            break;

        case SDLGLCFG_VAL_SHORT:
            sprintf(val_str, "%d", (int)*(short int *)value);
            break;

        case SDLGLCFG_VAL_INT:
            sprintf(val_str, "%d", *(int *)value);
            break;

        case SDLGLCFG_VAL_UCHAR:
            sprintf(val_str, "%u", (int)*(unsigned char *)value);
            break;

        case SDLGLCFG_VAL_USHORT:
            sprintf(val_str, "%u", (int)*(unsigned short int *)value);
            break;

        case SDLGLCFG_VAL_UINT:
            sprintf(val_str, "%u", *(int *)value);
            break;

        case SDLGLCFG_VAL_FLOAT:
            sprintf(val_str, "%.5f", *(float *)value);
            break;

        default:
            /* Return empty string, if unknown */
            val_str[0] = 0;
            break;

    }

}

/*
 * Name:
 *     sdlglcfgGetNamedValue
 * Description:
 *     Gets a line from the textfile. Comments ( ";" will be stripped ).
 *     The returned line holds at least one char.
 * Input:
 *      line *:    Line to get the value from
 *      vallist *: Holding all possible values with names
 */
static void sdlglcfgGetNamedValue(char *line, SDLGLCFG_NAMEDVALUE *vallist)
{

    char valstr[128];
    char namestr[128];
    char *pval;


    /* FIXME: Check for blocksign and return it, if needed */

    /* -------- */
    pval = strchr(line, '=');
    if (pval) {       /* Found an equal sign */

        *pval = 0;      /* Scan name later          */
        pval++;
        sscanf(line, "%s", namestr);
        sscanf(pval, "%s", valstr);
        
        if ( strcmp( pval, "" ) != 0 ) {
            
            strlwr(namestr); /* To lower for comparision */

            while (vallist -> type) {

                if (! strcmp(namestr, vallist -> name)) {

                    sdlglcfgStrToVal(valstr, vallist -> type, vallist -> data, vallist -> len);

                }

                vallist++;

            }  /* while (vallist -> type) */


        } /* if (pval) */

    }

}

/*
 * Name:
 *     sdlglcfgRecordFromLine
 * Description:
 *     Reads a 'record' from given 'line'. It's assumed that the values
 *     are delimited by a comma.
 * Input:
 *     line *:    Line to read values from
 *     rcf *:     Description of values on given line to read from
 *     fixedpos:  Values have a fixed position in line given in value-lenght
 *     delimiter: Delimiter to use 
 */
static void sdlglcfgRecordFromLine(char *line, SDLGLCFG_VALUE *rcf, int fixedpos, char delimiter)
{

    char *pbuf, *pbrk;
    char delimit_str[2];


    delimit_str[0] = delimiter;
    delimit_str[1] = 0;
    
    strcat(line, delimit_str);      /* Just in case it lacks... : Delimiter for last value in string */
    pbuf = &line[0];

    /* Scan trough all descriptors */
    while(rcf -> type > 0) {

        if (fixedpos) {
            
            /* Fixed column positions */
            sdlglcfgStrToVal(pbuf + rcf -> pos, rcf -> type, rcf -> data, rcf -> len);
        
        }
        else  {

            /* Remove leading spaces */
            while(*pbuf == ' ' || *pbuf == '\t') {
                pbuf++;
            }
            /* data in 'buffer' is decomma-limited  */
            pbrk = strchr(pbuf, delimiter);
            if (pbrk) {

                *pbrk = 0;          /* End of string to scan value from */
                sdlglcfgStrToVal(pbuf, rcf -> type, rcf -> data, rcf -> len);
                pbuf = &pbrk[1];    /* Next value                      */

            }
            else {

                /* No value anymore, bail out */
                break;

            }
            
        }
        
        rcf++;

    }    /*  while(rcf -> type > 0)  */

}

/*
 * Name:
 *     sdlglcfgCheckBlockName
 * Description:
 *     Checks if given line holds a block start.
 * Input:
 *     line *:     Pointer on line to check
 *  Output:
 *     Pointer on name of block, if any is found
 */
static int sdlglcfgCheckBlockName(char *line)
{

    char *pbs;


    BlockName[0] = 0;

    /* Get the name of a block, if any  */
    if (BlockSigns[0]) {

        pbs = strchr(line, BlockSigns[0]);
        if (pbs) {

            pbs++;
            line++;

            if (BlockSigns[1]) {

                /* Get rid of second blocksign, if asked for */

                pbs = strchr(pbs, BlockSigns[1]);
                if (pbs) {

                    *pbs = 0;

                }

            }

            sscanf(line, "%s", BlockName);
            strlwr(BlockName);      /* To lower for comparision of names */

            return 1;
        
        }

    } /* if (blocksigns[0]) */

    return 0;

}

/*
 * Name:
 *     sdlglcfgFindNextBlock
 * Description:
 *     Finds the next block in the actual text file
 * Input:
 *     None
 */
static int sdlglcfgFindNextBlock(void)
{

    char line[SDLGLCFG_LINELEN];
    

    while (sdlglcfgGetValidLine(TextFile, line, SDLGLCFG_LINELEN - 2, ';')) {

        /* Read the type of configuration */
        if (sdlglcfgCheckBlockName(line)) {

            return 1;
            
        }

    }
    
    return 0;

}

/*
 * Name:
 *     sdlglcfgReadEgoboo
 * Description:
 *     Special function to read egoboo files SPAWN.TXT and PASSAGE.TXT
 * Input:
 *     fname *:    Name of file to read 
 *     lineinfo *: Pointer on description how to read the data
 */
static void sdlglcfgReadEgoboo(char *fname, SDLGLCFG_LINEINFO *lineinfo)
{

    FILE *f;
    char line[SDLGLCFG_LINELEN];
    char *pbaserec, *pactrec;
    int  maxrec;
    char *pbrk;  
    
    
    pbaserec = (char *)lineinfo -> recdata;
    pactrec  = pbaserec + lineinfo -> recsize;
    maxrec   = lineinfo -> maxrec;
    
    /* -------- Clear the buffer, possible empty file --- */
    memset(pbaserec, 0, 2 * lineinfo -> recsize);
        
    f = fopen(fname, "rt");
    if (f) {    
        
        while (sdlglcfgGetValidLine(f, line, SDLGLCFG_LINELEN - 2, '/')) {
      
            if (maxrec > 0) {   /* if not filled yet */
            
                pbrk = strchr(line, ':');
                if (pbrk) {
            
                    *pbrk = 0;
                    /* Copy the description */
                    strncpy(pbaserec, line, lineinfo -> rcf[0].len);
                    pbaserec[lineinfo -> rcf[0].len] = 0;

                    sdlglcfgRecordFromLine(&pbrk[1], &lineinfo -> rcf[1], 0, ' ');
                    /* Copy data to actual rec */
                    memcpy(pactrec, pbaserec, lineinfo -> recsize);
                    
                    pactrec += lineinfo -> recsize;
               
                    maxrec--;
                    
                }
                
            }
     
        }
        
        fclose(f);
        
    }

}

/*
 * Name:
 *     sdlglcfgWriteEgoboo
 * Description:
 *     Special function to write egoboo-style files SPAWN.TXT and PASSAGE.TXT
 * Input:
 *     fname *:    Name of file to write
 *     lineinfo *: Pointer on description how to read the data
 */
static void sdlglcfgWriteEgoboo(char *fname, SDLGLCFG_LINEINFO *lineinfo)
{

    FILE *f;
    char line[SDLGLCFG_LINELEN];
    SDLGLCFG_VALUE *rcf;
    char *pbaserec, *pactrec;
    char val_str[200];
     
    
    f = fopen(fname, "wt");
    if (f) {

        fprintf(f, EGOBOO_FILE_CREATED);
        
        pbaserec = (char *)lineinfo -> recdata;
        pactrec  = pbaserec + lineinfo -> recsize;

        while(pactrec[0] != 0)  {

            /* Write the value before the colon */
            memcpy(pbaserec, pactrec, lineinfo -> recsize);

            sprintf(line, "%s\t:\t", pbaserec);

            rcf = &lineinfo -> rcf[1];

            /* Scan trough all descriptors */
            while(rcf -> type > 0) {

                sdlglcfgValToStr(val_str, rcf -> type, rcf -> data, rcf -> len);

                strcat(line, val_str);
                strcat(line, "\t");
                
                rcf++;
                                
            }  /* while(rcf -> type > 0) */
            
            /* Print the line */
            fprintf(f, "%s\n", line);
            
            pactrec += lineinfo -> recsize;
        
        }
        
        fputs("\r\n\r\n", f);
        
        fclose(f);
                
    }  
    
}

/* ========================================================================== */
/* ========================= PUBLIC FUNCTIONS =============================== */
/* ========================================================================== */

/*
 * Name:
 *     sdlglcfgOpenFile
 * Description:
 *     Opens given file as textfile for reading in line by line
 * Input:
 *     filename *: Name of file to read in
 *     blocksigns: Start and (possible) end of 'blockname'
 */
int sdlglcfgOpenFile(char *filename, char blocksigns[4])
{

    TextFile = fopen(filename, "rt");
    if (TextFile) {

        if (blocksigns[0]) {

            /* Save this block-signs for later use with other functions */
            BlockSigns[0] = blocksigns[0];
            BlockSigns[1] = blocksigns[1];
            BlockSigns[2] = blocksigns[2];
            BlockSigns[3] = blocksigns[3];

            if (BlockSigns[1] == ' ') {

                BlockSigns[1] = 0;

            }

            return sdlglcfgFindNextBlock();

        }
        else {

            return 1;

        }

    }

    return 0;

}

/*
 * Name:
 *     sdlglcfgSkipBlock
 * Description:
 *     Skips actual block
 * Input:
*       None
 */
int sdlglcfgSkipBlock(void)
{

    return sdlglcfgFindNextBlock();

}

/*
 * Name:
 *     sdlglcfgIsActualBlockName
 * Description:
 *     Returns TRUE if given name is actual block name
 * Input:
 *     name *:  Name to check
 * Output:
 *     Is this name yes/no
 */
int sdlglcfgIsActualBlockName(char *name)
{

    strlwr(name);       /* to lower for comparision */

    if (strcmp(name, BlockName) == 0) {

        return 1;
        
    }
    
    return 0;

}

/*
 * Name:
 *     sdlglcfgReadNamedValues
 * Description:
 *     Opens given file as textfile for reading in line by line
 * Input:
 *     vallist *:  Named values to look for
  * Output:
 *     Name of new block, if any
 */
int sdlglcfgReadNamedValues(SDLGLCFG_NAMEDVALUE *vallist)
{

    char line[SDLGLCFG_LINELEN];


    while (sdlglcfgGetValidLine(TextFile, line, SDLGLCFG_LINELEN -  2, ';')) {

        if (sdlglcfgCheckBlockName(line)) {       /* Start of a new block */

            return 1;

        }
        else {

            sdlglcfgGetNamedValue(line, vallist);
            return 1;

        }

    }

    return 0;

}

/*
 * Name:
 *     sdlglcfgReadValues
 * Description:
 *     Reads in values line by line until next blocksign is reached
 * Input:
 *     vallist *:  Values to read in
 * Output:
 *     New block to read yes/no
 */
int sdlglcfgReadValues(SDLGLCFG_VALUE *vallist)
{

    char line[SDLGLCFG_LINELEN];


    while (sdlglcfgGetValidLine(TextFile, line, SDLGLCFG_LINELEN -  2, ';')) {if (sdlglcfgCheckBlockName(line)) {       /* Start of a new block */

            return 1;

        }
        else {

            if (vallist -> type > 0) {  /* Only if there is something left to read */
            
                sdlglcfgStrToVal(line, vallist -> type, vallist -> data, vallist -> len);
                vallist++;              /* Get next value */
                
            }

        }

    }

    return 0;

}

/*
 * Name:
 *     sdlglcfgReadRecordLines
 * Description:
 *
 *     Opens given file as textfile for reading in line by line
 * Input:
 *     lineinfo *: Info about the values in line
 *     fixedpos:   Values have a fixed position in line given in value-lenght
 * Output:
 *     New block to read yes/no
 */
int sdlglcfgReadRecordLines(SDLGLCFG_LINEINFO *lineinfo, int fixedpos)
{

    char line[SDLGLCFG_LINELEN];
    char *pbaserec, *pactrec;
    int maxrec;


    pbaserec = (char *)lineinfo -> recdata;
    pactrec  = pbaserec + lineinfo -> recsize;
    maxrec   = lineinfo -> maxrec;

    while (sdlglcfgGetValidLine(TextFile, line, SDLGLCFG_LINELEN - 2, ';')) {

        if (sdlglcfgCheckBlockName(line)) {       /* Start of a new block */

            return 1;

        }
        else {

            if (maxrec > 0) {   /* if not filled yet */

                sdlglcfgRecordFromLine(line, lineinfo -> rcf, fixedpos, ',');
                /* Copy data to actial rec */
                memcpy(pactrec, pbaserec, lineinfo -> recsize);

                pactrec += lineinfo -> recsize;
                
                maxrec--;

            }

        }

    }

    return 0;   /* End of file */

}

/*
 * Name:
 *     sdlglcfgReadStrings
 * Description:
 *     Reads in strings, line by line until next block sign
 *     Closes actual text file, if open.
 * Input:
 *     buffer *:   Where to put the strings
 *     bufsize *:  Size of buffer for strings
 */
int sdlglcfgReadStrings(char *buffer, int bufsize)
{

    char line[SDLGLCFG_LINELEN];
    char *ptarget;
    int  slen, spaceleft;


    buffer[0] = 0;
    ptarget = buffer;

    while (sdlglcfgGetValidLine(TextFile, line, SDLGLCFG_LINELEN - 2, ';')) {

        if (sdlglcfgCheckBlockName(line)) {       /* Start of a new block */

            return 1;

        }
        else {

            /* Check if there is space left for the new string */
            slen = strlen(line);
            spaceleft = bufsize - (ptarget - buffer);

            if (spaceleft > (slen + 1)) {

                /* We can copy the string to the buffer */
                strcpy(ptarget, line);
                ptarget = strchr(ptarget, 0) + 1;

            }

        }

    }

    return 0;   /* End of file */

}

/*
 * Name:
 *     sdlglcfgCloseFile
 * Description:
 *     Closes actual text file, if open.
 * Input:
 *     None
 */
void sdlglcfgCloseFile(void)
{

    if (TextFile) {

        fclose(TextFile);

    }

}

/*
 * Name:
 *     sdlglcfgReadSimple
 * Description:
 *     Read a simple config file with named values
 * Input:
 *     filename *: Name of file to read in
 *     vallist  *: List of named values
 */
void sdlglcfgReadSimple(char *filename, SDLGLCFG_NAMEDVALUE *vallist)
{

    FILE *f;
    char line[SDLGLCFG_LINELEN];


    f = fopen(filename, "rt");
    if (f) {

        while (sdlglcfgGetValidLine(f, line, SDLGLCFG_LINELEN - 2, ';')) {

            sdlglcfgGetNamedValue(line, vallist);

        }

        fclose(f);

    }

}

/*
 * Name:
 *     sdlglcfgLoadFile
 * Description:
 *     Loads a complete files as one block into an dynamically allocated buffer
 * Input:
 *     dir_name *: Pointer on name of directory
 *     fdesc *: Pointer on a 'SDLGLCFG_FILE' description
 *
 */
void sdlglcfgLoadFile(char *dir_name, SDLGLCFG_FILE *fdesc)
{

    char file_name[512];
    FILE *f;
    

    sprintf(file_name, "%s/%s", dir_name, fdesc -> filename);

    if (file_name[0] > 0) {

        f = fopen(file_name, "rt");
        
        if (f) {

            fseek(f, 0, SEEK_END);

            fdesc -> size = ftell(f);

            if (fdesc -> size > 0) {
            
                fdesc -> buffer = (char *)malloc(fdesc -> size + 100);  /* Including some reserve */
                
                if (fdesc -> buffer != NULL) {

                    fseek(f, 0, SEEK_SET);
                    fread(fdesc -> buffer, fdesc -> size, 1, f); 
                    /* Sign end of buffer */
                    fdesc -> buffer[fdesc -> size] = 0;
                    
                }
                
            }
            
            fclose(f);
        
        }

    }

}

/*
 * Name:
 *     sdlglcfgFreeFile
 * Description:
 *     Releases the buffer of given file
 * Input:
 *     fdesc *: Pointer on an array of 'SDLGLCFG_FILE' descriptions
 */
void sdlglcfgFreeFile(SDLGLCFG_FILE *fdesc)
{

    while(fdesc -> filename[0] > 0) {
    
        if (fdesc -> buffer != NULL) {

            free(fdesc -> buffer);
            
        }
        
        fdesc++;
    
    }

}

/*
 * Name:
 *     sdlglcfgEgobooRecord
 * Description:
 *     Special function to read/write egoboo-style files SPAWN.TXT and PASSAGE.TXT
 *     ( One Record per line) 
 * Input:
 *     fname *:    Name of file to read 
 *     lineinfo *: Pointer on description how to read the data
 *     write:      Write it, yes / no  
 */
void sdlglcfgEgobooRecord(char *fname, SDLGLCFG_LINEINFO *lineinfo, int write)
{

    if (write) {
        sdlglcfgWriteEgoboo(fname, lineinfo);
    }
    else {
        sdlglcfgReadEgoboo(fname, lineinfo);
    }

}

/*
 * Name:
 *     sdlglcfgEgobooValues
 * Description:
 *     Special function to read/write egoboo-style files with values behind ':'
 *     ( One Value per line) 
 * Input:
 *     fname *:   Name of file to read / write
 *     vallist *: Pointer on list of values and it's description for read / write
 *     write:     Write it, yes / no  
 */
void sdlglcfgEgobooValues(char *fname, SDLGLCFG_NAMEDVALUE *vallist, int write)
{

    FILE *f;
    char line[SDLGLCFG_LINELEN];
    char *pcolon;

    
    if (write) {
    
        f = fopen(fname, "wt");
        if (f) {
        
            fputs(EGOBOO_FILE_CREATED, f);

            while(vallist -> type > 0) {

                if (vallist -> type == SDLGLCFG_VAL_LABEL) {
                
                     fprintf(f, "%s\n", vallist -> name);
                     
                }
                else {
                
                    sdlglcfgValToStr(line, vallist -> type, vallist -> data, vallist -> len);
                    if (! vallist -> name || vallist -> name == "") {
                    
                        fprintf(f, " : %s\n", line);

                    }
                    else {

                        fprintf(f, "%-30s : %s\n", vallist -> name, line);

                    }
                    
                }

                vallist++;

            }

            fputs("\n\n", f);
            fclose(f);

        }

    }
    else {

        f = fopen(fname, "rt");
        if (f) {

            while (sdlglcfgGetValidLine(f, line, SDLGLCFG_LINELEN - 2, '/')) {

                if (vallist -> type == 0) {
                    /* More values in file then in asked for */
                    break;

                }
                else if (vallist -> type == SDLGLCFG_VAL_LABEL) {

                    /* Ignore this line */
                    vallist++;

                }
                
                pcolon = strchr(line, ':');

                if (pcolon) {

                    pcolon++;

                    sdlglcfgStrToVal(pcolon, vallist -> type, vallist -> data, vallist -> len);

                    vallist++;      /* Read next value */

                }

            }

            fclose(f);

        }

    }
}
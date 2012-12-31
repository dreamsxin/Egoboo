/*******************************************************************************
*  MISC.C                                                                  *
*    - EGOBOO-Editor                                                           *     
*                                                                              *
*    - Functions for the random treausre table                                 *
*      (c)2012 Paul Mueller <muellerp61@bluewin.ch>                            *
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
* INCLUDES								                                       *
*******************************************************************************/

// #include <stdio.h>
#include <stdlib.h>     /* rand()       */
#include <string.h>
#include <time.h>

#include "editfile.h"       /* Get filepath name by type                    */ 
#include "sdlglcfg.h"       /* Read egoboo text files eg. passage, spawn    */

#include "misc.h"           /* Own header   */

/*******************************************************************************
* DEFINES								                                       *
*******************************************************************************/

#define MAX_TABLES	  32
#define MAX_TREASURE 300
#define MAX_STRLEN    31

/*******************************************************************************
* TYPEDEFS 								                                       *
*******************************************************************************/

typedef struct {

    char line_name[2];              /* Dummy for empty name                           */
    char obj_name[MAX_STRLEN + 1];  /* Name of object to load, "-": Deleted in editor */

} TABLE_T;

typedef struct {
    char *table_name;       /* Pointer on table-name                        */
    int  idx_no[2];         /* Line number of first and last object in list */
} TABLE_LIST_T;

/*******************************************************************************
* DATA									                                       *
*******************************************************************************/

static TABLE_LIST_T TreasureTableList[MAX_TABLES + 2];
static TABLE_T TreasureTable[MAX_TREASURE + 2];

static SDLGLCFG_VALUE TreasureVal[] =
{
	{ SDLGLCFG_VAL_STRING, TreasureTable[0].line_name, 1 },
	{ SDLGLCFG_VAL_STRING, TreasureTable[0].obj_name, MAX_STRLEN},
	{ 0 }
};

static SDLGLCFG_LINEINFO TreasureRec =
{
    &TreasureTable[0],          /* Read buffer */
	&TreasureTable[1],          /* Record buffer    */
	MAX_TREASURE,
	sizeof(TABLE_T),
	&TreasureVal[0]
};

/*******************************************************************************
* CODE									                                       *
*******************************************************************************/

/* ========================================================================== */
/* ============================= PUBLIC FUNCTION(S) ========================= */
/* ========================================================================== */

/* =============== Random values ============= */

/*
 * Name:
 *     miscSetRandSeed
 * Description:
 *     Sets the seed for the random generator. If 'seed' is 0, then the
 *     current system time is used as seed.
 * Input:
 *     seed: Seed
 */
void miscSetRandSeed(int seed)
{
    if (! seed) {

        seed = time(0);

    }

    srand(seed);
}

/*
 * Name:
 *     miscRandRange
 * Description:
 *     Returns a random in the range ipair[0] .. ipair[1]
 * Input:
 *     ipair[] : Minimum and maximum value
 * Output:
 *     Generated random number (min .. max)
 */
int miscRandRange(int ipair[2])
{
    if (ipair[1] < 2 || ipair[0] > ipair[1]) {

        if (ipair[1] < 0) {

            ipair[1] = 0;

        }

        return ipair[1];

    }

    return (int)((((long)rand()*(ipair[1]))/(RAND_MAX+1)) + ipair[0]);
}

/*
 * Name:
 *     miscRandVal
 * Description:
 *     Returns a random number between 1 and RAND_MAX.
 * Input:
 *     None
 * Output:
 *     Generated random number (1 .. RAND_MAX)
 */
int miscRandVal(void)
{

    return rand();

}


/*
 * Name:
 *     miscLoadRandTreasure
 * Description:
 *     Loads the table for the random treasures
 * Input:
 *     Nome
 */
void miscLoadRandTreasure(void)
{
    char *fname;
    int num_table, num_el;


    fname = editfileMakeFileName(EDITFILE_BASICDATDIR, "randomtreasure.txt");

    // Read the table
    sdlglcfgEgobooRecord(fname, &TreasureRec, 0);
    // Record 0 is always empty
    TreasureTable[0].obj_name[0] = 0;

    // Now create the table for random treasure
  	// Load each treasure table
	num_table = 0;
    num_el = 1;

    while(TreasureTable[num_el].obj_name[0] != 0)
    {
        if(num_table >= MAX_TABLES)
        {
            // Too much treasure tables
            break;
        }

        // Starts with table-name
        TreasureTableList[num_table].table_name = TreasureTable[num_el].obj_name;

        // Point on first object in list
        num_el++;
        TreasureTableList[num_table].idx_no[0] = num_el;     // Number of first object in list

        while(TreasureTable[num_el].obj_name[0] != 0 && strcmp(TreasureTable[num_el].obj_name, "END" ) != 0)
        {
            // It's an object name
            // All objects have lower-case names for loading
            strlwr(TreasureTable[num_el].obj_name);
            // Count it
            num_el++;
        }

        // Save number of last element in list (one before "END)
        TreasureTableList[num_table].idx_no[1] = num_el - 1;
        // Start of next table
        num_table++;
        // skip "END"
        num_el++;
    }
}

/* =============== Random treasure ============= */

/*
 * Name:
 *     miscGetRandTreasure
 * Description:
 *     Gets the name for a random treasure object from the table 
 * Input:
 *     pname *: Pointer on name of random treasure (including %-sign for random names)
 * Output:
 *     Pointer of the object name for the random treasure 
 */
char *miscGetRandTreasure(char *pname)
{
    int i;
    int treasure_index;
   
    
    //Trap invalid strings
	if(NULL == pname)
    {
        // Return an empty object name
        return TreasureTable[0].obj_name;
    }
    
    // Skip the '%'-sign, if needed
    if('%' == pname[0])
    {
        pname++;
    }
    else
    {
        return TreasureTable[0].obj_name;
    }

    //Iterate through every treasure table until we find the one we want
	for(i = 0; i < MAX_TABLES; i++ )
	{
        if(TreasureTableList[i].table_name[0] != 0)
        {
            if(strcmp(TreasureTableList[i].table_name, pname) == 0)
            {
                // We found a name
        		//Pick a random number between 0 and the length of the table to get a random element out of the array
                treasure_index = miscRandRange(TreasureTableList[i].idx_no);

                pname = TreasureTable[treasure_index].obj_name;

        		//See if it is an actual random object or a reference to a different random table
        		if('%' == pname[0])
                {
                    // Is a random treasure
                    pname = miscGetRandTreasure(pname);
                }

                return pname;
            }
		}
        else
        {
            break;
        }
	}

    // If not found, than it's an empty string */
    return TreasureTable[0].obj_name;
}
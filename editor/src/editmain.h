/*******************************************************************************
*  EDITMAIN.H                                                                  *
*	- Main edit functions for map and other things      	                   *
*									                                           *
*   Copyright (C) 2010  Paul Mueller <pmtech@swissonline.ch>                   *
*                                                                              *
*   This program is free software; you can redistribute it and/or modify       *
*   it under the terms of the GNU General Public License as published by       *
*   the Free Software Foundation; either version 2 of the License, or          *
*   (at your option) any later version.                                        *
*                                                                              *
*   This program is distributed in the hope that it will be useful,            *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of             *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
*   GNU Library General Public License for more details.                       *
*                                                                              *
*   You should have received a copy of the GNU General Public License          *
*   along with this program; if not, write to the Free Software                *
*   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. *
*                                                                              *
*                                                                              *
* Last change: 2008-06-21                                                      *
*******************************************************************************/

#ifndef _EDITMAIN_H_
#define _EDITMAIN_H_

/*******************************************************************************
* INCLUDES								                                       *
*******************************************************************************/

/*******************************************************************************
* DEFINES								                                       *
*******************************************************************************/

#define EDITMAIN_DRAWMAP    0
#define EDITMAIN_NEWMAP     1
#define EDITMAIN_LOADMAP    2
#define EDITMAIN_SAVEMAP    3

/* ---------- Edit-Flags -------- */
#define EDITMAIN_SHOW2DMAP 0x01         /* Display the 2DMap        */

/* --------- Other values ------- */
#define EDITMAIN_MAXSPAWN   150         /* Maximum Lines in spawn list  */

/* ---- Flags to toggle --------- */
#define EDITMAIN_TOGGLE_DRAWMODE    1
#define EDITMAIN_TOGGLE_FX          2
#define EDITMAIN_TOGGLE_TXHILO      3

/*******************************************************************************
* TYPEDEFS							                                           *
*******************************************************************************/

typedef struct {

    int fan_chosen;             /* Actual chosen fan for editing    */
    unsigned char display_flags;/* For display in main editor       */
    unsigned char draw_mode;    /* For copy into mesh - struct      */
    char brush_size;            /* From 'cartman'  0-3, (slider)    */    
    char brush_amount;          /* From 'cartman' -50,  50 (slider) */    
    FANDATA_T act_fan;          /* Data of actual fan chosen        */
    FANDATA_T new_fan;          /* Data of fan to place on map      */

} EDITMAIN_STATE_T;

typedef struct {

    char line_name[25];
    char item_name[20+1];
    char slot_no[3 + 1];
    char xpos[4 + 1];
    char ypos[4 + 1];
    char zpos[4 + 1];
    char dir[1 + 1];
    char mon[3 + 1];
    char skin[1 + 1];
    char pas[2 + 1];
    char con[2 + 1];
    char lvl[2 + 1];
    char stt[1 + 1];
    char gho[1 + 1];
    char team[12 + 1];

} SPAWN_OBJECT_T;       /* Description of a spawned object */

/*******************************************************************************
* CODE 								                                           *
*******************************************************************************/

void editmainInit(EDITMAIN_STATE_T *edit_state);
void editmainExit(void);
int  editmainMap(int command);
void editmainDrawMap2D(int x, int y, int w, int h);
SPAWN_OBJECT_T *editmainLoadSpawn(void);
void editmainToggleFlag(EDITMAIN_STATE_T *edit_state, int which, unsigned char flag);

#endif /* _EDITMAIN_H_	*/


//********************************************************************************************
//*
//*    This file is part of Egoboo.
//*
//*    Egoboo is free software: you can redistribute it and/or modify it
//*    under the terms of the GNU General Public License as published by
//*    the Free Software Foundation, either version 3 of the License, or
//*    (at your option) any later version.
//*
//*    Egoboo is distributed in the hope that it will be useful, but
//*    WITHOUT ANY WARRANTY; without even the implied warranty of
//*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//*    General Public License for more details.
//*
//*    You should have received a copy of the GNU General Public License
//*    along with Egoboo.  If not, see <http://www.gnu.org/licenses/>.
//*
//********************************************************************************************

/* Egoboo - game.c
*/

#define DECLARE_GLOBALS

#include "game.h"

#include "char.h"
#include "particle.h"
#include "mad.h"
#include "profile.h"
#include "controls_file.h"
#include "scancode_file.h"

#include "clock.h"
#include "link.h"
#include "ui.h"
#include "font_bmp.h"
#include "font_ttf.h"
#include "log.h"
#include "system.h"
#include "script.h"
#include "sound.h"
#include "graphic.h"
#include "passage.h"
#include "enchant.h"
#include "input.h"
#include "menu.h"
#include "network.h"
#include "mesh.h"
#include "texture.h"
#include "wawalite_file.h"
#include "clock.h"
#include "spawn_file.h"

#include "camera.h"
#include "id_md2.h"

#include "script_compile.h"
#include "script.h"

#include "egoboo.h"

#include "egoboo_vfs.h"
#include "egoboo_endian.h"
#include "egoboo_setup.h"
#include "egoboo_strutil.h"
#include "egoboo_fileutil.h"
#include "egoboo_vfs.h"

#include "SDL_extensions.h"

#include "egoboo_console.h"
#if defined(USE_LUA_CONSOLE)
#include "lua_console.h"
#endif

#include <SDL_image.h>

#include <time.h>
#include <assert.h>
#include <float.h>
#include <string.h>

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

#define CHR_MAX_COLLISIONS    512*16
#define COLLISION_HASH_NODES (CHR_MAX_COLLISIONS*2)

#define MAKE_HASH(AA,BB) CLIP_TO_08BITS( ((AA) * 0x0111 + 0x006E) + ((BB) * 0x0111 + 0x006E) )

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
struct s_line_of_sight_info
{
    float x0, y0, z0;
    float x1, y1, z1;
    Uint32 stopped_by;

    Uint16 collide_chr;
    Uint32 collide_fx;
    int    collide_x;
    int    collide_y;
};

typedef struct s_line_of_sight_info line_of_sight_info_t;

static bool_t collide_ray_with_mesh( line_of_sight_info_t * plos );
static bool_t collide_ray_with_characters( line_of_sight_info_t * plos );
static bool_t do_line_of_sight( line_of_sight_info_t * plos );

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
// Bump List
struct s_bumplist
{
    Uint16  chr;                     // For character collisions
    Uint16  chrnum;                  // Number on the block
    Uint16  prt;                     // For particle collisions
    Uint16  prtnum;                  // Number on the block
};
typedef struct s_bumplist bumplist_t;

//--------------------------------------------------------------------------------------------
// pair-wise collision data

struct s_collision_data
{
    Uint16 chra, chrb;
    Uint16 prtb;
};

typedef struct s_collision_data co_data_t;

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

static ego_mpd_t         _mesh[2];
static camera_t          _camera[2];
static ego_process_t     _eproc;
static menu_process_t    _mproc;
static game_process_t    _gproc;
static game_module_t gmod;

static ClockState_t    * _gclock = NULL;

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
bool_t  overrideslots      = bfalse;

bool_t    screenshotkeyready = btrue;

// End text
char   endtext[MAXENDTEXT] = EMPTY_CSTR;
int    endtextwrite = 0;

// Status displays
bool_t staton     = btrue;
int    numstat    = 0;
Uint16 statlist[MAXSTAT];

ego_mpd_t         * PMesh   = _mesh + 0;
camera_t          * PCamera = _camera + 0;
game_module_t * PMod    = &gmod;
ego_process_t     * EProc   = &_eproc;
menu_process_t    * MProc   = &_mproc;
game_process_t    * GProc   = &_gproc;

bool_t  pitskill  = bfalse;
bool_t  pitsfall  = bfalse;
Uint32  pitx;
Uint32  pity;
Uint32  pitz;

Uint16  glouseangle = 0;                                        // actually still used

Uint32                animtile_update_and = 0;
animtile_instance_t   animtile[2];
damagetile_instance_t damagetile;
weather_instance_t    weather;
water_instance_t      water;
fog_instance_t        fog;

Uint8  local_senseenemiesTeam = TEAM_GOOD; // TEAM_MAX;
IDSZ   local_senseenemiesID   = IDSZ_NONE;

// declare the variables to do profiling
PROFILE_DECLARE( update_loop );

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

// game initialization / deinitialization - not accessible by scripts
static void reset_timers();
static void _quit_game( ego_process_t * pgame );

// looping - stuff called every loop - not accessible by scripts
static void check_stats();
static void tilt_characters_to_terrain();
static void bump_all_objects( void );
//static void stat_return();
static void update_pits();
static void update_game();
static void game_update_timers();
static void do_damage_tiles( void );
static void set_local_latches( void );
static void let_all_characters_think();

// module initialization / deinitialization - not accessible by scripts
static bool_t game_load_module_data( const char *smallname );
static void   game_release_module_data();
static void   game_load_all_profiles( const char *modname );

static void   setup_characters( const char *modname );
static void   setup_alliances( const char *modname );
static void   load_all_global_objects();

static bool_t chr_setup_apply( Uint16 ichr, spawn_file_info_t *pinfo );

static void   game_reset_players();

// Model stuff
static void log_madused( const char *savename );

// Collision stuff
static bool_t add_chr_chr_interaction( Uint16 ichr_a, Uint16 ichr_b, co_data_t cdata[], int * cdata_count, hash_node_t hnlst[], int * hn_count );
static bool_t add_chr_prt_interaction( Uint16 ichr_a, Uint16 iprt_b, co_data_t cdata[], int * cdata_count, hash_node_t hnlst[], int * hn_count );

static bool_t detect_chr_chr_interaction( Uint16 ichr_a, Uint16 ichr_b );
static bool_t detect_chr_prt_interaction( Uint16 ichr_a, Uint16 iprt_b );

static bumplist_t bumplist[MAXMESHFAN/16];

static int           chr_co_count = 0;
static hash_list_t * chr_co_list;

// "process" management
static int do_ego_proc_begin( ego_process_t * eproc );
static int do_ego_proc_running( ego_process_t * eproc );
static int do_ego_proc_leaving( ego_process_t * eproc );
static int do_ego_proc_run( ego_process_t * eproc, double frameDuration );

static int do_menu_proc_begin( menu_process_t * mproc );
static int do_menu_proc_running( menu_process_t * mproc );
static int do_menu_proc_leaving( menu_process_t * mproc );
static int do_menu_proc_run( menu_process_t * mproc, double frameDuration );

static int do_game_proc_begin( game_process_t * gproc );
static int do_game_proc_running( game_process_t * gproc );
static int do_game_proc_leaving( game_process_t * gproc );
static int do_game_proc_run( game_process_t * gproc, double frameDuration );

// misc
static bool_t game_begin_menu( menu_process_t * mproc, which_menu_t which );
static void   game_end_menu( menu_process_t * mproc );

static void   memory_cleanUp(void);

static void   do_game_hud();

static int    ego_init_SDL();

static bool_t _sdl_atexit_registered    = bfalse;
static bool_t _sdl_initialized_base     = bfalse;


//--------------------------------------------------------------------------------------------
// Random Things-----------------------------------------------------------------
//--------------------------------------------------------------------------------------------
void export_one_character( Uint16 character, Uint16 owner, int number, bool_t is_local )
{
    // ZZ> This function exports a character
    int tnc = 0;
    cap_t * pcap;
    pro_t * pobj;

    STRING fromdir;
    STRING todir;
    STRING fromfile;
    STRING tofile;
    STRING todirname;
    STRING todirfullname;

    // Don't export enchants
    disenchant_character( character );

    pobj = chr_get_ppro(character);
    if( NULL == pobj ) return;

    pcap = chr_get_pcap(character);
    if( NULL == pcap ) return;

    if ( ( pcap->cancarrytonextmodule || !pcap->isitem ) && PMod->exportvalid )
    {
        // TWINK_BO.OBJ
        snprintf( todirname, SDL_arraysize( todirname), "%s", str_encode_path(ChrList.lst[owner].name) );

        // Is it a character or an item?
        if ( owner != character )
        {
            // Item is a subdirectory of the owner directory...
            snprintf( todirfullname, SDL_arraysize( todirfullname), "%s" SLASH_STR "%d.obj", todirname, number );
        }
        else
        {
            // Character directory
            snprintf( todirfullname, SDL_arraysize( todirfullname), "%s", todirname );
        }

        // players/twink.obj or players/twink.obj/sword.obj
        if ( is_local )
        {
            snprintf( todir, SDL_arraysize( todir), "players" SLASH_STR "%s", todirfullname );
        }
        else
        {
            snprintf( todir, SDL_arraysize( todir), "remote" SLASH_STR "%s", todirfullname );
        }

        // modules/advent.mod/objects/advent.obj
        snprintf( fromdir, SDL_arraysize( fromdir), "%s", pobj->name );

        // Delete all the old items
        if ( owner == character )
        {
            for ( tnc = 0; tnc < MAXIMPORTOBJECTS; tnc++ )
            {
                snprintf( tofile, SDL_arraysize( tofile), "%s" SLASH_STR "%d.obj", todir, tnc );  /*.OBJ*/
                vfs_removeDirectoryAndContents( tofile, btrue );
            }
        }

        // Make the directory
        vfs_mkdir( todir );

        // Build the DATA.TXT file
        snprintf( tofile, SDL_arraysize( tofile), "%s" SLASH_STR "data.txt", todir );  /*DATA.TXT*/
        export_one_character_profile( tofile, character );

        // Build the SKIN.TXT file
        snprintf( tofile, SDL_arraysize( tofile), "%s" SLASH_STR "skin.txt", todir );  /*SKIN.TXT*/
        export_one_character_skin( tofile, character );

        // Build the NAMING.TXT file
        snprintf( tofile, SDL_arraysize( tofile), "%s" SLASH_STR "naming.txt", todir );  /*NAMING.TXT*/
        export_one_character_name( tofile, character );

        // Copy all of the misc. data files
        snprintf( fromfile, SDL_arraysize( fromfile), "%s" SLASH_STR "message.txt", fromdir );  /*MESSAGE.TXT*/
        snprintf( tofile, SDL_arraysize( tofile), "%s" SLASH_STR "message.txt", todir );  /*MESSAGE.TXT*/
        vfs_copyFile( fromfile, tofile );

        snprintf( fromfile, SDL_arraysize( fromfile), "%s" SLASH_STR "tris.md2", fromdir );  /*TRIS.MD2*/
        snprintf( tofile, SDL_arraysize( tofile),   "%s" SLASH_STR "tris.md2", todir );  /*TRIS.MD2*/
        vfs_copyFile( fromfile, tofile );

        snprintf( fromfile, SDL_arraysize( fromfile), "%s" SLASH_STR "copy.txt", fromdir );  /*COPY.TXT*/
        snprintf( tofile, SDL_arraysize( tofile),   "%s" SLASH_STR "copy.txt", todir );  /*COPY.TXT*/
        vfs_copyFile( fromfile, tofile );

        snprintf( fromfile, SDL_arraysize( fromfile), "%s" SLASH_STR "script.txt", fromdir );
        snprintf( tofile, SDL_arraysize( tofile),   "%s" SLASH_STR "script.txt", todir );
        vfs_copyFile( fromfile, tofile );

        snprintf( fromfile, SDL_arraysize( fromfile), "%s" SLASH_STR "enchant.txt", fromdir );
        snprintf( tofile, SDL_arraysize( tofile),   "%s" SLASH_STR "enchant.txt", todir );
        vfs_copyFile( fromfile, tofile );

        snprintf( fromfile, SDL_arraysize( fromfile), "%s" SLASH_STR "credits.txt", fromdir );
        snprintf( tofile, SDL_arraysize( tofile),   "%s" SLASH_STR "credits.txt", todir );
        vfs_copyFile( fromfile, tofile );

        //snprintf( fromfile, SDL_arraysize( fromfile), "%s" SLASH_STR "quest.txt", fromdir );     Zefz> We can't do this yet, quests are written directly into players/x.obj
        //snprintf( tofile, SDL_arraysize( tofile),   "%s" SLASH_STR "quest.txt", todir );       instead of import/x.obj which should be changed or all changes are lost.
        //vfs_copyFile( fromfile, tofile );

        // Copy all of the particle files
        for ( tnc = 0; tnc < MAX_PIP_PER_PROFILE; tnc++ )
        {
            snprintf( fromfile, SDL_arraysize( fromfile), "%s" SLASH_STR "part%d.txt", fromdir, tnc );
            snprintf( tofile, SDL_arraysize( tofile),   "%s" SLASH_STR "part%d.txt", todir,   tnc );
            vfs_copyFile( fromfile, tofile );
        }

        // Copy all of the sound files
        for ( tnc = 0; tnc < MAX_WAVE; tnc++ )
        {
            snprintf( fromfile, SDL_arraysize( fromfile), "%s" SLASH_STR "sound%d.wav", fromdir, tnc );
            snprintf( tofile, SDL_arraysize( tofile),   "%s" SLASH_STR "sound%d.wav", todir,   tnc );
            vfs_copyFile( fromfile, tofile );

            snprintf( fromfile, SDL_arraysize( fromfile), "%s" SLASH_STR "sound%d.ogg", fromdir, tnc );
            snprintf( tofile, SDL_arraysize( tofile),   "%s" SLASH_STR "sound%d.ogg", todir,   tnc );
            vfs_copyFile( fromfile, tofile );
        }

        // Copy all of the image files (try to copy all supported formats too)
        for ( tnc = 0; tnc < MAX_SKIN; tnc++ )
        {
            Uint8 type;

            for (type = 0; type < maxformattypes; type++)
            {
                snprintf( fromfile, SDL_arraysize( fromfile), "%s" SLASH_STR "tris%d%s", fromdir, tnc, TxFormatSupported[type] );
                snprintf( tofile, SDL_arraysize( tofile),   "%s" SLASH_STR "tris%d%s", todir,   tnc, TxFormatSupported[type] );
                vfs_copyFile( fromfile, tofile );

                snprintf( fromfile, SDL_arraysize( fromfile), "%s" SLASH_STR "icon%d%s", fromdir, tnc, TxFormatSupported[type] );
                snprintf( tofile, SDL_arraysize( tofile),   "%s" SLASH_STR "icon%d%s", todir,   tnc, TxFormatSupported[type] );
                vfs_copyFile( fromfile, tofile );
            }
        }
    }
}

//---------------------------------------------------------------------------------------------
void export_all_players( bool_t require_local )
{
    // ZZ> This function saves all the local players in the
    //    PLAYERS directory
    bool_t is_local;
    int cnt, character, item, number;

    // Don't export if the module isn't running
    if ( !process_running( PROC_PBASE(GProc) ) ) return;

    // Stop if export isnt valid
    if (!PMod->exportvalid) return;

    // Check each player
    for ( cnt = 0; cnt < MAXPLAYER; cnt++ )
    {
        is_local = ( 0 != PlaList[cnt].device.bits );
        if ( require_local && !is_local ) continue;
        if ( !PlaList[cnt].valid ) continue;

        // Is it alive?
        character = PlaList[cnt].index;
        if ( !ACTIVE_CHR(character) || !ChrList.lst[character].alive ) continue;

        // Export the character
        export_one_character( character, character, 0, is_local );

        // Export the left hand item
        item = ChrList.lst[character].holdingwhich[SLOT_LEFT];
        if ( item != MAX_CHR && ChrList.lst[item].isitem )
        {
            export_one_character( item, character, SLOT_LEFT + 1, is_local );
        }

        // Export the right hand item
        item = ChrList.lst[character].holdingwhich[SLOT_RIGHT];
        if ( item != MAX_CHR && ChrList.lst[item].isitem )
        {
            export_one_character( item, character, SLOT_RIGHT + 1, is_local );
        }

        // Export the inventory
        number = 0;
        for ( item = ChrList.lst[character].pack_next;
              number < MAXINVENTORY && item != MAX_CHR;
              item = ChrList.lst[item].pack_next  )
        {
            if ( ChrList.lst[item].isitem )
            {
                export_one_character( item, character, number + 3, is_local );
                number++;
            }
        }
    }

}

//--------------------------------------------------------------------------------------------
void _quit_game( ego_process_t * pgame )
{
    // ZZ> This function exits the game entirely

    if ( process_running( PROC_PBASE(pgame) ) )
    {
        game_quit_module();
    }

    // tell the game to kill itself
    process_kill( PROC_PBASE(pgame) );

    vfs_empty_import_directory();
}

//--------------------------------------------------------------------------------------------
void getadd( int min, int value, int max, int* valuetoadd )
{
    // ZZ> This function figures out what value to add should be in order
    //    to not overflow the min and max bounds
    int newvalue;

    newvalue = value + ( *valuetoadd );
    if ( newvalue < min )
    {
        // Increase valuetoadd to fit
        *valuetoadd = min - value;
        if ( *valuetoadd > 0 )  *valuetoadd = 0;

        return;
    }
    if ( newvalue > max )
    {
        // Decrease valuetoadd to fit
        *valuetoadd = max - value;
        if ( *valuetoadd < 0 )  *valuetoadd = 0;
    }
}

//--------------------------------------------------------------------------------------------
void fgetadd( float min, float value, float max, float* valuetoadd )
{
    // ZZ> This function figures out what value to add should be in order
    //    to not overflow the min and max bounds
    float newvalue;

    newvalue = value + ( *valuetoadd );
    if ( newvalue < min )
    {
        // Increase valuetoadd to fit
        *valuetoadd = min - value;
        if ( *valuetoadd > 0 )  *valuetoadd = 0;

        return;
    }
    if ( newvalue > max )
    {
        // Decrease valuetoadd to fit
        *valuetoadd = max - value;
        if ( *valuetoadd < 0 )  *valuetoadd = 0;
    }
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
void log_madused( const char *savename )
{
    // ZZ> This is a debug function for checking model loads
    vfs_FILE* hFileWrite;
    int cnt;

    hFileWrite = vfs_openWrite( savename );
    if ( hFileWrite )
    {
        vfs_printf( hFileWrite, "Slot usage for objects in last module loaded...\n" );
        vfs_printf( hFileWrite, "%d of %d frames used...\n", md2_loadframe, MAXFRAME );
        cnt = 0;

        while ( cnt < MAX_PROFILE )
        {
            vfs_printf( hFileWrite, "%3d %32s %s\n", cnt, CapList[cnt].classname, MadList[cnt].name );
            cnt++;
        }

        vfs_close( hFileWrite );
    }
}

//--------------------------------------------------------------------------------------------
void statlist_add( Uint16 character )
{
    // ZZ> This function adds a status display to the do list

    chr_t * pchr;

    if ( numstat >= MAXSTAT ) return;

    if( !ACTIVE_CHR(character) ) return;
    pchr = ChrList.lst + character;

    if( pchr->staton ) return;

    statlist[numstat] = character;
    pchr->staton = btrue;
    numstat++;
}

//--------------------------------------------------------------------------------------------
void statlist_move_to_top( Uint16 character )
{
    // ZZ> This function puts the character on top of the statlist
    int cnt, oldloc;

    // Find where it is
    oldloc = numstat;

    for ( cnt = 0; cnt < numstat; cnt++ )
    {
        if ( statlist[cnt] == character )
        {
            oldloc = cnt;
            cnt = numstat;
        }
    }

    // Change position
    if ( oldloc < numstat )
    {
        // Move all the lower ones up
        while ( oldloc > 0 )
        {
            oldloc--;
            statlist[oldloc+1] = statlist[oldloc];
        }

        // Put the character in the top slot
        statlist[0] = character;
    }
}

//--------------------------------------------------------------------------------------------
void statlist_sort()
{
    // ZZ> This function puts all of the local players on top of the statlist
    int cnt;

    for ( cnt = 0; cnt < PlaList_count; cnt++ )
    {
        if ( PlaList[cnt].valid && PlaList[cnt].device.bits != INPUT_BITS_NONE )
        {
            statlist_move_to_top( PlaList[cnt].index );
        }
    }
}
//--------------------------------------------------------------------------------------------
void chr_play_action( Uint16 character, Uint16 action, Uint8 actionready )
{
    // ZZ> This function starts a generic action for a character

    chr_t * pchr;
    mad_t * pmad;

    if ( !ACTIVE_CHR(character) ) return;
    pchr = ChrList.lst + character;

    pmad = chr_get_pmad(character);
    if ( NULL == pmad ) return;

    if ( pmad->actionvalid[action] )
    {
        pchr->nextaction = ACTION_DA;
        pchr->action = action;

        pchr->inst.flip = 0;
        pchr->inst.ilip = 0;
        pchr->inst.frame_lst = pchr->inst.frame_nxt;
        pchr->inst.frame_nxt = pmad->actionstart[pchr->action];
        pchr->actionready    = actionready;
    }
}

//--------------------------------------------------------------------------------------------
void chr_set_frame( Uint16 character, Uint16 action, int frame, Uint16 lip )
{
    // ZZ> This function sets the frame for a character explicitly...  This is used to
    //    rotate Tank turrets

    chr_t * pchr;
    mad_t * pmad;

    if ( !ACTIVE_CHR(character) ) return;
    pchr = ChrList.lst + character;

    pmad = chr_get_pmad(character);
    if ( NULL == pmad ) return;

    if ( pmad->actionvalid[action] )
    {
        int framesinaction, frame_stt, frame_end;

        pchr->nextaction = ACTION_DA;
        pchr->action = ACTION_DA;
        pchr->actionready = btrue;

        framesinaction = (pmad->actionend[action] - pmad->actionstart[action]) + 1;
        if ( framesinaction <= 1 )
        {
            frame_stt = pmad->actionstart[action];
            frame_end = frame_stt;
        }
        else
        {
            frame = MIN(frame, framesinaction);
            frame_stt = pmad->actionstart[action] + frame;

            frame = MIN(frame + 1, framesinaction);
            frame_end = frame_stt + 1;
        }

        pchr->inst.ilip  = lip;
        pchr->inst.flip  = lip / 4.0f;
        pchr->inst.frame_lst = frame_stt;
        pchr->inst.frame_nxt = frame_end;
    }
}

//--------------------------------------------------------------------------------------------
void setup_alliances( const char *modname )
{
    // ZZ> This function reads the alliance file
    STRING newloadname;
    STRING szTemp;
    Uint8 teama, teamb;
    vfs_FILE *fileread;

    // Load the file
    make_newloadname( modname, "gamedat" SLASH_STR "alliance.txt", newloadname );
    fileread = vfs_openRead( newloadname );
    if ( fileread )
    {
        while ( goto_colon( NULL, fileread, btrue ) )
        {
            fget_string( fileread, szTemp, SDL_arraysize(szTemp) );
            teama = ( szTemp[0] - 'A' ) % TEAM_MAX;

            fget_string( fileread, szTemp, SDL_arraysize(szTemp) );
            teamb = ( szTemp[0] - 'A' ) % TEAM_MAX;
            TeamList[teama].hatesteam[teamb] = bfalse;
        }

        vfs_close( fileread );
    }
}

//--------------------------------------------------------------------------------------------
void update_all_objects()
{
    update_all_characters();
    update_all_particles();
    update_all_enchants();
}

//--------------------------------------------------------------------------------------------
void cleanup_all_objects()
{
    cleanup_all_characters();
    cleanup_all_particles();
    cleanup_all_enchants();
};

void move_all_objects()
{
    move_all_particles();
    move_all_characters();
}

//--------------------------------------------------------------------------------------------
void update_game()
{
    // ZZ> This function does several iterations of character movements and such
    //    to keep the game in sync.
    //    This is the main game loop

    int cnt, tnc, numdead, numalive;

    // Check for all local players being dead
    local_allpladead   = bfalse;
    local_seeinvisible = bfalse;
    local_seekurse     = bfalse;
    local_seedark      = 0;

    numplayer = 0;
    numdead = numalive = 0;
    for ( cnt = 0; cnt < MAXPLAYER; cnt++ )
    {
        Uint16 ichr;
        chr_t * pchr;

        if ( !PlaList[cnt].valid ) continue;

        // fix bad players
        ichr = PlaList[cnt].index;
        if ( !ACTIVE_CHR(ichr) )
        {
            PlaList[cnt].index = MAX_CHR;
            PlaList[cnt].valid = bfalse;
            continue;
        }
        pchr = ChrList.lst + ichr;

        // count the total number of players
        numplayer++;

        // only interested in local players
        if ( INPUT_BITS_NONE == PlaList[cnt].device.bits ) continue;

        if ( pchr->alive )
        {
            numalive++;

            if ( pchr->canseeinvisible )
            {
                local_seeinvisible = btrue;
            }

            if ( pchr->canseekurse )
            {
                local_seekurse = btrue;
            }

            if( pchr->hasdarkvision )
            {
                local_seedark = MAX(local_seedark, pchr->hasdarkvision);
            }
        }
        else
        {
            numdead++;
        }
    }

    // Did everyone die?
    if ( numdead >= local_numlpla )
    {
        local_allpladead = btrue;
    }

    // check for autorespawn
    for ( cnt = 0; cnt < MAXPLAYER; cnt++ )
    {
        Uint16 ichr;
        chr_t * pchr;

        if ( !PlaList[cnt].valid ) continue;

        ichr = PlaList[cnt].index;
        if ( !ACTIVE_CHR(ichr) ) continue;
        pchr = ChrList.lst + ichr;

        if ( !pchr->alive )
        {
            if ( cfg.difficulty < GAME_HARD && local_allpladead && SDLKEYDOWN( SDLK_SPACE ) && PMod->respawnvalid && 0 == revivetimer )
            {
                respawn_character( ichr );
                pchr->experience *= EXPKEEP;  // Apply xp Penality

                if (cfg.difficulty > GAME_EASY)
                {
                    pchr->money *= EXPKEEP;
                }
            }
        }
    }

    // get all player latches from the "remotes"
    sv_talkToRemotes();

    // [claforte Jan 6th 2001]
    // TODO: Put that back in place once networking is functional.
    update_lag = true_update - update_wld;
    for( tnc = 0; update_wld < true_update && tnc < 1000 ; tnc++)
    {
        // do important stuff to keep in sync inside this loop

        srand( PMod->randsave );
        PMod->randsave = rand();

        //---- begin the code for updating misc. game stuff
        {
            BillboardList_update_all();
            animate_tiles();
            move_water();
            looped_update_all_sound();
            do_damage_tiles();
            update_pits();
            do_weather_spawn_particles();
        }
        //---- end the code for updating misc. game stuff

        //---- begin the code for updating in-game objects
        {
            update_all_objects();

            let_all_characters_think();
            unbuffer_player_latches();

            move_all_objects();
            bump_all_objects();

            cleanup_all_objects();
        }
        //---- end the code for updating in-game objects

        game_update_timers();

        // Timers
        clock_wld += UPDATE_SKIP;
        clock_enc_stat++;
        clock_chr_stat++;

        // Reset the respawn timer
        if ( revivetimer > 0 ) revivetimer--;

        update_wld++;
        ups_loops++;

       update_lag = MAX( update_lag, true_update - update_wld );
    }
    update_lag = tnc;

    if ( PNet->on )
    {
        if ( numplatimes == 0 )
        {
            // The remote ran out of messages, and is now twiddling its thumbs...
            // Make it go slower so it doesn't happen again
            clock_wld += 25;
        }
        if ( numplatimes > 3 && !PNet->hostactive )
        {
            // The host has too many messages, and is probably experiencing control
            // lag...  Speed it up so it gets closer to sync
            clock_wld -= 5;
        }
    }
}

//--------------------------------------------------------------------------------------------
void game_update_timers()
{
    // ZZ> This function updates the game timers

    static bool_t update_was_paused = bfalse;

    int ticks_diff;
    int clock_diff;

    const float fold = 0.77f;
    const float fnew = 1.0f - fold;

    ticks_last = ticks_now;
    ticks_now  = SDL_GetTicks();

    // check to make sure that the game is running
    if( !process_running( PROC_PBASE(GProc) ) || GProc->mod_paused )
    {
        // for a local game, force the function to ignore the accumulation of time
        // until you re-join the game
        if( !PNet->on )
        {
            ticks_last = ticks_now;
            update_was_paused = btrue;
            return;
        }
    }

    // make sure some amount of time has passed
    ticks_diff = ticks_now - ticks_last;
    if( 0 == ticks_diff ) return;

    // measure the time since the last call
    clock_lst  = clock_all;

    // calculate the time since the from the last update
    // if the game was paused, assume that only one update time elapsed since the last time through this function
    clock_diff = UPDATE_SKIP;
    if( !update_was_paused )
    {
        clock_diff = ticks_diff;
        clock_diff = MIN(clock_diff, 10*UPDATE_SKIP);
    }

    if( PNet->on )
    {
        // if the network game is on, there really is no real "pause"
        // so we can always measure the game time from the first clock reading
        clock_all = ticks_now - clock_stt;
    }
    else
    {
        // if the net is not on, the game clock will pause when the local game is paused.
        // if we use the other calculation, the game will freeze while it handles the updates
        // for all the time that the game was paused... not so good
        clock_all  += clock_diff;
    }

    // Use the number of updates that should have been performed up to this point (true_update)
    // to try to regulate the update speed of the game
    // By limiting this loop to 10, you are essentially saying that the update loop
    // can go 10 times as fast as normal to help update_wld catch up to true_update,
    // but it can't completely bog doen the game
    true_update = clock_all / UPDATE_SKIP;

    // figure out the update rate
    ups_clock += clock_diff;

    if ( ups_loops > 0 && ups_clock > 0)
    {
        stabilized_ups_sum    = stabilized_ups_sum * fold + fnew * (float) ups_loops / ((float) ups_clock / TICKS_PER_SEC );
        stabilized_ups_weight = stabilized_ups_weight * fold + fnew;

        // blank these every so often so that the numbers don't overflow
        if( ups_loops > 10 * TARGET_UPS )
        {
            ups_loops = 0;
            ups_clock = 0;
        }
    }

    if ( stabilized_ups_weight > 0.5f )
    {
        stabilized_ups = stabilized_ups_sum / stabilized_ups_weight;
    }

    // if it got this far and the funciton had been paused, it is time to unpause it
    update_was_paused = bfalse;
}

//--------------------------------------------------------------------------------------------
void reset_timers()
{
    // ZZ> This function resets the timers...

    clock_stt = ticks_now = ticks_last = SDL_GetTicks();

    clock_all = 0;
    clock_lst = 0;
    clock_wld = 0;
    clock_enc_stat = 0;
    clock_chr_stat = 0;
    clock_pit = 0;

    update_wld = 0;
    frame_all = 0;
    frame_fps = 0;
    outofsync = bfalse;

    pitskill = pitsfall = bfalse;
}

//--------------------------------------------------------------------------------------------
int game_do_menu( menu_process_t * mproc )
{
    // BB> do menus

    int menuResult;
    bool_t need_menu = bfalse;

    need_menu = bfalse;
    if ( flip_pages_requested() )
    {
        // someone else (and that means the game) has drawn a frame
        // so we just need to draw the menu over that frame
        need_menu = btrue;

        // force the menu to be displayed immediately when the game stops
        mproc->base.dtime = 1.0f / (float)cfg.framelimit;
    }
    else if ( !process_running( PROC_PBASE(GProc) ) )
    {
        // the menu's frame rate is controlled by a timer
        mproc->ticks_now = SDL_GetTicks();
        if (mproc->ticks_now > mproc->ticks_next)
        {
            // FPS limit
            float  frameskip = (float)TICKS_PER_SEC / (float)cfg.framelimit;
            mproc->ticks_next = mproc->ticks_now + frameskip;

            need_menu = btrue;
            mproc->base.dtime = 1.0f / (float)cfg.framelimit;
        }
    }

    menuResult = 0;
    if ( need_menu )
    {
        ui_beginFrame( mproc->base.dtime );
        {
            menuResult = doMenu( mproc->base.dtime );
            request_flip_pages();
        }
        ui_endFrame();
    }

    return menuResult;
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
void console_init()
{
    // BB> initialize the console. This must happen after the screen has been defines,
    //     otherwise sdl_scr.x == sdl_scr.y == 0 and the screen will be defined to
    //     have no area...

    SDL_Rect blah = {0, 0, sdl_scr.x, sdl_scr.y / 4};

#if defined(USE_LUA_CONSOLE)
    lua_console_new(NULL, blah);
#else
    // without a callback, this console just dumps the input and generates no output
    egoboo_console_new(NULL, blah, NULL, NULL);
#endif
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
int do_ego_proc_begin( ego_process_t * eproc )
{
    const char * tmpname;

    // initialize the virtual filesystem first
    vfs_init( eproc->argv0 );

    // Initialize logging next, so that we can use it everywhere.
    log_init( vfs_resolveWriteFilename("debug/log.txt") );
    log_setLoggingLevel( 2 );

    // start initializing the various subsystems
    log_message( "Starting Egoboo " VERSION " ...\n" );

    sys_initialize();
    clk_init();
    _gclock = clk_create("global clock", -1);

    // read the "setup.txt" file
    tmpname = "setup.txt";	
    if ( !setup_read( tmpname ) )
    {
        log_error( "Could not find \"%s\".\n", tmpname );
    }

    // download the "setup.txt" values into the cfg struct
    setup_download( &cfg );

    // do basic system initialization
    ego_init_SDL();
    gfx_init();
    console_init();
    net_initialize();

    log_info( "Initializing SDL_Image version %d.%d.%d... ", SDL_IMAGE_MAJOR_VERSION, SDL_IMAGE_MINOR_VERSION, SDL_IMAGE_PATCHLEVEL );
    GLSetup_SupportedFormats();

    // read all the scantags
    scantag_read_all( "basicdat" SLASH_STR "scancode.txt" );
    input_settings_load( "controls.txt" );

    // synchronoze the config values with the various game subsystems
    // do this acter the ego_init_SDL() and ogl_init() in case the config values are clamped
    // to valid values
    setup_synch( &cfg );

    // initialize the sound system
    sound_initialize();
    load_all_music_sounds();

    // make sure that a bunch of stuff gets initialized properly
    game_module_init( &gmod );
    free_all_objects();
    mesh_new( PMesh );
    init_all_graphics();
    profile_init();

    // setup the menu system's gui
    ui_initialize( "basicdat" SLASH_STR "Negatori.ttf", 24 );
    font_bmp_load( "basicdat" SLASH_STR "font", "basicdat" SLASH_STR "font.txt" );  // must be done after init_all_graphics()

    // clear out the import directory
    vfs_empty_import_directory();

    // register the memory_cleanUp function to automatically run whenever the program exits
    atexit( memory_cleanUp );

    // initialize the game process (not active)
    game_process_init( GProc );

    // initialize the menu process (active)
    menu_process_init( MProc );
    process_start( PROC_PBASE(MProc) );

    // Initialize the process
    process_start( PROC_PBASE(eproc) );

    // initialize all the profile variables
    PROFILE_INIT( update_loop );

    return 1;
}

//--------------------------------------------------------------------------------------------
int do_ego_proc_running( ego_process_t * eproc )
{
    bool_t menu_valid, game_valid;

    if ( !process_validate( PROC_PBASE(eproc) ) ) return -1;

    eproc->was_active  = eproc->base.valid;

    menu_valid = process_validate( PROC_PBASE(MProc) );
    game_valid = process_validate( PROC_PBASE(GProc) );
    if ( !menu_valid && !game_valid )
    {
        process_kill( PROC_PBASE(eproc) );
        return 1;
    }

    if ( eproc->base.paused ) return 0;

    if ( process_running( PROC_PBASE(MProc) ) )
    {
        // menu settings
        SDL_WM_GrabInput ( SDL_GRAB_OFF );
        SDL_ShowCursor( SDL_ENABLE  );
    }
    else
    {
        // in-game settings
        SDL_ShowCursor( cfg.hide_mouse ? SDL_DISABLE : SDL_ENABLE );
        SDL_WM_GrabInput ( cfg.grab_mouse ? SDL_GRAB_ON : SDL_GRAB_OFF );
    }

    // Clock updates each frame
    clk_frameStep( _gclock );
    eproc->frameDuration = clk_getFrameDuration( _gclock );

    // read the input values
    input_read();

    if ( pickedmodule_ready && !process_running( PROC_PBASE(MProc) ) )
    {
        // a new module has been picked

        // reset the flag
        pickedmodule_ready = bfalse;

        // start the game process
        process_start( PROC_PBASE(GProc) );
    }

    // Test the panic button
    if ( SDLKEYDOWN( SDLK_q ) && SDLKEYDOWN( SDLK_LCTRL ) )
    {
        // terminate the program
        process_kill( PROC_PBASE(eproc) );
    }

    // Check for screenshots
    if ( !SDLKEYDOWN( SDLK_F11 ) )
    {
        screenshotkeyready = btrue;
    }
    else if ( screenshotkeyready && SDLKEYDOWN( SDLK_F11 ) && keyb.on )
    {
        screenshotkeyready = bfalse;

        if ( !dump_screenshot() )                // Take the shot, returns bfalse if failed
        {
            debug_printf( "Error writing screenshot!" );
            log_warning( "Error writing screenshot\n" );    // Log the error in log.txt
        }
    }

    // handle an escape by passing it on to all active sub-processes
    if ( eproc->escape_requested )
    {
        eproc->escape_requested = bfalse;

        if ( process_running( PROC_PBASE(GProc) ) )
        {
            GProc->escape_requested = btrue;
        }

        if ( process_running( PROC_PBASE(MProc) ) )
        {
            MProc->escape_requested = btrue;
        }
    }

    // run the sub-processes
    do_game_proc_run( GProc, EProc->frameDuration );
    do_menu_proc_run( MProc, EProc->frameDuration );

    // a heads up display that can be used to debug values that are used by both the menu and the game
    // do_game_hud();

    return 0;
}

//--------------------------------------------------------------------------------------------
int do_ego_proc_leaving( ego_process_t * eproc )
{
    if ( !process_validate( PROC_PBASE(eproc) )  ) return -1;

    // make sure that the
    if ( !GProc->base.terminated )
    {
        do_game_proc_run( GProc, eproc->frameDuration );
    }

    if ( !MProc->base.terminated )
    {
        do_menu_proc_run( MProc, eproc->frameDuration );
    }

    if ( GProc->base.terminated && MProc->base.terminated )
    {
        process_terminate( PROC_PBASE(eproc) );
    }

    return eproc->base.terminated ? 0 : 1;
}

//--------------------------------------------------------------------------------------------
int do_ego_proc_run( ego_process_t * eproc, double frameDuration )
{
    int result = 0, proc_result = 0;

    if ( !process_validate( PROC_PBASE(eproc) )  ) return -1;
    eproc->base.dtime = frameDuration;

    if ( !eproc->base.paused ) return 0;

    if ( eproc->base.killme )
    {
        eproc->base.state = proc_leaving;
    }

    switch ( eproc->base.state )
    {
        case proc_begin:
            proc_result = do_ego_proc_begin( eproc );

            if ( 1 == proc_result )
            {
                eproc->base.state = proc_entering;
            }
            break;

        case proc_entering:
            // proc_result = do_ego_proc_entering( eproc );

            eproc->base.state = proc_running;
            break;

        case proc_running:
            proc_result = do_ego_proc_running( eproc );

            if ( 1 == proc_result )
            {
                eproc->base.state = proc_leaving;
            }
            break;

        case proc_leaving:
            proc_result = do_ego_proc_leaving( eproc );

            if ( 1 == proc_result )
            {
                eproc->base.state  = proc_finish;
                eproc->base.killme = bfalse;
            }
            break;

        case proc_finish:
            process_terminate( PROC_PBASE(eproc) );
            break;
    }

    return result;
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
int do_menu_proc_begin( menu_process_t * mproc )
{
    // play some music
    sound_play_song( MENU_SONG, 0, -1 );

    // initialize all these structures
    mnu_init();        // start the menu menu

    // load all module info at menu initialization
    // this will not change unless a new module is downloaded for a network menu?
    mnu_load_all_module_info();

    // initialize the process state
    mproc->base.valid = btrue;

    return 1;
}

//--------------------------------------------------------------------------------------------
int do_menu_proc_running( menu_process_t * mproc )
{
    int menuResult;

    if ( !process_validate( PROC_PBASE(mproc) ) ) return -1;

    mproc->was_active = mproc->base.valid;

    if ( mproc->base.paused ) return 0;

    // play the menu music
    mnu_draw_background = !process_running( PROC_PBASE(GProc) );
    menuResult          = game_do_menu( mproc );

    switch ( menuResult )
    {
        case MENU_SELECT:
            // go ahead and start the game
            process_pause( PROC_PBASE(mproc) );
            break;

        case MENU_QUIT:
            // the user selected "quit"
            process_kill( PROC_PBASE(mproc) );
            break;
    }

    if ( mnu_get_menu_depth() <= GProc->menu_depth )
    {
        GProc->menu_depth   = -1;
        GProc->escape_latch = bfalse;

        // We have exited the menu and restarted the game
        GProc->mod_paused = bfalse;
        process_pause( PROC_PBASE(MProc) );
    }

    return 0;
}

//--------------------------------------------------------------------------------------------
int do_menu_proc_leaving( menu_process_t * mproc )
{
    if ( !process_validate( PROC_PBASE(mproc) ) ) return -1;

    // finish the menu song
    sound_finish_song( 500 );

    return 1;
}

//--------------------------------------------------------------------------------------------
int do_menu_proc_run( menu_process_t * mproc, double frameDuration )
{
    int result = 0, proc_result = 0;

    if ( !process_validate( PROC_PBASE(mproc) ) ) return -1;
    mproc->base.dtime = frameDuration;

    if ( mproc->base.paused ) return 0;

    if ( mproc->base.killme )
    {
        mproc->base.state = proc_leaving;
    }

    switch ( mproc->base.state )
    {
        case proc_begin:
            proc_result = do_menu_proc_begin( mproc );

            if ( 1 == proc_result )
            {
                mproc->base.state = proc_entering;
            }
            break;

        case proc_entering:
            // proc_result = do_menu_proc_entering( mproc );

            mproc->base.state = proc_running;
            break;

        case proc_running:
            proc_result = do_menu_proc_running( mproc );

            if ( 1 == proc_result )
            {
                mproc->base.state = proc_leaving;
            }
            break;

        case proc_leaving:
            proc_result = do_menu_proc_leaving( mproc );

            if ( 1 == proc_result )
            {
                mproc->base.state  = proc_finish;
                mproc->base.killme = bfalse;
            }
            break;

        case proc_finish:
            process_terminate( PROC_PBASE(mproc) );
            break;
    }

    return result;
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
int do_game_proc_begin( game_process_t * gproc )
{
    BillboardList_init_all();

    gproc->escape_latch = bfalse;

    // initialize math objects
    make_randie();
    make_turntosin();

    // reset the fps counter
    fps_clock             = 0;
    fps_loops             = 0;
    stabilized_fps_sum    = 0.1f * TARGET_FPS;
    stabilized_fps_weight = 0.1f;

    // Linking system
    log_info( "Initializing module linking... " );
    if ( link_build( "basicdat" SLASH_STR "link.txt", LinkList ) ) log_message( "Success!\n" );
    else log_message( "Failure!\n" );

    // intialize the "profile system"
    profile_init();

    // do some graphics initialization
    make_lightdirectionlookup();
    make_enviro();
    camera_new( PCamera );

    // try to start a new module
    if ( !game_begin_module( pickedmodule_name, (Uint32)~0 ) )
    {
        // failure - kill the game process
        process_kill( PROC_PBASE(gproc) );
        process_resume( PROC_PBASE(MProc) );
    }

    // Initialize the process
    gproc->base.valid = btrue;

    return 1;
}

//--------------------------------------------------------------------------------------------
int do_game_proc_running( game_process_t * gproc )
{
    if ( !process_validate( PROC_PBASE(gproc) ) ) return -1;

    gproc->was_active  = gproc->base.valid;

    if ( gproc->base.paused ) return 0;

    PROFILE_BEGIN( update_loop );
    {
        // This is the control loop
        if ( PNet->on && console_done )
        {
            net_send_message();
        }

        // update all the timers
        game_update_timers();

        // do the updates
        if ( gproc->mod_paused && !PNet->on )
        {
            clock_wld = clock_all;
        }
        else
        {
            // start the console mode?
            if ( control_is_pressed( INPUT_DEVICE_KEYBOARD, CONTROL_MESSAGE ) )
            {
                // reset the keyboard buffer
                SDL_EnableKeyRepeat(20, SDL_DEFAULT_REPEAT_DELAY);
                console_mode = btrue;
                console_done = bfalse;
                keyb.buffer_count = 0;
                keyb.buffer[0] = CSTR_END;
            }

            // NETWORK PORT
            listen_for_packets();

            //gproc->ups_ticks_now = SDL_GetTicks();
            //if (gproc->ups_ticks_now > gproc->ups_ticks_next)
            {
                // UPS limit
                //gproc->ups_ticks_next = gproc->ups_ticks_now + UPDATE_SKIP;

                check_stats();
                set_local_latches();
                check_passage_music();

                if ( !PNet->waitingforplayers )
                {
                    cl_talkToHost();
                    update_game();
                }
                else
                {
                    clock_wld = clock_all;
                }
            }
        }
    }
    PROFILE_END2( update_loop );

    // estimate how much time the main loop is taking per second
    est_update_time = 0.9 * est_update_time + 0.1 * PROFILE_QUERY(update_loop);
    est_max_ups     = 0.9 * est_max_ups     + 0.1 * (1.0f / PROFILE_QUERY(update_loop) );

    // Do the display stuff
    gproc->fps_ticks_now = SDL_GetTicks();
    if (gproc->fps_ticks_now > gproc->fps_ticks_next)
    {
        // FPS limit
        float  frameskip = (float)TICKS_PER_SEC / (float)cfg.framelimit;
        gproc->fps_ticks_next = gproc->fps_ticks_now + frameskip;

        camera_move(PCamera, PMesh);
        draw_main();

        msgtimechange++;
    }

    // Check for quitters
    // :TODO: local_noplayers is not set correctly
    // if( local_noplayers  )
    // {
    //  gproc->escape_requested  = btrue;
    // }

    if ( gproc->escape_requested )
    {
        gproc->escape_requested = bfalse;

        if ( !gproc->escape_latch )
        {
            if ( PMod->beat )
            {
                game_begin_menu( MProc, emnu_ShowEndgame );
            }
            else
            {
                game_begin_menu( MProc, emnu_GamePaused );
            }

            gproc->escape_latch = btrue;
            gproc->mod_paused   = btrue;
        }
    }

    return 0;
}

//--------------------------------------------------------------------------------------------
int do_game_proc_leaving( game_process_t * gproc )
{
    if ( !process_validate( PROC_PBASE(gproc) ) ) return -1;

    // get rid of all module data
    game_quit_module();

    // resume the menu
    process_resume( PROC_PBASE(MProc) );

    // reset the fps counter
    fps_clock             = 0;
    fps_loops             = 0;
    stabilized_fps_sum    = 0.1f * stabilized_fps_sum / stabilized_fps_weight;
    stabilized_fps_weight = 0.1f;

    return 1;
}

//--------------------------------------------------------------------------------------------
int do_game_proc_run( game_process_t * gproc, double frameDuration )
{
    int result = 0, proc_result = 0;

    if ( !process_validate( PROC_PBASE(gproc) ) ) return -1;
    gproc->base.dtime = frameDuration;

    if ( gproc->base.paused ) return 0;

    if ( gproc->base.killme )
    {
        gproc->base.state = proc_leaving;
    }

    switch ( gproc->base.state )
    {
        case proc_begin:
            proc_result = do_game_proc_begin( gproc );

            if ( 1 == proc_result )
            {
                gproc->base.state = proc_entering;
            }
            break;

        case proc_entering:
            // proc_result = do_game_proc_entering( gproc );

            gproc->base.state = proc_running;
            break;

        case proc_running:
            proc_result = do_game_proc_running( gproc );

            if ( 1 == proc_result )
            {
                gproc->base.state = proc_leaving;
            }
            break;

        case proc_leaving:
            proc_result = do_game_proc_leaving( gproc );

            if ( 1 == proc_result )
            {
                gproc->base.state  = proc_finish;
                gproc->base.killme = bfalse;
            }
            break;

        case proc_finish:
            process_terminate( PROC_PBASE(gproc) );
            process_resume   ( PROC_PBASE(MProc) );
            break;
    }

    return result;
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
int SDL_main( int argc, char **argv )
{
    // ZZ> This is where the program starts and all the high level stuff happens

    int result = 0;

    // initialize the process
    ego_process_init( EProc, argc, argv );

    // turn on all basic services
    do_ego_proc_begin( EProc );

    // run the processes
    request_clear_screen();
    while ( !EProc->base.killme && !EProc->base.terminated )
    {
        // let the OS breathe. It may delay as long as 10ms
        //SDL_Delay(1);

        // put a throttle on the ego process
        EProc->ticks_now = SDL_GetTicks();
        if (EProc->ticks_now < EProc->ticks_next) continue;

        // update the timer
        EProc->ticks_next = EProc->ticks_now + 10;

        // clear the screen if needed
        do_clear_screen();

        do_ego_proc_running( EProc );

        // flip the graphics page if need be
        do_flip_pages();
    }

    // terminate the game and menu processes
    process_kill( PROC_PBASE(GProc) );
    process_kill( PROC_PBASE(MProc) );
    while ( !EProc->base.terminated )
    {
        result = do_ego_proc_leaving( EProc );
    }

    return result;
}

//--------------------------------------------------------------------------------------------
void memory_cleanUp(void)
{
    // ZF> This function releases all loaded things in memory and cleans up everything properly

    log_info("memory_cleanUp() - Attempting to clean up loaded things in memory... ");

    // quit any existing game
    _quit_game( EProc );

    // synchronoze the config values with the various game subsystems
    setup_synch( &cfg );

    // quit the setup system, making sure that the setup file is written
    setup_upload( &cfg );
    setup_write();
    setup_quit();

    // delete all the graphics allocated by SDL and OpenGL
    delete_all_graphics();

    // make sure that the current control configuration is written
    input_settings_save( "controls.txt" );

    // shut down the ui
    ui_shutdown();

    // shut down the network
    if (PNet->on)
    {
        net_shutDown();
    }

    // shut down the clock services
    clk_destroy( &_gclock );
    clk_shutdown();

    log_message("Success!\n");
    log_info( "Exiting Egoboo " VERSION " the good way...\n" );

    // shut down the log services
    log_shutdown();
}

//--------------------------------------------------------------------------------------------
Uint16 prt_find_target( float pos_x, float pos_y, float pos_z, Uint16 facing,
                            Uint16 particletype, Uint8 team, Uint16 donttarget,
                            Uint16 oldtarget )
{
    // ZF> This is the new improved targeting system for particles. Also includes distance in the Z direction.

    const float max_dist2 = WIDE * WIDE;

    pip_t * ppip;

    Uint16 besttarget = MAX_CHR, cnt;
    float  longdist2 = max_dist2;

    if( INVALID_PIP(particletype) ) return MAX_CHR;
    ppip = PipStack.lst + particletype;

    for (cnt = 0; cnt < MAX_CHR; cnt++)
    {
        chr_t * pchr;
        bool_t target_friend, target_enemy;

        if ( !ACTIVE_CHR(cnt) ) continue;
        pchr = ChrList.lst + cnt;

        if ( !pchr->alive || pchr->isitem || ACTIVE_CHR(pchr->attachedto) ) continue;

        // ignore invictus
        if( pchr->invictus ) continue;

        // we are going to give the player a break and not target things that
        // can't be damaged, unless the particle is homing. If it homes in,
        // the he damagetime could drop off en route.
        if( !ppip->homing && (0 != pchr->damagetime) ) continue;

        // Don't retarget someone we already had or not supposed to target
        if ( cnt == oldtarget || cnt == donttarget) continue;

        target_friend = ppip->onlydamagefriendly && team == chr_get_iteam(cnt);
        target_enemy  = !ppip->onlydamagefriendly && TeamList[team].hatesteam[chr_get_iteam(cnt)];

        if ( target_friend || target_enemy )
        {
            Uint16 angle = - facing + vec_to_facing( pchr->pos.x - pos_x , pchr->pos.y - pos_y );

            // Only proceed if we are facing the target
            if (angle < ppip->targetangle || angle > ( 0xFFFF - ppip->targetangle ) )
            {
                float dist2 =
                    POW(ABS(pchr->pos.x - pos_x), 2) +
                    POW(ABS(pchr->pos.y - pos_y), 2) +
                    POW(ABS(pchr->pos.z - pos_z), 2);

                if ( dist2 < longdist2 && dist2 <= max_dist2 )
                {
                    glouseangle = angle;
                    besttarget = cnt;
                    longdist2 = dist2;
                }
            }
        }
    }

    // All done
    return besttarget;
}

//--------------------------------------------------------------------------------------------
bool_t check_target( chr_t * psrc, Uint16 ichr_test, TARGET_TYPE target_type, bool_t target_items, bool_t target_dead, IDSZ target_idsz, bool_t exclude_idsz, bool_t target_players )
{
    bool_t retval;

    bool_t is_hated, hates_me;
    bool_t is_friend, is_prey, is_predator, is_mutual;
    chr_t * ptst;

    // Skip non-existing objects
    if( !ACTIVE_PCHR( psrc ) ) return bfalse;

    if( !ACTIVE_CHR(ichr_test) ) return bfalse;
    ptst = ChrList.lst + ichr_test;

	// Players only?
	if ( target_players && !ptst->isplayer ) return bfalse;

    // Skip held objects and self
    if ( psrc == ptst || ACTIVE_CHR(ptst->attachedto) || ptst->pack_ispacked ) return bfalse;

    // Either only target dead stuff or alive stuff
    if ( target_dead == ptst->alive ) return bfalse;

    // Dont target invisible stuff, unless we can actually see them
    if ( !psrc->canseeinvisible && FF_MUL( ptst->inst.alpha, ptst->inst.max_light ) < INVISIBLE ) return bfalse;

	is_hated = TeamList[psrc->team].hatesteam[ptst->team];
    hates_me = TeamList[ptst->team].hatesteam[psrc->team];

    // Target neutral items? (still target evil items, could be pets)
    if ( !target_items && ( (ptst->isitem && is_hated) || ptst->invictus ) ) return bfalse;

    // these options are here for ideas of ways to mod this function
    is_friend    = !is_hated && !hates_me;
    is_prey      =  is_hated && !hates_me;
    is_predator  = !is_hated &&  hates_me;
    is_mutual    =  is_hated &&  hates_me;

    // Which target_type to target
    retval = bfalse;
    if ( target_type == TARGET_ALL || (target_type == TARGET_ENEMY && is_hated) || (target_type == TARGET_FRIEND && !is_hated) )
    {
        // Check for specific IDSZ too?
        if( IDSZ_NONE == target_idsz )
        {
            retval = btrue;
        }
        else
        {
            bool_t match_idsz = ( target_idsz == pro_get_idsz(ptst->iprofile,IDSZ_PARENT) ) ||
                                ( target_idsz == pro_get_idsz(ptst->iprofile,IDSZ_TYPE  ) );

            if (  match_idsz )
            {
                if ( !exclude_idsz ) retval = btrue;
            }
            else
            {
                if ( exclude_idsz ) retval = btrue;
            }
        }
    }

    return retval;
}

//--------------------------------------------------------------------------------------------
Uint16 chr_find_target( chr_t * psrc, float max_dist2, TARGET_TYPE target_type, bool_t target_items, bool_t target_dead, IDSZ target_idsz, bool_t exclude_idsz, bool_t target_players )
{
    // BB> this is the raw character targeting code, this is not throttled at all. You should call
    //     scr_get_chr_target() if you are calling this function from the scripting system.

    line_of_sight_info_t los_info;

    Uint16 cnt;
    Uint16 best_target = MAX_CHR;
    float  best_dist2;
	Uint16 search_list = target_players ? MAXPLAYER : MAX_CHR;

    if( TARGET_NONE == target_type ) return MAX_CHR;

    if( !ACTIVE_PCHR( psrc ) ) return MAX_CHR;

    // set the line-of-sight source
    los_info.x0         = psrc->pos.x;
    los_info.y0         = psrc->pos.y;
    los_info.z0         = psrc->pos.z + psrc->bump.height;
    los_info.stopped_by = psrc->stoppedby;

    best_target = MAX_CHR;
    best_dist2  = max_dist2;
    for ( cnt = 0; cnt < search_list; cnt++ )
    {
        float  dist2;
        GLvector3 diff;
        chr_t * ptst;
		Uint16 ichr_test = target_players ? PlaList[cnt].index : cnt;
		
        if( !ACTIVE_CHR(ichr_test) ) continue;
        ptst = ChrList.lst + ichr_test;

        if( !check_target( psrc, ichr_test, target_type, target_items, target_dead, target_idsz, exclude_idsz, target_players) )
        {
            continue;
        }

        diff  = VSub( psrc->pos, ptst->pos );
        dist2 = VDotProduct( diff, diff );

        if ( (0 == max_dist2 || dist2 <= max_dist2) && (MAX_CHR == best_target || dist2 < best_dist2) )
        {
            // set the line-of-sight source
            los_info.x1 = ptst->pos.x;
            los_info.y1 = ptst->pos.y;
            los_info.z1 = ptst->pos.z + MAX(1, ptst->bump.height);

            if ( !do_line_of_sight( &los_info ) )
            {
                best_target = ichr_test;
                best_dist2  = dist2;
            }
        }		
    }

    // make sure the target is valid
    if ( !ACTIVE_CHR(best_target) ) best_target = MAX_CHR;

    return best_target;
}

//--------------------------------------------------------------------------------------------
void do_damage_tiles()
{
    Uint16 character, distance;

    // do the damage tile stuff
    for ( character = 0; character < MAX_CHR; character++ )
    {
        cap_t * pcap;
        chr_t * pchr;

        if ( !ACTIVE_CHR(character) ) continue;
        pchr = ChrList.lst + character;

        pcap = pro_get_pcap( pchr->iprofile );
        if( NULL == pcap ) continue;

        if( pchr->pack_ispacked || pchr->is_hidden ) continue;

        // do damage tile stuff
        if ( pchr->alive )
        {
            if ( VALID_TILE( PMesh, pchr->onwhichfan ) &&
                    ( 0 != mesh_test_fx( PMesh, pchr->onwhichfan, MPDFX_DAMAGE ) ) &&
                    ( pchr->pos.z <= pchr->enviro.floor_level + DAMAGERAISE ) &&
                    ( MAX_CHR == pchr->attachedto ) )
            {
                if ( ( pchr->damagemodifier[damagetile.type]&DAMAGESHIFT ) != 3 && !pchr->invictus ) // 3 means they're pretty well immune
                {
                    distance = ABS( PCamera->track_pos.x - pchr->pos.x ) + ABS( PCamera->track_pos.y - pchr->pos.y );
                    if ( distance < damagetile.min_distance )
                    {
                        damagetile.min_distance = distance;
                    }
                    if ( distance < damagetile.min_distance + 256 )
                    {
                        damagetile.sound_time = 0;
                    }
                    if ( pchr->damagetime == 0 )
                    {
                        damage_character( character, ATK_BEHIND, damagetile.amount, damagetile.type, TEAM_DAMAGE, MAX_CHR, DAMFX_NBLOC | DAMFX_ARMO, bfalse );
                        pchr->damagetime = DAMAGETILETIME;
                    }
                    if ( (damagetile.parttype != ((Sint16)~0)) && ( update_wld & damagetile.partand ) == 0 )
                    {
                        spawn_one_particle( pchr->pos, 0, MAX_PROFILE, damagetile.parttype,
                                            MAX_CHR, GRIP_LAST, TEAM_NULL, MAX_CHR, TOTAL_MAX_PRT, 0, MAX_CHR );
                    }
                }

                if ( pchr->reaffirmdamagetype == damagetile.type )
                {
                    if ( 0 == ( update_wld&TILEREAFFIRMAND ) )
                    {
                        reaffirm_attached_particles( character );
                    }
                }
            }
        }
    }



};

//--------------------------------------------------------------------------------------------
Uint16 terp_dir( Uint16 majordir, Uint16 minordir )
{
    // ZZ> This function returns a direction between the major and minor ones, closer
    //    to the major.
    Uint16 temp;

    // Align major direction with 0
    minordir -= majordir;

    if ( minordir > 0x8000 )
    {
        temp = 0xFFFF;
    }
    else
    {
        temp = 0;
    }

    minordir  = ( minordir + ( temp << 3 ) - temp ) >> 3;
    minordir += majordir;

    return minordir;
}

//--------------------------------------------------------------------------------------------
Uint16 terp_dir_fast( Uint16 majordir, Uint16 minordir )
{
    // ZZ> This function returns a direction between the major and minor ones, closer
    //    to the major, but not by much.  Makes turning faster.
    Uint16 temp;

    // Align major direction with 0
    minordir -= majordir;

    if ( minordir > 0x8000 )
    {
        temp = 0xFFFF;
    }
    else
    {
        temp = 0;
    }

    minordir = ( minordir + ( temp << 1 ) - temp ) >> 1;
    minordir += majordir;

    return minordir;
}

//--------------------------------------------------------------------------------------------
void update_pits()
{
    // ZZ> This function kills any character in a deep pit...
    int cnt;

    if ( pitskill || pitsfall )
    {
        if ( clock_pit > 19 )
        {
            clock_pit = 0;

            // Kill any particles that fell in a pit, if they die in water...
            for ( cnt = 0; cnt < maxparticles; cnt++ )
            {
                if ( !ACTIVE_PRT( cnt ) || INVALID_PIP( PrtList.lst[cnt].pip_ref ) ) continue;

                if ( PrtList.lst[cnt].pos.z < PITDEPTH && prt_get_ppip(cnt)->endwater )
                {
                    prt_request_terminate( cnt );
                }
            }

            // Kill or teleport any characters that fell in a pit...
            for ( cnt = 0; cnt < MAX_CHR; cnt++ )
            {
                // Is it a valid character?
                if ( !ACTIVE_CHR( cnt ) || ChrList.lst[cnt].invictus || !ChrList.lst[cnt].alive  ) continue;
                if ( ChrList.lst[cnt].attachedto != MAX_CHR || ChrList.lst[cnt].pack_ispacked ) continue;

                // Do we kill it?
                if ( pitskill && ChrList.lst[cnt].pos.z < PITDEPTH )
                {
                    // Got one!
                    kill_character( cnt, MAX_CHR );
                    ChrList.lst[cnt].vel.x = 0;
                    ChrList.lst[cnt].vel.y = 0;

                    // Play sound effect
                    sound_play_chunk( ChrList.lst[cnt].pos, g_wavelist[GSND_PITFALL] );
                }

                // Do we teleport it?
                if ( pitsfall && ChrList.lst[cnt].pos.z < PITDEPTH << 3 )
                {
                    bool_t teleported;

                    // Teleport them back to a "safe" spot
                    teleported = chr_teleport(cnt, pitx, pity, pitz, ChrList.lst[cnt].turn_z );

                    if ( !teleported )
                    {
                        // Kill it instead
                        kill_character( cnt, MAX_CHR );
                    }
                    else
                    {
                        // Stop movement
                        ChrList.lst[cnt].vel.z = 0;
                        ChrList.lst[cnt].vel.x = 0;
                        ChrList.lst[cnt].vel.y = 0;

                        // Play sound effect
                        if (ChrList.lst[cnt].isplayer)
                        {
                            sound_play_chunk( PCamera->track_pos, g_wavelist[GSND_PITFALL] );
                        }
                        else
                        {
                            sound_play_chunk( ChrList.lst[cnt].pos, g_wavelist[GSND_PITFALL] );
                        }

                        // Do some damage (same as damage tile)
                        damage_character( cnt, ATK_BEHIND, damagetile.amount, damagetile.type, TEAM_DAMAGE, chr_get_pai(cnt)->bumplast, DAMFX_NBLOC | DAMFX_ARMO, btrue );
                    }
                }
            }
        }
        else
        {
            clock_pit++;
        }
    }
}

//--------------------------------------------------------------------------------------------
void do_weather_spawn_particles()
{
    // ZZ> This function drops snowflakes or rain or whatever, also swings the camera
    int particle, cnt;
    bool_t foundone;

    if ( weather.time > 0 )
    {
        weather.time--;
        if ( weather.time == 0 )
        {
            weather.time = weather.timer_reset;

            // Find a valid player
            foundone = bfalse;
            cnt = 0;

            while ( cnt < MAXPLAYER )
            {
                weather.iplayer = ( weather.iplayer + 1 ) & ( MAXPLAYER - 1 );
                if ( PlaList[weather.iplayer].valid )
                {
                    foundone = btrue;
                    break;
                }
                cnt++;
            }

            // Did we find one?
            if ( foundone )
            {
                // Yes, but is the character valid?
                cnt = PlaList[weather.iplayer].index;
                if ( ACTIVE_CHR(cnt) && !ChrList.lst[cnt].pack_ispacked )
                {
                    // Yes, so spawn over that character
                    particle = spawn_one_particle( ChrList.lst[cnt].pos, 0, MAX_PROFILE, PIP_WEATHER4, MAX_CHR, GRIP_LAST, TEAM_NULL, MAX_CHR, TOTAL_MAX_PRT, 0, MAX_CHR );

                    if ( ACTIVE_PRT(particle) )
                    {
                        bool_t destroy_particle = bfalse;

                        if ( __prthitawall( particle ) )
                        {
                            destroy_particle = btrue;
                        }
                        else if ( weather.over_water && !prt_is_over_water( particle ) )
                        {
                            destroy_particle = btrue;
                        }
                        else
                        {
                            //Weather particles spawned at the edge of the map look ugly, so don't spawn them there
                            float xpos = PrtList.lst[particle].pos.x;
                            float ypos = PrtList.lst[particle].pos.y;

                            if(      xpos < EDGE || xpos > PMesh->info.edge_x - EDGE) destroy_particle = btrue;
                            else if( ypos < EDGE || ypos > PMesh->info.edge_y - EDGE) destroy_particle = btrue;
                        }

                        if( destroy_particle )
                        {
                            EGO_OBJECT_TERMINATE( PrtList.lst + particle );
                            PrtList_free_one( particle );
                        };
                    }
                }
            }
        }
    }

    PCamera->swing = ( PCamera->swing + PCamera->swingrate ) & 16383;
}

//--------------------------------------------------------------------------------------------
void set_one_player_latch( Uint16 player )
{
    // ZZ> This function converts input readings to latch settings, so players can
    //    move around
    float newx, newy;
    Uint16 turnsin;
    float dist, scale;
    float inputx, inputy;
    float fsin, fcos;
    latch_t sum;

    chr_t          * pchr;
    player_t       * ppla;
    input_device_t * pdevice;

    if ( INVALID_PLA(player) ) return;
    ppla = PlaList + player;

    pdevice = &(ppla->device);

    if( !ACTIVE_CHR(ppla->index) ) return;
    pchr = ChrList.lst + ppla->index;

    // is the device a local device or an internet device?
    if( pdevice->bits == INPUT_BITS_NONE ) return;

    // Clear the player's latch buffers
    latch_init( &(sum) );

    // generate the transforms relative to the camera
    turnsin = PCamera->turn_z >> 2;
    fsin    = turntosin[turnsin & TRIG_TABLE_MASK ];
    fcos    = turntocos[turnsin & TRIG_TABLE_MASK ];

    // Mouse routines
    if ( HAS_SOME_BITS( pdevice->bits , INPUT_BITS_MOUSE ) && mous.on )
    {
        // Movement
        newx = 0;
        newy = 0;
        if ( ( PCamera->turn_mode == CAMTURN_GOOD && local_numlpla == 1 ) ||
            !control_is_pressed( INPUT_DEVICE_MOUSE,  CONTROL_CAMERA ) )  // Don't allow movement in camera control mode
        {
            dist = SQRT( mous.x * mous.x + mous.y * mous.y );
            if ( dist > 0 )
            {
                scale = mous.sense / dist;
                if ( dist < mous.sense )
                {
                    scale = dist / mous.sense;
                }

                scale = scale / mous.sense;
                inputx = mous.x * scale;
                inputy = mous.y * scale;

                if ( PCamera->turn_mode == CAMTURN_GOOD &&
                    local_numlpla == 1 &&
                    control_is_pressed( INPUT_DEVICE_MOUSE,  CONTROL_CAMERA ) == 0 )  inputx = 0;

                newx = ( inputx * fcos + inputy * fsin );
                newy = (-inputx * fsin + inputy * fcos );
            }
        }

        sum.x += newx;
        sum.y += newy;

        // Read buttons
        if ( control_is_pressed( INPUT_DEVICE_MOUSE,  CONTROL_JUMP ) )
            sum.b |= LATCHBUTTON_JUMP;
        if ( control_is_pressed( INPUT_DEVICE_MOUSE,  CONTROL_LEFT_USE ) )
            sum.b |= LATCHBUTTON_LEFT;
        if ( control_is_pressed( INPUT_DEVICE_MOUSE,  CONTROL_LEFT_GET ) )
            sum.b |= LATCHBUTTON_ALTLEFT;
        if ( control_is_pressed( INPUT_DEVICE_MOUSE,  CONTROL_LEFT_PACK ) )
            sum.b |= LATCHBUTTON_PACKLEFT;
        if ( control_is_pressed( INPUT_DEVICE_MOUSE,  CONTROL_RIGHT_USE ) )
            sum.b |= LATCHBUTTON_RIGHT;
        if ( control_is_pressed( INPUT_DEVICE_MOUSE,  CONTROL_RIGHT_GET ) )
            sum.b |= LATCHBUTTON_ALTRIGHT;
        if ( control_is_pressed( INPUT_DEVICE_MOUSE,  CONTROL_RIGHT_PACK ) )
            sum.b |= LATCHBUTTON_PACKRIGHT;
    }

    // Joystick A routines
    if ( HAS_SOME_BITS( pdevice->bits , INPUT_BITS_JOYA ) && joy[0].on )
    {
        newx = 0;
        newy = 0;
        // Movement
        if ( ( PCamera->turn_mode == CAMTURN_GOOD && local_numlpla == 1 ) ||
            !control_is_pressed( INPUT_DEVICE_JOY + 0, CONTROL_CAMERA ) )
        {
            inputx = joy[0].x;
            inputy = joy[0].y;

            dist = inputx * inputx + inputy * inputy;
            if ( dist > 1.0f )
            {
                scale = 1.0f / SQRT( dist );
                inputx *= scale;
                inputy *= scale;
            }

            if ( PCamera->turn_mode == CAMTURN_GOOD &&
                local_numlpla == 1 &&
                !control_is_pressed( INPUT_DEVICE_JOY + 0, CONTROL_CAMERA ) )  inputx = 0;

            newx = (  inputx * fcos + inputy * fsin );
            newy = ( -inputx * fsin + inputy * fcos );
        }

        sum.x += newx;
        sum.y += newy;

        // Read buttons
        if ( control_is_pressed( INPUT_DEVICE_JOY + 0, CONTROL_JUMP ) )
            sum.b |= LATCHBUTTON_JUMP;
        if ( control_is_pressed( INPUT_DEVICE_JOY + 0, CONTROL_LEFT_USE ) )
            sum.b |= LATCHBUTTON_LEFT;
        if ( control_is_pressed( INPUT_DEVICE_JOY + 0, CONTROL_LEFT_GET ) )
            sum.b |= LATCHBUTTON_ALTLEFT;
        if ( control_is_pressed( INPUT_DEVICE_JOY + 0, CONTROL_LEFT_PACK ) )
            sum.b |= LATCHBUTTON_PACKLEFT;
        if ( control_is_pressed( INPUT_DEVICE_JOY + 0, CONTROL_RIGHT_USE ) )
            sum.b |= LATCHBUTTON_RIGHT;
        if ( control_is_pressed( INPUT_DEVICE_JOY + 0, CONTROL_RIGHT_GET ) )
            sum.b |= LATCHBUTTON_ALTRIGHT;
        if ( control_is_pressed( INPUT_DEVICE_JOY + 0, CONTROL_RIGHT_PACK ) )
            sum.b |= LATCHBUTTON_PACKRIGHT;
    }

    // Joystick B routines
    if ( HAS_SOME_BITS( pdevice->bits , INPUT_BITS_JOYB ) && joy[1].on )
    {
        newx = 0;
        newy = 0;

        // Movement
        if ( ( PCamera->turn_mode == CAMTURN_GOOD && local_numlpla == 1 ) ||
            !control_is_pressed( INPUT_DEVICE_JOY + 1, CONTROL_CAMERA ) )
        {
            inputx = joy[1].x;
            inputy = joy[1].y;

            dist = inputx * inputx + inputy * inputy;
            if ( dist > 1.0f )
            {
                scale = 1.0f / SQRT( dist );
                inputx *= scale;
                inputy *= scale;
            }

            if ( PCamera->turn_mode == CAMTURN_GOOD &&
                local_numlpla == 1 &&
                !control_is_pressed( INPUT_DEVICE_JOY + 1, CONTROL_CAMERA ) )  inputx = 0;

            newx = (  inputx * fcos + inputy * fsin );
            newy = ( -inputx * fsin + inputy * fcos );
        }

        sum.x += newx;
        sum.y += newy;

        // Read buttons
        if ( control_is_pressed( INPUT_DEVICE_JOY + 1, CONTROL_JUMP ) )
            sum.b |= LATCHBUTTON_JUMP;
        if ( control_is_pressed( INPUT_DEVICE_JOY + 1, CONTROL_LEFT_USE ) )
            sum.b |= LATCHBUTTON_LEFT;
        if ( control_is_pressed( INPUT_DEVICE_JOY + 1, CONTROL_LEFT_GET ) )
            sum.b |= LATCHBUTTON_ALTLEFT;
        if ( control_is_pressed( INPUT_DEVICE_JOY + 1, CONTROL_LEFT_PACK ) )
            sum.b |= LATCHBUTTON_PACKLEFT;
        if ( control_is_pressed( INPUT_DEVICE_JOY + 1, CONTROL_RIGHT_USE ) )
            sum.b |= LATCHBUTTON_RIGHT;
        if ( control_is_pressed( INPUT_DEVICE_JOY + 1, CONTROL_RIGHT_GET ) )
            sum.b |= LATCHBUTTON_ALTRIGHT;
        if ( control_is_pressed( INPUT_DEVICE_JOY + 1, CONTROL_RIGHT_PACK ) )
            sum.b |= LATCHBUTTON_PACKRIGHT;
    }

    // Keyboard routines
    if ( HAS_SOME_BITS( pdevice->bits , INPUT_BITS_KEYBOARD ) && keyb.on )
    {
        // Movement

        // ???? is this if statement doing anything ????
        if ( ACTIVE_CHR(pchr->attachedto) )
        {
            // Mounted
            inputx = ( control_is_pressed( INPUT_DEVICE_KEYBOARD,  CONTROL_RIGHT ) - control_is_pressed( INPUT_DEVICE_KEYBOARD,  CONTROL_LEFT ) );
            inputy = ( control_is_pressed( INPUT_DEVICE_KEYBOARD,  CONTROL_DOWN ) - control_is_pressed( INPUT_DEVICE_KEYBOARD,  CONTROL_UP ) );
        }
        else
        {
            // Unmounted
            inputx = ( control_is_pressed( INPUT_DEVICE_KEYBOARD,  CONTROL_RIGHT ) - control_is_pressed( INPUT_DEVICE_KEYBOARD,  CONTROL_LEFT ) );
            inputy = ( control_is_pressed( INPUT_DEVICE_KEYBOARD,  CONTROL_DOWN ) - control_is_pressed( INPUT_DEVICE_KEYBOARD,  CONTROL_UP ) );
        }

        if ( PCamera->turn_mode == CAMTURN_GOOD && local_numlpla == 1 )  inputx = 0;

        newx = (  inputx * fcos + inputy * fsin );
        newy = ( -inputx * fsin + inputy * fcos );

        sum.x += newx;
        sum.y += newy;

        // Read buttons
        if ( control_is_pressed( INPUT_DEVICE_KEYBOARD,  CONTROL_JUMP ) )
            sum.b |= LATCHBUTTON_JUMP;
        if ( control_is_pressed( INPUT_DEVICE_KEYBOARD,  CONTROL_LEFT_USE ) )
            sum.b |= LATCHBUTTON_LEFT;
        if ( control_is_pressed( INPUT_DEVICE_KEYBOARD,  CONTROL_LEFT_GET ) )
            sum.b |= LATCHBUTTON_ALTLEFT;
        if ( control_is_pressed( INPUT_DEVICE_KEYBOARD,  CONTROL_LEFT_PACK ) )
            sum.b |= LATCHBUTTON_PACKLEFT;
        if ( control_is_pressed( INPUT_DEVICE_KEYBOARD,  CONTROL_RIGHT_USE ) )
            sum.b |= LATCHBUTTON_RIGHT;
        if ( control_is_pressed( INPUT_DEVICE_KEYBOARD,  CONTROL_RIGHT_GET ) )
            sum.b |= LATCHBUTTON_ALTRIGHT;
        if ( control_is_pressed( INPUT_DEVICE_KEYBOARD,  CONTROL_RIGHT_PACK ) )
            sum.b |= LATCHBUTTON_PACKRIGHT;
    }

    input_device_add_latch( pdevice, sum.x, sum.y );

    ppla->latch.x = pdevice->latch.x;
    ppla->latch.y = pdevice->latch.y;
    ppla->latch.b = sum.b;
}

//--------------------------------------------------------------------------------------------
void set_local_latches( void )
{
    // ZZ> This function emulates AI thinkin' by setting latches from input devices
    int cnt;

    for ( cnt = 0; cnt < MAXPLAYER; cnt++ )
    {
        set_one_player_latch( cnt );
    }
}

//--------------------------------------------------------------------------------------------
void check_stats()
{
    // ZZ> This function lets the players check character stats

    static int stat_check_timer = 0;
    static int stat_check_delay = 0;

    int ticks;
    if ( console_mode ) return;

    ticks = SDL_GetTicks();
    if ( ticks > stat_check_timer + 20 )
    {
        stat_check_timer = ticks;
    }

    stat_check_delay -= 20;
    if ( stat_check_delay > 0 )
        return;

    // Show map cheat
    if ( cfg.dev_mode && SDLKEYDOWN( SDLK_m ) && SDLKEYDOWN( SDLK_LSHIFT ) && mapvalid )
    {
        mapon = !mapon;
        youarehereon = btrue;
        stat_check_delay = 150;
    }

    // !!!BAD!!!  XP CHEAT
    if ( cfg.dev_mode && SDLKEYDOWN( SDLK_x ) )
    {
        if ( SDLKEYDOWN( SDLK_1 ) && ACTIVE_CHR(PlaList[0].index) )  { pla_get_pchr(0)->experience = ((float)pla_get_pchr(0)->experience+10)*1.10f; stat_check_delay = 1; }
        if ( SDLKEYDOWN( SDLK_2 ) && ACTIVE_CHR(PlaList[1].index) )  { pla_get_pchr(1)->experience = ((float)pla_get_pchr(1)->experience+10)*1.10f; stat_check_delay = 1; }
        if ( SDLKEYDOWN( SDLK_3 ) && ACTIVE_CHR(PlaList[2].index) )  { pla_get_pchr(2)->experience = ((float)pla_get_pchr(2)->experience+10)*1.10f; stat_check_delay = 1; }
        if ( SDLKEYDOWN( SDLK_4 ) && ACTIVE_CHR(PlaList[3].index) )  { pla_get_pchr(3)->experience = ((float)pla_get_pchr(3)->experience+10)*1.10f; stat_check_delay = 1; }

    }

    // !!!BAD!!!  LIFE CHEAT
    if ( cfg.dev_mode && SDLKEYDOWN( SDLK_z ) )
    {
        if ( SDLKEYDOWN( SDLK_1 ) && ACTIVE_CHR(PlaList[0].index) )  { pla_get_pchr(0)->life += 32; pla_get_pchr(0)->life = MIN(pla_get_pchr(0)->life, pla_get_pchr(0)->lifemax); stat_check_delay = 12; }
        if ( SDLKEYDOWN( SDLK_2 ) && ACTIVE_CHR(PlaList[1].index) )  { pla_get_pchr(1)->life += 32; pla_get_pchr(0)->life = MIN(pla_get_pchr(1)->life, pla_get_pchr(1)->lifemax); stat_check_delay = 12; }
        if ( SDLKEYDOWN( SDLK_3 ) && ACTIVE_CHR(PlaList[2].index) )  { pla_get_pchr(2)->life += 32; pla_get_pchr(0)->life = MIN(pla_get_pchr(2)->life, pla_get_pchr(2)->lifemax); stat_check_delay = 12; }
        if ( SDLKEYDOWN( SDLK_4 ) && ACTIVE_CHR(PlaList[3].index) )  { pla_get_pchr(3)->life += 32; pla_get_pchr(0)->life = MIN(pla_get_pchr(3)->life, pla_get_pchr(3)->lifemax); stat_check_delay = 12; }
    }

    // Display armor stats?
    if ( SDLKEYDOWN( SDLK_LSHIFT ) )
    {
        if ( SDLKEYDOWN( SDLK_1 ) )  { show_armor( 0 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_2 ) )  { show_armor( 1 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_3 ) )  { show_armor( 2 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_4 ) )  { show_armor( 3 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_5 ) )  { show_armor( 4 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_6 ) )  { show_armor( 5 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_7 ) )  { show_armor( 6 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_8 ) )  { show_armor( 7 ); stat_check_delay = 1000; }
    }

    // Display enchantment stats?
    else if (  SDLKEYDOWN( SDLK_LCTRL ) )
    {
        if ( SDLKEYDOWN( SDLK_1 ) )  { show_full_status( 0 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_2 ) )  { show_full_status( 1 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_3 ) )  { show_full_status( 2 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_4 ) )  { show_full_status( 3 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_5 ) )  { show_full_status( 4 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_6 ) )  { show_full_status( 5 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_7 ) )  { show_full_status( 6 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_8 ) )  { show_full_status( 7 ); stat_check_delay = 1000; }
    }

    // Display character special powers?
    else if ( SDLKEYDOWN( SDLK_LALT ) )
    {
        if ( SDLKEYDOWN( SDLK_1 ) )  { show_magic_status( 0 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_2 ) )  { show_magic_status( 1 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_3 ) )  { show_magic_status( 2 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_4 ) )  { show_magic_status( 3 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_5 ) )  { show_magic_status( 4 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_6 ) )  { show_magic_status( 5 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_7 ) )  { show_magic_status( 6 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_8 ) )  { show_magic_status( 7 ); stat_check_delay = 1000; }
    }

    // Display character stats?
    else
    {
        if ( SDLKEYDOWN( SDLK_1 ) )  { show_stat( 0 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_2 ) )  { show_stat( 1 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_3 ) )  { show_stat( 2 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_4 ) )  { show_stat( 3 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_5 ) )  { show_stat( 4 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_6 ) )  { show_stat( 5 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_7 ) )  { show_stat( 6 ); stat_check_delay = 1000; }
        if ( SDLKEYDOWN( SDLK_8 ) )  { show_stat( 7 ); stat_check_delay = 1000; }
    }

}

//--------------------------------------------------------------------------------------------
void show_stat( Uint16 statindex )
{
    // ZZ> This function shows the more specific stats for a character
    int character, level;
    char gender[8] = EMPTY_CSTR;

    if ( statindex < numstat )
    {
        character = statlist[statindex];

        if( ACTIVE_CHR(character) )
        {
            cap_t * pcap;
            chr_t * pchr = ChrList.lst + character;

            pcap = pro_get_pcap( pchr->iprofile );

            // Name
            debug_printf( "=%s=", pchr->name );

            // Level and gender and class
            gender[0] = 0;
            if ( pchr->alive )
            {
                int itmp;
                char * gender_str;

                gender_str = "";
                switch( pchr->gender )
                {
                    case GENDER_MALE: gender_str = "male "; break;
                    case GENDER_FEMALE: gender_str = "female "; break;
                }

                level = 1 + pchr->experiencelevel;
                itmp = level % 10;
                if ( 1 == itmp )
                {
                    debug_printf( "~%dst level %s%s", level, gender_str, pcap->classname );
                }
                else if ( 2 == itmp )
                {
                    debug_printf( "~%dnd level %s%s", level, gender_str, pcap->classname );
                }
                else if ( 3 == itmp )
                {
                    debug_printf( "~%drd level %s%s", level, gender_str, pcap->classname );
                }
                else
                {
                    debug_printf( "~%dth level %s%s", level, gender_str, pcap->classname );
                }
            }
            else
            {
                debug_printf( "~Dead %s", pcap->classname );
            }

            // Stats
            debug_printf( "~STR:~%2d~WIS:~%2d~DEF:~%d", FP8_TO_INT( pchr->strength ), FP8_TO_INT( pchr->wisdom ), 255 - pchr->defense );
            debug_printf( "~INT:~%2d~DEX:~%2d~EXP:~%d", FP8_TO_INT( pchr->intelligence ), FP8_TO_INT( pchr->dexterity ), pchr->experience );
        }
    }
}

//--------------------------------------------------------------------------------------------
void show_armor( Uint16 statindex )
{
    // ZF> This function shows detailed armor information for the character

    STRING tmps;
    Uint16 ichr;

    Uint8  skinlevel;

    cap_t * pcap;
    chr_t * pchr;

    if ( statindex >= numstat ) return;

    ichr = statlist[statindex];
    if ( !ACTIVE_CHR(ichr) ) return;

    pchr = ChrList.lst + ichr;
    skinlevel = pchr->skin;

    pcap = chr_get_pcap(ichr);
    if( NULL == pcap ) return;

    // Armor Name
    debug_printf( "=%s=", pcap->skinname[skinlevel] );

    // Armor Stats
    debug_printf( "~DEF: %d  SLASH:%3d~CRUSH:%3d POKE:%3d", 255 - pcap->defense[skinlevel],
        pcap->damagemodifier[0][skinlevel]&DAMAGESHIFT,
        pcap->damagemodifier[1][skinlevel]&DAMAGESHIFT,
        pcap->damagemodifier[2][skinlevel]&DAMAGESHIFT );

    debug_printf( "~HOLY:~%i~EVIL:~%i~FIRE:~%i~ICE:~%i~ZAP:~%i",
        pcap->damagemodifier[3][skinlevel]&DAMAGESHIFT,
        pcap->damagemodifier[4][skinlevel]&DAMAGESHIFT,
        pcap->damagemodifier[5][skinlevel]&DAMAGESHIFT,
        pcap->damagemodifier[6][skinlevel]&DAMAGESHIFT,
        pcap->damagemodifier[7][skinlevel]&DAMAGESHIFT );

    debug_printf( "~Type: %s", ( pcap->skindressy & (1 << skinlevel) ) ? "Light Armor" : "Heavy Armor" );

    // jumps
    tmps[0] = CSTR_END;
    switch ( pcap->jumpnumber )
    {
        case 0:  snprintf( tmps, SDL_arraysize( tmps), "None    (%i)", pchr->jumpnumberreset ); break;
        case 1:  snprintf( tmps, SDL_arraysize( tmps), "Novice  (%i)", pchr->jumpnumberreset ); break;
        case 2:  snprintf( tmps, SDL_arraysize( tmps), "Skilled (%i)", pchr->jumpnumberreset ); break;
        case 3:  snprintf( tmps, SDL_arraysize( tmps), "Adept   (%i)", pchr->jumpnumberreset ); break;
        default: snprintf( tmps, SDL_arraysize( tmps), "Master  (%i)", pchr->jumpnumberreset ); break;
    };

    debug_printf( "~Speed:~%3.0f~Jump Skill:~%s", pchr->maxaccel*80, tmps );
}

//--------------------------------------------------------------------------------------------
void show_full_status( Uint16 statindex )
{
    // ZF> This function shows detailed armor information for the character including magic

    Uint16 character, enchant;
    float manaregen, liferegen;
    chr_t * pchr;

    if ( statindex >= numstat ) return;

    character = statlist[statindex];
    if( !ACTIVE_CHR(character) ) return;
    pchr = ChrList.lst + character;

    // clean up the enchant list
    pchr->firstenchant = cleanup_enchant_list(pchr->firstenchant);

    // Enchanted?
    debug_printf( "=%s is %s=", pchr->name, ACTIVE_ENC(pchr->firstenchant) ? "enchanted" : "unenchanted" );

    // Armor Stats
    debug_printf( "~DEF: %d  SLASH:%3d~CRUSH:%3d POKE:%3d",
                255 - pchr->defense,
                pchr->damagemodifier[0]&DAMAGESHIFT,
                pchr->damagemodifier[1]&DAMAGESHIFT,
                pchr->damagemodifier[2]&DAMAGESHIFT );

    debug_printf( "~HOLY: %i~~EVIL:~%i~FIRE:~%i~ICE:~%i~ZAP: ~%i",
                pchr->damagemodifier[3]&DAMAGESHIFT,
                pchr->damagemodifier[4]&DAMAGESHIFT,
                pchr->damagemodifier[5]&DAMAGESHIFT,
                pchr->damagemodifier[6]&DAMAGESHIFT,
                pchr->damagemodifier[7]&DAMAGESHIFT );

    // Life and mana regeneration
    // Don't forget to add gains and costs from enchants
    manaregen = pchr->manareturn / MANARETURNSHIFT;
    liferegen = pchr->lifereturn;
    for ( enchant = 0; enchant < MAX_ENC; enchant++ )
    {
        enc_t * penc;

        if ( !ACTIVE_ENC(enchant) ) continue;
        penc = EncList.lst + enchant;

        if ( penc->target_ref == character )
        {
            liferegen += penc->targetlife;
            manaregen += penc->targetmana;
        }
        if ( penc->owner_ref == character )
        {
            liferegen += penc->ownerlife;
            manaregen += penc->ownermana;
        }
    }

    debug_printf( "Mana Regen:~%4.2f Life Regen:~%4.2f", FP8_TO_FLOAT(manaregen), FP8_TO_FLOAT(liferegen) );
}

//--------------------------------------------------------------------------------------------
void show_magic_status( Uint16 statindex )
{
    // ZF> Displays special enchantment effects for the character

    Uint16 character;
    char * missile_str;
    chr_t * pchr;

    if ( statindex >= numstat ) return;

    character = statlist[statindex];

    if( !ACTIVE_CHR(character) ) return;
    pchr = ChrList.lst + character;

    // clean up the enchant list
    pchr->firstenchant = cleanup_enchant_list(pchr->firstenchant);

    // Enchanted?
    if ( ACTIVE_ENC(pchr->firstenchant) )
    {
        debug_printf( "=%s is enchanted!=", pchr->name );
    }
    else
    {
        debug_printf( "=%s is unenchanted=", pchr->name );
    }

    // Enchantment status
    debug_printf( "~See Invisible: %s~~See Kurses: %s",
        pchr->canseeinvisible ? "Yes" : "No",
        pchr->canseekurse ? "Yes" : "No" );

    debug_printf( "~Channel Life: %s~~Waterwalking: %s",
        pchr->canchannel ? "Yes" : "No",
        pchr->waterwalk ? "Yes" : "No" );

    switch( pchr->missiletreatment )
    {
    case MISSILE_REFLECT: missile_str = "Reflect"; break;
    case MISSILE_DEFLECT: missile_str = "Deflect"; break;

    default:
    case MISSILE_NORMAL : missile_str = "None";    break;
    }

    debug_printf( "~Flying: %s~~Missile Protection: %s", (pchr->flyheight > 0) ? "Yes" : "No", missile_str );
}

//--------------------------------------------------------------------------------------------
void fill_bumplists()
{
    Uint16 character, particle;
    Uint32 fanblock;

    // Clear the lists
    for ( fanblock = 0; fanblock < PMesh->info.blocks_count; fanblock++ )
    {
        bumplist[fanblock].chr    = MAX_CHR;
        bumplist[fanblock].chrnum = 0;

        bumplist[fanblock].prt    = TOTAL_MAX_PRT;
        bumplist[fanblock].prtnum = 0;
    }

    // Fill 'em back up
    for ( character = 0; character < MAX_CHR; character++ )
    {
        chr_t * pchr;

        if ( !ACTIVE_CHR(character) ) continue;
        pchr = ChrList.lst + character;

        // reset the platform stuff each update
        pchr->holdingweight   = 0;
        pchr->onwhichplatform = MAX_CHR;
        pchr->enviro.level    = pchr->enviro.floor_level;

        // reset the fan and block position
        pchr->onwhichfan   = mesh_get_tile ( PMesh, pchr->pos.x, pchr->pos.y );
        pchr->onwhichblock = mesh_get_block( PMesh, pchr->pos.x, pchr->pos.y );

        // reject characters that are in packs, or are marked as non-colliding
        if ( pchr->pack_ispacked ) continue;

        // reject characters that are hidden
        if ( pchr->is_hidden ) continue;

        if ( INVALID_BLOCK != pchr->onwhichblock )
        {
            // Insert before any other characters on the block
            pchr->bumplist_next = bumplist[pchr->onwhichblock].chr;
            bumplist[pchr->onwhichblock].chr = character;
            bumplist[pchr->onwhichblock].chrnum++;
        }
    }

    for ( particle = 0; particle < maxparticles; particle++ )
    {
        prt_t * pprt;

        // reject invalid particles
        if ( !ACTIVE_PRT(particle) ) continue;
        pprt = PrtList.lst + particle;

        // reject characters that are hidden
        if ( pprt->is_hidden ) continue;

        // reset the fan and block position
        pprt->onwhichfan   = mesh_get_tile ( PMesh, pprt->pos.x, pprt->pos.y );
        pprt->onwhichblock = mesh_get_block( PMesh, pprt->pos.x, pprt->pos.y );

        if ( INVALID_BLOCK != pprt->onwhichblock )
        {
            // Insert before any other particles on the block
            pprt->bumplist_next = bumplist[pprt->onwhichblock].prt;
            bumplist[pprt->onwhichblock].prt = particle;
            bumplist[pprt->onwhichblock].prtnum++;
        }
    }
}

//--------------------------------------------------------------------------------------------
void fill_interaction_list( co_data_t cdata[], int * cdata_count, hash_node_t hnlst[], int * hn_count )
{
    Uint16 ichr_a, ichr_b, iprt_b;

    Uint32 fanblock;
    int tnc, chrinblock, prtinblock;
    int cnt;

    if (NULL == chr_co_list)
    {
        chr_co_list = hash_list_create(-1);
        assert(NULL != chr_co_list);
    }

    // renew the collision list. Since we are filling this list with pre-allocated hash_node_t's,
    // there is no need to delete any of the existing chr_co_list->sublist elements
    for (cnt = 0; cnt < 256; cnt++)
    {
        chr_co_list->subcount[cnt] = 0;
        chr_co_list->sublist[cnt]  = NULL;
    }
    (*cdata_count) = 0;
    (*hn_count)    = 0;

    for ( ichr_a = 0; ichr_a < MAX_CHR; ichr_a++ )
    {
        int ixmax, ixmin;
        int iymax, iymin;

        int ix_block, ixmax_block, ixmin_block;
        int iy_block, iymax_block, iymin_block;

        chr_t * pchr_a;

        // make sure that it is on
        if ( !ACTIVE_CHR(ichr_a) ) continue;
        pchr_a = ChrList.lst + ichr_a;

        // make sure we have a good collision size for this object
        //chr_update_collision_size( pchr_a, btrue );

        // reject characters that are in packs, or are marked as non-colliding
        if ( pchr_a->pack_ispacked ) continue;

        // reject characters that are hidden
        if ( pchr_a->is_hidden ) continue;

        // determine the size of this object in blocks
        ixmin = pchr_a->pos.x - pchr_a->bump.size; ixmin = CLIP(ixmin, 0, PMesh->info.edge_x);
        ixmax = pchr_a->pos.x + pchr_a->bump.size; ixmax = CLIP(ixmax, 0, PMesh->info.edge_x);

        iymin = pchr_a->pos.y - pchr_a->bump.size; iymin = CLIP(iymin, 0, PMesh->info.edge_y);
        iymax = pchr_a->pos.y + pchr_a->bump.size; iymax = CLIP(iymax, 0, PMesh->info.edge_y);

        ixmin_block = ixmin >> BLOCK_BITS; ixmin_block = CLIP( ixmin_block, 0, MAXMESHBLOCKY );
        ixmax_block = ixmax >> BLOCK_BITS; ixmax_block = CLIP( ixmax_block, 0, MAXMESHBLOCKY );

        iymin_block = iymin >> BLOCK_BITS; iymin_block = CLIP( iymin_block, 0, MAXMESHBLOCKY );
        iymax_block = iymax >> BLOCK_BITS; iymax_block = CLIP( iymax_block, 0, MAXMESHBLOCKY );

        // handle all the interactions on this block
        for (ix_block = ixmin_block; ix_block <= ixmax_block; ix_block++)
        {
            for (iy_block = iymin_block; iy_block <= iymax_block; iy_block++)
            {
                // Allow raw access here because we were careful :)
                fanblock = mesh_get_block_int(PMesh, ix_block, iy_block);
                if ( INVALID_BLOCK == fanblock ) continue;

                chrinblock = bumplist[fanblock].chrnum;
                prtinblock = bumplist[fanblock].prtnum;

                // detect all the character-character interactions
                for ( tnc = 0, ichr_b = bumplist[fanblock].chr;
                        tnc < chrinblock && MAX_CHR != ichr_b;
                        tnc++, ichr_b = ChrList.lst[ichr_b].bumplist_next)
                {
                    chr_t * pchr_b;
                    if( !ACTIVE_CHR( ichr_b ) ) continue;

                    pchr_b = ChrList.lst + ichr_b;

                    // make sure we have a good collision size for this object
                    //chr_update_collision_size( pchr_b, btrue );

                    if ( detect_chr_chr_interaction(ichr_a, ichr_b) )
                    {
                        add_chr_chr_interaction( ichr_a, ichr_b, cdata, cdata_count, hnlst, hn_count );
                    }
                }

                // detect all the character-particle interactions
                // for living characters
                if ( pchr_a->alive )
                {
                    for ( tnc = 0, iprt_b = bumplist[fanblock].prt;
                            tnc < prtinblock && TOTAL_MAX_PRT != iprt_b;
                            tnc++, iprt_b = PrtList.lst[iprt_b].bumplist_next )
                    {
                        if ( detect_chr_prt_interaction(ichr_a, iprt_b) )
                        {
                            add_chr_prt_interaction( ichr_a, iprt_b, cdata, cdata_count, hnlst, hn_count );
                        }
                    }
                }
            }
        }
    }
}

//--------------------------------------------------------------------------------------------
bool_t can_mount( Uint16 ichr_a, Uint16 ichr_b )
{
    bool_t is_valid_rider_a, is_valid_mount_b;

    chr_t * pchr_a, * pchr_b;
    cap_t * pcap_a, * pcap_b;

    // make sure that A is valid
    if ( !ACTIVE_CHR(ichr_a) ) return bfalse;
    pchr_a = ChrList.lst + ichr_a;

    pcap_a = chr_get_pcap( ichr_a );
    if ( NULL == pcap_a ) return bfalse;

    // make sure that B is valid
    if ( !ACTIVE_CHR(ichr_b) ) return bfalse;
    pchr_b = ChrList.lst + ichr_b;

    pcap_b = chr_get_pcap( ichr_b );
    if ( NULL == pcap_b ) return bfalse;

    is_valid_rider_a = !pchr_a->isitem && pchr_a->alive && 0 == pchr_a->flyheight &&
                       !ACTIVE_CHR(pchr_a->attachedto) && chr_get_pmad(ichr_a)->actionvalid[ACTION_MI];

    is_valid_mount_b = pchr_b->ismount && pchr_b->alive &&
                       pcap_b->slotvalid[SLOT_LEFT] && !ACTIVE_CHR(pchr_b->holdingwhich[SLOT_LEFT]);

    return is_valid_rider_a && is_valid_mount_b;
}

//--------------------------------------------------------------------------------------------
bool_t attach_chr_to_platform( chr_t * pchr, chr_t * pplat )
{
    // BB> attach a character to a platform

    cap_t * pchr_cap;
    GLvector3 platform_up;

    // verify that we do not have two dud pointers
    if( !ACTIVE_PCHR( pchr ) ) return bfalse;
    if( !ACTIVE_PCHR( pplat ) ) return bfalse;

    pchr_cap = pro_get_pcap( pchr->iprofile );
    if( NULL == pchr_cap ) return bfalse;

    // check if they can be connected
    if( !pchr_cap->canuseplatforms || (0 != pchr->flyheight) ) return bfalse;
    if( !pplat->platform ) return bfalse;

    // do the attachment
    pchr->onwhichplatform = GET_INDEX( pplat, MAX_CHR );

    // update the character's relationship to the ground
    pchr->enviro.level    = MAX( pchr->enviro.floor_level, pplat->pos.z + pplat->bump.height );
    pchr->enviro.zlerp    = (pchr->pos.z - pchr->enviro.level) / PLATTOLERANCE;
    pchr->enviro.zlerp    = CLIP(pchr->enviro.zlerp, 0, 1);
    pchr->enviro.grounded = (0 == pchr->flyheight) && (pchr->enviro.zlerp < 0.25f);

    // add the weight to the platform based on the new zlerp
    pplat->holdingweight += pchr->phys.weight * (1.0f - pchr->enviro.zlerp);

    // update the character jupming
    pchr->jumpready = pchr->enviro.grounded;
    if( pchr->jumpready )
    {
        pchr->jumpnumber = pchr->jumpnumberreset;
    }

    // what to do about traction if the platform is tilted... hmmm?
    chr_getMatUp( pplat, &platform_up );
    platform_up = VNormalize(platform_up);

    pchr->enviro.traction = ABS(platform_up.z) * (1.0f - pchr->enviro.zlerp) + 0.25 * pchr->enviro.zlerp;


    // tell the platform that we bumped into it
    // this is necessary for key buttons to work properly, for instance
    ai_state_set_bumplast( &(pplat->ai), GET_INDEX( pchr, MAX_CHR ) );

    return btrue;
}

//--------------------------------------------------------------------------------------------
bool_t do_platforms( Uint16 ichr_a, Uint16 ichr_b )
{
    float xa, ya, za;
    float xb, yb, zb;

    chr_t * pchr_a, * pchr_b;
    cap_t * pcap_a, * pcap_b;

    bool_t platform_a, platform_b;
    bool_t mount_a, mount_b;
    float  depth_x, depth_y, depth_z, depth_xy, depth_yx;

    bool_t collide_x  = bfalse;
    bool_t collide_y  = bfalse;
    bool_t collide_xy = bfalse;
    bool_t collide_yx = bfalse;
    bool_t collide_z  = bfalse;
    bool_t chara_on_top;

    // make sure that A is valid
    if ( !ACTIVE_CHR(ichr_a) ) return bfalse;
    pchr_a = ChrList.lst + ichr_a;

    pcap_a = chr_get_pcap( ichr_a );
    if ( NULL == pcap_a ) return bfalse;

    // make sure that B is valid
    if ( !ACTIVE_CHR(ichr_b) ) return bfalse;
    pchr_b = ChrList.lst + ichr_b;

    pcap_b = chr_get_pcap( ichr_b );
    if ( NULL == pcap_b ) return bfalse;

    // if you are mounted, only your mount is affected by platforms
    if ( ACTIVE_CHR(pchr_a->attachedto) || ACTIVE_CHR(pchr_b->attachedto) ) return bfalse;

    // only check possible object-platform interactions
    platform_a = pcap_b->canuseplatforms && pchr_a->platform;
    platform_b = pcap_a->canuseplatforms && pchr_b->platform;
    if ( !platform_a && !platform_b ) return bfalse;

    xa = pchr_a->pos.x;
    ya = pchr_a->pos.y;
    za = pchr_a->pos.z;

    xb = pchr_b->pos.x;
    yb = pchr_b->pos.y;
    zb = pchr_b->pos.z;

    // If we can mount this platform, skip it
    mount_a = can_mount(ichr_b, ichr_a);
    if ( mount_a && pchr_a->enviro.level < zb + pchr_b->bump.height + PLATTOLERANCE )
        return bfalse;

    // If we can mount this platform, skip it
    mount_b = can_mount(ichr_a, ichr_b);
    if ( mount_b && pchr_b->enviro.level < za + pchr_a->bump.height + PLATTOLERANCE )
        return bfalse;

    depth_z  = MIN(pchr_b->chr_chr_cv.max_z + pchr_b->pos.z, pchr_a->chr_chr_cv.max_z + pchr_a->pos.z ) -
               MAX(pchr_b->chr_chr_cv.min_z + pchr_b->pos.z, pchr_a->chr_chr_cv.min_z + pchr_a->pos.z );

    collide_z  = ( depth_z > -PLATTOLERANCE && depth_z < PLATTOLERANCE );

    if( !collide_z ) return bfalse;

    // initialize the overlap depths
    depth_x = depth_y = depth_xy = depth_yx = 0.0f;

    // determine how the characters can be attached
    chara_on_top = btrue;
    depth_z = 2 * PLATTOLERANCE;
    if ( platform_a && platform_b )
    {
        float depth_a, depth_b;

        depth_a = (zb + pchr_b->chr_chr_cv.max_z) - (za + pchr_a->chr_chr_cv.min_z);
        depth_b = (za + pchr_a->chr_chr_cv.max_z) - (zb + pchr_b->chr_chr_cv.min_z);

        depth_z = MIN( zb + pchr_b->chr_chr_cv.max_z, za + pchr_a->chr_chr_cv.max_z ) - MAX(zb + pchr_b->chr_chr_cv.min_z, za + pchr_a->chr_chr_cv.min_z);

        chara_on_top = ABS(depth_z - depth_a) < ABS(depth_z - depth_b);

        // the collision is determined by the platform size
        if ( chara_on_top )
        {
            // size of a doesn't matter
            depth_x  = MIN((pchr_b->chr_chr_cv.max_x + pchr_b->pos.x) - pchr_a->pos.x,
                           pchr_a->pos.x - (pchr_b->chr_chr_cv.min_x + pchr_b->pos.x) );

            depth_y  = MIN((pchr_b->chr_chr_cv.max_y + pchr_b->pos.y) -  pchr_a->pos.y,
                           pchr_a->pos.y - (pchr_b->chr_chr_cv.min_y + pchr_b->pos.y) );

            depth_xy = MIN((pchr_b->chr_chr_cv.max_xy + (pchr_b->pos.x + pchr_b->pos.y)) -  (pchr_a->pos.x + pchr_a->pos.y),
                           (pchr_a->pos.x + pchr_a->pos.y) - (pchr_b->chr_chr_cv.min_xy + (pchr_b->pos.x + pchr_b->pos.y)) );

            depth_yx = MIN((pchr_b->chr_chr_cv.max_yx + (-pchr_b->pos.x + pchr_b->pos.y)) - (-pchr_a->pos.x + pchr_a->pos.y),
                           (-pchr_a->pos.x + pchr_a->pos.y) - (pchr_b->chr_chr_cv.min_yx + (-pchr_b->pos.x + pchr_b->pos.y)) );
        }
        else
        {
            // size of b doesn't matter

            depth_x  = MIN((pchr_a->chr_chr_cv.max_x + pchr_a->pos.x) - pchr_b->pos.x,
                           pchr_b->pos.x - (pchr_a->chr_chr_cv.min_x + pchr_a->pos.x) );

            depth_y  = MIN((pchr_a->chr_chr_cv.max_y + pchr_a->pos.y) -  pchr_b->pos.y,
                           pchr_b->pos.y - (pchr_a->chr_chr_cv.min_y + pchr_a->pos.y) );

            depth_xy = MIN((pchr_a->chr_chr_cv.max_xy + (pchr_a->pos.x + pchr_a->pos.y)) -  (pchr_b->pos.x + pchr_b->pos.y),
                           (pchr_b->pos.x + pchr_b->pos.y) - (pchr_a->chr_chr_cv.min_xy + (pchr_a->pos.x + pchr_a->pos.y)) );

            depth_yx = MIN((pchr_a->chr_chr_cv.max_yx + (-pchr_a->pos.x + pchr_a->pos.y)) - (-pchr_b->pos.x + pchr_b->pos.y),
                           (-pchr_b->pos.x + pchr_b->pos.y) - (pchr_a->chr_chr_cv.min_yx + (-pchr_a->pos.x + pchr_a->pos.y)) );
        }
    }
    else if ( platform_a )
    {
        chara_on_top = bfalse;
        depth_z = (za + pchr_a->chr_chr_cv.max_z) - (zb + pchr_b->chr_chr_cv.min_z);

        // size of b doesn't matter

        depth_x  = MIN((pchr_a->chr_chr_cv.max_x + pchr_a->pos.x) - pchr_b->pos.x,
                       pchr_b->pos.x - (pchr_a->chr_chr_cv.min_x + pchr_a->pos.x) );

        depth_y  = MIN((pchr_a->chr_chr_cv.max_y + pchr_a->pos.y) -  pchr_b->pos.y,
                       pchr_b->pos.y - (pchr_a->chr_chr_cv.min_y + pchr_a->pos.y) );

        depth_xy = MIN((pchr_a->chr_chr_cv.max_xy + (pchr_a->pos.x + pchr_a->pos.y)) -  (pchr_b->pos.x + pchr_b->pos.y),
                       (pchr_b->pos.x + pchr_b->pos.y) - (pchr_a->chr_chr_cv.min_xy + (pchr_a->pos.x + pchr_a->pos.y)) );

        depth_yx = MIN((pchr_a->chr_chr_cv.max_yx + (-pchr_a->pos.x + pchr_a->pos.y)) - (-pchr_b->pos.x + pchr_b->pos.y),
                      (-pchr_b->pos.x + pchr_b->pos.y) - (pchr_a->chr_chr_cv.min_yx + (-pchr_a->pos.x + pchr_a->pos.y)) );
    }
    else if ( platform_b )
    {
        chara_on_top = btrue;
        depth_z = (zb + pchr_b->chr_chr_cv.max_z) - (za + pchr_a->chr_chr_cv.min_z);

        // size of a doesn't matter
        depth_x  = MIN((pchr_b->chr_chr_cv.max_x + pchr_b->pos.x) - pchr_a->pos.x,
                       pchr_a->pos.x - (pchr_b->chr_chr_cv.min_x + pchr_b->pos.x) );

        depth_y  = MIN(pchr_b->chr_chr_cv.max_y + (pchr_b->pos.y -  pchr_a->pos.y),
                       (pchr_a->pos.y - pchr_b->chr_chr_cv.min_y) + pchr_b->pos.y );

        depth_xy = MIN((pchr_b->chr_chr_cv.max_xy + (pchr_b->pos.x + pchr_b->pos.y)) -  (pchr_a->pos.x + pchr_a->pos.y),
                       (pchr_a->pos.x + pchr_a->pos.y) - (pchr_b->chr_chr_cv.min_xy + (pchr_b->pos.x + pchr_b->pos.y)) );

        depth_yx = MIN((pchr_b->chr_chr_cv.max_yx + (-pchr_b->pos.x + pchr_b->pos.y)) - (-pchr_a->pos.x + pchr_a->pos.y),
                      (-pchr_a->pos.x + pchr_a->pos.y) - (pchr_b->chr_chr_cv.min_yx + (-pchr_b->pos.x + pchr_b->pos.y)) );

    }
    collide_x  = depth_x  > 0;
    collide_y  = depth_y  > 0;
    collide_xy = depth_xy > 0;
    collide_yx = depth_yx > 0;
    collide_z  = ( depth_z > -PLATTOLERANCE && depth_z < PLATTOLERANCE  );

    if ( collide_x && collide_y && collide_xy && collide_yx && collide_z )
    {
        // check for the best possible attachment
        if ( chara_on_top )
        {
            if ( zb + pchr_b->chr_chr_cv.max_z > pchr_a->enviro.level )
            {
                attach_chr_to_platform( pchr_a, pchr_b );
            }
        }
        else
        {
            if ( za + pchr_a->chr_chr_cv.max_z > pchr_b->enviro.level )
            {
                attach_chr_to_platform( pchr_b, pchr_a );
            }
        }
    }

    return btrue;
}

//--------------------------------------------------------------------------------------------
bool_t do_mounts( Uint16 ichr_a, Uint16 ichr_b )
{
    float xa, ya, za;
    float xb, yb, zb;

    chr_t * pchr_a, * pchr_b;
    cap_t * pcap_a, * pcap_b;

    bool_t mount_a, mount_b;
    float  dx, dy, dist;
    float  depth_z;

    bool_t collide_x  = bfalse;
    bool_t collide_y  = bfalse;
    bool_t collide_xy = bfalse;

    bool_t mounted;

    // make sure that A is valid
    if ( !ACTIVE_CHR( ichr_a ) ) return bfalse;
    pchr_a = ChrList.lst + ichr_a;

    pcap_a = chr_get_pcap( ichr_a );
    if ( NULL == pcap_a ) return bfalse;

    // make sure that B is valid
    if ( !ACTIVE_CHR( ichr_b ) ) return bfalse;
    pchr_b = ChrList.lst + ichr_b;

    pcap_b = chr_get_pcap( ichr_b );
    if ( NULL == pcap_b ) return bfalse;

    xa = pchr_a->pos.x;
    ya = pchr_a->pos.y;
    za = pchr_a->pos.z;

    xb = pchr_b->pos.x;
    yb = pchr_b->pos.y;
    zb = pchr_b->pos.z;

    // can either of these objects mount the other?
    mount_a = can_mount(ichr_b, ichr_a);
    mount_b = can_mount(ichr_a, ichr_b);

    if ( !mount_a && !mount_b ) return bfalse;

    mounted = bfalse;
    if ( !mounted && mount_b && (pchr_a->vel.z - pchr_b->vel.z) < 0 )
    {
        // A falling on B?
        GLvector4 point[1], nupoint[1];

        // determine the actual location of the mount point
        {
            int vertex;
            chr_instance_t * pinst = &(pchr_b->inst);

            vertex = ego_md2_data[MadList[pinst->imad].md2_ref].vertices - GRIP_LEFT;

            // do the automatic update
            chr_instance_update_vertices( pinst, vertex, vertex );

            // Calculate grip point locations with linear interpolation and other silly things
            point[0].x = pinst->vlst[vertex].pos[XX];
            point[0].y = pinst->vlst[vertex].pos[YY];
            point[0].z = pinst->vlst[vertex].pos[ZZ];
            point[0].w = 1.0f;

            // Do the transform
            TransformVertices( &(pinst->matrix), point, nupoint, 1 );
        }

        dx = ABS( xa - nupoint[0].x );
        dy = ABS( ya - nupoint[0].y );
        dist = dx + dy;
        depth_z = za - nupoint[0].z;

        if ( depth_z >= -MOUNTTOLERANCE && depth_z <= MOUNTTOLERANCE )
        {
            // estimate the collisions this frame
            collide_x  = (dx <= pchr_a->bump.size * 2);
            collide_y  = (dy <= pchr_a->bump.size * 2);
            collide_xy = (dist <= pchr_a->bump.sizebig * 2);

            if ( collide_x && collide_y && collide_xy )
            {
                attach_character_to_mount( ichr_a, ichr_b, GRIP_ONLY );
                mounted = ACTIVE_CHR( pchr_a->attachedto );
            }
        }
    }

    if ( !mounted && mount_a && (pchr_b->vel.z - pchr_a->vel.z) < 0 )
    {
        // B falling on A?

        GLvector4 point[1], nupoint[1];

        // determine the actual location of the mount point
        {
            int vertex;
            chr_instance_t * pinst = &(pchr_a->inst);

            vertex = ego_md2_data[MadList[pinst->imad].md2_ref].vertices - GRIP_LEFT;

            // do the automatic update
            chr_instance_update_vertices( pinst, vertex, vertex );

            // Calculate grip point locations with linear interpolation and other silly things
            point[0].x = pinst->vlst[vertex].pos[XX];
            point[0].y = pinst->vlst[vertex].pos[YY];
            point[0].z = pinst->vlst[vertex].pos[ZZ];
            point[0].w = 1.0f;

            // Do the transform
            TransformVertices( &(pinst->matrix), point, nupoint, 1 );
        }

        dx = ABS( xb - nupoint[0].x );
        dy = ABS( yb - nupoint[0].y );
        dist = dx + dy;
        depth_z = zb - nupoint[0].z;

        if ( depth_z >= -MOUNTTOLERANCE && depth_z <= MOUNTTOLERANCE )
        {
            // estimate the collisions this frame
            collide_x  = (dx <= pchr_b->bump.size * 2);
            collide_y  = (dy <= pchr_b->bump.size * 2);
            collide_xy = (dist <= pchr_b->bump.sizebig * 2);

            if ( collide_x && collide_y && collide_xy )
            {
                attach_character_to_mount( ichr_b, ichr_a, GRIP_ONLY );
                mounted = ACTIVE_CHR( pchr_a->attachedto );
            }
        }
    }

    return btrue;
}

//--------------------------------------------------------------------------------------------
bool_t do_chr_platform_physics( chr_t * pitem, chr_t * pplat )
{
    // we know that ichr_a is a platform and ichr_b is on it
    Sint16 rot_a, rot_b;
    float lerp_z, vlerp_z;

    if( !ACTIVE_PCHR( pitem ) ) return bfalse;
    if( !ACTIVE_PCHR( pplat ) ) return bfalse;

    lerp_z = (pitem->pos.z - pitem->enviro.level) / PLATTOLERANCE;
    lerp_z = 1.0f - CLIP( lerp_z, 0.0f, 1.0f );

    // if your velocity is going up much faster then the
    // platform, there is no need to suck you to the level of the platform
    // this was one of the things preventing you from jumping from platforms
    // properly
    vlerp_z = ABS(pitem->vel.z - pplat->vel.z) / 5;
    vlerp_z  = 1.0f - CLIP(vlerp_z, 0.0f, 1.0f);

    // determine the rotation rates
    rot_b = pitem->turn_z - pitem->turn_old_z;
    rot_a = pplat->turn_z - pplat->turn_old_z;

    if ( lerp_z == 1.0f )
    {
        pitem->phys.apos_0.z += (pitem->enviro.level - pitem->pos.z) * 0.125f;
        pitem->phys.avel.z += ( pplat->vel.z  - pitem->vel.z ) * 0.25f;
        pitem->turn_z      += ( rot_a         - rot_b        ) * platstick;
    }
    else
    {
        pitem->phys.apos_0.z += (pitem->enviro.level - pitem->pos.z) * 0.125f * lerp_z * vlerp_z;
        pitem->phys.avel.z += ( pplat->vel.z  - pitem->vel.z ) * 0.25f * lerp_z * vlerp_z;
        pitem->turn_z      += ( rot_a         - rot_b        ) * platstick * lerp_z * vlerp_z;
    };

    return btrue;
}
//--------------------------------------------------------------------------------------------
bool_t do_chr_chr_collision( Uint16 ichr_a, Uint16 ichr_b )
{
    float xa,ya,za,xya,yxa;
    float xb,yb,zb,xyb,yxb;
    float was_xa,was_ya,was_za,was_xya,was_yxa;
    float was_xb,was_yb,was_zb,was_xyb,was_yxb;

    float depth_x, depth_y, depth_xy, depth_yx, depth_z;
    float was_depth_x, was_depth_y, was_depth_xy, was_depth_yx, was_depth_z;

    chr_t * pchr_a, * pchr_b;
    cap_t * pcap_a, * pcap_b;

    float wta, wtb;

    GLvector3 nrm;
    int exponent = 1;

    bool_t collide_x  = bfalse, was_collide_x;
    bool_t collide_y  = bfalse, was_collide_y;
    bool_t collide_xy = bfalse, was_collide_xy;
    bool_t collide_yx = bfalse, was_collide_yx;
    bool_t collide_z  = bfalse, was_collide_z;
    bool_t collision  = bfalse, bump = bfalse;

    float interaction_strength = 1.0f;

    // make sure that it is on
    if ( !ACTIVE_CHR( ichr_a ) ) return bfalse;
    pchr_a = ChrList.lst + ichr_a;

    pcap_a = chr_get_pcap( ichr_a );
    if ( NULL == pcap_a ) return bfalse;

    // make sure that it is on
    if ( !ACTIVE_CHR( ichr_b ) ) return bfalse;
    pchr_b = ChrList.lst + ichr_b;

    pcap_b = chr_get_pcap( ichr_b );
    if ( NULL == pcap_b ) return bfalse;

    // platform interaction. if the onwhichplatform is set, then
    // all collision tests have been met
    if ( ichr_a == pchr_b->onwhichplatform )
    {
        if( do_chr_platform_physics( pchr_b, pchr_a ) )
        {
            // this is handled
            return btrue;
        }
    }

    // platform interaction. if the onwhichplatform is set, then
    // all collision tests have been met
    if ( ichr_b == pchr_a->onwhichplatform )
    {
        if( do_chr_platform_physics( pchr_a, pchr_b ) )
        {
            // this is handled
            return btrue;
        }
    }

    // items can interact with platforms but not with other characters/objects
    if ( pchr_a->isitem || pchr_b->isitem ) return bfalse;

    // don't interact with your mount, or your held items
    if ( ichr_a == pchr_b->attachedto || ichr_b == pchr_a->attachedto ) return bfalse;

    // don't do anything if there is no interaction strength
    if ( 0 == pchr_a->bump.size || 0 == pchr_b->bump.size ) return bfalse;

    // make sure we have good collision sizes for the two objects
    // these functions can be called here, even though they are relatively expensive
    // because they are called relatively rarely... we hope!
    //chr_update_collision_size( pchr_a, btrue );
    //chr_update_collision_size( pchr_b, btrue );

    interaction_strength = 1.0f;
    interaction_strength *= pchr_a->inst.alpha * INV_FF;
    interaction_strength *= pchr_b->inst.alpha * INV_FF;

    // estimate the collision based on the "collision bounding box", chr_chr_cv
    xa  = pchr_a->pos.x;
    ya  = pchr_a->pos.y;
    za  = pchr_a->pos.z;
    xya = pchr_a->pos.x + pchr_a->pos.y;
    yxa =-pchr_a->pos.x + pchr_a->pos.y;


    xb  = pchr_b->pos.x;
    yb  = pchr_b->pos.y;
    zb  = pchr_b->pos.z;
    xyb = pchr_b->pos.x + pchr_b->pos.y;
    yxb =-pchr_b->pos.x + pchr_b->pos.y;

    depth_x  = MIN(pchr_b->chr_chr_cv.max_x + xb, pchr_a->chr_chr_cv.max_x + xa ) -
               MAX(pchr_b->chr_chr_cv.min_x + xb, pchr_a->chr_chr_cv.min_x + xa );

    depth_y  = MIN(pchr_b->chr_chr_cv.max_y + yb, pchr_a->chr_chr_cv.max_y + ya ) -
               MAX(pchr_b->chr_chr_cv.min_y + yb, pchr_a->chr_chr_cv.min_y + ya );

    depth_z  = MIN(pchr_b->chr_chr_cv.max_z + zb, pchr_a->chr_chr_cv.max_z + za ) -
               MAX(pchr_b->chr_chr_cv.min_z + zb, pchr_a->chr_chr_cv.min_z + za );

    depth_xy = MIN(pchr_b->chr_chr_cv.max_xy + xyb, pchr_a->chr_chr_cv.max_xy + xya ) -
               MAX(pchr_b->chr_chr_cv.min_xy + xyb, pchr_a->chr_chr_cv.min_xy + xya );
    depth_xy *= INV_SQRT_TWO;

    depth_yx = MIN(pchr_b->chr_chr_cv.max_yx + yxb, pchr_a->chr_chr_cv.max_yx + yxa ) -
                   MAX(pchr_b->chr_chr_cv.min_yx + yxb, pchr_a->chr_chr_cv.min_yx + yxa );
    depth_yx *= INV_SQRT_TWO;

    // estimate the collisions this frame
    collide_x  = depth_x  > 0.0f;
    collide_y  = depth_y  > 0.0f;
    collide_z  = depth_z  > 0.0f;
    collide_xy = depth_xy > 0.0f;
    collide_yx = depth_yx > 0.0f;

    if( !collide_x || !collide_y || !collide_z || !collide_xy || !collide_yx ) return bfalse;

    // determine whether the character was "colliding" with this object the last update
    was_xa  = pchr_a->pos_old.x;
    was_ya  = pchr_a->pos_old.y;
    was_za  = pchr_a->pos_old.z;
    was_xya = pchr_a->pos_old.x + pchr_a->pos_old.y;
    was_yxa =-pchr_a->pos_old.x + pchr_a->pos_old.y;

    was_xb  = pchr_b->pos_old.x;
    was_yb  = pchr_b->pos_old.y;
    was_zb  = pchr_b->pos_old.z;
    was_xyb = pchr_b->pos_old.x + pchr_b->pos_old.y;
    was_yxb =-pchr_b->pos_old.x + pchr_b->pos_old.y;


    was_depth_x  = MIN(pchr_b->chr_chr_cv.max_x + was_xb, pchr_a->chr_chr_cv.max_x + was_xa ) -
                   MAX(pchr_b->chr_chr_cv.min_x + was_xb, pchr_a->chr_chr_cv.min_x + was_xa );

    was_depth_y  = MIN(pchr_b->chr_chr_cv.max_y + was_yb, pchr_a->chr_chr_cv.max_y + was_ya ) -
                   MAX(pchr_b->chr_chr_cv.min_y + was_yb, pchr_a->chr_chr_cv.min_y + was_ya );

    was_depth_z  = MIN(pchr_b->chr_chr_cv.max_z + was_zb, pchr_a->chr_chr_cv.max_z + was_za ) -
                   MAX(pchr_b->chr_chr_cv.min_z + was_zb, pchr_a->chr_chr_cv.min_z + was_za );

    was_depth_xy = MIN(pchr_b->chr_chr_cv.max_xy + was_xyb, pchr_a->chr_chr_cv.max_xy + was_xya ) -
                   MAX(pchr_b->chr_chr_cv.min_xy + was_xyb, pchr_a->chr_chr_cv.min_xy + was_xya );
    was_depth_xy *= INV_SQRT_TWO;

    was_depth_yx = MIN(pchr_b->chr_chr_cv.max_yx + was_yxb, pchr_a->chr_chr_cv.max_yx + was_yxa ) -
                   MAX(pchr_b->chr_chr_cv.min_yx + was_yxb, pchr_a->chr_chr_cv.min_yx + was_yxa );
    was_depth_yx *= INV_SQRT_TWO;

    was_collide_x  = was_depth_x  > 0.0f;
    was_collide_y  = was_depth_y  > 0.0f;
    was_collide_z  = was_depth_z  > 0.0f;
    was_collide_xy = was_depth_xy > 0.0f;
    was_collide_yx = was_depth_yx > 0.0f;

    collision = !was_collide_x || !was_collide_y || !was_collide_z || !was_collide_xy || !was_collide_yx ;

    //------------------
    // do character-character interactions

    wta = (0xFFFFFFFF == pchr_a->phys.weight) ? -(float)0xFFFFFFFF : pchr_a->phys.weight;
    wtb = (0xFFFFFFFF == pchr_b->phys.weight) ? -(float)0xFFFFFFFF : pchr_b->phys.weight;

    if ( wta == 0 && wtb == 0 )
    {
        wta = wtb = 1;
    }
    else if ( wta == 0 )
    {
        wta = 1;
        wtb = -0xFFFF;
    }
    else if ( wtb == 0 )
    {
        wtb = 1;
        wta = -0xFFFF;
    }

    if ( 0.0f == pchr_a->phys.bumpdampen && 0.0f == pchr_b->phys.bumpdampen )
    {
        /* do nothing */
    }
    else if ( 0.0f == pchr_a->phys.bumpdampen )
    {
        // make the weight infinite
        wta = -0xFFFF;
    }
    else if ( 0.0f == pchr_b->phys.bumpdampen )
    {
        // make the weight infinite
        wtb = -0xFFFF;
    }
    else
    {
        // adjust the weights to respect bumpdampen
        wta /= pchr_a->phys.bumpdampen;
        wtb /= pchr_b->phys.bumpdampen;
    }

    // determine the interaction normal
    if ( pcap_a->canuseplatforms && pchr_b->platform ) exponent += 2;
    if ( pcap_b->canuseplatforms && pchr_a->platform ) exponent += 2;

    nrm.x = nrm.y = nrm.z = 0.0f;

    if ( depth_x <= 0.0f )
    {
        depth_x = 0.0f;
    }
    else
    {
        float sgn = xb - xa;
        sgn = sgn > 0 ? -1 : 1;

        nrm.x += sgn / POW(depth_x / PLATTOLERANCE, exponent);
    }

    if ( depth_y <= 0.0f )
    {
        depth_y = 0.0f;
    }
    else
    {
        float sgn = yb - ya;
        sgn = sgn > 0 ? -1 : 1;

        nrm.y += sgn / POW(depth_y / PLATTOLERANCE, exponent);
    }

    if ( depth_xy <= 0.0f )
    {
        depth_xy = 0.0f;
    }
    else
    {
        float sgn = (xb + yb) - (xa + ya);
        sgn = sgn > 0 ? -1 : 1;

        nrm.x += sgn / POW(depth_xy / PLATTOLERANCE, exponent);
        nrm.y += sgn / POW(depth_xy / PLATTOLERANCE, exponent);
    }

    if ( depth_yx <= 0.0f )
    {
        depth_yx = 0.0f;
    }
    else
    {
        float sgn = (-xb + yb) - (-xa + ya);
        sgn = sgn > 0 ? -1 : 1;
        nrm.x -= sgn / POW(depth_yx / PLATTOLERANCE, exponent);
        nrm.y += sgn / POW(depth_yx / PLATTOLERANCE, exponent);
    }


    if ( depth_z <= 0.0f )
    {
        depth_z = 0.0f;
    }
    else
    {
        float sgn = (zb + (pchr_b->chr_chr_cv.max_z + pchr_b->pos.z) / 2) - (za + (pchr_a->chr_chr_cv.max_z + pchr_a->pos.z) / 2);
        sgn = sgn > 0 ? -1 : 1;

        nrm.z += sgn / POW(exponent * depth_z / PLATTOLERANCE, exponent);
    }

    if ( ABS(nrm.x) + ABS(nrm.y) + ABS(nrm.z) > 0.0f )
    {
        GLvector3 vel_a, vel_b;
        GLvector3 vpara_a, vperp_a;
        GLvector3 vpara_b, vperp_b;
        GLvector3 imp_a, imp_b;
        float     vdot;

        nrm = VNormalize( nrm );

        vel_a.x = pchr_a->vel.x;
        vel_a.y = pchr_a->vel.y;
        vel_a.z = pchr_a->vel.z;

        vel_b.x = pchr_b->vel.x;
        vel_b.y = pchr_b->vel.y;
        vel_b.z = pchr_b->vel.z;

        vdot = VDotProduct( nrm, vel_a );
        vperp_a.x = nrm.x * vdot;
        vperp_a.y = nrm.y * vdot;
        vperp_a.z = nrm.z * vdot;
        vpara_a = VSub( vel_a, vperp_a );

        vdot = VDotProduct( nrm, vel_b );
        vperp_b.x = nrm.x * vdot;
        vperp_b.y = nrm.y * vdot;
        vperp_b.z = nrm.z * vdot;
        vpara_b = VSub( vel_b, vperp_b );

        // clear the "impulses"
        imp_a.x = imp_a.y = imp_a.z = 0.0f;
        imp_b.x = imp_b.y = imp_b.z = 0.0f;

        // what type of "collision" is this? (impulse or pressure)
        if ( collision )
        {
            // an actual bump, use impulse to make the objects bounce appart

            // generic coefficient of restitution
            float cr = 0.5f;

            if ( (wta < 0 && wtb < 0) || (wta == wtb) )
            {
                float factor = 0.5f * (1.0f - cr);

                imp_a.x = factor * (vperp_b.x - vperp_a.x);
                imp_a.y = factor * (vperp_b.y - vperp_a.y);
                imp_a.z = factor * (vperp_b.z - vperp_a.z);

                imp_b.x = factor * (vperp_a.x - vperp_b.x);
                imp_b.y = factor * (vperp_a.y - vperp_b.y);
                imp_b.z = factor * (vperp_a.z - vperp_b.z);
            }
            else if ( (wta < 0) || (wtb == 0) )
            {
                float factor = (1.0f - cr);

                imp_b.x = factor * (vperp_a.x - vperp_b.x);
                imp_b.y = factor * (vperp_a.y - vperp_b.y);
                imp_b.z = factor * (vperp_a.z - vperp_b.z);
            }
            else if ( (wtb < 0) || (wta == 0) )
            {
                float factor = (1.0f - cr);

                imp_a.x = factor * (vperp_b.x - vperp_a.x);
                imp_a.y = factor * (vperp_b.y - vperp_a.y);
                imp_a.z = factor * (vperp_b.z - vperp_a.z);
            }
            else
            {
                float factor;

                factor = (1.0f - cr) * wtb / ( wta + wtb );
                imp_a.x = factor * (vperp_b.x - vperp_a.x);
                imp_a.y = factor * (vperp_b.y - vperp_a.y);
                imp_a.z = factor * (vperp_b.z - vperp_a.z);

                factor = (1.0f - cr) * wta / ( wta + wtb );
                imp_b.x = factor * (vperp_a.x - vperp_b.x);
                imp_b.y = factor * (vperp_a.y - vperp_b.y);
                imp_b.z = factor * (vperp_a.z - vperp_b.z);
            }

            // add in the bump impulses
            pchr_a->phys.avel.x += imp_a.x * interaction_strength;
            pchr_a->phys.avel.y += imp_a.y * interaction_strength;
            pchr_a->phys.avel.z += imp_a.z * interaction_strength;
            LOG_NAN(pchr_a->phys.avel.z);

            pchr_b->phys.avel.x += imp_b.x * interaction_strength;
            pchr_b->phys.avel.y += imp_b.y * interaction_strength;
            pchr_b->phys.avel.z += imp_b.z * interaction_strength;
            LOG_NAN(pchr_b->phys.avel.z);

            bump = btrue;
        }
        else
        {
            // not a bump at all. two objects are rubbing against one another
            // and continually overlapping. use pressure to push them appart.

            float tmin;

            tmin = 1e6;
            if ( nrm.x != 0 )
            {
                tmin = MIN(tmin, depth_x / ABS(nrm.x) );
            }
            if ( nrm.y != 0 )
            {
                tmin = MIN(tmin, depth_y / ABS(nrm.y) );
            }
            if ( nrm.z != 0 )
            {
                tmin = MIN(tmin, depth_z / ABS(nrm.z) );
            }

            if ( nrm.x + nrm.y != 0 )
            {
                tmin = MIN(tmin, depth_xy / ABS(nrm.x + nrm.y) );
            }

            if ( -nrm.x + nrm.y != 0 )
            {
                tmin = MIN(tmin, depth_yx / ABS(-nrm.x + nrm.y) );
            }

            if ( tmin < 1e6 )
            {
                const float pressure_strength = 0.125f * 0.5f;
                if ( wta >= 0.0f )
                {
                    float ratio = (float)ABS(wtb) / ((float)ABS(wta) + (float)ABS(wtb));

                    imp_a.x = tmin * nrm.x * ratio * pressure_strength;
                    imp_a.y = tmin * nrm.y * ratio * pressure_strength;
                    imp_a.z = tmin * nrm.z * ratio * pressure_strength;
                }

                if ( wtb >= 0.0f )
                {
                    float ratio = (float)ABS(wta) / ((float)ABS(wta) + (float)ABS(wtb));

                    imp_b.x = -tmin * nrm.x * ratio * pressure_strength;
                    imp_b.y = -tmin * nrm.y * ratio * pressure_strength;
                    imp_b.z = -tmin * nrm.z * ratio * pressure_strength;
                }
            }

            // add in the bump impulses
            pchr_a->phys.apos_1.x += imp_a.x * interaction_strength;
            pchr_a->phys.apos_1.y += imp_a.y * interaction_strength;
            pchr_a->phys.apos_1.z += imp_a.z * interaction_strength;

            pchr_b->phys.apos_1.x += imp_b.x * interaction_strength;
            pchr_b->phys.apos_1.y += imp_b.y * interaction_strength;
            pchr_b->phys.apos_1.z += imp_b.z * interaction_strength;

            // you could "bump" something if you changed your velocity, even if you were still touching
            bump = (VDotProduct( pchr_a->vel, nrm ) * VDotProduct( pchr_a->vel_old, nrm ) < 0 ) ||
                   (VDotProduct( pchr_b->vel, nrm ) * VDotProduct( pchr_b->vel_old, nrm ) < 0 );
        }

        // add in the friction due to the "collision"
        // assume coeff of friction of 0.5
        if ( ABS(imp_a.x) + ABS(imp_a.y) + ABS(imp_a.z) > 0.0f &&
            ABS(vpara_a.x) + ABS(vpara_a.y) + ABS(vpara_a.z) > 0.0f &&
            pchr_a->dismount_timer <= 0)
        {
            float imp, vel, factor;

            imp = 0.5f * SQRT( imp_a.x * imp_a.x + imp_a.y * imp_a.y + imp_a.z * imp_a.z );
            vel = SQRT( vpara_a.x * vpara_a.x + vpara_a.y * vpara_a.y + vpara_a.z * vpara_a.z );

            factor = imp / vel;
            factor = CLIP(factor, 0.0f, 1.0f);

            pchr_a->phys.avel.x -= factor * vpara_a.x * interaction_strength;
            pchr_a->phys.avel.y -= factor * vpara_a.y * interaction_strength;
            pchr_a->phys.avel.z -= factor * vpara_a.z * interaction_strength;
            LOG_NAN(pchr_a->phys.avel.z);
        }

        if ( ABS(imp_b.x) + ABS(imp_b.y) + ABS(imp_b.z) > 0.0f &&
            ABS(vpara_b.x) + ABS(vpara_b.y) + ABS(vpara_b.z) > 0.0f &&
            pchr_b->dismount_timer <= 0)
        {
            float imp, vel, factor;

            imp = 0.5f * SQRT( imp_b.x * imp_b.x + imp_b.y * imp_b.y + imp_b.z * imp_b.z );
            vel = SQRT( vpara_b.x * vpara_b.x + vpara_b.y * vpara_b.y + vpara_b.z * vpara_b.z );

            factor = imp / vel;
            factor = CLIP(factor, 0.0f, 1.0f);

            pchr_b->phys.avel.x -= factor * vpara_b.x * interaction_strength;
            pchr_b->phys.avel.y -= factor * vpara_b.y * interaction_strength;
            pchr_b->phys.avel.z -= factor * vpara_b.z * interaction_strength;
            LOG_NAN(pchr_b->phys.avel.z);
        }
    }

    if ( bump )
    {
        ai_state_set_bumplast( &(pchr_a->ai), ichr_b );
        ai_state_set_bumplast( &(pchr_b->ai), ichr_a );
    }

    return btrue;
}


//--------------------------------------------------------------------------------------------
bool_t do_chr_prt_collision( Uint16 ichr_a, Uint16 iprt_b )
{
    Uint16 ipip_b;
    float ftmp1, ftmp2;

    bool_t retval = bfalse;

    float xa, ya, za;
    float xb, yb, zb;

    float  depth_x, depth_y, depth_z, depth_xy, depth_yx;
    bool_t collide_x, collide_y, collide_z, collide_xy, collide_yx;

    bool_t do_platform;

    chr_t * pchr_a;
    cap_t * pcap_a;

    prt_t * pprt_b;
    pip_t * ppip_b;

    // make sure that it is on
    if ( !ACTIVE_CHR(ichr_a) ) return bfalse;
    pchr_a = ChrList.lst + ichr_a;

    if ( !pchr_a->alive ) return bfalse;

    pcap_a = chr_get_pcap( ichr_a );
    if ( NULL == pcap_a ) return bfalse;

    if ( !ACTIVE_PRT( iprt_b ) ) return bfalse;
    pprt_b = PrtList.lst + iprt_b;

    ipip_b = pprt_b->pip_ref;
    if ( INVALID_PIP( ipip_b ) ) return bfalse;
    ppip_b = PipStack.lst + ipip_b;

    // do not collide with the thing that you're attached to
    //if ( ichr_a == pprt_b->attachedto_ref ) return bfalse;

    xa = pchr_a->pos.x;
    ya = pchr_a->pos.y;
    za = pchr_a->pos.z;

    xb = pprt_b->pos.x;
    yb = pprt_b->pos.y;
    zb = pprt_b->pos.z;

    ftmp1 = MIN( (xb + pprt_b->bumpsize) - xa, xa - (xb - pprt_b->bumpsize) );
    ftmp2 = MIN( (xa + pchr_a->chr_prt_cv.max_x) - xb, xb - (xa + pchr_a->chr_prt_cv.min_x));
    depth_x = MAX( ftmp1, ftmp2 );
    collide_x = depth_x > 0;
    if( !collide_x ) return bfalse;

    ftmp1 = MIN( (yb + pprt_b->bumpsize) - ya, ya - (yb - pprt_b->bumpsize) );
    ftmp2 = MIN( (ya + pchr_a->chr_prt_cv.max_y) - yb, yb - (ya + pchr_a->chr_prt_cv.min_y));
    depth_y = MAX( ftmp1, ftmp2 );
    collide_y = depth_y > 0;
    if( !collide_y ) return bfalse;

    ftmp1 = MIN( ((xb + yb) + pprt_b->bumpsizebig) - (xa + ya), (xa + ya) - ((xb + yb) - pprt_b->bumpsizebig) );
    ftmp2 = MIN( ((xa + ya) + pchr_a->chr_prt_cv.max_xy) - (xb + yb), (xb + yb) - ((xa + ya) + pchr_a->chr_prt_cv.min_xy));
    depth_xy = MAX( ftmp1, ftmp2 );
    collide_xy = depth_xy > 0;
    if( !collide_xy ) return bfalse;
    depth_xy *= INV_SQRT_TWO;

    ftmp1 = MIN( ((-xb + yb) + pprt_b->bumpsizebig) - (-xa + ya), (-xa + ya) - ((-xb + yb) - pprt_b->bumpsizebig) );
    ftmp2 = MIN( ((-xa + ya) + pchr_a->chr_prt_cv.max_yx) - (-xb + yb), (-xb + yb) - ((-xa + ya) + pchr_a->chr_prt_cv.min_yx));
    depth_yx = MAX( ftmp1, ftmp2 );
    collide_yx = depth_yx > 0;
    if( !collide_yx ) return bfalse;
    depth_yx *= INV_SQRT_TWO;

    // detect the platform tolerance
    do_platform = bfalse;
    if( pchr_a->platform && !ACTIVE_CHR(pprt_b->attachedto_ref) )
    {
        depth_z = zb - (za + pchr_a->chr_prt_cv.max_z);
        do_platform = ( depth_z > -PLATTOLERANCE && depth_z < PLATTOLERANCE);
    }

    // interacting with a platform?
    if( do_platform  )
    {
        // the only way to get to this point is if the two objects don't collide
        // but they are within the PLATTOLERANCE of each other in the z direction

        // gravity is not handled here

        bool_t z_collide, was_z_collide;

        // it is a valid platform. now figure out the physics

        // are they colliding for the first time?
        z_collide     = (zb < za + pchr_a->chr_prt_cv.max_z);
        was_z_collide = (zb - pprt_b->vel.z < za + pchr_a->chr_prt_cv.max_z - pchr_a->vel.z);

        if( z_collide && !was_z_collide )
        {
            // Particle is falling onto the platform
            pprt_b->pos.z = za + pchr_a->chr_prt_cv.max_z;
            pprt_b->vel.z = pchr_a->vel.z - pprt_b->vel.z * ppip_b->dampen;
            retval = btrue;
        }
        else if (z_collide)
        {
            // colliding this time and last time. particle is *embedded* in the platform
            pprt_b->pos.z = za + pchr_a->chr_prt_cv.max_z;

            if( pprt_b->vel.z - pchr_a->vel.z < 0 )
            {
                pprt_b->vel.z = pchr_a->vel.z * ppip_b->dampen + platstick * pchr_a->vel.z;
            }
            else
            {
                pprt_b->vel.z = pprt_b->vel.z * (1.0f - platstick) + pchr_a->vel.z * platstick;
            }
            pprt_b->vel.x = pprt_b->vel.x * (1.0f - platstick) + pchr_a->vel.x * platstick;
            pprt_b->vel.y = pprt_b->vel.y * (1.0f - platstick) + pchr_a->vel.y * platstick;
            retval = btrue;
        }
        else
        {
            // not colliding this time or last time. particle is just near the platform
            float lerp_z = (zb < za + pchr_a->chr_prt_cv.max_z) / PLATTOLERANCE;
            lerp_z = CLIP(lerp_z, -1, 1);

            if( lerp_z > 0 )
            {
                float tmp_platstick = platstick * lerp_z;
                pprt_b->vel.z = pprt_b->vel.z * (1.0f - tmp_platstick) + pchr_a->vel.z * tmp_platstick;
                pprt_b->vel.x = pprt_b->vel.x * (1.0f - tmp_platstick) + pchr_a->vel.x * tmp_platstick;
                pprt_b->vel.y = pprt_b->vel.y * (1.0f - tmp_platstick) + pchr_a->vel.y * tmp_platstick;
                retval = btrue;
            }
        }

        return btrue;
    }

    // detect normal collision in the z direction
    depth_z = MIN( zb + pprt_b->bumpsize, za + pchr_a->chr_prt_cv.max_z ) - MAX( zb - pprt_b->bumpsize, za + pchr_a->chr_prt_cv.min_z );
    collide_z = depth_z > 0;
    if( !collide_z ) return bfalse;

    retval = bfalse;
    {
        //float was_xa, was_ya, was_za;
        //float was_xb, was_yb, was_zb;
        //float was_depth_x, was_depth_y, was_depth_z, was_depth_xy, was_depth_yx;
        //bool_t was_collide_x, was_collide_y, was_collide_z, was_collide_xy, was_collide_yx;
        //bool_t was_collide;

        // ??? there is no point in dealing with particles and pressure ???
        //was_xa = pchr_a->pos_old.x;
        //was_ya = pchr_a->pos_old.y;
        //was_za = pchr_a->pos_old.z;

        //was_xb = xb - pprt_b->vel.x;
        //was_yb = yb - pprt_b->vel.y;
        //was_zb = zb - pprt_b->vel.z;

        //ftmp1 = MIN( (was_xb + pprt_b->bumpsize) - was_xa, was_xa - (was_xb - pprt_b->bumpsize) );
        //ftmp2 = MIN( (was_xa + pchr_a->chr_prt_cv.max_x) - was_xb, was_xb - (was_xa + pchr_a->chr_prt_cv.min_x));
        //was_depth_x = MAX( ftmp1, ftmp2 );
        //was_collide_x = was_depth_x > 0;

        //ftmp1 = MIN( (was_yb + pprt_b->bumpsize) - was_ya, was_ya - (was_yb - pprt_b->bumpsize) );
        //ftmp2 = MIN( (was_ya + pchr_a->chr_prt_cv.max_y) - was_yb, was_yb - (was_ya + pchr_a->chr_prt_cv.min_y));
        //was_depth_y = MAX( ftmp1, ftmp2 );
        //was_collide_y = was_depth_y > 0;

        //ftmp1 = MIN( (was_zb + pprt_b->bumpsize) - was_za, was_za - (was_zb - pprt_b->bumpsize) );
        //ftmp2 = MIN( (was_za + pchr_a->chr_prt_cv.max_z) - was_zb, was_zb - (was_za + pchr_a->chr_prt_cv.min_z));
        //was_depth_z = MAX( ftmp1, ftmp2 );
        //was_collide_z = was_depth_z > 0;

        //ftmp1 = MIN( ((was_xb + was_yb) + pprt_b->bumpsize) - (was_xa + was_ya), (was_xa + was_ya) - ((was_xb + was_yb) - pprt_b->bumpsize) );
        //ftmp2 = MIN( ((was_xa + was_ya) + pchr_a->chr_prt_cv.max_xy) - (was_xb + was_yb), (was_xb + was_yb) - ((was_xa + was_ya) + pchr_a->chr_prt_cv.min_xy));
        //was_depth_xy = MAX( ftmp1, ftmp2 );
        //was_depth_xy *= INV_SQRT_TWO;
        //was_collide_xy = was_depth_xy > 0;

        //ftmp1 = MIN( ((-was_xb + was_yb) + pprt_b->bumpsize) - (-was_xa + was_ya), (-was_xa + was_ya) - ((-was_xb + was_yb) - pprt_b->bumpsize) );
        //ftmp2 = MIN( ((-was_xa + was_ya) + pchr_a->chr_prt_cv.max_yx) - (-was_xb + was_yb), (-was_xb + was_yb) - ((-was_xa + was_ya) + pchr_a->chr_prt_cv.min_yx));
        //was_depth_yx = MAX( ftmp1, ftmp2 );
        //was_depth_yx *= INV_SQRT_TWO;
        //was_collide_yx = was_depth_yx > 0;

        //was_collide = was_collide_x && was_collide_y && was_collide_z && was_collide_xy && was_collide_yx;

        bool_t prt_wants_deflection;
        bool_t prt_might_bump_chr;
        bool_t mana_paid;
        Uint16 direction;
        Uint16 enchant, enc_next;
        float ax, ay, nx, ny, scale;  // For deflection
        Uint16 facing = 0;

        // Check reaffirmation of particles
        if ( pchr_a->reloadtime == 0 && pchr_a->damagetime == 0 )
        {
            if ( ichr_a != pprt_b->attachedto_ref && pchr_a->reaffirmdamagetype == pprt_b->damagetype )
            {
                reaffirm_attached_particles( ichr_a );
                retval = btrue;
            }
        }

        // determine whether the missile should be deflected
        prt_wants_deflection  = (MISSILE_NORMAL != pchr_a->missiletreatment) &&
            !pchr_a->invictus && ( 0 != pprt_b->damage.base + pprt_b->damage.rand ) &&
            !ACTIVE_CHR(pprt_b->attachedto_ref) && (pprt_b->owner_ref != ichr_a);

        // try to deflect the particle
        mana_paid = bfalse;
        if ( prt_wants_deflection )
        {
            mana_paid = cost_mana( pchr_a->missilehandler, pchr_a->missilecost << 8, pprt_b->owner_ref );
            if ( mana_paid )
            {
                retval = btrue;

                // Treat the missile
                if ( pchr_a->missiletreatment == MISSILE_DEFLECT )
                {
                    // Use old position to find normal
                    ax = pchr_a->pos.x - pprt_b->pos_old.x;
                    ay = pchr_a->pos.y - pprt_b->pos_old.y;

                    // Find size of normal
                    scale = ax * ax + ay * ay;
                    if ( scale > 0 )
                    {
                        // Make the normal a unit normal
                        scale = SQRT( scale );
                        nx = ax / scale;
                        ny = ay / scale;

                        // Deflect the incoming ray off the normal
                        scale = ( pprt_b->vel.x * nx + pprt_b->vel.y * ny ) * 2;
                        ax = scale * nx;
                        ay = scale * ny;
                        pprt_b->vel.x = pprt_b->vel.x - ax;
                        pprt_b->vel.y = pprt_b->vel.y - ay;
                    }
                }
                else if ( pchr_a->missiletreatment == MISSILE_DEFLECT )
                {
                    // Reflect it back in the direction it came
                    pprt_b->vel.x *= -1.0f;
                    pprt_b->vel.y *= -1.0f;
                }

                // Change the owner of the missile
                pprt_b->team = pchr_a->team;
                pprt_b->owner_ref = ichr_a;
                ppip_b->homing = bfalse;

                // Change the direction of the particle
                if ( ppip_b->rotatetoface )
                {
                    // Turn to face new direction
                    facing = vec_to_facing( pprt_b->vel.x , pprt_b->vel.y );
                    pprt_b->facing = facing;
                }
            }
        }

        // if the mana was paid, the particle was definitely deflected
        prt_might_bump_chr = !pchr_a->invictus && !mana_paid && (pprt_b->attachedto_ref != ichr_a);

        // Refine the simple bump test
        if ( prt_might_bump_chr  )
        {
            //bool_t prt_belongs_to_chr;
            //bool_t needs_friendlyfire;
            //bool_t prt_can_hit_chr;
            //bool_t prt_hates_chr;

            //prt_belongs_to_chr = ichr_a == pprt_b->owner_ref ||
            //                     ichr_a == ChrList.lst[pprt_b->owner_ref].attachedto;

            //needs_friendlyfire = !prt_belongs_to_chr || ppip_b->onlydamagefriendly;

            //prt_hates_chr = TeamList[pprt_b->team].hatesteam[pchr_a->team];

            //prt_can_hit_chr =  prt_hates_chr || (ppip_b->friendlyfire && needs_friendlyfire);

            // ignore onlydamagefriendly for the moment
            bool_t prt_belongs_to_chr;
            bool_t prt_can_hit_chr;
            bool_t prt_hates_chr;
            bool_t can_onlydamagefriendly;
            bool_t can_friendlyfire;
            bool_t terminate_particle;

            prt_hates_chr   = TeamList[pprt_b->team].hatesteam[pchr_a->team];

            prt_belongs_to_chr = ichr_a == pprt_b->owner_ref ||
                                 ichr_a == ChrList.lst[pprt_b->owner_ref].attachedto;

            // this is the onlydamagefriendly condition from the particle search code
            can_onlydamagefriendly = (ppip_b->onlydamagefriendly && pprt_b->team == pchr_a->team) ||
                                     (!ppip_b->onlydamagefriendly && prt_hates_chr);

            // I guess "friendly fire" does not mean "self fire", which is a bit unfortunate.
            can_friendlyfire = ppip_b->friendlyfire && !prt_hates_chr && !prt_belongs_to_chr;

            prt_can_hit_chr =  can_friendlyfire || can_onlydamagefriendly;

            terminate_particle = bfalse;
            if ( prt_can_hit_chr )
            {
                int actual_damage, max_damage;

                retval = btrue;

                // Catch on fire
                spawn_bump_particles( ichr_a, iprt_b );

                // we can't even get to this point if the character is completely invulnerable (invictus)
                // so the only barier left is whether the character can be damaged this update
                if ( 0 == pchr_a->damagetime )
                {
                    GLvector3 vdiff;
                    float prt_mass, prt_ke;

                    // clean up the enchant list before doing anything
                    pchr_a->firstenchant = cleanup_enchant_list( pchr_a->firstenchant );

                    // Check all enchants to see if they are removed
                    enchant = pchr_a->firstenchant;
                    while ( enchant != MAX_ENC )
                    {
                        enc_next = EncList.lst[enchant].nextenchant_ref;
                        if ( enc_is_removed( enchant, pprt_b->profile_ref ) )
                        {
                            remove_enchant( enchant );
                        }
                        enchant = enc_next;
                    }

                    // Do confuse effects
                    if ( 0 == ( Md2FrameList[pchr_a->inst.frame_nxt].framefx&MADFX_INVICTUS ) || ppip_b->damfx&DAMFX_NBLOC )
                    {
                        if ( ppip_b->grogtime > 0 && pcap_a->canbegrogged )
                        {
                            pchr_a->ai.alert |= ALERTIF_GROGGED;
                            if( ppip_b->grogtime > pchr_a->grogtime )
                            {
                                pchr_a->grogtime = MAX(0, pchr_a->grogtime + ppip_b->grogtime);
                            }
                        }
                        if ( ppip_b->dazetime > 0 && pcap_a->canbedazed )
                        {
                            pchr_a->ai.alert |= ALERTIF_DAZED;
                            if( ppip_b->dazetime > pchr_a->dazetime )
                            {
                                pchr_a->dazetime = MAX(0, pchr_a->dazetime + ppip_b->dazetime);
                            }
                        }
                    }

                    // DAMFX_ARRO means that it only does damage when stuck to something, not when it hits?
                    max_damage    = 0;
                    actual_damage = 0;
                    if( 0 == ( ppip_b->damfx&DAMFX_ARRO ) )
                    {
                        IPair loc_damage = pprt_b->damage;

                        direction = vec_to_facing( pprt_b->vel.x , pprt_b->vel.y );
                        direction = pchr_a->turn_z - direction + ATK_BEHIND;

                        // Apply intelligence/wisdom bonus damage for particles with the [IDAM] and [WDAM] expansions (Low ability gives penality)
                        // +2% bonus for every point of intelligence and/or wisdom above 14. Below 14 gives -2% instead!
                        if ( ppip_b->intdamagebonus )
                        {
                            float percent;
                            percent = ( (FP8_TO_INT(ChrList.lst[pprt_b->owner_ref].intelligence)) - 14 ) * 2;
                            percent /= 100;
                            loc_damage.base *= 1.00f + percent;
                        }

                        if ( ppip_b->wisdamagebonus )
                        {
                            float percent;
                            percent = ( FP8_TO_INT(ChrList.lst[pprt_b->owner_ref].wisdom) - 14 ) * 2;
                            percent /= 100;
                            loc_damage.base *= 1.00f + percent;
                        }

                        // handle vulnerabilities
                        if ( chr_has_vulnie( ichr_a, pprt_b->profile_ref ) )
                        {
                            loc_damage.base = (loc_damage.base << 1);
                            loc_damage.rand = (loc_damage.rand << 1) | 1;

                            pchr_a->ai.alert |= ALERTIF_HITVULNERABLE;
                        }
                        max_damage = loc_damage.base + loc_damage.rand;

                        // Damage the character
                        actual_damage = damage_character( ichr_a, direction, loc_damage, pprt_b->damagetype, pprt_b->team, pprt_b->owner_ref, ppip_b->damfx, bfalse );

                        // we're supposed to blank out the damage here so that swords and such don't
                        // kill everything in one swipe?
                        pprt_b->damage = loc_damage;
                    }

                    // determine an "effective mass" for the particle
                    {
                        GLvector3 vtmp;
                        float prt_vel2, vel_nrm;

                        //if( ACTIVE_CHR(pprt_b->attachedto_ref) )
                        //{
                        //    // just in case there is a problem with the pprt_b->pos calculation
                        //    // for attached particles
                        //    vtmp = VSub( pprt_b->pos, pprt_b->pos_old );
                        //}
                        //else
                        //{
                            vtmp = pprt_b->vel;
                        //}
                        vdiff = VSub( vtmp, pchr_a->vel );

                        prt_vel2 = VDotProduct( vdiff, vdiff );

                        vdiff = VNormalize( vdiff );
                        vel_nrm = SQRT( MIN( 100.0f, prt_vel2 ) );
                        vdiff.x *= vel_nrm;
                        vdiff.y *= vel_nrm;
                        vdiff.z *= vel_nrm;

                        // make a minimum velocity (a maximum mass)
                        prt_vel2 = MAX( 100.0f, prt_vel2 );

                        // get the "kinetic energy" from the damage
                        prt_ke = 40.0f * ABS(max_damage);

                        // the faster the particle is going, the smaller the "mass" it
                        // needs to do the damage
                        prt_mass = 0.0f;
                        if( prt_vel2 > 0.0f )
                        {
                            prt_mass = prt_ke / ( 0.5f * prt_vel2 );
                        }
                        prt_mass = MAX( 1.0f, prt_mass );
                    }

                    // Do the impulse to the object that was hit
                    if ( ppip_b->allowpush  && pchr_a->phys.weight != 0xFFFFFFFF && 
                        (ABS(actual_damage) > 0) && (ABS(max_damage) > 0) && 
                        (ABS(vdiff.x) + ABS(vdiff.y) + ABS(vdiff.z) > 0.0f)  )
                    {
                        float factor;
                        GLvector3 impulse;

                        // the faster the particle (the smaller the particle "mass" ), the bigger the
                        // kickback
                        factor = 1.0f / (1.0f + pchr_a->phys.weight / ABS(prt_mass) );

                        // modify it by the damage type
                        if( DAMAGE_CRUSH == pprt_b->damagetype )
                        {
                            // very blunt type of attack, the maximum effect
                            //factor *= 1.0f;
                        }
                        else if ( DAMAGE_POKE == pprt_b->damagetype )
                        {
                            // very focussed type of attack, the minimum effect
                            factor /= 2.0f;
                        }
                        else
                        {
                            // all other damage types are in the middle
                            factor *= INV_SQRT_TWO;
                        }

                        // modify it by the the severity of the damage
                        // reduces the damage below actual_damage == pchr_a->lifemax
                        // and it doubles it if actual_damage is really huge
                        factor *= 2.0f * (float)actual_damage / (float)(ABS(actual_damage) + pchr_a->lifemax);

                        // calculate the "impulse"
                        impulse = vdiff;
                        impulse.x *= factor;
                        impulse.y *= factor;
                        impulse.z *= factor;

                        // apply the "impulse"
                        pchr_a->phys.avel.x  += impulse.x;
                        pchr_a->phys.avel.y  += impulse.y;
                        pchr_a->phys.avel.z  += impulse.z;
                    }

                    // do the rebound impulse on the particle
                    if( (ABS(max_damage) - ABS(actual_damage)) > 0 && ppip_b->allowpush )
                    {
                        float recoil;
                        float factor;
                        GLvector3 impulse;

                        recoil = (float)(ABS(max_damage) - ABS(actual_damage)) / (float)ABS(max_damage);

                        // calculate the "impulse"
                        impulse = vdiff;
                        impulse.x *= -recoil;
                        impulse.y *= -recoil;
                        impulse.z *= -recoil;

                        if( ACTIVE_CHR(pprt_b->attachedto_ref) )
                        {
                            // transmit the force of the blow back to the character that is
                            // holding the weapon
                            if( ACTIVE_CHR( pprt_b->owner_ref ) )
                            {
                                chr_t * powner = ChrList.lst + pprt_b->owner_ref;

                                factor = 1.0f;
                                if( powner->phys.weight > 0 )
                                {
                                    factor = (float) MIN(prt_mass, 120.0f) / (float)powner->phys.weight;
                                }
                                factor = MIN(factor, 1.0f);

                                powner->phys.avel.x += impulse.x * factor;
                                powner->phys.avel.y += impulse.y * factor;
                                powner->phys.avel.z += impulse.z * factor;
                            }
                        }
                        else
                        {
                            // if 100% damage, then the particle comes to rest in the character
                            pprt_b->vel.x += impulse.x;
                            pprt_b->vel.y += impulse.y;
                            pprt_b->vel.z += impulse.z;
                        }
                    }

                    // Notify the attacker of a scored hit
                    if ( ACTIVE_CHR(pprt_b->owner_ref) )
                    {
                        Uint16 item;

                        chr_get_pai(pprt_b->owner_ref)->alert |= ALERTIF_SCOREDAHIT;
                        chr_get_pai(pprt_b->owner_ref)->hitlast = ichr_a;

                        //Tell the weapons who the attacker hit last
                        item = ChrList.lst[pprt_b->owner_ref].holdingwhich[SLOT_LEFT];
                        if ( ACTIVE_CHR(item) )
                        {
                            ChrList.lst[item].ai.hitlast = ichr_a;
                        }

                        item = ChrList.lst[pprt_b->owner_ref].holdingwhich[SLOT_RIGHT];
                        if ( ACTIVE_CHR(item) )
                        {
                            ChrList.lst[item].ai.hitlast = ichr_a;
                        }
                    }
                }

                if ( ppip_b->endbump )
                {
                    if ( ppip_b->bumpmoney )
                    {
                        if ( pchr_a->cangrabmoney && pchr_a->alive && 0 == pchr_a->damagetime && pchr_a->money < MAXMONEY )
                        {
                            Uint16 igrabber;

                            igrabber = ichr_a;

                            // Let mounts collect money for their riders
                            if ( pchr_a->ismount && ACTIVE_CHR(pchr_a->holdingwhich[SLOT_LEFT]) )
                            {
                                igrabber = pchr_a->holdingwhich[SLOT_LEFT];
                            }

                            ChrList.lst[igrabber].money += ppip_b->bumpmoney;
                            ChrList.lst[igrabber].money = CLIP( ChrList.lst[igrabber].money, 0, MAXMONEY );

                            // the coin disappears when you pick it up
                            terminate_particle = btrue;
                        }

                    }
                    else
                    {
                        // Only hit one character, not several
                        terminate_particle = btrue;
                    }
                }
            }

            if( terminate_particle )
            {
                prt_request_terminate( iprt_b );
            }

        }
    }


    return retval;
}

//--------------------------------------------------------------------------------------------
// collision data
static int       cdata_count = 0;
static co_data_t cdata[CHR_MAX_COLLISIONS];

// collision data hash nodes
static int         hn_count = 0;
static hash_node_t hnlst[COLLISION_HASH_NODES];

void bump_all_objects( void )
{
    // ZZ> This function sets handles characters hitting other characters or particles

    int tnc, cnt;

    co_data_t   * d;

    // blank the accumulators
    for ( cnt = 0; cnt < MAX_CHR; cnt++ )
    {
        ChrList.lst[cnt].phys.apos_0.x = 0.0f;
        ChrList.lst[cnt].phys.apos_0.y = 0.0f;
        ChrList.lst[cnt].phys.apos_0.z = 0.0f;

        ChrList.lst[cnt].phys.apos_1.x = 0.0f;
        ChrList.lst[cnt].phys.apos_1.y = 0.0f;
        ChrList.lst[cnt].phys.apos_1.z = 0.0f;

        ChrList.lst[cnt].phys.avel.x = 0.0f;
        ChrList.lst[cnt].phys.avel.y = 0.0f;
        ChrList.lst[cnt].phys.avel.z = 0.0f;
    }

    // fill up the bumplists
    fill_bumplists();

    // fill the collision list with all possible binary interactions
    fill_interaction_list( cdata, &cdata_count, hnlst, &hn_count );

    // Do platforms
    for ( cnt = 0; cnt < chr_co_list->allocated; cnt++ )
    {
        hash_node_t * n;
        int count = chr_co_list->subcount[cnt];

        n = chr_co_list->sublist[cnt];
        for ( tnc = 0; tnc < count && NULL != n; tnc++, n = n->next )
        {
            // only look at character-character interactions
            d = (co_data_t *)(n->data);
            if (TOTAL_MAX_PRT != d->prtb) continue;

            do_platforms( d->chra, d->chrb );
        }
    }

    // Do mounts
    for ( cnt = 0; cnt < chr_co_list->allocated; cnt++ )
    {
        hash_node_t * n;
        int count = chr_co_list->subcount[cnt];

        n = chr_co_list->sublist[cnt];
        for ( tnc = 0; tnc < count && NULL != n; tnc++, n = n->next )
        {
            // only look at character-character interactions
            d = (co_data_t *)(n->data);
            if (TOTAL_MAX_PRT != d->prtb) continue;

            do_mounts( d->chra, d->chrb );
        }
    }

    // do all interactions
    for ( cnt = 0; cnt < chr_co_list->allocated; cnt++ )
    {
        hash_node_t * n;
        int count = chr_co_list->subcount[cnt];

        n = chr_co_list->sublist[cnt];
        for ( tnc = 0; tnc < count && NULL != n; tnc++, n = n->next )
        {
            // only look at character-character interactions
            d = (co_data_t *)(n->data);

            if ( TOTAL_MAX_PRT == d->prtb )
            {
                do_chr_chr_collision( d->chra, d->chrb );
            }
            else if ( MAX_CHR == d->chrb )
            {
                do_chr_prt_collision( d->chra, d->prtb );
            }
        }
    }

    // accumulate the accumulators
    for ( cnt = 0; cnt < MAX_CHR; cnt++ )
    {
        float tmpx, tmpy, tmpz;
        chr_t * pchr;
        float bump_str;
        float nrm[2];

        if ( !ACTIVE_CHR(cnt) ) continue;
        pchr = ChrList.lst + cnt;

        bump_str = 1.0f;
        if ( ACTIVE_CHR( pchr->attachedto ) )
        {
            bump_str = 0;
        }
        else if ( pchr->dismount_timer > 0 )
        {
            bump_str = 1.0f - (float)pchr->dismount_timer / PHYS_DISMOUNT_TIME;
            bump_str = bump_str * bump_str * 0.5f;
        };

        // decrement the dismount timer
        if ( pchr->dismount_timer > 0 ) pchr->dismount_timer--;

        // do the "integration" of the accumulated accelerations
        pchr->vel.x += pchr->phys.avel.x;
        pchr->vel.y += pchr->phys.avel.y;
        pchr->vel.z += pchr->phys.avel.z;

        // do the "integration" on the position
        if ( ABS(pchr->phys.apos_0.x + pchr->phys.apos_1.x) > 0 )
        {
            tmpx = pchr->pos.x;
            pchr->pos.x += pchr->phys.apos_0.x + pchr->phys.apos_1.x;
            if ( __chrhitawall(cnt, nrm) )
            {
                // restore the old values
                pchr->pos.x = tmpx;
            }
            else
            {
                pchr->vel.x += pchr->phys.apos_1.x * bump_str;
                pchr->pos_safe.x = tmpx;
            }
        }

        if ( ABS(pchr->phys.apos_0.y + pchr->phys.apos_1.y) > 0 )
        {
            tmpy = pchr->pos.y;
            pchr->pos.y += pchr->phys.apos_0.y + pchr->phys.apos_1.y;
            if ( __chrhitawall(cnt, nrm) )
            {
                // restore the old values
                pchr->pos.y = tmpy;
            }
            else
            {
                pchr->vel.y += pchr->phys.apos_1.y * bump_str;
                pchr->pos_safe.y = tmpy;
            }
        }

        if ( ABS(pchr->phys.apos_0.z + pchr->phys.apos_1.z) > 0 )
        {
            tmpz = pchr->pos.z;
            pchr->pos.z += pchr->phys.apos_0.z + pchr->phys.apos_1.z;
            if ( pchr->pos.z < pchr->enviro.floor_level )
            {
                // restore the old values
                pchr->pos.z = tmpz;
            }
            else
            {
                pchr->vel.z += pchr->phys.apos_1.z * bump_str;
                pchr->pos_safe.z = tmpz;
            }
        }

        pchr->safe_valid = ( 0 == __chrhitawall(cnt, nrm) );
    }

    // The following functions need to be called any time you actually change a charcter's position
    keep_weapons_with_holders();
    attach_particles();
    make_all_character_matrices( update_wld != 0 );
}

//--------------------------------------------------------------------------------------------
void tilt_characters_to_terrain()
{
    // ZZ> This function sets all of the character's starting tilt values
    int cnt;
    Uint8 twist;

    for ( cnt = 0; cnt < MAX_CHR; cnt++ )
    {
        if ( !ACTIVE_CHR(cnt) ) continue;

        if ( ChrList.lst[cnt].stickybutt && VALID_TILE(PMesh, ChrList.lst[cnt].onwhichfan) )
        {
            twist = PMesh->mmem.tile_list[ChrList.lst[cnt].onwhichfan].twist;
            ChrList.lst[cnt].map_turn_y = map_twist_y[twist];
            ChrList.lst[cnt].map_turn_x = map_twist_x[twist];
        }
        else
        {
            ChrList.lst[cnt].map_turn_y = MAP_TURN_OFFSET;
            ChrList.lst[cnt].map_turn_x = MAP_TURN_OFFSET;
        }
    }

}

//--------------------------------------------------------------------------------------------
void import_dir_profiles( const char * dirname )
{
    STRING newloadname;
    STRING filename;
    int cnt;

    if( NULL == PMod || INVALID_CSTR(dirname) ) return;

    if( !PMod->importvalid || 0 == PMod->importamount ) return;

    for ( cnt = 0; cnt < PMod->importamount*MAXIMPORTPERPLAYER; cnt++ )
    {
        // Make sure the object exists...
        snprintf( filename, SDL_arraysize( filename), "%s" SLASH_STR "temp%04d.obj", dirname, cnt );
        snprintf( newloadname, SDL_arraysize( newloadname), "%s" SLASH_STR "data.txt", filename );

        if ( vfs_exists(newloadname) )
        {
            // new player found
            if ( 0 == ( cnt % MAXIMPORTPERPLAYER ) ) import_data.player++;

            // store the slot info
            import_data.slot = cnt;

            // load it
            import_data.slot_lst[cnt] = load_one_profile( filename, MAX_PROFILE );
            import_data.max_slot      = MAX(import_data.max_slot, cnt);
        }
    }

}

//--------------------------------------------------------------------------------------------
void load_all_profiles_import()
{
    int cnt;

    // Clear the import slots...
    for ( cnt = 0; cnt < MAX_PROFILE; cnt++ )
    {
        import_data.slot_lst[cnt] = 10000;
    }
    import_data.max_slot = -1;

    // This overwrites existing loaded slots that are loaded globally
    overrideslots = btrue;
    import_data.player = -1;
    import_data.slot   = -100;

    import_dir_profiles( "import" );
    import_dir_profiles( "remote" );
}

//--------------------------------------------------------------------------------------------
void game_load_module_profiles( const char *modname )
{
    // BB> Search for .obj directories int the module directory and load them

    const char *filehandle;
    STRING newloadname;

    import_data.slot = -100;
    make_newloadname( modname, "objects", newloadname );

    filehandle = vfs_findFirst( newloadname, "obj", VFS_SEARCH_DIR );
    while ( filehandle != NULL )
    {
        load_one_profile( filehandle, MAX_PROFILE );
        filehandle = vfs_findNext();
    }
    vfs_findClose();
}

//--------------------------------------------------------------------------------------------
void game_load_global_profiles()
{
    // load all special objects
    load_one_profile( "basicdat" SLASH_STR "book.obj", SPELLBOOK );

    // load the objects from various import directories
    load_all_profiles_import();
}

//--------------------------------------------------------------------------------------------
void game_load_all_profiles( const char *modname )
{
    // ZZ> This function loads a module's local objects and overrides the global ones already loaded

    // Log all of the script errors
    parseerror = bfalse;

    // clear out any old object definitions
    release_all_pro();

    // load the global objects
    game_load_global_profiles();

    // load the objects from the module's directory
    game_load_module_profiles( modname );

    log_madused( "debug" SLASH_STR "slotused.txt" );
}

//--------------------------------------------------------------------------------------------
bool_t chr_setup_apply( Uint16 ichr, spawn_file_info_t *pinfo )
{
    chr_t * pchr;

    // trap bad pointers
    if ( NULL == pinfo ) return bfalse;

    if ( !ACTIVE_CHR(ichr) ) return bfalse;
    pchr = ChrList.lst + ichr;

    pchr->money += pinfo->money;
    if ( pchr->money > MAXMONEY )  pchr->money = MAXMONEY;
    if ( pchr->money < 0 )  pchr->money = 0;

    pchr->ai.content = pinfo->content;
    pchr->ai.passage = pinfo->passage;

    if ( pinfo->attach == ATTACH_INVENTORY )
    {
        // Inventory character
        inventory_add_item( ichr, pinfo->parent );

        pchr->ai.alert |= ALERTIF_GRABBED;  // Make spellbooks change
        pchr->attachedto = pinfo->parent;  // Make grab work
        let_character_think( ichr );  // Empty the grabbed messages

        pchr->attachedto = MAX_CHR;  // Fix grab

    }
    else if ( pinfo->attach == ATTACH_LEFT || pinfo->attach == ATTACH_RIGHT )
    {
        // Wielded character
        grip_offset_t grip_off = ( ATTACH_LEFT == pinfo->attach ) ? GRIP_LEFT : GRIP_RIGHT;
        attach_character_to_mount( ichr, pinfo->parent, grip_off );

        // Handle the "grabbed" messages
        let_character_think( ichr );
    }

    // Set the starting pinfo->level
    if ( pinfo->level > 0 )
    {
        while ( pchr->experiencelevel < pinfo->level && pchr->experience < MAXXP )
        {
            give_experience( ichr, 25, XP_DIRECT, btrue );
            do_level_up( ichr );
        }
    }

    return btrue;
}

//--------------------------------------------------------------------------------------------
// gcc does not define this function on linux (at least not Ubuntu),
// but it is defined under MinGW, which is yucky.
// I actually had to spend like 45 minutes looking up the compiler flags
// to catch this... good documentation, guys!
#if defined(__GNUC__) && !(defined (__MINGW) || defined(__MINGW32__))
int strlwr( char * str )
{
    if( NULL == str ) return -1;

    while( CSTR_END != *str )
    {
        *str = tolower(*str);
        str++;
    }

    return 0;
}
#endif

//--------------------------------------------------------------------------------------------
bool_t setup_characters_load_object( spawn_file_info_t * psp_info )
{
    // BB> Try to load a global object named psp_info->spawn_coment into slot psp_info->slot

    STRING filename;

    if( NULL == psp_info || VALID_PRO(psp_info->slot) ) return bfalse;

    // trim any excess spaces off the psp_info->spawn_coment
    str_trim(psp_info->spawn_coment);

    if( NULL == strstr(psp_info->spawn_coment, ".obj" ) )
    {
        strcat( psp_info->spawn_coment, ".obj" );
    }

    strlwr( psp_info->spawn_coment );

    // do the loading
    if( CSTR_END != psp_info->spawn_coment[0] )
    {
        snprintf( filename, SDL_arraysize(filename), "basicdat" SLASH_STR "globalobjects" SLASH_STR "%s", psp_info->spawn_coment );

        psp_info->slot = load_one_profile( filename, psp_info->slot );
    }

    return btrue;
}

//--------------------------------------------------------------------------------------------
bool_t setup_characters_spawn( spawn_file_info_t * psp_info )
{
    int tnc;
    int new_object, local_index = 0;

    if( NULL == psp_info || !psp_info->do_spawn ) return bfalse;

    // Spawn the character
    new_object = spawn_one_character( psp_info->pos, psp_info->slot, psp_info->team, psp_info->skin, psp_info->facing, psp_info->pname, MAX_CHR );
    if ( !ACTIVE_CHR(new_object) ) return bfalse;

    // determine the attachment
    if ( psp_info->attach == ATTACH_NONE )
    {
        // Free character
        psp_info->parent = new_object;
        make_one_character_matrix( new_object );
    }

    chr_setup_apply( new_object, psp_info );

    // Turn on PlaList_count input devices
    if ( psp_info->stat )
    {
        if ( 0 == PMod->importamount && PlaList_count < PMod->playeramount )
        {
            if ( 0 == local_numlpla )
            {
                // the first player gets everything
                add_player( new_object, PlaList_count, (Uint32)(~0) );
            }
            else
            {
                Uint32 bits;

                // each new player steals an input device from the 1st player
                bits = 1 << local_numlpla;
                for ( tnc = 0; tnc < MAXPLAYER; tnc++ )
                {
                    PlaList[tnc].device.bits &= ~bits;
                }

                add_player( new_object, PlaList_count, bits );
            }
        }
        else if ( PlaList_count < local_import_count && PlaList_count < PMod->importamount && PlaList_count < PMod->playeramount )
        {
            // Multiplayer import module
            local_index = -1;
            for ( tnc = 0; tnc < local_import_count; tnc++ )
            {
                if ( ChrList.lst[new_object].iprofile <= import_data.max_slot && ChrList.lst[new_object].iprofile < MAX_PROFILE )
                {
                    if( import_data.slot_lst[ChrList.lst[new_object].iprofile] == local_import_slot[tnc] )
                    {
                        local_index = tnc;
                        break;
                    }
                }
            }

            if ( -1 != local_index )
            {
                // It's a local PlaList_count
                add_player( new_object, PlaList_count, local_import_control[local_index] );
            }
            else
            {
                // It's a remote PlaList_count
                add_player( new_object, PlaList_count, INPUT_BITS_NONE );
            }
        }

        // Turn on the stat display
        statlist_add( new_object );
    }

    return btrue;
}

//--------------------------------------------------------------------------------------------
void setup_characters( const char *modname )
{
    // ZZ> This function sets up character data, loaded from "SPAWN.TXT"

    STRING            newloadname;
    vfs_FILE         *fileread;
    spawn_file_info_t sp_info;

    // Turn some back on
    make_newloadname( modname, "gamedat" SLASH_STR "spawn.txt", newloadname );
    fileread = vfs_openRead( newloadname );

    PlaList_count = 0;
    sp_info.parent = MAX_CHR;
    if ( NULL == fileread )
    {
        log_error( "Cannot read file: %s\n", newloadname );
    }
    else
    {
        sp_info.parent = 0;

        while ( spawn_file_scan( fileread, &sp_info ) )
        {
            int save_slot = sp_info.slot;

            // check to see if the slot is valid
            if( -1 == sp_info.slot || sp_info.slot >= MAX_PROFILE )
            {
                log_warning( "Invalid slot %d for \"%s\" in file \"%s\"\n", sp_info.slot, sp_info.spawn_coment, newloadname );
                continue;
            }

            // check to see if something is in that slot
            if( INVALID_PRO(sp_info.slot) )
            {
                setup_characters_load_object( &sp_info );
            }

            if( INVALID_PRO(sp_info.slot) )
            {
                if ( save_slot > PMod->importamount * MAXIMPORTPERPLAYER )
                {
                    log_warning( "The object \"%s\"(slot %d) in file \"%s\" does not exist on this machine\n", sp_info.spawn_coment, save_slot, newloadname );
                }
                continue;
            }

            // do the spawning if need be
            setup_characters_spawn( &sp_info );
        }

        vfs_close( fileread );
    }

    clear_messages();

    // Make sure local players are displayed first
    statlist_sort();

    // Fix tilting trees problem
    tilt_characters_to_terrain();
}

//--------------------------------------------------------------------------------------------
void load_all_global_objects()
{
    // ZF> This function loads all global objects found in the basicdat folder
    const char *filehandle;

    // Warn the user for any duplicate slots
    overrideslots = bfalse;

    // Search for .obj directories and load them
    filehandle = vfs_findFirst( "basicdat" SLASH_STR "globalobjects", "obj", VFS_SEARCH_DIR );
    while ( VALID_CSTR(filehandle) )
    {
        load_one_profile( filehandle, MAX_PROFILE );
        filehandle = vfs_findNext();
    }

    vfs_findClose();
}

//--------------------------------------------------------------------------------------------
void game_reset_module_data()
{
    // reset all
    log_info( "Resetting module data\n" );

    // unload a lot of data
    reset_teams();
    release_all_profiles();
    free_all_objects();
    reset_messages();
    prime_names();
    game_reset_players();

    reset_end_text();
    reset_renderlist();
}

//--------------------------------------------------------------------------------------------
void game_load_global_assets()
{
    // load a bunch of assets that are used in the module

    // Load all the global icons
    if ( !load_all_global_icons() )
    {
        log_warning( "Could not load all global icons!\n" );
    }
    load_blip_bitmap();
    load_bars();
    font_bmp_load( "basicdat" SLASH_STR "font", "basicdat" SLASH_STR "font.txt" );
}

//--------------------------------------------------------------------------------------------
void game_load_module_assets( const char *modname )
{
    // load a bunch of assets that are used in the module
    load_global_waves( modname );
    reset_particles( modname );
    read_wawalite( modname );
    load_basic_textures( modname );
    load_map( modname );

    upload_wawalite();
}

//--------------------------------------------------------------------------------------------
void game_load_all_assets( const char *modname )
{
    game_load_global_assets();

    game_load_module_assets( modname );
}

//--------------------------------------------------------------------------------------------
void game_setup_module( const char *smallname )
{
    // ZZ> This runst the setup functions for a module

    STRING modname;

    // make sure the object lists are empty
    free_all_objects();

    // generate the module directory
    strncpy(modname, smallname, SDL_arraysize(modname));
    str_append_slash(modname, SDL_arraysize(modname));

    setup_particles();
    setup_all_passages( modname );
    setup_characters( modname );
    setup_alliances( modname );
}

//--------------------------------------------------------------------------------------------
bool_t game_load_module_data( const char *smallname )
{
    // ZZ> This function loads a module
    STRING modname;

    log_info( "Loading module \"%s\"\n", smallname );

    load_ai_script( "basicdat" SLASH_STR "script.txt" );

    // generate the module directory
    strncpy(modname, smallname, SDL_arraysize(modname));
    str_append_slash(modname, SDL_arraysize(modname));

    // load all module assets
    game_load_all_assets( modname );

    // load all module objects
    game_load_all_profiles(modname);

    if ( NULL == mesh_load( modname, PMesh ) )
    {
        // do not cause the program to fail, in case we are using a script function to load a module
        // just return a failure value and log a warning message for debugging purposes
        log_warning( "Uh oh! Problems loading the mesh! (%s)\n", modname );
        return bfalse;
    }

    return btrue;
}

//--------------------------------------------------------------------------------------------
void disaffirm_attached_particles( Uint16 character )
{
    // ZZ> This function makes sure a character has no attached particles
    Uint16 particle;

    for ( particle = 0; particle < maxparticles; particle++ )
    {
        if ( !ACTIVE_PRT(particle) ) continue;

        if( PrtList.lst[particle].attachedto_ref == character )
        {
            prt_request_terminate( particle );
        }
    }

    if( ACTIVE_CHR(character) )
    {
        // Set the alert for disaffirmation ( wet torch )
        ChrList.lst[character].ai.alert |= ALERTIF_DISAFFIRMED;
    }
}

//--------------------------------------------------------------------------------------------
Uint16 number_of_attached_particles( Uint16 character )
{
    // ZZ> This function returns the number of particles attached to the given character
    Uint16 cnt = 0;
    Uint16 particle;

    for ( particle = 0; particle < maxparticles; particle++ )
    {
        if ( ACTIVE_PRT(particle) && PrtList.lst[particle].attachedto_ref == character )
        {
            cnt++;
        }
    }

    return cnt;
}

//--------------------------------------------------------------------------------------------
void reaffirm_attached_particles( Uint16 character )
{
    // ZZ> This function makes sure a character has all of it's particles
    Uint16 numberattached;
    Uint16 particle;
    chr_t * pchr;
    cap_t * pcap;

    if( !ACTIVE_CHR(character) ) return;
    pchr = ChrList.lst + character;

    pcap = pro_get_pcap(pchr->iprofile);
    if( NULL == pcap ) return;

    numberattached = number_of_attached_particles( character );

    for ( /*nothing*/; numberattached < pcap->attachedprt_amount; numberattached++ )
    {
        particle = spawn_one_particle( pchr->pos, 0, pchr->iprofile, pcap->attachedprt_pip, character, GRIP_LAST + numberattached, chr_get_iteam(character), character, TOTAL_MAX_PRT, numberattached, MAX_CHR );
        if ( ACTIVE_PRT(particle) )
        {
            attach_particle_to_character( particle, character, PrtList.lst[particle].vrt_off );
        }
    }

    // Set the alert for reaffirmation ( for exploding barrels with fire )
    pchr->ai.alert |= ALERTIF_REAFFIRMED;
}

//--------------------------------------------------------------------------------------------
void game_quit_module()
{
    // BB > all of the de-initialization code after the module actually ends

    // stop the module
    game_module_stop( PMod );

    // get rid of the game/module data
    game_release_module_data();

    // turn off networking
    close_session();

    // reset the "ui" mouse state
    cursor_reset();

    // re-initialize all game/module data
    game_reset_module_data();

    // finish whatever in-game song is playing
    sound_finish_sound();
}

//-----------------------------------------------------------------
bool_t game_begin_module( const char * modname, Uint32 seed )
{
    // BB> all of the initialization code before the module actually starts

    if ( ~0 == seed ) seed = time(NULL);

    // make sure the old game has been quit
    game_quit_module();


    reset_timers();
	
    // load all the in-game module data
    srand( seed );
    if ( !game_load_module_data( modname ) )
    {
        game_module_stop( PMod );
        return bfalse;
    };

    game_setup_module( modname );

    // make sure the per-module configuration settings are correct
    setup_synch( &cfg );

    // initialize the game objects
    update_all_objects();
    cursor_reset();
    game_module_reset( PMod, seed );
    camera_reset( PCamera, PMesh );
    make_all_character_matrices( update_wld != 0 );
    attach_particles();

    // initialize the network
    net_initialize();
    net_sayHello();

    // start the module
    game_module_start( PMod );

    // initislize the timers as tha very last thing
    timeron = bfalse;

    return btrue;
}

//-----------------------------------------------------------------
bool_t game_update_imports()
{
    // BB> This function saves all the players to the players dir
    //    and also copies them into the imports dir to prepare for the next module

    bool_t is_local;
    int cnt, tnc, j, character, player;
    STRING srcPlayer, srcDir, destDir;

    // do the normal export to save all the player settings
    export_all_players( btrue );

    // reload all of the available players
    check_player_import( "players", btrue  );
    check_player_import( "remote",  bfalse );

    // build the import directory using the player info
    vfs_empty_import_directory();
    vfs_mkdir( "import" );

    // export all of the players directly from memory straight to the "import" dir
    for ( player = 0, cnt = 0; cnt < MAXPLAYER; cnt++ )
    {
        if ( !PlaList[cnt].valid ) continue;

        // Is it alive?
        character = PlaList[cnt].index;
        if ( !ACTIVE_CHR(character) ) continue;

        is_local = ( INPUT_BITS_NONE != PlaList[cnt].device.bits );

        // find the saved copy of the players that are in memory right now
        for ( tnc = 0; tnc < loadplayer_count; tnc++ )
        {
            if ( 0 == strcmp( loadplayer[tnc].dir, str_encode_path(ChrList.lst[character].name) ) )
            {
                break;
            }
        }

        if ( tnc == loadplayer_count )
        {
            log_warning( "game_update_imports() - cannot find exported file for \"%s\" (\"%s\") \n", ChrList.lst[character].name, str_encode_path(ChrList.lst[character].name) ) ;
            continue;
        }

        // grab the controls from the currently loaded players
        // calculate the slot from the current player count
        local_import_control[player] = PlaList[cnt].device.bits;
        local_import_slot[player]    = player * MAXIMPORTPERPLAYER;
        player++;

        // Copy the character to the import directory
        if ( is_local )
        {
            snprintf( srcPlayer, SDL_arraysize( srcPlayer), "players" SLASH_STR "%s", loadplayer[tnc].dir );
        }
        else
        {
            snprintf( srcPlayer, SDL_arraysize( srcPlayer), "remote" SLASH_STR "%s", loadplayer[tnc].dir );
        }

        snprintf( destDir, SDL_arraysize( destDir), "import" SLASH_STR "temp%04d.obj", local_import_slot[tnc] );
        vfs_copyDirectory( srcPlayer, destDir );

        // Copy all of the character's items to the import directory
        for ( j = 0; j < MAXIMPORTOBJECTS; j++ )
        {
            snprintf( srcDir, SDL_arraysize( srcDir), "%s" SLASH_STR "%d.obj", srcPlayer, j );
            snprintf( destDir, SDL_arraysize( destDir), "import" SLASH_STR "temp%04d.obj", local_import_slot[tnc] + j + 1 );

            vfs_copyDirectory( srcDir, destDir );
        }
    }

    return btrue;
}

//--------------------------------------------------------------------------------------------
void game_release_module_data()
{
    // ZZ> This function frees up memory used by the module

    // Disable EMP
    local_senseenemiesID = IDSZ_NONE;
    local_senseenemiesTeam = TEAM_MAX;

    release_all_graphics();
    release_all_profiles();
    release_all_ai_scripts();

    mesh_delete( PMesh );
}

//--------------------------------------------------------------------------------------------
void attach_particles()
{
    // ZZ> This function attaches particles to their characters so everything gets
    //    drawn right
    int cnt;

    for ( cnt = 0; cnt < maxparticles; cnt++ )
    {
        prt_t * pprt;

        if ( !ACTIVE_PRT(cnt) ) continue;
        pprt = PrtList.lst + cnt;

        if( ACTIVE_CHR(pprt->attachedto_ref) )
        {
            attach_particle_to_character( cnt, pprt->attachedto_ref, pprt->vrt_off );

            // the previous function can inactivate a perticle
            if( ACTIVE_PRT(cnt) )
            {
                pip_t * ppip = prt_get_ppip(cnt);

                // Correct facing so swords knock characters in the right direction...
                if ( NULL != ppip && ppip->damfx & DAMFX_TURN )
                {
                    pprt->facing = ChrList.lst[pprt->attachedto_ref].turn_z;
                }
            }
        }
    }
}

//--------------------------------------------------------------------------------------------
int add_player( Uint16 character, Uint16 player, Uint32 device_bits )
{
    // ZZ> This function adds a player, returning bfalse if it fails, btrue otherwise

    bool_t retval = bfalse;

    if ( VALID_PLA_RANGE(player) && !PlaList[player].valid )
    {
        player_t * ppla = PlaList + player;

        player_init( ppla );

        ChrList.lst[character].isplayer = btrue;
        ppla->index           = character;
        ppla->valid           = btrue;
        ppla->device.bits     = device_bits;

        if ( device_bits != INPUT_BITS_NONE )
        {
            local_noplayers = bfalse;
            ChrList.lst[character].islocalplayer = btrue;
            local_numlpla++;
        }

        PlaList_count++;

        retval = btrue;
    }

    return retval;
}

//--------------------------------------------------------------------------------------------
void let_all_characters_think()
{
    // ZZ> This function lets every computer controlled character do AI stuff
    int character;
    static Uint32 last_update = (Uint32)(~0);

    // make sure there is only one script update per game update;
    if ( update_wld == last_update ) return;
    last_update = update_wld;

    numblip = 0;
    for ( character = 0; character < MAX_CHR; character++ )
    {
        chr_t * pchr;
        cap_t * pcap;

        bool_t is_crushed, is_cleanedup, can_think;

        if ( !ACTIVE_CHR(character) ) continue;
        pchr = ChrList.lst + character;

        pcap = chr_get_pcap(character);
        if( NULL == pcap ) continue;

        // check for actions that must always be handled
        is_cleanedup = HAS_SOME_BITS( pchr->ai.alert, ALERTIF_CLEANEDUP );
        is_crushed   = HAS_SOME_BITS( pchr->ai.alert, ALERTIF_CRUSHED   );

        // let the script run sometimes even if the item is in your backpack
        can_think = !pchr->pack_ispacked || pcap->isequipment;

        // only let dead/destroyed things think if they have beem crushed/cleanedup
        if ( ( pchr->alive && can_think ) || is_crushed || is_cleanedup )
        {
            // Figure out alerts that weren't already set
            set_alerts( character );

            // Crushed characters shouldn't be alert to anything else
            if ( is_crushed )  { pchr->ai.alert = ALERTIF_CRUSHED; pchr->ai.timer = update_wld + 1; }

            // Cleaned up characters shouldn't be alert to anything else
            if ( is_cleanedup )  { pchr->ai.alert = ALERTIF_CLEANEDUP; pchr->ai.timer = update_wld + 1; }

            let_character_think( character );
        }
    }
}

//--------------------------------------------------------------------------------------------
bool_t game_begin_menu( menu_process_t * mproc, which_menu_t which )
{
    if ( NULL == mproc ) return bfalse;

    if ( !process_running( PROC_PBASE(mproc) ) )
    {
        GProc->menu_depth = mnu_get_menu_depth();
    }

    if ( mnu_begin_menu( which ) )
    {
        process_start( PROC_PBASE(mproc) );
    }

    return btrue;
}

//--------------------------------------------------------------------------------------------
void game_end_menu( menu_process_t * mproc )
{
    mnu_end_menu();

    if ( mnu_get_menu_depth() <= GProc->menu_depth )
    {
        process_resume( PROC_PBASE(MProc) );
        GProc->menu_depth = -1;
    }
}

//--------------------------------------------------------------------------------------------
void game_finish_module()
{
    // export all the local and remote characters
    game_update_imports();

    // quit the old module
    game_quit_module();
}

//--------------------------------------------------------------------------------------------
void free_all_objects( void )
{
    // BB > free every instance of the three object types used in the game.

    PrtList_free_all();
    EncList_free_all();
    free_all_chraracters();
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
bool_t add_chr_chr_interaction( Uint16 ichr_a, Uint16 ichr_b, co_data_t cdata[], int * cdata_count, hash_node_t hnlst[], int * hn_count )
{
    Uint32 hashval = 0;
    int count;
    bool_t found;

    hash_node_t * n;
    co_data_t   * d;

    // there is no situation in the game where we allow characters to interact with themselves
    if( ichr_a == ichr_b ) return bfalse;

    // create a hash that is order-independent
    hashval = MAKE_HASH(ichr_a, ichr_b);

    found = bfalse;
    count = chr_co_list->subcount[hashval];
    if ( count > 0)
    {
        int i;

        // this hash already exists. check to see if the binary collision exists, too
        n = chr_co_list->sublist[hashval];
        for (i = 0; i < count; i++)
        {
            d = (co_data_t *)(n->data);

            // make sure to test both orders
            if ( (d->chra == ichr_a && d->chrb == ichr_b) || (d->chra == ichr_b && d->chrb == ichr_a) )
            {
                found = btrue;
                break;
            }
        }
    }

    // insert this collision
    if (!found)
    {
        // pick a free collision data
        assert((*cdata_count) < CHR_MAX_COLLISIONS);
        d = cdata + (*cdata_count);
        (*cdata_count)++;

        // fill it in
        d->chra = ichr_a;
        d->chrb = ichr_b;
        d->prtb = TOTAL_MAX_PRT;

        // generate a new hash node
        assert((*hn_count) < COLLISION_HASH_NODES);
        n = hnlst + (*hn_count);
        (*hn_count)++;
        hash_node_ctor(n, (void*)d);

        // insert the node
        chr_co_list->subcount[hashval]++;
        chr_co_list->sublist[hashval] = hash_node_insert_before(chr_co_list->sublist[hashval], n);
    }

    return !found;
}

//--------------------------------------------------------------------------------------------
bool_t add_chr_prt_interaction( Uint16 ichr_a, Uint16 iprt_b, co_data_t cdata[], int * cdata_count, hash_node_t hnlst[], int * hn_count )
{
    bool_t found;
    int    count;
    Uint32 hashval = 0;

    hash_node_t * n;
    co_data_t   * d;

    // create a hash that is order-independent
    hashval = MAKE_HASH(ichr_a, iprt_b);

    found = bfalse;
    count = chr_co_list->subcount[hashval];
    if ( count > 0)
    {
        int i ;

        // this hash already exists. check to see if the binary collision exists, too
        n = chr_co_list->sublist[hashval];
        for (i = 0; i < count; i++)
        {
            d = (co_data_t *)(n->data);
            if (d->chra == ichr_a && d->prtb == iprt_b)
            {
                found = btrue;
                break;
            }
        }
    }

    // insert this collision
    if (!found)
    {
        // pick a free collision data
        assert((*cdata_count) < CHR_MAX_COLLISIONS);
        d = cdata + (*cdata_count);
        (*cdata_count)++;

        // fill it in
        d->chra = ichr_a;
        d->chrb = MAX_CHR;
        d->prtb = iprt_b;

        // generate a new hash node
        assert((*hn_count) < COLLISION_HASH_NODES);
        n = hnlst + (*hn_count);
        (*hn_count)++;
        hash_node_ctor(n, (void*)d);

        // insert the node
        chr_co_list->subcount[hashval]++;
        chr_co_list->sublist[hashval] = hash_node_insert_before(chr_co_list->sublist[hashval], n);
    }

    return !found;
}

//--------------------------------------------------------------------------------------------
bool_t detect_chr_chr_interaction( Uint16 ichr_a, Uint16 ichr_b )
{
    bool_t interact_x  = bfalse;
    bool_t interact_y  = bfalse;
    bool_t interact_xy = bfalse;
    bool_t interact_z  = bfalse;

    float xa, ya, za;
    float xb, yb, zb;
    float dxy, dx, dy, depth_z;

    chr_t *pchr_a, *pchr_b;
    cap_t *pcap_a, *pcap_b;

    // Don't interact with self
    if ( ichr_a == ichr_b ) return bfalse;

    // Ignore invalid characters
    if ( !ACTIVE_CHR(ichr_a) ) return bfalse;
    pchr_a = ChrList.lst + ichr_a;

    pcap_a = chr_get_pcap( ichr_a );
    if ( NULL == pcap_a ) return bfalse;

    // Ignore invalid characters
    if ( !ACTIVE_CHR(ichr_b) ) return bfalse;
    pchr_b = ChrList.lst + ichr_b;

    pcap_b = chr_get_pcap( ichr_b );
    if ( NULL == pcap_b ) return bfalse;

    xa = pchr_a->pos.x;
    ya = pchr_a->pos.y;
    za = pchr_a->pos.z;

    xb = pchr_b->pos.x;
    yb = pchr_b->pos.y;
    zb = pchr_b->pos.z;

    // don't interact if there is no interaction
    if ( 0 == pchr_a->bump.size || 0 == pchr_b->bump.size ) return bfalse;

    // don't interact with your mount, or your held items
    if ( ichr_a == pchr_b->attachedto || ichr_b == pchr_a->attachedto ) return bfalse;

    // reject characters that are hidden
    if ( pchr_a->is_hidden || pchr_b->is_hidden ) return bfalse;

    // First check absolute value diamond
    dx = ABS( xa - xb );
    dy = ABS( ya - yb );
    dxy = dx + dy;

    // detect z interactions based on the actual vertical extent of the bounding box
    depth_z = MIN( zb + pchr_b->chr_chr_cv.max_z, za + pchr_a->chr_chr_cv.max_z ) -
              MAX( zb + pchr_b->chr_chr_cv.min_z, za + pchr_a->chr_chr_cv.min_z );

    // detect x-y interactions based on a potentially gigantor bounding box
    interact_x  = (dx  <= pchr_a->bump_1.size    + pchr_b->bump_1.size   );
    interact_y  = (dy  <= pchr_a->bump_1.size    + pchr_b->bump_1.size   );
    interact_xy = (dxy <= pchr_a->bump_1.sizebig + pchr_b->bump_1.sizebig);

    if ( (pchr_a->platform && pcap_b->canuseplatforms) ||
         (pchr_b->platform && pcap_a->canuseplatforms) )
    {
        interact_z  = (depth_z > -PLATTOLERANCE);
    }
    else
    {
        interact_z  = (depth_z > 0);
    }

    return interact_x && interact_y && interact_xy && interact_z;
}

//--------------------------------------------------------------------------------------------
bool_t detect_chr_prt_interaction( Uint16 ichr_a, Uint16 iprt_b )
{
    bool_t interact_x  = bfalse;
    bool_t interact_y  = bfalse;
    bool_t interact_xy = bfalse;
    bool_t interact_z  = bfalse;
    bool_t interact_platform = bfalse;

    float dxy, dx, dy, depth_z;

    chr_t * pchr_a;
    prt_t * pprt_b;

    // Ignore invalid characters
    if ( !ACTIVE_CHR(ichr_a) ) return bfalse;
    pchr_a = ChrList.lst + ichr_a;

    // Ignore invalid characters
    if ( !ACTIVE_PRT(iprt_b) ) return bfalse;
    pprt_b = PrtList.lst + iprt_b;

    // reject characters that are hidden
    if ( pchr_a->is_hidden || pprt_b->is_hidden ) return bfalse;

    // particles don't "collide" with anything they are attached to.
    // that only happes through doing bump particle damamge
    if( ichr_a == pprt_b->attachedto_ref ) return bfalse;

    // don't interact if there is no interaction
    //if ( 0 == pchr_a->bump_1.size || 0 == pprt_b->bumpsize ) return bfalse;

    // First check absolute value diamond
    dx = ABS( pchr_a->pos.x - pprt_b->pos.x );
    dy = ABS( pchr_a->pos.y - pprt_b->pos.y );
    dxy = dx + dy;

    // estimate the horizontal interactions this frame
    interact_x  = (dx  <= (pchr_a->bump_1.size    + pprt_b->bumpsize   ));
    interact_y  = (dy  <= (pchr_a->bump_1.size    + pprt_b->bumpsize   ));
    interact_xy = (dxy <= (pchr_a->bump_1.sizebig + pprt_b->bumpsizebig));

    // estimate the vertical interactions this frame
    depth_z = pprt_b->pos.z - (pchr_a->pos.z + pchr_a->bump_1.height);
    if( depth_z > -PLATTOLERANCE && depth_z < PLATTOLERANCE )
    {
        interact_platform = ( pchr_a->platform && !ACTIVE_CHR(pprt_b->attachedto_ref) );
    }

    depth_z = MIN( pchr_a->pos.z + pchr_a->chr_prt_cv.max_z, pprt_b->pos.z + pprt_b->bumpheight ) - MAX(pchr_a->pos.z + pchr_a->chr_prt_cv.min_z, pprt_b->pos.z - pprt_b->bumpheight);
    interact_z  = (depth_z > 0) || interact_platform;

    if ( !interact_x || !interact_y || !interact_xy || !interact_z ) return bfalse;

    return btrue;
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
ego_mpd_t * set_PMesh( ego_mpd_t * pmpd )
{
    ego_mpd_t * pmpd_old = PMesh;

    PMesh = pmpd;

    return pmpd_old;
}

//--------------------------------------------------------------------------------------------
camera_t * set_PCamera( camera_t * pcam )
{
    camera_t * pcam_old = PCamera;

    PCamera = pcam;

    // Matrix init stuff (from remove.c)
    rotmeshtopside    = ( (float)sdl_scr.x / sdl_scr.y ) * ROTMESHTOPSIDE / ( 1.33333f );
    rotmeshbottomside = ( (float)sdl_scr.x / sdl_scr.y ) * ROTMESHBOTTOMSIDE / ( 1.33333f );
    rotmeshup         = ( (float)sdl_scr.x / sdl_scr.y ) * ROTMESHUP / ( 1.33333f );
    rotmeshdown       = ( (float)sdl_scr.x / sdl_scr.y ) * ROTMESHDOWN / ( 1.33333f );

    return pcam_old;
}

//---------------------------------------------------------------------------------------------
float get_mesh_level( ego_mpd_t * pmesh, float x, float y, bool_t waterwalk )
{
    // ZZ> This function returns the height of a point within a mesh fan, precise
    //    If waterwalk is nonzero and the fan is watery, then the level returned is the
    //    level of the water.

    float zdone;

    zdone = mesh_get_level( pmesh, x, y );

    if ( waterwalk && water.surface_level > zdone && water.is_water )
    {
        int tile = mesh_get_tile( pmesh, x, y );

        if (  0 != mesh_test_fx( pmesh, tile, MPDFX_WATER ) )
        {
            zdone = water.surface_level;
        }
    }

    return zdone;
}

//--------------------------------------------------------------------------------------------
bool_t make_water( water_instance_t * pinst, wawalite_water_t * pdata )
{
    // ZZ> This function sets up water movements
    int layer, frame, point, cnt;
    float temp;
    Uint8 spek;

    if ( NULL == pinst || NULL == pdata ) return bfalse;

    for ( layer = 0; layer < pdata->layer_count; layer++ )
    {
        pinst->layer[layer].tx.x = 0;
        pinst->layer[layer].tx.y = 0;

        for ( frame = 0; frame < MAXWATERFRAME; frame++ )
        {
            // Do first mode
            for ( point = 0; point < WATERPOINTS; point++ )
            {
                temp = SIN( ( frame * TWO_PI / MAXWATERFRAME ) + ( TWO_PI * point / WATERPOINTS ) + ( PI / 2 * layer / MAXWATERLAYER ) );
                pinst->layer_z_add[layer][frame][point] = temp * pdata->layer[layer].amp;
            }
        }
    }

    // Calculate specular highlights
    spek = 0;
    for ( cnt = 0; cnt < 256; cnt++ )
    {
        spek = 0;
        if ( cnt > pdata->spek_start )
        {
            temp = cnt - pdata->spek_start;
            temp = temp / ( 256 - pdata->spek_start );
            temp = temp * temp;
            spek = temp * pdata->spek_level;
        }

        // [claforte] Probably need to replace this with a
        //           GL_DEBUG(glColor4f)(spek/256.0f, spek/256.0f, spek/256.0f, 1.0f) call:
        if ( gfx.shading == GL_FLAT )
            pinst->spek[cnt] = 0;
        else
            pinst->spek[cnt] = spek;
    }

    return btrue;
}

//--------------------------------------------------------------------------------------------
void reset_end_text()
{
    // ZZ> This function resets the end-module text
    endtextwrite = snprintf( endtext, SDL_arraysize( endtext), "The game has ended..." );

    /*
    if ( PlaList_count > 1 )
    {
        endtextwrite = snprintf( endtext, SDL_arraysize( endtext), "Sadly, they were never heard from again..." );
    }
    else
    {
        if ( PlaList_count == 0 )
        {
            // No players???
            endtextwrite = snprintf( endtext, SDL_arraysize( endtext), "The game has ended..." );
        }
        else
        {
            // One player
            endtextwrite = snprintf( endtext, SDL_arraysize( endtext), "Sadly, no trace was ever found..." );
        }
    }
    */

    str_add_linebreaks( endtext, endtextwrite, 20 );
}

//--------------------------------------------------------------------------------------------
void expand_escape_codes( Uint16 ichr, script_state_t * pstate, char * src, char * src_end, char * dst, char * dst_end )
{
    int    cnt;
    STRING szTmp;

    chr_t      * pchr, *ptarget, *powner;
    ai_state_t * pai;

    pchr    = !ACTIVE_CHR(ichr) ? NULL : ChrList.lst + ichr;
    pai     = (NULL == pchr)    ? NULL : &(pchr->ai);

    ptarget = ((NULL == pai) || !ACTIVE_CHR(pai->target)) ? pchr : ChrList.lst + pai->target;
    powner  = ((NULL == pai) || !ACTIVE_CHR(pai->owner )) ? pchr : ChrList.lst + pai->owner;

    cnt = 0;
    while ( CSTR_END != *src && src < src_end && dst < dst_end )
    {
        if ( '%' == *src )
        {
            char * ebuffer, * ebuffer_end;

            // go to the escape character
            src++;

            // set up the buffer to hold the escape data
            ebuffer     = szTmp;
            ebuffer_end = szTmp + SDL_arraysize(szTmp) - 1;

            // make the excape buffer an empty string
            *ebuffer = CSTR_END;

            switch ( *src )
            {
                case '%' : // the % symbol
                    {
                        snprintf( szTmp, SDL_arraysize( szTmp), "%%" );
                    }
                    break;

                case 'n' : // Name
                    {
                        snprintf( szTmp, SDL_arraysize( szTmp), "%s", chr_get_name( ichr, btrue ) );
                    }
                    break;

                case 'c':  // Class name
                    {
                        if ( NULL != pchr )
                        {
                            ebuffer     = chr_get_pcap(ichr)->classname;
                            ebuffer_end = ebuffer + SDL_arraysize(chr_get_pcap(ichr)->classname);
                        }
                    }
                    break;

                case 't':  // Target name
                    {
                        if ( NULL != pai )
                        {
                            snprintf( szTmp, SDL_arraysize( szTmp), "%s", chr_get_name( pai->target, btrue ) );
                        }
                    }
                    break;

                case 'o':  // Owner name
                    {
                        if ( NULL != pai )
                        {
                            snprintf( szTmp, SDL_arraysize( szTmp), "%s", chr_get_name( pai->owner, btrue ) );
                        }
                    }
                    break;

                case 's':  // Target class name
                    {
                        if ( NULL != ptarget)
                        {
                            ebuffer     = chr_get_pcap(pai->target)->classname;
                            ebuffer_end = ebuffer + SDL_arraysize(chr_get_pcap(pai->target)->classname);
                        }
                    }
                    break;

                case '0':
                case '1':
                case '2':
                case '3': // Target's skin name
                    {
                        if ( NULL != ptarget)
                        {
                            ebuffer = chr_get_pcap(pai->target)->skinname[(*src)-'0'];
                            ebuffer_end = ebuffer + SDL_arraysize(chr_get_pcap(pai->target)->skinname[(*src)-'0']);
                        }
                    }
                    break;

                case 'a':  // Character's ammo
                    {
                        if ( NULL != pchr)
                        {
                            if ( pchr->ammoknown )
                            {
                                snprintf( szTmp, SDL_arraysize( szTmp), "%d", pchr->ammo );
                            }
                            else
                            {
                                snprintf( szTmp, SDL_arraysize( szTmp), "?" );
                            }
                        }
                    }
                    break;

                case 'k':  // Kurse state
                    {
                        if ( NULL != pchr )
                        {
                            if ( pchr->iskursed )
                            {
                                snprintf( szTmp, SDL_arraysize( szTmp), "kursed" );
                            }
                            else
                            {
                                snprintf( szTmp, SDL_arraysize( szTmp), "unkursed" );
                            }
                        }
                    }
                    break;

                case 'p':  // Character's possessive
                    {
                        if ( NULL != pchr )
                        {
                            if ( pchr->gender == GENDER_FEMALE )
                            {
                                snprintf( szTmp, SDL_arraysize( szTmp), "her" );
                            }
                            else if ( pchr->gender == GENDER_MALE )
                            {
                                snprintf( szTmp, SDL_arraysize( szTmp), "his" );
                            }
                            else
                            {
                                snprintf( szTmp, SDL_arraysize( szTmp), "its" );
                            }
                        }
                    }
                    break;

                case 'm':  // Character's gender
                    {
                        if ( NULL != pchr )
                        {
                            if ( pchr->gender == GENDER_FEMALE )
                            {
                                snprintf( szTmp, SDL_arraysize( szTmp), "female " );
                            }
                            else if ( pchr->gender == GENDER_MALE )
                            {
                                snprintf( szTmp, SDL_arraysize( szTmp), "male " );
                            }
                            else
                            {
                                snprintf( szTmp, SDL_arraysize( szTmp), " " );
                            }
                        }
                    }
                    break;

                case 'g':  // Target's possessive
                    {
                        if ( NULL != ptarget )
                        {
                            if ( ptarget->gender == GENDER_FEMALE )
                            {
                                snprintf( szTmp, SDL_arraysize( szTmp), "her" );
                            }
                            else if ( ptarget->gender == GENDER_MALE )
                            {
                                snprintf( szTmp, SDL_arraysize( szTmp), "his" );
                            }
                            else
                            {
                                snprintf( szTmp, SDL_arraysize( szTmp), "its" );
                            }
                        }
                    }
                    break;

                case '#':  // New line (enter)
                    {
                        snprintf( szTmp, SDL_arraysize( szTmp), "\n" );
                    }
                    break;

                case 'd':  // tmpdistance value
                    {
                        if ( NULL != pstate )
                        {
                            snprintf( szTmp, SDL_arraysize( szTmp), "%d", pstate->distance );
                        }
                    }
                    break;

                case 'x':  // tmpx value
                    {
                        if ( NULL != pstate )
                        {
                            snprintf( szTmp, SDL_arraysize( szTmp), "%d", pstate->x );
                        }
                    }
                    break;

                case 'y':  // tmpy value
                    {
                        if ( NULL != pstate )
                        {
                            snprintf( szTmp, SDL_arraysize( szTmp), "%d", pstate->y );
                        }
                    }
                    break;

                case 'D':  // tmpdistance value
                    {
                        if ( NULL != pstate )
                        {
                            snprintf( szTmp, SDL_arraysize( szTmp), "%2d", pstate->distance );
                        }
                    }
                    break;

                case 'X':  // tmpx value
                    {
                        if ( NULL != pstate )
                        {
                            snprintf( szTmp, SDL_arraysize( szTmp), "%2d", pstate->x );
                        }
                    }
                    break;

                case 'Y':  // tmpy value
                    {
                        if ( NULL != pstate )
                        {
                            snprintf( szTmp, SDL_arraysize( szTmp), "%2d", pstate->y );
                        }
                    }
                    break;

                default:
                    snprintf( szTmp, SDL_arraysize( szTmp), "%%%c???", (*src) );
                    break;
            }

            if ( CSTR_END == *ebuffer )
            {
                ebuffer     = szTmp;
                ebuffer_end = szTmp + SDL_arraysize(szTmp);
                snprintf( szTmp, SDL_arraysize( szTmp), "%%%c???", (*src) );
            }

            // make the line capitalized if necessary
            if ( 0 == cnt && NULL != ebuffer )  *ebuffer = toupper( *ebuffer );

            // Copy the generated text
            while ( CSTR_END != *ebuffer && ebuffer < ebuffer_end && dst < dst_end )
            {
                *dst++ = *ebuffer++;
            }
            *dst = CSTR_END;
        }
        else
        {
            // Copy the letter
            *dst = *src;
            dst++;
        }

        src++;
        cnt++;
    }

    // make sure the destination string is terminated
    if ( dst < dst_end )
    {
        *dst = CSTR_END;
    }
    *dst_end = CSTR_END;
}

//--------------------------------------------------------------------------------------------
bool_t game_choose_module( int imod, int seed )
{
    if ( seed < 0 ) seed = time(NULL);

    if ( NULL == PMod ) PMod = &gmod;

    return game_module_setup( PMod, mnu_ModList_get_base(imod), mnu_ModList_get_name(imod), seed );
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
process_t * process_init( process_t * proc )
{
    if ( NULL == proc ) return proc;

    memset( proc, 0, sizeof(process_t) );

    proc->terminated = btrue;

    return proc;
}

bool_t process_start( process_t * proc )
{
    if ( NULL == proc ) return bfalse;

    // choose the correct proc->state
    if ( proc->terminated || proc->state > proc_leaving )
    {
        // must re-initialize the process
        proc->state = proc_begin;
    }
    if ( proc->state > proc_entering )
    {
        // the process is already initialized, just put it back in
        // proc_entering mode
        proc->state = proc_entering;
    }

    // tell it to run
    proc->terminated = bfalse;
    proc->valid      = btrue;
    proc->paused     = bfalse;

    return btrue;
}

bool_t process_kill( process_t * proc )
{
    if ( NULL == proc ) return bfalse;
    if ( !process_validate(proc) ) return btrue;

    // turn the process back on with an order to commit suicide
    proc->paused = bfalse;
    proc->killme = btrue;

    return btrue;
}

bool_t process_validate( process_t * proc )
{
    if (NULL == proc) return bfalse;

    if ( !proc->valid || proc->terminated )
    {
        process_terminate( proc );
    }

    return proc->valid;
}

bool_t process_terminate( process_t * proc )
{
    if (NULL == proc) return bfalse;

    proc->valid      = bfalse;
    proc->terminated = btrue;
    proc->state      = proc_begin;

    return btrue;
}

bool_t process_pause( process_t * proc )
{
    bool_t old_value;

    if ( !process_validate(proc) ) return bfalse;

    old_value    = proc->paused;
    proc->paused = btrue;

    return old_value != proc->paused;
}

bool_t process_resume( process_t * proc )
{
    bool_t old_value;

    if ( !process_validate(proc) ) return bfalse;

    old_value    = proc->paused;
    proc->paused = bfalse;

    return old_value != proc->paused;
}

bool_t process_running( process_t * proc )
{
    if ( !process_validate(proc) ) return bfalse;

    return !proc->paused;
}

//--------------------------------------------------------------------------------------------
ego_process_t * ego_process_init( ego_process_t * eproc, int argc, char **argv )
{
    if ( NULL == eproc ) return NULL;

    memset( eproc, 0, sizeof(ego_process_t) );

    process_init( PROC_PBASE(eproc) );

    eproc->argv0 = (argc > 0) ? argv[0] : NULL;

    return eproc;
}

//--------------------------------------------------------------------------------------------
menu_process_t * menu_process_init( menu_process_t * mproc )
{
    if ( NULL == mproc ) return NULL;

    memset( mproc, 0, sizeof(menu_process_t) );

    process_init( PROC_PBASE(mproc) );

    return mproc;
}

//--------------------------------------------------------------------------------------------
game_process_t * game_process_init( game_process_t * gproc )
{
    if ( NULL == gproc ) return NULL;

    memset( gproc, 0, sizeof(game_process_t) );

    process_init( PROC_PBASE(gproc) );

    gproc->menu_depth = -1;
    gproc->pause_key_ready = btrue;

    return gproc;
}

//---------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
/*Uint8 find_target_in_block( int x, int y, float chrx, float chry, Uint16 facing,
Uint8 onlyfriends, Uint8 anyone, Uint8 team,
Uint16 donttarget, Uint16 oldtarget )
{
// ZZ> This function helps find a target, returning btrue if it found a decent target
int cnt;
Uint16 angle;
Uint16 charb;
Uint8 enemies, returncode;
Uint32 fanblock;
int distance;

returncode = bfalse;

// Current fanblock
if ( x >= 0 && x < meshbloksx && y >= 0 && y < meshbloksy )
{
fanblock = mesh_get_block_int(PMesh, x,y);

enemies = bfalse;
if ( !onlyfriends ) enemies = btrue;

charb = bumplist[fanblock].chr;
cnt = 0;
while ( cnt < bumplist[fanblock].chrnum )
{
if ( ChrList.lst[charb].alive && !ChrList.lst[charb].invictus && charb != donttarget && charb != oldtarget )
{
if ( anyone || ( chr_get_iteam(charb) == team && onlyfriends ) || ( TeamList[team].hatesteam[chr_get_iteam(charb)] && enemies ) )
{
distance = ABS( ChrList.lst[charb].pos.x - chrx ) + ABS( ChrList.lst[charb].pos.y - chry );
if ( distance < globestdistance )
{
angle = vec_to_facing( ChrList.lst[charb].pos.x - chrx , ChrList.lst[charb].pos.y - chry );
angle = facing - angle;
if ( angle < globestangle || angle > ( 0x00010000 - globestangle ) )
{
returncode = btrue;
globesttarget = charb;
globestdistance = distance;
glouseangle = angle;
if ( angle  > 32767 )
globestangle = -angle;
else
globestangle = angle;
}
}
}
}
charb = ChrList.lst[charb].bumplist_next;
cnt++;
}
}
return returncode;
}*/

//--------------------------------------------------------------------------------------------
/*Uint16 find_target( float chrx, float chry, Uint16 facing,
Uint16 targetangle, Uint8 onlyfriends, Uint8 anyone,
Uint8 team, Uint16 donttarget, Uint16 oldtarget )
{
// This function finds the best target for the given parameters
Uint8 done;
int x, y;

x = chrx;
y = chry;
x = x >> BLOCK_BITS;
y = y >> BLOCK_BITS;
globestdistance = 9999;
globestangle = targetangle;
done = find_target_in_block( x, y, chrx, chry, facing, onlyfriends, anyone, team, donttarget, oldtarget );
done |= find_target_in_block( x + 1, y, chrx, chry, facing, onlyfriends, anyone, team, donttarget, oldtarget );
done |= find_target_in_block( x - 1, y, chrx, chry, facing, onlyfriends, anyone, team, donttarget, oldtarget );
done |= find_target_in_block( x, y + 1, chrx, chry, facing, onlyfriends, anyone, team, donttarget, oldtarget );
done |= find_target_in_block( x, y - 1, chrx, chry, facing, onlyfriends, anyone, team, donttarget, oldtarget );
if ( done ) return globesttarget;

done = find_target_in_block( x + 1, y + 1, chrx, chry, facing, onlyfriends, anyone, team, donttarget, oldtarget );
done |= find_target_in_block( x + 1, y - 1, chrx, chry, facing, onlyfriends, anyone, team, donttarget, oldtarget );
done |= find_target_in_block( x - 1, y + 1, chrx, chry, facing, onlyfriends, anyone, team, donttarget, oldtarget );
done |= find_target_in_block( x - 1, y - 1, chrx, chry, facing, onlyfriends, anyone, team, donttarget, oldtarget );
if ( done ) return globesttarget;

return MAX_CHR;
}*/

void do_game_hud()
{
    int y = 0;

    if ( flip_pages_requested() && cfg.dev_mode )
    {
        GL_DEBUG(glColor4f)( 1, 1, 1, 1 );
        if ( fpson )
        {
            y = draw_string( 0, y, "%2.3f FPS, %2.3f UPS", stabilized_fps, stabilized_ups );
            y = draw_string( 0, y, "estimated max FPS %2.3f", est_max_fps );
        }

        y = draw_string( 0, y, "Menu time %f", MProc->base.dtime );
    }
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
bool_t collide_ray_with_mesh( line_of_sight_info_t * plos )
{
    Uint32 fan_last;

    int Dx, Dy;
    int ix, ix_stt, ix_end;
    int iy, iy_stt, iy_end;

    int Dbig, Dsmall;
    int ibig, ibig_stt, ibig_end;
    int ismall, ismall_stt, ismall_end;
    int dbig, dsmall;
    int TwoDsmall, TwoDsmallMinusTwoDbig, TwoDsmallMinusDbig;

    bool_t steep;

    if (NULL == plos) return bfalse;

    if ( 0 == plos->stopped_by ) return bfalse;

    ix_stt = plos->x0 / TILE_SIZE;
    ix_end = plos->x1 / TILE_SIZE;

    iy_stt = plos->y0 / TILE_SIZE;
    iy_end = plos->y1 / TILE_SIZE;

    Dx = plos->x1 - plos->x0;
    Dy = plos->y1 - plos->y0;

    steep = (ABS(Dy) >= ABS(Dx));

    // determine which are the big and small values
    if (steep)
    {
        ibig_stt = iy_stt;
        ibig_end = iy_end;

        ismall_stt = ix_stt;
        ismall_end = ix_end;
    }
    else
    {
        ibig_stt = ix_stt;
        ibig_end = ix_end;

        ismall_stt = iy_stt;
        ismall_end = iy_end;
    }

    // set up the big loop variables
    dbig = 1;
    Dbig = ibig_end - ibig_stt;
    if (Dbig < 0)
    {
        dbig = -1;
        Dbig = -Dbig;
        ibig_end--;
    }
    else
    {
        ibig_end++;
    }

    // set up the small loop variables
    dsmall = 1;
    Dsmall = ismall_end - ismall_stt;
    if (Dsmall < 0)
    {
        dsmall = -1;
        Dsmall = -Dsmall;
    }

    // pre-compute some common values
    TwoDsmall             = 2 * Dsmall;
    TwoDsmallMinusTwoDbig = TwoDsmall - 2 * Dbig;
    TwoDsmallMinusDbig    = TwoDsmall - Dbig;

    fan_last = INVALID_TILE;
    for (ibig = ibig_stt, ismall = ismall_stt;  ibig != ibig_end;  ibig += dbig )
    {
        Uint32 fan;

        if (steep)
        {
            ix = ismall;
            iy = ibig;
        }
        else
        {
            ix = ibig;
            iy = ismall;
        }

        // check to see if the "ray" collides with the mesh
        fan = mesh_get_tile_int(PMesh, ix, iy);
        if ( INVALID_TILE != fan && fan != fan_last )
        {
            // collide the ray with the mesh

            if ( HAS_SOME_BITS(PMesh->mmem.tile_list[fan].fx, plos->stopped_by) )
            {
                plos->collide_x  = ix;
                plos->collide_y  = iy;
                plos->collide_fx = PMesh->mmem.tile_list[fan].fx & plos->stopped_by;

                return btrue;
            }

            fan_last = fan;
        }

        // go to the next step
        if (TwoDsmallMinusDbig > 0)
        {
            TwoDsmallMinusDbig += TwoDsmallMinusTwoDbig;
            ismall             += dsmall;
        }
        else
        {
            TwoDsmallMinusDbig += TwoDsmall;
        }
    }

    return bfalse;
}

//--------------------------------------------------------------------------------------------
bool_t collide_ray_with_characters( line_of_sight_info_t * plos )
{
    Uint16 ichr;

    if (NULL == plos) return bfalse;

    for ( ichr = 0; ichr < MAX_CHR; ichr++)
    {
        if ( !ACTIVE_CHR(ichr) ) continue;

        // do line/character intersection
    }

    return bfalse;
}

//--------------------------------------------------------------------------------------------
bool_t do_line_of_sight( line_of_sight_info_t * plos )
{
    bool_t mesh_hit = bfalse, chr_hit = bfalse;
    mesh_hit = collide_ray_with_mesh( plos );

    /*if ( mesh_hit )
    {
        plos->x1 = (plos->collide_x + 0.5f) * TILE_SIZE;
        plos->y1 = (plos->collide_y + 0.5f) * TILE_SIZE;
    }

    chr_hit = collide_ray_with_characters( plos );
    */

    return mesh_hit || chr_hit;
}

//--------------------------------------------------------------------------------------------
void game_reset_players()
{
    // ZZ> This function clears the player list data

    // Reset the local data stuff
    local_seekurse         = bfalse;
    local_senseenemiesTeam = TEAM_MAX;
    local_seeinvisible     = bfalse;
    local_allpladead       = bfalse;

    net_reset_players();
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
bool_t upload_water_layer_data( water_instance_layer_t inst[], wawalite_water_layer_t data[], int layer_count )
{
    int layer;

    if( NULL == inst || 0 == layer_count ) return bfalse;

    // clear all data
    memset(inst, 0, layer_count * sizeof(water_instance_layer_t));

    // set the frame
    for ( layer = 0; layer < layer_count; layer++)
    {
        inst[layer].frame = generate_randmask( 0 , WATERFRAMEAND );
    }

    if ( NULL != data )
    {
        for ( layer = 0; layer < layer_count; layer++)
        {
            inst[layer].z         = data[layer].z;
            inst[layer].amp       = data[layer].amp;

            inst[layer].dist.x    = data[layer].dist[XX];
            inst[layer].dist.y    = data[layer].dist[YY];

            inst[layer].light_dir = data[layer].light_dir / 63.0f;
            inst[layer].light_add = data[layer].light_add / 63.0f;

            inst[layer].tx_add.x    = data[layer].tx_add[XX];
            inst[layer].tx_add.y    = data[layer].tx_add[YY];

            inst[layer].alpha       = data[layer].alpha;
        }
    }

    return btrue;
}

//--------------------------------------------------------------------------------------------
bool_t upload_water_data( water_instance_t * pinst, wawalite_water_t * pdata )
{
    int layer;

    if (NULL == pinst) return bfalse;

    memset(pinst, 0, sizeof(water_instance_t));

    if ( NULL != pdata )
    {
        // upload the data

        pinst->surface_level = pdata->surface_level;
        pinst->douse_level   = pdata->douse_level;

        pinst->is_water         = pdata->is_water;
        pinst->overlay_req      = pdata->overlay_req;
        pinst->background_req   = pdata->background_req;

        pinst->light            = pdata->light;

        pinst->foregroundrepeat = pdata->foregroundrepeat;
        pinst->backgroundrepeat = pdata->backgroundrepeat;

        // upload the layer data
        pinst->layer_count   = pdata->layer_count;
        upload_water_layer_data( pinst->layer, pdata->layer, pdata->layer_count );
    }

    // fix the alpha in case of self-lit textures
    if ( pdata->light )
    {
        for ( layer = 0; layer < pinst->layer_count; layer++ )
        {
            pinst->layer[layer].alpha = 255;  // Some cards don't support alpha lights...
        }
    }

    make_water( pinst, pdata );

    // Allow slow machines to ignore the fancy stuff
    if ( !cfg.twolayerwater_allowed && pinst->layer_count > 1 )
    {
        int iTmp = pdata->layer[0].alpha;
        iTmp = FF_MUL( pdata->layer[1].alpha, iTmp ) + iTmp;
        if ( iTmp > 255 ) iTmp = 255;

        pinst->layer_count = 1;
        pinst->layer[0].alpha = iTmp;
    }

    return btrue;
}

//--------------------------------------------------------------------------------------------
bool_t upload_weather_data( weather_instance_t * pinst, wawalite_weather_t * pdata )
{
    if ( NULL == pinst ) return bfalse;

    memset( pinst, 0, sizeof(weather_instance_t) );

    // set a default value
    pinst->timer_reset = 10;

    if ( NULL != pdata )
    {
        // copy the data
        pinst->timer_reset = pdata->timer_reset;
        pinst->over_water  = pdata->over_water;
    }

    // set the new data
    pinst->time = pinst->timer_reset;

    return btrue;
}

//--------------------------------------------------------------------------------------------
bool_t upload_fog_data( fog_instance_t * pinst, wawalite_fog_t * pdata )
{
    if ( NULL == pinst ) return bfalse;

    memset( pinst, 0, sizeof(fog_instance_t) );

    pdata->top      = 0;
    pdata->bottom   = -100;
    pinst->on       = cfg.fog_allowed;

    if ( NULL != pdata )
    {
        pinst->on     = pdata->found && pinst->on;
        pinst->top    = pdata->top;
        pinst->bottom = pdata->bottom;

        pinst->red    = pdata->red * 0xFF;
        pinst->grn    = pdata->grn * 0xFF;
        pinst->blu    = pdata->blu * 0xFF;
    }

    pinst->distance = ( pdata->top - pdata->bottom );
    pinst->on       = ( pinst->distance < 1.0f ) && pinst->on;

    return btrue;
}

//--------------------------------------------------------------------------------------------
bool_t upload_damagetile_data( damagetile_instance_t * pinst, wawalite_damagetile_t * pdata )
{
    if ( NULL == pinst ) return bfalse;

    memset( pinst, 0, sizeof(damagetile_instance_t) );

    pinst->sound_time   = TILESOUNDTIME;
    pinst->min_distance = 9999;

    if ( NULL != pdata )
    {
        pinst->amount.base  = pdata->amount;
        pinst->amount.rand  = 1;
        pinst->type         = pdata->type;

        pinst->parttype     = pdata->parttype;
        pinst->partand      = pdata->partand;
        pinst->sound        = CLIP(pdata->sound, INVALID_SOUND, MAX_WAVE);
    }

    return btrue;
}

//--------------------------------------------------------------------------------------------
bool_t upload_animtile_data( animtile_instance_t inst[], wawalite_animtile_t * pdata, size_t animtile_count )
{
    Uint32 size;

    if ( NULL == inst || 0 == animtile_count ) return bfalse;

    memset( inst, 0, sizeof(damagetile_instance_t) );

    for( size = 0; size < animtile_count; size++ )
    {
        inst[size].frame_and  = (1 << (size+2)) - 1;
        inst[size].base_and   = ~inst[size].frame_and;
        inst[size].frame_add  = 0;
    }

    if ( NULL != pdata )
    {
        inst[0].update_and = pdata->update_and;
        inst[0].frame_and  = pdata->frame_and;
        inst[0].base_and   = ~inst[0].frame_and;

        for( size = 1; size < animtile_count; size++ )
        {
            inst[size].update_and = pdata->update_and;
            inst[size].frame_and  = ( inst[size-1].frame_and << 1 ) | 1;
            inst[size].base_and   = ~inst[size].frame_and;
        }
    }

    return btrue;
}

//--------------------------------------------------------------------------------------------
bool_t upload_light_data( wawalite_data_t * pdata )
{
    if( NULL == pdata ) return bfalse;

    // upload the lighting data
    light_x = pdata->light_x;
    light_y = pdata->light_y;
    light_z = pdata->light_z;
    light_a = pdata->light_a * 10.0f;

    light_d = 0.0f;
    if ( ABS(light_x) + ABS(light_y) + ABS(light_z) > 0 )
    {
        float fTmp = SQRT( light_x * light_x + light_y * light_y + light_z * light_z );

        // get the extra magnitude of the direct light
        light_d = (1.0f - light_a) * fTmp;
        light_d = CLIP(light_d, 0.0f, 1.0f);

        light_x /= fTmp;
        light_y /= fTmp;
        light_z /= fTmp;
    }

    make_lighttable( pdata->light_x, pdata->light_y, pdata->light_z, pdata->light_a * 10.0f );
    make_lighttospek();

    return btrue;
}

//--------------------------------------------------------------------------------------------
bool_t upload_phys_data( wawalite_physics_t * pdata )
{
    if( NULL == pdata ) return bfalse;

    // upload the physics data
    hillslide      = pdata->hillslide;
    slippyfriction = pdata->slippyfriction;
    airfriction    = pdata->airfriction;
    waterfriction  = pdata->waterfriction;
    noslipfriction = pdata->noslipfriction;
    gravity        = pdata->gravity;

    return btrue;
}

//--------------------------------------------------------------------------------------------
bool_t upload_graphics_data( wawalite_graphics_t * pdata )
{
    if( NULL == pdata ) return bfalse;

    // Read extra data
    gfx.exploremode = pdata->exploremode;
    gfx.usefaredge  = pdata->usefaredge;

    return btrue;
}

//--------------------------------------------------------------------------------------------
bool_t upload_camera_data( wawalite_camera_t * pdata )
{
    if( NULL == pdata ) return bfalse;

    PCamera->swing     = pdata->swing;
    PCamera->swingrate = pdata->swingrate;
    PCamera->swingamp  = pdata->swingamp;

    return btrue;
}

//--------------------------------------------------------------------------------------------
void upload_wawalite()
{
    // ZZ> This function sets up water and lighting for the module

    wawalite_data_t * pdata = &wawalite_data;

    upload_light_data( pdata );
    upload_phys_data( &(pdata->phys) );
    upload_graphics_data( &(pdata->graphics) );
    upload_camera_data( &(pdata->camera) );
    upload_fog_data( &fog, &(pdata->fog) );
    upload_water_data( &water, &(pdata->water) );
    upload_weather_data( &weather, &(pdata->weather) );
    upload_damagetile_data( &damagetile, &(pdata->damagetile) );
    upload_animtile_data( animtile, &(pdata->animtile), SDL_arraysize(animtile) );
}

//---------------------------------------------------------------------------------------------
int ego_init_SDL()
{
    ego_init_SDL_base();
    input_init();

    return _sdl_initialized_base;
}

//---------------------------------------------------------------------------------------------
void ego_init_SDL_base()
{
    if( _sdl_initialized_base ) return;

    log_info ( "Initializing SDL version %d.%d.%d... ", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL );
    if ( SDL_Init(0) < 0 )
    {
        log_message( "Failure!\n" );
        log_error( "Unable to initialize SDL: %s\n", SDL_GetError() );
    }
    else
    {
        log_message( "Success!\n" );
    }

    if ( !_sdl_atexit_registered )
    {
        atexit( SDL_Quit );
        _sdl_atexit_registered = bfalse;
    }

    log_info( "Intializing SDL Timing Services... " );
    if ( SDL_InitSubSystem( SDL_INIT_TIMER ) < 0 )
    {
        log_message( "Failed!\n" );
        log_warning( "SDL error == \"%s\"\n", SDL_GetError() );
    }
    else
    {
        log_message( "Succeess!\n" );
    }

    log_info( "Intializing SDL Event Threading... " );
    if ( SDL_InitSubSystem( SDL_INIT_EVENTTHREAD ) < 0 )
    {
        log_message( "Failed!\n" );
        log_warning( "SDL error == \"%s\"\n", SDL_GetError() );
    }
    else
    {
        log_message( "Succeess!\n" );
    }

    _sdl_initialized_base = btrue;
}

//--------------------------------------------------------------------------------------------
bool_t game_module_setup( game_module_t * pinst, mod_file_t * pdata, const char * loadname, Uint32 seed )
{
    //Prepeares a module to be played
    if ( NULL == pdata ) return bfalse;

    if ( !game_module_init(pinst) ) return bfalse;

    pinst->importamount   = pdata->importamount;
    pinst->exportvalid    = pdata->allowexport;
    pinst->playeramount   = pdata->maxplayers;
    pinst->importvalid    = ( pinst->importamount > 0 );
    pinst->respawnvalid   = ( bfalse != pdata->respawnvalid );
    pinst->respawnanytime = ( RESPAWN_ANYTIME == pdata->respawnvalid );

    strncpy(pinst->loadname, loadname, SDL_arraysize(pinst->loadname));

    pinst->active = bfalse;
    pinst->beat   = bfalse;
    pinst->seed   = seed;

    return btrue;
}

//--------------------------------------------------------------------------------------------
bool_t game_module_init( game_module_t * pinst )
{
    if ( NULL == pinst ) return bfalse;

    memset( pinst, 0, sizeof(game_module_t) );

    pinst->seed = (Uint32)~0;

    return btrue;
}

//--------------------------------------------------------------------------------------------
bool_t game_module_reset( game_module_t * pinst, Uint32 seed )
{
    if (NULL == pinst) return bfalse;

    pinst->beat        = bfalse;
    //pinst->exportvalid = bfalse;  //Zefz> we can't disable export here, some modules are supposed to allow export (towns)
    pinst->seed        = seed;

    return btrue;
}

//--------------------------------------------------------------------------------------------
bool_t game_module_start( game_module_t * pinst )
{
    // BB> Let the module go

    if (NULL == pinst) return bfalse;

    pinst->active = btrue;

    srand( pinst->seed );
    pinst->randsave = rand();
    randindex = rand() % RANDIE_COUNT;

    PNet->hostactive = btrue; // very important or the input will not work

    return btrue;
}

//--------------------------------------------------------------------------------------------
bool_t game_module_stop( game_module_t * pinst )
{
    // BB> stop the module

    if (NULL == pinst) return bfalse;

    pinst->active      = bfalse;

    // ntework stuff
    PNet->hostactive  = bfalse;

    return btrue;
}


////--------------------------------------------------------------------------------------------
//bool_t do_chr_chr_collision( Uint16 ichr_a, Uint16 ichr_b )
//{
//    float xa, ya, za, xb, yb, zb;
//    float was_xa, was_ya, was_za, was_xb, was_yb, was_zb;
//    chr_t * pchr_a, * pchr_b;
//    cap_t * pcap_a, * pcap_b;
//
//    float dx, dy, dist;
//    float was_dx, was_dy, was_dist;
//    float depth_z, was_depth_z;
//    float lerp_z, radius, radius_xy;
//    float wta, wtb;
//
//    bool_t collide_x  = bfalse, was_collide_x;
//    bool_t collide_y  = bfalse, was_collide_y;
//    bool_t collide_xy = bfalse, was_collide_xy;
//    bool_t collide_z  = bfalse, was_collide_z;
//    bool_t collision  = bfalse;
//
//    float interaction_strength = 1.0f;
//
//    // make sure that it is on
//    if ( !ACTIVE_CHR( ichr_a ) ) return bfalse;
//    pchr_a = ChrList.lst + ichr_a;
//
//    pcap_a = chr_get_pcap( ichr_a );
//    if ( NULL == pcap_a ) return bfalse;
//
//    // make sure that it is on
//    if ( !ACTIVE_CHR( ichr_b ) ) return bfalse;
//    pchr_b = ChrList.lst + ichr_b;
//
//    pcap_b = chr_get_pcap( ichr_b );
//    if ( NULL == pcap_b ) return bfalse;
//
//    // platform interaction. if the onwhichplatform is set, then
//    // all collision tests have been met
//    if ( ichr_a == pchr_b->onwhichplatform )
//    {
//        if( do_chr_platform_physics( pchr_b, pchr_a ) )
//        {
//            // this is handled
//            return btrue;
//        }
//    }
//
//    // platform interaction. if the onwhichplatform is set, then
//    // all collision tests have been met
//    if ( ichr_b == pchr_a->onwhichplatform )
//    {
//        if( do_chr_platform_physics( pchr_a, pchr_b ) )
//        {
//            // this is handled
//            return btrue;
//        }
//    }
//
//    // items can interact with platforms but not with other characters/objects
//    if ( pchr_a->isitem || pchr_b->isitem ) return bfalse;
//
//    // don't interact with your mount, or your held items
//    if ( ichr_a == pchr_b->attachedto || ichr_b == pchr_a->attachedto ) return bfalse;
//
//    // don't do anything if there is no interaction strength
//    if ( 0 == pchr_a->bump.size || 0 == pchr_b->bump.size ) return bfalse;
//
//    interaction_strength = 1.0f;
//    interaction_strength *= pchr_a->inst.alpha * INV_FF;
//    interaction_strength *= pchr_b->inst.alpha * INV_FF;
//
//    xa = pchr_a->pos.x;
//    ya = pchr_a->pos.y;
//    za = pchr_a->pos.z;
//
//    was_xa = xa - pchr_a->vel.x;
//    was_ya = ya - pchr_a->vel.y;
//    was_za = za - pchr_a->vel.z;
//
//    xb = pchr_b->pos.x;
//    yb = pchr_b->pos.y;
//    zb = pchr_b->pos.z;
//
//    was_xb = xb - pchr_b->vel.x;
//    was_yb = yb - pchr_b->vel.y;
//    was_zb = zb - pchr_b->vel.z;
//
//    dx = ABS( xa - xb );
//    dy = ABS( ya - yb );
//    dist = dx + dy;
//
//    was_dx = ABS( was_xa - was_xb );
//    was_dy = ABS( was_ya - was_yb );
//    was_dist = was_dx + was_dy;
//
//    depth_z = MIN( zb + pchr_b->bump.height, za + pchr_a->bump.height ) - MAX(za, zb);
//    was_depth_z = MIN( was_zb + pchr_b->bump.height, was_za + pchr_a->bump.height ) - MAX(was_za, was_zb);
//
//    // estimate the radius of interaction based on the z overlap
//    lerp_z  = depth_z / PLATTOLERANCE;
//    lerp_z  = CLIP( lerp_z, 0, 1 );
//
//    radius    = pchr_a->bump.size    + pchr_b->bump.size;
//    radius_xy = pchr_a->bump.sizebig + pchr_b->bump.sizebig;
//
//    // estimate the collisions this frame
//    collide_x  = (dx < radius);
//    collide_y  = (dy < radius);
//    collide_xy = (dist < radius_xy);
//    collide_z  = (depth_z > 0);
//
//    // estimate the collisions last frame
//    was_collide_x  = (was_dx < radius);
//    was_collide_y  = (was_dy < radius);
//    was_collide_xy = (was_dist < radius_xy);
//    was_collide_z  = (was_depth_z > 0);
//
//    //------------------
//    // do character-character interactions
//    if ( !collide_x || !collide_y || !collide_xy || depth_z < -PLATTOLERANCE ) return bfalse;
//
//    wta = (0xFFFFFFFF == pchr_a->phys.weight) ? -(float)0xFFFFFFFF : pchr_a->phys.weight;
//    wtb = (0xFFFFFFFF == pchr_b->phys.weight) ? -(float)0xFFFFFFFF : pchr_b->phys.weight;
//
//    if ( wta == 0 && wtb == 0 )
//    {
//        wta = wtb = 1;
//    }
//    else if ( wta == 0 )
//    {
//        wta = 1;
//        wtb = -0xFFFF;
//    }
//    else if ( wtb == 0 )
//    {
//        wtb = 1;
//        wta = -0xFFFF;
//    }
//
//    if ( 0.0f == pchr_a->phys.bumpdampen && 0.0f == pchr_b->phys.bumpdampen )
//    {
//        /* do nothing */
//    }
//    else if ( 0.0f == pchr_a->phys.bumpdampen )
//    {
//        // make the weight infinite
//        wta = -0xFFFF;
//    }
//    else if ( 0.0f == pchr_b->phys.bumpdampen )
//    {
//        // make the weight infinite
//        wtb = -0xFFFF;
//    }
//    else
//    {
//        // adjust the weights to respect bumpdampen
//        wta /= pchr_a->phys.bumpdampen;
//        wtb /= pchr_b->phys.bumpdampen;
//    }
//
//    if ( !collision && collide_z )
//    {
//        float depth_x, depth_y, depth_xy, depth_yx, depth_z;
//        GLvector3 nrm;
//        int exponent = 1;
//
//        if ( pcap_a->canuseplatforms && pchr_b->platform ) exponent += 2;
//        if ( pcap_b->canuseplatforms && pchr_a->platform ) exponent += 2;
//
//        nrm.x = nrm.y = nrm.z = 0.0f;
//
//        depth_x  = MIN(xa + pchr_a->bump.size, xb + pchr_b->bump.size) - MAX(xa - pchr_a->bump.size, xb - pchr_b->bump.size);
//        if ( depth_x <= 0.0f )
//        {
//            depth_x = 0.0f;
//        }
//        else
//        {
//            float sgn = xb - xa;
//            sgn = sgn > 0 ? -1 : 1;
//
//            nrm.x += sgn / POW(depth_x / PLATTOLERANCE, exponent);
//        }
//
//        depth_y  = MIN(ya + pchr_a->bump.size, yb + pchr_b->bump.size) - MAX(ya - pchr_a->bump.size, yb - pchr_b->bump.size);
//        if ( depth_y <= 0.0f )
//        {
//            depth_y = 0.0f;
//        }
//        else
//        {
//            float sgn = yb - ya;
//            sgn = sgn > 0 ? -1 : 1;
//
//            nrm.y += sgn / POW(depth_y / PLATTOLERANCE, exponent);
//        }
//
//        depth_xy = MIN(xa + ya + pchr_a->bump.sizebig, xb + yb + pchr_b->bump.sizebig) - MAX(xa + ya - pchr_a->bump.sizebig, xb + yb - pchr_b->bump.sizebig);
//        if ( depth_xy <= 0.0f )
//        {
//            depth_xy = 0.0f;
//        }
//        else
//        {
//            float sgn = (xb + yb) - (xa + ya);
//            sgn = sgn > 0 ? -1 : 1;
//
//            nrm.x += sgn / POW(depth_xy / PLATTOLERANCE, exponent);
//            nrm.y += sgn / POW(depth_xy / PLATTOLERANCE, exponent);
//        }
//
//        depth_yx = MIN(-xa + ya + pchr_a->bump.sizebig, -xb + yb + pchr_b->bump.sizebig) - MAX(-xa + ya - pchr_a->bump.sizebig, -xb + yb - pchr_b->bump.sizebig);
//        if ( depth_yx <= 0.0f )
//        {
//            depth_yx = 0.0f;
//        }
//        else
//        {
//            float sgn = (-xb + yb) - (-xa + ya);
//            sgn = sgn > 0 ? -1 : 1;
//            nrm.x -= sgn / POW(depth_yx / PLATTOLERANCE, exponent);
//            nrm.y += sgn / POW(depth_yx / PLATTOLERANCE, exponent);
//        }
//
//        depth_z  = MIN(za + pchr_a->bump.height, zb + pchr_b->bump.height) - MAX( za, zb );
//        if ( depth_z <= 0.0f )
//        {
//            depth_z = 0.0f;
//        }
//        else
//        {
//            float sgn = (zb + pchr_b->bump.height / 2) - (za + pchr_a->bump.height / 2);
//            sgn = sgn > 0 ? -1 : 1;
//
//            nrm.z += sgn / POW(exponent * depth_z / PLATTOLERANCE, exponent);
//        }
//
//        if ( ABS(nrm.x) + ABS(nrm.y) + ABS(nrm.z) > 0.0f )
//        {
//            GLvector3 vel_a, vel_b;
//            GLvector3 vpara_a, vperp_a;
//            GLvector3 vpara_b, vperp_b;
//            GLvector3 imp_a, imp_b;
//            float     vdot;
//
//            nrm = VNormalize( nrm );
//
//            vel_a.x = pchr_a->vel.x;
//            vel_a.y = pchr_a->vel.y;
//            vel_a.z = pchr_a->vel.z;
//
//            vel_b.x = pchr_b->vel.x;
//            vel_b.y = pchr_b->vel.y;
//            vel_b.z = pchr_b->vel.z;
//
//            vdot = VDotProduct( nrm, vel_a );
//            vperp_a.x = nrm.x * vdot;
//            vperp_a.y = nrm.y * vdot;
//            vperp_a.z = nrm.z * vdot;
//            vpara_a = VSub( vel_a, vperp_a );
//
//            vdot = VDotProduct( nrm, vel_b );
//            vperp_b.x = nrm.x * vdot;
//            vperp_b.y = nrm.y * vdot;
//            vperp_b.z = nrm.z * vdot;
//            vpara_b = VSub( vel_b, vperp_b );
//
//            // clear the "impulses"
//            imp_a.x = imp_a.y = imp_a.z = 0.0f;
//            imp_b.x = imp_b.y = imp_b.z = 0.0f;
//
//            if ( collide_xy != was_collide_xy || collide_x != was_collide_x || collide_y != was_collide_y )
//            {
//                // an actual collision
//
//                // generic coefficient of restitution
//                float cr = 0.5f;
//
//                if ( (wta < 0 && wtb < 0) || (wta == wtb) )
//                {
//                    float factor = 0.5f * (1.0f - cr);
//
//                    imp_a.x = factor * (vperp_b.x - vperp_a.x);
//                    imp_a.y = factor * (vperp_b.y - vperp_a.y);
//                    imp_a.z = factor * (vperp_b.z - vperp_a.z);
//
//                    imp_b.x = factor * (vperp_a.x - vperp_b.x);
//                    imp_b.y = factor * (vperp_a.y - vperp_b.y);
//                    imp_b.z = factor * (vperp_a.z - vperp_b.z);
//                }
//                else if ( (wta < 0) || (wtb == 0) )
//                {
//                    float factor = (1.0f - cr);
//
//                    imp_b.x = factor * (vperp_a.x - vperp_b.x);
//                    imp_b.y = factor * (vperp_a.y - vperp_b.y);
//                    imp_b.z = factor * (vperp_a.z - vperp_b.z);
//                }
//                else if ( (wtb < 0) || (wta == 0) )
//                {
//                    float factor = (1.0f - cr);
//
//                    imp_a.x = factor * (vperp_b.x - vperp_a.x);
//                    imp_a.y = factor * (vperp_b.y - vperp_a.y);
//                    imp_a.z = factor * (vperp_b.z - vperp_a.z);
//                }
//                else
//                {
//                    float factor;
//
//                    factor = (1.0f - cr) * wtb / ( wta + wtb );
//                    imp_a.x = factor * (vperp_b.x - vperp_a.x);
//                    imp_a.y = factor * (vperp_b.y - vperp_a.y);
//                    imp_a.z = factor * (vperp_b.z - vperp_a.z);
//
//                    factor = (1.0f - cr) * wta / ( wta + wtb );
//                    imp_b.x = factor * (vperp_a.x - vperp_b.x);
//                    imp_b.y = factor * (vperp_a.y - vperp_b.y);
//                    imp_b.z = factor * (vperp_a.z - vperp_b.z);
//                }
//
//                // add in the collision impulses
//                pchr_a->phys.avel.x += imp_a.x;
//                pchr_a->phys.avel.y += imp_a.y;
//                pchr_a->phys.avel.z += imp_a.z;
//                LOG_NAN(pchr_a->phys.avel.z);
//
//                pchr_b->phys.avel.x += imp_b.x;
//                pchr_b->phys.avel.y += imp_b.y;
//                pchr_b->phys.avel.z += imp_b.z;
//                LOG_NAN(pchr_b->phys.avel.z);
//
//                collision = btrue;
//            }
//            else
//            {
//                float tmin;
//
//                tmin = 1e6;
//                if ( nrm.x != 0 )
//                {
//                    tmin = MIN(tmin, depth_x / ABS(nrm.x) );
//                }
//                if ( nrm.y != 0 )
//                {
//                    tmin = MIN(tmin, depth_y / ABS(nrm.y) );
//                }
//                if ( nrm.z != 0 )
//                {
//                    tmin = MIN(tmin, depth_z / ABS(nrm.z) );
//                }
//
//                if ( nrm.x + nrm.y != 0 )
//                {
//                    tmin = MIN(tmin, depth_xy / ABS(nrm.x + nrm.y) );
//                }
//
//                if ( -nrm.x + nrm.y != 0 )
//                {
//                    tmin = MIN(tmin, depth_yx / ABS(-nrm.x + nrm.y) );
//                }
//
//                if ( tmin < 1e6 )
//                {
//                    if ( wta >= 0.0f )
//                    {
//                        float ratio = (float)ABS(wtb) / ((float)ABS(wta) + (float)ABS(wtb));
//
//                        imp_a.x = tmin * nrm.x * 0.25f * ratio;
//                        imp_a.y = tmin * nrm.y * 0.25f * ratio;
//                        imp_a.z = tmin * nrm.z * 0.25f * ratio;
//                    }
//
//                    if ( wtb >= 0.0f )
//                    {
//                        float ratio = (float)ABS(wta) / ((float)ABS(wta) + (float)ABS(wtb));
//
//                        imp_b.x = -tmin * nrm.x * 0.25f * ratio;
//                        imp_b.y = -tmin * nrm.y * 0.25f * ratio;
//                        imp_b.z = -tmin * nrm.z * 0.25f * ratio;
//                    }
//                }
//
//                // add in the collision impulses
//                pchr_a->phys.apos_1.x += imp_a.x;
//                pchr_a->phys.apos_1.y += imp_a.y;
//                pchr_a->phys.apos_1.z += imp_a.z;
//
//                pchr_b->phys.apos_1.x += imp_b.x;
//                pchr_b->phys.apos_1.y += imp_b.y;
//                pchr_b->phys.apos_1.z += imp_b.z;
//
//                // you could "bump" something if you changed your velocity, even if you were still touching
//                collision = (VDotProduct( pchr_a->vel, nrm ) * VDotProduct( pchr_a->vel_old, nrm ) < 0 ) ||
//                            (VDotProduct( pchr_b->vel, nrm ) * VDotProduct( pchr_b->vel_old, nrm ) < 0 );
//
//            }
//
//            // add in the friction due to the "collision"
//            // assume coeff of friction of 0.5
//            if ( ABS(imp_a.x) + ABS(imp_a.y) + ABS(imp_a.z) > 0.0f &&
//                 ABS(vpara_a.x) + ABS(vpara_a.y) + ABS(vpara_a.z) > 0.0f &&
//                 pchr_a->dismount_timer <= 0)
//            {
//                float imp, vel, factor;
//
//                imp = 0.5f * SQRT( imp_a.x * imp_a.x + imp_a.y * imp_a.y + imp_a.z * imp_a.z );
//                vel = SQRT( vpara_a.x * vpara_a.x + vpara_a.y * vpara_a.y + vpara_a.z * vpara_a.z );
//
//                factor = imp / vel;
//                factor = CLIP(factor, 0.0f, 1.0f);
//
//                pchr_a->phys.avel.x -= factor * vpara_a.x;
//                pchr_a->phys.avel.y -= factor * vpara_a.y;
//                pchr_a->phys.avel.z -= factor * vpara_a.z;
//                LOG_NAN(pchr_a->phys.avel.z);
//            }
//
//            if ( ABS(imp_b.x) + ABS(imp_b.y) + ABS(imp_b.z) > 0.0f &&
//                 ABS(vpara_b.x) + ABS(vpara_b.y) + ABS(vpara_b.z) > 0.0f &&
//                 pchr_b->dismount_timer <= 0)
//            {
//                float imp, vel, factor;
//
//                imp = 0.5f * SQRT( imp_b.x * imp_b.x + imp_b.y * imp_b.y + imp_b.z * imp_b.z );
//                vel = SQRT( vpara_b.x * vpara_b.x + vpara_b.y * vpara_b.y + vpara_b.z * vpara_b.z );
//
//                factor = imp / vel;
//                factor = CLIP(factor, 0.0f, 1.0f);
//
//                pchr_b->phys.avel.x -= factor * vpara_b.x;
//                pchr_b->phys.avel.y -= factor * vpara_b.y;
//                pchr_b->phys.avel.z -= factor * vpara_b.z;
//                LOG_NAN(pchr_b->phys.avel.z);
//            }
//        }
//    }
//
//    if ( collision )
//    {
//        ai_state_set_bumplast( &(pchr_a->ai), ichr_b );
//        ai_state_set_bumplast( &(pchr_b->ai), ichr_a );
//    }
//
//    return btrue;
//}

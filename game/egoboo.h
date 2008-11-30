#pragma once

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

///
/// @file
/// @brief Original monolithic header.
/// @details "Disgusting, hairy, and way too monolithic" 
///   This file is in severe need of cleaning up.  Venture here with extreme
///   caution, and bring one of those canaries with you to make sure you
///   don't run out of oxygen.

#include "proto.h"
#include "Menu.h"
#include "configfile.h"
#include "Md2.h"

#include "ogl_texture.h"        // OpenGL texture loader

#include "egoboo_config.h"          // system dependent configuration information
#include "egoboo_math.h"            // vector and matrix math
#include "egoboo_types.h"           // Typedefs for egoboo

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

struct sGame;
struct sGraphics_Data;


// The following magic allows this include to work in multiple files
#ifdef DECLARE_GLOBALS
#    define EXTERN
#    define EQ(x) =x;
#else
#    define EXTERN extern
#    define EQ(x)
#endif

EXTERN EGO_CONST char VERSION[] EQ( "2.8.x" );   ///< Version of the game

#define DEFAULT_SCREEN_W 640
#define DEFAULT_SCREEN_H 480

#define NOSPARKLE           255

#define DELAY_RESIZE            50                       ///< Time it takes to resize a character


#define NOHIDE              127                      ///< Don't hide

typedef enum e_damage_effects_bits
{
  DAMFX_NONE           = 0,                       ///< Damage effects
  DAMFX_ARMO           = 1 << 0,                  ///< Armor piercing
  DAMFX_BLOC           = 1 << 1,                  ///< Cannot be blocked by shield
  DAMFX_ARRO           = 1 << 2,                  ///< Only hurts the one it's attached to
  DAMFX_TURN           = 1 << 3,                  ///< Turn to attached direction
  DAMFX_TIME           = 1 << 4                   //
} DAMFX_BITS;

//Particle Texture Types
enum e_part_type
{
  PART_NORMAL,
  PART_SMOOTH,
  PART_FAST
};
typedef enum e_part_type PART_TYPE;

typedef enum e_SIGNAL
{
  SIGNAL_BUY     = 0,
  SIGNAL_SELL,
  SIGNAL_REJECT,
  SIGNAL_STEAL,
  SIGNAL_ENTERPASSAGE,
  SIGNAL_EXITPASSAGE
} SIGNAL;


typedef enum e_move
{
  MOVE_MELEE = 300,
  MOVE_RANGED = -600,
  MOVE_DISTANCE = -175,
  MOVE_RETREAT = 900,
  MOVE_CHARGE = 1500,
  MOVE_FOLLOW = 0
} MOVE;

typedef enum e_search_bits
{
  SEARCH_DEAD      = 1 << 0,
  SEARCH_ENEMIES   = 1 << 1,
  SEARCH_FRIENDS   = 1 << 2,
  SEARCH_ITEMS     = 1 << 3,
  SEARCH_INVERT    = 1 << 4
} SEARCH_BITS;

/// The spellbook model 
/// @todo change this badly thing
#define SPELLBOOK           127                     

#define NOQUEST             -2						 ///< Quest not found
#define QUESTBEATEN         -1                       ///< Quest is beaten

#define MAXSTAT             16                       ///< Maximum status displays



#define DELAY_DAMAGETILE      32                       ///< Invincibility time
#define DELAY_DAMAGE          16                       ///< Invincibility time
#define DELAY_DEFEND          16                       ///< Invincibility time


#define RAISE 5  ///< 25                               // Helps correct z level
#define SHADOWRAISE 5                               //
#define DAMAGERAISE 25                              //




#define BAR_COUNT                          6            ///< Number of status bars
#define TABX                            32           ///< Size of little name tag on the bar
#define BARX                            112          ///< Size of bar
#define BARY                            16          //
#define NUMTICK                         10           ///< Number of ticks per row
#define TICKX                           8            ///< X size of each tick
#define MAXTICK                         (NUMTICK*5) // Max number of ticks to draw

#define MAXTEXTURE                      512          ///< Max number of textures
#define MAXICONTX                       MAXTEXTURE+1


/* SDL_GetTicks() always returns milli seconds */
#define TICKS_PER_SEC                   1000.0f


#define TARGETFPS                       30.0f
#define FRAMESKIP                       (TICKS_PER_SEC/TARGETFPS)    // 1000 tics per sec / 50 fps = 20 ticks per frame

#define EMULATEUPS                      50.0f
#define TARGETUPS                       30.0f
#define UPDATESCALE                     (EMULATEUPS/(stabilized_ups_sum/stabilized_ups_weight))
#define UPDATESKIP                      (TICKS_PER_SEC/TARGETUPS)    // 1000 tics per sec / 50 fps = 20 ticks per frame
#define ONESECOND                       (TICKS_PER_SEC/UPDATESKIP)    // 1000 tics per sec / 20 ticks per frame = 50 fps

#define STOPBOUNCING                    0.5f          ///< To make objects stop bouncing
#define STOPBOUNCINGPART                1.0f          ///< To make particles stop bouncing


struct s_tile_animated
{
  int    updateand;         ///< New tile every 7 frames
  Uint16 frameand;          ///< Only 4 frames
  Uint16 baseand;           //
  Uint16 bigframeand;       ///< For big tiles
  Uint16 bigbaseand;        //
  float  framefloat;        ///< Current frame
  Uint16 frameadd;          ///< Current frame
};
typedef struct s_tile_animated TILE_ANIMATED;

bool_t tile_animated_reset(TILE_ANIMATED * t);

#define NORTH 16384                                  ///< Character facings
#define SOUTH 49152                                 //
#define EAST 32768                                  //
#define WEST 0                                      //

#define FRONT 0                                      ///< Attack directions
#define BEHIND 32768                                //
#define LEFT 49152                                  //
#define RIGHT 16384                                 //


//Minimap stuff
#define MAXBLIP     32      ///< Max number of blips displayed on the map
typedef struct s_blip
{
  Uint16  x;
  Uint16  y;
  COLR    c;

  // !!! wrong, but it will work !!!
  IRect_t           rect;           ///< The blip rectangles
} BLIP;


EXTERN Sint32          ups_clock             EQ( 0 );             ///< The number of ticks this second
EXTERN Uint32          ups_loops             EQ( 0 );             ///< The number of frames drawn this second
EXTERN float           stabilized_ups        EQ( TARGETUPS );
EXTERN float           stabilized_ups_sum    EQ( TARGETUPS );
EXTERN float           stabilized_ups_weight EQ( 1 );

#define DELAY_TURN 16

//Networking
EXTERN int                     localplayer_count;                 ///< Number of imports from this machine
EXTERN Uint8                   localplayer_control[16];           ///< For local imports
EXTERN OBJ_REF                 localplayer_slot[16];              ///< For local imports

// EWWWW. GLOBALS ARE EVIL.


/*Special Textures*/
typedef enum e_tx_type
{
  TX_PARTICLE = 0,
  TX_TILE_0,
  TX_TILE_1,
  TX_TILE_2,
  TX_TILE_3,
  TX_WATER_TOP,
  TX_WATER_LOW,
  TX_PHONG,
  TX_LAST
} TX_TYPE;

/*Special Textures*/
typedef enum e_ico_type
{
  ICO_NULL,
  ICO_KEYB,
  ICO_MOUS,
  ICO_JOYA,
  ICO_JOYB,
  ICO_BOOK_0,   ///< The first book icon
  ICO_COUNT
} ICO_TYPE;


//Texture filtering
typedef enum e_tx_filters
{
  TX_UNFILTERED,
  TX_LINEAR,
  TX_MIPMAP,
  TX_BILINEAR,
  TX_TRILINEAR_1,
  TX_TRILINEAR_2,
  TX_ANISOTROPIC
} TX_FILTERS;

//Anisotropic filtering - yay! :P
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

#define MAXWATERLAYER 2                              ///< Maximum water layers
#define MAXWATERFRAME 512                            ///< Maximum number of wave frames
#define WATERFRAMEAND (MAXWATERFRAME-1)             //
#define WATERPOINTS 4                                ///< Points in a water fan
#define WATERMODE 4                                  ///< Ummm...  For making it work, yeah...

struct s_water_layer
{
  Uint16    lightlevel_fp8; ///< General light amount (0-63)
  Uint16    lightadd_fp8;   ///< Ambient light amount (0-63)
  Uint16    alpha_fp8;      ///< Transparency

  float     u;              ///< Coordinates of texture
  float     v;              //
  float     uadd;           ///< Texture movement
  float     vadd;           //

  float     amp;            ///< Amplitude of waves
  float     z;              ///< Base height of water
  float     zadd[MAXWATERFRAME][WATERMODE][WATERPOINTS];
  Uint8     color[MAXWATERFRAME][WATERMODE][WATERPOINTS];
  Uint16    frame;          ///< Frame
  Uint16    frameadd;       ///< Speed

  float     distx;          ///< For distant backgrounds
  float     disty;          //
};
typedef struct s_water_layer WATER_LAYER;

struct s_water_info
{
  float     surfacelevel;         ///< Surface level for water striders
  float     douselevel;           ///< Surface level for torches
  bool_t    light;                ///< Is it light ( default is alpha )
  Uint8     spekstart;            ///< Specular begins at which light value
  Uint8     speklevel_fp8;        ///< General specular amount (0-255)
  bool_t    iswater;              ///< Is it water?  ( Or lava... )

  int         layer_count; ///< EQ( 0 );              ///< Number of layers
  WATER_LAYER layer[MAXWATERLAYER];

  Uint32    spek[256];             ///< Specular highlights
};
typedef struct s_water_info WATER_INFO;

bool_t make_water(WATER_INFO * wi);

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
// Global lighting stuff
struct s_lighting_info
{
  bool_t on;
  float  spek;
  vect3  spekdir;
  vect3  spekdir_stt;
  vect3  spekcol;
  float  ambi;
  vect3  ambicol;
};
typedef struct s_lighting_info LIGHTING_INFO;

bool_t lighting_info_reset(LIGHTING_INFO * li);
bool_t setup_lighting( LIGHTING_INFO * li);

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
//Fog stuff
struct s_fog_info
{
  bool_t          on;               ///< Do ground fog?
  float           bottom;           //
  float           top;              //
  float           distance;         //
  Uint8           red;              ///<  Fog collour
  Uint8           grn;              //
  Uint8           blu;              //
  bool_t          affectswater;
};
typedef struct s_fog_info FOG_INFO;

bool_t fog_info_reset(FOG_INFO * f);

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
//Dynamic Lightning effects

#define MAXDYNA                           64         ///< Number of dynamic lights
#define MAXDYNADIST                     2700         ///< Leeway for offscreen lights

#define DYNALIGHT_MEMBERS                \
  float  level;      /* Light level    */ \
  float  falloff;    /* Light falloff  */

struct s_dynalight_info
{
  DYNALIGHT_MEMBERS
  bool_t permanent;
  vect3  pos;        ///< Light position
  int    distance;      ///< The distances
};
typedef struct s_dynalight_info DYNALIGHT_INFO;

#ifdef __cplusplus
  typedef TList<DYNALIGHT_INFO, MAXDYNA> DLightList_t;
  typedef TPList<DYNALIGHT_INFO, MAXDYNA> PDLight_t;
#else
  typedef DYNALIGHT_INFO   DLightList_t[MAXDYNA];
  typedef DYNALIGHT_INFO * PDLight_t;
#endif

size_t DLightList_clear( struct sGraphics_Data * gfx );
size_t DLightList_prune( struct sGraphics_Data * gfx );
size_t DLightList_add( struct sGraphics_Data * gfx, DYNALIGHT_INFO * di );

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
// per-module graphics data that is shared between Game_t and the graphics module
struct sGraphics_Data
{
  egoboo_key_t ekey;

  bool_t render_overlay;
  bool_t render_background;
  bool_t render_fog;
  bool_t usefaredge;                 ///< Far edge maps? (Outdoor)
  bool_t exploremode;                ///< Explore mode? (fog of war)

  float  foregroundrepeat;
  float  backgroundrepeat;

  // Map stuff
  GLtexture  Map_tex;
  IRect_t    Map_rect;
  float      Map_scale;
  bool_t     Map_on;
  bool_t     Map_valid;
  bool_t     Map_youarehereon;

  // Bars
  GLtexture  TxBars;                              ///< status bar bitmap
  IRect_t    TxBars_rect[BAR_COUNT];              ///< The bar rectangles

  // Blips
  GLtexture  BlipList_tex;                        ///< "you are here" texture
  Uint16     BlipList_count;
  BLIP       BlipList[MAXBLIP];

  // water and lighting info
  WATER_INFO    Water;
  LIGHTING_INFO Light;
  FOG_INFO      Fog;

  // dynamic lighting info
  int          DLightList_distancetobeat;      ///< The number to beat
  int          DLightList_count;               ///< Number of dynamic lights
  DLightList_t DLightList;

  // Texture info
  GLtexture TxTexture[MAXTEXTURE];   ///< All normal textures
  int       ico_lst[ICO_COUNT];      ///< A lookup table for special icons

  // Icon stuff
  IRect_t   TxIcon_rect;             ///< The 32x32 icon rectangle
  int       TxIcon_count;            ///< Number of icons
  GLtexture TxIcon[MAXICONTX];       ///< All icon textures
};
typedef struct sGraphics_Data Graphics_Data_t;

Graphics_Data_t * Graphics_Data_new(Graphics_Data_t * gd);
bool_t            Graphics_Data_delete(Graphics_Data_t * gd);



//------------------------------------
//Particle variables
//------------------------------------
EXTERN float           textureoffset[256];         ///< For moving textures


EXTERN EGO_CONST Uint32 particletrans_fp8  EQ( 0x80 );
EXTERN EGO_CONST Uint32 antialiastrans_fp8  EQ( 0xC0 );


//Network Stuff
//#define CHARVEL 5.0

EXTERN STRING      CStringTmp1, CStringTmp2;


struct sNet;
struct sClient;
struct sServer;


//--------------------------------------------------------------------------------------------
struct sClockState;

struct sMachineState
{
  egoboo_key_t ekey;

  time_t i_actual_time;
  double f_bishop_time;
  int    i_bishop_time_y;
  int    i_bishop_time_m;
  int    i_bishop_time_d;
  float  f_bishop_time_h;

  struct sClockState * clk;
};
typedef struct sMachineState MachineState_t;

MachineState_t * get_MachineState( void );
retval_t         MachineState_update(MachineState_t * mac);

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

/// \file graphic.c
/// \brief Simple Egoboo renderer
/// \details All sorts of stuff related to drawing the game

#include "graphic.h"
#include "graphic_prt.h"
#include "graphic_mad.h"
#include "graphic_fan.h"

#include "char.inl"
#include "particle.inl"
#include "enchant.inl"
#include "mad.h"
#include "profile.inl"
#include "mesh.inl"

#include "collision.h"
#include "object_BSP.h"
#include "mesh_BSP.h"

#include "log.h"
#include "script.h"
#include "camera.h"
#include "file_formats/id_md2.h"
#include "input.h"
#include "network.h"
#include "passage.h"
#include "menu.h"
#include "script_compile.h"
#include "game.h"
#include "ui.h"
#include "texture.h"
#include "clock.h"
#include "font_bmp.h"
#include "lighting.h"

#include "extensions/SDL_extensions.h"
#include "extensions/SDL_GL_extensions.h"

#include "egoboo_vfs.h"
#include "egoboo_setup.h"
#include "egoboo_strutil.h"
#include "egoboo_display_list.h"

#if defined(USE_LUA_CONSOLE)
#    include "lua_console.h"
#else
#    include "egoboo_console.h"
#endif

#include "egoboo_fileutil.h"
#include "egoboo.h"

#include <SDL_image.h>

#include <assert.h>

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

#define SPARKLESIZE 28
#define SPARKLEADD 2
#define BLIPSIZE 6

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

/// Structure for keeping track of which dynalights are visible
struct ego_dynalight_registry
{
    int         reference;
    ego_frect_t bound;
};

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

static SDL_bool _sdl_initialized_graphics = SDL_FALSE;
static bool_t   _ogl_initialized          = bfalse;

static float sinlut[MAXLIGHTROTATION];
static float coslut[MAXLIGHTROTATION];

// Camera optimization stuff
static float                   cornerx[4];
static float                   cornery[4];
static int                     cornerlistlowtohighy[4];
static float                   cornerlowx;
static float                   cornerhighx;
static float                   cornerlowy;
static float                   cornerhighy;

static float       dyna_distancetobeat;           // The number to beat
static int         dyna_list_count = 0;           // Number of dynamic lights
static ego_dynalight dyna_list[MAX_DYNA];

// Interface stuff
static ego_irect_t         iconrect;                   // The 32x32 icon rectangle

static ego_irect_t         tabrect[NUMBAR];            // The tab rectangles
static ego_irect_t         barrect[NUMBAR];            // The bar rectangles
static ego_irect_t         bliprect[COLOR_MAX];        // The blip rectangles
static ego_irect_t         maprect;                    // The map rectangle

static bool_t             gfx_page_flip_requested  = bfalse;
static bool_t             gfx_page_clear_requested = btrue;

static float dynalight_keep = 0.9f;

t_list< ego_billboard_data, BILLBOARD_COUNT  > BillboardList;

PROFILE_DECLARE( render_scene_init );
PROFILE_DECLARE( render_scene_mesh );
PROFILE_DECLARE( render_scene_solid );
PROFILE_DECLARE( render_scene_water );
PROFILE_DECLARE( render_scene_trans );

PROFILE_DECLARE( renderlist_make );
PROFILE_DECLARE( dolist_make );
PROFILE_DECLARE( do_grid_lighting );
PROFILE_DECLARE( light_fans );
PROFILE_DECLARE( update_all_chr_instance );
PROFILE_DECLARE( update_all_prt_instance );

PROFILE_DECLARE( render_scene_mesh_dolist_sort );
PROFILE_DECLARE( render_scene_mesh_ndr );
PROFILE_DECLARE( render_scene_mesh_drf_back );
PROFILE_DECLARE( render_scene_mesh_ref );
PROFILE_DECLARE( render_scene_mesh_ref_chr );
PROFILE_DECLARE( render_scene_mesh_drf_solid );
PROFILE_DECLARE( render_scene_mesh_render_shadows );

float time_draw_scene       = 0.0f;
float time_render_scene_init  = 0.0f;
float time_render_scene_mesh  = 0.0f;
float time_render_scene_solid = 0.0f;
float time_render_scene_water = 0.0f;
float time_render_scene_trans = 0.0f;

float time_render_scene_init_renderlist_make         = 0.0f;
float time_render_scene_init_dolist_make             = 0.0f;
float time_render_scene_init_do_grid_dynalight       = 0.0f;
float time_render_scene_init_light_fans              = 0.0f;
float time_render_scene_init_update_all_chr_instance = 0.0f;
float time_render_scene_init_update_all_prt_instance = 0.0f;

float time_render_scene_mesh_dolist_sort    = 0.0f;
float time_render_scene_mesh_ndr            = 0.0f;
float time_render_scene_mesh_drf_back       = 0.0f;
float time_render_scene_mesh_ref            = 0.0f;
float time_render_scene_mesh_ref_chr        = 0.0f;
float time_render_scene_mesh_drf_solid      = 0.0f;
float time_render_scene_mesh_render_shadows = 0.0f;

int GFX_WIDTH  = 800;
int GFX_HEIGHT = 600;

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

ego_gfx_config     gfx;

SDLX_video_parameters_t sdl_vparam;
oglx_video_parameters_t ogl_vparam;

size_t                dolist_count = 0;
ego_obj_registry_entity dolist[DOLIST_SIZE];

ego_renderlist     renderlist = {0, 0, 0, 0, 0, 0, 0};         // zero all the counters at startup

float            indextoenvirox[EGO_NORMAL_COUNT];
float            lighttoenviroy[256];

int rotmeshtopside;
int rotmeshbottomside;
int rotmeshup;
int rotmeshdown;

Uint8   mapon         = bfalse;
Uint8   mapvalid      = bfalse;
Uint8   youarehereon  = bfalse;

size_t  blip_count    = 0;
float   blip_x[MAXBLIP];
float   blip_y[MAXBLIP];
Uint8   blip_c[MAXBLIP];

int     msgtimechange = 0;

DisplayMsgAry_t  DisplayMsg;

ego_line_data line_list[LINE_COUNT];

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

static void gfx_init_SDL_graphics();

static void _flip_pages();
static void _debug_print( const char *text );
static int  _debug_vprintf( const char *format, va_list args );
static int  _va_draw_string( const float x, const float y, const char *format, va_list args );
static int  _draw_string_raw( const float x, const float y, const char *format, ... );

static void project_view( ego_camera * pcam );

static void init_icon_data();
static void init_bar_data();
static void init_blip_data();
static void init_map_data();

static bool_t render_billboard( const ego_camera * pcam, ego_billboard_data * pbb, const float scale );

static void gfx_update_timers();

static void gfx_begin_text();
static void gfx_end_text();

static void gfx_enable_texturing();
static void gfx_disable_texturing();

static void gfx_begin_2d( void );
static void gfx_end_2d( void );

static void light_fans( ego_renderlist * prlist );
static void render_water( ego_renderlist * prlist );

static void gfx_make_dynalist( ego_camera * pcam );

static void draw_one_quad( oglx_texture_t * ptex, const ego_frect_t & scr_rect, const ego_frect_t & tx_rect, const bool_t use_alpha = bfalse );

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

gfx_config_data_t * gfx_get_config()
{
    return static_cast<gfx_config_data_t *>( &gfx );
}

//--------------------------------------------------------------------------------------------
// MODULE "PRIVATE" FUNCTIONS
//--------------------------------------------------------------------------------------------
void _debug_print( const char *text )
{
    /// \author ZZ
    /// \details  This function sticks a message in the display queue and sets its timer

    int          slot;
    const char * src;
    char       * dst, * dst_end;
    ego_msg      * pmsg;

    if ( INVALID_CSTR( text ) ) return;

    // Get a "free" message
    slot = DisplayMsg_get_free();
    pmsg = DisplayMsg.ary + slot;

    // Copy the message
    for ( src = text, dst = pmsg->textdisplay, dst_end = dst + MESSAGESIZE;
          CSTR_END != *src && dst < dst_end;
          src++, dst++ )
    {
        *dst = *src;
    }
    if ( dst < dst_end ) *dst = CSTR_END;

    // Set the time
    pmsg->time = cfg.message_duration;
}

//--------------------------------------------------------------------------------------------
int _debug_vprintf( const char *format, va_list args )
{
    int retval = 0;

    if ( VALID_CSTR( format ) )
    {
        STRING szTmp;

        retval = SDL_vsnprintf( szTmp, SDL_arraysize( szTmp ), format, args );
        _debug_print( szTmp );
    }

    return retval;
}

//--------------------------------------------------------------------------------------------
int _va_draw_string( const float old_x, const float old_y, const char *format, va_list args )
{
    int cnt = 1;
    int x_stt;
    STRING szText;
    Uint8 cTmp;

    float new_x = old_x;
    float new_y = old_y;

    if ( SDL_vsnprintf( szText, SDL_arraysize( szText ) - 1, format, args ) <= 0 )
    {
        return new_y;
    }

    gfx_begin_text();
    {
        x_stt = new_x;
        cnt = 0;
        cTmp = szText[cnt];
        while ( CSTR_END != cTmp )
        {
            // Convert ASCII to our own little font
            if ( '~' == cTmp )
            {
                // Use squiggle for tab
                new_x = (( int( new_x ) ) & TABAND ) + TABADD;
            }
            else if ( '\n' == cTmp )
            {
                new_x  = x_stt;
                new_y += fontyspacing;
            }
            else
            {
                // Normal letter
                cTmp = asciitofont[cTmp];
                draw_one_font( cTmp, new_x, new_y );
                new_x += fontxspacing[cTmp];
            }

            cnt++;
            cTmp = szText[cnt];
        }
    }
    gfx_end_text();

    new_y += fontyspacing;

    return new_y;
}

//--------------------------------------------------------------------------------------------
int _draw_string_raw( const float old_x, const float old_y, const char *format, ... )
{
    /// \author BB
    /// \details  the same as draw string, but it does not use the gfx_begin_2d() ... gfx_end_2d()
    ///    bookends.

    va_list args;

    float new_y = old_y;

    va_start( args, format );
    new_y = _va_draw_string( old_x, new_y, format, args );
    va_end( args );

    return new_y;
}

//--------------------------------------------------------------------------------------------
// MODULE INITIALIZATION
//--------------------------------------------------------------------------------------------
void gfx_system_begin()
{
    // set the graphics state
    gfx_init_SDL_graphics();
    ogl_init();

    // initialize the gfx data structures
    BillboardList_free_all();
    TxTexture_init_all();

    // initialize the profiling variables
    PROFILE_INIT( render_scene_init );
    PROFILE_INIT( render_scene_mesh );
    PROFILE_INIT( render_scene_solid );
    PROFILE_INIT( render_scene_water );
    PROFILE_INIT( render_scene_trans );

    PROFILE_INIT( renderlist_make );
    PROFILE_INIT( dolist_make );
    PROFILE_INIT( do_grid_lighting );
    PROFILE_INIT( light_fans );
    PROFILE_INIT( update_all_chr_instance );
    PROFILE_INIT( update_all_prt_instance );

    PROFILE_INIT( render_scene_mesh_dolist_sort );
    PROFILE_INIT( render_scene_mesh_ndr );
    PROFILE_INIT( render_scene_mesh_drf_back );
    PROFILE_INIT( render_scene_mesh_ref );
    PROFILE_INIT( render_scene_mesh_ref_chr );
    PROFILE_INIT( render_scene_mesh_drf_solid );
    PROFILE_INIT( render_scene_mesh_render_shadows );

    // init some other variables
    stabilized_fps_sum    = 0.1f * TARGET_FPS;
    stabilized_fps_weight = 0.1f;
}

//--------------------------------------------------------------------------------------------
void gfx_system_end()
{
    // initialize the profiling variables
    PROFILE_FREE( render_scene_init );
    PROFILE_FREE( render_scene_mesh );
    PROFILE_FREE( render_scene_solid );
    PROFILE_FREE( render_scene_water );
    PROFILE_FREE( render_scene_trans );

    PROFILE_FREE( renderlist_make );
    PROFILE_FREE( dolist_make );
    PROFILE_FREE( do_grid_lighting );
    PROFILE_FREE( light_fans );
    PROFILE_FREE( update_all_chr_instance );
    PROFILE_FREE( update_all_prt_instance );

    PROFILE_FREE( render_scene_mesh_dolist_sort );
    PROFILE_FREE( render_scene_mesh_ndr );
    PROFILE_FREE( render_scene_mesh_drf_back );
    PROFILE_FREE( render_scene_mesh_ref );
    PROFILE_FREE( render_scene_mesh_ref_chr );
    PROFILE_FREE( render_scene_mesh_drf_solid );
    PROFILE_FREE( render_scene_mesh_render_shadows );

    BillboardList_free_all();
    TxTexture_release_all();
}

//--------------------------------------------------------------------------------------------
int ogl_init()
{
    gfx_init_SDL_graphics();

    // GL_DEBUG(glClear)) stuff
    GL_DEBUG( glClearColor )( 0.0f, 0.0f, 0.0f, 0.0f ); // Set the background black
    GL_DEBUG( glClearDepth )( 1.0f );

    // depth buffer stuff
    GL_DEBUG( glClearDepth )( 1.0f );
    GL_DEBUG( glDepthMask )( GL_TRUE );
    GL_DEBUG( glEnable )( GL_DEPTH_TEST );
    GL_DEBUG( glDepthFunc )( GL_LESS );

    // alpha stuff
    GL_DEBUG( glDisable )( GL_BLEND );
    GL_DEBUG( glEnable )( GL_ALPHA_TEST );
    GL_DEBUG( glAlphaFunc )( GL_GREATER, 0 );

    /// \todo Including backface culling here prevents the mesh from getting rendered
    // backface culling
    // GL_DEBUG(glEnable)(GL_CULL_FACE);
    // GL_DEBUG(glFrontFace)(GL_CW);            // GL_POLYGON_BIT
    // GL_DEBUG(glCullFace)(GL_BACK);

    // disable OpenGL lighting
    GL_DEBUG( glDisable )( GL_LIGHTING );

    // fill mode
    GL_DEBUG( glPolygonMode )( GL_FRONT, GL_FILL );
    GL_DEBUG( glPolygonMode )( GL_BACK,  GL_FILL );

    // ?Need this for color + lighting?
    GL_DEBUG( glEnable )( GL_COLOR_MATERIAL );  // Need this for color + lighting

    // set up environment mapping
    GL_DEBUG( glTexGeni )( GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP );  // Set The Texture Generation Mode For S To Sphere Mapping (NEW)
    GL_DEBUG( glTexGeni )( GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP );  // Set The Texture Generation Mode For T To Sphere Mapping (NEW)

    //Initialize the motion blur buffer
    GL_DEBUG( glClearAccum )( 0.0f, 0.0f, 0.0f, 1.0f );
    GL_DEBUG( glClear )( GL_ACCUM_BUFFER_BIT );

    // Load the current graphical settings
    // load_graphics();

    _ogl_initialized = btrue;

    return _ogl_initialized && _sdl_initialized_graphics;
}

//--------------------------------------------------------------------------------------------
void gfx_init_SDL_graphics()
{
    if ( _sdl_initialized_graphics ) return;

    ego_init_SDL_base();

    log_info( "Intializing SDL Video... " );
    if ( SDL_InitSubSystem( SDL_INIT_VIDEO ) < 0 )
    {
        log_message( "Failed!\n" );
        log_warning( "SDL error == \"%s\"\n", SDL_GetError() );
    }
    else
    {
        log_message( "Success!\n" );
    }

#if !defined(__APPLE__)
    {
        // Setup the cute windows manager icon, don't do this on Mac
        SDL_Surface *theSurface;
        const char * fname = "icon.bmp";
        STRING fileload;

        SDL_snprintf( fileload, SDL_arraysize( fileload ), "mp_data/%s", fname );

        theSurface = IMG_Load( vfs_resolveReadFilename( fileload ) );
        if ( NULL == theSurface )
        {
            log_warning( "Unable to load icon (%s)\n", fname );
        }
        else
        {
            SDL_WM_SetIcon( theSurface, NULL );
        }
    }
#endif

    // Set the window name
    SDL_WM_SetCaption( "Egoboo " VERSION, "Egoboo" );

#if defined(__unix__)

    // GLX doesn't differentiate between 24 and 32 bpp, asking for 32 bpp
    // will cause SDL_SetVideoMode to fail with:
    // "Unable to set video mode: Couldn't find matching GLX visual"
    if ( cfg.scrd_req == 32 ) cfg.scrd_req = 24;
    if ( cfg.scrz_req == 32 ) cfg.scrz_req = 24;

#endif

    // the flags to pass to SDL_SetVideoMode
    sdl_vparam.width                     = cfg.scrx_req;
    sdl_vparam.height                    = cfg.scry_req;
    sdl_vparam.depth                     = cfg.scrd_req;

    sdl_vparam.flags.opengl              = SDL_TRUE;
    sdl_vparam.flags.double_buf          = SDL_TRUE;
    sdl_vparam.flags.full_screen         = cfg.fullscreen_req;

    sdl_vparam.gl_att.buffer_size        = cfg.scrd_req;
    sdl_vparam.gl_att.depth_size         = cfg.scrz_req;
    sdl_vparam.gl_att.multi_buffers      = ( cfg.multisamples > 1 ) ? 1 : 0;
    sdl_vparam.gl_att.multi_samples      = cfg.multisamples;
    sdl_vparam.gl_att.accelerated_visual = GL_TRUE;

    ogl_vparam.dither         = cfg.use_dither ? GL_TRUE : GL_FALSE;
    ogl_vparam.antialiasing   = GL_TRUE;
    ogl_vparam.perspective    = cfg.use_perspective ? GL_NICEST : GL_FASTEST;
    ogl_vparam.mip_hint       = cfg.mipmap_quality ? GL_NICEST : GL_FASTEST;
    ogl_vparam.shading        = GL_SMOOTH;
    ogl_vparam.userAnisotropy = SDL_max( 0.0f, cfg.texturefilter_req - TX_ANISOTROPIC ) + 1.0f;
    ogl_vparam.userAnisotropy = CLIP( ogl_vparam.userAnisotropy, 1.0f, 16.0f );

    log_info( "Opening SDL Video Mode...\n" );

    // redirect the output of the SDL_GL_* debug functions
    SDL_GL_set_stdout( log_get_file() );

    // actually set the video mode
    if ( NULL == SDL_GL_set_mode( NULL, &sdl_vparam, &ogl_vparam, _sdl_initialized_graphics ) )
    {
        log_message( "Failed!\n" );
        log_error( "I can't get SDL to set any video mode: %s\n", SDL_GetError() );
    }
    else
    {
        GFX_WIDTH = ( const float )GFX_HEIGHT / ( const float )sdl_vparam.height * ( const float )sdl_vparam.width;
        log_message( "Success!\n" );
    }

    _sdl_initialized_graphics = SDL_TRUE;
}

//--------------------------------------------------------------------------------------------
bool_t gfx_set_virtual_screen( gfx_config_data_t * pgfx )
{
    float kx, ky;

    if ( NULL == pgfx ) return bfalse;

    kx = ( const float )GFX_WIDTH  / ( const float )sdl_scr.x;
    ky = ( const float )GFX_HEIGHT / ( const float )sdl_scr.y;

    if ( kx == ky )
    {
        pgfx->vw = sdl_scr.x;
        pgfx->vh = sdl_scr.y;
    }
    else if ( kx > ky )
    {
        pgfx->vw = sdl_scr.x * kx / ky;
        pgfx->vh = sdl_scr.y;
    }
    else
    {
        pgfx->vw = sdl_scr.x;
        pgfx->vh = sdl_scr.y * ky / kx;
    }

    pgfx->vdw = ( GFX_WIDTH  - pgfx->vw ) * 0.5f;
    pgfx->vdh = ( GFX_HEIGHT - pgfx->vh ) * 0.5f;

    ui_set_virtual_screen( pgfx->vw, pgfx->vh, GFX_WIDTH, GFX_HEIGHT );

    return btrue;
}

//--------------------------------------------------------------------------------------------
bool_t gfx_synch_config( gfx_config_data_t * pgfx, const struct s_config_data * pcfg )
{
    // call ego_gfx_config::init(), even if the config data is invalid
    if ( !gfx_config_data_init( pgfx ) ) return bfalse;

    // if there is no config data, do not proceed
    if ( NULL == pcfg ) return bfalse;

    pgfx->antialiasing = pcfg->multisamples > 0;

    pgfx->refon        = pcfg->reflect_allowed;
    pgfx->reffadeor    = pcfg->reflect_fade ? 0 : 255;

    pgfx->shaon        = pcfg->shadow_allowed;
    pgfx->shasprite    = pcfg->shadow_sprite;

    pgfx->shading      = pcfg->gourard_req ? GL_SMOOTH : GL_FLAT;
    pgfx->dither       = pcfg->use_dither;
    pgfx->perspective  = pcfg->use_perspective;
    pgfx->phongon      = pcfg->use_phong;
    pgfx->mipmap       = pcfg->mipmap_quality;

    pgfx->draw_background = pcfg->background_allowed && water.background_req;
    pgfx->draw_overlay    = pcfg->overlay_allowed && water.overlay_req;

    pgfx->dyna_list_max = CLIP( pcfg->dyna_count_req, 0, MAX_DYNA );

    pgfx->draw_water_0 = !pgfx->draw_overlay && ( water.layer_count > 0 );
    pgfx->clearson     = !pgfx->draw_background;
    pgfx->draw_water_1 = !pgfx->draw_background && ( water.layer_count > 1 );

    gfx_set_virtual_screen( pgfx );

    return btrue;
}

//--------------------------------------------------------------------------------------------
bool_t gfx_config_data_init( gfx_config_data_t * pgfx )
{
    if ( NULL == pgfx ) return bfalse;

    pgfx->shading          = GL_SMOOTH;
    pgfx->refon            = btrue;
    pgfx->reffadeor        = 0;
    pgfx->antialiasing     = bfalse;
    pgfx->dither           = bfalse;
    pgfx->perspective      = bfalse;
    pgfx->phongon          = btrue;
    pgfx->shaon            = btrue;
    pgfx->shasprite        = btrue;

    pgfx->clearson         = btrue;   // Do we clear every time?
    pgfx->draw_background  = bfalse;   // Do we draw the background image?
    pgfx->draw_overlay     = bfalse;   // Draw overlay?
    pgfx->draw_water_0     = btrue;   // Do we draw water layer 1 (TX_WATER_LOW)
    pgfx->draw_water_1     = btrue;   // Do we draw water layer 2 (TX_WATER_TOP)

    pgfx->dyna_list_max    = 8;

    return btrue;
}

//--------------------------------------------------------------------------------------------
bool_t ego_gfx_config::init( ego_gfx_config * pgfx )
{
    /* put any additional initialization here */

    return gfx_config_data_init( pgfx );
}

//--------------------------------------------------------------------------------------------
bool_t gfx_synch_oglx_texture_parameters( struct s_oglx_texture_parameters * ptex, const struct s_config_data * pcfg )
{
    /// \author BB
    /// \details  synch the texture parameters with the video mode

    if ( NULL == ptex || NULL == pcfg ) return GL_FALSE;

    if ( !ogl_caps.anisotropic_supported || ogl_caps.maxAnisotropy < 1.0f )
    {
        ptex->userAnisotropy = 1.0f;
        ptex->texturefilter  = ( TX_FILTERS )SDL_min( pcfg->texturefilter_req, TX_TRILINEAR_2 );
    }
    else
    {
        ptex->userAnisotropy = SDL_max( 0.0f, pcfg->texturefilter_req - TX_ANISOTROPIC ) + 1.0f;
        ptex->userAnisotropy = CLIP( ptex->userAnisotropy, 1.0f, ogl_caps.maxAnisotropy );
        ptex->texturefilter  = ( TX_FILTERS )SDL_min( pcfg->texturefilter_req, TX_TRILINEAR_2 );
    }

    return GL_TRUE;
}

//--------------------------------------------------------------------------------------------
// 2D RENDERER FUNCTIONS
//--------------------------------------------------------------------------------------------
int debug_printf( const char *format, ... )
{
    va_list args;
    int retval;

    va_start( args, format );
    retval = _debug_vprintf( format, args );
    va_end( args );

    return retval;
}

//--------------------------------------------------------------------------------------------
void draw_blip( const float sizeFactor, const Uint8 color, const float x, const float y, const bool_t mini_map )
{
    /// \author ZZ
    /// \details  This function draws a single blip
    ego_frect_t txrect;
    float   width, height;

    float mx, my;

    // Adjust the position values so that they fit inside the minimap
    mx = -1;
    my = -1;
    if ( mini_map )
    {
        mx = x * MAPSIZE / PMesh->gmem.edge_x;
        my = y * MAPSIZE / PMesh->gmem.edge_y + sdl_scr.y - MAPSIZE;
    }

    // Now draw it
    if ( mx > 0 && my > 0 )
    {
        oglx_texture_t * ptex = TxTexture_get_ptr( TX_REF( TX_BLIP ) );

        gfx_enable_texturing();
        GL_DEBUG( glColor4f )( 1.0f, 1.0f, 1.0f, 1.0f );
        GL_DEBUG( glNormal3f )( 0.0f, 0.0f, 1.0f );

        oglx_texture_Bind( ptex );

        txrect.xmin = ( const float )bliprect[color].xmin / ( const float )oglx_texture_GetTextureWidth( ptex );
        txrect.xmax = ( const float )bliprect[color].xmax / ( const float )oglx_texture_GetTextureWidth( ptex );
        txrect.ymin = ( const float )bliprect[color].ymin / ( const float )oglx_texture_GetTextureHeight( ptex );
        txrect.ymax = ( const float )bliprect[color].ymax / ( const float )oglx_texture_GetTextureHeight( ptex );

        width  = bliprect[color].xmax - bliprect[color].xmin;
        height = bliprect[color].ymax - bliprect[color].ymin;

        width  *= sizeFactor;
        height *= sizeFactor;

        GL_DEBUG_BEGIN( GL_QUADS );
        {
            GL_DEBUG( glTexCoord2f )( txrect.xmin, txrect.ymax ); GL_DEBUG( glVertex2f )( mx - ( width / 2 ), my + ( height / 2 ) );
            GL_DEBUG( glTexCoord2f )( txrect.xmax, txrect.ymax ); GL_DEBUG( glVertex2f )( mx + ( width / 2 ), my + ( height / 2 ) );
            GL_DEBUG( glTexCoord2f )( txrect.xmax, txrect.ymin ); GL_DEBUG( glVertex2f )( mx + ( width / 2 ), my - ( height / 2 ) );
            GL_DEBUG( glTexCoord2f )( txrect.xmin, txrect.ymin ); GL_DEBUG( glVertex2f )( mx - ( width / 2 ), my - ( height / 2 ) );
        }
        GL_DEBUG_END();
    }
}

//--------------------------------------------------------------------------------------------
void draw_one_icon( const TX_REF & icontype, const float x, const float y, const Uint8 sparkle )
{
    /// \author ZZ
    /// \details  This function draws an icon

    int     position, blip_pos_x, blip_pos_y;
    float   width, height;
    ego_frect_t txrect;

    txrect.xmin = ( const float )iconrect.xmin / ( const float )ICON_SIZE;
    txrect.xmax = ( const float )iconrect.xmax / ( const float )ICON_SIZE;
    txrect.ymin = ( const float )iconrect.ymin / ( const float )ICON_SIZE;
    txrect.ymax = ( const float )iconrect.ymax / ( const float )ICON_SIZE;

    width  = iconrect.xmax - iconrect.xmin;
    height = iconrect.ymax - iconrect.ymin;

    oglx_texture_t * ptex = TxTexture_get_ptr( icontype );

    ATTRIB_PUSH( __FUNCTION__, GL_ENABLE_BIT | GL_CURRENT_BIT );
    {
        GL_DEBUG( glColor4f )( 1.0f, 1.0f, 1.0f, 1.0f ); // GL_CURRENT_BIT

        // an alternate way of setting a "null" texture... see if this reduces ogl errors
        if ( NULL == ptex || INVALID_GL_ID == ptex->base.binding )
        {
            GL_DEBUG( glDisable )( GL_TEXTURE_2D );        // GL_ENABLE_BIT
        }
        else
        {
            GL_DEBUG( glEnable )( GL_TEXTURE_2D );         // GL_ENABLE_BIT
            oglx_texture_Bind( ptex );
        }

        GL_DEBUG_BEGIN( GL_QUADS );
        {
            GL_DEBUG( glTexCoord2f )( txrect.xmin, txrect.ymax ); GL_DEBUG( glVertex2f )( x,         y + height );
            GL_DEBUG( glTexCoord2f )( txrect.xmax, txrect.ymax ); GL_DEBUG( glVertex2f )( x + width, y + height );
            GL_DEBUG( glTexCoord2f )( txrect.xmax, txrect.ymin ); GL_DEBUG( glVertex2f )( x + width, y );
            GL_DEBUG( glTexCoord2f )( txrect.xmin, txrect.ymin ); GL_DEBUG( glVertex2f )( x,         y );
        }
        GL_DEBUG_END();
    }
    ATTRIB_POP( __FUNCTION__ )

    if ( sparkle != NOSPARKLE )
    {
        position = update_wld & 0x1F;
        position = ( SPARKLESIZE * position >> 5 );

        blip_pos_x = x + SPARKLEADD + position;
        blip_pos_y = y + SPARKLEADD;
        draw_blip( 0.5f, sparkle, blip_pos_x, blip_pos_y, bfalse );

        blip_pos_x = x + SPARKLEADD + SPARKLESIZE;
        blip_pos_y = y + SPARKLEADD + position;
        draw_blip( 0.5f, sparkle, blip_pos_x, blip_pos_y, bfalse );

        blip_pos_x = blip_pos_x - position;
        blip_pos_y = y + SPARKLEADD + SPARKLESIZE;
        draw_blip( 0.5f, sparkle, blip_pos_x, blip_pos_y, bfalse );

        blip_pos_x = x + SPARKLEADD;
        blip_pos_y = blip_pos_y - position;
        draw_blip( 0.5f, sparkle, blip_pos_x, blip_pos_y, bfalse );
    }
}

//--------------------------------------------------------------------------------------------
void draw_one_font( const int fonttype, const float x, const float y )
{
    /// \author ZZ
    /// \details  This function draws a letter or number
    /// \note GAC@> Very nasty version for starters.  Lots of room for improvement.

    GLfloat dx, dy, fx1, fx2, fy1, fy2, border;
    GLfloat x_min, y_min;
    GLfloat x_max, y_max;

    x_min = x;
    y_max = y + fontoffset;
    x_max = x + fontrect[fonttype].w;
    y_min = y_max - fontrect[fonttype].h;

    dx = 2.0f / 512.0f;
    dy = 1.0f / 256.0f;
    border = 1.0f / 512.0f;

    fx1 = fontrect[fonttype].x * dx + border;
    fx2 = ( fontrect[fonttype].x + fontrect[fonttype].w ) * dx - border;
    fy1 = fontrect[fonttype].y * dy + border;
    fy2 = ( fontrect[fonttype].y + fontrect[fonttype].h ) * dy - border;

    GL_DEBUG_BEGIN( GL_QUADS );
    {
        GL_DEBUG( glTexCoord2f )( fx1, fy2 );   GL_DEBUG( glVertex2f )( x_min, y_max );
        GL_DEBUG( glTexCoord2f )( fx2, fy2 );   GL_DEBUG( glVertex2f )( x_max, y_max );
        GL_DEBUG( glTexCoord2f )( fx2, fy1 );   GL_DEBUG( glVertex2f )( x_max, y_min );
        GL_DEBUG( glTexCoord2f )( fx1, fy1 );   GL_DEBUG( glVertex2f )( x_min, y_min );
    }
    GL_DEBUG_END();
}

//--------------------------------------------------------------------------------------------
void draw_map_texture( const float x, const float y )
{
    /// \author ZZ
    /// \details  This function draws the map

    gfx_enable_texturing();

    oglx_texture_Bind( TxTexture_get_ptr( TX_REF( TX_MAP ) ) );

    GL_DEBUG_BEGIN( GL_QUADS );
    {
        GL_DEBUG( glTexCoord2f )( 0.0f, 1.0f ); GL_DEBUG( glVertex2f )( x,           y + MAPSIZE );
        GL_DEBUG( glTexCoord2f )( 1.0f, 1.0f ); GL_DEBUG( glVertex2f )( x + MAPSIZE, y + MAPSIZE );
        GL_DEBUG( glTexCoord2f )( 1.0f, 0.0f ); GL_DEBUG( glVertex2f )( x + MAPSIZE, y );
        GL_DEBUG( glTexCoord2f )( 0.0f, 0.0f ); GL_DEBUG( glVertex2f )( x,           y );
    }
    GL_DEBUG_END();
}

//--------------------------------------------------------------------------------------------
float draw_one_xp_bar( const float old_x, const float old_y, const Uint8 ticks )
{
    /// \author ZF
    /// \details  This function draws a xp bar and returns the old_y position for the next one

    float width, height;
    Uint8 cnt;
    ego_frect_t txrect;

    Uint8 loc_ticks = SDL_min( ticks, NUMTICK );

    float new_x = old_x;
    float new_y = old_y;

    gfx_enable_texturing();               // Enable texture mapping
    GL_DEBUG( glColor4f )( 1.0f, 1.0f, 1.0f, 1.0f );

    // Draw the tab (always colored)
    oglx_texture_Bind( TxTexture_get_ptr( TX_REF( TX_XP_BAR ) ) );

    txrect.xmin   = 0.0f;
    txrect.xmax  = 32 / 128.0f;
    txrect.ymin    = XPTICK / 16.0f;
    txrect.ymax = XPTICK * 2.0f / 16.0f;

    width  = 16;
    height = XPTICK;

    GL_DEBUG_BEGIN( GL_QUADS );
    {
        GL_DEBUG( glTexCoord2f )( txrect.xmin, txrect.ymax ); GL_DEBUG( glVertex2f )( new_x,         new_y + height );
        GL_DEBUG( glTexCoord2f )( txrect.xmax, txrect.ymax ); GL_DEBUG( glVertex2f )( new_x + width, new_y + height );
        GL_DEBUG( glTexCoord2f )( txrect.xmax, txrect.ymin ); GL_DEBUG( glVertex2f )( new_x + width, new_y );
        GL_DEBUG( glTexCoord2f )( txrect.xmin, txrect.ymin ); GL_DEBUG( glVertex2f )( new_x,         new_y );
    }
    GL_DEBUG_END();
    new_x += 16;

    // Draw the filled ones
    txrect.xmin   = 0.0f;
    txrect.xmax  = 32 / 128.0f;
    txrect.ymin    = XPTICK / 16.0f;
    txrect.ymax = XPTICK * 2.0f / 16.0f;

    width  = XPTICK;
    height = XPTICK;

    for ( cnt = 0; cnt < loc_ticks; cnt++ )
    {
        oglx_texture_Bind( TxTexture_get_ptr( TX_REF( TX_XP_BAR ) ) );
        GL_DEBUG_BEGIN( GL_QUADS );
        {
            GL_DEBUG( glTexCoord2f )( txrect.xmin, txrect.ymax ); GL_DEBUG( glVertex2f )(( cnt * width ) + new_x,         new_y + height );
            GL_DEBUG( glTexCoord2f )( txrect.xmax, txrect.ymax ); GL_DEBUG( glVertex2f )(( cnt * width ) + new_x + width, new_y + height );
            GL_DEBUG( glTexCoord2f )( txrect.xmax, txrect.ymin ); GL_DEBUG( glVertex2f )(( cnt * width ) + new_x + width, new_y );
            GL_DEBUG( glTexCoord2f )( txrect.xmin, txrect.ymin ); GL_DEBUG( glVertex2f )(( cnt * width ) + new_x,         new_y );
        }
        GL_DEBUG_END();
    }

    // Draw the remaining empty ones
    txrect.xmin   = 0.0f;
    txrect.xmax  = 32 / 128.0f;
    txrect.ymin    = 0.0f;
    txrect.ymax = XPTICK / 16.0f;

    width  = XPTICK;
    height = XPTICK;

    for ( /*nothing*/; cnt < NUMTICK; cnt++ )
    {
        oglx_texture_Bind( TxTexture_get_ptr( TX_REF( TX_XP_BAR ) ) );
        GL_DEBUG_BEGIN( GL_QUADS );
        {
            GL_DEBUG( glTexCoord2f )( txrect.xmin, txrect.ymax ); GL_DEBUG( glVertex2f )(( cnt * width ) + new_x,         new_y + height );
            GL_DEBUG( glTexCoord2f )( txrect.xmax, txrect.ymax ); GL_DEBUG( glVertex2f )(( cnt * width ) + new_x + width, new_y + height );
            GL_DEBUG( glTexCoord2f )( txrect.xmax, txrect.ymin ); GL_DEBUG( glVertex2f )(( cnt * width ) + new_x + width, new_y );
            GL_DEBUG( glTexCoord2f )( txrect.xmin, txrect.ymin ); GL_DEBUG( glVertex2f )(( cnt * width ) + new_x,         new_y );
        }
        GL_DEBUG_END();
    }

    return new_y + XPTICK;
}

//--------------------------------------------------------------------------------------------
float draw_one_bar( const Uint8 bartype, const float old_x, const float old_y, const int ticks, const int maxticks )
{
    /// \author ZZ
    /// \details  This function draws a bar and returns the old_y position for the next one

    int     noticks;
    float   width, height;
    ego_frect_t tx_rect, sc_rect;
    oglx_texture_t * ptex;

    float new_x = old_x;
    float new_y = old_y;

    if ( maxticks <= 0 || ticks < 0 || bartype > NUMBAR ) return new_y;

    ptex = TxTexture_get_ptr( TX_REF( TX_BARS ) );

    // Draw the tab
    tx_rect.xmin = tabrect[bartype].xmin / 128.0f;
    tx_rect.xmax = tabrect[bartype].xmax / 128.0f;
    tx_rect.ymin = tabrect[bartype].ymin / 128.0f;
    tx_rect.ymax = tabrect[bartype].ymax / 128.0f;

    width  = tabrect[bartype].xmax - tabrect[bartype].xmin;
    height = tabrect[bartype].ymax - tabrect[bartype].ymin;

    sc_rect.xmin = new_x;
    sc_rect.xmax = new_x + width;
    sc_rect.ymin = new_y;
    sc_rect.ymax = new_y + height;

    draw_one_quad( ptex, sc_rect, tx_rect, btrue );

    // Error check
    int loc_maxticks = maxticks;
    if ( loc_maxticks > MAXTICK ) loc_maxticks = MAXTICK;

    int loc_ticks = ticks;
    if ( loc_ticks > loc_maxticks ) loc_ticks = loc_maxticks;

    // Draw the full rows of loc_ticks
    new_x += TABX;

    while ( loc_ticks >= NUMTICK )
    {
        barrect[bartype].xmax = BARX;

        tx_rect.xmin = barrect[bartype].xmin / 128.0f;
        tx_rect.xmax = barrect[bartype].xmax / 128.0f;
        tx_rect.ymin = barrect[bartype].ymin / 128.0f;
        tx_rect.ymax = barrect[bartype].ymax / 128.0f;

        width  = barrect[bartype].xmax - barrect[bartype].xmin;
        height = barrect[bartype].ymax - barrect[bartype].ymin;

        sc_rect.xmin = new_x;
        sc_rect.xmax = new_x + width;
        sc_rect.ymin = new_y;
        sc_rect.ymax = new_y + height;

        draw_one_quad( ptex, sc_rect, tx_rect, btrue );

        new_y += BARY;
        loc_ticks -= NUMTICK;
        loc_maxticks -= NUMTICK;
    }

    // Draw any partial rows of loc_ticks
    if ( loc_maxticks > 0 )
    {
        // Draw the filled ones
        barrect[bartype].xmax = ( loc_ticks << 3 ) + TABX;

        tx_rect.xmin   = barrect[bartype].xmin   / 128.0f;
        tx_rect.xmax  = barrect[bartype].xmax  / 128.0f;
        tx_rect.ymin    = barrect[bartype].ymin    / 128.0f;
        tx_rect.ymax = barrect[bartype].ymax / 128.0f;

        width = barrect[bartype].xmax - barrect[bartype].xmin;
        height = barrect[bartype].ymax - barrect[bartype].ymin;

        sc_rect.xmin = new_x;
        sc_rect.xmax = new_x + width;
        sc_rect.ymin = new_y;
        sc_rect.ymax = new_y + height;

        draw_one_quad( ptex, sc_rect, tx_rect, btrue );

        // Draw the empty ones
        noticks = loc_maxticks - loc_ticks;
        if ( noticks > ( NUMTICK - loc_ticks ) ) noticks = ( NUMTICK - loc_ticks );

        barrect[0].xmax = ( noticks << 3 ) + TABX;
        oglx_texture_Bind( TxTexture_get_ptr( TX_REF( TX_BARS ) ) );

        tx_rect.xmin = barrect[0].xmin / 128.0f;
        tx_rect.xmax = barrect[0].xmax / 128.0f;
        tx_rect.ymin = barrect[0].ymin / 128.0f;
        tx_rect.ymax = barrect[0].ymax / 128.0f;

        width  = barrect[0].xmax - barrect[0].xmin;
        height = barrect[0].ymax - barrect[0].ymin;

        sc_rect.xmin = new_x;
        sc_rect.xmax = new_x + width;
        sc_rect.ymin = new_y;
        sc_rect.ymax = new_y + height;

        draw_one_quad( ptex, sc_rect, tx_rect, btrue );

        loc_maxticks -= NUMTICK;
        new_y += BARY;
    }

    // Draw full rows of empty loc_ticks
    while ( loc_maxticks >= NUMTICK )
    {
        barrect[0].xmax = BARX;

        tx_rect.xmin = barrect[0].xmin / 128.0f;
        tx_rect.xmax = barrect[0].xmax / 128.0f;
        tx_rect.ymin = barrect[0].ymin / 128.0f;
        tx_rect.ymax = barrect[0].ymax / 128.0f;

        width  = barrect[0].xmax - barrect[0].xmin;
        height = barrect[0].ymax - barrect[0].ymin;

        sc_rect.xmin = new_x;
        sc_rect.xmax = new_x + width;
        sc_rect.ymin = new_y;
        sc_rect.ymax = new_y + height;

        draw_one_quad( ptex, sc_rect, tx_rect, btrue );

        new_y += BARY;
        loc_maxticks -= NUMTICK;
    }

    // Draw the last of the empty ones
    if ( loc_maxticks > 0 )
    {
        barrect[0].xmax = ( loc_maxticks << 3 ) + TABX;
        oglx_texture_Bind( TxTexture_get_ptr( TX_REF( TX_BARS ) ) );

        tx_rect.xmin   = barrect[0].xmin   / 128.0f;
        tx_rect.xmax  = barrect[0].xmax  / 128.0f;
        tx_rect.ymin    = barrect[0].ymin    / 128.0f;
        tx_rect.ymax = barrect[0].ymax / 128.0f;

        width = barrect[0].xmax - barrect[0].xmin;
        height = barrect[0].ymax - barrect[0].ymin;

        sc_rect.xmin = new_x;
        sc_rect.xmax = new_x + width;
        sc_rect.ymin = new_y;
        sc_rect.ymax = new_y + height;

        draw_one_quad( ptex, sc_rect, tx_rect, btrue );

        loc_maxticks -= NUMTICK;
        new_y += BARY;
    }

    return new_y;
}

//--------------------------------------------------------------------------------------------
float draw_string( const float old_x, const float old_y, const char *format, ... )
{
    /// \author ZZ
    /// \details  This function spits a line of null terminated text onto the backbuffer
    ///
    /// details BB@> Uses gfx_begin_2d() ... gfx_end_2d() so that the function can basically be called from anywhere
    ///    The way they are currently implemented, this breaks the icon drawing in draw_status() if
    ///    you use draw_string() and then draw_icon(). Use _draw_string_raw(), instead.

    va_list args;

    float new_y = old_y;

    gfx_begin_2d();
    {
        va_start( args, format );
        new_y = _va_draw_string( old_x, new_y, format, args );
        va_end( args );
    }
    gfx_end_2d();

    return new_y;
}

//--------------------------------------------------------------------------------------------
float draw_wrap_string( const char *szText, const float old_x, const float old_y, const int maxx )
{
    /// \author ZZ
    /// \details  This function spits a line of null terminated text onto the backbuffer,
    ///    wrapping over the right side and returning the new old_y value

    Uint8 cTmp = szText[0];
    Uint8 newword = btrue;
    int cnt = 1;

    gfx_begin_text();

    float min_x = old_x;
    float min_y = old_y;
    float stt_x = old_x;
    float max_x = maxx + stt_x;
    float max_y = min_y + fontyspacing;

    while ( cTmp != 0 )
    {
        // Check each new word for wrapping
        if ( newword )
        {
            int endx = min_x + font_bmp_length_of_word( szText + cnt - 1 );

            newword = bfalse;
            if ( endx > max_x )
            {
                // Wrap the end and cut off spaces and tabs
                min_x = stt_x + fontyspacing;
                min_y += fontyspacing;
                max_y += fontyspacing;

                while ( ' ' == cTmp || '~' == cTmp )
                {
                    cTmp = szText[cnt];
                    cnt++;
                }
            }
        }
        else
        {
            if ( '~' == cTmp )
            {
                // Use squiggle for tab
                min_x = (( int( min_x ) ) & TABAND ) + TABADD;
            }
            else if ( '\n' == cTmp )
            {
                min_x = stt_x;
                min_y += fontyspacing;
                max_y += fontyspacing;
            }
            else
            {
                // Normal letter
                cTmp = asciitofont[cTmp];
                draw_one_font( cTmp, min_x, min_y );
                min_x += fontxspacing[cTmp];
            }

            cTmp = szText[cnt];
            if ( '~' == cTmp || ' ' == cTmp )
            {
                newword = btrue;
            }

            cnt++;
        }
    }
    gfx_end_text();

    return max_y;
}

//--------------------------------------------------------------------------------------------
void draw_one_character_icon( const CHR_REF & item, const float old_x, const float old_y, const bool_t draw_ammo )
{
    /// \author BB
    /// \details  Draw an icon for the given item at the position <old_x,old_y>.
    ///     If the object is invalid, draw the null icon instead of failing

    TX_REF icon_ref;
    Uint8  draw_sparkle;

    float new_x = old_x;
    float new_y = old_y;

    ego_chr * pitem = !INGAME_CHR( item ) ? NULL : ChrObjList.get_data_ptr( item );

    // grab the icon reference
    icon_ref = ego_chr::get_icon_ref( item );

    // draw the icon
    draw_sparkle = ( NULL == pitem ) ? NOSPARKLE : pitem->sparkle;
    draw_one_icon( icon_ref, new_x, new_y, draw_sparkle );

    // draw the ammo, if requested
    if ( draw_ammo && ( NULL != pitem ) )
    {
        if ( pitem->ammo_max != 0 && pitem->ammoknown )
        {
            ego_cap * pitem_cap = ego_chr::get_pcap( item );

            if (( NULL != pitem_cap && !pitem_cap->isstackable ) || pitem->ammo > 1 )
            {
                // Show amount of ammo left
                _draw_string_raw( new_x, new_y - 8, "%2d", pitem->ammo );
            }
        }
    }
}

//--------------------------------------------------------------------------------------------
float draw_character_xp_bar( const CHR_REF & character, const float old_x, const float old_y )
{
    ego_chr * pchr;
    ego_cap * pcap;

    float new_y = old_y;

    pchr = ChrObjList.get_allocated_data_ptr( character );
    if ( !INGAME_PCHR( pchr ) ) return new_y;

    pcap = pro_get_pcap( pchr->profile_ref );
    if ( NULL == pcap ) return new_y;

    // Draw the small XP progress bar
    if ( pchr->experience_level < MAXLEVEL )
    {
        Uint8  curlevel    = pchr->experience_level + 1;
        Uint32 xplastlevel = pcap->experience_forlevel[curlevel-1];
        Uint32 xpneed      = pcap->experience_forlevel[curlevel];

        float fraction = ( const float )SDL_max( pchr->experience - xplastlevel, 0 ) / ( const float )SDL_max( xpneed - xplastlevel, 1 );
        int   numticks = fraction * NUMTICK;

        new_y = draw_one_xp_bar( old_x, new_y, numticks );
    }

    return new_y;
}

//--------------------------------------------------------------------------------------------
float draw_status( const CHR_REF & character, const float old_x, const float old_y )
{
    /// \author ZZ
    /// \details  This function shows a character's icon, status and inventory
    ///    The old_x,old_y coordinates are the top left point of the image to draw

    int cnt;
    char cTmp;
    char *readtext;
    STRING generictext;
    int life, life_max;
    int mana, mana_max;

    ego_chr * pchr;
    ego_cap * pcap;

    float new_y = old_y;

    pchr = ChrObjList.get_allocated_data_ptr( character );
    if ( !INGAME_PCHR( pchr ) || !pchr->draw_stats ) return new_y;

    pcap = ego_chr::get_pcap( character );
    if ( NULL == pcap ) return new_y;

    life     = SFP8_TO_SINT( pchr->life );
    life_max  = SFP8_TO_SINT( pchr->life_max );
    mana     = SFP8_TO_SINT( pchr->mana );
    mana_max  = SFP8_TO_SINT( pchr->mana_max );

    // grab the character's display name
    readtext = ( char * )ego_chr::get_name( character, CHRNAME_CAPITAL );

    // make a short name for the actual display
    for ( cnt = 0; cnt < 7; cnt++ )
    {
        cTmp = readtext[cnt];

        if ( ' ' == cTmp || CSTR_END == cTmp )
        {
            generictext[cnt] = CSTR_END;
            break;
        }
        else
        {
            generictext[cnt] = cTmp;
        }
    }
    generictext[7] = CSTR_END;

    // draw the name
    new_y = _draw_string_raw( old_x + 8, new_y, generictext );

    // draw the character's money
    new_y = _draw_string_raw( old_x + 8, new_y, "$%4d", pchr->money ) + 8;

    // draw the character's main icon
    draw_one_character_icon( character, old_x + 40, new_y, bfalse );

    // draw the left hand item icon
    draw_one_character_icon( pchr->holdingwhich[SLOT_LEFT], old_x + 8, new_y, btrue );

    // draw the right hand item icon
    draw_one_character_icon( pchr->holdingwhich[SLOT_RIGHT], old_x + 72, new_y, btrue );

    // skip to the next row
    new_y += 32;

    // Draw the small XP progress bar
    new_y = draw_character_xp_bar( character, old_x + 16, new_y );

    // Draw the life bar
    if ( pchr->alive )
    {
        new_y = draw_one_bar( pchr->life_color, old_x, new_y, life, life_max );
    }
    else
    {
        new_y = draw_one_bar( 0, old_x, new_y, 0, life_max );  // Draw a black bar
    }

    // Draw the mana bar
    if ( mana_max > 0 )
    {
        new_y = draw_one_bar( pchr->mana_color, old_x, new_y, mana, mana_max );
    }

    return new_y;
}

//--------------------------------------------------------------------------------------------
float draw_all_status( const float old_y )
{
    int cnt;

    float new_y = old_y;

    if ( StatList.on )
    {
        for ( cnt = 0; cnt < StatList.count && new_y < sdl_scr.y; cnt++ )
        {
            new_y = draw_status( StatList[cnt], sdl_scr.x - BARX, new_y );
        }
    }

    return new_y;
}

//--------------------------------------------------------------------------------------------
void draw_map()
{
    size_t cnt;

    // Map display
    if ( !mapvalid || !mapon ) return;

    ATTRIB_PUSH( "draw_map()", GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT );
    {
        GL_DEBUG( glEnable )( GL_BLEND );                               // GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT
        GL_DEBUG( glBlendFunc )( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );  // GL_COLOR_BUFFER_BIT

        GL_DEBUG( glColor4f )( 1.0f, 1.0f, 1.0f, 1.0f );
        draw_map_texture( 0, sdl_scr.y - MAPSIZE );

        GL_DEBUG( glBlendFunc )( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );  // GL_COLOR_BUFFER_BIT

        // If one of the players can sense enemies via EMP, draw them as blips on the map
        if ( TEAM_MAX != local_stats.sense_enemy_ID )
        {
            CHR_BEGIN_LOOP_PROCESSING( ichr, pchr )
            {
                ego_cap * pcap;

                if ( blip_count >= MAXBLIP ) break;

                pcap = ego_chr::get_pcap( ichr );
                if ( NULL == pcap ) continue;

                // Show only teams that will attack the player
                if ( team_hates_team( pchr->team, local_stats.sense_enemy_team ) )
                {
                    // Only if they match the required IDSZ ([NONE] always works)
                    if ( local_stats.sense_enemy_ID == IDSZ_NONE ||
                         local_stats.sense_enemy_ID == pcap->idsz[IDSZ_PARENT] ||
                         local_stats.sense_enemy_ID == pcap->idsz[IDSZ_TYPE  ] )
                    {
                        // Inside the map?
                        if ( pchr->pos.x < PMesh->gmem.edge_x && pchr->pos.y < PMesh->gmem.edge_y )
                        {
                            // Valid colors only
                            blip_x[blip_count] = pchr->pos.x;
                            blip_y[blip_count] = pchr->pos.y;
                            blip_c[blip_count] = COLOR_RED; // Red blips
                            blip_count++;
                        }
                    }
                }
            }
            CHR_END_LOOP();
        }

        // draw all the blips
        for ( cnt = 0; cnt < blip_count; cnt++ )
        {
            draw_blip( 0.75f, blip_c[cnt], blip_x[cnt], blip_y[cnt], btrue );
        }
        blip_count = 0;

        // Show local player position(s)
        if ( youarehereon && ( update_wld & 8 ) )
        {
            for ( player_deque::iterator ipla = PlaDeque.begin(); ipla != PlaDeque.end(); ipla++ )
            {
                if ( !ipla->valid ) continue;

                if ( INPUT_BITS_NONE != ipla->device.bits && INGAME_CHR( ipla->index ) )
                {
                    ego_chr * pchr = ChrObjList.get_data_ptr( ipla->index );
                    if ( pchr->alive )
                    {
                        draw_blip( 0.75f, COLOR_WHITE, pchr->pos.x, pchr->pos.y, btrue );
                    }
                }
            }
        }

        //// draw the camera
        // if ( update_wld & 2 )
        // {
        //   draw_blip( 0.75f, COLOR_PURPLE, GET_MAP_X(PMesh, PCamera->pos.x), GET_MAP_Y(PMesh, PCamera->pos.y));
        // }
    }
    ATTRIB_POP( "draw_map()" )
}

//--------------------------------------------------------------------------------------------
float draw_fps( const float old_y )
{
    // FPS text

    float new_y = old_y;

    if ( net_stats.out_of_sync )
    {
        new_y = _draw_string_raw( 0, new_y, "OUT OF SYNC" );
    }

    if ( parseerror )
    {
        new_y = _draw_string_raw( 0, new_y, "SCRIPT ERROR ( see \"/debug/log.txt\" )" );
    }

    if ( fpson )
    {
        new_y = _draw_string_raw( 0, new_y, "%2.3f FPS, %2.3f UPS, Update lag = %d", stabilized_fps, stabilized_ups, update_lag );
    }

#    if defined(BSP_INFO)
    new_y = _draw_string_raw( 0, new_y, "BSP chr %d/%d - BSP prt %d/%d", ego_obj_BSP::chr_count, MAX_CHR - chr_count_free(), ego_obj_BSP::prt_count, maxparticles - PrtObjList.free_count() );
    new_y = _draw_string_raw( 0, new_y, "BSP collisions %d", collision_system::hash_nodes_inserted );
    new_y = _draw_string_raw( 0, new_y, "chr-mesh tests %04d - prt-mesh tests %04d", ego_chr::stoppedby_tests + ego_chr::pressure_tests, ego_prt::stoppedby_tests + ego_prt::pressure_tests );
#    endif

#if defined(RENDERLIST_INFO)
    new_y = _draw_string_raw( 0, new_y, "Renderlist tiles %d/%d", renderlist.all_count, PMesh->info.tiles_count );
#endif

#if defined(PROFILE_RENDER) && PROFILE_DISPLAY
    new_y = _draw_string_raw( 0, new_y, "estimated max FPS %2.3f UPS %4.2f GFX %4.2f", est_max_fps, est_max_ups, est_max_gfx );
    new_y = _draw_string_raw( 0, new_y, "gfx:total %2.4g, render:total %2.4g", est_render_time, time_draw_scene );
    new_y = _draw_string_raw( 0, new_y, "render:init %2.4g,  render:mesh %2.4g", time_render_scene_init, time_render_scene_mesh );
    new_y = _draw_string_raw( 0, new_y, "render:solid %2.4g, render:water %2.4g", time_render_scene_solid, time_render_scene_water );
    new_y = _draw_string_raw( 0, new_y, "render:trans %2.4g", time_render_scene_trans );
#endif

#if defined(PROFILE_MESH) && PROFILE_DISPLAY
    new_y = _draw_string_raw( 0, new_y, "mesh:total %2.4g", time_render_scene_mesh );
    new_y = _draw_string_raw( 0, new_y, "mesh:dolist_sort %2.4g, mesh:ndr %2.4g", time_render_scene_mesh_dolist_sort , time_render_scene_mesh_ndr );
    new_y = _draw_string_raw( 0, new_y, "mesh:drf_back %2.4g, mesh:ref %2.4g", time_render_scene_mesh_drf_back, time_render_scene_mesh_ref );
    new_y = _draw_string_raw( 0, new_y, "mesh:ref_chr %2.4g, mesh:drf_solid %2.4g", time_render_scene_mesh_ref_chr, time_render_scene_mesh_drf_solid );
    new_y = _draw_string_raw( 0, new_y, "mesh:render_shadows %2.4g", time_render_scene_mesh_render_shadows );
#endif

#if defined(PROFILE_INIT) && PROFILE_DISPLAY
    new_y = _draw_string_raw( 0, new_y, "init:total %2.4g", time_render_scene_init );
    new_y = _draw_string_raw( 0, new_y, "init:renderlist_make %2.4g, init:dolist_make %2.4g", time_render_scene_init_renderlist_make, time_render_scene_init_dolist_make );
    new_y = _draw_string_raw( 0, new_y, "init:do_grid_lighting %2.4g, init:light_fans %2.4g", time_render_scene_init_do_grid_dynalight, time_render_scene_init_light_fans );
    new_y = _draw_string_raw( 0, new_y, "init:update_all_chr_instance %2.4g", time_render_scene_init_update_all_chr_instance );
    new_y = _draw_string_raw( 0, new_y, "init:update_all_prt_instance %2.4g", time_render_scene_init_update_all_prt_instance );
#endif

    return new_y;
}

//--------------------------------------------------------------------------------------------
float draw_help( const float old_y )
{
    float new_y = old_y;

    if ( SDLKEYDOWN( SDLK_F1 ) )
    {
        // In-Game help
        new_y = _draw_string_raw( 0, new_y, "!!!MOUSE HELP!!!" );
        new_y = _draw_string_raw( 0, new_y, "~~Go to input settings to change" );
        new_y = _draw_string_raw( 0, new_y, "Default settings" );
        new_y = _draw_string_raw( 0, new_y, "~~Left Click to use an item" );
        new_y = _draw_string_raw( 0, new_y, "~~Left and Right Click to grab" );
        new_y = _draw_string_raw( 0, new_y, "~~Middle Click to jump" );
        new_y = _draw_string_raw( 0, new_y, "~~A and S keys do stuff" );
        new_y = _draw_string_raw( 0, new_y, "~~Right Drag to move camera" );
    }
    if ( SDLKEYDOWN( SDLK_F2 ) )
    {
        // In-Game help
        new_y = _draw_string_raw( 0, new_y, "!!!JOYSTICK HELP!!!" );
        new_y = _draw_string_raw( 0, new_y, "~~Go to input settings to change." );
        new_y = _draw_string_raw( 0, new_y, "~~Hit the buttons" );
        new_y = _draw_string_raw( 0, new_y, "~~You'll figure it out" );
    }
    if ( SDLKEYDOWN( SDLK_F3 ) )
    {
        // In-Game help
        new_y = _draw_string_raw( 0, new_y, "!!!KEYBOARD HELP!!!" );
        new_y = _draw_string_raw( 0, new_y, "~~Go to input settings to change." );
        new_y = _draw_string_raw( 0, new_y, "Default settings" );
        new_y = _draw_string_raw( 0, new_y, "~~TGB control left hand" );
        new_y = _draw_string_raw( 0, new_y, "~~YHN control right hand" );
        new_y = _draw_string_raw( 0, new_y, "~~Keypad to move and jump" );
        new_y = _draw_string_raw( 0, new_y, "~~Number keys for stats" );
    }

    return new_y;
}

//--------------------------------------------------------------------------------------------
float draw_debug_character( const CHR_REF & ichr, const float old_y )
{
    float new_y = old_y;

    ego_chr * pchr = ChrObjList.get_allocated_data_ptr( ichr );
    if ( !DEFINED_PCHR( pchr ) ) return new_y;

    return new_y;
}

//--------------------------------------------------------------------------------------------
float draw_debug_player( const PLA_REF & ipla, const float old_y )
{
    float new_y = old_y;

    ego_player * ppla = PlaDeque.find_by_ref( ipla );
    if ( NULL == ppla || !ppla->valid ) return new_y;

    if ( DEFINED_CHR( ppla->index ) )
    {
        CHR_REF ichr = ppla->index;
        ego_chr  *pchr = ChrObjList.get_data_ptr( ichr );

        new_y = _draw_string_raw( 0, new_y, "~~PLA%d DEF %d %d %d %d %d %d %d %d", ipla.get_value(),
                                  GET_DAMAGE_RESIST( pchr->damagemodifier[DAMAGE_SLASH] ),
                                  GET_DAMAGE_RESIST( pchr->damagemodifier[DAMAGE_CRUSH] ),
                                  GET_DAMAGE_RESIST( pchr->damagemodifier[DAMAGE_POKE ] ),
                                  GET_DAMAGE_RESIST( pchr->damagemodifier[DAMAGE_HOLY ] ),
                                  GET_DAMAGE_RESIST( pchr->damagemodifier[DAMAGE_EVIL ] ),
                                  GET_DAMAGE_RESIST( pchr->damagemodifier[DAMAGE_FIRE ] ),
                                  GET_DAMAGE_RESIST( pchr->damagemodifier[DAMAGE_ICE  ] ),
                                  GET_DAMAGE_RESIST( pchr->damagemodifier[DAMAGE_ZAP  ] ) );

        new_y = _draw_string_raw( 0, new_y, "~~PLA%d %5.1f %5.1f", ipla.get_value(), pchr->pos.x / GRID_SIZE, pchr->pos.y / GRID_SIZE );
    }

    return new_y;
}

//--------------------------------------------------------------------------------------------
float draw_debug( const float old_y )
{
    //PLA_REF ipla;
    //ego_player * ppla;

    float new_y = old_y;

    if ( !cfg.dev_mode ) return new_y;

    // draw the character's speed
    //ppla = PlaDeque.find_by_ref( PLA_REF( 0 ) );
    //if ( NULL != ppla && ppla->valid )
    //{
    //    ego_chr * pchr = ChrObjList.get_allocated_data_ptr( ppla->index );
    //    if ( DEFINED_PCHR( pchr ) )
    //    {
    //        new_y = _draw_string_raw( 0, new_y, "PLA%d hspeed %2.4f vspeed %2.4f %s", ipla.get_value(), fvec2_length( pchr->vel.v ), pchr->vel.z, pchr->enviro.is_slipping ? " - slipping" : "" );
    //    }
    //}

    if ( SDLKEYDOWN( SDLK_F5 ) )
    {
        new_y = _draw_string_raw( 0, new_y, "!!!DEBUG MODE-5!!!" );
        new_y = draw_debug_player( PLA_REF( 0 ), new_y );
        new_y = draw_debug_player( PLA_REF( 1 ), new_y );
        new_y = draw_debug_player( PLA_REF( 2 ), new_y );
    }

    if ( SDLKEYDOWN( SDLK_F6 ) )
    {
        // More debug information

        new_y = _draw_string_raw( 0, new_y, "!!!DEBUG MODE-6!!!" );
        new_y = _draw_string_raw( 0, new_y, "~~FREE CHR : %d", ChrObjList.free_count() );
        new_y = _draw_string_raw( 0, new_y, "~~FREE ENC : %d", EncObjList.free_count() );
        new_y = _draw_string_raw( 0, new_y, "~~FREE PRT : %d", PrtObjList.free_count() );
        new_y = _draw_string_raw( 0, new_y, "~~PASS CNT : %d/%d", ShopStack.count, PassageStack.count );

        const net_instance * pnet = net_get_instance();
        if ( NULL != pnet )
        {
            STRING sz_machine_type;

            if ( 0 == pnet->machine_type ) strncpy( sz_machine_type, "HOST ID=0", SDL_arraysize( sz_machine_type ) );
            else SDL_snprintf( sz_machine_type, SDL_arraysize( sz_machine_type ), "REMOTE ID=%d", pnet->machine_type );

            new_y = _draw_string_raw( 0, new_y, "~~MACHINE : %s", sz_machine_type );
            new_y = _draw_string_raw( 0, new_y, "~~NET PLA : %d", pnet->player_count );
        }

        new_y = _draw_string_raw( 0, new_y, "~~DAMAGE   : %d", damagetile.parttype );
        new_y = _draw_string_raw( 0, new_y, "~~EXPORT   : %s", PMod->exportvalid ? "TRUE" : "FALSE" );
        // new_y = _draw_string_raw( 0, new_y, "~~FOGAFF %d", fog_data.affects_water );
    }

    return new_y;
}

//--------------------------------------------------------------------------------------------
float draw_timer( const float old_y )
{
    int fifties, seconds, minutes;

    float new_y = old_y;

    if ( timeron )
    {
        fifties = ( timervalue % 50 ) << 1;
        seconds = (( timervalue / 50 ) % 60 );
        minutes = ( timervalue / 3000 );
        new_y = _draw_string_raw( 0, new_y, "=%d:%02d:%02d=", minutes, seconds, fifties );
    }

    return new_y;
}

//--------------------------------------------------------------------------------------------
float draw_game_status( const float old_y )
{
    float new_y = old_y;

    if ( net_waiting_for_players() )
    {
        new_y = _draw_string_raw( 0, new_y, "Waiting for players... " );
    }
    else if ( net_stats.pla_count_total > 0 )
    {
        if (( 0 == net_stats.pla_count_total_alive ) || PMod->respawnanytime )
        {
            if ( PMod->respawnvalid && cfg.difficulty < GAME_HARD )
            {
                new_y = _draw_string_raw( 0, new_y, "PRESS SPACE TO RESPAWN" );
            }
            else
            {
                new_y = _draw_string_raw( 0, new_y, "PRESS ESCAPE TO QUIT" );
            }
        }
        else if ( PMod->beat )
        {
            new_y = _draw_string_raw( 0, new_y, "VICTORY!  PRESS ESCAPE" );
        }
    }
    else
    {
        new_y = _draw_string_raw( 0, new_y, "ERROR: MISSING PLAYERS" );
    }

    return new_y;
}

//--------------------------------------------------------------------------------------------
float draw_messages( const float old_y )
{
    int cnt, tnc;

    float new_y = old_y;

    // Messages
    if ( messageon )
    {
        // Display the messages
        tnc = DisplayMsg.count;
        for ( cnt = 0; cnt < maxmessage; cnt++ )
        {
            if ( DisplayMsg.ary[tnc].time > 0 )
            {
                new_y = draw_wrap_string( DisplayMsg.ary[tnc].textdisplay, 0, new_y, sdl_scr.x - wraptolerance );
                if ( DisplayMsg.ary[tnc].time > msgtimechange )
                {
                    DisplayMsg.ary[tnc].time -= msgtimechange;
                }
                else
                {
                    DisplayMsg.ary[tnc].time = 0;
                }
            }

            tnc = ( tnc + 1 ) % maxmessage;
        }

        msgtimechange = 0;
    }

    return new_y;
}

//--------------------------------------------------------------------------------------------
void draw_text()
{
    /// \author ZZ
    /// \details  draw in-game heads up display

    float y;

    gfx_begin_2d();
    {
        draw_map();

        y = draw_all_status( 0 );

        y = draw_fps( 0 );
        y = draw_help( y );
        y = draw_debug( y );
        y = draw_timer( y );
        y = draw_game_status( y );

        // Network message input
        if ( console_mode )
        {
            char buffer[KEYB_BUFFER_SIZE + 128] = EMPTY_CSTR;

            SDL_snprintf( buffer, SDL_arraysize( buffer ), "%s > %s%s", cfg.network_messagename, keyb.buffer, HAS_NO_BITS( update_wld, 8 ) ? "x" : "+" );

            y = draw_wrap_string( buffer, 0, y, sdl_scr.x - wraptolerance );
        }

        y = draw_messages( y );
    }
    gfx_end_2d();
}

//--------------------------------------------------------------------------------------------
// 3D RENDERER FUNCTIONS
//--------------------------------------------------------------------------------------------
void project_view( ego_camera * pcam )
{
    /// \author ZZ
    /// \details  This function figures out where the corners of the view area
    ///    go when projected onto the plane of the PMesh->  Used later for
    ///    determining which mesh fans need to be rendered

    int cnt, tnc, extra[4];
    float ztemp;
    float numstep;
    float zproject;
    float xfin, yfin, zfin;
    fmat_4x4_t mTemp;

    // Range
    ztemp = ( pcam->pos.z );

    // Topleft
    mTemp = MatrixMult( RotateY( -rotmeshtopside * PI / 360 ), PCamera->mView );
    mTemp = MatrixMult( RotateX( rotmeshup * PI / 360 ), mTemp );
    zproject = mTemp.CNV( 2, 2 );             // 2,2
    // Camera must look down
    if ( zproject < 0 )
    {
        numstep = -ztemp / zproject;
        xfin = pcam->pos.x + ( numstep * mTemp.CNV( 0, 2 ) );  // xgg      // 0,2
        yfin = pcam->pos.y + ( numstep * mTemp.CNV( 1, 2 ) );    // 1,2
        zfin = 0;
        cornerx[0] = xfin;
        cornery[0] = yfin;
    }

    // Topright
    mTemp = MatrixMult( RotateY( rotmeshtopside * PI / 360 ), PCamera->mView );
    mTemp = MatrixMult( RotateX( rotmeshup * PI / 360 ), mTemp );
    zproject = mTemp.CNV( 2, 2 );             // 2,2
    // Camera must look down
    if ( zproject < 0 )
    {
        numstep = -ztemp / zproject;
        xfin = pcam->pos.x + ( numstep * mTemp.CNV( 0, 2 ) );  // xgg      // 0,2
        yfin = pcam->pos.y + ( numstep * mTemp.CNV( 1, 2 ) );    // 1,2
        zfin = 0;
        cornerx[1] = xfin;
        cornery[1] = yfin;
    }

    // Bottomright
    mTemp = MatrixMult( RotateY( rotmeshbottomside * PI / 360 ), PCamera->mView );
    mTemp = MatrixMult( RotateX( -rotmeshdown * PI / 360 ), mTemp );
    zproject = mTemp.CNV( 2, 2 );             // 2,2

    // Camera must look down
    if ( zproject < 0 )
    {
        numstep = -ztemp / zproject;
        xfin = pcam->pos.x + ( numstep * mTemp.CNV( 0, 2 ) );  // xgg      // 0,2
        yfin = pcam->pos.y + ( numstep * mTemp.CNV( 1, 2 ) );    // 1,2
        zfin = 0;
        cornerx[2] = xfin;
        cornery[2] = yfin;
    }

    // Bottomleft
    mTemp = MatrixMult( RotateY( -rotmeshbottomside * PI / 360 ), PCamera->mView );
    mTemp = MatrixMult( RotateX( -rotmeshdown * PI / 360 ), mTemp );
    zproject = mTemp.CNV( 2, 2 );             // 2,2
    // Camera must look down
    if ( zproject < 0 )
    {
        numstep = -ztemp / zproject;
        xfin = pcam->pos.x + ( numstep * mTemp.CNV( 0, 2 ) );  // xgg      // 0,2
        yfin = pcam->pos.y + ( numstep * mTemp.CNV( 1, 2 ) );    // 1,2
        zfin = 0;
        cornerx[3] = xfin;
        cornery[3] = yfin;
    }

    // Get the extreme values
    cornerlowx = cornerx[0];
    cornerlowy = cornery[0];
    cornerhighx = cornerx[0];
    cornerhighy = cornery[0];
    cornerlistlowtohighy[0] = 0;
    cornerlistlowtohighy[3] = 0;

    for ( cnt = 0; cnt < 4; cnt++ )
    {
        if ( cornerx[cnt] < cornerlowx )
            cornerlowx = cornerx[cnt];
        if ( cornery[cnt] < cornerlowy )
        {
            cornerlowy = cornery[cnt];
            cornerlistlowtohighy[0] = cnt;
        }
        if ( cornerx[cnt] > cornerhighx )
            cornerhighx = cornerx[cnt];
        if ( cornery[cnt] > cornerhighy )
        {
            cornerhighy = cornery[cnt];
            cornerlistlowtohighy[3] = cnt;
        }
    }

    // Figure out the order of points
    for ( cnt = 0, tnc = 0; cnt < 4; cnt++ )
    {
        if ( cnt != cornerlistlowtohighy[0] && cnt != cornerlistlowtohighy[3] )
        {
            extra[tnc] = cnt;
            tnc++;
        }
    }

    cornerlistlowtohighy[1] = extra[1];
    cornerlistlowtohighy[2] = extra[0];
    if ( cornery[extra[0]] < cornery[extra[1]] )
    {
        cornerlistlowtohighy[1] = extra[0];
        cornerlistlowtohighy[2] = extra[1];
    }
}

//--------------------------------------------------------------------------------------------
void render_shadow_sprite( const float intensity, ego_GLvertex v[] )
{
    int i;

    if ( intensity*255.0f < 1.0f ) return;

    GL_DEBUG( glColor4f )( intensity, intensity, intensity, 1.0f );

    GL_DEBUG_BEGIN( GL_TRIANGLE_FAN );
    {
        for ( i = 0; i < 4; i++ )
        {
            GL_DEBUG( glTexCoord2fv )( v[i].tex );
            GL_DEBUG( glVertex3fv )( v[i].pos );
        }
    }
    GL_DEBUG_END();
}

//--------------------------------------------------------------------------------------------
void render_shadow( const CHR_REF & character )
{
    /// \author ZZ
    /// \details  This function draws a NIFTY shadow
    ego_GLvertex v[4];

    const float size_factor = 1.5f;

    TX_REF  itex;
    int     itex_style;
    float   x, y;
    float   level;
    float   height, size_umbra, size_penumbra;
    float   alpha, alpha_umbra, alpha_penumbra;
    ego_chr * pchr;

    if ( IS_ATTACHED_CHR( character ) ) return;
    pchr = ChrObjList.get_data_ptr( character );

    // if the character is hidden, not drawn at all, so no shadow
    if ( pchr->is_hidden || 0 == pchr->shadow_size ) return;

    // no shadow if off the mesh
    if ( !ego_mpd::grid_is_valid( PMesh, pchr->onwhichgrid ) ) return;

    // no shadow if invalid tile image
    if ( TILE_IS_FANOFF( PMesh->tmem.tile_list[pchr->onwhichgrid] ) ) return;

    // no shadow if completely transparent
    alpha = ( 255 == pchr->gfx_inst.light ) ? pchr->gfx_inst.alpha * INV_FF : ( pchr->gfx_inst.alpha - pchr->gfx_inst.light ) * INV_FF;

    /// \test ZF@> The previous test didn't work, but this one does
    //if ( alpha * 255 < 1 ) return;
    //if ( pchr->gfx_inst.light <= INVISIBLE || pchr->gfx_inst.alpha <= INVISIBLE ) return;
    if ( alpha <= 0.0f ) return;

    // much reduced shadow if on a reflective tile
    if ( 0 != ego_mpd::test_fx( PMesh, pchr->onwhichgrid, MPDFX_DRAWREF ) )
    {
        alpha *= 0.1f;
    }

    // Original points
    level  = pchr->enviro.grid_level + SHADOWRAISE;
    height = ( pchr->pos.z + pchr->chr_cv.maxs[kZ] ) - level;
    if ( height < 0 ) height = 0;

    size_umbra    = size_factor * ( pchr->bump_1.size - height / 30.0f );
    size_penumbra = size_factor * ( pchr->bump_1.size + height / 30.0f );

    alpha_umbra = alpha_penumbra = alpha * 0.3f;
    if ( height > 0.0f )
    {
        float factor_penumbra = size_factor * (( pchr->bump_1.size ) / size_penumbra );
        float factor_umbra    = size_factor * (( pchr->bump_1.size ) / size_umbra );

        factor_umbra    = SDL_max( 1.0f, factor_umbra );
        factor_penumbra = SDL_max( 1.0f, factor_penumbra );

        alpha_umbra    *= 1.0f / factor_umbra / factor_umbra / size_factor;
        alpha_penumbra *= 1.0f / factor_penumbra / factor_penumbra / size_factor;

        alpha_umbra    = CLIP( alpha_umbra,    0.0f, 1.0f );
        alpha_penumbra = CLIP( alpha_penumbra, 0.0f, 1.0f );
    }

    x = pchr->gfx_inst.matrix.CNV( 3, 0 );
    y = pchr->gfx_inst.matrix.CNV( 3, 1 );

    // Choose texture.
    itex = TX_PARTICLE_LIGHT;
    oglx_texture_Bind( TxTexture_get_ptr( itex ) );

    itex_style = get_prt_texture_style( itex );
    if ( itex_style < 0 ) itex_style = 0;

    // GOOD SHADOW
    v[0].tex[SS] = CALCULATE_PRT_U0( itex_style, 238 );
    v[0].tex[TT] = CALCULATE_PRT_V0( itex_style, 238 );

    v[1].tex[SS] = CALCULATE_PRT_U1( itex_style, 255 );
    v[1].tex[TT] = CALCULATE_PRT_V0( itex_style, 238 );

    v[2].tex[SS] = CALCULATE_PRT_U1( itex_style, 255 );
    v[2].tex[TT] = CALCULATE_PRT_V1( itex_style, 255 );

    v[3].tex[SS] = CALCULATE_PRT_U0( itex_style, 238 );
    v[3].tex[TT] = CALCULATE_PRT_V1( itex_style, 255 );

    if ( size_penumbra > 0.0f )
    {
        v[0].pos[XX] = x + size_penumbra;
        v[0].pos[YY] = y - size_penumbra;
        v[0].pos[ZZ] = level;

        v[1].pos[XX] = x + size_penumbra;
        v[1].pos[YY] = y + size_penumbra;
        v[1].pos[ZZ] = level;

        v[2].pos[XX] = x - size_penumbra;
        v[2].pos[YY] = y + size_penumbra;
        v[2].pos[ZZ] = level;

        v[3].pos[XX] = x - size_penumbra;
        v[3].pos[YY] = y - size_penumbra;
        v[3].pos[ZZ] = level;

        render_shadow_sprite( alpha_penumbra, v );
    };

    if ( size_umbra > 0.0f )
    {
        v[0].pos[XX] = x + size_umbra;
        v[0].pos[YY] = y - size_umbra;
        v[0].pos[ZZ] = level + 0.1f;

        v[1].pos[XX] = x + size_umbra;
        v[1].pos[YY] = y + size_umbra;
        v[1].pos[ZZ] = level + 0.1f;

        v[2].pos[XX] = x - size_umbra;
        v[2].pos[YY] = y + size_umbra;
        v[2].pos[ZZ] = level + 0.1f;

        v[3].pos[XX] = x - size_umbra;
        v[3].pos[YY] = y - size_umbra;
        v[3].pos[ZZ] = level + 0.1f;

        render_shadow_sprite( alpha_umbra, v );
    };
}

//--------------------------------------------------------------------------------------------
void render_bad_shadow( const CHR_REF & character )
{
    /// \author ZZ
    /// \details  This function draws a sprite shadow

    ego_GLvertex v[4];

    TX_REF  itex;
    int     itex_style;
    float   size, x, y;
    float   level, height, height_factor, alpha;
    ego_chr * pchr;

    if ( IS_ATTACHED_CHR( character ) ) return;
    pchr = ChrObjList.get_data_ptr( character );

    // if the character is hidden, not drawn at all, so no shadow
    if ( pchr->is_hidden || pchr->shadow_size == 0 ) return;

    // no shadow if off the mesh
    if ( !ego_mpd::grid_is_valid( PMesh, pchr->onwhichgrid ) ) return;

    // no shadow if invalid tile image
    if ( TILE_IS_FANOFF( PMesh->tmem.tile_list[pchr->onwhichgrid] ) ) return;

    // no shadow if completely transparent or completely glowing
    alpha = ( 255 == pchr->gfx_inst.light ) ? pchr->gfx_inst.alpha  * INV_FF : ( pchr->gfx_inst.alpha - pchr->gfx_inst.light ) * INV_FF;

    /// \test ZF@> previous test didn't work, but this one does
    //if ( alpha * 255 < 1 ) return;
    if ( pchr->gfx_inst.light <= INVISIBLE || pchr->gfx_inst.alpha <= INVISIBLE ) return;

    // much reduced shadow if on a reflective tile
    if ( 0 != ego_mpd::test_fx( PMesh, pchr->onwhichgrid, MPDFX_DRAWREF ) )
    {
        alpha *= 0.1f;
    }
    if ( alpha < INV_FF ) return;

    // Original points
    level = pchr->enviro.grid_level;
    level += SHADOWRAISE;
    height = pchr->gfx_inst.matrix.CNV( 3, 2 ) - level;
    height_factor = 1.0f - height / ( pchr->shadow_size * 5.0f );
    if ( height_factor <= 0.0f ) return;

    // how much transparency from height
    alpha *= height_factor * 0.5f + 0.25f;
    if ( alpha < INV_FF ) return;

    x = pchr->gfx_inst.matrix.CNV( 3, 0 );
    y = pchr->gfx_inst.matrix.CNV( 3, 1 );

    size = pchr->shadow_size * height_factor;

    v[0].pos[XX] = ( const float ) x + size;
    v[0].pos[YY] = ( const float ) y - size;
    v[0].pos[ZZ] = ( const float ) level;

    v[1].pos[XX] = ( const float ) x + size;
    v[1].pos[YY] = ( const float ) y + size;
    v[1].pos[ZZ] = ( const float ) level;

    v[2].pos[XX] = ( const float ) x - size;
    v[2].pos[YY] = ( const float ) y + size;
    v[2].pos[ZZ] = ( const float ) level;

    v[3].pos[XX] = ( const float ) x - size;
    v[3].pos[YY] = ( const float ) y - size;
    v[3].pos[ZZ] = ( const float ) level;

    // Choose texture and matrix
    itex = TX_PARTICLE_LIGHT;
    oglx_texture_Bind( TxTexture_get_ptr( itex ) );

    itex_style = get_prt_texture_style( itex );
    if ( itex_style < 0 ) itex_style = 0;

    v[0].tex[SS] = CALCULATE_PRT_U0( itex_style, 236 );
    v[0].tex[TT] = CALCULATE_PRT_V0( itex_style, 236 );

    v[1].tex[SS] = CALCULATE_PRT_U1( itex_style, 253 );
    v[1].tex[TT] = CALCULATE_PRT_V0( itex_style, 236 );

    v[2].tex[SS] = CALCULATE_PRT_U1( itex_style, 253 );
    v[2].tex[TT] = CALCULATE_PRT_V1( itex_style, 253 );

    v[3].tex[SS] = CALCULATE_PRT_U0( itex_style, 236 );
    v[3].tex[TT] = CALCULATE_PRT_V1( itex_style, 253 );

    render_shadow_sprite( alpha, v );
}

//--------------------------------------------------------------------------------------------
void update_all_chr_instance()
{
    CHR_REF cnt;

    CHR_BEGIN_LOOP_PROCESSING( cnt, pchr )
    {
        if ( !ego_mpd::grid_is_valid( PMesh, pchr->onwhichgrid ) ) continue;

        if ( rv_success == ego_chr::update_instance( pchr ) )
        {
            // the instance has changed, refresh the collision bound
            ego_chr::update_collision_size( pchr, btrue );
        }
    }
    CHR_END_LOOP()
}

//--------------------------------------------------------------------------------------------
bool_t render_fans_by_list( ego_mpd * pmesh, const Uint32 list[], const size_t list_size )
{
    Uint32 cnt;
    TX_REF tx;

    if ( NULL == pmesh || NULL == list || 0 == list_size ) return bfalse;

    if ( meshnotexture )
    {
        meshlasttexture = Uint16( ~0 );
        oglx_texture_Bind( NULL );

        for ( cnt = 0; cnt < list_size; cnt++ )
        {
            render_fan( pmesh, list[cnt] );
        }
    }
    else
    {
        for ( tx = TX_TILE_0; tx <= TX_TILE_3; tx++ )
        {
            meshlasttexture = tx;
            oglx_texture_Bind( TxTexture_get_ptr( tx ) );

            for ( cnt = 0; cnt < list_size; cnt++ )
            {
                render_fan( pmesh, list[cnt] );
            }
        }
    }

    return btrue;
}

//--------------------------------------------------------------------------------------------
void render_scene_init( ego_mpd * pmesh, ego_camera * pcam )
{

    PROFILE_BEGIN( renderlist_make );
    {
        // Which tiles can be displayed
        renderlist_make( pmesh, pcam );
    }
    PROFILE_END( renderlist_make );

    PROFILE_BEGIN( dolist_make );
    {
        // determine which objects are visible
        dolist_make( renderlist.pmesh );
    }
    PROFILE_END( dolist_make );

    // put off sorting the dolist until later
    // because it has to be sorted differently for reflected and non-reflected objects
    // dolist_sort( pcam, bfalse );

    PROFILE_BEGIN( do_grid_lighting );
    {
        // figure out the terrain lighting
        do_grid_lighting( renderlist.pmesh, pcam );
    }
    PROFILE_END( do_grid_lighting );

    PROFILE_BEGIN( light_fans );
    {
        // apply the lighting to the characters and particles
        light_fans( &renderlist );
    }
    PROFILE_END( light_fans );

    PROFILE_BEGIN( update_all_chr_instance );
    {
        // make sure the characters are ready to draw
        update_all_chr_instance();
    }
    PROFILE_END( update_all_chr_instance );

    PROFILE_BEGIN( update_all_prt_instance );
    {
        // make sure the particles are ready to draw
        update_all_prt_instance( pcam );
    }
    PROFILE_END( update_all_prt_instance );

    time_render_scene_init_renderlist_make         = PROFILE_QUERY( renderlist_make ) * TARGET_FPS;
    time_render_scene_init_dolist_make             = PROFILE_QUERY( dolist_make ) * TARGET_FPS;
    time_render_scene_init_do_grid_dynalight       = PROFILE_QUERY( do_grid_lighting ) * TARGET_FPS;
    time_render_scene_init_light_fans              = PROFILE_QUERY( light_fans ) * TARGET_FPS;
    time_render_scene_init_update_all_chr_instance = PROFILE_QUERY( update_all_chr_instance ) * TARGET_FPS;
    time_render_scene_init_update_all_prt_instance = PROFILE_QUERY( update_all_prt_instance ) * TARGET_FPS;
}

//--------------------------------------------------------------------------------------------
void render_scene_mesh( ego_renderlist * prlist )
{
    /// \author BB
    /// \details  draw the mesh and any reflected objects

    size_t      cnt;
    ego_sint    rcnt;
    ego_mpd * pmesh;

    if ( NULL == prlist ) return;

    if ( NULL == prlist->pmesh ) return;
    pmesh = prlist->pmesh;

    // advance the animation of all animated tiles
    animate_all_tiles( prlist->pmesh );

    PROFILE_BEGIN( render_scene_mesh_ndr );
    {
        //---------------------------------------------
        // draw all tiles that do not reflect characters
        ATTRIB_PUSH( "render_scene_mesh() - ndr", GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT );
        {
            GL_DEBUG( glDepthMask )( GL_TRUE );         // GL_DEPTH_BUFFER_BIT - store the surface depth

            GL_DEBUG( glEnable )( GL_DEPTH_TEST );      // GL_ENABLE_BIT - do not draw hidden surfaces
            GL_DEBUG( glDepthFunc )( GL_LEQUAL );       // GL_DEPTH_BUFFER_BIT

            GL_DEBUG( glDisable )( GL_BLEND );          // GL_ENABLE_BIT - no transparency
            GL_DEBUG( glDisable )( GL_CULL_FACE );      // GL_ENABLE_BIT - draw front and back of tiles

            GL_DEBUG( glEnable )( GL_ALPHA_TEST );      // GL_ENABLE_BIT - use alpha test to allow the thatched roof tiles to look like thatch
            GL_DEBUG( glAlphaFunc )( GL_GREATER, 0 );   // GL_COLOR_BUFFER_BIT - speed-up drawing of surfaces with alpha == 0 sections

            // reduce texture hashing by loading up each texture only once
            render_fans_by_list( pmesh, prlist->ndr, prlist->ndr_count );
        }
        ATTRIB_POP( "render_scene_mesh() - ndr" );
    }
    PROFILE_END( render_scene_mesh_ndr );

    //--------------------------------
    // draw the reflective tiles and any reflected objects
    if ( gfx.refon )
    {
        PROFILE_BEGIN( render_scene_mesh_drf_back );
        {
            //------------------------------
            // draw the reflective tiles, but turn off the depth buffer
            // this blanks out any background that might've been drawn

            ATTRIB_PUSH( "render_scene_mesh() - drf - back", GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT );
            {
                GL_DEBUG( glDepthMask )( GL_FALSE );        // GL_DEPTH_BUFFER_BIT - DO NOT store the surface depth

                GL_DEBUG( glEnable )( GL_DEPTH_TEST );      // GL_ENABLE_BIT - do not draw hidden surfaces
                GL_DEBUG( glDepthFunc )( GL_LEQUAL );       // GL_DEPTH_BUFFER_BIT

                // black out any backgound, but allow the background to show through any holes in the floor
                GL_DEBUG( glEnable )( GL_BLEND );                   // GL_ENABLE_BIT - allow transparent surfaces
                GL_DEBUG( glBlendFunc )( GL_ZERO, GL_ONE_MINUS_SRC_ALPHA );    // GL_COLOR_BUFFER_BIT - use the alpha channel to modulate the transparency

                GL_DEBUG( glEnable )( GL_ALPHA_TEST );      // GL_ENABLE_BIT - use alpha test to allow the thatched roof tiles to look like thatch
                GL_DEBUG( glAlphaFunc )( GL_GREATER, 0 );   // GL_COLOR_BUFFER_BIT - speed-up drawing of surfaces with alpha == 0 sections

                // reduce texture hashing by loading up each texture only once
                render_fans_by_list( pmesh, prlist->drf, prlist->drf_count );
            }
            ATTRIB_POP( "render_scene_mesh() - drf - back" );
        }
        PROFILE_END( render_scene_mesh_drf_back );

        PROFILE_BEGIN( render_scene_mesh_ref );
        {
            //------------------------------
            // Render all reflected objects
            ATTRIB_PUSH( "render_scene_mesh() - reflected characters",  GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_POLYGON_BIT | GL_CURRENT_BIT );
            {
                GL_DEBUG( glDepthMask )( GL_FALSE );      // GL_DEPTH_BUFFER_BIT - turn off the depth mask by default. Can cause glitches if used improperly.

                GL_DEBUG( glEnable )( GL_DEPTH_TEST );    // GL_ENABLE_BIT - do not draw hidden surfaces
                GL_DEBUG( glDepthFunc )( GL_LEQUAL );     // GL_DEPTH_BUFFER_BIT - surfaces must be closer to the camera to be drawn

                // this must be done with a signed iterator or it will fail horribly
                for ( rcnt = ego_sint( dolist_count ) - 1; rcnt >= 0; rcnt-- )
                {
                    if ( MAX_PRT == dolist[rcnt].iprt && INGAME_CHR( dolist[rcnt].ichr ) )
                    {
                        CHR_REF ichr;
                        Uint32 itile;

                        GL_DEBUG( glEnable )( GL_CULL_FACE );   // GL_ENABLE_BIT - cull backward facing faces
                        GL_DEBUG( glFrontFace )( GL_CCW );      // GL_POLYGON_BIT - use counter-clockwise orientation to determine backfaces

                        GL_DEBUG( glEnable )( GL_BLEND );                 // GL_ENABLE_BIT - allow transparent objects
                        GL_DEBUG( glBlendFunc )( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );  // GL_COLOR_BUFFER_BIT - use the alpha channel to modulate the transparency

                        ichr  = dolist[rcnt].ichr;
                        itile = ChrObjList.get_data_ref( ichr ).onwhichgrid;

                        if ( ego_mpd::grid_is_valid( pmesh, itile ) && ( 0 != ego_mpd::test_fx( pmesh, itile, MPDFX_DRAWREF ) ) )
                        {
                            GL_DEBUG( glColor4f )( 1.0f, 1.0f, 1.0f, 1.0f );          // GL_CURRENT_BIT
                            render_one_mad_ref( ichr );
                        }
                    }
                    else if ( MAX_CHR == dolist[rcnt].ichr && INGAME_PRT( dolist[rcnt].iprt ) )
                    {
                        Uint32 itile;
                        PRT_REF iprt;
                        iprt = dolist[rcnt].iprt;
                        itile = PrtObjList.get_data_ref( iprt ).onwhichgrid;

                        GL_DEBUG( glDisable )( GL_CULL_FACE );

                        // render_one_prt_ref() actually sets its own blend function, but just to be safe
                        GL_DEBUG( glEnable )( GL_BLEND );                    // GL_ENABLE_BIT - allow transparent objects
                        GL_DEBUG( glBlendFunc )( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );     // GL_COLOR_BUFFER_BIT - set the default particle blending

                        if ( ego_mpd::grid_is_valid( pmesh, itile ) && ( 0 != ego_mpd::test_fx( pmesh, itile, MPDFX_DRAWREF ) ) )
                        {
                            render_one_prt_ref( iprt );
                        }
                    }
                }

            }
            ATTRIB_POP( "render_scene_mesh() - reflected characters" )
        }
        PROFILE_END( render_scene_mesh_ref );

        PROFILE_BEGIN( render_scene_mesh_ref_chr );
        {
            //------------------------------
            // Render the shadow floors ( let everything show through )
            // turn on the depth mask, so that no objects under the floor will show through
            // this assumes that the floor is not partially transparent...
            ATTRIB_PUSH( "render_scene_mesh() - drf - front", GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT );
            {
                GL_DEBUG( glDepthMask )( GL_TRUE );                   // GL_DEPTH_BUFFER_BIT - set the depth of these tiles

                GL_DEBUG( glEnable )( GL_BLEND );                     // GL_ENABLE_BIT
                GL_DEBUG( glBlendFunc )( GL_SRC_ALPHA, GL_ONE );      // GL_COLOR_BUFFER_BIT

                GL_DEBUG( glDisable )( GL_CULL_FACE );                // GL_ENABLE_BIT - draw all faces

                GL_DEBUG( glEnable )( GL_DEPTH_TEST );                // GL_ENABLE_BIT - do not draw hidden surfaces
                GL_DEBUG( glDepthFunc )( GL_LEQUAL );                 // GL_DEPTH_BUFFER_BIT

                // reduce texture hashing by loading up each texture only once
                render_fans_by_list( pmesh, prlist->drf, prlist->drf_count );
            }
            ATTRIB_POP( "render_scene_mesh() - drf - front" )
        }
        PROFILE_END( render_scene_mesh_ref_chr );
    }
    else
    {
        PROFILE_BEGIN( render_scene_mesh_drf_solid );
        {
            //------------------------------
            // Render the shadow floors as normal solid floors

            ATTRIB_PUSH( "render_scene_mesh() - drf - solid", GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT );
            {
                GL_DEBUG( glDisable )( GL_BLEND );          // GL_ENABLE_BIT - no transparency
                GL_DEBUG( glDisable )( GL_CULL_FACE );      // GL_ENABLE_BIT - draw front and back of tiles

                GL_DEBUG( glEnable )( GL_DEPTH_TEST );      // GL_ENABLE_BIT - do not draw hidden surfaces
                GL_DEBUG( glDepthMask )( GL_TRUE );         // GL_DEPTH_BUFFER_BIT - store the surface depth

                GL_DEBUG( glEnable )( GL_ALPHA_TEST );      // GL_ENABLE_BIT - use alpha test to allow the thatched roof tiles to look like thatch
                GL_DEBUG( glAlphaFunc )( GL_GREATER, 0 );   // GL_COLOR_BUFFER_BIT - speed-up drawing of surfaces with alpha == 0 sections

                // reduce texture hashing by loading up each texture only once
                render_fans_by_list( pmesh, prlist->drf, prlist->drf_count );
            }
            ATTRIB_POP( "render_scene_mesh() - drf - solid" );
        }
        PROFILE_END( render_scene_mesh_drf_solid );
    }

#if defined(RENDER_HMAP) && EGO_DEBUG
    //------------------------------
    // render the heighmap
    for ( cnt = 0; cnt < prlist->all_count; cnt++ )
    {
        render_hmap_fan( pmesh, prlist->all[cnt] );
    }
#endif

    PROFILE_BEGIN( render_scene_mesh_render_shadows );
    {
        //------------------------------
        // Render the shadows
        if ( gfx.shaon )
        {
            GL_DEBUG( glDepthMask )( GL_FALSE );
            GL_DEBUG( glEnable )( GL_DEPTH_TEST );

            GL_DEBUG( glEnable )( GL_BLEND );
            GL_DEBUG( glBlendFunc )( GL_ZERO, GL_ONE_MINUS_SRC_COLOR );

            if ( gfx.shasprite )
            {
                // Bad shadows
                for ( cnt = 0; cnt < dolist_count; cnt++ )
                {
                    ego_obj_chr * pchr = ChrObjList.get_allocated_data_ptr( dolist[cnt].ichr );
                    if ( NULL == pchr || 0 == pchr->shadow_size ) continue;

                    render_bad_shadow( dolist[cnt].ichr );
                }
            }
            else
            {
                // Good shadows for me
                for ( cnt = 0; cnt < dolist_count; cnt++ )
                {
                    ego_obj_chr * pchr = ChrObjList.get_allocated_data_ptr( dolist[cnt].ichr );
                    if ( NULL == pchr || 0 == pchr->shadow_size ) continue;

                    render_shadow( dolist[cnt].ichr );
                }
            }
        }
    }
    PROFILE_END( render_scene_mesh_render_shadows );
}

//--------------------------------------------------------------------------------------------
void render_scene_solid()
{
    Uint32 cnt;

    //------------------------------
    // Render all solid objects
    for ( cnt = 0; cnt < dolist_count; cnt++ )
    {
        GL_DEBUG( glDepthMask )( GL_TRUE );

        GL_DEBUG( glEnable )( GL_DEPTH_TEST );
        GL_DEBUG( glDepthFunc )( GL_LEQUAL );

        GL_DEBUG( glEnable )( GL_ALPHA_TEST );
        GL_DEBUG( glAlphaFunc )( GL_GREATER, 0 );

        GL_DEBUG( glDisable )( GL_BLEND );

        GL_DEBUG( glDisable )( GL_CULL_FACE );

        if ( MAX_PRT == dolist[cnt].iprt )
        {
            GLXvector4f tint;
            gfx_mad_instance * pinst = ego_chr::get_pinstance( dolist[cnt].ichr );

            if ( NULL != pinst && pinst->alpha == 255 && pinst->light == 255 )
            {
                gfx_mad_instance::get_tint( pinst, tint, CHR_SOLID );

                render_one_mad( dolist[cnt].ichr, tint, CHR_SOLID );
            }
        }
        else if ( MAX_CHR == dolist[cnt].ichr && INGAME_PRT( dolist[cnt].iprt ) )
        {
            GL_DEBUG( glDisable )( GL_CULL_FACE );

            render_one_prt_solid( dolist[cnt].iprt );
        }
    }

    // Render the solid billboards
    render_all_billboards( PCamera );

    // daw some debugging lines
    draw_all_lines( PCamera );
}

//--------------------------------------------------------------------------------------------
void render_scene_trans()
{
    /// \author BB
    /// \details  draw transparent objects

    ego_sint    rcnt;
    GLXvector4f tint;

    // set the the transparency parameters
    GL_DEBUG( glDepthMask )( GL_FALSE );                   // GL_DEPTH_BUFFER_BIT

    GL_DEBUG( glEnable )( GL_DEPTH_TEST );                // GL_ENABLE_BIT
    GL_DEBUG( glDepthFunc )( GL_LEQUAL );                 // GL_DEPTH_BUFFER_BIT

    // Now render all transparent and light objects
    // this must be iterated with a signed variable or it fails horribly
    for ( rcnt = ego_sint( dolist_count ) - 1; rcnt >= 0; rcnt-- )
    {
        if ( MAX_PRT == dolist[rcnt].iprt && INGAME_CHR( dolist[rcnt].ichr ) )
        {
            CHR_REF  ichr = dolist[rcnt].ichr;
            ego_chr * pchr = ChrObjList.get_data_ptr( ichr );
            gfx_mad_instance * pinst = &( pchr->gfx_inst );

            GL_DEBUG( glEnable )( GL_CULL_FACE );         // GL_ENABLE_BIT
            GL_DEBUG( glFrontFace )( GL_CW );             // GL_POLYGON_BIT

            if ( pinst->alpha != 255 && pinst->light == 255 )
            {
                GL_DEBUG( glEnable )( GL_BLEND );                                     // GL_ENABLE_BIT
                GL_DEBUG( glBlendFunc )( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );      // GL_COLOR_BUFFER_BIT

                gfx_mad_instance::get_tint( pinst, tint, CHR_ALPHA );

                render_one_mad( ichr, tint, CHR_ALPHA );
            }

            if ( pinst->light != 255 )
            {
                GL_DEBUG( glEnable )( GL_BLEND );           // GL_ENABLE_BIT
                GL_DEBUG( glBlendFunc )( GL_ONE, GL_ONE );  // GL_COLOR_BUFFER_BIT

                gfx_mad_instance::get_tint( pinst, tint, CHR_LIGHT );

                render_one_mad( ichr, tint, CHR_LIGHT );
            }

            if ( gfx.phongon && pinst->sheen > 0 )
            {
                GL_DEBUG( glEnable )( GL_BLEND );             // GL_ENABLE_BIT
                GL_DEBUG( glBlendFunc )( GL_ONE, GL_ONE );    // GL_COLOR_BUFFER_BIT

                gfx_mad_instance::get_tint( pinst, tint, CHR_PHONG );

                render_one_mad( ichr, tint, CHR_PHONG );
            }
        }
        else if ( MAX_CHR == dolist[rcnt].ichr && INGAME_PRT( dolist[rcnt].iprt ) )
        {
            render_one_prt_trans( dolist[rcnt].iprt );
        }
    }
}

//--------------------------------------------------------------------------------------------
void render_scene( ego_mpd * pmesh, ego_camera * pcam )
{
    /// \author ZZ
    /// \details  This function draws 3D objects

    if ( NULL == pcam ) pcam = PCamera;
    if ( NULL == pmesh ) pmesh = PMesh;

    PROFILE_BEGIN( render_scene_init );
    {
        render_scene_init( pmesh, pcam );
    }
    PROFILE_END( render_scene_init );

    PROFILE_BEGIN( render_scene_mesh );
    {
        PROFILE_BEGIN( render_scene_mesh_dolist_sort );
        {
            // sort the dolist for reflected objects
            // reflected characters and objects are drawn in this pass
            dolist_sort( pcam, btrue );
        }
        PROFILE_END( render_scene_mesh_dolist_sort );

        // do the render pass for the mesh
        render_scene_mesh( &renderlist );

        time_render_scene_mesh_dolist_sort    = PROFILE_QUERY( render_scene_mesh_dolist_sort ) * TARGET_FPS;
        time_render_scene_mesh_ndr            = PROFILE_QUERY( render_scene_mesh_ndr ) * TARGET_FPS;
        time_render_scene_mesh_drf_back       = PROFILE_QUERY( render_scene_mesh_drf_back ) * TARGET_FPS;
        time_render_scene_mesh_ref            = PROFILE_QUERY( render_scene_mesh_ref ) * TARGET_FPS;
        time_render_scene_mesh_ref_chr        = PROFILE_QUERY( render_scene_mesh_ref_chr ) * TARGET_FPS;
        time_render_scene_mesh_drf_solid      = PROFILE_QUERY( render_scene_mesh_drf_solid ) * TARGET_FPS;
        time_render_scene_mesh_render_shadows = PROFILE_QUERY( render_scene_mesh_render_shadows ) * TARGET_FPS;
    }
    PROFILE_END( render_scene_mesh );

    PROFILE_BEGIN( render_scene_solid );
    {
        // sort the dolist for non-reflected objects
        dolist_sort( pcam, bfalse );

        // do the render pass for solid objects
        render_scene_solid();
    }
    PROFILE_END( render_scene_solid );

    PROFILE_BEGIN( render_scene_water );
    {
        // draw the water
        render_water( &renderlist );
    }
    PROFILE_END( render_scene_water );

    PROFILE_BEGIN( render_scene_trans );
    {
        // do the render pass for transparent objects
        render_scene_trans();
    }
    PROFILE_END( render_scene_trans );

//#if EGO_DEBUG
//    render_all_prt_attachment();
//#endif

#if EGO_DEBUG && defined(DEBUG_PRT_BBOX)
    render_all_prt_bbox();
#endif

    time_render_scene_init  = PROFILE_QUERY( render_scene_init ) * TARGET_FPS;
    time_render_scene_mesh  = PROFILE_QUERY( render_scene_mesh ) * TARGET_FPS;
    time_render_scene_solid = PROFILE_QUERY( render_scene_solid ) * TARGET_FPS;
    time_render_scene_water = PROFILE_QUERY( render_scene_water ) * TARGET_FPS;
    time_render_scene_trans = PROFILE_QUERY( render_scene_trans ) * TARGET_FPS;

    time_draw_scene = time_render_scene_init + time_render_scene_mesh + time_render_scene_solid + time_render_scene_water + time_render_scene_trans;
}

//--------------------------------------------------------------------------------------------
void render_world_background( const TX_REF & texture )
{
    /// \author ZZ
    /// \details  This function draws the large background
    ego_GLvertex vtlist[4];
    int i;
    float z0, Qx, Qy;
    float light = 1.0f, intens = 1.0f, alpha = 1.0f;

    float xmag, Cx_0, Cx_1;
    float ymag, Cy_0, Cy_1;

    ego_mpd_info * pinfo;
    ego_grid_mem     * pgmem;
    oglx_texture_t   * ptex;
    ego_water_layer_instance * ilayer;

    pinfo = &( PMesh->info );
    pgmem = &( PMesh->gmem );

    // which layer
    ilayer = water.layer + 0;

    // the "official" camera height
    z0 = 1500;

    // clip the waterlayer uv offset
    ilayer->tx.x = ilayer->tx.x - ( const float )FLOOR( ilayer->tx.x );
    ilayer->tx.y = ilayer->tx.y - ( const float )FLOOR( ilayer->tx.y );

    // determine the constants for the x-coordinate
    xmag = water.backgroundrepeat / 4 / ( 1.0f + z0 * ilayer->dist.x ) / GRID_SIZE;
    Cx_0 = xmag * ( 1.0f +  PCamera->pos.z       * ilayer->dist.x );
    Cx_1 = -xmag * ( 1.0f + ( PCamera->pos.z - z0 ) * ilayer->dist.x );

    // determine the constants for the y-coordinate
    ymag = water.backgroundrepeat / 4 / ( 1.0f + z0 * ilayer->dist.y ) / GRID_SIZE;
    Cy_0 = ymag * ( 1.0f +  PCamera->pos.z       * ilayer->dist.y );
    Cy_1 = -ymag * ( 1.0f + ( PCamera->pos.z - z0 ) * ilayer->dist.y );

    // Figure out the coordinates of its corners
    Qx = -pgmem->edge_x;
    Qy = -pgmem->edge_y;
    vtlist[0].pos[XX] = Qx;
    vtlist[0].pos[YY] = Qy;
    vtlist[0].pos[ZZ] = PCamera->pos.z - z0;
    vtlist[0].tex[SS] = Cx_0 * Qx + Cx_1 * PCamera->pos.x + ilayer->tx.x;
    vtlist[0].tex[TT] = Cy_0 * Qy + Cy_1 * PCamera->pos.y + ilayer->tx.y;

    Qx = 2 * pgmem->edge_x;
    Qy = -pgmem->edge_y;
    vtlist[1].pos[XX] = Qx;
    vtlist[1].pos[YY] = Qy;
    vtlist[1].pos[ZZ] = PCamera->pos.z - z0;
    vtlist[1].tex[SS] = Cx_0 * Qx + Cx_1 * PCamera->pos.x + ilayer->tx.x;
    vtlist[1].tex[TT] = Cy_0 * Qy + Cy_1 * PCamera->pos.y + ilayer->tx.y;

    Qx = 2 * pgmem->edge_x;
    Qy = 2 * pgmem->edge_y;
    vtlist[2].pos[XX] = Qx;
    vtlist[2].pos[YY] = Qy;
    vtlist[2].pos[ZZ] = PCamera->pos.z - z0;
    vtlist[2].tex[SS] = Cx_0 * Qx + Cx_1 * PCamera->pos.x + ilayer->tx.x;
    vtlist[2].tex[TT] = Cy_0 * Qy + Cy_1 * PCamera->pos.y + ilayer->tx.y;

    Qx = -pgmem->edge_x;
    Qy = 2 * pgmem->edge_y;
    vtlist[3].pos[XX] = Qx;
    vtlist[3].pos[YY] = Qy;
    vtlist[3].pos[ZZ] = PCamera->pos.z - z0;
    vtlist[3].tex[SS] = Cx_0 * Qx + Cx_1 * PCamera->pos.x + ilayer->tx.x;
    vtlist[3].tex[TT] = Cy_0 * Qy + Cy_1 * PCamera->pos.y + ilayer->tx.y;

    light = water.light ? 1.0f : 0.0f;
    alpha = ilayer->alpha * INV_FF;

    if ( gfx.usefaredge )
    {
        float fcos;

        intens = light_a * ilayer->light_add;

        fcos = light_nrm[kZ];
        if ( fcos > 0.0f )
        {
            intens += fcos * fcos * light_d * ilayer->light_dir;
        }

        intens = CLIP( intens, 0.0f, 1.0f );
    }

    ptex = TxTexture_get_ptr( texture );

    oglx_texture_Bind( ptex );

    ATTRIB_PUSH( "render_world_background()", GL_LIGHTING_BIT | GL_DEPTH_BUFFER_BIT | GL_ENABLE_BIT );
    {
        GL_DEBUG( glShadeModel )( GL_FLAT );      // GL_LIGHTING_BIT - Flat shade this
        GL_DEBUG( glDepthMask )( GL_FALSE );      // GL_DEPTH_BUFFER_BIT
        GL_DEBUG( glDepthFunc )( GL_ALWAYS );     // GL_DEPTH_BUFFER_BIT
        GL_DEBUG( glDisable )( GL_CULL_FACE );    // GL_ENABLE_BIT

        if ( alpha > 0.0f )
        {
            ATTRIB_PUSH( "render_world_background() - alpha", GL_ENABLE_BIT | GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT );
            {
                GL_DEBUG( glColor4f )( intens, intens, intens, alpha );             // GL_CURRENT_BIT

                if ( alpha >= 1.0f )
                {
                    GL_DEBUG( glDisable )( GL_BLEND );                               // GL_ENABLE_BIT
                }
                else
                {
                    GL_DEBUG( glEnable )( GL_BLEND );                               // GL_ENABLE_BIT
                    GL_DEBUG( glBlendFunc )( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );  // GL_COLOR_BUFFER_BIT
                }

                GL_DEBUG_BEGIN( GL_TRIANGLE_FAN );
                {
                    for ( i = 0; i < 4; i++ )
                    {
                        GL_DEBUG( glTexCoord2fv )( vtlist[i].tex );
                        GL_DEBUG( glVertex3fv )( vtlist[i].pos );
                    }
                }
                GL_DEBUG_END();
            }
            ATTRIB_POP( "render_world_background() - alpha" );
        }

        if ( light > 0.0f )
        {
            ATTRIB_PUSH( "render_world_background() - glow", GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT );
            {
                GL_DEBUG( glDisable )( GL_BLEND );                           // GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT
                GL_DEBUG( glBlendFunc )( GL_SRC_ALPHA, GL_ONE );            // GL_COLOR_BUFFER_BIT

                GL_DEBUG( glColor4f )( light, light, light, 1.0f );         // GL_CURRENT_BIT

                GL_DEBUG_BEGIN( GL_TRIANGLE_FAN );
                {
                    for ( i = 0; i < 4; i++ )
                    {
                        GL_DEBUG( glTexCoord2fv )( vtlist[i].tex );
                        GL_DEBUG( glVertex3fv )( vtlist[i].pos );
                    }
                }
                GL_DEBUG_END();
            }
            ATTRIB_POP( "render_world_background() - glow" );
        }
    }
    ATTRIB_POP( "render_world_background()" );
}

//--------------------------------------------------------------------------------------------
void render_world_overlay( const TX_REF & texture )
{
    /// \author ZZ
    /// \details  This function draws the large foreground

    float alpha, ftmp;
    fvec3_t   vforw_wind, vforw_cam;

    oglx_texture_t           * ptex;

    ego_water_layer_instance * ilayer = water.layer + 1;

    vforw_wind.x = ilayer->tx_add.x;
    vforw_wind.y = ilayer->tx_add.y;
    vforw_wind.z = 0;
    fvec3_self_normalize( vforw_wind.v );

    vforw_cam = mat_getCamForward( PCamera->mView );
    fvec3_self_normalize( vforw_cam.v );

    // make the texture begin to disappear if you are not looking straight down
    ftmp = fvec3_dot_product( vforw_wind.v, vforw_cam.v );

    alpha = ( 1.0f - ftmp * ftmp ) * ( ilayer->alpha * INV_FF );

    if ( alpha != 0.0f )
    {
        ego_GLvertex vtlist[4];
        int i;
        float size;
        float sinsize, cossize;
        float x, y, z;
        float loc_foregroundrepeat;

        // Figure out the screen coordinates of its corners
        x = sdl_scr.x << 6;
        y = sdl_scr.y << 6;
        z = 0;
        size = x + y + 1;
        sinsize = turntosin[( 3*2047 ) & TRIG_TABLE_MASK] * size;
        cossize = turntocos[( 3*2047 ) & TRIG_TABLE_MASK] * size;
        loc_foregroundrepeat = water.foregroundrepeat * SDL_min( x / sdl_scr.x, y / sdl_scr.x );

        vtlist[0].pos[XX] = x + cossize;
        vtlist[0].pos[YY] = y - sinsize;
        vtlist[0].pos[ZZ] = z;
        vtlist[0].tex[SS] = ilayer->tx.x;
        vtlist[0].tex[TT] = ilayer->tx.y;

        vtlist[1].pos[XX] = x + sinsize;
        vtlist[1].pos[YY] = y + cossize;
        vtlist[1].pos[ZZ] = z;
        vtlist[1].tex[SS] = ilayer->tx.x + loc_foregroundrepeat;
        vtlist[1].tex[TT] = ilayer->tx.y;

        vtlist[2].pos[XX] = x - cossize;
        vtlist[2].pos[YY] = y + sinsize;
        vtlist[2].pos[ZZ] = z;
        vtlist[2].tex[SS] = ilayer->tx.x + loc_foregroundrepeat;
        vtlist[2].tex[TT] = ilayer->tx.y + loc_foregroundrepeat;

        vtlist[3].pos[XX] = x - sinsize;
        vtlist[3].pos[YY] = y - cossize;
        vtlist[3].pos[ZZ] = z;
        vtlist[3].tex[SS] = ilayer->tx.x;
        vtlist[3].tex[TT] = ilayer->tx.y + loc_foregroundrepeat;

        ptex = TxTexture_get_ptr( texture );

        ATTRIB_PUSH( "render_world_overlay()", GL_ENABLE_BIT | GL_LIGHTING_BIT | GL_DEPTH_BUFFER_BIT | GL_POLYGON_BIT | GL_COLOR_BUFFER_BIT );
        {
            GL_DEBUG( glHint )( GL_POLYGON_SMOOTH_HINT, GL_NICEST );          // GL_HINT_BIT - make sure that the texture is as smooth as possible

            GL_DEBUG( glShadeModel )( GL_FLAT );    // GL_LIGHTING_BIT - Flat shade this

            GL_DEBUG( glDepthMask )( GL_FALSE );    // GL_DEPTH_BUFFER_BIT
            GL_DEBUG( glDepthFunc )( GL_ALWAYS );   // GL_DEPTH_BUFFER_BIT

            GL_DEBUG( glDisable )( GL_CULL_FACE );  // GL_ENABLE_BIT

            GL_DEBUG( glEnable )( GL_ALPHA_TEST );  // GL_COLOR_BUFFER_BIT | GL_ENABLE_BIT
            GL_DEBUG( glAlphaFunc )( GL_GREATER, 0 ); // GL_COLOR_BUFFER_BIT

            GL_DEBUG( glEnable )( GL_BLEND );                               // GL_COLOR_BUFFER_BIT | GL_ENABLE_BIT
            GL_DEBUG( glBlendFunc )( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR );  // GL_COLOR_BUFFER_BIT - make the texture a filter

            oglx_texture_Bind( ptex );

            GL_DEBUG( glColor4f )( 1.0f, 1.0f, 1.0f, 1.0f - SDL_abs( alpha ) );
            GL_DEBUG_BEGIN( GL_TRIANGLE_FAN );
            for ( i = 0; i < 4; i++ )
            {
                GL_DEBUG( glTexCoord2fv )( vtlist[i].tex );
                GL_DEBUG( glVertex3fv )( vtlist[i].pos );
            }
            GL_DEBUG_END();
        }
        ATTRIB_POP( "render_world_overlay()" );
    }
}

//--------------------------------------------------------------------------------------------
void render_world( ego_camera * pcam )
{
    gfx_begin_3d( pcam );
    {
        if ( gfx.draw_background )
        {
            // Render the background
            render_world_background( TX_REF( TX_WATER_LOW ) ); // TX_WATER_LOW for waterlow.bmp
        }

        render_scene( PMesh, pcam );

        if ( gfx.draw_overlay )
        {
            // Foreground overlay
            render_world_overlay( TX_REF( TX_WATER_TOP ) ); // TX_WATER_TOP is watertop.bmp
        }

        if ( pcam->motion_blur > 0 )
        {
            //Do motion blur
            GL_DEBUG( glAccum )( GL_MULT, pcam->motion_blur );
            GL_DEBUG( glAccum )( GL_ACCUM, 1.00f - pcam->motion_blur );
            GL_DEBUG( glAccum )( GL_RETURN, 1.0f );
        }
    }
    gfx_end_3d();
}

//--------------------------------------------------------------------------------------------
void gfx_main()
{
    /// \author ZZ
    /// \details  This function does all the drawing stuff

    render_world( PCamera );
    draw_text();

    request_flip_pages();
}

//--------------------------------------------------------------------------------------------
// UTILITY FUNCTIONS
//--------------------------------------------------------------------------------------------
bool_t dump_screenshot()
{
    /// \author BB
    /// \details  dumps the current screen (GL context) to a new bitmap file
    /// right now it dumps it to whatever the current directory is

    // returns btrue if successful, bfalse otherwise

    int i;
    bool_t savefound = bfalse;
    bool_t saved     = bfalse;
    STRING szFilename, szResolvedFilename;

    // find a valid file name
    savefound = bfalse;
    i = 0;
    while ( !savefound && ( i < 100 ) )
    {
        SDL_snprintf( szFilename, SDL_arraysize( szFilename ), "ego%02d.bmp", i );

        // lame way of checking if the file already exists...
        savefound = !vfs_exists( szFilename );
        if ( !savefound )
        {
            i++;
        }
    }

    if ( !savefound ) return bfalse;

    // convert the file path to the correct write path
    strncpy( szResolvedFilename, vfs_resolveWriteFilename( szFilename ), sizeof( szFilename ) );

    // if we are not using OpenGL, use SDL to dump the screen
    if ( HAS_NO_BITS( sdl_scr.pscreen->flags, SDL_OPENGL ) )
    {
        SDL_SaveBMP( sdl_scr.pscreen, szResolvedFilename );
        return bfalse;
    }

    EGOBOO_ASSERT( !oglx_NewList_active );

    // we ARE using OpenGL
    GL_DEBUG( glPushClientAttrib )( GL_CLIENT_PIXEL_STORE_BIT ) ;
    {
        SDL_Surface *temp;

        // create a SDL surface
        temp = SDL_CreateRGBSurface( SDL_SWSURFACE, sdl_scr.x, sdl_scr.y, 24, sdl_r_mask, sdl_g_mask, sdl_b_mask, 0 );

        if ( NULL == temp )
        {
            // Something went wrong
            SDL_FreeSurface( temp );
            return bfalse;
        }

        // Now lock the surface so that we can read it
        if ( -1 != SDL_LockSurface( temp ) )
        {
            SDL_Rect rect;

            SDL_memcpy( &rect, &( sdl_scr.pscreen->clip_rect ), sizeof( SDL_Rect ) );
            if ( 0 == rect.w && 0 == rect.h )
            {
                rect.w = sdl_scr.x;
                rect.h = sdl_scr.y;
            }
            if ( rect.w > 0 && rect.h > 0 )
            {
                int y;
                Uint8 * pixels;

                //GL_DEBUG( glGetError )();

                //// use the allocated screen to tell OpenGL about the row length (including the lapse) in pixels
                //// stolen from SDL ;)
                // GL_DEBUG(glPixelStorei)(GL_UNPACK_ROW_LENGTH, temp->pitch / temp->format->BytesPerPixel );
                // EGOBOO_ASSERT( GL_NO_ERROR == GL_DEBUG(glGetError)() );

                //// since we have specified the row actual length and will give a pointer to the actual pixel buffer,
                //// it is not necesssaty to mess with the alignment
                // GL_DEBUG(glPixelStorei)(GL_UNPACK_ALIGNMENT, 1 );
                // EGOBOO_ASSERT( GL_NO_ERROR == GL_DEBUG(glGetError)() );

                // ARGH! Must copy the pixels row-by-row, since the OpenGL video memory is flipped vertically
                // relative to the SDL Screen memory

                // this is supposed to be a DirectX thing, so it needs to be tested out on glx
                // there should probably be [SCREENSHOT_INVERT] and [SCREENSHOT_VALID] keys in setup.txt
                pixels = ( Uint8 * )temp->pixels;
                for ( y = rect.y; y < rect.y + rect.h; y++ )
                {
                    GL_DEBUG( glReadPixels )( rect.x, ( rect.h - y ) - 1, rect.w, 1, GL_RGB, GL_UNSIGNED_BYTE, pixels );
                    pixels += temp->pitch;
                }
                //EGOBOO_ASSERT( GL_NO_ERROR == GL_DEBUG( glGetError )() );
            }

            SDL_UnlockSurface( temp );

            // Save the file as a .bmp
            saved = ( -1 != SDL_SaveBMP( temp, szResolvedFilename ) );
        }

        // free the SDL surface
        SDL_FreeSurface( temp );
        if ( saved )
        {
            // tell the user what we did
            debug_printf( "Saved to %s", szFilename );
        }
    }
    GL_DEBUG( glPopClientAttrib )();

    return savefound && saved;
}

//--------------------------------------------------------------------------------------------
void clear_messages()
{
    /// \author ZZ
    /// \details  This function empties the message buffer
    int cnt;

    cnt = 0;

    while ( cnt < MAX_MESSAGE )
    {
        DisplayMsg.ary[cnt].time = 0;
        cnt++;
    }
}

//--------------------------------------------------------------------------------------------
float calc_light_rotation( const int rotation, const int normal )
{
    /// \author ZZ
    /// \details  This function helps make_lighttable
    fvec3_t   nrm, nrm2;
    float sinrot, cosrot;

    nrm.x = ego_MD2_Model::kNormals[normal][0];
    nrm.y = ego_MD2_Model::kNormals[normal][1];
    nrm.z = ego_MD2_Model::kNormals[normal][2];

    sinrot = sinlut[rotation];
    cosrot = coslut[rotation];

    nrm2.x = cosrot * nrm.x + sinrot * nrm.y;
    nrm2.y = cosrot * nrm.y - sinrot * nrm.x;
    nrm2.z = nrm.z;

    return ( nrm2.x < 0 ) ? 0 : ( nrm2.x * nrm2.x );
}

//--------------------------------------------------------------------------------------------
float calc_light_global( const int rotation, const int normal, const float lx, const float ly, const float lz )
{
    /// \author ZZ
    /// \details  This function helps make_lighttable
    float fTmp;
    fvec3_t   nrm, nrm2;
    float sinrot, cosrot;

    nrm.x = ego_MD2_Model::kNormals[normal][0];
    nrm.y = ego_MD2_Model::kNormals[normal][1];
    nrm.z = ego_MD2_Model::kNormals[normal][2];

    sinrot = sinlut[rotation];
    cosrot = coslut[rotation];

    nrm2.x = cosrot * nrm.x + sinrot * nrm.y;
    nrm2.y = cosrot * nrm.y - sinrot * nrm.x;
    nrm2.z = nrm.z;

    fTmp = nrm2.x * lx + nrm2.y * ly + nrm2.z * lz;
    if ( fTmp < 0 ) fTmp = 0;

    return fTmp * fTmp;
}

//--------------------------------------------------------------------------------------------
void make_enviro( void )
{
    /// \author ZZ
    /// \details  This function sets up the environment mapping table
    int cnt;
    float x, y, z;

    // Find the environment map positions
    for ( cnt = 0; cnt < EGO_NORMAL_COUNT; cnt++ )
    {
        x = ego_MD2_Model::kNormals[cnt][0];
        y = ego_MD2_Model::kNormals[cnt][1];
        indextoenvirox[cnt] = ATAN2( y, x ) / TWO_PI;
    }

    for ( cnt = 0; cnt < 256; cnt++ )
    {
        z = cnt / 255.0f;  // Z is between 0 and 1
        lighttoenviroy[cnt] = z;
    }
}

//--------------------------------------------------------------------------------------------
float grid_lighting_test( const ego_mpd * pmesh, GLXvector3f pos, float * low_diff, float * hgh_diff )
{
    int ix, iy, cnt;
    Uint32 fan[4];
    float u, v;

    ego_grid_info  * glist;
    ego_lighting_cache * cache_list[4];

    if ( NULL == pmesh ) return bfalse;
    glist = pmesh->gmem.grid_list;

    ix = FLOOR( pos[XX] / GRID_SIZE );
    iy = FLOOR( pos[YY] / GRID_SIZE );

    fan[0] = ego_mpd::get_tile_int( pmesh, ix,     iy );
    fan[1] = ego_mpd::get_tile_int( pmesh, ix + 1, iy );
    fan[2] = ego_mpd::get_tile_int( pmesh, ix,     iy + 1 );
    fan[3] = ego_mpd::get_tile_int( pmesh, ix + 1, iy + 1 );

    for ( cnt = 0; cnt < 4; cnt++ )
    {
        cache_list[cnt] = NULL;
        if ( ego_mpd::grid_is_valid( pmesh, fan[cnt] ) )
        {
            cache_list[cnt] = &( glist[fan[cnt]].cache );
        }
    }

    u = pos[XX] / GRID_SIZE - ix;
    v = pos[YY] / GRID_SIZE - iy;

    return lighting_cache_test( cache_list, u, v, low_diff, hgh_diff );
}

//--------------------------------------------------------------------------------------------
bool_t grid_lighting_interpolate( ego_mpd * pmesh, ego_lighting_cache * dst, const float fx, const float fy )
{
    int ix, iy, cnt;
    Uint32 fan[4];
    float u, v;

    ego_grid_info  * glist;
    ego_lighting_cache * cache_list[4];

    if ( NULL == pmesh ) return bfalse;
    glist = pmesh->gmem.grid_list;

    ix = FLOOR( fx / GRID_SIZE );
    iy = FLOOR( fy / GRID_SIZE );

    fan[0] = ego_mpd::get_tile_int( pmesh, ix,     iy );
    fan[1] = ego_mpd::get_tile_int( pmesh, ix + 1, iy );
    fan[2] = ego_mpd::get_tile_int( pmesh, ix,     iy + 1 );
    fan[3] = ego_mpd::get_tile_int( pmesh, ix + 1, iy + 1 );

    for ( cnt = 0; cnt < 4; cnt++ )
    {
        cache_list[cnt] = NULL;
        if ( ego_mpd::grid_is_valid( pmesh, fan[cnt] ) )
        {
            cache_list[cnt] = &( glist[fan[cnt]].cache );
        }
    }

    u = fx / GRID_SIZE - ix;
    v = fy / GRID_SIZE - iy;

    return lighting_cache_interpolate( dst, cache_list, u, v );
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
void gfx_update_timers()
{
    /// \author ZZ
    /// \details  This function updates the graphics timers

    // the definition of the low-pass filter coefficients
    const float fold = 0.2f;
    const float fnew = 1.0f - fold;

    // state variables
    static bool_t mnu_on_old = bfalse, game_on_old = bfalse;

    // variables to store the raw number of ticks
    static Uint32 ego_ticks_old = 0;
    static Uint32 ego_ticks_new = 0;
    Sint32 ego_ticks_diff = 0;

    // grab the current number of ticks
    ego_ticks_old  = ego_ticks_new;
    ego_ticks_new  = egoboo_get_ticks();
    ego_ticks_diff = ego_ticks_new - ego_ticks_old;

    // do nothing if the there is no process running which can generate frames
    if (( !MProc->running() && !GProc->running() ) || !EProc->running() )
    {
        mnu_on_old  = bfalse;
        game_on_old = bfalse;
        return;
    }

    // detect a transition in the game or menu state
    if (( MProc->running() != mnu_on_old ) || ( GProc->running() != game_on_old ) )
    {
        fps_clk.initialized = bfalse;
    }

    // save the old process states
    mnu_on_old  = rv_success == MProc->running();
    game_on_old = rv_success == GProc->running();

    // determine how many frames and game updates there have been since the last time
    fps_clk.update_counters();

    // determine the amount of ticks for various conditions
    if ( !fps_clk.initialized )
    {
        fps_clk.init();
        ego_ticks_diff = 0;
    }
    else if ( single_update_mode )
    {
        ego_ticks_diff = FRAME_SKIP * fps_clk.frame_dif;
    }

    // make sure that there is at least one tick since the last function call
    if ( 0 == ego_ticks_diff ) return;

    // increment the fps_clk's ticks
    fps_clk.update_ticks( ego_ticks_diff );

    if ( fps_clk.frame_cnt > 0 && fps_clk.tick_cnt > 0 )
    {
        stabilized_fps_sum    = fold * stabilized_fps_sum    + fnew * ( const float ) fps_clk.frame_cnt / (( const float ) fps_clk.tick_cnt / TICKS_PER_SEC );
        stabilized_fps_weight = fold * stabilized_fps_weight + fnew;

        // blank these every so often (once every 10 seconds?) so that the numbers don't overflow
        if ( fps_clk.frame_cnt > 10 * TARGET_FPS )
        {

            fps_clk.blank();
        }
    };

    if ( stabilized_fps_weight > 0.0f )
    {
        stabilized_fps = stabilized_fps_sum / stabilized_fps_weight;
    }
}

//--------------------------------------------------------------------------------------------
// BILLBOARD DATA IMPLEMENTATION
//--------------------------------------------------------------------------------------------
ego_billboard_data * ego_billboard_data::clear( ego_billboard_data * pbb )
{
    if ( NULL == pbb ) return pbb;

    SDL_memset( pbb, 0, sizeof( *pbb ) );

    return pbb;
}

//--------------------------------------------------------------------------------------------
ego_billboard_data * ego_billboard_data::init( ego_billboard_data * pbb )
{
    if ( NULL == pbb ) return pbb;

    pbb = ego_billboard_data::clear( pbb );
    if ( NULL == pbb ) return pbb;

    pbb->tex_ref = INVALID_TX_TEXTURE;
    pbb->ichr    = CHR_REF( MAX_CHR );

    pbb->tint[RR] = pbb->tint[GG] = pbb->tint[BB] = pbb->tint[AA] = 1.0f;
    pbb->tint_add[AA] -= 1.0f / 100.0f;

    pbb->size = 1.0f;
    pbb->size_add -= 1.0f / 200.0f;

    pbb->offset_add[ZZ] += 127 / 50.0f * 2.0f;

    return pbb;
}

//--------------------------------------------------------------------------------------------
bool_t ego_billboard_data::dealloc( ego_billboard_data * pbb )
{
    if ( NULL == pbb || !pbb->valid ) return bfalse;

    // free any allocated texture
    TxTexture_free_one( pbb->tex_ref );

    ego_billboard_data::init( pbb );

    return btrue;
}

//--------------------------------------------------------------------------------------------
bool_t ego_billboard_data::update( ego_billboard_data * pbb )
{
    fvec3_t     vup, pos_new;
    ego_chr     * pchr;
    float       height, offset;

    if ( NULL == pbb || !pbb->valid ) return bfalse;

    if ( !INGAME_CHR( pbb->ichr ) ) return bfalse;
    pchr = ChrObjList.get_data_ptr( pbb->ichr );

    // determine where the new position should be
    ego_chr::get_MatUp( pchr, &vup );

    height = pchr->bump.height;
    offset = SDL_min( pchr->bump_1.height * 0.5f, pchr->bump_1.size );

    pos_new.x = pchr->pos.x + vup.x * ( height + offset );
    pos_new.y = pchr->pos.y + vup.y * ( height + offset );
    pos_new.z = pchr->pos.z + vup.z * ( height + offset );

    // allow the billboards to be a bit bouncy
    pbb->pos.x = pbb->pos.x * 0.5f + pos_new.x * 0.5f;
    pbb->pos.y = pbb->pos.y * 0.5f + pos_new.y * 0.5f;
    pbb->pos.z = pbb->pos.z * 0.5f + pos_new.z * 0.5f;

    pbb->size += pbb->size_add;

    pbb->tint[RR] += pbb->tint_add[RR];
    pbb->tint[GG] += pbb->tint_add[GG];
    pbb->tint[BB] += pbb->tint_add[BB];
    pbb->tint[AA] += pbb->tint_add[AA];

    pbb->offset[XX] += pbb->offset_add[XX];
    pbb->offset[YY] += pbb->offset_add[YY];
    pbb->offset[ZZ] += pbb->offset_add[ZZ];

    // automatically kill a billboard that is no longer useful
    if ( pbb->tint[AA] == 0.0f || pbb->size == 0.0f )
    {
        ego_billboard_data::dealloc( pbb );
    }

    return btrue;
}

//--------------------------------------------------------------------------------------------
bool_t ego_billboard_data::printf_ttf( ego_billboard_data * pbb, TTF_Font *font, SDL_Color color, const char * format, ... )
{
    va_list          args      = NULL;
    oglx_texture_t * oglx_ptex = NULL;
    display_item_t  * fnt_ptex  = NULL;
    frect_t        * prect     = NULL;

    // check for bad input data
    if ( NULL == pbb || !pbb->valid ) return bfalse;

    // release any existing texture in case there is an error
    oglx_ptex = TxTexture_get_ptr( pbb->tex_ref );
    oglx_texture_Release( oglx_ptex );

    // pring the text to an ogl texture
    va_start( args, format );
    fnt_ptex = fnt_vprintf( fnt_ptex, font, color, format, args );
    va_end( args );

    // if there was an error, quit
    if ( NULL == fnt_ptex ) return bfalse;

    prect = display_item_prect( fnt_ptex );

    // set some basic texture parameters
    oglx_ptex->base.binding = display_item_texture_name( fnt_ptex );
    oglx_ptex->base_valid   = bfalse;
    strncpy( oglx_ptex->name, "billboard text", SDL_arraysize( oglx_ptex->name ) );

    // grab a bunch of info from ogl about the texture
    oglx_grab_texture_state( GL_TEXTURE_2D, 0, oglx_ptex );
    oglx_ptex->alpha = 1.0f;
    oglx_ptex->imgW  = prect->w;
    oglx_ptex->imgH  = prect->h;

    // transfer ownership of the fnt_ptex's resources to oglx_ptex.
    // now, freeing fnt_ptex won't get rid of the resource
    display_item_release_ownership( fnt_ptex );

    // we own this pointer, so delete it completely
    fnt_ptex = display_item_free( fnt_ptex, btrue );

    return btrue;
}

//--------------------------------------------------------------------------------------------
// BILLBOARD IMPLEMENTATION
//--------------------------------------------------------------------------------------------
void BillboardList_clear_data()
{
    /// \author BB
    /// \details  reset the free billboard list.

    int cnt;

    for ( cnt = 0; cnt < BILLBOARD_COUNT; cnt++ )
    {
        BillboardList.free_ref[cnt] = cnt;
    }
    BillboardList._free_count = cnt;
}

//--------------------------------------------------------------------------------------------
void BillboardList_init_all()
{
    BBOARD_REF cnt;

    for ( cnt = 0; cnt < BILLBOARD_COUNT; cnt++ )
    {
        ego_billboard_data::init( BillboardList.lst + cnt );
    }

    BillboardList_clear_data();
}

//--------------------------------------------------------------------------------------------
void BillboardList_update_all()
{
    BBOARD_REF cnt;
    Uint32     ticks;

    ticks = egoboo_get_ticks();

    for ( cnt = 0; cnt < BILLBOARD_COUNT; cnt++ )
    {
        bool_t is_invalid;

        ego_billboard_data * pbb = BillboardList.lst + cnt;

        if ( !pbb->valid ) continue;

        is_invalid = bfalse;
        if ( ticks >= pbb->time || NULL == TxTexture_get_ptr( pbb->tex_ref ) )
        {
            is_invalid = btrue;
        }

        if ( !INGAME_CHR( pbb->ichr ) || IS_ATTACHED_PCHR( ChrObjList.get_data_ptr( pbb->ichr ) ) )
        {
            is_invalid = btrue;
        }

        if ( is_invalid )
        {
            // the billboard has expired

            // unlink it from the character
            if ( INGAME_CHR( pbb->ichr ) )
            {
                ChrObjList.get_data_ref( pbb->ichr ).ibillboard = INVALID_BILLBOARD;
            }

            // deallocate the billboard
            BillboardList_free_one( cnt.get_value() );
        }
        else
        {
            ego_billboard_data::update( BillboardList.lst + cnt );
        }
    }
}

//--------------------------------------------------------------------------------------------
void BillboardList_free_all()
{
    BBOARD_REF cnt;

    for ( cnt = 0; cnt < BILLBOARD_COUNT; cnt++ )
    {
        if ( !BillboardList.lst[cnt].valid ) continue;

        ego_billboard_data::update( BillboardList.lst + cnt );
    }
}

//--------------------------------------------------------------------------------------------
size_t BillboardList_get_free( const Uint32 lifetime_secs )
{
    TX_REF             itex = TX_REF( INVALID_TX_TEXTURE );
    size_t             ibb  = INVALID_BILLBOARD;
    ego_billboard_data * pbb  = NULL;

    if ( BillboardList._free_count <= 0 ) return INVALID_BILLBOARD;

    if ( 0 == lifetime_secs ) return INVALID_BILLBOARD;

    itex = TxTexture_get_free( TX_REF( INVALID_TX_TEXTURE ) );
    if ( INVALID_TX_TEXTURE == itex ) return INVALID_BILLBOARD;

    // grab the top index
    BillboardList._free_count--;
    BillboardList.update_guid++;

    ibb = BillboardList.free_ref[BillboardList._free_count];

    if ( VALID_BILLBOARD_RANGE( ibb ) )
    {
        pbb = BillboardList.lst + BBOARD_REF( ibb );

        ego_billboard_data::init( pbb );

        pbb->tex_ref = itex;
        pbb->time    = egoboo_get_ticks() + lifetime_secs * TICKS_PER_SEC;
        pbb->valid   = btrue;
    }
    else
    {
        // the billboard allocation returned an ivaild value
        // deallocate the texture
        TxTexture_free_one( itex );

        ibb = INVALID_BILLBOARD;
    }

    return ibb;
}

//--------------------------------------------------------------------------------------------
bool_t BillboardList_free_one( const size_t ibb )
{
    ego_billboard_data * pbb;

    if ( !VALID_BILLBOARD_RANGE( ibb ) ) return bfalse;
    pbb = BillboardList.lst + BBOARD_REF( ibb );

    ego_billboard_data::dealloc( pbb );

#if EGO_DEBUG && defined(DEBUG_LIST)
    {
        size_t cnt;

        // determine whether this texture is already in the list of free textures
        // that is an error
        for ( cnt = 0; cnt < BillboardList._free_count; cnt++ )
        {
            if ( ibb == BillboardList.free_ref[cnt] ) return bfalse;
        }
    }
#endif

    if ( BillboardList._free_count >= BILLBOARD_COUNT )
        return bfalse;

    // do not put anything below TX_LAST back onto the free stack
    BillboardList.free_ref[BillboardList._free_count] = ibb;

    BillboardList._free_count++;
    BillboardList.update_guid++;

    return btrue;
}

//--------------------------------------------------------------------------------------------
ego_billboard_data * BillboardList_get_ptr( const BBOARD_REF & ibb )
{
    if ( !VALID_BILLBOARD( ibb ) ) return NULL;

    return BillboardList.lst + ibb;
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
bool_t render_billboard( const ego_camera * pcam, ego_billboard_data * pbb, const float scale )
{
    int i;
    ego_GLvertex vtlist[4];
    float x1, y1;
    float w, h;
    fvec4_t   vector_up, vector_right;

    oglx_texture_t     * ptex;

    if ( NULL == pbb || !pbb->valid ) return bfalse;

    // do not display for objects that are mounted or being held
    if ( INGAME_CHR( pbb->ichr ) && IS_ATTACHED_PCHR( ChrObjList.get_data_ptr( pbb->ichr ) ) ) return bfalse;

    ptex = TxTexture_get_ptr( pbb->tex_ref );

    oglx_texture_Bind( ptex );

    w = oglx_texture_GetImageWidth( ptex );
    h = oglx_texture_GetImageHeight( ptex );

    x1 = w  / ( const float ) oglx_texture_GetTextureWidth( ptex );
    y1 = h  / ( const float ) oglx_texture_GetTextureHeight( ptex );

    vector_right.x =  pcam->mView.CNV( 0, 0 ) * w * scale * pbb->size;
    vector_right.y =  pcam->mView.CNV( 1, 0 ) * w * scale * pbb->size;
    vector_right.z =  pcam->mView.CNV( 2, 0 ) * w * scale * pbb->size;

    vector_up.x    = -pcam->mView.CNV( 0, 1 ) * h * scale * pbb->size;
    vector_up.y    = -pcam->mView.CNV( 1, 1 ) * h * scale * pbb->size;
    vector_up.z    = -pcam->mView.CNV( 2, 1 ) * h * scale * pbb->size;

    // \todo this billboard stuff needs to be implemented as a OpenGL transform

    // bottom left
    vtlist[0].pos[XX] = pbb->pos.x + ( -vector_right.x - 0 * vector_up.x );
    vtlist[0].pos[YY] = pbb->pos.y + ( -vector_right.y - 0 * vector_up.y );
    vtlist[0].pos[ZZ] = pbb->pos.z + ( -vector_right.z - 0 * vector_up.z );
    vtlist[0].tex[SS] = x1;
    vtlist[0].tex[TT] = y1;

    // top left
    vtlist[1].pos[XX] = pbb->pos.x + ( -vector_right.x + 2 * vector_up.x );
    vtlist[1].pos[YY] = pbb->pos.y + ( -vector_right.y + 2 * vector_up.y );
    vtlist[1].pos[ZZ] = pbb->pos.z + ( -vector_right.z + 2 * vector_up.z );
    vtlist[1].tex[SS] = x1;
    vtlist[1].tex[TT] = 0;

    // top right
    vtlist[2].pos[XX] = pbb->pos.x + ( vector_right.x + 2 * vector_up.x );
    vtlist[2].pos[YY] = pbb->pos.y + ( vector_right.y + 2 * vector_up.y );
    vtlist[2].pos[ZZ] = pbb->pos.z + ( vector_right.z + 2 * vector_up.z );
    vtlist[2].tex[SS] = 0;
    vtlist[2].tex[TT] = 0;

    // bottom right
    vtlist[3].pos[XX] = pbb->pos.x + ( vector_right.x - 0 * vector_up.x );
    vtlist[3].pos[YY] = pbb->pos.y + ( vector_right.y - 0 * vector_up.y );
    vtlist[3].pos[ZZ] = pbb->pos.z + ( vector_right.z - 0 * vector_up.z );
    vtlist[3].tex[SS] = 0;
    vtlist[3].tex[TT] = y1;

    ATTRIB_PUSH( "render_billboard", GL_LIGHTING_BIT | GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT );
    {
        GL_DEBUG( glMatrixMode )( GL_MODELVIEW );
        GL_DEBUG( glPushMatrix )();
        GL_DEBUG( glTranslatef )( pbb->offset[XX], pbb->offset[YY], pbb->offset[ZZ] );

        GL_DEBUG( glShadeModel )( GL_FLAT );    // GL_LIGHTING_BIT - Flat shade this

        if ( pbb->tint[AA] < 1.0f )
        {
            GL_DEBUG( glEnable )( GL_BLEND );                             // GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT
            GL_DEBUG( glBlendFunc )( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR );  // GL_COLOR_BUFFER_BIT

            GL_DEBUG( glEnable )( GL_ALPHA_TEST );  // GL_COLOR_BUFFER_BIT GL_ENABLE_BIT
            GL_DEBUG( glAlphaFunc )( GL_GREATER, 0 ); // GL_COLOR_BUFFER_BIT
        }

        // Go on and draw it
        GL_DEBUG_BEGIN( GL_QUADS );
        {
            GL_DEBUG( glColor4fv )( pbb->tint );

            for ( i = 0; i < 4; i++ )
            {
                GL_DEBUG( glTexCoord2fv )( vtlist[i].tex );
                GL_DEBUG( glVertex3fv )( vtlist[i].pos );
            }
        }
        GL_DEBUG_END();

        GL_DEBUG( glMatrixMode )( GL_MODELVIEW );
        GL_DEBUG( glPopMatrix )();
    }
    ATTRIB_POP( "render_billboard" );

    return btrue;
}

//--------------------------------------------------------------------------------------------
void render_all_billboards( const ego_camera * pcam )
{
    BBOARD_REF cnt;

    if ( NULL == pcam ) pcam = PCamera;
    if ( NULL == pcam ) return;

    gfx_begin_3d( pcam );
    {
        ATTRIB_PUSH( "render_all_billboards()", GL_LIGHTING_BIT | GL_DEPTH_BUFFER_BIT | GL_POLYGON_BIT | GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT );
        {
            GL_DEBUG( glShadeModel )( GL_FLAT );    // GL_LIGHTING_BIT - Flat shade this
            GL_DEBUG( glDepthMask )( GL_FALSE );    // GL_DEPTH_BUFFER_BIT
            GL_DEBUG( glDepthFunc )( GL_LEQUAL );   // GL_DEPTH_BUFFER_BIT
            GL_DEBUG( glDisable )( GL_CULL_FACE );  // GL_ENABLE_BIT

            GL_DEBUG( glEnable )( GL_BLEND );                                     // GL_ENABLE_BIT
            GL_DEBUG( glBlendFunc )( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );      // GL_COLOR_BUFFER_BIT

            GL_DEBUG( glColor4f )( 1.0f, 1.0f, 1.0f, 1.0f );

            for ( cnt = 0; cnt < BILLBOARD_COUNT; cnt++ )
            {
                ego_billboard_data * pbb = BillboardList.lst + cnt;

                if ( !pbb->valid ) continue;

                render_billboard( pcam, pbb, 0.75f );
            }
        }
        ATTRIB_POP( "render_all_billboards()" );
    }
    gfx_end_3d();
}

//--------------------------------------------------------------------------------------------
// LINE IMPLENTATION
//--------------------------------------------------------------------------------------------
int get_free_line()
{
    int cnt;

    for ( cnt = 0; cnt < LINE_COUNT; cnt++ )
    {
        if ( line_list[cnt].time < 0 )
        {
            break;
        }
    }

    return cnt < LINE_COUNT ? cnt : -1;
}

//--------------------------------------------------------------------------------------------
void draw_all_lines( ego_camera * pcam )
{
    /// \author BB
    /// \details  draw some lines for debugging purposes

    int cnt, ticks;

    gfx_begin_3d( pcam );
    {
        ATTRIB_PUSH( "render_all_billboards()", GL_ENABLE_BIT | GL_LIGHTING_BIT | GL_DEPTH_BUFFER_BIT | GL_CURRENT_BIT );
        {
            GL_DEBUG( glShadeModel )( GL_FLAT );     // GL_LIGHTING_BIT - Flat shade this
            GL_DEBUG( glDepthMask )( GL_FALSE );     // GL_DEPTH_BUFFER_BIT
            GL_DEBUG( glDepthFunc )( GL_LEQUAL );    // GL_DEPTH_BUFFER_BIT
            GL_DEBUG( glDisable )( GL_CULL_FACE );   // GL_ENABLE_BIT

            GL_DEBUG( glDisable )( GL_BLEND );       // GL_ENABLE_BIT

            GL_DEBUG( glDisable )( GL_TEXTURE_2D );  // GL_ENABLE_BIT - we do not want texture mapped lines

            ticks = egoboo_get_ticks();

            for ( cnt = 0; cnt < LINE_COUNT; cnt++ )
            {
                if ( line_list[cnt].time < 0 ) continue;

                if ( line_list[cnt].time < ticks )
                {
                    line_list[cnt].time = -1;
                    continue;
                }

                GL_DEBUG( glColor4fv )( line_list[cnt].color.v );       // GL_CURRENT_BIT
                GL_DEBUG_BEGIN( GL_LINES );
                {
                    GL_DEBUG( glVertex3fv )( line_list[cnt].src.v );
                    GL_DEBUG( glVertex3fv )( line_list[cnt].dst.v );
                }
                GL_DEBUG_END();
            }
        }
        ATTRIB_POP( "render_all_billboards()" );
    }
    gfx_end_3d();
}

//--------------------------------------------------------------------------------------------
// AXIS BOUNDING BOX IMPLEMENTATION(S)
//--------------------------------------------------------------------------------------------
bool_t render_aabb( ego_aabb * pbbox )
{
    GLXvector3f * pmin, * pmax;

    if ( NULL == pbbox ) return bfalse;

    GL_DEBUG( glPushMatrix )();
    {
        pmin = &( pbbox->mins );
        pmax = &( pbbox->maxs );

        // !!!! there must be an optimized way of doing this !!!!

        GL_DEBUG_BEGIN( GL_QUADS );
        {
            // Front Face
            GL_DEBUG( glVertex3f )(( *pmin )[XX], ( *pmin )[YY], ( *pmax )[ZZ] );
            GL_DEBUG( glVertex3f )(( *pmax )[XX], ( *pmin )[YY], ( *pmax )[ZZ] );
            GL_DEBUG( glVertex3f )(( *pmax )[XX], ( *pmax )[YY], ( *pmax )[ZZ] );
            GL_DEBUG( glVertex3f )(( *pmin )[XX], ( *pmax )[YY], ( *pmax )[ZZ] );

            // Back Face
            GL_DEBUG( glVertex3f )(( *pmin )[XX], ( *pmin )[YY], ( *pmin )[ZZ] );
            GL_DEBUG( glVertex3f )(( *pmin )[XX], ( *pmax )[YY], ( *pmin )[ZZ] );
            GL_DEBUG( glVertex3f )(( *pmax )[XX], ( *pmax )[YY], ( *pmin )[ZZ] );
            GL_DEBUG( glVertex3f )(( *pmax )[XX], ( *pmin )[YY], ( *pmin )[ZZ] );

            // Top Face
            GL_DEBUG( glVertex3f )(( *pmin )[XX], ( *pmax )[YY], ( *pmin )[ZZ] );
            GL_DEBUG( glVertex3f )(( *pmin )[XX], ( *pmax )[YY], ( *pmax )[ZZ] );
            GL_DEBUG( glVertex3f )(( *pmax )[XX], ( *pmax )[YY], ( *pmax )[ZZ] );
            GL_DEBUG( glVertex3f )(( *pmax )[XX], ( *pmax )[YY], ( *pmin )[ZZ] );

            // Bottom Face
            GL_DEBUG( glVertex3f )(( *pmin )[XX], ( *pmin )[YY], ( *pmin )[ZZ] );
            GL_DEBUG( glVertex3f )(( *pmax )[XX], ( *pmin )[YY], ( *pmin )[ZZ] );
            GL_DEBUG( glVertex3f )(( *pmax )[XX], ( *pmin )[YY], ( *pmax )[ZZ] );
            GL_DEBUG( glVertex3f )(( *pmin )[XX], ( *pmin )[YY], ( *pmax )[ZZ] );

            // Right face
            GL_DEBUG( glVertex3f )(( *pmax )[XX], ( *pmin )[YY], ( *pmin )[ZZ] );
            GL_DEBUG( glVertex3f )(( *pmax )[XX], ( *pmax )[YY], ( *pmin )[ZZ] );
            GL_DEBUG( glVertex3f )(( *pmax )[XX], ( *pmax )[YY], ( *pmax )[ZZ] );
            GL_DEBUG( glVertex3f )(( *pmax )[XX], ( *pmin )[YY], ( *pmax )[ZZ] );

            // Left Face
            GL_DEBUG( glVertex3f )(( *pmin )[XX], ( *pmin )[YY], ( *pmin )[ZZ] );
            GL_DEBUG( glVertex3f )(( *pmin )[XX], ( *pmin )[YY], ( *pmax )[ZZ] );
            GL_DEBUG( glVertex3f )(( *pmin )[XX], ( *pmax )[YY], ( *pmax )[ZZ] );
            GL_DEBUG( glVertex3f )(( *pmin )[XX], ( *pmax )[YY], ( *pmin )[ZZ] );
        }
        GL_DEBUG_END();
    }
    GL_DEBUG( glPopMatrix )();

    return btrue;
}

//--------------------------------------------------------------------------------------------
bool_t render_oct_bb( const ego_oct_bb * bb, const bool_t draw_square, const bool_t draw_diamond )
{
    bool_t retval = bfalse;

    if ( NULL == bb ) return bfalse;

    ATTRIB_PUSH( "render_oct_bb", GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT );
    {
        // don't write into the depth buffer
        GL_DEBUG( glDepthMask )( GL_FALSE );
        GL_DEBUG( glEnable )( GL_DEPTH_TEST );

        // fix the poorly chosen normals...
        GL_DEBUG( glDisable )( GL_CULL_FACE );

        // make them transparent
        GL_DEBUG( glEnable )( GL_BLEND );
        GL_DEBUG( glBlendFunc )( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

        // choose a "white" texture
        oglx_texture_Bind( NULL );

        //------------------------------------------------
        // DIAGONAL BBOX
        if ( draw_diamond )
        {
            float p1_x, p1_y;
            float p2_x, p2_y;

            GL_DEBUG( glColor4f )( 0.5f, 1.0f, 1.0f, 0.1f );

            p1_x = 0.5f * ( bb->maxs[OCT_XY] - bb->maxs[OCT_YX] );
            p1_y = 0.5f * ( bb->maxs[OCT_XY] + bb->maxs[OCT_YX] );
            p2_x = 0.5f * ( bb->maxs[OCT_XY] - bb->mins[OCT_YX] );
            p2_y = 0.5f * ( bb->maxs[OCT_XY] + bb->mins[OCT_YX] );

            GL_DEBUG_BEGIN( GL_QUADS );
            GL_DEBUG( glVertex3f )( p1_x, p1_y, bb->mins[OCT_Z] );
            GL_DEBUG( glVertex3f )( p2_x, p2_y, bb->mins[OCT_Z] );
            GL_DEBUG( glVertex3f )( p2_x, p2_y, bb->maxs[OCT_Z] );
            GL_DEBUG( glVertex3f )( p1_x, p1_y, bb->maxs[OCT_Z] );
            GL_DEBUG_END();

            p1_x = 0.5f * ( bb->maxs[OCT_XY] - bb->mins[OCT_YX] );
            p1_y = 0.5f * ( bb->maxs[OCT_XY] + bb->mins[OCT_YX] );
            p2_x = 0.5f * ( bb->mins[OCT_XY] - bb->mins[OCT_YX] );
            p2_y = 0.5f * ( bb->mins[OCT_XY] + bb->mins[OCT_YX] );

            GL_DEBUG_BEGIN( GL_QUADS );
            GL_DEBUG( glVertex3f )( p1_x, p1_y, bb->mins[OCT_Z] );
            GL_DEBUG( glVertex3f )( p2_x, p2_y, bb->mins[OCT_Z] );
            GL_DEBUG( glVertex3f )( p2_x, p2_y, bb->maxs[OCT_Z] );
            GL_DEBUG( glVertex3f )( p1_x, p1_y, bb->maxs[OCT_Z] );
            GL_DEBUG_END();

            p1_x = 0.5f * ( bb->mins[OCT_XY] - bb->mins[OCT_YX] );
            p1_y = 0.5f * ( bb->mins[OCT_XY] + bb->mins[OCT_YX] );
            p2_x = 0.5f * ( bb->mins[OCT_XY] - bb->maxs[OCT_YX] );
            p2_y = 0.5f * ( bb->mins[OCT_XY] + bb->maxs[OCT_YX] );

            GL_DEBUG_BEGIN( GL_QUADS );
            GL_DEBUG( glVertex3f )( p1_x, p1_y, bb->mins[OCT_Z] );
            GL_DEBUG( glVertex3f )( p2_x, p2_y, bb->mins[OCT_Z] );
            GL_DEBUG( glVertex3f )( p2_x, p2_y, bb->maxs[OCT_Z] );
            GL_DEBUG( glVertex3f )( p1_x, p1_y, bb->maxs[OCT_Z] );
            GL_DEBUG_END();

            p1_x = 0.5f * ( bb->mins[OCT_XY] - bb->maxs[OCT_YX] );
            p1_y = 0.5f * ( bb->mins[OCT_XY] + bb->maxs[OCT_YX] );
            p2_x = 0.5f * ( bb->maxs[OCT_XY] - bb->maxs[OCT_YX] );
            p2_y = 0.5f * ( bb->maxs[OCT_XY] + bb->maxs[OCT_YX] );

            GL_DEBUG_BEGIN( GL_QUADS );
            GL_DEBUG( glVertex3f )( p1_x, p1_y, bb->mins[OCT_Z] );
            GL_DEBUG( glVertex3f )( p2_x, p2_y, bb->mins[OCT_Z] );
            GL_DEBUG( glVertex3f )( p2_x, p2_y, bb->maxs[OCT_Z] );
            GL_DEBUG( glVertex3f )( p1_x, p1_y, bb->maxs[OCT_Z] );
            GL_DEBUG_END();

            retval = btrue;
        }

        //------------------------------------------------
        // SQUARE BBOX
        if ( draw_square )
        {
            GL_DEBUG( glColor4f )( 1.0f, 0.5f, 1.0f, 0.1f );

            // XZ FACE, min Y
            GL_DEBUG_BEGIN( GL_QUADS );
            GL_DEBUG( glVertex3f )( bb->mins[OCT_X], bb->mins[OCT_Y], bb->mins[OCT_Z] );
            GL_DEBUG( glVertex3f )( bb->mins[OCT_X], bb->mins[OCT_Y], bb->maxs[OCT_Z] );
            GL_DEBUG( glVertex3f )( bb->maxs[OCT_X], bb->mins[OCT_Y], bb->maxs[OCT_Z] );
            GL_DEBUG( glVertex3f )( bb->maxs[OCT_X], bb->mins[OCT_Y], bb->mins[OCT_Z] );
            GL_DEBUG_END();

            // YZ FACE, min X
            GL_DEBUG_BEGIN( GL_QUADS );
            GL_DEBUG( glVertex3f )( bb->mins[OCT_X], bb->mins[OCT_Y], bb->mins[OCT_Z] );
            GL_DEBUG( glVertex3f )( bb->mins[OCT_X], bb->mins[OCT_Y], bb->maxs[OCT_Z] );
            GL_DEBUG( glVertex3f )( bb->mins[OCT_X], bb->maxs[OCT_Y], bb->maxs[OCT_Z] );
            GL_DEBUG( glVertex3f )( bb->mins[OCT_X], bb->maxs[OCT_Y], bb->mins[OCT_Z] );
            GL_DEBUG_END();

            // XZ FACE, max Y
            GL_DEBUG_BEGIN( GL_QUADS );
            GL_DEBUG( glVertex3f )( bb->mins[OCT_X], bb->maxs[OCT_Y], bb->mins[OCT_Z] );
            GL_DEBUG( glVertex3f )( bb->mins[OCT_X], bb->maxs[OCT_Y], bb->maxs[OCT_Z] );
            GL_DEBUG( glVertex3f )( bb->maxs[OCT_X], bb->maxs[OCT_Y], bb->maxs[OCT_Z] );
            GL_DEBUG( glVertex3f )( bb->maxs[OCT_X], bb->maxs[OCT_Y], bb->mins[OCT_Z] );
            GL_DEBUG_END();

            // YZ FACE, max X
            GL_DEBUG_BEGIN( GL_QUADS );
            GL_DEBUG( glVertex3f )( bb->maxs[OCT_X], bb->mins[OCT_Y], bb->mins[OCT_Z] );
            GL_DEBUG( glVertex3f )( bb->maxs[OCT_X], bb->mins[OCT_Y], bb->maxs[OCT_Z] );
            GL_DEBUG( glVertex3f )( bb->maxs[OCT_X], bb->maxs[OCT_Y], bb->maxs[OCT_Z] );
            GL_DEBUG( glVertex3f )( bb->maxs[OCT_X], bb->maxs[OCT_Y], bb->mins[OCT_Z] );
            GL_DEBUG_END();

            // XY FACE, min Z
            GL_DEBUG_BEGIN( GL_QUADS );
            GL_DEBUG( glVertex3f )( bb->mins[OCT_X], bb->mins[OCT_Y], bb->mins[OCT_Z] );
            GL_DEBUG( glVertex3f )( bb->mins[OCT_X], bb->maxs[OCT_Y], bb->mins[OCT_Z] );
            GL_DEBUG( glVertex3f )( bb->maxs[OCT_X], bb->maxs[OCT_Y], bb->mins[OCT_Z] );
            GL_DEBUG( glVertex3f )( bb->maxs[OCT_X], bb->mins[OCT_Y], bb->mins[OCT_Z] );
            GL_DEBUG_END();

            // XY FACE, max Z
            GL_DEBUG_BEGIN( GL_QUADS );
            GL_DEBUG( glVertex3f )( bb->mins[OCT_X], bb->mins[OCT_Y], bb->maxs[OCT_Z] );
            GL_DEBUG( glVertex3f )( bb->mins[OCT_X], bb->maxs[OCT_Y], bb->maxs[OCT_Z] );
            GL_DEBUG( glVertex3f )( bb->maxs[OCT_X], bb->maxs[OCT_Y], bb->maxs[OCT_Z] );
            GL_DEBUG( glVertex3f )( bb->maxs[OCT_X], bb->mins[OCT_Y], bb->maxs[OCT_Z] );
            GL_DEBUG_END();

            retval = btrue;
        }

    }
    ATTRIB_POP( "render_oct_bb" );

    return retval;
}

//--------------------------------------------------------------------------------------------
// GRAPHICS OPTIMIZATIONS
//--------------------------------------------------------------------------------------------
bool_t dolist_add_chr( const ego_mpd * pmesh, const CHR_REF & ichr )
{
    /// \author ZF
    /// \details This function puts a character in the list

    Uint32 itile;
    ego_chr * pchr;
    ego_cap * pcap;
    gfx_mad_instance * pinst;

    if ( dolist_count >= DOLIST_SIZE ) return bfalse;

    if ( !INGAME_CHR( ichr ) ) return bfalse;
    pchr  = ChrObjList.get_data_ptr( ichr );
    pinst = &( pchr->gfx_inst );

    if ( pinst->indolist ) return btrue;

    pcap = ego_chr::get_pcap( ichr );
    if ( NULL == pcap ) return bfalse;

    itile = pchr->onwhichgrid;
    if ( !ego_mpd::grid_is_valid( pmesh, itile ) ) return bfalse;

    if ( pmesh->tmem.tile_list[itile].inrenderlist )
    {
        dolist[dolist_count].ichr = ichr;
        dolist[dolist_count].iprt = MAX_PRT;
        dolist_count++;

        pinst->indolist = btrue;
    }
    else if ( pcap->alwaysdraw )
    {
        // Double check for large/special objects

        dolist[dolist_count].ichr = ichr;
        dolist[dolist_count].iprt = MAX_PRT;
        dolist_count++;

        pinst->indolist = btrue;
    }

    if ( pinst->indolist )
    {
        // Add its weapons too
        dolist_add_chr( pmesh, pchr->holdingwhich[SLOT_LEFT] );
        dolist_add_chr( pmesh, pchr->holdingwhich[SLOT_RIGHT] );
    }

    return btrue;
}

//--------------------------------------------------------------------------------------------
bool_t dolist_add_prt( const ego_mpd * pmesh, const PRT_REF & iprt )
{
    /// \author ZF
    /// \details This function puts a character in the list

    ego_prt * pprt;
    ego_prt_instance * pinst;

    if ( dolist_count >= DOLIST_SIZE ) return bfalse;

    if ( !DEFINED_PRT( iprt ) ) return bfalse;
    pprt = PrtObjList.get_data_ptr( iprt );
    pinst = &( pprt->inst );

    if ( pinst->indolist ) return btrue;

    if ( 0 == pinst->size || pprt->is_hidden || !ego_mpd::grid_is_valid( pmesh, pprt->onwhichgrid ) ) return bfalse;

    dolist[dolist_count].ichr = CHR_REF( MAX_CHR );
    dolist[dolist_count].iprt = iprt;
    dolist_count++;

    pinst->indolist = btrue;

    return btrue;
}

//--------------------------------------------------------------------------------------------
void dolist_make( const ego_mpd * pmesh )
{
    /// \author ZZ
    /// \details  This function finds the characters that need to be drawn and puts them in the list

    size_t cnt;
    CHR_REF ichr;

    // Remove everyone from the dolist
    for ( cnt = 0; cnt < dolist_count; cnt++ )
    {
        if ( MAX_PRT == dolist[cnt].iprt && MAX_CHR != dolist[cnt].ichr )
        {
            ChrObjList.get_data_ref( dolist[cnt].ichr ).gfx_inst.indolist = bfalse;
        }
        else if ( MAX_CHR == dolist[cnt].ichr && MAX_PRT != dolist[cnt].iprt )
        {
            PrtObjList.get_data_ref( dolist[cnt].iprt ).inst.indolist = bfalse;
        }
    }
    dolist_count = 0;

    // Now fill it up again
    CHR_BEGIN_LOOP_PROCESSING( ichr, pchr )
    {
        if ( !pchr->pack.is_packed )
        {
            // Add the character
            dolist_add_chr( pmesh, ichr );
        }
    }
    CHR_END_LOOP();

    PRT_BEGIN_LOOP_ALLOCATED( iprt, pprt )
    {
        if ( ego_mpd::grid_is_valid( pmesh, pprt->onwhichgrid ) )
        {
            // Add the character
            dolist_add_prt( pmesh, iprt );
        }
    }
    PRT_END_LOOP();
}

//--------------------------------------------------------------------------------------------
void dolist_sort( const ego_camera * pcam, const bool_t do_reflect )
{
    /// \author ZZ
    /// \details  This function orders the dolist based on distance from camera,
    ///    which is needed for reflections to properly clip themselves.
    ///    Order from closest to farthest

    Uint32    cnt;
    fvec3_t   vcam;
    size_t    count;

    vcam = mat_getCamForward( pcam->mView );

    // Figure the distance of each
    count = 0;
    for ( cnt = 0; cnt < dolist_count; cnt++ )
    {
        fvec3_t   vtmp;
        float dist;

        if ( MAX_PRT == dolist[cnt].iprt && INGAME_CHR( dolist[cnt].ichr ) )
        {
            CHR_REF ichr;
            fvec3_t pos_tmp;

            ichr = dolist[cnt].ichr;

            if ( do_reflect )
            {
                pos_tmp = mat_getTranslate( ChrObjList.get_data_ref( ichr ).gfx_inst.ref.matrix );
            }
            else
            {
                pos_tmp = mat_getTranslate( ChrObjList.get_data_ref( ichr ).gfx_inst.matrix );
            }

            vtmp = fvec3_sub( pos_tmp.v, pcam->pos.v );
        }
        else if ( MAX_CHR == dolist[cnt].ichr && INGAME_PRT( dolist[cnt].iprt ) )
        {
            PRT_REF iprt = dolist[cnt].iprt;

            if ( do_reflect )
            {
                vtmp = fvec3_sub( PrtObjList.get_data_ref( iprt ).inst.pos.v, pcam->pos.v );
            }
            else
            {
                vtmp = fvec3_sub( PrtObjList.get_data_ref( iprt ).inst.ref_pos.v, pcam->pos.v );
            }
        }
        else
        {
            continue;
        }

        dist = fvec3_dot_product( vtmp.v, vcam.v );
        if ( dist > 0 )
        {
            dolist[count].ichr = dolist[cnt].ichr;
            dolist[count].iprt = dolist[cnt].iprt;
            dolist[count].dist = dist;
            count++;
        }
    }
    dolist_count = count;

    // use qsort to sort the list in-place
    SDL_qsort( dolist, dolist_count, sizeof( ego_obj_registry_entity ), obj_registry_entity_cmp );
}

//--------------------------------------------------------------------------------------------
int obj_registry_entity_cmp( const void * pleft, const void * pright )
{
    ego_obj_registry_entity * dleft  = ( ego_obj_registry_entity * ) pleft;
    ego_obj_registry_entity * dright = ( ego_obj_registry_entity * ) pright;

    int   rv;
    float diff;

    diff = dleft->dist - dright->dist;

    if ( diff < 0.0f )
    {
        rv = -1;
    }
    else if ( diff > 0.0f )
    {
        rv = 1;
    }
    else
    {
        rv = 0;
    }

    return rv;
}

//--------------------------------------------------------------------------------------------
void renderlist_reset()
{
    /// \author BB
    /// \details  Clear old render lists

    if ( NULL != renderlist.pmesh )
    {
        int cnt;

        // clear out the inrenderlist flag for the old mesh
        ego_tile_info * tlist = renderlist.pmesh->tmem.tile_list;

        for ( cnt = 0; cnt < renderlist.all_count; cnt++ )
        {
            Uint32 fan = renderlist.all[cnt];
            if ( fan < renderlist.pmesh->info.tiles_count )
            {
                tlist[fan].inrenderlist       = bfalse;
                tlist[fan].inrenderlist_frame = 0;
            }
        }

        renderlist.pmesh = NULL;
    }

    renderlist.all_count = 0;
    renderlist.ref_count = 0;
    renderlist.sha_count = 0;
    renderlist.drf_count = 0;
    renderlist.ndr_count = 0;
    renderlist.wat_count = 0;
}

//--------------------------------------------------------------------------------------------
void renderlist_make( ego_mpd * pmesh, ego_camera * pcam )
{
    /// \author ZZ
    /// \details  This function figures out which mesh fans to draw
    int cnt, grid_x, grid_y;
    int row, run, numrow;
    int corner_x[4], corner_y[4];
    int leftnum, leftlist[4];
    int rightnum, rightlist[4];
    int rowstt[128], rowend[128];
    int x, stepx, divx, basex;
    int from, to;

    ego_tile_info * tlist;

    if ( 0 != ( frame_all & 7 ) ) return;

    // reset the renderlist
    renderlist_reset();

    // Make sure it doesn't die ugly
    if ( NULL == pcam ) return;

    // Find the render area corners
    project_view( pcam );

    if ( NULL == pmesh ) return;

    renderlist.pmesh = pmesh;
    tlist = pmesh->tmem.tile_list;

    // It works better this way...
    cornery[cornerlistlowtohighy[3]] += 256;

    // Make life simpler
    for ( cnt = 0; cnt < 4; cnt++ )
    {
        corner_x[cnt] = cornerx[cornerlistlowtohighy[cnt]];
        corner_y[cnt] = cornery[cornerlistlowtohighy[cnt]];
    }

    // Find the center line
    divx = corner_y[3] - corner_y[0]; if ( divx < 1 ) return;
    stepx = corner_x[3] - corner_x[0];
    basex = corner_x[0];

    // Find the points in each edge
    leftlist[0] = 0;  leftnum = 1;
    rightlist[0] = 0;  rightnum = 1;
    if ( corner_x[1] < ( stepx*( corner_y[1] - corner_y[0] ) / divx ) + basex )
    {
        leftlist[leftnum] = 1;  leftnum++;
        cornerx[1] -= 512;
    }
    else
    {
        rightlist[rightnum] = 1;  rightnum++;
        cornerx[1] += 512;
    }
    if ( corner_x[2] < ( stepx*( corner_y[2] - corner_y[0] ) / divx ) + basex )
    {
        leftlist[leftnum] = 2;  leftnum++;
        cornerx[2] -= 512;
    }
    else
    {
        rightlist[rightnum] = 2;  rightnum++;
        cornerx[2] += 512;
    }

    leftlist[leftnum] = 3;  leftnum++;
    rightlist[rightnum] = 3;  rightnum++;

    // Make the left edge ( rowstt )
    grid_y = corner_y[0] >> TILE_BITS;
    row = 0;
    cnt = 1;
    for ( cnt = 1; cnt < leftnum; cnt++ )
    {
        from = leftlist[cnt-1];  to = leftlist[cnt];
        x = corner_x[from];
        divx = corner_y[to] - corner_y[from];
        stepx = 0;
        if ( divx > 0 )
        {
            stepx = (( corner_x[to] - corner_x[from] ) << TILE_BITS ) / divx;
        }

        x -= 256;
        run = corner_y[to] >> TILE_BITS;
        while ( grid_y < run )
        {
            if ( grid_y >= 0 && size_t( grid_y ) < pmesh->info.tiles_y )
            {
                grid_x = x >> TILE_BITS;
                if ( grid_x < 0 )  grid_x = 0;
                else if ( size_t( grid_x ) >= pmesh->info.tiles_x )  grid_x = ego_sint( pmesh->info.tiles_x ) - 1;

                rowstt[row] = grid_x;
                row++;
            }

            x += stepx;
            grid_y++;
        }
    }
    numrow = row;

    // Make the right edge ( rowrun )
    grid_y = corner_y[0] >> TILE_BITS;
    row = 0;
    for ( cnt = 1; cnt < rightnum; cnt++ )
    {
        from = rightlist[cnt-1];  to = rightlist[cnt];
        x = corner_x[from];
        x += 128;
        divx = corner_y[to] - corner_y[from];
        stepx = 0;
        if ( divx > 0 )
        {
            stepx = (( corner_x[to] - corner_x[from] ) << TILE_BITS ) / divx;
        }

        run = corner_y[to] >> TILE_BITS;

        while ( grid_y < run )
        {
            if ( grid_y >= 0 && size_t( grid_y ) < pmesh->info.tiles_y )
            {
                grid_x = x >> TILE_BITS;
                if ( grid_x < 0 )  grid_x = 0;
                else if ( size_t( grid_x ) >= pmesh->info.tiles_x - 1 )  grid_x = ego_sint( pmesh->info.tiles_x ) - 1;// -2

                rowend[row] = grid_x;
                row++;
            }

            x += stepx;
            grid_y++;
        }
    }

    if ( numrow != row )
    {
        log_error( "ROW error (%i, %i)\n", numrow, row );
    }

    // fill the renderlist from the projected view
    grid_y = corner_y[0] / TILE_ISIZE;
    grid_y = CLIP( grid_y, 0, ego_sint( pmesh->info.tiles_y ) - 1 );
    for ( row = 0; row < numrow; row++, grid_y++ )
    {
        for ( grid_x = rowstt[row]; grid_x <= rowend[row] && renderlist.all_count < MAXMESHRENDER; grid_x++ )
        {
            cnt = pmesh->gmem.tilestart[grid_y] + grid_x;

            // Flag the tile as in the renderlist
            tlist[cnt].inrenderlist       = btrue;

            // if the tile was not in the renderlist last frame, then we need to force a lighting update of this tile
            if ( tlist[cnt].inrenderlist_frame < ego_sint( frame_all ) - 1 )
            {
                tlist[cnt].needs_lighting_update = btrue;
            }

            // make sure to cache the frame number of this update
            tlist[cnt].inrenderlist_frame = frame_all;

            // Put each tile in basic list
            renderlist.all[renderlist.all_count] = cnt;
            renderlist.all_count++;

            // Put each tile in one other list, for shadows and relections
            if ( 0 != ego_mpd::test_fx( pmesh, cnt, MPDFX_SHA ) )
            {
                renderlist.sha[renderlist.sha_count] = cnt;
                renderlist.sha_count++;
            }
            else
            {
                renderlist.ref[renderlist.ref_count] = cnt;
                renderlist.ref_count++;
            }

            if ( 0 != ego_mpd::test_fx( pmesh, cnt, MPDFX_DRAWREF ) )
            {
                renderlist.drf[renderlist.drf_count] = cnt;
                renderlist.drf_count++;
            }
            else
            {
                renderlist.ndr[renderlist.ndr_count] = cnt;
                renderlist.ndr_count++;
            }

            if ( 0 != ego_mpd::test_fx( pmesh, cnt, MPDFX_WATER ) )
            {
                renderlist.wat[renderlist.wat_count] = cnt;
                renderlist.wat_count++;
            }
        }
    }
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
int DisplayMsg_get_free()
{
    /// \author ZF
    /// \details This function finds the best message to use
    /// Pick the first one

    int tnc = DisplayMsg.count;

    DisplayMsg.count++;
    DisplayMsg.count %= maxmessage;

    return tnc;
}

//--------------------------------------------------------------------------------------------
// ASSET INITIALIZATION
//--------------------------------------------------------------------------------------------
void init_icon_data()
{
    /// \author ZZ
    /// \details  This function sets the icon pointers to NULL

    iconrect.xmin = 0;
    iconrect.xmax = ICON_SIZE;
    iconrect.ymin = 0;
    iconrect.ymax = ICON_SIZE;
}

//--------------------------------------------------------------------------------------------
void init_bar_data()
{
    Uint8 cnt;

    // Initialize the life and mana bars
    for ( cnt = 0; cnt < NUMBAR; cnt++ )
    {
        tabrect[cnt].xmin = 0;
        tabrect[cnt].xmax = TABX;
        tabrect[cnt].ymin = cnt * BARY;
        tabrect[cnt].ymax = ( cnt + 1 ) * BARY;

        barrect[cnt].xmin = TABX;
        barrect[cnt].xmax = BARX;  // This is reset whenever a bar is drawn
        barrect[cnt].ymin = tabrect[cnt].ymin;
        barrect[cnt].ymax = tabrect[cnt].ymax;
    }
}

//--------------------------------------------------------------------------------------------
void init_blip_data()
{
    int cnt;

    // Set up the rectangles
    for ( cnt = 0; cnt < COLOR_MAX; cnt++ )
    {
        bliprect[cnt].xmin = cnt * BLIPSIZE;
        bliprect[cnt].xmax = cnt * BLIPSIZE + BLIPSIZE;
        bliprect[cnt].ymin = 0;
        bliprect[cnt].ymax = BLIPSIZE;
    }

    youarehereon = bfalse;
    blip_count      = 0;
}

//--------------------------------------------------------------------------------------------
void init_map_data()
{
    /// \author ZZ
    /// \details  This function releases all the map images

    // Set up the rectangles
    maprect.xmin = 0;
    maprect.xmax = MAPSIZE;
    maprect.ymin = 0;
    maprect.ymax = MAPSIZE;

    mapvalid = bfalse;
    mapon    = bfalse;
}

//--------------------------------------------------------------------------------------------
void init_all_graphics()
{
    init_icon_data();
    init_bar_data();
    init_blip_data();
    init_map_data();
    font_bmp_init();

    BillboardList_free_all();
    TxTexture_init_all();

    PROFILE_RESET( render_scene_init );
    PROFILE_RESET( render_scene_mesh );
    PROFILE_RESET( render_scene_solid );
    PROFILE_RESET( render_scene_water );
    PROFILE_RESET( render_scene_trans );

    PROFILE_RESET( renderlist_make );
    PROFILE_RESET( dolist_make );
    PROFILE_RESET( do_grid_lighting );
    PROFILE_RESET( light_fans );
    PROFILE_RESET( update_all_chr_instance );
    PROFILE_RESET( update_all_prt_instance );

    PROFILE_RESET( render_scene_mesh_dolist_sort );
    PROFILE_RESET( render_scene_mesh_ndr );
    PROFILE_RESET( render_scene_mesh_drf_back );
    PROFILE_RESET( render_scene_mesh_ref );
    PROFILE_RESET( render_scene_mesh_ref_chr );
    PROFILE_RESET( render_scene_mesh_drf_solid );
    PROFILE_RESET( render_scene_mesh_render_shadows );

    stabilized_fps_sum    = 0.0f;
    stabilized_fps_weight = 0.0f;
}

//--------------------------------------------------------------------------------------------
void release_all_graphics()
{
    init_icon_data();
    init_bar_data();
    init_blip_data();
    init_map_data();

    BillboardList_free_all();
    TxTexture_release_all();
}

//--------------------------------------------------------------------------------------------
void delete_all_graphics()
{
    init_icon_data();
    init_bar_data();
    init_blip_data();
    init_map_data();

    BillboardList_free_all();
    TxTexture_delete_all();
}

//--------------------------------------------------------------------------------------------
bool_t load_all_global_icons()
{
    /// \author ZF
    /// \details  Load all the global icons used in all modules

    const char * fname = NULL;
    bool_t result = btrue;

    // Now load every icon
    fname = "mp_data/nullicon";
    if ( INVALID_TX_TEXTURE == TxTexture_load_one_vfs( fname, TX_REF( ICON_NULL ), INVALID_KEY ) )
    {
        log_warning( "Could not load icon file \"%s\"", fname );
        result = bfalse;
    }

    fname = "mp_data/keybicon";
    if ( INVALID_TX_TEXTURE == TxTexture_load_one_vfs( fname, TX_REF( ICON_KEYB ), INVALID_KEY ) )
    {
        log_warning( "Could not load icon file \"%s\"", fname );
        result = bfalse;
    }

    fname = "mp_data/mousicon";
    if ( INVALID_TX_TEXTURE == TxTexture_load_one_vfs( fname, TX_REF( ICON_MOUS ), INVALID_KEY ) )
    {
        log_warning( "Could not load icon file \"%s\"", fname );
        result = bfalse;
    }

    fname = "mp_data/joyaicon";
    if ( INVALID_TX_TEXTURE == TxTexture_load_one_vfs( fname, TX_REF( ICON_JOYA ), INVALID_KEY ) )
    {
        log_warning( "Could not load icon file \"%s\"", fname );
        result = bfalse;
    }

    fname = "mp_data/joybicon";
    if ( INVALID_TX_TEXTURE == TxTexture_load_one_vfs( fname, TX_REF( ICON_JOYB ), INVALID_KEY ) )
    {
        log_warning( "Could not load icon file \"%s\"", fname );
        result = bfalse;
    }

    // Spit out a warning if needed
    if ( !result )
    {
        log_warning( "Could not load all global icons!\n" );
    }

    return result;
}

//--------------------------------------------------------------------------------------------
void load_basic_textures( /* const char *modname */ )
{
    /// \author ZZ
    /// \details  This function loads the standard textures for a module

    // Particle sprites
    TxTexture_load_one_vfs( "mp_data/particle_trans", TX_REF( TX_PARTICLE_TRANS ), TRANSCOLOR );
    set_prt_texture_params( TX_REF( TX_PARTICLE_TRANS ) );

    TxTexture_load_one_vfs( "mp_data/particle_light", TX_REF( TX_PARTICLE_LIGHT ), INVALID_KEY );
    set_prt_texture_params( TX_REF( TX_PARTICLE_LIGHT ) );

    // Module background tiles
    TxTexture_load_one_vfs( "mp_data/tile0", TX_REF( TX_TILE_0 ), TRANSCOLOR );
    TxTexture_load_one_vfs( "mp_data/tile1", TX_REF( TX_TILE_1 ), TRANSCOLOR );
    TxTexture_load_one_vfs( "mp_data/tile2", TX_REF( TX_TILE_2 ), TRANSCOLOR );
    TxTexture_load_one_vfs( "mp_data/tile3", TX_REF( TX_TILE_3 ), TRANSCOLOR );

    // Water textures
    TxTexture_load_one_vfs( "mp_data/watertop", TX_REF( TX_WATER_TOP ), TRANSCOLOR );
    TxTexture_load_one_vfs( "mp_data/waterlow", TX_REF( TX_WATER_LOW ), TRANSCOLOR );

    // Texture 7 is the Phong map
    TxTexture_load_one_vfs( "mp_data/Phong", TX_REF( TX_PHONG ), TRANSCOLOR );

    PROFILE_RESET( render_scene_init );
    PROFILE_RESET( render_scene_mesh );
    PROFILE_RESET( render_scene_solid );
    PROFILE_RESET( render_scene_water );
    PROFILE_RESET( render_scene_trans );

    PROFILE_RESET( renderlist_make );
    PROFILE_RESET( dolist_make );
    PROFILE_RESET( do_grid_lighting );
    PROFILE_RESET( light_fans );
    PROFILE_RESET( update_all_chr_instance );
    PROFILE_RESET( update_all_prt_instance );

    PROFILE_RESET( render_scene_mesh_dolist_sort );
    PROFILE_RESET( render_scene_mesh_ndr );
    PROFILE_RESET( render_scene_mesh_drf_back );
    PROFILE_RESET( render_scene_mesh_ref );
    PROFILE_RESET( render_scene_mesh_ref_chr );
    PROFILE_RESET( render_scene_mesh_drf_solid );
    PROFILE_RESET( render_scene_mesh_render_shadows );

    stabilized_fps_sum    = 0.0f;
    stabilized_fps_weight = 0.0f;
}

//--------------------------------------------------------------------------------------------
void load_bars()
{
    /// \author ZZ
    /// \details  This function loads the status bar bitmap

    const char * pname;

    pname = "mp_data/bars";
    if ( INVALID_TX_TEXTURE == TxTexture_load_one_vfs( pname, TX_REF( TX_BARS ), TRANSCOLOR ) )
    {
        log_warning( "%s - Cannot load file! (\"%s\")\n", __FUNCTION__, pname );
    }

    pname = "mp_data/xpbar";
    if ( INVALID_TX_TEXTURE == TxTexture_load_one_vfs( pname, TX_REF( TX_XP_BAR ), TRANSCOLOR ) )
    {
        log_warning( "%s - Cannot load file! (\"%s\")\n", __FUNCTION__, pname );
    }
}

//--------------------------------------------------------------------------------------------
void load_map()
{
    /// \author ZZ
    /// \details  This function loads the map bitmap

    const char* szMap;

    // Turn it all off
    mapvalid = bfalse;
    mapon = bfalse;
    youarehereon = bfalse;
    blip_count = 0;

    // Load the images
    szMap = "mp_data/plan";
    if ( INVALID_TX_TEXTURE == TxTexture_load_one_vfs( szMap, TX_REF( TX_MAP ), INVALID_KEY ) )
    {
        log_debug( "%s - Cannot load file! (\"%s\")\n", __FUNCTION__, szMap );
    }
    else
    {
        mapvalid = btrue;
    }
}

//--------------------------------------------------------------------------------------------
bool_t load_cursor()
{
    /// \author ZF
    /// \details  Loads any cursor bitmaps

    bool_t retval = btrue;

    if ( INVALID_TX_TEXTURE == TxTexture_load_one_vfs( "mp_data/cursor", TX_REF( TX_CURSOR ), INVALID_KEY ) )
    {
        log_warning( "Blip bitmap not loaded! (\"mp_data/cursor\")\n" );
        retval = bfalse;
    }

    return retval;
}

//--------------------------------------------------------------------------------------------
bool_t load_blips()
{
    /// \author ZF
    /// \details This function loads the blip bitmaps

    if ( INVALID_TX_TEXTURE == TxTexture_load_one_vfs( "mp_data/blip", TX_REF( TX_BLIP ), INVALID_KEY ) )
    {
        log_warning( "Blip bitmap not loaded! (\"mp_data/blip\")\n" );
        return bfalse;
    }

    return btrue;
}

//--------------------------------------------------------------------------------------------
void load_graphics()
{
    /// \author ZF
    /// \details  This function loads all the graphics based on the game settings

    GLenum quality;

    // Check if the computer graphic driver supports anisotropic filtering

    if ( !ogl_caps.anisotropic_supported )
    {
        if ( tex_params.texturefilter >= TX_ANISOTROPIC )
        {
            tex_params.texturefilter = TX_TRILINEAR_2;
            log_warning( "Your graphics driver does not support anisotropic filtering.\n" );
        }
    }

    // Enable prespective correction?
    if ( gfx.perspective ) quality = GL_NICEST;
    else quality = GL_FASTEST;
    GL_DEBUG( glHint )( GL_PERSPECTIVE_CORRECTION_HINT, gfx.perspective ? GL_NICEST : GL_FASTEST );

    // Enable dithering?
    if ( gfx.dither )
    {
        GL_DEBUG( glEnable )( GL_DITHER );
    }
    else
    {
        GL_DEBUG( glDisable )( GL_DITHER );
    }

    // mipmap quality
    GL_DEBUG( glHint )( GL_GENERATE_MIPMAP_HINT, gfx.mipmap ? GL_NICEST : GL_FASTEST );

    // Enable Gouraud shading? (Important!)
    GL_DEBUG( glShadeModel )( gfx.shading );

    // Enable antialiasing?
    if ( gfx.antialiasing )
    {
        GL_DEBUG( glEnable )( GL_MULTISAMPLE_ARB );

        GL_DEBUG( glEnable )( GL_LINE_SMOOTH );
        GL_DEBUG( glHint )( GL_LINE_SMOOTH_HINT,    GL_NICEST );

        GL_DEBUG( glEnable )( GL_POINT_SMOOTH );
        GL_DEBUG( glHint )( GL_POINT_SMOOTH_HINT,   GL_NICEST );

        GL_DEBUG( glDisable )( GL_POLYGON_SMOOTH );
        GL_DEBUG( glHint )( GL_POLYGON_SMOOTH_HINT,    GL_FASTEST );

        // PLEASE do not turn this on unless you use
        // GL_DEBUG(glEnable)(GL_BLEND);
        // GL_DEBUG(glBlendFunc)(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        // before every single draw command
        //
        // GL_DEBUG(glEnable)(GL_POLYGON_SMOOTH);
        // GL_DEBUG(glHint)(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
    }
    else
    {
        GL_DEBUG( glDisable )( GL_MULTISAMPLE_ARB );
        GL_DEBUG( glDisable )( GL_POINT_SMOOTH );
        GL_DEBUG( glDisable )( GL_LINE_SMOOTH );
        GL_DEBUG( glDisable )( GL_POLYGON_SMOOTH );
    }
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
void do_chr_flashing()
{
    Uint32 i;

    for ( i = 0; i < dolist_count; i++ )
    {
        CHR_REF ichr = dolist[i].ichr;

        if ( !INGAME_CHR( ichr ) ) continue;

        // Do flashing
        if ( HAS_NO_BITS( true_frame, ChrObjList.get_data_ref( ichr ).flashand ) && ChrObjList.get_data_ref( ichr ).flashand != DONTFLASH )
        {
            flash_character( ichr, 255 );
        }

        // Do blacking
        if ( HAS_NO_BITS( true_frame, SEEKURSEAND ) && local_stats.seekurse_level && ChrObjList.get_data_ref( ichr ).iskursed )
        {
            flash_character( ichr, 0 );
        }
    }
}

//--------------------------------------------------------------------------------------------
void flash_character( const CHR_REF & character, const Uint8 value )
{
    /// \author ZZ
    /// \details  This function sets a character's lighting

    gfx_mad_instance * pinst = ego_chr::get_pinstance( character );

    if ( NULL != pinst )
    {
        pinst->color_amb = value;
    }
}

//--------------------------------------------------------------------------------------------
// MODE CONTROL
//--------------------------------------------------------------------------------------------
void gfx_begin_text()
{
    gfx_enable_texturing();    // Enable texture mapping

    oglx_texture_Bind( TxTexture_get_ptr( TX_REF( TX_FONT ) ) );

    GL_DEBUG( glEnable )( GL_ALPHA_TEST );
    GL_DEBUG( glAlphaFunc )( GL_GREATER, 0 );

    GL_DEBUG( glEnable )( GL_BLEND );
    GL_DEBUG( glBlendFunc )( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

    GL_DEBUG( glDisable )( GL_DEPTH_TEST );
    GL_DEBUG( glDisable )( GL_CULL_FACE );

    GL_DEBUG( glColor4f )( 1.0f, 1.0f, 1.0f, 1.0f );
}

//--------------------------------------------------------------------------------------------
void gfx_end_text()
{
    GL_DEBUG( glDisable )( GL_BLEND );
    GL_DEBUG( glDisable )( GL_ALPHA_TEST );
}

//--------------------------------------------------------------------------------------------
void gfx_enable_texturing()
{
    if ( !GL_DEBUG( glIsEnabled )( GL_TEXTURE_2D ) )
    {
        GL_DEBUG( glEnable )( GL_TEXTURE_2D );
    }
}

//--------------------------------------------------------------------------------------------
void gfx_disable_texturing()
{
    if ( GL_DEBUG( glIsEnabled )( GL_TEXTURE_2D ) )
    {
        GL_DEBUG( glDisable )( GL_TEXTURE_2D );
    }
}

//--------------------------------------------------------------------------------------------
void gfx_begin_3d( const ego_camera * pcam )
{
    GL_DEBUG( glMatrixMode )( GL_PROJECTION );
    GL_DEBUG( glPushMatrix )();
    GL_DEBUG( glLoadMatrixf )( pcam->mProjection.v );

    GL_DEBUG( glMatrixMode )( GL_MODELVIEW );
    GL_DEBUG( glPushMatrix )();
    GL_DEBUG( glLoadMatrixf )( pcam->mView.v );
}

//--------------------------------------------------------------------------------------------
void gfx_end_3d()
{
    GL_DEBUG( glMatrixMode )( GL_MODELVIEW );
    GL_DEBUG( glPopMatrix )();

    GL_DEBUG( glMatrixMode )( GL_PROJECTION );
    GL_DEBUG( glPopMatrix )();
}

//--------------------------------------------------------------------------------------------
void gfx_begin_2d( void )
{
    GL_DEBUG( glMatrixMode )( GL_PROJECTION );
    GL_DEBUG( glLoadIdentity )();                // Reset The Projection Matrix
    GL_DEBUG( glOrtho )( 0, sdl_scr.x, sdl_scr.y, 0, -1, 1 );     // Set up an orthogonal projection

    GL_DEBUG( glMatrixMode )( GL_MODELVIEW );
    GL_DEBUG( glLoadIdentity )();

    GL_DEBUG( glDisable )( GL_DEPTH_TEST );
    GL_DEBUG( glDisable )( GL_CULL_FACE );
}

//--------------------------------------------------------------------------------------------
void gfx_end_2d( void )
{
    GL_DEBUG( glEnable )( GL_CULL_FACE );
    GL_DEBUG( glEnable )( GL_DEPTH_TEST );
}

//--------------------------------------------------------------------------------------------
void gfx_reshape_viewport( const int w, const int h )
{
    GL_DEBUG( glViewport )( 0, 0, w, h );
}

//--------------------------------------------------------------------------------------------
void request_clear_screen()
{
    gfx_page_clear_requested = btrue;
}

//--------------------------------------------------------------------------------------------
void do_clear_screen()
{
    bool_t try_clear;

    try_clear = bfalse;
    if (( rv_success == GProc->running() ) && GProc->state > proc_beginning )
    {
        try_clear = gfx_page_clear_requested;
    }
    else if (( rv_success == MProc->running() ) && MProc->state > proc_beginning )
    {
        try_clear = gfx_page_clear_requested;
    }

    if ( try_clear )
    {
        bool_t game_needs_clear, menu_needs_clear;

        gfx_page_clear_requested = bfalse;

        // clear the depth buffer no matter what
        GL_DEBUG( glDepthMask )( GL_TRUE );
        GL_DEBUG( glClear )( GL_DEPTH_BUFFER_BIT );

        // clear the color buffer only if necessary
        game_needs_clear = gfx.clearson && ( rv_success == GProc->running() );
        menu_needs_clear = mnu_draw_background && ( rv_success == MProc->running() );

        if ( game_needs_clear || menu_needs_clear )
        {
            GL_DEBUG( glClear )( GL_COLOR_BUFFER_BIT );
        }
    }
}

//--------------------------------------------------------------------------------------------
void do_flip_pages()
{
    bool_t try_flip;

    try_flip = bfalse;
    if (( rv_success == GProc->running() ) && GProc->state > proc_beginning )
    {
        try_flip = gfx_page_flip_requested;
    }
    else if (( rv_success == MProc->running() ) && MProc->state > proc_beginning )
    {
        try_flip = gfx_page_flip_requested;
    }

    if ( try_flip )
    {
        gfx_page_flip_requested = bfalse;
        _flip_pages();

        gfx_page_clear_requested = btrue;
    }
}

//--------------------------------------------------------------------------------------------
void request_flip_pages()
{
    gfx_page_flip_requested = btrue;
}

//--------------------------------------------------------------------------------------------
bool_t flip_pages_requested()
{
    return gfx_page_flip_requested;
}

//--------------------------------------------------------------------------------------------
void _flip_pages()
{
    GL_DEBUG( glFlush )();

    // draw the console on top of everything
    egoboo_console_draw_all();

    frame_all++;

    SDL_GL_SwapBuffers();

    gfx_update_timers();

    if ( EProc->screenshot_requested )
    {
        EProc->screenshot_requested = bfalse;

        // take the screenshot NOW, since we have just updated the screen buffer
        if ( !dump_screenshot() )
        {
            debug_printf( "Error writing screenshot!" );    // send a failure message to the screen
            log_warning( "Error writing screenshot\n" );    // Log the error in log.txt
        }
    }
}

//--------------------------------------------------------------------------------------------
// LIGHTING FUNCTIONS
//--------------------------------------------------------------------------------------------
void light_fans( ego_renderlist * prlist )
{
    int    entry;
    Uint8  type;
    float  light;
    int    numvertices, vertex, needs_interpolation_count;
    float  local_mesh_lighting_keep;

    /// \note we are measuring the change in the intensity at the corner of a tile (the "delta") as
    /// a fraction of the current intensity. This is because your eye is much more sensitive to
    /// intensity differences when the intensity is low.
    ///
    /// \note it is normally assumed that 64 colors of gray can make a smoothly colored black and white picture
    /// which means that the threshold could be set as low as 1/64 = 0.015625f.
    const float delta_threshold = 0.05f;

    ego_mpd        * pmesh;
    ego_tile_mem     * ptmem;
    ego_grid_mem     * pgmem;

    if ( NULL == prlist ) return;

#if defined(CLIP_ALL_LIGHT_FANS)
    // update all visible fans once every 4 updates
    if ( 0 != ( frame_all & 0x03 ) ) return;
#endif

    pmesh = prlist->pmesh;
    if ( NULL == pmesh ) return;

    ptmem = &( pmesh->tmem );
    pgmem = &( pmesh->gmem );

#if !defined(CLIP_LIGHT_FANS)
    // update only every frame
    local_mesh_lighting_keep = 0.9f;
#else
    // update only every 4 frames
    local_mesh_lighting_keep = POW( 0.9f, 4 );
#endif

    // cache the grid lighting
    needs_interpolation_count = 0;
    for ( entry = 0; entry < prlist->all_count; entry++ )
    {
        bool_t needs_update;
        int fan;
        float delta;
        ego_tile_info * ptile;

        fan = prlist->all[entry];
        if ( !ego_mpd::grid_is_valid( pmesh, fan ) ) continue;

        ptile = ptmem->tile_list + fan;

#if defined(CLIP_LIGHT_FANS) && !defined(CLIP_ALL_LIGHT_FANS)

        // visible fans based on the update "need"
        needs_update = mesh_test_corners( pmesh, fan, delta_threshold );

        // update every 4 fans even if there is no need
        if ( !needs_update )
        {
            int ix, iy;

            // use a kind of checkerboard pattern
            ix = fan % pgmem->grids_x;
            iy = fan / pgmem->grids_x;
            if ( 0 != ((( ix ^ iy ) + frame_all ) & 0x03 ) )
            {
                needs_update = btrue;
            }
        }

#else
        needs_update = btrue;
#endif

        // does thit tile need a lighting update?
        ptile->needs_lighting_update = needs_update;

        // if there's no need for an update, go to the next tile
        if ( !needs_update ) continue;

        delta = ego_mpd::light_corners( prlist->pmesh, fan, local_mesh_lighting_keep );

#if defined(CLIP_LIGHT_FANS)
        // use the actual measured change in the intensity at the tile edge to
        // signal whether we need to calculate the next stage
        ptile->needs_lighting_update = ( delta > delta_threshold );
#endif

        // optimize the use of the second loop
        if ( ptile->needs_lighting_update )
        {
            numvertices = tile_dict[ptile->type].numvertices;

            if ( numvertices <= 4 )
            {
                int ivrt;

                vertex = ptile->vrtstart;

                // the second loop is not needed at all
                for ( ivrt = 0; ivrt < numvertices; ivrt++, vertex++ )
                {
                    light = ptile->lcache[ivrt];

                    light = CLIP( light, 0.0f, 255.0f );
                    ptmem->clst[vertex][RR] =
                        ptmem->clst[vertex][GG] =
                            ptmem->clst[vertex][BB] = light * INV_FF;
                };

                // clear out the deltas
                SDL_memset( ptile->d1_cache, 0, sizeof( ptile->d1_cache ) );
                SDL_memset( ptile->d2_cache, 0, sizeof( ptile->d2_cache ) );

                // everything has been handled. no need to do this in another loop.
                ptile->needs_lighting_update = bfalse;
            }
            else
            {
                // the clst cannot be updated at this time. defer it to the next loop.
                needs_interpolation_count++;
            }
        }
    }

    // can we avoid this whole loop?
    if ( needs_interpolation_count > 0 )
    {
        // use the grid to light the tiles
        for ( entry = 0; entry < prlist->all_count; entry++ )
        {
            int ivrt;
            Uint32 fan;
            ego_tile_info * ptile;

            fan = prlist->all[entry];
            if ( !ego_mpd::grid_is_valid( pmesh, fan ) ) continue;

            ptile = ptmem->tile_list + fan;

            if ( !ptile->needs_lighting_update ) continue;

            type        = ptile->type;
            numvertices = tile_dict[type].numvertices;
            vertex      = ptile->vrtstart;

            // copy the 1st 4 vertices
            for ( ivrt = 0; ivrt < 4; ivrt++, vertex++ )
            {
                light = ptile->lcache[ivrt];

                light = CLIP( light, 0.0f, 255.0f );
                ptmem->clst[vertex][RR] =
                    ptmem->clst[vertex][GG] =
                        ptmem->clst[vertex][BB] = light * INV_FF;
            };

            for ( /* nothing */ ; ivrt < numvertices; ivrt++, vertex++ )
            {
                light = 0;
                ego_tile_mem::interpolate_vertex( ptmem, fan, ptmem->plst[vertex], &light );

                light = CLIP( light, 0.0f, 255.0f );
                ptmem->clst[vertex][RR] =
                    ptmem->clst[vertex][GG] =
                        ptmem->clst[vertex][BB] = light * INV_FF;
            };

            // clear out the deltas
            SDL_memset( ptile->d1_cache, 0, sizeof( ptile->d1_cache ) );
            SDL_memset( ptile->d2_cache, 0, sizeof( ptile->d2_cache ) );

            // untag this tile
            ptile->needs_lighting_update = bfalse;
        }
    }
}

//--------------------------------------------------------------------------------------------
float get_ambient_level()
{
    /// \author BB
    /// \details  get the actual global ambient level

    float glob_amb, min_amb;

    glob_amb = 0.0f;
    min_amb  = 0.0f;
    if ( gfx.usefaredge )
    {
        glob_amb = light_a * 255.0f;
    }
    else
    {
        glob_amb = light_a * 32.0f;
    }

    // determine the minimum ambient, based on darkvision
    min_amb = INVISIBLE;
    if ( local_stats.seedark_level > 0 )
    {
        min_amb = 52.0f * light_a * ( 1 + local_stats.seedark_level );
    }

    return SDL_max( glob_amb, min_amb );
}

//--------------------------------------------------------------------------------------------
bool_t sum_global_lighting( lighting_vector_t lighting )
{
    /// \author BB
    /// \details  do ambient lighting. if the module is inside, the ambient lighting
    /// is reduced by up to a factor of 8. It is still kept just high enough
    /// so that ordinary objects will not be made invisible. This was breaking some of the AIs

    int cnt;
    float glob_amb;

    if ( NULL == lighting ) return bfalse;

    glob_amb = get_ambient_level();

    for ( cnt = 0; cnt < LVEC_AMB; cnt++ )
    {
        lighting[cnt] = 0.0f;
    }
    lighting[LVEC_AMB] = glob_amb;

    if ( !gfx.usefaredge ) return btrue;

    // do "outside" directional lighting (i.e. sunlight)
    lighting_vector_sum( lighting, light_nrm, light_d * 255, 0.0f );

    return btrue;
}

//--------------------------------------------------------------------------------------------
// SEMI OBSOLETE FUNCTIONS
//--------------------------------------------------------------------------------------------
//void draw_cursor()
//{
//    /// \author ZF
//    /// \details This function implements a mouse cursor
//
//    if ( cursor.x < 6 )  cursor.x = 6;
//    if ( cursor.x > sdl_scr.x - 16 )  cursor.x = sdl_scr.x - 16;
//
//    if ( cursor.y < 8 )  cursor.y = 8;
//    if ( cursor.y > sdl_scr.y - 24 )  cursor.y = sdl_scr.y - 24;
//
//    // Needed to setup text mode
//    gfx_begin_text();
//    {
//        draw_one_font( 95, cursor.x - 5, cursor.y - 7 );
//    }
//    // Needed when done with text mode
//    gfx_end_text();
//}

//--------------------------------------------------------------------------------------------
void gfx_make_dynalist( ego_camera * pcam )
{
    /// \author ZZ
    /// \details  This function figures out which particles are visible, and it sets up dynamic
    ///    lighting

    int tnc, slot;
    fvec3_t  vdist;
    float    distance;

    // Don't really make a list, just set to visible or not
    dyna_list_count = 0;
    dyna_distancetobeat = 1e12;
    PRT_BEGIN_LOOP_ALLOCATED( iprt, pprt )
    {
        pprt->inview = bfalse;

        if ( !ego_mpd::grid_is_valid( PMesh, pprt->onwhichgrid ) ) continue;

        pprt->inview = PMesh->tmem.tile_list[pprt->onwhichgrid].inrenderlist;

        // Set up the lights we need
        if ( !pprt->dynalight.on ) continue;

        vdist = fvec3_sub( ego_prt::get_pos_v( pprt ), pcam->track_pos.v );

        distance = vdist.x * vdist.x + vdist.y * vdist.y + vdist.z * vdist.z;
        if ( distance < dyna_distancetobeat )
        {
            bool_t found = bfalse;
            if ( dyna_list_count < gfx.dyna_list_max )
            {
                // Just add the light
                slot = dyna_list_count;
                dyna_list[slot].distance = distance;
                dyna_list_count++;
                found = btrue;
            }
            else
            {
                // Overwrite the worst one
                slot = 0;
                dyna_distancetobeat = dyna_list[0].distance;
                for ( tnc = 1; tnc < gfx.dyna_list_max; tnc++ )
                {
                    if ( dyna_list[tnc].distance > dyna_distancetobeat )
                    {
                        slot = tnc;
                        found = btrue;
                    }
                }

                if ( found )
                {
                    dyna_list[slot].distance = distance;

                    // Find the new distance to beat
                    dyna_distancetobeat = dyna_list[0].distance;
                    for ( tnc = 1; tnc < gfx.dyna_list_max; tnc++ )
                    {
                        if ( dyna_list[tnc].distance > dyna_distancetobeat )
                        {
                            dyna_distancetobeat = dyna_list[tnc].distance;
                        }
                    }
                }
            }

            if ( found )
            {
                dyna_list[slot].pos     = ego_prt::get_pos( pprt );
                dyna_list[slot].level   = pprt->dynalight.level;
                dyna_list[slot].falloff = pprt->dynalight.falloff;
            }
        }
    }
    PRT_END_LOOP();
}

//--------------------------------------------------------------------------------------------
void do_grid_lighting( ego_mpd * pmesh, ego_camera * pcam )
{
    /// \author ZZ
    /// \details  Do all tile lighting, dynamic and global

    int   cnt, tnc, fan, entry;
    int ix, iy;
    float x0, y0, local_keep;
    bool_t needs_dynalight;

    lighting_vector_t global_lighting;

    int                  reg_count;
    ego_dynalight_registry reg[MAX_DYNA];

    ego_frect_t mesh_bound, light_bound;

    ego_mpd_info  * pinfo;
    ego_grid_mem      * pgmem;
    ego_tile_mem      * ptmem;
    ego_grid_info * glist;

    if ( NULL == pmesh ) return;
    pinfo = &( pmesh->info );
    pgmem = &( pmesh->gmem );
    ptmem = &( pmesh->tmem );

    glist = pgmem->grid_list;

    // refresh the dynamic light list
    gfx_make_dynalist( pcam );

    // find a bounding box for the "frustum"
    mesh_bound.xmin = pgmem->edge_x;
    mesh_bound.xmax = 0;
    mesh_bound.ymin = pgmem->edge_y;
    mesh_bound.ymax = 0;
    for ( entry = 0; entry < renderlist.all_count; entry++ )
    {
        fan = renderlist.all[entry];
        if ( !ego_mpd::grid_is_valid( pmesh, fan ) ) continue;

        ix = fan % pinfo->tiles_x;
        iy = fan / pinfo->tiles_x;

        x0 = ix * GRID_SIZE;
        y0 = iy * GRID_SIZE;

        mesh_bound.xmin = SDL_min( mesh_bound.xmin, x0 - GRID_SIZE / 2 );
        mesh_bound.xmax = SDL_max( mesh_bound.xmax, x0 + GRID_SIZE / 2 );
        mesh_bound.ymin = SDL_min( mesh_bound.ymin, y0 - GRID_SIZE / 2 );
        mesh_bound.ymax = SDL_max( mesh_bound.ymax, y0 + GRID_SIZE / 2 );
    }

    if ( mesh_bound.xmin >= mesh_bound.xmax || mesh_bound.ymin >= mesh_bound.ymax ) return;

    // make bounding boxes for each dynamic light
    reg_count = 0;
    light_bound.xmin = pgmem->edge_x;
    light_bound.xmax = 0;
    light_bound.ymin = pgmem->edge_y;
    light_bound.ymax = 0;
    for ( cnt = 0; cnt < dyna_list_count; cnt++ )
    {
        float radius;
        ego_frect_t ftmp;

        ego_dynalight * pdyna = dyna_list + cnt;

        if ( pdyna->falloff <= 0 ) continue;

        radius = SQRT( pdyna->falloff * 765.0f / 2.0f );

        ftmp.xmin = FLOOR(( pdyna->pos.x - radius ) / GRID_SIZE ) * GRID_SIZE;
        ftmp.xmax = CEIL(( pdyna->pos.x + radius ) / GRID_SIZE ) * GRID_SIZE;
        ftmp.ymin = FLOOR(( pdyna->pos.y - radius ) / GRID_SIZE ) * GRID_SIZE;
        ftmp.ymax = CEIL(( pdyna->pos.y + radius ) / GRID_SIZE ) * GRID_SIZE;

        // check to see if it intersects the "frustum"
        if ( ftmp.xmin <= mesh_bound.xmax && ftmp.xmax >= mesh_bound.xmin )
        {
            if ( ftmp.ymin <= mesh_bound.ymax && ftmp.ymax >= mesh_bound.ymin )
            {
                reg[reg_count].bound     = ftmp;
                reg[reg_count].reference = cnt;
                reg_count++;

                // determine the maximum bounding box that encloses all valid lights
                light_bound.xmin = SDL_min( light_bound.xmin, ftmp.xmin );
                light_bound.xmax = SDL_max( light_bound.xmax, ftmp.xmax );
                light_bound.ymin = SDL_min( light_bound.ymin, ftmp.ymin );
                light_bound.ymax = SDL_max( light_bound.ymax, ftmp.ymax );
            }
        }
    }

    needs_dynalight = btrue;
    if ( 0 == reg_count || light_bound.xmin >= light_bound.xmax || light_bound.ymin >= light_bound.ymax )
    {
        needs_dynalight = bfalse;
    }

    // sum up the lighting from global sources
    sum_global_lighting( global_lighting );

    // make the grids update their lighting every 4 frames
    local_keep = POW( dynalight_keep, 4 );

    // Add to base light level in normal mode
    for ( entry = 0; entry < renderlist.all_count; entry++ )
    {
        bool_t resist_lighting_calculation = btrue;

        // grab each grid box in the "frustum"
        fan = renderlist.all[entry];
        if ( !ego_mpd::grid_is_valid( pmesh, fan ) ) continue;

        ix = fan % pinfo->tiles_x;
        iy = fan / pinfo->tiles_x;

        // Resist the lighting calculation?
        // This is a speedup for lighting calculations so that
        // not every light-tile calculation is done every single frame
        resist_lighting_calculation = ( 0 != ((( ix ^ iy ) + frame_all ) & 0x03 ) );

        if ( !resist_lighting_calculation )
        {
            ego_lighting_cache * pcache_old;
            ego_lighting_cache   cache_new;

            int    dynalight_count = 0;

            // this is not a "bad" grid box, so grab the lighting info
            pcache_old = &( glist[fan].cache );

            lighting_cache_init( &cache_new );

            // copy the global lighting
            for ( tnc = 0; tnc < LIGHTING_VEC_SIZE; tnc++ )
            {
                cache_new.low.lighting[tnc] = global_lighting[tnc];
                cache_new.hgh.lighting[tnc] = global_lighting[tnc];
            };

            // do we need any dynamic lighting at all?
            if ( gfx.shading != GL_FLAT && needs_dynalight )
            {
                // calculate the local lighting

                ego_frect_t fgrid_rect;

                x0 = ix * GRID_SIZE;
                y0 = iy * GRID_SIZE;

                // check this grid vertex relative to the measured light_bound
                fgrid_rect.xmin = x0 - GRID_SIZE / 2;
                fgrid_rect.xmax = x0 + GRID_SIZE / 2;
                fgrid_rect.ymin = y0 - GRID_SIZE / 2;
                fgrid_rect.ymax = y0 + GRID_SIZE / 2;

                // check the bounding box of this grid vs. the bounding box of the lighting
                if ( fgrid_rect.xmin <= light_bound.xmax && fgrid_rect.xmax >= light_bound.xmin )
                {
                    if ( fgrid_rect.ymin <= light_bound.ymax && fgrid_rect.ymax >= light_bound.ymin )
                    {
                        // this grid has dynamic lighting. add it.
                        for ( cnt = 0; cnt < reg_count; cnt++ )
                        {
                            fvec3_t       nrm;
                            ego_dynalight * pdyna;

                            // does this dynamic light intersects this grid?
                            if ( fgrid_rect.xmin > reg[cnt].bound.xmax || fgrid_rect.xmax < reg[cnt].bound.xmin ) continue;
                            if ( fgrid_rect.ymin > reg[cnt].bound.ymax || fgrid_rect.ymax < reg[cnt].bound.ymin ) continue;

                            dynalight_count++;

                            // this should be a valid intersection, so proceed
                            tnc = reg[cnt].reference;
                            pdyna = dyna_list + tnc;

                            nrm.x = pdyna->pos.x - x0;
                            nrm.y = pdyna->pos.y - y0;
                            nrm.z = pdyna->pos.z - ptmem->bbox.mins[ZZ];
                            sum_dyna_lighting( pdyna, cache_new.low.lighting, nrm.v );

                            nrm.z = pdyna->pos.z - ptmem->bbox.maxs[ZZ];
                            sum_dyna_lighting( pdyna, cache_new.hgh.lighting, nrm.v );
                        }
                    }
                }
            }

            // blend in the global lighting every single time

            // average this in with the existing lighting
            lighting_cache_blend( pcache_old, &cache_new, local_keep );

            // find the max intensity
            lighting_cache_max_light( pcache_old );
        }
    }
}

//--------------------------------------------------------------------------------------------
void render_water( ego_renderlist * prlist )
{
    /// \author ZZ
    /// \details  This function draws all of the water fans

    int cnt;

    // Bottom layer first
    if ( gfx.draw_water_1 /* && water.layer[1].z > -water.layer[1].amp */ )
    {
        for ( cnt = 0; cnt < prlist->wat_count; cnt++ )
        {
            render_water_fan( prlist->pmesh, prlist->wat[cnt], 1 );
        }
    }

    // Top layer second
    if ( gfx.draw_water_0 /* && water.layer[0].z > -water.layer[0].amp */ )
    {
        for ( cnt = 0; cnt < prlist->wat_count; cnt++ )
        {
            render_water_fan( prlist->pmesh, prlist->wat[cnt], 0 );
        }
    }
}

//--------------------------------------------------------------------------------------------
void gfx_reload_all_textures()
{
    TxTitleImage_reload_all();
    TxTexture_reload_all();
}

//--------------------------------------------------------------------------------------------
void draw_one_quad( oglx_texture_t * ptex, const ego_frect_t & scr_rect, const ego_frect_t & tx_rect, const bool_t use_alpha )
{
    ATTRIB_PUSH( __FUNCTION__, GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT )
    {
        if ( NULL == ptex || INVALID_GL_ID == ptex->base.binding )
        {
            GL_DEBUG( glDisable )( GL_TEXTURE_1D );                               // GL_ENABLE_BIT
            GL_DEBUG( glDisable )( GL_TEXTURE_2D );                               // GL_ENABLE_BIT
        }
        else
        {
            GL_DEBUG( glEnable )( ptex->base.target );                               // GL_ENABLE_BIT
            oglx_texture_Bind( ptex );
        }

        if ( use_alpha )
        {
            GL_DEBUG( glEnable )( GL_BLEND );                                 // GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT
            GL_DEBUG( glBlendFunc )( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );  // GL_COLOR_BUFFER_BIT
        }
        else
        {
            GL_DEBUG( glDisable )( GL_BLEND );                                 // GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT
        }

        GL_DEBUG_BEGIN( GL_QUADS );
        {
            GL_DEBUG( glTexCoord2f )( tx_rect.xmin, tx_rect.ymax ); GL_DEBUG( glVertex2f )( scr_rect.xmin, scr_rect.ymax );
            GL_DEBUG( glTexCoord2f )( tx_rect.xmax, tx_rect.ymax ); GL_DEBUG( glVertex2f )( scr_rect.xmax, scr_rect.ymax );
            GL_DEBUG( glTexCoord2f )( tx_rect.xmax, tx_rect.ymin ); GL_DEBUG( glVertex2f )( scr_rect.xmax, scr_rect.ymin );
            GL_DEBUG( glTexCoord2f )( tx_rect.xmin, tx_rect.ymin ); GL_DEBUG( glVertex2f )( scr_rect.xmin, scr_rect.ymin );
        }
        GL_DEBUG_END();
    }
    ATTRIB_POP( __FUNCTION__ );
}

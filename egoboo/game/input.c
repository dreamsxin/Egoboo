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

/// @file input.c
/// @brief Keyboard, mouse, and joystick handling code.
/// @details

#include "input.h"

#include "controls_file.h"

#if defined(USE_LUA_CONSOLE)
#    include "lua_console.h"
#else
#    include "egoboo_console.h"
#endif

#include "ui.h"
#include "log.h"
#include "network.h"
#include "menu.h"
#include "graphic.h"
#include "camera.h"

#include "egoboo_setup.h"
#include "egoboo_fileutil.h"
#include "egoboo_strutil.h"
#include "egoboo_math.h"
#include "egoboo.h"

#include "SDL_extensions.h"

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

input_device_t    controls[MAX_LOCAL_PLAYERS];                  //Up to 4 local players

//Raw input devices
mouse_t           mous;
keyboard_t        keyb;
device_joystick_t joy[MAXJOYSTICK];
cursor_t          cursor = {0, 0, bfalse, bfalse, bfalse, bfalse};

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

static void input_read_mouse();
static void input_read_keyboard();
static void input_read_joysticks();
static void input_read_joystick( int which );

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
void input_device_init( input_device_t * pdevice )
{
    INPUT_DEVICE type;
    if ( NULL == pdevice ) return;

    type = pdevice->device_type;
    
    memset( pdevice, 0, sizeof( *pdevice ) );
    pdevice->sustain = 0.58f;
    pdevice->cover   = 1.0f - pdevice->sustain;
    pdevice->device_type = type;
}

//--------------------------------------------------------------------------------------------
void input_init_keyboard()
{
    // set up the keyboard
    memset( &keyb, 0, sizeof( keyb ) );
    init_scancodes();
    keyb.on        = btrue;
    keyb.count     = 0;
    keyb.state_ptr = NULL;
}

//--------------------------------------------------------------------------------------------
void input_init_mouse()
{
    /// @details BB@> set up the mouse
    memset( &mous, 0, sizeof( mous ) );
    mous.on      = btrue;
    mous.sense   = 12; //24;
}

//--------------------------------------------------------------------------------------------
void input_init_joysticks()
{
    /// @details BB@> init the joysticks

    int i;

    for ( i = 0; i < MAXJOYSTICK; i++ )
    {
        memset( joy + i, 0, sizeof( joy[i] ) );

        if ( i < SDL_NumJoysticks() )
        {
            joy[i].sdl_ptr = SDL_JoystickOpen( i );
            joy[i].on      = ( NULL != joy[i].sdl_ptr );
        }
    }
}

//--------------------------------------------------------------------------------------------
void input_init()
{
    /// @details BB@> initialize the inputs

    log_info( "Intializing SDL Joystick... " );
    if ( SDL_InitSubSystem( SDL_INIT_JOYSTICK ) < 0 )
    {
        log_message( "Failed!\n" );
    }
    else
    {
        log_message( "Success!\n" );
    }

    input_init_keyboard();
    input_init_mouse();
    input_init_joysticks();
}

//--------------------------------------------------------------------------------------------
void input_read_mouse()
{
    int x, y, b;

    if ( process_running( PROC_PBASE( MProc ) ) )
    {
        b = SDL_GetMouseState( &x, &y );
    }
    else
    {
        b = SDL_GetRelativeMouseState( &x, &y );

        //Move mouse to the center of the screen since SDL does not detect motion outside the window
        if( !cfg.dev_mode ) SDL_WarpMouse( GFX_WIDTH>>1, GFX_HEIGHT>>1 );
    }

    mous.x = -x; // mous.x and mous.y are the wrong type to use in above call
    mous.y = -y;
    mous.button[0] = ( b & SDL_BUTTON( 1 ) ) ? 1 : 0;
    mous.button[1] = ( b & SDL_BUTTON( 3 ) ) ? 1 : 0;
    mous.button[2] = ( b & SDL_BUTTON( 2 ) ) ? 1 : 0; // Middle is 2 on SDL
    mous.button[3] = ( b & SDL_BUTTON( 4 ) ) ? 1 : 0;

    // Mouse mask
    mous.b = ( mous.button[3] << 3 ) | ( mous.button[2] << 2 ) | ( mous.button[1] << 1 ) | ( mous.button[0] << 0 );
}

//--------------------------------------------------------------------------------------------
void input_read_keyboard()
{
    keyb.state_ptr = SDL_GetKeyState( &keyb.count );
}

//--------------------------------------------------------------------------------------------
void input_read_joystick( int which )
{
    int dead_zone = 0x8000 / 10;
    int i, button_count, x, y;
    device_joystick_t * pjoy;

    if ( !joy[which].on ) return;

    pjoy = joy + which;

    // get the raw values
    x = SDL_JoystickGetAxis( pjoy->sdl_ptr, 0 );
    y = SDL_JoystickGetAxis( pjoy->sdl_ptr, 1 );

    // make a dead zone
    if ( x > dead_zone ) x -= dead_zone;
    else if ( x < -dead_zone ) x += dead_zone;
    else x = 0;

    if ( y > dead_zone ) y -= dead_zone;
    else if ( y < -dead_zone ) y += dead_zone;
    else y = 0;

    // store the values
    pjoy->x = x / ( float )( 0x8000 - dead_zone );
    pjoy->y = y / ( float )( 0x8000 - dead_zone );

    // get buttons
    button_count = SDL_JoystickNumButtons( pjoy->sdl_ptr );
    button_count = MIN( JOYBUTTON, button_count );
    for ( i = 0; i < button_count; i++ )
    {
        pjoy->button[i] = SDL_JoystickGetButton( pjoy->sdl_ptr, i );
    }

    // buttonmask mask
    pjoy->b = 0;
    for ( i = 0; i < button_count; i++ )
    {
        SET_BIT( pjoy->b, pjoy->button[i] << i );
    }

    return;
}

//--------------------------------------------------------------------------------------------
void input_read_joysticks()
{
    int cnt;

    SDL_JoystickUpdate();
    for ( cnt = 0; cnt < MAXJOYSTICK; cnt++ )
    {
        input_read_joystick( cnt );
    }
}

//--------------------------------------------------------------------------------------------
void input_read()
{
    /// @details ZZ@> This function gets all the current player input states

    SDL_Event evt;

    if ( EMPTY_BIT_FIELD == SDL_WasInit( SDL_INIT_EVERYTHING ) ) return;

    // Run through SDL's event loop to get info in the way that we want
    // it for the Gui code
    while ( SDL_PollEvent( &evt ) )
    {
        if ( cfg.dev_mode )
        {
            if ( NULL == egoboo_console_handle_events( &evt ) )
            {
                continue;
            }
        }

        ui_handleSDLEvent( &evt );

        switch ( evt.type )
        {
            case SDL_ACTIVEEVENT:
                // the application has gained or lost some form of focus
                if ( SDL_APPACTIVE == evt.active.type && 1 == evt.active.gain )
                {
                    // the application has recovered from being minimized
                    // the textures need to be reloaded into OpenGL memory

                    gfx_reload_all_textures();
                }
                break;

            case SDL_VIDEORESIZE:
                if ( SDL_VIDEORESIZE == evt.resize.type )
                {
                    // The video has been resized.
                    // If the game is active, some camera info mught need to be recalculated
                    // and possibly the auto-formatting for the menu system and the ui system
                    // The ui will handle its own issues.

                    // grab all the new SDL screen info
                    SDLX_Get_Screen_Info( &sdl_scr, SDL_FALSE );

                    // fix the camera rotation angles to estimate what is in-view
                    camera_rotmesh__init();
                }
                break;

            case SDL_VIDEOEXPOSE:
                // something has been done to the screen and it needs to be re-drawn.
                // For instance, a window above the app window was moved. This has no
                // effect on the game at the moment.
                break;

            case SDL_MOUSEBUTTONDOWN:
                if ( evt.button.button == SDL_BUTTON_WHEELUP )
                {
                    cursor.z++;
                    cursor.wheel_event = btrue;
                }
                else if ( evt.button.button == SDL_BUTTON_WHEELDOWN )
                {
                    cursor.z--;
                    cursor.wheel_event = btrue;
                }
                else
                {
                    cursor.pending_click = btrue;
                }
                break;

            case SDL_MOUSEBUTTONUP:
                cursor.pending_click = bfalse;
                break;

            case SDL_MOUSEMOTION:
                cursor.x = evt.motion.x;
                cursor.y = evt.motion.y;
                break;

            case SDL_QUIT:
                //Someone pressed the little X in the corner while running windowed mode
                exit(0);
                break;

                // use this loop to grab any console-mode entry from the keyboard
            case SDL_KEYDOWN:
                {
                    Uint32 kmod;
                    bool_t is_alt, is_shift;

                    kmod = SDL_GetModState();

                    is_alt   = HAS_SOME_BITS( kmod, KMOD_ALT | KMOD_CTRL );
                    is_shift = HAS_SOME_BITS( kmod, KMOD_SHIFT );
                    if ( console_mode )
                    {
                        if ( !is_alt )
                        {
                            if ( SDLK_RETURN == evt.key.keysym.sym || SDLK_KP_ENTER == evt.key.keysym.sym )
                            {
                                keyb.buffer[keyb.buffer_count] = CSTR_END;
                                console_mode = bfalse;
                                console_done = btrue;
                                SDL_EnableKeyRepeat( 0, SDL_DEFAULT_REPEAT_DELAY );
                            }
                            else if ( SDLK_ESCAPE == evt.key.keysym.sym )
                            {
                                // reset the keyboard buffer
                                console_mode = bfalse;
                                console_done = bfalse;
                                keyb.buffer_count = 0;
                                keyb.buffer[0] = CSTR_END;
                                SDL_EnableKeyRepeat( 0, SDL_DEFAULT_REPEAT_DELAY );
                            }
                            else if ( SDLK_BACKSPACE == evt.key.keysym.sym )
                            {
                                if ( keyb.buffer_count > 0 )
                                {
                                    keyb.buffer_count--;
                                }
                                keyb.buffer[keyb.buffer_count] = CSTR_END;
                            }
                            else if ( keyb.buffer_count < KEYB_BUFFER_SIZE )
                            {
                                if ( is_shift )
                                {
                                    keyb.buffer[keyb.buffer_count++] = scancode_to_ascii_shift[evt.key.keysym.sym];
                                }
                                else
                                {
                                    keyb.buffer[keyb.buffer_count++] = scancode_to_ascii[evt.key.keysym.sym];
                                }
                                keyb.buffer[keyb.buffer_count] = CSTR_END;
                            }
                        }
                    }
                    else
                    {
                        if ( SDLK_ESCAPE == evt.key.keysym.sym )
                        {
                            // tell the main process about the escape request
                            EProc->escape_requested = btrue;
                        }
                    }
                }
                break;
        }
    }

    // Get immediate mode state for the rest of the game
    input_read_keyboard();
    input_read_mouse();
    input_read_joysticks();
}

//--------------------------------------------------------------------------------------------
BIT_FIELD input_get_buttonmask( input_device_t *pdevice )
{
    BIT_FIELD buttonmask = EMPTY_BIT_FIELD;

    // make sure the idevice is valid
    if ( NULL == pdevice ) return EMPTY_BIT_FIELD;

    switch ( pdevice->device_type )
    {
        case INPUT_DEVICE_KEYBOARD: buttonmask = EMPTY_BIT_FIELD; break;
        case INPUT_DEVICE_MOUSE:    buttonmask = mous.b;   break;
        case INPUT_DEVICE_JOY_A:    buttonmask = joy[0].b; break;
        case INPUT_DEVICE_JOY_B:    buttonmask = joy[1].b; break;
    }

    return buttonmask;
}

//--------------------------------------------------------------------------------------------
bool_t input_is_enabled( input_device_t *pdevice )
{
    //@details ZF@> This determines if the specified input device is enabled or not

    // make sure the idevice is valid
    if ( NULL == pdevice ) return bfalse;

    switch ( pdevice->device_type )
    {
        case INPUT_DEVICE_KEYBOARD: return keyb.on;
        case INPUT_DEVICE_MOUSE:    return mous.on;
        case INPUT_DEVICE_JOY_A:    return joy[0].on;
        case INPUT_DEVICE_JOY_B:    return joy[1].on;
    }

    return bfalse;
}


//--------------------------------------------------------------------------------------------
bool_t control_is_pressed( input_device_t *pdevice, CONTROL_BUTTON icontrol )
{
    /// @details ZZ@> This function returns btrue if the given icontrol is pressed...

    bool_t      retval = bfalse;
    control_t   * pcontrol;

    // make sure the idevice is valid
    if ( NULL == pdevice ) return bfalse;
    pcontrol = pdevice->control + icontrol;
    
    if ( INPUT_DEVICE_KEYBOARD == pdevice->device_type || pcontrol->is_key )
    {
        retval = SDLKEYDOWN( pcontrol->tag );
    }
    else
    {
        retval = ( input_get_buttonmask( pdevice ) == pcontrol->tag );
    }

    return retval;
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
void cursor_reset()
{
    cursor.pressed       = bfalse;
    cursor.clicked       = bfalse;
    cursor.pending_click = bfalse;
    cursor.wheel_event   = bfalse;
    cursor.z             = 0;
}

//--------------------------------------------------------------------------------------------
void cursor_finish_wheel_event()
{
    cursor.wheel_event   = bfalse;
    cursor.z             = 0;
}

//--------------------------------------------------------------------------------------------
bool_t cursor_wheel_event_pending()
{
    if ( cursor.wheel_event && 0 == cursor.z )
    {
        cursor.wheel_event = bfalse;
    }

    return cursor.wheel_event;
}

//--------------------------------------------------------------------------------------------
INPUT_DEVICE translate_string_to_input_type( const char *string )
{
    /// @details ZF@> This function turns a string into a input type (mouse, keyboard, joystick, etc.)

    if      ( 0 == strcmp( string, "KEYBOARD" ) )   return INPUT_DEVICE_KEYBOARD;
    else if ( 0 == strcmp( string, "MOUSE" ) )      return INPUT_DEVICE_MOUSE;
    else if ( 0 == strcmp( string, "JOYSTICK_A" ) ) return INPUT_DEVICE_JOY_A;
    else if ( 0 == strcmp( string, "JOYSTICK_B" ) ) return INPUT_DEVICE_JOY_B;

    // No matches
    log_warning("Unknown device controller parsed (%s) - defaulted to Keyboard\n", string);
    return INPUT_DEVICE_KEYBOARD;
}

//--------------------------------------------------------------------------------------------
const char* translate_input_type_to_string( const INPUT_DEVICE type )
{
    /// @details ZF@> This function turns a input type into a string

    if( type == INPUT_DEVICE_KEYBOARD )   return "KEYBOARD";
    else if( type == INPUT_DEVICE_MOUSE ) return "MOUSE";
    else if( type == INPUT_DEVICE_JOY_A ) return "JOYSTICK_A";
    else if( type == INPUT_DEVICE_JOY_B ) return "JOYSTICK_B";

    // No matches
    return "UNKNOWN";
}

//********************************************************************************************
//*
//*    This file is part of Cartman.
//*
//*    Cartman is free software: you can redistribute it and/or modify it
//*    under the terms of the GNU General Public License as published by
//*    the Free Software Foundation, either version 3 of the License, or
//*    (at your option) any later version.
//*
//*    Cartman is distributed in the hope that it will be useful, but
//*    WITHOUT ANY WARRANTY; without even the implied warranty of
//*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//*    General Public License for more details.
//*
//*    You should have received a copy of the GNU General Public License
//*    along with Cartman.  If not, see <http://www.gnu.org/licenses/>.
//*
//********************************************************************************************

#include "egolib/egolib.h"

#include "cartman/cartman_gfx.h"
#include "cartman/cartman.h"
#include "cartman/cartman_map.h"
#include "cartman/cartman_gui.h"
#include "cartman/cartman_functions.h"
#include "cartman/cartman_select.h"
#include "cartman/cartman_math.h"
#include "egolib/FileFormats/Globals.hpp"
#include "egolib/Graphics/GraphicsSystem.hpp"
#include "cartman/Clocks.h"

//--------------------------------------------------------------------------------------------

const Ego::Math::Colour4f WHITE = Ego::Math::Colour4f::white();
const Ego::Math::Colour4f BLACK = Ego::Math::Colour4f::black();

std::shared_ptr<Ego::Font> gfx_font_ptr = NULL;

//--------------------------------------------------------------------------------------------

camera_t cam;

Sint16          damagetileparttype;
short           damagetilepartand;
short           damagetilesound;
short           damagetilesoundtime;
Uint16          damagetilemindistance;
int             damagetileamount = 256;                           // Amount of damage
Uint8           damagetiletype  = DAMAGE_FIRE;                      // Type of damage

int    animtileupdateand   = 7;
Uint16 animtileframeand    = 3;
Uint16 animtilebaseand     = ( Uint16 )( ~3 );
Uint16 biganimtileframeand = 7;
Uint16 biganimtilebaseand  = ( Uint16 )( ~7 );
Uint16 animtileframeadd    = 0;

int     numsmalltile = 0;   //
int     numbigtile = 0;     //

//--------------------------------------------------------------------------------------------

static void get_small_tiles( SDL_Surface* bmpload );
static void get_big_tiles( SDL_Surface* bmpload );
static void  gfx_init_ogl();

//--------------------------------------------------------------------------------------------

void gfx_system_begin()
{
    Ego::App::initialize();

    // Set the window title.
    Ego::GraphicsSystem::window->setTitle(NAME " " VERSION_STR);

    gfx_init_ogl();   
    gfx_font_ptr = Ego::FontManager::get().loadFont("editor/pc8x8.fon", 12);
}

//--------------------------------------------------------------------------------------------

void gfx_system_end()
{
    gfx_font_ptr.reset();
    Ego::App::uninitialize();
}

//--------------------------------------------------------------------------------------------

std::shared_ptr<Ego::Texture> tiny_tile_at( cartman_mpd_t * pmesh, Index2D index2d )
{
    Uint16 tx_bits, basetile;
    Uint8 fantype, fx;

    cartman_mpd_tile_t * pfan;

    tx_bits = 0;
    fantype = 0;
    fx = 0;

    if ( NULL == pmesh ) pmesh = &mesh;

    if ( index2d.x() < 0 || index2d.x() >= pmesh->info.getTileCountX() || index2d.y() < 0 || index2d.y() >= pmesh->info.getTileCountY() )
    {
        return NULL;
    }

    pfan = pmesh->get_pfan(index2d);
    if ( NULL == pfan ) return NULL;

    if ( TILE_IS_FANOFF( pfan->tx_bits ) )
    {
        return NULL;
    }

    tx_bits = pfan->tx_bits;
    fantype = pfan->type;
    fx      = pfan->fx;

    if ( HAS_BITS( fx, MAPFX_ANIM ) )
    {
        animtileframeadd = ( Clocks::timePassed<Time::Unit::Ticks,int>() >> 3 ) & 3;
        if ( fantype >= tile_dict.offset )
        {
            // Big tiles
            basetile = tx_bits & biganimtilebaseand;// Animation set
            tx_bits += ( animtileframeadd << 1 );   // Animated tx_bits
            tx_bits = ( tx_bits & biganimtileframeand ) + basetile;
        }
        else
        {
            // Small tiles
            basetile = tx_bits & animtilebaseand;// Animation set
            tx_bits += animtileframeadd;       // Animated tx_bits
            tx_bits = ( tx_bits & animtileframeand ) + basetile;
        }
    }

    // remove any of the upper bit information
    tx_bits &= 0xFF;

    if ( fantype >= tile_dict.offset )
    {
        return Resources::get().tx_bigtile[tx_bits];
    }
    else
    {
        return Resources::get().tx_smalltile[tx_bits];
    }
}

//--------------------------------------------------------------------------------------------

std::shared_ptr<Ego::Texture> tile_at( cartman_mpd_t * pmesh, int fan )
{
    int    img;
    Uint16 img_base;
    Uint8  type, fx;

    cartman_mpd_tile_t   * pfan   = NULL;

    if ( NULL == pmesh ) pmesh = &mesh;

    pfan = CART_MPD_FAN_PTR( pmesh, fan );
    if ( NULL == pfan ) return NULL;

    if ( TILE_IS_FANOFF( pfan->tx_bits ) )
    {
        return NULL;
    }

    img = TILE_GET_LOWER_BITS( pfan->tx_bits );
    type = pfan->type;
    fx = pfan->fx;

    if ( HAS_BITS( fx, MAPFX_ANIM ) )
    {
        animtileframeadd = ( Clocks::timePassed<Time::Unit::Ticks,int>() >> 3 ) & 3;
        if ( type >= tile_dict.offset )
        {
            // Big tiles
            img_base = img & biganimtilebaseand;// Animation set
            img += ( animtileframeadd << 1 );   // Animated img
            img = ( img & biganimtileframeand ) + img_base;
        }
        else
        {
            // Small tiles
            img_base = img & animtilebaseand;  // Animation set
            img += animtileframeadd;           // Animated img
            img = ( img & animtileframeand ) + img_base;
        }
    }

    // remove any of the upper bit information
    img &= 0xFF;

    if ( type >= tile_dict.offset )
    {
        return Resources::get().tx_bigtile[img];
    }
    else
    {
        return Resources::get().tx_smalltile[img];
    }
}

//--------------------------------------------------------------------------------------------

void make_hitemap( cartman_mpd_t * pmesh )
{
    if ( NULL == pmesh ) pmesh = &mesh;

    Resources::get().bmphitemap = Ego::Graphics::SDL::createSurface( pmesh->info.getTileCountX() << 2, pmesh->info.getTileCountY() << 2 );
    if ( NULL == Resources::get().bmphitemap ) return;

    for (int pixy = 0, y = 16; pixy < ( pmesh->info.getTileCountY() << 2 ); pixy++, y += 32 )
    {
        for (int pixx = 0, x = 16; pixx < ( pmesh->info.getTileCountX() << 2 ); pixx++, x += 32 )
        {
            int level = ( pmesh->get_level(x, y) * 255 ) / pmesh->info.getEdgeZ();  // level is 0 to 255
            if ( level > 252 ) level = 252;

            cartman_mpd_tile_t *pfan = pmesh->get_pfan({pixx >> 2, pixy >> 2});
            if ( NULL == pfan ) continue;

            if ( HAS_BITS( pfan->fx, MAPFX_WALL ) ) level = 253;  // Wall
            if ( HAS_BITS( pfan->fx, MAPFX_IMPASS ) ) level = 254;   // Impass
            if ( HAS_BITS( pfan->fx, MAPFX_WALL ) && HAS_BITS( pfan->fx, MAPFX_IMPASS ) ) level = 255;   // Both

            Ego::Graphics::SDL::putPixel(Resources::get().bmphitemap, pixx, pixy, level );
        }
    }
}

//--------------------------------------------------------------------------------------------

void make_planmap( cartman_mpd_t * pmesh )
{
    int x, y, putx, puty;
    //SDL_Surface* bmptemp;

    //bmptemp = SDL_GL_createSurface(64, 64);
    //if(NULL != bmptemp)  return;

    if ( NULL == pmesh ) pmesh = &mesh;

    Resources::get().bmphitemap = Ego::Graphics::SDL::createSurface( pmesh->info.getTileCountX() * TINYXY, pmesh->info.getTileCountY() * TINYXY );

    SDL_FillRect( Resources::get().bmphitemap.get(), NULL, make_rgb(Resources::get().bmphitemap, Ego::Math::Colour3b::black()) );

    puty = 0;
    for ( y = 0; y < pmesh->info.getTileCountY(); y++ )
    {
        putx = 0;
        for ( x = 0; x < pmesh->info.getTileCountX(); x++ )
        {
            std::shared_ptr<Ego::Texture> tx_tile;
            tx_tile = tiny_tile_at(pmesh, {x, y});

            if ( NULL != tx_tile )
            {
                SDL_Rect dst = {static_cast<Sint16>(putx), static_cast<Sint16>(puty), TINYXY, TINYXY};
                cartman_BlitSurface(tx_tile->_source.get(), nullptr, Resources::get().bmphitemap.get(), &dst);
            }
            putx += TINYXY;
        }
        puty += TINYXY;
    }

    //SDL_SoftStretch(bmphitemap, NULL, bmptemp, NULL);
    //SDL_FreeSurface(bmphitemap);

    //bmphitemap = bmptemp;
}

//--------------------------------------------------------------------------------------------

void draw_top_fan( select_lst_t& plst, int fan, float zoom_hrz, float zoom_vrt )
{
    // ZZ> This function draws the line drawing preview of the tile type...
    //     A wireframe tile from a vertex connection window

    static Uint32 faketoreal[MAP_FAN_VERTICES_MAX];

    int cnt, stt, end, vert;
    Ego::Math::Colour4f color;
    float size;

    cart_vec_t vup    = { 0, 1, 0};
    cart_vec_t vright = { -1, 0, 0};
    cart_vec_t vpos;

	plst.synch_mesh(&mesh);

	cartman_mpd_t *pmesh = plst.get_mesh();
    if (!pmesh) return;

	const cartman_mpd_tile_t *pfan = CART_MPD_FAN_PTR( pmesh, fan );
    if (!pfan) return;

	tile_definition_t *pdef = tile_dict.get( pfan->type );
    if (!pdef) return;

	tile_line_data_t *plines = tile_dict_lines + pfan->type;

    if ( 0 == pdef->numvertices || pdef->numvertices > MAP_FAN_VERTICES_MAX ) return;

    color = make_rgba(255, 128, 128, 255);
    if ( pfan->type >= tile_dict.offset )
    {
        color = make_rgba(255, 255, 128, 128);
    }

    for ( cnt = 0, vert = pfan->vrtstart;
          cnt < pdef->numvertices && CHAINEND != vert;
          cnt++, vert = pmesh->vrt2[vert].next)
    {
        faketoreal[cnt] = vert;
    }

    glPushAttrib( GL_TEXTURE_BIT | GL_ENABLE_BIT );
    {
        Ego::Renderer::get().setColour(color);
        Ego::Renderer::get().getTextureUnit().setActivated(nullptr);

        glBegin( GL_LINES );
        {
            for ( cnt = 0; cnt < plines->count; cnt++ )
            {
                stt = faketoreal[plines->start[cnt]];
                end = faketoreal[plines->end[cnt]];

                glVertex3f(pmesh->vrt2[stt].x, pmesh->vrt2[stt].y, pmesh->vrt2[stt].z);
                glVertex3f(pmesh->vrt2[end].x, pmesh->vrt2[end].y, pmesh->vrt2[end].z);
            }
        }
        glEnd();
    }
    glPopAttrib();

    Ego::Renderer::get().setColour(Ego::Math::Colour4f::white());
    for ( cnt = 0; cnt < pdef->numvertices; cnt++ )
    {
        int point_size;

        vert = faketoreal[cnt];

        size = MAXPOINTSIZE * pmesh->vrt2[vert].z / (float)pmesh->info.getEdgeZ();
        if ( size < 0.0f ) size = 0.0f;
        if ( size > MAXPOINTSIZE ) size = MAXPOINTSIZE;

        point_size = 4.0f * POINT_SIZE( size ) / zoom_hrz;

        if ( point_size > 0 )
        {
            int select_rv;
            std::shared_ptr<Ego::Texture> tx_tmp;

            select_rv = plst.find(vert);
            if ( select_rv < 0 )
            {
                tx_tmp = Resources::get().tx_point;
            }
            else
            {
                tx_tmp = Resources::get().tx_pointon;
            }

            vpos[kX] = pmesh->vrt2[vert].x;
            vpos[kY] = pmesh->vrt2[vert].y;
            vpos[kZ] = pmesh->vrt2[vert].z;

            ogl_draw_sprite_3d( tx_tmp, vpos, vup, vright, point_size, point_size );
        }
    }
}

//--------------------------------------------------------------------------------------------

void draw_side_fan( select_lst_t& plst, int fan, float zoom_hrz, float zoom_vrt )
{
    // ZZ> This function draws the line drawing preview of the tile type...
    //     A wireframe tile from a vertex connection window ( Side view )

    static Uint32 faketoreal[MAP_FAN_VERTICES_MAX];

    cart_vec_t vup    = { 0, 0, 1};
    cart_vec_t vright = { 1, 0, 0};
    cart_vec_t vpos;

    int cnt, stt, end, vert;
    Ego::Math::Colour4f color;
    float size;
    float point_size;

	plst.synch_mesh(&mesh);

	cartman_mpd_t *pmesh = plst.get_mesh();
    if (!pmesh) return;

	cartman_mpd_tile_t *pfan = CART_MPD_FAN_PTR( pmesh, fan );
    if (!pfan) return;

	tile_definition_t *pdef = tile_dict.get( pfan->type );
    if (!pdef) return;

	tile_line_data_t *plines = tile_dict_lines + pfan->type;

    color = make_rgba(255, 128, 128, 255);
    if ( pfan->type >= tile_dict.offset )
    {
        color = make_rgba(255, 255, 128, 128);
    }

    for ( cnt = 0, vert = pfan->vrtstart;
          cnt < pdef->numvertices && CHAINEND != vert;
          cnt++, vert = pmesh->vrt2[vert].next )
    {
        faketoreal[cnt] = vert;
    }

    glPushAttrib( GL_TEXTURE_BIT | GL_ENABLE_BIT );
    {
        Ego::Renderer::get().setColour(color);
        Ego::Renderer::get().getTextureUnit().setActivated(nullptr);

        glBegin( GL_LINES );
        {
            for ( cnt = 0; cnt < plines->count; cnt++ )
            {
                stt = faketoreal[plines->start[cnt]];
                end = faketoreal[plines->end[cnt]];

                glVertex3f(pmesh->vrt2[stt].x, pmesh->vrt2[stt].y, pmesh->vrt2[stt].z);
                glVertex3f(pmesh->vrt2[end].x, pmesh->vrt2[end].y, pmesh->vrt2[end].z);
            }
        }
        glEnd();
    }
    glPopAttrib();

    size = 7;
    point_size = 4.0f * POINT_SIZE( size ) / zoom_hrz;

    Ego::Renderer::get().setColour(Ego::Math::Colour4f::white());

    for ( cnt = 0; cnt < pdef->numvertices; cnt++ )
    {
        int select_rv;
        std::shared_ptr<Ego::Texture> tx_tmp = NULL;

        vert = faketoreal[cnt];

        select_rv = plst.find( vert );
        if ( select_rv < 0 )
        {
            tx_tmp = Resources::get().tx_point;
        }
        else
        {
            tx_tmp = Resources::get().tx_pointon;
        }

        vpos[kX] = pmesh->vrt2[vert].x;
        vpos[kY] = pmesh->vrt2[vert].y;
        vpos[kZ] = pmesh->vrt2[vert].z;

        ogl_draw_sprite_3d( tx_tmp, vpos, vup, vright, point_size, point_size );
    }
}

//--------------------------------------------------------------------------------------------

void draw_schematic(std::shared_ptr<Cartman::Gui::Window> pwin, int fantype, int x, int y)
{
    // ZZ> This function draws the line drawing preview of the tile type...
    //     The wireframe on the left side of the theSurface.

    int cnt, stt, end;
    Ego::Math::Colour4f color;

    // aliases
    tile_line_data_t     * plines = NULL;
    tile_definition_t    * pdef   = NULL;

    pdef   = tile_dict.get(fantype );
    if ( NULL == pdef ) return;

    plines = tile_dict_lines + fantype;

    color = make_rgba(255, 128, 128, 255);
    if ( fantype >= tile_dict.offset )
    {
        color = make_rgba(255, 255, 128, 128);
    }

    glPushAttrib( GL_TEXTURE_BIT | GL_ENABLE_BIT );
    {
        Ego::Renderer::get().setColour(color);
        Ego::Renderer::get().getTextureUnit().setActivated(nullptr);

        glBegin( GL_LINES );
        {
            for ( cnt = 0; cnt < plines->count; cnt++ )
            {
                stt = plines->start[cnt];
                end = plines->end[cnt];

                glVertex2f( GRID_TO_POS( pdef->vertices[stt].grid_ix ) + x, GRID_TO_POS( pdef->vertices[stt].grid_iy ) + y );
                glVertex2f( GRID_TO_POS( pdef->vertices[end].grid_ix ) + x, GRID_TO_POS( pdef->vertices[end].grid_iy ) + y );
            }
        }
        glEnd();
    }
    glPopAttrib();
}

//--------------------------------------------------------------------------------------------

void draw_top_tile( float x0, float y0, int fan, std::shared_ptr<Ego::Texture> tx_tile, bool draw_tile, cartman_mpd_t * pmesh )
{
    static simple_vertex_t loc_vrt[4];

    const float dst = 1.0f / 64.0f;

    int cnt;
    Uint32 ivrt;
    float min_s, min_t, max_s, max_t;

    // aliases
    cartman_mpd_tile_t   * pfan   = NULL;

    if ( NULL == pmesh ) pmesh = &mesh;

    pfan = CART_MPD_FAN_PTR( pmesh, fan );
    if ( NULL == pfan ) return;

    // don't draw FANOFF
    if ( TILE_IS_FANOFF( pfan->tx_bits ) ) return;

    // don't draw if there is no texture
    if ( NULL == tx_tile ) return;
	Ego::Renderer::get().getTextureUnit().setActivated(tx_tile.get());

    min_s = dst;
    min_t = dst;

    max_s = -dst + (float)tx_tile->getSourceHeight() / (float)tx_tile->getWidth();
    max_t = -dst + (float)tx_tile->getSourceHeight() / (float)tx_tile->getHeight();

    // set the texture coordinates
    loc_vrt[0].s = min_s;
    loc_vrt[0].t = min_t;

    loc_vrt[1].s = max_s;
    loc_vrt[1].t = min_t;

    loc_vrt[2].s = max_s;
    loc_vrt[2].t = max_t;

    loc_vrt[3].s = min_s;
    loc_vrt[3].t = max_t;

    // set the tile corners
    if ( draw_tile )
    {
        // draw the tile on a 31x31 grix, using the values of x0,y0

        ivrt = pfan->vrtstart;

        // Top Left
        loc_vrt[0].x = x0;
        loc_vrt[0].y = y0;
        loc_vrt[0].z = 0;
        loc_vrt[0].l = pmesh->vrt2[ivrt].a / 255.0f;
        ivrt = pmesh->vrt2[ivrt].next;

        // Top Right
        loc_vrt[1].x = x0 + Info<float>::Grid::Size();
        loc_vrt[1].y = y0;
        loc_vrt[1].z = 0;
        loc_vrt[1].l = pmesh->vrt2[ivrt].a / 255.0f;
        ivrt = pmesh->vrt2[ivrt].next;

        // Bottom Right
        loc_vrt[2].x = x0 + Info<float>::Grid::Size();
        loc_vrt[2].y = y0 + Info<float>::Grid::Size();
        loc_vrt[2].z = 0;
        loc_vrt[2].l = pmesh->vrt2[ivrt].a / 255.0f;
        ivrt = pmesh->vrt2[ivrt].next;

        // Bottom Left
        loc_vrt[3].x = x0;
        loc_vrt[3].y = y0 + Info<float>::Grid::Size();
        loc_vrt[3].z = 0;
        loc_vrt[3].l = pmesh->vrt2[ivrt].a / 255.0f;
    }
    else
    {
        // draw the tile using the actual values of the coordinates

        int cnt;

        ivrt = pfan->vrtstart;
        for ( cnt = 0;
              cnt < 4 && CHAINEND != ivrt;
              cnt++, ivrt = pmesh->vrt2[ivrt].next)
        {
            loc_vrt[cnt].x = pmesh->vrt2[ivrt].x;
            loc_vrt[cnt].y = pmesh->vrt2[ivrt].y;
            loc_vrt[cnt].z = pmesh->vrt2[ivrt].z;
            loc_vrt[cnt].l = pmesh->vrt2[ivrt].a / 255.0f;
        }
    }

    // Draw A Quad
    glBegin( GL_QUADS );
    {

        for ( cnt = 0; cnt < 4; cnt++ )
        {
            glColor3f( loc_vrt[cnt].l,  loc_vrt[cnt].l,  loc_vrt[cnt].l );
            glTexCoord2f( loc_vrt[cnt].s, loc_vrt[cnt].t );
            glVertex3f( loc_vrt[cnt].x, loc_vrt[cnt].y, loc_vrt[cnt].z );
        };
    }
    glEnd();
}

//--------------------------------------------------------------------------------------------

void draw_tile_fx( float x, float y, Uint8 fx, float scale )
{
    const int ioff_0 = Info<int>::Grid::Size() >> 3;
    const int ioff_1 = Info<int>::Grid::Size() >> 4;

    const float foff_0 = ioff_0 * scale;
    const float foff_1 = ioff_1 * scale;

    float x1, y1;
    float w1, h1;

    // water is whole tile
    if ( HAS_BITS( fx, MAPFX_WATER ) )
    {
        x1 = x;
        y1 = y;
        w1 = Resources::get().tx_water->getSourceWidth() * scale;
        h1 = Resources::get().tx_water->getSourceHeight() * scale;

        ogl_draw_sprite_2d(Resources::get().tx_water, x1, y1, w1, h1 );
    }

    // "reflectable tile" is upper left
    if ( !HAS_BITS( fx, MAPFX_SHA ) )
    {
        x1 = x;
        y1 = y;
        w1 = Resources::get().tx_ref->getSourceWidth() * scale;
        h1 = Resources::get().tx_ref->getSourceHeight() * scale;

        ogl_draw_sprite_2d(Resources::get().tx_ref, x1, y1, w1, h1 );
    }

    // "reflects entities" is upper right
    if ( HAS_BITS( fx, MAPFX_REFLECTIVE ) )
    {
        x1 = x + foff_0;
        y1 = y;

        w1 = Resources::get().tx_drawref->getSourceWidth() * scale;
        h1 = Resources::get().tx_drawref->getSourceHeight() * scale;

        ogl_draw_sprite_2d(Resources::get().tx_drawref, x1, y1, w1, h1 );
    }

    // "animated tile" is lower left
    if ( HAS_BITS( fx, MAPFX_ANIM ) )
    {
        x1 = x;
        y1 = y + foff_0;

        w1 = Resources::get().tx_anim->getSourceWidth() * scale;
        h1 = Resources::get().tx_anim->getSourceHeight() * scale;

        ogl_draw_sprite_2d(Resources::get().tx_anim, x1, y1, w1, h1 );
    }

    // the following are all in the lower left quad
    x1 = x + foff_0;
    y1 = y + foff_0;

    if ( HAS_BITS( fx, MAPFX_WALL ) )
    {
        float x2 = x1;
        float y2 = y1;

        w1 = Resources::get().tx_wall->getSourceWidth() * scale;
        h1 = Resources::get().tx_wall->getSourceHeight() * scale;

        ogl_draw_sprite_2d(Resources::get().tx_wall, x2, y2, w1, h1 );
    }

    if ( HAS_BITS( fx, MAPFX_IMPASS ) )
    {
        float x2 = x1 + foff_1;
        float y2 = y1;

        w1 = Resources::get().tx_impass->getSourceWidth() * scale;
        h1 = Resources::get().tx_impass->getSourceHeight() * scale;

        ogl_draw_sprite_2d(Resources::get().tx_impass, x2, y2, w1, h1 );
    }

    if ( HAS_BITS( fx, MAPFX_DAMAGE ) )
    {
        float x2 = x1;
        float y2 = y1 + foff_1;

        w1 = Resources::get().tx_damage->getSourceWidth() * scale;
        h1 = Resources::get().tx_damage->getSourceHeight() * scale;

        ogl_draw_sprite_2d(Resources::get().tx_damage, x2, y2, w1, h1 );
    }

    if ( HAS_BITS( fx, MAPFX_SLIPPY ) )
    {
        float x2 = x1 + foff_1;
        float y2 = y1 + foff_1;

        w1 = Resources::get().tx_slippy->getSourceWidth() * scale;
        h1 = Resources::get().tx_slippy->getSourceHeight() * scale;

        ogl_draw_sprite_2d(Resources::get().tx_slippy, x2, y2, w1, h1 );
    }

}

//--------------------------------------------------------------------------------------------

void ogl_draw_sprite_2d(std::shared_ptr<Ego::Texture> img, float x, float y, float width, float height )
{
    float min_s, max_s, min_t, max_t;

    const float dst = 1.0f / 64.0f;

    float w = width;
    float h = height;

    if ( NULL != img )
    {
        if ( width == 0 || height == 0 )
        {
            w = img->getWidth();
            h = img->getHeight();
        }

        min_s = dst;
        min_t = dst;

        max_s = -dst + (float)img->getSourceWidth() / (float)img->getWidth();
        max_t = -dst + (float)img->getSourceHeight() / (float)img->getHeight();
    }
    else
    {
        min_s = dst;
        min_t = dst;

        max_s = 1.0f - dst;
        max_t = 1.0f - dst;
    }

    // Draw the image
	Ego::Renderer::get().getTextureUnit().setActivated(img.get());

    Ego::Renderer::get().setColour(Ego::Math::Colour4f::white());

    glBegin( GL_TRIANGLE_STRIP );
    {
        glTexCoord2f( min_s, min_t );  glVertex2f( x,     y );
        glTexCoord2f( max_s, min_t );  glVertex2f( x + w, y );
        glTexCoord2f( min_s, max_t );  glVertex2f( x,     y + h );
        glTexCoord2f( max_s, max_t );  glVertex2f( x + w, y + h );
    }
    glEnd();
}

void ogl_draw_sprite_3d(std::shared_ptr<Ego::Texture> img, cart_vec_t pos, cart_vec_t vup, cart_vec_t vright, float width, float height )
{
    float min_s, max_s, min_t, max_t;
    cart_vec_t bboard[4];

    const float dst = 1.0f / 64.0f;

    float w = width;
    float h = height;

    if ( NULL != img )
    {
        if ( width == 0 || height == 0 )
        {
            w = img->getWidth();
            h = img->getHeight();
        }

        min_s = dst;
        min_t = dst;

        max_s = -dst + (float)img->getSourceWidth() / (float)img->getWidth();
        max_t = -dst + (float)img->getSourceHeight() / (float)img->getHeight();
    }
    else
    {
        min_s = dst;
        min_t = dst;

        max_s = 1.0f - dst;
        max_t = 1.0f - dst;
    }

    // Draw the image
	Ego::Renderer::get().getTextureUnit().setActivated(img.get());

    Ego::Renderer::get().setColour(Ego::Math::Colour4f::white());

    bboard[0][kX] = pos[kX] - w / 2 * vright[kX] + h / 2 * vup[kX];
    bboard[0][kY] = pos[kY] - w / 2 * vright[kY] + h / 2 * vup[kY];
    bboard[0][kZ] = pos[kZ] - w / 2 * vright[kZ] + h / 2 * vup[kZ];

    bboard[1][kX] = pos[kX] + w / 2 * vright[kX] + h / 2 * vup[kX];
    bboard[1][kY] = pos[kY] + w / 2 * vright[kY] + h / 2 * vup[kY];
    bboard[1][kZ] = pos[kZ] + w / 2 * vright[kZ] + h / 2 * vup[kZ];

    bboard[2][kX] = pos[kX] - w / 2 * vright[kX] - h / 2 * vup[kX];
    bboard[2][kY] = pos[kY] - w / 2 * vright[kY] - h / 2 * vup[kY];
    bboard[2][kZ] = pos[kZ] - w / 2 * vright[kZ] - h / 2 * vup[kZ];

    bboard[3][kX] = pos[kX] + w / 2 * vright[kX] - h / 2 * vup[kX];
    bboard[3][kY] = pos[kY] + w / 2 * vright[kY] - h / 2 * vup[kY];
    bboard[3][kZ] = pos[kZ] + w / 2 * vright[kZ] - h / 2 * vup[kZ];

    glBegin( GL_TRIANGLE_STRIP );
    {
        glTexCoord2f( min_s, min_t );  glVertex3fv( bboard[0] );
        glTexCoord2f( max_s, min_t );  glVertex3fv( bboard[1] );
        glTexCoord2f( min_s, max_t );  glVertex3fv( bboard[2] );
        glTexCoord2f( max_s, max_t );  glVertex3fv( bboard[3] );
    }
    glEnd();
}

//--------------------------------------------------------------------------------------------

void ogl_draw_box_xy( float x, float y, float z, float w, float h, Ego::Math::Colour4f& colour )
{
    glPushAttrib( GL_ENABLE_BIT );
    {
        Ego::Renderer::get().getTextureUnit().setActivated(nullptr);
        Ego::Renderer::get().setColour(colour);

        glBegin( GL_QUADS );
        {
            glVertex3f( x,     y,     z );
            glVertex3f( x,     y + h, z );
            glVertex3f( x + w, y + h, z );
            glVertex3f( x + w, y,     z );
        }
        glEnd();
    }
    glPopAttrib();
};

void ogl_draw_box_xz( float x, float y, float z, float w, float d, Ego::Math::Colour4f& colour )
{
    glPushAttrib( GL_ENABLE_BIT );
    {
        Ego::Renderer::get().getTextureUnit().setActivated(nullptr);
        Ego::Renderer::get().setColour(colour);

        glBegin( GL_QUADS );
        {
            glVertex3f( x,     y, z );
            glVertex3f( x,     y, z + d );
            glVertex3f( x + w, y, z + d );
            glVertex3f( x + w, y, z );
        }
        glEnd();
    }
    glPopAttrib();
};

//--------------------------------------------------------------------------------------------
void ogl_beginFrame()
{
    auto& renderer = Ego::Renderer::get();
    glPushAttrib( GL_ENABLE_BIT );
	renderer.setDepthTestEnabled(false);
    renderer.setCullingMode(Ego::CullingMode::None);
    glEnable( GL_TEXTURE_2D );

    renderer.setBlendingEnabled(true);
    renderer.setBlendFunction(Ego::BlendFunction::SourceAlpha, Ego::BlendFunction::OneMinusSourceAlpha);

    auto drawableSize = Ego::GraphicsSystem::window->getDrawableSize();
    renderer.setViewportRectangle(0, 0, drawableSize.width(), drawableSize.height());

    // Set up an ortho projection for the gui to use.  Controls are free to modify this
    // later, but most of them will need this, so it's done by default at the beginning
    // of a frame
    auto windowSize = Ego::GraphicsSystem::window->getSize();
	Matrix4f4f projection = Ego::Math::Transform::ortho(0, windowSize.width(), windowSize.height(), 0, -1, 1);
    renderer.setProjectionMatrix(projection);
    renderer.setWorldMatrix(Matrix4f4f::identity());
    renderer.setViewMatrix(Matrix4f4f::identity());
}

//--------------------------------------------------------------------------------------------
void ogl_endFrame()
{
    // Re-enable any states disabled by gui_beginFrame
    glPopAttrib();
}

//--------------------------------------------------------------------------------------------
void draw_sprite( SDL_Surface * dst, SDL_Surface * sprite, int x, int y )
{
    SDL_Rect rdst;

    if ( NULL == dst || NULL == sprite ) return;

    rdst.x = x;
    rdst.y = y;
    rdst.w = sprite->w;
    rdst.h = sprite->h;

    cartman_BlitSurface( sprite, NULL, dst, &rdst );
}

//--------------------------------------------------------------------------------------------
int cartman_BlitSurface( SDL_Surface *src, ::SDL_Rect *srcrect, SDL_Surface *dst, ::SDL_Rect *dstrect )
{
    return SDL_BlitSurface( src, srcrect, dst, dstrect );
}

//--------------------------------------------------------------------------------------------
SDL_Surface *cartman_LoadIMG(const std::string& name)
{
    // Load the image.
    SDL_Surface *originalImage = IMG_Load_RW(vfs_openRWopsRead(name), 1);
    if (!originalImage)
    {
		Log::get().error("unable to load image `%s` - reason `%s`\n", name.c_str(), IMG_GetError());
        return nullptr;
    }

    // Convert the image to the same pixel format as the expanded screen format.
    SDL_Surface *convertedImage = SDL_ConvertSurfaceFormat(originalImage, SDL_PIXELFORMAT_RGBA8888, 0);
    SDL_FreeSurface(originalImage);
    if (!convertedImage)
    {
		Log::get().error("unable to convert image `%s`\n", name.c_str());
		return nullptr;
    }

    return convertedImage;
}

//--------------------------------------------------------------------------------------------
void cartman_begin_ortho_camera_hrz(Cartman::Gui::Window& pwin, camera_t * pcam, float zoom_x, float zoom_y)
{
    using namespace Cartman::Gui;
    static const float factor_x = (float)DEFAULT_RESOLUTION * Info<float>::Grid::Size() / (float)Window::defaultWidth,
        factor_y = (float)DEFAULT_RESOLUTION * Info<float>::Grid::Size() / (float)Window::defaultHeight;
    float w = (float)pwin.size.width() * factor_x / zoom_x;
    float h = (float)pwin.size.height() * factor_y / zoom_y;
    float d = DEFAULT_Z_SIZE;

    pcam->w = w;
    pcam->h = h;
    pcam->d = d;

    float left   = - w / 2;
    float right  =   w / 2;
    float bottom = - h / 2;
    float top    =   h / 2;
    float front  = -d;
    float back   =  d;

    float aspect = ( float ) w / h;
    if ( aspect < 1.0f )
    {
        // window taller than wide
        bottom /= aspect;
        top /= aspect;
    }
    else
    {
        left *= aspect;
        right *= aspect;
    }
    
	Matrix4f4f matrix;
    auto &renderer = Ego::Renderer::get();
    matrix = Ego::Math::Transform::ortho(left, right, bottom, top, front, back);
    renderer.setProjectionMatrix(matrix);
    renderer.setWorldMatrix(Ego::Math::Transform::scaling({-1.0f, 1.0f, 1.0f}));
    renderer.setViewMatrix(Ego::Math::Transform::lookAt({pcam->x, pcam->y, back}, {pcam->x, pcam->y, front}, {0.0f, -1.0f, 0.0f}));
}

//--------------------------------------------------------------------------------------------
void cartman_begin_ortho_camera_vrt(Cartman::Gui::Window& pwin, camera_t * pcam, float zoom_x, float zoom_z)
{
    using namespace Cartman::Gui;

    static const float factor_x = (float)DEFAULT_RESOLUTION * Info<float>::Grid::Size() / (float)Window::defaultWidth,
        factor_y = (float)DEFAULT_RESOLUTION * Info<float>::Grid::Size() / (float)Window::defaultHeight;
    float w = pwin.size.width() * factor_x / zoom_x;
    float h = w;
    float d = pwin.size.height() * factor_y / zoom_z;

    pcam->w = w;
    pcam->h = h;
    pcam->d = d;

    float left   = - w / 2;
    float right  =   w / 2;
    float bottom = - d / 2;
    float top    =   d / 2;
    float front  =   0;
    float back   =   h;

    float aspect = ( float ) w / ( float ) d;
    if ( aspect < 1.0f )
    {
        // window taller than wide
        bottom /= aspect;
        top /= aspect;
    }
    else
    {
        left *= aspect;
        right *= aspect;
    }
    
	Matrix4f4f matrix;
    auto &renderer = Ego::Renderer::get();

    matrix = Ego::Math::Transform::ortho(left, right, bottom, top, front, back);
    renderer.setProjectionMatrix(matrix);
    matrix = Ego::Math::Transform::lookAt({pcam->x, pcam->y, pcam->z}, {pcam->x, pcam->y + back, pcam->z}, {0.0f, 0.0f, 1.0f});
    renderer.setWorldMatrix(Matrix4f4f::identity());
    renderer.setViewMatrix(matrix);
}

//--------------------------------------------------------------------------------------------

void cartman_end_ortho_camera()
{
    auto &renderer = Ego::Renderer::get();
    auto windowSize = Ego::GraphicsSystem::window->getSize();
    Matrix4f4f projection = Ego::Math::Transform::ortho(0, windowSize.width(), windowSize.height(), 0, -1, 1);
    renderer.setProjectionMatrix(projection);
    renderer.setWorldMatrix(Matrix4f4f::identity());
    renderer.setViewMatrix(Matrix4f4f::identity());
}

//--------------------------------------------------------------------------------------------
void load_img()
{
    Resources::get().tx_point = std::make_shared<Ego::OpenGL::Texture>();
    if (!Resources::get().tx_point->load("editor/point.png", gfx_loadImage("editor/point.png")))
    {
		Log::get().warn( "Cannot load image \"%s\".\n", "editor/point.png" );
    }
    
    Resources::get().tx_pointon = std::make_shared<Ego::OpenGL::Texture>();
    if (!Resources::get().tx_pointon->load("editor/pointon.png", gfx_loadImage("editor/pointon.png")))
    {
		Log::get().warn( "Cannot load image \"%s\".\n", "editor/pointon.png" );
    }
    
    Resources::get().tx_ref = std::make_shared<Ego::OpenGL::Texture>();
    if (!Resources::get().tx_ref->load("editor/ref.png", gfx_loadImage("editor/ref.png")))
    {
		Log::get().warn( "Cannot load image \"%s\".\n", "editor/ref.png" );
    }
    
    Resources::get().tx_drawref = std::make_shared<Ego::OpenGL::Texture>();
    if (!Resources::get().tx_drawref->load("editor/drawref.png", gfx_loadImage("editor/drawref.png")))
    {
		Log::get().warn( "Cannot load image \"%s\".\n", "editor/drawref.png" );
    }
    
    Resources::get().tx_anim = std::make_shared<Ego::OpenGL::Texture>();
    if (!Resources::get().tx_anim->load("editor/anim.png", gfx_loadImage("editor/anim.png")))
    {
		Log::get().warn( "Cannot load image \"%s\".\n", "editor/anim.png" );
    }
    
    Resources::get().tx_water = std::make_shared<Ego::OpenGL::Texture>();
    if (!Resources::get().tx_water->load("editor/water.png", gfx_loadImage("editor/water.png")))
    {
		Log::get().warn( "Cannot load image \"%s\".\n", "editor/water.png" );
    }
    
    Resources::get().tx_wall = std::make_shared<Ego::OpenGL::Texture>();
    if (!Resources::get().tx_wall->load("editor/slit.png", gfx_loadImage("editor/slit.png")))
    {
		Log::get().warn( "Cannot load image \"%s\".\n", "editor/slit.png" );
    }
    
    Resources::get().tx_impass = std::make_shared<Ego::OpenGL::Texture>();
    if (!Resources::get().tx_impass->load("editor/impass.png", gfx_loadImage("editor/impass.png")))
    {
		Log::get().warn( "Cannot load image \"%s\".\n", "editor/impass.png" );
    }
    
    Resources::get().tx_damage = std::make_shared<Ego::OpenGL::Texture>();
    if (!Resources::get().tx_damage->load("editor/damage.png", gfx_loadImage("editor/damage.png")))
    {
		Log::get().warn( "Cannot load image \"%s\".\n", "editor/damage.png" );
    }
    
    Resources::get().tx_slippy = std::make_shared<Ego::OpenGL::Texture>();
    if (!Resources::get().tx_slippy->load("editor/slippy.png", gfx_loadImage("editor/slippy.png")))
    {
		Log::get().warn( "Cannot load image \"%s\".\n", "editor/slippy.png" );
    }
}

//--------------------------------------------------------------------------------------------
void get_small_tiles( SDL_Surface* bmpload )
{
    std::shared_ptr<SDL_Surface> image;

    int x, y, x1, y1;
    int sz_x = bmpload->w;
    int sz_y = bmpload->h;
    int step_x = sz_x >> 3;
    int step_y = sz_y >> 3;

    if ( step_x == 0 ) step_x = 1;
    if ( step_y == 0 ) step_y = 1;

    for ( y = 0, y1 = 0; y < sz_y && y1 < 256; y += step_y, y1 += SMALLXY )
    {
        for ( x = 0, x1 = 0; x < sz_x && x1 < 256; x += step_x, x1 += SMALLXY )
        {
            SDL_Rect src1 = { static_cast<Sint16>(x), static_cast<Sint16>(y), static_cast<Uint16>( step_x - 1 ), static_cast<Uint16>( step_y - 1 ) };

            Resources::get().tx_smalltile[numsmalltile] = std::make_shared<Ego::OpenGL::Texture>();

            image = Ego::Graphics::SDL::createSurface( SMALLXY, SMALLXY );
            if (!image)
            {
                throw std::runtime_error("unable to create surface");
            }
            SDL_FillRect( image.get(), NULL, make_rgb( image, Ego::Math::Colour3b::black() ) );
            SDL_SoftStretch( bmpload, &src1, image.get(), NULL );

            Resources::get().tx_smalltile[numsmalltile]->load(image);

            numsmalltile++;
        }
    }
}

//--------------------------------------------------------------------------------------------
void get_big_tiles( SDL_Surface* bmpload )
{
    std::shared_ptr<SDL_Surface> image;

    int x, y, x1, y1;
    int sz_x = bmpload->w;
    int sz_y = bmpload->h;
    int step_x = sz_x >> 3;
    int step_y = sz_y >> 3;

    if ( step_x == 0 ) step_x = 1;
    if ( step_y == 0 ) step_y = 1;

    for ( y = 0, y1 = 0; y < sz_y; y += step_y, y1 += SMALLXY )
    {
        for ( x = 0, x1 = 0; x < sz_x; x += step_x, x1 += SMALLXY )
        {
            int wid, hgt;

            SDL_Rect src1;

            wid = ( 2 * step_x - 1 );
            if ( x + wid > bmpload->w ) wid = bmpload->w - x;

            hgt = ( 2 * step_y - 1 );
            if ( y + hgt > bmpload->h ) hgt = bmpload->h - y;

            src1.x = x;
            src1.y = y;
            src1.w = wid;
            src1.h = hgt;

            Resources::get().tx_bigtile[numbigtile] = std::make_shared<Ego::OpenGL::Texture>();

            image = Ego::Graphics::SDL::createSurface( SMALLXY, SMALLXY );
            if (!image)
            {
                throw std::runtime_error("unable to create surface");
            }
            SDL_FillRect( image.get(), NULL, make_rgb( image, Ego::Math::Colour3b::black() ) );

            SDL_SoftStretch( bmpload, &src1, image.get(), NULL );

            Resources::get().tx_bigtile[numbigtile]->load(image);

            numbigtile++;
        }
    }
}

//--------------------------------------------------------------------------------------------
void get_tiles( SDL_Surface* bmpload )
{
    get_small_tiles( bmpload );
    get_big_tiles( bmpload );
}

//--------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------
void gfx_init_ogl()
{
    using namespace Ego;
    using namespace Ego::Math;

    auto& renderer = Renderer::get();
    // Set clear colour.
    renderer.getColourBuffer().setClearValue(Colour4f(0, 0, 0, 0)); // Set black/transparent background.
    
    // Set clear depth.
    renderer.getDepthBuffer().setClearValue(1.0f);

    // Enable writing to the depth buffer.
    renderer.setDepthWriteEnabled(true);

    // Enable depth testing: Incoming fragment's depth value must be less.
    renderer.setDepthTestEnabled(true);
    renderer.setDepthFunction(CompareFunction::Less);

    // Disable blending.
    renderer.setBlendingEnabled(false);

    // do not display the completely transparent portion
    renderer.setAlphaTestEnabled(true);
	renderer.setAlphaFunction(CompareFunction::Greater, 0.0f);

    /// @todo Including backface culling here prevents the mesh from getting rendered
    /// backface culling
    // oglx_begin_culling(Ego::CullingMode::Back, Ego::WindingMode::Clockwise);            // GL_ENABLE_BIT | GL_POLYGON_BIT

    // disable OpenGL lighting
	renderer.setLightingEnabled(false);

    // fill mode
	renderer.setRasterizationMode(RasterizationMode::Solid);

    // set up environment mapping
    /// @todo: this isn't used anywhere
    GL_DEBUG( glTexGeni )( GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP );  // Set The Texture Generation Mode For S To Sphere Mapping (NEW)
    GL_DEBUG( glTexGeni )( GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP );  // Set The Texture Generation Mode For T To Sphere Mapping (NEW)

    //Initialize the motion blur buffer
    renderer.getAccumulationBuffer().setClearValue(Colour4f(0.0f, 0.0f, 0.0f, 1.0f));
    renderer.getAccumulationBuffer().clear();
}

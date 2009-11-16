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

/// @file graphic_fan.c
/// @brief World mesh drawing.
/// @details

#include "graphic.h"
#include "game.h"
#include "texture.h"

#include "egoboo.h"

#include "SDL_extensions.h"

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
void render_fan( ego_mpd_t * pmesh, Uint32 fan )
{
    /// @details ZZ@> This function draws a mesh fan

    // optimized to use gl*Pointer() and glArrayElement() for vertex presentation

    int    cnt, tnc, entry, vertex;
    Uint16 commands, vertices;
    Uint16 basetile;
    Uint16 image;
    Uint8  fx, type;
    int    texture, tile;

    mesh_mem_t  * pmem;
    tile_info_t * ptile;

    if ( NULL == pmesh ) return;
    pmem  = &( pmesh->mmem );

    if ( !VALID_TILE( pmesh, fan ) ) return;
    ptile = pmem->tile_list + fan;

    // do not render the fan if the image image is invalid
    if ( TILE_IS_FANOFF( *ptile ) )  return;

    image = TILE_GET_LOWER_BITS( ptile->img ); // Tile image
    fx    = ptile->fx;                         // Fx bits
    type  = ptile->type;                       // Command type ( index to points in fan )

    // Animate the tiles
    if ( HAS_SOME_BITS( fx, MPDFX_ANIM ) )
    {
        Uint16 base_and, frame_and, frame_add;

        if ( type >= ( MAXMESHTYPE >> 1 ) )
        {
            // Big tiles
            base_and  = animtile[1].base_and;            // Animation set
            frame_and = animtile[1].frame_and;
            frame_add = animtile[1].frame_add;    // Animated image
        }
        else
        {
            // Small tiles
            base_and  = animtile[0].base_and;            // Animation set
            frame_and = animtile[0].frame_and;
            frame_add = animtile[0].frame_add;         // Animated image
        }

        basetile = image & base_and;
        image    = frame_add + basetile;
    }

    tile     = image & 0x3F;                     // tile type
    texture  = (( image >> 6 ) & 3 ) + TX_TILE_0; // 64 tiles in each 256x256 texture
    vertices = tile_dict[type].numvertices;      // Number of vertices
    commands = tile_dict[type].command_count;    // Number of commands

    if ( !meshnotexture && texture != meshlasttexture ) return;

    // actually update the animated texture info
    mesh_set_texture( pmesh, fan, image );

    GL_DEBUG( glPushClientAttrib )( GL_CLIENT_VERTEX_ARRAY_BIT );
    {
        GL_DEBUG( glShadeModel )( GL_SMOOTH );

        // [claforte] Put this in an initialization function.
        GL_DEBUG( glEnableClientState )( GL_VERTEX_ARRAY );
        GL_DEBUG( glVertexPointer )( 3, GL_FLOAT, 0, pmem->plst + ptile->vrtstart );

        GL_DEBUG( glEnableClientState )( GL_TEXTURE_COORD_ARRAY );
        GL_DEBUG( glTexCoordPointer )( 2, GL_FLOAT, 0, pmem->tlst + ptile->vrtstart );

        GL_DEBUG( glEnableClientState )( GL_COLOR_ARRAY );
        GL_DEBUG( glColorPointer )( 3, GL_FLOAT, 0, pmem->clst + ptile->vrtstart );

        // Render each command
        entry = 0;
        for ( cnt = 0; cnt < commands; cnt++ )
        {
            GL_DEBUG( glBegin )( GL_TRIANGLE_FAN );
            {
                for ( tnc = 0; tnc < tile_dict[type].command_entries[cnt]; tnc++ )
                {
                    vertex = tile_dict[type].command_verts[entry];

                    GL_DEBUG( glArrayElement )( vertex );

                    entry++;
                }

            }
            GL_DEBUG_END();
        }
    }
    GL_DEBUG( glPopClientAttrib )();

#if defined(DEBUG_MESH_NORMALS)
    GL_DEBUG( glDisable )( GL_TEXTURE_2D );
    GL_DEBUG( glColor4f )( 1, 1, 1, 1 );
    entry = ptile->vrtstart;
    for ( cnt = 0; cnt < 4; cnt++, entry++ )
    {
        GL_DEBUG( glBegin )( GL_LINES );
        {
            GL_DEBUG( glVertex3fv )( pmem->plst[entry] );
            GL_DEBUG( glVertex3f )(
                pmem->plst[entry][XX] + TILE_SIZE*pmem->ncache[fan][cnt][XX],
                pmem->plst[entry][YY] + TILE_SIZE*pmem->ncache[fan][cnt][YY],
                pmem->plst[entry][ZZ] + TILE_SIZE*pmem->ncache[fan][cnt][ZZ] );

        }
        GL_DEBUG_END();
    }
    GL_DEBUG( glEnable )( GL_TEXTURE_2D );
#endif

}

//--------------------------------------------------------------------------------------------
void render_hmap_fan( ego_mpd_t * pmesh, Uint32 fan )
{
    /// @details ZZ@> This function draws a mesh fan
    GLvertex v[4];

    int cnt, vertex, badvertex;
    int ix, iy, ix_off[4] = {0, 1, 1, 0}, iy_off[4] = {0, 0, 1, 1};
    Uint16 tile;
    Uint8  fx, type, twist;

    mesh_mem_t  * pmem;
    ego_mpd_info_t * pinfo;
    tile_info_t * ptile;

    if ( NULL == pmesh ) return;
    pmem  = &( pmesh->mmem );
    pinfo = &( pmesh->info );

    if ( !VALID_TILE( pmesh, fan ) ) return;
    ptile = pmem->tile_list + fan;

    /// @details BB@> the water info is for TILES, not for vertices, so ignore all vertex info and just draw the water
    ///     tile where it's supposed to go

    ix = fan % pmesh->info.tiles_x;
    iy = fan / pmesh->info.tiles_x;

    // vertex is a value from 0-15, for the meshcommandref/u/v variables
    // badvertex is a value that references the actual vertex number

    tile  = TILE_GET_LOWER_BITS( ptile->img ); // Tile
    fx    = ptile->fx;                       // Fx bits
    type  = ptile->type;                     // Command type ( index to points in fan )
    twist = ptile->twist;

    type &= 0x3F;

    // Original points
    badvertex = ptile->vrtstart;          // Get big reference value
    for ( cnt = 0; cnt < 4; cnt++ )
    {
        float tmp;
        v[cnt].pos[XX] = ( ix + ix_off[cnt] ) * TILE_SIZE;
        v[cnt].pos[YY] = ( iy + iy_off[cnt] ) * TILE_SIZE;
        v[cnt].pos[ZZ] = pmem->plst[badvertex][ZZ];

        tmp = map_twist_nrm[twist].z;
        tmp *= tmp;

        v[cnt].col[RR] = tmp * ( tmp + ( 1.0f - tmp ) * map_twist_nrm[twist].x * map_twist_nrm[twist].x );
        v[cnt].col[GG] = tmp * ( tmp + ( 1.0f - tmp ) * map_twist_nrm[twist].y * map_twist_nrm[twist].y );
        v[cnt].col[BB] = tmp;
        v[cnt].col[AA] = 1.0f;

        v[cnt].col[RR] = CLIP( v[cnt].col[RR], 0.0f, 1.0f );
        v[cnt].col[GG] = CLIP( v[cnt].col[GG], 0.0f, 1.0f );
        v[cnt].col[BB] = CLIP( v[cnt].col[BB], 0.0f, 1.0f );

        badvertex++;
    }

    oglx_texture_Bind( NULL );

    // Render each command
    GL_DEBUG( glBegin )( GL_TRIANGLE_FAN );
    {
        for ( vertex = 0; vertex < 4; vertex++ )
        {
            GL_DEBUG( glColor3fv )( v[vertex].col );
            GL_DEBUG( glVertex3fv )( v[vertex].pos );
        }
    }
    GL_DEBUG_END();

}

//--------------------------------------------------------------------------------------------
void render_water_fan( ego_mpd_t * pmesh, Uint32 fan, Uint8 layer )
{
    /// @details ZZ@> This function draws a water fan

    GLvertex v[4];

    int    cnt, tnc, badvertex;
    Uint16 type, commands, vertices, texture;
    Uint16 frame;
    float offu, offv;
    int ix, iy;
    int ix_off[4] = {1, 1, 0, 0}, iy_off[4] = {0, 1, 1, 0};
    int  imap[4];
    float x1, y1, fx_off[4], fy_off[4];
    float falpha;

    ego_mpd_info_t * pinfo;
    mesh_mem_t     * pmmem;
    grid_mem_t     * pgmem;
    tile_info_t    * ptile;
    oglx_texture   * ptex;

    if ( NULL == pmesh ) return;
    pinfo = &( pmesh->info );
    pmmem = &( pmesh->mmem );
    pgmem = &( pmesh->gmem );

    if ( !VALID_TILE( pmesh, fan ) ) return;
    ptile = pmmem->tile_list + fan;

    falpha = FF_TO_FLOAT( water.layer[layer].alpha );
    falpha = CLIP(falpha, 0.0f, 1.0f);

    /// @note BB@> the water info is for TILES, not for vertices, so ignore all vertex info and just draw the water
    ///            tile where it's supposed to go

    ix = fan % pinfo->tiles_x;
    iy = fan / pinfo->tiles_x;

    // To make life easier
    type  = 0;                                         // Command type ( index to points in fan )
    offu  = water.layer[layer].tx.x;                   // Texture offsets
    offv  = water.layer[layer].tx.y;
    frame = water.layer[layer].frame;                  // Frame

    texture  = layer + TX_WATER_TOP;                   // Water starts at texture TX_WATER_TOP
    vertices = tile_dict[type].numvertices;            // Number of vertices
    commands = tile_dict[type].command_count;          // Number of commands

    ptex = TxTexture_get_ptr( texture );

    x1 = ( float ) oglx_texture_GetTextureWidth( ptex ) / ( float ) oglx_texture_GetImageWidth( ptex );
    y1 = ( float ) oglx_texture_GetTextureHeight( ptex ) / ( float ) oglx_texture_GetImageHeight( ptex );

    for ( cnt = 0; cnt < 4; cnt ++ )
    {
        fx_off[cnt] = x1 * ix_off[cnt];
        fy_off[cnt] = y1 * iy_off[cnt];

        imap[cnt] = cnt;
    }

    // flip the coordinates around based on the "mode" of the tile
    if ( HAS_NO_BITS( ix, 1 ) )
    {
        SWAP( int, imap[0], imap[3] );
        SWAP( int, imap[1], imap[2] );
    }

    if ( HAS_NO_BITS( iy, 1 ) )
    {
        SWAP( int, imap[0], imap[1] );
        SWAP( int, imap[2], imap[3] );
    }

    // Original points
    GL_DEBUG( glDisable )( GL_CULL_FACE );
    badvertex = ptile->vrtstart;
    {
        GLXvector3f nrm = {0, 0, 1};
        float alight;

        alight = get_ambient_level() + water.layer->light_add;
        alight = CLIP( alight, 0.0f, 1.0f );

        for ( cnt = 0; cnt < 4; cnt++ )
        {
            float dlight;
            int jx, jy;

            tnc = imap[cnt];

            jx = ix + ix_off[cnt];
            jy = iy + iy_off[cnt];

            v[cnt].pos[XX] = jx * TILE_SIZE;
            v[cnt].pos[YY] = jy * TILE_SIZE;
            v[cnt].pos[ZZ] = water.layer_z_add[layer][frame][tnc] + water.layer[layer].z;

            v[cnt].tex[SS] = fx_off[cnt] + offu;
            v[cnt].tex[TT] = fy_off[cnt] + offv;

            // get the lighting info from the grid
            fan = mesh_get_tile_int( pmesh, jx, jy );
            if ( grid_light_one_corner( pmesh, fan, v[cnt].pos[ZZ], nrm, &dlight ) )
            {
                // take the v[cnt].color from the tnc vertices so that it is oriented prroperly
                v[cnt].col[RR] = dlight * INV_FF + alight;
                v[cnt].col[GG] = dlight * INV_FF + alight;
                v[cnt].col[BB] = dlight * INV_FF + alight;
                v[cnt].col[AA] = 1.0f;

                v[cnt].col[RR] = CLIP( v[cnt].col[RR], 0.0f, 1.0f );
                v[cnt].col[GG] = CLIP( v[cnt].col[GG], 0.0f, 1.0f );
                v[cnt].col[BB] = CLIP( v[cnt].col[BB], 0.0f, 1.0f );
            }
            else
            {
                v[cnt].col[RR] = v[cnt].col[GG] = v[cnt].col[BB] = 0.0f;
            }

            // the application of alpha to the tile depends on the blending mode
            if( water.light )
            {
                // blend using light
                v[cnt].col[RR] *= falpha;
                v[cnt].col[GG] *= falpha;
                v[cnt].col[BB] *= falpha;
                v[cnt].col[AA]  = 1.0f;
            }
            else
            {
                // blend using alpha
                v[cnt].col[AA] = falpha;
            }

            badvertex++;
        }
    }

    // Change texture if need be
    if ( meshlasttexture != texture )
    {
        oglx_texture_Bind( ptex );
        meshlasttexture = texture;
    }

    ATTRIB_PUSH( "render_water_fan", GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_LIGHTING_BIT | GL_CURRENT_BIT | GL_POLYGON_BIT );
    {
        GLboolean use_depth_mask = (!water.light && (1.0f == falpha)) ? GL_TRUE : GL_FALSE;

        GL_DEBUG( glEnable )( GL_DEPTH_TEST );                                  // GL_ENABLE_BIT
        GL_DEBUG( glDepthFunc )( GL_LEQUAL );                                   // GL_DEPTH_BUFFER_BIT

        // only use the depth mask if the tile is NOT transparent
        GL_DEBUG( glDepthMask )( use_depth_mask );                              // GL_DEPTH_BUFFER_BIT

        GL_DEBUG( glEnable )( GL_CULL_FACE );                                   // GL_ENABLE_BIT
        GL_DEBUG( glFrontFace )( GL_CW );                                       // GL_POLYGON_BIT

        // set the blending mode
        if( water.light )
        {
            GL_DEBUG( glEnable )( GL_BLEND );                                   // GL_ENABLE_BIT
            GL_DEBUG( glBlendFunc )( GL_ONE, GL_ONE_MINUS_SRC_COLOR );          // GL_COLOR_BUFFER_BIT
        }
        else
        {
            GL_DEBUG( glEnable )( GL_BLEND );                                    // GL_ENABLE_BIT
            GL_DEBUG( glBlendFunc )( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );     // GL_COLOR_BUFFER_BIT
        }
        
        // Render each command
        GL_DEBUG( glShadeModel )( GL_SMOOTH );                // GL_LIGHTING_BIT
        GL_DEBUG( glBegin )( GL_TRIANGLE_FAN );
        {
            for ( cnt = 0; cnt < 4; cnt++ )
            {
                GL_DEBUG( glColor4fv )( v[cnt].col );         // GL_CURRENT_BIT
                GL_DEBUG( glTexCoord2fv )( v[cnt].tex );
                GL_DEBUG( glVertex3fv )( v[cnt].pos );
            }
        }
        GL_DEBUG_END();
    }
    ATTRIB_POP( "render_water_fan" );
}

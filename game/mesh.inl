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

/// @file mesh.inl

#include "mesh.h"

//--------------------------------------------------------------------------------------------
// FORWARD DECLARATIONS
//--------------------------------------------------------------------------------------------

INLINE float  mesh_get_level( ego_mpd_t * pmesh, float x, float y );
INLINE Uint32 mesh_get_block( ego_mpd_t * pmesh, float pos_x, float pos_y );
INLINE Uint32 mesh_get_tile( ego_mpd_t * pmesh, float pos_x, float pos_y );

INLINE Uint32 mesh_get_block_int( ego_mpd_t * pmesh, int block_x, int block_y );
INLINE Uint32 mesh_get_tile_int( ego_mpd_t * pmesh, int grid_x,  int grid_y );

INLINE Uint32 mesh_test_fx( ego_mpd_t * pmesh, Uint32 itile, Uint32 flags );
INLINE bool_t mesh_clear_fx( ego_mpd_t * pmesh, Uint32 itile, Uint32 flags );
INLINE bool_t mesh_add_fx( ego_mpd_t * pmesh, Uint32 itile, Uint32 flags );

//--------------------------------------------------------------------------------------------
// IMPLEMENTATION
//--------------------------------------------------------------------------------------------

INLINE float mesh_get_level( ego_mpd_t * pmesh, float x, float y )
{
    /// @details ZZ@> This function returns the height of a point within a mesh fan, precisely

    Uint32 tile;
    int ix, iy;

    float z0, z1, z2, z3;         // Height of each fan corner
    float zleft, zright, zdone;   // Weighted height of each side

    tile = mesh_get_tile( pmesh, x, y );
    if ( !VALID_GRID( pmesh, tile ) ) return 0;

    ix = x;
    iy = y;

    ix &= TILE_MASK;
    iy &= TILE_MASK;

    z0 = pmesh->tmem.plst[ pmesh->tmem.tile_list[tile].vrtstart + 0 ][ZZ];
    z1 = pmesh->tmem.plst[ pmesh->tmem.tile_list[tile].vrtstart + 1 ][ZZ];
    z2 = pmesh->tmem.plst[ pmesh->tmem.tile_list[tile].vrtstart + 2 ][ZZ];
    z3 = pmesh->tmem.plst[ pmesh->tmem.tile_list[tile].vrtstart + 3 ][ZZ];

    zleft  = ( z0 * ( GRID_SIZE - iy ) + z3 * iy ) / GRID_SIZE;
    zright = ( z1 * ( GRID_SIZE - iy ) + z2 * iy ) / GRID_SIZE;
    zdone  = ( zleft * ( GRID_SIZE - ix ) + zright * ix ) / GRID_SIZE;

    return zdone;
}

//--------------------------------------------------------------------------------------------
INLINE Uint32 mesh_get_block( ego_mpd_t * pmesh, float pos_x, float pos_y )
{
    Uint32 block = INVALID_BLOCK;

    if ( pos_x >= 0.0f && pos_x <= pmesh->gmem.edge_x && pos_y >= 0.0f && pos_y <= pmesh->gmem.edge_y )
    {
        int ix, iy;

        ix = pos_x;
        iy = pos_y;

        ix >>= BLOCK_BITS;
        iy >>= BLOCK_BITS;

        block = mesh_get_block_int( pmesh, ix, iy );
    }

    return block;
}

//--------------------------------------------------------------------------------------------
INLINE Uint32 mesh_get_tile( ego_mpd_t * pmesh, float pos_x, float pos_y )
{
    Uint32 tile = INVALID_TILE;

    if ( pos_x >= 0.0f && pos_x < pmesh->gmem.edge_x && pos_y >= 0.0f && pos_y < pmesh->gmem.edge_y )
    {
        int ix, iy;

        ix = pos_x;
        iy = pos_y;

        ix >>= TILE_BITS;
        iy >>= TILE_BITS;

        tile = mesh_get_tile_int( pmesh, ix, iy );
    }

    return tile;
}

//--------------------------------------------------------------------------------------------
INLINE Uint32 mesh_get_block_int( ego_mpd_t * pmesh, int block_x, int block_y )
{
    if ( NULL == pmesh ) return INVALID_BLOCK;

    if ( block_x < 0 || block_x >= pmesh->gmem.blocks_x )  return INVALID_BLOCK;
    if ( block_y < 0 || block_y >= pmesh->gmem.blocks_y )  return INVALID_BLOCK;

    return block_x + pmesh->gmem.blockstart[block_y];
}

//--------------------------------------------------------------------------------------------
INLINE Uint32 mesh_get_tile_int( ego_mpd_t * pmesh, int grid_x,  int grid_y )
{
    if ( NULL == pmesh ) return INVALID_TILE;

    if ( grid_x < 0 || grid_x >= pmesh->info.tiles_x )  return INVALID_TILE;
    if ( grid_y < 0 || grid_y >= pmesh->info.tiles_y )  return INVALID_TILE;

    return grid_x + pmesh->gmem.tilestart[grid_y];
}

//--------------------------------------------------------------------------------------------
INLINE bool_t mesh_clear_fx( ego_mpd_t * pmesh, Uint32 itile, Uint32 flags )
{
    Uint32 old_flags;

    // test for mesh
    if ( NULL == pmesh ) return bfalse;

    // test for invalid tile
    if ( itile > pmesh->info.tiles_count ) return bfalse;

    // save a copy of the fx
    old_flags = pmesh->gmem.grid_list[itile].fx;

    // clear the wall and impass flags
    pmesh->gmem.grid_list[itile].fx &= ~flags;

    // succeed only of something actually changed
    return 0 != ( old_flags & flags );
}

//--------------------------------------------------------------------------------------------
INLINE bool_t mesh_add_fx( ego_mpd_t * pmesh, Uint32 itile, Uint32 flags )
{
    Uint32 old_flags;

    // test for mesh
    if ( NULL == pmesh ) return bfalse;

    // test for invalid tile
    if ( itile > pmesh->info.tiles_count ) return bfalse;

    // save a copy of the fx
    old_flags = pmesh->gmem.grid_list[itile].fx;

    // add in the flags
    pmesh->gmem.grid_list[itile].fx = old_flags | flags;

    // succeed only of something actually changed
    return 0 != ( old_flags & flags );
}

//--------------------------------------------------------------------------------------------
INLINE Uint32 mesh_test_fx( ego_mpd_t * pmesh, Uint32 itile, Uint32 flags )
{
    // test for mesh
    if ( NULL == pmesh ) return 0;

    // test for invalid tile
    if ( itile > pmesh->info.tiles_count )
    {
        return flags & ( MPDFX_WALL | MPDFX_IMPASS );
    }

    // if the tile is actually labelled as FANOFF, ignore it completely
    if ( FANOFF == pmesh->tmem.tile_list[itile].img )
    {
        return flags & ( MPDFX_WALL | MPDFX_IMPASS );
    }

    return pmesh->gmem.grid_list[itile].fx & flags;
}

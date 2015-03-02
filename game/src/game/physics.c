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

/// @file game/physics.c

#include "game/physics.h"
#include "game/game.h"
#include "game/char.h"
#include "game/mesh.h"
#include "game/entities/_Include.hpp"

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
float   hillslide       =  1.00f;
float   slippyfriction  =  1.00f;
float   airfriction     =  0.91f;
float   waterfriction   =  0.80f;
float   noslipfriction  =  0.91f;
float   gravity         = -1.00f;
const float PLATFORM_STICKINESS =  0.01f;
fvec3_t windspeed       = fvec3_t::zero;
fvec3_t waterspeed      = fvec3_t::zero;

static int breadcrumb_guid = 0;

const float air_friction = 0.9868f;
const float ice_friction = 0.9738f;  // the square of air_friction

static egolib_rv phys_intersect_oct_bb_index( int index, const oct_bb_t * src1, const oct_vec_v2_t& ovel1, const oct_bb_t *  src2, const oct_vec_v2_t& ovel2, int test_platform, float *tmin, float *tmax );
static egolib_rv phys_intersect_oct_bb_close_index( int index, const oct_bb_t * src1, const oct_vec_v2_t& ovel1, const oct_bb_t *  src2, const oct_vec_v2_t& ovel2, int test_platform, float *tmin, float *tmax );

/// @brief A test to determine whether two "fast moving" objects are interacting within a frame.
///        Designed to determine whether a bullet particle will interact with character.
static bool phys_intersect_oct_bb_close( const oct_bb_t * src1_orig, const fvec3_t& pos1, const fvec3_t& vel1, const oct_bb_t *  src2_orig, const fvec3_base_t pos2, const fvec3_base_t vel2, int test_platform, oct_bb_t * pdst, float *tmin, float *tmax );
static bool phys_estimate_depth( const oct_vec_v2_t& podepth, const float exponent, fvec3_t& nrm, float * depth );
static float phys_get_depth( const oct_vec_v2_t& podepth, const fvec3_t& nrm );
static bool phys_warp_normal( const float exponent, fvec3_t& nrm );
static bool phys_get_pressure_depth( const oct_bb_t * pbb_a, const oct_bb_t * pbb_b, oct_vec_v2_t& podepth);
static bool phys_get_collision_depth( const oct_bb_t * pbb_a, const oct_bb_t * pbb_b, oct_vec_v2_t& podepth);

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
bool phys_get_collision_depth( const oct_bb_t * pbb_a, const oct_bb_t * pbb_b, oct_vec_v2_t& podepth )
{
    int cnt;
    float fdiff, fdepth;
    bool retval;

    oct_bb_t otmp;
    oct_vec_v2_t opos_a, opos_b;

    podepth.setZero();

    // are the initial volumes any good?
    if ( NULL == pbb_a || pbb_a->empty ) return false;
    if ( NULL == pbb_b || pbb_b->empty ) return false;

    // is there any overlap?
    if ( rv_success != oct_bb_intersection( pbb_a, pbb_b, &otmp ) )
    {
        return false;
    }

    // estimate the "cm position" of the objects by the bounding volumes
    for ( cnt = 0; cnt < OCT_COUNT; cnt++ )
    {
        opos_a[cnt] = ( pbb_a->maxs[cnt] + pbb_a->mins[cnt] ) * 0.5f;
        opos_b[cnt] = ( pbb_b->maxs[cnt] + pbb_b->mins[cnt] ) * 0.5f;
    }

    // find the (signed) depth in each dimension
    retval = true;
    for ( cnt = 0; cnt < OCT_COUNT; cnt++ )
    {
        fdiff  = opos_b[cnt] - opos_a[cnt];
        fdepth = otmp.maxs[cnt] - otmp.mins[cnt];

        // if the measured depth is less than zero, or the difference in positions
        // is ambiguous, this algorithm fails
        if ( fdepth <= 0.0f || 0.0f == fdiff ) retval = false;

        podepth[cnt] = ( fdiff < 0.0f ) ? -fdepth : fdepth;
    }
    podepth[OCT_XY] *= INV_SQRT_TWO;
    podepth[OCT_YX] *= INV_SQRT_TWO;

    return retval;
}

//--------------------------------------------------------------------------------------------
bool phys_get_pressure_depth( const oct_bb_t * pbb_a, const oct_bb_t * pbb_b, oct_vec_v2_t& podepth )
{
    int cnt;
    bool rv;

    podepth.setZero();

    if ( NULL == pbb_a || NULL == pbb_b ) return false;

    // assume the best
    rv = true;

    // scan through the dimensions of the oct_bbs
    for ( cnt = 0; cnt < OCT_COUNT; cnt++ )
    {
        float diff1 = 0.0f;
        float diff2 = 0.0f;

        diff1 = pbb_a->maxs[cnt] - pbb_b->mins[cnt];
        diff2 = pbb_b->maxs[cnt] - pbb_a->mins[cnt];

        if ( diff1 < 0.0f || diff2 < 0.0f )
        {
            // this case will only happen if there is no overlap in one of the dimensions,
            // meaning there was a bad collision detection... it should NEVER happen.
            // In any case this math still generates the proper direction for
            // the normal pointing away from b.
            if ( ABS( diff1 ) < ABS( diff2 ) )
            {
                podepth[cnt] = diff1;
            }
            else
            {
                podepth[cnt] = -diff2;
            }

            rv = false;
        }
        else if ( diff1 < diff2 )
        {
            podepth[cnt] = -diff1;
        }
        else
        {
            podepth[cnt] = diff2;
        }
    }

    return rv;
}

//--------------------------------------------------------------------------------------------
bool phys_warp_normal( const float exponent, fvec3_t& nrm )
{
    // use the exponent to warp the normal into a cylinder-like shape, if needed

    if ( 1.0f == exponent ) return true;

    if ( 0.0f == nrm.length_abs() ) return false;

    float length_hrz_2 = fvec2_t(nrm[kX],nrm[kY]).length_2();
    float length_vrt_2 = nrm.length_2() - length_hrz_2;

    nrm[kX] = nrm[kX] * POW( length_hrz_2, 0.5f * ( exponent - 1.0f ) );
    nrm[kY] = nrm[kY] * POW( length_hrz_2, 0.5f * ( exponent - 1.0f ) );
    nrm[kZ] = nrm[kZ] * POW( length_vrt_2, 0.5f * ( exponent - 1.0f ) );

    // normalize the normal
	nrm.normalize();
    return nrm.length() >= 0.0f;
}

//--------------------------------------------------------------------------------------------
float phys_get_depth( const oct_vec_v2_t& podepth, const fvec3_t& nrm )
{
    const float max_val = 1e6;

    int cnt;
    float depth, ftmp;
    oct_vec_v2_t onrm;

    if ( 0.0f == nrm.length_abs() ) return max_val;

    // convert the normal into an oct_vec_t
    onrm.ctor( nrm );
    onrm[OCT_XY] *= INV_SQRT_TWO;
    onrm[OCT_YX] *= INV_SQRT_TWO;

    // calculate the minimum depth in each dimension
    depth = max_val;
    for ( cnt = 0; cnt < OCT_COUNT; cnt++ )
    {
        if ( 0.0f == podepth[cnt] )
        {
            depth = 0.0f;
            break;
        }

        if ( 0.0f != onrm[cnt] )
        {
            ftmp = podepth[cnt] / onrm[cnt];
            if ( ftmp <= 0.0f )
            {
                depth = 0.0f;
                break;
            }

            depth = std::min( depth, ftmp );
        }
    }

    return depth;
}

//--------------------------------------------------------------------------------------------
bool phys_estimate_depth( const oct_vec_v2_t& podepth, const float exponent, fvec3_t& nrm, float * depth )
{
    // use the given (signed) podepth info to make a normal vector, and measure
    // the shortest distance to the border

    float   tmin_aa, tmin_diag, ftmp, tmin;
    fvec3_t nrm_aa, nrm_diag;

    bool rv;


    if ( 0.0f != podepth[OCT_X] ) nrm_aa.x = 1.0f / podepth[OCT_X];
    if ( 0.0f != podepth[OCT_Y] ) nrm_aa.y = 1.0f / podepth[OCT_Y];
    if ( 0.0f != podepth[OCT_Z] ) nrm_aa.z = 1.0f / podepth[OCT_Z];

    if ( 1.0f == exponent )
    {
        nrm_aa.normalize();
    }
    else
    {
        phys_warp_normal( exponent, nrm_aa );
    }

    // find a minimum distance
    tmin_aa = 1e6;

    if ( nrm_aa.x != 0.0f )
    {
        ftmp = podepth[OCT_X] / nrm_aa.x;
        ftmp = std::max( 0.0f, ftmp );
        tmin_aa = std::min( tmin_aa, ftmp );
    }

    if ( nrm_aa.y != 0.0f )
    {
        ftmp = podepth[OCT_Y] / nrm_aa.y;
        ftmp = std::max( 0.0f, ftmp );
        tmin_aa = std::min( tmin_aa, ftmp );
    }

    if ( nrm_aa.z != 0.0f )
    {
        ftmp = podepth[OCT_Z] / nrm_aa.z;
        ftmp = std::max( 0.0f, ftmp );
        tmin_aa = std::min( tmin_aa, ftmp );
    }

    if ( tmin_aa <= 0.0f || tmin_aa >= 1e6 ) return false;

    // next do the diagonal axes
	nrm_diag = fvec3_t::zero;

    if ( 0.0f != podepth[OCT_XY] ) nrm_diag.x = 1.0f / (podepth[OCT_XY] * INV_SQRT_TWO );
    if ( 0.0f != podepth[OCT_YX] ) nrm_diag.y = 1.0f / (podepth[OCT_YX] * INV_SQRT_TWO );
    if ( 0.0f != podepth[OCT_Z ] ) nrm_diag.z = 1.0f / podepth[OCT_Z];

    if ( 1.0f == exponent )
    {
        nrm_diag.normalize();
    }
    else
    {
        phys_warp_normal( exponent, nrm_diag );
    }

    // find a minimum distance
    tmin_diag = 1e6;

    if ( nrm_diag.x != 0.0f )
    {
        ftmp = INV_SQRT_TWO * podepth[OCT_XY] / nrm_diag.x;
        ftmp = std::max( 0.0f, ftmp );
        tmin_diag = std::min( tmin_diag, ftmp );
    }

    if ( nrm_diag.y != 0.0f )
    {
        ftmp = INV_SQRT_TWO * podepth[OCT_YX] / nrm_diag.y;
        ftmp = std::max( 0.0f, ftmp );
        tmin_diag = std::min( tmin_diag, ftmp );
    }

    if ( nrm_diag.z != 0.0f )
    {
        ftmp = podepth[OCT_Z] / nrm_diag.z;
        ftmp = std::max( 0.0f, ftmp );
        tmin_diag = std::min( tmin_diag, ftmp );
    }

    if ( tmin_diag <= 0.0f || tmin_diag >= 1e6 ) return false;

    if ( tmin_aa < tmin_diag )
    {
        tmin = tmin_aa;
		nrm = nrm_aa;
    }
    else
    {
        tmin = tmin_diag;

        // !!!! rotate the diagonal axes onto the axis aligned ones !!!!!
        nrm[kX] = ( nrm_diag.x - nrm_diag.y ) * INV_SQRT_TWO;
        nrm[kY] = ( nrm_diag.x + nrm_diag.y ) * INV_SQRT_TWO;
        nrm[kZ] = nrm_diag.z;
    }

    // normalize this normal
	nrm.normalize();
    rv = nrm.length() > 0.0f;

    // find the depth in the direction of the normal, if possible
    if ( rv && NULL != depth )
    {
        *depth = tmin;
    }

    return rv;
}

//--------------------------------------------------------------------------------------------
bool phys_estimate_collision_normal( const oct_bb_t * pobb_a, const oct_bb_t * pobb_b, const float exponent, oct_vec_v2_t& podepth, fvec3_t& nrm, float * depth )
{
    // estimate the normal for collision volumes that are partially overlapping

    bool use_pressure;

    // is everything valid?
    if ( NULL == pobb_a || NULL == pobb_b ) return false;

    // do we need to use the more expensive algorithm?
    use_pressure = false;
    if ( oct_bb_t::contains( pobb_a, pobb_b ) )
    {
        use_pressure = true;
    }
    else if ( oct_bb_t::contains( pobb_b, pobb_a ) )
    {
        use_pressure = true;
    }

    if ( !use_pressure )
    {
        // try to get the collision depth using the given oct_bb's
        if ( !phys_get_collision_depth( pobb_a, pobb_b, podepth ) )
        {
            use_pressure = true;
        }
    }

    if ( use_pressure )
    {
        return phys_estimate_pressure_normal( pobb_a, pobb_b, exponent, podepth, nrm, depth );
    }

    return phys_estimate_depth( podepth, exponent, nrm, depth );
}

//--------------------------------------------------------------------------------------------
bool phys_estimate_pressure_normal( const oct_bb_t * pobb_a, const oct_bb_t * pobb_b, const float exponent, oct_vec_v2_t& podepth, fvec3_t& nrm, float * depth )
{
    // use a more robust algorithm to get the normal no matter how the 2 volumes are
    // related

    float     loc_tmin;
    fvec3_t   loc_nrm;
    oct_vec_t loc_odepth;

    bool rv;

    // handle "optional" parameters
    if ( NULL == depth ) depth = &loc_tmin;
    if ( NULL == pobb_a || NULL == pobb_b ) return false;

    // calculate the direction of the nearest way out for each octagonal axis
    rv = phys_get_pressure_depth( pobb_a, pobb_b, podepth );
    if ( !rv ) return false;

    return phys_estimate_depth( podepth, exponent, nrm, depth );
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
egolib_rv phys_intersect_oct_bb_index( int index, const oct_bb_t * src1, const oct_vec_v2_t& ovel1, const oct_bb_t * src2, const oct_vec_v2_t& ovel2, int test_platform, float *tmin, float *tmax )
{
    float vdiff;
    float src1_min, src1_max;
    float src2_min, src2_max;

    if ( NULL == tmin || NULL == tmax ) return rv_error;

    if ( index < 0 || index >= OCT_COUNT ) return rv_error;

    vdiff = ovel2[index] - ovel1[index];
    if ( 0.0f == vdiff ) return rv_fail;

    src1_min = src1->mins[index];
    src1_max = src1->maxs[index];
    src2_min = src2->mins[index];
    src2_max = src2->maxs[index];

    if ( OCT_Z != index )
    {
        bool close_test_1, close_test_2;

        // is there any possibility of the 2 objects acting as a platform pair
        close_test_1 = HAS_SOME_BITS( test_platform, PHYS_PLATFORM_OBJ1 );
        close_test_2 = HAS_SOME_BITS( test_platform, PHYS_PLATFORM_OBJ2 );

        // only do a close test if the object's feet are above the platform
        close_test_1 = close_test_1 && ( src1->mins[OCT_Z] > src2->maxs[OCT_Z] );
        close_test_2 = close_test_2 && ( src2->mins[OCT_Z] > src1->maxs[OCT_Z] );

        if ( !close_test_1 && !close_test_2 )
        {
            // NEITHER ia a platform
            float time[4];

            time[0] = ( src1_min - src2_min ) / vdiff;
            time[1] = ( src1_min - src2_max ) / vdiff;
            time[2] = ( src1_max - src2_min ) / vdiff;
            time[3] = ( src1_max - src2_max ) / vdiff;

            *tmin = std::min( std::min( time[0], time[1] ), std::min( time[2], time[3] ) );
            *tmax = std::max( std::max( time[0], time[1] ), std::max( time[2], time[3] ) );
        }
        else
        {
            return phys_intersect_oct_bb_close_index( index, src1, ovel1, src2, ovel2, test_platform, tmin, tmax );
        }
    }
    else /* OCT_Z == index */
    {
        float tolerance_1, tolerance_2;
        float plat_min, plat_max;

        // add in a tolerance into the vertical direction for platforms
        tolerance_1 = HAS_SOME_BITS( test_platform, PHYS_PLATFORM_OBJ1 ) ? PLATTOLERANCE : 0.0f;
        tolerance_2 = HAS_SOME_BITS( test_platform, PHYS_PLATFORM_OBJ2 ) ? PLATTOLERANCE : 0.0f;

        if ( 0.0f == tolerance_1 && 0.0f == tolerance_2 )
        {
            // NEITHER ia a platform

            float time[4];

            time[0] = ( src1_min - src2_min ) / vdiff;
            time[1] = ( src1_min - src2_max ) / vdiff;
            time[2] = ( src1_max - src2_min ) / vdiff;
            time[3] = ( src1_max - src2_max ) / vdiff;

            *tmin = std::min( std::min( time[0], time[1] ), std::min( time[2], time[3] ) );
            *tmax = std::max( std::max( time[0], time[1] ), std::max( time[2], time[3] ) );
        }
        else if ( 0.0f == tolerance_1 )
        {
            float time[4];

            // obj2 is platform
            plat_min = src2_min;
            plat_max = src2_max + tolerance_2;

            time[0] = ( src1_min - plat_min ) / vdiff;
            time[1] = ( src1_min - plat_max ) / vdiff;
            time[2] = ( src1_max - plat_min ) / vdiff;
            time[3] = ( src1_max - plat_max ) / vdiff;

            *tmin = std::min( std::min( time[0], time[1] ), std::min( time[2], time[3] ) );
            *tmax = std::max( std::max( time[0], time[1] ), std::max( time[2], time[3] ) );
        }
        else if ( 0.0f == tolerance_2 )
        {
            // ONLY src1 is a platform
            float time[4];

            // obj1 is platform
            plat_min = src1_min;
            plat_max = src1_max + tolerance_2;

            time[0] = ( plat_min - src2_min ) / vdiff;
            time[1] = ( plat_min - src2_max ) / vdiff;
            time[2] = ( plat_max - src2_min ) / vdiff;
            time[3] = ( plat_max - src2_max ) / vdiff;

            *tmin = std::min( std::min( time[0], time[1] ), std::min( time[2], time[3] ) );
            *tmax = std::max( std::max( time[0], time[1] ), std::max( time[2], time[3] ) );
        }

        else if ( tolerance_1 > 0.0f && tolerance_2 > 0.0f )
        {
            // BOTH are platforms
            // they cannot both act as plaforms at the same time, so do 8 tests

            float time[8];
            float tmp_min1, tmp_max1;
            float tmp_min2, tmp_max2;

            // obj2 is platform
            plat_min = src2_min;
            plat_max = src2_max + tolerance_2;

            time[0] = ( src1_min - plat_min ) / vdiff;
            time[1] = ( src1_min - plat_max ) / vdiff;
            time[2] = ( src1_max - plat_min ) / vdiff;
            time[3] = ( src1_max - plat_max ) / vdiff;
            tmp_min1 = std::min( std::min( time[0], time[1] ), std::min( time[2], time[3] ) );
            tmp_max1 = std::max( std::max( time[0], time[1] ), std::max( time[2], time[3] ) );

            // obj1 is platform
            plat_min = src1_min;
            plat_max = src1_max + tolerance_2;

            time[4] = ( plat_min - src2_min ) / vdiff;
            time[5] = ( plat_min - src2_max ) / vdiff;
            time[6] = ( plat_max - src2_min ) / vdiff;
            time[7] = ( plat_max - src2_max ) / vdiff;
            tmp_min2 = std::min( std::min( time[4], time[5] ), std::min( time[6], time[7] ) );
            tmp_max2 = std::max( std::max( time[4], time[5] ), std::max( time[6], time[7] ) );

            *tmin = std::min( tmp_min1, tmp_min2 );
            *tmax = std::max( tmp_max1, tmp_max2 );
        }
    }

    // normalize the results for the diagonal directions
    if ( OCT_XY == index || OCT_YX == index )
    {
        *tmin *= INV_SQRT_TWO;
        *tmax *= INV_SQRT_TWO;
    }

    if ( *tmax <= *tmin ) return rv_fail;

    return rv_success;
}

//--------------------------------------------------------------------------------------------
bool phys_intersect_oct_bb( const oct_bb_t * src1_orig, const fvec3_t& pos1, const fvec3_t& vel1, const oct_bb_t * src2_orig, const fvec3_t& pos2, const fvec3_t& vel2, int test_platform, oct_bb_t * pdst, float *tmin, float *tmax )
{
    /// @author BB
	/// @details A test to determine whether two "fast moving" objects are interacting within a frame.
	///               Designed to determine whether a bullet particle will interact with character.


    oct_bb_t src1, src2;
    oct_bb_t exp1, exp2;

    int    index;
    bool found;
    float  local_tmin, local_tmax;

    int    failure_count = 0;
    bool failure[OCT_COUNT];

    // handle optional parameters
    if ( NULL == tmin ) tmin = &local_tmin;
    if ( NULL == tmax ) tmax = &local_tmax;

    // convert the position and velocity vectors to octagonal format
    oct_vec_v2_t opos1(pos1), opos2(pos2),
                 ovel1(vel1), ovel2(vel2);

    // shift the bounding boxes to their starting positions
    oct_bb_add_ovec( src1_orig, opos1, &src1 );
    oct_bb_add_ovec( src2_orig, opos2, &src2 );

    found = false;
    *tmin = +1.0e6;
    *tmax = -1.0e6;
    if ( fvec3_dist_abs( vel1, vel2 ) < 1.0e-6 )
    {
        // no relative motion, so avoid the loop to save time
        failure_count = OCT_COUNT;
    }
    else
    {
        // cycle through the coordinates to see when the two volumes might coincide
        for ( index = 0; index < OCT_COUNT; index++ )
        {
            egolib_rv retval;

            if ( ABS( ovel1[index] - ovel2[index] ) < 1.0e-6 )
            {
                failure[index] = true;
                failure_count++;
            }
            else
            {
                float tmp_min = 0.0f, tmp_max = 0.0f;

                retval = phys_intersect_oct_bb_index( index, &src1, ovel1, &src2, ovel2, test_platform, &tmp_min, &tmp_max );

                // check for overflow
                if (float_bad( tmp_min ) || float_bad(tmp_max))
                {
                    retval = rv_fail;
                }

                if ( rv_fail == retval )
                {
                    // This case will only occur if the objects are not moving relative to each other.

                    failure[index] = true;
                    failure_count++;
                }
                else if ( rv_success == retval )
                {
                    failure[index] = false;

                    if ( !found )
                    {
                        *tmin = tmp_min;
                        *tmax = tmp_max;
                        found = true;
                    }
                    else
                    {
                        *tmin = std::max( *tmin, tmp_min );
                        *tmax = std::min( *tmax, tmp_max );
                    }

                    // check the values vs. reasonable bounds
                    if ( *tmax <= *tmin ) return false;
                    if ( *tmin > 1.0f || *tmax < 0.0f ) return false;
                }
            }
        }
    }

    if ( OCT_COUNT == failure_count )
    {
        // No relative motion on any axis.
        // Just say that they are interacting for the whole frame

        *tmin = 0.0f;
        *tmax = 1.0f;

        // determine the intersection of these two expanded volumes (for this frame)
        oct_bb_intersection( &src1, &src2, pdst );
    }
    else
    {
        float tmp_min, tmp_max;

        // check to see if there the intersection times make any sense
        if ( *tmax <= *tmin ) return false;

        // check whether there is any overlap this frame
        if ( *tmin >= 1.0f || *tmax <= 0.0f ) return false;

        // clip the interaction time to just one frame
        tmp_min = CLIP( *tmin, 0.0f, 1.0f );
        tmp_max = CLIP( *tmax, 0.0f, 1.0f );

        // determine the expanded collision volumes for both objects (for this frame)
        phys_expand_oct_bb( &src1, vel1, tmp_min, tmp_max, &exp1 );
        phys_expand_oct_bb( &src2, vel2, tmp_min, tmp_max, &exp2 );

        // determine the intersection of these two expanded volumes (for this frame)
        oct_bb_intersection( &exp1, &exp2, pdst );
    }

    if ( 0 != test_platform )
    {
        pdst->maxs[OCT_Z] += PLATTOLERANCE;
        oct_bb_t::validate( pdst );
    }

    if ( pdst->empty ) return false;

    return true;
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
egolib_rv phys_intersect_oct_bb_close_index( int index, const oct_bb_t * src1, const oct_vec_v2_t& ovel1, const oct_bb_t * src2, const oct_vec_v2_t& ovel2, int test_platform, float *tmin, float *tmax )
{
    float vdiff;
    float opos1, opos2;
    float src1_min, src1_max;
    float src2_min, src2_max;

    if ( NULL == tmin || NULL == tmax ) return rv_error;

    if ( index < 0 || index >= OCT_COUNT ) return rv_error;

    vdiff = ovel2[index] - ovel1[index];
    if ( 0.0f == vdiff ) return rv_fail;

    src1_min = src1->mins[index];
    src1_max = src1->maxs[index];
    opos1 = ( src1_min + src1_max ) * 0.5f;

    src2_min = src2->mins[index];
    src2_max = src2->maxs[index];
    opos2 = ( src2_min + src2_max ) * 0.5f;

    if ( OCT_Z != index )
    {
        bool platform_1;
        bool platform_2;

        platform_1 = HAS_SOME_BITS( test_platform, PHYS_PLATFORM_OBJ1 );
        platform_2 = HAS_SOME_BITS( test_platform, PHYS_PLATFORM_OBJ2 );

        if ( !platform_1 && !platform_2 )
        {
            // NEITHER ia a platform
            // use the eqn. from phys_intersect_oct_bb_index()

            float time[4];

            time[0] = ( src1_min - src2_min ) / vdiff;
            time[1] = ( src1_min - src2_max ) / vdiff;
            time[2] = ( src1_max - src2_min ) / vdiff;
            time[3] = ( src1_max - src2_max ) / vdiff;

			*tmin = std::min({ time[0], time[1], time[2], time[3] });
			*tmax = std::max({ time[0], time[1], time[2], time[3] });
        }
        else if ( platform_1 && !platform_2 )
        {
            float time[2];

            // obj1 is the platform
            time[0] = ( src1_min - opos2 ) / vdiff;
            time[1] = ( src1_max - opos2 ) / vdiff;

            *tmin = std::min( time[0], time[1] );
            *tmax = std::max( time[0], time[1] );
        }
        else if ( !platform_1 && platform_2 )
        {
            float time[2];

            // obj2 is the platform
            time[0] = ( opos1 - src2_min ) / vdiff;
            time[1] = ( opos1 - src2_max ) / vdiff;

            *tmin = std::min( time[0], time[1] );
            *tmax = std::max( time[0], time[1] );
        }
        else
        {
            // BOTH are platforms. must check all possibilities
            float time[4];

            // object 1 is the platform
            time[0] = ( src1_min - opos2 ) / vdiff;
            time[1] = ( src1_max - opos2 ) / vdiff;

            // object 2 is the platform
            time[2] = ( opos1 - src2_min ) / vdiff;
            time[3] = ( opos1 - src2_max ) / vdiff;

            *tmin = std::min( std::min( time[0], time[1] ), std::min( time[2], time[3] ) );
            *tmax = std::max( std::max( time[0], time[1] ), std::max( time[2], time[3] ) );
        }
    }
    else /* OCT_Z == index */
    {
        float tolerance_1, tolerance_2;

        float plat_min, plat_max;
        float obj_pos;

        tolerance_1 =  HAS_SOME_BITS( test_platform, PHYS_PLATFORM_OBJ1 ) ? PLATTOLERANCE : 0.0f;
        tolerance_2 =  HAS_SOME_BITS( test_platform, PHYS_PLATFORM_OBJ2 ) ? PLATTOLERANCE : 0.0f;

        if ( 0.0f == tolerance_1 && 0.0f == tolerance_2 )
        {
            // NEITHER ia a platform
            // use the eqn. from phys_intersect_oct_bb_index()

            float time[4];

            time[0] = ( src1_min - src2_min ) / vdiff;
            time[1] = ( src1_min - src2_max ) / vdiff;
            time[2] = ( src1_max - src2_min ) / vdiff;
            time[3] = ( src1_max - src2_max ) / vdiff;

            *tmin = std::min( std::min( time[0], time[1] ), std::min( time[2], time[3] ) );
            *tmax = std::max( std::max( time[0], time[1] ), std::max( time[2], time[3] ) );
        }
        else if ( 0.0f != tolerance_1 && 0.0f == tolerance_2 )
        {
            float time[2];

            // obj1 is the platform
            obj_pos  = src2_min;
            plat_min = src1_min;
            plat_max = src1_max + tolerance_1;

            time[0] = ( plat_min - obj_pos ) / vdiff;
            time[1] = ( plat_max - obj_pos ) / vdiff;

            *tmin = std::min( time[0], time[1] );
            *tmax = std::max( time[0], time[1] );
        }
        else if ( 0.0f == tolerance_1 && 0.0f != tolerance_2 )
        {
            float time[2];

            // obj2 is the platform
            obj_pos  = src1_min;
            plat_min = src2_min;
            plat_max = src2_max + tolerance_2;

            time[0] = ( obj_pos - plat_min ) / vdiff;
            time[1] = ( obj_pos - plat_max ) / vdiff;

            *tmin = std::min( time[0], time[1] );
            *tmax = std::max( time[0], time[1] );
        }
        else
        {
            // BOTH are platforms
            float time[4];

            // obj2 is the platform
            obj_pos  = src1_min;
            plat_min = src2_min;
            plat_max = src2_max + tolerance_2;

            time[0] = ( obj_pos - plat_min ) / vdiff;
            time[1] = ( obj_pos - plat_max ) / vdiff;

            // obj1 is the platform
            obj_pos  = src2_min;
            plat_min = src1_min;
            plat_max = src1_max + tolerance_1;

            time[2] = ( plat_min - obj_pos ) / vdiff;
            time[3] = ( plat_max - obj_pos ) / vdiff;

            *tmin = std::min( std::min( time[0], time[1] ), std::min( time[2], time[3] ) );
            *tmax = std::max( std::max( time[0], time[1] ), std::max( time[2], time[3] ) );
        }
    }

    // normalize the results for the diagonal directions
    if ( OCT_XY == index || OCT_YX == index )
    {
        *tmin *= INV_SQRT_TWO;
        *tmax *= INV_SQRT_TWO;
    }

    if ( *tmax < *tmin ) return rv_fail;

    return rv_success;
}

//--------------------------------------------------------------------------------------------
bool phys_intersect_oct_bb_close( const oct_bb_t * src1_orig, const fvec3_t& pos1, const fvec3_t& vel1, const oct_bb_t *  src2_orig, const fvec3_t& pos2, const fvec3_t& vel2, int test_platform, oct_bb_t * pdst, float *tmin, float *tmax )
{
    oct_bb_t src1, src2;
    oct_bb_t exp1, exp2;

    oct_bb_t intersection;

    int    cnt, index;
    bool found;
    float  tolerance;
    float  local_tmin, local_tmax;

    // handle optional parameters
    if ( NULL == tmin ) tmin = &local_tmin;
    if ( NULL == tmax ) tmax = &local_tmax;

    // do the objects interact at the very beginning of the update?
    if ( test_interaction_2( src1_orig, pos2, src2_orig, pos2, test_platform ) )
    {
        if ( NULL != pdst )
        {
            oct_bb_intersection( src1_orig, src2_orig, pdst );
        }

        return true;
    }

    // convert the position and velocity vectors to octagonal format
    oct_vec_v2_t opos1(pos1), opos2(pos2),
                 ovel1(vel1), ovel2(vel2);

    oct_bb_add_ovec( src1_orig, opos1, &src1 );
    oct_bb_add_ovec( src2_orig, opos2, &src2 );

    // cycle through the coordinates to see when the two volumes might coincide
    found = false;
    *tmin = *tmax = -1.0f;
    for ( index = 0; index < OCT_COUNT; index ++ )
    {
        egolib_rv retval;
        float tmp_min, tmp_max;

        retval = phys_intersect_oct_bb_close_index( index, &src1, ovel1, &src2, ovel2, test_platform, &tmp_min, &tmp_max );
        if ( rv_fail == retval ) return false;

        if ( rv_success == retval )
        {
            if ( !found )
            {
                *tmin = tmp_min;
                *tmax = tmp_max;
                found = true;
            }
            else
            {
                *tmin = std::max( *tmin, tmp_min );
                *tmax = std::min( *tmax, tmp_max );
            }
        }

        if ( *tmax < *tmin ) return false;
    }

    // if the objects do not interact this frame let the caller know
    if ( *tmin > 1.0f || *tmax < 0.0f ) return false;

    // determine the expanded collision volumes for both objects
    phys_expand_oct_bb( &src1, vel1, *tmin, *tmax, &exp1 );
    phys_expand_oct_bb( &src2, vel2, *tmin, *tmax, &exp2 );

    // determine the intersection of these two volumes
    oct_bb_intersection( &exp1, &exp2, &intersection );

    // check to see if there is any possibility of interaction at all
    for ( cnt = 0; cnt < OCT_Z; cnt++ )
    {
        if ( intersection.mins[cnt] > intersection.maxs[cnt] ) return false;
    }

    tolerance = ( 0 == test_platform ) ? 0.0f : PLATTOLERANCE;
    if ( intersection.mins[OCT_Z] > intersection.maxs[OCT_Z] + tolerance ) return false;

    return true;
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
bool phys_expand_oct_bb(const oct_bb_t *psrc, const fvec3_t& vel, const float tmin, const float tmax, oct_bb_t *pdst)
{
    oct_bb_t tmp_min, tmp_max;

    float abs_vel = vel.length_abs();
    if ( 0.0f == abs_vel )
    {
        return oct_bb_copy( pdst, psrc ) ? true : false;
    }

    // determine the bounding volume at t == tmin
    if ( 0.0f == tmin )
    {
        oct_bb_copy( &tmp_min, psrc );
    }
    else
    {
        fvec3_t tmp_diff = vel * tmin;
        // adjust the bounding box to take in the position at the next step
        if ( !oct_bb_add_fvec3( psrc, tmp_diff, &tmp_min ) ) return false;
    }

    // determine the bounding volume at t == tmax
    if ( tmax == 0.0f )
    {
        oct_bb_copy( &tmp_max, psrc );
    }
    else
    {
        fvec3_t tmp_diff = vel * tmax;
        // adjust the bounding box to take in the position at the next step
        if ( !oct_bb_add_fvec3( psrc, tmp_diff, &tmp_max ) ) return false;
    }

    // determine bounding box for the range of times
    if ( !oct_bb_union( &tmp_min, &tmp_max, pdst ) ) return false;

    return true;
}

//--------------------------------------------------------------------------------------------
bool phys_expand_chr_bb( Object * pchr, float tmin, float tmax, oct_bb_t * pdst )
{
    if ( !ACTIVE_PCHR( pchr ) ) return false;

    // copy the volume
    oct_bb_t tmp_oct1 = pchr->chr_max_cv;

    // add in the current position to the bounding volume
    oct_bb_t tmp_oct2;
    oct_bb_add_fvec3( &( tmp_oct1 ), pchr->getPosition(), &tmp_oct2 );

    // streach the bounging volume to cover the path of the object
    return phys_expand_oct_bb( &tmp_oct2, pchr->vel, tmin, tmax, pdst );
}

//--------------------------------------------------------------------------------------------
bool phys_expand_prt_bb( prt_t * pprt, float tmin, float tmax, oct_bb_t * pdst )
{
    /// @author BB
    /// @details use the object velocity to figure out where the volume that the particle will
    ///               occupy during this update

    if ( !ACTIVE_PPRT( pprt ) ) return false;

    // copy the volume
    oct_bb_t tmp_oct1 = pprt->prt_max_cv;

    // add in the current position to the bounding volume
    oct_bb_t tmp_oct2;
    oct_bb_add_fvec3( &tmp_oct1, pprt->pos, &tmp_oct2 );

    // streach the bounging volume to cover the path of the object
    return phys_expand_oct_bb( &tmp_oct2, pprt->vel, tmin, tmax, pdst );
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

/**
 * @brief
 *  Snap a world coordinate point to grid.
 *  
 *  The point is moved along the x- and y-axis such that it is centered on the tile it is on.
 * @param p
 *  the point
 * @return
 *  the snapped world coordinate point
 */
static fvec3_t snap(const fvec3_t& p)
{
    return fvec3_t((FLOOR(p.x / GRID_FSIZE) + 0.5f) * GRID_FSIZE,
                   (FLOOR(p.y / GRID_FSIZE) + 0.5f) * GRID_FSIZE,
                   p.z);
}

breadcrumb_t *breadcrumb_t::init(breadcrumb_t *self, Object *chr)
{
    if (!self)
    {
        throw std::invalid_argument("nullptr == self");
    }
    if (!chr)
    {
        throw std::invalid_argument("nullptr == chr");
    }
    self->reset();
    self->time = update_wld;

    self->bits   = chr->stoppedby;
    self->radius = chr->bump_1.size;
    self->pos = snap(chr->getPosition());
    self->grid   = ego_mesh_t::get_grid(PMesh, PointWorld(self->pos.x, self->pos.y)).getI();
    self->valid  = (0 == ego_mesh_test_wall(PMesh, self->pos, self->radius, self->bits, nullptr));

    self->id = breadcrumb_guid++;

    return self;
}

breadcrumb_t *breadcrumb_t::init(breadcrumb_t *self, prt_t *particle)
{
    if (!self)
    {
        throw std::invalid_argument("nullptr == self");
    }
    if (!particle)
    {
        throw std::invalid_argument("nullptr == particle");
    }
    pip_t *profile = prt_get_ppip(GET_REF_PPRT(particle));
    if (!profile)
    {
        throw std::invalid_argument("nullptr == prpfile");
    }
    self->reset();
    self->time = update_wld;


    BIT_FIELD bits = MAPFX_IMPASS;
    if (0 != profile->bump_money) SET_BIT(bits, MAPFX_WALL);

    self->bits = bits;
    self->radius = particle->bump_real.size;

    fvec3_t pos = prt_t::get_pos_v_const(particle);
    self->pos = snap(prt_t::get_pos_v_const(particle));
    self->grid   = ego_mesh_t::get_grid(PMesh, PointWorld(self->pos.x, self->pos.y)).getI();
    self->valid  = ( 0 == ego_mesh_test_wall( PMesh, self->pos, self->radius, self->bits, nullptr));

    self->id = breadcrumb_guid++;

    return self;
}

bool breadcrumb_t::cmp(const breadcrumb_t& x, const breadcrumb_t& y)
{
    if (x.time < y.time)
    {
        return true;
    }
    else if (x.time > y.time)
    {
        return false;
    }
    if (x.id < y.id)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void breadcrumb_list_t::validate(breadcrumb_list_t *self)
{
    if (!self || !self->on) return;

    // Invalidate breadcrumbs which are not valid anymore.
	size_t invalid = 0;
    for (size_t i = 0; i < self->count; ++i)
    {
        breadcrumb_t& breadcrumb = self->lst[i];

        if (!breadcrumb.valid)
        {
            invalid++;
        }
        else
        {
            if (0 != ego_mesh_test_wall(PMesh, breadcrumb.pos, breadcrumb.radius, breadcrumb.bits, nullptr))
            {
                breadcrumb.valid = false;
                invalid++;
            }
        }
    }

    // If there were invalid breadcrumbs ...
    if (invalid > 0)
    {
        // ... compact the list.
        self->compact();
    }

    // Sort the values from lowest to highest.
    if (self->count > 1)
    {
        std::sort(self->lst.begin(),self->lst.begin() + self->count,breadcrumb_t::cmp);
    }
}

breadcrumb_t *breadcrumb_list_t::last_valid(breadcrumb_list_t *self)
{
    if (!self || !self->on)
    {
        return nullptr;
    }

    breadcrumb_list_t::validate(self);

    if (!self->empty())
    {
        return &(self->lst[0]);
    }

    return nullptr;
}

breadcrumb_t *breadcrumb_list_t::newest(breadcrumb_list_t *self)
{
    if (!self || !self->on)
    {
        return nullptr;
    }

    breadcrumb_t *pointer = nullptr;
    size_t i;

    // Get the first valid breadcrumb.
    for (i = 0; i < self->count; ++i)
    {
        breadcrumb_t *bc = &(self->lst[i]);
        if (bc->valid)
        {
            pointer = bc;
            break;
        }
    }

    // Not a single valid breadcrumb was found.
    if (!pointer)
    {
        return nullptr;
    }

    // A valid breadcrumb was found. Check if there are newer valid breadcrumbs.
    for (i++; i < self->count; ++i)
    {
        breadcrumb_t *bc = &(self->lst[i]);
        if (bc->valid)
        {
            if (breadcrumb_t::isYounger(*bc, *pointer))
            {
                pointer = bc;
            }
        }
    }

    return pointer;
}

breadcrumb_t *breadcrumb_list_t::oldest(breadcrumb_list_t *self)
{
    if (!self || !self->on)
    {
        return nullptr;
    }

    breadcrumb_t *pointer = nullptr;
    size_t i;

    // Get the first valid breadcrumb.
    for (i = 0; i < self->count; ++i)
    {
        breadcrumb_t *bc = &(self->lst[i]);
        if (bc->valid)
        {
            pointer = bc;
            break;
        }
    }

    // Not a single valid breadcrumb was found.
    if (!pointer)
    {
        return nullptr;
    }

    // A valid breadcrumb was found. Check if there are older valid breadcrumbs.
    for (i++; i < self->count; ++i)
    {
        breadcrumb_t *bc = &(self->lst[i]);
        if (bc->valid)
        {
            if (breadcrumb_t::isOlder(*bc, *pointer))
            {
                pointer = bc;
            }
        }
    }


    return pointer;
}

breadcrumb_t *breadcrumb_list_t::oldest_grid(breadcrumb_list_t *self, Uint32 grid)
{
    if (!self || !self->on)
    {
        return nullptr;
    }

    breadcrumb_t *pointer = nullptr;
    size_t i;

    // Get the first valid breadcrumb for the given grid.
    for (i = 0; i < self->count; ++i)
    {
        breadcrumb_t *bc = &(self->lst[i]);

        if (bc->valid)
        {
            if (bc->grid == grid)
            {
                pointer = bc;
                break;
            }
        }
    }

    // Not a single valid breadcrumb for this grid was found.
    if (!pointer)
    {
        return nullptr;
    }

    // A valid breadcrumb for this grid was found.
    // Check if there are newer, valid breadcrumbs for this grid.
    for (i++; i < self->count; ++i)
    {
        breadcrumb_t *bc = &(self->lst[i]);
        if (bc->valid)
        {
            if (bc->grid == grid)
            {
                if (breadcrumb_t::isYounger(*bc, *pointer))
                {
                    pointer = bc;
                }
            }
        }
    }

    return pointer;
}

breadcrumb_t *breadcrumb_list_t::alloc(breadcrumb_list_t *self)
{
    if (!self)
    {
        return nullptr;
    }

    // If the list is full ...
    if (self->full())
    {
        // ... try to compact it.
        self->compact();
    }

    // If the list is still full after compaction ...
    if (self->full())
    {
        // .. re-use the oldest element.
        return breadcrumb_list_t::oldest(self);
    }
    else
    {
        breadcrumb_t *breadcrumb = &(self->lst[self->count]);
        self->count++;
        breadcrumb->id = breadcrumb_guid++;
        return breadcrumb;
    }
}

bool breadcrumb_list_t::add(breadcrumb_list_t *self, breadcrumb_t *element)
{
    if (!self || !self->on || !element)
    {
        return false;
    }

    // If the list is full ...
    if (self->full())
    {
        // ... compact it.
        self->compact();
    }

    breadcrumb_t *old;
    // If the list is full after compaction ...
    if (self->full())
    {
        // we must reuse a breadcrumb:

        // Try the oldest breadcrumb for the given grid index.
        old = breadcrumb_list_t::oldest_grid(self, element->grid);

        if (!old)
        {
            // No element for the given grid index exists:
            // Try the oldest breadcrumb.
            old = breadcrumb_list_t::oldest(self);
        }
    }
    else
    {
        // The list is not full, so just allocate an element as normal.
        old = breadcrumb_list_t::alloc(self);
    }

    // If a breadcrumb is available ...
    if (old)
    {
        // ... assign the data.
        *old = *element;
        return true;
    }
    return false;
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
phys_data_t *phys_data_clear(phys_data_t *self)
{
    if (!self)
    {
        return nullptr;
    }

    apos_t::self_clear(&(self->aplat));
    apos_t::self_clear(&(self->acoll));
    self->avel = fvec3_t::zero;
    /// @todo Seems like dynamic and loaded data are mixed here;
    /// We may not blank bumpdampen, weight or dampen for now.
#if 0
    self->bumpdampen = 1.0f;
    self->weight = 1.0f;
    self->dampen = 0.5f;
#endif
    return self;
}

phys_data_t *phys_data_ctor(phys_data_t *self)
{
    if (!self)
    {
        return nullptr;
    }
    
    apos_t::ctor(&(self->aplat));
    apos_t::ctor(&(self->acoll));
    self->avel = fvec3_t::zero;
    self->bumpdampen = 1.0f;
    self->weight = 1.0f;
    self->dampen = 0.5f;

    return self;
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
bool apos_t::self_union(apos_t *self, const apos_t *other)
{
    if (!self || !other)
    {
        return false;
    }
    // Scan through the components of the vector and find the maximum displacement.
    for (size_t i = 0; i < 3; ++i)
    {
        self->mins[i] = std::min(self->mins[i], other->mins[i]);
        self->maxs[i] = std::max(self->maxs[i], other->maxs[i]);
        self->sum[i] += other->sum[i];
    }

    return true;
}

bool apos_t::self_union(apos_t *self, const fvec3_t& other)
{
    if (!self)
    {
        return false;
    }
    // Scan through the components of the vector and find the maximum displacement.
    for (size_t i = 0; i < 3; ++i)
    {
        // Find the extrema of the displacement.
        if (other[i] > 0.0f)
        {
            self->maxs[i] = std::max(self->maxs[i], other[i]);
        }
        else if (other[i] < 0.0f)
        {
            self->mins[i] = std::min(self->mins[i], other[i]);
        }

        // Find the sum of the displacement.
        self->sum.v[i] += other[i];
    }

    return true;
}

//--------------------------------------------------------------------------------------------
bool apos_self_union_index( apos_t * lhs, const float val, const int index )
{
    // find the maximum displacement at the given index

    if ( NULL == lhs ) return false;

    if ( index < 0 || index > 2 ) return false;

    LOG_NAN( val );

    // find the extrema of the displacement
    if ( val > 0.0f )
    {
        lhs->maxs.v[index] = std::max( lhs->maxs.v[index], val );
    }
    else if ( val < 0.0f )
    {
        lhs->mins.v[index] = std::min( lhs->mins.v[index], val );
    }

    // find the sum of the displacement
    lhs->sum.v[index] += val;

    return true;
}

//--------------------------------------------------------------------------------------------
bool apos_evaluate(const apos_t *src, fvec3_t& dst)
{
    if ( NULL == src )
    {
		dst = fvec3_t::zero;
		return true;
    }

    for (size_t i = 0; i < 3; ++i)
    {
        dst[i] = src->maxs[i] + src->mins[i];
    }

    return true;
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
phys_data_t *phys_data_sum_aplat(phys_data_t *self, const fvec3_t& v)
{
    if (!self)
    {
        return nullptr;
    }
    apos_t::self_union(&(self->aplat ), v);
    return self;
}

//--------------------------------------------------------------------------------------------
phys_data_t *phys_data_sum_acoll(phys_data_t *self, const fvec3_t& v)
{
    if (!self)
    {
        return nullptr;
    }
    apos_t::self_union(&(self->acoll), v);
    return self;
}

//--------------------------------------------------------------------------------------------
phys_data_t *phys_data_sum_avel(phys_data_t *self, const fvec3_t& v)
{
    if (!self)
    {
        return nullptr;
    }
    self->avel += v;

    return self;
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
phys_data_t * phys_data_sum_aplat_index( phys_data_t * pphys, const float val, const int index )
{
    if (NULL == pphys) return pphys;

    LOG_NAN( val );

    apos_self_union_index( &( pphys->aplat ), val, index );

    return pphys;
}

//--------------------------------------------------------------------------------------------
phys_data_t * phys_data_sum_acoll_index( phys_data_t * pphys, const float val, const int index )
{
    if ( NULL == pphys ) return pphys;

    LOG_NAN( val );

    apos_self_union_index( &( pphys->acoll ), val, index );

    return pphys;
}

//--------------------------------------------------------------------------------------------
phys_data_t * phys_data_sum_avel_index( phys_data_t * pphys, const float val, const int index )
{
    if ( NULL == pphys ) return pphys;

    if ( index < 0 || index > 2 ) return pphys;

    LOG_NAN( val );

    pphys->avel.v[index] += val;

    return pphys;
}

//--------------------------------------------------------------------------------------------
//Inline below
//--------------------------------------------------------------------------------------------
bool test_interaction_close_0( bumper_t bump_a, const fvec3_t& pos_a, bumper_t bump_b, const fvec3_t& pos_b, int test_platform )
{
    oct_bb_t cv_a, cv_b;

    // convert the bumpers to the correct format
    cv_a.assign(bump_a);
    cv_b.assign(bump_b);

    return test_interaction_close_2( &cv_a, pos_a, &cv_b, pos_b, test_platform );
}

//--------------------------------------------------------------------------------------------
bool test_interaction_0( bumper_t bump_a, const fvec3_t& pos_a, bumper_t bump_b, const fvec3_t& pos_b, int test_platform )
{
    oct_bb_t cv_a, cv_b;

    // convert the bumpers to the correct format
    cv_a.assign(bump_a);
    cv_b.assign(bump_b);

    return test_interaction_2( &cv_a, pos_a, &cv_b, pos_b, test_platform );
}

//--------------------------------------------------------------------------------------------
bool test_interaction_close_1( const oct_bb_t * cv_a, const fvec3_t& pos_a, bumper_t bump_b, const fvec3_t& pos_b, int test_platform )
{
    oct_bb_t cv_b;

    // convert the bumper to the correct format
    cv_b.assign(bump_b);

    return test_interaction_close_2( cv_a, pos_a, &cv_b, pos_b, test_platform );
}

//--------------------------------------------------------------------------------------------
bool test_interaction_1( const oct_bb_t * cv_a, const fvec3_t& pos_a, bumper_t bump_b, const fvec3_t& pos_b, int test_platform )
{
    oct_bb_t cv_b;

    // convert the bumper to the correct format
    cv_b.assign(bump_b);

    return test_interaction_2( cv_a, pos_a, &cv_b, pos_b, test_platform );
}

//--------------------------------------------------------------------------------------------
bool test_interaction_close_2( const oct_bb_t * cv_a, const fvec3_t& pos_a, const oct_bb_t * cv_b, const fvec3_t& pos_b, int test_platform )
{
    int cnt;
    float depth;
    oct_vec_v2_t oa, ob;

    if ( NULL == cv_a || NULL == cv_b ) return false;

    // translate the positions to oct_vecs
    oa.ctor( pos_a );
    ob.ctor( pos_b );

    // calculate the depth
    for ( cnt = 0; cnt < OCT_Z; cnt++ )
    {
        float ftmp1 = std::min(( ob[cnt] + cv_b->maxs[cnt] ) - oa[cnt], oa[cnt] - ( ob[cnt] + cv_b->mins[cnt] ) );
        float ftmp2 = std::min(( oa[cnt] + cv_a->maxs[cnt] ) - ob[cnt], ob[cnt] - ( oa[cnt] + cv_a->mins[cnt] ) );
        depth = std::max( ftmp1, ftmp2 );
        if ( depth <= 0.0f ) return false;
    }

    // treat the z coordinate the same as always
    depth = std::min( cv_b->maxs[OCT_Z] + ob[OCT_Z], cv_a->maxs[OCT_Z] + oa[OCT_Z] ) -
            std::max( cv_b->mins[OCT_Z] + ob[OCT_Z], cv_a->mins[OCT_Z] + oa[OCT_Z] );

    return TO_C_BOOL( test_platform ? ( depth > -PLATTOLERANCE ) : ( depth > 0.0f ) );
}

//--------------------------------------------------------------------------------------------
bool test_interaction_2( const oct_bb_t * cv_a, const fvec3_t& pos_a, const oct_bb_t * cv_b, const fvec3_t& pos_b, int test_platform )
{


    int cnt;
    oct_vec_v2_t oa, ob;
    float depth;

    if ( NULL == cv_a || NULL == cv_b ) return false;

    // translate the positions to oct_vecs
    oa.ctor( pos_a );
    ob.ctor( pos_b );

    // calculate the depth
    for ( cnt = 0; cnt < OCT_Z; cnt++ )
    {
        depth  = std::min( cv_b->maxs[cnt] + ob[cnt], cv_a->maxs[cnt] + oa[cnt] ) -
                 std::max( cv_b->mins[cnt] + ob[cnt], cv_a->mins[cnt] + oa[cnt] );

        if ( depth <= 0.0f ) return false;
    }

    // treat the z coordinate the same as always
    depth = std::min( cv_b->maxs[OCT_Z] + ob[OCT_Z], cv_a->maxs[OCT_Z] + oa[OCT_Z] ) -
            std::max( cv_b->mins[OCT_Z] + ob[OCT_Z], cv_a->mins[OCT_Z] + oa[OCT_Z] );

    return TO_C_BOOL(( 0 != test_platform ) ? ( depth > -PLATTOLERANCE ) : ( depth > 0.0f ) );
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
bool get_depth_close_0( bumper_t bump_a, const fvec3_t& pos_a, bumper_t bump_b, const fvec3_t& pos_b, bool break_out, oct_vec_v2_t& depth )
{
    oct_bb_t cv_a, cv_b;

    // convert the bumpers to the correct format
    cv_a.assign(bump_a);
    cv_b.assign(bump_b);

    // shift the bumpers
    cv_a.translate( pos_a );
    cv_b.translate( pos_b );

    return get_depth_close_2( &cv_a, &cv_b, break_out, depth );
}

//--------------------------------------------------------------------------------------------
bool get_depth_0( bumper_t bump_a, const fvec3_t& pos_a, bumper_t bump_b, const fvec3_t& pos_b, bool break_out, oct_vec_v2_t& depth )
{
    oct_bb_t cv_a, cv_b;

    // convert the bumpers to the correct format
    cv_a.assign(bump_a);
    cv_b.assign(bump_b);

    return get_depth_2( &cv_a, pos_a, &cv_b, pos_b, break_out, depth );
}

//--------------------------------------------------------------------------------------------
bool get_depth_close_1( const oct_bb_t * cv_a, bumper_t bump_b, const fvec3_t& pos_b, bool break_out, oct_vec_v2_t& depth )
{
    oct_bb_t cv_b;

    // convert the bumper to the correct format
    cv_b.assign(bump_b);

    // shift the bumper
    cv_b.translate(pos_b);

    return get_depth_close_2( cv_a, &cv_b, break_out, depth );
}

//--------------------------------------------------------------------------------------------
bool get_depth_1( const oct_bb_t * cv_a, const fvec3_t& pos_a, bumper_t bump_b, const fvec3_t& pos_b, bool break_out, oct_vec_v2_t& depth )
{
    oct_bb_t cv_b;

    // convert the bumper to the correct format
    cv_b.assign(bump_b);

    return get_depth_2( cv_a, pos_a, &cv_b, pos_b, break_out, depth );
}

//--------------------------------------------------------------------------------------------
bool get_depth_close_2( const oct_bb_t * cv_a, const oct_bb_t * cv_b, bool break_out, oct_vec_v2_t& depth )
{
    int cnt;
    bool valid;
    float ftmp1, ftmp2;
    float opos_a, opos_b;

    if ( NULL == cv_a || NULL == cv_b ) return false;

    // calculate the depth
    valid = true;
    for ( cnt = 0; cnt < OCT_Z; cnt++ )
    {
        // get positions from the bounding volumes
        opos_a = ( cv_a->mins[cnt] + cv_a->maxs[cnt] ) * 0.5f;
        opos_b = ( cv_b->mins[cnt] + cv_b->maxs[cnt] ) * 0.5f;

        // measue the depth
        ftmp1 = std::min( cv_b->maxs[cnt] - opos_a, opos_a - cv_b->mins[cnt] );
        ftmp2 = std::min( cv_a->maxs[cnt] - opos_b, opos_b - cv_a->mins[cnt] );
        depth[cnt] = std::max( ftmp1, ftmp2 );

        if ( depth[cnt] <= 0.0f )
        {
            valid = false;
            if ( break_out ) return false;
        }
    }

    // treat the z coordinate the same as always
    depth[OCT_Z]  = std::min( cv_b->maxs[OCT_Z], cv_a->maxs[OCT_Z] ) -
                    std::max( cv_b->mins[OCT_Z], cv_a->mins[OCT_Z] );

    if ( depth[OCT_Z] <= 0.0f )
    {
        valid = false;
        if ( break_out ) return false;
    }

    // scale the diagonal components so that they are actually distances
    depth[OCT_XY] *= INV_SQRT_TWO;
    depth[OCT_YX] *= INV_SQRT_TWO;

    return valid;
}

//--------------------------------------------------------------------------------------------
bool get_depth_2( const oct_bb_t * cv_a, const fvec3_t& pos_a, const oct_bb_t * cv_b, const fvec3_t& pos_b, bool break_out, oct_vec_v2_t& depth )
{
    int cnt;
    oct_vec_v2_t oa, ob;
    bool valid;

    if ( NULL == cv_a || NULL == cv_b) return false;

    // translate the positions to oct_vecs
    oa.ctor( pos_a );
    ob.ctor( pos_b );

    // calculate the depth
    valid = true;
    for ( cnt = 0; cnt < OCT_COUNT; cnt++ )
    {
        depth[cnt]  = std::min( cv_b->maxs[cnt] + ob[cnt], cv_a->maxs[cnt] + oa[cnt] ) -
                      std::max( cv_b->mins[cnt] + ob[cnt], cv_a->mins[cnt] + oa[cnt] );

        if ( depth[cnt] <= 0.0f )
        {
            valid = false;
            if ( break_out ) return false;
        }
    }

    // scale the diagonal components so that they are actually distances
    depth[OCT_XY] *= INV_SQRT_TWO;
    depth[OCT_YX] *= INV_SQRT_TWO;

    return valid;
}

//--------------------------------------------------------------------------------------------
#if 0
apos_t * apos_self_clear( apos_t * val )
{
    if ( NULL == val ) return val;

    BLANK_STRUCT_PTR( val )

    return val;
}
#endif

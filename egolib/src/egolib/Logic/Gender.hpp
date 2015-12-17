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
#pragma once

#include "egolib/platform.h"

/**
 * @brief
 *  What gender a character can be spawned with.
 * @todo
 *  MH: This will become an enum class.
 * @todo
 *  MH: The "random" enum element is transitional,
 *  it is always resolved by a dice roll to either
 *  female, male or other. It should not be part
 *  of this enumeration.
 */
enum CharacterGender : uint8_t
{
    /**
     * @brief
     *  "female" gender.
     */
    GENDER_FEMALE = 0,
    /**
     * @brief
     *  "male" gender.
     */
    GENDER_MALE,
    /**
     * @brief
     *  "neutral/other" gender.
     */
    GENDER_OTHER,
    GENDER_RANDOM,
    GENDER_COUNT
};

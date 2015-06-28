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

/// @file game/GUI/MiniMap.hpp
/// @details GUI widget to render that tiny minimap in the corner
/// @author Johan Jansen

#pragma once

#include "game/GUI/GUIComponent.hpp"

class MiniMap : public GUIComponent
{
public:
    MiniMap();

    virtual void draw() override;

    void setShowPlayerPosition(bool show);

    void addBlip(const float x, const float y, const uint8_t color);

private:

    //Private blip class
    class Blip
    {
    public:
        Blip(const float setX, const float setY, const uint8_t setColor) :
            x(setX),
            y(setY),
            color(setColor % COLOR_MAX)
        {
            //ctor
        }

        float x;
        float y;
        uint8_t color;
    };

private:
    uint32_t _markerBlinkTimer;     //< Ticks until next minimap blink is shown
    bool _showPlayerPosition;
    std::vector<Blip> _blips;
};

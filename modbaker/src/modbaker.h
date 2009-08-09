//---------------------------------------------------------------------
// ModBaker - a module editor for Egoboo
//---------------------------------------------------------------------
// Copyright (C) 2009 Tobias Gall
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//---------------------------------------------------------------------

/// @file
/// @brief ModBaker class definition
/// @details Defines the modbaker class (a big container holding everything together)

#ifndef modbaker_h
#define modbaker_h
//---------------------------------------------------------------------
//-   The main class
//---------------------------------------------------------------------
#include "global.h"
#include "core/module.h"
#include "renderer.h"
#include "graphics/ui.h"



//---------------------------------------------------------------------
///   the modbaker class
//---------------------------------------------------------------------
class c_modbaker : public c_ui, public c_module_editor,
	public c_renderer
{
	private:
	protected:
	public:
		c_modbaker();
		~c_modbaker();

		void init(string);
		void main_loop();
		bool get_GL_pos(int, int);
		bool perform_nongui_action(int);
		bool perform_gui_action(int);
		void action(const gcn::ActionEvent &);
		bool load_complete_module(string, string);
		void mousePressed(gcn::MouseEvent &);

		bool render_positions();
		bool render_models();
		bool render_mesh();
		void render_fan(Uint32, bool set_texture);
		void load_basic_textures(string);

		int get_selected_tile_flag();
		void reload_object_window();
};
#endif
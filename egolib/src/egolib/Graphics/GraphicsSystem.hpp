#pragma once

#include "egolib/platform.h"
#include "egolib/Extensions/SDL_extensions.h"
#include "egolib/Extensions/SDL_GL_extensions.h"

namespace Ego {

// Forward declaration.
struct GraphicsWindow;

struct GraphicsSystem {
public:
    static int gfx_width;
    static int gfx_height;
    static SDLX_video_parameters_t sdl_vparam;
    static oglx_video_parameters_t ogl_vparam;
    static bool initialized;
    /**
     * @brief A pointer to the (single) SDL window if it exists, a null pointer otherwise.
     */
    static GraphicsWindow *window;
    /**
     * @brief Initialize the graphics system.
     * @remark This method is a no-op if the graphics system is initialized.
     */
    static void initialize();
    /**
     * @brief Uninitialize the graphics system.
     * @remark This method is a no-op if the graphics system is uninitialized.
     */
    static void uninitialize();
    /**
     * @brief Set the cursor visibility.
     * @param visibility @a true shows the cursor, @a false hides the cursor
     */
    static void setCursorVisibility(bool show);
    /**
     * @brief Get the cursor visibility.
     * @return @a true if the cursor is shown, @a false otherwise
     */
    static bool getCursorVisibility();

};
} // namespace Ego

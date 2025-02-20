/***************************************************************************
                          AnimBG.h  -  description
                             -------------------
    begin                : 2024
    description          : Self-contained GIF89a decoder for animated backgrounds.
                           No external libraries required — LZW and frame
                           compositing are implemented here.
 ***************************************************************************/

#ifndef __ANIMGIF_H
#define __ANIMGIF_H

#include <vector>
#include "SDL.h"

/**
 * A single decoded animation frame.
 */
struct AnimFrame
{
    SDL_Surface* surface;   ///< Converted to display format; caller owns this.
    unsigned int delay_ms;  ///< Frame delay in milliseconds (from GCE).
};

/**
 * Load an animated GIF from disk into a list of SDL_Surface frames.
 *
 * Each frame has already been converted to the current display format via
 * SDL_DisplayFormat / SDL_DisplayFormatAlpha so blitting is fast.
 *
 * Returns an empty vector on failure (file not found, not a GIF, etc.).
 * The caller is responsible for calling SDL_FreeSurface() on each frame and
 * clearing the vector when done.
 */
std::vector<AnimFrame> LoadAnimatedGIF( const char* a_pcFilepath );

#endif // __ANIMGIF_H

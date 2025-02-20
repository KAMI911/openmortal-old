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
 * Load an animated GIF (GIF87a / GIF89a) from disk.
 *
 * Self-contained decoder — no giflib required.
 * Handles transparency, all disposal methods, interlacing, local colour tables.
 *
 * Returns an empty vector on failure.
 */
std::vector<AnimFrame> LoadAnimatedGIF( const char* a_pcFilepath );

/**
 * Load a frame-sequence animation from a plain-text .anim descriptor file.
 *
 * File format (one frame per line, '#' lines are comments):
 *   <filename>  <delay_ms>
 *
 * Filenames are relative to DATADIR/gfx/.  Any format supported by
 * SDL_image (PNG, JPG, BMP …) can be used — no 256-colour limit.
 *
 * Returns an empty vector on failure.
 */
std::vector<AnimFrame> LoadFrameSequence( const char* a_pcFilepath );

#endif // __ANIMGIF_H

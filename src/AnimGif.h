#ifndef ANIMGIF_H
#define ANIMGIF_H

#include <vector>
struct SDL_Surface;

/**
 * Load all frames of an animated GIF as coalesced SDL_Surfaces.
 * Returns an empty vector on failure or for non-GIF files.
 * Each returned surface is in display format; caller owns them.
 */
std::vector<SDL_Surface*> LoadAnimatedGIF( const char* a_pcPath );

#endif // ANIMGIF_H

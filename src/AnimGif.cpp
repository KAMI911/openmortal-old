/***************************************************************************
                         AnimGif.cpp  -  Animated GIF loader
                            -------------------
   Loads all frames of an animated GIF into a vector of SDL_Surfaces,
   correctly handling per-frame offsets, transparency, and GIF disposal
   methods (do-not-dispose, restore-to-background, restore-to-previous).
 ***************************************************************************/

#include "AnimGif.h"
#include "common.h"

#include "SDL.h"
#include <gif_lib.h>
#include <string.h>

extern SDL_Surface* gamescreen;


std::vector<SDL_Surface*> LoadAnimatedGIF( const char* a_pcPath )
{
    std::vector<SDL_Surface*> aoFrames;

    int iError = 0;
    GifFileType* gif = DGifOpenFileName( a_pcPath, &iError );
    if ( !gif )
    {
        debug( "LoadAnimatedGIF: cannot open '%s' (error %d)\n", a_pcPath, iError );
        return aoFrames;
    }

    if ( DGifSlurp( gif ) != GIF_OK )
    {
        debug( "LoadAnimatedGIF: DGifSlurp failed for '%s'\n", a_pcPath );
        DGifCloseFile( gif, &iError );
        return aoFrames;
    }

    int iW = gif->SWidth;
    int iH = gif->SHeight;

    if ( iW <= 0 || iH <= 0 || gif->ImageCount <= 0 )
    {
        DGifCloseFile( gif, &iError );
        return aoFrames;
    }

    // Working canvas: 32-bit BGRA for easy per-pixel compositing.
    SDL_Surface* poCanvas = SDL_CreateRGBSurface( SDL_SWSURFACE, iW, iH, 32,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000 );
    if ( !poCanvas )
    {
        DGifCloseFile( gif, &iError );
        return aoFrames;
    }

    // A saved copy of the canvas for DISPOSE_PREVIOUS.
    SDL_Surface* poPrevCanvas = SDL_CreateRGBSurface( SDL_SWSURFACE, iW, iH, 32,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000 );
    if ( !poPrevCanvas )
    {
        SDL_FreeSurface( poCanvas );
        DGifCloseFile( gif, &iError );
        return aoFrames;
    }

    // Start with transparent canvas.
    SDL_FillRect( poCanvas, NULL, SDL_MapRGBA( poCanvas->format, 0, 0, 0, 0 ) );

    int iPrevDisposal = DISPOSAL_UNSPECIFIED;
    SDL_Rect oPrevRect = { 0, 0, 0, 0 };

    for ( int i = 0; i < gif->ImageCount; ++i )
    {
        SavedImage& roFrame = gif->SavedImages[i];

        // Read Graphics Control Block (may not exist for this frame).
        GraphicsControlBlock gcb;
        gcb.DisposalMode     = DISPOSAL_UNSPECIFIED;
        gcb.UserInputFlag    = false;
        gcb.DelayTime        = 0;
        gcb.TransparentColor = NO_TRANSPARENT_COLOR;
        DGifSavedExtensionToGCB( gif, i, &gcb );

        SDL_Rect oFR;
        oFR.x = (Sint16)roFrame.ImageDesc.Left;
        oFR.y = (Sint16)roFrame.ImageDesc.Top;
        oFR.w = (Uint16)roFrame.ImageDesc.Width;
        oFR.h = (Uint16)roFrame.ImageDesc.Height;

        // --- Apply previous frame's disposal before drawing this frame ---
        if ( iPrevDisposal == DISPOSE_BACKGROUND )
        {
            // Clear the previous frame's area to transparent.
            SDL_FillRect( poCanvas, &oPrevRect,
                SDL_MapRGBA( poCanvas->format, 0, 0, 0, 0 ) );
        }
        else if ( iPrevDisposal == DISPOSE_PREVIOUS )
        {
            // Restore canvas to the snapshot taken before last frame.
            SDL_BlitSurface( poPrevCanvas, &oPrevRect, poCanvas, &oPrevRect );
        }

        // Save current canvas state (needed if this frame uses DISPOSE_PREVIOUS).
        SDL_BlitSurface( poCanvas, NULL, poPrevCanvas, NULL );

        // --- Draw current frame pixels onto canvas ---
        ColorMapObject* pcmap = roFrame.ImageDesc.ColorMap
                              ? roFrame.ImageDesc.ColorMap
                              : gif->SColorMap;
        if ( !pcmap )
        {
            debug( "LoadAnimatedGIF: frame %d has no color map, skipping\n", i );
            continue;
        }

        SDL_LockSurface( poCanvas );
        Uint32* pixels  = (Uint32*)poCanvas->pixels;
        int     iStride = poCanvas->pitch / 4;

        for ( int fy = 0; fy < oFR.h; ++fy )
        {
            for ( int fx = 0; fx < oFR.w; ++fx )
            {
                int cx = oFR.x + fx;
                int cy = oFR.y + fy;
                if ( cx < 0 || cx >= iW || cy < 0 || cy >= iH ) continue;

                GifByteType idx = roFrame.RasterBits[ fy * oFR.w + fx ];

                // Skip transparent pixels.
                if ( gcb.TransparentColor != NO_TRANSPARENT_COLOR
                     && (int)idx == gcb.TransparentColor )
                    continue;

                if ( idx >= pcmap->ColorCount ) continue;
                GifColorType& c = pcmap->Colors[idx];
                pixels[ cy * iStride + cx ] =
                    SDL_MapRGBA( poCanvas->format, c.Red, c.Green, c.Blue, 0xFF );
            }
        }
        SDL_UnlockSurface( poCanvas );

        // Convert canvas snapshot to display format and store as a frame.
        // Use DisplayFormatAlpha to preserve per-pixel transparency.
        SDL_Surface* poFrameSurface = SDL_DisplayFormatAlpha( poCanvas );
        if ( poFrameSurface )
            aoFrames.push_back( poFrameSurface );

        iPrevDisposal = gcb.DisposalMode;
        oPrevRect = oFR;
    }

    SDL_FreeSurface( poCanvas );
    SDL_FreeSurface( poPrevCanvas );
    DGifCloseFile( gif, &iError );

    debug( "LoadAnimatedGIF: loaded %d frames from '%s'\n",
           (int)aoFrames.size(), a_pcPath );
    return aoFrames;
}

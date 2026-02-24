/***************************************************************************
                          AnimBG.cpp  -  description
                             -------------------
    begin                : 2024
    description          : Self-contained GIF89a / GIF87a decoder.
                           Implements:
                             - LZW decompressor
                             - Global / local colour table parsing
                             - Graphic Control Extension (transparency, delay,
                               disposal methods 0-3)
                             - Interlace de-interlacer
                             - Frame compositor producing SDL_Surface frames
                           No external libraries (no giflib, no libpng etc.)
 ***************************************************************************/

#include "AnimBG.h"
#include "SDL.h"
#include "SDL_image.h"
#include "common.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// ---------------------------------------------------------------------------
// Internal types / constants
// ---------------------------------------------------------------------------

static const int GIF_MAX_COLORS = 256;
static const int LZW_MAX_CODES  = 4096;

struct GifColor { Uint8 r, g, b; };

// Graphic Control Extension parsed state
struct GCE
{
    bool        hasTransparency;
    Uint8       transparentIndex;
    unsigned int delayMs;          // centiseconds * 10
    int         disposalMethod;   // 0-3
};

// Per-image-descriptor state
struct ImageDesc
{
    int left, top, width, height;
    bool interlaced;
    bool hasLocalCT;
    int  localCTSize;
    GifColor localCT[GIF_MAX_COLORS];
};

// ---------------------------------------------------------------------------
// Bit-stream reader (LSB-first, as GIF LZW requires)
// ---------------------------------------------------------------------------

struct BitStream
{
    const Uint8* data;
    size_t       size;
    size_t       bytePos;
    int          bitBuf;
    int          bitsLeft;

    void init( const Uint8* d, size_t s )
    {
        data = d; size = s; bytePos = 0; bitBuf = 0; bitsLeft = 0;
    }

    // Read up to 16 bits (LSB first).
    int readBits( int n )
    {
        while ( bitsLeft < n )
        {
            if ( bytePos >= size ) return -1;
            bitBuf |= ( (int)data[bytePos++] ) << bitsLeft;
            bitsLeft += 8;
        }
        int val = bitBuf & ( (1 << n) - 1 );
        bitBuf  >>= n;
        bitsLeft  -= n;
        return val;
    }
};

// ---------------------------------------------------------------------------
// LZW Decompressor
// ---------------------------------------------------------------------------

struct LZWEntry
{
    int  prefix; // index into table, or -1 for root
    Uint8 suffix;
};

// Expand one GIF LZW sub-image block list into raw pixel indices.
// data[] is the concatenation of all sub-blocks (without their length bytes).
// Returns false on error.
static bool lzwDecompress(
    const Uint8* data, size_t dataLen,
    int lzwMinCodeSize,
    std::vector<Uint8>& out )
{
    if ( lzwMinCodeSize < 2 || lzwMinCodeSize > 8 ) return false;

    int clearCode = 1 << lzwMinCodeSize;
    int eofCode   = clearCode + 1;
    int nextCode  = eofCode + 1;
    int codeSize  = lzwMinCodeSize + 1;

    LZWEntry table[LZW_MAX_CODES];
    // Root entries: single pixel for each colour index
    for ( int i = 0; i < clearCode; ++i )
    {
        table[i].prefix = -1;
        table[i].suffix = (Uint8)i;
    }

    BitStream bs;
    bs.init( data, dataLen );

    int code, prev = -1;
    std::vector<Uint8> stack;
    stack.reserve( 4096 );

    while ( true )
    {
        code = bs.readBits( codeSize );
        if ( code < 0 ) break;            // truncated data — treat as EOF

        if ( code == clearCode )
        {
            nextCode = eofCode + 1;
            codeSize = lzwMinCodeSize + 1;
            prev = -1;
            continue;
        }
        if ( code == eofCode ) break;

        // Decode the code into the stack (reversed)
        int entry = code;
        if ( entry >= nextCode )
        {
            // Special case: code == nextCode
            if ( prev < 0 ) return false;
            stack.push_back( table[prev].suffix );
            entry = prev;
        }

        while ( entry >= 0 && entry < LZW_MAX_CODES )
        {
            stack.push_back( table[entry].suffix );
            entry = table[entry].prefix;
        }

        // Flush stack (reversed) to output
        for ( int i = (int)stack.size()-1; i >= 0; --i )
            out.push_back( stack[i] );

        // Add new table entry
        if ( prev >= 0 && nextCode < LZW_MAX_CODES )
        {
            // The new entry's suffix is the first pixel of current code
            table[nextCode].prefix = prev;
            table[nextCode].suffix = stack.back();
            ++nextCode;

            // Grow code size if needed
            if ( nextCode > (1 << codeSize) && codeSize < 12 )
            {
                ++codeSize;
            }
        }

        prev = code;
        stack.clear();
    }
    return true;
}

// ---------------------------------------------------------------------------
// De-interlacer
// ---------------------------------------------------------------------------

static void deinterlace( std::vector<Uint8>& pixels, int w, int h )
{
    std::vector<Uint8> tmp( pixels.size() );
    // GIF interlace: pass 0 rows 0,8,16…; pass 1 rows 4,12…;
    //                pass 2 rows 2,6…;   pass 3 rows 1,3,5…
    static const int start[4]  = { 0, 4, 2, 1 };
    static const int step[4]   = { 8, 8, 4, 2 };
    size_t src = 0;
    for ( int pass = 0; pass < 4; ++pass )
    {
        for ( int y = start[pass]; y < h; y += step[pass] )
        {
            memcpy( &tmp[y * w], &pixels[src], w );
            src += w;
        }
    }
    pixels.swap( tmp );
}

// ---------------------------------------------------------------------------
// Build an SDL_Surface from decoded palette pixels
// ---------------------------------------------------------------------------

static SDL_Surface* buildSurface(
    const std::vector<Uint8>& pixels,
    int w, int h,
    const GifColor* ct,
    const GCE& gce )
{
    // Create a 32-bit RGBA surface so transparency works cleanly
    SDL_Surface* surf = SDL_CreateRGBSurface(
        SDL_SWSURFACE, w, h, 32,
        0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0xFF000000u );
    if ( !surf ) return NULL;

    SDL_LockSurface( surf );

    Uint32* dst = (Uint32*)surf->pixels;
    for ( int y = 0; y < h; ++y )
    {
        Uint32* row = dst + y * ( surf->pitch / 4 );
        for ( int x = 0; x < w; ++x )
        {
            Uint8 idx = pixels[ y * w + x ];
            Uint8 alpha = 0xFF;
            if ( gce.hasTransparency && idx == gce.transparentIndex )
                alpha = 0x00;
            const GifColor& c = ct[idx];
            row[x] = ( (Uint32)alpha << 24 )
                   | ( (Uint32)c.r  << 16 )
                   | ( (Uint32)c.g  <<  8 )
                   |   (Uint32)c.b;
        }
    }

    SDL_UnlockSurface( surf );
    return surf;
}

// ---------------------------------------------------------------------------
// Helper: read the entire contents of a file into a vector<Uint8>
// ---------------------------------------------------------------------------

static bool readFile( const char* path, std::vector<Uint8>& buf )
{
    FILE* f = fopen( path, "rb" );
    if ( !f ) return false;
    fseek( f, 0, SEEK_END );
    long sz = ftell( f );
    fseek( f, 0, SEEK_SET );
    if ( sz <= 0 ) { fclose(f); return false; }
    buf.resize( (size_t)sz );
    size_t rd = fread( &buf[0], 1, (size_t)sz, f );
    fclose( f );
    return rd == (size_t)sz;
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------

std::vector<AnimFrame> LoadAnimatedGIF( const char* a_pcFilepath )
{
    std::vector<AnimFrame> frames;

    // --- 1. Load file ---
    std::vector<Uint8> raw;
    if ( !readFile( a_pcFilepath, raw ) )
    {
        debug( "AnimBG: cannot open %s\n", a_pcFilepath );
        return frames;
    }

    const Uint8* p   = &raw[0];
    const Uint8* end = p + raw.size();

#define NEED(n)   if ( p + (n) > end ) { debug("AnimBG: truncated GIF\n"); return frames; }
#define READ1()   (*p++)
#define READ2()   ( p+=2, (int)(*(p-2)) | ((int)(*(p-1)) << 8) )

    // --- 2. Header ---
    NEED(6);
    if ( memcmp( p, "GIF87a", 6 ) != 0 && memcmp( p, "GIF89a", 6 ) != 0 )
    {
        debug( "AnimBG: not a GIF file: %s\n", a_pcFilepath );
        return frames;
    }
    p += 6;

    // --- 3. Logical Screen Descriptor ---
    NEED(7);
    int screenW = READ2();
    int screenH = READ2();
    Uint8 packed = READ1();
    p++; /* background color index */
    p++; /* pixel aspect ratio     */

    bool hasGlobalCT    = ( packed & 0x80 ) != 0;
    int  globalCTSize   = hasGlobalCT ? ( 1 << ( (packed & 0x07) + 1 ) ) : 0;

    GifColor globalCT[GIF_MAX_COLORS];
    memset( globalCT, 0, sizeof(globalCT) );

    if ( hasGlobalCT )
    {
        NEED( globalCTSize * 3 );
        for ( int i = 0; i < globalCTSize; ++i )
        {
            globalCT[i].r = READ1();
            globalCT[i].g = READ1();
            globalCT[i].b = READ1();
        }
    }

    // Canvas to composite onto (handles disposal methods correctly)
    // We keep a 32-bit RGBA canvas.
    SDL_Surface* canvas = SDL_CreateRGBSurface(
        SDL_SWSURFACE, screenW, screenH, 32,
        0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0xFF000000u );
    if ( !canvas )
    {
        debug( "AnimBG: cannot allocate canvas\n" );
        return frames;
    }
    SDL_FillRect( canvas, NULL, 0x00000000u ); // transparent black

    // "previous" canvas snapshot (for disposal method 3)
    SDL_Surface* prevCanvas = SDL_CreateRGBSurface(
        SDL_SWSURFACE, screenW, screenH, 32,
        0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0xFF000000u );
    if ( !prevCanvas ) { SDL_FreeSurface(canvas); return frames; }
    SDL_FillRect( prevCanvas, NULL, 0x00000000u );

    // Current GCE state (reset between frames)
    GCE gce;
    memset( &gce, 0, sizeof(gce) );

    // --- 4. Block loop ---
    while ( p < end )
    {
        NEED(1);
        Uint8 introducer = READ1();

        // --- 4a. Image Descriptor (0x2C = ',') ---
        if ( introducer == 0x2C )
        {
            NEED(9);
            ImageDesc id;
            id.left   = READ2();
            id.top    = READ2();
            id.width  = READ2();
            id.height = READ2();
            Uint8 ipacked  = READ1();
            id.interlaced  = ( ipacked & 0x40 ) != 0;
            id.hasLocalCT  = ( ipacked & 0x80 ) != 0;
            id.localCTSize = id.hasLocalCT ? ( 1 << ( (ipacked & 0x07) + 1 ) ) : 0;

            if ( id.hasLocalCT )
            {
                NEED( id.localCTSize * 3 );
                for ( int i = 0; i < id.localCTSize; ++i )
                {
                    id.localCT[i].r = READ1();
                    id.localCT[i].g = READ1();
                    id.localCT[i].b = READ1();
                }
            }

            // LZW minimum code size
            NEED(1);
            int lzwMin = READ1();

            // Read sub-blocks into a flat buffer
            std::vector<Uint8> lzwData;
            lzwData.reserve( id.width * id.height );
            while ( p < end )
            {
                NEED(1);
                Uint8 blockLen = READ1();
                if ( blockLen == 0 ) break;
                NEED( blockLen );
                size_t offset = lzwData.size();
                lzwData.resize( offset + blockLen );
                memcpy( &lzwData[offset], p, blockLen );
                p += blockLen;
            }

            // Decompress LZW
            std::vector<Uint8> pixels;
            pixels.reserve( id.width * id.height );
            if ( !lzwDecompress( &lzwData[0], lzwData.size(), lzwMin, pixels ) )
            {
                debug( "AnimBG: LZW decompression failed\n" );
                // skip this frame
                memset( &gce, 0, sizeof(gce) );
                continue;
            }

            // Clamp pixel count
            if ( (int)pixels.size() > id.width * id.height )
                pixels.resize( id.width * id.height );
            while ( (int)pixels.size() < id.width * id.height )
                pixels.push_back( 0 );

            // De-interlace if needed
            if ( id.interlaced )
                deinterlace( pixels, id.width, id.height );

            // Choose colour table
            const GifColor* ct = id.hasLocalCT ? id.localCT : globalCT;

            // Disposal 3: snapshot canvas BEFORE we draw this frame
            if ( gce.disposalMethod == 3 )
            {
                SDL_BlitSurface( canvas, NULL, prevCanvas, NULL );
            }

            // Build a surface for just this sub-image
            SDL_Surface* frameSurf = buildSurface( pixels, id.width, id.height, ct, gce );

            // Composite onto canvas
            if ( frameSurf )
            {
                SDL_Rect dst;
                dst.x = (Sint16)id.left;
                dst.y = (Sint16)id.top;
                dst.w = (Uint16)id.width;
                dst.h = (Uint16)id.height;

                // Enable alpha blending for compositing
                SDL_SetAlpha( frameSurf, SDL_SRCALPHA, SDL_ALPHA_OPAQUE );
                SDL_BlitSurface( frameSurf, NULL, canvas, &dst );
                SDL_FreeSurface( frameSurf );
            }

            // Snapshot the composited canvas as a display-format surface
            SDL_Surface* displayed = SDL_DisplayFormatAlpha( canvas );
            if ( displayed )
            {
                AnimFrame af;
                af.surface  = displayed;
                af.delay_ms = gce.delayMs ? gce.delayMs : 100; // default 100ms
                frames.push_back( af );
            }

            // Apply disposal method for next frame
            switch ( gce.disposalMethod )
            {
            case 2:
                // Restore to background: clear the sub-image region
                {
                    SDL_Rect region;
                    region.x = (Sint16)id.left;
                    region.y = (Sint16)id.top;
                    region.w = (Uint16)id.width;
                    region.h = (Uint16)id.height;
                    SDL_FillRect( canvas, &region, 0x00000000u );
                }
                break;
            case 3:
                // Restore to previous
                SDL_BlitSurface( prevCanvas, NULL, canvas, NULL );
                break;
            default:
                // 0 or 1: leave canvas as-is
                break;
            }

            // Reset GCE for next frame
            memset( &gce, 0, sizeof(gce) );
        }

        // --- 4b. Extension block (0x21 = '!') ---
        else if ( introducer == 0x21 )
        {
            NEED(1);
            Uint8 label = READ1();

            if ( label == 0xF9 ) // Graphic Control Extension
            {
                NEED(1);
                Uint8 blockLen = READ1();
                if ( blockLen >= 4 )
                {
                    NEED(4);
                    Uint8 gPacked    = READ1();
                    int   delayCS    = READ2(); // centiseconds
                    Uint8 transIdx   = READ1();
                    gce.disposalMethod    = ( gPacked >> 2 ) & 0x07;
                    gce.hasTransparency   = ( gPacked & 0x01 ) != 0;
                    gce.transparentIndex  = transIdx;
                    gce.delayMs           = (unsigned int)delayCS * 10;
                    blockLen -= 4;
                }
                // Skip remaining bytes in this GCE block
                while ( blockLen-- && p < end ) { (void)READ1(); }
            }

            // Skip sub-blocks for this extension (any label)
            while ( p < end )
            {
                NEED(1);
                Uint8 blockLen = READ1();
                if ( blockLen == 0 ) break;
                NEED( blockLen );
                p += blockLen;
            }
        }

        // --- 4c. Trailer (0x3B = ';') ---
        else if ( introducer == 0x3B )
        {
            break;
        }

        // --- 4d. Unknown block: skip ---
        else
        {
            // Try to skip sub-blocks
            while ( p < end )
            {
                NEED(1);
                Uint8 blockLen = READ1();
                if ( blockLen == 0 ) break;
                if ( p + blockLen > end ) break;
                p += blockLen;
            }
        }
    }

#undef NEED
#undef READ1
#undef READ2

    SDL_FreeSurface( canvas );
    SDL_FreeSurface( prevCanvas );

    if ( frames.empty() )
        debug( "AnimBG: no frames decoded from %s\n", a_pcFilepath );
    else
        debug( "AnimBG: loaded %d frames from %s\n", (int)frames.size(), a_pcFilepath );

    return frames;
}

// ---------------------------------------------------------------------------
// Frame-sequence loader (.anim descriptor)
// ---------------------------------------------------------------------------

std::vector<AnimFrame> LoadFrameSequence( const char* a_pcFilepath )
{
    std::vector<AnimFrame> frames;

    FILE* f = fopen( a_pcFilepath, "r" );
    if ( !f )
    {
        debug( "AnimBG: cannot open sequence file: %s\n", a_pcFilepath );
        return frames;
    }

    char line[FILENAME_MAX + 32];
    while ( fgets( line, (int)sizeof(line), f ) )
    {
        // Skip comments and blank lines
        char* s = line;
        while ( *s == ' ' || *s == '\t' ) ++s;
        if ( *s == '#' || *s == '\n' || *s == '\r' || *s == '\0' ) continue;

        char fname[FILENAME_MAX + 1];
        fname[0] = '\0';
        unsigned int delay_ms = 100;
        sscanf( s, "%s %u", fname, &delay_ms );
        if ( !fname[0] ) continue;

        // Build full path: DATADIR/gfx/<fname>
        char path[FILENAME_MAX + 1];
        snprintf( path, sizeof(path), "%s/gfx/%s", DATADIR, fname );

        SDL_Surface* raw = IMG_Load( path );
        if ( !raw )
        {
            debug( "AnimBG: cannot load frame: %s\n", path );
            continue;
        }

        // Convert to display format with alpha so transparency works
        SDL_Surface* conv = SDL_DisplayFormatAlpha( raw );
        SDL_FreeSurface( raw );
        if ( !conv ) continue;

        AnimFrame af;
        af.surface  = conv;
        af.delay_ms = delay_ms ? delay_ms : 100;
        frames.push_back( af );
    }

    fclose( f );

    if ( frames.empty() )
        debug( "AnimBG: no frames in sequence %s\n", a_pcFilepath );
    else
        debug( "AnimBG: loaded %d frames from sequence %s\n", (int)frames.size(), a_pcFilepath );

    return frames;
}

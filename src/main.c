#define LICENSE_NOTICE "\
gifPlayer: A terminal-based video player for GIF files.                 \n\
Copyright (C) 2022 Weiju Wang.                                          \n\
                                                                        \n\
This program is free software: you can redistribute it and/or modify    \n\
it under the terms of the GNU General Public License as published by    \n\
the Free Software Foundation, either version 3 of the License, or       \n\
(at your option) any later version.                                     \n\
                                                                        \n\
This program is distributed in the hope that it will be useful,         \n\
but WITHOUT ANY WARRANTY; without even the implied warranty of          \n\
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           \n\
GNU General Public License for more details.                            \n\
                                                                        \n\
You should have received a copy of the GNU General Public License       \n\
along with this program.  If not, see <https://www.gnu.org/licenses/>."

/*
References:
- https://en.wikipedia.org/wiki/GIF
- https://www.w3.org/Graphics/GIF/spec-gif89a.txt
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <ncurses.h>

// Messages displayed to the user
#define MSG_USAGE "Usage: %s [-hlb] [file]\n" \
    "-h: Display this help message.\n" \
    "-l: Display the license notice.\n" \
    "-b: Play in black-and-white.\n"
#define MSG_NOFILE "Nothing to play."
#define MSG_COULD_NOT_OPEN "Could not open file" // Do not add punctuation; `perror` adds a colon
#define MSG_COULD_NOT_READ "Could not read file."
#define MSG_COULD_NOT_CLOSE "Could not close file."
#define MSG_INVALID_GIF "Could not play file (invalid data at position %lx).\n" // Must have \n
#define MSG_NO_COLORS "This terminal does not support colors."

// Format: position (of the rightmost bit), size
// Big-endian positions; higher positions = higher place values
#define FLAG_CT 7
#define FLAG_BIT_DEPTH 4
#define FLAG_GCT_SORTED 3
#define FLAG_CT_SIZE 0
#define FLAG_INTERLACE 6
#define FLAG_LCT_SORTED 5
#define FLAG_DISPOSAL_METHOD 2
#define FLAG_USER_INPUT 1
#define FLAG_TRANSPARENT 0

// Fixed values
#define GCE_BLOCK_SIZE 4
#define APP_EXT_BLOCK_SIZE 11
#define TXT_EXT_BLOCK_SIZE 12
#define BLOCK_TERMINATOR 0

#define BYTE(n) (fileContents[n])
#define FREE_IF_ALLOCATED(p) \
    if((p) != NULL) \
        free(p);

////////////////////////////////////////////////////////////////////////////////

struct buffer
{
    uint8_t* data;
    size_t size;
};

////////////////////////////////////////////////////////////////////////////////

const char asciiLuminance[] = " .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";

struct
{
    uint_fast16_t width;
    uint_fast16_t height;
    uint_fast8_t bkgdColorIndex;
    uint_fast8_t pixelAspectRatio;
    size_t bitDepth;
    bool isSorted;
} lsd;

struct
{
    uint_fast16_t left;
    uint_fast16_t top;
    uint_fast16_t width;
    uint_fast16_t height;
    bool isSorted;
    bool interlaced;
    size_t lzwMinCodeSz;
} img;

struct
{
    enum
    {
        noAction = 0,
        doNotDispose = 1,
        restoreBkgd = 2,
        restorePrevious = 3
    } disposalMethod;

    bool expectingUserInput;
    bool hasTransparencyIndex;
    uint_fast16_t delayTime;
    uint_fast8_t transparentColorIndex;
} gce;

struct buffer gct, lct;
uint_fast8_t flags;
uint_fast8_t cmtBlockSize;

struct buffer file;
FILE* filePtr = NULL;
size_t currPos = 0;

bool ncursesStarted = false, playColor = true;

////////////////////////////////////////////////////////////////////////////////

int getFlag(const size_t pos, const size_t numBits)
{
    return (flags >> pos) % (1U << numBits);
}

void invalidGif(void)
{
    printf(MSG_INVALID_GIF, currPos);
    exit(EXIT_FAILURE);
}

void expect(const uint_fast8_t val)
{
    if(file.data[currPos] != val)
    {
        invalidGif();
    }
    else ++currPos;
}

uint_fast8_t get8(void)
{
    uint_fast8_t temp = file.data[currPos];
    ++currPos;
    return temp;
}

uint_fast16_t get16(void)
{
    uint_fast16_t temp = *(uint16_t*)(file.data + currPos);
    currPos += 2;
    return temp;
}

void teardown(void)
{
    FREE_IF_ALLOCATED(file.data);
    FREE_IF_ALLOCATED(gct.data);
    FREE_IF_ALLOCATED(lct.data);

    if(ncursesStarted)
        endwin(); // done with ncurses, back to normal terminal
}

uint8_t* compileColorTableIfExists(uint8_t* const previousColorTable)
{
    uint8_t* colorTable;

    // Compile the color table if it exists
    if(getFlag(FLAG_CT, 1))
    {
        // Size of color table
        size_t ctSize = (1U << (getFlag(FLAG_CT_SIZE, 3) + 1));
        uint_fast8_t r, g, b;

        // The color table is likely going to be resized; allocate new memory for it
        colorTable = realloc(previousColorTable, ctSize * sizeof(uint8_t));

        // For each color in the color table...
        for(int i = 0; i < ctSize; ++i)
        {
            // Retrieve its RGB values
            r = file.data[currPos + 3*i];
            g = file.data[currPos + 3*i + 1];
            b = file.data[currPos + 3*i + 2];

            // Colored mode
            if(playColor)
            {
                // TODO compileColorTableIfExists(), colored mode
            }
            // Black-and-white mode
            else
            {
                // Compute the color's luminance and find the matching ASCII luminance character
                float colorPos = (sizeof(asciiLuminance) - 1) * (r * 0.299 + g * 0.587 + b * 0.114) / 256;
                colorTable[i] = asciiLuminance[(int)colorPos];
            }
        }

        return colorTable;
    }
    else return NULL;
}

////////////////////////////////////////////////////////////////////////////////

int main(const int argc, char** argv)
{
    atexit(teardown);

    ////////////////////////////////////////////////////////////////////////////
    /* Parse command-line options.
    */

    int currFlag;

    // For each flag passed...
    while((currFlag = getopt(argc, argv, "hlb")) != -1)
    {
        switch(currFlag)
        {
            // Black-and-white mode
            case 'b':
                playColor = false;
                break;

            // Display license
            case 'l':
                puts(LICENSE_NOTICE);
                exit(EXIT_FAILURE);

            // Display help
            case 'h':
            default:
                printf(MSG_USAGE, argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    /*
    Did the user NOT provide a file to play?
    `optind` is the index of the first argument that isn't a flag (in this case, the name of the file we want to play).
    If the index doesn't even make sense, then we must not have any non-flags.
    */
    if(optind >= argc)
    {
        puts(MSG_NOFILE);
        exit(EXIT_FAILURE);
    }

    ////////////////////////////////////////////////////////////////////////////
    /* Read GIF file into memory
    The file extension is not enforced; there is no point in doing so.
    Thus, a file containing contents that would be a valid GIF file will still
    be played.
    */

    filePtr = fopen(argv[optind], "rb");

    // Could the file NOT be opened?
    if(filePtr == NULL)
    {
        perror(MSG_COULD_NOT_OPEN);
        exit(EXIT_FAILURE);
    }

    // Find file length
    fseek(filePtr, 0L, SEEK_END); // Go one byte past the end of the file
    file.size = ftell(filePtr); // How many bytes from us to the beginning? (i.e. how long is the file)
    rewind(filePtr); // Head back to the beginning

    // Read the file's contents into memory
    file.data = malloc(file.size * sizeof(uint8_t));
    fread(file.data, sizeof(uint8_t), file.size, filePtr);

    // Could we NOT read the file?
    if(file.data == NULL)
    {
        puts(MSG_COULD_NOT_READ);
        exit(EXIT_FAILURE);
    }

    // Try closing the file, since its contents are now in memory. If it doesn't close...
    if(fclose(filePtr) == EOF)
    {
        puts(MSG_COULD_NOT_CLOSE);
        exit(EXIT_FAILURE);
    }

    // The file is now read into memory and closed.

    ////////////////////////////////////////////////////////////////////////////
    /* Init ncurses
    FROM THIS POINT ON, DO NOT USE STANDARD TERMINAL IO.
    */

    // Start the screen
    initscr();
    ncursesStarted = true;

    // If we're playing in color, the terminal must be able to support colors
    if(playColor)
    {
        if(!has_colors())
        {
            puts(MSG_NO_COLORS);
            exit(EXIT_FAILURE);
        }

        start_color();
    }

    noecho(); // Do not echo user input back to the screen
    clear();

    ////////////////////////////////////////////////////////////////////////////
    /* Validate and parse the file
    */

    // Header - "GIF"
    expect('G');
    expect('I');
    expect('F');

    // Version - "89a"
    expect('8');
    expect('9');

    // Logical Screen Descriptor
    lsd.width = get16();
    lsd.height = get16();

    flags = get8();
    lsd.bitDepth = getFlag(FLAG_BIT_DEPTH, 3) + 1;
    lsd.isSorted = getFlag(FLAG_GCT_SORTED, 1);

    lsd.bkgdColorIndex = get8();
    lsd.pixelAspectRatio = (get8() + 15) / 64;

    gct.data = compileColorTableIfExists(gct.data);

    while(true)
    {
        switch(get8())
        {
            // Image separator (0x2c)
            case ',':
                img.left = get16();
                img.top = get16();
                img.width = get16();
                img.height = get16();

                flags = get8();
                img.interlaced = getFlag(FLAG_INTERLACE, 1);
                img.isSorted = getFlag(FLAG_LCT_SORTED, 1);

                lct.data = compileColorTableIfExists(lct.data);

                // TODO Parse image data

                break;

            // Extension introducer (0x21)
            case '!':
                switch(get8())
                {
                    // Graphics Control Extension
                    case 0xf9:

                        // GCE block size is always 4 bytes
                        expect(GCE_BLOCK_SIZE);

                        flags = get8();
                        gce.disposalMethod = getFlag(FLAG_DISPOSAL_METHOD, 3);
                        gce.expectingUserInput = getFlag(FLAG_USER_INPUT, 1);
                        gce.hasTransparencyIndex = getFlag(FLAG_TRANSPARENT, 1);

                        gce.delayTime = get16();
                        gce.transparentColorIndex = get8();

                        expect(BLOCK_TERMINATOR);
                        break;

                    // This program does not read any data from Comment Extensions.
                    case 0xfe:

                        while(true)
                        {
                            cmtBlockSize = get8();

                            if(cmtBlockSize == 0)
                                break;

                            currPos += cmtBlockSize;
                        }

                        break;

                    // TODO Plain Text Extension
                    case 0x01:
                        expect(TXT_EXT_BLOCK_SIZE);

                        expect(BLOCK_TERMINATOR);
                        break;

                    // TODO Application Extension
                    case 0xff:
                        expect(APP_EXT_BLOCK_SIZE);

                        expect(BLOCK_TERMINATOR);
                        break;

                    default:
                        exit(EXIT_FAILURE);
                        break;
                }

                break;

            // Trailer
            case ';':
                // We should be at the end of the file, since the trailer should be the last character
                if(currPos == file.size)
                {
                    exit(EXIT_SUCCESS);
                }
                else
                {
                    // TODO Display error message if the trailer is misplaced
                    exit(EXIT_FAILURE);
                }

                break;

            // Unrecognized introducer
            default:
                exit(EXIT_FAILURE);
                break;
        }
    }
}
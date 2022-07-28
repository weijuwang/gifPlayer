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

#define BYTE(n) (fileContents[n])

////////////////////////////////////////////////////////////////////////////////

#define BW_RAMP_LEN 70
const char bwPixels[BW_RAMP_LEN] = " .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";

bool ncursesStarted = false, playColor = true;

int currFlag;
FILE* filePtr;

long fileLen;
long currPos = 0;
char* fileContents = NULL;

int version, scrWidth, scrHeight, flags, bkgdColorIndex, pixelAspectRatio, bitDepth, imgLeft, imgTop, imgWidth, imgHeight;
bool isSorted, isInterlaced;
uint8_t* colorTable = NULL;

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
    if(fileContents[currPos] != val)
    {
        invalidGif();
    }
    else ++currPos;
}

uint_fast8_t get8(void)
{
    uint_fast8_t temp = fileContents[currPos];
    ++currPos;
    return temp;
}

uint_fast16_t get16(void)
{
    uint_fast16_t temp = *(uint16_t*)(fileContents + currPos);
    currPos += 2;
    return temp;
}

void teardown(void)
{
    if(fileContents != NULL)
        free(fileContents);

    if(colorTable != NULL)
        free(colorTable);

    if(ncursesStarted)
    {
        endwin(); // done with ncurses, back to normal terminal
    }
}

void compileColorTableIfExists(void)
{
    if(getFlag(FLAG_CT, 1))
    {
        size_t ctSize = (1U << (getFlag(FLAG_CT_SIZE, 3) + 1));
        uint_fast8_t r, g, b;

        colorTable = realloc(colorTable, ctSize * sizeof(uint8_t));

        // For each color in the color table...
        for(int i = 0; i < ctSize; ++i)
        {
            r = fileContents[currPos + 3*i];
            g = fileContents[currPos + 3*i + 1];
            b = fileContents[currPos + 3*i + 2];

            // Colored mode
            if(playColor)
            {
                //TODO compileColorTable(), colored mode
            }
            // Black-and-white mode
            else
            {
                float colorPos = BW_RAMP_LEN * (r * 0.299 + g * 0.587 + b * 0.114) / 256;
                colorTable[i] = bwPixels[(int)colorPos];
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

int main(const int argc, char** argv)
{
    atexit(teardown);

    ////////////////////////////////////////////////////////////////////////////
    /* Parse command-line options.
    */

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
    fileLen = ftell(filePtr); // How many bytes from us to the beginning? (i.e. how long is the file)
    rewind(filePtr); // Head back to the beginning

    // Read the file's contents into memory
    fileContents = malloc(fileLen);
    fread(fileContents, sizeof(uint8_t), fileLen, filePtr);

    // Could we NOT read the file?
    if(fileContents == NULL)
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
    /* Validate and parse the file
    */

    // Header - "GIF"
    expect('G');
    expect('I');
    expect('F');

    // Version - "87a", "89a"
    expect('8');
    version = get16();

    // Logical Screen Descriptor
    scrWidth = get16();
    scrHeight = get16();
    flags = get8();
    bkgdColorIndex = get8();
    pixelAspectRatio = (get8() + 15) / 64;
    bitDepth = getFlag(FLAG_BIT_DEPTH, 3) + 1;
    isSorted = getFlag(FLAG_GCT_SORTED, 1);

    compileColorTableIfExists();

    while(true)
    {
        switch(get8())
        {
            // Image separator (0x2c)
            case ',':
                imgLeft = get16();
                imgTop = get16();
                imgWidth = get16();
                imgHeight = get16();
                flags = get8();
                isInterlaced = getFlag(FLAG_INTERLACE, 1);
                isSorted = getFlag(FLAG_LCT_SORTED, 1);

                compileColorTableIfExists();

                break;

            // Extension
            case '!':
                break;

            // End of file
            case ';':
                break;
        }
    }

exit(EXIT_SUCCESS); // debug; skips ncurses
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
    /* TODO Play the video
    */

    ////////////////////////////////////////////////////////////////////////////
    /* Exit
    See `teardown()`, defined earlier, which is called automatically by `exit()`.
    */

    exit(EXIT_SUCCESS);
}
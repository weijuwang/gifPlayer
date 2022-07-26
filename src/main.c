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

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <ncurses.h>

#define MSG_USAGE "Usage: %s [-hl] [file]\n" // Must have \n
#define MSG_NOFILE "Nothing to play."
#define MSG_COULD_NOT_OPEN "Could not open file" // Do not add punctuation; `perror` adds a colon
#define MSG_COULD_NOT_READ "Could not read file."
#define MSG_COULD_NOT_CLOSE "Could not close file."
#define MSG_INVALID_GIF "Invalid GIF file (invalid data at position %lx).\n" // Must have \n

// Format: position, size
// Big-endian positions; higher positions = higher place values
#define FLAG_GCT 7
#define FLAG_BIT_DEPTH 6
#define FLAG_SORTED 3
#define FLAG_GCT_SIZE 2

#define BYTE(n) (fileContents[n])

////////////////////////////////////////////////////////////////////////////////

int currFlag;
FILE* filePtr;

long fileLen;
long currPos = 0;
char* fileContents;

uint16_t version, scrWidth, scrHeight;
uint8_t flags, bkgdColor, pixelAspectRatio, bitDepth, gctSize;
bool gctExists, isSorted;

////////////////////////////////////////////////////////////////////////////////

uint8_t
getFlag(const int pos, const int numBits)
{
    return (flags >> pos) % (1U << numBits);
}

void
invalidGif(void)
{
    printf(MSG_INVALID_GIF, currPos);
    exit(EXIT_FAILURE);
}

void
expect(const uint8_t val)
{
    if(fileContents[currPos] != val)
    {
        invalidGif();
    }
    else ++currPos;
}

uint8_t
get8(void)
{
    uint8_t temp = fileContents[currPos];
    ++currPos;
    return temp;
}

uint16_t
get16(void)
{
    uint16_t temp = *(uint16_t*)(fileContents + currPos);
    currPos += 2;
    return temp;
}

void
teardown(void)
{
    free(fileContents);
}

////////////////////////////////////////////////////////////////////////////////

int
main(const int argc, char** argv)
{
    ////////////////////////////////////////////////////////////////////////////
    /* Parse command-line options.
    */

    // For each flag passed...
    while((currFlag = getopt(argc, argv, "hl")) != -1)
    {
        switch(currFlag)
        {
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

    atexit(teardown);

    // Try closing the file, since its contents are now in memory. If it doesn't close...
    if(fclose(filePtr) == EOF)
    {
        puts(MSG_COULD_NOT_CLOSE);
        exit(EXIT_FAILURE);
    }

    // The file is now read into memory and closed.

    ////////////////////////////////////////////////////////////////////////////
    /* TODO Validate and parse the file
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
    bkgdColor = get8();
    pixelAspectRatio = get8();

    gctExists = getFlag(FLAG_GCT, 1);
    bitDepth = getFlag(FLAG_BIT_DEPTH, 3);
    isSorted = getFlag(FLAG_SORTED, 1);
    gctSize = getFlag(FLAG_GCT_SIZE, 3);

exit(EXIT_SUCCESS); // debug; skips ncurses
    ////////////////////////////////////////////////////////////////////////////
    /* Init ncurses
    FROM THIS POINT ON, DO NOT USE STANDARD TERMINAL IO.
    */

    initscr(); // Start the screen
    noecho(); // Do not echo user input back to the screen
    clear();

    ////////////////////////////////////////////////////////////////////////////
    /* TODO Play the video
    */

    ////////////////////////////////////////////////////////////////////////////
    /* Teardown
    `teardown()`, defined earlier, is called automatically by `exit()`.
    */

    endwin(); // done with ncurses, back to normal terminal
    exit(EXIT_SUCCESS);
}
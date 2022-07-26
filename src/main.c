const char* LICENSE_NOTICE = "\
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
along with this program.  If not, see <https://www.gnu.org/licenses/>.";

#include <stdlib.h> // exit, EXIT_FAILURE, EXIT_SUCCESS
#include <unistd.h> // getopt

#include <stdio.h> // printf, puts
#include <ncurses.h>

#define MSG_USAGE "Usage: %s [-hl] [file]\n" // Must have \n because this is passed to `printf`, not `puts` like the others
#define MSG_NOFILE "Nothing to play."
#define MSG_COULD_NOT_OPEN "Could not open file" // Do not add punctuation; `perror` adds a colon
#define MSG_COULD_NOT_READ "Could not read file."
#define MSG_COULD_NOT_CLOSE "Could not close file."

int
main(const int argc, char** argv)
{
    int currFlag;
    FILE* filePtr;
    long fileLen;
    char* fileContents;

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
                return EXIT_FAILURE;

            // Display help
            case 'h':
            default:
                printf(MSG_USAGE, argv[0]);
                return EXIT_FAILURE;
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
        return EXIT_FAILURE;
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
        return EXIT_FAILURE;
    }

    // Find file length
    fseek(filePtr, 0L, SEEK_END); // Go one byte past the end of the file
    fileLen = ftell(filePtr); // How many bytes from us to the beginning? (i.e. how long is the file)
    rewind(filePtr); // Head back to the beginning

    // Read the file's contents into memory
    fileContents = malloc(fileLen);

    // Could we NOT read the file?
    if(fileContents == NULL)
    {
        puts(MSG_COULD_NOT_READ);
        return EXIT_FAILURE;
    }

    // Try closing the file, since its contents are now in memory. If it doesn't close...
    if(fclose(filePtr) == EOF)
    {
        puts(MSG_COULD_NOT_CLOSE);
        return EXIT_FAILURE;
    }

    // The file is now read into memory and closed.

    ////////////////////////////////////////////////////////////////////////////
    /* Init ncurses
    FROM THIS POINT ON, DO NOT USE STANDARD TERMINAL IO.
    */

    initscr(); // Start the screen
    noecho(); // Do not echo user input back to the screen

    ////////////////////////////////////////////////////////////////////////////

    // TODO Actually play the video

    ////////////////////////////////////////////////////////////////////////////
    /* Teardown
    */

    endwin(); // done with ncurses, back to normal terminal
    free(fileContents);
    return EXIT_SUCCESS;
}
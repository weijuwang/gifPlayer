#include <stdio.h>
#include <ncurses.h>

int main(int argc, char** argv)
{
    // Initialize ncurses
    initscr();
    noecho();
    cbreak();

    printw("Hello World!\n");
    refresh();
    getch();

    // Tear down ncurses
    endwin();

    return 0;
}
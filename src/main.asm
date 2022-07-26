; gifPlayer, a terminal-based GIF player written in Assembly
; Copyright (C) 2022 Weiju Wang.
;
; This program is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program.  If not, see <https://www.gnu.org/licenses/>.

; Assemble: nasm -f elf64 -o main.o main.asm
; Link: gcc -o main main.o -lncurses

%define SYS_EXIT    60
%define SYS_WRITE   1

%define STDIN       0
%define STDOUT      1
%define STDERR      2

[section .rodata]

    helloWorld db "Hello World!", 0xa, 0

[section .text]

    global main

    extern initscr
    extern cbreak
    extern noecho
    extern endwin

    extern refresh
    extern clear

    extern printw
    extern getch

main:

    ; Fix stack alignment to 16 bytes
    sub rsp, 8

    ; Initialize ncurses
    call initscr
    call cbreak ; no line buffering
    call noecho ; do not echo user input back to terminal

    mov rdi, helloWorld
    call printw

    call refresh

    call getch

    ; Take down ncurses
    call endwin

    ; Exit w/ error code 0
    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

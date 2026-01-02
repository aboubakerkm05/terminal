@echo off
windres apt.rc apt.o
g++ main.cpp apt.o -o apt.exe -lwininet
pause

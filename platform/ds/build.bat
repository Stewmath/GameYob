@echo off
cd arm7
del build /f /q
rmdir build
cd..\arm9
del build /f /q
rmdir build
cd..
cls
make
pause > nul
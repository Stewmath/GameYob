@echo off
del gameyob.nds /f
cd arm7
del gameyob.elf /f
del build /f /q
rmdir build
cd..\arm9
del gameyob.elf /f
del build /f /q
rmdir build
cd..
cls
make
pause > nul
del E:\gameyob.test.nds /f
copy gameyob.nds E:\gameyob.test.nds
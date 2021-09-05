@echo off
if x==%1x goto IMPLICIT
make.exe -f Makefile.dos %1
goto END
:IMPLICIT
make.exe -f Makefile.dos clean
make.exe -f Makefile.dos
make.exe -f Makefile.dos dist
:END

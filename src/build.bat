@echo off

if not exist ..\build mkdir ..\build

set CommonCompilerFlags=-MTd -nologo -Gm- -GR- -EHa- -O2 -Oi -WX -W4 -wd4505 -wd4996 -wd4100 -wd4127 -FC -Z7 -DDEBUG=0
set CommonLinkerFlags= -incremental:no -opt:ref 

pushd ..\build
cl ..\src\server_win32.cpp %CommonCompilerFlags% /link %CommonLinkerFlags%
popd
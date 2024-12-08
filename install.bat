@echo off

@rem TODO - use drive env in script below
 
set bindir=d:\opt\bin2
set devenv="C:\PROGRA~1\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.com"
set devenv=F:\opt\VisualStudio\2022\Preview\Common7\IDE\devenv.exe 

@rem @echo Clean Build 
@rem %devenv% llfile.sln /Clean "Debug|x64" /Projectconfig "Debug|x64"
@rem %devenv% llfile.sln /Clean "Release|x64" /Projectconfig "Release|x64"
 

@echo ---- Clean Release llfile
del Bin\x64\Release\llfile.exe 2> nul
rmdir /s Obj  2> nul

@echo.
@echo ---- Build Release llfile
%devenv% llfile.sln /Build "Release|x64" 

@rem if not exist "Bin\x64\Release\llfile.exe" (
@rem @echo Try 2nd way of building.
@rem %devenv% llfile.sln /Project zlib /Build "Release|x64"  /Projectconfig "Release|x64"
@rem )

@echo.
@echo ---- Build done 
dir Bin\x64\Release\llfile.exe

if not exist "Bin\x64\Release\llfile.exe" (
   echo ". . .  Failed to build llfile.exe "
   goto _end
)

@echo ---- Uninstall llfile
Bin\x64\Release\llfile.exe -xu %bindir% > nul
Bin\x64\Release\llfile.exe -xr -f %bindir%\llfile.exe > nul
Bin\x64\Release\llfile.exe %bindir%\*.exe

@echo ---- Copy Release to c:\opt\bin2
copy Bin\x64\Release\llfile.exe %bindir%\llfile.exe
dir  Bin\x64\Release\llfile.exe %bindir%\llfile.exe

:: @echo
:: @echo Compare md5 hash
:: cmp -h Bin\x64\Release\llfile.exe c:\opt\bin\llfile.exe

:: @echo.
:: @echo List all llfile.exe
:: ld -r -h -p -F=llfile.exe bin c:\opt\bin

@echo ---- Install using llfile -xi
pushd %bindir%
llfile -xi > nul
ld -i *.exe 
popd
 
:_end


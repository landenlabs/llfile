@echo off

@rem TODO - use drive env in script below

set prog=llfile
set bindir=d:\opt\bin2

set devenv="C:\PROGRA~1\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.com"
set devenv=F:\opt\VisualStudio\2022\Preview\Common7\IDE\devenv.exe 
set msbuild=F:\opt\VisualStudio\2022\Preview\MSBuild\Current\Bin\MSBuild.exe

@rem @echo Clean Build 
@rem %devenv% llfile.sln /Clean "Debug|x64" /Projectconfig "Debug|x64"
@rem %devenv% llfile.sln /Clean "Release|x64" /Projectconfig "Release|x64"
 

@echo ---- Clean Release llfile
del Bin\x64\Release\llfile.exe 2> nul
lldu -sum Obj\x64\* 2> nul 
rmdir /s Obj  2> nul
@rem %msbuild% llfile.sln  -t:Clean

@echo.
@echo ---- Build Release llfile
@rem %devenv% llfile.sln /Project llfile /Build "Release|x64"  /Projectconfig "Release|x64"
%msbuild% llfile.sln -p:Configuration="Release";Platform=x64 -verbosity:minimal  -detailedSummary:True

@echo.
@echo ---- Build done 
if not exist "Bin\x64\Release\llfile.exe" (
   echo Failed to build llfile.exe 
   goto _end
)
dir Bin\x64\Release\llfile.exe

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

@rem play happy tone
rundll32.exe cmdext.dll,MessageBeepStub
rundll32 user32.dll,MessageBeep
 
:_end


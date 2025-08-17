@echo on

@rem TODO - use drive env in script below

set prog=llfile

set dstdir=%bindir%
if not exist "%dstdir%" (
 if exist c:\opt\bin  set dstdir=c:\opt\bin
 if exist d:\opt\bin2 set dstdir=d:\opt\bin2
)

if not exist "%msbuild%" (
echo Fall back msbuild not found at "%msbuild%"
set msbuild=G:\opt\VisualStudio\2022\Preview\MSBuild\Current\Bin\MSBuild.exe
)

@echo ---- Clean Release llfile
del Bin\x64\Release\llfile.exe 2> nul
lldu -sum Obj\x64\* 2> nul 
rmdir /s Obj  2> nul
@rem %msbuild% llfile.sln  -t:Clean

@echo.
@echo ---- Build Release llfile
@rem %devenv% llfile.sln /Project llfile /Build "Release|x64"  /Projectconfig "Release|x64"
"%msbuild%" llfile.sln -p:Configuration="Release";Platform=x64 -verbosity:minimal  -detailedSummary:True

@echo.
@echo ---- Build done 
if not exist "Bin\x64\Release\llfile.exe" (
   echo Failed to build llfile.exe 
   goto _end
)
dir Bin\x64\Release\llfile.exe

@echo ---- Uninstall llfile
Bin\x64\Release\llfile.exe -xu %dstdir% > nul
Bin\x64\Release\llfile.exe -xr -f %dstdir%\llfile.exe > nul
Bin\x64\Release\llfile.exe %dstdir%\*.exe

@echo ---- Copy Release to %dstdir%
copy Bin\x64\Release\llfile.exe %dstdir%\llfile.exe
dir  Bin\x64\Release\llfile.exe %dstdir%\llfile.exe

:: @echo
:: @echo Compare md5 hash
:: cmp -h Bin\x64\Release\llfile.exe %dstdir%\llfile.exe

:: @echo.
:: @echo List all llfile.exe
:: ld -r -h -p -F=llfile.exe bin %dstdir%

@echo ---- Install using llfile -xi
pushd %dstdir%
llfile -xi > nul
ld -i *.exe 
popd

@rem play happy tone
rundll32.exe cmdext.dll,MessageBeepStub
rundll32 user32.dll,MessageBeep
 
:_end


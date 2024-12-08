@echo off

@rem TODO - use drive env in script below
 
set bindir=d:\opt\bin2
set devenv="C:\PROGRA~1\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.com"
set devenv=F:\opt\VisualStudio\2022\Preview\Common7\IDE\devenv.exe 

@rem @echo Clean Build 
@rem %devenv% llfile22.sln /Clean "Debug|x64" /Projectconfig "Debug|x64"
@rem %devenv% llfile22.sln /Clean "Release|x64" /Projectconfig "Release|x64"
@rem lr -rf Obj Bin

@echo Build Release
%devenv% llfile22.sln /Build "Release|x64"  /Projectconfig "Release|x64"
%devenv% llfile22.sln /Project zlib /Build "Release|x64"  /Projectconfig "Release|x64"

ld Bin

@echo Uninstall
%bindir%\llfile.exe -xu %bindir%

@echo Copy Release to c:\opt\bin
del %bindir%\llfile.exe
copy Bin\x64\Release\llfile.exe %bindir%\llfile.exe
dir Bin\x64\Release\llfile.exe %bindir%\llfile.exe

:: @echo
:: @echo Compare md5 hash
:: cmp -h Bin\x64\Release\llfile.exe c:\opt\bin\llfile.exe

:: @echo.
:: @echo List all llfile.exe
:: ld -r -h -p -F=llfile.exe bin c:\opt\bin

@echo If all good - install using llfile -xi
pushd %bindir%
llfile -xi
popd


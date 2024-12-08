


@rem https://docs.microsoft.com/en-us/windows/win32/appxpkg/how-to-create-a-package-signing-certificate
@rem https://docs.microsoft.com/en-us/windows/win32/appxpkg/how-to-sign-a-package-using-signtool



@Rem use password Salem2nh-key

makecert /n "CN=LanDen Labs" /r /h 0   /eku "1.3.6.1.5.5.7.3.3,1.3.6.1.4.1.311.10.3.13" /e 01/01/2030  /sv llfiletest.pvk llfiletest.cer

@Rem convert pvk to pfx
pvk2pfx /pvk llfiletest.pvk /pi Salem2nh-key /spc llfiletest.cer /pfx llfiletest.pfx

@Rem tell computer it is trusted
certutil  -addStore TrustedPeople  llfiletest.cer

@Rem sign app
SignTool sign /fd SHA256 /a /f llfiletest.pfx /p Salem2nh-key llfile.exe




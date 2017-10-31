@echo off
set data_path=Data\Configs
set bin_path=Output\Targets\Windows-x86-32\Release\bin
set output_path=Output\Targets\Windows-x86-32\Quake
mkdir %output_path%
mkdir %output_path%\hipnotic
mkdir %output_path%\id1
mkdir %output_path%\rogue
xcopy /y "Data\Windows\Dissolution of Eternity.bat" %output_path%
xcopy /y "Data\Windows\Quake.bat" %output_path%
xcopy /y "Data\Windows\Scourge of Armagon.bat" %output_path%
xcopy /y %bin_path%\quake-gles1.exe %output_path%
xcopy /y %bin_path%\quake-gles2.exe %output_path%
xcopy /y %bin_path%\libEGL.dll %output_path%
xcopy /y %bin_path%\libGLES_CM.dll %output_path%
xcopy /y %bin_path%\libGLESv2.dll %output_path%
xcopy /y %bin_path%\SDL2.dll %output_path%
xcopy /y LICENSE.txt %output_path%
xcopy /y LICENSE_POWERVR_SDK.txt %output_path%
xcopy /y %data_path%\hipnotic\config.cfg %output_path%\hipnotic
xcopy /y %data_path%\id1\config.cfg %output_path%\id1
xcopy /y %data_path%\rogue\config.cfg %output_path%\rogue
pause

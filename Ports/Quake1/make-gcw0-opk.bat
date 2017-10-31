@echo off
set data_path=Data\GCWZero
set bin_path=Output\Targets\GCWZero\Release\bin
set output_path=Output\Targets\GCWZero\opk
mkdir %output_path%
mkdir %output_path%\hipnotic
mkdir %output_path%\id1
mkdir %output_path%\rogue
xcopy /y %data_path%\hipnotic.gcw0.desktop %output_path%
xcopy /y %data_path%\quake.gcw0.desktop %output_path%
xcopy /y %data_path%\rogue.gcw0.desktop %output_path%
xcopy /y %data_path%\manual-gcw0.txt %output_path%
xcopy /y %data_path%\hipnotic.png %output_path%
xcopy /y %data_path%\quake.png %output_path%
xcopy /y %data_path%\rogue.png %output_path%
xcopy /y %bin_path%\quake-gles1 %output_path%
xcopy /y %bin_path%\quake-gles2 %output_path%
xcopy /y %data_path%\hipnotic\platform.cfg %output_path%\hipnotic
xcopy /y %data_path%\id1\platform.cfg %output_path%\id1
xcopy /y %data_path%\rogue\platform.cfg %output_path%\rogue
rem mksquashfs %output_path% %output_path%\Quake2-GCWZero.opk
pause

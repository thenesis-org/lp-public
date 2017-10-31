@echo off
set data_path=Data\GCWZero
set bin_path=Output\Targets\GCWZero\Release\bin
set output_path=Output\Targets\GCWZero\Quake
mkdir %output_path%
mkdir %output_path%\hipnotic
mkdir %output_path%\id1
mkdir %output_path%\rogue
xcopy /y "Data\Linux\Dissolution of Eternity.sh" %output_path%
xcopy /y "Data\Linux\Quake.sh" %output_path%
xcopy /y "Data\Linux\Scourge of Armagon.sh" %output_path%
xcopy /y %bin_path%\quake-gles1 %output_path%
xcopy /y %bin_path%\quake-gles2 %output_path%
xcopy /y LICENSE.txt %output_path%
xcopy /y %data_path%\hipnotic\platform.cfg %output_path%\hipnotic
xcopy /y %data_path%\id1\platform.cfg %output_path%\id1
xcopy /y %data_path%\rogue\platform.cfg %output_path%\rogue
pause

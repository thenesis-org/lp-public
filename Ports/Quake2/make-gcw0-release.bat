@echo off
set data_path=Data\GCWZero
set bin_path=Output\Targets\GCWZero\Release\bin
set output_path=Output\Targets\GCWZero\Quake2
mkdir %output_path%
mkdir %output_path%\baseq2
mkdir %output_path%\ctf
mkdir %output_path%\rogue
mkdir %output_path%\xatrix
xcopy /y %data_path%\ctf.gcw0.desktop %output_path%
xcopy /y %data_path%\quake2.gcw0.desktop %output_path%
xcopy /y %data_path%\rogue.gcw0.desktop %output_path%
xcopy /y %data_path%\xatrix.gcw0.desktop %output_path%
xcopy /y %data_path%\manual-gcw0.txt %output_path%
xcopy /y %data_path%\ctf.png %output_path%
xcopy /y %data_path%\quake2.png %output_path%
xcopy /y %data_path%\rogue.png %output_path%
xcopy /y %data_path%\xatrix.png %output_path%
xcopy /y %bin_path%\quake2-gles1 %output_path%
xcopy /y %bin_path%\baseq2\game.so %output_path%\baseq2
xcopy /y %data_path%\baseq2\platform.cfg %output_path%\baseq2
xcopy /y %bin_path%\ctf\game.so %output_path%\ctf
xcopy /y %data_path%\ctf\platform.cfg %output_path%\ctf
xcopy /y %bin_path%\rogue\game.so %output_path%\rogue
xcopy /y %data_path%\rogue\platform.cfg %output_path%\rogue
xcopy /y %bin_path%\xatrix\game.so %output_path%\xatrix
xcopy /y %data_path%\xatrix\platform.cfg %output_path%\xatrix
rem mksquashfs %output_path% %output_path%\Quake2-GCWZero.opk
pause

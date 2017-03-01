@echo off
set output_path=Output\opk
set bin_path=Output\Targets\Generic\Release\bin
mkdir %output_path%
mkdir %output_path%\baseq2
mkdir %output_path%\ctf
mkdir %output_path%\rogue
mkdir %output_path%\xatrix
xcopy /y Data\GCWZero\ctf.gcw0.desktop %output_path%
xcopy /y Data\GCWZero\quake2.gcw0.desktop %output_path%
xcopy /y Data\GCWZero\rogue.gcw0.desktop %output_path%
xcopy /y Data\GCWZero\xatrix.gcw0.desktop %output_path%
xcopy /y Data\GCWZero\manual-gcw0.txt %output_path%
xcopy /y Data\GCWZero\ctf.png %output_path%
xcopy /y Data\GCWZero\quake2.png %output_path%
xcopy /y Data\GCWZero\rogue.png %output_path%
xcopy /y Data\GCWZero\xatrix.png %output_path%
xcopy /y %bin_path%\quake2-gles1 %output_path%
xcopy /y %bin_path%\baseq2\game.so %output_path%\baseq2
xcopy /y %bin_path%\ctf\game.so %output_path%\ctf
xcopy /y %bin_path%\rogue\game.so %output_path%\rogue
xcopy /y %bin_path%\xatrix\game.so %output_path%\xatrix
rem mksquashfs %output_path% %output_path%\quake2.opk
pause

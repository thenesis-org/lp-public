@echo off
set data_path=Data\GCWZero
set bin_path=Output\Targets\GCWZero\Release\bin
set output_path=Output\Targets\GCWZero\Quake2
mkdir %output_path%
mkdir %output_path%\baseq2
mkdir %output_path%\ctf
mkdir %output_path%\rogue
mkdir %output_path%\xatrix
xcopy /y "Data\Linux\Capture The Flag.sh" %output_path%
xcopy /y "Data\Linux\Ground Zero.sh" %output_path%
xcopy /y "Data\Linux\Quake 2.sh" %output_path%
xcopy /y "Data\Linux\The Reckoning.sh" %output_path%
xcopy /y %bin_path%\quake2-gles1 %output_path%
xcopy /y %bin_path%\quake2-gles2 %output_path%
xcopy /y %bin_path%\baseq2\game.so %output_path%\baseq2
xcopy /y %data_path%\baseq2\platform.cfg %output_path%\baseq2
xcopy /y %bin_path%\ctf\game.so %output_path%\ctf
xcopy /y %data_path%\ctf\platform.cfg %output_path%\ctf
xcopy /y %bin_path%\rogue\game.so %output_path%\rogue
xcopy /y %data_path%\rogue\platform.cfg %output_path%\rogue
xcopy /y %bin_path%\xatrix\game.so %output_path%\xatrix
xcopy /y %data_path%\xatrix\platform.cfg %output_path%\xatrix
pause

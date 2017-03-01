@echo off
set output_path=Output\Windows
set bin_path=Output\Targets\Generic\Release\bin
mkdir %output_path%
mkdir %output_path%\baseq2
mkdir %output_path%\ctf
mkdir %output_path%\rogue
mkdir %output_path%\xatrix
xcopy /y "Data\Windows\Capture The Flag.bat" %output_path%
xcopy /y "Data\Windows\Ground Zero.bat" %output_path%
xcopy /y "Data\Windows\Quake 2.bat" %output_path%
xcopy /y "Data\Windows\The Reckoning.bat" %output_path%
xcopy /y %bin_path%\quake2-gles1 %output_path%
xcopy /y %bin_path%\baseq2\game.dll %output_path%\baseq2
xcopy /y %bin_path%\ctf\game.dll %output_path%\ctf
xcopy /y %bin_path%\rogue\game.dll %output_path%\rogue
xcopy /y %bin_path%\xatrix\game.dll %output_path%\xatrix
pause

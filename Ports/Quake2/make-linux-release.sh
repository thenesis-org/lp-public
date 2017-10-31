#!/bin/bash
bin_path=Output/Targets/Linux-x86-32/Release/bin
output_path=Output/Targets/Linux-x86-32/Quake2
mkdir -p $output_path
mkdir -p $output_path/baseq2
mkdir -p $output_path/ctf
mkdir -p $output_path/rogue
mkdir -p $output_path/xatrix
cp -f "Data/Linux/Capture The Flag.sh" $output_path
cp -f "Data/Linux/Ground Zero.sh" $output_path
cp -f "Data/Linux/Quake 2.sh" $output_path
cp -f "Data/Linux/The Reckoning.sh" $output_path
cp -f "$bin_path/quake2-gles1" $output_path
cp -f "$bin_path/quake2-gles2" $output_path
cp -f "$bin_path/baseq2/game.so" $output_path/baseq2
cp -f "$bin_path/ctf/game.so" $output_path/ctf
cp -f "$bin_path/rogue/game.so" $output_path/rogue
cp -f "$bin_path/xatrix/game.so" $output_path/xatrix

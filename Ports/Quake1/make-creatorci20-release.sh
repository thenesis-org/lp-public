#!/bin/bash
data_path=Data/Configs
bin_path=Output/Targets/CreatorCI20/Release/bin
output_path=Output/Targets/CreatorCI20/Quake
mkdir -p $output_path
mkdir -p $output_path/hipnotic
mkdir -p $output_path/id1
mkdir -p $output_path/rogue
cp -f "Data/Linux/Dissolution of Eternity.sh" $output_path
cp -f "Data/Linux/Quake.sh" $output_path
cp -f "Data/Linux/Scourge of Armagon.sh" $output_path
cp -f "$bin_path/quake-gles1" $output_path
cp -f "$bin_path/quake-gles2" $output_path
cp -f "LICENSE.txt" $output_path
cp -f "$data_path/hipnotic/platform.cfg" $output_path/hipnotic
cp -f "$data_path/id1/platform.cfg" $output_path/id1
cp -f "$data_path/rogue/platform.cfg" $output_path/rogue

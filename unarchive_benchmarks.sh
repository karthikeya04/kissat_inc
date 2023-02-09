#!/bin/sh

src_dir=$1
# dest_dir=$2

for file in "$src_dir"/*.xz
do
	# echo $file
	unxz $file
	filename=$(basename -- "$file")
	cnf_file="${filename%.*}"
done

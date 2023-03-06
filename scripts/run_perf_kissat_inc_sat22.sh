#!/bin/bash

original_kissat_bin_path="selected_solvers/kissat_inc/sources/build/kissat"
optimized_kissat_bin_path="selected_solvers/karthikeya-kissat_inc/build/kissat"

benchmark_dir=$1
out_csv_file=$2
errors_file="erroneous_combs.csv"
touch $errors_file

echo
echo "original kissat_inc binary path: $original_kissat_bin_path"
echo "optimized kissat_inc binary path: $optimized_kissat_bin_path"

benchmarks=($(ls $benchmark_dir/**.cnf))
echo "total count of benchmarks: ${#benchmarks[@]}"
echo; echo "================"; echo

# create a new csv file to store results
# out_csv_file="06_03_22_kissat_inc_prefetch_one.csv"
# add a header row
echo "benchmark_name,original_time,optimized_time" > "$out_csv_file"

for cnf_file_cnt in ${!benchmarks[@]}; do
	cnf_file="${benchmarks[$cnf_file_cnt]}"
	echo "Benchmark count: $cnf_file_cnt"
	filename=$( echo "${cnf_file#*-}" ); echo "Benchmark name: $filename.xz"
	
	perf stat -o perf_original_out.log $original_kissat_bin_path $cnf_file > original_kissat_inc_output.log &
	wait
	temp=$( grep "seconds time elapsed" < perf_original_out.log )
	original_time_elapsed="${temp%seconds*}"
	echo "Original => time elapsed in seconds: ${original_time_elapsed// /}"
	
	perf stat -o perf_optimized_out.log $optimized_kissat_bin_path $cnf_file > optimized_kissat_inc_output.log 
	exit_code=$?
	if [[ $exit_code -eq 139 ]]; then echo "$filename.xz,${exit_code// /}" >> "$errors_file"; fi

	temp=$( grep "seconds time elapsed" < perf_optimized_out.log )
	optimized_time_elapsed="${temp%seconds*}"
	echo "Optimized => time elapsed in seconds: ${optimized_time_elapsed// /}"
	
	echo "$filename.xz,${original_time_elapsed// /},${optimized_time_elapsed// /}" >> "$out_csv_file"
	echo; echo "======================="; echo
done


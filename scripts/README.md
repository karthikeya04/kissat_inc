
### Script: `run_perf_kissat_inc_sat22.sh`

---

Run the script as shown below:

```sh
bash run_perf_kissat_inc_sat22.sh main_track_benchmarks 06_03_22_kissat_inc_orig_vs_pref_layout.csv
```



It takes the benchmarks directory as 1st argument and the output csv file name as 2nd argument.



The script first runs the `perf` for original kissat_inc binary for a benchmark and then runs the `perf` for optimized kissat_inc binary [commit id: `0c523c6`] for the same benchmark. The binary output is logged into separate files. Also, the perf output is captured in separate files.
The elapsed time parsed from the `perf` output is saved into the csv file.


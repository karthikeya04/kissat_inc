
The Kissat SAT Solver
=====================

Kissat is a "keep it simple and clean bare metal SAT solver" written in C.
It is a port of CaDiCaL back to C with improved data structures, better
scheduling of inprocessing and optimized algorithms and implementation.

Coincidentally "kissat" also means "cats" in Finnish.

Run `./configure && make test` to configure, build and test in `build`.

## Benchmark instances
You can use the benchmark `0398e6b20de133ba8b49c74b67dad7b7-6s133-sc2014.cnf` present in `benchmark_instances_2022` directory to run the solver(this might take around 3-5mins). 
All available bechmarks can be downloaded using following steps
```sh
cd benchmark_instances_2022
wget --content-disposition -i track_main_2022.uri
```
This will download the benchmark files in `.cnf.xz` format. This will download huge number of files, so it's better to kill the process after getting required number of benchmarks.
Unarchive these benchmarks by running `unarchive_benchmarks.sh` file present in the top level directory as follows:
```sh
bash unarchive_benchmarks.sh benchmark_instances_2022
```
This will extract all the benchmarks to their desirable format of `.cnf` which can be given to SAT solver as input.


all:
	$(MAKE) -C "/home/karthikeya/Desktop/SATarch/2022_experiments/selected_solvers/kissat_inc/sources/build"
kissat:
	$(MAKE) -C "/home/karthikeya/Desktop/SATarch/2022_experiments/selected_solvers/kissat_inc/sources/build" kissat
tissat:
	$(MAKE) -C "/home/karthikeya/Desktop/SATarch/2022_experiments/selected_solvers/kissat_inc/sources/build" tissat
clean:
	rm -f "/home/karthikeya/Desktop/SATarch/2022_experiments/selected_solvers/kissat_inc/sources"/makefile
	-$(MAKE) -C "/home/karthikeya/Desktop/SATarch/2022_experiments/selected_solvers/kissat_inc/sources/build" clean
	rm -rf "/home/karthikeya/Desktop/SATarch/2022_experiments/selected_solvers/kissat_inc/sources/build"
coverage:
	$(MAKE) -C "/home/karthikeya/Desktop/SATarch/2022_experiments/selected_solvers/kissat_inc/sources/build" coverage
indent:
	$(MAKE) -C "/home/karthikeya/Desktop/SATarch/2022_experiments/selected_solvers/kissat_inc/sources/build" indent
test:
	$(MAKE) -C "/home/karthikeya/Desktop/SATarch/2022_experiments/selected_solvers/kissat_inc/sources/build" test
.PHONY: all clean coverage indent kissat test tissat

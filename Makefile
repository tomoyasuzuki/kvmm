kvmm: main.c
	gcc -pthread -g -o kvmm main.c

clean:
	rm kvmm out.txt; touch out.txt;
h1-counter: h1-counter.cpp
	g++ -std=c++11 h1-counter.cpp -Wall -pedantic -o h1-counter

clean:
	rm -f h1-counter *.o



logjam: logjam.cpp
	g++  -Wall -std=c++11 -O3 -o logjam $<

clean:
	rm -rf *.o *~ logjam

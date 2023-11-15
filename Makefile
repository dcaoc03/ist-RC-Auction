FLAGS = -O3 -std=c++11 -Wall 

all:
	g++ $(FLAGS) user.cpp -lm -o user.o

clean:
	rm -f *.o
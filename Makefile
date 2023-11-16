FLAGS = -O3 -std=c++11 -Wall 
objects = user AS

all:
	g++ $(FLAGS) user.cpp -lm -o user
	g++ $(FLAGS) as.cpp -lm -o AS

clean:
	rm -f *.o $(objects)
FLAGS = -O3 -std=c++11 -Wall 
objects = user AS

all:
	g++ $(FLAGS) user.cpp ./common/utils.cpp -lm -o user
	g++ $(FLAGS) as.cpp ./common/utils.cpp ./common/database_utils.cpp -lm -o AS -pthread

clean:
	rm -f *.o $(objects)

all: mainMaster.o Master.o
	g++ -pthread -o a mainMaster.o Master.o

mainMaster.o: mainMaster.cpp Master.h
	g++ -c mainMaster.cpp
Master.o: Master.cpp Master.h
	g++ -c Master.cpp
clean:
	rm *.o 
	rm a

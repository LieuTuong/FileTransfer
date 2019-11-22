
all: server.o crc.o
	g++ -pthread -o ser server.o crc.o

server.o: server.cpp crc.h
	g++ -c mainMaster.cpp
crc.o: crc.cpp crc.h
	g++ -c crc.cpp
clean:
	rm *.o 
	rm a

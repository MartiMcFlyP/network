all: clean receiver
	

receiver: receiver.o socket.o packet_implement.o
	gcc -Wall -Werror -g -o receiver socket.o packet_implement.o -lz

receiver.o : src/receiver.c receiver.h
	gcc -Wall -Werror -g -c src/receiver.c -lz

receiver.o : src/packet_implement.c src/packet_interface.h
	gcc -Wall -Werror -g -c src/packet_implement.c -lz

receiver.o : src/socket.c socket.h
	gcc -Wall -Werror -g -c src/socket.c -lz



clean: @rm -mo *.o receiver 

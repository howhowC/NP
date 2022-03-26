all: hw4_server.o
	g++ -o hw4_server hw4_server.o -l sqlite3

%.o: %.cpp
	g++ -c $<

clean:
	rm -f *.o hw4_server *.db

rmpost.db:
	rm -f post.db

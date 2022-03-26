all: hw2_server.o
	g++ -o hw2_server hw2_server.o -l sqlite3

%.o: %.cpp
	g++ -c $<

clean:
	rm -f *.o hw2_server *.db

rmpost.db:
	rm -f post.db

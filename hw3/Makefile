all: hw3_server.o
	g++ -o hw3_server hw3_server.o -l sqlite3

%.o: %.cpp
	g++ -c $<

clean:
	rm -f *.o hw3_server *.db *.txt

rmpost.db:
	rm -f post.db

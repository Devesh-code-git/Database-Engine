# -*- MakeFile -*-

all: db

db: main.o b_plus_tree.o disk_operations.o cache.o
	gcc main.o b_plus_tree.o disk_operations.o cache.o -o db

main.o: main.c b_plus_tree.h disk_operations.h cache.h
	gcc -c main.c -o main.o

disk_btree.o: b_plus_tree.c b_plus_tree.h disk_operations.h
	gcc -c b_plus_btree.c -o disk_btree.o

disk_operations.o: disk_operations.c disk_operations.h cache.h b_plus_tree.h
	gcc -c disk_operations.c -o disk_operations.o

cache.o: cache.c cache.h disk_operations.h b_plus_tree.h
	gcc -c cache.c -o cache.o

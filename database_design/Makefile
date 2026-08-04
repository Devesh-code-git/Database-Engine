# -*- MakeFile -*-

all: db.exe

db.exe: main.o indexing/b_plus_tree.o disk_design/disk_operations.o disk_design/cache.o tools/engine.o tools/cursor.o display/show.o interaction/lexer.o interaction/tokenize.o
	gcc -I. main.o indexing/b_plus_tree.o disk_design/disk_operations.o disk_design/cache.o tools/engine.o tools/cursor.o display/show.o interaction/lexer.o interaction/tokenize.o -o db.exe

main.o: main.c interaction/lexer.h
	gcc -I. -c main.c -o main.o

indexing/b_plus_tree.o: indexing/b_plus_tree.c indexing/b_plus_tree.h tools/cursor.c disk_design/disk_operations.h
	gcc -I. -c indexing/b_plus_tree.c -o indexing/b_plus_tree.o

disk_design/disk_operations.o: disk_design/disk_operations.c disk_design/disk_operations.h disk_design/cache.h indexing/b_plus_tree.h
	gcc -I. -c disk_design/disk_operations.c -o disk_design/disk_operations.o

disk_design/cache.o: disk_design/cache.c disk_design/cache.h disk_design/disk_operations.h indexing/b_plus_tree.h
	gcc -I. -c disk_design/cache.c -o disk_design/cache.o

tools/engine.o: tools/engine.c tools/engine.h tools/cursor.h disk_design/disk_operations.h indexing/b_plus_tree.h display/show.h
	gcc -I. -c tools/engine.c -o tools/engine.o

tools/cursor.o: tools/cursor.c tools/cursor.h indexing/b_plus_tree.h disk_design/disk_operations.h display/show.h
	gcc -I. -c tools/cursor.c -o tools/cursor.o

display/show.o: display/show.c display/show.h indexing/b_plus_tree.h
	gcc -I. -c display/show.c -o display/show.o

interaction/lexer.o: interaction/lexer.c interaction/lexer.h interaction/tokenize.h interaction/tokens.h
	gcc -I. -c interaction/lexer.c -o interaction/lexer.o

interaction/tokenize.o: interaction/tokenize.c interaction/tokenize.h interaction/tokens.h tools/engine.h tools/cursor.h
	gcc -I. -c interaction/tokenize.c -o interaction/tokenize.o

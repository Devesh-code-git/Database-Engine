# Database-Engine

This repo contains the source code for my simple database engine project I made. The database works similarly to SQL-Lite,  
where it is a on-disk database engine which the user can interact with directly with their system, though there a few important differences.  

For one, this was mainly an education project on storing information on disk, and for implementing a persistent B-Plus Tree data structure,  
for simplicity I only included fixed size data, where the user can store a persons ID, NAME, and EMAIL in a Table. Also unlike SQL-Lite  
which stores all of its data inside one file on disk, in my engine, each table gets it own file, which also gets deleted when the user DROPS a table.  
As well, the data isn't serialized, so the files containing information about the tables wont necessarily work between other architectures and OS's apart  
from where it was first made.

How to run
-
Inside the database_design folder is all of the source code including the Makefile which will create the db.exe to run the program.  
The program takes in user input from the command line, like a REPL to perform actions based on what the user entered.  
For this project I kept a limited amount of features of a database, but these are all of the valid statements that can be entered to  
perform actions, it is in the style of a SQL type language.  

CREATE TABLE table_name;\n
DROP TABLE table_name;
SELECT * FROM table_name;
SELECT * FROM table_name WHERE id {<, <=, >, >=, =} num;
SELECT * FROM table_name WHERE id BETWEEN num_one AND num_two;
INSERT INTO table_name VALUES (num, "string one", "string two");
UPDATE table_name SET {name, email} = "string" WHERE id = num;
UPDATE table_name SET {name, email} = "string one", {name, email} = "string_two" WHERE id = num;
DELETE FROM table_name WHERE id = num;

I treated the ID value as the primary key for this database, so that means there cant be duplicate ID's in a table.  
The values in braces for the SELECT and UPDATE statements means any one of those value can be picked for the statements, 
except for in the UPDATE statement for two values, as only name or email can be changed once in that statement.  
When inserting a new row, every columns value must be defined. Dropping a table means deleting it, so be careful as the file  
containing data for that table will be permanently removed. String values must be surrounded by double-quotations. Also for  
SELECT statements I only included '*' for selecting all columns for readability, this engine does not support selecting individual  
columns.

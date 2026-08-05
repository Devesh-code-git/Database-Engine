# Database-Engine

This repo contains the source code for my simple database engine project I made. The database works similarly to SQL-Lite,  
where it is a on-disk database engine which the user can interact with directly with their system, though there a few important differences.  

For one, this was mainly an education project on storing information on disk, and for implementing a persistent B-Plus Tree data structure, for simplicity I only included fixed size data, where the user can store a persons ID, NAME, and EMAIL in a Table. Also unlike SQL-Lite  
which stores all of its data inside one file on disk, in my engine, each table gets it own file, which also gets deleted when the user DROPS a table.
As well, the data isn't serialized, so the files containing information about the tables wont necessarily work between other architectures and OS's apart from where it was first made.

How to run
-
Inside the database_design folder is all of the source code including the Makefile which will create the db.exe to run the program.  
The program takes in user input from the command line, like a REPL to perform actions based on what the user entered.  
For this project I kept a limited amount of features of a database, but these are all of the valid statements that can be entered to  
perform actions, it is in the style of a SQL type language.  

CREATE TABLE table_name;  
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
columns. They keywords are case sensitive and have to be typed in all capitals. Beyond that, the engine does give error messages when doing something wrong.
And to quit the program just type :q.  

Design of the database engine
-
The design for this engine can be broken up like this:  

User Input -> Lexer/Tokenizer -> Indexing -> Cache -> Disk -> Output  

The way it works is by taking what the user entered and tokenizing it, then seeing if it matches a valid statement and performing the required actions needed for it. 
The lexer loops through the statement and tokenizes what was entered, the lexer also gives compile time errors like missing semi-colons, unclosed strings, etc.. Then instead of 
building a full parser, since I only had 9 valid statements that could be entered, I just saw if what the user entered matches any of the 9 statements in tokenized form. 
During tokenization is when the user can get run time errors like if the table does not exist, ID already exists, etc.  

After that indexing is performed, where we find where the information is stored on disk. The data for each table is represented as a series of 4KB pages in one file. There are different kinds of pages representing different information. Page 0 is reserved for meta-data which saves different states of the table when starting up the program again. 
A FreePages page holds a list of pages that were freed, so they can be used again. A Records page is a page which holds Slots of information, each slot is the actual row data (ID, NAME, EMAIL). A FreeSlotsPage page holds a list of free slots that can be used again when inserting data. Then we have the pages representing the B-Plus Tree, an InternalPage is used to represent an internal node of the tree, and a LeafPage is used to represent a leaf node of the tree. At the indexing step we use the properties of the B-Plus tree to quickly traverse the nodes to find the Slot(s) at which the information needed is stored at around O(log n) time.  

While the indexing is happening, we use a caching system to hold frequently used pages in memory. I used the clock policy algorithm for caching. With the bit being continually incremented by 1 when a page is accessed, and the bit is halved with a bit-shit right when performing the sweeping algorithm. If the Page we need is already in cache then there is no need to access the disk, otherwise we have to get the page from disk and add it to the cache. The cache speeds up traversal of the tree mostly as it stores frequently used nodes longer than other nodes. Along with the B-Plus tree and Caching, I also implemented a simple cursor for ranges queries. As the leaf nodes primary keys are sorted in order, and each leaf node points to the leaf nodes to the left and right of it, a cursor can be used to save the location of the leaf node that holds the first element of the ranged query, and from there the cursor walks forwards or backwards until hitting the upper or lower bound, or reaching the end of the tree. This makes range searches must faster then traversing the tree for every entry in the ranges query.

And finally the data gets outputted in a nice format to the terminal for the user to see. For the output for names and emails too long to fit on the printed table, it gets truncated with 3 dots "...", so the format is not messed up.

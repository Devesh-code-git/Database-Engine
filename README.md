# Database-Engine

This repo contains the source code for my simple database engine project I made. The database works similarly to SQL-Lite,  
where it is a on-disk database engine which the user can interact with directly with their system, though there a few important differences.  

For one, this was mainly an education project on storing information on disk, and for implementing a persistent B-Plus Tree data structure,  
for simplicity I only included fixed size data, where the user can store a persons ID, NAME, and EMAIL in a Table. Also unlike SQL-Lite  
which stores all of its data inside one file on disk, in my engine, each table gets it own file, which also gets deleted when the user DROPS a table.  
As well, the data isn't serialized, so the files containing information about the tables wont necessarily work between other architectures and OS's apart  
from where it was first made.

--
--How to run

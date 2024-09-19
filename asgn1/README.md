# Assignment 1 directory

This directory contains source code and other files for Assignment 1.


While working on this assingment I saw a general overview/approach to implement memory.c and ended up using some of the suggesstions in my approach as I thought a lot of the suggestions it gave made sense and helped augment what I had already implemented. The post is attached below. I also consulted stack overflow when faced with differentiating between files and directorys. I ended up using the stat struct to handle this issue as it seemed to be the common reccomendation online. 



Overview Consulted for this project:


"Many people have asked questions about how to approach this assignment and the answer is: there's ten thousand different approaches. Fortunately for me, I watched Kung Fu Panda and learned as a small child that simpler (and slower) was better than more complicated (and not working). 

So, some protips before we get started:
Write a helper function that always writes out "n" bits from a buffer. This will make your life a lot easier.
Write a helper function that always reads out "n" bits from an fd and returns the number of bytes it read. Two exceptions: if it hits EOF (0), return the bytes it read. If it hits an error (-1), return -1

Alright, so now let's start with parsing. How do we parse the command? This is quite shrimple really. Make a buffer with 4 bytes of room. Read four bytes into the buffer. Now use strcmp to see if that buffer contains get\n or set\n. If not error. Boom.

Now, let's read in the file name. How do we do that? Do you want to do it fast? Too bad. This is the simple method so it will be slow. Alloc a buffer of MAX_PATH (maybe +1, idk) size. Read one byte at a time until you hit a new line. Once you hit a new line, replace that new line with a null byte. If you never hit a new line, throw an error. If you go over MAX_PATH in length, throw an error.
Now in get, that's basically all of the hard part. Open the file, handle errors, and output the file to stdout. Make sure to buffer reading the file and writing the file.

Now in set, you still need to get content length. Do the same thing as you did for getting the file name, except lower the maximum length to a smaller number. IDK, 30 should work. Now, once you have your buffer, use strtoull to get a number out of it. 

Great, let's finish off set. Do literally the exact same code as get but in reverse (stdin->file instead of the other way around). Handle edgecases around how many bytes to read.

This is (I believe), the shrimplest way to solve this assignment. If you like use anything here, probably a good idea to cite this guide in your README."

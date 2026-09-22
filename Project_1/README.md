# lobo-shell
An assignment for a simple shell for CS450

Please note, test 5 and 6a I believe will fail only on subsequent runs.
Basically, you have to run make clean before make, then make check
It has something to do with the umask and permissions not updating properly and make clean is the best solution I found.
Other than that, the other tests should run fine every time.

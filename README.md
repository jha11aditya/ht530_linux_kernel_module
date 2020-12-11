# ht530_linux_kernel_module
LKM device driver for hash table in linux kernel.

TO BUILD:
```make```

TO INSTALL:
```sudo insmod ht530.ko```

TO Test:
```./test```

Files: 
- ht530.c : main lkm source code
- test_ht530.c : main 4 threaded test driver code
- test_ht530_0.c: it was for initial testing(not included in submission)

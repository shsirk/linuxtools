
Minimal linux fuzz harness invoke application to redirect stdout/err to file and monitor for crash signal. 

1. ASAN messages will be dumped to file
2. On crash signal message will be dumped to file 

automate with shell script to run fuzz program with this harness and look into output file to check if crash is occured?

```
#make harness and sample crash programs to test
make

#make and run test
make test
```

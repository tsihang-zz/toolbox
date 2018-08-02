If you want to use this library, please do like this:
S1. Download $applsdk (https://github.com/tsihang/applsdk.git);
S2. Config env $APPL_SDK_HOME as the path of it.
      export APPL_SDK_HOME=$applsdk_real_path
S3. Download libcvhash ($THIS)
S3. Step into $libcvhash root, and make

# libcvhash
Consistent Virtual Node Hash library.
We are trying to implement 10 "physical machines" (actually, they are not the reality) with 10,000,000 objects injecting for this test. 
After finishing the test, it will show you the result about hashing mapping ratio for each physical machine.

# Dependency
libapr.apr_md5.
Acturally, you can remove md5_hash method in oryx_cvhash.c to cut this dependency down.

# How to run this library
$ make
$ ./cv_hash

# Result implement
A good hashing must satisfied 3 principles: Balance,Monotonicity,Spread(Load).
# 1st, Balancing test. we can see how 1000000 objects balanced to 10 machines from this test.

      Trying to inject 10000000 data ...

      Total              10(1600  vns) machines
              MACHINE          IPADDR VNS            HIT          RATIO
            Machine_0 103.198.105.115 160         992218           9.92%
            Machine_1   81.255.74.236 160         867227           8.67%
            Machine_2  41.205.186.171 160         943078           9.43%
            Machine_3  242.251.227.70 160        1022226          10.22%
            Machine_4  124.194.84.248 160        1010009          10.10%
            Machine_5  27.232.231.141 160        1031440          10.31%
            Machine_6    118.90.46.99 160        1086819          10.87%
            Machine_7  51.159.201.154 160         992705           9.93%
            Machine_8   102.50.13.183 160        1000492          10.00%
            Machine_9    49.88.163.90 160        1053786          10.54%

# 2nd, changes test when remove a machine (node) from orignal cluster.
      Trying to remove node ... "Machine_1", done, total_vns=1440

      Removing ...     "81.255.74.236" (Machine_1)
          Changes (867227  8.67%)

# 3rd, changes test when add a machine (node) to orignal cluster.
      Trying to add node ... "3ktubkpyro5uj0hq7kx2n4lqv4xyvwp", done, total_vns=1760

      Adding ...     81.255.74.236 (Machine_1)
       Changes (882733  8.83%)

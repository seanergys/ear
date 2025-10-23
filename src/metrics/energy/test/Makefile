all:
	gcc -g -I ../../../ -fPIC -shared -o ../node/hwmon.so ../node/hwmon.c ../../../common/libcommon.a
	gcc -DTEST=1 -g -I ../../.. -rdynamic -o test ../energy_node.c -Wl,--whole-archive ../../common/libmetrics-common.a -Wl,--no-whole-archive ../../libmetrics.a ../../../common/libcommon.a -lpthread -lm

clean:
	rm -f test


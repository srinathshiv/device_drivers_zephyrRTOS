cd build
cmake -GNinja -DBOARD=galileo ..
make
#expect /home/ninja07/temp/zephyr_kern/zephyr/samples/measure_n/pswd.exp
cd ..


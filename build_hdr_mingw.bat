cmake -G "MinGW Makefiles" -B build -DCMAKE_BUILD_TYPE=Release -DSIMPLE_NAMED_PIPE_BUILD_STATIC=OFF -DSIMPLE_NAMED_PIPE_BUILD_EXAMPLES=ON
cmake --build build
pause
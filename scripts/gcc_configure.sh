#!/bin/bash
set -x
export LDFLAGS="-Wl,-rpath=${CMAKE_BINARY_DIR}/lib64 -Wl,-rpath=${CMAKE_BINARY_DIR}/lib -static-libstdc++ -static-libgcc"
${GCC_SOURCE_DIR}/configure --prefix=${GCC_INSTALL_DIR} \
                            --disable-multilib \
                            --enable-languages=c,c++,fortran \
                            --disable-nls \
                            --with-system-zlib \
                            --with-mpfr-include=${CMAKE_BINARY_DIR}/include \
                            --with-mpfr-lib=${CMAKE_BINARY_DIR}/lib \
                            --with-gmp-include=${CMAKE_BINARY_DIR}/include \
                            --with-gmp-lib=${CMAKE_BINARY_DIR}/lib \
                            --with-mpc-include=${CMAKE_BINARY_DIR}/include \
                            --with-mpc-lib=${CMAKE_BINARY_DIR}/lib \
                            --with-isl-include=${CMAKE_BINARY_DIR}/include \
                            --with-isl-lib=${CMAKE_BINARY_DIR}/lib 
cmake_minimum_required(VERSION 3.18)

project(openmp-aid)

include(ExternalProject)

set(MPFR_VERSION "3.1.6")
set(GMP_VERSION "6.1.2")
set(ISL_VERSION "0.18")
set(MPC_VERSION "1.1.0")
set(ZLIB_VERSION "1.2.11")
set(GCC_VERSION "8.3.0")

set(MPFR_URL "https://www.mpfr.org/mpfr-${MPFR_VERSION}/mpfr-${MPFR_VERSION}.zip")
set(GMP_URL "https://gmplib.org/download/gmp/gmp-${GMP_VERSION}.tar.bz2")
set(ISL_URL "http://isl.gforge.inria.fr/isl-${ISL_VERSION}.tar.bz2")
set(MPC_URL "https://ftp.gnu.org/gnu/mpc/mpc-${MPC_VERSION}.tar.gz")
set(ZLIB_URL "https://zlib.net/fossils/zlib-${ZLIB_VERSION}.tar.gz")
set(GCC_URL "http://ftp.tsukuba.wide.ad.jp/software/gcc/releases/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.xz")

include(ProcessorCount)
ProcessorCount(NPROC)
if (NPROC EQUAL 0) 
  set(NPROC 1)
endif()
message(STATUS "ProcessorCount: ${NPROC}")

ExternalProject_Add(MPFR
  PREFIX            MPFR
  INSTALL_DIR       ${CMAKE_BINARY_DIR}
  URL               ${MPFR_URL}
  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
  BUILD_COMMAND     make -j${NPROC}
)

ExternalProject_Add(GMP
  PREFIX            GMP
  INSTALL_DIR       ${CMAKE_BINARY_DIR}
  URL               ${GMP_URL}
  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
  BUILD_COMMAND     make -j${NPROC}
)

ExternalProject_Add(ISL
  PREFIX            ISL
  INSTALL_DIR       ${CMAKE_BINARY_DIR}
  URL               ${ISL_URL}
  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
  BUILD_COMMAND     make -j${NPROC}
)

ExternalProject_Add(ZLIB
  PREFIX            ZLIB
  INSTALL_DIR       ${CMAKE_BINARY_DIR}
  URL               ${ZLIB_URL}
  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
  BUILD_COMMAND     make -j${NPROC}
)

ExternalProject_Add(MPC
  PREFIX            MPC
  INSTALL_DIR       ${CMAKE_BINARY_DIR}
  URL               ${MPC_URL}
  CONFIGURE_COMMAND <SOURCE_DIR>/configure
                    --prefix=<INSTALL_DIR>
                    --with-mpfr-include=${CMAKE_BINARY_DIR}/include 
                    --with-mpfr-lib=${CMAKE_BINARY_DIR}/lib 
                    --with-gmp-include=${CMAKE_BINARY_DIR}/include 
                    --with-gmp-lib=${CMAKE_BINARY_DIR}/lib
  BUILD_COMMAND     make -j${NPROC}
  DEPENDS           MPFR GMP
)

ExternalProject_Add(GCC
  PREFIX            GCC
  INSTALL_DIR       ${CMAKE_BINARY_DIR}
  URL               ${GCC_URL}
  CONFIGURE_COMMAND export CMAKE_BINARY_DIR=${CMAKE_BINARY_DIR} &&
                    export GCC_INSTALL_DIR=<INSTALL_DIR> &&
                    export GCC_SOURCE_DIR=<SOURCE_DIR> &&
                    ${PROJECT_SOURCE_DIR}/scripts/gcc_configure.sh
  UPDATE_COMMAND    rsync -a --delete --info=progress2 ${PROJECT_SOURCE_DIR}/gcc/ <SOURCE_DIR>/gcc &&
                    rsync -a --delete --info=progress2 ${PROJECT_SOURCE_DIR}/libgomp/ <SOURCE_DIR>/libgomp
  BUILD_COMMAND     make -j${NPROC}
  DEPENDS           MPFR GMP MPC ISL ZLIB
)
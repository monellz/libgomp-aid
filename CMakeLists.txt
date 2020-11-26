cmake_minimum_required(VERSION 3.18)

project(openmp-aid)

set(MPFR_VERSION "3.1.6")
set(GMP_VERSION "6.1.2")
set(MPC_VERSION "1.1.0")
set(ISL_VERSION "0.18")

function(DownloadProject NAME URL MD5)
  include(ExternalProject)
  ExternalProject_Add(${NAME}
    PREFIX            ${NAME}
    INSTALL_DIR       ${CMAKE_BINARY_DIR}
    URL               ${URL}
    URL_HASH          MD5=${MD5}
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> ${ARGN}
    BUILD_COMMAND     make -j$(nproc)
  )
  #set(${NAME}_LIBRARIES "${CMAKE_BINARY_DIR}/${NAME}/lib" PARENT_SCOPE)
  #set(${NAME}_INCLUDE "${CMAKE_BINARY_DIR}/${NAME}/include" PARENT_SCOPE)
endfunction()

DownloadProject(MPFR "https://www.mpfr.org/mpfr-${MPFR_VERSION}/mpfr-${MPFR_VERSION}.zip" "f5b65c53f2dd3dc2247b62c284721faa")
#ExternalProject_Get_property(MPFR SOURCE_DIR)
#message(STATUS "MPFR_LIBRARIES: ${MPFR_LIBRARIES}")
#message(STATUS "MPFR_INCLUDE: ${MPFR_INCLUDE}")

DownloadProject(GMP "https://gmplib.org/download/gmp/gmp-${GMP_VERSION}.tar.bz2" "8ddbb26dc3bd4e2302984debba1406a5")
#message(STATUS "GMP_LIBRARIES: ${GMP_LIBRARIES}")
#message(STATUS "GMP_INCLUDE: ${GMP_INCLUDE}")


DownloadProject(ISL "http://isl.gforge.inria.fr/isl-${ISL_VERSION}.tar.bz2" "11436d6b205e516635b666090b94ab32")
#message(STATUS "ISL_LIBRARIES: ${ISL_LIBRARIES}")
#message(STATUS "ISL_INCLUDE: ${ISL_INCLUDE}")

DownloadProject(MPC
  "https://ftp.gnu.org/gnu/mpc/mpc-${MPC_VERSION}.tar.gz"
  "4125404e41e482ec68282a2e687f6c73"
  "--with-mpfr-include=${CMAKE_BINARY_DIR}/include"
  "--with-mpfr-lib=${CMAKE_BINARY_DIR}/lib"
  "--with-gmp-include=${CMAKE_BINARY_DIR}/include"
  "--with-gmp-lib=${CMAKE_BINARY_DIR}/lib")
#message(STATUS "MPC_LIBRARIES: ${MPC_LIBRARIES}")
#message(STATUS "MPC_INCLUDE: ${MPC_INCLUDE}")
ExternalProject_Add_StepDependencies(MPC build MPFR GMP)


DownloadProject(GCC
  "http://ftp.tsukuba.wide.ad.jp/software/gcc/releases/gcc-8.3.0/gcc-8.3.0.tar.xz"
  "65b210b4bfe7e060051f799e0f994896"
  "--disable-multilib"
  "--enable-languages=c,c++,fortran"
  "--disable-nls"
  "--with-system-zlib"
  "--with-mpfr-include=${CMAKE_BINARY_DIR}/include"
  "--with-mpfr-lib=${CMAKE_BINARY_DIR}/lib"
  "--with-gmp-include=${CMAKE_BINARY_DIR}/include"
  "--with-gmp-lib=${CMAKE_BINARY_DIR}/lib"
  "--with-mpc-include=${CMAKE_BINARY_DIR}/include"
  "--with-mpc-lib=${CMAKE_BINARY_DIR}/lib"
  "--with-isl-include=${CMAKE_BINARY_DIR}/include"
  "--with-isl-lib=${CMAKE_BINARY_DIR}/lib"
  "--disable-shared"
  "--disable-host-shared"
  "--with-boot-ldflags=-static"
  "--with-stage1-ldflags=-static"
  #--with-stage1-ldflags="-Wl,-rpath=${CMAKE_BINARY_DIR}/lib64 -Wl,-rpath=${CMAKE_BINARY_DIR}/lib -Wl,-rpath=/home/zhongrunxin/spack/opt/spack/linux-ubuntu16.04-haswell/gcc-7.3.0/zlib-1.2.11-5q6bhs5vfgvppoadvm3jczuxb255wei3/lib"
  #"--with-stage1-ldflags='-Wl,-rpath,${CMAKE_BINARY_DIR}/lib -Wl,-rpath,/home/zhongrunxin/spack/opt/spack/linux-ubuntu16.04-haswell/gcc-7.3.0/zlib-1.2.11-5q6bhs5vfgvppoadvm3jczuxb255wei3/lib'"
  #--with-boot-ldflags="-Wl,-rpath=${CMAKE_BINARY_DIR}/lib64 -Wl,-rpath=${CMAKE_BINARY_DIR}/lib -Wl,-rpath=/home/zhongrunxin/spack/opt/spack/linux-ubuntu16.04-haswell/gcc-7.3.0/zlib-1.2.11-5q6bhs5vfgvppoadvm3jczuxb255wei3/lib"
)
#message(STATUS "GCC_LIBRARIES: ${GCC_LIBRARIES}")
#message(STATUS "GCC_INCLUDE: ${GCC_INCLUDE}")
ExternalProject_Add_StepDependencies(GCC build MPFR GMP MPC ISL)

# V2I project


## Table of Contents

1. [Introduction](#introduction)
2. [Get Code](#download-source-code)
3. [Third party](#third-party)
4. [TTC](#ttc)
5. [Gap](#gap)
6. [HMM](#hmm)


## Introduction
This project provides 

## Download Source Code
```shell
git clone git@github.com:foxbao/V2I.git
git checkout -b dev origin/dev
```

## Third Party
1. Eigen
```
sudo apt install libeigen3-dev
```
2. Ceres

Start by installing all the dependencies.
```shell
# CMake
sudo apt-get install cmake
# google-glog + gflags
sudo apt-get install libgoogle-glog-dev libgflags-dev
# Use ATLAS for BLAS & LAPACK
sudo apt-get install libatlas-base-dev
# SuiteSparse (optional)
sudo apt-get install libsuitesparse-dev
```
We are now ready to build, test, and install Ceres.
```shell
wget http://ceres-solver.org/ceres-solver-2.1.0.tar.gz
tar zxf ceres-solver-2.1.0.tar.gz
mkdir ceres-bin
cd ceres-bin
cmake ../ceres-solver-2.1.0
make -j3
make test
# Optionally install Ceres, it can also be exported using CMake which
# allows Ceres to be used without requiring installation, see the documentation
# for the EXPORT_BUILD_DIR option for more information.
make install
```



## 2. Build
```
mkdir build
cd build
cmake ..
make
```




## 3. Code structure
```
├─CMakeLists.txt
├─README.md
├─main.cpp
├─modules
|    ├─CMakeLists.txt
|    ├─README.md
|    ├─common
|    |   └common.h
|    ├─TTC
|    |  ├─CMakeLists.txt
|    |  ├─ttc.cpp
|    |  ├─ttc.h
|    |  └ttc_test.cpp
|    ├─HMM
|    |  ├─CMakeLists.txt
|    |  ├─hmm.cpp
|    |  ├─hmm.h
|    |  ├─map.cpp
|    |  ├─map.h
|    |  ├─sequence.cpp
|    |  ├─sequence.h
|    |  ├─state.cpp
|    |  ├─state.h
|    |  ├─trellis.cpp
|    |  ├─trellis.h
|    |  └viterbi.cpp
|    ├─Gap
|    |  ├─CMakeLists.txt
|    |  ├─MLE.cpp
|    |  ├─MLE.h
|    |  ├─gap.cpp
|    |  └gap.h
```

## TTC
Time to Collision(TTC) is implemented to estimate safety of a moving vehicle with reference to another vehicle with which it may interacte. 

## GAP

Gap acceptance calculates the probability of passing before a vehicle. 
The gap parameters are estimated via Maximum Likelihood Estimate (MLE), which is implemented in the MLE. 

## HMM











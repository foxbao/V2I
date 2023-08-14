# V2I project


## Table of Contents

- [V2I project](#v2i-project)
  - [Table of Contents](#table-of-contents)
  - [Introduction](#introduction)
  - [Download Source Code](#download-source-code)
  - [Third Party](#third-party)
  - [Build](#build)
  - [TTC](#ttc)
  - [GAP](#gap)
  - [HMM](#hmm)


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

2. Opencv (Optional)

## Build
```
mkdir build
cd build
cmake ..
make
```


## TTC
Time to Collision(TTC) is implemented to estimate safety of a moving vehicle with reference to another vehicle with which it may interacte. 

## GAP

Gap acceptance calculates the probability of passing before a vehicle. 
The gap parameters are estimated via Maximum Likelihood Estimate (MLE), which is implemented in the MLE. 

## HMM

HMM is used to infer the vehicle localization on the lane level in order to know if the vehicle is driving on the 









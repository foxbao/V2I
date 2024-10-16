# V2I project

- [V2I project](#v2i-project)
  - [Introduction](#introduction)
  - [Download Source Code](#download-source-code)
  - [Docker Version Installation](#docker-version-installation)
    - [Build Docker](#build-docker)
    - [Start Docker Container](#start-docker-container)
    - [Enter Docker Container](#enter-docker-container)
    - [Build project in docker](#build-project-in-docker)
  - [Normal Version Installation](#normal-version-installation)
    - [Dependency](#dependency)
    - [Build](#build)
  - [TTC](#ttc)
  - [GAP](#gap)
  - [HMM](#hmm)
  - [view](#view)


## Introduction
This project provides 

## Download Source Code
```shell
git clone git@github.com:foxbao/V2I.git
git checkout -b dev origin/dev
```

## Docker Version Installation
### Build Docker
we can build a light docker without visualization function without OpenCV
```shell
cd docker/build
docker build  -f base.x86_64.dockerfile -t civ:v2i .
```
### Start Docker Container
```shell
cd V2I
docker run --rm -i -d -v `pwd`:/home/baojiali/Downloads/V2I --name v2i civ:v2i
```
### Enter Docker Container
```shell
cd V2I
docker exec -it v2i /bin/bash
```
### Build project in docker
```shell
mkdir build
cd build
cmake ..
make
```

## Normal Version Installation
### Dependency
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

3. Boost
```shell
sudo apt-get install libboost-all-dev
```
4. Proj
refer to https://www.osgeo.cn/proj/install.html. Download proj-9.0.0.tar.gz
```
sudo apt install sqlite3 
```



5. Opencv (Optional)
If we want to visualize something, we need to install OpenCV
https://docs.opencv.org/4.x/d2/de6/tutorial_py_setup_in_ubuntu.html
```shell
sudo apt-get install libpng-dev libjpeg-dev libopenexr-dev libtiff-dev libwebp-dev    
git clone https://github.com/opencv/opencv.git
cd opencv
mkdir build
cd build
cmake ../
make -j16
sudo make install
```
Attention, if we have already installed Anaconda, it may cause some problems in the installation of OpenCV. Please temporarily move Anaconda

### Build
```shell
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


```shell

```

## view
The viewer will show the map, trajectory and the HMM result
```shell
./build/bin/civview_test
```
The result will be saved in the folder img_result







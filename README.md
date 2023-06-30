# V2I Introduction

## 1. Requirement
1. Eigen \
sudo apt install libeigen3-dev
2. 

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

## 4. TTC
Time to Collision(TTC) is implemented to estimate safety of a moving vehicle with reference to another vehicle with which it may interacte. 

## 5. GAP

Gap acceptance calculates the probability of passing before a vehicle. 
The gap parameters are estimated via Maximum Likelihood Estimate (MLE), which is implemented in the MLE. 

## 6. HMM









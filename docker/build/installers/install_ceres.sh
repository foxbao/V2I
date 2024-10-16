cd /Downloads
# CMake
apt-get install -y cmake
# google-glog + gflags
apt-get install -y libgoogle-glog-dev libgflags-dev

# Use ATLAS for BLAS & LAPACK
apt-get install -y libatlas-base-dev
# SuiteSparse (optional)
apt-get install -y libsuitesparse-dev

wget http://ceres-solver.org/ceres-solver-2.1.0.tar.gz
tar zxf ceres-solver-2.1.0.tar.gz
mkdir ceres-bin
cd ceres-bin
cmake ../ceres-solver-2.1.0
make -j$(nproc)
make test
# Optionally install Ceres, it can also be exported using CMake which
# allows Ceres to be used without requiring installation, see the documentation
# for the EXPORT_BUILD_DIR option for more information.
make install
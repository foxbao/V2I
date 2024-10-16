FROM ubuntu:18.04

RUN mkdir /Downloads

COPY installers /Downloads/installers

RUN apt-get update
RUN apt-get install -y libeigen3-dev cmake wget
RUN apt-get install -y libboost-all-dev

RUN bash /Downloads/installers/install_ceres.sh
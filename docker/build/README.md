# V2I Docker Image Build Process
- [V2I Docker Image Build Process](#v2i-docker-image-build-process)
  - [build the light Docker](#build-the-light-docker)
  - [Start Docker Container](#start-docker-container)
  - [Enter Docker Container](#enter-docker-container)


## build the light Docker
we can build a light docker without visualization function without OpenCV
```shell
cd docker/build
docker build  -f base.x86_64.dockerfile -t civ:v2i .
```

## Start Docker Container
```shell
cd V2I
docker run --rm -i -d -v `pwd`:/home/baojiali/Downloads/V2I --name v2i civ:v2i
```

## Enter Docker Container
```shell
cd V2I
docker exec -it v2i /bin/bash
```


#!/bin/bash
cd ~/mec
git pull
git submodule init
cd ~/mec/tools/osm-parser1/odrviewer
git pull
cd ~/mec/fusion-service
git pull
cd ~/mec/zsfd
git pull
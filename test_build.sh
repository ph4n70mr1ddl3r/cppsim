#!/bin/bash
cd /home/riddler/cppsim
rm -rf build
cmake -B build -S .
cmake --build build

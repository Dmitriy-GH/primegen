#!/bin/bash
clear
g++ primegen.cpp -O2 -oprimegen -pthread -std=c++11

./primegen 100000000

./primegen 100000000 200000000 -t:3

./primegen 1000 -f:prime1000.txt -w:50

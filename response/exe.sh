#!/bin/bash
clear
make clean
make
./bin/http-response examples/response_example2.txt

#!/bin/bash

find $@ > find.txt 2>&1
$MYFIND $@ > me.txt 2>&1

meld find.txt me.txt
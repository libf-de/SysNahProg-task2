#!/bin/bash

find $@ > find.txt 2>&1
./main $@ > me.txt 2>&1
diff find.txt me.txt

#meld find.txt me.txt

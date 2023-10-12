#!/bin/bash

find $@ > find.txt 2> find2.txt
$MYFIND $@ > me.txt 2> me2.txt
echo "stdout diff:"
diff find.txt me.txt
echo "stderr diff:"
diff find2.txt me2.txt
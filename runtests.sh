#!/bin/bash

for filename in tests/*.txt; do
  name=`basename $filename .txt`
  echo -e "Testing $filename:\n"
  ./testfile.sh $name
  echo "----------------------------------------"
done

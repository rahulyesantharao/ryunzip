#!/bin/bash

# check number of arguments
if [ $# -ne 1 ]; then
  echo "usage: test <test name>"
  exit 1
fi

# switch to the tests directory
cd tests/

# make sure file exists
if [ ! -f "$1.txt" ]; then
  echo "tests/$1.txt does not exist"
  exit 1
fi

# make a copy of the original
cp "$1.txt" "$1.1.txt"

# zip the original
gzip "$1.txt"

# unzip the original
echo "Unzipping $1.txt.gz"
../ryunzip "$1.txt.gz"

# check the differences
echo -e "\n\nDifferences:\n"
diff "$1.txt" "$1.1.txt"
if [ $? -eq 0 ]; then
  echo -e "None!\n"
  mv "$1.1.txt" "passed/$1.txt"
  rm "$1.txt"
else
  rm "$1.txt"
  mv "$1.1.txt" "$1.txt"
fi

# clean up
rm "$1.txt.gz"

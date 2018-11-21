#!/bin/bash

function cleanup { # filename, passed
  if [ $2 -ne 0 ]; then # failed
    rm -f "$1.txt"
    mv "$1.1.txt" "$1.txt"
  else # passed
    mv "$1.1.txt" "passed/$1.txt"
    rm "$1.txt"
  fi
  rm "$1.txt.gz"
  exit $2
}

# Parameters Parsing
filename=""
verbose=0
if [ $# -ne 1 ] && [ $# -ne 2 ]; then # check number of inputs
  echo "usage: testfile.sh [-v] <test name>"
  exit 1
fi
# parse out parameters
if [ $# -eq 1 ]; then # just filename
  filename=$1
else # flag and filename
  if [ "$1" != "-v" ]; then # check flag
    echo -e "invalid flag: $1\nusage: testfile.sh [-v] <testname>"
    exit 1
  fi
  filename=$2
  verbose=1
fi
# make sure file exists
if [ ! -f "tests/$filename.txt" ]; then
  echo "tests/$filename.txt does not exist"
  exit 1
fi

# switch to the tests directory
cd tests/

# make a copy of the original
cp "$filename.txt" "$filename.1.txt"

# zip the original
gzip "$filename.txt"

# unzip the original
echo "Unzipping $filename.txt.gz:"
if [ $verbose -eq 0 ]; then
  ../ryunzip "$filename.txt.gz"
else
  ../ryunzip -v "$filename.txt.gz"
fi
if [ $? -ne 0 ]; then # ryunzip failed
  echo "Unzip Failed"
  cleanup "$filename" 1
fi
echo "Unzip Succeeded!"

# check the differences
echo -e "\nDifferences:"
diff "$filename.txt" "$filename.1.txt"
if [ $? -ne 0 ]; then # differences exist
  cleanup "$filename" 1
fi

# everything passed!
echo "None!"
cleanup "$filename" 0
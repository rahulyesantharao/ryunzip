#!/bin/bash
shopt -s nullglob
testfiles=(tests/*.txt)

# check if there are tests
if [ ${#testfiles[@]} -eq 0 ]; then
  echo "No Tests!"
  exit 0
fi

shopt -u nullglob

# run through tests
passed=0
for filename in ${testfiles[@]}; do
  name=`basename $filename .txt`
  echo "Testing $filename:"
  scripts/testfile.sh $name
  if [ $? -eq 0 ]; then
    ((passed++))
  fi
  echo "----------------------------------------"
done
echo "$passed/${#testfiles[@]} Tests Passed!"

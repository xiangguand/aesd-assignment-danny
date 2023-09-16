#!/usr/bin/bash

# $1 : File Directory Path.
# $2 : String to be searched in the specified directory path.

### First: Check the argument number
if [ $# -ne 2 ]
then
  echo "Please give two argument as follows"
  echo "  1) File Directory Path."
  echo "  2) String to be searched in the specified directory path."
  echo "For example: ./finder.sh /tmp/aesd/assignment1 linux"
  exit 1
fi

X=$(ls $1 | wc -l)
Y=$(grep -r $2 $1 | wc -l)

echo "The number of files are ${X} and the number of matching lines are ${Y}"


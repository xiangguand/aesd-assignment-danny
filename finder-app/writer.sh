#!/usr/bin/bash

# $1 : A full path to a file.
# $2 : Text string which will be written within the file.

if [ $# -ne 2 ]
then
  echo "Please give two argument as follows"
  echo "  1) A full path to a file."
  echo "  2) Text string which will be written within the file."
  echo "For example: ./writer.sh /tmp/aesd/assignment1/sample.txt ios"
  exit 1
fi


# Get the directory name
DIR="$(dirname "$1")"
mkdir -p ${DIR}

if [ -f "$1" ]
then
  echo "File already exists"
else
  # echo "File doesn't exit, create and write"
  echo $2 > "$1"
fi



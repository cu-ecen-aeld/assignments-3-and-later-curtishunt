#!/bin/sh

# Check number of inputs
if [ "$#" -ne 2 ]; then
    echo "ERROR! usage is: finder.sh <path to dir> <search pattern>"
    exit 1
fi

# Assign args
filesdir=$1
searchstr=$2

# Check if filesdir is valid 
if [ ! -d "$filesdir" ]; then
    echo "Error: '$filesdir' is not a directory, enter a path to a directory."
    exit 1
fi

#counter vars
file_count=0
line_count=0


# Iterate over all files in the directory and subdirectories

for i in $(find "$filesdir" -type f)
do
    num_count=$(grep -c "$searchstr" "$i")
    line_count=$((num_count + line_count))
    if [ "$num_count" -gt 0 ]; then
        file_count=$((file_count+1))
    fi
done

echo "The number of files are $file_count and the number of matching lines are $line_count"

exit 0

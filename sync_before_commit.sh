#!/bin/bash
set -x

if [[ -n "$1" ]] && [[ "$1" != "rev" ]]; then
    echo "Wrong Parameter: $1"
    exit 1
fi

if [ "$1" == "rev" ]; then
    rsync -a --delete --info=progress2 libgomp/ gcc-8.3.0/libgomp
    rsync -a --delete --info=progress2 gcc/ gcc-8.3.0/gcc
else
    rsync -a --delete --info=progress2 gcc-8.3.0/libgomp/ libgomp
    rsync -a --delete --info=progress2 gcc-8.3.0/gcc/ gcc 
fi



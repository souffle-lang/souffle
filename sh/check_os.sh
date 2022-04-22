#!/bin/bash

if [ -f "/etc/os-release" ]; then
    file="/etc/os-release"
elif [ -f "/usr/lib/os-release" ]; then
    file="/usr/lib/os-release"
else 
    exit 1
fi

ID=$(grep -G "^ID_LIKE=" $file | tr a-z A-Z)

#Fedora is special and has no ID_LIKE
if [ -z $ID ]; then
    ID=$(grep -G "^ID=" $file | tr a-z A-Z)
fi

echo $ID

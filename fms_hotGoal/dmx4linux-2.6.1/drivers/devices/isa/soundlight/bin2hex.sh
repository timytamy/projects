#!/bin/sh

STATIC=static
NAME=`basename $1 .asm`

echo "$STATIC unsigned char ${NAME}_bin [] = {"
od -tx1 -v | cut -b9-100 | sed -e 's/[0-9a-zA-Z][0-9a-zA-Z]/0x&,/g'
echo "};"
echo "$STATIC DMXBinary firmware_$NAME = {\"$NAME\", sizeof(${NAME}_bin), ${NAME}_bin};"

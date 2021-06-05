#!
if [[ $# -ne 1 ]]; then
	echo "$0 [flash.bin]"
	exit
fi
FILE=$1
if [ ! -f "$FILE" ]; then
	echo "$FILE not found"
	exit
fi

picotool load -o 0x10050000 $FILE

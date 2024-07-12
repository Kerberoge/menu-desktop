#!/bin/bash

ROWS=10000

create_random() {
	arr=()
	for i in {a..z} {0..9}; do
		arr[$RANDOM]=$i
	done
}

[[ -f data ]] && rm data

for it in $(seq 1 $ROWS); do
	create_random

	if [[ $it -le $(($ROWS / 2)) ]]; then
		printf "/usr/share/applications/" >> data
	else
		printf "/home/$USER/.local/share/applications/" >> data
	fi

	printf "%s" ${arr[@]::4} $'\n' >> data
done

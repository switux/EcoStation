#!/bin/bash -

dat=$(date +"%Y%m%d")
if [ -f "$1/$dat.seq" ]; then
	seq=$(cat "$1/$dat.seq")
	seq=$(expr $seq + 1 )
else
	seq=1
fi
rm -f $1/*.seq
echo $seq > $1/${dat}.seq
seq=$(printf "%03d" $seq)
build=${dat}.${seq}
echo '#define BUILD_ID "'$build'"' > ${1}/build_id.h
clean=$(git diff)
branch=$(git rev-parse --abbrev-ref HEAD)
_hash=$(git log -p -1 | head -n 1 | awk '{print $2}')
hash=${_hash:0:7}
if [ -n "$clean" ]; then
	hash=${hash}" unclean"
fi
hash="$branch "${hash}
echo '#define GITHASH "'$hash'"' >> ${1}/build_id.h

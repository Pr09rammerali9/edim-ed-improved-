exe=edim
binpath=/usr/bin
flgs=-o $exe -lncurses

cc edim.c $flgs

if [[ $1 == "mv" ]]; then
mv $exe $binpath/$exe
fi 
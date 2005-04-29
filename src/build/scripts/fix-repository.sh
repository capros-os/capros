#!/bin/sh

for i in `find ${1} -type f -print | grep /CVS/Root$`
do
    echo -n "Updating \"${i}\"... "
    sed "s@${2}@${3}@" $i > $i.new
    mv ${i}.new ${i}
    echo "done"
done

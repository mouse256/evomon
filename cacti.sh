#!/bin/bash

#DATA=`telnet localhost 3123 2>/dev/null`

cat `dirname $0`/state.out | grep ";$1;" | sed "s/.*;.*;.*;.*;\(.*\);\(.*\);\(.*\);\(.*\);\(.*\)\(.\+\)/tempSet:\1 tempSetZone:\2 tempMeasured:\3 tempMeasuredZone:\4 heatDemandZone:\5.\6O/"

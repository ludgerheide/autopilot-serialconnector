#!/bin/sh
ssh 192.168.15.14 "killall gdbserver; rm /tmp/flightManager"
scp cmake-build-debug/flightManager  192.168.15.14:/tmp/
ssh -n -f  192.168.15.14 "gdbserver 0.0.0.0:8080 /tmp/flightManager"
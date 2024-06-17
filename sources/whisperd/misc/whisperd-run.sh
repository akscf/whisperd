#!/bin/bash

ulimit -S -c unlimited
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/whisperd/lib /opt/whisperd/bin/whisperd $*

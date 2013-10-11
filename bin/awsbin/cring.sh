#!/bin/sh

~/bin/grep_local_log.sh 200 lsd 0 total|awk '{total=total+$8}END{print total/200}'

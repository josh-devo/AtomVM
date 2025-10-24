#!/bin/bash

# Send events one at a time to demonstrate real-time processing

echo "user_login:alice"
sleep 0.3

echo "data_update:temperature=25C"
sleep 0.3

echo "notification:hello_from_host"
sleep 0.3

echo "STATS"
sleep 0.3

echo "QUIT"

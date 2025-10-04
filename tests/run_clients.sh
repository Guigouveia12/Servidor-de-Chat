#!/usr/bin/env bash
NUM=${1:-5}
HOST=${2:-127.0.0.1}
PORT=${3:-12345}

for i in $(seq 1 $NUM); do
  ( 
    printf "Hello from client-%02d\n" $i
    for j in $(seq 1 10); do
      printf "client-%02d message %02d\n" $i $j
      sleep $((RANDOM % 3 + 1))
    done
  ) | ./chat_client $HOST $PORT > "./client_${i}.log" 2>&1 &
  sleep 0.1
done

echo "Spawned $NUM clients in background. Logs: client_*.log"

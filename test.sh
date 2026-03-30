#!/bin/bash

echo "=== Basic Test ==="
curl -s http://localhost:8080/notes.txt -o out.txt
cat out.txt

echo -e "\n=== Error Test ==="
curl -s http://localhost:8080/invalid.txt

echo -e "\n=== Load Test ==="
for i in {1..20}; do
  curl -s http://localhost:8080/notes.txt &
done
wait

echo "Done"
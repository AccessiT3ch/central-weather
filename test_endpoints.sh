#!/bin/bash

BASE_URL="http://localhost:8000"

echo "=========================================="
echo "Testing Central Weather FastAPI Endpoints"
echo "=========================================="

echo -e "\n[1] Testing Root / Health Check..."
curl -s -i -X GET "$BASE_URL/"

echo -e "\n\n[2] Testing Current Telemetry Endpoint (/api/current)..."
curl -s -X GET "$BASE_URL/api/current" | json_pp 2>/dev/null || curl -s -X GET "$BASE_URL/api/current"

echo -e "\n\n[3] Testing Historical Telemetry Endpoint (/api/history)..."
curl -s -X GET "$BASE_URL/api/history" | json_pp 2>/dev/null || curl -s -X GET "$BASE_URL/api/history"

echo -e "\n=========================================="
echo "Tests complete."

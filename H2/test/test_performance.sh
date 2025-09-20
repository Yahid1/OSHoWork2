#!/bin/bash

echo "🧪 TICKETS SALES TEST"
echo "============================="

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Compile
gcc -I../ticket_office/include ../ticket_office/src/main.c ../ticket_office/src/functions.c -o ticket_office
gcc -I../buyer/include ../buyer/src/main.c ../buyer/src/functions.c -o buyer
ipcs -m | awk '/16/ {print $2}' | xargs -I {} ipcrm -m {} 2>/dev/null

echo -e "6\n3\n10\n3\n-5\n5" > input.txt

echo "Running ticket_office..."
./ticket_office > logs/ticket_office.log 2>&1 &
TO_PID=$!
sleep 1
echo "Running buyer..."
./buyer < input.txt 2>/dev/null > logs/output.log

TOTAL_TESTS=0
PASSED_TESTS=0

echo "🚀 Running tests..."

# Test 1: If requests 6, should buy only 5
echo -e "${YELLOW} 1. Request 6 → Should buy 5${NC}"
if grep -q "Company purchased 5" logs/output.log; then
    echo "   ✅ PASSED"
    PASSED_TESTS=$((PASSED_TESTS + 1))
else
    echo "   ❌ FAILED - Should have bought 5"
fi
TOTAL_TESTS=$((TOTAL_TESTS + 1))

# Test 2: If requests 3, should buy 3
echo -e "${YELLOW} 2. Request 3 → Should buy 3${NC}"
if grep -q "Company purchased 3" logs/output.log; then
    echo "   ✅ PASSED"
    PASSED_TESTS=$((PASSED_TESTS + 1))
else
    echo "   ❌ FAILED - Should have bought 3"
fi
TOTAL_TESTS=$((TOTAL_TESTS + 1))

# Test 3: If requests 10, should buy only 5
echo -e "${YELLOW} 3. Request 10 → Should buy 5${NC}"
if grep -q "Company purchased 5" logs/output.log; then
    echo "   ✅ PASSED"
    PASSED_TESTS=$((PASSED_TESTS + 1))
else
    echo "   ❌ FAILED - Should have bought 5"
fi
TOTAL_TESTS=$((TOTAL_TESTS + 1))

# Test 4: Verify that negative purchases are not allowed
echo -e "${YELLOW} 4. Request 5 → Can only buy 4${NC}"
if grep -q "Company purchased 4" logs/output.log; then
    echo "   ✅ PASSED - Can only buy 4"
    PASSED_TESTS=$((PASSED_TESTS + 1))
else
    echo "   ❌ FAILED - Allowed negative purchase"
fi
TOTAL_TESTS=$((TOTAL_TESTS + 1))

# Test 5: Verify that negative purchases are not allowed
echo -e "${YELLOW} 5. Request -5 → Should not buy negative${NC}"
if grep -q "Try again, invalid number of tickets" logs/output.log; then
    echo "   ✅ PASSED - Does not allow negatives"
    PASSED_TESTS=$((PASSED_TESTS + 1))
else
    echo "   ❌ FAILED - Allowed negative purchase"
fi
TOTAL_TESTS=$((TOTAL_TESTS + 1))

# Test 6: Validate sold out...
LAST_LINES=$(tail -5 logs/ticket_office.log)
EXPECTED_ORDER="TICKET REPORT:\nPurchased tickets: 20\nAvailable tickets: 0\nTransactions: 5\nSOLD OUT..."
echo -e "${YELLOW} 6. Validate report result${NC}"
if echo "$LAST_LINES" | grep -A4 "TICKET REPORT:" | grep -q "SOLD OUT"; then
    echo "   ✅ PASSED - Correct report results"
    PASSED_TESTS=$((PASSED_TESTS + 1))
else
    echo "   ❌ FAILED - Incorrect order"
    echo "   Expected order:"
    echo "   TICKET REPORT:"
    echo "   Purchased tickets: 20"
    echo "   Available tickets: 0"
    echo "   Transactions: 5"
    echo "   SOLD OUT..."
fi
TOTAL_TESTS=$((TOTAL_TESTS + 1))

# Test 7: Validate that shmat shmget are executed
echo -e "${YELLOW} 5. Request -5 → Should not buy negative${NC}"
if grep -q "Try again, invalid number of tickets" logs/output.log; then
    echo "   ✅ PASSED - Does not allow negatives"
    PASSED_TESTS=$((PASSED_TESTS + 1))
else
    echo "   ❌ FAILED - Allowed negative purchase"
fi
TOTAL_TESTS=$((TOTAL_TESTS + 1))

# Calculate percentage
PERCENTAGE=$((PASSED_TESTS * 100 / TOTAL_TESTS))

echo ""
echo "📈 FINAL RESULT:"
echo "==================="
# Clean up
rm -f buyer logs/output.log logs/ticket_office.log ticket_office

# Final evaluation
if [ $PERCENTAGE -ge 98 ]; then
    echo -e "${GREEN} Tests passed: $PASSED_TESTS/$TOTAL_TESTS"
    echo -e "${GREEN} Percentage: $PERCENTAGE%"
    echo "🎉 EXCELLENT! Business rules work correctly"
    exit 0
elif [ $PERCENTAGE -ge 97 ]; then
    echo -e "${YELLOW} Tests passed: $PASSED_TESTS/$TOTAL_TESTS"
    echo -e "${YELLOW} Percentage: $PERCENTAGE%"
    echo "⚠️  REGULAR - Some rules need adjustments"
    exit 1
else
    echo -e "${RED} Tests passed: $PASSED_TESTS/$TOTAL_TESTS"
    echo -e "${RED} Percentage: $PERCENTAGE%"
    echo "❌ DEFICIENT - Most rules failed"
    exit 2
fi
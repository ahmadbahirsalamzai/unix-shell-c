SRC := src
RESULT := tests-out

all:
	gcc -Wall -Wextra $(SRC)/wish.c -o wish

test:
	./wish

test-batch: all
	./tests/run-tests.sh $*

clean:
	rm -rf $(RESULT)
	rm -f wish

.PHONY: all test test-batch clean

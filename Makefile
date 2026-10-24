PYTHON ?= python3
FUZZ_COUNT ?= 500

.PHONY: all test python-test c-test asan fuzz golden-update clean

all:
	$(MAKE) -C c

test: python-test c-test fuzz

python-test:
	cd python && $(PYTHON) -m pytest -q

c-test:
	$(MAKE) -C c test

asan:
	$(MAKE) -C c asan

fuzz: all
	$(PYTHON) tools/diff_fuzz.py --binary c/build/minic --count $(FUZZ_COUNT)

golden-update:
	PYTHONPATH=$(CURDIR)/python sh tests/golden.sh update $(PYTHON) -m minic

clean:
	$(MAKE) -C c clean
	find . -name __pycache__ -type d -prune -exec rm -rf {} +
	rm -rf python/.pytest_cache python/*.egg-info python/build

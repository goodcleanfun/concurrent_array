install:
	clib install --dev

test:
	@$(CC) test.c $(CLFAGS) -I src -I deps -o $@
	@./$@

.PHONY: test

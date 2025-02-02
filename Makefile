install:
	clib install --dev

test:
	@$(CC) $(CFLAGS) $(LDFLAGS) test.c -I src -I deps -o $@
	@./$@

.PHONY: install test

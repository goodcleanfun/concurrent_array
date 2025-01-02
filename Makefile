
test:
	clib install --dev
	@$(CC) test.c $(CLFAGS) -I src -I deps -o $@
	@./$@

.PHONY: test

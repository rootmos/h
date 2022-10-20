README.md: README.in.md deps-table.md
	./include "$<" "$@"

deps-table.md:
	./deps-table > "$@"

.PHONY: clean
clean:
	rm -f deps-table.md

/* Leaks are not the bug class this target hunts; ASan's memory-corruption checks stay on. */
int __lsan_is_turned_off(void) { return 1; }

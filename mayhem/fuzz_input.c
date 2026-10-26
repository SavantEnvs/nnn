/* nnn's stdin file-list loader: NUL-separated paths resolved, grouped and linked into a temp tree. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>
/* load_input() sleeps after reporting limit/empty-input errors; skip the sleep so those paths stay fast */
#define usleep(us) ((void)(us), 0)
#include "nnn_harness.h"
#include <ftw.h>
#include <sys/mman.h>

static int rm_entry(const char *p, const struct stat *sb, int type, struct FTW *ftw)
{
	(void)sb; (void)type; (void)ftw;
	return remove(p);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	static char base[TMP_LEN_MAX];

	/* 2 bytes is the smallest entry ("x\0"): stays under LIST_FILES_MAX */
	if (size < 2 || size > 16384)
		return -1;

	nnn_harness_init();
	home = NULL; /* load_input() reports through msg() (stderr) rather than curses before init */

	if (!base[0]) {
		snprintf(base, sizeof(base), "/tmp/nnnfzXXXXXX");
		if (!mkdtemp(base))
			abort();
		xstrsncpy(g_tmpfpath, base, TMP_LEN_MAX);
		tmpfplen = (uchar_t)(strlen(base) + 1);
	}

	static const char * const cwds[] = { NULL, "/", "/usr/lib" };
	const char *cwd = cwds[data[0] % 3];

	int fd = memfd_create("nnn-input", 0);
	if (fd < 0)
		return -1;
	if (write(fd, data + 1, size - 1) != (ssize_t)(size - 1) || lseek(fd, 0, SEEK_SET) != 0) {
		close(fd);
		return -1;
	}

	char *tmpdir = load_input(fd, cwd);
	close(fd);

	if (tmpdir) {
		nftw(tmpdir, rm_entry, 16, FTW_DEPTH | FTW_PHYS);
		free(tmpdir);
	}
	free(listroot);
	listroot = NULL;
	listpath = NULL;
	return 0;
}

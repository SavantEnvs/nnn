/* Path and string helpers nnn applies to names from the filesystem, stdin lists and the command line. */
#include "nnn_harness.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	/* nnn works on paths bounded by PATH_MAX; its path buffers are PATH_MAX bytes. */
	if (size < 2 || size > 3000)
		return -1;

	nnn_harness_init();

	char *buf = malloc(size + 2);
	if (!buf)
		return -1;
	memcpy(buf, data, size);
	buf[size] = '\0';
	buf[size + 1] = '\0';

	uchar_t sel = (uchar_t)buf[0];
	char *s1 = buf + 1;
	char *s2 = s1 + strlen(s1) + 1;
	if (s2 > buf + size)
		s2 = buf + size;

	/* abspath(): heap-allocated result, with an explicit absolute cwd or the process cwd */
	char *cwd = (s2[0] == '/') ? s2 : NULL;
	char *abs1 = abspath(s1, cwd, NULL);

	/* abspath(): caller-provided PATH_MAX buffer (how nnn calls it for paths from the UI) */
	char out[PATH_MAX];
	size_t cwdlen = cwd ? strlen(cwd) : 64;
	if (strlen(s1) + cwdlen + 2 < PATH_MAX) {
		char *r = abspath(s1, cwd, out);
		if (r && r != out)
			abort();
	}

	if (abs1) {
		char *d = xstrdup(abs1);
		char *name = xstrdup(abs1);
		char *prefix = calloc(1, PATH_MAX);
		char *ext;

		if (d && name && prefix) {
			if (strlen(d) < PATH_MAX) {
				xdirname(d);
				common_prefix(d, prefix);
				char *abs2 = abspath(s2, cwd, NULL);
				if (abs2) {
					char *d2 = xstrdup(abs2);
					if (d2 && strlen(d2) < PATH_MAX) {
						xdirname(d2);
						common_prefix(d2, prefix);
					}
					free(d2);
					free(abs2);
				}
			}

			char *base = xbasename(name);
			size_t blen = strlen(base);
			if (blen) {
				ext = xextension(base, blen);
				(void)ext;
			}
		}
		free(d);
		free(name);
		free(prefix);
	}
	free(abs1);

	/* mkpath() joins an absolute directory with a name into a PATH_MAX buffer */
	if (s2[0] == '/' && strlen(s2) < PATH_MAX / 2 && strlen(s1) < PATH_MAX / 2)
		mkpath(s2, s1, out);

	/* shell_escape(): dynamic buffer and caller-sized buffer */
	{
		char *dyn = NULL;
		size_t dynlen = (sel & 1) ? 0 : 16 + sel;
		ssize_t r = shell_escape(&dyn, &dynlen, s1);
		if (r >= 0 && dyn && (size_t)r != strlen(dyn))
			abort();
		free(dyn);

		char fixed[512];
		size_t fixedlen = 3 + (sel % (sizeof(fixed) - 3));
		char *pf = fixed;
		r = shell_escape(&pf, &fixedlen, s1);
		if (r >= 0 && (size_t)r != strlen(fixed))
			abort();
	}

	/* name comparison used for sorting */
	{
		(void)xstricmp(s1, s2);
		(void)xstricmp(s2, s1);
		(void)xstrverscasecmp(s1, s2);
		(void)xstrverscasecmp(s2, s1);
		(void)is_suffix(s1, s2);
	}

	/* display-name sanitising into g_buf */
	if (strlen(s1) <= NAME_MAX)
		(void)unescape(s1, 1 + (sel % NAME_MAX));

	/* shell-like command splitting */
	{
		char *argv[EXEC_ARGS_MAX];
		int idx = 0;
		char *line = parseargs(s1, argv, &idx);
		free(line);
	}

	free(buf);
	return 0;
}

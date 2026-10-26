/* nnn's directory-listing filter and sort: substring, regex and fuzzy matching over a list of names. */
#include "nnn_harness.h"

#define MAX_NAMES 64

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (size < 3 || size > 4096)
		return -1;

	nnn_harness_init();

	char *buf = malloc(size + 1);
	if (!buf)
		return -1;
	memcpy(buf, data, size);
	buf[size] = '\0';

	uchar_t flags = (uchar_t)buf[0];
	char *fltr = buf + 1;
	char *nl = memchr(fltr, '\n', size - 1);
	if (!nl || (size_t)(nl - fltr) > REGEX_MAX - 2) { /* the UI keeps the pattern under REGEX_MAX */
		free(buf);
		return -1;
	}
	*nl = '\0';

	/* glibc regcomp() expands stacked repetition operators exponentially (gigabytes for ~25 of them);
	 * that is libc behaviour, not nnn's, so keep regex patterns to a bounded operator count. */
	if (flags & 1) {
		int reps = 0;
		for (const char *c = fltr; *c; ++c) {
			if (*c == '{')
				reps += 8;
			else if (*c == '*' || *c == '+' || *c == '?')
				++reps;
		}
		if (reps > 6) {
			free(buf);
			return -1;
		}
	}

	/* Directory entries as readdir() yields them: 1..NAME_MAX bytes, no '/', no NUL. */
	struct entry *ents = calloc(MAX_NAMES, sizeof(*ents));
	int n = 0;
	char *line = nl + 1;
	while (*line || line < buf + size) {
		char *end = strchr(line, '\n');
		size_t len = end ? (size_t)(end - line) : strlen(line);

		if (len == 0) {
			if (!end)
				break;
			line = end + 1;
			continue;
		}
		if (len < 2 || len > NAME_MAX + 1 || n == MAX_NAMES || memchr(line + 1, '/', len - 1)) {
			free(ents);
			free(buf);
			return -1;
		}

		uchar_t meta = (uchar_t)line[0];
		line[len] = '\0';
		ents[n].name = line + 1;
		ents[n].nlen = len; /* strlen(name) + 1 */
		ents[n].mode = (meta & 1) ? S_IFDIR : S_IFREG;
		ents[n].flags = (meta & 1) ? DIR_OR_DIRLNK : 0;
		ents[n].size = meta >> 1;
		ents[n].sec = (meta * 7) & 0xf;
		ents[n].blocks = meta >> 3;
		++n;

		if (!end)
			break;
		line = end + 1;
	}

	memset(&cfg, 0, sizeof(cfg));
	cfg.regex = flags & 1;
	cfg.fuzzy = !cfg.regex && (flags & 2);
	cfg.reverse = (flags >> 3) & 1;
	cfg.version = (flags >> 4) & 1;
	cfg.timeorder = ((flags >> 5) & 3) == 1;
	cfg.sizeorder = ((flags >> 5) & 3) == 2;
	cfg.extnorder = ((flags >> 5) & 3) == 3;
	filterfn = cfg.regex ? &visible_re : (cfg.fuzzy ? &visible_fuzzy : &visible_str);
	fnstrstr = (flags & 4) ? &strstr : &strcasestr;
	entrycmpfn = cfg.reverse ? &reventrycmp : &entrycmp;
	namecmpfn = cfg.version ? &xstrverscasecmp : &xstricmp;

	/* the filter prompt text, as stored in the context: prefix char + pattern */
	char prompt[REGEX_MAX];
	snprintf(prompt, sizeof(prompt), "%c%s", cfg.regex ? '/' : ':', fltr);

	/* keep a pristine copy: matches() reorders and shrinks the array */
	struct entry *all = malloc(sizeof(*ents) * (n ? n : 1));
	memcpy(all, ents, sizeof(*ents) * n);

	pdents = ents;
	ndents = n;
	int kept = matches(fltr);
	pdents = NULL;

	if (kept > 0) {
		uchar_t matched[NAME_MAX];
		for (int i = 0; i < kept; ++i) {
			if (cfg.fuzzy)
				fuzzy_match_positions(prompt + 1, ents[i].name, matched);
			else if (!cfg.regex)
				string_match_positions(prompt + 1, ents[i].name, matched);
		}
	}

	/* the same names through the other matchers and the fuzzy ranking */
	for (int i = 0; i < n; ++i) {
		fltrexp_t fx = { .regex = NULL, .str = fltr };
		(void)visible_str(&fx, all[i].name);
		(void)visible_fuzzy(&fx, all[i].name);
		(void)fuzzy_match_score(fltr, all[i].name);
	}

	free(all);
	free(ents);
	free(buf);
	return 0;
}

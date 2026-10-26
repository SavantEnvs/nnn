/* Known-answer probe over nnn's own code (built with upstream's normal flags). test.sh compares its output. */
#include "nnn_harness.h"
#include <dirent.h>
#include <ftw.h>

static int rm_entry(const char *p, const struct stat *sb, int type, struct FTW *ftw)
{
	(void)sb; (void)type; (void)ftw;
	return remove(p);
}

static void put(const char *name, const char *val)
{
	printf("%s=%s\n", name, val ? val : "(null)");
}

static void putsign(const char *name, int v)
{
	put(name, v < 0 ? "neg" : (v > 0 ? "pos" : "zero"));
}

static void filter_case(const char *label, unsigned flags, const char *fltr)
{
	static const struct { const char *name; int dir; } names[] = {
		{ "alpha", 0 }, { "beta", 0 }, { "Gamma", 0 }, { "delta", 0 }, { "theta", 0 }, { "zeta", 1 },
	};
	struct entry ents[6];
	char out[256] = "";

	memset(ents, 0, sizeof(ents));
	for (int i = 0; i < 6; ++i) {
		ents[i].name = (char *)names[i].name;
		ents[i].nlen = strlen(names[i].name) + 1;
		ents[i].mode = names[i].dir ? S_IFDIR : S_IFREG;
		ents[i].flags = names[i].dir ? DIR_OR_DIRLNK : 0;
	}

	memset(&cfg, 0, sizeof(cfg));
	cfg.regex = flags & 1;
	cfg.fuzzy = (flags & 2) != 0;
	filterfn = cfg.regex ? &visible_re : (cfg.fuzzy ? &visible_fuzzy : &visible_str);
	fnstrstr = &strcasestr;
	entrycmpfn = &entrycmp;
	namecmpfn = &xstricmp;
	pdents = ents;
	ndents = 6;
	int kept = matches(fltr);
	for (int i = 0; i < kept; ++i) {
		strcat(out, i ? "," : "");
		strcat(out, ents[i].name);
	}
	pdents = NULL;
	put(label, out);
}

int main(void)
{
	char buf[PATH_MAX];

	setlocale(LC_ALL, "C.UTF-8");
	home = strdup("/home/mayhem");
	homelen = (ushort_t)strlen(home);

	char *p = abspath("a/./b/../c", "/x/y", NULL);
	put("abspath_rel", p);
	free(p);
	p = abspath("/a//b/../../c", NULL, NULL);
	put("abspath_abs", p);
	free(p);
	p = abspath("~/docs", "/", NULL);
	put("abspath_tilde", p);
	free(p);

	char *esc = NULL;
	size_t esclen = 0;
	shell_escape(&esc, &esclen, "it's a b");
	put("shell_escape", esc);
	free(esc);

	char prefix[PATH_MAX] = "";
	char d1[] = "/usr/lib", d2[] = "/usr/local";
	common_prefix(d1, prefix);
	common_prefix(d2, prefix);
	put("common_prefix", prefix);

	putsign("verscmp_file2_file10", xstrverscasecmp("file2", "file10"));
	putsign("verscmp_a_A", xstrverscasecmp("a", "A"));
	putsign("stricmp_2_10", xstricmp("2", "10"));

	setenv(env_cfg[NNN_BMS], "a:/tmp;b:/usr/bin;c:/var", 1);
	kv *arr = NULL;
	char *copy = NULL;
	uchar_t items = 0;
	bool ok = parsekvpair(&arr, &copy, NNN_BMS, &items);
	snprintf(buf, sizeof(buf), "%d,%d", ok, items);
	put("kv_bms_parse", buf);
	bmstr = copy;
	put("kv_bms_b", get_kv_val(arr, buf, 'b', items, NNN_BMS));
	bmstr = NULL;

	setenv(env_cfg[NNN_PLUG], "x:foo;y:bar -l", 1);
	arr = NULL; copy = NULL; items = 0;
	parsekvpair(&arr, &copy, NNN_PLUG, &items);
	pluginstr = copy;
	put("kv_plug_y", get_kv_val(arr, buf, 'y', items, NNN_PLUG));
	pluginstr = NULL;

	setenv(env_cfg[NNN_ORDER], "t:time;s:size", 1);
	arr = NULL; copy = NULL; items = 0;
	parsekvpair(&arr, &copy, NNN_ORDER, &items);
	orderstr = copy;
	snprintf(buf, sizeof(buf), "%c", get_kv_key(arr, "size", items, NNN_ORDER));
	put("kv_order_size", buf);
	orderstr = NULL;

	filter_case("filter_substr", 0, "ta");
	filter_case("filter_regex", 1, "^[a-d]");
	filter_case("filter_fuzzy_name", 2, "gma");
	filter_case("filter_fuzzy_rank", 2, "ea");

	char line[] = "cp -r a b";
	char *argv[EXEC_ARGS_MAX];
	int idx = 0;
	char *cmd = parseargs(line, argv, &idx);
	snprintf(buf, sizeof(buf), "%d,%s,%s", idx, argv[2], argv[3]);
	put("parseargs", buf);
	free(cmd);

	/* stdin list loader: two paths in /usr/bin share a root and get linked into a temp tree */
	char base[] = "/tmp/nnnkatXXXXXX";
	if (!mkdtemp(base))
		return 2;
	xstrsncpy(g_tmpfpath, base, TMP_LEN_MAX);
	tmpfplen = (uchar_t)(strlen(base) + 1);
	home = NULL;
	int fds[2];
	const char list[] = "/usr/bin/env\0/usr/bin/ls\0";
	if (pipe(fds) || write(fds[1], list, sizeof(list) - 1) != (ssize_t)(sizeof(list) - 1))
		return 2;
	close(fds[1]);
	char *tmpdir = load_input(fds[0], "/");
	close(fds[0]);
	put("input_root", listroot);
	char names[256] = "";
	if (tmpdir) {
		DIR *d = opendir(tmpdir);
		struct dirent *e;
		int seen_env = 0, seen_ls = 0;
		while (d && (e = readdir(d))) {
			seen_env |= !strcmp(e->d_name, "env");
			seen_ls |= !strcmp(e->d_name, "ls");
		}
		if (d)
			closedir(d);
		snprintf(names, sizeof(names), "%s%s", seen_env ? "env," : "", seen_ls ? "ls" : "");
		nftw(tmpdir, rm_entry, 16, FTW_DEPTH | FTW_PHYS);
		free(tmpdir);
	}
	put("input_links", names);
	rmdir(base);
	return 0;
}

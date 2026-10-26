/* nnn's NNN_BMS / NNN_PLUG / NNN_ORDER key:value configuration parsing and lookup. */
#include "nnn_harness.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	/* an environment value naming paths; nnn's path buffers are PATH_MAX */
	if (size < 2 || size > 2048)
		return -1;

	nnn_harness_init();

	uchar_t id = (uchar_t[]){NNN_BMS, NNN_PLUG, NNN_ORDER}[data[0] % 3];

	/* putenv() into a static buffer: glibc setenv() allocates a string per call and never frees it */
	static char envstr[64 + 2048];
	int pre = snprintf(envstr, 64, "%s=", env_cfg[id]);
	memcpy(envstr + pre, data + 1, size - 1);
	envstr[pre + size - 1] = '\0';
	if (putenv(envstr))
		return -1;

	kv *arr = NULL;
	char *copy = NULL;
	uchar_t items = 0;
	bool ok = parsekvpair(&arr, &copy, id, &items);

	if (ok) {
		char path[PATH_MAX];

		if (id == NNN_BMS)
			bmstr = copy;
		else if (id == NNN_PLUG)
			pluginstr = copy;
		else
			orderstr = copy;

		for (int i = 0; i < items; ++i) {
			int key = arr[i].key;

			if (id != NNN_ORDER)
				(void)get_kv_val(arr, path, key, items, id);
			else if (copy)
				(void)get_kv_key(arr, copy + arr[i].off, items, id);
		}
		if (id != NNN_ORDER)
			(void)get_kv_val(arr, path, '\x7f', items, id);
	}

	bmstr = pluginstr = orderstr = NULL;
	unsetenv(env_cfg[id]);
	free(arr);
	free(copy);
	return 0;
}

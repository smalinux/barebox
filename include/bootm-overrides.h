/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __BOOTM_OVERRIDES_H
#define __BOOTM_OVERRIDES_H

enum bootm_override {
	BOOTM_OVERRIDE_NONE,
	BOOTM_OVERRIDE_FALSE,
	BOOTM_OVERRIDE_TRUE,
};

struct bootm_overrides {
	const char *os_file;
	const char *oftree_file;
	const char *initrd_file;
	enum bootm_override appendroot;
};

#ifdef CONFIG_BOOT_OVERRIDE
void bootm_set_overrides(const struct bootm_overrides *overrides);
#else
static inline void bootm_set_overrides(const struct bootm_overrides *overrides) {}
#endif

static inline void bootm_merge_overrides(struct bootm_overrides *dst,
					 const struct bootm_overrides *src)
{
	if (!IS_ENABLED(CONFIG_BOOT_OVERRIDE))
		return;
	if (src->os_file)
		dst->os_file = src->os_file;
	if (src->oftree_file)
		dst->oftree_file = src->oftree_file;
	if (src->initrd_file)
		dst->initrd_file = src->initrd_file;
}

#endif

// SPDX-License-Identifier: GPL-2.0-only
/*
 * Generic show_mem() implementation
 *
 * Copyright (C) 2008 Johannes Weiner <hannes@saeurebad.de>
 */

#include <linux/mm.h>
#include <linux/cma.h>
#include <trace/hooks/mm.h>
#ifdef CONFIG_MAS_UNISTORE_PRESERVE
#include <linux/blkdev.h>
#endif

#ifdef CONFIG_HP_CORE
#include <linux/hyperhold_inf.h>
#endif
void show_mem(unsigned int filter, nodemask_t *nodemask)
{
	pg_data_t *pgdat;
	unsigned long total = 0, reserved = 0, highmem = 0;

	printk("Mem-Info:\n");
	show_free_areas(filter, nodemask);

	for_each_online_pgdat(pgdat) {
		int zoneid;

		for (zoneid = 0; zoneid < MAX_NR_ZONES; zoneid++) {
			struct zone *zone = &pgdat->node_zones[zoneid];
			if (!populated_zone(zone))
				continue;

			total += zone->present_pages;
			reserved += zone->present_pages - zone_managed_pages(zone);

			if (is_highmem_idx(zoneid))
				highmem += zone->present_pages;
		}
	}

	printk("%lu pages RAM\n", total);
	printk("%lu pages HighMem/MovableOnly\n", highmem);
	printk("%lu pages reserved\n", reserved);
#ifdef CONFIG_CMA
	printk("%lu pages cma reserved\n", totalcma_pages);
#endif
#ifdef CONFIG_MEMORY_FAILURE
	printk("%lu pages hwpoisoned\n", atomic_long_read(&num_poisoned_pages));
#endif
	trace_android_vh_show_mem(filter, nodemask);
#ifdef CONFIG_MAS_UNISTORE_PRESERVE
	printk("%d anon pages, %d non anon pages, unistore_reset_recovery\n",
		mas_blk_get_recovery_pages(true), mas_blk_get_recovery_pages(false));
#endif
#ifdef CONFIG_HP_CORE
	printk("%lu pages in hp cache\n", get_hyperhold_cache_pages());
#endif
}
EXPORT_SYMBOL_GPL(show_mem);

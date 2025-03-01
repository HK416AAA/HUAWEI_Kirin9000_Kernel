/*
 *
 * (C) COPYRIGHT 2018-2020 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#include <mali_kbase.h>
#include <tl/mali_kbase_tracepoints.h>
#include <mali_kbase_ctx_sched.h>
#include "backend/gpu/mali_kbase_device_internal.h"
#include "mali_kbase_csf.h"
#include <linux/export.h>

#ifdef CONFIG_SYNC_FILE
#include "mali_kbase_fence.h"
#include "mali_kbase_sync.h"

static DEFINE_SPINLOCK(kbase_csf_fence_lock);
#endif

static void kcpu_queue_process(struct kbase_kcpu_command_queue *kcpu_queue,
			bool ignore_waits);

static void kcpu_queue_process_worker(struct work_struct *data);

static int kbase_kcpu_map_import_prepare(
		struct kbase_kcpu_command_queue *kcpu_queue,
		struct base_kcpu_command_import_info *import_info,
		struct kbase_kcpu_command *current_command)
{
	struct kbase_context *const kctx = kcpu_queue->kctx;
	struct kbase_va_region *reg;
	int ret = 0;

	lockdep_assert_held(&kctx->csf.kcpu_queues.lock);

	/* Take the processes mmap lock */
	down_read(&current->mm->mmap_sem);
	kbase_gpu_vm_lock(kctx);

	reg = kbase_region_tracker_find_region_enclosing_address(kctx,
					import_info->handle);

	if (kbase_is_region_invalid_or_free(reg) ||
	    !kbase_mem_is_imported(reg->gpu_alloc->type)) {
		ret = -EINVAL;
		goto out;
	}

	if (reg->gpu_alloc->type == KBASE_MEM_TYPE_IMPORTED_USER_BUF) {
		/* Pin the physical pages backing the user buffer while
		 * we are in the process context and holding the mmap lock.
		 * The dma mapping & GPU mapping of the pages would be done
		 * when the MAP_IMPORT operation is executed.
		 *
		 * Though the pages would be pinned, no reference is taken
		 * on the physical pages tracking object. When the last
		 * reference to the tracking object is dropped the pages
		 * would be unpinned if they weren't unpinned before.
		 */
		ret = kbase_jd_user_buf_pin_pages(kctx, reg);
		if (ret)
			goto out;
	}

	current_command->type = BASE_KCPU_COMMAND_TYPE_MAP_IMPORT;
	current_command->info.import.gpu_va = import_info->handle;

out:
	kbase_gpu_vm_unlock(kctx);
	/* Release the processes mmap lock */
	up_read(&current->mm->mmap_sem);

	return ret;
}

static int kbase_kcpu_unmap_import_prepare_internal(
		struct kbase_kcpu_command_queue *kcpu_queue,
		struct base_kcpu_command_import_info *import_info,
		struct kbase_kcpu_command *current_command,
		enum base_kcpu_command_type type)
{
	struct kbase_context *const kctx = kcpu_queue->kctx;
	struct kbase_va_region *reg;
	int ret = 0;

	lockdep_assert_held(&kctx->csf.kcpu_queues.lock);

	kbase_gpu_vm_lock(kctx);

	reg = kbase_region_tracker_find_region_enclosing_address(kctx,
					import_info->handle);

	if (kbase_is_region_invalid_or_free(reg) ||
	    !kbase_mem_is_imported(reg->gpu_alloc->type)) {
		ret = -EINVAL;
		goto out;
	}

	if (reg->gpu_alloc->type == KBASE_MEM_TYPE_IMPORTED_USER_BUF) {
		/* The pages should have been pinned when MAP_IMPORT
		 * was enqueued previously.
		 */
		if (reg->gpu_alloc->nents !=
		    reg->gpu_alloc->imported.user_buf.nr_pages) {
			ret = -EINVAL;
			goto out;
		}
	}

	current_command->type = type;
	current_command->info.import.gpu_va = import_info->handle;

out:
	kbase_gpu_vm_unlock(kctx);

	return ret;
}

static int kbase_kcpu_unmap_import_prepare(
		struct kbase_kcpu_command_queue *kcpu_queue,
		struct base_kcpu_command_import_info *import_info,
		struct kbase_kcpu_command *current_command)
{
	return kbase_kcpu_unmap_import_prepare_internal(kcpu_queue,
			import_info, current_command,
			BASE_KCPU_COMMAND_TYPE_UNMAP_IMPORT);
}

static int kbase_kcpu_unmap_import_force_prepare(
		struct kbase_kcpu_command_queue *kcpu_queue,
		struct base_kcpu_command_import_info *import_info,
		struct kbase_kcpu_command *current_command)
{
	return kbase_kcpu_unmap_import_prepare_internal(kcpu_queue,
			import_info, current_command,
			BASE_KCPU_COMMAND_TYPE_UNMAP_IMPORT_FORCE);
}

static int kbase_kcpu_debug_copy_prepare(
		struct kbase_kcpu_command_queue *kcpu_queue,
		struct base_kcpu_command_debug_copy_info *debug_copy,
		struct kbase_kcpu_command *current_command)
{
	struct kbase_context *const kctx = kcpu_queue->kctx;
	void __user *user_structs = u64_to_user_ptr(debug_copy->buffers);
	struct base_jd_debug_copy_buffer *user_buffers = NULL;
	struct kbase_debug_copy_buffer *buffers = NULL;
	unsigned int i;
	int ret;

	lockdep_assert_held(&kctx->csf.kcpu_queues.lock);

	buffers = kcalloc(debug_copy->nr,
			sizeof(struct kbase_debug_copy_buffer), GFP_KERNEL);
	if (!buffers) {
		ret = -ENOMEM;
		goto out_cleanup;
	}

	user_buffers = kmalloc_array(debug_copy->nr,
			sizeof(*user_buffers), GFP_KERNEL);
	if (!user_buffers) {
		ret = -ENOMEM;
		goto out_cleanup;
	}

	ret = copy_from_user(user_buffers, user_structs,
			sizeof(*user_buffers) * debug_copy->nr);
	if (ret) {
		ret = -EFAULT;
		goto out_cleanup;
	}

	for (i = 0; i < debug_copy->nr; i++) {
		u64 addr = user_buffers[i].address;
		u64 page_addr = addr & PAGE_MASK;
		u64 end_addr = addr + user_buffers[i].size - 1;
		u64 last_page_addr = end_addr & PAGE_MASK;
		int nr_pages = (last_page_addr - page_addr) / PAGE_SIZE + 1;
		int pinned_pages;
		struct kbase_va_region *reg;
		struct base_external_resource user_extres;

		if (!addr)
			continue;

		buffers[i].nr_pages = nr_pages;
		buffers[i].offset = addr & ~PAGE_MASK;
		if (buffers[i].offset >= PAGE_SIZE) {
			ret = -EINVAL;
			goto out_cleanup;
		}
		buffers[i].size = user_buffers[i].size;

		buffers[i].pages = kcalloc(nr_pages, sizeof(struct page *),
				GFP_KERNEL);
		if (!buffers[i].pages) {
			ret = -ENOMEM;
			goto out_cleanup;
		}

		pinned_pages = get_user_pages_fast(page_addr, nr_pages, 1,
					buffers[i].pages);
		if (pinned_pages < 0) {
			ret = pinned_pages;
			goto out_cleanup;
		}
		if (pinned_pages != nr_pages) {
			ret = -EINVAL;
			goto out_cleanup;
		}

		user_extres = user_buffers[i].extres;
		if (user_extres.ext_resource == 0ULL) {
			ret = -EINVAL;
			goto out_cleanup;
		}

		kbase_gpu_vm_lock(kctx);
		reg = kbase_region_tracker_find_region_enclosing_address(
				kcpu_queue->kctx, user_extres.ext_resource &
				~BASE_EXT_RES_ACCESS_EXCLUSIVE);

		if (kbase_is_region_invalid_or_free(reg) ||
		    reg->gpu_alloc == NULL) {
			ret = -EINVAL;
			goto out_unlock;
		}

		buffers[i].gpu_alloc = kbase_mem_phy_alloc_get(reg->gpu_alloc);
		buffers[i].nr_extres_pages = reg->nr_pages;

		if (reg->nr_pages * PAGE_SIZE != buffers[i].size)
			dev_warn(kctx->kbdev->dev, "Copy buffer is not of the same size as the external resource to copy\n");

		switch (reg->gpu_alloc->type) {
		case KBASE_MEM_TYPE_IMPORTED_USER_BUF:
		{
			struct kbase_mem_phy_alloc *alloc = reg->gpu_alloc;
			unsigned int nr_pages =
				alloc->imported.user_buf.nr_pages;

			if (alloc->imported.user_buf.mm != current->mm) {
				ret = -EINVAL;
				goto out_unlock;
			}
			buffers[i].extres_pages = kcalloc(nr_pages,
					sizeof(struct page *), GFP_KERNEL);
			if (!buffers[i].extres_pages) {
				ret = -ENOMEM;
				goto out_unlock;
			}

			ret = get_user_pages_fast(
					alloc->imported.user_buf.address,
					nr_pages, 0,
					buffers[i].extres_pages);
			if (ret != nr_pages)
				goto out_unlock;
			ret = 0;
			break;
		}
		default:
			break;
		}
		kbase_gpu_vm_unlock(kctx);
	}

	current_command->type = BASE_KCPU_COMMAND_TYPE_DEBUG_COPY;
	current_command->info.debug_copy.buffers = buffers;
	current_command->info.debug_copy.nr = debug_copy->nr;

	kfree(user_buffers);

	return ret;

out_unlock:
	kbase_gpu_vm_unlock(kctx);
out_cleanup:
	kfree(user_buffers);
	kfree(buffers);

	return ret;
}

static int kbase_kcpu_debug_copy(struct kbase_context *kctx,
		struct kbase_kcpu_command_debug_copy_info *debug_copy)
{
	struct kbase_debug_copy_buffer *buffers = debug_copy->buffers;
	unsigned int i;
	int ret = 0;

	lockdep_assert_held(&kctx->csf.kcpu_queues.lock);

	if (WARN_ON(!buffers))
		return -EINVAL;

	for (i = 0; i < debug_copy->nr; i++) {
		int res = kbase_mem_copy_from_extres(kctx, &buffers[i]);

		if (res) {
			ret = res;
			goto finish;
		}
	}
finish:
	kbase_gpu_vm_lock(kctx);

	for (i = 0; i < debug_copy->nr; i++) {
		unsigned int p;
		struct kbase_mem_phy_alloc *gpu_alloc =
				debug_copy->buffers[i].gpu_alloc;

		if (!debug_copy->buffers[i].pages)
			break;
		for (p = 0; p < debug_copy->buffers[i].nr_pages; p++) {
			struct page *pg = debug_copy->buffers[i].pages[p];

			if (pg)
				put_page(pg);
		}

		kfree(debug_copy->buffers[i].pages);

		if (gpu_alloc) {
			switch (gpu_alloc->type) {
			case KBASE_MEM_TYPE_IMPORTED_USER_BUF:
				kbase_free_user_buffer(&debug_copy->buffers[i]);
			default:
				/* Nothing to be done. */
				break;
			}

			kbase_mem_phy_alloc_put(gpu_alloc);
		}
	}

	kbase_gpu_vm_unlock(kctx);

	kfree(debug_copy->buffers);

	return ret;
}

static int kbase_kcpu_jit_allocate_process(struct kbase_context *kctx,
			struct kbase_kcpu_command_jit_alloc_info *alloc_info)
{
	struct base_jit_alloc_info *info = alloc_info->info;
	struct kbase_vmap_struct mapping;
	struct kbase_va_region *reg;
	u32 count = alloc_info->count;
	u64 *ptr, new_addr;
	u32 i;
	int ret;

	lockdep_assert_held(&kctx->csf.kcpu_queues.lock);

	if (WARN_ON(!info))
		return -EINVAL;

	/* Check if all JIT IDs are not in use */
	for (i = 0; i < count; i++, info++) {
		/* The JIT ID is still in use so fail the allocation */
		if (kctx->jit_alloc[info->id]) {
			dev_warn(kctx->kbdev->dev, "JIT ID still in use\n");
			return -EINVAL;
		}
	}

	/* Now start the allocation loop */
	for (i = 0, info = alloc_info->info; i < count; i++, info++) {
		if (kctx->jit_alloc[info->id]) {
			/* The JIT ID is duplicated in this command. Roll back
			 * previous allocations and fail.
			 */
			dev_warn(kctx->kbdev->dev, "JIT ID is duplicated\n");
			ret = -EINVAL;
			goto fail;
		}

		/* Create a JIT allocation */
		reg = kbase_jit_allocate(kctx, info);
		if (!reg) {
			ret = -ENOMEM;
			goto fail;
		}

		/* Bind it to the user provided ID. */
		kctx->jit_alloc[info->id] = reg;
	}

	for (i = 0, info = alloc_info->info; i < count; i++, info++) {
		/*
		 * Write the address of the JIT allocation to the user provided
		 * GPU allocation.
		 */
		ptr = kbase_vmap(kctx, info->gpu_alloc_addr, sizeof(*ptr),
				&mapping);
		if (!ptr) {
			ret = -ENOMEM;
			goto fail;
		}

		reg = kctx->jit_alloc[info->id];
		new_addr = reg->start_pfn << PAGE_SHIFT;
		*ptr = new_addr;
		kbase_vunmap(kctx, &mapping);
	}

	return 0;

fail:
	/* Roll back completely */
	for (i = 0, info = alloc_info->info; i < count; i++, info++) {
		/* Free the allocations that were successful.
		 * Mark all the allocations including the failed one and the
		 * other un-attempted allocations in the set, so we know they
		 * are in use.
		 */
		if (kctx->jit_alloc[info->id])
			kbase_jit_free(kctx, kctx->jit_alloc[info->id]);

		kctx->jit_alloc[info->id] = KBASE_RESERVED_REG_JIT_ALLOC;
	}

	return ret;
}

static int kbase_kcpu_jit_allocate_prepare(
		struct kbase_kcpu_command_queue *kcpu_queue,
		struct base_kcpu_command_jit_alloc_info *alloc_info,
		struct kbase_kcpu_command *current_command)
{
	struct kbase_context *const kctx = kcpu_queue->kctx;
	void __user *data = u64_to_user_ptr(alloc_info->info);
	struct base_jit_alloc_info *info;
	u32 count = alloc_info->count;
	int ret = 0;
	u32 i;

	lockdep_assert_held(&kctx->csf.kcpu_queues.lock);

	if (!data || count > kcpu_queue->kctx->jit_max_allocations ||
			count > ARRAY_SIZE(kctx->jit_alloc)) {
		ret = -EINVAL;
		goto out;
	}

	info = kmalloc_array(count, sizeof(*info), GFP_KERNEL);
	if (!info) {
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(info, data, sizeof(*info) * count) != 0) {
		ret = -EINVAL;
		goto out_free;
	}

	for (i = 0; i < count; i++) {
		ret = kbasep_jit_alloc_validate(kctx, &info[i]);
		if (ret)
			goto out_free;
	}

	current_command->type = BASE_KCPU_COMMAND_TYPE_JIT_ALLOC;
	current_command->info.jit_alloc.info = info;
	current_command->info.jit_alloc.count = count;

	return 0;
out_free:
	kfree(info);
out:
	return ret;
}

static int kbase_kcpu_jit_free_process(struct kbase_context *kctx,
			struct kbase_kcpu_command_jit_free_info *free_info)
{
	u8 *ids = free_info->ids;
	u32 count = free_info->count;
	u32 i;

	if (WARN_ON(!ids))
		return -EINVAL;

	lockdep_assert_held(&kctx->csf.kcpu_queues.lock);

	for (i = 0; i < count; i++, ids++) {
		if ((*ids == 0) || (kctx->jit_alloc[*ids] == NULL)) {
			dev_warn(kctx->kbdev->dev, "invalid JIT free ID\n");
		} else {
			/* If the ID is valid but the allocation request
			 * failed, still succeed this command but don't
			 * try and free the allocation.
			 */
			if (kctx->jit_alloc[*ids] !=
					KBASE_RESERVED_REG_JIT_ALLOC)
				kbase_jit_free(kctx, kctx->jit_alloc[*ids]);

			kctx->jit_alloc[*ids] = NULL;
		}
	}

	/* Free the list of ids */
	kfree(free_info->ids);

	return 0;
}

static int kbase_kcpu_jit_free_prepare(
		struct kbase_kcpu_command_queue *kcpu_queue,
		struct base_kcpu_command_jit_free_info *free_info,
		struct kbase_kcpu_command *current_command)
{
	struct kbase_context *const kctx = kcpu_queue->kctx;
	void __user *data = u64_to_user_ptr(free_info->ids);
	u8 *ids;
	u32 count = free_info->count;
	int ret;
	u32 i;

	lockdep_assert_held(&kctx->csf.kcpu_queues.lock);

	/* Sanity checks */
	if (!count || count > ARRAY_SIZE(kctx->jit_alloc)) {
		ret = -EINVAL;
		goto out;
	}

	/* Copy the information for safe access and future storage */
	ids = kmalloc_array(count, sizeof(*ids), GFP_KERNEL);
	if (!ids) {
		ret = -ENOMEM;
		goto out;
	}

	if (!data) {
		ret = -EINVAL;
		goto out_free;
	}

	if (copy_from_user(ids, data, sizeof(*ids) * count)) {
		ret = -EINVAL;
		goto out_free;
	}

	for (i = 0; i < count; i++) {
		/* Fail the command if ID sent is zero */
		if (!ids[i]) {
			ret = -EINVAL;
			goto out_free;
		}
	}

	current_command->type = BASE_KCPU_COMMAND_TYPE_JIT_FREE;
	current_command->info.jit_free.ids = ids;
	current_command->info.jit_free.count = count;

	return 0;
out_free:
	kfree(ids);
out:
	return ret;
}

static int kbase_csf_queue_group_suspend_prepare(
		struct kbase_kcpu_command_queue *kcpu_queue,
		struct base_kcpu_command_group_suspend_info *suspend_buf,
		struct kbase_kcpu_command *current_command)
{
	struct kbase_context *const kctx = kcpu_queue->kctx;
	struct kbase_suspend_copy_buffer *sus_buf = NULL;
	u64 addr = suspend_buf->buffer;
	u64 page_addr = addr & PAGE_MASK;
	u64 end_addr = addr + suspend_buf->size - 1;
	u64 last_page_addr = end_addr & PAGE_MASK;
	int nr_pages = (last_page_addr - page_addr) / PAGE_SIZE + 1;
	int pinned_pages;
	int ret = 0;

	lockdep_assert_held(&kctx->csf.kcpu_queues.lock);

	if (suspend_buf->size <
			kctx->kbdev->csf.global_iface.groups[0].suspend_size)
		return -EINVAL;

	ret = kbase_csf_queue_group_handle_is_valid(kctx,
			suspend_buf->group_handle);
	if (ret)
		return ret;

	sus_buf = kzalloc(sizeof(*sus_buf), GFP_KERNEL);
	if (!sus_buf)
		return -ENOMEM;

	sus_buf->size = suspend_buf->size;
	sus_buf->nr_pages = nr_pages;
	sus_buf->offset = addr & ~PAGE_MASK;

	sus_buf->pages = kcalloc(nr_pages, sizeof(struct page *), GFP_KERNEL);
	if (!sus_buf->pages) {
		ret = -ENOMEM;
		goto out_clean_sus_buf;
	}

	pinned_pages = get_user_pages_fast(page_addr, nr_pages, 1,
			sus_buf->pages);
	if (pinned_pages < 0) {
		ret = pinned_pages;
		goto out_clean_pages;
	}
	if (pinned_pages != nr_pages) {
		ret = -EINVAL;
		goto out_clean_pages;
	}

	current_command->type = BASE_KCPU_COMMAND_TYPE_GROUP_SUSPEND;
	current_command->info.suspend_buf_copy.sus_buf = sus_buf;
	current_command->info.suspend_buf_copy.group_handle =
				suspend_buf->group_handle;
	return ret;

out_clean_pages:
	kfree(sus_buf->pages);
out_clean_sus_buf:
	kfree(sus_buf);
	return ret;
}

static int kbase_csf_queue_group_suspend_process(struct kbase_context *kctx,
		struct kbase_suspend_copy_buffer *sus_buf,
		u8 group_handle)
{
	return kbase_csf_queue_group_suspend(kctx, sus_buf, group_handle);
}

static enum kbase_csf_event_callback_action event_cqs_callback(void *param)
{
	struct kbase_kcpu_command_queue *kcpu_queue =
		(struct kbase_kcpu_command_queue *)param;
	struct kbase_context *const kctx = kcpu_queue->kctx;

	queue_work(kctx->csf.kcpu_queues.wq, &kcpu_queue->work);

	return KBASE_CSF_EVENT_CALLBACK_KEEP;
}

static void cleanup_cqs_wait(struct kbase_kcpu_command_queue *queue,
		struct kbase_kcpu_command_cqs_wait_info *cqs_wait)
{
	WARN_ON(!cqs_wait->nr_objs);
	WARN_ON(!cqs_wait->objs);
	WARN_ON(!queue->cqs_wait_count);

	if (--queue->cqs_wait_count == 0) {
		kbase_csf_event_wait_remove(queue->kctx,
				event_cqs_callback, queue);
	}

	kfree(cqs_wait->objs);
	cqs_wait->objs = NULL;
}

static int kbase_kcpu_cqs_wait_process(struct kbase_kcpu_command_queue *queue,
			struct kbase_kcpu_command_cqs_wait_info *cqs_wait)
{
	bool signaled = true;
	unsigned int i;

	lockdep_assert_held(&queue->kctx->csf.kcpu_queues.lock);

	if (WARN_ON(!cqs_wait->nr_objs))
		return -EINVAL;

	if (WARN_ON(!cqs_wait->objs))
		return -EINVAL;

	/* For the queue to progress further, all cqs objects should get
	 * signaled.
	 */
	for (i = 0; (i < cqs_wait->nr_objs) && signaled; i++) {
		struct kbase_vmap_struct *mapping;
		void *cpu_ptr = kbase_phy_alloc_mapping_get(queue->kctx,
					cqs_wait->objs[i].addr, &mapping);

		if (!cpu_ptr)
			return -EINVAL;

		signaled &= (*(u32 *)cpu_ptr > cqs_wait->objs[i].val);

		kbase_phy_alloc_mapping_put(queue->kctx, mapping);
	}

	return signaled;
}

static int kbase_kcpu_cqs_wait_prepare(struct kbase_kcpu_command_queue *queue,
		struct base_kcpu_command_cqs_wait_info *cqs_wait_info,
		struct kbase_kcpu_command *current_command)
{
	struct base_cqs_wait *objs;
	unsigned int nr_objs = cqs_wait_info->nr_objs;

	lockdep_assert_held(&queue->kctx->csf.kcpu_queues.lock);

	objs = kcalloc(nr_objs, sizeof(*objs), GFP_KERNEL);
	if (!objs)
		return -ENOMEM;

	if (copy_from_user(objs, u64_to_user_ptr(cqs_wait_info->objs),
			nr_objs * sizeof(*objs))) {
		kfree(objs);
		return -ENOMEM;
	}

	if (++queue->cqs_wait_count == 1) {
		if (kbase_csf_event_wait_add(queue->kctx,
				event_cqs_callback, queue)) {
			kfree(objs);
			return -ENOMEM;
		}
	}

	current_command->type = BASE_KCPU_COMMAND_TYPE_CQS_WAIT;
	current_command->info.cqs_wait.nr_objs = nr_objs;
	current_command->info.cqs_wait.objs = objs;

	return 0;
}

static void kbase_kcpu_cqs_set_process(struct kbase_kcpu_command_queue *queue,
		struct kbase_kcpu_command_cqs_set_info *cqs_set)
{
	unsigned int i;

	lockdep_assert_held(&queue->kctx->csf.kcpu_queues.lock);

	WARN_ON(!cqs_set->nr_objs);
	WARN_ON(!cqs_set->objs);

	for (i = 0; i < cqs_set->nr_objs; i++) {
		struct kbase_vmap_struct *mapping;
		void *cpu_ptr = kbase_phy_alloc_mapping_get(queue->kctx,
					cqs_set->objs[i].addr, &mapping);

		if (!cpu_ptr)
			goto out;

		(*(u32 *)cpu_ptr)++;

		kbase_phy_alloc_mapping_put(queue->kctx, mapping);
	}

	kbase_csf_event_signal_notify_gpu(queue->kctx);

out:
	kfree(cqs_set->objs);
	cqs_set->objs = NULL;
}

static int kbase_kcpu_cqs_set_prepare(
		struct kbase_kcpu_command_queue *kcpu_queue,
		struct base_kcpu_command_cqs_set_info *cqs_set_info,
		struct kbase_kcpu_command *current_command)
{
	struct kbase_context *const kctx = kcpu_queue->kctx;
	struct base_cqs_set *objs;
	unsigned int nr_objs = cqs_set_info->nr_objs;

	lockdep_assert_held(&kctx->csf.kcpu_queues.lock);

	objs = kcalloc(nr_objs, sizeof(*objs), GFP_KERNEL);
	if (!objs)
		return -ENOMEM;

	if (copy_from_user(objs, u64_to_user_ptr(cqs_set_info->objs),
			nr_objs * sizeof(*objs))) {
		kfree(objs);
		return -ENOMEM;
	}

	current_command->type = BASE_KCPU_COMMAND_TYPE_CQS_SET;
	current_command->info.cqs_set.nr_objs = nr_objs;
	current_command->info.cqs_set.objs = objs;

	return 0;
}

#ifdef CONFIG_SYNC_FILE
#if (KERNEL_VERSION(4, 10, 0) > LINUX_VERSION_CODE)
static void kbase_csf_fence_wait_callback(struct fence *fence,
			struct fence_cb *cb)
#else
static void kbase_csf_fence_wait_callback(struct dma_fence *fence,
			struct dma_fence_cb *cb)
#endif
{
	struct kbase_kcpu_command_fence_info *fence_info = container_of(cb,
			struct kbase_kcpu_command_fence_info, fence_cb);
	struct kbase_kcpu_command_queue *kcpu_queue = fence_info->kcpu_queue;
	struct kbase_context *const kctx = kcpu_queue->kctx;

	kcpu_queue->fence_signaled = true;

	/* Resume kcpu command queue processing. */
	queue_work(kctx->csf.kcpu_queues.wq, &kcpu_queue->work);
}

static void kbase_kcpu_fence_wait_cancel(
		struct kbase_kcpu_command_queue *kcpu_queue,
		struct kbase_kcpu_command_fence_info *fence_info)
{
	struct kbase_context *const kctx = kcpu_queue->kctx;

	lockdep_assert_held(&kctx->csf.kcpu_queues.lock);

	if (WARN_ON(!fence_info->fence))
		return;

	if (kcpu_queue->fence_wait_processed) {
		dma_fence_remove_callback(fence_info->fence,
				&fence_info->fence_cb);
	}

	/* Release the reference which is kept by the kcpu_queue */
	kbase_fence_put(fence_info->fence);
	kcpu_queue->fence_wait_processed = false;

	fence_info->fence = NULL;
}

static void kbase_kcpu_fence_wait_finish(
		struct kbase_kcpu_command_queue *kcpu_queue,
		struct kbase_kcpu_command_fence_info *fence_info)
{
	struct kbase_context *const kctx = kcpu_queue->kctx;

	lockdep_assert_held(&kctx->csf.kcpu_queues.lock);

	if (WARN_ON(!fence_info->fence))
		return;

	if (WARN_ON(!kcpu_queue->fence_signaled))
		return;

	/* Release the reference which is kept by the kcpu_queue */
	kbase_fence_put(fence_info->fence);
	kcpu_queue->fence_wait_processed = false;

	fence_info->fence = NULL;
}

static int kbase_kcpu_fence_wait_process(
		struct kbase_kcpu_command_queue *kcpu_queue,
		struct kbase_kcpu_command_fence_info *fence_info)
{
	int ret = 0;
#if (KERNEL_VERSION(4, 10, 0) > LINUX_VERSION_CODE)
	struct fence *fence;
#else
	struct dma_fence *fence;
#endif

	if (WARN_ON(!fence_info->fence))
		return -EINVAL;

	if (WARN_ON(kcpu_queue->fence_wait_processed))
		return -EINVAL;

	if (WARN_ON(!kcpu_queue->fence_signaled))
		return -EINVAL;

	fence = fence_info->fence;

	kcpu_queue->fence_signaled = false;
	ret = dma_fence_add_callback(fence, &fence_info->fence_cb,
				kbase_csf_fence_wait_callback);
	if (ret == -ENOENT) {
		/* Fence signaled, get the completion result */
		ret = dma_fence_get_status(fence);

		/* remap success completion to err code */
		if (ret == 1)
			ret = 0;

		kcpu_queue->fence_signaled = true;
	} else if (ret) {
		kcpu_queue->fence_signaled = true;
	} else {
		kcpu_queue->fence_wait_processed = true;
	}

	return ret;
}

static int kbase_kcpu_fence_wait_prepare(
		struct kbase_kcpu_command_queue *kcpu_queue,
		struct base_kcpu_command_fence_info *fence_info,
		struct kbase_kcpu_command *current_command)
{
	struct kbase_context *const kctx = kcpu_queue->kctx;
#if (KERNEL_VERSION(4, 10, 0) > LINUX_VERSION_CODE)
	struct fence *fence_in;
#else
	struct dma_fence *fence_in;
#endif
	struct base_fence fence;

	lockdep_assert_held(&kctx->csf.kcpu_queues.lock);

	if (copy_from_user(&fence, u64_to_user_ptr(fence_info->fence),
			sizeof(fence)))
		return -ENOMEM;

	fence_in = sync_file_get_fence(fence.basep.fd);

	if (!fence_in)
		return -ENOENT;

	current_command->type = BASE_KCPU_COMMAND_TYPE_FENCE_WAIT;
	current_command->info.fence.fence = fence_in;
	current_command->info.fence.kcpu_queue = kcpu_queue;

	return 0;
}

static int kbase_kcpu_fence_signal_process(
		struct kbase_kcpu_command_queue *kcpu_queue,
		struct kbase_kcpu_command_fence_info *fence_info)
{
	struct kbase_context *const kctx = kcpu_queue->kctx;
	int ret;

	if (WARN_ON(!fence_info->fence))
		return -EINVAL;

	ret = dma_fence_signal(fence_info->fence);

	if (unlikely(ret < 0)) {
		dev_warn(kctx->kbdev->dev,
			"fence_signal() failed with %d\n", ret);
	}

	dma_fence_put(fence_info->fence);
	fence_info->fence = NULL;

	return ret;
}

static int kbase_kcpu_fence_signal_prepare(
		struct kbase_kcpu_command_queue *kcpu_queue,
		struct base_kcpu_command_fence_info *fence_info,
		struct kbase_kcpu_command *current_command)
{
	struct kbase_context *const kctx = kcpu_queue->kctx;
#if (KERNEL_VERSION(4, 10, 0) > LINUX_VERSION_CODE)
	struct fence *fence_out;
#else
	struct dma_fence *fence_out;
#endif
	struct base_fence fence;
	struct sync_file *sync_file;
	int ret = 0;
	int fd;

	lockdep_assert_held(&kctx->csf.kcpu_queues.lock);

	if (copy_from_user(&fence, u64_to_user_ptr(fence_info->fence),
			sizeof(fence)))
		return -EFAULT;

	fence_out = kzalloc(sizeof(*fence_out), GFP_KERNEL);
	if (!fence_out)
		return -ENOMEM;

	dma_fence_init(fence_out,
		       &kbase_fence_ops,
		       &kbase_csf_fence_lock,
		       kcpu_queue->fence_context,
		       ++kcpu_queue->fence_seqno);

#if (KERNEL_VERSION(4, 9, 67) >= LINUX_VERSION_CODE)
	/* Take an extra reference to the fence on behalf of the sync file.
	 * This is only needded on older kernels where sync_file_create()
	 * does not take its own reference. This was changed in v4.9.68
	 * where sync_file_create() now takes its own reference.
	 */
	dma_fence_get(fence_out);
#endif

	/* create a sync_file fd representing the fence */
	sync_file = sync_file_create(fence_out);
	if (!sync_file) {
#if (KERNEL_VERSION(4, 9, 67) >= LINUX_VERSION_CODE)
		dma_fence_put(fence_out);
#endif
		ret = -ENOMEM;
		goto file_create_fail;
	}

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
		ret = fd;
		goto fd_flags_fail;
	}

	fd_install(fd, sync_file->file);

	fence.basep.fd = fd;

	current_command->type = BASE_KCPU_COMMAND_TYPE_FENCE_SIGNAL;
	current_command->info.fence.fence = fence_out;

	if (copy_to_user(u64_to_user_ptr(fence_info->fence), &fence,
			sizeof(fence))) {
		ret = -EFAULT;
		goto fd_flags_fail;
	}

	return 0;

fd_flags_fail:
	fput(sync_file->file);
file_create_fail:
	dma_fence_put(fence_out);

	return ret;
}
#endif /* CONFIG_SYNC_FILE */

static void kcpu_queue_process_worker(struct work_struct *data)
{
	struct kbase_kcpu_command_queue *queue = container_of(data,
				struct kbase_kcpu_command_queue, work);

	mutex_lock(&queue->kctx->csf.kcpu_queues.lock);

	kcpu_queue_process(queue, false);

	mutex_unlock(&queue->kctx->csf.kcpu_queues.lock);
}

static int delete_queue(struct kbase_context *kctx, u32 id)
{
	int err = 0;

	mutex_lock(&kctx->csf.kcpu_queues.lock);

	if ((id < KBASEP_MAX_KCPU_QUEUES) && kctx->csf.kcpu_queues.array[id]) {
		struct kbase_kcpu_command_queue *queue =
					kctx->csf.kcpu_queues.array[id];

		/* Drain the remaining work for this queue first and go past
		 * all the waits.
		 */
		kcpu_queue_process(queue, true);

		/* All commands should have been processed */
		WARN_ON(queue->num_pending_cmds);

		/* All CQS wait commands should have been cleaned up */
		WARN_ON(queue->cqs_wait_count);

		kctx->csf.kcpu_queues.array[id] = NULL;
		bitmap_clear(kctx->csf.kcpu_queues.in_use, id, 1);

		/* Fire the tracepoint with the mutex held to enforce correct
		 * ordering with the summary stream.
		 */
		KBASE_TLSTREAM_TL_KBASE_DEL_KCPUQUEUE(kctx->kbdev, queue);

		mutex_unlock(&kctx->csf.kcpu_queues.lock);

		cancel_work_sync(&queue->work);

		kfree(queue);
	} else {
		dev_warn(kctx->kbdev->dev,
			"Attempt to delete a non-existent KCPU queue\n");
		mutex_unlock(&kctx->csf.kcpu_queues.lock);
		err = -EINVAL;
	}
	return err;
}

static void KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_EXECUTE_JIT_ALLOC_INFO(
	struct kbase_device *kbdev,
	const struct kbase_kcpu_command_queue *queue,
	const struct kbase_kcpu_command_jit_alloc_info *jit_alloc,
	bool alloc_success)
{
	u8 i;

	KBASE_TLSTREAM_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_EXECUTE_JIT_ALLOC_END(
		kbdev, queue);
	for (i = 0; i < jit_alloc->count; i++) {
		const u8 id = jit_alloc->info[i].id;
		const struct kbase_va_region *reg = queue->kctx->jit_alloc[id];
		u64 gpu_alloc_addr = 0;
		u64 mmu_flags = 0;

		if (alloc_success && !WARN_ON(!reg) &&
			!WARN_ON(reg == KBASE_RESERVED_REG_JIT_ALLOC)) {
#ifdef CONFIG_MALI_VECTOR_DUMP
			struct tagged_addr phy = {0};
#endif /* CONFIG_MALI_VECTOR_DUMP */

			gpu_alloc_addr = reg->start_pfn << PAGE_SHIFT;
#ifdef CONFIG_MALI_VECTOR_DUMP
			mmu_flags = kbase_mmu_create_ate(kbdev,
				phy, reg->flags,
				MIDGARD_MMU_BOTTOMLEVEL,
				queue->kctx->jit_group_id);
#endif /* CONFIG_MALI_VECTOR_DUMP */
		}
		KBASE_TLSTREAM_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_EXECUTE_JIT_ALLOC_END(
			kbdev, queue, gpu_alloc_addr, mmu_flags);
	}
}

static void KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_EXECUTE_JIT_ALLOC_END(
	struct kbase_device *kbdev,
	const struct kbase_kcpu_command_queue *queue)
{
	KBASE_TLSTREAM_TL_KBASE_ARRAY_END_KCPUQUEUE_EXECUTE_JIT_ALLOC_END(
		kbdev, queue);
}

static void KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_EXECUTE_JIT_FREE_INFO(
	struct kbase_device *kbdev,
	const struct kbase_kcpu_command_queue *queue,
	const struct kbase_kcpu_command_jit_free_info *jit_free)
{
	u8 i;

	KBASE_TLSTREAM_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_EXECUTE_JIT_FREE_END(
		kbdev, queue);
	for (i = 0; i < jit_free->count; i++) {
		const u8 id = jit_free->ids[i];
		u64 pages_used = 0;

		if (id != 0) {
			const struct kbase_va_region *reg =
				queue->kctx->jit_alloc[id];
			if (reg && (reg != KBASE_RESERVED_REG_JIT_ALLOC))
				pages_used = reg->gpu_alloc->nents;
		}
		KBASE_TLSTREAM_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_EXECUTE_JIT_FREE_END(
			kbdev, queue, pages_used);
	}
}

static void KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_EXECUTE_JIT_FREE_END(
	struct kbase_device *kbdev,
	const struct kbase_kcpu_command_queue *queue)
{
	KBASE_TLSTREAM_TL_KBASE_ARRAY_END_KCPUQUEUE_EXECUTE_JIT_FREE_END(
		kbdev, queue);
}

static void kcpu_queue_process(struct kbase_kcpu_command_queue *queue,
			bool ignore_waits)
{
	struct kbase_device *kbdev = queue->kctx->kbdev;
	bool process_next = true;
	size_t i;

	lockdep_assert_held(&queue->kctx->csf.kcpu_queues.lock);

	for (i = 0; i != queue->num_pending_cmds; ++i) {
		struct kbase_kcpu_command *cmd =
			&queue->commands[(u8)(queue->start_offset + i)];
		int status;

		switch (cmd->type) {
		case BASE_KCPU_COMMAND_TYPE_FENCE_WAIT:
			if (!queue->command_started) {
				KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_EXECUTE_FENCE_WAIT_START(
					kbdev, queue);
				queue->command_started = true;
			}

#ifdef CONFIG_SYNC_FILE
			status = 0;

			/* Check if this fence wait command has already been
			 * processed.
			 */
			if (!queue->fence_wait_processed) {
				status = kbase_kcpu_fence_wait_process(queue,
							&cmd->info.fence);
			}

			/* TBD: error handling */

			/* Check if the fence has been signaled. */
			if (queue->fence_signaled) {
				kbase_kcpu_fence_wait_finish(queue,
							&cmd->info.fence);
			} else if (ignore_waits) {
				kbase_kcpu_fence_wait_cancel(queue,
							&cmd->info.fence);
			} else {
				process_next = false;
			}
#else
			dev_warn(kbdev->dev,
				"unexpected fence wait command found\n");
#endif

			if (process_next) {
				KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_EXECUTE_FENCE_WAIT_END(
					kbdev, queue);
				queue->command_started = false;
			}
			break;
		case BASE_KCPU_COMMAND_TYPE_FENCE_SIGNAL:
			KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_EXECUTE_FENCE_SIGNAL_START(
				kbdev, queue);

#ifdef CONFIG_SYNC_FILE
			kbase_kcpu_fence_signal_process(queue,
						&cmd->info.fence);
#else
			dev_warn(kbdev->dev,
				"unexpected fence signal command found\n");
#endif

			KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_EXECUTE_FENCE_SIGNAL_END(
				kbdev, queue);
			break;
		case BASE_KCPU_COMMAND_TYPE_CQS_WAIT:
			if (!queue->command_started) {
				KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_EXECUTE_CQS_WAIT_START(
					kbdev, queue);
				queue->command_started = true;
			}

			status = kbase_kcpu_cqs_wait_process(queue,
						&cmd->info.cqs_wait);

			if (!status && !ignore_waits) {
				process_next = false;
			} else {
				/* Either all CQS objects were signaled or
				 * there was an error or the queue itself is
				 * being deleted.
				 * In all cases can move to the next command.
				 * TBD: handle the error
				 */
				cleanup_cqs_wait(queue,	&cmd->info.cqs_wait);
			}

			if (process_next) {
				KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_EXECUTE_CQS_WAIT_END(
					kbdev, queue);
				queue->command_started = false;
			}
			break;
		case BASE_KCPU_COMMAND_TYPE_CQS_SET:
			KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_EXECUTE_CQS_SET(
				kbdev, queue);

			kbase_kcpu_cqs_set_process(queue, &cmd->info.cqs_set);

			/* CQS sets are only traced before execution */
			break;
		case BASE_KCPU_COMMAND_TYPE_MAP_IMPORT:
			KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_EXECUTE_MAP_IMPORT_START(
				kbdev, queue);

			kbase_gpu_vm_lock(queue->kctx);
			kbase_sticky_resource_acquire(queue->kctx,
						cmd->info.import.gpu_va);
			kbase_gpu_vm_unlock(queue->kctx);

			KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_EXECUTE_MAP_IMPORT_END(
				kbdev, queue);
			break;
		case BASE_KCPU_COMMAND_TYPE_UNMAP_IMPORT:
			KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_EXECUTE_UNMAP_IMPORT_START(
				kbdev, queue);

			kbase_gpu_vm_lock(queue->kctx);
			kbase_sticky_resource_release(queue->kctx, NULL,
						cmd->info.import.gpu_va);
			kbase_gpu_vm_unlock(queue->kctx);

			KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_EXECUTE_UNMAP_IMPORT_END(
				kbdev, queue);
			break;
		case BASE_KCPU_COMMAND_TYPE_UNMAP_IMPORT_FORCE:
			KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_EXECUTE_UNMAP_IMPORT_FORCE_START(
					kbdev, queue);

			kbase_gpu_vm_lock(queue->kctx);
			kbase_sticky_resource_release_force(queue->kctx, NULL,
						cmd->info.import.gpu_va);
			kbase_gpu_vm_unlock(queue->kctx);

			KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_EXECUTE_UNMAP_IMPORT_FORCE_END(
					kbdev, queue);
			break;
		case BASE_KCPU_COMMAND_TYPE_DEBUG_COPY:
			KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_EXECUTE_DEBUGCOPY_START(
				kbdev, queue);

			kbase_kcpu_debug_copy(queue->kctx,
						&cmd->info.debug_copy);

			KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_EXECUTE_DEBUGCOPY_END(
				kbdev, queue);
			break;
		case BASE_KCPU_COMMAND_TYPE_JIT_ALLOC:
		{
			KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_EXECUTE_JIT_ALLOC_START(
				kbdev, queue);

			status = kbase_kcpu_jit_allocate_process(queue->kctx,
						&cmd->info.jit_alloc);

			KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_EXECUTE_JIT_ALLOC_INFO(
				kbdev, queue,
				&cmd->info.jit_alloc, (status == 0));

			kfree(cmd->info.jit_alloc.info);

			KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_EXECUTE_JIT_ALLOC_END(
				kbdev, queue);
			break;
		}
		case BASE_KCPU_COMMAND_TYPE_JIT_FREE:
			KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_EXECUTE_JIT_FREE_START(
				kbdev, queue);

			KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_EXECUTE_JIT_FREE_INFO(
				kbdev, queue, &cmd->info.jit_free);

			kbase_kcpu_jit_free_process(queue->kctx,
						&cmd->info.jit_free);

			KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_EXECUTE_JIT_FREE_END(
				kbdev, queue);
			break;
		case BASE_KCPU_COMMAND_TYPE_GROUP_SUSPEND:
			kbase_csf_queue_group_suspend_process(queue->kctx,
				cmd->info.suspend_buf_copy.sus_buf,
				cmd->info.suspend_buf_copy.group_handle);
			kfree(cmd->info.suspend_buf_copy.sus_buf->pages);
			kfree(cmd->info.suspend_buf_copy.sus_buf);
			break;
		default:
			dev_warn(kbdev->dev,
				"Unrecognized command type\n");
			break;
		} /* switch */

		/*TBD: error handling */

		if (!process_next)
			break;
	}

	if (i > 0) {
		queue->start_offset += i;
		queue->num_pending_cmds -= i;

		/* If an attempt to enqueue commands failed then we must raise
		 * an event in case the client wants to retry now that there is
		 * free space in the buffer.
		 */
		if (queue->enqueue_failed) {
			queue->enqueue_failed = false;
			kbase_csf_event_signal_cpu_only(queue->kctx);
		}
	}
}

static size_t kcpu_queue_get_space(struct kbase_kcpu_command_queue *queue)
{
	return KBASEP_KCPU_QUEUE_SIZE - queue->num_pending_cmds;
}

static void KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_ENQUEUE_COMMAND(
	const struct kbase_kcpu_command_queue *queue,
	const struct kbase_kcpu_command *cmd)
{
	struct kbase_device *kbdev = queue->kctx->kbdev;

	switch (cmd->type) {
	case BASE_KCPU_COMMAND_TYPE_FENCE_WAIT:
		KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_ENQUEUE_FENCE_WAIT(
			kbdev, queue, cmd->info.fence.fence);
		break;
	case BASE_KCPU_COMMAND_TYPE_FENCE_SIGNAL:
		KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_ENQUEUE_FENCE_SIGNAL(
			kbdev, queue, cmd->info.fence.fence);
		break;
	case BASE_KCPU_COMMAND_TYPE_CQS_WAIT:
	{
		const struct base_cqs_wait *waits = cmd->info.cqs_wait.objs;
		unsigned int i;

		KBASE_TLSTREAM_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_ENQUEUE_CQS_WAIT(
			kbdev, queue);
		for (i = 0; i < cmd->info.cqs_wait.nr_objs; i++) {
			KBASE_TLSTREAM_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_ENQUEUE_CQS_WAIT(
				kbdev, queue, waits[i].addr, waits[i].val);
		}
		KBASE_TLSTREAM_TL_KBASE_ARRAY_END_KCPUQUEUE_ENQUEUE_CQS_WAIT(
			kbdev, queue);
		break;
	}
	case BASE_KCPU_COMMAND_TYPE_CQS_SET:
	{
		const struct base_cqs_set *sets = cmd->info.cqs_set.objs;
		unsigned int i;

		KBASE_TLSTREAM_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_ENQUEUE_CQS_SET(
			kbdev, queue);
		for (i = 0; i < cmd->info.cqs_set.nr_objs; i++) {
			KBASE_TLSTREAM_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_ENQUEUE_CQS_SET(
				kbdev, queue, sets[i].addr);
		}
		KBASE_TLSTREAM_TL_KBASE_ARRAY_END_KCPUQUEUE_ENQUEUE_CQS_SET(
			kbdev, queue);
		break;
	}
	case BASE_KCPU_COMMAND_TYPE_DEBUG_COPY:
	{
		const struct kbase_debug_copy_buffer *buffers =
			cmd->info.debug_copy.buffers;
		unsigned int i;

		KBASE_TLSTREAM_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_ENQUEUE_DEBUGCOPY(
			kbdev, queue);
		for (i = 0; i < cmd->info.debug_copy.nr; i++) {
			KBASE_TLSTREAM_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_ENQUEUE_DEBUGCOPY(
				kbdev, queue, buffers[i].size);
		}
		KBASE_TLSTREAM_TL_KBASE_ARRAY_END_KCPUQUEUE_ENQUEUE_DEBUGCOPY(
			kbdev, queue);
		break;
	}
	case BASE_KCPU_COMMAND_TYPE_MAP_IMPORT:
		KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_ENQUEUE_MAP_IMPORT(
			kbdev, queue, cmd->info.import.gpu_va);
		break;
	case BASE_KCPU_COMMAND_TYPE_UNMAP_IMPORT:
		KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_ENQUEUE_UNMAP_IMPORT(
			kbdev, queue, cmd->info.import.gpu_va);
		break;
	case BASE_KCPU_COMMAND_TYPE_UNMAP_IMPORT_FORCE:
		KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_ENQUEUE_UNMAP_IMPORT_FORCE(
			kbdev, queue, cmd->info.import.gpu_va);
		break;
	case BASE_KCPU_COMMAND_TYPE_JIT_ALLOC:
	{
		u8 i;

		KBASE_TLSTREAM_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_ENQUEUE_JIT_ALLOC(
			kbdev, queue);
		for (i = 0; i < cmd->info.jit_alloc.count; i++) {
			const struct base_jit_alloc_info *info =
				&cmd->info.jit_alloc.info[i];

			KBASE_TLSTREAM_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_ENQUEUE_JIT_ALLOC(
				kbdev, queue,
				info->gpu_alloc_addr, info->va_pages,
				info->commit_pages, info->extent, info->id,
				info->bin_id, info->max_allocations,
				info->flags, info->usage_id);
		}
		KBASE_TLSTREAM_TL_KBASE_ARRAY_END_KCPUQUEUE_ENQUEUE_JIT_ALLOC(
			kbdev, queue);
		break;
	}
	case BASE_KCPU_COMMAND_TYPE_JIT_FREE:
	{
		u8 i;

		KBASE_TLSTREAM_TL_KBASE_ARRAY_BEGIN_KCPUQUEUE_ENQUEUE_JIT_FREE(
			kbdev, queue);
		for (i = 0; i < cmd->info.jit_free.count; i++) {
			KBASE_TLSTREAM_TL_KBASE_ARRAY_ITEM_KCPUQUEUE_ENQUEUE_JIT_FREE(
				kbdev, queue, cmd->info.jit_free.ids[i]);
		}
		KBASE_TLSTREAM_TL_KBASE_ARRAY_END_KCPUQUEUE_ENQUEUE_JIT_FREE(
			kbdev, queue);
		break;
	}
	}
}

int kbase_csf_kcpu_queue_enqueue(struct kbase_context *kctx,
			struct kbase_ioctl_kcpu_queue_enqueue *enq)
{
	struct kbase_kcpu_command_queue *queue = NULL;
	void __user *user_cmds = u64_to_user_ptr(enq->addr);
	int ret = 0;
	u32 i;

	/* The offset to the first command that is being processed or yet to
	 * be processed is of u8 type, so the number of commands inside the
	 * queue cannot be more than 256.
	 */
	BUILD_BUG_ON(KBASEP_KCPU_QUEUE_SIZE > 256);

	/* Whilst the backend interface allows enqueueing multiple commands in
	 * a single operation, the Base interface does not expose any mechanism
	 * to do so. And also right now the handling is missing for the case
	 * where multiple commands are submitted and the enqueue of one of the
	 * command in the set fails after successfully enqueuing other commands
	 * in the set.
	 */
	if (enq->nr_commands != 1) {
		dev_err(kctx->kbdev->dev,
			"More than one commands enqueued\n");
		return -EINVAL;
	}

	mutex_lock(&kctx->csf.kcpu_queues.lock);

	if (!kctx->csf.kcpu_queues.array[enq->id]) {
		ret = -EINVAL;
		goto out;
	}

	queue = kctx->csf.kcpu_queues.array[enq->id];

	if (kcpu_queue_get_space(queue) < enq->nr_commands) {
		ret = -EBUSY;
		queue->enqueue_failed = true;
		goto out;
	}

	/* Copy all command's info to the command buffer.
	 * Note: it would be more efficient to process all commands in-line
	 * until we encounter an unresolved CQS_ / FENCE_WAIT, however, the
	 * interface allows multiple commands to be enqueued so we must account
	 * for the possibility to roll back.
	 */

	for (i = 0; (i != enq->nr_commands) && !ret; ++i) {
		struct kbase_kcpu_command *kcpu_cmd =
			&queue->commands[(u8)(queue->start_offset + queue->num_pending_cmds + i)];
		struct base_kcpu_command command;

		if (copy_from_user(&command, user_cmds, sizeof(command))) {
			ret = -EFAULT;
			goto out;
		}

		user_cmds = (void __user *)((uintptr_t)user_cmds +
				sizeof(struct base_kcpu_command));

		switch (command.type) {
		case BASE_KCPU_COMMAND_TYPE_FENCE_WAIT:
#ifdef CONFIG_SYNC_FILE
			ret = kbase_kcpu_fence_wait_prepare(queue,
						&command.info.fence, kcpu_cmd);
#else
			ret = -EINVAL;
			dev_warn(kctx->kbdev->dev, "fence wait command unsupported\n");
#endif
			break;
		case BASE_KCPU_COMMAND_TYPE_FENCE_SIGNAL:
#ifdef CONFIG_SYNC_FILE
			ret = kbase_kcpu_fence_signal_prepare(queue,
						&command.info.fence, kcpu_cmd);
#else
			ret = -EINVAL;
			dev_warn(kctx->kbdev->dev, "fence signal command unsupported\n");
#endif
			break;
		case BASE_KCPU_COMMAND_TYPE_CQS_WAIT:
			ret = kbase_kcpu_cqs_wait_prepare(queue,
					&command.info.cqs_wait, kcpu_cmd);
			break;
		case BASE_KCPU_COMMAND_TYPE_CQS_SET:
			ret = kbase_kcpu_cqs_set_prepare(queue,
					&command.info.cqs_set, kcpu_cmd);
			break;
		case BASE_KCPU_COMMAND_TYPE_DEBUG_COPY:
			ret = kbase_kcpu_debug_copy_prepare(queue,
					&command.info.debug_copy, kcpu_cmd);
			break;
		case BASE_KCPU_COMMAND_TYPE_MAP_IMPORT:
			ret = kbase_kcpu_map_import_prepare(queue,
					&command.info.import, kcpu_cmd);
			break;
		case BASE_KCPU_COMMAND_TYPE_UNMAP_IMPORT:
			ret = kbase_kcpu_unmap_import_prepare(queue,
					&command.info.import, kcpu_cmd);
			break;
		case BASE_KCPU_COMMAND_TYPE_UNMAP_IMPORT_FORCE:
			ret = kbase_kcpu_unmap_import_force_prepare(queue,
					&command.info.import, kcpu_cmd);
			break;
		case BASE_KCPU_COMMAND_TYPE_JIT_ALLOC:
			ret = kbase_kcpu_jit_allocate_prepare(queue,
					&command.info.jit_alloc, kcpu_cmd);
			break;
		case BASE_KCPU_COMMAND_TYPE_JIT_FREE:
			ret = kbase_kcpu_jit_free_prepare(queue,
					&command.info.jit_free, kcpu_cmd);
			break;
		case BASE_KCPU_COMMAND_TYPE_GROUP_SUSPEND:
			ret = kbase_csf_queue_group_suspend_prepare(queue,
					&command.info.suspend_buf_copy,
					kcpu_cmd);
			break;

		default:
			dev_warn(queue->kctx->kbdev->dev,
				"Unknown command type %u\n", command.type);
			ret = -EINVAL;
			break;
		}
	}

	if (!ret) {
		/* We only instrument the enqueues after all commands have been
		 * successfully enqueued, as if we do them during the enqueue
		 * and there is an error, we won't be able to roll them back
		 * like is done for the command enqueues themselves.
		 */
		for (i = 0; i != enq->nr_commands; ++i) {
			u8 cmd_idx = (u8)(queue->start_offset + queue->num_pending_cmds + i);

			KBASE_TLSTREAM_TL_KBASE_KCPUQUEUE_ENQUEUE_COMMAND(
				queue, &queue->commands[cmd_idx]);
		}

		queue->num_pending_cmds += enq->nr_commands;
		kcpu_queue_process(queue, false);
	}

out:
	mutex_unlock(&kctx->csf.kcpu_queues.lock);

	return ret;
}

int kbase_csf_kcpu_queue_context_init(struct kbase_context *kctx)
{
	int idx;

	bitmap_zero(kctx->csf.kcpu_queues.in_use, KBASEP_MAX_KCPU_QUEUES);

	for (idx = 0; idx < KBASEP_MAX_KCPU_QUEUES; ++idx)
		kctx->csf.kcpu_queues.array[idx] = NULL;

	kctx->csf.kcpu_queues.wq = alloc_workqueue("mali_kbase_csf_kcpu",
					WQ_UNBOUND | WQ_HIGHPRI, 0);
	if (!kctx->csf.kcpu_queues.wq)
		return -ENOMEM;

	mutex_init(&kctx->csf.kcpu_queues.lock);

	return 0;
}

void kbase_csf_kcpu_queue_context_term(struct kbase_context *kctx)
{
	while (!bitmap_empty(kctx->csf.kcpu_queues.in_use,
			KBASEP_MAX_KCPU_QUEUES)) {
		int id = find_first_bit(kctx->csf.kcpu_queues.in_use,
				KBASEP_MAX_KCPU_QUEUES);

		if (WARN_ON(!kctx->csf.kcpu_queues.array[id]))
			clear_bit(id, kctx->csf.kcpu_queues.in_use);
		else
			(void)delete_queue(kctx, id);
	}

	destroy_workqueue(kctx->csf.kcpu_queues.wq);
	mutex_destroy(&kctx->csf.kcpu_queues.lock);
}

int kbase_csf_kcpu_queue_delete(struct kbase_context *kctx,
			struct kbase_ioctl_kcpu_queue_delete *del)
{
	return delete_queue(kctx, (u32)del->id);
}

int kbase_csf_kcpu_queue_new(struct kbase_context *kctx,
			struct kbase_ioctl_kcpu_queue_new *newq)
{
	struct kbase_kcpu_command_queue *queue;
	int idx;
	int ret = 0;

	/* The queue id is of u8 type and we use the index of the kcpu_queues
	 * array as an id, so the number of elements in the array can't be
	 * more than 256.
	 */
	BUILD_BUG_ON(KBASEP_MAX_KCPU_QUEUES > 256);

	mutex_lock(&kctx->csf.kcpu_queues.lock);

	idx = find_first_zero_bit(kctx->csf.kcpu_queues.in_use,
			KBASEP_MAX_KCPU_QUEUES);
	if (idx >= (int)KBASEP_MAX_KCPU_QUEUES) {
		ret = -ENOMEM;
		goto out;
	}

	if (WARN_ON(kctx->csf.kcpu_queues.array[idx])) {
		ret = -EINVAL;
		goto out;
	}

	queue = kzalloc(sizeof(*queue), GFP_KERNEL);

	if (!queue) {
		ret = -ENOMEM;
		goto out;
	}

	bitmap_set(kctx->csf.kcpu_queues.in_use, idx, 1);
	kctx->csf.kcpu_queues.array[idx] = queue;
	queue->kctx = kctx;
	queue->start_offset = 0;
	queue->num_pending_cmds = 0;
#ifdef CONFIG_SYNC_FILE
	queue->fence_context = dma_fence_context_alloc(1);
	queue->fence_seqno = 0;
	queue->fence_signaled = true;
#endif
	queue->enqueue_failed = false;
	queue->command_started = false;
	INIT_WORK(&queue->work, kcpu_queue_process_worker);

	newq->id = idx;

	/* Fire the tracepoint with the mutex held to enforce correct ordering
	 * with the summary stream.
	 */
	KBASE_TLSTREAM_TL_KBASE_NEW_KCPUQUEUE(
		kctx->kbdev, queue, kctx->id, queue->num_pending_cmds);
out:
	mutex_unlock(&kctx->csf.kcpu_queues.lock);

	return ret;
}

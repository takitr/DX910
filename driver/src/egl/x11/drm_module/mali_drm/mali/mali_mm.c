/*
 * Copyright (C) 2010, 2012-2013, 2015 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/version.h>

#include <drm/drmP.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0)
#include <drm/drm_legacy.h>
#endif
#include "mali_drm.h"
#include "mali_drv.h"

#define VIDEO_TYPE 0
#define MEM_TYPE 1

#define MALI_MM_ALIGN_SHIFT 4
#define MALI_MM_ALIGN_MASK ( (1 << MALI_MM_ALIGN_SHIFT) - 1)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
struct mali_memblock {
	struct drm_mm_node mm_node;
	struct list_head owner_list;
};
#else
static void *mali_sman_mm_allocate(void *private, unsigned long size, unsigned alignment)
{
	printk(KERN_ERR "DRM: %s\n", __func__);
	return NULL;
}

static void mali_sman_mm_free(void *private, void *ref)
{
	printk(KERN_ERR "DRM: %s\n", __func__);
}

static void mali_sman_mm_destroy(void *private)
{
	printk(KERN_ERR "DRM: %s\n", __func__);
}

static unsigned long mali_sman_mm_offset(void *private, void *ref)
{
	printk(KERN_ERR "DRM: %s\n", __func__);
	return ~((unsigned long)ref);
}
#endif

static int mali_fb_init(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_mali_private_t *dev_priv = dev->dev_private;
	drm_mali_fb_t *fb = data;
	printk(KERN_ERR "DRM: %s\n", __func__);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
#else
	int ret;
#endif

	mutex_lock(&dev->struct_mutex);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	drm_mm_init(&dev_priv->vram_mm, 0, fb->size >> MALI_MM_ALIGN_SHIFT);
#else
	{
		struct drm_sman_mm sman_mm;
		sman_mm.private = (void *)0xFFFFFFFF;
		sman_mm.allocate = mali_sman_mm_allocate;
		sman_mm.free = mali_sman_mm_free;
		sman_mm.destroy = mali_sman_mm_destroy;
		sman_mm.offset = mali_sman_mm_offset;
		ret = drm_sman_set_manager(&dev_priv->sman, VIDEO_TYPE, &sman_mm);
	}

	if (ret)
	{
		DRM_ERROR("VRAM memory manager initialisation error\n");
		mutex_unlock(&dev->struct_mutex);
		return ret;
	}
#endif

	dev_priv->vram_initialized = 1;
	dev_priv->vram_offset = fb->offset;

	mutex_unlock(&dev->struct_mutex);
	DRM_DEBUG("offset = %u, size = %u\n", fb->offset, fb->size);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
static int mali_drm_alloc(struct drm_device *dev, struct drm_file *file, void *data, int pool)
#else
static int mali_drm_alloc(struct drm_device *dev, struct drm_file *file_priv, void *data, int pool)
#endif
{
	drm_mali_private_t *dev_priv = dev->dev_private;
	drm_mali_mem_t *mem = data;
	int retval = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	int user_key;
	struct mali_memblock *item;
	unsigned long offset;
	struct mali_file_private *file_priv = file->driver_priv;
#else
	struct drm_memblock_item *item;
#endif
	printk(KERN_ERR "DRM: %s\n", __func__);

	/* Untested */
	if (pool > MEM_TYPE) {
		DRM_ERROR("Unknown memory type allocation\n");
		return -EINVAL;
	}
	/* END: Untested */
	mutex_lock(&dev->struct_mutex);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	if (0 == ((pool == VIDEO_TYPE) ? dev_priv->vram_initialized : dev_priv->mem_initialized))
#else
	if (0 == dev_priv->vram_initialized)
#endif
	{
		DRM_ERROR("Attempt to allocate from uninitialized memory manager.\n");
		mutex_unlock(&dev->struct_mutex);
		return -EINVAL;
	}

	/* Untested */
	item = kzalloc(sizeof(*item), GFP_KERNEL);
	if (!item) {
		retval = -ENOMEM;
		goto fail_alloc;
	}
	/* END: Untested */

	mem->size = (mem->size + MALI_MM_ALIGN_MASK) >> MALI_MM_ALIGN_SHIFT;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	if (pool == MEM_TYPE) {
		retval = drm_mm_insert_node(&dev_priv->mem_mm,
					   &item->mm_node,
					   mem->size, 0,
					   DRM_MM_SEARCH_DEFAULT);
		offset = item->mm_node.start;
	} else {
		retval = drm_mm_insert_node(&dev_priv->vram_mm,
					   &item->mm_node,
					   mem->size, 0,
					   DRM_MM_SEARCH_DEFAULT);
		offset = item->mm_node.start;
	}
	if (retval)
		goto fail_alloc;
#else
	item = drm_sman_alloc(&dev_priv->sman, pool, mem->size, 0,
	                      (unsigned long)file_priv);
#endif

	/* Untested */
	retval = idr_alloc(&dev_priv->object_idr, item, 1, 0, GFP_KERNEL);
	if (retval < 0)
		goto fail_idr;
	user_key = retval;

	list_add(&item->owner_list, &file_priv->obj_list);
	/* END: Untested */
	mutex_unlock(&dev->struct_mutex);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	mem->offset = dev_priv->vram_offset + (offset << MALI_MM_ALIGN_SHIFT);
	mem->free = user_key;
	mem->size = mem->size << MALI_MM_ALIGN_SHIFT;

	return 0;

fail_idr:
	drm_mm_remove_node(&item->mm_node);
fail_alloc:
	kfree(item);
	mutex_unlock(&dev->struct_mutex);

	mem->offset = 0;
	mem->size = 0;
	mem->free = 0;
#else // TODO: Applying a cleaner solution above
	if (item)
	{
		mem->offset = dev_priv->vram_offset + (item->mm->offset(item->mm, item->mm_info) << MALI_MM_ALIGN_SHIFT);
		mem->free = item->user_hash.key;
		mem->size = mem->size << MALI_MM_ALIGN_SHIFT;
	}
	else
	{
		mem->offset = 0;
		mem->size = 0;
		mem->free = 0;
		retval = -ENOMEM;
	}
#endif

	DRM_DEBUG("alloc %d, size = %u, offset = %u\n", pool, mem->size, mem->offset);

	return retval;
}

static int mali_drm_free(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_mali_private_t *dev_priv = dev->dev_private;
	drm_mali_mem_t *mem = data;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	struct mali_memblock *obj;
#else
	int ret;
#endif
	printk(KERN_ERR "DRM: %s\n", __func__);

	mutex_lock(&dev->struct_mutex);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	obj = idr_find(&dev_priv->object_idr, mem->free);
	if (obj == NULL) {
		mutex_unlock(&dev->struct_mutex);
		return -EINVAL;
	}

	idr_remove(&dev_priv->object_idr, mem->free);
	list_del(&obj->owner_list);
	if (drm_mm_node_allocated(&obj->mm_node))
		drm_mm_remove_node(&obj->mm_node);
	kfree(obj);
#else
	ret = drm_sman_free_key(&dev_priv->sman, mem->free);
#endif
	mutex_unlock(&dev->struct_mutex);
	DRM_DEBUG("free = 0x%lx\n", mem->free);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	return 0;
#else
	return ret;
#endif
}

static int mali_fb_alloc(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	printk(KERN_ERR "DRM: %s\n", __func__);
	return mali_drm_alloc(dev, file_priv, data, VIDEO_TYPE);
}

static int mali_ioctl_mem_init(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_mali_private_t *dev_priv = dev->dev_private;
	drm_mali_mem_t *mem = data;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
#else
	int ret;
#endif
	dev_priv = dev->dev_private;
	printk(KERN_ERR "DRM: %s\n", __func__);

	mutex_lock(&dev->struct_mutex);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	drm_mm_init(&dev_priv->mem_mm, 0, mem->size >> MALI_MM_ALIGN_SHIFT);
#else
	ret = drm_sman_set_range(&dev_priv->sman, MEM_TYPE, 0, mem->size >> MALI_MM_ALIGN_SHIFT);

	if (ret)
	{
		DRM_ERROR("MEM memory manager initialisation error\n");
		mutex_unlock(&dev->struct_mutex);
		return ret;
	}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	dev_priv->mem_initialized = 1;
	dev_priv->mem_offset = mem->offset;
#endif
	mutex_unlock(&dev->struct_mutex);

	DRM_DEBUG("offset = %u, size = %u\n", mem->offset, mem->size);
	return 0;
}

static int mali_ioctl_mem_alloc(struct drm_device *dev, void *data,
                                struct drm_file *file_priv)
{

	printk(KERN_ERR "DRM: %s\n", __func__);
	return mali_drm_alloc(dev, file_priv, data, MEM_TYPE);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
#else
static drm_local_map_t *mem_reg_init(struct drm_device *dev)
{
	struct drm_map_list *entry;
	drm_local_map_t *map;
	printk(KERN_ERR "DRM: %s\n", __func__);

	list_for_each_entry(entry, &dev->maplist, head)
	{
		map = entry->map;

		if (!map)
		{
			continue;
		}

		if (map->type == _DRM_REGISTERS)
		{
			return map;
		}
	}
	return NULL;
}
#endif

int mali_idle(struct drm_device *dev)
{
	drm_mali_private_t *dev_priv = dev->dev_private;
	/* TODO: Not needed, isn't it?
	uint32_t idle_reg;
	unsigned long end;
	int i;
	*/
	printk(KERN_ERR "DRM: %s\n", __func__);

	if (dev_priv->idle_fault)
	{
		return 0;
	}

	return 0;
}


void mali_lastclose(struct drm_device *dev)
{
	drm_mali_private_t *dev_priv = dev->dev_private;
	printk(KERN_ERR "DRM: %s\n", __func__);

	if (!dev_priv)
	{
		return;
	}

	mutex_lock(&dev->struct_mutex);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	if (dev_priv->vram_initialized) {
		drm_mm_takedown(&dev_priv->vram_mm);
		dev_priv->vram_initialized = 0;
	}
	if (dev_priv->mem_initialized) {
		drm_mm_takedown(&dev_priv->mem_mm);
		dev_priv->mem_initialized = 0;
	}
#else
	drm_sman_cleanup(&dev_priv->sman);
	dev_priv->vram_initialized = 0;
#endif
	dev_priv->mmio = NULL;
	mutex_unlock(&dev->struct_mutex);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
void mali_reclaim_buffers_locked(struct drm_device *dev, struct drm_file *file)
#else
void mali_reclaim_buffers_locked(struct drm_device *dev, struct drm_file *file_priv)
#endif
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	struct mali_file_private *file_priv = file->driver_priv;
	struct mali_memblock *entry, *next;
#else
	drm_mali_private_t *dev_priv = dev->dev_private;
#endif
	printk(KERN_ERR "DRM: %s\n", __func__);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	if (!(file->minor->master && file->master->lock.hw_lock))
		return;

	printk(KERN_ERR "DRM: %s A\n", __func__);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0)
	drm_legacy_idlelock_take(&file->master->lock);
#else
	drm_idlelock_take(&file->master->lock);
#endif

	mutex_lock(&dev->struct_mutex);
	if (list_empty(&file_priv->obj_list)) {
		mutex_unlock(&dev->struct_mutex);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0)
		drm_legacy_idlelock_release(&file->master->lock);
#else
		drm_idlelock_release(&file->master->lock);
#endif
		return;
	}

	printk(KERN_ERR "DRM: %s B\n", __func__);
	mali_idle(dev);

	list_for_each_entry_safe(entry, next, &file_priv->obj_list,
				 owner_list) {
		list_del(&entry->owner_list);
		if (drm_mm_node_allocated(&entry->mm_node))
			drm_mm_remove_node(&entry->mm_node);
		kfree(entry);
	}
#else
	mutex_lock(&dev->struct_mutex);

	if (drm_sman_owner_clean(&dev_priv->sman, (unsigned long)file_priv))
	{
		mutex_unlock(&dev->struct_mutex);
		return;
	}

	if (dev->driver->dma_quiescent)
	{
		dev->driver->dma_quiescent(dev);
	}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
#else
	drm_sman_owner_cleanup(&dev_priv->sman, (unsigned long)file_priv);
#endif
	mutex_unlock(&dev->struct_mutex);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0)
	drm_legacy_idlelock_release(&file->master->lock);
#else
	drm_idlelock_release(&file->master->lock);
#endif
#endif

	return;
}

const struct drm_ioctl_desc mali_ioctls[] =
{
	DRM_IOCTL_DEF_DRV(DRM_MALI_FB_ALLOC, mali_fb_alloc, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(DRM_MALI_FB_FREE, mali_drm_free, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(DRM_MALI_MEM_INIT, mali_ioctl_mem_init, DRM_AUTH | DRM_MASTER | DRM_ROOT_ONLY),
	DRM_IOCTL_DEF_DRV(DRM_MALI_MEM_ALLOC, mali_ioctl_mem_alloc, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(DRM_MALI_MEM_FREE, mali_drm_free, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(DRM_MALI_FB_INIT, mali_fb_init, DRM_AUTH | DRM_MASTER | DRM_ROOT_ONLY),
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
int mali_max_ioctl =     ARRAY_SIZE(mali_ioctls);
#else
int mali_max_ioctl = DRM_ARRAY_SIZE(mali_ioctls);
#endif

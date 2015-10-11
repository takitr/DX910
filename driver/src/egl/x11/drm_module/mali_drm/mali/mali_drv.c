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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
#include <linux/module.h>
#endif

#include <drm/drmP.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0)
#include <drm/drm_legacy.h>
#endif

#include "mali_drm.h"
#include "mali_drv.h"

#ifndef MODULE_ARCH_VERMAGIC
#include <linux/vermagic.h>
#endif

static struct platform_device *pdev;

static int mali_platform_drm_probe(struct platform_device *pdev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	pr_info("DRM: mali_platform_drm_probe()\n");
	return mali_drm_init(pdev);
#else
	return 0;
#endif
}

static int mali_platform_drm_remove(struct platform_device *pdev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	pr_info("DRM: mali_platform_drm_remove()\n");
	mali_drm_exit(pdev);
#endif
	return 0;
}

static int mali_platform_drm_suspend(struct platform_device *dev, pm_message_t state)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	pr_info("DRM: mali_platform_drm_suspend()\n");
#endif
	return 0;
}

static int mali_platform_drm_resume(struct platform_device *dev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	pr_info("DRM: mali_platform_drm_resume()\n");
#endif
	return 0;
}


static char mali_drm_device_name[] = "mali_drm";
static struct platform_driver platform_drm_driver =
{
	.probe = mali_platform_drm_probe,
	.remove = mali_platform_drm_remove,
	.suspend = mali_platform_drm_suspend,
	.resume = mali_platform_drm_resume,
	.driver = {
		.name = mali_drm_device_name,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
		.set_busid = drm_platform_set_busid,
#endif
		.owner = THIS_MODULE,
	},
};

#if 0
static const struct drm_device_id dock_device_ids[] =
{
	{"MALIDRM", 0},
	{"", 0},
};
#endif

static int mali_driver_load(struct drm_device *dev, unsigned long chipset)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
#else	
	int ret;
	unsigned long base, size;
#endif
	drm_mali_private_t *dev_priv;
	printk(KERN_ERR "DRM: mali_driver_load start\n");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	dev_priv = kmalloc(sizeof(drm_mali_private_t), GFP_KERNEL);
	if ( dev_priv == NULL ) return -ENOMEM;
#else
	dev_priv = drm_calloc(1, sizeof(drm_mali_private_t), DRM_MEM_DRIVER);

	if (dev_priv == NULL)
	{
		return -ENOMEM;
	}
#endif

	dev->dev_private = (void *)dev_priv;

	if (NULL == dev->platformdev)
	{
		dev->platformdev = platform_device_register_simple(mali_drm_device_name, 0, NULL, 0);
		pdev = dev->platformdev;
	}

#if 0
	base = drm_get_resource_start(dev, 1);
	size = drm_get_resource_len(dev, 1);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	idr_init(&dev_priv->object_idr);
#else
	ret = drm_sman_init(&dev_priv->sman, 2, 12, 8);

	if (ret)
	{
		drm_free(dev_priv, sizeof(dev_priv), DRM_MEM_DRIVER);
	}

	//if ( ret ) kfree( dev_priv );
#endif

	printk(KERN_ERR "DRM: mali_driver_load done\n");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	return 0;
#else
	return ret;
#endif
}

static int mali_driver_unload(struct drm_device *dev)
{
	drm_mali_private_t *dev_priv = dev->dev_private;
	printk(KERN_ERR "DRM: mali_driver_unload start\n");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	idr_destroy(&dev_priv->object_idr);
	kfree(dev_priv);
#else
	drm_sman_takedown(&dev_priv->sman);
	drm_free(dev_priv, sizeof(*dev_priv), DRM_MEM_DRIVER);
#endif
	//kfree( dev_priv );
	printk(KERN_ERR "DRM: mali_driver_unload done\n");

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
static int mali_driver_open(struct drm_device *dev, struct drm_file *file)
{
	struct mali_file_private *file_priv;

	DRM_DEBUG_DRIVER("\n");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0)
	file_priv = kzalloc(sizeof(*file_priv), GFP_KERNEL);
#else
	file_priv = kmalloc(sizeof(*file_priv), GFP_KERNEL);
#endif
	if (!file_priv)
		return -ENOMEM;

	file->driver_priv = file_priv;

	INIT_LIST_HEAD(&file_priv->obj_list);
	return 0;
}

void mali_driver_postclose(struct drm_device *dev, struct drm_file *file)
{
	struct via_file_private *file_priv = file->driver_priv;

	kfree(file_priv);
}
#endif

static const struct file_operations mali_driver_fops = {
        .owner = THIS_MODULE,
        .open = drm_open,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0)
        .mmap = drm_legacy_mmap,
#else
        .mmap = drm_mmap,
#endif
        .poll = drm_poll,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
	.ioctl = drm_ioctl,
#else
	.unlocked_ioctl = drm_ioctl,
#endif
        .release = drm_release,
#ifdef CONFIG_COMPAT
	.compat_ioctl = drm_compat_ioctl,
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,12,0))
	.fasync = drm_fasync,
#endif
};

static struct drm_driver driver =
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	.driver_features = DRIVER_BUS_PLATFORM,
#else
	.driver_features = DRIVER_USE_PLATFORM_DEVICE,
#endif
#endif
	.load = mali_driver_load,
	.unload = mali_driver_unload,
	.context_dtor = NULL,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	.open = mali_driver_open,
	.preclose = mali_reclaim_buffers_locked,
	.postclose = mali_driver_postclose,
#endif
	.dma_quiescent = mali_idle,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
#else
	.reclaim_buffers = NULL,
	.reclaim_buffers_idlelocked = mali_reclaim_buffers_locked,
#endif
	.lastclose = mali_lastclose,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
#else
	.get_map_ofs = drm_core_get_map_ofs,
	.get_reg_ofs = drm_core_get_reg_ofs,
#endif
	.ioctls = mali_ioctls,
	.fops = &mali_driver_fops,
	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
	.patchlevel = DRIVER_PATCHLEVEL,
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38)
int mali_drm_init(struct platform_device *dev)
{
        pr_info("mali_drm_init(), driver name: %s, version %d.%d\n", DRIVER_NAME, DRIVER_MAJOR, DRIVER_MINOR);
        driver.num_ioctls = mali_max_ioctl;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
#else
        driver.kdriver.platform_device = dev;
#endif
        return drm_platform_init(&driver, dev);
}

void mali_drm_exit(struct platform_device *dev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
	drm_put_dev(platform_get_drvdata(dev));
#else
	drm_platform_exit(&driver, dev);
#endif
}

static int __init mali_platform_drm_init(void)
{
        pdev = platform_device_register_simple(mali_drm_device_name, 0, NULL, 0);
        return platform_driver_register(&platform_drm_driver);
}

static void __exit mali_platform_drm_exit(void)
{
        platform_device_unregister(pdev);
        platform_driver_unregister(&platform_drm_driver);
}
#else
static int __init mali_init(void)
{
	driver.num_ioctls = mali_max_ioctl;
	return drm_init(&driver);
}

static void __exit mali_exit(void)
{
	platform_device_unregister(pdev);
	drm_exit(&driver);
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38)
module_init(mali_platform_drm_init);
module_exit(mali_platform_drm_exit);
#else
module_init(mali_init);
module_exit(mali_exit);
#endif


/* TODO: Causes an error:
MODULE_INFO(vermagic, VERMAGIC_STRING);
*/
MODULE_AUTHOR("ARM Ltd.");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL and additional rights");

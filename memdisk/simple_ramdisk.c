// simple_ramdisk.c (for linux-5.4 and later)

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/bio.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h> // <--- 包含新的 blk-mq 头文件
#include <linux/vmalloc.h>

// --- 元数据和定义 ---
#define SBD_DEVICE_NAME "sbd"
#define SBD_MINORS 1
#define KERNEL_SECTOR_SIZE 512

// --- 模块参数 ---
static int disk_size_mb = 64;
module_param(disk_size_mb, int, 0644);
MODULE_PARM_DESC(disk_size_mb, "Size of the simple ramdisk in MB");

// --- 全局变量 ---
static int sbd_major;
static char *sbd_data;
static struct gendisk *sbd_disk;
static struct request_queue *sbd_queue;
static struct blk_mq_tag_set sbd_tag_set; // <--- 新增 blk-mq tag set
static spinlock_t sbd_lock; // 我们仍然需要一个锁来保护我们的数据
static long sbd_nr_sectors;

// --- 请求处理函数 (blk-mq 版本) ---
// 这个函数处理单个请求
// --- 请求处理函数 (blk-mq 版本 - 修正最终版) ---
static blk_status_t sbd_queue_rq(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd) {
    struct request *rq = bd->rq;
    char *offset;
    struct bio_vec bvec;
    struct req_iterator iter;

    // 开始处理请求
    blk_mq_start_request(rq);

    // 计算在我们的内存盘中的起始偏移地址
    offset = sbd_data + (blk_rq_pos(rq) * KERNEL_SECTOR_SIZE);

    // 检查请求是否越界 (这是一个好的健壮性措施)
    if ((blk_rq_pos(rq) + blk_rq_sectors(rq)) > sbd_nr_sectors) {
        printk(KERN_ERR "SBD: Invalid request: pos=%llu, sectors=%u, capacity=%lu\n",
               (unsigned long long)blk_rq_pos(rq), blk_rq_sectors(rq), sbd_nr_sectors);
        blk_mq_end_request(rq, BLK_STS_IOERR);
        return BLK_STS_OK;
    }

    // 在访问共享数据sbd_data之前加锁
    spin_lock(&sbd_lock);

    // 判断一次请求方向，然后进入对应的处理逻辑
    if (rq_data_dir(rq) == WRITE) {
        // --- 处理写请求 ---
        // 遍历请求中的每一个数据段(bvec)，将数据从请求缓冲区拷贝到我们的内存盘
        rq_for_each_segment(bvec, rq, iter) {
            size_t bytes = bvec.bv_len;
            // 获取数据段的内核虚拟地址
            char *p = kmap_atomic(bvec.bv_page) + bvec.bv_offset;
            // 从 p (请求的源) 拷贝到 offset (我们的内存盘)
            memcpy(offset, p, bytes);
            // 释放映射
            kunmap_atomic(p);
            // 移动内存盘的指针，准备写入下一个数据段
            offset += bytes;
        }
    } else {
        // --- 处理读请求 ---
        // 遍历请求中的每一个数据段(bvec)，将数据从我们的内存盘拷贝到请求缓冲区
        rq_for_each_segment(bvec, rq, iter) {
            size_t bytes = bvec.bv_len;
            // 获取数据段的内核虚拟地址
            char *p = kmap_atomic(bvec.bv_page) + bvec.bv_offset;
            // 从 offset (我们的内存盘) 拷贝到 p (请求的目的地)
            memcpy(p, offset, bytes);
            // 释放映射
            kunmap_atomic(p);
            // 移动内存盘的指针，准备读取下一个数据段
            offset += bytes;
        }
    }

    // 所有数据处理完毕，解锁
    spin_unlock(&sbd_lock);

    // 结束请求，通知内核我们已成功处理
    blk_mq_end_request(rq, BLK_STS_OK);

    return BLK_STS_OK;
}
// --- blk-mq 回调函数集 ---
static const struct blk_mq_ops sbd_mq_ops = {
    .queue_rq = sbd_queue_rq,
};

// --- 文件操作集 ---
static const struct block_device_operations sbd_fops = {
    .owner = THIS_MODULE,
};

static int __init sbd_init(void) {
    printk(KERN_INFO "SBD: Initializing Simple Ramdisk (blk-mq version)...\n");

    sbd_nr_sectors = disk_size_mb * 1024 * 1024 / KERNEL_SECTOR_SIZE;
    sbd_data = vzalloc(sbd_nr_sectors * KERNEL_SECTOR_SIZE);
    if (!sbd_data) return -ENOMEM;
    printk(KERN_INFO "SBD: Allocated %d MB of memory.\n", disk_size_mb);
    
    spin_lock_init(&sbd_lock);

    sbd_major = register_blkdev(0, SBD_DEVICE_NAME);
    if (sbd_major < 0) {
        printk(KERN_ERR "SBD: register_blkdev failed.\n");
        vfree(sbd_data);
        return sbd_major;
    }
    printk(KERN_INFO "SBD: Registered with major number %d.\n", sbd_major);

    sbd_disk = alloc_disk(SBD_MINORS);
    if (!sbd_disk) goto out_unregister_blkdev;

    // --- blk-mq 初始化流程 ---
    sbd_tag_set.ops = &sbd_mq_ops;
    sbd_tag_set.nr_hw_queues = 1; // 我们是简单的软件设备，一个硬件队列就够了
    sbd_tag_set.queue_depth = 128; // 队列深度
    sbd_tag_set.numa_node = NUMA_NO_NODE;
    sbd_tag_set.flags = BLK_MQ_F_SHOULD_MERGE;

    if (blk_mq_alloc_tag_set(&sbd_tag_set)) {
        goto out_put_disk;
    }

    sbd_queue = blk_mq_init_queue(&sbd_tag_set);
    if (IS_ERR(sbd_queue)) {
        blk_mq_free_tag_set(&sbd_tag_set);
        goto out_put_disk;
    }
    sbd_queue->queuedata = sbd_disk;

    // --- 配置 gendisk ---
    sbd_disk->major = sbd_major;
    sbd_disk->first_minor = 0;
    sbd_disk->fops = &sbd_fops;
    sbd_disk->queue = sbd_queue;
    sbd_disk->private_data = NULL;
    snprintf(sbd_disk->disk_name, 32, "%s0", SBD_DEVICE_NAME);
    set_capacity(sbd_disk, sbd_nr_sectors);

    add_disk(sbd_disk);
    printk(KERN_INFO "SBD: Ramdisk device %s created successfully.\n", sbd_disk->disk_name);

    return 0;

// --- 错误处理 ---
out_put_disk:
    put_disk(sbd_disk);
out_unregister_blkdev:
    unregister_blkdev(sbd_major, SBD_DEVICE_NAME);
    vfree(sbd_data);
    return -ENOMEM;
}

static void __exit sbd_exit(void) {
    printk(KERN_INFO "SBD: Exiting Simple Ramdisk...\n");

    del_gendisk(sbd_disk);
    put_disk(sbd_disk);
    blk_cleanup_queue(sbd_queue);
    blk_mq_free_tag_set(&sbd_tag_set);
    unregister_blkdev(sbd_major, SBD_DEVICE_NAME);
    vfree(sbd_data);

    printk(KERN_INFO "SBD: Ramdisk unloaded.\n");
}

module_init(sbd_init);
module_exit(sbd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AI Assistant");
MODULE_DESCRIPTION("A simple ramdisk block device driver (blk-mq).");
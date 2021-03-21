#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/vmalloc.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("cyy");
MODULE_DESCRIPTION("Kernel spy");

int spy_major = 0;

static char* spy_addr = "0";//addr in hex string
module_param(spy_addr,charp,0);

static int spy_ptr_nested = 0;
module_param(spy_ptr_nested,int,0);

static int spy_dev_size = 16*1024*1024;
module_param(spy_dev_size,int,0);


#define KERNEL_SECTOR_SIZE 512

struct spy_dev {
	unsigned char *data;
	struct request_queue *queue;
	struct gendisk *gd;
	struct blk_mq_tag_set tag_set;
} spy_dev;

static const struct block_device_operations spy_fops = {
	.owner = THIS_MODULE,
};

static blk_status_t spy_queue_rq(struct blk_mq_hw_ctx *hctx,const struct blk_mq_queue_data* bd) {
	blk_status_t ret = BLK_STS_OK;
	struct request *req = bd->rq;
	struct spy_dev *dev = req->rq_disk->private_data;
	sector_t rq_pos = blk_rq_pos(req);
	blk_mq_start_request(req);

	struct bio_vec bvec;
	struct req_iterator iter;
	rq_for_each_segment(bvec, req, iter) {
		size_t num_sector = blk_rq_cur_sectors(req);
		unsigned char *buffer = page_address(bvec.bv_page) + bvec.bv_offset;
		unsigned long offset = rq_pos * KERNEL_SECTOR_SIZE;
		if ((offset + num_sector*KERNEL_SECTOR_SIZE) <= spy_dev_size) {//avoid buffer overflow
			if (rq_data_dir(req) == WRITE) memcpy(dev->data+offset,buffer,num_sector*KERNEL_SECTOR_SIZE);
			else memcpy(buffer,dev->data+offset,num_sector*KERNEL_SECTOR_SIZE);
		}
		else {
			ret = BLK_STS_IOERR;
			goto end;
		}
		rq_pos += num_sector;
	}
end:
	blk_mq_end_request(req,ret);
	return ret;
}

static const struct blk_mq_ops spy_mq_ops = {
	.queue_rq = spy_queue_rq,
};

static void spy_add_device(unsigned char *spy_data,struct spy_dev *dev,int first_minor) {
	//find the memory position
	dev->data = spy_data;
	//init mq
	dev->queue = blk_mq_init_sq_queue(&dev->tag_set,&spy_mq_ops,128,BLK_MQ_F_SHOULD_MERGE);
	blk_queue_logical_block_size(dev->queue,KERNEL_SECTOR_SIZE);
	dev->queue->queuedata = dev;
	//init gendisk
	dev->gd = alloc_disk(spy_major);
	dev->gd->major = spy_major;
	dev->gd->first_minor = first_minor;
	dev->gd->fops = &spy_fops;
	dev->gd->private_data = dev;
	dev->gd->queue = dev->queue;
	sprintf(dev->gd->disk_name,"spy%d",first_minor);//the filename in /dev/
	set_capacity(dev->gd,spy_dev_size / KERNEL_SECTOR_SIZE);
	//add disk
	add_disk(dev->gd);
}

static int __init spy_init(void) {
	spy_major = register_blkdev(0,"spy");
	if (spy_major < 0) {
		printk(KERN_WARNING "register_blkdev spy failed.\n");
	}
	else {
		unsigned long long addr;
		sscanf(spy_addr,"%llx",&addr);
		unsigned long long *spy_data = (unsigned long long *)addr;
		printk(KERN_INFO "spy_addr = %llx\n",(unsigned long long)spy_data);
		int i;
		for (i=0;i<spy_ptr_nested;i++) spy_data = *spy_data;
		printk(KERN_INFO "spy_data_ptr = %llx\n",(unsigned long long)spy_data);
		spy_add_device((unsigned char *)spy_data,&spy_dev,0);
	}
	return 0;
}

static void __exit spy_exit(void) {
	if (spy_dev.queue) blk_cleanup_queue(spy_dev.queue);
	if (spy_dev.gd) {
		del_gendisk(spy_dev.gd);
		put_disk(spy_dev.gd);
	}
	unregister_blkdev(spy_major,"spy");
}

module_init(spy_init);
module_exit(spy_exit);

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/vmalloc.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("cyy");
MODULE_DESCRIPTION("Simple Ramdisk Module");

int myramdisk_major = 0;

#define myramdisk_ndev 2
const int myramdisk_dev_size = 16*1024*1024;//16M
#define KERNEL_SECTOR_SIZE 512

struct myramdisk_dev {
	unsigned char *data;
	struct request_queue *queue;
	struct gendisk *gd;
	struct blk_mq_tag_set tag_set;
} myramdisk_devs[myramdisk_ndev];

static const struct block_device_operations myramdisk_fops = {
	.owner = THIS_MODULE,
};

static blk_status_t myramdisk_queue_rq(struct blk_mq_hw_ctx *hctx,const struct blk_mq_queue_data* bd) {
	blk_status_t ret = BLK_STS_OK;
	struct request *req = bd->rq;
	struct myramdisk_dev *dev = req->rq_disk->private_data;
	sector_t rq_pos = blk_rq_pos(req);
	blk_mq_start_request(req);

	struct bio_vec bvec;
	struct req_iterator iter;
	rq_for_each_segment(bvec, req, iter) {
		size_t num_sector = blk_rq_cur_sectors(req);
		unsigned char *buffer = page_address(bvec.bv_page) + bvec.bv_offset;
		unsigned long offset = rq_pos * KERNEL_SECTOR_SIZE;
		if ((offset + num_sector*KERNEL_SECTOR_SIZE) <= myramdisk_dev_size) {//avoid buffer overflow
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

static const struct blk_mq_ops myramdisk_mq_ops = {
	.queue_rq = myramdisk_queue_rq,
};

static void myramdisk_add_device(struct myramdisk_dev *dev,int first_minor) {
	//alloc memory for ramdisk data
	dev->data = vmalloc(myramdisk_dev_size);
	printk(KERN_INFO "alloc memory ok\n");
	//init mq
	dev->queue = blk_mq_init_sq_queue(&dev->tag_set,&myramdisk_mq_ops,128,BLK_MQ_F_SHOULD_MERGE);
	blk_queue_logical_block_size(dev->queue,KERNEL_SECTOR_SIZE);
	dev->queue->queuedata = dev;
	printk(KERN_INFO "init mq ok\n");
	//init gendisk
	dev->gd = alloc_disk(myramdisk_major);
	dev->gd->major = myramdisk_major;
	dev->gd->first_minor = first_minor;
	dev->gd->fops = &myramdisk_fops;
	dev->gd->private_data = dev;
	dev->gd->queue = dev->queue;
	sprintf(dev->gd->disk_name,"myramdisk%d",first_minor);//the filename in /dev/
	set_capacity(dev->gd,myramdisk_dev_size / KERNEL_SECTOR_SIZE);
	printk(KERN_INFO "init gendisk ok\n");
	//add disk
	add_disk(dev->gd);
	printk(KERN_INFO "add disk ok\n");
}

static int __init myramdisk_init(void) {
	myramdisk_major = register_blkdev(0,"myramdisk");
	if (myramdisk_major < 0) {
		printk(KERN_WARNING "register_blkdev myramdisk failed.\n");
	}
	else {
		printk(KERN_INFO "register_blkdev myramdisk succeed, major = %d.\n",myramdisk_major);
	}

	int i;
	for (i=0;i<myramdisk_ndev;i++) 
		myramdisk_add_device(&myramdisk_devs[i],i*16);
	return 0;
}

static void __exit myramdisk_exit(void) {
	int i;
	for (i=0;i<myramdisk_ndev;i++) {
		if (myramdisk_devs[i].data) vfree(myramdisk_devs[i].data);
		if (myramdisk_devs[i].queue) blk_cleanup_queue(myramdisk_devs[i].queue);
		if (myramdisk_devs[i].gd) {
			del_gendisk(myramdisk_devs[i].gd);
			put_disk(myramdisk_devs[i].gd);
		}
	}
	unregister_blkdev(myramdisk_major,"myramdisk");
}

module_init(myramdisk_init);
module_exit(myramdisk_exit);

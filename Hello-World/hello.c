#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("cyy");
MODULE_DESCRIPTION("Hello World Module");

static int __init hello_init(void) {
	printk(KERN_INFO "Hello World from test module\n");
	return 0;
}

static void __exit hello_exit(void) {
	printk(KERN_INFO "Goodbye from test module\n");
}

module_init(hello_init);
module_exit(hello_exit);
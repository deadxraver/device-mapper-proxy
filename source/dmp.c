#include <linux/module.h>
#include <linux/printk.h>

#define MODULE_NAME "dmp"

#define LOG(fmt, ...) pr_info("[" MODULE_NAME "]: " fmt, ##__VA_ARGS__)

static int __init dmp_init(void) {
  LOG("init");
  return 0;
}

static void __exit dmp_exit(void) {
  LOG("exit");
}

module_init(dmp_init);
module_exit(dmp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("deadxraver");
MODULE_DESCRIPTION("Device Mapper Proxy");


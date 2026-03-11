#include <linux/module.h>
#include <linux/printk.h>
#include <linux/kobject.h>

#define MODULE_NAME "dmp"

#define LOG(fmt, ...) pr_info("[" MODULE_NAME "]: " fmt, ##__VA_ARGS__)

static int dmp_stat_calls = 0;
static struct kobject* dmp_module;

static ssize_t dmp_stat_show(struct kobject* kobj,
                             struct kobj_attribute* attr,
                             char* buf) {
  return sprintf(buf, "Nothing to see here yet, %d\n", ++dmp_stat_calls);
}

static ssize_t dmp_stat_store(struct kobject* kobj,
                             struct kobj_attribute* attr,
                             const char* buf, size_t count) {
  return -EPERM;
}

static struct kobj_attribute dmp_stat_attr = __ATTR(dmp_stat_calls, 0660, dmp_stat_show, dmp_stat_store);

static int __init dmp_init(void) {
  LOG("init");
  dmp_module = kobject_create_and_add("dmp_module", kernel_kobj);
  if (dmp_module == NULL)
    return -ENOMEM;
  int ret = sysfs_create_file(dmp_module, &dmp_stat_attr.attr);
  if (ret) {
    kobject_put(dmp_module);
    LOG("could not create file\n");
  }
  return ret;
}

static void __exit dmp_exit(void) {
  LOG("exit");
  kobject_put(dmp_module);
}

module_init(dmp_init);
module_exit(dmp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("deadxraver");
MODULE_DESCRIPTION("Device Mapper Proxy");


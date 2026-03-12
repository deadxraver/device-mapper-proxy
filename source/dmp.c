#include <linux/module.h>
#include <linux/printk.h>
#include <linux/kobject.h>

#include <linux/device-mapper.h>

#define MODULE_NAME "dmp"

#define LOG(fmt, ...) pr_info("[" MODULE_NAME "]: " fmt, ##__VA_ARGS__)

static int dmp_stat_calls = 0;
static struct kobject* dmp_module;

static int dmp_ctr(struct dm_target* ti, unsigned int argc, char* argv[]) {
  // TODO:
  LOG("ctr called");
  return -EINVAL;
}

static int dmp_map(struct dm_target* ti, struct bio* bio) {
  // TODO:
  LOG("map called");
  return -EINVAL;
}

static void dmp_io_hints(struct dm_target* ti, struct queue_limits* limits) {
  LOG("io hints called");
  // TODO:
}

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

static struct target_type dmp_target = {
	.name   = "dmp",
	.version = {1, 0, 0},
	.features = DM_TARGET_NOWAIT,
	.module = THIS_MODULE,
	.ctr    = dmp_ctr,
	.map    = dmp_map,
	.io_hints = dmp_io_hints,
};

static int __init dmp_init(void) {
  LOG("init");
  dm_register_target(&dmp_target);
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
  dm_unregister_target(&dmp_target);
}

module_init(dmp_init);
module_exit(dmp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("deadxraver");
MODULE_DESCRIPTION("Device Mapper Proxy");


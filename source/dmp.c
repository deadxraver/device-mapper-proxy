#include "dmp.h"

struct list {
  struct dmpstats stats;
  struct list* next;
  struct list* prev;
} list_head;

static struct kset* dmp_root;

static struct kobj_attribute dmp_stat_attr = __ATTR(dmp_stats, 0660, dmp_stat_show, dmp_stat_store);

static struct target_type dmp_target = {
  .name     = "dmp",
  .version  = {1, 0, 0},
  .features = DM_TARGET_NOWAIT,
  .module   = THIS_MODULE,
  .ctr      = dmp_ctr,
  .map      = dmp_map,
  .dtr      = dmp_dtr,
};

static void list_init(void) {
  list_head.next = &list_head;
  list_head.prev = &list_head;
}

static int list_push(struct dmpstats contents) {
  struct list* node = (struct list*) kvmalloc(sizeof(*node), GFP_KERNEL);
  if (node == NULL) {
    LOG("could not allocate node");
    return -ENOMEM;
  }
  node->prev = &list_head;
  node->next = list_head.next;
  list_head.next = node;
  node->next->prev = node;
  node->stats = contents;
  return 0;
}

static void list_destroy(void) {
  struct list* node = list_head.next;
  while (node != &list_head) {
    sysfs_remove_file(node->stats.module, &dmp_stat_attr.attr);
    kobject_put(node->stats.module);
    struct list* tmp = node->next;
    kvfree(node);
    node = tmp;
  }
}

static int dmp_ctr(struct dm_target* ti, unsigned int argc, char* argv[]) {
  LOG("ctr called");
  int ret;
  if (argc != 1) {
    LOG("expecting only path to device");
    ti->error = "expecting only path to device";
    return -EINVAL;
  }
  char* path = argv[0];
  char* name = path;
  for (char* c = path; *c; ++c) {
    if (*c == '/')
      name = c;
  }
  ++name; // skip `/`
  LOG("path: %s", path);
  LOG("name: %s", name);
  struct dmpstats dmp_stats = {0};
  ret = dm_get_device(ti, path, dm_table_get_mode(ti->table), &dmp_stats.ddev);
  if (ret) {
    LOG("error getting device");
    ti->error = "cannot open device";
    return ret;
  }
  struct kobject* dmp_module = kobject_create_and_add(name, &dmp_root->kobj);
  if (dmp_module == NULL) {
    LOG("could not create kobject for %s", name);
    ti->error = "could not create object";
    return -ENOMEM;
  }
  LOG("dmp_module kobject created");
  ret = sysfs_create_file(dmp_module, &dmp_stat_attr.attr);
  if (ret) {
    LOG("failed to create file");
    dm_put_device(ti, dmp_stats.ddev);
    kobject_put(dmp_module);
    ti->error = "could not create file";
    return ret;
  }
  LOG("file created");
  dmp_stats.module = dmp_module;
  if ((ret = list_push(dmp_stats))) {
    LOG("could not push to list, no mem");
    dm_put_device(ti, dmp_stats.ddev);
    ti->error = "could not push to list";
    kobject_put(dmp_module);
    return ret;
  }
  LOG("pushed to list");
  LOG("ctr OK, proceeding...");
  ti->private = list_head.next;
  LOG("NOTE: ti->private = 0x%lx", ti->private);
  return 0;
}

void dmp_dtr(struct dm_target* ti) {
  LOG("dtr called");
  struct list* node = (struct list*)ti->private;
  LOG("deleting %s", node->stats.module->name);
  node->prev->next = node->next;
  node->next->prev = node->prev;
  sysfs_remove_file(node->stats.module, &dmp_stat_attr.attr);
  kobject_put(node->stats.module);
  dm_put_device(ti, node->stats.ddev);
  kvfree(node);
  LOG("successfully deleted");
}

static int dmp_map(struct dm_target* ti, struct bio* bio) {
  LOG("map called");
  if (ti == NULL || ti->private == NULL) {
    LOG("ti or private is NULL, ti=0x%lx", ti);
    return -EINVAL;
  }
  LOG("NOTE: ti->private = 0x%lx", ti->private);
  struct list* node = (struct list*)ti->private;
  LOG("mapping %s", node->stats.module->name);
  // TODO:
  bio_set_dev(bio, node->stats.ddev->bdev);
  submit_bio_noacct(bio);
  return DM_MAPIO_SUBMITTED;
}

static ssize_t dmp_stat_show(struct kobject* kobj,
                             struct kobj_attribute* attr,
                             char* buf) {
  return sprintf(buf, "Nothing to see here yet(%s)\n", kobj->name);
}

static ssize_t dmp_stat_store(struct kobject* kobj,
                              struct kobj_attribute* attr,
                              const char* buf, size_t count) {
  return -EPERM;
}

static int __init dmp_init(void) {
  LOG("init");
  list_init();
  dm_register_target(&dmp_target);
  dmp_root = kset_create_and_add("dmp", NULL, kernel_kobj);
  if (dmp_root == NULL) {
    LOG("could not create dmp_root");
    return -ENOMEM;
  }
  LOG("registered dmp_target");
  return 0;
}

static void __exit dmp_exit(void) {
  LOG("exit");
  list_destroy();
  kset_put(dmp_root);
  dm_unregister_target(&dmp_target);
}

module_init(dmp_init);
module_exit(dmp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("deadxraver");
MODULE_DESCRIPTION("Device Mapper Proxy");


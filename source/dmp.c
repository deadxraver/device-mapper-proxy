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
    ERR("could not allocate node");
    return -ENOMEM;
  }
  node->prev = &list_head;
  node->next = list_head.next;
  list_head.next = node;
  node->next->prev = node;
  node->stats = contents;
  return 0;
}

static struct list* list_get_by_name(const char* name) {
  for (struct list* node = list_head.next; node != &list_head; node = node->next) {
    if (strcmp(node->stats.module->name, name) == 0)
      return node;
  }
  return NULL;
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
    WRN("expecting only path to device");
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
    WRN("error getting device");
    ti->error = "cannot open device";
    return ret;
  }
  struct kobject* dmp_module = kobject_create_and_add(name, &dmp_root->kobj);
  if (dmp_module == NULL) {
    ERR("could not create kobject for %s", name);
    ti->error = "could not create object";
    return -ENOMEM;
  }
  LOG("dmp_module kobject created");
  ret = sysfs_create_file(dmp_module, &dmp_stat_attr.attr);
  if (ret) {
    ERR("failed to create file");
    dm_put_device(ti, dmp_stats.ddev);
    kobject_put(dmp_module);
    ti->error = "could not create file";
    return ret;
  }
  LOG("file created");
  dmp_stats.module = dmp_module;
  if ((ret = list_push(dmp_stats))) {
    ERR("could not push to list, no mem");
    dm_put_device(ti, dmp_stats.ddev);
    ti->error = "could not push to list";
    kobject_put(dmp_module);
    return ret;
  }
  LOG("pushed to list");
  LOG("ctr OK, proceeding...");
  ti->private = list_head.next;
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
    ERR("ti or private is NULL, ti=0x%lx", (unsigned long)ti);
    return -EINVAL;
  }
  struct list* node = (struct list*)ti->private;
  LOG("mapping %s", node->stats.module->name);
  if (bio_op(bio) == REQ_OP_READ) {
    atomic_inc(&node->stats.r_reqs);
    atomic64_add(bio->bi_iter.bi_size, &node->stats.r_blk_sum);
  }
  if (bio_op(bio) == REQ_OP_WRITE) {
    atomic_inc(&node->stats.w_reqs);
    atomic64_add(bio->bi_iter.bi_size, &node->stats.w_blk_sum);
  }
  bio_set_dev(bio, node->stats.ddev->bdev);
  submit_bio_noacct(bio);
  return DM_MAPIO_SUBMITTED;
}

static ssize_t dmp_stat_show(struct kobject* kobj,
                             struct kobj_attribute* attr,
                             char* buf) {
  struct list* node = list_get_by_name(kobj->name);
  if (node == NULL) {
    ERR("could not find %s", kobj->name);
    return -ENOENT;
  }
  unsigned rr = atomic_read(&node->stats.r_reqs);
  unsigned wr = atomic_read(&node->stats.w_reqs);
  long rb = atomic64_read(&node->stats.r_blk_sum);
  long wb = atomic64_read(&node->stats.w_blk_sum);
  return sprintf(
    buf,
    "read:\n"
    "\treqs: %u\n"
    "\tavg size: %ld\n"
    "write:\n"
    "\treqs: %u\n"
    "\tavg size: %ld\n"
    "total:\n"
    "\treqs: %u\n"
    "\tavg size: %ld\n"
    ,
    rr,
    rr == 0 ? 0 : (rb / rr),
    wr,
    wr == 0 ? 0 : (wb / wr),
    rr + wr,
    (rr + wr) == 0 ? 0 : ((rb + wb) / (rr + wr))
  );
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
    ERR("could not create dmp_root");
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


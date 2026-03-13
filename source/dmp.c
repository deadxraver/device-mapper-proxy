#include "dmp.h"

#include <linux/container_of.h>

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

static struct kobj_type dmp_kobj_type = {
  .release = dmp_kobj_release,
  .sysfs_ops = &kobj_sysfs_ops,
};

static void dmp_kobj_release(struct kobject* kobj) {
  LOG("kobject %s release", kobj->name);
}

static int dmp_ctr(struct dm_target* ti, unsigned int argc, char* argv[]) {
  LOG("ctr called");
  int ret;
  struct kobject* dmp_module = NULL;
  struct dmpstats* dmp_stats = NULL;
  ti->private = NULL;
  if (argc != 1) {
    WRN("expecting only path to device");
    ti->error = "expecting only path to device";
    ret = -EINVAL;
    goto error;
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
  dmp_stats = (struct dmpstats*)kvmalloc(sizeof(struct dmpstats), GFP_KERNEL);
  if (dmp_stats == NULL) {
    ERR("could not allocate memory for dmp_stats");
    ti->error = "not enough memory";
    ret = -ENOMEM;
    goto error;
  }
  atomic_set(&dmp_stats->r_reqs, 0);
  atomic_set(&dmp_stats->w_reqs, 0);
  atomic64_set(&dmp_stats->r_blk_sum, 0);
  atomic64_set(&dmp_stats->w_blk_sum, 0);
  dmp_stats->ddev = NULL;
  ti->private = dmp_stats;
  ret = dm_get_device(ti, path, dm_table_get_mode(ti->table), &dmp_stats->ddev);
  if (ret) {
    WRN("error getting device");
    ti->error = "cannot open device";
    goto error;
  }
  dmp_module = &dmp_stats->module;
  ret = kobject_init_and_add(dmp_module, &dmp_kobj_type, &dmp_root->kobj, "%s", name);
  if (ret) {
    ERR("could not init kobject");
    dmp_module = NULL;
    ti->error = "could not init kobject";
    goto error;
  }
  ret = sysfs_create_file(dmp_module, &dmp_stat_attr.attr);
  if (ret) {
    ERR("failed to create sysfs file");
    ti->error = "failed to create sysfs file";
    goto error;
  }
  LOG("file created");
  return 0;
error:
  if (dmp_stats && dmp_stats->ddev) {
    dm_put_device(ti, dmp_stats->ddev);
  }
  if (dmp_module) {
    kobject_put(dmp_module);
  }
  if (ti->private) {
    kvfree(ti->private);
    ti->private = NULL;
  }
  return ret;
}

static void dmp_dtr(struct dm_target* ti) {
  LOG("dtr called");
  if (ti->private == NULL) {
    WRN("called dtr for ti with NULL private field");
    return;
  }
  struct dmpstats* dmp_stats = (struct dmpstats*)ti->private;
  sysfs_remove_file(&dmp_stats->module, &dmp_stat_attr.attr);
  kobject_put(&dmp_stats->module);
  dm_put_device(ti, dmp_stats->ddev);
  kvfree(dmp_stats);
  ti->private = NULL;
  LOG("successfully deleted");
}

static int dmp_map(struct dm_target* ti, struct bio* bio) {
  LOG("map called");
  if (ti == NULL || ti->private == NULL) {
    ERR("ti or private is NULL, ti=0x%lx", (unsigned long)ti);
    return -EINVAL;
  }
  struct dmpstats* dmp_stats = (struct dmpstats*)ti->private;
  LOG("mapping %s", dmp_stats->module.name);
  if (bio_op(bio) == REQ_OP_READ) {
    atomic_inc(&dmp_stats->r_reqs);
    atomic64_add(bio->bi_iter.bi_size, &dmp_stats->r_blk_sum);
  }
  if (bio_op(bio) == REQ_OP_WRITE) {
    atomic_inc(&dmp_stats->w_reqs);
    atomic64_add(bio->bi_iter.bi_size, &dmp_stats->w_blk_sum);
  }
  bio_set_dev(bio, dmp_stats->ddev->bdev);
  submit_bio_noacct(bio);
  return DM_MAPIO_SUBMITTED;
}

static ssize_t dmp_stat_show(struct kobject* kobj,
                             struct kobj_attribute* attr,
                             char* buf) {
  struct dmpstats* dmp_stats = container_of(kobj, struct dmpstats, module);
  unsigned rr = atomic_read(&dmp_stats->r_reqs);
  unsigned wr = atomic_read(&dmp_stats->w_reqs);
  long rb = atomic64_read(&dmp_stats->r_blk_sum);
  long wb = atomic64_read(&dmp_stats->w_blk_sum);
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
  kset_put(dmp_root);
  dm_unregister_target(&dmp_target);
}

module_init(dmp_init);
module_exit(dmp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("deadxraver");
MODULE_DESCRIPTION("Device Mapper Proxy");


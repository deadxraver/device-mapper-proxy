#ifndef _DMP_H

#define _DMP_H

#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/mm.h>
#include <linux/types.h>

#include <linux/device-mapper.h>

#define MODULE_NAME "dmp"

#define LOG(fmt, ...) pr_info("[" MODULE_NAME "]: " fmt, ##__VA_ARGS__)
#define WRN(fmt, ...) pr_warn("[" MODULE_NAME "]: " fmt, ##__VA_ARGS__)
#define ERR(fmt, ...) pr_err("[" MODULE_NAME "]: " fmt, ##__VA_ARGS__)

struct dmpstats {
  struct kobject module;
  struct dm_dev* ddev;
  atomic_t r_reqs;  // read requests
  atomic_t w_reqs;  // write requests
  atomic64_t r_blk_sum; // sum of blocks to read
  atomic64_t w_blk_sum; // sum of blocks to write
};

static void dmp_kobj_release(struct kobject* kobj);

static int dmp_ctr(struct dm_target* ti, unsigned int argc, char* argv[]);

static void dmp_dtr(struct dm_target* ti);

static int dmp_map(struct dm_target* ti, struct bio* bio);

static ssize_t dmp_stat_show(struct kobject* kobj,
                             struct kobj_attribute* attr,
                             char* buf);

static ssize_t dmp_stat_store(struct kobject* kobj,
                              struct kobj_attribute* attr,
                              const char* buf, size_t count);

#endif // !_DMP_H

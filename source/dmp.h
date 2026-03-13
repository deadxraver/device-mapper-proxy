#ifndef _DMP_H

#define _DMP_H

#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/mm.h>

#include <linux/device-mapper.h>

#define MODULE_NAME "dmp"

#define LOG(fmt, ...) pr_info("[" MODULE_NAME "]: " fmt, ##__VA_ARGS__)

// consts for map return
#define DMP_RESUBMIT  0
#define DMP_COMPLETE  1
#define DMP_PUSH_BACK 2

struct dmpstats {
  struct kobject* module;
  struct dm_dev* ddev;
  unsigned r_reqs;  // read requests
  unsigned w_reqs;  // write requests
  size_t r_blk_sum; // sum of blocks to read
  size_t w_blk_sum; // sum of blocks to write
};

static int dmp_ctr(struct dm_target* ti, unsigned int argc, char* argv[]);

void dmp_dtr(struct dm_target* ti);

static int dmp_map(struct dm_target* ti, struct bio* bio);

static ssize_t dmp_stat_show(struct kobject* kobj,
                             struct kobj_attribute* attr,
                             char* buf);

static ssize_t dmp_stat_store(struct kobject* kobj,
                              struct kobj_attribute* attr,
                              const char* buf, size_t count);

#endif // !_DMP_H

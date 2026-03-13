# Device Mapper Proxy

## Setup VM (if needed)

```bash
QEMU=qemu-system-riscv64
IMG=debian-13-nocloud-riscv64
URL=https://cloud.debian.org/images/cloud/trixie/latest/$IMG.tar.xz

wget $URL
tar -xpJf $IMG.tar.xz
qemu-img resize -f raw disk.raw +5G

$QEMU \
	-machine virt \
	-nographic \
	-m 5G \
	-smp $(shell nproc) \
	-bios default \
	-kernel /usr/lib/u-boot/qemu-riscv64_smode/uboot.elf \
	-device virtio-rng-pci \
	-drive file=disk.raw,format=raw,if=virtio \
	-device virtio-net-device,netdev=net \
	-netdev user,id=net,hostfwd=tcp::2222-:22
```

Setup password, etc. ...

```bash
apt-get update
apt install -y ssh
```

Set `PermitRootLogin` to `yes` in `/etc/ssh/sshd_config`

## Dependencies

On VM (or local machine if previous step skipped) do

```bash
sudo apt-get update
sudo apt install build-essential linux-headers-`uname -r` dmsetup
```

## Build & Test

If using VM, first run from **local machine**:
```bash
cd .. && scp -P 2222 -r device-mapper-proxy/ root@localhost:~
```

```bash
make
sudo lsmod | grep dm_mod
sudo modprobe dm_mod # if previous showed nothing
sudo insmod source.dmp.ko
```

```bash
sudo dmsetup create zero1 --table "0 10000 zero"
ls -la /dev/mapper/ | grep zero
sudo dmsetup create dmp1 --table "0 10000 dmp /dev/mapper/zero1"
```

```bash
ls -la /sys/kernel/dmp/
# total 0
# drwxr-xr-x  3 root root 0 Mar 13 09:32 .
# drwxr-xr-x 15 root root 0 Mar 13 09:32 ..
# drwxr-xr-x  2 root root 0 Mar 13 09:32 zero1
cat /sys/kernel/dmp/zero1/dmp_stats
```
```
# Output:
 read:
	reqs: 62
	avg size: 4096
write:
	reqs: 0
	avg_size: 0
total:
	reqs: 62
	avg_size: 4096
```

```bash
dd if=/dev/urandom of=/dev/mapper/dmp1 oflag=direct bs=4K count=1
dd if=/dev/mapper/dmp1 of=/dev/null iflag=direct bs=4K count=1
cat /sys/kernel/dmp/zero1/dmp_stats
```
```
# Output:
read:
	reqs: 124
	avg size: 4096
write:
	reqs: 1
	avg_size: 4096
total:
	reqs: 125
	avg_size: 4096
```

## Cleanup

If you have created any dmp devices, they should first be removed before removing the module.

```bash
sudo dmsetup remove <dmp_name>
sudo dmsetup remove dmp1 # dev from the example
```

Remove module by:
```bash
sudo rmmod dmp
```

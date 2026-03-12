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
sudo insmod source.dmp.ko
sudo lsmod | grep dm_map
```

If `dm_map` was not found, `sudo modprobe dm_map`.

```bash
sudo dmsetup create zero1 --table "0 1 zero"
ls -la /dev/mapper/ | grep zero
sudo dmsetup create dmp1 --table "0 1 dmp /dev/mapper/zero1"
```

Currently `EINVAL` would be thrown after this.

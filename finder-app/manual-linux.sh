#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    ### Add your kernel build steps here
    echo "Build my kernel"
    make -j6 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    make -j6 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    ### Fix multiple definition of 'yylloc'
    cp ${FINDER_APP_DIR}/fix_multiple_def_yyloc.patch .
    git apply fix_multiple_def_yyloc.patch
    make -j6 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    make -j6 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    make -j6 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs

fi

echo "Adding the Image in outdir"
cd "$OUTDIR"
cp linux-stable/arch/${ARCH}/boot/Image $OUTDIR

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

### Create necessary base directories
cd "$OUTDIR"
mkdir -p rootfs && cd rootfs
### Add linux rootfs
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    ###  Configure busybox
    echo "Build my busybox"
    make -j6 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} distclean
    make -j6 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
else
    cd busybox
fi

### Make and install busybox
make -j6 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX=${OUTDIR}/rootfs install
cd "$OUTDIR"/rootfs

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
#       [Requesting program interpreter: /lib/ld-linux-aarch64.so.1]
#  0x0000000000000001 (NEEDED)             Shared library: [libm.so.6]
#  0x0000000000000001 (NEEDED)             Shared library: [libresolv.so.2]
#  0x0000000000000001 (NEEDED)             Shared library: [libc.so.6]

# ${CROSS_COMPILE}readelf -a bin/busybox  | grep "interpreter" | grep -oE '/[^]]+' | sed 's/^\///'
# abb=$(${CROSS_COMPILE}readelf -a bin/busybox  | grep "interpreter" | xargs -n 1 basename)
P=$(which ${CROSS_COMPILE}gcc)
P=$(dirname $P)/../
find "$P" -name ld-linux-aarch64.so.1 -exec cp {} "$OUTDIR/rootfs/lib" \;
find "$P" -name libm.so.6 -exec cp {} "$OUTDIR/rootfs/lib64" \;
find "$P" -name libresolv.so.2 -exec cp {} "$OUTDIR/rootfs/lib64" \;
find "$P" -name libc.so.6 -exec cp {} "$OUTDIR/rootfs/lib64" \;

# ${CROSS_COMPILE}readelf -a bin/busybox  | grep "Shared library" | grep -oE '\[.*\]' | tr -d '[]'

### Make device nodes
cd "$OUTDIR/rootfs"
sudo mknod -m 666 dev/null c 1 3 # null device
sudo mknod -m 666 dev/console c 5 1 # console device
sudo mknod dev/ram b 1 0 

### Clean and build the writer utility
cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

### Copy the finder related scripts and executables to the /home directory
cd "$OUTDIR/rootfs/home"
cp -r ${FINDER_APP_DIR}/../conf .
cp ${FINDER_APP_DIR}/writer.sh .
cp ${FINDER_APP_DIR}/finder.sh .
cp ${FINDER_APP_DIR}/finder-test.sh .
cp ${FINDER_APP_DIR}/autorun-qemu.sh .

### Chown the root directory
# sudo chown -R root:root "$OUTDIR/rootfs/home"

### Create initramfs.cpio.gz
cd "$OUTDIR/rootfs"
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ..
gzip -f initramfs.cpio


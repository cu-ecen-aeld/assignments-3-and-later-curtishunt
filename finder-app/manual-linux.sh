#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
REPO_ROOT=$(pwd)
#FINDER_APP_DIR=${REPO_ROOT}/finder-app
export PATH=/home/chunt/toolchain/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/bin:$PATH

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

    # TODO: Add your kernel build steps here
    #Deep clean the kernel build tree
    echo "cleaning kernel build"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    #Configure for virt arm dev board to be simulated with Qemu	
    echo "configuring compilation for virtual hw to be simulated with qemu"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    #Build a kernel image for booting with Qemu	
    echo "building kernel image"
    make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    #Build any kernel modules
    echo "building kernel modules"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    #Build the device tree
    echo "building device tree"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

echo "Adding the Image in outdir"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p rootfs/bin rootfs/dev rootfs/etc rootfs/home rootfs/lib rootfs/lib64 rootfs/proc rootfs/sbin rootfs/sys rootfs/tmp rootfs/usr/bin rootfs/usr/sbin rootfs/usr/lib rootfs/var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    echo "cloning into busybox"
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean

    make defconfig
    # Enable static build for BusyBox
    sed -i 's/^# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
    # Make sure no conflicting options exist
    sed -i 's/^CONFIG_STATIC=.*/CONFIG_STATIC=y/' .config
    
else
    echo "busybox repo already exists"
    cd busybox
fi

# TODO: Make and install busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

# TODO: Add library dependencies to rootfs
echo "Adding library dependencies to rootfs"

BUSYBOX_BIN=${OUTDIR}/rootfs/bin/busybox

SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)

# Extract shared library dependencies from busybox using readelf
LIBS=$(${CROSS_COMPILE}readelf -a ${BUSYBOX_BIN} | grep "Shared library" | awk -F'[][]' '{print $2}')
echo "Shared lib dependencies: ${LIBS}"
# Create lib and lib64 directories if not exist
mkdir -p ${OUTDIR}/rootfs/lib
mkdir -p ${OUTDIR}/rootfs/lib64

# Copy each needed library
for i in $LIBS; do
    LIB_PATH=$(find $SYSROOT -name "$i" | head -n 1)
    if [ -z "$LIB_PATH" ]; then
        echo "ERROR: Could not find necessar file: $i"
        exit 1
    fi

    echo "getting file: $i from $LIB_PATH"

    # Copy the file
    cp -u --parents "$LIB_PATH" ${OUTDIR}/rootfs
done

# TODO: Make device nodes
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 600 ${OUTDIR}/rootfs/dev/console c 5 1

# TODO: Clean and build the writer utility
echo "REPO ROOT IS: ${REPO_ROOT}"
echo "OUTDIR IS: ${OUTDIR}"
cd ${REPO_ROOT}
echo "FILES IN REPO ROOT: $(ls .)"
echo "Files in conf: $(ls conf/)"
echo "Compiling writer application..."
make clean
make CROSS_COMPILE=${CROSS_COMPILE}


# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp writer ${OUTDIR}/rootfs/home/
cp finder.sh ${OUTDIR}/rootfs/home/
cp finder-test.sh ${OUTDIR}/rootfs/home/
cp -rL conf ${OUTDIR}/rootfs/home/
cp autorun-qemu.sh ${OUTDIR}/rootfs/home/
#change paths from ..conf/ to just conf/ in finder-test.sh
sed -i 's|\.\./conf|conf|' ${OUTDIR}/rootfs/home/finder-test.sh

# TODO: Chown the root directory
cd ${OUTDIR}/rootfs
#changes user to root and group to root
sudo chown -R root:root *

rm -f init sbin/init 
ln -sf bin/busybox init

# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}
echo "FILES IN OUTDIR: $(ls .)"
cd ${OUTDIR}/rootfs
echo "FILES IN OUTDIR/rootfs: $(ls .)"
echo "Creating initramfs..."
if find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio; then
    gzip -f ${OUTDIR}/initramfs.cpio
    echo "initramfs.cpio.gz created at ${OUTDIR}/initramfs.cpio.gz"
else
    echo "ERROR: Failed to create initramfs.cpio"
    exit 1
fi

cp ${OUTDIR}/linux-stable/arch/arm64/boot/Image ${OUTDIR}/

cd ${REPO_ROOT}



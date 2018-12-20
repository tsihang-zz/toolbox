
ramdir=/mnt/mem/
echo "Mount ramfs to $ramdir"
mkdir $ramdir
mount -t ramfs none $ramdir

echo "Prepare eMMC image $1"
cp $1 $ramdir
cd $ramdir
tar -xvf $1

echo "Starting eMMC programming ..."

dd if=$1 of=/dev/mmcblk1

echo "eMMC programming finished"



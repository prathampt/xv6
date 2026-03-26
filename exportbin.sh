# compile and export binaries for each CPU
# to MNTPATH

set -e

if [ $# -ne 1 ]
then
  echo "usage: $0 <kernel_name>"
  exit 0
fi

MNTPATH=/media/pratham/xv6/
BOOTPATH=boot/$1_
GRUB_CONF=$MNTPATH/boot/grub/grub.cfg

cp param.h tmpparam.h
grep "#define NCPU 1" param.h

cpus=1

while [ $cpus -le 4 ]
do
  echo "compiling image for $cpus CPUS"
  make clean 1>/dev/null 2>/dev/null
  make kernelmemfs 1>/dev/null 2>/dev/null
  sudo cp kernelmemfs $MNTPATH$BOOTPATH$cpus
  echo "copied image to $MNTPATH$BOOTPATH$cpus"

  ENTRY_NAME=$1_$cpus
  if ! grep -q "menuentry \"$ENTRY_NAME\"" "$GRUB_CONF"; then
    echo "Adding GRUB entry for $ENTRY_NAME..."

    cat <<EOF | sudo tee -a "$GRUB_CONF" > /dev/null

menuentry "$ENTRY_NAME" {
    insmod part_msdos
    insmod ext2
    set root='hd0,msdos1'
    echo "Loading $1 ($cpus CPU)"
    multiboot /$BOOTPATH$cpus /$BOOTPATH$cpus
    boot
}
EOF
  else
    echo "GRUB entry for $ENTRY_NAME already exists. Skipping."
  fi

  # update param.h for next NCPU value
  oldcpus=$cpus
  cpus=`expr $cpus + 1`
  sed -i s/NCPU\ $oldcpus/NCPU\ $cpus/g param.h 1>/dev/null
  grep NCPU param.h
done

cp tmpparam.h param.h

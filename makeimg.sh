echo -e "Copying zImage to output directory.....\n"
cp ~/Xiaomi/arch/arm/boot/zImage ~/Output/R2-KK/raw/
cp ~/Xiaomi/arch/arm/boot/zImage ~/Output/R2-KK/zip/
echo "File found started to copy Kernel files...."
echo -e "Copying dt.img to output directory.....\n"
cp ~/Xiaomi/arch/arm/boot/dt.img ~/Output/R2-KK/raw/
echo "File found started to copy files...."
cd ~/Output/R2-KK/convert
echo "Cleaning up the directory for the fresh build......"
./cleanup.sh
echo "Unpacking boot image now......"
./unpackimg.sh boot.img
echo "Moving and replacing the files for repacking...."
mv ~/Output/R2-KK/raw/zImage ~/Output/R2-KK/convert/split_img/boot.img-zImage
mv ~/Output/R2-KK/raw/dt.img ~/Output/R2-KK/convert/split_img/boot.img-dtb
echo -e "Done replacing......\n\n"
echo "Now repacking kernel to boot.img..........."
./repackimg.sh
echo "Geting your boot.img file......"
cp ~/Output/R2-KK/convert/image-new.img ~/Output/R2-KK/image/
cd ~/Output/R2-KK/image/
build_date=$(date '+%d%m%y')
for file in image-new*
do
  mv "$file" "boot-$build_date${file#image-new}"
done
nautilus ~/Output/R2-KK/image/

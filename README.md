name: Build KernelSU Next + SUSFS
run-name: Building Kernel ✅
on:
  workflow_dispatch:

jobs:
  build:
    name: Build Kernel
    runs-on: ubuntu-latest
    steps:
      - name: ⬇️ Cài đặt công cụ
        run: |
          sudo apt-get update -y
          sudo apt-get install -y git bc bison build-essential libssl-dev libelf-dev zip curl wget python3 python-is-python3 gcc-aarch64-linux-gnu libncurses5-dev device-tree-compiler libstdc++-12-dev

      - name: 📥 Tải nguồn Kernel
        run: |
          git clone --depth=1 LINK_NGUON_DUNG kernel
          cd kernel
          echo "✅ Tải xong nguồn"
          ls -la

      - name: 🧠 Tích hợp KernelSU Next
        run: |
          cd kernel
          curl -LSs "https://raw.githubusercontent.com/rifsxd/KernelSU-Next/next/kernel/setup.sh" | bash -s next
          echo "✅ KernelSU Next đã thêm"

      - name: 🔒 Thêm SUSFS
        run: |
          cd kernel
          git clone https://github.com/SUSFS/susfs.git susfs || true
          echo "✅ SUSFS đã thêm"

      - name: ⚙️ Cấu hình
        run: |
          cd kernel
          make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- ten_defconfig_dung
          echo "CONFIG_KSU=y" >> .config
          echo "CONFIG_KSU_SUSFS=y" >> .config
          echo "CONFIG_SUSFS=y" >> .config
          echo "CONFIG_SUSFS_DEBUG=n" >> .config
          echo "CONFIG_SUSFS_HIDE_KSU=y" >> .config

      - name: 🔨 Build
        run: |
          cd kernel
          make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j$(nproc)

      - name: ✅ Kết quả
        run: |
          cd kernel/arch/arm64/boot
          ls -lh Image*

      - name: 📤 Tải file Image
        uses: actions/upload-artifact@v4
        with:
          name: Kernel-Image-Thanh-Cong
          path: kernel/arch/arm64/boot/Image
          retention-days: 60

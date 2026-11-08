# Frequently Asked Questions (FAQ)
## What is Xiaomi Kernel OpenSource?
This repository contains Linux kernel source code released by Xiaomi for supported Xiaomi, Redmi, and POCO devices. Developers can use these sources to build custom kernels and contribute improvements.
## What is a kernel source?
The Linux kernel is the core of the Android operating system. Kernel source code allows developers to build custom kernels, fix bugs, and improve hardware support.
## Which branch should I use?
Each device has its own kernel branch.

Find your device in the README and use the branch listed for your device.
## How do I know my device codename?
You can check your device codename using:

adb shell getprop ro.product.device

Then search for that codename in the README.
## Why isn't my device listed?
Possible reasons include:

- Xiaomi has not released the kernel source.
- The repository has not yet been updated.
- Your device uses another codename.
## What compiler is required?
The required compiler depends on the specific kernel branch. Refer to the branch documentation or build scripts for the recommended compiler version.
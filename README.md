# kitun-ix
An unix-like hobby operating system.

It uses the ZNU kernel (a kernel made by myself) to run. Most things are handled by it.

IMPORTANT NOTE: AI assistance WAS USED to help guide the project. THIS PROJECT IS NOT VIBECODED.

* Is now bootable (without shell)
* If you're wondering why theres so much python code it's for the `make menuconfig`

To make, do `make` in the ZNU directory.
To run in QEMU, do `make run` (in the ZNU directory).

Latest important update:
* Alphabet added
* Use `printk()` in the kernel to use it
* When you boot into it, if you had the `drivers/keyboards/keyboard` usabled, it will allow you to type.

# To-Do:
* Add a shell
  This will make it so you can boot into it and make/edit some files, but to do that we of
  course need:
* Filesystem
  Self explanatory, will likely use Fat32.

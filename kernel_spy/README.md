# Spy

## Usage

1. Get the address you want to spy
	
	Tips: You can get the address through `cat /proc/kallsyms` by root.

2. `sudo insmod spy.ko spy_addr=$spy_addr spy_ptr_nested=0 spy_dev_size=16777216`

	- `ptr_nested` is designed for nested pointer
	- The unit of `spy_dev_size` is byte.

3. Use `dd` to `read` or `write` the memory.


## Examples

[https://blog.cyyself.name/linux-kernel-memory-spy-module/](https://blog.cyyself.name/linux-kernel-memory-spy-module/)

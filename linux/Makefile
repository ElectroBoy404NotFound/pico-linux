all : image

buildroot:
	git clone https://github.com/tvlad1234/buildroot --recurse-submodules --depth 1
	mkdir buildroot/output
	mkdir buildroot/output/target
	cp -a configs/custom_kernel_config buildroot/kernel_config
	cp -a configs/buildroot_config buildroot/.config
	cp -a configs/busybox_config buildroot/busybox_config
	cp -a configs/rootfsoverlay/* buildroot/output/target/

toolchain:
	make -C buildroot

image: toolchain
	make -C c4

updateConfig:
	rm configs/custom_kernel_config      || true
	rm configs/buildroot_config          || true
	rm configs/busybox_config            || true
	rm configs/rootfsoverlay/etc/inittab || true
	
	cp -a buildroot/output/build/linux-6.1.14/.config configs/custom_kernel_config
	cp -a buildroot/output/build/busybox-1.36.0/.config configs/busybox_config
	cp -a buildroot/output/target/etc/inittab configs/rootfsoverlay/etc/inittab
	cp -a buildroot/.config configs/buildroot_config
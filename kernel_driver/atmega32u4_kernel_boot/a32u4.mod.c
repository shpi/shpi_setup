#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

MODULE_INFO(intree, "Y");

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x5179cf67, "module_layout" },
	{ 0xb0cc8424, "usb_deregister" },
	{ 0x373010d8, "usb_register_driver" },
	{ 0xedbd4eba, "kmalloc_caches" },
	{ 0x86332725, "__stack_chk_fail" },
	{ 0x250dd13b, "_dev_info" },
	{ 0x3de43f89, "_dev_err" },
	{ 0x45f25b96, "usb_control_msg" },
	{ 0xd35ad39b, "usb_get_dev" },
	{ 0xc2c35de3, "kmem_cache_alloc" },
	{ 0x8f678b07, "__stack_chk_guard" },
	{ 0xefd6cf06, "__aeabi_unwind_cpp_pr0" },
	{ 0x37a0cba, "kfree" },
	{ 0xe61ab27d, "usb_put_dev" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("usb:v03EBp2FF4d*dc*dsc*dp*ic*isc*ip*in*");

MODULE_INFO(srcversion, "D988DEB7C4B5C79DD8697ED");

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/delay.h>

#include "peach.h"
#include "guest.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ScratchLab");

static dev_t peach_dev;
static struct cdev peach_cdev;

static long peach_ioctl(struct file *file,
			unsigned int cmd,
			unsigned long data);
static struct file_operations peach_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = peach_ioctl,
};

struct vmcs_hdr {
	u32 revision_id:31;
	u32 shadow:1;
};

#define VMX_SIZE_MAX 4096
struct vmcs {
	struct vmcs_hdr hdr;
	u32 abort;
	char data[VMX_SIZE_MAX - 8];
};

static struct vmcs *vmxon;
static struct vmcs *vmcs;

static u8 *stack;

#define GUEST_MEMORY_SIZE (0x1000 * 16)
static u8 *guest_memory;

#define EPT_MEMORY_SIZE (0x1000 * 4)
static unsigned char *ept_memory;

static void init_ept(u64 *ept_pointer, u64 guest_memory_pa);
static void init_ept_pointer(u64 *p, u64 pa);
static void init_pml4e(u64 *entry, u64 pa);
static void init_pdpte(u64 *entry, u64 pa);
static void init_pde(u64 *entry, u64 pa);
static void init_pte(u64 *entry, u64 pa);

void _vmexit_handler(void);

struct guest_regs {
	u64 rax;
	u64 rcx;
	u64 rdx;
	u64 rbx;
	u64 rbp;
	u64 rsp;
	u64 rsi;
	u64 rdi;
	u64 r8;
	u64 r9;
	u64 r10;
	u64 r11;
	u64 r12;
	u64 r13;
	u64 r14;
	u64 r15;
};
static void dump_guest_regs(struct guest_regs *regs);

static u64 shutdown_rsp;
static u64 shutdown_rbp;

static int peach_init(void)
{
	printk("PEACH INIT\n");

	peach_dev = MKDEV(PEACH_MAJOR, PEACH_MINOR);
	if (0 < register_chrdev_region(peach_dev, PEACH_COUNT, "peach")) {
		printk("register_chrdev_region error\n");

		goto err0;
	}

	cdev_init(&peach_cdev, &peach_fops);
	peach_cdev.owner = THIS_MODULE;

	if (0 < cdev_add(&peach_cdev, peach_dev, 1)) {
		printk("cdev_add error\n");

		goto err1;
	}

	return 0;

err1:
	unregister_chrdev_region(peach_dev, 1);

err0:

	return -1;
}

static void peach_exit(void)
{
	printk("PEACH EXIT\n");

	cdev_del(&peach_cdev);
	unregister_chrdev_region(peach_dev, 1);

	return;
}

static long peach_ioctl(struct file *file,
			unsigned int cmd,
			unsigned long arg)
{
	int i;

	u8 ret1;

	u32 edx, eax, ecx;
	u64 rdx;

	u64 vmcs_pa;
	u64 vmxon_pa;

	u8 xdtr[10];
	u64 vmcs_field;
	u64 vmcs_field_value;

	u64 host_tr_selector;
	u64 host_gdt_base;
	u64 host_tr_desc;

	u64 ept_pointer;
	u64 guest_memory_pa;

	switch (cmd) {
	case PEACH_PROBE:
		printk("PEACH PROBE\n");

		ecx = 0x480;
		asm volatile (
			"rdmsr\n\t"
			: "=a" (eax), "=d" (edx)
			: "c" (ecx)
		);
		printk("IA32_VMX_BASIC = 0x%08x%08x\n", edx, eax);

		ecx = 0x486;
		asm volatile (
			"rdmsr\n\t"
			: "=a" (eax), "=d" (edx)
			: "c" (ecx)
		);
		printk("IA32_VMX_CR0_FIXED0 = 0x%08x%08x\n", edx, eax);

		ecx = 0x487;
		asm volatile (
			"rdmsr\n\t"
			: "=a" (eax), "=d" (edx)
			: "c" (ecx)
		);
		printk("IA32_VMX_CR0_FIXED1 = 0x%08x%08x\n", edx, eax);

		ecx = 0x488; 
		asm volatile (
			"rdmsr\n\t"
			: "=a" (eax), "=d" (edx)
			: "c" (ecx)
		);
		printk("IA32_VMX_CR4_FIXED0 = 0x%08x%08x\n", edx, eax);

		ecx = 0x489;
		asm volatile (
			"rdmsr\n\t"
			: "=a" (eax), "=d" (edx)
			: "c" (ecx)
		);
		printk("IA32_VMX_CR4_FIXED1 = 0x%08x%08x\n", edx, eax);

		ecx = 0x48D; 
		asm volatile (
			"rdmsr\n\t"
			: "=a" (eax), "=d" (edx)
			: "c" (ecx)
		);
		printk("IA32_VMX_TRUE_PINBASED_CTLS = 0x%08x%08x\n", edx, eax);

		ecx = 0x48E; 
		asm volatile (
			"rdmsr\n\t"
			: "=a" (eax), "=d" (edx)
			: "c" (ecx)
		);
		printk("IA32_VMX_TRUE_PROCBASED_CTLS = 0x%08x%08x\n", edx, eax);

		ecx = 0x48B; 
		asm volatile (
			"rdmsr\n\t"
			: "=a" (eax), "=d" (edx)
			: "c" (ecx)
		);
		printk("IA32_VMX_PROCBASED_CTLS2 = 0x%08x%08x\n", edx, eax);

		ecx = 0x48F; 
		asm volatile (
			"rdmsr\n\t"
			: "=a" (eax), "=d" (edx)
			: "c" (ecx)
		);
		printk("IA32_VMX_TRUE_EXIT_CTLS = 0x%08x%08x\n", edx, eax);

		ecx = 0x490; 
		asm volatile (
			"rdmsr\n\t"
			: "=a" (eax), "=d" (edx)
			: "c" (ecx)
		);
		printk("IA32_VMX_TRUE_ENTRY_CTLS = 0x%08x%08x\n", edx, eax);

		ecx = 0x48C; 
		asm volatile (
			"rdmsr\n\t"
			: "=a" (eax), "=d" (edx)
			: "c" (ecx)
		);

		ecx = 0x48C; 
		asm volatile (
			"rdmsr\n\t"
			: "=a" (eax), "=d" (edx)
			: "c" (ecx)
		);
		printk("IA32_VMX_EPT_VPID_CAP = 0x%08x%08x\n", edx, eax);

		break;

	case PEACH_RUN:
		printk("PEACH RUN\n");

		guest_memory = (u8 *) kmalloc(GUEST_MEMORY_SIZE,
							GFP_KERNEL);
		guest_memory_pa = __pa(guest_memory);

		for (i = 0; i < guest_bin_len; i++) {
			guest_memory[i] = guest_bin[i];
		}

		init_ept(&ept_pointer, guest_memory_pa);

		vmxon = (struct vmcs *) kmalloc(4096, GFP_KERNEL);
		memset(vmxon, 0, 4096);
		vmxon->hdr.revision_id = 0x00000001;
		vmxon->hdr.shadow = 0x00000000;
		vmxon_pa = __pa(vmxon);

		vmcs = (struct vmcs *) kmalloc(4096, GFP_KERNEL);
		memset(vmcs, 0, 4096);
		vmcs->hdr.revision_id = 0x00000001;
		vmcs->hdr.shadow = 0x00000000;
		vmcs_pa = __pa(vmcs);

		asm volatile (
			"movq %cr4, %rax\n\t"
			"bts $13, %rax\n\t"
			"movq %rax, %cr4"
		);

		asm volatile (
			"vmxon %[pa]; setna %[ret]"
			: [ret] "=rm" (ret1)
			: [pa] "m" (vmxon_pa)
			: "cc", "memory"
		);
		printk("vmxon = %d\n", ret1);

		asm volatile (
			"vmclear %[pa]; setna %[ret]"
			: [ret] "=rm" (ret1)
			: [pa] "m" (vmcs_pa)
			: "cc", "memory"
		);
		printk("vmclear = %d\n", ret1);

		asm volatile (
			"vmptrld %[pa]; setna %[ret]"
			: [ret] "=rm" (ret1)
			: [pa] "m" (vmcs_pa)
			: "cc", "memory"
		);
		printk("vmptrld = %d\n", ret1);

		vmcs_field = 0x00000802;
		vmcs_field_value = 0x0000;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Guest CS selctor = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x0000080E;
		vmcs_field_value = 0x0000;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Guest TR selctor = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00002800;
		vmcs_field_value = 0xFFFFFFFFFFFFFFFF;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("VMCS link pointer = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00004802;
		vmcs_field_value = 0x0000FFFF;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Guest CS limit = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x0000480E;
		vmcs_field_value = 0x0000000FF;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Guest TR limit = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00004814;
		vmcs_field_value = 0x00010000;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Guest ES access rights = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00004816;
		vmcs_field_value = 0x0000009B;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Guest CS access rights = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00004818;
		vmcs_field_value = 0x00010000;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Guest SS access rights = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x0000481A;
		vmcs_field_value = 0x00010000;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Guest DS access rights = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x0000481C;
		vmcs_field_value = 0x00010000;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Guest FS access rights = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x0000481E;
		vmcs_field_value = 0x00010000;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Guest GS access rights = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00004820;
		vmcs_field_value = 0x00010000;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Guest LDTR access rights = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00004822;
		vmcs_field_value = 0x0000008B;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Guest TR access rights = 0x%llx\n", vmcs_field_value);

		vmcs_field =  0x00006800;
		vmcs_field_value = 0x00000020;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Guest CR0 = 0x%llx\n", vmcs_field_value);

		vmcs_field =  0x00006804;
		vmcs_field_value = 0x0000000000002000;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Guest CR4 = 0x%llx\n", vmcs_field_value);

		vmcs_field =  0x00006808;
		vmcs_field_value = 0x0000000000000000;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Guest CS base = 0x%llx\n", vmcs_field_value);

		vmcs_field =  0x00006814;
		vmcs_field_value = 0x0000000000008000;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Guest TR base = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x0000681E;
		vmcs_field_value = 0x0000000000000000;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Guest RIP = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00006820;
		vmcs_field_value = 0x0000000000000002;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Guest RFLAGS = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00000C00;
		asm volatile (
			"movq %%es, %0\n\t"
			: "=a" (vmcs_field_value)
			:
		);
		vmcs_field_value &= 0xF8;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Host ES selctor = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00000C02;
		asm volatile (
			"movq %%cs, %0\n\t"
			: "=a" (vmcs_field_value)
			:
		);
		vmcs_field_value &= 0xF8;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Host CS selctor = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00000C04;
		asm volatile (
			"movq %%ss, %0\n\t"
			: "=a" (vmcs_field_value)
			:
		);
		vmcs_field_value &= 0xF8;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Host SS selctor = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00000C06;
		asm volatile (
			"movq %%ds, %0\n\t"
			: "=a" (vmcs_field_value)
			:
		);
		vmcs_field_value &= 0xF8;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Host DS selctor = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00000C08;
		asm volatile (
			"movq %%fs, %0\n\t"
			: "=a" (vmcs_field_value)
			:
		);
		vmcs_field_value &= 0xF8;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Host FS selctor = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00000C0A;
		asm volatile (
			"movq %%gs, %0\n\t"
			: "=a" (vmcs_field_value)
			:
		);
		vmcs_field_value &= 0xF8;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Host GS selctor = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00000C0C;
		asm volatile (
			"str %0\n\t"
			: "=a" (vmcs_field_value)
			:
		);
		vmcs_field_value &= 0xF8;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Host TR selctor = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00002C00;
		ecx = 0x277;
		asm volatile (
			"rdmsr\n\t"
			: "=a" (eax), "=d" (edx)
			: "c" (ecx)
		);
		rdx = edx;
		vmcs_field_value = rdx << 32 | eax;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Host IA32_PAT = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00002C02;
		ecx = 0xC0000080;
		asm volatile (
			"rdmsr\n\t"
			: "=a" (eax), "=d" (edx)
			: "c" (ecx)
		);
		rdx = edx;
		vmcs_field_value = rdx << 32 | eax;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Host IA32_EFER = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00002C04;
		ecx = 0x38F;
		asm volatile (
			"rdmsr\n\t"
			: "=a" (eax), "=d" (edx)
			: "c" (ecx)
		);
		rdx = edx;
		vmcs_field_value = rdx << 32 | eax;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Host IA32_PERF_GLOBAL_CTRL = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00004C00;
		ecx = 0x174;
		asm volatile (
			"rdmsr\n\t"
			: "=a" (eax), "=d" (edx)
			: "c" (ecx)
		);
		rdx = edx;
		vmcs_field_value = rdx << 32 | eax;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Host IA32_SYSENTER_CS = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00006C00;
		asm volatile (
			"movq %%cr0, %0\n\t"
			: "=a" (vmcs_field_value)
			:
		);
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Host CR0 = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00006C02;
		asm volatile (
			"movq %%cr3, %0\n\t"
			: "=a" (vmcs_field_value)
			:
		);
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Host CR3 = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00006C04;
		asm volatile (
			"movq %%cr4, %0\n\t"
			: "=a" (vmcs_field_value)
			:
		);
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Host CR4 = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00006C06;
		ecx = 0xC0000100;
		asm volatile (
			"rdmsr\n\t"
			: "=a" (eax), "=d" (edx)
			: "c" (ecx)
		);
		rdx = edx;
		vmcs_field_value = rdx << 32 | eax;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Host FS base = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00006C08;
		ecx = 0xC0000101;
		asm volatile (
			"rdmsr\n\t"
			: "=a" (eax), "=d" (edx)
			: "c" (ecx)
		);
		rdx = edx;
		vmcs_field_value = rdx << 32 | eax;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Host GS base = 0x%llx\n", vmcs_field_value);

		asm volatile (
			"str %0\n\t"
			: "=a" (host_tr_selector)
			:
		);
		host_tr_selector &= 0xF8;

		asm volatile (
			"sgdt %0\n\t"
			: "=m" (xdtr)
			:
		);
		host_gdt_base = *((u64 *) (xdtr + 2));

		host_tr_desc = *((u64 *) (host_gdt_base + host_tr_selector));
		vmcs_field_value = ((host_tr_desc & 0x000000FFFFFF0000) >> 16) | ((host_tr_desc & 0xFF00000000000000) >> 32);

		host_tr_desc = *((u64 *) (host_gdt_base + host_tr_selector + 8));
		host_tr_desc <<= 32;
		vmcs_field_value |= host_tr_desc;

		vmcs_field = 0x00006C0A;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Host TR base = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00006C0C;
		asm volatile (
			"sgdt %0\n\t"
			: "=m" (xdtr)
			:
		);
		vmcs_field_value = *((u64 *) (xdtr + 2));
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Host GDTR base = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00006C0E;
		asm volatile (
			"sidt %0\n\t"
			: "=m" (xdtr)
			:
		);
		vmcs_field_value = *((u64 *) (xdtr + 2));
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Host IDTR base = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00006C10;
		ecx = 0x175;
		asm volatile (
			"rdmsr\n\t"
			: "=a" (eax), "=d" (edx)
			: "c" (ecx)
		);
		rdx = edx;
		vmcs_field_value = rdx << 32 | eax;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Host IA32_SYSENTER_ESP = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00006C12;
		ecx = 0x176;
		asm volatile (
			"rdmsr\n\t"
			: "=a" (eax), "=d" (edx)
			: "c" (ecx)
		);
		rdx = edx;
		vmcs_field_value = rdx << 32 | eax;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Host IA32_SYSENTER_EIP = 0x%llx\n", vmcs_field_value);

		stack = (u8 *) kmalloc(0x8000, GFP_KERNEL);
		vmcs_field = 0x00006C14;
		vmcs_field_value = (u64) stack + 0x8000;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Host RSP = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00006C16;
		vmcs_field_value = (u64) _vmexit_handler;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Host RIP = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00000000;
		vmcs_field_value = 0x0001;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("VPID = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x0000201A;
		vmcs_field_value = ept_pointer;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("EPT_POINTER = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00004000;
		vmcs_field_value = 0x00000016;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Pin-based VM-execution controls = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00004002;
		vmcs_field_value = 0x840061F2;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Primary Processor-based VM-execution controls = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x0000401E;
		vmcs_field_value = 0x000000A2;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("Secondary Processor-based VM-execution controls = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x00004012;
		vmcs_field_value = 0x000011fb;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("VM-entry controls = 0x%llx\n", vmcs_field_value);

		vmcs_field = 0x0000400C;
		vmcs_field_value = 0x00036ffb;
		asm volatile (
			"vmwrite %1, %0\n\t"
			:
			: "r" (vmcs_field), "r" (vmcs_field_value)
		);
		printk("VM-exit controls = 0x%llx\n", vmcs_field_value);

		asm volatile (
			"movq %%rsp, %0\n\t"
			"movq %%rbp, %1\n\t"
			: "=a" (shutdown_rsp), "=b" (shutdown_rbp)
			:
		);

		asm volatile (
			"vmlaunch; setna %[ret]"
			: [ret] "=rm" (ret1)
			:
			: "cc", "memory"
		);
		printk("vmlaunch = %d\n", ret1);

		vmcs_field = 0x00004402;
		asm volatile (
			"vmread %1, %0\n\t"
			: "=r" (vmcs_field_value)
			: "r" (vmcs_field)
		);
		printk("EXIT_REASON = 0x%llx\n", vmcs_field_value);

		asm volatile ("shutdown:");
		printk("********** guest shutdown **********\n");

		asm volatile ("vmxoff");

		asm volatile (
			"movq %cr4, %rax\n\t"
			"btr $13, %rax\n\t"
			"movq %rax, %cr4"
		);

		break;
	}

	return 0;
}

void handle_vmexit(struct guest_regs *regs)
{
	u64 vmcs_field;
	u64 vmcs_field_value;
	u64 guest_rip;

	dump_guest_regs(regs);

	vmcs_field = 0x00004402;
	asm volatile (
		"vmread %1, %0\n\t"
		: "=r" (vmcs_field_value)
		: "r" (vmcs_field)
	);
	printk("EXIT_REASON = 0x%llx\n", vmcs_field_value);

	switch (vmcs_field_value) {
	case 0x0C:
		asm volatile (
			"movq %0, %%rsp\n\t"
			"movq %1, %%rbp\n\t"
			"jmp shutdown\n\t"
			:
			: "a" (shutdown_rsp), "b" (shutdown_rbp)
		);

		break;

	case 0x0A:
		regs->rax = 0x6368;
		regs->rbx = 0x6561;
		regs->rcx = 0x70;

		break;

	default:
		break;
	}

	vmcs_field = 0x0000681E;
	asm volatile (
		"vmread %1, %0\n\t"
		: "=r" (vmcs_field_value)
		: "r" (vmcs_field)
	);
	printk("Guest RIP = 0x%llx\n", vmcs_field_value);

	guest_rip = vmcs_field_value;

	vmcs_field = 0x0000440C;
	asm volatile (
		"vmread %1, %0\n\t"
		: "=r" (vmcs_field_value)
		: "r" (vmcs_field)
	);
	printk("VM-exit instruction length = 0x%llx\n", vmcs_field_value);

	vmcs_field = 0x0000681E;
	vmcs_field_value = guest_rip + vmcs_field_value;
	asm volatile (
		"vmwrite %1, %0\n\t"
		:
		: "r" (vmcs_field), "r" (vmcs_field_value)
	);
	printk("Guest RIP = 0x%llx\n", vmcs_field_value);

	return;
}

static void init_ept(u64 *ept_pointer, u64 guest_memory_pa)
{
	int i;

	u64 ept_va;
	u64 ept_pa;

	u64 *entry;

	ept_memory = (u8 *) kmalloc(EPT_MEMORY_SIZE, GFP_KERNEL);
	memset(ept_memory, 0, EPT_MEMORY_SIZE);

	ept_va = (u64) ept_memory;
	ept_pa = __pa(ept_memory);

	init_ept_pointer(ept_pointer, ept_pa);

	entry = (u64 *) ept_va;
	init_pml4e(entry, ept_pa + 0x1000);
	printk("pml4e = 0x%llx\n", *entry);

	entry = (u64 *) (ept_va + 0x1000);
	init_pdpte(entry, ept_pa + 0x2000);
	printk("pdpte = 0x%llx\n", *entry);

	entry = (u64 *) (ept_va + 0x2000);
	init_pde(entry, ept_pa + 0x3000);
	printk("pdte = 0x%llx\n", *entry);

	for (i = 0; i < 16; i++) {
		entry = (u64 *) (ept_va + 0x3000 + i * 8);
		init_pte(entry, guest_memory_pa + i * 0x1000);
		printk("pte = 0x%llx\n", *entry);
	}

	return;
}

static void init_ept_pointer(u64 *p, u64 pa)
{
	*p = pa | 1 << 6 | 3 << 3 | 6;

	return;
}

static void init_pml4e(u64 *entry, u64 pa)
{
	*entry = pa | 1 << 2 | 1 << 1 | 1;

	return;
}

static void init_pdpte(u64 *entry, u64 pa)
{
	*entry = pa | 1 << 2 | 1 << 1 | 1;

	return;
}

static void init_pde(u64 *entry, u64 pa)
{
	*entry = pa | 1 << 2 | 1 << 1 | 1;

	return;
}

static void init_pte(u64 *entry, u64 pa)
{
	*entry = pa | 6 << 3 | 1 << 2 | 1 << 1 | 1;

	return;
}

static void dump_guest_regs(struct guest_regs *regs)
{
	printk("********** guest regs **********\n");
	printk("* rax = 0x%llx\n", regs->rax);
	printk("* rcx = 0x%llx\n", regs->rcx);
	printk("* rdx = 0x%llx\n", regs->rdx);
	printk("* rbx = 0x%llx\n", regs->rbx);
	printk("* rbp = 0x%llx\n", regs->rbp);
	printk("* rsi = 0x%llx\n", regs->rsi);
	printk("* rdi = 0x%llx\n", regs->rdi);
	printk("* r8 = 0x%llx\n", regs->r8);
	printk("* r9 = 0x%llx\n", regs->r9);
	printk("* r10 = 0x%llx\n", regs->r10);
	printk("* r11 = 0x%llx\n", regs->r11);
	printk("* r12 = 0x%llx\n", regs->r12);
	printk("* r13 = 0x%llx\n", regs->r13);
	printk("* r14 = 0x%llx\n", regs->r14);
	printk("* r15 = 0x%llx\n", regs->r15);
	printk("********************************\n");
}

module_init(peach_init);
module_exit(peach_exit);

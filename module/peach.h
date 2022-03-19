#ifndef __PEACH_H__
#define __PEACH_H__

#include <linux/ioctl.h>

#ifdef USERSPACE
	#include <stdint.h>
	#define u64 uint64_t
	#define u32 uint32_t
	#define u8 uint8_t
#else
	#include <linux/types.h>
#endif

#define PEACH_COUNT 1

#define PEACH_MAJOR 511
#define PEACH_MINOR 0

#define PEACH_MAGIC 'M'
#define PEACH_PROBE _IOR(PEACH_MAGIC, 0, u64)
#define PEACH_RUN _IO(PEACH_MAGIC, 1)

#endif

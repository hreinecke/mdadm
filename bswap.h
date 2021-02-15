#ifndef _BSWAP_H
#define _BSWAP_H

#include <endian.h>
/* Redhat don't like to #include <asm/byteorder.h>, and
 * some time include <linux/byteorder/xxx_endian.h> isn't enough,
 * and there is no standard conversion function so... */
/* And dietlibc doesn't think byteswap is ok, so.. */
/*  #include <byteswap.h> */
#define __mdadm_bswap_16(x) (((x) & 0x00ffU) << 8 | \
			     ((x) & 0xff00U) >> 8)
#define __mdadm_bswap_32(x) (((x) & 0x000000ffU) << 24 | \
			     ((x) & 0xff000000U) >> 24 | \
			     ((x) & 0x0000ff00U) << 8  | \
			     ((x) & 0x00ff0000U) >> 8)
#define __mdadm_bswap_64(x) (((x) & 0x00000000000000ffULL) << 56 | \
			     ((x) & 0xff00000000000000ULL) >> 56 | \
			     ((x) & 0x000000000000ff00ULL) << 40 | \
			     ((x) & 0x00ff000000000000ULL) >> 40 | \
			     ((x) & 0x0000000000ff0000ULL) << 24 | \
			     ((x) & 0x0000ff0000000000ULL) >> 24 | \
			     ((x) & 0x00000000ff000000ULL) << 8 |  \
			     ((x) & 0x000000ff00000000ULL) >> 8)

#if !defined(__KLIBC__)
#if BYTE_ORDER == LITTLE_ENDIAN
#define	__cpu_to_le16(_x) (unsigned int)(_x)
#define __cpu_to_le32(_x) (unsigned int)(_x)
#define __cpu_to_le64(_x) (unsigned long long)(_x)
#define	__le16_to_cpu(_x) (unsigned int)(_x)
#define __le32_to_cpu(_x) (unsigned int)(_x)
#define __le64_to_cpu(_x) (unsigned long long)(_x)

#define	__cpu_to_be16(_x) __mdadm_bswap_16(_x)
#define __cpu_to_be32(_x) __mdadm_bswap_32(_x)
#define __cpu_to_be64(_x) __mdadm_bswap_64(_x)
#define	__be16_to_cpu(_x) __mdadm_bswap_16(_x)
#define __be32_to_cpu(_x) __mdadm_bswap_32(_x)
#define __be64_to_cpu(_x) __mdadm_bswap_64(_x)
#elif BYTE_ORDER == BIG_ENDIAN
#define	__cpu_to_le16(_x) __mdadm_bswap_16(_x)
#define __cpu_to_le32(_x) __mdadm_bswap_32(_x)
#define __cpu_to_le64(_x) __mdadm_bswap_64(_x)
#define	__le16_to_cpu(_x) __mdadm_bswap_16(_x)
#define __le32_to_cpu(_x) __mdadm_bswap_32(_x)
#define __le64_to_cpu(_x) __mdadm_bswap_64(_x)

#define	__cpu_to_be16(_x) (unsigned int)(_x)
#define __cpu_to_be32(_x) (unsigned int)(_x)
#define __cpu_to_be64(_x) (unsigned long long)(_x)
#define	__be16_to_cpu(_x) (unsigned int)(_x)
#define __be32_to_cpu(_x) (unsigned int)(_x)
#define __be64_to_cpu(_x) (unsigned long long)(_x)
#else
#  error "unknown endianness."
#endif
#endif /* __KLIBC__ */

/*
 * Partially stolen from include/linux/unaligned/packed_struct.h
 */
struct __una_u16 { __u16 x; } __attribute__ ((packed));
struct __una_u32 { __u32 x; } __attribute__ ((packed));

static inline __u16 __get_unaligned16(const void *p)
{
	const struct __una_u16 *ptr = (const struct __una_u16 *)p;
	return ptr->x;
}

static inline __u32 __get_unaligned32(const void *p)
{
	const struct __una_u32 *ptr = (const struct __una_u32 *)p;
	return ptr->x;
}

static inline void __put_unaligned16(__u16 val, void *p)
{
	struct __una_u16 *ptr = (struct __una_u16 *)p;
	ptr->x = val;
}

static inline void __put_unaligned32(__u32 val, void *p)
{
	struct __una_u32 *ptr = (struct __una_u32 *)p;
	ptr->x = val;
}

#endif /* _BSWAP_H */

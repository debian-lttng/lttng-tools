/*
 * Copyright (C) 2011 - David Goulet <dgoulet@efficios.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 only,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _COMPAT_ENDIAN_H
#define _COMPAT_ENDIAN_H

#ifdef __linux__
#include <endian.h>

/*
 * htobe/betoh are not defined for glibc <2.9, so add them
 * explicitly if they are missing.
 */
#ifdef __USE_BSD
/* Conversion interfaces. */
# include <byteswap.h>

# if __BYTE_ORDER == __LITTLE_ENDIAN
#  ifndef htobe16
#   define htobe16(x) __bswap_16(x)
#  endif
#  ifndef htole16
#   define htole16(x) (x)
#  endif
#  ifndef be16toh
#   define be16toh(x) __bswap_16(x)
#  endif
#  ifndef le16toh
#   define le16toh(x) (x)
#  endif

#  ifndef htobe32
#   define htobe32(x) __bswap_32(x)
#  endif
#  ifndef htole32
#   define htole32(x) (x)
#  endif
#  ifndef be32toh
#   define be32toh(x) __bswap_32(x)
#  endif
#  ifndef le32toh
#   define le32toh(x) (x)
#  endif

#  ifndef htobe64
#   define htobe64(x) __bswap_64(x)
#  endif
#  ifndef htole64
#   define htole64(x) (x)
#  endif
#  ifndef be64toh
#   define be64toh(x) __bswap_64(x)
#  endif
#  ifndef le64toh
#   define le64toh(x) (x)
#  endif

# else /* __BYTE_ORDER == __LITTLE_ENDIAN */
#  ifndef htobe16
#   define htobe16(x) (x)
#  endif
#  ifndef htole16
#   define htole16(x) __bswap_16(x)
#  endif
#  ifndef be16toh
#   define be16toh(x) (x)
#  endif
#  ifndef le16toh
#   define le16toh(x) __bswap_16(x)
#  endif

#  ifndef htobe32
#   define htobe32(x) (x)
#  endif
#  ifndef htole32
#   define htole32(x) __bswap_32(x)
#  endif
#  ifndef be32toh
#   define be32toh(x) (x)
#  endif
#  ifndef le32toh
#   define le32toh(x) __bswap_32(x)
#  endif

#  ifndef htobe64
#   define htobe64(x) (x)
#  endif
#  ifndef htole64
#   define htole64(x) __bswap_64(x)
#  endif
#  ifndef be64toh
#   define be64toh(x) (x)
#  endif
#  ifndef le64toh
#   define le64toh(x) __bswap_64(x)
#  endif

# endif /* __BYTE_ORDER == __LITTLE_ENDIAN */
#endif /* __USE_BSD */

#elif defined(__FreeBSD__)
#include <machine/endian.h>
#else
#error "Please add support for your OS."
#endif

#endif /* _COMPAT_ENDIAN_H */
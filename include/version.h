/*
 * Copyright (C) 2013-2014 - Raphaël Beamonte <raphael.beamonte@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 only,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <config.h>

#ifndef VERSION_H
#define VERSION_H

#define GIT_VERSION "2.5.0"

/*
 * Define the macro containing the FULL version
 */
#ifdef GIT_VERSION
#define FULL_VERSION "" GIT_VERSION
#else /* GIT_VERSION */
#define FULL_VERSION "" VERSION
#endif /* GIT_VERSION */

#endif /* VERSION_H */

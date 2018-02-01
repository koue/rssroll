# Copyright (c) 2018 Nikola Kolev <koue@chaosophia.net>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# FindLibcez
# ----------
#
# Find libcez headers and libraries
#
# ::
#
#  LIBCEZ_CONFIG_INCLUDE_DIR - libcez config include directory
#  LIBCEZ_FOSSIL_INCLUDE_DIR - libcez fossil include directory
#  LIBCEZ_MISC_INCLUDE_DIR - libcez misc include directory
#
#  LIBCEZ_CONFIG_LIBRARIES - libcez config library
#  LIBCEZ_FOSSIL_LIBRARIES - libcez fossil library
#  LIBCEZ_MISC_LIBRARIES - libcez misc library
#
#  LIBCEZ_FOUND - libcez found

# Look for libcez config header file.
find_path(LIBCEZ_CONFIG_INCLUDE_DIR NAMES cez-config.h)
# Look for libcez fossil header file.
find_path(LIBCEZ_FOSSIL_INCLUDE_DIR NAMES cez-fossil.h)
# Look for libcez misc header file.
find_path(LIBCEZ_MISC_INCLUDE_DIR NAMES cez-misc.h)

# Look for libcez config library
find_library(LIBCEZ_CONFIG_LIBRARIES NAMES cezconfig)
# Look for libcez fossil library
find_library(LIBCEZ_FOSSIL_LIBRARIES NAMES cezfossil)
# Look for libcez misc library
find_library(LIBCEZ_MISC_LIBRARIES NAMES cezmisc)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Libcez DEFAULT_MSG
			LIBCEZ_CONFIG_INCLUDE_DIR LIBCEZ_CONFIG_LIBRARIES
			LIBCEZ_FOSSIL_INCLUDE_DIR LIBCEZ_FOSSIL_LIBRARIES
			LIBCEZ_MISC_INCLUDE_DIR LIBCEZ_MISC_LIBRARIES
)

mark_as_advanced(LIBCEZ_CONFIG_INCLUDE_DIR LIBCEZ_CONFIG_LIBRARIES
		LIBCEZ_FOSSIL_INCLUDE_DIR LIBCEZ_FOSSIL_LIBRARIES
		LIBCEZ_MISC_INCLUDE_DIR LIBCEZ_MISC_LIBRARIES
)

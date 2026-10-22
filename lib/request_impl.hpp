// SPDX-License-Identifier: MIT; Copyright 2026 Andrew D Smith

#ifndef LIB_REQUEST_IMPL_HPP_
#define LIB_REQUEST_IMPL_HPP_

#ifdef UNIT_TEST
#define STATIC
#define INLINE
#else
#define STATIC static
#define INLINE inline
#endif

#include <system_error>

namespace transferase {

struct request;

[[nodiscard]] STATIC INLINE auto
compose(char *buffer, const std::size_t size, const request &req)
  -> std::error_code;

}  // namespace transferase

#endif  // LIB_REQUEST_IMPL_HPP_

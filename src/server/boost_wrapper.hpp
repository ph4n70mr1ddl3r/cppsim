#pragma once

// Wrapper header to suppress strict compiler warnings in Boost libraries

#if defined(_MSC_VER)
// MSVC warning suppression
#pragma warning(push, 0)
#pragma warning(disable: 4244)  // conversion warning
#pragma warning(disable: 4267)  // size_t conversion
#pragma warning(disable: 4996)  // deprecated functions
#pragma warning(disable: 4459)  // shadowing
#pragma warning(disable: 4702)  // unreachable code
#else
// GCC/Clang warning suppression
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#pragma GCC diagnostic ignored "-Wduplicated-branches"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
#endif

// Common Boost.Asio and Beast includes
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#if defined(_MSC_VER)
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif

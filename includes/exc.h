#pragma once

#include <exception>

namespace flow {

class full : public std::exception {};
class empty : public std::exception {};
class shutdown : public std::exception {};
class unavailable : public std::exception {};

}

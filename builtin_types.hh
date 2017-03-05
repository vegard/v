#ifndef V_BUILTIN_TYPES_H
#define V_BUILTIN_TYPES_H

#include "value.hh"

static auto builtin_type_u64 = std::make_shared<value_type>(value_type{8, 8});

#endif

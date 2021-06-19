#pragma once

#include "cris/core/node/base.h"

namespace cris::core {

template<class node_t, CRNodeType base_t = CRNodeBase>
class CRNamedNode : public base_t {
   public:
    // Forward constructor parameters to base_t
    template<class... args_t>
    CRNamedNode(args_t&&... args) : base_t(std::forward<args_t>(args)...) {}

    std::string GetName() const override { return GetTypeName<node_t>(); }
};

}  // namespace cris::core

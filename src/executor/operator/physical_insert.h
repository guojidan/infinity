//
// Created by JinHai on 2022/7/28.
//

#pragma once

#include "executor/physical_operator.h"

namespace infinity {

class PhysicalInsert : public PhysicalOperator {
public:
    explicit PhysicalInsert(uint64_t id)
        : PhysicalOperator(PhysicalOperatorType::kInsert, nullptr, nullptr, id) {}
    ~PhysicalInsert() = default;
    void Execute() override;
};

}
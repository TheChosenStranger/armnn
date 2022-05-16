//
// Copyright © 2021 Arm Ltd and Contributors. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include <iostream>

namespace armnn
{
template<typename T>
struct log
    {
        typedef T result_type;
        typedef T argument_type;

        T
        operator () (const T&  inputData) const
        {
            // computes natural logarithm of inputData
            return std::log(inputData);
        }
    };

} //namespace armnn

//
// Copyright © 2017 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "NetworkQuantizationScheme.hpp"

#include <armnn/Tensor.hpp>
#include <armnn/TypesUtils.hpp>
#include <armnn/ILayerVisitor.hpp>

#include <utility>
#include <limits>

#include <boost/assert.hpp>

namespace armnn
{

template<typename srcType>
void QuantizeConstant(const srcType* src, uint8_t* dst, size_t numElements, float& scale, int& offset)
{
    BOOST_ASSERT(src);
    BOOST_ASSERT(dst);

    float min = std::numeric_limits<srcType>::max();
    float max = std::numeric_limits<srcType>::lowest();
    for (size_t i = 0; i < numElements; ++i)
    {
        min = std::min(min, src[i]);
        max = std::max(max, src[i]);
    }

    QAsymm8QuantizationScheme quantizationScheme;
    OffsetScalePair qParams = quantizationScheme.ComputeScheme(min, max);
    scale = qParams.first;
    offset = qParams.second;

    for (size_t i = 0; i < numElements; ++i)
    {
        dst[i] = armnn::Quantize<uint8_t>(src[i], scale, offset);
    }
}

ConstTensor CreateQuantizedConst(const ConstTensor& tensor, std::vector<uint8_t>& backing);

template <typename LayerContainer>
void VisitLayers(const LayerContainer& layerContainer, ILayerVisitor& visitor)
{
    for (auto layer : layerContainer)
    {
        layer->Accept(visitor);
    }
}

} // namespace armnn

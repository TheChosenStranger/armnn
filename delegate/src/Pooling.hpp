//
// Copyright © 2020 Arm Ltd and Contributors. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include <tensorflow/lite/builtin_ops.h>
#include <tensorflow/lite/c/builtin_op_data.h>
#include <tensorflow/lite/c/common.h>
#include <tensorflow/lite/minimal_logging.h>

namespace armnnDelegate
{

void CalcPadding(uint32_t inputSize,
                 uint32_t filterSize,
                 uint32_t stride,
                 uint32_t dilation,
                 uint32_t& paddingFront,
                 uint32_t& paddingBack,
                 TfLitePadding padding)
{
    paddingFront = 0;
    paddingBack = 0;
    if (padding == kTfLitePaddingSame)
    {
        uint32_t outputSize = (inputSize + stride - 1) / stride;
        uint32_t dilatedSize = filterSize + (dilation - 1) * (filterSize - 1);
        uint32_t temp = (outputSize - 1) * stride + dilatedSize;
        if (temp > inputSize)
        {
            paddingFront = (temp - inputSize) / 2;
            paddingBack = (temp - inputSize) - paddingFront;
        }
    }
}

TfLiteStatus VisitPoolingOperator(DelegateData& delegateData,
                                  TfLiteContext* tfLiteContext,
                                  TfLiteNode* tfLiteNode,
                                  int nodeIndex,
                                  int32_t tfLitePoolingOperatorCode)
{
    TF_LITE_ENSURE_STATUS(ValidateNumInputs(tfLiteContext, tfLiteNode, 1, nodeIndex));
    TF_LITE_ENSURE_STATUS(ValidateNumOutputs(tfLiteContext, tfLiteNode, 1, nodeIndex));

    const TfLiteTensor* tfLiteTensors = tfLiteContext->tensors;
    const TfLiteTensor& tfLiteInputTensor = tfLiteTensors[tfLiteNode->inputs->data[0]];
    if (IsDynamicTensor(tfLiteInputTensor))
    {
        TF_LITE_MAYBE_KERNEL_LOG(
            tfLiteContext,
            "TfLiteArmnnDelegate: Dynamic input tensors are not supported in operator #%d node #%d: ",
            tfLitePoolingOperatorCode, nodeIndex);
        return kTfLiteError;
    }

    const TfLiteTensor& tfLiteOutputTensor = tfLiteTensors[tfLiteNode->outputs->data[0]];
    if (IsDynamicTensor(tfLiteOutputTensor))
    {
        TF_LITE_MAYBE_KERNEL_LOG(
            tfLiteContext,
            "TfLiteArmnnDelegate: Dynamic output tensors are not supported in operator #%d node #%d: ",
            tfLitePoolingOperatorCode, nodeIndex);
        return kTfLiteError;
    }

    const armnn::TensorInfo& inputTensorInfo = GetTensorInfoForTfLiteTensor(tfLiteInputTensor);
    const armnn::TensorInfo& outputTensorInfo = GetTensorInfoForTfLiteTensor(tfLiteOutputTensor);

    armnn::PoolingAlgorithm poolingAlgorithm;
    switch(tfLitePoolingOperatorCode)
    {
        case kTfLiteBuiltinMaxPool2d:
            poolingAlgorithm = armnn::PoolingAlgorithm::Max;
            break;
        default:
            return kTfLiteError;
    }

    armnn::Pooling2dDescriptor descriptor;
    descriptor.m_PoolType = poolingAlgorithm;

    auto* params = reinterpret_cast<TfLitePoolParams*>(tfLiteNode->builtin_data);
    descriptor.m_PoolWidth = params->filter_width;
    descriptor.m_PoolHeight = params->filter_height;
    descriptor.m_StrideX = params->stride_width;
    descriptor.m_StrideY = params->stride_height;
    descriptor.m_DataLayout = armnn::DataLayout::NHWC;

    unsigned int inputHeight = inputTensorInfo.GetShape()[1];
    unsigned int inputWidth  = inputTensorInfo.GetShape()[2];

    CalcPadding(inputHeight, descriptor.m_PoolHeight, descriptor.m_StrideY, 1u,
                descriptor.m_PadTop, descriptor.m_PadBottom, params->padding);
    CalcPadding(inputWidth, descriptor.m_PoolWidth, descriptor.m_StrideX, 1u,
                descriptor.m_PadLeft, descriptor.m_PadRight, params->padding);

    bool isSupported = false;
    auto validateFunc = [&](const armnn::TensorInfo& outputTensorInfo, bool& isSupported)
    {
        FORWARD_LAYER_SUPPORT_FUNC(__func__,
                                   tfLiteContext,
                                   IsPooling2dSupported,
                                   delegateData.m_Backends,
                                   isSupported,
                                   inputTensorInfo,
                                   outputTensorInfo,
                                   descriptor);
    };

    if (!delegateData.m_Network)
    {
        validateFunc(outputTensorInfo, isSupported);
        return isSupported ? kTfLiteOk : kTfLiteError;
    }

    armnn::IConnectableLayer* poolingLayer = delegateData.m_Network->AddPooling2dLayer(descriptor);
    ARMNN_ASSERT(poolingLayer != nullptr);

    armnn::IOutputSlot& outputSlot = poolingLayer->GetOutputSlot(0);
    outputSlot.SetTensorInfo(outputTensorInfo);
    Connect(poolingLayer, tfLiteNode, delegateData);

    // Check activation
    TfLiteFusedActivation activationType = params->activation;
    return FusedActivation(tfLiteContext, tfLiteNode, activationType, poolingLayer, 0, delegateData);
}

} // namespace armnnDelegate
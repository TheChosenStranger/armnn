//
// Copyright © 2020 Arm Ltd and Contributors. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "ArmnnNetworkExecutor.hpp"
#include "Decoder.hpp"
#include "MFCC.hpp"
#include "Preprocess.hpp"

namespace asr
{
/**
 * Generic Speech Recognition pipeline with 3 steps: data pre-processing, inference execution and inference
 * result post-processing.
 *
 */
class ASRPipeline
{
public:

    /**
     * Creates speech recognition pipeline with given network executor and decoder.
     * @param executor - unique pointer to inference runner
     * @param decoder - unique pointer to inference results decoder
     */
    ASRPipeline(std::unique_ptr<common::ArmnnNetworkExecutor<int8_t>> executor,
                std::unique_ptr<Decoder> decoder);

    /**
     * @brief Standard audio pre-processing implementation.
     *
     * Preprocesses and prepares the data for inference by
     * extracting the MFCC features.

     * @param[in] audio - the raw audio data
     * @param[out] preprocessor - the preprocessor object, which handles the data prepreration
     */
    template<typename Tin,typename Tout>
    std::vector<Tout> PreProcessing(std::vector<Tin>& audio, Preprocess& preprocessor)
    {
        int audioDataToPreProcess = preprocessor._m_windowLen +
                ((preprocessor._m_mfcc._m_params.m_numMfccVectors -1) *preprocessor._m_windowStride);
        int outputBufferSize = preprocessor._m_mfcc._m_params.m_numMfccVectors
                * preprocessor._m_mfcc._m_params.m_numMfccFeatures * 3;
        std::vector<Tout> outputBuffer(outputBufferSize);
        preprocessor.Invoke(audio.data(), audioDataToPreProcess, outputBuffer, m_executor->GetQuantizationOffset(),
                            m_executor->GetQuantizationScale());
        return outputBuffer;
    }

    /**
     * @brief Executes inference
     *
     * Calls inference runner provided during instance construction.
     *
     * @param[in] preprocessedData - input inference data. Data type should be aligned with input tensor.
     * @param[out] result - raw inference results.
     */
    template<typename T>
    void Inference(const std::vector<T>& preprocessedData, common::InferenceResults<int8_t>& result)
    {
        size_t data_bytes = sizeof(std::vector<T>) + (sizeof(T) * preprocessedData.size());
        m_executor->Run(preprocessedData.data(), data_bytes, result);
    }

    /**
     * @brief Standard inference results post-processing implementation.
     *
     * Decodes inference results using decoder provided during construction.
     *
     * @param[in] inferenceResult - inference results to be decoded.
     * @param[in] isFirstWindow - for checking if this is the first window of the sliding window.
     * @param[in] isLastWindow - for checking if this is the last window of the sliding window.
     * @param[in] currentRContext - the right context of the output text. To be output if it is the last window.
     */
    template<typename T>
    void PostProcessing(common::InferenceResults<int8_t>& inferenceResult,
                                     bool& isFirstWindow,
                                     bool isLastWindow,
                                     std::string currentRContext)
    {
        int rowLength = 29;
        int middleContextStart = 49;
        int middleContextEnd = 99;
        int leftContextStart = 0;
        int rightContextStart = 100;
        int rightContextEnd = 148;

        std::vector<T> contextToProcess;

        // If isFirstWindow we keep the left context of the output
        if(isFirstWindow)
        {
            std::vector<T> chunk(&inferenceResult[0][leftContextStart],
                    &inferenceResult[0][middleContextEnd * rowLength]);
            contextToProcess = chunk;
        }
        // Else we only keep the middle context of the output
        else
        {
            std::vector<T> chunk(&inferenceResult[0][middleContextStart * rowLength],
                    &inferenceResult[0][middleContextEnd * rowLength]);
            contextToProcess = chunk;
        }
        std::string output = this->m_decoder->DecodeOutput<T>(contextToProcess);
        isFirstWindow = false;
        std::cout << output << std::flush;

        // If this is the last window, we print the right context of the output
        if(isLastWindow)
        {
            std::vector<T> rContext(&inferenceResult[0][rightContextStart*rowLength],
                    &inferenceResult[0][rightContextEnd * rowLength]);
            currentRContext = this->m_decoder->DecodeOutput(rContext);
            std::cout << currentRContext << std::endl;
        }
    }

protected:
    std::unique_ptr<common::ArmnnNetworkExecutor<int8_t>> m_executor;
    std::unique_ptr<Decoder> m_decoder;
};

using IPipelinePtr = std::unique_ptr<asr::ASRPipeline>;

/**
 * Constructs speech recognition pipeline based on configuration provided.
 *
 * @param[in] config - speech recognition pipeline configuration.
 * @param[in] labels - asr labels
 *
 * @return unique pointer to asr pipeline.
 */
IPipelinePtr CreatePipeline(common::PipelineOptions& config, std::map<int, std::string>& labels);

}// namespace asr
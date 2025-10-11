﻿#pragma once

#include <cmath>
#include <algorithm>
#include <optional>

#include "audio_stream.h"
#include "modulator.h"
#include "bitstream.h"

#include "external/aprsroute.hpp"

// **************************************************************** //
//                                                                  //
//                                                                  //
// modem                                                            //
//                                                                  //
//                                                                  //
// **************************************************************** //

struct modem
{
    void initialize(audio_stream& stream, modulator_base& modulator, bitstream_converter_base& converter);

    void transmit();
    void transmit(aprs::router::packet p);
    void transmit(const std::vector<uint8_t>& bits);
    
    size_t receive(std::vector<aprs::router::packet>& packets);

    void preemphasis(bool);
    bool preemphasis() const;
    void gain(double);
    double gain() const;
    void start_silence(double);
    double start_silence() const;
    void end_silence(double);
    double end_silence() const;
    void tx_delay(double);
    double tx_delay() const;
    void tx_tail(double);
    double tx_tail() const;
    void baud_rate(int);
    int baud_rate() const;

private:
    void postprocess_audio(std::vector<double>& audio_buffer);
    void render_audio(const std::vector<double>& audio_buffer);
    void modulate_bitstream(const std::vector<uint8_t>& bitstream, std::vector<double>& audio_buffer);

    std::optional<std::reference_wrapper<audio_stream>> audio;
    std::optional<std::reference_wrapper<modulator_base>> mod;
    std::optional<std::reference_wrapper<bitstream_converter_base>> conv;
    double start_silence_duration_s = 0.0;
    double end_silence_duration_s = 0.0;
    bool preemphasis_enabled = false;
    double gain_value = 1.0; // Linear scale (1.0 = no change)
    double tx_delay_ms = 0.0;
    double tx_tail_ms = 0.0;
    int baud_rate_ = 1200;
    int preamble_flags = 1; // Number of HDLC flags before frame
    int postamble_flags = 1; // Number of HDLC flags after frame
};

// **************************************************************** //
//                                                                  //
//                                                                  //
// silence, gain, preemphasis                                       //
//                                                                  //
//                                                                  //
// **************************************************************** //

template<typename OutputIt>
inline void insert_silence(OutputIt out, int sample_rate, double duration_seconds = 0.1)
{
    int silence_samples = static_cast<int>(duration_seconds * sample_rate);

    for (int i = 0; i < silence_samples; ++i)
    {
        *out++ = 0.0;
    }
}

template<typename It>
inline void apply_gain(It first, It last, double gain)
{
    for (auto it = first; it != last; ++it)
    {
        *it *= gain;
    }
}

template<typename It>
inline void apply_preemphasis(It first, It last, int sample_rate, double tau = 75e-6)
{
    if (first == last) return;

    // Calculate filter coefficient from time constant
    // For 75μs at 48kHz: alpha_pre ≈ 0.845
    // This controls the pole location in the IIR filter
    double alpha_pre = std::exp(-1.0 / (sample_rate * tau));

    // Initialize filter state with first sample
    // Prevents startup transient
    double x_prev = *first;  // Previous input sample
    double y_prev = *first;  // Previous output sample
    ++first;  // Skip first sample (already used for initialization)

    // Apply first-order IIR high-pass filter
    // Transfer function: H(z) = (1 - z^-1) / (1 - alpha*z^-1)
    // This emphasizes high frequencies for FM pre-emphasis
    for (auto it = first; it != last; ++it)
    {
        double x = *it;  // Current input

        // Difference equation: y[n] = x[n] - x[n-1] + alpha * y[n-1]
        // Zero at DC (blocks low frequencies)
        // Pole at alpha (creates rising frequency response)
        double y = x - x_prev + alpha_pre * y_prev;

        // Update state for next iteration
        x_prev = x;
        y_prev = y;

        // Write filtered output back to input
        *it = y;
    }
}

#include "modulator.h"

#include <cassert>

// **************************************************************** //
//                                                                  //
//                                                                  //
// dds_afsk_modulator                                               //
//                                                                  //
//                                                                  //
// **************************************************************** //

dds_afsk_modulator::dds_afsk_modulator(double f_mark = 1200.0, double f_space = 2200.0, int bitrate = 1200, int sample_rate = 48000, double alpha = 0.3)
{
    this->f_mark = f_mark;
    this->f_space = f_space;
    this->sample_rate = sample_rate;
    this->alpha = alpha;

    freq_smooth = f_mark;
    samples_per_bit_ = sample_rate / bitrate;
}

double dds_afsk_modulator::modulate(uint8_t bit)
{
    // DDS (Direct Digital Synthesis) AFSK modulator - processes one sample at a time
    // This function implements the core modulation algorithm:
    //   - Map input bit to target frequency
    //   - Smooth frequency transitions using exponential smoothing (IIR filter)
    //   - Accumulate phase and wrap around to prevent overflow
    //   - Generate output sample using cosine of the current phase
    //
    // Process one bit and generate one audio sample
    // Call this function at the sample rate (e.g., 48000 times/second
    // Each bit must be held for samples_per_bit samples to achieve correct baud rate

    constexpr double two_pi = 2.0 * 3.14159265358979323846;

    // Select target frequency based on input bit
    // Mark (1) = lower frequency, Space (0) = higher frequency
    // For AFSK1200: mark=1200Hz, space=2200Hz
    double freq_target = (bit == 1) ? f_mark : f_space;

    // Exponential smoothing (single-pole IIR low-pass filter)
    // This smooths abrupt frequency changes to reduce spectral splatter
    // Formula: y[n] = α·x[n] + (1 - α)·y[n - 1]
    // where α (alpha) controls smoothing: lower = smoother, higher = sharper
    // Typical α = 0.08 balances clean spectrum with decoder timing requirements
    freq_smooth = alpha * freq_target + (1.0 - alpha) * freq_smooth;

    // Phase accumulation(the "DDS" core)
    // Phase advances by 2π·f/fs radians per sample
    // This creates the desired output frequency
    // fmod() wraps phase to [0, 2π) to prevent numerical precision loss
    // over long transmissions (phase would grow unbounded otherwise)
    phase = std::fmod(phase + two_pi * freq_smooth / sample_rate, two_pi);

    assert(phase >= 0.0 && phase < two_pi);

    // Generate output sample
    // Convert phase (0 to 2π radians) to amplitude using cosine function
    // cos(phase) oscillates between -1.0 and +1.0
    // Phase continuity ensures smooth transitions (no clicks/pops)
    return std::cos(phase);
}

void dds_afsk_modulator::reset()
{
    // WARNING: Calling this during transmission will create phase discontinuities!
    // Only call reset() before starting a new independent transmission where
    // phase continuity with previous data is not required.

    freq_smooth = f_mark;
    phase = 0.0;
}

int dds_afsk_modulator::samples_per_bit() const
{
    return samples_per_bit_;
}

// **************************************************************** //
//                                                                  //
//                                                                  //
// cpfsk_modulator                                                  //
//                                                                  //
//                                                                  //
// **************************************************************** //

cpfsk_modulator::cpfsk_modulator(double f_mark, double f_space, int bitrate, int sample_rate)
    : f_center_((f_mark + f_space) / 2.0),      // Calculate center frequency
    f_delta_((f_mark - f_space) / 2.0),       // Calculate deviation (can be negative)
    sample_rate_(sample_rate),
    samples_per_bit_(sample_rate / bitrate),  // Samples needed per bit period
    m_(0.0),
    current_sample_(0)
{
}

double cpfsk_modulator::modulate(uint8_t bit)
{
    constexpr double two_pi = 2.0 * 3.14159265358979323846;

    // On bit boundaries, convert bit to NRZ and append to bitstream
    // NRZ encoding: bit 1 → -1.0, bit 0 → +1.0
    if (current_sample_ % samples_per_bit_ == 0)
    {
        double nrz = (bit == 1) ? -1.0 : 1.0;
        bitstream_nrz_.push_back(nrz);
    }

    // Offset sample counter by 2 to align with reference implementation
    // This ensures proper phase alignment at the start of modulation
    double i = current_sample_ + 2.0;

    // Calculate array indices using ceil() for proper bit-to-sample mapping
    // The ceil() function ensures we reference the current bit for the entire bit period
    // Subtract 1 to convert from 1-based conceptual indexing to 0-based array indexing
    int index = static_cast<int>(std::ceil(i / samples_per_bit_)) - 1;
    int index_prev = static_cast<int>(std::ceil((i - 1.0) / samples_per_bit_)) - 1;

    // Handle case where no bits have been received yet
    if (bitstream_nrz_.empty())
    {
        current_sample_++;
        // Generate unmodulated carrier at center frequency
        return std::cos(two_pi * i * (f_center_ / sample_rate_));
    }

    // Clamp indices to valid array bounds
    // This handles edge cases at the start and ensures we never access out of bounds
    index = (std::max)(0, (std::min)(index, static_cast<int>(bitstream_nrz_.size()) - 1));
    index_prev = (std::max)(0, (std::min)(index_prev, static_cast<int>(bitstream_nrz_.size()) - 1));

    // Trapezoidal integration of the NRZ bitstream
    // This accumulates the integral ∫m(τ)dτ which is used for phase calculation
    // Trapezoidal rule: integrate by averaging adjacent samples
    // The accumulator m_ represents the cumulative phase deviation
    m_ += (bitstream_nrz_[index_prev] + bitstream_nrz_[index]) / 2.0;

    // Calculate instantaneous phase for CPFSK
    // First term: 2π·f_center·t - carrier phase advancing at center frequency
    // Second term: -2π·f_delta·m - phase deviation based on integrated NRZ data
    //   When bit=1 (nrz=+1): m increases, phase decreases → frequency = f_center - f_delta
    //   When bit=0 (nrz=-1): m decreases, phase increases → frequency = f_center + f_delta
    double phase = two_pi * i * (f_center_ / sample_rate_) -
                   two_pi * m_ * (f_delta_ / sample_rate_);

    current_sample_++;

    return std::cos(phase);
}

void cpfsk_modulator::reset()
{
    // WARNING: Calling this during transmission will create phase discontinuities!
    // Only call reset() before starting a new independent transmission where
    // phase continuity with previous data is not required.
    m_ = 0.0;
    bitstream_nrz_.clear();
    current_sample_ = 0;
}

int cpfsk_modulator::samples_per_bit() const { return samples_per_bit_; }

// **************************************************************** //
//                                                                  //
//                                                                  //
// bessel_null_modulator                                            //
//                                                                  //
//                                                                  //
// **************************************************************** //

bessel_null_modulator::bessel_null_modulator(double f_mark = 1200.0,
    double f_space = 2200.0, int bitrate = 1200, int sample_rate = 48000,
    double alpha = 0.08)
    : f_mark_(f_mark)
    , f_space_(f_space)
    , bitrate_(bitrate)
    , sample_rate_(sample_rate)
    , alpha_(alpha)
    , phase_(0.0)
    , sample_index_(0)
    , current_freq_(f_mark)
    , use_mark_(true)
{
    samples_per_bit_ = sample_rate_ / bitrate_;

    // Calculate transition samples for smooth frequency changes
    transition_samples_ = static_cast<int>(alpha_ * samples_per_bit_);
    if (transition_samples_ < 1)
    {
        transition_samples_ = 1;
    }

    // Precompute Bessel transition window
    bessel_window_.resize(transition_samples_);
    compute_bessel_window();
}

double bessel_null_modulator::modulate(uint8_t bit)
{
    (void)bit; // Ignore bit parameter - used for calibration alternating pattern

    double output = 0.0;

    // Alternate between mark and space frequencies for calibration
    double target_freq = use_mark_ ? f_mark_ : f_space_;

    // Calculate instantaneous frequency with smooth transition
    double freq = current_freq_;

    if (sample_index_ < transition_samples_)
    {
        // Smooth transition using Bessel window
        double prev_freq = use_mark_ ? f_space_ : f_mark_;
        double blend = bessel_window_[sample_index_];
        freq = prev_freq + (target_freq - prev_freq) * blend;
    }
    else
    {
        // Steady state frequency
        freq = target_freq;
    }

    current_freq_ = freq;

    // Generate output sample using phase accumulation
    double phase_increment = 2.0 * pi * freq / sample_rate_;
    output = std::sin(phase_);

    // Accumulate phase
    phase_ += phase_increment;

    // Keep phase in range [0, 2*PI] to prevent numerical issues
    while (phase_ >= 2.0 * pi)
    {
        phase_ -= 2.0 * pi;
    }
    while (phase_ < 0.0)
    {
        phase_ += 2.0 * pi;
    }

    // Increment sample counter
    sample_index_++;
    if (sample_index_ >= samples_per_bit_)
    {
        sample_index_ = 0;
        use_mark_ = !use_mark_; // Toggle for next bit period
    }

    return output;
}

void bessel_null_modulator::reset()
{
    phase_ = 0.0;
    sample_index_ = 0;
    current_freq_ = f_mark_;
    use_mark_ = true;
}

int bessel_null_modulator::samples_per_bit() const
{
    return samples_per_bit_;
}

void bessel_null_modulator::compute_bessel_window()
{
    // Compute modified Bessel function of the first kind, order 0
    // This creates a smooth transition window

    for (int i = 0; i < transition_samples_; i++)
    {
        double x = static_cast<double>(i) / (transition_samples_ - 1);

        // Use raised cosine for smooth transition (simplified Bessel approximation)
        // This provides similar smoothness to Bessel null filter
        bessel_window_[i] = 0.5 * (1.0 - std::cos(pi * x));
    }
}

double bessel_null_modulator::bessel_i0(double x)
{
    // Modified Bessel function of the first kind, order 0
    // Used for optimal filter design
    double sum = 1.0;
    double term = 1.0;
    double x_half_sq = (x * 0.5) * (x * 0.5);

    for (int k = 1; k < 50; k++)
    {
        term *= x_half_sq / (k * k);
        sum += term;

        if (term < 1e-12 * sum)
        {
            break;
        }
    }

    return sum;
}

// **************************************************************** //
//                                                                  //
//                                                                  //
// modulator_base                                                   //
//                                                                  //
//                                                                  //
// **************************************************************** //

double modulator_base::modulate(uint8_t bit)
{
    return 0.0;
}

int16_t modulator_base::modulate_int(uint8_t bit)
{
    return 0;
}

// **************************************************************** //
//                                                                  //
//                                                                  //
// dds_afsk_modulator_adapter                                       //
//                                                                  //
//                                                                  //
// **************************************************************** //

dds_afsk_modulator_adapter::dds_afsk_modulator_adapter(double f_mark, double f_space,
    int bitrate, int sample_rate,
    double alpha)
    : dds_mod(f_mark, f_space, bitrate, sample_rate, alpha)
{
}

double dds_afsk_modulator_adapter::modulate(uint8_t bit)
{
    return dds_mod.modulate(bit);
}

void dds_afsk_modulator_adapter::reset()
{
    dds_mod.reset();
}

int dds_afsk_modulator_adapter::samples_per_bit() const
{
    return dds_mod.samples_per_bit();
}

// **************************************************************** //
//                                                                  //
//                                                                  //
// dds_afsk_modulator_fast_adapter                                  //
//                                                                  //
//                                                                  //
// **************************************************************** //

dds_afsk_modulator_fast_adapter::dds_afsk_modulator_fast_adapter(double f_mark, double f_space,
    int bitrate, int sample_rate)
    : dds_mod(f_mark, f_space, bitrate, sample_rate)
{
}

double dds_afsk_modulator_fast_adapter::modulate(uint8_t bit)
{
    return dds_mod.modulate(bit);
}

void dds_afsk_modulator_fast_adapter::reset()
{
    dds_mod.reset();
}

int dds_afsk_modulator_fast_adapter::samples_per_bit() const
{
    return dds_mod.samples_per_bit();
}

// **************************************************************** //
//                                                                  //
//                                                                  //
// cpfsk_modulator_adaptor                                          //
//                                                                  //
//                                                                  //
// **************************************************************** //

cpfsk_modulator_adaptor::cpfsk_modulator_adaptor(double f_mark, double f_space,
    int bitrate, int sample_rate)
    : cpfsk_mod(f_mark, f_space, bitrate, sample_rate)
{
}

double cpfsk_modulator_adaptor::modulate(uint8_t bit)
{
    return cpfsk_mod.modulate(bit);
}

void cpfsk_modulator_adaptor::reset()
{
    cpfsk_mod.reset();
}

int cpfsk_modulator_adaptor::samples_per_bit() const
{
    return cpfsk_mod.samples_per_bit();
}

// **************************************************************** //
//                                                                  //
//                                                                  //
// bessel_null_modulator_adapter                                    //
//                                                                  //
//                                                                  //
// **************************************************************** //

bessel_null_modulator_adapter::bessel_null_modulator_adapter(double f_mark, double f_space,
    int bitrate, int sample_rate,
    double alpha)
    : bessel_mod(f_mark, f_space, bitrate, sample_rate, alpha)
{
}

double bessel_null_modulator_adapter::modulate(uint8_t bit)
{
    return bessel_mod.modulate(bit);
}

void bessel_null_modulator_adapter::reset()
{
    bessel_mod.reset();
}

int bessel_null_modulator_adapter::samples_per_bit() const
{
    return bessel_mod.samples_per_bit();
}
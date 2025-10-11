#pragma once

#include <cstdint>
#include <vector>
#include <cmath>

// **************************************************************** //
//                                                                  //
//                                                                  //
// dds_afsk_modulator                                               //
//                                                                  //
//                                                                  //
// **************************************************************** //

struct dds_afsk_modulator
{
    dds_afsk_modulator(double f_mark, double f_space, int bitrate, int sample_rate, double alpha);

    double modulate(uint8_t bit);
    void reset();
    int samples_per_bit() const;

private:
    double f_mark;
    double f_space;
    int sample_rate;
    double alpha;
    double freq_smooth;
    double phase = 0.0;
    int samples_per_bit_;
};

// **************************************************************** //
//                                                                  //
//                                                                  //
// dds_afsk_modulator_fast                                          //
//                                                                  //
//                                                                  //
// **************************************************************** //

template <typename T>
struct dds_afsk_modulator_fast
{
    dds_afsk_modulator_fast(double f_mark, double f_space, int bitrate, int sample_rate);

    T modulate(uint8_t bit);
    void reset();
    int samples_per_bit() const;

private:
    double f_mark;
    double f_space;
    int sample_rate;
    int samples_per_bit_;
    std::vector<T> lookup_table_;
    unsigned int lookup_table_bits_ = 0;
    unsigned int lookup_table_mask_ = 0;
    unsigned int phase_accumulator_ = 0;
    unsigned int phase_increment_mark_ = 0;
    unsigned int phase_increment_space_ = 0;
};

template<typename T>
inline dds_afsk_modulator_fast<T>::dds_afsk_modulator_fast(double f_mark, double f_space, int bitrate, int sample_rate) : f_mark(f_mark), f_space(f_space), sample_rate(sample_rate), samples_per_bit_(static_cast<int>((sample_rate + (bitrate / 2)) / bitrate))
{
    const unsigned int default_lut_size = 1024;

    unsigned int lut_size = default_lut_size;

    unsigned int bits = 0;
    while ((1u << bits) != lut_size) { ++bits; }
    lookup_table_bits_ = bits;
    lookup_table_mask_ = lut_size - 1;

    lookup_table_.resize(lut_size);

    constexpr double two_pi = 2.0 * 3.14159265358979323846;

    for (unsigned int i = 0; i < lut_size; i++)
    {
        double theta = two_pi * static_cast<double>(i) / static_cast<double>(lut_size);
        double s = (std::sin)(theta);

        if constexpr (std::is_same<T, int16_t>::value)
        {
            lookup_table_[i] = static_cast<int16_t>(s * 32767.0);  // Scale to int16_t range
        }
        else if constexpr (std::is_same<T, double>::value)
        {
            lookup_table_[i] = s;
        }
    }

    phase_increment_mark_ = static_cast<unsigned int>(((static_cast<uint64_t>(static_cast<unsigned int>(this->f_mark)) << 32) / static_cast<uint64_t>(this->sample_rate)));
    phase_increment_space_ = static_cast<unsigned int>(((static_cast<uint64_t>(static_cast<unsigned int>(this->f_space)) << 32) / static_cast<uint64_t>(this->sample_rate)));
    phase_accumulator_ = 0;
}

template<typename T>
inline T dds_afsk_modulator_fast<T>::modulate(uint8_t bit)
{
    // Select phase increment based on bit value (mark = 1, space = 0)
    const unsigned int phase_increment = bit ? phase_increment_mark_ : phase_increment_space_;

    // Update phase accumulator
    phase_accumulator_ += phase_increment;

    // Extract lookup table index from upper bits of phase accumulator
    const unsigned int shift_amount = 32u - lookup_table_bits_;
    const unsigned int index = (phase_accumulator_ >> shift_amount) & lookup_table_mask_;

    return lookup_table_[index];
}

template<typename T>
inline void dds_afsk_modulator_fast<T>::reset()
{
    phase_accumulator_ = 0;
}

template<typename T>
inline int dds_afsk_modulator_fast<T>::samples_per_bit() const
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

class cpfsk_modulator
{
public:
    cpfsk_modulator(double f_mark, double f_space, int bitrate, int sample_rate);

    double modulate(uint8_t bit);
    void reset();
    int samples_per_bit() const;

private:
    double f_center_;         // Center frequency (Hz) - midpoint between mark and space
    double f_delta_;          // Frequency deviation (Hz) - half the difference between mark and space
    int samples_per_bit_;     // Number of samples per bit period
    int sample_rate_;         // Audio sample rate (Hz)
    double m_;                // Phase integration accumulator (integral of NRZ bitstream)
    std::vector<double> bitstream_nrz_;  // NRZ-encoded bitstream (+1 or -1 per bit)
    int current_sample_;      // Current sample index in the output stream
};

// **************************************************************** //
//                                                                  //
//                                                                  //
// bessel_null_modulator                                            //
//                                                                  //
//                                                                  //
// **************************************************************** //

struct bessel_null_modulator
{
    static constexpr double pi = 3.14159265358979323846;

    bessel_null_modulator(double f_mark, double f_space, int bitrate, int sample_rate, double alpha);

    double modulate(uint8_t bit);
    void reset();
    int samples_per_bit() const;

private:
    void compute_bessel_window();
    double bessel_i0(double x);

    double f_mark_;              // Mark frequency (typically represents '1')
    double f_space_;             // Space frequency (typically represents '0')
    int bitrate_;                // Bits per second
    int sample_rate_;            // Samples per second
    double alpha_;               // Transition bandwidth factor (0-1)

    double phase_;               // Current phase accumulator
    int sample_index_;           // Current sample within bit period
    int samples_per_bit_;        // Number of samples per bit
    int transition_samples_;     // Number of samples for frequency transition
    double current_freq_;        // Current instantaneous frequency
    bool use_mark_;              // Toggle between mark and space

    std::vector<double> bessel_window_;  // Precomputed transition window
};

// **************************************************************** //
//                                                                  //
//                                                                  //
// modulator_base                                                   //
//                                                                  //
//                                                                  //
// **************************************************************** //

struct modulator_base
{
    virtual double modulate(uint8_t bit);
    virtual int16_t modulate_int(uint8_t bit);
    virtual void reset() = 0;
    virtual int samples_per_bit() const = 0;
    virtual ~modulator_base() = default;
};

// **************************************************************** //
//                                                                  //
//                                                                  //
// dds_afsk_modulator_adapter                                       //
//                                                                  //
//                                                                  //
// **************************************************************** //

struct dds_afsk_modulator_adapter : public modulator_base
{
    dds_afsk_modulator_adapter(double f_mark = 1200.0, double f_space = 2200.0, int bitrate = 1200, int sample_rate = 48000, double alpha = 0.3);

    double modulate(uint8_t bit) override;
    void reset() override;
    int samples_per_bit() const override;

private:
    dds_afsk_modulator dds_mod;
};

// **************************************************************** //
//                                                                  //
//                                                                  //
// dds_afsk_modulator_fast_adapter                                  //
//                                                                  //
//                                                                  //
// **************************************************************** //

struct dds_afsk_modulator_fast_adapter : public modulator_base
{
    dds_afsk_modulator_fast_adapter(double f_mark = 1200.0, double f_space = 2200.0, int bitrate = 1200, int sample_rate = 48000);
   
    double modulate(uint8_t bit) override;
    void reset() override;
    int samples_per_bit() const override;

private:
    dds_afsk_modulator_fast<double> dds_mod;
};

// **************************************************************** //
//                                                                  //
//                                                                  //
// cpfsk_modulator_adaptor                                          //
//                                                                  //
//                                                                  //
// **************************************************************** //

struct cpfsk_modulator_adaptor : public modulator_base
{
    cpfsk_modulator_adaptor(double f_mark = 1200.0, double f_space = 2200.0, int bitrate = 1200, int sample_rate = 48000);

    double modulate(uint8_t bit) override;
    void reset() override;
    int samples_per_bit() const override;

private:
    cpfsk_modulator cpfsk_mod;
};

// **************************************************************** //
//                                                                  //
//                                                                  //
// bessel_null_modulator_adapter                                    //
//                                                                  //
//                                                                  //
// **************************************************************** //

struct bessel_null_modulator_adapter : public modulator_base
{
    bessel_null_modulator_adapter(double f_mark = 1200.0, double f_space = 2200.0, int bitrate = 1200, int sample_rate = 48000, double alpha = 0.08);

    double modulate(uint8_t bit) override;
    void reset() override;
    int samples_per_bit() const override;

private:
    bessel_null_modulator bessel_mod;
};
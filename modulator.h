#pragma once

#include <cstdint>
#include <vector>

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

struct dds_afsk_modulator_fast
{
    dds_afsk_modulator_fast(double f_mark, double f_space, int bitrate, int sample_rate);

    double modulate(uint8_t bit);
    void reset();
    int samples_per_bit() const;

private:
    double f_mark;
    double f_space;
    int sample_rate;
    int samples_per_bit_;
    std::vector<double> lookup_table_;
    unsigned int lookup_table_bits_ = 0;
    unsigned int lookup_table_mask_ = 0;
    unsigned int phase_accumulator_ = 0;
    unsigned int phase_increment_mark_ = 0;
    unsigned int phase_increment_space_ = 0;
};

// **************************************************************** //
//                                                                  //
//                                                                  //
// dds_afsk_modulator_fast_int                                      //
//                                                                  //
//                                                                  //
// **************************************************************** //

struct dds_afsk_modulator_fast_int
{
    dds_afsk_modulator_fast_int(double f_mark, double f_space, int bitrate, int sample_rate);

    int16_t modulate_int(uint8_t bit);
    void reset();
    int samples_per_bit() const;

private:
    double f_mark;
    double f_space;
    int sample_rate;
    int samples_per_bit_;
    std::vector<int16_t> lookup_table_int_;
    unsigned int lookup_table_bits_ = 0;
    unsigned int lookup_table_mask_ = 0;
    unsigned int phase_accumulator_ = 0;
    unsigned int phase_increment_mark_ = 0;
    unsigned int phase_increment_space_ = 0;
};

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
    dds_afsk_modulator_fast dds_mod;
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
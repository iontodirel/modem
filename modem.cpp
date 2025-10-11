#include "modem.h"

#include "modulator.h"
#include "bitstream.h"
#include "audio_stream.h"

#include <memory>

// **************************************************************** //
//                                                                  //
//                                                                  //
// modem                                                            //
//                                                                  //
//                                                                  //
// **************************************************************** //

void modem::initialize(audio_stream& stream, modulator_base& modulator, bitstream_converter_base& converter)
{
    audio = std::ref(stream);
    mod = std::ref(modulator);
    conv = std::ref(converter);

    double ms_per_flag = (8.0 * 1000.0) / baud_rate_;

    preamble_flags = (std::max)(static_cast<int>(tx_delay_ms / ms_per_flag), 1);
    postamble_flags = (std::max)(static_cast<int>(tx_tail_ms / ms_per_flag), 1);
}

void modem::transmit()
{
    std::vector<uint8_t> bitstream({ 0 });

    transmit(bitstream);
}

void modem::transmit(aprs::router::packet p)
{
    bitstream_converter_base& converter = conv.value().get();

    // Convert packet to bitstream
    // 
    // - Compute CRC
    // - Append CRC to the AX.25 frame
    // - Convert bytes to bits (LSB-first)
    // - Bit-stuffing (insert 0 after five consecutive 1s)
    // - Add HDLC flags (0x7E) at start and end
    // - NRZI encoding (invert on 1, no change on 0)

    std::vector<uint8_t> bitstream = converter.encode(p, preamble_flags, postamble_flags);

    transmit(bitstream);
}

void modem::transmit(const std::vector<uint8_t>& bits)
{
    modulator_base& modulator = mod.value().get();
    struct audio_stream& audio_stream = audio.value().get();

    // AFSK modulation

    std::vector<double> audio_buffer;

    modulate_bitstream(bits, audio_buffer);

    // Apply pre-emphasis filter and gain

    postprocess_audio(audio_buffer);

    // Render audio to output audio device

    render_audio(audio_buffer);
}

void modem::postprocess_audio(std::vector<double>& audio_buffer)
{
    struct audio_stream& audio_stream = audio.value().get();

    int silence_samples = static_cast<int>(start_silence_duration_s * audio_stream.sample_rate());

    if (preemphasis_enabled)
    {
        apply_preemphasis(audio_buffer.begin() + silence_samples, audio_buffer.end(), audio_stream.sample_rate(), /*tau*/ 75e-6);
    }

    apply_gain(audio_buffer.begin() + silence_samples, audio_buffer.end(), gain_value);

    insert_silence(audio_buffer.begin(), audio_stream.sample_rate(), start_silence_duration_s);

    insert_silence(std::back_inserter(audio_buffer), audio_stream.sample_rate(), end_silence_duration_s);
}

void modem::modulate_bitstream(const std::vector<uint8_t>& bitstream, std::vector<double>& audio_buffer)
{
    modulator_base& modulator = mod.value().get();
    struct audio_stream& audio_stream = audio.value().get();

    size_t signal_samples = bitstream.size() * modulator.samples_per_bit();

    int silence_samples = static_cast<int>(start_silence_duration_s * audio_stream.sample_rate());

    audio_buffer.resize(silence_samples + signal_samples);

    int write_pos = silence_samples;
    for (uint8_t bit : bitstream)
    {
        for (int i = 0; i < modulator.samples_per_bit(); ++i)
        {
            audio_buffer[write_pos++] = modulator.modulate(bit);
        }
    }

    modulator.reset();
}

void modem::render_audio(const std::vector<double>& audio_buffer)
{
    struct audio_stream& audio_stream = audio.value().get();
    const size_t chunk_size = 480;  // 10ms at 48kHz
    size_t pos = 0;
    while (pos < audio_buffer.size())
    {
        size_t remaining = audio_buffer.size() - pos;
        size_t to_write = (std::min)(chunk_size, remaining);
        size_t written = audio_stream.write(&audio_buffer[pos], to_write);
        if (written > 0)
        {
            pos += written;
        }
        else
        {
            // Buffer full, wait a bit
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

size_t modem::receive(std::vector<aprs::router::packet>& packets)
{
    return 0;
}

void modem::preemphasis(bool enable)
{
    preemphasis_enabled = enable;
}

bool modem::preemphasis() const
{
    return preemphasis_enabled;
}

void modem::gain(double g)
{
    gain_value = g;
}

double modem::gain() const
{
    return gain_value;
}

void modem::start_silence(double d)
{
    if (d < 0.0) d = 0.0;
    start_silence_duration_s = d;
}

double modem::start_silence() const
{
    return start_silence_duration_s;
}

void modem::end_silence(double d)
{
    if (d < 0.0) d = 0.0;
    end_silence_duration_s = d;
}

double modem::end_silence() const
{
    return end_silence_duration_s;
}

void modem::tx_delay(double d)
{
    if (d < 0.0) d = 0.0;
    tx_delay_ms = d;
}

double modem::tx_delay() const
{
    return tx_delay_ms;
}

void modem::tx_tail(double d)
{
    if (d < 0.0) d = 0.0;
    tx_tail_ms = d;
}

double modem::tx_tail() const
{
    return tx_tail_ms;
}

void modem::baud_rate(int b)
{
    if (b <= 0) b = 1200;
    baud_rate_ = b;
}

int modem::baud_rate() const
{
    return baud_rate_;
}

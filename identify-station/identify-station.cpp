/*
 * 
 * 
 * Sending AM sigal to audio that works. Duration is Good here.
 * 
 * 
 * 
 */
#include <iostream>
#include <rtl-sdr.h>
#include <vector>
#include <cmath>
#include <liquid/liquid.h>
#include <alsa/asoundlib.h>
#include <chrono>
#include <array>
#include <map>
#include <fstream>
#include <string>
#include <sstream>


#define SAMPLE_RATE 1800000   // RTL-SDR sample rate
#define DECIMATION 40         // Reduce sample rate to ~51.2 kHz
#define AUDIO_RATE 48000      // Target audio sample rate
#define BUFFER_SIZE 16384     // Buffer size for async read

// Adjustable Morse timing parameters
#define DOT_DURATION 50    // Dot duration in milliseconds
#define DASH_DURATION 300   // Dash duration in milliseconds
#define CHAR_GAP 350        // Gap between characters in milliseconds
#define WORD_GAP 4000       // Gap between words in milliseconds

constexpr int PCM_RATE = 48000;
constexpr int MIN_DURATION_MS = 100;
constexpr int MIN_DURATION_SAMPLES = PCM_RATE * MIN_DURATION_MS / 1000;
constexpr double LOW_FREQ = 900.0;
constexpr double HIGH_FREQ = 1100.0;

#include "fir_coeffs_900_1100Hz.h"

rtlsdr_dev_t *dev = nullptr;
firfilt_rrrf filter;

int squelch_threshold = 20;  // Default squelch threshold (1-100)
std:string station_id = "";

std::map<std::string, char> morseMap = {
    {".-", 'A'}, {"-...", 'B'}, {"-.-.", 'C'}, {"-..", 'D'},
    {".", 'E'}, {"..-.", 'F'}, {"--.", 'G'}, {"....", 'H'},
    {"..", 'I'}, {".---", 'J'}, {"-.-", 'K'}, {".-..", 'L'},
    {"--", 'M'}, {"-.", 'N'}, {"---", 'O'}, {".--.", 'P'},
    {"--.-", 'Q'}, {".-.", 'R'}, {"...", 'S'}, {"-", 'T'},
    {"..-", 'U'}, {"...-", 'V'}, {".--", 'W'}, {"-..-", 'X'},
    {"-.--", 'Y'}, {"--..", 'Z'},
    {"-----", '0'}, {".----", '1'}, {"..---", '2'},
    {"...--", '3'}, {"....-", '4'}, {".....", '5'},
    {"-....", '6'}, {"--...", '7'}, {"---..", '8'},
    {"----.", '9'},
};

std::string morse;
auto last_tone_start = std::chrono::steady_clock::now();
auto last_tone_end = std::chrono::steady_clock::now();
double silence_duration;// = std::chrono::duration_cast<std::chrono::milliseconds>;

// Create Low-pass FIR filter using liquid-dsp
void createLowPassFilter(float cutoff, float fs) {
    int filter_order = 101;
    filter = firfilt_rrrf_create_kaiser(filter_order, cutoff / (fs / 2), 60.0f, 0.0f);
}

// Low-pass FIR filter using liquid-dsp
void lowPassFilter(std::vector<float> &signal) {
    for (size_t i = 0; i < signal.size(); i++) {
        firfilt_rrrf_push(filter, signal[i]);
        firfilt_rrrf_execute(filter, &signal[i]);
    }
}

// Simple FIR filter function
std::vector<double> apply_fir_filter(const std::vector<double>& input) {
    std::vector<double> output(input.size(), 0);
    for (size_t i = FIR_ORDER; i < input.size(); ++i) {
        double sum = 0;
        for (size_t j = 0; j < FIR_ORDER; ++j) {
            sum += input[i - j] * fir_coeffs[j];
        }
        output[i] = sum;
    }
    return output;
}

// Simple Hamming-windowed FIR band-pass filter (900–1100 Hz)
class FIRFilter {
    static constexpr int TAPS = 101;
    std::array<double, TAPS> coeffs;
    std::vector<int16_t> history;
    int pos = 0;

public:
    FIRFilter() : history(TAPS, 0) {
        double fc_low = LOW_FREQ / PCM_RATE;
        double fc_high = HIGH_FREQ / PCM_RATE;

        for (int i = 0; i < TAPS; i++) {
            int m = i - (TAPS - 1) / 2;
            if (m == 0) {
                coeffs[i] = 2 * (fc_high - fc_low);
            } else {
                coeffs[i] = (sin(2 * M_PI * fc_high * m) - sin(2 * M_PI * fc_low * m)) / (M_PI * m);
            }
            // Apply Hamming window
            coeffs[i] *= 0.54 - 0.46 * cos(2 * M_PI * i / (TAPS - 1));
        }
    }

    double process(int16_t sample) {
        history[pos] = sample;
        double output = 0;
        int j = pos;
        for (int i = 0; i < TAPS; i++) {
            output += coeffs[i] * history[j];
            j = (j - 1 + TAPS) % TAPS;
        }
        pos = (pos + 1) % TAPS;
        return output;
    }
};

// A simple Goertzel detector for detecting a frequency in a buffer
class GoertzelDetector {
    double s_prev = 0, s_prev2 = 0;
    double coeff;

public:
    GoertzelDetector(double target_freq, int sample_rate, int buffer_size) {
        double normalized_freq = target_freq / sample_rate;
        coeff = 2 * cos(2 * M_PI * normalized_freq);
    }

    void reset() {
        s_prev = s_prev2 = 0;
    }

    void process(const std::vector<double>& buffer) {
        reset();
        for (double sample : buffer) {
            double s = sample + coeff * s_prev - s_prev2;
            s_prev2 = s_prev;
            s_prev = s;
        }
    }

    double get_power() const {
        return s_prev2 * s_prev2 + s_prev * s_prev - coeff * s_prev * s_prev2;
    }
};

// Main class for tone detection
class ToneDetector {
    GoertzelDetector goertzel_low;
    GoertzelDetector goertzel_high;
    std::chrono::steady_clock::time_point start_time;
    bool tone_active = false;
    int active_samples = 0;
    double noise_floor = 1e-6; // Start with a small nonzero noise floor

public:
    ToneDetector(int sample_rate, int buffer_size)
        : goertzel_low(LOW_FREQ, sample_rate, buffer_size),
          goertzel_high(HIGH_FREQ, sample_rate, buffer_size) {
        start_time = std::chrono::steady_clock::now();
    }

    void process_buffer(const std::vector<int16_t>& buffer) {
        std::vector<double> normalized_buffer(buffer.size());
        static char idCode[10] = "         ";
        static int idInx = 0;

        // Normalize PCM values to [-1,1]
        for (size_t i = 0; i < buffer.size(); ++i) {
            normalized_buffer[i] = buffer[i] / 32768.0;
        }

        // Apply FIR band-pass filter
        std::vector<double> filtered_signal = apply_fir_filter(normalized_buffer);

        // Compute Goertzel power
        goertzel_low.process(filtered_signal);
        goertzel_high.process(filtered_signal);

        double power_low = goertzel_low.get_power();
        double power_high = goertzel_high.get_power();
        double power = power_low + power_high;

// Adaptive noise floor update
//        noise_floor = 0.99 * noise_floor + 0.01 * power;
//        std::cout << "Power: " << power << " | Noise Floor: " << noise_floor << "\n";
//        std::cout << "Power low: " << power_low << "Power high: " << power_high << " Total power: " << power << "\n";
//        bool detected = power > (noise_floor * 4.0);

        bool detected = power > 4;
        if (detected) {
            active_samples += buffer.size();
            if (!tone_active && active_samples >= MIN_DURATION_SAMPLES) {
                tone_active = true;
                auto now = std::chrono::steady_clock::now();
//                double elapsed_ms = std::chrono::duration<double, std::milli>(now - start_time).count();
				silence_duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_tone_end).count();
//                std::cout << "Tone started at: " << elapsed_ms << " ms, >>>>>>> space is " << silence_duration << std::endl;
				last_tone_start = now;
				tone_active = true;
            }
        } else {
            if (tone_active && active_samples >= MIN_DURATION_SAMPLES) {
                auto now = std::chrono::steady_clock::now();
//                double elapsed_ms = std::chrono::duration<double, std::milli>(now - start_time).count();
				auto tone_duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_tone_start).count();
//                std::cout << "Tone stopped at: " << elapsed_ms << " ms, duration is " << tone_duration << std::endl;
				if (tone_duration < DOT_DURATION) morse += '.';
				else morse += '-';
				
				last_tone_end = now;
				tone_active = false;
            }
            tone_active = false;
            active_samples = 0;
        }
        
        if (tone_active && !morse.empty()) {
            if (silence_duration > WORD_GAP) {
                if (morseMap.count(morse)) std::cout << morseMap[morse];
                std::cout << "\n" << std::flush;
                idCode[idInx++] = morseMap[morse];
                idCode[idInx] = 0;
                idInx = 0;
                std::cout << "Decoded ID: " << idCode << std::endl;
                if (station_id == idCode) {
                    std::cout << "Station ID matched: " << station_id << std::endl;
                } else {
                    std::cout << "Station ID did not match." << std::endl;
                }
                morse.clear();

            } else if (silence_duration > CHAR_GAP) {
                if (morseMap.count(morse)) {
                    std::cout << morseMap[morse] << std::flush;
                    idCode[idInx++] = morseMap[morse];
                }
                morse.clear();
            }
        }
    }
};

void processIQ(uint8_t *iq_buffer, uint32_t length, int squelch_threshold) {
    static ToneDetector detector(PCM_RATE, length);
    size_t num_samples = length / 2;
    std::vector<float> i_samples(num_samples), q_samples(num_samples), am_signal(num_samples / DECIMATION);

    // Convert IQ samples to floating point values
    for (size_t i = 0; i < num_samples; i++) {
        i_samples[i] = (iq_buffer[2 * i] - 127.5f) / 127.5f;
        q_samples[i] = (iq_buffer[2 * i + 1] - 127.5f) / 127.5f;
    }

    lowPassFilter(i_samples);
    lowPassFilter(q_samples);

    // Apply squelch: mute signals below the threshold
    float threshold = squelch_threshold / 100.0;
    for (size_t i = 0; i < num_samples; i++) {
        float signal_magnitude = std::sqrt(i_samples[i] * i_samples[i] + q_samples[i] * q_samples[i]);
        if (signal_magnitude < threshold) {
            i_samples[i] = 0.0f;
            q_samples[i] = 0.0f;
        }
    }

    // AGC Variables
    static float gain = 1.0f; // Initial gain
    const float target_level = 0.5f; // Desired average amplitude
    const float agc_rate = 0.01f; // Adjusts how fast AGC reacts

    for (size_t j = 0; j < num_samples; j += DECIMATION) {
        float i = i_samples[j] * gain;
        float q = q_samples[j] * gain;
        am_signal[j / DECIMATION] = std::sqrt(i * i + q * q); // Envelope detection

        // AGC: Adjust gain based on signal level
        float signal_level = am_signal[j / DECIMATION];
        gain *= (1.0f + agc_rate * (target_level - signal_level)); // Adaptive gain control
    }

    // Convert to PCM format
    std::vector<int16_t> pcm(am_signal.size());
    for (size_t i = 0; i < am_signal.size(); i++) {
		int16_t sample = static_cast<int16_t>(am_signal[i] * 32767);
        pcm[i] = sample;
    }
	detector.process_buffer(pcm);	// instead, you can send it to audio...
    }

// RTL-SDR Async Callback Function
void rtlCallback(uint8_t *buf, uint32_t len, void *ctx) {
    if (len > 0) {
        processIQ(buf, len, squelch_threshold);
    }
}

int main(int argc, char **argv) {
    int freq = 114300000; // Default frequency

    // Parse frequency argument
    if (argc > 1) {
        freq = static_cast<int>(atof(argv[1]) * 1e6);
    }

    // Parse squelch threshold argument
    if (argc > 2) {
        int threshold = atoi(argv[2]);
        if (threshold >= 1 && threshold <= 100) {
            squelch_threshold = threshold;
        } else {
            std::cerr << "Invalid squelch value (must be 1-100), using default: " << squelch_threshold << "\n";
        }
    }

    // Parse station_id argument
    if (argc > 3) {
        station_id = argv[3];
        std::cout << "Station ID argument: " << station_id << std::endl;
    }

    if (rtlsdr_open(&dev, 0) < 0) {
        std::cerr << "Failed to open RTL-SDR" << std::endl;
        return -1;
    }

    rtlsdr_set_center_freq(dev, freq);
    rtlsdr_set_sample_rate(dev, SAMPLE_RATE);
    // Enable AGC mode
    if (rtlsdr_set_agc_mode(dev, 1) < 0) {
        std::cerr << "Failed to enable AGC mode" << std::endl;
    } else {
        std::cout << "AGC mode enabled successfully" << std::endl;
    }
    rtlsdr_reset_buffer(dev);

    createLowPassFilter(3000.0f, SAMPLE_RATE);

    std::cout << "Starting RTL-SDR async stream... at " << float(freq/1000000.0) << "Mhz" << std::endl;
    rtlsdr_read_async(dev, rtlCallback, nullptr, 0, BUFFER_SIZE);

    firfilt_rrrf_destroy(filter);
    rtlsdr_close(dev);
    return 0;
}

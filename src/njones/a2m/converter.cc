#include "converter.h"

#include <math.h>
#include <algorithm>
#include <string>

njones::audio::a2m::Note::Note() : pitch(0), velocity(0), count(0) {}

njones::audio::a2m::Note::Note(const unsigned int pitch, const unsigned int velocity)
    : pitch(pitch), velocity(velocity), count(0) {}

bool njones::audio::a2m::Note::operator<(const njones::audio::a2m::Note& rhs) const {
    return velocity < rhs.velocity;
}

bool njones::audio::a2m::Note::operator>(const njones::audio::a2m::Note& rhs) const {
    return velocity > rhs.velocity;
}

bool njones::audio::a2m::Note::operator==(const njones::audio::a2m::Note& rhs) const {
    return pitch == rhs.pitch;
}

njones::audio::a2m::Converter::Converter(const unsigned int samplerate,
                                         const unsigned int block_size,
                                         const double activation_level,
                                         const std::vector<unsigned int> pitch_set,
                                         const std::array<unsigned int, 2> pitch_range,
                                         const unsigned int note_count,
                                         const int transpose,
                                         const double ceiling)
    : samplerate(samplerate),
      block_size(block_size),
      activation_level(activation_level),
      pitch_set(pitch_set),
      pitch_range(pitch_range),
      note_count(note_count),
      notes(njones::audio::a2m::generate_notes()),
      fft_output(nullptr),
      fft_input(nullptr) {
    set_activation_level(activation_level);
    set_transpose(transpose);
    set_ceiling(ceiling);
    fft_plan = fftw_plan_dft_r2c_1d(0, nullptr, nullptr, FFTW_ESTIMATE);
    determine_ranges();

    for (unsigned int i = 0; i < 128; ++i) {
        accumulator[i].pitch = i;
        accumulator[i].amplitude = 0.0;
        accumulator[i].count = 0;
    }
}

njones::audio::a2m::Converter::~Converter() {
    if (fft_output != nullptr)
        free(fft_output);
    if (fft_input != nullptr)
        free(fft_input);
    fftw_destroy_plan(fft_plan);
}

void njones::audio::a2m::Converter::set_samplerate(const unsigned int samplerate) {
    std::lock_guard<std::recursive_mutex> guard(lock);
    if (this->samplerate != samplerate) {
        this->samplerate = samplerate;
        determine_ranges();
    }
}
void njones::audio::a2m::Converter::set_block_size(const unsigned int block_size) {
    std::lock_guard<std::recursive_mutex> guard(lock);
    if (this->block_size != block_size) {
        this->block_size = block_size;
        determine_ranges();
    }
}
void njones::audio::a2m::Converter::set_activation_level(const double activation_level) {
    std::lock_guard<std::recursive_mutex> guard(lock);
    this->activation_level = activation_level;
    if (activation_level != 0.0)
        velocity_limit = static_cast<unsigned int>(127 * activation_level);
    else
        velocity_limit = 1;
}
void njones::audio::a2m::Converter::set_pitch_set(const std::vector<unsigned int>& pitch_set) {
    std::lock_guard<std::recursive_mutex> guard(lock);
    this->pitch_set = pitch_set;
    cached_freqs.clear();
}
void njones::audio::a2m::Converter::set_pitch_range(const std::array<unsigned int, 2>& pitch_range) {
    std::lock_guard<std::recursive_mutex> guard(lock);
    this->pitch_range = pitch_range;
}
void njones::audio::a2m::Converter::set_note_count(const int note_count) {
    std::lock_guard<std::recursive_mutex> guard(lock);
    this->note_count = note_count;
}
void njones::audio::a2m::Converter::set_transpose(const int transpose) {
    std::lock_guard<std::recursive_mutex> guard(lock);
    this->transpose = std::max(-127, transpose);
    this->transpose = std::min(127, this->transpose);
}
void njones::audio::a2m::Converter::set_ceiling(const double ceiling) {
    std::lock_guard<std::recursive_mutex> guard(lock);
    this->ceiling = std::max(0.0, ceiling);
    this->ceiling = std::min(1.0, this->ceiling);
}

void njones::audio::a2m::Converter::determine_ranges() {
    time_window = std::chrono::milliseconds(static_cast<int>(block_size / (static_cast<double>(samplerate) / 1000)));
    if (time_window.count() > 0) {
        max_freq = std::min(notes[127].high, static_cast<double>(samplerate) / 2);
        min_freq = std::max(notes[0].low, static_cast<double>(1000 / time_window.count()));
        bins = block_size / 2;
        bin_freqs = std::vector<double>(bins);

        for (size_t i = 0; i < bins; ++i)
            bin_freqs[i] = static_cast<double>(i * samplerate) / block_size;

        min_bin = 0;
        for (unsigned int i = 0; i < bins; ++i)
            if (bin_freqs[i] >= min_freq) {
                min_bin = i;
                break;
            }

        max_bin = bins - 1;
        for (unsigned int i = 0; i < bins; ++i)
            if (bin_freqs[i] >= max_freq) {
                max_bin = i - 1;
                break;
            }

        frequencies.resize(max_bin - min_bin);

        if (fft_output != nullptr)
            free(fft_output);

        if (fft_input != nullptr)
            free(fft_input);

        fft_output = (fftw_complex*)malloc(block_size * sizeof(fftw_complex));
        if (fft_output == nullptr)
            throw std::runtime_error("Failed to allocate FFT output buffer.");

        fft_input = (double*)malloc(block_size * sizeof(double));
        if (fft_input == nullptr)
            throw std::runtime_error("Failed to allocate FFT input buffer.");

        fftw_destroy_plan(fft_plan);
        fft_plan = fftw_plan_dft_r2c_1d(block_size, fft_input, fft_output, FFTW_ESTIMATE);
    }
}

template <class T, class C>
static T nearest_value(T val, C arr) {
    auto copy = arr;
    std::sort(copy.begin(), copy.end());

    auto lower = std::lower_bound(copy.begin(), copy.end(), val);
    auto upper = std::upper_bound(copy.begin(), copy.end(), val);

    if (lower == copy.end() && upper == copy.end())
        return arr.back();
    else if (upper == copy.end())
        return *lower;
    else if (lower == copy.end())
        return *upper;
    else {
        auto lower_diff = val - *lower;
        auto upper_diff = *upper - val;

        if (lower_diff < upper_diff)
            return *lower;
        return *upper;
    }
}

unsigned int njones::audio::a2m::Converter::snap_to_key(unsigned int pitch) {
    if (pitch_set.size() > 0) {
        unsigned int mod = pitch % 12;
        pitch = (12 * (pitch / 12)) + nearest_value(mod, pitch_set);
    }
    int ret = pitch;
    ret = std::min(ret, 127);
    return std::max(0, ret);
}

unsigned int njones::audio::a2m::Converter::freq_to_pitch(const double freq) {
    try {
        return cached_freqs.at(freq);
    } catch (const std::exception&) {
        int ret = 128;
        for (auto& note : notes) {
            if (note.second.low <= freq && freq <= note.second.high) {
                ret = snap_to_key(note.first);
                break;
            }
        }
        cached_freqs[freq] = ret;
        return ret;
    }
}

unsigned int njones::audio::a2m::Converter::amplitude_to_velocity(const double amplitude) {
    return std::min(127, static_cast<int>(127 * (amplitude / (bins * ceiling))));
}

std::vector<njones::audio::a2m::Note> njones::audio::a2m::Converter::freqs_to_notes() {
    std::vector<njones::audio::a2m::Note> ret;

    for (const auto& freq : frequencies) {
        if (freq.second > 0.0)
            [[likely]] {
                auto& note = accumulator[freq_to_pitch(freq.first)];
                note.amplitude += freq.second;
                note.count += 1;
            }
    }

    for (auto& note : accumulator) {
        const int new_pitch = note.pitch + transpose;
        if (note.count > 0 && new_pitch >= pitch_range[0] && new_pitch <= pitch_range[1]) {
            auto new_note = njones::audio::a2m::Note(new_pitch, amplitude_to_velocity(note.amplitude / note.count));
            if (new_note.velocity > velocity_limit)
                ret.push_back(new_note);
            note.count = 0;
            note.amplitude = 0.0;
        }
    }

    if (note_count > 0) {
        std::sort(ret.begin(), ret.end(), std::greater<>());
        std::vector<njones::audio::a2m::Note> slice;
        if (ret.size() >= note_count)
            std::copy(ret.begin(), ret.begin() + note_count, std::back_inserter(slice));
        else
            slice = ret;
        return slice;
    } else
        return ret;
}

void njones::audio::a2m::Converter::samples_to_freqs(double* samples) {
    memcpy(fft_input, samples, sizeof(double) * block_size);
    fftw_execute(fft_plan);
    for (size_t i = min_bin; i < max_bin; ++i)
        frequencies[i - min_bin] = {bin_freqs[i], sqrt(pow(fft_output[i][0], 2) + pow(fft_output[i][1], 2))};
}

std::vector<njones::audio::a2m::Note> njones::audio::a2m::Converter::convert(double* samples) {
    std::lock_guard<std::recursive_mutex> guard(lock);
    samples_to_freqs(samples);
    return freqs_to_notes();
}

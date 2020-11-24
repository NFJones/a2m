#include "converter.h"

#include <fftw3.h>
#include <math.h>
#include <algorithm>
#include <string>

a2m::Note::Note() : pitch(0), velocity(0), count(0) {}

a2m::Note::Note(const unsigned int pitch, const unsigned int velocity) : pitch(pitch), velocity(velocity), count(0) {}

bool a2m::Note::operator<(const a2m::Note& rhs) {
    return velocity < rhs.velocity;
}

a2m::Converter::Converter(const unsigned int samplerate,
                          const unsigned int block_size,
                          const double activation_level,
                          const int transpose,
                          const std::vector<unsigned int> pitch_set,
                          const std::array<unsigned int, 2> pitch_range,
                          const unsigned int note_count)
    : samplerate(samplerate),
      block_size(block_size),
      activation_level(activation_level),
      transpose(transpose),
      pitch_set(pitch_set),
      pitch_range(pitch_range),
      note_count(note_count),
      notes(a2m::generate_notes()) {
    if (activation_level != 0.0)
        velocity_limit = 127 * activation_level;
    else
        velocity_limit = 1;

    determine_ranges();
}

a2m::Converter::~Converter() {}

void a2m::Converter::set_samplerate(const unsigned int samplerate) {
    std::lock_guard<std::recursive_mutex> guard(lock);
    this->samplerate = samplerate;
    determine_ranges();
}
void a2m::Converter::set_block_size(const unsigned int block_size) {
    std::lock_guard<std::recursive_mutex> guard(lock);
    this->block_size = block_size;
    determine_ranges();
}
void a2m::Converter::set_activation_level(const double activation_level) {
    std::lock_guard<std::recursive_mutex> guard(lock);
    this->activation_level = activation_level;
}
void a2m::Converter::set_transpose(const int transpose) {
    std::lock_guard<std::recursive_mutex> guard(lock);
    this->transpose = transpose;
}
void a2m::Converter::set_pitch_set(const std::vector<unsigned int>& pitch_set) {
    std::lock_guard<std::recursive_mutex> guard(lock);
    this->pitch_set = pitch_set;
}
void a2m::Converter::set_pitch_range(const std::array<unsigned int, 2>& pitch_range) {
    std::lock_guard<std::recursive_mutex> guard(lock);
    this->pitch_range = pitch_range;
}

void a2m::Converter::determine_ranges() {
    time_window = std::chrono::milliseconds(static_cast<int>(block_size / (static_cast<double>(samplerate) / 1000)));
    max_freq = std::min(notes[127].high, static_cast<double>(samplerate) / 2);
    min_freq = std::max(notes[0].low, static_cast<double>(1000 / time_window.count()));
    bins = block_size / 2;
    bin_freqs = std::vector<double>{};

    for (size_t i = 0; i < bins; ++i)
        bin_freqs.push_back(static_cast<double>(i * samplerate) / block_size);

    min_bin = 0;
    for (size_t i = 0; i < bin_freqs.size(); ++i) {
        if (bin_freqs[i] >= min_freq) {
            min_bin = i;
            break;
        }
    }

    max_bin = bin_freqs.size() - 1;
    for (size_t i = 0; i < bin_freqs.size(); ++i) {
        if (bin_freqs[i] >= max_freq) {
            max_bin = i - 1;
            break;
        }
    }
}

template <class T, class C>
static T nearest_value(T val, C arr) {
    auto copy = arr;
    std::sort(copy.begin(), copy.end());

    for (auto iter = copy.begin(); iter != copy.end(); iter++) {
        if (*iter < val) {
            if ((iter + 1 == copy.end()) || ((val - *iter) < (*(iter + 1) - val)))
                return *iter;
            else
                return *(iter++);
        }
    }

    throw std::runtime_error("Failed to determin nearest value for: " + std::to_string(val));
}

unsigned int a2m::Converter::snap_to_key(unsigned int pitch) {
    if (pitch_set.size() > 0) {
        unsigned int mod = pitch % 12;
        pitch = (12 * (pitch / 12)) + nearest_value(mod, pitch_set);
    }
    int ret = pitch + transpose;
    ret = std::min(ret, 127);
    return std::max(0, ret);
}

unsigned int a2m::Converter::freq_to_pitch(const double freq) {
    static std::map<double, unsigned int> memo;
    unsigned int ret = 127;
    try {
        ret = memo.at(freq);
    } catch (const std::exception&) {
        for (auto& note : notes) {
            if (note.second.low <= freq && freq <= note.second.high) {
                ret = snap_to_key(note.first);
                break;
            }
        }
        memo[freq] = ret;
    }
    return ret;
}

unsigned int a2m::Converter::amplitude_to_velocity(const double amplitude) {
    return std::min(127, static_cast<int>(127 * (amplitude / bins)));
}

std::vector<a2m::Note> a2m::Converter::freqs_to_notes(const std::vector<std::pair<double, double>>& freqs) {
    std::vector<a2m::Note> ret;

    std::vector<a2m::Note> accumulator(128);
    for (size_t i = 0; i < 128; ++i) {
        accumulator[i].pitch = i;
        accumulator[i].velocity = 0;
    }

    for (const auto& freq : freqs) {
        auto new_note = a2m::Note(freq_to_pitch(freq.first), amplitude_to_velocity(freq.second));
        if (new_note.velocity > 0) {
            accumulator[new_note.pitch].pitch = new_note.pitch;
            accumulator[new_note.pitch].velocity =
                ((accumulator[new_note.pitch].velocity * accumulator[new_note.pitch].count) + new_note.velocity) /
                (accumulator[new_note.pitch].count + 1);
            accumulator[new_note.pitch].count += 1;
        }
    }

    for (auto& note : accumulator)
        if (note.count > 0 && note.pitch >= pitch_range[0] && note.pitch <= pitch_range[1]) {
            ret.push_back(note);
        }

    if (note_count > 0) {
        std::sort(ret.begin(), ret.end());
        std::vector<a2m::Note> slice;
        std::copy(ret.begin(), ret.begin() + note_count + 1, std::back_inserter(slice));
        return slice;
    } else
        return ret;
}

std::vector<std::pair<double, double>> a2m::Converter::samples_to_freqs(double* samples) {
    auto ret = std::vector<std::pair<double, double>>(bins);

    std::vector<double> in(samples, samples + block_size);
    std::vector<fftw_complex> out(block_size);

    fftw_plan p = fftw_plan_dft_r2c_1d(block_size, in.data(), out.data(), FFTW_ESTIMATE);
    fftw_execute(p);

    for (size_t i = 0; i < bins; ++i) {
        ret[i] = {bin_freqs[i], sqrt(pow(out[i][0], 2)) + sqrt(pow(out[i][1], 2))};
    }

    fftw_destroy_plan(p);
    return ret;
}

std::vector<a2m::Note> a2m::Converter::convert(double* samples) {
    std::lock_guard<std::recursive_mutex> guard(lock);
    const auto freqs = samples_to_freqs(samples);
    return freqs_to_notes(freqs);
}

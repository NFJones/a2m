#include "converter.h"

#include <fftw3.h>
#include <math.h>
#include <algorithm>
#include <string>

a2m::Note::Note() : pitch(0), velocity(0), count(0) {}

a2m::Note::Note(const unsigned int pitch, const unsigned int velocity) : pitch(pitch), velocity(velocity), count(0) {}

bool a2m::Note::operator<(const a2m::Note& rhs) const {
    return velocity < rhs.velocity;
}

bool a2m::Note::operator>(const a2m::Note& rhs) const {
    return velocity > rhs.velocity;
}

bool a2m::Note::operator==(const a2m::Note& rhs) const {
    return pitch == rhs.pitch;
}

a2m::Converter::Converter(const unsigned int samplerate,
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
      notes(a2m::generate_notes()) {
    set_activation_level(activation_level);
    set_transpose(transpose);
    set_ceiling(ceiling);
    determine_ranges();
}

a2m::Converter::~Converter() {}

void a2m::Converter::set_samplerate(const unsigned int samplerate) {
    std::lock_guard<std::recursive_mutex> guard(lock);
    if (this->samplerate != samplerate) {
        this->samplerate = samplerate;
        determine_ranges();
    }
}
void a2m::Converter::set_block_size(const unsigned int block_size) {
    std::lock_guard<std::recursive_mutex> guard(lock);
    if (this->block_size != block_size) {
        this->block_size = block_size;
        determine_ranges();
    }
}
void a2m::Converter::set_activation_level(const double activation_level) {
    std::lock_guard<std::recursive_mutex> guard(lock);
    this->activation_level = activation_level;
    if (activation_level != 0.0)
        velocity_limit = 127 * activation_level;
    else
        velocity_limit = 1;
}
void a2m::Converter::set_pitch_set(const std::vector<unsigned int>& pitch_set) {
    std::lock_guard<std::recursive_mutex> guard(lock);
    this->pitch_set = pitch_set;
}
void a2m::Converter::set_pitch_range(const std::array<unsigned int, 2>& pitch_range) {
    std::lock_guard<std::recursive_mutex> guard(lock);
    this->pitch_range = pitch_range;
}
void a2m::Converter::set_note_count(const int note_count) {
    std::lock_guard<std::recursive_mutex> guard(lock);
    this->note_count = note_count;
}
void a2m::Converter::set_transpose(const int transpose) {
    std::lock_guard<std::recursive_mutex> guard(lock);
    this->transpose = std::max(-127, transpose);
    this->transpose = std::min(127, this->transpose);
}
void a2m::Converter::set_ceiling(const double ceiling) {
    std::lock_guard<std::recursive_mutex> guard(lock);
    this->ceiling = std::max(0.0, ceiling);
    this->ceiling = std::min(1.0, this->ceiling);
}

void a2m::Converter::determine_ranges() {
    time_window = std::chrono::milliseconds(static_cast<int>(block_size / (static_cast<double>(samplerate) / 1000)));
    if (time_window.count() > 0) {
        max_freq = std::min(notes[127].high, static_cast<double>(samplerate) / 2);
        min_freq = std::max(notes[0].low, static_cast<double>(1000 / time_window.count()));
        bins = block_size / 2;
        bin_freqs = std::vector<double>(bins);

        for (size_t i = 0; i < bins; ++i)
            bin_freqs[i] = static_cast<double>(i * samplerate) / block_size;

        min_bin = 0;
        for (size_t i = 0; i < bins; ++i) {
            if (bin_freqs[i] >= min_freq) {
                min_bin = i;
                break;
            }
        }

        max_bin = bins - 1;
        for (size_t i = 0; i < bins; ++i) {
            if (bin_freqs[i] >= max_freq) {
                max_bin = i - 1;
                break;
            }
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

    throw std::runtime_error("Failed to determine nearest value for: " + std::to_string(val));
}

unsigned int a2m::Converter::snap_to_key(unsigned int pitch) {
    if (pitch_set.size() > 0) {
        unsigned int mod = pitch % 12;
        pitch = (12 * (pitch / 12)) + nearest_value(mod, pitch_set);
    }
    int ret = pitch;
    ret = std::min(ret, 127);
    return std::max(0, ret);
}

unsigned int a2m::Converter::freq_to_pitch(const double freq) {
    int ret = 127;
    try {
        ret = cached_freqs.at(freq);
    } catch (const std::exception&) {
        for (auto& note : notes) {
            if (note.second.low <= freq && freq <= note.second.high) {
                ret = snap_to_key(note.first);
                break;
            }
        }
        cached_freqs[freq] = ret;
    }
    ret = std::min(ret, 127);
    return std::max(0, ret);
}

unsigned int a2m::Converter::amplitude_to_velocity(const double amplitude) {
    return std::min(127, static_cast<int>(127 * (amplitude / (bins * ceiling))));
}

struct AccummulatedNote {
    unsigned int pitch;
    double amplitude;
    size_t count;
};

std::vector<a2m::Note> a2m::Converter::freqs_to_notes(const std::vector<std::pair<double, double>>& freqs) {
    std::vector<a2m::Note> ret;

    static std::vector<AccummulatedNote> accumulator(128);
    for (size_t i = 0; i < 128; ++i) {
        accumulator[i].pitch = i;
        accumulator[i].amplitude = 0.0;
        accumulator[i].count = 0;
    }

    for (const auto& freq : freqs) {
        auto new_note = AccummulatedNote{freq_to_pitch(freq.first), freq.second, 0};
        accumulator[new_note.pitch].pitch = new_note.pitch;
        accumulator[new_note.pitch].amplitude =
            ((accumulator[new_note.pitch].amplitude * accumulator[new_note.pitch].count) + new_note.amplitude) /
            (accumulator[new_note.pitch].count + 1);
        accumulator[new_note.pitch].count += 1;
    }

    for (auto& note : accumulator) {
        if (note.count > 0 && note.pitch >= pitch_range[0] && note.pitch <= pitch_range[1]) {
            auto new_note = a2m::Note(note.pitch + transpose, amplitude_to_velocity(note.amplitude));
            if (new_note.velocity > velocity_limit)
                ret.push_back(new_note);
        }
    }

    if (note_count > 0) {
        std::sort(ret.begin(), ret.end(), std::greater<>());
        std::vector<a2m::Note> slice;
        if (ret.size() >= note_count)
            std::copy(ret.begin(), ret.begin() + note_count, std::back_inserter(slice));
        else
            slice = ret;
        return slice;
    } else
        return ret;
}

std::vector<std::pair<double, double>> a2m::Converter::samples_to_freqs(double* samples) {
    auto ret = std::vector<std::pair<double, double>>(bins);

    std::vector<fftw_complex> out(block_size);

    fftw_plan p = fftw_plan_dft_r2c_1d(block_size, samples, out.data(), FFTW_ESTIMATE | FFTW_PRESERVE_INPUT);
    fftw_execute(p);

    for (size_t i = 0; i < bins; ++i)
        ret[i] = {bin_freqs[i], sqrt(pow(out[i][0], 2) + pow(out[i][1], 2))};

    fftw_destroy_plan(p);
    return ret;
}

std::vector<a2m::Note> a2m::Converter::convert(double* samples) {
    std::lock_guard<std::recursive_mutex> guard(lock);
    return freqs_to_notes(samples_to_freqs(samples));
}

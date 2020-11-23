#include <stdint.h>

#include <a2m/notes.h>
#include <array>
#include <chrono>
#include <mutex>
#include <vector>

namespace a2m {
struct Note {
    Note(const uint8_t pitch = 0, const uint8_t velocity = 0);

    uint8_t pitch;
    uint8_t velocity;
    unsigned int count;

    bool operator<(const Note& rhs);
};

class Converter {
   public:
    Converter(const unsigned int samplerate,
              const unsigned int block_size,
              const double activation_level = 0.0,
              const int transpose = 0,
              const std::vector<unsigned int> pitch_set = std::vector<unsigned int>{},
              const std::array<uint, 2> pitch_range = std::array<uint, 2>{0, 127},
              const unsigned int note_count = 0);
    ~Converter();

    std::vector<Note> convert(double* samples);
    void set_samplerate(const unsigned int samplerate);
    void set_block_size(const unsigned int block_size);
    void set_activation_level(const double activation_level);
    void set_transpose(const int transpose);
    void set_pitch_set(const std::vector<unsigned int>& pitch_set);
    void set_pitch_range(const std::array<uint, 2>& pitch_range);

   protected:
    unsigned int samplerate;
    unsigned int block_size;
    double activation_level;
    unsigned int velocity_limit;
    int transpose;
    std::vector<unsigned int> pitch_set;
    std::array<uint, 2> pitch_range;
    unsigned int note_count;
    note_map notes;
    double min_freq;
    double max_freq;
    std::chrono::milliseconds time_window;
    unsigned int min_bin;
    unsigned int max_bin;
    unsigned int bins;
    std::vector<double> bin_freqs;

    void determine_ranges();
    std::vector<std::pair<double, double>> samples_to_freqs(double* samples);
    std::vector<Note> freqs_to_notes(const std::vector<std::pair<double, double>>& freqs);
    uint8_t freq_to_pitch(const double freq);
    uint8_t amplitude_to_velocity(const double amplitude);
    uint8_t snap_to_key(uint8_t pitch);

   private:
    Converter(const Converter&) = delete;
    Converter(Converter&&) = delete;

    std::recursive_mutex lock;
};
}  // namespace a2m
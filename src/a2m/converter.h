#include <a2m/notes.h>
#include <stddef.h>
#include <array>
#include <chrono>
#include <mutex>
#include <vector>

namespace a2m {
/**
 * @brief A MIDI note event representation containing the pitch and velocity.
 */
struct Note {
    Note();
    Note(const unsigned int pitch, const unsigned int velocity);
    Note(const Note& rhs) = default;
    Note(Note&& rhs) = default;

    Note& operator=(const Note& rhs) = default;
    Note& operator=(Note&& rhs) = default;
    bool operator<(const Note& rhs) const;
    bool operator==(const Note& rhs) const;

    unsigned int pitch;
    unsigned int velocity;
    unsigned int count;
};

/**
 * @brief An FFT to MIDI note converter that analyzes a block of samples
 * and maps the frequency data to the 12 tone equal temperment scale.
 * Time related parameter setting is supported and is thread safe.
 */
class Converter {
   public:
    /**
     * @param samplerate The samplerate of the audio data passed into convert.
     * @param block_size The number of samples processed per call to convert().
     * @param activation_level The normalized amplitude threshold in the range [0.0, 1.0].
     * Notes with velocities below the threshold will be ignored.
     * @param pitch_set A set of pitches in the range [0, 11] to which MIDI notes should be snapped.
     * @param pitch_range A range from [0, 127] used for filtering out unwanted MIDI notes.
     * @param note_count The maximum number of notes to return per conversion.
     */
    Converter(const unsigned int samplerate,
              const unsigned int block_size,
              const double activation_level = 0.0,
              const std::vector<unsigned int> pitch_set = std::vector<unsigned int>{},
              const std::array<unsigned int, 2> pitch_range = std::array<unsigned int, 2>{0, 127},
              const unsigned int note_count = 0);
    ~Converter();

    /**
     * @brief Converts a block of samples into a2m::Note instances.
     * @param samples
     * @return
     */
    std::vector<Note> convert(double* samples);

    void set_samplerate(const unsigned int samplerate);
    void set_block_size(const unsigned int block_size);
    void set_activation_level(const double activation_level);
    void set_pitch_set(const std::vector<unsigned int>& pitch_set);
    void set_pitch_range(const std::array<unsigned int, 2>& pitch_range);
    void set_note_count(const int note_count);

   protected:
    unsigned int samplerate;
    unsigned int block_size;
    double activation_level;
    unsigned int velocity_limit;
    std::vector<unsigned int> pitch_set;
    std::array<unsigned int, 2> pitch_range;
    unsigned int note_count;
    note_map notes;
    double min_freq;
    double max_freq;
    std::chrono::milliseconds time_window;
    unsigned int min_bin;
    unsigned int max_bin;
    unsigned int bins;
    std::vector<double> bin_freqs;
    std::map<double, unsigned int> cached_freqs;

    void determine_ranges();
    std::vector<std::pair<double, double>> samples_to_freqs(double* samples);
    std::vector<Note> freqs_to_notes(const std::vector<std::pair<double, double>>& freqs);
    unsigned int freq_to_pitch(const double freq);
    unsigned int amplitude_to_velocity(const double amplitude);
    unsigned int snap_to_key(unsigned int pitch);

   private:
    Converter(const Converter&) = delete;
    Converter(Converter&&) = delete;

    std::recursive_mutex lock;
};
}  // namespace a2m
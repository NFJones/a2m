#include <map>
#include <vector>

namespace a2m {
struct note_range {
    double low;
    double mid;
    double high;
};

typedef std::map<int, note_range> note_map;

/**
 * @brief Generates a map of note ranges [low, mid, high] which can be used to map
 * data returned from FFTs into the 12 tone equal temperment scale.
 * @return a2m::note_map
 */
note_map generate_notes();
}  // namespace a2m
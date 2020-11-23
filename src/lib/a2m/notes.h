#include <unordered_map>
#include <vector>

namespace a2m {
struct note_range {
    double low;
    double mid;
    double high;
};

typedef std::unordered_map<int, note_range> note_map;

note_map generate_notes();
}  // namespace a2m
#include <vector>

// /mnt/c/Users/neilf/Downloads/stereo.wav
// samplerate: 96000 Hz
// channels: 2
// duration: 1.163 s
// format: WAV (Microsoft) [WAV]
// subtype: Signed 16 bit PCM [PCM_16]

namespace a2m {
namespace test {
int stereo_samplerate();
std::vector<std::vector<double>> stereo_samples();
}
}

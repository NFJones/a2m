#include <a2m/converter.h>
#include <cxxtest/TestSuite.h>
#include <iostream>

#include "data/stereo.h"

using namespace std;

class audio_to_midi_test_suite : public CxxTest::TestSuite {
   public:
    void test_stereo_conversion() {
        auto samples = a2m::test::stereo_samples();
        auto block_size = 512;
        auto converter = a2m::Converter(a2m::test::stereo_samplerate(), block_size);

        for (auto& channel : samples) {
            for (size_t i = 0; i < channel.size() / block_size; ++i) {
                auto notes = converter.convert(channel.data() + (i * block_size));
            }
        }
        TS_ASSERT(true);
    }
};

# a2m

`a2m` is a C++ library that is able to convert raw sound samples into MIDI notes. It is intended for use in VSTs and other plugin frameworks.
The `fftw` library was used for the FFT calculation and must be linked.

## Example

```c++
#include <a2m/converter.h>

void my_converter(double *samples, size_t nsamples, int samplerate, int block_size) {
    auto converter = a2m::Converter(samplerate, block_size);

    for (size_t i = 0; i < nsamples / block_size; ++i) {
        auto notes = converter.convert(samples + (i * block_size));

        for (auto &note : notes) {
            std::cout << "Pitch =" << note.pitch << endl;        // 0 - 127
            std::cout << "Velocity = " << note.velocity << endl; // 1 - 127
        }
    }
}
```

#include "notes.h"

a2m::note_map a2m::generate_notes() {
    auto notes = a2m::note_map();

    // 12th root of 2
    static const double multiplier = 1.0594630943592953;

    // C0
    notes[0] = a2m::note_range{7.946362749, 8.1757989155, 8.4188780665};
    for (ssize_t i = 1; i < 128; ++i) {
        notes[i] = a2m::note_range{7.946362749, 8.1757989155, 8.4188780665};
        notes[i].mid = multiplier * notes[i - 1].mid;
        notes[i].low = (notes[i].mid + notes[i - 1].mid) / 2.0;
        notes[i].high = (notes[i].mid + (multiplier * notes[i].mid)) / 2.0;
    }

    return notes;
}
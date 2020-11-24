#!/usr/bin/env python3

import argparse
import soundfile
import os


def generate_sample_include(infile, outfile):
    info = soundfile.info(infile)
    samples = soundfile.read(infile, always_2d=True)

    channels = [[] for _ in range(info.channels)]
    for sample in samples[0]:
        for channel in range(info.channels):
            channels[channel].append(sample[channel])

    outname = os.path.basename(outfile)

    info_lines = [f"// {line}\n" for line in str(info).split("\n")]

    with open(f"{outfile}.h", "w") as output:
        output.write("#include <vector>\n\n")
        output.writelines(info_lines)
        output.write("\nnamespace a2m {\n")
        output.write("namespace test {\n")
        output.write(f"int {outname}_samplerate();\n")
        output.write(f"std::vector<std::vector<double>> {outname}_samples();\n")
        output.write("}\n")
        output.write("}\n")

    with open(f"{outfile}.cc", "w") as output:
        output.write(f'#include "{outname}.h"\n')
        output.write("\n")
        output.write(f"int a2m::test::{outname}_samplerate() {{\n")
        output.write(f"    return {info.samplerate};\n")
        output.write("}\n\n")
        output.write(
            f"std::vector<std::vector<double>> a2m::test::{outname}_samples() {{\n"
        )
        output.write("    auto SAMPLES = std::vector<std::vector<double>>();\n")
        output.write("\n")
        for channel, data in enumerate(channels):
            output.write("    SAMPLES.push_back(std::vector<double>{\n")
            for sample in data:
                output.write(f"        {float(sample)},\n")
            output.write("    });\n")
        output.write("\n")
        output.write("    return SAMPLES;\n")
        output.write("}\n")
        output.write("\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("infile", help="The soundfile to generate sample data from.")
    parser.add_argument("outfile", help="The C++ .cc and .h file pair to generate.")
    args = parser.parse_args()

    generate_sample_include(args.infile, args.outfile)


if __name__ == "__main__":
    main()

import numpy as np
from scipy.signal import firwin

FIR_ORDER = 101
LOW = 900
HIGH = 1100
FS = 48000

coeffs = firwin(FIR_ORDER, [LOW, HIGH], pass_zero=False, fs=FS)

with open("fir_coeffs_900_1100Hz.h", "w") as f:
    f.write("#pragma once\n")
    f.write(f"constexpr int FIR_ORDER = {FIR_ORDER};\n")
    f.write("static const double fir_coeffs[FIR_ORDER] = {\n")
    for i, c in enumerate(coeffs):
        f.write(f"    {c:.16e},\n")
    f.write("};\n")
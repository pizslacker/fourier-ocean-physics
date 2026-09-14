# Ocean-physics

This implementation uses the classic **Tessendorf model** for ocean simulation. It generates a 1D frequency spectrum based on wind direction, evolves the wave phases over time using the physical dispersion relation **($\omega = \sqrt{gk}$)**, and converts the frequencies back into physical spatial heights using an Inverse Discrete Fourier Transform (IDFT).

- **Painter's Algorithm** (drawing from back to front) and `SDL_RenderGeometry` to construct solid, shaded polygons for the faces of the water block.
- **The Spectrum (P)**: Rather than manually creating individual sine waves, we assign random Gaussian amplitudes to hundreds of frequencies at once. Lower frequencies (large swells) are assigned exponentially higher energy than high frequencies (choppy ripples).
- **Dispersion ($\omega = \sqrt{g\vert{}k\vert{}}$)**: In real oceans, large waves travel faster than small waves. The square-root gravity dispersion relation enforces this behavior perfectly, stopping the surface from looking like rigid moving noise.
- **Complex Conjugation**: By feedingd `conj(h0[-k])` into the time-step equation alongside `h0[k]`, we mathematically guarantee that when the imaginary frequencies are collapsed by the IDFT, the resulting physical height data is strictly real (no floating-point phasing errors).

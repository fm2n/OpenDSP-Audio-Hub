# OpenDSP-Audio-Hub
An audio system based on ADI SigmaDSP ADAU1452, featuring three input sources: Raspberry Pi 4B streamer, Amanero USB audio interface, and SPDIF, with 6-channel high-quality DAC and PGA outputs.

## Features
- Network audio streaming via Raspberry Pi
- USB Audio (Amanero Combo384)
- SPDIF coaxial input
- ADAU1452 DSP for EQ, crossover and routing
- 6-channel high-quality DAC + PGA (AD1852 + CS3310)
- Raspberry Pi Pico for system control

## Repository Structure
- `hardware/` – Altium Designer projects
- `dsp/` – SigmaStudio projects
- `firmware/` – Pico (Arduino) firmware
- `docs/` – Documentation and diagrams

## Status
- Hardware: A00 prototype working, with some bodge wires
- DSP: Current functional requirements implemented and working
- Firmware: Current functional requirements implemented and working

## License
- Firmware and software: MIT License
- Hardware design files: CERN-OHL-S v2

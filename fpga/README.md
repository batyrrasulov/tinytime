# TinyTime FPGA — Custom Circuits

This directory contains two custom FPGA circuits for the TinyTime project,
designed for the **DE10-Standard** board (Cyclone V 5CSXFC6D6F31C6).

## Circuits

### Circuit 1: BCD to 7-Segment Decoder (`hex_decoder`)

**Type:** Combinational logic

The HPS writes packed BCD digits via the HPS-to-FPGA lightweight bridge.
The FPGA decodes each 4-bit nibble into a 7-segment display pattern and
continuously drives the six HEX displays.

- **Input:** 16-bit packed BCD from HPS (4 digits × 4 bits)
- **Output:** 6 × 7-segment display patterns wired to HEX0–HEX5
- **Avalon-MM register 0x00:** HEX3–HEX0 packed BCD
- **Avalon-MM register 0x04:** HEX5–HEX4 packed BCD

### Circuit 2: Hardware Key Debouncer (`key_debouncer`)

**Type:** Sequential logic (counter-based)

Debounces the four KEY push buttons in hardware using a double-flop
synchroniser and per-key saturating counters.  Provides an edge-capture
register for detecting button press events.  **This fixes the broken
KEY peripheral** that was stuck at 0x00000000 in the stock FPGA image.

- **Input:** 4-bit KEY signal from physical buttons (active-low)
- **Output:** Debounced key state + edge-capture register
- **Avalon-MM register 0x00 (read):** Debounced KEY state
- **Avalon-MM register 0x04 (R/W):** Edge capture (write 1 to clear)
- **Debounce period:** ~21 ms at 50 MHz (configurable via DEBOUNCE_BITS parameter)

---

## Directory Structure

```
fpga/
├── rtl/
│   ├── bcd_to_7seg.v           Single-digit BCD → 7-seg (used by hex_decoder)
│   ├── hex_decoder.v           Circuit 1: full decoder with Avalon-MM
│   ├── key_debouncer.v         Circuit 2: debouncer with Avalon-MM
│   └── tinytime_fpga_top.v     Top-level template (connects to soc_system)
├── testbench/
│   ├── tb_hex_decoder.v        Testbench for hex_decoder
│   ├── tb_key_debouncer.v      Testbench for key_debouncer
│   └── run_sim.sh              Script to run simulations with Icarus Verilog
├── ip/
│   ├── hex_decoder_hw.tcl      Platform Designer component descriptor
│   └── key_debouncer_hw.tcl    Platform Designer component descriptor
├── quartus/
│   ├── tinytime_fpga.qpf       Quartus project file
│   ├── tinytime_fpga.qsf       Settings and pin assignments
│   └── tinytime_fpga.sdc       Timing constraints (50 MHz)
└── README.md                   This file
```

---

## Running Testbenches (Simulation)

Requires Icarus Verilog (`iverilog`).  On macOS: `brew install icarus-verilog`.

```bash
cd fpga/testbench
./run_sim.sh all      # Run both testbenches
./run_sim.sh hex      # Run hex_decoder only
./run_sim.sh key      # Run key_debouncer only
```

For waveform simulation (for the design spec), the testbenches generate
`.vcd` files.  Open them with GTKWave: `gtkwave hex_decoder.vcd`.

---

## Quartus Build (Lab PC)

The FPGA bitstream must be built on the lab PC with **Quartus Prime**.
No internet is needed — everything is on the USB drive.

**The approach:** Modify the existing DE10-Standard Computer project
(provided in class under `.docs/DE10-Standard_Computer/verilog/`) to
replace the stock HEX and KEY PIO components with our custom circuits.
Same addresses, same MMIO interface — the C application sees no change.

### Quick Start

See **[LAB_PC_STEPS.txt](LAB_PC_STEPS.txt)** for the detailed,
step-by-step lab PC guide (designed to follow with machines side by side).

### Summary of Changes to Existing Project

1. **Platform Designer:** Remove HEX/KEY PIO → Add hex_decoder + key_debouncer
2. **Top-level Verilog:** Update wire declarations and port connections
3. **Compile → Generate .rbf → Flash to board**

The address map is unchanged:
| Peripheral | Offset from 0xFF200000 | Status |
|---|---|---|
| LEDR | 0x00000000 | Unchanged |
| hex_decoder | 0x00000020 | Replaces hex3_hex0 PIO |
| SW | 0x00000040 | Unchanged |
| key_debouncer | 0x00000050 | Replaces pushbuttons PIO |

---

## HPS Application Integration

The C application (`tinytime`) supports both the stock and custom FPGA
images via the `--custom-fpga` command-line flag:

```bash
# Stock FPGA image (terminal-only control, SW→KEY emulation)
./tinytime

# Custom FPGA image (physical buttons work, BCD to 7-seg in hardware)
./tinytime --custom-fpga
```

When `--custom-fpga` is active:
- HEX display uses packed BCD format (FPGA does the 7-seg decoding)
- Physical KEY buttons work natively (hardware debouncer)
- Physical SW switches are read for mode selection
- Terminal SSH commands still work alongside physical inputs

---

## Fallback Plans

If the Quartus build or custom FPGA integration fails:

1. **Fallback A:** Use the stock FPGA image (`assets/soc_system.rbf`). The
   C app works with terminal-only control (no `--custom-fpga`).
2. **Fallback B:** Add Verilog source files to the Quartus project without
   modifying Platform Designer. The circuits exist as documented code with
   passing testbenches — this still shows custom FPGA work.
3. **Fallback C:** Demo simulation waveforms in the screencast (VCD files
   from testbenches) alongside the working terminal-control application.

## Troubleshooting

| Issue | Fix |
|-------|-----|
| HEX displays show inverted segments | Remove the `~` inversion in the top-level `assign` statements |
| HEX displays show backwards MM:SS | Swap the digit-to-display mapping in `hex_decoder.v` |
| KEY still reads 0x00000000 | Verify key_debouncer is connected correctly in Platform Designer; check conduit export |
| Address discovery fails | Update the device tree overlay from the new Qsys `.sopcinfo` |
| "HEX format unknown" warning | Expected on first run; the code falls back to 7-seg (use `--custom-fpga` to force packed BCD) |

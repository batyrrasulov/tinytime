# ============================================================================
# TinyTime FPGA — Timing Constraints (SDC)
# Target: DE10-Standard (50 MHz main clock)
# ============================================================================

# 50 MHz board clock → 20 ns period
create_clock -name CLOCK_50 -period 20.000 [get_ports {CLOCK_50}]

# Derive PLL clocks automatically (if any PLLs are instantiated)
derive_pll_clocks

# Derive clock uncertainty
derive_clock_uncertainty

# Constrain I/O timing (relaxed — FPGA fabric peripherals are not timing-critical)
set_input_delay  -clock CLOCK_50 -max 5.0 [get_ports {KEY[*]}]
set_input_delay  -clock CLOCK_50 -min 0.0 [get_ports {KEY[*]}]
set_input_delay  -clock CLOCK_50 -max 5.0 [get_ports {SW[*]}]
set_input_delay  -clock CLOCK_50 -min 0.0 [get_ports {SW[*]}]
set_output_delay -clock CLOCK_50 -max 5.0 [get_ports {HEX0[*] HEX1[*] HEX2[*] HEX3[*] HEX4[*] HEX5[*]}]
set_output_delay -clock CLOCK_50 -min 0.0 [get_ports {HEX0[*] HEX1[*] HEX2[*] HEX3[*] HEX4[*] HEX5[*]}]
set_output_delay -clock CLOCK_50 -max 5.0 [get_ports {LEDR[*]}]
set_output_delay -clock CLOCK_50 -min 0.0 [get_ports {LEDR[*]}]

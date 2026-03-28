# modify_qsys.tcl
# TinyTime — Platform Designer (Qsys) Automation Script
#
# This script modifies the existing Computer_System.qsys to:
#   1) Remove HEX3_HEX0, HEX5_HEX4, and Pushbuttons PIOs
#   2) Add hex_decoder custom component at address 0x0000_0020
#   3) Add key_debouncer custom component at address 0x0000_0050
#   4) Connect both to system clock, reset, and HPS LW bridge
#   5) Export conduit interfaces for top-level wiring
#
# Usage (from the verilog/ directory):
#   qsys-script --script=ip/tinytime/modify_qsys.tcl
#
# After running, regenerate HDL:
#   qsys-generate Computer_System.qsys --synthesis=VERILOG

package require -exact qsys 16.0

# Load the existing system
load_system Computer_System.qsys

# ====================================================================
# Step 1: Remove the old PIO components we are replacing
# ====================================================================
puts "Removing old HEX3_HEX0 PIO..."
remove_instance HEX3_HEX0

puts "Removing old HEX5_HEX4 PIO..."
remove_instance HEX5_HEX4

puts "Removing old Pushbuttons PIO..."
remove_instance Pushbuttons

# ====================================================================
# Step 2: Add hex_decoder custom component
# ====================================================================
puts "Adding hex_decoder_0..."
add_instance hex_decoder_0 hex_decoder

# Connect clock and reset
add_connection system_pll.sys_clk hex_decoder_0.clock
add_connection system_pll.sys_clk_reset hex_decoder_0.reset

# Connect Avalon-MM slave to HPS lightweight bridge
add_connection ARM_A9_HPS.h2f_lw_axi_master hex_decoder_0.avs
set_connection_parameter_value ARM_A9_HPS.h2f_lw_axi_master/hex_decoder_0.avs baseAddress 0x00000020

# Export the HEX conduit signals to top level
set_interface_property hex_decoder_0_hex_conduit EXPORT_OF hex_decoder_0.hex_conduit

# ====================================================================
# Step 3: Add key_debouncer custom component
# ====================================================================
puts "Adding key_debouncer_0..."
add_instance key_debouncer_0 key_debouncer

# Connect clock and reset
add_connection system_pll.sys_clk key_debouncer_0.clock
add_connection system_pll.sys_clk_reset key_debouncer_0.reset

# Connect Avalon-MM slave to HPS lightweight bridge
add_connection ARM_A9_HPS.h2f_lw_axi_master key_debouncer_0.avs
set_connection_parameter_value ARM_A9_HPS.h2f_lw_axi_master/key_debouncer_0.avs baseAddress 0x00000050

# Export the KEY conduit signals to top level
set_interface_property key_debouncer_0_key_conduit EXPORT_OF key_debouncer_0.key_conduit

# ====================================================================
# Step 4: Save
# ====================================================================
puts "Saving modified Computer_System.qsys..."
save_system Computer_System.qsys

puts ""
puts "=== Done! ==="
puts "Next steps:"
puts "  1) qsys-generate Computer_System.qsys --synthesis=VERILOG"
puts "  2) Replace DE10_Standard_Computer.v with the modified version"
puts "  3) Compile in Quartus"

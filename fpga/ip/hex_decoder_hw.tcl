# hex_decoder_hw.tcl
# Platform Designer (Qsys) Component Descriptor for hex_decoder
#
# To use: In Platform Designer, go to "IP Catalog" → right-click →
# "Add IP Search Path…" → add the fpga/ip/ folder.
# The "BCD to 7-Seg HEX Decoder" component will then appear.

package require -exact qsys 16.0

set_module_property NAME                  hex_decoder
set_module_property DISPLAY_NAME          "BCD to 7-Seg HEX Decoder (TinyTime)"
set_module_property VERSION               1.0
set_module_property GROUP                 "TinyTime Custom"
set_module_property DESCRIPTION           "Decodes packed BCD into 7-segment patterns for HEX displays"
set_module_property AUTHOR                "TinyTime"
set_module_property INSTANTIATE_IN_SYSTEM_MODULE true

# --------------------------------------------------------------------------
# File sets
# --------------------------------------------------------------------------
add_fileset QUARTUS_SYNTH QUARTUS_SYNTH "" ""
set_fileset_property QUARTUS_SYNTH TOP_LEVEL hex_decoder
add_fileset_file hex_decoder.v  VERILOG PATH hex_decoder.v  TOP_LEVEL_FILE
add_fileset_file bcd_to_7seg.v  VERILOG PATH bcd_to_7seg.v

add_fileset SIM_VERILOG SIM_VERILOG "" ""
set_fileset_property SIM_VERILOG TOP_LEVEL hex_decoder
add_fileset_file hex_decoder.v  VERILOG PATH hex_decoder.v  TOP_LEVEL_FILE
add_fileset_file bcd_to_7seg.v  VERILOG PATH bcd_to_7seg.v

# --------------------------------------------------------------------------
# Clock interface
# --------------------------------------------------------------------------
add_interface           clock clock end
set_interface_property  clock clockRate 0
add_interface_port      clock clk clk Input 1

# --------------------------------------------------------------------------
# Reset interface (active-low)
# --------------------------------------------------------------------------
add_interface           reset reset end
set_interface_property  reset associatedClock clock
set_interface_property  reset synchronousEdges DEASSERT
add_interface_port      reset reset_n reset_n Input 1

# --------------------------------------------------------------------------
# Avalon-MM slave interface
# --------------------------------------------------------------------------
add_interface           avs avalon end
set_interface_property  avs addressUnits          WORDS
set_interface_property  avs associatedClock       clock
set_interface_property  avs associatedReset       reset
set_interface_property  avs readLatency           0
set_interface_property  avs readWaitTime          0
set_interface_property  avs writeWaitTime         0
set_interface_property  avs holdTime              0
set_interface_property  avs setupTime             0

add_interface_port avs avs_address   address   Input  2
add_interface_port avs avs_write     write     Input  1
add_interface_port avs avs_writedata writedata Input  32
add_interface_port avs avs_read      read      Input  1
add_interface_port avs avs_readdata  readdata  Output 32

# --------------------------------------------------------------------------
# Conduit interface — export HEX display signals to top level
# --------------------------------------------------------------------------
add_interface           hex_conduit conduit end
set_interface_property  hex_conduit associatedClock clock
set_interface_property  hex_conduit associatedReset reset

add_interface_port hex_conduit hex0 hex0 Output 7
add_interface_port hex_conduit hex1 hex1 Output 7
add_interface_port hex_conduit hex2 hex2 Output 7
add_interface_port hex_conduit hex3 hex3 Output 7
add_interface_port hex_conduit hex4 hex4 Output 7
add_interface_port hex_conduit hex5 hex5 Output 7

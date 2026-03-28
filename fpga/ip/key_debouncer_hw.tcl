# key_debouncer_hw.tcl
# Platform Designer (Qsys) Component Descriptor for key_debouncer
#
# To use: In Platform Designer, go to "IP Catalog" → right-click →
# "Add IP Search Path…" → add the fpga/ip/ folder.
# The "Key Debouncer" component will then appear.

package require -exact qsys 16.0

set_module_property NAME                  key_debouncer
set_module_property DISPLAY_NAME          "Hardware Key Debouncer (TinyTime)"
set_module_property VERSION               1.0
set_module_property GROUP                 "TinyTime Custom"
set_module_property DESCRIPTION           "Debounces 4 KEY inputs with edge-capture register"
set_module_property AUTHOR                "TinyTime"
set_module_property INSTANTIATE_IN_SYSTEM_MODULE true

# --------------------------------------------------------------------------
# File sets
# --------------------------------------------------------------------------
add_fileset QUARTUS_SYNTH QUARTUS_SYNTH "" ""
set_fileset_property QUARTUS_SYNTH TOP_LEVEL key_debouncer
add_fileset_file key_debouncer.v VERILOG PATH key_debouncer.v TOP_LEVEL_FILE

add_fileset SIM_VERILOG SIM_VERILOG "" ""
set_fileset_property SIM_VERILOG TOP_LEVEL key_debouncer
add_fileset_file key_debouncer.v VERILOG PATH key_debouncer.v TOP_LEVEL_FILE

# --------------------------------------------------------------------------
# Parameters
# --------------------------------------------------------------------------
add_parameter          DEBOUNCE_BITS INTEGER 20
set_parameter_property DEBOUNCE_BITS DISPLAY_NAME "Debounce counter bits"
set_parameter_property DEBOUNCE_BITS DESCRIPTION  "Counter width; threshold = 2^N clocks. 20 → ~21 ms at 50 MHz."
set_parameter_property DEBOUNCE_BITS ALLOWED_RANGES 4:24
set_parameter_property DEBOUNCE_BITS HDL_PARAMETER  true

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
# Conduit interface — import KEY physical pins from top level
# --------------------------------------------------------------------------
add_interface           key_conduit conduit end
set_interface_property  key_conduit associatedClock clock
set_interface_property  key_conduit associatedReset reset

add_interface_port key_conduit key_in key_in Input 4
add_interface_port key_conduit key_out key_out Output 4

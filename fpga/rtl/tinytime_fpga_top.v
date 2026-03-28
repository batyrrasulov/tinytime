/*
 * tinytime_fpga_top.v
 * TinyTime FPGA Top-Level Module
 *
 * This file shows the MODIFICATIONS to make to the existing
 * DE10_Standard_Computer.v to integrate the two custom circuits.
 *
 * Strategy: Modify the GHRD's existing top-level to replace the
 * HEX and KEY PIO components with our custom hex_decoder and
 * key_debouncer circuits.
 *
 * APPROACH: Modify the existing Computer_System.qsys in Platform
 * Designer to add our two custom Avalon-MM components, then update
 * the top-level wiring.
 *
 * This file is a REFERENCE — copy the relevant sections into the
 * existing DE10_Standard_Computer.v on the lab PC.
 */

/*
 * ===================================================================
 * CHANGES TO EXISTING DE10_Standard_Computer.v
 * ===================================================================
 *
 * 1) In the REG/WIRE section, REMOVE these lines:
 *      wire [31:0] hex3_hex0;
 *      wire [15:0] hex5_hex4;
 *
 *    ADD these wires instead:
 */

// Custom circuit output wires
wire [6:0] hex_dec_hex0;
wire [6:0] hex_dec_hex1;
wire [6:0] hex_dec_hex2;
wire [6:0] hex_dec_hex3;
wire [6:0] hex_dec_hex4;
wire [6:0] hex_dec_hex5;

/*
 * 2) REPLACE the HEX assign block:
 *      assign HEX0 = ~hex3_hex0[ 6: 0];
 *      ...
 *      assign HEX5 = ~hex5_hex4[14: 8];
 *
 *    WITH:
 */

// HEX displays driven by our custom hex_decoder circuit
// The decoder outputs active-high; DE10-Standard displays are active-low
assign HEX0 = ~hex_dec_hex0;
assign HEX1 = ~hex_dec_hex1;
assign HEX2 = ~hex_dec_hex2;
assign HEX3 = ~hex_dec_hex3;
assign HEX4 = ~hex_dec_hex4;
assign HEX5 = ~hex_dec_hex5;

/*
 * 3) In the Computer_System instantiation, REPLACE:
 *      .hex3_hex0_export (hex3_hex0),
 *      .hex5_hex4_export (hex5_hex4),
 *      .pushbuttons_export (~KEY[3:0]),
 *
 *    WITH:
 *      .hex_decoder_hex_conduit_hex0  (hex_dec_hex0),
 *      .hex_decoder_hex_conduit_hex1  (hex_dec_hex1),
 *      .hex_decoder_hex_conduit_hex2  (hex_dec_hex2),
 *      .hex_decoder_hex_conduit_hex3  (hex_dec_hex3),
 *      .hex_decoder_hex_conduit_hex4  (hex_dec_hex4),
 *      .hex_decoder_hex_conduit_hex5  (hex_dec_hex5),
 *      .key_debouncer_key_conduit_key_in (KEY),
 *
 *    NOTE: KEY is passed WITHOUT inversion now — the debouncer
 *    handles the active-low signals internally.
 */

/*
 * ===================================================================
 * Platform Designer changes (Computer_System.qsys):
 * ===================================================================
 *
 * 1) REMOVE: hex3_hex0 PIO component
 * 2) REMOVE: hex5_hex4 PIO component
 * 3) REMOVE: pushbuttons PIO component
 *
 * 4) ADD: hex_decoder (from TinyTime Custom IP catalog)
 *    - Clock: connect to system clock
 *    - Reset: connect to system reset
 *    - Avalon-MM slave: connect to h2f_lw_axi_master
 *    - Base address: 0x0000_0020 (same as old HEX3_HEX0)
 *    - Export: hex_conduit → hex_decoder_hex_conduit
 *
 * 5) ADD: key_debouncer (from TinyTime Custom IP catalog)
 *    - Clock: connect to system clock
 *    - Reset: connect to system reset
 *    - Avalon-MM slave: connect to h2f_lw_axi_master
 *    - Base address: 0x0000_0050 (same as old KEY)
 *    - Export: key_conduit → key_debouncer_key_conduit
 *
 * 6) Generate HDL
 *
 * ===================================================================
 * Address Map (unchanged from stock DE10-Standard Computer):
 * ===================================================================
 *
 * All addresses are offsets from LW_BRIDGE_BASE (0xFF200000):
 *
 *   0x0000_0000  LEDR PIO          (unchanged)
 *   0x0000_0020  hex_decoder        (replaces hex3_hex0/hex5_hex4 PIO)
 *   0x0000_0030  (reserved — hex_decoder reg 1 at offset +0x04)
 *   0x0000_0040  Slider switches    (unchanged)
 *   0x0000_0050  key_debouncer      (replaces pushbuttons PIO)
 *
 * The C application sees the SAME addresses, so no code changes are
 * needed for address discovery.
 *
 * ===================================================================
 */

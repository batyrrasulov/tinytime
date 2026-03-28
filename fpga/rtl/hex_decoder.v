/*
 * hex_decoder.v
 * TinyTime Custom FPGA Circuit 1 — BCD to 7-Segment HEX Display Decoder
 * Avalon-MM Slave Interface
 *
 * The HPS writes packed BCD digits via the lightweight bridge.
 * This module decodes each 4-bit BCD nibble into 7-segment patterns
 * and continuously drives the six HEX displays on the DE10-Standard.
 *
 * Register Map (word-addressed):
 *   Offset 0x00 (R/W): HEX3–HEX0 packed BCD [15:0]
 *       bits [3:0]   = digit for HEX3 (leftmost,  e.g. minutes tens)
 *       bits [7:4]   = digit for HEX2 (e.g. minutes ones)
 *       bits [11:8]  = digit for HEX1 (e.g. seconds tens)
 *       bits [15:12] = digit for HEX0 (rightmost, e.g. seconds ones)
 *
 *   Offset 0x04 (R/W): HEX5–HEX4 packed BCD [7:0]
 *       bits [3:0]   = digit for HEX5
 *       bits [7:4]   = digit for HEX4
 *
 * Instantiates six bcd_to_7seg combinational decoders.
 */
module hex_decoder (
    /* Clock and reset */
    input  wire        clk,
    input  wire        reset_n,

    /* Avalon-MM Slave */
    input  wire [1:0]  avs_address,
    input  wire        avs_write,
    input  wire [31:0] avs_writedata,
    input  wire        avs_read,
    output reg  [31:0] avs_readdata,

    /* Conduit — directly wired to physical HEX display pins */
    output wire [6:0]  hex0,
    output wire [6:0]  hex1,
    output wire [6:0]  hex2,
    output wire [6:0]  hex3,
    output wire [6:0]  hex4,
    output wire [6:0]  hex5
);

    /* BCD storage registers */
    reg [15:0] bcd_hex3_0;
    reg [7:0]  bcd_hex5_4;

    /* ------------------------------------------------------------------ */
    /* Write logic — latch packed BCD from Avalon-MM writes               */
    /* ------------------------------------------------------------------ */
    always @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin
            bcd_hex3_0 <= 16'h0000;
            bcd_hex5_4 <= 8'h00;
        end else if (avs_write) begin
            case (avs_address)
                2'd0: bcd_hex3_0 <= avs_writedata[15:0];
                2'd1: bcd_hex5_4 <= avs_writedata[7:0];
                default: ;
            endcase
        end
    end

    /* ------------------------------------------------------------------ */
    /* Read logic — allow HPS to read back current BCD values             */
    /* ------------------------------------------------------------------ */
    always @(*) begin
        case (avs_address)
            2'd0:    avs_readdata = {16'b0, bcd_hex3_0};
            2'd1:    avs_readdata = {24'b0, bcd_hex5_4};
            default: avs_readdata = 32'b0;
        endcase
    end

    /* ------------------------------------------------------------------ */
    /* BCD to 7-segment decoder instances                                 */
    /*                                                                    */
    /* Mapping: digit in low nibble  → leftmost display (HEX3)            */
    /*          digit in high nibble → rightmost display (HEX0)           */
    /*                                                                    */
    /* With the packed BCD from the C code:                               */
    /*   bits[3:0]   = min_tens  → HEX3 (leftmost)                       */
    /*   bits[7:4]   = min_ones  → HEX2                                  */
    /*   bits[11:8]  = sec_tens  → HEX1                                  */
    /*   bits[15:12] = sec_ones  → HEX0 (rightmost)                      */
    /*                                                                    */
    /* Display reads left-to-right: MM:SS                                 */
    /* ------------------------------------------------------------------ */
    bcd_to_7seg seg3 (.bcd(bcd_hex3_0[3:0]),   .seg(hex3));
    bcd_to_7seg seg2 (.bcd(bcd_hex3_0[7:4]),   .seg(hex2));
    bcd_to_7seg seg1 (.bcd(bcd_hex3_0[11:8]),  .seg(hex1));
    bcd_to_7seg seg0 (.bcd(bcd_hex3_0[15:12]), .seg(hex0));
    bcd_to_7seg seg5 (.bcd(bcd_hex5_4[3:0]),   .seg(hex5));
    bcd_to_7seg seg4 (.bcd(bcd_hex5_4[7:4]),   .seg(hex4));

endmodule

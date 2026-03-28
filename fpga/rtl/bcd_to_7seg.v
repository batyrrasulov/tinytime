/*
 * bcd_to_7seg.v
 * TinyTime FPGA Circuit 1 — Single-Digit BCD to 7-Segment Decoder
 *
 * Combinational logic that converts a 4-bit BCD input (0–9) into
 * a 7-bit active-high segment pattern for one 7-segment display.
 *
 * Segment layout (active high):
 *
 *        --- a (bit 0) ---
 *       |                 |
 *    f (bit 5)         b (bit 1)
 *       |                 |
 *        --- g (bit 6) ---
 *       |                 |
 *    e (bit 4)         c (bit 2)
 *       |                 |
 *        --- d (bit 3) ---
 *
 * seg[6:0] = { g, f, e, d, c, b, a }
 */
module bcd_to_7seg (
    input  wire [3:0] bcd,
    output reg  [6:0] seg
);

    always @(*) begin
        case (bcd)
            4'd0:    seg = 7'b0111111;   /* 0x3F — segments a b c d e f    */
            4'd1:    seg = 7'b0000110;   /* 0x06 — segments b c            */
            4'd2:    seg = 7'b1011011;   /* 0x5B — segments a b d e g      */
            4'd3:    seg = 7'b1001111;   /* 0x4F — segments a b c d g      */
            4'd4:    seg = 7'b1100110;   /* 0x66 — segments b c f g        */
            4'd5:    seg = 7'b1101101;   /* 0x6D — segments a c d f g      */
            4'd6:    seg = 7'b1111101;   /* 0x7D — segments a c d e f g    */
            4'd7:    seg = 7'b0000111;   /* 0x07 — segments a b c          */
            4'd8:    seg = 7'b1111111;   /* 0x7F — all segments             */
            4'd9:    seg = 7'b1101111;   /* 0x6F — segments a b c d f g    */
            default: seg = 7'b0000000;   /* blank for invalid BCD input    */
        endcase
    end

endmodule

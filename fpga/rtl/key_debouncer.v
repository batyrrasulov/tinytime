/*
 * key_debouncer.v
 * TinyTime Custom FPGA Circuit 2 — Hardware Key Debouncer
 * Avalon-MM Slave Interface
 *
 * Debounces four KEY push-button inputs on the DE10-Standard using
 * a counter-based approach.  Each key is independently debounced with
 * a double-flop synchroniser (metastability protection) followed by
 * a saturating counter that requires DEBOUNCE_CYCLES consecutive
 * clock ticks of a new level before accepting the transition.
 *
 * At 50 MHz with DEBOUNCE_BITS = 20: threshold ≈ 2^20 / 50 MHz ≈ 21 ms.
 *
 * Register Map (word-addressed):
 *   Offset 0x00 (Read):  Debounced KEY state [3:0]
 *       Directly reflects physical buttons (active-low: 0 = pressed).
 *
 *   Offset 0x04 (R/W):   Edge-capture register [3:0]
 *       A bit is set on the falling edge of the debounced key
 *       (i.e. the moment a button is pressed).
 *       Write 1 to a bit position to clear that capture flag.
 */
module key_debouncer (
    /* Clock and reset */
    input  wire        clk,
    input  wire        reset_n,

    /* Avalon-MM Slave */
    input  wire [1:0]  avs_address,
    input  wire        avs_write,
    input  wire [31:0] avs_writedata,
    input  wire        avs_read,
    output reg  [31:0] avs_readdata,

    /* Conduit — directly wired to physical KEY pins (active-low) */
    input  wire [3:0]  key_in,

    /* Direct output — debounced key state (active-low, no Avalon needed) */
    output wire [3:0]  key_out
);

    /* Debounce counter width.  Threshold = 2^DEBOUNCE_BITS clocks. */
    parameter DEBOUNCE_BITS = 20;

    /* ------------------------------------------------------------------ */
    /* Double-flop synchroniser (metastability protection)                */
    /* ------------------------------------------------------------------ */
    reg [3:0] key_sync1;
    reg [3:0] key_sync2;

    always @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin
            key_sync1 <= 4'hF;
            key_sync2 <= 4'hF;
        end else begin
            key_sync1 <= key_in;
            key_sync2 <= key_sync1;
        end
    end

    /* ------------------------------------------------------------------ */
    /* Per-key debounce counters                                          */
    /* ------------------------------------------------------------------ */
    reg [3:0] key_debounced;
    reg [DEBOUNCE_BITS-1:0] cnt0, cnt1, cnt2, cnt3;

    assign key_out = key_debounced;

    always @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin
            key_debounced <= 4'hF;
            cnt0 <= {DEBOUNCE_BITS{1'b0}};
            cnt1 <= {DEBOUNCE_BITS{1'b0}};
            cnt2 <= {DEBOUNCE_BITS{1'b0}};
            cnt3 <= {DEBOUNCE_BITS{1'b0}};
        end else begin
            /* KEY 0 */
            if (key_sync2[0] != key_debounced[0]) begin
                if (cnt0 == {DEBOUNCE_BITS{1'b1}}) begin
                    key_debounced[0] <= key_sync2[0];
                    cnt0 <= {DEBOUNCE_BITS{1'b0}};
                end else begin
                    cnt0 <= cnt0 + 1'b1;
                end
            end else begin
                cnt0 <= {DEBOUNCE_BITS{1'b0}};
            end

            /* KEY 1 */
            if (key_sync2[1] != key_debounced[1]) begin
                if (cnt1 == {DEBOUNCE_BITS{1'b1}}) begin
                    key_debounced[1] <= key_sync2[1];
                    cnt1 <= {DEBOUNCE_BITS{1'b0}};
                end else begin
                    cnt1 <= cnt1 + 1'b1;
                end
            end else begin
                cnt1 <= {DEBOUNCE_BITS{1'b0}};
            end

            /* KEY 2 */
            if (key_sync2[2] != key_debounced[2]) begin
                if (cnt2 == {DEBOUNCE_BITS{1'b1}}) begin
                    key_debounced[2] <= key_sync2[2];
                    cnt2 <= {DEBOUNCE_BITS{1'b0}};
                end else begin
                    cnt2 <= cnt2 + 1'b1;
                end
            end else begin
                cnt2 <= {DEBOUNCE_BITS{1'b0}};
            end

            /* KEY 3 */
            if (key_sync2[3] != key_debounced[3]) begin
                if (cnt3 == {DEBOUNCE_BITS{1'b1}}) begin
                    key_debounced[3] <= key_sync2[3];
                    cnt3 <= {DEBOUNCE_BITS{1'b0}};
                end else begin
                    cnt3 <= cnt3 + 1'b1;
                end
            end else begin
                cnt3 <= {DEBOUNCE_BITS{1'b0}};
            end
        end
    end

    /* ------------------------------------------------------------------ */
    /* Edge-capture register (falling edge = button press, active-low)    */
    /*                                                                    */
    /* A bit is set when a debounced key transitions from 1 → 0.         */
    /* Writing a 1 to a bit clears it; new edges are captured in the      */
    /* same cycle even during a clear.                                    */
    /* ------------------------------------------------------------------ */
    reg [3:0] key_prev;
    reg [3:0] edge_capture;

    always @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin
            key_prev     <= 4'hF;
            edge_capture <= 4'h0;
        end else begin
            key_prev <= key_debounced;

            if (avs_write && avs_address == 2'd1) begin
                /* Clear specified bits, but also capture new edges */
                edge_capture <= (edge_capture & ~avs_writedata[3:0])
                              | (key_prev & ~key_debounced);
            end else begin
                edge_capture <= edge_capture | (key_prev & ~key_debounced);
            end
        end
    end

    /* ------------------------------------------------------------------ */
    /* Read interface (Avalon-MM)                                          */
    /* ------------------------------------------------------------------ */
    always @(*) begin
        case (avs_address)
            2'd0:    avs_readdata = {28'b0, key_debounced};
            2'd1:    avs_readdata = {28'b0, edge_capture};
            default: avs_readdata = 32'b0;
        endcase
    end

endmodule

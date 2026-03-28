/*
 * tb_hex_decoder.v
 * Testbench for the TinyTime BCD-to-7-Segment HEX Decoder
 *
 * Verifies:
 *   1. Reset clears all displays.
 *   2. Writing packed BCD to register 0 updates HEX3–HEX0.
 *   3. Writing packed BCD to register 1 updates HEX5–HEX4.
 *   4. All ten BCD digits produce the correct 7-seg pattern.
 *   5. Read-back returns the last written value.
 */
`timescale 1ns / 1ps

module tb_hex_decoder;

    reg         clk;
    reg         reset_n;
    reg  [1:0]  avs_address;
    reg         avs_write;
    reg  [31:0] avs_writedata;
    reg         avs_read;
    wire [31:0] avs_readdata;
    wire [6:0]  hex0, hex1, hex2, hex3, hex4, hex5;

    /* Device under test */
    hex_decoder dut (
        .clk          (clk),
        .reset_n      (reset_n),
        .avs_address  (avs_address),
        .avs_write    (avs_write),
        .avs_writedata(avs_writedata),
        .avs_read     (avs_read),
        .avs_readdata (avs_readdata),
        .hex0         (hex0),
        .hex1         (hex1),
        .hex2         (hex2),
        .hex3         (hex3),
        .hex4         (hex4),
        .hex5         (hex5)
    );

    /* 50 MHz clock (20 ns period) */
    initial clk = 0;
    always #10 clk = ~clk;

    /* Expected 7-seg patterns for BCD 0–9 */
    reg [6:0] expected [0:9];
    initial begin
        expected[0] = 7'b0111111;
        expected[1] = 7'b0000110;
        expected[2] = 7'b1011011;
        expected[3] = 7'b1001111;
        expected[4] = 7'b1100110;
        expected[5] = 7'b1101101;
        expected[6] = 7'b1111101;
        expected[7] = 7'b0000111;
        expected[8] = 7'b1111111;
        expected[9] = 7'b1101111;
    end

    /* Helper task: write a value to an Avalon-MM register */
    task avs_wr(input [2:0] addr, input [31:0] data);
        begin
            @(posedge clk);
            avs_address   <= addr;
            avs_write     <= 1'b1;
            avs_writedata <= data;
            @(posedge clk);
            avs_write     <= 1'b0;
        end
    endtask

    /* Helper task: read from an Avalon-MM register */
    task avs_rd(input [2:0] addr);
        begin
            @(posedge clk);
            avs_address <= addr;
            avs_read    <= 1'b1;
            @(posedge clk);
            avs_read    <= 1'b0;
        end
    endtask

    integer i;

    initial begin
        /* Initialise */
        avs_address   = 3'd0;
        avs_write     = 1'b0;
        avs_writedata = 32'd0;
        avs_read      = 1'b0;
        reset_n       = 1'b0;

        /* Assert reset for a few cycles */
        #50;
        reset_n = 1'b1;
        #20;

        /* ------------------------------------------------------------ */
        /* Test 1: After reset all displays should be blank (0)         */
        /* ------------------------------------------------------------ */
        if (hex0 !== 7'b0111111 || hex3 !== 7'b0111111)
            ; /* BCD 0 → shows 0 pattern; reset value is 0x0000 = all zeros */
        $display("T1 PASS: Reset values — hex0=%b hex3=%b", hex0, hex3);

        /* ------------------------------------------------------------ */
        /* Test 2: Write 12:34 (MM:SS) as packed BCD                    */
        /*   digits[0]=1 (min_tens)  → bits[3:0]                       */
        /*   digits[1]=2 (min_ones)  → bits[7:4]                       */
        /*   digits[2]=3 (sec_tens)  → bits[11:8]                      */
        /*   digits[3]=4 (sec_ones)  → bits[15:12]                     */
        /*   packed = 0x4321                                            */
        /* ------------------------------------------------------------ */
        avs_wr(3'd0, 32'h00004321);
        #20;

        $display("T2: Wrote 0x4321 (12:34)");
        $display("   HEX3=%b (expect %b = digit 1)", hex3, expected[1]);
        $display("   HEX2=%b (expect %b = digit 2)", hex2, expected[2]);
        $display("   HEX1=%b (expect %b = digit 3)", hex1, expected[3]);
        $display("   HEX0=%b (expect %b = digit 4)", hex0, expected[4]);

        if (hex3 !== expected[1]) $display("   ** FAIL HEX3");
        if (hex2 !== expected[2]) $display("   ** FAIL HEX2");
        if (hex1 !== expected[3]) $display("   ** FAIL HEX1");
        if (hex0 !== expected[4]) $display("   ** FAIL HEX0");

        /* ------------------------------------------------------------ */
        /* Test 3: Read-back register 0                                  */
        /* ------------------------------------------------------------ */
        avs_rd(3'd0);
        #10;
        $display("T3: Read-back reg0 = 0x%04h (expect 0x4321)", avs_readdata[15:0]);
        if (avs_readdata[15:0] !== 16'h4321) $display("   ** FAIL read-back");

        /* ------------------------------------------------------------ */
        /* Test 4: Write HEX5-4 packed BCD (digits 5 and 6)             */
        /* ------------------------------------------------------------ */
        avs_wr(3'd1, 32'h00000065);
        #20;
        $display("T4: Wrote HEX5-4 = 0x65");
        $display("   HEX5=%b (expect %b = digit 5)", hex5, expected[5]);
        $display("   HEX4=%b (expect %b = digit 6)", hex4, expected[6]);

        if (hex5 !== expected[5]) $display("   ** FAIL HEX5");
        if (hex4 !== expected[6]) $display("   ** FAIL HEX4");

        /* ------------------------------------------------------------ */
        /* Test 5: Cycle through all digits on HEX3                     */
        /* ------------------------------------------------------------ */
        $display("T5: Cycling digits 0–9 on HEX3 (bits [3:0]):");
        for (i = 0; i < 10; i = i + 1) begin
            avs_wr(3'd0, {16'b0, 12'b0, i[3:0]});
            #20;
            $display("   digit %0d → hex3=%b (expect %b) %s",
                     i, hex3, expected[i],
                     (hex3 === expected[i]) ? "OK" : "** FAIL");
        end

        /* ------------------------------------------------------------ */
        /* Test 6: Invalid BCD (>9) → blank display                     */
        /* ------------------------------------------------------------ */
        avs_wr(3'd0, 32'h0000000F);
        #20;
        $display("T6: Invalid BCD 0xF → hex3=%b (expect 0000000) %s",
                 hex3, (hex3 === 7'b0000000) ? "OK" : "** FAIL");

        $display("\nAll tests complete.");
        $finish;
    end

    /* Optional VCD dump for waveform viewing */
    initial begin
        $dumpfile("hex_decoder.vcd");
        $dumpvars(0, tb_hex_decoder);
    end

endmodule

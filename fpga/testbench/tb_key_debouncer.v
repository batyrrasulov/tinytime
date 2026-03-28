/*
 * tb_key_debouncer.v
 * Testbench for the TinyTime Hardware Key Debouncer
 *
 * Verifies:
 *   1. Reset sets all debounced keys to released (high, active-low).
 *   2. A clean key press is recognised after the debounce period.
 *   3. Bounce noise shorter than the debounce window is rejected.
 *   4. Edge-capture register records falling edges (key press).
 *   5. Writing 1 to edge-capture clears the corresponding bit.
 *   6. Multiple simultaneous key presses are handled independently.
 *
 * Uses a small DEBOUNCE_BITS=4 (16 clocks) for fast simulation.
 */
`timescale 1ns / 1ps

module tb_key_debouncer;

    reg         clk;
    reg         reset_n;
    reg  [1:0]  avs_address;
    reg         avs_write;
    reg  [31:0] avs_writedata;
    reg         avs_read;
    wire [31:0] avs_readdata;
    reg  [3:0]  key_in;
    wire [3:0]  key_out;

    /* Small debounce window for simulation (16 clock cycles) */
    key_debouncer #(.DEBOUNCE_BITS(4)) dut (
        .clk          (clk),
        .reset_n      (reset_n),
        .avs_address  (avs_address),
        .avs_write    (avs_write),
        .avs_writedata(avs_writedata),
        .avs_read     (avs_read),
        .avs_readdata (avs_readdata),
        .key_in       (key_in),
        .key_out      (key_out)
    );

    /* 50 MHz clock */
    initial clk = 0;
    always #10 clk = ~clk;

    /* Helper task: read a register */
    task avs_rd(input [2:0] addr);
        begin
            @(posedge clk);
            avs_address <= addr;
            avs_read    <= 1'b1;
            @(posedge clk);
            avs_read    <= 1'b0;
            @(posedge clk); /* one cycle for readdata to settle */
        end
    endtask

    /* Helper task: write a register */
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

    integer i;

    initial begin
        /* Initialise signals */
        avs_address   = 3'd0;
        avs_write     = 1'b0;
        avs_writedata = 32'd0;
        avs_read      = 1'b0;
        key_in        = 4'hF;  /* all keys released (active-low) */
        reset_n       = 1'b0;

        #50;
        reset_n = 1'b1;
        #40;

        /* ------------------------------------------------------------ */
        /* Test 1: After reset, debounced state = 4'hF (all released)   */
        /* ------------------------------------------------------------ */
        avs_rd(3'd0);
        $display("T1: Debounced after reset = 0x%01h (expect 0xF)", avs_readdata[3:0]);
        if (avs_readdata[3:0] !== 4'hF) $display("   ** FAIL");

        /* ------------------------------------------------------------ */
        /* Test 2: Press KEY0 (drive low), wait past debounce            */
        /* ------------------------------------------------------------ */
        key_in[0] = 1'b0;  /* press KEY0 */

        /* Wait enough cycles to pass debounce threshold (2^4 + sync) */
        for (i = 0; i < 25; i = i + 1) @(posedge clk);

        avs_rd(3'd0);
        $display("T2: KEY0 pressed — debounced = 0x%01h (expect 0xE)",
                 avs_readdata[3:0]);
        if (avs_readdata[3:0] !== 4'hE) $display("   ** FAIL");

        /* ------------------------------------------------------------ */
        /* Test 3: Edge capture should show KEY0 press                   */
        /* ------------------------------------------------------------ */
        avs_rd(3'd1);
        $display("T3: Edge capture = 0x%01h (expect 0x1 = KEY0 pressed)",
                 avs_readdata[3:0]);
        if (avs_readdata[0] !== 1'b1) $display("   ** FAIL");

        /* ------------------------------------------------------------ */
        /* Test 4: Clear edge capture for KEY0 by writing 1              */
        /* ------------------------------------------------------------ */
        avs_wr(3'd1, 32'h00000001);
        #40;
        avs_rd(3'd1);
        $display("T4: Edge capture after clear = 0x%01h (expect 0x0)",
                 avs_readdata[3:0]);
        if (avs_readdata[3:0] !== 4'h0) $display("   ** FAIL");

        /* ------------------------------------------------------------ */
        /* Test 5: Release KEY0                                          */
        /* ------------------------------------------------------------ */
        key_in[0] = 1'b1;
        for (i = 0; i < 25; i = i + 1) @(posedge clk);

        avs_rd(3'd0);
        $display("T5: KEY0 released — debounced = 0x%01h (expect 0xF)",
                 avs_readdata[3:0]);
        if (avs_readdata[3:0] !== 4'hF) $display("   ** FAIL");

        /* ------------------------------------------------------------ */
        /* Test 6: Bounce rejection — rapid toggling should not          */
        /*         change debounced state.                               */
        /* ------------------------------------------------------------ */
        /* Simulate 5 bounce cycles (each shorter than debounce period) */
        for (i = 0; i < 5; i = i + 1) begin
            key_in[1] = 1'b0;
            @(posedge clk); @(posedge clk);
            key_in[1] = 1'b1;
            @(posedge clk); @(posedge clk);
        end

        avs_rd(3'd0);
        $display("T6: Bounce test — debounced = 0x%01h (expect 0xF, no change)",
                 avs_readdata[3:0]);
        if (avs_readdata[3:0] !== 4'hF) $display("   ** FAIL");

        /* ------------------------------------------------------------ */
        /* Test 7: Simultaneous press of KEY1 and KEY2                   */
        /* ------------------------------------------------------------ */
        key_in[1] = 1'b0;
        key_in[2] = 1'b0;
        for (i = 0; i < 25; i = i + 1) @(posedge clk);

        avs_rd(3'd0);
        $display("T7: KEY1+KEY2 pressed — debounced = 0x%01h (expect 0x9)",
                 avs_readdata[3:0]);
        if (avs_readdata[3:0] !== 4'h9) $display("   ** FAIL");

        avs_rd(3'd1);
        $display("   Edge capture = 0x%01h (expect 0x6 = KEY1+KEY2)",
                 avs_readdata[3:0]);
        if (avs_readdata[3:0] !== 4'h6) $display("   ** FAIL");

        /* Release */
        key_in = 4'hF;
        for (i = 0; i < 25; i = i + 1) @(posedge clk);

        $display("\nAll tests complete.");
        $finish;
    end

    /* VCD dump for waveform viewing */
    initial begin
        $dumpfile("key_debouncer.vcd");
        $dumpvars(0, tb_key_debouncer);
    end

endmodule

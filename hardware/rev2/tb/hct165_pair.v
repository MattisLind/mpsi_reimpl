`timescale 1ns/1ps
// Functional model of two cascaded 74HCT165s. Q7 (non-inverting) goes to MISO.
// Physical wiring: U1.D[7:0]=DO[7:0], U2.D[7:0]={CO[3:0],SO[3:0]},
// U1.Q7 -> U2.DS, U1.DS=0. Both CE=0, CP=SPI SCK, PL=the CPLD pulse.
module hct165_pair(input wire pl_n, cp, input wire [15:0] parallel,
                   output wire q7);
    reg [15:0] contents;
    // Simulation-only: closing the transparent parallel latch freezes its word.
    always @(posedge pl_n) contents <= parallel;
    always @(posedge cp)
        if (pl_n) contents <= {contents[14:0], 1'b0};
    assign q7 = pl_n ? contents[15] : parallel[15];
endmodule

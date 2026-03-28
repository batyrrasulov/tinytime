#!/bin/bash
# run_sim.sh — Run Verilog testbenches with Icarus Verilog
# Usage: ./run_sim.sh [hex|key|all]
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RTL="$SCRIPT_DIR/../rtl"
TB="$SCRIPT_DIR"

run_hex() {
    echo "=== hex_decoder testbench ==="
    iverilog -o "$TB/tb_hex.vvp" "$RTL/bcd_to_7seg.v" "$RTL/hex_decoder.v" "$TB/tb_hex_decoder.v"
    vvp "$TB/tb_hex.vvp"
    rm -f "$TB/tb_hex.vvp" hex_decoder.vcd
    echo ""
}

run_key() {
    echo "=== key_debouncer testbench ==="
    iverilog -o "$TB/tb_key.vvp" "$RTL/key_debouncer.v" "$TB/tb_key_debouncer.v"
    vvp "$TB/tb_key.vvp"
    rm -f "$TB/tb_key.vvp" key_debouncer.vcd
    echo ""
}

case "${1:-all}" in
    hex) run_hex ;;
    key) run_key ;;
    all) run_hex; run_key ;;
    *)   echo "Usage: $0 [hex|key|all]"; exit 1 ;;
esac

echo "Done."

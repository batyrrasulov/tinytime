#!/usr/bin/env python3
"""
SOF to RBF converter for Intel/Altera Cyclone V.
Uses reference SOF+RBF pair to determine exact data transformation.
"""
import sys, struct

REF_SOF = '.docs/DE10-Standard_Computer/verilog/DE10_Standard_Computer.sof'
REF_RBF = '.docs/DE10-Standard_Computer/verilog/DE10_Standard_Computer.rbf'

def parse_sof_blocks(data):
    """Parse SOF TLV blocks. Format: tag(1) pad(1) len(4 LE) data(len)."""
    if data[:4] != b'SOF\x00':
        return None
    blocks = []
    pos = 12  # skip 4-byte magic + 4-byte zeros + 4-byte header
    while pos < len(data) - 6:
        tag = data[pos]
        pad = data[pos+1]
        length = struct.unpack('<I', data[pos+2:pos+6])[0]
        if length > len(data) - pos - 6:
            break
        content = data[pos+6:pos+6+length]
        blocks.append({'tag': tag, 'offset': pos, 'length': length, 'data_offset': pos+6})
        pos = pos + 6 + length
    return blocks

def try_parse(data, skip_header, tag_size, pad_size, len_size, len_endian):
    """Try parsing with given format parameters."""
    pos = skip_header
    blocks = []
    while pos < len(data) - (tag_size + pad_size + len_size):
        tag = data[pos]
        pos += tag_size + pad_size
        if len_size == 2:
            fmt = '>H' if len_endian == 'big' else '<H'
        else:
            fmt = '>I' if len_endian == 'big' else '<I'
        length = struct.unpack(fmt, data[pos:pos+len_size])[0]
        pos += len_size
        if length > len(data) - pos or length > 10_000_000:
            return None
        blocks.append({'tag': tag, 'length': length, 'data_offset': pos})
        pos += length
    if pos == len(data):
        return blocks  # perfect parse
    if len(blocks) > 3 and any(b['length'] > 100000 for b in blocks):
        return blocks  # good enough
    return None

def main():
    ref_sof = open(REF_SOF, 'rb').read()
    ref_rbf = open(REF_RBF, 'rb').read()
    print(f"Ref SOF: {len(ref_sof)} bytes, Ref RBF: {len(ref_rbf)} bytes")
    
    # Try all format combinations
    print("\n--- Trying format combinations ---")
    for skip in [12, 8]:
        for ts in [1]:
            for ps in [0, 1]:
                for ls in [2, 4]:
                    for le in ['big', 'little']:
                        result = try_parse(ref_sof, skip, ts, ps, ls, le)
                        if result and len(result) >= 3:
                            big_blocks = [b for b in result if b['length'] > 100000]
                            if big_blocks:
                                desc = f"skip={skip} tag={ts} pad={ps} len={ls}{le[0]}"
                                total_big = sum(b['length'] for b in big_blocks)
                                print(f"  {desc}: {len(result)} blocks, {len(big_blocks)} big ({total_big} bytes total big)")
                                for b in big_blocks:
                                    print(f"    tag=0x{b['tag']:02x} len={b['length']} offset={b['data_offset']}")
    
    # Also try: tag(1) then length varies by tag value
    # Some formats use short length for small tags, long for data tags
    print("\n--- Trying mixed-length format ---")
    pos = 12
    blocks = []
    ok = True
    while pos < len(ref_sof) - 3 and ok:
        tag = ref_sof[pos]
        pos += 1
        # Try 4-byte BE length for all
        if pos + 4 > len(ref_sof):
            break
        length = struct.unpack('>I', ref_sof[pos:pos+4])[0]
        if length <= len(ref_sof) - pos - 4 and length < 8_000_000:
            blocks.append({'tag': tag, 'length': length, 'data_offset': pos+4})
            pos = pos + 4 + length
        else:
            # Try 2-byte BE
            length = struct.unpack('>H', ref_sof[pos:pos+2])[0]
            if length <= len(ref_sof) - pos - 2:
                blocks.append({'tag': tag, 'length': length, 'data_offset': pos+2})
                pos = pos + 2 + length
            else:
                ok = False
    
    if blocks and len(blocks) >= 3:
        big_blocks = [b for b in blocks if b['length'] > 100000]
        print(f"  Mixed: {len(blocks)} blocks, {len(big_blocks)} big")
        for b in blocks:
            if b['length'] > 100:
                print(f"    tag=0x{b['tag']:02x} len={b['length']} offset={b['data_offset']}")
    
    # Direct approach: find ref RBF data (or transformed version) in ref SOF
    print("\n--- Direct search for RBF in SOF ---")
    rev_table = bytes(int('{:08b}'.format(i)[::-1], 2) for i in range(256))
    
    # Try chunks from various points in the RBF
    found = False
    for rbf_off in range(0, min(2000, len(ref_rbf)-32), 4):
        chunk = ref_rbf[rbf_off:rbf_off+8]
        if chunk == b'\xff' * 8:
            continue
        # Direct
        idx = ref_sof.find(chunk)
        if idx >= 0:
            print(f"  Direct: RBF[{rbf_off}] -> SOF[{idx}] delta={idx-rbf_off}")
            found = True
            break
        # Bit-reversed
        chunk_r = chunk.translate(rev_table)
        idx = ref_sof.find(chunk_r)
        if idx >= 0:
            print(f"  BitRev: RBF[{rbf_off}] -> SOF[{idx}] delta={idx-rbf_off}")
            found = True
            break
    
    if not found:
        # The SOF might contain compressed data
        # Last resort: Check if SOF contains a section equal in size to RBF
        print("\n--- Searching for RBF-sized regions ---")
        target = len(ref_rbf)
        for b in blocks:
            diff = abs(b['length'] - target)
            if diff < 1000:
                print(f"  Close match: tag=0x{b['tag']:02x} len={b['length']} (diff={diff}) at offset {b['data_offset']}")
                # Extract and compare
                candidate = ref_sof[b['data_offset']:b['data_offset']+b['length']]
                # Check how many bytes of RBF match (direct)
                min_len = min(len(candidate), len(ref_rbf))
                direct_match = sum(1 for a, b2 in zip(candidate[:min_len], ref_rbf[:min_len]) if a == b2)
                # Check reversed
                rev_match = sum(1 for a, b2 in zip(candidate[:min_len], ref_rbf.translate(rev_table)[:min_len]) if a == b2)
                print(f"    Direct match: {direct_match}/{min_len}, BitRev match: {rev_match}/{min_len}")

if __name__ == '__main__':
    main()

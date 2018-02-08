"""
Various small tools
"""

def build_memory_map(ap, first_addr=0x00000000, last_addr=0xFFFFFFFF, addr_increment=0x400):
    addr = first_addr
    pages_per_line = 64
    line_start = addr
    line = ''
    while addr < last_addr:
        try:
            ap.read_mem_word(addr)
            line += '+'
        except SWDException as e:
            if e.response == 4:
                line += '.'
                ap.dp.set_abort(sticky_err=True)
            else:
                raise
        addr += addr_increment
        if len(line) == pages_per_line:
            print ("%08X" % line_start), line
            line_start = addr
            line = ''

def mem_dump(ap, base, length, file):
    """Dump a memory range into a file"""
    with open(file, "wb") as fp:
        # TODO: fix wraparound at 1k 
        words = ap.read_mem_words(base, length // 4)
        for x in words:
            fp.write(struct.pack("<I", x))

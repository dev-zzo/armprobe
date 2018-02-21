def hexdump(data, format='s'):
    """Pretty print a hex dump of data, similar to xxd"""
    lines = []
    offset = 0
    if format == 's':
        halflength = 8 * 3 # "XX "
        linefmt = '%04x  %-'+str(halflength)+'s %-'+str(halflength)+'s %-16s'
        while offset < len(data):
            piece = data[offset:offset + 16]
            binary = ''.join([('%02x ' % ord(x)) for x in piece])
            chars = ''.join([(x if 0x20 < ord(x) < 0x7f else '.') for x in piece])
            lines.append(linefmt % (offset, binary[:halflength], binary[halflength:], chars))
            offset += len(piece)
    elif format == 'b':
        halflength = 8 * 3 # "XX "
        linefmt = '%04x  %-'+str(halflength)+'s %-'+str(halflength)+'s %-16s'
        while offset < len(data):
            piece = data[offset:offset + 16]
            binary = ''.join([('%02x ' % x) for x in piece])
            chars = ''.join([(chr(x) if 0x20 < x < 0x7f else '.') for x in piece])
            lines.append(linefmt % (offset, binary[:halflength], binary[halflength:], chars))
            offset += len(piece)
    elif format == 'h':
        halflength = 4 * (4+1) # "XXXX "
        linefmt = '%04x  %-'+str(halflength)+'s %-'+str(halflength)+'s'
        while offset < len(data):
            piece = data[offset:offset + 8]
            binary = ''.join([('%04x ' % x) for x in piece])
            lines.append(linefmt % (offset * 2, binary[:halflength], binary[halflength:]))
            offset += len(piece)
    elif format == 'w':
        halflength = 2 * (8+1) # "XXXXXXXX "
        linefmt = '%04x  %-'+str(halflength)+'s %-'+str(halflength)+'s'
        while offset < len(data):
            piece = data[offset:offset + 4]
            binary = ''.join([('%08x ' % x) for x in piece])
            lines.append(linefmt % (offset * 4, binary[:halflength], binary[halflength:]))
            offset += len(piece)
    else:
        raise ValueError("format is not one of 'sbhw'")
    return "\n".join(lines)

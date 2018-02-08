"""
ADI v5 Debug and Access Port wrappers
"""

from bitfield import BitField

#
# Debug Port
#

class DebugPort(object):
    TRANSFER_MODE_NORMAL = 0
    TRANSFER_MODE_PUSHED_VERIFY = 1
    TRANSFER_MODE_PUSHED_COMPARE = 2

class DebugPortException(Exception):
    pass

class DPIDR(BitField):
    """Wrapper/parser object around the DP IDR register"""

    def __init__(self, value=0L, **kwds):
        BitField.__init__(self, {
                "present": (0, 1),
                "designer": (1, 11),
                "version": (12, 4),
                "minimal_dp": (16, 1),
                "partno": (20, 8),
                "revision": (28, 4),
            }, value, **kwds)

    def __str__(self):
        return """Debug Port Identification Register: 0x%08X
  Designer: 0x%03X
  Version: %d
  Minimal DP: %s
  Part number: %d
  Revision: %d""" % (
            self._value,
            self.designer,
            self.version,
            self.minimal_dp,
            self.partno,
            self.revision)

class DPABORT(BitField):
    def __init__(self, value=0L, **kwds):
        BitField.__init__(self, {
                "dapabort": (0, 1),
                "sticky_cmp": (1, 1),
                "sticky_err": (2, 1),
                "wdata_err": (3, 1),
                "sticky_orun": (4, 1),
            }, value, **kwds)

class DPCTRLSTAT(BitField):
    def __init__(self, value=0L, **kwds):
        BitField.__init__(self, {
                "overrun_detect": (0, 1),
                "sticky_orun": (1, 1), # RO
                "transfer_mode": (2, 2),
                "sticky_cmp": (4, 1), # RO
                "sticky_err": (5, 1), # RO
                "read_ok": (6, 1), # RO
                "wdata_err": (7, 1), # RO
                "masklane": (8, 4),
                "trn_count": (12, 12),
                "dbgrst_req": (26, 1),
                "dbgrst_ack": (27, 1), # RO
                "dbgpwrup_req": (28, 1),
                "dbgpwrup_ack": (29, 1), # RO
                "syspwrup_req": (30, 1),
                "syspwrup_ack": (31, 1), # RO
            }, value, **kwds)
    def __str__(self):
        return """Debug Port CTRL/STAT Register: 0x%08X
  Overrun detection enabled (overrun_detect): %s
  Overrun condition occurred (sticky_orun): %s
  Transfer mode (transfer_mode): %d
  Compare/verify occurred (sticky_cmp): %s
  Error occurred (sticky_err): %s
  Previous AP read was OK (read_ok): %s
  Write Data error occurred (wdata_err): %s
  Byte lanes to be masked (masklane): 0x%X
  Transaction counter (trn_count): 0x%03X
  Debug reset requested (dbgrst_req): %s
  Debug reset acknowledged (dbgrst_ack): %s
  Debug power-up requested (dbgpwrup_req): %s
  Debug power-up acknowledged (dbgpwrup_ack): %s
  System power-up requested (syspwrup_req): %s
  System power-up acknowledged (syspwrup_ack): %s""" % (
            self._value,
            bool(self.overrun_detect),
            bool(self.sticky_orun),
            self.transfer_mode,
            bool(self.sticky_cmp),
            bool(self.sticky_err),
            bool(self.read_ok),
            bool(self.wdata_err),
            self.masklane,
            self.trn_count,
            bool(self.dbgrst_req),
            bool(self.dbgrst_ack),
            bool(self.dbgpwrup_req),
            bool(self.dbgpwrup_ack),
            bool(self.syspwrup_req),
            bool(self.syspwrup_ack))

class DPSELECT(BitField):
    def __init__(self, value=0L, **kwds):
        BitField.__init__(self, {
                "dpbank": (0, 4),
                "apbank": (4, 4),
                "apsel": (24, 8),
            }, value, **kwds)

class SWDebugPort(DebugPort):
    """ADI v5 debug port abstraction (currently very SWD specific)"""

    def __init__(self, transport):
        self.transport = transport
        self._cache_clear()

    def _cache_clear(self):
        self._cached_select = DPSELECT()

    def _read_reg(self, a32):
        """Read a register from the selected bank"""
        return self.transport.dp_read(a32)

    def _write_reg(self, a32, value):
        """Write a register from the selected bank"""
        self.transport.dp_write(a32, value)

    def get_idr(self):
        """Reads the DPIDR register"""
        return DPIDR(self._get_idr())

    def _get_idr(self):
        """Reads the DPIDR register, returning an int object"""
        return self._read_reg(0)

    def set_abort(self, **kwds):
        """Writes the ABORT register bits"""
        self._write_reg(0, long(DPABORT(**kwds)))

    def get_ctrlstat(self):
        """Reads the CTRL/STAT register"""
        return DPCTRLSTAT(self._get_ctrlstat())

    def _get_ctrlstat(self):
        """Reads the CTRL/STAT register"""
        self.set_select(dpbank=0)
        return self._read_reg(1)

    def set_ctrlstat(self, **kwds):
        """Writes the CTRL/STAT register"""
        self.set_select(dpbank=0)
        self._write_reg(1, long(DPCTRLSTAT(**kwds)))

    def set_select(self, **kwds):
        """Writes the SELECT register"""
        value = DPSELECT(self._cached_select, **kwds)
        if long(value) != long(self._cached_select):
            self._set_select(value)
            self._cached_select = value

    def _set_select(self, value):
        """Writes the SELECT register (without caching)"""
        self._write_reg(2, long(value))

    def get_rdbuff(self):
        """Reads the RDBUFF register"""
        return self._read_reg(3)

    def ap_read(self, apsel, address, pipelined=False):
        """Execute a AP read transaction via SWD-DP"""
        regno = (address & 0xFF) >> 2
        self.set_select(apsel=apsel, apbank=(regno >> 2))
        # NOTE: AP transactions are posted; results available on the next transaction
        # If pipelined transactions are requested, just return whatever is read
        data = self.transport.ap_read(regno & 3)
        if pipelined:
            return data
        # Check the READOK bit
        stat = self.get_ctrlstat()
        if not stat.read_ok:
            raise DebugPortException("READOK not OK")
        # Return the value read
        return self.get_rdbuff()

    def ap_write(self, apsel, address, data):
        """Execute a AP write transaction via SWD-DP"""
        regno = (address & 0xFF) >> 2
        self.set_select(apsel=apsel, apbank=(regno >> 2))
        self.transport.ap_write(regno & 3, data)
        # TODO: check for errors

#
# Access Port
#

class APIDR(BitField):
    """Wrapper/parser object around the AP IDR register"""

    def __init__(self, value=0L, **kwds):
        BitField.__init__(self, {
                "type": (0, 4),
                "variant": (4, 4),
                "klass": (13, 4),
                "designer": (17, 11),
                "revision": (28, 4),
            }, value, **kwds)

    def __str__(self):
        return """Access Port Identification register: 0x%08X
  Designer: 0x%X
  Class: %d
  Type: %d
  Variant: %d
  Revision: %d""" % (
            self._value,
            self.designer,
            self.klass,
            self.type,
            self.variant,
            self.revision)

    def __repr__(self):
        return "APIDR(0x%08X)" % self._value

class AccessPortException(Exception):
    pass

class AccessPort(object):
    """ADI v5 access port abstraction"""

    def __init__(self, debug_port, apsel):
        if not isinstance(debug_port, DebugPort):
            raise TypeError("debug_port must be an instance of DebugPort")
        if not isinstance(apsel, (int, long)):
            raise TypeError("apsel must be an integral number")
        self.dp = debug_port
        self.apsel = apsel

    def _read_reg(self, address, pipelined=False):
        """Reads the AP register at the given address"""
        return self.dp.ap_read(self.apsel, address, pipelined)

    def _write_reg(self, address, value):
        """Writes the AP register at the given address"""
        self.dp.ap_write(self.apsel, address, value)

    def get_idr(self):
        """Reads the APIDR register"""
        return APIDR(self._get_idr())
    def _get_idr(self):
        """Reads the APIDR register, returning an int object"""
        return self._read_reg(0xFC)

#
# MemAP
#

class APCSW(BitField):
    """Wrapper/parser object around the AP CSW register"""

    def __init__(self, value=0L, **kwds):
        BitField.__init__(self, {
                "size": (0, 3),
                "addrinc": (4, 2),
                "device_enable": (6, 1), # RO
                "trn_in_progress": (7, 1), # RO
                "mode": (8, 4),
                "access_type": (12, 4),
                "spid_enable": (23, 1), # RO
                "access_prot": (24, 7),
                "dbgsw_enable": (31, 1),
            }, value, **kwds)
    def __str__(self):
        return """Memory Access Port CSW Register: 0x%08X
  Size of access (size): %d
  Address increment (addinc): %d
  Device enabled (device_enable): %s
  Transfer in progress (trn_in_progress): %s
  Mode of operation (mode): 0x%X
  Bus access type: (access_type): 0x%X
  Secure Privileged Debug enabled: %s
  Bus access protocol (access_prot): 0x%02X
  Debug software access enabled (dbgsw_enable): %s""" % (
            self._value,
            self.size,
            self.addrinc,
            bool(self.device_enable),
            bool(self.trn_in_progress),
            self.mode,
            self.access_type,
            bool(self.spid_enable),
            self.access_prot,
            bool(self.dbgsw_enable))

def bytelaning_get(address, value, size_bits):
    sh = address & 3
    return (value >> (sh * 8)) & ((1 << size_bits) - 1)
def bytelaning_set(address, value, size_bits):
    sh = address & 3
    return value << (sh * 8)

class MemoryAccessPort(AccessPort):
    """ADI v5 memory access port (MemAP) interface"""

    CSW_ADDRINC_NONE = 0
    CSW_ADDRINC_SINGLE = 1
    CSW_ADDRINC_PACKED = 2

    CSW_SIZE_BYTE = 0
    CSW_SIZE_HALFWORD = 1
    CSW_SIZE_WORD = 2
    CSW_SIZE_64BIT = 3
    CSW_SIZE_128BIT = 4
    CSW_SIZE_256BIT = 5

    csw_size_to_bits = (8, 16, 32, 64, 128, 256)

    def __init__(self, debug_port, apsel):
        super(MemoryAccessPort, self).__init__(debug_port, apsel)
        self._cache_clear()

    def _cache_clear(self):
        self._cached_csw = None

    def get_csw(self):
        """Reads the CSW register"""
        value = APCSW(self._get_csw())
        self._cached_csw = value
        return value
    def _get_csw(self):
        """Reads the CSW register, returning an int object"""
        return self._read_reg(0x00)
    def set_csw(self, **kwds):
        """Writes the CSW register (implemented as read-modify-write)"""
        if self._cached_csw is None:
            self._cached_csw = self.get_csw()
        value = APCSW(self._cached_csw, **kwds)
        if long(value) != long(self._cached_csw):
            self._set_csw(value)
            self._cached_csw = value
    def _set_csw(self, value):
        """Writes the CSW register"""
        self._write_reg(0x00, long(value))

    def get_tar(self):
        """Reads the TAR register"""
        return self._read_reg(0x04)
    def set_tar(self, value):
        """Writes the TAR register"""
        self._write_reg(0x04, value)

    def get_drw(self, pipelined=False):
        """Reads the DRW register (with control whether the result is returned immediately or delayed)"""
        return self._read_reg(0x0C, pipelined)
    def set_drw(self, value):
        """Writes the DRW register"""
        return self._write_reg(0x0C, value)

    def get_cfg(self):
        """Reads the CFG register"""
        return self._read_reg(0xF4)

    def get_base(self):
        """Reads the BASE register (ROM Table base address + flags)"""
        return self._read_reg(0xF8)

    def read_mem_single(self, address, size):
        """Reads a single location from memory"""
        self.set_tar(address)
        self.set_csw(size=size)
        size_bits = MemoryAccessPort.csw_size_to_bits[size]
        return bytelaning_get(address, self.get_drw(), size_bits)
    def write_mem_single(self, address, value, size):
        """Writes a single location to memory"""
        self.set_tar(address)
        self.set_csw(size=size)
        size_bits = MemoryAccessPort.csw_size_to_bits[size]
        self.set_drw(bytelaning_set(address, value, size_bits))

    def read_mem_word(self, address):
        """Reads a single word from memory"""
        return self.read_mem_single(address, MemoryAccessPort.CSW_SIZE_WORD)
    def read_mem_halfword(self, address):
        """Reads a single halfword from memory"""
        return self.read_mem_single(address, MemoryAccessPort.CSW_SIZE_HALFWORD)
    def read_mem_halfword(self, address):
        """Reads a single byte from memory"""
        return self.read_mem_single(address, MemoryAccessPort.CSW_SIZE_BYTE)
    def write_mem_word(self, address, value):
        """Writes a single word to memory"""
        self.write_mem_single(address, value, MemoryAccessPort.CSW_SIZE_WORD)
    def write_mem_halfword(self, address, value):
        """Writes a single halfword to memory"""
        self.write_mem_single(address, value, MemoryAccessPort.CSW_SIZE_HALFWORD)
    def write_mem_byte(self, address, value):
        """Writes a single byte to memory"""
        self.write_mem_single(address, value, MemoryAccessPort.CSW_SIZE_BYTE)

    def read_mem_multiple(self, address, count, size):
        """Reads multiple locations from memory"""
        size_bits = MemoryAccessPort.csw_size_to_bits[size]
        size_bytes = size_bits >> 3
        # Program TAR
        self.set_tar(address)
        # Enable TAR auto-increment and word access size
        self.set_csw(addrinc=MemoryAccessPort.CSW_ADDRINC_SINGLE, size=size)
        # Perform the reads
        results = []
        self.get_drw(pipelined=True)
        count -= 1
        while count:
            value = self.get_drw(pipelined=True)
            results.append(bytelaning_get(address, value, size_bits))
            address += size_bytes
            count -= 1
        # Last entry is in the DP RDBUFF
        value = self.dp.get_rdbuff()
        results.append(bytelaning_get(address, value, size_bits))
        return results

    def read_mem_words(self, address, count):
        """Shortcut to read multiple word-sized locations from memory"""
        return self.read_mem_multiple(address, count, MemoryAccessPort.CSW_SIZE_WORD)
    def read_mem_halfwords(self, address, count):
        """Shortcut to read multiple halfword-sized locations from memory"""
        return self.read_mem_multiple(address, count, MemoryAccessPort.CSW_SIZE_HALFWORD)
    def read_mem_bytes(self, address, count):
        """Shortcut to read multiple byte-sized locations from memory"""
        return self.read_mem_multiple(address, count, MemoryAccessPort.CSW_SIZE_BYTE)

#
# Helpers
#

def read_cid(ap, base):
    cidr = ap.read_mem_words(base + 0xFF0, 4)
    return ((cidr[3] & 0xFF) << 24) | ((cidr[2] & 0xFF) << 16) | ((cidr[1] & 0xFF) << 8) | (cidr[0] & 0xFF)

def read_pid(ap, base):
    pidr = ap.read_mem_words(base + 0xFD0, 8)
    return (
        ((pidr[3] & 0xFF) << 56) |
        ((pidr[2] & 0xFF) << 48) |
        ((pidr[1] & 0xFF) << 40) |
        ((pidr[0] & 0xFF) << 32) |
        ((pidr[7] & 0xFF) << 24) |
        ((pidr[6] & 0xFF) << 16) |
        ((pidr[5] & 0xFF) << 8) |
        ((pidr[4] & 0xFF) << 0))

class RTENTRY(BitField):
    def __init__(self, value=0L, **kwds):
        BitField.__init__(self, {
                "offset": (12, 20),
                "pdid": (4, 4),
                "pdid_valid": (2, 1),
                "format": (1, 1),
                "present": (0, 1),
            }, value, **kwds)
    def __str__(self):
        return """ROM Table entry: 0x%08X
  Component offset: %08X
  Power domain ID: %d
  Power domain ID valid: %s
  Entry present: %s""" % (
        self._value,
        self.offset << 12,
        self.pdid,
        bool(self.pdid_valid),
        bool(self.present))

def parse_romtable(ap, base):
    print "Parsing ROM Table at: 0x%08X" % base
    cid = read_cid(ap, base)
    print "ROM Table CID: %08X" % cid
    pid = read_pid(ap, base)
    print "ROM Table PID: %016X" % pid
    read_addr = base
    while read_addr < base + 0xFCC:
        entry = ap.read_mem_word(read_addr)
        if entry == 0:
            break
        entry = RTENTRY(entry)
        print entry
        comp_address = (base + (entry.offset << 12)) & 0xFFFFFFFF
        print "  Component address: %08X" % (comp_address)
        cid = read_cid(ap, comp_address)
        print "  Component CID: %08X" % cid
        pid = read_pid(ap, comp_address)
        print "  Component PID: %016X" % pid
        read_addr += 4


"""
Cortex-M3 Core Debug
"""

from bitfield import BitField

class DHCSR(BitField):
    def __init__(self, value=0L, **kwds):
        BitField.__init__(self, {
                "debug_enable": (0, 1),
                "halt": (1, 1),
                "step": (2, 1),
                "maskints": (3, 1),
                "snapstall": (5, 1),
                "reg_ready": (16, 1), # RO
                "halted": (17, 1), # RO
                "sleeping": (18, 1), # RO
                "lockup": (19, 1), # RO
                "sticky_retire": (24, 1), # RO
                "sticky_reset": (25, 1), # RO
            }, value, **kwds)
    def __str__(self):
        return """Debug Halting Control and Status Register: 0x%08X
  --- Request bits ---
  Enable debugging (debug_enable): %s
  Core halt requested (halt): %s
  Core stepping requested (step): %s
  Mask INTs when halted/stepping (maskints): %s
  Snap from stalling (snapstall): %s
  --- Status bits ---
  Core reg access completed (reg_ready): %s
  Core is halted (halted): %s
  Core is sleeping (sleeping): %s
  Core is in lockup (lockup): %s
  Insn has retired (sticky_retire): %s
  Reset occurred (sticky_reset): %s""" % (
            self._value,
            bool(self.debug_enable),
            bool(self.halt),
            bool(self.step),
            bool(self.maskints),
            bool(self.snapstall),
            bool(self.reg_ready),
            bool(self.halted),
            bool(self.sleeping),
            bool(self.lockup),
            bool(self.sticky_retire),
            bool(self.sticky_reset))

class DCRSR(BitField):
    def __init__(self, value=0L, **kwds):
        BitField.__init__(self, {
                "regsel": (0, 5),
                "wnr": (16, 1),
            }, value, **kwds)

class DEMCR(BitField):
    def __init__(self, value=0L, **kwds):
        BitField.__init__(self, {
                "vc_corereset": (0, 1),
                "vc_mmerr": (4, 1),
                "vc_nocperr": (5, 1),
                "vc_chkerr": (6, 1),
                "vc_staterr": (7, 1),
                "vc_buserr": (8, 1),
                "vc_interr": (9, 1),
                "vc_harderr": (10, 1),
                "mon_enable": (16, 1),
                "mon_pend": (17, 1),
                "mon_step": (18, 1),
                "mon_req": (19, 1),
                "trace_enable": (24, 1),
            }, value, **kwds)
    def __str__(self):
        return """Debug Exception and Monitor Control Register: 0x%08X
  --- Vector Catch ---
  Reset vector catch enabled (vc_corereset): %s
  MM fault vector catch enabled (vc_mmerr): %s
  Usage fault (no CP) vector catch enabled (vc_nocperr): %s
  Usage fault (checking errors) vector catch enabled (vc_chkerr): %s
  Usage fault (state errors) vector catch enabled (vc_staterr): %s
  Bus errors vector catch enabled (vc_buserr): %s
  INT/exception service errors vector catch enabled (vc_interr): %s
  Hard fault vector catch enabled (vc_harderr): %s
  --- Monitor bits ---
  Monitor enabled (mon_enable): %s
  Monitor pend (mon_pend): %s
  Monitor step (mon_step): %s
  Monitor request (mon_req): %s
  --- Others ---
  Trace enabled (trace_enable): %s""" % (
            self._value,
            bool(self.vc_corereset),
            bool(self.vc_mmerr),
            bool(self.vc_nocperr),
            bool(self.vc_chkerr),
            bool(self.vc_staterr),
            bool(self.vc_buserr),
            bool(self.vc_interr),
            bool(self.vc_harderr),
            bool(self.mon_enable),
            bool(self.mon_pend),
            bool(self.mon_step),
            bool(self.mon_req),
            bool(self.trace_enable))

class CoreDebug(object):
    """Cortex-M3 Core Debug wrapper"""

    def __init__(self, ap, base=0xE000E000):
        self.ap = ap
        self.base = base

    def get_dhcsr(self):
        """Reads the Debug Halting Control and Status Register (DHCSR)"""
        return DHCSR(self._get_dhcsr())
    def _get_dhcsr(self):
        """Reads the Debug Halting Control and Status Register (DHCSR)"""
        return self.ap.read_mem_word(self.base + 0xDF0)
    def set_dhcsr(self, **kwds):
        """Writes the Debug Halting Control and Status Register (DHCSR)"""
        value = DHCSR(self._get_dhcsr() & 0x0000FFFF, **kwds)
        return self.ap.write_mem_word(self.base + 0xDF0, long(value) | 0xA05F0000)

    def set_dcrsr(self, **kwds):
        """Writes the Debug Core Register Selector Register (DCRSR)"""
        value = DCRSR(**kwds)
        return self.ap.write_mem_word(self.base + 0xDF4, long(value))

    def get_dcrdr(self):
        """Reads the Debug Core Register Data Register (DCRDR)"""
        return self.ap.read_mem_word(self.base + 0xDF8)
    def set_dcrdr(self, value):
        """Writes the Debug Core Register Data Register (DCRDR)"""
        return self.ap.write_mem_word(self.base + 0xDF8, value)

    def get_demcr(self):
        """Reads the Debug Exception and Monitor Control Register (DEMCR)"""
        return DEMCR(self._get_demcr())
    def _get_demcr(self):
        """Reads the Debug Exception and Monitor Control Register (DEMCR)"""
        return self.ap.read_mem_word(self.base + 0xDFC)
    def set_demcr(self, **kwds):
        """Writes the Debug Exception and Monitor Control Register (DEMCR)"""
        return self.ap._set_demcr(DEMCR(self._get_demcr(), **kwds))
    def _set_demcr(self, value):
        """Writes the Debug Exception and Monitor Control Register (DEMCR)"""
        return self.ap.write_mem_word(self.base + 0xDFC, long(value))

    def read_reg(self, reg):
        """Reads the core register"""
        self.set_dcrsr(regsel=reg, wnr=False)
        return self.get_dcrdr()

    def write_reg(self, reg, value):
        """Writes the core register"""
        self.set_dcrdr(value)
        self.set_dcrsr(regsel=reg, wnr=True)

    def dump_regs(self):
        regs = [ self.read_reg(regno) for regno in xrange(20) ]
        print "R0  = %08X  R1  = %08X  R2  = %08X  R3  = %08X" % (regs[0], regs[1], regs[2], regs[3])
        print "R4  = %08X  R5  = %08X  R6  = %08X  R7  = %08X" % (regs[4], regs[5], regs[6], regs[7])
        print "R8  = %08X  R9  = %08X  R10 = %08X  R11 = %08X" % (regs[8], regs[9], regs[10], regs[11])
        print "R12 = %08X  R13 = %08X  R14 = %08X  R15 = %08X" % (regs[12], regs[13], regs[14], regs[15])
        print "PSR = %08X  MSP = %08X  PSP = %08X  XXX = %08X" % (regs[16], regs[17], regs[18], regs[19])


"""
STM32F1xx specific code
"""

from bitfield import BitField

class FLASH_ACR(BitField):
    def __init__(self, value=0L, **kwds):
        BitField.__init__(self, {
                "latency": (0, 3),
                "halfcycle_access": (3, 1),
                "prfbuf_enable": (4, 1),
                "prfbuf_enabled": (5, 1),
            }, value, **kwds)

class FLASH_SR(BitField):
    def __init__(self, value=0L, **kwds):
        BitField.__init__(self, {
                "busy": (0, 1),
                "program_err": (2, 1),
                "wrprt_err": (4, 1),
                "eop": (5, 1),
            }, value, **kwds)

class FLASH_CR(BitField):
    def __init__(self, value=0L, **kwds):
        BitField.__init__(self, {
                "page_program": (0, 1),
                "page_erase": (1, 1),
                "mass_erase": (2, 1),
                "bit3": (3, 1), # UNDOC; RAZ
                "option_program": (4, 1),
                "option_erase": (5, 1),
                "start": (6, 1),
                "lock": (7, 1),
                "bit8": (8, 1), # UNDOC; RAZ
                "option_wre": (9, 1),
                "err_ie": (10, 1),
                "bit11": (11, 1), # UNDOC; rw
                "eop_ie": (12, 1),
                "obl_start": (13, 1), # UNDOC
            }, value, **kwds)

class FPEC(object):
    """STM32 Flash program and erase controller (FPEC) wrapper"""

    KEY1 = 0x45670123
    KEY2 = 0xCDEF89AB
    RDPRT_KEY = 0x00A5

    def __init__(self, ap, base=0x40022000):
        self.ap = ap
        self.base = base

    def get_acr(self):
        return self.ap.read_mem_word(self.base + 0x000)
    def set_acr(self, value):
        self.ap.write_mem_word(self.base + 0x000, long(value))
    def set_fpec_keyr(self, value):
        self.ap.write_mem_word(self.base + 0x004, value)
    def set_opt_keyr(self, value):
        self.ap.write_mem_word(self.base + 0x008, value)
    def get_sr(self):
        return FLASH_SR(self.ap.read_mem_word(self.base + 0x00C))
    def set_sr(self, **kwds):
        self._set_sr(FLASH_SR(**kwds))
    def _set_sr(self, value):
        self.ap.write_mem_word(self.base + 0x00C, long(value))
    def get_cr(self):
        return FLASH_CR(self.ap.read_mem_word(self.base + 0x010))
    def set_cr(self, **kwds):
        self._set_cr(FLASH_CR(**kwds))
    def _set_cr(self, value):
        self.ap.write_mem_word(self.base + 0x010, long(value))
    def set_ar(self, value):
        self.ap.write_mem_word(self.base + 0x014, value)
    def get_obr(self):
        return self.ap.read_mem_word(self.base + 0x01C)
    def get_wrpr(self):
        return self.ap.read_mem_word(self.base + 0x020)

    def unlock(self):
        self.set_fpec_keyr(FPEC.KEY1)
        self.set_fpec_keyr(FPEC.KEY2)
        return not bool(self.get_cr().lock)

    def unlock_option(self):
        self.unlock()
        self.set_opt_keyr(FPEC.KEY1)
        self.set_opt_keyr(FPEC.KEY2)
        return bool(self.get_cr().option_wre)

    def erase_option(self):
        self.set_cr(option_erase=1, option_wre=1)
        self.set_cr(option_wre=1, start=1)
        while self.get_sr().busy:
            pass

    def erase_flash_page(self, address):
        self.set_cr(page_erase=1)
        self.set_ar(address)
        self.set_cr(start=1)
        while self.get_sr().busy:
            pass

    def erase_flash(self):
        self.set_cr(mass_erase=1)
        self.set_cr(start=1)
        while self.get_sr().busy:
            pass

    def program_option(self, address, value):
        self.set_cr(option_program=1, option_wre=1)
        self.set_sr(program_err=1)
        self.ap.write_mem_halfword(address, value)
        while self.get_sr().busy:
            pass
        sr = self.get_sr()
        while sr.busy:
            sr = self.get_sr()
        return not bool(sr.program_err)

    def program_flash(self, address, value):
        self.set_cr(page_program=1)
        self.set_sr(program_err=1)
        self.ap.write_mem_halfword(address, value)
        sr = self.get_sr()
        while sr.busy:
            sr = self.get_sr()
        return not bool(sr.program_err)


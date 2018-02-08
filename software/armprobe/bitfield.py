
class BitField(object):
    """Simple bit field implementation to handle registers"""

    def __init__(self, format, value=0L, **kwds):
        object.__setattr__(self, "_format", format)
        object.__setattr__(self, "_value", long(value))
        for k, v in kwds.iteritems():
            setattr(self, k, v)

    def __getattr__(self, name):
        try:
            at, count = self._format[name]
        except KeyError:
            raise AttributeError(name)
        return (self._value >> at) & ((1 << count) - 1)

    def __setattr__(self, name, value):
        try:
            at, count = self._format[name]
        except KeyError:
            raise AttributeError(name)
        mask = ((1 << count) - 1)
        value &= mask
        object.__setattr__(self, "_value", (self._value & ~(mask << at)) | (value << at))

    def __int__(self):
        return self._value
    def __long__(self):
        return self._value
            
    def __repr__(self):
        return "%s(0x%08X)" % (self.__class__.__name__, self._value)

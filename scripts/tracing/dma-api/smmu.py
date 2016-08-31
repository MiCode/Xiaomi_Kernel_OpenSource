"""Low-level memory management tracking"""

VERBOSITY = 0 # TODO: use logging

class Bitmap(object):
    """Just a raw bitmap for reserving the pages"""
    def __init__(self, size, verbosity):
        self._size = size
        self._bits = 0
        self._verbosity = verbosity
        self._bits_allocd = 0

    def _mask(self, offset, length):
        """length amount of 1's, shifted left by offset"""
        assert offset >= 0, "offset < 0"
        assert offset < self._size, "offset 0x%d >= size %s" % (offset, self._size)
        bitstring = (1 << length) - 1
        return bitstring << offset

    def _indices(self, offset, length):
        """Bit numbers starting from offset"""
        return range(offset, offset + length)

    def alloc(self, offset, length):
        """Reserve the bits, warn if verbose and some set already"""
        mask = self._mask(offset, length)
        if (self._bits & mask) != 0 and self._verbosity >= 1:
            print "warning: duplicate allocation"
        self._bits |= mask
        self._bits_allocd += length
        return self._indices(offset, length)

    def free(self, offset, length):
        """Free the bits, warn if verbose and some not set yet"""
        mask = self._mask(offset, length)
        if (self._bits & mask) != mask and self._verbosity >= 1:
            print "warning: freeing freed memory, mapbits %x" % (self._bits & mask)
        self._bits &= ~mask
        self._bits_allocd -= length
        return self._indices(offset, length)

    def contains(self, offset, length):
        """Are some bits in the given range set?"""
        mask = self._mask(offset, length)
        return self._bits & mask

    def __repr__(self):
        return "<bitmap, %d/%d allocd>" % (self._bits_allocd, self._size)

class Memory(object):
    """Store and handle raw bitmaps, check mapping collisions between devices"""
    PAGESHIFT = 12
    PAGESIZE = 1 << PAGESHIFT
    PAGEMASK = PAGESIZE - 1

    def __init__(self, addr, size, asid):
        """addr: lowest possible address, size: bytes, asid: arbitrary id"""
        assert (addr & self.PAGEMASK) == 0, addr
        assert (size & self.PAGEMASK) == 0, size

        self._addr = addr
        self._size = size
        self._end = addr + size
        self._asid = asid
        self._bitmap = Bitmap(size >> self.PAGESHIFT, VERBOSITY)
        self._devmaps = {}

        if VERBOSITY >= 1:
            print "memory at %08x-%08x" % (addr, addr + size)

    def to_bit(self, addr):
        """Address to bitmap position"""
        return addr >> self.PAGESHIFT

    def alloc(self, dev, addr, size):
        """Allocate (map) for the given device, verify things"""
        if addr >= self._end:
            if VERBOSITY >= 1:
                print "warning: %s mapping beyond bitmap: %08x" % (dev, addr)
            return []

        if (addr & self.PAGEMASK) != 0:
            if VERBOSITY >= 1:
                print "warning: alloc not aligned at 0x%x, size %d (new addr 0x%x, size %d)" % (
                        addr, size, addr & ~self.PAGEMASK, size + (addr & self.PAGEMASK))
            addr &= ~self.PAGEMASK
            size += addr & self.PAGEMASK

        if size < self.PAGESIZE:
            size = self.PAGESIZE

        for user, bmp in self._devmaps.iteritems():
            if bmp.contains(self.to_bit(addr - self._addr), self.to_bit(size)):
                if VERBOSITY >= 1:
                    print "warning: %s mapping [0x%x,0x%x) already used by %s" % (
                            dev, addr, addr + size, user)

        devmap = self._devmaps.setdefault(dev, Bitmap(self._bitmap._size, 0))

        self._alloc(devmap, addr, size)
        bits = self._alloc(self._bitmap, addr, size)
        return bits

    def _alloc(self, bitmap, addr, size):
        """Allocate from an internal bitmap"""
        return bitmap.alloc(self.to_bit(addr - self._addr), self.to_bit(size))

    def free(self, dev, addr, size):
        """Free (unmap) for the given device, verify things"""
        if (addr & self.PAGEMASK) != 0:
            if VERBOSITY >= 1:
                print "warning: free not aligned at 0x%x, size %d (new addr 0x%x, size %d)" % (
                        addr, size, addr & ~self.PAGEMASK, size + (addr & self.PAGEMASK))
            addr &= ~self.PAGEMASK
            size += addr & self.PAGEMASK

        if size < self.PAGESIZE:
            size = self.PAGESIZE

        devmap = self._devmaps.setdefault(dev, Bitmap(self._bitmap._size, 0))

        owners = []
        for user, bmp in self._devmaps.iteritems():
            if bmp.contains(self.to_bit(addr - self._addr), self.to_bit(size)):
                owners.append((user, bmp))

        if len(owners) == 0:
            if VERBOSITY >= 1:
                print "warning: %s freeing 0x%x that nobody owns" % (dev, addr)
        elif len(owners) == 1:
            if owners[0][0] != dev and VERBOSITY >= 2:
                print "note: %s freeing 0x%x allocd by %s" % (
                        dev, addr, owners[0][0])
            devmap = owners[0][1]

        self._free(devmap, addr, size)
        bits = self._free(self._bitmap, addr, size)
        return bits

    def _free(self, bitmap, addr, size):
        """Free from an internal bitmap"""
        return bitmap.free(self.to_bit(addr - self._addr), self.to_bit(size))


class Device(object):
    """Keep track of allocations per device/process

    This needs more tricky work for tracking inter-process maps/unmaps :(
    """

    def __init__(self, name, mem):
        self._name = name
        self._mem = mem
        self._max_alloc = 0
        self._cur_alloc = 0
        self._alloc_history = []
        self._addresses = []

    def alloc(self, addr, size):
        pages = self._mem.alloc(self, addr, size)
        if pages is not False:
            self._cur_alloc += size
            self._max_alloc = max(self._max_alloc, self._cur_alloc)
            self._alloc_history.append(self._cur_alloc)
            if addr in self._addresses:
                if VERBOSITY >= 1:
                    print "warning: %s allocing dupe address %x %s" % (self._name, addr, len([x for x in self._addresses if x == addr]))
            self._addresses.append(addr)
        return pages

    def free(self, addr, size):
        pages = self._mem.free(self, addr, size)
        self._cur_alloc -= size
        if addr in self._addresses:
            self._addresses.remove(addr)
        else:
            if VERBOSITY >= 1:
                print "warning: %s freeing unallocated %x" % (self._name, addr)
        return pages

    def history_at(self, i):
        return self._alloc_history[i]

    @property
    def name(self):
        return self._name

    def __str__(self):
        return self.name

    def __repr__(self):
        return "<dev: %s>" % self.name

    @property
    def max_allocated(self):
        return self._max_alloc


class AsidSpace(object):
    # TODO: don't pre-grep by archdata but put devices' mem maps here.
    pass


"""Main program and stuff"""

#from pprint import pprint
from sys import stdin
import os.path
import re
from argparse import ArgumentParser
import cPickle as pickle
from collections import namedtuple
from plotting import plotseries, disp_pic
import smmu

class TracelineParser(object):
    """Parse the needed information out of an ftrace line"""
    #            <...>-6     [000] d..2     5.287079: dmadebug_iommu_map_page: device=sdhci-tegra.3, addr=0x01048000, size=4096 page=c13e7214 archdata=ed504640
    def __init__(self):
        self.pattern = re.compile("device=(?P<dev>.*), addr=(?P<addr>.*), size=(?P<size>.*) page=(?P<page>.*) archdata=(?P<archdata>.*)")
    def parse(self, args):
        args = self.pattern.match(args)
        return (args.group("dev"), int(args.group("addr"), 16),
                int(args.group("size")), int(args.group("page"), 16),
                int(args.group("archdata"), 16))

def biggest_indices(items, n):
    """Return list of indices of n biggest elements in items"""
    with_indices = [(x, i) for i, x in enumerate(items)]
    ordered = sorted(with_indices)
    return [i for x, i in ordered[-n:]]

def by_indices(xs, ids):
    """Get elements from the list xs by their indices"""
    return [xs[i] for i in ids]

"""Event represents one input line"""
Event = namedtuple("Event", ["time", "dev", "data", "delta"])

class Trace(object):
    def __init__(self, args):
        smmu.VERBOSITY = args.verbosity
        self._args = args
        self.devlist = []
        self.events = []
        self.metrics = {
                "max_peak": self._usage_peak,
                "activity_rate": self._usage_activity,
                "average_mem": self._usage_avg
        }
        self.traceliner = TracelineParser()

    @staticmethod
    def get_metrics():
        """What filter metrics to get max users"""
        return ["max_peak", "activity_rate", "average_mem"]

    def show(self):
        """Shuffle events around, build plots, and show them"""
        if self._args.max_plots:
            evs = self.merge_events()
        else:
            evs = self.events
        series, devlist = self.unload(evs)
        if not self._args.no_plots:
            self.plot(series, devlist)

    def _get_usage(self, evs):
        """Return a metric of how active the events in evs are"""
        return self.metrics[self._args.max_metric](evs)

    def _usage_peak(self, evs):
        """Return the biggest peak"""
        return max(e.data for e in evs)

    def _usage_activity(self, evs):
        """Return the activity count: simply the length of the event list"""
        return len(evs)

    def _usage_avg(self, evs):
        """Return the average over all points"""
        # FIXME: the data points are not uniform in time, so this might be
        # somewhat off.
        return float(sum(e.data for e in evs)) / len(e)

    def merge_events(self):
        """Find out biggest users, keep them and flatten others to a single user"""
        sizes = []
        dev_evs = []
        for i, dev in enumerate(self.devlist):
            dev_evs.append([e for e in self.events if e.dev == dev])
            sizes.append(self._get_usage(dev_evs[i]))

        # indices of the devices
        biggestix = biggest_indices(sizes, self._args.max_plots)
        print biggestix
        is_big = {}
        for i, dev in enumerate(self.devlist):
            is_big[dev] = i in biggestix

        evs = []
        for e in self.events:
            if not is_big[e.dev]:
                e = Event(e.time, "others", e.data, e.delta)
            evs.append(e)

        self.devlist.append("others")
        return evs

    def unload(self, events):
        """Prepare the event list for plotting

        series ends up as [([time0], [data0]), ([time1], [data1]), ...]
        """
        # ([x], [y]) for matplotlib
        series = [([], []) for x in self.devlist]
        devidx = dict([(d, i) for i, d in enumerate(self.devlist)])

        for event in events:
            devid = devidx[event.dev]
            series[devid][0].append(event.time)
            series[devid][1].append(event.data) # self.dev_data(event.dev))

        series_out = []
        devlist_out = []

        for ser, dev in zip(series, self.devlist):
            if len(ser[0]) > 0:
                series_out.append(ser)
                devlist_out.append(dev)

        return series_out, devlist_out

    def plot(self, series, devlist):
        """Display the plots"""
        #series, devlist = flatten_axes(self.series, self.devlist,
        #        self._args.max_plots)
        devinfo = (series, map(str, devlist))
        allocfreeinfo = (self.allocsfrees, ["allocd", "freed", "current"])
        plotseries(devinfo, allocfreeinfo)
        #plotseries(devinfo)

    def dev_data(self, dev):
        """what data to plot against time"""
        return dev._cur_alloc

    def _cache_hash(self, filename):
        """The trace files are probably not of the same size"""
        return str(os.path.getsize(filename))

    def load_cache(self):
        """Get the trace data from a database file, if one exists"""
        has = self._cache_hash(self._args.filename)
        try:
            cache = open("trace." + has)
        except IOError:
            pass
        else:
            self._load_cache(pickle.load(cache))
            return True
        return False

    def save_cache(self):
        """Store the raw trace data to a database"""
        data = self._save_cache()
        fh = open("trace." + self._cache_hash(self._args.filename), "w")
        pickle.dump(data, fh)

    def _save_cache(self):
        """Return the internal data that is needed to be pickled"""
        return self.events, self.devlist, self.allocsfrees

    def _load_cache(self, data):
        """Get the data from an unpickled object"""
        self.events, self.devlist, self.allocsfrees = data

    def load_events(self):
        """Get the internal data from a trace file or cache"""
        if self._args.filename:
            if self._args.cache and self.load_cache():
                return
            fh = open(self._args.filename)
        else:
            fh = stdin

        self.parse(fh)

        if self._args.cache and self._args.filename:
            self.save_cache()

    def parse(self, fh):
        """Parse the trace file in fh, store data to self"""
        mems = {}
        dev_by_name = {}
        devlist = []
        buf_owners = {}
        events = []
        allocsfrees = [([], []), ([], []), ([], [])] # allocs, frees, current
        allocs = 0
        frees = 0
        curbufs = 0

        mem_bytes = 1024 * 1024 * 1024
        npages = mem_bytes / 4096
        ncols = 512
        le_pic = [0] * npages
        lastupd = 0

        for lineidx, line in enumerate(fh):
            # no comments
            if line.startswith("#"):
                continue

            taskpid, cpu, flags, timestamp, func, args = line.strip().split(None, 5)
            func = func[:-len(":")]
            # unneeded events may be there too
            if not func.startswith("dmadebug"):
                continue

            if self._args.verbosity >= 3:
                print line.rstrip()

            timestamp = float(timestamp[:-1])
            if timestamp < self._args.start:
                continue
            if timestamp >= self._args.end:
                break

            devname, addr, size, page, archdata = self.traceliner.parse(args)
            if self._args.processes:
                devname = taskpid.split("-")[0]
            mapping = archdata

            try:
                memmap = mems[mapping]
            except KeyError:
                memmap = mem(mapping)
                mems[mapping] = memmap

            try:
                dev = dev_by_name[devname]
            except KeyError:
                dev = smmu.Device(devname, memmap)
                dev_by_name[devname] = dev
                devlist.append(dev)

            allocfuncs = ["dmadebug_map_page", "dmadebug_map_sg", "dmadebug_alloc_coherent"]
            freefuncs = ["dmadebug_unmap_page", "dmadebug_unmap_sg", "dmadebug_free_coherent"]
            ignfuncs = []

            if timestamp-lastupd > 0.1:
                # just some debug prints for now
                lastupd = timestamp
                print lineidx,timestamp
                le_pic2 = [le_pic[i:i+ncols] for i in range(0, npages, ncols)]
                #disp_pic(le_pic2)

            # animating the bitmap would be cool
            #for row in le_pic:
            #    for i, a in enumerate(row):
            #        pass
                    #row[i] = 0.09 * a

            if func in allocfuncs:
                pages = dev_by_name[devname].alloc(addr, size)
                for p in pages:
                    le_pic[p] = 1
                buf_owners[addr] = dev_by_name[devname]
                allocs += 1
                curbufs += 1
                allocsfrees[0][0].append(timestamp)
                allocsfrees[0][1].append(allocs)
            elif func in freefuncs:
                if addr not in buf_owners:
                    if self._args.verbosity >= 1:
                        print "warning: %s unmapping unmapped %s" % (dev, addr)
                    buf_owners[addr] = dev
                # fixme: move this to bitmap handling
                # get to know the owners of bits
                # allocs/frees calls should be traced separately from maps?
                # map_pages is traced per page :(
                if buf_owners[addr] != dev and self._args.verbosity >= 2:
                    print "note: %s unmapping [%d,%d) mapped by %s" % (
                            dev, addr, addr+size, buf_owners[addr])
                pages = buf_owners[addr].free(addr, size)
                for p in pages:
                    le_pic[p] = 0
                frees -= 1
                curbufs -= 1
                allocsfrees[1][0].append(timestamp)
                allocsfrees[1][1].append(frees)
            elif func not in ignfuncs:
                raise ValueError("unhandled %s" % func)

            allocsfrees[2][0].append(timestamp)
            allocsfrees[2][1].append(curbufs)

            events.append(Event(timestamp, dev, self.dev_data(dev), size))

        self.events = events
        self.devlist = devlist
        self.allocsfrees = allocsfrees

        le_pic2 = [le_pic[i:i+ncols] for i in range(0, npages, ncols)]
        # FIXME: not quite ready yet
        disp_pic(le_pic2)

        return

def mem(asid):
    """Create a new memory object for the given asid space"""
    SZ_2G = 2 * 1024 * 1024 * 1024
    SZ_1M = 1 * 1024 * 1024
    # arch/arm/mach-tegra/include/mach/iomap.h TEGRA_SMMU_(BASE|SIZE)
    base = 0x80000000
    size = SZ_2G - SZ_1M
    return smmu.Memory(base, size, asid)

def get_args():
    """Eat command line arguments, return argparse namespace for settings"""
    parser = ArgumentParser()
    parser.add_argument("filename", nargs="?",
            help="trace file dump, stdin if not given")
    parser.add_argument("-s", "--start", type=float, default=0,
            help="start timestamp")
    parser.add_argument("-e", "--end", type=float, default=1e9,
            help="end timestamp")
    parser.add_argument("-v", "--verbosity", action="count", default=0,
            help="amount of extra information: once for warns (dup addrs), "
            "twice for notices (different client in map/unmap), "
            "three for echoing all back")
    parser.add_argument("-p", "--processes", action="store_true",
            help="use processes as memory clients instead of devices")
    parser.add_argument("-n", "--no-plots", action="store_true",
            help="Don't draw the plots, only read the trace")
    parser.add_argument("-c", "--cache", action="store_true",
            help="Pickle the data and make a cache file for fast reloading")
    parser.add_argument("-m", "--max-plots", type=int,
            help="Maximum number of clients to show; show biggest and sum others")
    parser.add_argument("-M", "--max-metric", choices=Trace.get_metrics(),
            default=Trace.get_metrics()[0],
            help="Metric to use when choosing clients in --max-plots")
    return parser.parse_args()

def main():
    args = get_args()
    trace = Trace(args)
    trace.load_events()
    trace.show()

if __name__ == "__main__":
    main()

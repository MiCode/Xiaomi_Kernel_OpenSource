"""Ugly graph drawing tools"""
import matplotlib.pyplot as plt
import matplotlib.cm as cmap
#import numpy as np
from matplotlib import cbook

# http://stackoverflow.com/questions/4652439/is-there-a-matplotlib-equivalent-of-matlabs-datacursormode
class DataCursor(object):
    """A simple data cursor widget that displays the x,y location of a
    matplotlib artist when it is selected."""
    def __init__(self, artists, tolerance=5, offsets=(-20, 20),
                 template='x: %0.2f\ny: %0.2f', display_all=False):
        """Create the data cursor and connect it to the relevant figure.
        "artists" is the matplotlib artist or sequence of artists that will be
            selected.
        "tolerance" is the radius (in points) that the mouse click must be
            within to select the artist.
        "offsets" is a tuple of (x,y) offsets in points from the selected
            point to the displayed annotation box
        "template" is the format string to be used. Note: For compatibility
            with older versions of python, this uses the old-style (%)
            formatting specification.
        "display_all" controls whether more than one annotation box will
            be shown if there are multiple axes.  Only one will be shown
            per-axis, regardless.
        """
        self.template = template
        self.offsets = offsets
        self.display_all = display_all
        if not cbook.iterable(artists):
            artists = [artists]
        self.artists = artists
        self.axes = tuple(set(art.axes for art in self.artists))
        self.figures = tuple(set(ax.figure for ax in self.axes))

        self.annotations = {}
        for ax in self.axes:
            self.annotations[ax] = self.annotate(ax)

        for artist in self.artists:
            artist.set_picker(tolerance)
        for fig in self.figures:
            fig.canvas.mpl_connect('pick_event', self)

    def annotate(self, ax):
        """Draws and hides the annotation box for the given axis "ax"."""
        annotation = ax.annotate(self.template, xy=(0, 0), ha='right',
                xytext=self.offsets, textcoords='offset points', va='bottom',
                bbox=dict(boxstyle='round,pad=0.5', fc='yellow', alpha=0.5),
                arrowprops=dict(arrowstyle='->', connectionstyle='arc3,rad=0')
                )
        annotation.set_visible(False)
        return annotation

    def __call__(self, event):
        """Intended to be called through "mpl_connect"."""
        # Rather than trying to interpolate, just display the clicked coords
        # This will only be called if it's within "tolerance", anyway.
        x, y = event.mouseevent.xdata, event.mouseevent.ydata
        try:
            annotation = self.annotations[event.artist.axes]
        except KeyError:
            return
        if x is not None:
            if not self.display_all:
                # Hide any other annotation boxes...
                for ann in self.annotations.values():
                    ann.set_visible(False)
            # Update the annotation in the current axis..
            annotation.xy = x, y
            annotation.set_text(self.template % (x, y))
            annotation.set_visible(True)
            event.canvas.draw()

def plotseries(*serieslabels):
    """Plot lists of series in separate axes, tie time axis together"""
    global fig
    fig, axes = plt.subplots(nrows=len(serieslabels), sharex=True)

    for subplot, ax in zip(serieslabels, axes):
        for ser, lab in zip(*subplot): # subplot = ([x], [y])
            ax.step(ser[0], ser[1], label=lab, where="post")
        ax.grid(True)
        ax.legend()

        (DataCursor(ax.lines))
        plt.grid(True)
    plt.show()



def disp_pic(bitmap):
    """Display the allocation bitmap. TODO."""
    fig=plt.figure()
    a=fig.add_subplot(1,1,1)
    fig.clf()
    implt=plt.imshow(bitmap, extent=(0, len(bitmap[0]), 0, len(bitmap)),
            interpolation="nearest", cmap=cmap.gist_heat)
    fig.canvas.draw()
    plt.show()

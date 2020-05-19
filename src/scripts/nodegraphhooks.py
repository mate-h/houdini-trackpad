from canvaseventtypes import *

def createEventHandler(uievent, pending_actions):
    if isinstance(uievent, MouseEvent) and \
           uievent.eventtype == 'mousewheel':
        return None, True

    return None, False
import sys;
import string;

debug = 0
last = None
path = []
state = "<unknown??>"

def path_append(cur, path):
    if path == []:
        return [cur]

    top = path[0]
    if (top == cur):
        return path

    return [cur] + path

def squeeze_path(cur, path):
    (cur_file, cur_state, cur_depth) = cur
    
    if path == []:
        return path
    
    plist = path
    best = None
        
    while plist != []:
        (w, s, d) = plist[0]

        if (d < cur_depth):
            break
        
        if (s == cur_state and d == cur_depth):
            # We are not appending this, but we need to remember
            # it in case it is a procedure call or return:
            best = plist
            break

        plist = plist[1:]

    if (best != None):
        return best

    return path
    
def maybe_push_state(cur, path):
    global last

    (where, state, depth) = cur

    if last == None:
        last = cur
        path = path_append(cur, squeeze_path(cur, path))
        return path
    
    # If this transition is a self-transition, skip it:
    if (last[1] == state and last[2] == depth):
        last = cur
        return path

    # If this is a transition to a different state at same
    # level, push it, since it must be on the path to dissaster.
    if (last[2] == depth and last[1] != state):
        if (debug): print "state " + str(cur)
        path = [cur] + squeeze_path(cur, path)
        last = cur
        if (debug): print path
        return path

    # If this is a transition to a deeper state at same
    # level, push it, since it MAY be on the path to dissaster.
    # However, we may discard this later.
    if (last[2] < depth):
        if (debug): print "deeper last " + str(last)
        if (debug): print "deeper cur  " + str(cur)

        if (last != None): path = [last] + squeeze_path(last, path)
        path = [cur] + squeeze_path(cur, path)
        last = cur
        if (debug): print path
        return path

    # Tricky case: if this is a transition to a SHALLOWER state
    # then we need to see whether to toss some prior states:
    if (last[2] > depth):
        if (debug): print "shallow " + str(cur)

        path = path_append(cur, squeeze_path(cur, path))
        last = cur
        if (debug): print path

    return path
        

def filter_line(l):
    global state
    global depth
    global path
    
    if (string.find(l, ":") >= 0):
        fields = string.split(l)

        path = maybe_push_state((fields[0], fields[1], fields[2]), path)
    else:
        sys.stdout.write(l)

def list_reverse(l):
    r = []
    
    while (l != []):
        first, rest = l[0], l[1:]

        r = [first] + r

        l = rest;

    return r;
    
tracefile = open(sys.argv[1])

for line in tracefile.readlines():
    filter_line(line)

if (debug): print list_reverse(path)

map(lambda x: sys.stdout.write("%s %s %s\n" % (x[0], x[1], x[2])), list_reverse(path))

tracefile.close()

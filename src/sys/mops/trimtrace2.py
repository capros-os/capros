import sys;
import string;

lnvec = []
    
def acquire_line(vec, l):
    global lnvec

    if (string.find(l, ":") < 0):
        return vec

    #print l
    
    fields = string.split(l)
    where = fields[0]
    state = fields[1]
    depth = fields[2]

    fields = where.split(':')
    file = fields[0]
    line = fields[1]

    return vec + [(file, line, state, depth)]

def trim4(vec):
    if len(vec) <= 3:
        return vec
        
    (file0, line0, state0, depth0) = vec[0]
    (file1, line1, state1, depth1) = vec[1]
    (file2, line2, state2, depth2) = vec[2]
    (file3, line3, state3, depth3) = vec[3]

    if (state0 == state1 and state1 == state2 and
        state2 == state3 and depth0 == depth3 and depth1 == depth2 and
        string.atoi(depth1) == string.atoi(depth0) + 1):
        return trimtrace([vec[0]] + vec[3:])
    else:
        return [vec[0]] + trim4(vec[1:])

def trimtrace(vec):
    if len(vec) <= 2:
        return vec
    
    (file0, line0, state0, depth0) = vec[0]
    (file1, line1, state1, depth1) = vec[1]
    (file2, line2, state2, depth2) = vec[2]

    if (state0 == state1 and state1 == state2 and
        depth0 == depth1 and depth1 == depth2):
        return trimtrace([vec[0]] + vec[2:])
    else:
        return [vec[0]] + trimtrace(vec[1:])
        
def process(nm):
    f = open(nm)
    vec = []

    for line in f.readlines():
        vec = acquire_line(vec, line)

    f.close()

    done = 0
    while done == 0:
        vec = trimtrace(vec)
        size = len(vec)
        vec = trim4(vec)
        newsize = len(vec)

        if (size == newsize): done = 1
        
    return vec


def output(vec):
    for v in vec:
        (file, line, state, depth) = v
        sys.stdout.write("%s:%s: %s %s\n" % (file, line, state, depth))
        
output(process(sys.argv[1]))

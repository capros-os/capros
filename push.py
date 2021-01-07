from __future__ import print_function
from subprocess import Popen, PIPE
from os import walk
from os.path import join
import sys

def check_clean():
    clean = True
    for base, dirnames, filenames in walk("."):
        for filename in filenames:
            if filename.endswith("~") or filename.startswith(".#"):
                print(join(base, filename))
                clean = False
        if "BUILD" in dirnames:
            print(join(base, "BUILD"))
            clean = False
    if not clean:
        sys.exit("please clean the repository and try again.")


check_clean()

with open(".git/refs/heads/master") as f:
    capos_commit = f.read().strip()
capos_hash = Popen(["guix", "hash", "-rx", "."], stdout=PIPE).stdout.read().strip()

defines = '(define capos-commit "%s")\n(define capos-hash "%s")\n' % (
    capos_commit, capos_hash
)

print(defines)

Popen(["xclip"], stdin=PIPE).communicate(defines)
Popen(["xclip", "-sel", "clip"], stdin=PIPE).communicate(defines)

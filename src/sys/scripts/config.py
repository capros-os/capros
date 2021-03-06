#
# Copyright (C) 1998, 1999, Jonathan S. Shapiro.
# Copyright (C) 2007, Strawberry Development Group.
#
# This file is part of the CapROS Operating System,
# and is derived from the EROS Operating System.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2,
# or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

# This material is based upon work supported by the US Defense Advanced
# Research Projects Agency under Contract No. W31P4Q-07-C-0070.
# Approved for public release, distribution unlimited.

# Python script to configure a kernel.
#
# Documentation for this script can be found in the 
# documentation tree at 
#
#         www/devel/ProgGuide/kernel-config.html
#
#

import sys
import os
import glob
# import regex

config_name_table = { }


def publishf(f):
    config_name_table[f.__name__] = f
    return f


def publish(name):
    config_name_table[name] = globals()[name]

################################################
##
## Global variables:
##
################################################

## Known device classes:

BT_NONE = -1
BT_BASE = 0
BT_PCI = 1
BT_EISA = 2
BT_ISA = 3
BT_SCSI = 4
BT_USB = 5

device_classes = {
    -1 : "BT_NONE",
    0  : "BT_BASE",
    1  : "BT_PCI",
    2  : "BT_EISA",
    3  : "BT_ISA",
    4  : "BT_SCSI",
    5  : "BT_USB"
}

publish('BT_BASE')
publish('BT_PCI')
publish('BT_EISA')
publish('BT_ISA')
publish('BT_SCSI')

conf_machine = None
conf_arch = None
conf_cpus = [ ]
conf_options = {}
conf_primoptions = {}
conf_targets = [ ]
conf_busses = [ BT_BASE ]
conf_defines = {}

depends_on = {}

targdir = "../BUILD/%s" % sys.argv[1]
conf_name = os.path.basename(sys.argv[1])

port_table = { }
port_table[0] = -1

mem_addr_table = { }
mem_addr_table[0] = -1

# file_stack = []
 
# RE_devname = regex.compile("\([a-zA-Z]+\)\([0-9]*\)?");


################################################
##
## Error Messages
##
################################################
NoMachine = "Unknown machine"


################################################
##
## Known machine and CPU types:
##
################################################

machine_types = {
    'arm' : [ "edb9315", "edb9315_memtest" ],
    'x86' : [ "pc" ],
    }
cpu_types = {
    'arm' : [ "arm920t", ],
    'x86' : [ "i386", "i486", "pentium", "ppro" ],
    }

def cleanup_targdir():
    if not os.path.exists("../BUILD"):
        os.mkdir("../BUILD", 0o755)
    elif not os.path.isdir("../BUILD"):
        error("\"../BUILD\" is not a directory")

    if not (os.path.exists(targdir)):
        os.mkdir(targdir, 0o755)
    elif not os.path.isdir(targdir):
        error("\"%s\" is not a directory" % targdir)

    # Clean out the existing contents of that directory
    for nm in glob.glob("%s/.*" % targdir):
        os.remove(nm)
    for nm in glob.glob("%s/*" % targdir):
        os.remove(nm)

def error(s):
#    print "Error in \"%s\" at line %d: %s" % (current_file, current_line, s)
    print("Error:", s)
    sys.exit(1)


config_table = {};
config_by_name = {};

#def device(name, parent=None, irq = None, port = None, mem = None, sz = 0):
#    global config_by_name
#    if (parent):
#        parent = config_by_name[parent]
#    else:
#        error("Templates must identify their parents")
#    tmpl = ConfTemplate(name, parent, irq, port, mem, sz, 'dev ')
#    add_conf(tmpl)
#

next_instance_index = {}

#def instance(name, instname, parent = None, irq = None, port = None, mem = None, sz = 0):
#    global next_instance_index
#
#    if not parent:
#        error("Instances require parents")
#
#    if (name): tmpl = config_by_name[name]
#    else: tmpl = None
#
#    parent = config_by_name[parent]
#
#    if parent.devClass != DC_INSTANCE:
#        error("Instances can only be hung off of instances")
#
#    if tmpl:
#        if (not irq and type(tmpl.irq) == type(5)):
#            irq = tmpl.irq
#
#        if tmpl.irq and irq != tmpl.irq and not irq in tmpl.irq:
#            error("Specified irq not in irq list")
#
#        if (not port and type(tmpl.io_port) == type(5)):
#            port = tmpl.irq
#
#        if (tmpl.io_port and port != tmpl.io_port and not port in tmpl.io_port):
#            error("Specified port not in port list")
#
#        if not mem and tmpl.mem_addr:
#            error("Bad memory address specified")
#
#        if tmpl.mem_addr and not mem in tmpl.mem_addr:
#            error("Specified memory address not in address list")
#
#        name = "%s%d" % (tmpl.name, tmpl.nInstance)
#        if (name != instname):
#            error("That instance is already allocated")
#
#        tmpl.nInstance = tmpl.nInstance + 1
#    else:
#        RE_devname.match(instname)
#        if (not next_instance_index.has_key(RE_devname.group(1))):
#            ndx = 0
#        else:
#            ndx = next_instance_index[RE_devname.group(1)]
#
#        next_instance_index[RE_devname.group(1)] = ndx+1
#
#        if (RE_devname.group(2) != str(ndx)):
#            error("%s: Unique instances must have sequential indices beginning at 0" % instname)
#        name = instname
#    
#    tmpl = ConfTemplate(name, DC_INSTANCE, parent, irq, port, mem, sz)
#    add_conf(tmpl)
#    print "Defined instance '%s'." % name

@publishf
def machine(name):
    global conf_machine

    valid_machines = machine_types[conf_arch]

    if conf_machine is not None:
        error("Machine already defined!");
    elif name in valid_machines:
        print("Configuring for machine class '%s'." % name)
        conf_machine = name
    else:
        error("Unknown machine type")


@publishf
def arch(name):
    global conf_arch
    if conf_arch is not None:
        error("Architecture already defined!");
    elif name in cpu_types:
        print("Configuring for architecture '%s'." % name)
        conf_arch = name
    else:
        error("Unknown arch type");


@publishf
def cpu(name):
    if conf_machine is None:
        error("Unknown machine type!");

    valid_cpus = cpu_types[conf_arch]

    if name in valid_cpus:
#        print("Adding cpu type '%s'." % name)
        conf_cpus.append(name)
    else:
        error("Unknown CPU type")


@publishf
def defoption(name, prim=0):
    if prim:
        conf_primoptions[name] = 0
    else:
        conf_options[name] = 0

##
## Following is for internal value extraction:
##
def option_value(name):
    if name in conf_options:
        return conf_options[name]
    elif name in conf_primoptions:
        return conf_primoptions[name]
    else:
        error("Option_value(%r) on undefined option." % name)

@publishf
def option(name):
    if name in conf_options:
        conf_options[name] = 1
    elif name in conf_primoptions:
        conf_primoptions[name] = 1
    else:
        error("Unknown option %r" % name)


@publishf
def define(name):
    conf_defines[name] = 1


@publishf
def exclude(name):
    if name in conf_options:
        conf_options[name] = 0
    elif name in conf_primoptions:
        conf_primoptions[name] = 0
    else:
        error("Unknown option %r" % name)


@publishf
def depends(name1, name2):
    depends_on.setdefault(name1, []).append(name2)


#def defbus(name):
#    global conf_busses
#    conf_busses[name] = 0
#publish('defoption')
#
@publishf
def bus(name):
    conf_busses.append(name)

#def target(name):
#    global conf_targets
#
#    conf_targets = conf_targets + [conf_name + name]
#publish('target')

@publishf
def isoption(name):
    if name in conf_options:
        return conf_options[name]
    elif name in conf_primoptions:
        return conf_primoptions[name]
    else:
        error("Unknown option \"%s\"" % name)


@publishf
def ifdevice(name):
    return 1 if name in config_by_name else 0


@publishf
def pseudo_device(name, count=1):
    filename = "%s/%s.h" % (targdir,name)
    with open(filename, 'w') as out:
        cpp_define = "#define N%s %d" % (name.upper(), count)
        out.write("%s\n" % cpp_define)


#root = ConfTemplate("root", DC_INSTANCE, parent = None, irq = None, port = None, mem = None, sz=0)
#main = ConfTemplate("mainboard", DC_BUS, parent = root, irq = None, port = None, mem = None, sz=0)

#add_conf(root)
#add_conf(main)

cleanup_targdir()


def execfile(filename, local_vars, global_vars=None):
    if global_vars is None:
        global_vars = globals()
    with open(filename) as f:
        exec(f.read(), local_vars, global_vars)


@publishf
def include(name):
    execfile(name, config_name_table)


################################################
##
## Now process the machine description file.
##
################################################

execfile(sys.argv[1], config_name_table, {})

#def ctrlr_stmt(l, cls):
#    global parent_objects
#    global config_table
#
#    # Device name takes one of two forms:  cccc or cccc?
#    RE_devname.match(l[1])
#    devname = RE_devname.group(1)
#    isunique = RE_devname.group(2)
#
#    if len(l) < 4:
#        error ("device must be attached!")
#
#    if (l[2] != "at"):
#            error ("device statement expects 'at'")
#
#    print(parent_objects)
#    if (not parent_objects.has_key(l[3])):
#            error ("device must be attached to controller or bus")
#
#    parent = parent_objects[l[3]]
#
#    print("parent is")
#    print(parent)
#
#    dev = DeviceInfo(devname, parent, cls)
#
#    dev.process_options(l[4:], cls != 'dev')
#
#    config_table = config_table + [ dev ];
#    if (cls != 'dev'):
#        parent_objects[l[0]] = dev;


# print(config_table)

def dump_ac_types(out):
    out.write("\n/* Driver structure declarations: */\n");
    for i, tmpl in enumerate(config_table):
        tmpl = config_table[i]
        out.write("extern struct Driver ac_%s;\n" % tmpl.name);

def dump_table(name, tbl, out):
    out.write("\nstatic int32_t %s[] = {" % name)

    for p in range(len(tbl)-1):
        if (p % 8 == 0):
            out.write("\n")

        value = tbl[p]
        if (value == -1):
            out.write(" -1,")
        else:
            out.write(" 0x%x," % value)

    ndx = len(tbl)-1
    value = tbl[len(tbl)-1]

    if (ndx % 8 == 0): out.write("\n")

    if (value == -1):
        out.write(" -1")
    else:
        out.write(" 0x%x" % value)
    out.write("\n};\n")

def dump_optvar(out):
    for b in device_classes:
        if b < 0: continue;        # that one is a placeholder
        if b in conf_busses:
            out.write("CONFIG_%s=1\n" % device_classes[b][3:])
        else:
            out.write("CONFIG_%s=0\n" % device_classes[b][3:])
    for o in conf_options:
        if conf_options[o]:
            out.write("OPT_%s=1\n" % o.upper())
        else:
            out.write("OPT_%s=0\n" % o.upper())
    for o in conf_primoptions:
        if conf_primoptions[o]:
            out.write("PRIMOPT_%s=1\n" % o.upper())
        else:
            out.write("PRIMOPT_%s=0\n" % o.upper())

def dump_options(out):
    for b in device_classes:
        if b in conf_busses:
            out.write("OPTIONS += -DCONFIG_%s=1\n" % device_classes[b][3:])
    for o in conf_options:
        if conf_options[o]:
            out.write("OPTIONS += -DOPTION_%s=1\n" % o.upper())
    for o in conf_primoptions:
        if conf_primoptions[o]:
            out.write("OPTIONS += -D%s=1\n" % o.upper())
    for c in conf_cpus:
        out.write("OPTIONS += -DCPU_%s=1\n" % c.upper())
    out.write("OPTIONS += -DARCH_%s=1\n" % conf_arch.upper())
    out.write("OPTIONS += -DMACHINE_%s=1\n" % conf_arch.upper())

def dump_defines(out):
    for d in conf_defines:
        out.write("CONF_DEFS += -D%s=1\n" % d)

def dump_options_header(out):
    for b in device_classes:
        if b in conf_busses:
            out.write("#define CONFIG_%s 1\n" % device_classes[b][3:])
    for o in conf_options:
        if conf_options[o]:
            out.write("#define OPTION_%s 1\n" % o.upper())
    for o in conf_primoptions:
        if conf_primoptions[o]:
            out.write("#define %s 1\n" % o.upper())
    for c in conf_cpus:
        out.write("#define CPU_%s 1\n" % c.upper())
    out.write("#define ARCH_%s 1\n" % conf_arch.upper())
    out.write("#define MACHINE_%s 1\n" % conf_machine.upper())
    for d in conf_defines:
        out.write("#define %s 1\n" % d)

#def dump_targets(out):
#    for o in conf_targets:
#        out.write("TARGETS += %s\n" % o)

# dump_options(sys.stdout)

# print(globals().keys())



################################################
##
## Now process the file list.
##
################################################

filenames = "files.%s" % conf_machine
src_file_list = []
obj_file_list = []
cfg_file_list = []     # MOPS

config_name_table = { }

###############################################
##
## We permit files to be multiply specified,
## because this allows the machine-dependent
## code to pull forward some files that must
## be co-located for IPC performance.
##
###############################################
@publishf
def file(name, condition = not None):
    global src_file_list
    global obj_file_list
    global cfg_file_list
    if (condition and not name in src_file_list):
        src_file_list = src_file_list + [name]
        output = os.path.basename(name)
        suffix =  os.path.splitext(output)[1]
        output = os.path.splitext(output)[0]
        ofile = output + ".o"
        ofile = "$(BUILDDIR)/" + ofile
        cfgfile = output + ".cfg"
        cfgfile = "$(BUILDDIR)/" + cfgfile
        obj_file_list = obj_file_list + [ofile]
        if (suffix == ".c"):
            cfg_file_list = cfg_file_list + [cfgfile]

publish('include')
publish('ifdevice')
publish('BT_BASE')
publish('BT_PCI')
publish('BT_EISA')
publish('BT_ISA')
publish('BT_SCSI')

for i in conf_options:
    config_name_table[i] = conf_options[i]

for i in conf_primoptions:
    config_name_table[i] = conf_primoptions[i]

for i in conf_cpus:
    config_name_table[i] = 1

#for i in range(len(config_table)):
#    config_name_table[config_table[i].name] = 1

execfile(filenames, config_name_table)


def check_dependencies():
    ###############################################
    ##
    ## Cross-check the dependencies:
    ##
    ##
    ###############################################

    for nm in depends_on:
        buggered = 0

        for require in depends_on[nm]:
            if option_value(nm) and not option_value(require):
                print("Error: \"%s\" depends on \"%s\"" % (nm , require))
                buggered = buggered + 1

        if buggered:
            error("%d dependency errors" % buggered)


check_dependencies()

###############################################
##
## Generate the makefile fragment for the files
##
##
###############################################

makefiletemplate = "Makefile.%s" % conf_machine
makefilename = "%s/Makefile" % targdir
optfilefilename = "%s/options.h" % targdir

print("building makefile %s" % makefilename)

template = open(makefiletemplate, 'r')
with open(makefilename, 'w') as out:
    for line in template.readlines():
        if (line == "%config\n"):
            out.write("CONFIG=%s\n" % conf_name)
        #elif (line == "%targets\n"):
            #dump_targets(out)
        elif (line == "%optvar\n"):
            dump_optvar(out)
        elif (line == "%options\n"):
            dump_options(out)
            dump_defines(out)
        elif (line == "%objects\n"):
            for o in obj_file_list:
                out.write("OBJECTS += %s\n" % o)
            out.write("\n")
            for o in cfg_file_list:
                out.write("CFGFILES += %s\n" % o)
        elif (line == "%depend\n"):
            for f in src_file_list:
                ofile = os.path.basename(f)
                suffix =  os.path.splitext(ofile)[1]
                ofile = os.path.splitext(ofile)[0]
                out.write("$(BUILDDIR)/%s.o: $(TOP)/%s\n" % (ofile, f))
                if (suffix == ".c"):
                    out.write("\t$(C_BUILD)\n")
                    out.write("\t$(C_DEP)\n\n")
                elif (suffix == ".cpp"):
                    out.write("\t$(CPP_BUILD)\n")
                    out.write("\t$(CPP_DEP)\n\n")
                elif (suffix == ".S"):
                    out.write("\t$(ASM_BUILD)\n")
                    out.write("\t$(ASM_DEP)\n\n")

                if (suffix == ".c"):
                    out.write("$(BUILDDIR)/%s.cfg: $(TOP)/%s\n" % (ofile, f))
                    out.write("\t$(MOPS_BUILD)\n")
                    out.write("\t$(MOPS_DEP)\n\n")
        else:
            out.write(line)


###############################################
##
## Generate the compile-time options file
##
###############################################


optfilename = "%s/kernel-config.h" % targdir
with open(optfilename, 'w') as out:
    dump_options_header(out)

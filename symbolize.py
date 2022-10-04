#!/usr/bin/env python3

import argparse
import os
import subprocess


def dump_syms_path(bin):
    if bin.endswith(".exe"): # Windows target
        ds_path = "src/tools/windows/dump_syms_dwarf/dump_syms"
    else: # Linux or NaCl target
        ds_path = "src/tools/linux/dump_syms/dump_syms"
    return os.path.join(os.path.dirname(__file__), ds_path)


def symbolize(bin, symdir):
    if not os.path.isfile(bin):
        # Check because dump_syms gives a bad error message for this case
        print("Binary doesn't exist:", bin)
        exit(1)
    if not os.path.isdir(symdir):
        # Avoid creating a new symbol directory on typos
        print("Symbol directory doesn't exist:", symdir)
        exit(1)
    try:
        proc = subprocess.Popen([dump_syms_path(bin), bin], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE)
    except FileNotFoundError:
        print("You need to build the dump_syms binary", dump_syms_path(bin))
        exit(1)
    try:
        out = None
        lines = []
        for ln in proc.stdout:
            if ln.startswith(b"MODULE"):
                assert out is None
                spl = ln.split()
                xid = spl[3].decode()
                xnam = spl[4].decode()
                if xid.strip("0") == "":
                    print("Binary lacks build id")
                    exit(1)
                if xnam.endswith(".nexe"):
                    tname = xnam = "main.nexe"
                else:
                    tname = os.path.basename(bin)
                outp = os.path.join(symdir, tname, xid, xnam + ".sym")
                opdn = os.path.dirname(outp)
                if not os.path.exists(opdn):
                    os.makedirs(opdn)
                print("Writing:", outp)
                out = open(outp, "wb")
            out.write(ln)
        proc.wait()
        return proc.returncode
    finally:
        proc.terminate()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Generate debug symbols and store in required directory structure",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("binary", help="Non-stripped Linux, Windows, or NaCl binary")
    symbols = os.path.join(os.path.dirname(__file__), "symbols")
    parser.add_argument("-s", "--symbol-directory", default=symbols, help="Where to store output")
    args = parser.parse_args()
    exit(symbolize(args.binary, args.symbol_directory))

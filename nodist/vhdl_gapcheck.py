#!/usr/bin/env python3
# pylint: disable=C0114,C0116,C0209,R0912,R0914,R0915
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of either the GNU Lesser General Public License Version 3
# or the Perl Artistic License Version 2.0.
# SPDX-FileCopyrightText: 2026-2026 Wilson Snyder
# SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
"""Filter the corpus scoreboard's "must fail and did not" list by what the
reference simulators actually do with each source.

A corpus expectation is not by itself a rule to implement.  VESTS demands an
analysis error for several things that nvc and GHDL both accept without a
word -- the operands of a logical operator being arrays of different lengths,
for one, which both leave to run time.  Enforcing those would refuse designs
those tools compile.  What is worth working on is the subset the references
refuse, and this prints exactly that, grouped by the LRM clause VESTS names
in each case's own identifier.

Two references disagree often enough to matter, so a third breaks the tie
where one is available: pass --questa to compile each source with Questa's
vcom as well, and a case is counted as one to act on when a majority of the
references available refuse it.

Typical use, from the Verilator repository root:

    nodist/vhdl_corpus.py --root ../ --json gaps.json
    nodist/vhdl_gapcheck.py gaps.json --questa
"""

import argparse
import collections
import importlib.util
import json
import os
import re
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor


def _corpus_module():
    """The scoreboard, whose reading of each suite's revisions this shares."""
    spec = importlib.util.spec_from_file_location(
        "vhdl_corpus", os.path.join(os.path.dirname(os.path.abspath(__file__)), "vhdl_corpus.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


_CORPUS = _corpus_module()


def std_of(path):
    """Which revision this source is written against, as its suite says:
    "93", "2000", "2002", "2008" or "2019"."""
    flags = _CORPUS.extra_flags(path)
    if "--vhdl-std" in flags:
        return flags[flags.index("--vhdl-std") + 1]
    return "2008"


def refuses(command, cwd):
    # In a process group of its own, so a timeout takes any child with it
    try:
        return _CORPUS.run_group(command, 60, cwd=cwd)[1] != 0
    except (subprocess.TimeoutExpired, OSError):
        return False


def siblings_of(path):
    """The other VHDL sources beside this one, which may hold what it uses.

    A corpus keeps a case's package in its own file next to the entity that
    uses it, so analyzing the case alone makes every reference report a
    design unit it cannot find -- a verdict about this script's working
    directory rather than about the source.  Analyzing the neighbours first
    is what makes a refusal evidence of anything.  Their own errors are
    ignored: only the named source's verdict is being taken.
    """
    folder = os.path.dirname(path) or "."
    try:
        names = sorted(os.listdir(folder))
    except OSError:
        return []
    return [os.path.join(folder, n) for n in names
            if n.lower().endswith((".vhd", ".vhdl")) and os.path.join(folder, n) != path]


def verdicts(args_path):
    """Return one entry per reference asked: True where it refuses the source."""
    args, path = args_path
    # More than a handful is a corpus directory rather than a case, and
    # analyzing all of it would cost more than the answer is worth
    others = siblings_of(path)
    if len(others) > 12:
        others = []
    with tempfile.TemporaryDirectory() as work:
        std = std_of(path)
        year = "1993" if std == "93" else std
        # Twice over, since nothing here knows which neighbour comes first
        for _ in range(2 if others else 0):
            for other in others:
                refuses([args.nvc, "--std=" + year, "-a", other], work)
                refuses([args.ghdl, "-a", "--std=" + year[2:], other], work)
        out = [("nvc", refuses([args.nvc, "--std=" + year, "-a", path], work)),
               ("ghdl", refuses([args.ghdl, "-a", "--std=" + year[2:], path], work))]
        if args.questa:
            # vcom needs a library of its own, and one per source keeps the
            # sources from seeing each other's design units
            subprocess.run([args.vlib, "work"], cwd=work, capture_output=True, check=False)
            # vcom has no switch for VHDL-2000, whose rules VHDL-2002 kept
            vcom_std = "-" + {"93": "93", "2000": "2002"}.get(std, year)
            for _ in range(2 if others else 0):
                for other in others:
                    refuses([args.vcom, vcom_std, "-quiet", other], work)
            out.append(("questa", refuses([args.vcom, vcom_std, "-quiet", path], work)))
        return out


def clause_of(path):
    """The LRM clause VESTS names in a case's own identifier, and its message."""
    try:
        with open(path, encoding="latin-1") as fh:
            text = fh.read()
    except OSError:
        return "(unreadable)", ""
    match = re.search(r"\bc(\d\d)s(\d\d)b", text)
    if not match:
        return "(unnamed)", os.path.basename(path)
    said = re.search(r"FAILED TEST: \S+ - (.{10,100})", text)
    return ("clause %d.%d" % (int(match.group(1)), int(match.group(2))),
            said.group(1).strip() if said else os.path.basename(path))


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('json', help='scoreboard JSON written by vhdl_corpus.py --json')
    parser.add_argument('--corpus', action='append', default=[], help='limit to these corpora')
    parser.add_argument('--nvc', default='nvc', help='nvc binary (default: nvc)')
    parser.add_argument('--ghdl', default='ghdl', help='ghdl binary (default: ghdl)')
    parser.add_argument('--questa',
                        action='store_true',
                        help="also ask Questa's vcom, which breaks a tie between the other two")
    parser.add_argument('--vcom', default='vcom', help='Questa vcom binary (default: vcom)')
    parser.add_argument('--vlib', default='vlib', help='Questa vlib binary (default: vlib)')
    parser.add_argument('--jobs', type=int, default=8, help='parallel jobs (default: 8)')
    parser.add_argument('--groups', type=int, default=20, help='clauses to list (default: 20)')
    parser.add_argument('--list',
                        action='append',
                        default=[],
                        metavar='VERDICT',
                        help="name every source under one verdict line, e.g."
                        " --list 'nvc refuses, ghdl refuses, questa accepts'."
                        " Give 'split' for every source the references do not"
                        " settle by a majority; repeatable.")
    parser.add_argument('--save',
                        default=None,
                        help='write each source and how every reference answered')
    args = parser.parse_args()

    with open(args.json, encoding='utf-8') as fh:
        report = json.load(fh)['corpora']
    todo = []
    for name, entry in report.items():
        if args.corpus and name not in args.corpus:
            continue
        for failure in entry.get('failures', []):
            if failure.get('expected') in ('reject', 'diagnose'):
                # Absolute, since each reference runs in a directory of its own
                todo.append(os.path.abspath(os.path.join(entry['root'], failure['file'])))
    if not todo:
        print('Nothing to check: no source in this report was expected to fail and did not')
        return 0
    who = [args.nvc, args.ghdl] + ([args.vcom] if args.questa else [])
    print('Checking %d sources against %s' % (len(todo), ', '.join(who)))
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        outcomes = list(pool.map(verdicts, [(args, path) for path in todo]))

    groups = collections.Counter()
    by_group = collections.defaultdict(list)
    agreed = []
    split = []
    for path, answers in zip(todo, outcomes):
        line = ', '.join('%s %s' % (who, 'refuses' if no else 'accepts') for who, no in answers)
        groups[line] += 1
        by_group[line].append(path)
        refused = sum(1 for _, no in answers if no)
        if refused * 2 > len(answers):
            agreed.append(path)
        elif refused:
            split.append(path)
    if args.save:
        with open(args.save, 'w', encoding='utf-8') as fh:
            json.dump({
                path: dict(answers)
                for path, answers in zip(todo, outcomes)
            },
                      fh,
                      indent=2,
                      sort_keys=True)
        print('Wrote %s' % args.save)
    print()
    for text, count in groups.most_common():
        print('  %5d  %s' % (count, text))
    print('\nA majority of the references refuse these, so they are rules worth'
          ' having: %d' % len(agreed))
    if split:
        print('And %d where the references are split without a majority' % len(split))
    print()

    clauses = collections.Counter()
    example = {}
    for path in agreed:
        clause, said = clause_of(path)
        clauses[clause] += 1
        example.setdefault(clause, said)
    for clause, count in clauses.most_common(args.groups):
        print('  %4d  %-12s %s' % (count, clause, example.get(clause, '')[:84]))

    for wanted in args.list:
        chosen = split if wanted == 'split' else by_group.get(wanted, [])
        print('\n%s -- %d source%s' % (wanted, len(chosen), '' if len(chosen) == 1 else 's'))
        if not chosen and wanted != 'split':
            print('  (no such verdict line; the lines above are the ones seen)')
        for path in sorted(chosen):
            clause, said = clause_of(path)
            print('  %-58s %-12s %s' % (shorten(path), clause, said[:60]))
    return 0


def shorten(path):
    """Enough of a path to find the file by, without the checkout in front."""
    for mark in ('/vests/', '/testsuite/', '/test/'):
        cut = path.find(mark)
        if cut >= 0:
            return path[cut + 1:]
    return os.path.basename(path)


if __name__ == '__main__':
    sys.exit(main())

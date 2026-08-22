#!/usr/bin/env python3
# pylint: disable=C0114,C0116,C0209,R0902,R0903,R0912,R0914
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of either the GNU Lesser General Public License Version 3
# or the Perl Artistic License Version 2.0.
# SPDX-FileCopyrightText: 2026-2026 Wilson Snyder
# SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
"""Score the VHDL frontend against external VHDL corpora.

The corpora are large, third-party, and deliberately kept out of this
repository; this script only reads them from wherever they happen to be
checked out.  It runs Verilator over every VHDL source it finds and reports
how many were accepted, so progress across milestones is a number rather
than an impression.

Corpus roots are discovered from --root (the directory holding the checkouts),
from $VHDL_CORPUS_ROOT, or given individually with --corpus NAME=PATH.

Typical use, from the Verilator repository root:

    nodist/vhdl_corpus.py --root ../ --jobs 24
    nodist/vhdl_corpus.py --root ../ --json scoreboard.json
    nodist/vhdl_corpus.py --root ../ --corpus nvc --verbose
"""

import argparse
import json
import multiprocessing
import os
import re
import subprocess
import sys
import time

# Known corpora: name -> (subdirectory under --root, globs to search)
CORPORA = {
    'nvc-parse': ('nvc', ['test/parse']),
    'nvc-sem': ('nvc', ['test/sem']),
    'nvc-bounds': ('nvc', ['test/bounds']),
    'nvc-simp': ('nvc', ['test/simp']),
    'nvc-regress': ('nvc', ['test/regress']),
    'ghdl': ('ghdl', ['testsuite']),
    'compliance': ('Compliance-Tests', ['vhdl_2008', 'vhdl_2019']),
    'uvvm': ('UVVM', ['.']),
    'vunit': ('vunit', ['vunit/vhdl']),
}

VHDL_SUFFIXES = ('.vhd', '.vhdl')


class Result:
    """Outcome of running Verilator over one source file."""

    __slots__ = ('path', 'ok', 'seconds', 'message')

    def __init__(self, path, ok, seconds, message):
        self.path = path
        self.ok = ok
        self.seconds = seconds
        self.message = message


def find_sources(root, subdirs):
    """Return every VHDL source under the given subdirectories, sorted."""
    found = []
    for subdir in subdirs:
        base = os.path.join(root, subdir)
        if not os.path.isdir(base):
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = [d for d in dirnames if not d.startswith('.')]
            for filename in filenames:
                if filename.endswith(VHDL_SUFFIXES):
                    found.append(os.path.join(dirpath, filename))
    return sorted(found)


def run_one(args):
    """Run Verilator over one source. Returns a Result."""
    verilator, flags, timeout, path = args
    command = [verilator] + flags + [path]
    start = time.time()
    try:
        proc = subprocess.run(command,
                              stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT,
                              timeout=timeout,
                              check=False)
        output = proc.stdout.decode('utf-8', errors='replace')
        ok = proc.returncode == 0
    except subprocess.TimeoutExpired:
        return Result(path, False, time.time() - start, 'TIMEOUT')
    except OSError as exc:
        return Result(path, False, time.time() - start, 'EXEC: %s' % exc)

    message = ''
    if not ok:
        # First %Error line is the most informative summary of the failure
        for line in output.splitlines():
            if line.startswith('%Error'):
                message = line.strip()
                break
        else:
            message = (output.strip().splitlines() or ['(no output)'])[-1]
    return Result(path, ok, time.time() - start, message)


def summarize_failures(results, limit):
    """Group failure messages so the common blockers surface first."""
    counts = {}
    for result in results:
        if result.ok:
            continue
        # Strip file/line so distinct files with the same cause group together
        key = re.sub(r'^%Error[^:]*: [^ ]+?:\d+:\d+: ', '%Error: ', result.message)
        key = re.sub(r"'[^']*'", "'...'", key)
        counts[key] = counts.get(key, 0) + 1
    return sorted(counts.items(), key=lambda kv: -kv[1])[:limit]


def score_corpus(name, root, globs, args):
    """Run one corpus and print its line. Returns its report entry, or None."""
    if not os.path.isdir(root):
        print('%-16s  -- not found at %s' % (name, root))
        return None
    sources = find_sources(root, globs)
    if args.limit:
        sources = sources[:args.limit]
    if not sources:
        print('%-16s  -- no VHDL sources under %s' % (name, root))
        return None

    work = [(args.verilator_bin, args.flag_list, args.timeout, path) for path in sources]
    started = time.time()
    if args.jobs > 1:
        with multiprocessing.Pool(args.jobs) as pool:
            results = pool.map(run_one, work)
    else:
        results = [run_one(item) for item in work]
    elapsed = time.time() - started

    passed = sum(1 for r in results if r.ok)
    total = len(results)
    percent = (100.0 * passed / total) if total else 0.0
    print('%-16s  %5d/%-5d  %5.1f%%   (%.1fs)' % (name, passed, total, percent, elapsed))

    for message, count in summarize_failures(results, args.failures):
        print('%-16s      %4dx  %s' % ('', count, message[:120]))
    if args.verbose:
        for result in results:
            if not result.ok:
                print('%-16s      FAIL %s' % ('', os.path.relpath(result.path, root)))

    return {
        'root':
        root,
        'passed':
        passed,
        'total':
        total,
        'percent':
        round(percent, 2),
        'seconds':
        round(elapsed, 2),
        'failures': [{
            'file': os.path.relpath(r.path, root),
            'message': r.message
        } for r in results if not r.ok],
    }


def main():
    parser = argparse.ArgumentParser(
        description='Score the VHDL frontend against external VHDL corpora.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__)
    parser.add_argument('--root',
                        default=os.environ.get('VHDL_CORPUS_ROOT', '..'),
                        help='directory holding the corpus checkouts (default: ..)')
    parser.add_argument('--corpus',
                        action='append',
                        default=[],
                        metavar='NAME[=PATH]',
                        help='limit to this corpus, optionally overriding its path;'
                        ' repeatable. Default is every corpus found.')
    parser.add_argument('--verilator',
                        default=None,
                        help='verilator binary (default: ./bin/verilator relative to repo)')
    parser.add_argument('--flags',
                        default='--lint-only',
                        help='flags passed to verilator (default: --lint-only)')
    parser.add_argument('--jobs',
                        type=int,
                        default=multiprocessing.cpu_count(),
                        help='parallel jobs (default: number of cores)')
    parser.add_argument('--timeout', type=float, default=60.0, help='per-file timeout in seconds')
    parser.add_argument('--limit',
                        type=int,
                        default=0,
                        help='only run the first N files per corpus')
    parser.add_argument('--json', default=None, help='also write results to this JSON file')
    parser.add_argument('--failures',
                        type=int,
                        default=10,
                        help='show this many distinct failure causes per corpus')
    parser.add_argument('--verbose', action='store_true', help='list each failing file')
    args = parser.parse_args()

    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    verilator = args.verilator or os.path.join(repo, 'bin', 'verilator')
    if not os.path.exists(verilator):
        sys.exit('%%Error: no verilator binary at %s (build first, or pass --verilator)' %
                 verilator)
    os.environ.setdefault('VERILATOR_ROOT', repo)

    # Resolve which corpora to score
    overrides = {}
    selected = []
    for entry in args.corpus:
        name, _, path = entry.partition('=')
        if name not in CORPORA:
            sys.exit('%%Error: unknown corpus %r; known: %s' % (name, ', '.join(sorted(CORPORA))))
        selected.append(name)
        if path:
            overrides[name] = path
    if not selected:
        selected = sorted(CORPORA)

    # Stash the resolved binary and flags on args, so score_corpus takes one context
    args.verilator_bin = verilator
    args.flag_list = args.flags.split()
    grand_total = 0
    grand_pass = 0
    report = {}

    for name in selected:
        subdir, globs = CORPORA[name]
        root = overrides.get(name, os.path.join(args.root, subdir))
        entry = score_corpus(name, root, globs, args)
        if entry:
            report[name] = entry
            grand_total += entry['total']
            grand_pass += entry['passed']

    percent = (100.0 * grand_pass / grand_total) if grand_total else 0.0
    print('%-16s  %5d/%-5d  %5.1f%%' % ('TOTAL', grand_pass, grand_total, percent))

    if args.json:
        with open(args.json, 'w', encoding='utf-8') as fh:
            json.dump({'flags': args.flags, 'corpora': report}, fh, indent=2, sort_keys=True)
        print('Wrote %s' % args.json)
    return 0


if __name__ == '__main__':
    sys.exit(main())

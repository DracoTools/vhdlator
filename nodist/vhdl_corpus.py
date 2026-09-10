#!/usr/bin/env python3
# pylint: disable=C0114,C0116,C0209,R0902,R0903,R0912,R0913,R0914
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
import signal
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
    'ghdl': ('ghdl', ['testsuite/gna', 'testsuite/sanity', 'testsuite/synth']),
    'vests': ('ghdl', ['testsuite/vests']),
    'compliance': ('Compliance-Tests', ['vhdl_2008', 'vhdl_2019']),
    'uvvm': ('UVVM', ['.']),
    'vunit': ('vunit', ['vunit/vhdl']),
}

VHDL_SUFFIXES = ('.vhd', '.vhdl')

# Corpora whose files are not self-contained: each source names a library the
# corpus compiles alongside it -- VUnit's own, UVVM's, OSVVM's -- so running
# one file on its own reports a missing design unit whatever the frontend
# does with the language. The number for these is a floor and nothing more,
# and saying so is better than printing a percentage that means nothing.
NEEDS_LIBRARY = frozenset(('compliance', 'uvvm', 'vunit'))

# A corpus that says what it expects of each file has to be scored against
# that and not against "did it compile".  VESTS declares the expectation in
# its own directory layout: a compliant case must be accepted, and a
# non-compliant one must be *rejected*, so counting an accepted file as a
# success is exactly backwards for the 1,500-odd cases whose whole point is
# to be refused.  Where a corpus does not declare an expectation, acceptance
# is the only thing to measure, and that is what is reported.
EXPECT_RULES = (
    (re.compile(r'/vests/.*/disputed/'), 'skip'),
    (re.compile(r'/vests/vhdl-ams/'), 'skip'),  # VHDL-AMS is a different language
    (re.compile(r'/vests/.*/non_compliant/'), 'reject'),
    (re.compile(r'/vests/.*/compliant/'), 'accept'),
    (re.compile(r'/vests/.*/iofiles/'), 'skip'),
)

# Sources a corpus writes against an older revision than the default
PER_PATH_FLAGS = ((re.compile(r'/vests/vhdl-93/'), ['--vhdl-std', '93']), )


# NVC's phase corpora are half deliberately-illegal source, and NVC declares
# which half in its own C test harness rather than in the directory layout: a
# test that lists expected diagnostics is a source that must be refused.  Read
# that, so those corpora are scored the way their author scores them.
NVC_PHASE_RE = re.compile(r'(.*/nvc)/test/(parse|sem|bounds|simp)/[^/]+$')
_NVC_EXPECT = {}


def nvc_expectations(nvc_root, phase):
    """Map absolute source path -> 'accept' or 'reject' for one NVC phase."""
    key = (nvc_root, phase)
    if key in _NVC_EXPECT:
        return _NVC_EXPECT[key]
    table = {}
    harness = os.path.join(nvc_root, 'test', 'test_%s.c' % phase)
    try:
        with open(harness, encoding='utf-8', errors='replace') as fh:
            text = fh.read()
    except OSError:
        _NVC_EXPECT[key] = table
        return table
    for chunk in text.split('START_TEST(')[1:]:
        body = chunk.split('END_TEST')[0]
        # An entry of the form '{ 20, "..." }' is a diagnostic the source must
        # produce; the list always ends '{ -1, NULL }', which is not one
        expects = re.search(r'error_t\s+\w*expect\w*\s*\[\]\s*=\s*\{(.*?)\n\s*\};', body,
                            re.S)
        wants_error = bool(expects and re.search(r'\{\s*\d+\s*,', expects.group(1)))
        for name in re.findall(r'input_from_file\(TESTDIR\s*"([^"]+)"', body):
            full = os.path.normpath(os.path.join(nvc_root, 'test', name.lstrip('/')))
            # An entry may be a warning as readily as an error, so what NVC
            # asks of the source is a diagnostic, not necessarily a refusal
            if wants_error or table.get(full) == 'diagnose':
                table[full] = 'diagnose'
            else:
                table.setdefault(full, 'accept')
    _NVC_EXPECT[key] = table
    return table


# GHDL's own testsuite says what it expects of each source in a shell script
# beside it: "analyze foo.vhdl" must be accepted, "analyze_failure foo.vhdl"
# must be refused. Read that for the same reason VESTS' layout is read -- a
# directory of bug reproducers is half deliberately-illegal source, and
# counting clean compiles scores that half backwards.
GHDL_VERB_RE = re.compile(r'^\s*(analyze_failure|analyze)\b(.*)$', re.M)
_GHDL_EXPECT = {}


def ghdl_expectations(directory):
    """Map basename -> 'accept' or 'reject' for one GHDL testsuite directory."""
    if directory in _GHDL_EXPECT:
        return _GHDL_EXPECT[directory][0]
    table = {}
    order = []
    try:
        with open(os.path.join(directory, 'testsuite.sh'), encoding='utf-8',
                  errors='replace') as fh:
            text = fh.read()
    except OSError:
        _GHDL_EXPECT[directory] = (table, order)
        return table
    for verb, rest in GHDL_VERB_RE.findall(text):
        outcome = 'reject' if verb == 'analyze_failure' else 'accept'
        for word in rest.split():
            if word.endswith(VHDL_SUFFIXES):
                if word not in order:
                    order.append(word)
                # A source named both ways must be refused: the failing use
                # is the one the suite is checking
                if outcome == 'reject' or table.get(word) != 'reject':
                    table[word] = outcome
    # Some of these tests write their own sources before analyzing them --
    # "cp pkga_v1.vhdl pkga.vhdl", or a sed that patches one. The checkout
    # alone cannot reproduce the sequence, so the whole directory is left out
    # rather than measured against files that are not there.
    for word in order:
        if not os.path.exists(os.path.join(directory, word)):
            table = {}
            order = []
            break
    _GHDL_EXPECT[directory] = (table, order)
    return table


def ghdl_companions(path):
    """The sources GHDL's script analyzes before this one, and expects to hold.

    A test of several files is analyzed into one library in the order the
    script gives, so running one file alone reports a missing design unit
    whatever the frontend does with the language. The ones the script expects
    to *fail* are left out: they are there to be refused, and their errors
    would be read as this file's.
    """
    directory = os.path.dirname(path)
    table = ghdl_expectations(directory)
    if not table:
        return []
    order = _GHDL_EXPECT[directory][1]
    name = os.path.basename(path)
    out = []
    for other in order:
        if other == name:
            break
        if table.get(other) == 'accept':
            out.append(os.path.join(directory, other))
    return out


def expected_outcome(path):
    """Return 'accept', 'reject' or 'skip' for one source."""
    for pattern, outcome in EXPECT_RULES:
        if pattern.search(path):
            return outcome
    match = NVC_PHASE_RE.match(path)
    if match:
        table = nvc_expectations(match.group(1), match.group(2))
        # A source no test names is not part of the suite's own scoring
        return table.get(os.path.normpath(path), 'skip')
    if '/ghdl/testsuite/' in path:
        directory = os.path.dirname(path)
        if os.path.exists(os.path.join(directory, 'testsuite.sh')):
            # A directory whose script cannot be followed is not scored at all
            table = ghdl_expectations(directory)
            return table.get(os.path.basename(path), 'skip') if table else 'skip'
    return 'accept'


def extra_flags(path):
    """Return flags this source needs beyond the common ones."""
    for pattern, flags in PER_PATH_FLAGS:
        if pattern.search(path):
            return flags
    return []


class Result:
    """Outcome of running Verilator over one source file."""

    __slots__ = ('path', 'ok', 'seconds', 'message', 'expected', 'diagnosed')

    def __init__(self, path, ok, seconds, message, expected='accept', diagnosed=False):
        self.path = path
        self.ok = ok
        self.seconds = seconds
        self.message = message
        self.expected = expected
        self.diagnosed = diagnosed

    @property
    def as_expected(self):
        """Whether the run came out the way the corpus says it should."""
        if self.expected == 'accept':
            return self.ok
        if self.expected == 'diagnose':
            return self.diagnosed
        return not self.ok


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


def run_group(command, timeout):
    """Run a command in a process group of its own, and on timeout kill the
    whole group.  bin/verilator is a wrapper that runs verilator_bin as a
    child, so killing only the process started would leave the compiler
    running -- for hours, and holding its memory, on a source that hangs it."""
    proc = subprocess.Popen(  # pylint: disable=consider-using-with
        command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, start_new_session=True)
    try:
        stdout, _ = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except OSError:
            pass
        proc.communicate()
        raise
    return stdout, proc.returncode


def run_one(args):
    """Run Verilator over one source. Returns a Result."""
    verilator, flags, timeout, path = args
    expected = expected_outcome(path)
    command = ([verilator] + flags + extra_flags(path)
               + (ghdl_companions(path) if '/ghdl/testsuite/' in path else []) + [path])
    start = time.time()
    try:
        stdout, returncode = run_group(command, timeout)
        output = stdout.decode('utf-8', errors='replace')
        ok = returncode == 0
        diagnosed = any(
            line.startswith('%Error') or line.startswith('%Warning')
            for line in output.splitlines())
    except subprocess.TimeoutExpired:
        return Result(path, False, time.time() - start, 'TIMEOUT', expected, False)
    except OSError as exc:
        return Result(path, False, time.time() - start, 'EXEC: %s' % exc, expected, False)

    message = ''
    if not ok:
        # First %Error line is the most informative summary of the failure
        for line in output.splitlines():
            if line.startswith('%Error'):
                message = line.strip()
                break
        else:
            message = (output.strip().splitlines() or ['(no output)'])[-1]
    return Result(path, ok, time.time() - start, message, expected, diagnosed)


def summarize_failures(results, limit):
    """Group failure messages so the common blockers surface first."""
    counts = {}
    for result in results:
        if result.as_expected:
            continue
        if result.expected in ('reject', 'diagnose'):
            key = ('(accepted, but the corpus says this must be refused)'
                   if result.expected == 'reject' else
                   '(nothing reported, but the corpus expects a diagnostic here)')
            counts[key] = counts.get(key, 0) + 1
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
    sources = [p for p in find_sources(root, globs) if expected_outcome(p) != 'skip']
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

    passed = sum(1 for r in results if r.as_expected)
    total = len(results)
    must_reject = sum(1 for r in results if r.expected in ('reject', 'diagnose'))
    wrongly_accepted = sum(1 for r in results if r.expected in ('reject', 'diagnose')
                           and not r.as_expected)
    # VESTS says a non-compliant case must not compile; NVC's harness lists
    # warnings as readily as errors, so what it asks for is a diagnostic
    demand = 'diagnosed' if any(r.expected == 'diagnose' for r in results) else 'refused'
    percent = (100.0 * passed / total) if total else 0.0
    note = ''
    if must_reject:
        note = '   [%d must be %s, %d were not]' % (must_reject, demand, wrongly_accepted)
    if name in NEEDS_LIBRARY:
        note += '   [a floor: each file needs its library compiled alongside]'
    print('%-16s  %5d/%-5d  %5.1f%%   (%.1fs)%s' %
          (name, passed, total, percent, elapsed, note))

    for message, count in summarize_failures(results, args.failures):
        print('%-16s      %4dx  %s' % ('', count, message[:120]))
    if args.verbose:
        for result in results:
            if not result.as_expected:
                print('%-16s      FAIL %s' % ('', os.path.relpath(result.path, root)))

    return {
        'root':
        root,
        'passed':
        passed,
        'total':
        total,
        'must_reject':
        must_reject,
        'wrongly_accepted':
        wrongly_accepted,
        'percent':
        round(percent, 2),
        'seconds':
        round(elapsed, 2),
        'failures': [{
            'file': os.path.relpath(r.path, root),
            'expected': r.expected,
            'message': r.message or '(accepted, but must be refused)'
        } for r in results if not r.as_expected],
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
    # A warning is Verilator's opinion of a design, not a verdict on whether the
    # source is legal, and no VHDL corpus is written to avoid them. Scoring one
    # as a rejection would count a lint remark as a conformance failure, so
    # warnings are not fatal here; errors still are.
    parser.add_argument('--flags',
                        default='--lint-only -Wno-fatal',
                        help='flags passed to verilator (default: --lint-only -Wno-fatal)')
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
            entry['needs_library'] = name in NEEDS_LIBRARY
            report[name] = entry
            if name in NEEDS_LIBRARY:
                continue  # A floor, so leave it out of the total rather than depress it
            grand_total += entry['total']
            grand_pass += entry['passed']

    percent = (100.0 * grand_pass / grand_total) if grand_total else 0.0
    print('%-16s  %5d/%-5d  %5.1f%%   (corpora needing their library left out)' %
          ('TOTAL', grand_pass, grand_total, percent))

    if args.json:
        with open(args.json, 'w', encoding='utf-8') as fh:
            json.dump({'flags': args.flags, 'corpora': report}, fh, indent=2, sort_keys=True)
        print('Wrote %s' % args.json)
    return 0


if __name__ == '__main__':
    sys.exit(main())

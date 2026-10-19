..
   SPDX-FileCopyrightText: 2003-2026 Wilson Snyder
   SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

***************
Input Languages
***************

This section describes the languages Verilator takes as input. See also
:ref:`Verilator Control Files`.


Language Standard Support
=========================

Verilog 2001 (IEEE 1364-2001) Support
-------------------------------------

Verilator supports most Verilog 2001 language features. This includes
signed numbers, "always @\*", generate statements, multidimensional arrays,
localparam, and C-style declarations inside port lists.


Verilog 2005 (IEEE 1364-2005) Support
-------------------------------------

Verilator supports most Verilog 2005 language features. This includes the
\`begin_keywords and \`end_keywords compiler directives, $clog2, and the
uwire keyword.


SystemVerilog 2005 (IEEE 1800-2005) Support
-------------------------------------------

Verilator supports ==? and !=? operators, ++ and -- in some contexts,
$bits, $countbits, $countones, $error, $fatal, $info, $isunknown, $onehot,
$onehot0, $unit, $warning, always_comb, always_ff, always_latch, bit, byte,
chandle, const, do-while, enum, export, final, import, int, interface,
logic, longint, modport, package, program, shortint, struct, time, typedef,
union, var, void, priority case/if, and unique case/if.


SystemVerilog 2012 (IEEE 1800-2012) Support
-------------------------------------------

Verilator implements a full SystemVerilog-compliant preprocessor, including
function call-like preprocessor defines, default define arguments,
\`__FILE__, \`__LINE__ and \`undefineall.


SystemVerilog 2017 (IEEE 1800-2017) Support
-------------------------------------------

Verilator supports the 2017 "for" loop constructs and several cleanups IEEE
made in 1800-2017.


SystemVerilog 2023 (IEEE 1800-2023) Support
-------------------------------------------

Verilator supports some of the 2023 improvements, including triple-quoted
string blocks that may include newlines and single quotes.

Verilator implements a full IEEE 1800-2023 compliant preprocessor,
including triple-quoted strings, and \`ifdef expressions.


VHDL (IEEE 1076) Support
------------------------

Verilator reads VHDL as well as Verilog. Files with a :file:`.vhd` or
:file:`.vhdl` extension are taken as VHDL.

.. code-block:: bash

     verilator --binary --timing counter.vhd --top counter

VHDL-2008 is the default, and the revision this support is built for.
:vlopt:`--vhdl-std` selects another one: ``1993``, ``2000``, ``2002`` or
``2019``. An earlier revision is not a separate implementation: it reserves
that revision's words, refuses what that revision forbids and VHDL-2008
allows, and hides from a unit compiled under it what VHDL-2008 added to
``STANDARD``, ``TEXTIO`` and the IEEE packages. ``93c`` is VHDL-93 with the
relaxations GHDL's default mode of that name allows. VHDL-2019 is supported
in part: its reserved words and conditional analysis, but not yet most of
what it adds to the language (see :ref:`VHDL Limitations`).

Conditional analysis (VHDL-2019 24.2, which VHDL-2008 tools commonly accept
too) is applied in every revision: ```if``, ```elsif``, ```else``, ```end``,
```warning`` and ```error``, over the standard identifiers
``VHDL_VERSION``, ``TOOL_TYPE``, ``TOOL_VENDOR``, ``TOOL_NAME``,
``TOOL_EDITION`` and ``TOOL_VERSION``, and over identifiers defined with
:vlopt:`-D <-D<var>>`.

Nothing else is needed to use the standard libraries. ``library std`` is
always available, and a context or use clause naming a package of ``library
ieee``, such as ``ieee.numeric_std``, causes Verilator to read the IEEE
package it ships and whatever that package depends on. Pass
:vlopt:`--no-std-vhdl` to supply your own copies instead. Verilator ships
``std.standard``, ``std.textio`` and ``std.env``; IEEE's ``std_logic_1164``,
``numeric_std``, ``numeric_bit``, ``numeric_std_unsigned``,
``numeric_bit_unsigned``, ``fixed_pkg``, ``float_pkg``, ``math_real`` and
``math_complex``, with the two context declarations ``ieee_std_context`` and
``ieee_bit_context``; and Synopsys' ``std_logic_arith``,
``std_logic_unsigned``, ``std_logic_signed``, ``std_logic_misc`` and
``std_logic_textio``.

Supported are entities and architectures, packages and package bodies,
context declarations, configurations, all the standard type classes
(enumeration, integer, floating point, physical, array, record, access,
file and protected), shared variables, subprograms with all four parameter
classes, generic types, subprograms and packages, resolution functions and
multiple drivers, all the concurrent and sequential statements, blocks,
guarded signals, ``for``/``if``/``case`` generate statements, component and
direct entity instantiation, external names, aliases and attributes.
Verification libraries built on these run: UVVM, VUnit and OSVVM.

``std_ulogic`` keeps its nine values by default, each element held in four
bits, and the IEEE packages, resolution and edge tests behave as in other
VHDL simulators. :vlopt:`--no-vhdl-metavalues` holds one bit per element
instead, which roughly halves the run time and loses what ``'U'``,
``'X'``, ``'W'`` and ``'-'`` say (see :ref:`VHDL Limitations`).

VHDL makes many values an error when they are evaluated: an index outside
its array, a value outside its subtype, a length that does not match, a
division by zero. The model checks them as it runs and stops where other
VHDL simulators stop; an error that elaboration would find stops it when it
starts. :vlopt:`--vhdl-bound-check` lists the checks, and
:vlopt:`--no-vhdl-bound-check` drops them for speed.

A VHDL design that suspends -- a process with no sensitivity list, ``wait``
in any of its forms, or an ``after`` delay -- needs :vlopt:`--timing`, the
same as the equivalent Verilog does. Synthesizable VHDL, where every process
has a sensitivity list, does not.

VHDL is not case sensitive, so a signal declared ``Count`` is the same
signal as ``COUNT``. Verilator lowercases basic identifiers, and that is the
spelling that appears in traces, in the generated C++, and to VPI. Extended
identifiers, written between backslashes, keep their case and are distinct
from every basic identifier. Wherever a name is written on the command line
-- :vlopt:`-G <-G<name>>` and :vlopt:`--top-module` -- any spelling of it is accepted.

A generic of the top-level entity takes its value from :vlopt:`-G <-G<name>>`, which
saves editing the source to try another configuration. A string generic needs
its quotes to survive the shell:

.. code-block:: bash

     verilator --binary --timing fifo.vhd --top fifo \
         -GWIDTH=32 -GDEPTH=256 -GNAME='"fifo0"'

A file declaration that names its file -- ``file f : text open write_mode is
"name";`` -- opens it as the declaration is elaborated: once for an
architecture, a block or a process, and again on every call for a subprogram.
The name is an ordinary expression, so a design that wants to write beside
its model can take the directory as a generic rather than hard-coding it.

VHDL has no ``$dumpfile``, so a design cannot ask for a waveform from inside
itself the way a Verilog testbench does. Given :vlopt:`--trace-vcd` (or
:vlopt:`--trace-fst`), Verilator asks on the design's behalf: the model
writes every signal, from the top down, to :file:`{obj_dir}/{prefix}.vcd`
beside its other output. A design that wants the file elsewhere, or wants to
start and stop tracing, can do that from its own C++ main with
:vlopt:`--exe` as any Verilator model can.

A VHDL design is reached over VPI as other VHDL simulators present one, which
is what cocotb drives, and a subprogram declared with GHDL's ``VHPIDIRECT``
form of the ``FOREIGN`` attribute calls C as a DPI import does.

See :ref:`VHDL Limitations` for where the compiled model diverges from
the LRM, and :doc:`mixed-language` for instantiating VHDL from Verilog and
Verilog from VHDL.

Verilog AMS Support
-------------------

Verilator implements a very small subset of Verilog AMS (Verilog Analog and
Mixed-Signal Extensions) with the subset corresponding to those VMS
keywords with near-equivalents in Verilog IEEE 1364 or SystemVerilog IEEE
1800.

AMS parsing is enabled with :vlopt:`--language VAMS <--language>` or
:vlopt:`--language 1800+VAMS <--language>`.

Verilator implements ceil, exp, floor, ln, log, pow, sqrt, string, and
wreal.


Synthesis Directive Assertion Support
-------------------------------------

Verilator reads any ``//synopsys full_case`` or ``//synopsys
parallel_case`` directives. The same applies to any ``//ambit synthesis``,
``//cadence`` or ``//pragma`` directives of the same form.

When these synthesis directives are discovered, unless
:vlopt:`--no-assert-case` option is used, Verilator will either formally
prove the directive to be true, or, failing that, will insert the
appropriate code to detect failing cases at simulation runtime and print an
"Assertion failed" error message.

Verilator likewise also asserts any "unique" or "priority" SystemVerilog
keywords on case statements, as well as "unique" on if statements. However,
"priority if" is currently ignored.


Time
====

With :vlopt:`--timing`, all timing controls are supported:

- delay statements,
- event control statements not only at the top of a process,
- intra-assignment timing controls,
- net delays,
- ``wait`` statements,

as well as all flavors of ``fork``.

Compiling a Verilated design that uses these features requires a compiler
with C++20 coroutine support, e.g. Clang 5, GCC 10, or newer.

Rising/falling/turn-off delays are currently unsupported and cause the
:option:`RISEFALLDLY` warning.

Minimum/typical/maximum delays are currently unsupported. The typical delay
is always the one chosen. Such expressions cause the :option:`MINTYPMAXDLY`
warning.

Another consequence of using :vlopt:`--timing` is that the :vlopt:`--main`
option generates a main file with a proper timing eval loop, eliminating
the need for writing any driving C++ code. You can simply compile the
simulation (perhaps using :vlopt:`--build`) and run it.

With :vlopt:`--no-timing`, all timing controls cause the :option:`NOTIMING`
error, except:

- delay statements - they are ignored (as they are in synthesis), though
  they do issue a :option:`STMTDLY` warning,
- intra-assignment timing controls - they are ignored, though they do issue
  an :option:`ASSIGNDLY` warning,
- net delays - they are ignored,
- event controls at the top of the procedure,

Forks cause this error as well, except:

- forks with no statements,
- ``fork..join`` or ``fork..join_any`` with one statement,
- forks with :vlopt:`--bbox-unsup`.

If neither :vlopt:`--timing` nor :vlopt:`--no-timing` is specified, all
timing controls cause the :option:`NEEDTIMINGOPT` error, except event
controls at the top of the process. Forks cause this error as well, except:

- forks with no statements,
- ``fork..join`` or ``fork..join_any`` with one statement,
- forks with :vlopt:`--bbox-unsup`.

Timing controls and forks can also be ignored in specific files or parts of
files. The :option:`/*verilator&32;timing_off*/` and
:option:`/*verilator&32;timing_off*/` metacomments will make Verilator
ignore the encompassed timing controls and forks, regardless of the chosen
:vlopt:`--timing` or :vlopt:`--no-timing` option. This can also be achieved
using the :option:`timing_off` and :option:`timing_off` options in
Verilator Control Files.


.. _language limitations:

Language Limitations
====================

This section describes the language limitations of Verilator. Many of these
restrictions are by intent.

Synthesis Subset
----------------

Verilator supports the Synthesis subset with other verification constructs
being added over time. Verilator also simulates events as Synopsys's Design
Compiler would, namely given a block of the form:

.. code-block:: sv

   always @(x) y = x & z;

This will recompute y when there is a potential for change in x or a change
in z; that is when the flops computing x or z evaluate (which is what
Design Compiler will synthesize.) A compliant simulator will only calculate
y if x changes. We recommend using always_comb to make the code run the
same everywhere. Also avoid putting $displays in combo blocks, as they may
print multiple times when not desired, even on compliant simulators as
event ordering is not specified.


Signal Naming
-------------

To avoid conflicts with C symbol naming, any character in a signal name
that is not alphanumeric nor a single underscore will be replaced by
`__0hh` where `hh` is the hex code of the character. To avoid conflicts
with Verilator's internal symbols, any double underscore is replaced with
`___05F` (5F is the hex code of an underscore.)


Bind
----

Verilator only supports bind to a target module name, not to an instance
path.


Class
-----

Verilator class support is limited but in active development. Verilator
supports members, methods, class extend, and class parameters.


Dotted cross-hierarchy references
---------------------------------

Verilator supports dotted references to variables, functions, and tasks in
different modules. The portion before the dot must have a constant value;
for example a[2].b is acceptable, while a[x].b is generally not.

References into generated and arrayed instances use the instance names
specified in the Verilog standard; arrayed instances are named
``{instanceName}[{instanceNumber}]`` in Verilog, which becomes
``{instanceName}__BRA__{instanceNumber}__KET__`` inside the generated C++
code.


Latches
-------

Verilator is optimized for edge-sensitive (flop-based) designs. It will
attempt to do the correct thing for latches, but most performance
optimizations will be disabled around the latch.


Structures and Unions
---------------------

All structures and unions are scheduled together, which means that
generating one member of a structure from blocking, and another from
non-blocking assignments is unsupported.


.. _unknown states:

Unknown States
--------------

Verilator is mostly a two-state simulator, not a four-state simulator.
However, it has two features that uncover most initialization bugs
(including many that a four-state simulator will miss.)

Identity comparisons (=== or !==) are converted to standard ==/!= when
neither side is a constant. This may make the expression yield a different
result than a four-state simulator. An === comparison to X will always be
false, so that Verilog code which checks for uninitialized logic will not
fire.

Assigning X to a variable will assign a constant value as determined by the
:vlopt:`--x-assign` option. This allows runtime randomization; thus, if the
value is used, the random value should cause downstream errors. Integers
also get randomized, even though the Verilog 2001 specification says they
initialize to zero. However, randomization happens at initialization time;
hence, during a single simulation run, the same constant (but random) value
will be used every time the assignment is executed.

All variables, depending on :vlopt:`--x-initial` setting, are typically
randomly initialized using a function. You can determine that reset is
working correctly by running several random simulation runs. On the first
run, have the function initialize variables to zero. On the second, have it
initialize variables to one. On the third and following runs, have it
initialize them randomly. If the results match, reset works. (Note that
this is what the hardware will do.) In practice, setting all variables to
one at startup finds the most problems (since control signals are typically
active-high).

:vlopt:`--x-assign` applies to variables explicitly initialized or assigned
an X. Uninitialized clocks are initialized to zero, while all other state
holding variables are initialized to a random value. Event-driven
simulators will generally trigger an edge on a transition from X to 1
(posedge) or X to 0 (negedge). However, by default, since clocks are
initialized to zero, Verilator will not trigger an initial negedge. Some
code (particularly for reset) may rely on X->0 triggering an edge. The
:vlopt:`--x-initial-edge` option enables this behavior. Comparing runs with
and without this option will find such problems.


Tri/Inout
---------

Verilator converts some simple tristate structures into two state. Pullup,
pulldown, bufif0, bufif1, notif0, notif1, pmos, nmos, tri0 and tri1 are
also supported. Simple comparisons with ``=== 1'bz`` are also supported.

An assignment of the form:

.. code-block:: sv

   inout driver;
   wire driver = (enable) ? output_value : 1'bz;

Will be converted to:

.. code-block:: sv

   input driver;        // Value being driven in from "external" drivers
   output driver__en;   // True if driven from this module
   output driver__out;  // Value being driven from this module

External logic will be needed to combine these signals with any external
drivers.

Tristate drivers are not supported inside functions and tasks; an inout
there will be considered a two-state variable that is read and written
instead of a four-state variable.


Gate Primitives
---------------

The 2-state gate primitives (and, buf, nand, nor, not, or, xnor, xor) are
directly converted to behavioral equivalents. The 3-state and MOS gate
primitives are not supported. User-defined primitive (UDP) tables are
supported.


Specify blocks
--------------

All specify blocks and timing checks are ignored. All min:typ:max delays
use the typical value.


Array Initialization
--------------------

When initializing a large array, you need to use non-delayed assignments.
Verilator will tell you when this needs to be fixed; see the BLKLOOPINIT
error for more information.


Array Out of Bounds
-------------------

Writing a memory element outside the bounds specified for the array may
cause a different memory element inside the array to be written instead.
For power-of-2 sized arrays, Verilator will give a width warning and the
address. For non-power-of-2-sizes arrays, index 0 will be written.

Reading a memory element outside the bounds specified for the array will
give a width warning and wrap around the power-of-2 size. For
non-power-of-2 sizes, it will return an unspecified constant of the
appropriate width.


Assertions
----------

Verilator partially supports assertions and assertion-driven functional
coverage.

Coverage
--------

Verilator partially supports SystemVerilog functional coverage with
``covergroup``, ``coverpoint``, bins, cross coverage, and transition bins.
See :ref:`Covergroup Coverage`.


.. _VHDL Limitations:

VHDL Limitations
----------------

Verilator compiles VHDL to the same statically scheduled model it compiles
Verilog to. That is where the speed comes from. Where the compiled model
differs from what the LRM says, it is listed here, so that a design that
depends on one of these can be recognized as such.

**What is not supported.** Most of what VHDL-2019 adds: interface mode
views, the reflection API (``value_mirror``, ``subtype_mirror``), the
additions to ``std.env``, the assertion and report control API, the new
generic type classes, protected types as parameters and their ``private``
part, conditional expressions, sequential blocks, named return values, and
the predefined ``TO_STRING`` and ``'VALUE`` of composite types. Nor PSL,
VHPI, or the VITAL timing libraries. Each is reported as unsupported where
it is written.

**Metavalues under --no-vhdl-metavalues.** With
:vlopt:`--no-vhdl-metavalues` an element of ``std_ulogic`` is one bit:
``'1'`` and ``'H'`` are 1, ``'0'`` and ``'L'`` are 0, ``'Z'`` is high
impedance, and ``'U'``, ``'X'``, ``'W'`` and ``'-'`` are 0. So a comparison
with ``'U'`` is never true, ``numeric_std`` gives no ``'X'`` results and no
``metavalue detected`` warnings, and an uninitialized signal reads as
``'0'``. A record signal holding ``'Z'`` in one of its elements is refused
in that mode. The default, nine values, has none of these limits; it does
not yet support an array of resolved vectors that several instances drive
through ``inout`` ports, which two states resolve.

**A delta cycle is observable; the delta count is not.** A signal assignment
schedules an update, and each delta cycle is a pass of the scheduler, so a
process sees exactly the values VHDL says it sees, ``wait for 0 ns`` resumes
after one delta, and a postponed process runs after the last one. Logic that
no process can observe between deltas is evaluated combinationally, which no
process can tell apart. The number of deltas a time step took is not
counted, and a message reports the time without it.

**The projected waveform of some signals keeps every transaction.**
Inertial delay, ``reject`` and the preemption of transactions already
scheduled follow the LRM (10.5.2.2) for scalars, vectors, records of up to
64 scalar elements and resolved signals, element by element. An array of
arrays, a record of more than 64 scalar elements, a resolved signal reached
through a procedure's signal parameter, and a concurrent assignment to a
resolved signal deliver every transaction they are given.

**Some run-time errors are not detected.** :vlopt:`--vhdl-bound-check`
lists what is checked. Where nvc, GHDL and Questa do not agree that a run
stops, the majority decides, and so these run on: an INTEGER ``+``, ``-``
or ``*`` that overflows (GHDL and Questa wrap), a REAL converted to an
INTEGER it does not fit (GHDL and Questa carry on), and a qualified
expression whose operand only running decides (only Questa checks it).
Also unchecked: a length mismatch through a port map expression or an
external name, and an index into an array whose range names an attribute.

**A string's bounds are its length, where only running decides them.** A
``STRING`` is held as text, which is what makes ``LINE``, ``textio`` and
``'IMAGE`` cheap, and text has a length but no bounds. Bounds that analysis
or elaboration settles are kept; a string declared over a range computed
while the design runs answers ``'LEFT`` with 1. Likewise, what a ``LINE``
holds after a ``READ`` is indexed from 1, where the LRM keeps the indexes
the characters had before.

**An external name climbs one level.** ``<< signal ^.x : t >>`` names an
object of the unit that instantiates this one, where one unit does. A
pathname that climbs further, or out of a unit instantiated from more than
one place, is not supported.

**A guarded signal's disconnection takes effect at once.** A guarded
assignment's driver is disconnected as its block's ``GUARD`` becomes false,
and a ``bus`` or ``register`` signal is resolved without it. A
``disconnect ... after`` specification is accepted, and its delay ignored.

**A transaction is recorded per signal, not per element.** ``S'ACTIVE``,
``S'TRANSACTION``, ``S'QUIET`` and ``S'LAST_ACTIVE`` ask whether a driver
assigned, not whether the value changed, and each signal they name is given
a bit that every assignment to it flips. Assigning one element of a
composite signal flips the whole signal's bit, so ``S(i)'TRANSACTION``
reports a transaction on ``S``.

**Arithmetic on a width only running decides.** Verilator emits
fixed-width operations, so arithmetic whose operand width is only known
while the design runs is refused. This is reachable only by doing
arithmetic on a value whose length came from a run-time computation.

**``'DELAYED`` needs a static delay.** ``S'DELAYED(T)`` becomes a signal of
its own driven from ``S`` through ``T``, which requires ``T`` to be known
when the model is built. The LRM requires ``T`` to be a static expression,
so this refuses only what was already illegal.

**The image of a real is a VHDL literal, but not always the same one.**
``real'image`` gives text that reads back as a VHDL literal -- ``4.0``,
``0.5``, ``-3.25`` -- but which literal is implementation defined, and the
reference simulators do not agree either: GHDL 7.0.0-dev writes ``5.0e-1``
where NVC 1.22.1 writes ``0.5``. The number of digits a value is written to
may differ from both.

Encrypted Verilog
-----------------

Open-source simulators like Verilator cannot use encrypted RTL (i.e. IEEE
P1735). Talk to your IP vendor about delivering IP blocks using Verilator's
:vlopt:`--protect-lib` feature.


Language Keyword Limitations
============================

This section describes specific limitations for each language keyword.

.. Hack to get long definition list:

.. |cmdslong1| replace:: \`__FILE__, \`__LINE__, \`begin_keywords,
   \`begin_keywords, \`begin_keywords, \`begin_keywords, \`begin_keywords,
   \`define, \`else, \`elsif, \`end_keywords, \`endif, \`error, \`ifdef,
   \`ifndef, \`include, \`line, \`systemc_ctor, \`systemc_dtor,
   \`systemc_header, \`systemc_header_post, \`systemc_imp_header,
   \`systemc_implementation, \`systemc_interface, \`undef, \`verilog

|cmdslong1|
   Fully supported.

.. Hack to get long definition list:

.. |cmdslong2| replace:: always, always_comb, always_ff, always_latch, and,
   assign, begin, buf, byte, case, casex, casez, default, defparam,
   do-while, else, end, endcase, endfunction, endgenerate, endmodule,
   endspecify, endtask, final, for, function, generate, genvar, if,
   initial, inout, input, int, integer, localparam, logic, longint,
   macromodule, module, nand, negedge, nor, not, or, output, parameter,
   posedge, reg, scalared, shortint, signed, supply0, supply1, task, time,
   tri, typedef, var, vectored, while, wire, xnor, xor

|cmdslong2|
   Generally supported.

++, -- operators
   Increment/decrement can only be used as standalone statements or in
   certain limited cases.

'{} operator
   Assignment patterns with an order based, default, constant integer
   (array) or member identifier (struct/union) keys are supported. Data
   type keys and keys computed from a constant expression are not
   supported.

\`uselib
   Uselib, a vendor-specific library specification method, is ignored along
   with anything following it until the end of that line.

cast operator
   Casting is supported only between simple scalar types, signed and
   unsigned, not arrays nor structs.

chandle
   Treated as a "longint"; does not yet warn about operations specified as
   illegal on chandles.

checker
   Treated as a "module"; does not yet warn about many constructs illegal
   inside a checker.

disable
   Disable statements may be used only if the block being disabled is a
   block the disable statement itself is inside. This was commonly used to
   provide loop break and continue functionality before SystemVerilog added
   the break and continue keywords.

force, release
   Verilator supports the procedural `force` (and corresponding `release`)
   statement. However, the behavior of the `force` statement does not
   entirely comply with IEEE 1800-2023:

   #. Using forced variable as a value to another force statement is
      currently not supported. The dependent force statement is forced by
      an initial constant value.

   #. Force/release with procedural continuous assignment is not supported.
      The assignment is treated as procedural.

   #. Expressions using multiple variable references or function calls on
      forced right-hand side are not sensitive to dependency changes.

inside
   Inside expressions may not include unpacked array traversal or $ as an
   upper bound. Case inside and case matches are also unsupported.

interface
   Interfaces and modports, including generated data types are supported.
   Generate blocks around modports are not supported, nor are virtual
   interfaces nor unnamed interfaces.

shortreal
   Short floating point (shortreal) numbers are converted to real. Most
   other simulators either do not support float, or convert likewise.

specify specparam
   All timing checks and specify blocks (except specparam, which is
   supported) are ignored.

uwire
   Verilator does not perform warning checking on uwires; it treats the
   uwire keyword as if it were the normal wire keyword.

$bits, $countbits, $countones, $finish, $isunknown, $onehot, $onehot0, $signed, $stime, $stop, $time, $unsigned,
   Generally supported.

$dump/$dumpports and related
   $dumpfile or $dumpports will create a VCD or FST file (based on the
   :vlopt:`--trace-vcd` option given when the model was Verilated). This
   will take effect starting at the next eval() call. If you have multiple
   Verilated designs under the same C model, this will dump signals only
   from the design containing the $dumpvars.

   $dumpvars and $dumpports module identifier is ignored; the traced
   instances will always start at the top of the design. The levels
   argument is also ignored; use tracing_on/tracing_off pragmas instead.

   $dumpportson/$dumpportsoff/$dumpportsall/$dumpportslimit filename
   argument is ignored; only a single trace file may be active at once.

   $dumpall/$dumpportsall, $dumpon/$dumpportson, $dumpoff/$dumpportsoff,
   and $dumplimit/$dumpportlimit are currently ignored.

$error, $fatal, $info, $warning.
   Generally supported.

$exit, $finish, $stop
   The rarely used optional parameter to $finish and $stop is ignored;
   $exit is aliased to $finish.

$fopen, $fclose, $fdisplay, $ferror, $feof, $fflush, $fgetc, $fgets, $fscanf, $fwrite, $fscanf, $sscanf
   Generally supported.

$fullskew, $hold, $nochange, $period, $recovery, $recrem, $removal, $setup, $setuphold, $skew, $timeskew, $width
   All specify blocks and timing checks are ignored.

$random, $urandom, $urandom_range
   Use :vlopt:`+verilator+seed+\<value\>` runtime option to set the seed if
   there is no $random nor $urandom optional argument to set the seed.
   There is one random seed per C thread, not per module for $random, nor
   per object for random stability of $urandom/$urandom_range.

$readmemb, $readmemh
   Read memory commands are supported. Verilator and the Verilog
   specification do not include support for readmem to multi-dimensional
   arrays.

$stacktrace
   The `$stacktrace` system call will show the C++ stack, not the Verilog
   call stack, though the function names typically correlate. To get
   symbolic names, the model must have debug symbols, e.g. compile with
   `-CFLAGS -ggdb -LDFLAGS -ggdb -LDFLAGS -rdynamic`.

$test$plusargs, $value$plusargs
   Supported, but the instantiating C++/SystemC wrapper must call

   .. code-block:: C++

      {VerilatedContext*} ->commandArgs(argc, argv);

   to register the command line before calling $test$plusargs or
   $value$plusargs. Or use :vlopt:`--binary` or :vlopt:`--main`.

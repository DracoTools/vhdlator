..
   SPDX-FileCopyrightText: 2003-2026 Wilson Snyder
   SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

**********************
Connecting VHDL to C
**********************

This section is about the ways a VHDL design and C or C++ code reach each
other: VHDL calling C, C calling VHDL, VPI, and cocotb. For a VHDL design
that instantiates Verilog or is instantiated by it, see
:doc:`mixed-language`.

Everything here compiles to one statically linked model. There is no
``dlopen``, no separate foreign-model process, and no interface layer
between the languages: a VHDL foreign subprogram and a SystemVerilog DPI
import become the same thing in the generated code, so one C function can
serve both.


VHDL Calling C
==============

VHDL says that a subprogram is implemented outside the language by giving it
the predefined ``FOREIGN`` attribute (IEEE 1076-2008 clause 16.2). The
attribute is already declared in ``STD.STANDARD``, so nothing needs to be
declared to use it. Its string is implementation defined; Verilator accepts
the form GHDL uses, so existing source needs no change.

.. code-block:: vhdl

     package foreign_pkg is
       function c_popcount (v : integer) return integer;
       attribute foreign of c_popcount : function is "VHPIDIRECT c_popcount";
     end package;

.. code-block:: c

     int c_popcount(int v) {
         int count = 0;
         for (unsigned u = v; u; u >>= 1) count += u & 1;
         return count;
     }

Compile the two together and call the function like any other:

.. code-block:: bash

     verilator --binary --timing design.vhd popcount.c --top-module design

The subprogram has no body in VHDL -- the C function is its body. Giving it
one as well is an error.

Accepted forms of the attribute string:

.. list-table::
   :header-rows: 1

   * - String
     - Meaning
   * - ``"VHPIDIRECT c_popcount"``
     - The C symbol ``c_popcount``. Portable to GHDL, and the recommended form.
   * - ``"VHPIDIRECT libfoo.so c_popcount"``
     - The same. GHDL names a shared library to load; Verilator links
       statically, so the library name has nothing to name and is ignored.
   * - ``"DPI-C c_popcount"``
     - The same C symbol, said in a way that makes plain the function is
       shared with a SystemVerilog testbench. Not portable to other VHDL
       tools.
   * - ``"c_popcount"``
     - A bare symbol name, also accepted.

A **procedure** becomes a C function returning ``void``, with its ``out``
and ``inout`` parameters passed by pointer:

.. code-block:: vhdl

     procedure c_divmod (num : in integer; den : in integer;
                         q : out integer; r : out integer);
     attribute foreign of c_divmod : procedure is "VHPIDIRECT c_divmod";

.. code-block:: c

     void c_divmod(int num, int den, int* q, int* r) {
         *q = num / den;
         *r = num % den;
     }

Verilator writes the prototype it expects into
:file:`{obj_dir}/V{prefix}__Dpi.h`. Including that header in the C file is
the surest way to keep the two sides in step, since the C compiler then
reports a mismatch rather than the program misbehaving at run time.

Parameter and result types map as follows.

.. list-table::
   :header-rows: 1

   * - VHDL
     - C
   * - ``integer``, ``natural``, ``positive``
     - ``int``
   * - ``real``
     - ``double``
   * - ``boolean``, ``std_logic``, ``bit``
     - ``svLogic`` (an ``unsigned char``)
   * - ``std_logic_vector``, ``unsigned``, ``signed``, up to 32 bits
     - ``svBitVecVal`` (a ``uint32_t``)
   * - ``std_logic_vector`` wider than 32 bits
     - ``svBitVecVal*``, least significant word first
   * - ``string``
     - ``const char*``
   * - ``out`` or ``inout`` parameter of any of the above
     - a pointer to it

A parameter whose length is not fixed -- an unconstrained array parameter --
is not yet carried across; constrain it, or pass its length as a separate
parameter.


C Calling VHDL
==============

VHDL has no syntax for saying that a subprogram may be called from outside,
so saying it in the source would mean inventing a construct and making the
file unreadable to other tools. It is said in a :ref:`Verilator Control Files`
instead, and the VHDL stays portable:

.. code-block:: vhdl

     architecture rtl of design is
       function vhdl_square (v : integer) return integer is
       begin
         return v * v;
       end function;
     begin
     end architecture;

.. code-block::

     `verilator_config
     vhdl_export -module "design" -function "vhdl_square"

The C side calls it by name, after saying which instance of the design it
is to run in:

.. code-block:: cpp

     #include "svdpi.h"
     #include "Vdesign__Dpi.h"

     svSetScope(svGetScopeFromName("TOP.design"));
     const int nine = vhdl_square(3);

``svSetScope`` is needed because a design may hold more than one instance of
the entity, each with its own signals. The scope name is the instance path,
spelled as VPI spells it.

Use ``-task`` rather than ``-function`` for a procedure. An overloaded
subprogram cannot be exported, since the C name would not say which of them
is meant.


Reaching Signals Over VPI
=========================

Verilator's VPI works on VHDL objects with nothing added. A VHDL signal,
port, variable or generic lands in the same runtime tables a Verilog one
does, so ``vpi_handle_by_name``, ``vpi_get_value`` and ``vpi_put_value``
reach it as usual. The object must be public, which
:vlopt:`--public-flat-rw` does for everything, or a control file does
selectively:

.. code-block::

     `verilator_config
     public_flat_rw -module "counter" -var "count"

.. code-block:: cpp

     vpiHandle countp = vpi_handle_by_name(
         const_cast<char*>("TOP.top.u_cnt.count"), nullptr);
     t_vpi_value value;
     value.format = vpiIntVal;
     vpi_get_value(countp, &value);

Two things to know about the names. VHDL folds a basic identifier to lower
case, and that lowercased spelling is what VPI sees, whatever case the
source used. And the path starts at ``TOP``, then the top entity, then the
instance labels down to the object.

A value written through VPI takes effect at the next ``eval()``, exactly as
it does for Verilog.


Running cocotb Against a VHDL Design
====================================

cocotb reaches Verilator over VPI, which is the same path it uses for GHDL,
so a VHDL design under Verilator needs no cocotb-side change. Point cocotb's
makefile at the ``.vhd`` sources:

.. code-block:: make

     SIM = verilator
     VERILOG_SOURCES = $(PWD)/dut.vhd
     TOPLEVEL = dut
     MODULE = test_dut
     EXTRA_ARGS += --timing
     include $(shell cocotb-config --makefiles)/Makefile.sim

.. code-block:: python

     import cocotb
     from cocotb.clock import Clock
     from cocotb.triggers import RisingEdge


     @cocotb.test()
     async def test_count(dut):
         cocotb.start_soon(Clock(dut.clk, 10, unit="ns").start())
         dut.rst.value = 1
         await RisingEdge(dut.clk)
         dut.rst.value = 0
         for expected in range(1, 6):
             await RisingEdge(dut.clk)
             assert dut.q.value == expected

Names in the Python are the lowercased VHDL names. ``TOPLEVEL`` names the
entity.

cocotb's documentation notes that GHDL's VPI "prevents cocotb from accessing
some VHDL-specific constructs, like 9-value signals". That limitation does
not apply here, because the model is two-state by design; see :ref:`VHDL
Limitations` for what that means.


Sharing One C Model
===================

Because a VHDL foreign subprogram and a SystemVerilog DPI import become the
same node, one C function serves both languages in one model, with no
bridging:

.. code-block:: vhdl

     function c_popcount (v : integer) return integer;
     attribute foreign of c_popcount : function is "VHPIDIRECT c_popcount";

.. code-block::

     import "DPI-C" function int c_popcount(input int v);

Both call the same ``c_popcount``, and the C compiler can inline it into
either caller.

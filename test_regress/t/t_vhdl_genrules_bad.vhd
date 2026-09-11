-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A generic type is closely related to itself alone, so nothing converts to
-- or from it (9.3.6), and it comes with "=" and "/=", which a file or a
-- protected type cannot give it (6.5.3). References: nvc 1.22.1, GHDL
-- 7.0.0-dev, Questa 2025.2.

package gen is
  generic (type t; type u);
  function f(x : t; y : u) return integer;
end package;

package body gen is
  function f(x : t; y : u) return integer is
    variable v : t;
    variable i : integer;
  begin
    v := t(x);
    v := t(0);  -- Bad
    v := t(y);  -- Bad
    i := integer(x);  -- Bad
    return i;
  end function;
end package body;

package pp is
  type pt is protected
    procedure p;
  end protected;
end package;

package body pp is
  type pt is protected body
    procedure p is
    begin
    end procedure;
  end protected body;
end package body;

use std.textio.all;
use work.pp.all;

entity t is
end entity;

architecture a of t is
  package ok_inst is new work.gen generic map (t => integer, u => bit);
  package bad_file is new work.gen generic map (t => text, u => bit);  -- Bad
  package bad_prot is new work.gen generic map (t => integer, u => pt);  -- Bad
begin
end architecture;

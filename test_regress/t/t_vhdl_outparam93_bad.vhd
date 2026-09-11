-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Before VHDL-2008 nothing reads a port or a parameter of mode out, and a
-- procedure's parameter of mode in or inout reads what it is given (4.2.2.1).
-- References: nvc 1.22.1, GHDL 7.0.0-dev, Questa 2025.2.

package pkg is
  procedure take(signal sig : in integer);
  procedure bump(x : inout integer);
  procedure put(signal sig : out integer);
end package;

package body pkg is
  procedure take(signal sig : in integer) is
  begin
  end procedure;
  procedure bump(x : inout integer) is
  begin
    x := x + 1;
  end procedure;
  procedure put(signal sig : out integer) is
  begin
    sig <= 1;
  end procedure;
end package body;

use work.pkg.all;

entity t is
  port (po : out integer; pi : in integer);
end entity;

architecture a of t is
  procedure outer(v : out integer) is
  begin
    bump(v);  -- Bad
  end procedure;
begin
  process
  begin
    take(pi);
    put(po);
    take(po);  -- Bad
    wait;
  end process;
end architecture;

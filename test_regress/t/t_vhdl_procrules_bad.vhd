-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A method of a protected type holds the object until it returns, so it
-- cannot wait. A procedure declared outside any process assigns only the
-- signals it is given, though it may force any. A type conversion is between
-- closely related types.
-- A generic with no default is given a value by every instance.

entity t_vhdl_procrules_sub is
  generic (width : positive; depth : natural := 4);
end entity;

architecture a of t_vhdl_procrules_sub is
begin
end architecture;

entity t_vhdl_procrules_bad is
end entity;

architecture a of t_vhdl_procrules_bad is
  signal s : bit;
  type counter is protected
    procedure incr;
  end protected;
  type counter is protected body
    variable n : integer := 0;
    procedure incr is
    begin
      n := n + 1;
      wait for 1 ns;
    end procedure;
  end protected body;
  type rec1 is record
    a : integer;
  end record;
  type rec2 is record
    a : integer;
  end record;
  -- Legal: the signal is its parameter, or is only forced
  procedure drive (signal p : out bit) is
  begin
    p <= '1';
    s <= force '1';
  end procedure;
  -- Illegal
  procedure drive_s is
  begin
    s <= '0';
  end procedure;
begin
  ok : entity work.t_vhdl_procrules_sub generic map (width => 8);
  bad : entity work.t_vhdl_procrules_sub generic map (depth => 2);
  process
    variable r1 : rec1;
    variable r2 : rec2;
    variable i : integer;
    variable x : real;
  begin
    x := real(i);
    r2 := rec2(r1);
    wait;
  end process;
end architecture;

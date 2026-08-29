-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_dynlen is
end entity;

architecture a of t_vhdl_dynlen is

  type int_vec is array (natural range <>) of integer;

  -- Nothing here says how long the result is: only the value of n, which
  -- the call site cannot fold, so a copy per call site cannot serve
  function build (n : natural) return int_vec is
    variable v : int_vec(1 to n);
  begin
    for i in v'range loop v(i) := i; end loop;
    return v;
  end function;

  -- Nor how long the parameter is
  function total (v : int_vec) return integer is
    variable s : integer := 0;
  begin
    for i in v'range loop s := s + v(i); end loop;
    return s;
  end function;

  function twice (v : int_vec) return integer is
  begin
    return 2 * total(v);
  end function;

  function measure (v : int_vec) return integer is
  begin
    return v'length + v'left + v'right;
  end function;

  function fixedSum (v : int_vec(1 to 3)) return integer is
  begin
    return v(1) + v(2) + v(3);
  end function;

  -- One that runs the other way round
  function down (n : natural) return int_vec is
    variable v : int_vec(n downto 1);
  begin
    for i in v'range loop v(i) := i; end loop;
    return v;
  end function;

  -- A parameter the body writes: its bounds are the actual's, so it goes by
  -- reference rather than by value
  procedure doubled (v : in int_vec; w : out int_vec; n : out natural) is
  begin
    n := 0;
    for i in v'range loop
      w(i) := 2 * v(i);
      n := n + 1;
    end loop;
  end procedure;

  function viaOut (k : natural) return integer is
    variable src : int_vec(1 to k);
    variable dst : int_vec(1 to k);
    variable cnt : natural;
    variable s : integer := 0;
  begin
    for i in src'range loop src(i) := i; end loop;
    doubled(src, dst, cnt);
    for i in dst'range loop s := s + dst(i); end loop;
    return s + cnt;
  end function;

begin

  main : process is
    variable n : natural := 3;
    variable c : int_vec(1 to 3);
    variable d : int_vec(3 downto 1);
  begin
    assert total(build(n)) = 6 report "sum" severity failure;
    assert twice(build(n)) = 12 report "passed on" severity failure;
    assert measure(build(n)) = 7 report "bounds" severity failure;
    assert fixedSum(build(n)) = 6 report "into a fixed parameter" severity failure;

    c := (1, 2, 3);
    assert build(n) = c report "compared with a fixed value" severity failure;
    c := build(n);
    assert c(2) = 2 report "assigned into a fixed object" severity failure;

    -- A descending range keeps its own bounds
    assert measure(down(n)) = 7 report "descending bounds" severity failure;
    d := down(n);
    assert d(3) = 3 and d(1) = 1 report "descending elements" severity failure;

    -- A length only the value knows, given to something that carries it
    n := 5;
    assert total(build(n)) = 15 report "second length" severity failure;

    n := 3;
    assert viaOut(n) = 15 report "written by reference" severity failure;
    n := 5;

    -- The width a conversion is given need not be a number here either
    assert to_integer(unsigned(std_logic_vector(to_unsigned(9, n)))) = 9
      report "conversion" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;

end architecture;

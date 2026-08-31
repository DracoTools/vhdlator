-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A parameter the body writes and whose length the call decides goes by
-- reference (4.2.2.3).  Where the call hands it an object whose own
-- subtype fixes the length, a carrier is made for the call and copied
-- back into the object afterwards, which is what a copy-out parameter
-- means anyway.  Answers from nvc 1.22.1.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_dynout is
end entity;

architecture rtl of t_vhdl_dynout is

  signal k : integer := 12;

  function mkv(constant kk : integer) return std_logic_vector is
    variable z : std_logic_vector(kk - 1 downto 0) := (others => '0');
  begin
    z(2) := '1';
    return z;
  end function;

  procedure stir(constant expr : in std_logic_vector;
                 variable ref : inout std_logic_vector;
                 variable o : out integer) is
  begin
    ref := expr;
    -- Between two closely related array types, over values that carry
    -- their own bounds, a conversion changes nothing at all (9.3.6)
    ref := std_logic_vector(unsigned(ref) + 3);
    o := to_integer(unsigned(ref));
  end procedure;

  -- The object handed over carries its own bounds
  procedure wide(variable o : out integer) is
    variable ref : std_logic_vector(11 downto 0);
  begin
    stir(mkv(12), ref, o);
  end procedure;

  -- And here it has a length of its own, so it makes the round trip
  procedure roundtrip(variable o : out integer) is
    variable ref : std_logic_vector(0 to 11);
  begin
    stir(mkv(k), ref, o);
    o := o + 100 * to_integer(unsigned(ref));
  end procedure;

begin
  process
    variable r : integer;
  begin
    wide(r);
    assert r = 7 report "by reference" severity failure;
    roundtrip(r);
    assert r = 707 report "copied in and back out" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;

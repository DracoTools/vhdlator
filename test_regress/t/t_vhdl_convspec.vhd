-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A conversion of std_logic_1164 keeps the same bits and is emitted as
-- nothing at all, so what it gives back is worth exactly what it was handed.
-- Where it was handed the result of a function written against an
-- unconstrained array, that function is compiled once per call site and the
-- copy hands back a value of a fixed width -- but the conversion around it
-- was read as handing back a value that carries its own bounds, because the
-- declaration of the function says no length and the copy that does had not
-- been made yet when the conversion was analyzed.
--
-- ieee.fixed_pkg reaches this on every arithmetic result: TO_REAL writes
-- "arg_int := To_X01(cleanvec(arg))" where cleanvec is "return arg".

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_convspec is
end entity;

architecture a of t_vhdl_convspec is

  -- Written against an unconstrained array, and hands back what it was
  -- given: a copy made for one call site returns a fixed width
  function clean (v : std_ulogic_vector) return std_ulogic_vector is
  begin
    return v;
  end function;

  -- The conversion sits between that call and an object whose subtype fixes
  -- the bounds, which is where the two answers had to agree
  function ones (v : std_ulogic_vector) return integer is
    constant l : integer := v'high;
    constant r : integer := v'low;
    variable w : std_ulogic_vector (l downto r);
    variable n : integer := 0;
  begin
    w := to_x01 (clean (v));
    for i in w'range loop
      if w (i) = '1' then n := n + 1; end if;
    end loop;
    return n;
  end function;

  -- The other family of the same rule, numbered the other way round
  function zeros (v : std_ulogic_vector) return integer is
    variable w : std_ulogic_vector (v'range);
    variable n : integer := 0;
  begin
    w := to_01 (clean (v));
    for i in w'range loop
      if w (i) = '0' then n := n + 1; end if;
    end loop;
    return n;
  end function;

  -- And across the logic families, which is the same conversion again
  function ones_bit (v : bit_vector) return integer is
    variable w : bit_vector (v'range);
    variable n : integer := 0;
  begin
    w := to_bitvector (to_stdulogicvector (clean (to_stdulogicvector (v))));
    for i in w'range loop
      if w (i) = '1' then n := n + 1; end if;
    end loop;
    return n;
  end function;

  signal eight : std_ulogic_vector (7 downto 0) := "10110001";
  signal five  : std_ulogic_vector (4 downto 0) := "11010";
  signal up    : std_ulogic_vector (0 to 5) := "011011";
  signal bits  : bit_vector (7 downto 0) := "11100001";

begin

  process
  begin
    -- Two widths of the same function, so the copies really are two
    assert ones (eight) = 4 report "bad ones eight" severity failure;
    assert ones (five) = 3 report "bad ones five" severity failure;
    assert ones (up) = 4 report "bad ones up" severity failure;
    assert zeros (eight) = 4 report "bad zeros eight" severity failure;
    assert zeros (five) = 2 report "bad zeros five" severity failure;
    assert ones_bit (bits) = 4 report "bad ones_bit" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;

end architecture;

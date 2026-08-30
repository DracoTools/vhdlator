-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- "v'range" is "v'left to v'right" and nothing shorter (16.2.4).  Where
-- v's own length is only decided by running, an object declared with that
-- range is as long as v turns out to be -- which is how VUnit's codec
-- gives a decoded value the length its encoding says.

package dynrange_pkg is
  type rng_t is array (integer range <>) of bit;
  function make(n : natural) return rng_t;
  function twice(n : natural) return natural;
end package;

package body dynrange_pkg is
  function make(n : natural) return rng_t is
    variable r : rng_t(1 to n) := (others => '1');
  begin
    return r;
  end;

  function twice(n : natural) return natural is
    constant got : rng_t := make(n);
    variable v : bit_vector(got'range) := (others => '0');
    variable w : bit_vector(got'reverse_range) := (others => '0');
  begin
    return got'length + v'length + w'length;
  end;
end package body;

use work.dynrange_pkg.all;

entity t_vhdl_dynrange is
end entity;

architecture a of t_vhdl_dynrange is
begin
  process
    variable n : natural := 5;
  begin
    assert twice(n) = 15 report "wrong total" severity failure;
    n := 1;
    assert twice(n) = 3 report "wrong total for one" severity failure;
    n := 0;
    assert twice(n) = 0 report "wrong total for none" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;

-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A function returning an unconstrained vector, called with a record of
-- integers that an architecture works out from a string generic: the copy
-- reads the constant where it reads the parameter, so the result is as long
-- as each instance settles, and a constant of the function computed from
-- the record sizes a variable too. The value is a generic of an inner
-- instance, so elaboration has to work it out per instance. Shaped after
-- en_cl_fix's cl_fix_from_real (open-logic's olo_fix_sample_hold).

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.math_real.all;
package t_vhdl_recstandin_pkg is
  type fmt_t is record
    s : natural range 0 to 1;
    i : integer;
    f : integer;
  end record;
  function width(fmt : fmt_t) return natural;
  function fmt_of(s : string) return fmt_t;
  function from_real(a : real; fmt : fmt_t) return std_logic_vector;
end package;
package body t_vhdl_recstandin_pkg is
  function width(fmt : fmt_t) return natural is
  begin
    return fmt.s + fmt.i + fmt.f;
  end function;
  -- "(s,i,f)", one digit each
  function fmt_of(s : string) return fmt_t is
    constant n : string(1 to s'length) := s;
  begin
    return (s => character'pos(n(2)) - character'pos('0'),
            i => character'pos(n(4)) - character'pos('0'),
            f => character'pos(n(6)) - character'pos('0'));
  end function;
  function from_real(a : real; fmt : fmt_t) return std_logic_vector is
    constant chunk_c : positive := 4;
    constant count_c : positive := (width(fmt) + chunk_c - 1) / chunk_c;
    variable a_v : real;
    variable r_v : std_logic_vector(chunk_c * count_c - 1 downto 0);
  begin
    a_v := floor(a * 2.0 ** fmt.f + 0.5);
    for k in 0 to count_c - 1 loop
      r_v((k + 1) * chunk_c - 1 downto k * chunk_c)
        := std_logic_vector(to_unsigned(integer(a_v - 2.0 ** chunk_c * floor(a_v / 2.0 ** chunk_c)), chunk_c));
      a_v := floor(a_v / 2.0 ** chunk_c);
    end loop;
    return r_v(width(fmt) - 1 downto 0);
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_recstandin_leaf is
  generic (w : positive; r : std_logic_vector);
  port (o : out std_logic_vector(w - 1 downto 0));
end entity;
architecture a of t_vhdl_recstandin_leaf is
begin
  o <= r;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_recstandin_pkg.all;
entity t_vhdl_recstandin_mid is
  generic (fmt_g : string; value_g : real);
  port (o : out std_logic_vector(width(fmt_of(fmt_g)) - 1 downto 0));
end entity;
architecture a of t_vhdl_recstandin_mid is
  constant fmt_c : fmt_t := fmt_of(fmt_g);
  constant rst_c : std_logic_vector(width(fmt_c) - 1 downto 0) := from_real(value_g, fmt_c);
begin
  u : entity work.t_vhdl_recstandin_leaf generic map (w => width(fmt_c), r => rst_c)
    port map (o => o);
end architecture;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_recstandin is
end entity;
architecture a of t_vhdl_recstandin is
  signal o1 : std_logic_vector(7 downto 0);
  signal o2 : std_logic_vector(9 downto 0);
begin
  u1 : entity work.t_vhdl_recstandin_mid generic map (fmt_g => "(0,8,0)", value_g => 3.0)
    port map (o => o1);
  -- 2.75 with two fraction bits: 1011, in ten bits
  u2 : entity work.t_vhdl_recstandin_mid generic map (fmt_g => "(1,7,2)", value_g => 2.75)
    port map (o => o2);
  process
  begin
    wait for 1 ns;
    assert o1 = "00000011" report "o1 " & to_string(o1) severity failure;
    assert o2 = "0000001011" report "o2 " & to_string(o2) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;

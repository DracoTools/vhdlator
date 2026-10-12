-- DESCRIPTION: Verilator: VHDL record aggregate settling a call's copy
--
-- A function over a record of two open sfixed is copied for a call whose actual is an aggregate: the aggregate's elements say how long they are. A function returning a qualified aggregate of such a record builds it in the shape it returns (from NVC issue810).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.fixed_pkg.all;
package t_vhdl_recaggspec_pkg is
  type complex is record
    re : sfixed;
    im : sfixed;
  end record;
  function mk(r : sfixed) return complex;
  function "*"(l, r : complex) return complex;
  function "*"(l : complex; r : sfixed) return complex;
end package;
package body t_vhdl_recaggspec_pkg is
  function mk(r : sfixed) return complex is
  begin
    return complex'(re => r, im => resize(-r, r'high, r'low));
  end function;
  function "*"(l, r : complex) return complex is
  begin
    return complex'(re => (l.re * r.re - l.im * r.im), im => (l.re * r.im + l.im * r.re));
  end function;
  function "*"(l : complex; r : sfixed) return complex is
  begin
    return l * complex'(re => r, im => to_sfixed(0.0, r'high, r'low));
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use ieee.fixed_pkg.all;
use work.t_vhdl_recaggspec_pkg.all;
entity t_vhdl_recaggspec is end entity;
architecture a of t_vhdl_recaggspec is
begin
  process
    variable a : complex(re(3 downto -4), im(3 downto -4));
    variable b : sfixed(3 downto -4);
    variable p : complex(re(8 downto -8), im(8 downto -8));
  begin
    a := mk(to_sfixed(1.5, 3, -4));
    assert to_real(a.im) = -1.5 report "mk " & to_string(a.im) severity failure;
    b := to_sfixed(2.0, 3, -4);
    p := a * b;
    assert to_real(p.re) = 3.0 report "re " & to_string(p.re) severity failure;
    assert to_real(p.im) = -3.0 report "im " & to_string(p.im) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;

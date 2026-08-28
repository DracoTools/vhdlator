-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

package pads is
  -- The result is as long as the second argument says
  function pad(constant d : std_logic_vector; constant w : positive := 128)
    return std_logic_vector;
  -- and this one's result is as long as what it returns, which is that call
  function pad64(constant d : std_logic_vector) return std_logic_vector;
end package;

package body pads is
  function pad(constant d : std_logic_vector; constant w : positive := 128)
    return std_logic_vector is
    variable v : std_logic_vector(w - 1 downto 0) := (others => '0');
  begin
    v(d'length - 1 downto 0) := d;
    return v;
  end function;

  function pad64(constant d : std_logic_vector) return std_logic_vector is
  begin
    return pad(d, 64);
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.pads.all;

entity t_vhdl_reslen is
end entity;

architecture a of t_vhdl_reslen is
  -- The parameter is 64 long, and only the body of pad64 says so
  procedure take(constant v : in std_logic_vector(63 downto 0);
                 constant want : in std_logic_vector(7 downto 0)) is
  begin
    assert v'length = 64 report "length is " & integer'image(v'length) severity failure;
    assert v(7 downto 0) = want report "low byte wrong" severity failure;
    assert v(63 downto 8) = (63 downto 8 => '0') report "pad wrong" severity failure;
  end procedure;
begin
  main : process
    variable b : std_logic_vector(7 downto 0) := x"a5";
  begin
    take(pad64(b), x"a5");
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;

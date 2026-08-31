-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A call may hand one value whose length only running decides and another
-- whose length is fixed to the same subprogram.  Both are checked against
-- nvc 1.22.1, including numeric_std's own equality, which compares what
-- the two operands are worth rather than how long they are (16.5).

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_dynmix is
end entity;

architecture rtl of t_vhdl_dynmix is

  function mk(constant kk : integer; constant seed : integer) return unsigned is
    variable z : unsigned(kk - 1 downto 0) := (others => '0');
  begin
    for i in 0 to kk - 1 loop
      if ((seed / (2 ** (i mod 20))) mod 2) = 1 then z(i) := '1'; end if;
    end loop;
    return z;
  end function;

  function pu(constant n : unsigned; constant m : unsigned) return string is
  begin
    return integer'image(n'length) & "," & integer'image(m'length) & ","
      & integer'image(to_integer(n)) & "," & integer'image(to_integer(m)) & ","
      & boolean'image(n = m) & "," & boolean'image(n > m) & ","
      & boolean'image(n /= m);
  end function;

  signal k : integer := 12;

begin
  process
  begin
    -- One of a length only running decides, one of a fixed length
    assert pu(mk(k, 300), mk(9, 300)) = "12,9,300,300,true,false,false"
      report "carried then fixed" severity failure;
    assert pu(mk(9, 300), mk(k, 300)) = "9,12,300,300,true,false,false"
      report "fixed then carried" severity failure;
    -- And the two calls that hand over one kind alone
    assert pu(mk(9, 300), mk(9, 301)) = "9,9,300,301,false,false,true"
      report "both fixed" severity failure;
    assert pu(mk(k, 300), mk(k, 300)) = "12,12,300,300,true,false,false"
      report "both carried" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;

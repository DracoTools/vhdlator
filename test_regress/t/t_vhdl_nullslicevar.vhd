-- DESCRIPTION: Verilator: VHDL null slices whose ends are variables
--
-- "v(n - 4 downto n)" is null whatever n is, since its ends are the wrong
-- way round by a fixed amount (5.2.1): read it is a value of no elements,
-- written it is given nothing.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_nullslicevar is
end entity;

architecture a of t_vhdl_nullslicevar is
  type mem_t is array (0 to 3) of bit_vector(7 downto 0);
begin
  process
    variable v : bit_vector(7 downto 0) := x"5a";
    variable m : mem_t := (x"01", x"02", x"03", x"04");
    variable n : integer := 3;
  begin
    assert v(n - 4 downto n) = "" severity failure;
    assert v(n - 4 downto n)'length = 0 severity failure;
    v(n - 1 downto n) := "";
    assert v = x"5a" severity failure;
    m(n + 1 to n) := m(n to n - 1);
    assert m = (x"01", x"02", x"03", x"04") severity failure;
    n := 6;
    assert v(n - 4 downto n) = "" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;

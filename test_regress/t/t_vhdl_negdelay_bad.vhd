-- DESCRIPTION: Verilator: VHDL run-time check: a negative delay
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_negdelay_bad is
end entity;

architecture a of t_vhdl_negdelay_bad is
  signal s : integer := 0;
  function noret(x : integer) return integer is
  begin
    if x > 0 then
      return x;
    end if;
  end function;
begin
  process is
    variable i : integer := 2;
    variable n : integer := 1;
  begin
    s <= 1 after n * 1 ns;
    wait for n * 1 ns;
    report "one ns fits";
    n := -1;
    s <= 2 after n * 1 ns;
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
